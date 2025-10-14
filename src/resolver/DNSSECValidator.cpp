/*
 * ----------------------------------------
 * Arquivo: DNSSECValidator.cpp
 * Propósito: Implementação do validador de cadeia de confiança DNSSEC e assinaturas criptográficas
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#include "dns_resolver/DNSSECValidator.h"
#include "dns_resolver/DNSParser.h"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

namespace dns_resolver {

DNSSECValidator::DNSSECValidator(
    const TrustAnchorStore& trust_anchors,
    bool trace_enabled
) : trust_anchors_(trust_anchors), trace_enabled_(trace_enabled) {
}

// ========== CÁLCULO DE KEY TAG ==========

uint16_t DNSSECValidator::calculateKeyTag(const DNSKEYRecord& dnskey) {
    std::vector<uint8_t> rdata;
    
    // Serializar DNSKEY RDATA
    rdata.push_back((dnskey.flags >> 8) & 0xFF);
    rdata.push_back(dnskey.flags & 0xFF);
    rdata.push_back(dnskey.protocol);
    rdata.push_back(dnskey.algorithm);
    rdata.insert(rdata.end(), dnskey.public_key.begin(), dnskey.public_key.end());
    
    // Algoritmo de checksum conforme RFC 4034 Appendix B
    uint32_t ac = 0;
    for (size_t i = 0; i < rdata.size(); i++) {
        ac += (i & 1) ? rdata[i] : (static_cast<uint32_t>(rdata[i]) << 8);
    }
    ac += (ac >> 16) & 0xFFFF;
    
    return static_cast<uint16_t>(ac & 0xFFFF);
}

// ========== ENCODING PARA DIGEST ==========

std::vector<uint8_t> DNSSECValidator::encodeDomainName(const std::string& domain) const {
    std::vector<uint8_t> buffer;
    
    if (domain == ".") {
        buffer.push_back(0x00);  // Root
        return buffer;
    }
    
    // Encode manualmente (sem trailing dot)
    std::string clean_domain = domain;
    if (!clean_domain.empty() && clean_domain.back() == '.') {
        clean_domain.pop_back();
    }
    
    std::istringstream iss(clean_domain);
    std::string label;
    while (std::getline(iss, label, '.')) {
        if (!label.empty()) {
            buffer.push_back(static_cast<uint8_t>(label.size()));
            buffer.insert(buffer.end(), label.begin(), label.end());
        }
    }
    buffer.push_back(0x00);  // Terminator
    
    return buffer;
}

std::vector<uint8_t> DNSSECValidator::encodeDNSKEYForDigest(
    const DNSKEYRecord& dnskey,
    const std::string& zone
) {
    std::vector<uint8_t> buffer;
    
    // 1. Encode owner name (zone) no formato wire
    std::vector<uint8_t> owner = encodeDomainName(zone);
    buffer.insert(buffer.end(), owner.begin(), owner.end());
    
    // 2. Adicionar DNSKEY RDATA
    buffer.push_back((dnskey.flags >> 8) & 0xFF);
    buffer.push_back(dnskey.flags & 0xFF);
    buffer.push_back(dnskey.protocol);
    buffer.push_back(dnskey.algorithm);
    buffer.insert(buffer.end(), dnskey.public_key.begin(), dnskey.public_key.end());
    
    return buffer;
}

// ========== CÁLCULO DE DIGEST ==========

std::vector<uint8_t> DNSSECValidator::calculateDigest(
    const DNSKEYRecord& dnskey,
    const std::string& zone,
    uint8_t digest_type
) {
    // Criar buffer: owner name + DNSKEY RDATA
    std::vector<uint8_t> buffer = encodeDNSKEYForDigest(dnskey, zone);
    
    // Calcular hash
    if (digest_type == 2) {  // SHA-256
        unsigned char digest[SHA256_DIGEST_LENGTH];
        SHA256(buffer.data(), buffer.size(), digest);
        return std::vector<uint8_t>(digest, digest + SHA256_DIGEST_LENGTH);
    } else if (digest_type == 1) {  // SHA-1
        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1(buffer.data(), buffer.size(), digest);
        return std::vector<uint8_t>(digest, digest + SHA_DIGEST_LENGTH);
    } else {
        throw std::runtime_error(
            "Unsupported digest type: " + std::to_string(digest_type)
        );
    }
}

// ========== VALIDAÇÃO DS ↔ DNSKEY ==========

bool DNSSECValidator::validateDNSKEY(
    const DNSKEYRecord& dnskey,
    const DSRecord& ds,
    const std::string& zone
) {
    traceLog("  Validating DNSKEY for zone: " + zone);
    
    // 1. Verificar key tag
    uint16_t calculated_tag = calculateKeyTag(dnskey);
    traceLog("    Calculated key tag: " + std::to_string(calculated_tag));
    traceLog("    Expected key tag:   " + std::to_string(ds.key_tag));
    
    if (calculated_tag != ds.key_tag) {
        traceLog("    ❌ Key tag mismatch!");
        return false;
    }
    traceLog("    ✅ Key tag match");
    
    // 2. Verificar algorithm
    if (dnskey.algorithm != ds.algorithm) {
        traceLog("    ❌ Algorithm mismatch!");
        return false;
    }
    traceLog("    ✅ Algorithm match (" + std::to_string(dnskey.algorithm) + ")");
    
    // 3. Calcular digest
    std::vector<uint8_t> calculated_digest = calculateDigest(
        dnskey, zone, ds.digest_type
    );
    
    traceLog("    Calculated digest: " + formatHexShort(calculated_digest));
    traceLog("    Expected digest:   " + formatHexShort(ds.digest));
    
    // 4. Comparar digests (timing-safe - comparar todos bytes)
    if (calculated_digest.size() != ds.digest.size()) {
        traceLog("    ❌ Digest size mismatch!");
        return false;
    }
    
    bool match = true;
    for (size_t i = 0; i < calculated_digest.size(); i++) {
        if (calculated_digest[i] != ds.digest[i]) {
            match = false;
            // Continue comparando (timing-safe)
        }
    }
    
    if (!match) {
        traceLog("    ❌ Digest mismatch!");
        return false;
    }
    
    traceLog("    ✅ Digest match - DNSKEY validated!");
    return true;
}

bool DNSSECValidator::validateDNSKEYWithTrustAnchor(
    const DNSKEYRecord& dnskey,
    const TrustAnchor& ta,
    const std::string& zone
) {
    // Converter trust anchor para DS format
    DSRecord ta_as_ds;
    ta_as_ds.key_tag = ta.key_tag;
    ta_as_ds.algorithm = ta.algorithm;
    ta_as_ds.digest_type = ta.digest_type;
    ta_as_ds.digest = ta.digest;
    
    return validateDNSKEY(dnskey, ta_as_ds, zone);
}

// ========== VALIDAÇÃO DE CADEIA COMPLETA ==========

ValidationResult DNSSECValidator::validateChain(
    const std::string& target_zone,
    const std::map<std::string, std::vector<DNSKEYRecord>>& dnskeys,
    const std::map<std::string, std::vector<DSRecord>>& ds_records
) {
    traceLog("\n=== DNSSEC Chain Validation ===");
    traceLog("Target zone: " + target_zone);
    
    // 1. Validar root DNSKEY com trust anchor
    auto root_tas = trust_anchors_.getTrustAnchorsForZone(".");
    if (root_tas.empty()) {
        traceLog("❌ No trust anchor for root zone");
        return ValidationResult::Indeterminate;
    }
    
    traceLog("\nStep 1: Validate root DNSKEY with trust anchor");
    traceLog("Trust anchors for root: " + std::to_string(root_tas.size()));
    
    auto root_dnskeys_it = dnskeys.find(".");
    if (root_dnskeys_it == dnskeys.end() || root_dnskeys_it->second.empty()) {
        traceLog("❌ No DNSKEY for root zone (DNSSEC not available)");
        return ValidationResult::Insecure;
    }
    
    bool root_validated = false;
    for (const auto& ta : root_tas) {
        traceLog("  Trying trust anchor (Key Tag " + std::to_string(ta.key_tag) + ")");
        
        for (const auto& dnskey : root_dnskeys_it->second) {
            if (validateDNSKEYWithTrustAnchor(dnskey, ta, ".")) {
                traceLog("✅ Root DNSKEY validated with trust anchor!");
                root_validated = true;
                break;
            }
        }
        if (root_validated) break;
    }
    
    if (!root_validated) {
        traceLog("❌ Root DNSKEY validation failed - BOGUS!");
        return ValidationResult::Bogus;
    }
    
    // 2. Validar cada zona da cadeia (bottom-up)
    std::string current_zone = target_zone;
    
    while (current_zone != ".") {
        std::string parent = getParentZone(current_zone);
        traceLog("\nStep: Validate zone '" + current_zone + "' (parent: '" + parent + "')");
        
        // Obter DS da zona pai (ou trust anchor se parent é root)
        auto ds_it = ds_records.find(current_zone);
        if (ds_it == ds_records.end() || ds_it->second.empty()) {
            traceLog("  No DS records for " + current_zone + " (zone is insecure)");
            return ValidationResult::Insecure;
        }
        
        traceLog("  Found " + std::to_string(ds_it->second.size()) + " DS record(s)");
        
        // Obter DNSKEY da zona atual
        auto dnskey_it = dnskeys.find(current_zone);
        if (dnskey_it == dnskeys.end() || dnskey_it->second.empty()) {
            traceLog("  No DNSKEY for " + current_zone + " (zone is insecure)");
            return ValidationResult::Insecure;
        }
        
        traceLog("  Found " + std::to_string(dnskey_it->second.size()) + " DNSKEY(s)");
        
        // Validar pelo menos uma DNSKEY com algum DS
        bool zone_validated = false;
        for (const auto& ds : ds_it->second) {
            for (const auto& dnskey : dnskey_it->second) {
                if (validateDNSKEY(dnskey, ds, current_zone)) {
                    traceLog("✅ Zone '" + current_zone + "' DNSKEY validated!");
                    zone_validated = true;
                    break;
                }
            }
            if (zone_validated) break;
        }
        
        if (!zone_validated) {
            traceLog("❌ Zone '" + current_zone + "' DNSKEY validation failed - BOGUS!");
            return ValidationResult::Bogus;
        }
        
        current_zone = parent;
    }
    
    traceLog("\n✅ DNSSEC CHAIN VALIDATED: SECURE");
    return ValidationResult::Secure;
}

// ========== HELPERS ==========

std::string DNSSECValidator::getParentZone(const std::string& zone) const {
    if (zone == "." || zone.empty()) {
        return "";  // Root não tem pai
    }
    
    // Remover trailing dot se presente
    std::string clean_zone = zone;
    if (!clean_zone.empty() && clean_zone.back() == '.') {
        clean_zone.pop_back();
    }
    
    size_t dot_pos = clean_zone.find('.');
    if (dot_pos == std::string::npos) {
        return ".";  // TLD → root
    }
    
    return clean_zone.substr(dot_pos + 1);  // Remove primeiro label
}

void DNSSECValidator::traceLog(const std::string& message) const {
    if (trace_enabled_) {
        std::cerr << ";; " << message << std::endl;
    }
}

std::string DNSSECValidator::formatHexShort(
    const std::vector<uint8_t>& data,
    size_t max_bytes
) const {
    std::ostringstream oss;
    size_t limit = std::min(data.size(), max_bytes);
    for (size_t i = 0; i < limit; i++) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
            << static_cast<int>(data[i]);
    }
    if (data.size() > max_bytes) {
        oss << "...";
    }
    return oss.str();
}

std::string DNSSECValidator::toLowercase(const std::string& str) const {
    std::string result = str;
    for (char& c : result) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    return result;
}

// ========== VALIDAÇÃO DE RRSIG ==========

bool DNSSECValidator::validateRRSIG(
    const std::vector<DNSResourceRecord>& rrset,
    const RRSIGRecord& rrsig,
    const DNSKEYRecord& dnskey,
    const std::string& zone
) {
    traceLog("  Validating RRSIG for zone: " + zone);
    traceLog("    Type covered: " + std::to_string(rrsig.type_covered));
    traceLog("    Algorithm: " + std::to_string(rrsig.algorithm));
    traceLog("    Key tag: " + std::to_string(rrsig.key_tag));
    traceLog("    Signer: " + rrsig.signer_name);
    
    // 1. Validar período de validade
    time_t now = time(nullptr);
    
    if (now < static_cast<time_t>(rrsig.signature_inception)) {
        traceLog("    ❌ Signature not yet valid (inception in future)");
        return false;
    }
    
    if (now > static_cast<time_t>(rrsig.signature_expiration)) {
        traceLog("    ❌ Signature expired");
        return false;
    }
    
    traceLog("    ✅ Period valid");
    
    // 2. Verificar que DNSKEY key_tag corresponde
    uint16_t dnskey_tag = calculateKeyTag(dnskey);
    if (dnskey_tag != rrsig.key_tag) {
        traceLog("    ❌ Key tag mismatch");
        return false;
    }
    
    traceLog("    ✅ Key tag match");
    
    // 3. Verificar algoritmo
    if (dnskey.algorithm != rrsig.algorithm) {
        traceLog("    ❌ Algorithm mismatch");
        return false;
    }
    
    traceLog("    ✅ Algorithm match (" + std::to_string(rrsig.algorithm) + ")");
    
    // 4. Canonicalizar RRset
    traceLog("    Canonicalizing RRset (" + std::to_string(rrset.size()) + " records)...");
    std::vector<uint8_t> canonical = canonicalizeRRset(rrset, rrsig);
    
    // 5. Construir buffer de verificação
    std::vector<uint8_t> verification_buffer = buildVerificationBuffer(rrsig, canonical);
    
    // 6. Verificar assinatura conforme algoritmo
    bool signature_valid = false;
    
    if (rrsig.algorithm == 8) {
        // RSA/SHA-256
        traceLog("    Verifying RSA/SHA-256 signature...");
        signature_valid = verifyRSASignature(dnskey.public_key, verification_buffer, rrsig.signature);
    } else if (rrsig.algorithm == 13) {
        // ECDSA P-256/SHA-256
        traceLog("    Verifying ECDSA P-256/SHA-256 signature...");
        signature_valid = verifyECDSASignature(dnskey.public_key, verification_buffer, rrsig.signature);
    } else {
        traceLog("    ❌ Unsupported algorithm: " + std::to_string(rrsig.algorithm));
        return false;
    }
    
    if (signature_valid) {
        traceLog("    ✅ RRSIG signature valid!");
        return true;
    } else {
        traceLog("    ❌ RRSIG signature invalid!");
        return false;
    }
}

std::vector<uint8_t> DNSSECValidator::canonicalizeRRset(
    const std::vector<DNSResourceRecord>& rrset,
    const RRSIGRecord& rrsig
) {
    // RFC 4034 §6.2: Canonical RR Form
    std::vector<uint8_t> buffer;
    
    // Copiar rrset para ordenar
    std::vector<DNSResourceRecord> sorted_rrset = rrset;
    
    // Ordenar lexicograficamente por RDATA
    std::sort(sorted_rrset.begin(), sorted_rrset.end(), 
        [](const DNSResourceRecord& a, const DNSResourceRecord& b) {
            return a.rdata < b.rdata;
        });
    
    for (const auto& rr : sorted_rrset) {
        // Owner name (lowercase, sem compressão)
        std::string owner_lower = toLowercase(rr.name);
        std::vector<uint8_t> owner_encoded = encodeDomainName(owner_lower);
        buffer.insert(buffer.end(), owner_encoded.begin(), owner_encoded.end());
        
        // Type (2 bytes)
        buffer.push_back((rr.type >> 8) & 0xFF);
        buffer.push_back(rr.type & 0xFF);
        
        // Class (2 bytes)
        buffer.push_back((rr.rr_class >> 8) & 0xFF);
        buffer.push_back(rr.rr_class & 0xFF);
        
        // Original TTL do RRSIG (4 bytes)
        uint32_t original_ttl = rrsig.original_ttl;
        buffer.push_back((original_ttl >> 24) & 0xFF);
        buffer.push_back((original_ttl >> 16) & 0xFF);
        buffer.push_back((original_ttl >> 8) & 0xFF);
        buffer.push_back(original_ttl & 0xFF);
        
        // RDLENGTH (2 bytes)
        buffer.push_back((rr.rdlength >> 8) & 0xFF);
        buffer.push_back(rr.rdlength & 0xFF);
        
        // RDATA (bruto)
        buffer.insert(buffer.end(), rr.rdata.begin(), rr.rdata.end());
    }
    
    return buffer;
}

std::vector<uint8_t> DNSSECValidator::buildVerificationBuffer(
    const RRSIGRecord& rrsig,
    const std::vector<uint8_t>& canonical_rrset
) {
    // RFC 4034 §3.1.8.1: RRSIG RDATA (sem signature) + canonical RRset
    std::vector<uint8_t> buffer;
    
    // Type covered (2 bytes)
    buffer.push_back((rrsig.type_covered >> 8) & 0xFF);
    buffer.push_back(rrsig.type_covered & 0xFF);
    
    // Algorithm (1 byte)
    buffer.push_back(rrsig.algorithm);
    
    // Labels (1 byte)
    buffer.push_back(rrsig.labels);
    
    // Original TTL (4 bytes)
    buffer.push_back((rrsig.original_ttl >> 24) & 0xFF);
    buffer.push_back((rrsig.original_ttl >> 16) & 0xFF);
    buffer.push_back((rrsig.original_ttl >> 8) & 0xFF);
    buffer.push_back(rrsig.original_ttl & 0xFF);
    
    // Expiration (4 bytes)
    buffer.push_back((rrsig.signature_expiration >> 24) & 0xFF);
    buffer.push_back((rrsig.signature_expiration >> 16) & 0xFF);
    buffer.push_back((rrsig.signature_expiration >> 8) & 0xFF);
    buffer.push_back(rrsig.signature_expiration & 0xFF);
    
    // Inception (4 bytes)
    buffer.push_back((rrsig.signature_inception >> 24) & 0xFF);
    buffer.push_back((rrsig.signature_inception >> 16) & 0xFF);
    buffer.push_back((rrsig.signature_inception >> 8) & 0xFF);
    buffer.push_back(rrsig.signature_inception & 0xFF);
    
    // Key tag (2 bytes)
    buffer.push_back((rrsig.key_tag >> 8) & 0xFF);
    buffer.push_back(rrsig.key_tag & 0xFF);
    
    // Signer name (wire format, lowercase)
    std::string signer_lower = toLowercase(rrsig.signer_name);
    std::vector<uint8_t> signer_encoded = encodeDomainName(signer_lower);
    buffer.insert(buffer.end(), signer_encoded.begin(), signer_encoded.end());
    
    // Adicionar RRset canonicalizado
    buffer.insert(buffer.end(), canonical_rrset.begin(), canonical_rrset.end());
    
    return buffer;
}

// ========== ALGORITMOS CRIPTOGRÁFICOS ==========

// RAII Guard para EVP_PKEY
struct PKEYGuard {
    EVP_PKEY* pkey;
    PKEYGuard(EVP_PKEY* p) : pkey(p) {}
    ~PKEYGuard() { if (pkey) EVP_PKEY_free(pkey); }
};

void* DNSSECValidator::convertDNSKEYToECDSA(const std::vector<uint8_t>& public_key) {
    // ECDSA P-256: 64 bytes (32 X + 32 Y) - RFC 6605
    if (public_key.size() != 64) {
        throw std::runtime_error("Invalid ECDSA P-256 key size: " + 
                                 std::to_string(public_key.size()) + " (expected 64)");
    }
    
    const uint8_t* x_coord = public_key.data();
    const uint8_t* y_coord = public_key.data() + 32;
    
    // Criar EC_KEY com curva P-256
    EC_KEY* ec_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!ec_key) {
        throw std::runtime_error("Failed to create EC_KEY for P-256");
    }
    
    // Criar BIGNUMs para coordenadas
    BIGNUM* x = BN_bin2bn(x_coord, 32, nullptr);
    BIGNUM* y = BN_bin2bn(y_coord, 32, nullptr);
    
    if (!x || !y) {
        if (x) BN_free(x);
        if (y) BN_free(y);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to create BIGNUMs for ECDSA coordinates");
    }
    
    // Setar coordenadas do ponto público
    if (EC_KEY_set_public_key_affine_coordinates(ec_key, x, y) != 1) {
        BN_free(x);
        BN_free(y);
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to set ECDSA public key coordinates");
    }
    
    BN_free(x);
    BN_free(y);
    
    // Converter para EVP_PKEY
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        EC_KEY_free(ec_key);
        throw std::runtime_error("Failed to create EVP_PKEY for ECDSA");
    }
    
    EVP_PKEY_assign_EC_KEY(pkey, ec_key);
    
    return pkey;
}

void* DNSSECValidator::convertDNSKEYToRSA(const std::vector<uint8_t>& public_key) {
    // RSA format (RFC 3110):
    // [exp_len (1 byte)] [exponent] [modulus]
    // OU (para exponent >= 256 bytes):
    // [0x00] [exp_len high] [exp_len low] [exponent] [modulus]
    
    const uint8_t* data = public_key.data();
    size_t len = public_key.size();
    
    if (len < 3) {
        throw std::runtime_error("RSA key too short (< 3 bytes)");
    }
    
    // Parsear exponent length
    size_t exp_len;
    size_t exp_offset;
    
    if (data[0] == 0) {
        // Multi-byte exponent length
        if (len < 4) {
            throw std::runtime_error("Invalid RSA key format (multi-byte exp_len)");
        }
        exp_len = (static_cast<size_t>(data[1]) << 8) | data[2];
        exp_offset = 3;
    } else {
        // Single-byte exponent length
        exp_len = data[0];
        exp_offset = 1;
    }
    
    // Validar formato
    if (exp_offset + exp_len > len) {
        throw std::runtime_error("RSA exponent exceeds key data");
    }
    
    // Extrair exponent e modulus
    const uint8_t* exponent = data + exp_offset;
    const uint8_t* modulus = data + exp_offset + exp_len;
    size_t modulus_len = len - exp_offset - exp_len;
    
    if (modulus_len == 0) {
        throw std::runtime_error("RSA modulus is empty");
    }
    
    // Criar BIGNUMs
    BIGNUM* n = BN_bin2bn(modulus, modulus_len, nullptr);
    BIGNUM* e = BN_bin2bn(exponent, exp_len, nullptr);
    
    if (!n || !e) {
        if (n) BN_free(n);
        if (e) BN_free(e);
        throw std::runtime_error("Failed to create BIGNUMs for RSA");
    }
    
    // Criar RSA key
    RSA* rsa = RSA_new();
    if (!rsa) {
        BN_free(n);
        BN_free(e);
        throw std::runtime_error("Failed to create RSA key");
    }
    
    RSA_set0_key(rsa, n, e, nullptr);
    
    // Converter para EVP_PKEY
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey) {
        RSA_free(rsa);
        throw std::runtime_error("Failed to create EVP_PKEY for RSA");
    }
    
    EVP_PKEY_assign_RSA(pkey, rsa);
    
    return pkey;
}

bool DNSSECValidator::verifyECDSASignature(
    const std::vector<uint8_t>& public_key,
    const std::vector<uint8_t>& data,
    const std::vector<uint8_t>& signature
) {
    traceLog("  Verifying ECDSA P-256/SHA-256 signature...");
    
    // Validações básicas
    if (public_key.empty() || signature.empty() || data.empty()) {
        traceLog("  ❌ Empty input (key/signature/data)");
        return false;
    }
    
    // Converter DNSKEY para EVP_PKEY
    EVP_PKEY* pkey = nullptr;
    try {
        pkey = static_cast<EVP_PKEY*>(convertDNSKEYToECDSA(public_key));
    } catch (const std::exception& e) {
        traceLog("  ❌ Failed to convert DNSKEY to ECDSA: " + std::string(e.what()));
        return false;
    }
    
    if (!pkey) {
        traceLog("  ❌ Failed to create ECDSA key (null)");
        return false;
    }
    
    PKEYGuard guard(pkey);
    
    // Criar contexto de verificação
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        traceLog("  ❌ Failed to create EVP_MD_CTX");
        return false;
    }
    
    // Inicializar verificação com SHA-256
    int result = EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey);
    if (result != 1) {
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        traceLog("  ❌ DigestVerifyInit failed: " + std::string(err_buf));
        EVP_MD_CTX_free(ctx);
        return false;
    }
    
    // Verificar assinatura
    result = EVP_DigestVerify(ctx, signature.data(), signature.size(),
                              data.data(), data.size());
    
    EVP_MD_CTX_free(ctx);
    
    if (result == 1) {
        traceLog("  ✅ ECDSA signature valid!");
        return true;
    } else {
        unsigned long err = ERR_get_error();
        if (err != 0) {
            char err_buf[256];
            ERR_error_string_n(err, err_buf, sizeof(err_buf));
            traceLog("  ❌ ECDSA verification failed: " + std::string(err_buf));
        } else {
            traceLog("  ❌ ECDSA signature invalid (no OpenSSL error)");
        }
        return false;
    }
}

bool DNSSECValidator::verifyRSASignature(
    const std::vector<uint8_t>& public_key,
    const std::vector<uint8_t>& data,
    const std::vector<uint8_t>& signature
) {
    traceLog("  Verifying RSA/SHA-256 signature...");
    
    // Validações básicas
    if (public_key.empty() || signature.empty() || data.empty()) {
        traceLog("  ❌ Empty input (key/signature/data)");
        return false;
    }
    
    // Converter DNSKEY para EVP_PKEY
    EVP_PKEY* pkey = nullptr;
    try {
        pkey = static_cast<EVP_PKEY*>(convertDNSKEYToRSA(public_key));
    } catch (const std::exception& e) {
        traceLog("  ❌ Failed to convert DNSKEY to RSA: " + std::string(e.what()));
        return false;
    }
    
    if (!pkey) {
        traceLog("  ❌ Failed to create RSA key (null)");
        return false;
    }
    
    PKEYGuard guard(pkey);
    
    // Criar contexto de verificação
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        traceLog("  ❌ Failed to create EVP_MD_CTX");
        return false;
    }
    
    // Inicializar verificação com SHA-256
    int result = EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey);
    if (result != 1) {
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        traceLog("  ❌ DigestVerifyInit failed: " + std::string(err_buf));
        EVP_MD_CTX_free(ctx);
        return false;
    }
    
    // Verificar assinatura
    result = EVP_DigestVerify(ctx, signature.data(), signature.size(),
                              data.data(), data.size());
    
    EVP_MD_CTX_free(ctx);
    
    if (result == 1) {
        traceLog("  ✅ RSA signature valid!");
        return true;
    } else {
        unsigned long err = ERR_get_error();
        if (err != 0) {
            char err_buf[256];
            ERR_error_string_n(err, err_buf, sizeof(err_buf));
            traceLog("  ❌ RSA verification failed: " + std::string(err_buf));
        } else {
            traceLog("  ❌ RSA signature invalid (no OpenSSL error)");
        }
        return false;
    }
}

} // namespace dns_resolver

