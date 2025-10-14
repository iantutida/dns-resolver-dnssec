/*
 * ----------------------------------------
 * Arquivo: DNSSECValidator.h
 * Propósito: Validador de cadeia de confiança DNSSEC e assinaturas criptográficas
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#pragma once

#include "dns_resolver/types.h"
#include "dns_resolver/TrustAnchorStore.h"
#include <string>
#include <vector>
#include <map>

namespace dns_resolver {

// Validador de Cadeia de Confiança DNSSEC
// Responsável por validar que DNSKEYs correspondem aos DS records
// da zona pai, estabelecendo uma cadeia de confiança desde o
// Trust Anchor até a zona alvo
class DNSSECValidator {
public:
    explicit DNSSECValidator(
        const TrustAnchorStore& trust_anchors,
        bool trace_enabled = false
    );
    
    // Validar que DNSKEY corresponde a DS
    bool validateDNSKEY(
        const DNSKEYRecord& dnskey,
        const DSRecord& ds,
        const std::string& zone
    );
    
    // Validar DNSKEY com Trust Anchor (para root zone)
    bool validateDNSKEYWithTrustAnchor(
        const DNSKEYRecord& dnskey,
        const TrustAnchor& ta,
        const std::string& zone
    );
    
    // Validar cadeia completa de confiança
    ValidationResult validateChain(
        const std::string& target_zone,
        const std::map<std::string, std::vector<DNSKEYRecord>>& dnskeys,
        const std::map<std::string, std::vector<DSRecord>>& ds_records
    );
    
    // Obter zona pai de uma zona
    std::string getParentZone(const std::string& zone) const;
    
    // Calcula digest (hash) de uma DNSKEY
    std::vector<uint8_t> calculateDigest(
        const DNSKEYRecord& dnskey,
        const std::string& zone,
        uint8_t digest_type
    );
    
    // Calcula key tag de uma DNSKEY
    uint16_t calculateKeyTag(const DNSKEYRecord& dnskey);
    
    // Validar assinatura RRSIG de um RRset
    bool validateRRSIG(
        const std::vector<DNSResourceRecord>& rrset,
        const RRSIGRecord& rrsig,
        const DNSKEYRecord& dnskey,
        const std::string& zone
    );
    
private:
    const TrustAnchorStore& trust_anchors_;
    bool trace_enabled_;
    
    // Encode domain name no formato wire
    std::vector<uint8_t> encodeDomainName(const std::string& domain) const;
    
    // Encode DNSKEY no formato wire para cálculo de digest
    std::vector<uint8_t> encodeDNSKEYForDigest(
        const DNSKEYRecord& dnskey,
        const std::string& zone
    );
    
    // Canonicaliza RRset conforme RFC 4034 §6.2
    std::vector<uint8_t> canonicalizeRRset(
        const std::vector<DNSResourceRecord>& rrset,
        const RRSIGRecord& rrsig
    );
    
    // Constrói buffer para verificação de assinatura
    std::vector<uint8_t> buildVerificationBuffer(
        const RRSIGRecord& rrsig,
        const std::vector<uint8_t>& canonical_rrset
    );
    
    // Verifica assinatura RSA/SHA-256 (algorithm 8)
    bool verifyRSASignature(
        const std::vector<uint8_t>& public_key,
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& signature
    );
    
    // Verifica assinatura ECDSA P-256/SHA-256 (algorithm 13)
    bool verifyECDSASignature(
        const std::vector<uint8_t>& public_key,
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& signature
    );
    
    // Converte chave pública DNSKEY RSA para EVP_PKEY
    void* convertDNSKEYToRSA(const std::vector<uint8_t>& public_key);
    
    // Converte chave pública DNSKEY ECDSA para EVP_PKEY
    void* convertDNSKEYToECDSA(const std::vector<uint8_t>& public_key);
    
    // Converte string para lowercase (DNS canonical form)
    std::string toLowercase(const std::string& str) const;
    
    // Log de trace (se trace_enabled_)
    void traceLog(const std::string& message) const;
    
    // Formata bytes em hex (primeiros N bytes)
    std::string formatHexShort(const std::vector<uint8_t>& data, size_t max_bytes = 16) const;
};

} // namespace dns_resolver

