#pragma once

#include "dns_resolver/types.h"
#include "dns_resolver/TrustAnchorStore.h"
#include <string>
#include <vector>
#include <map>

namespace dns_resolver {

/**
 * Validador de Cadeia de Confiança DNSSEC (Story 3.3)
 * 
 * Responsável por validar que DNSKEYs correspondem aos DS records
 * da zona pai, estabelecendo uma cadeia de confiança desde o
 * Trust Anchor até a zona alvo.
 */
class DNSSECValidator {
public:
    /**
     * Construtor
     * @param trust_anchors Trust anchors carregados (Story 3.1)
     * @param trace_enabled Se true, emite logs detalhados
     */
    explicit DNSSECValidator(
        const TrustAnchorStore& trust_anchors,
        bool trace_enabled = false
    );
    
    /**
     * Validar que DNSKEY corresponde a DS
     * @param dnskey DNSKEY a validar
     * @param ds DS record da zona pai
     * @param zone Nome da zona (ex: "com")
     * @return true se DNSKEY é autêntico
     */
    bool validateDNSKEY(
        const DNSKEYRecord& dnskey,
        const DSRecord& ds,
        const std::string& zone
    );
    
    /**
     * Validar DNSKEY com Trust Anchor (para root zone)
     * @param dnskey DNSKEY a validar
     * @param ta Trust anchor (convertido para DS format)
     * @param zone Nome da zona (geralmente ".")
     * @return true se DNSKEY é autêntico
     */
    bool validateDNSKEYWithTrustAnchor(
        const DNSKEYRecord& dnskey,
        const TrustAnchor& ta,
        const std::string& zone
    );
    
    /**
     * Validar cadeia completa de confiança
     * @param target_zone Zona final (ex: "google.com")
     * @param dnskeys Mapa zona → lista de DNSKEYs coletados
     * @param ds_records Mapa zona → lista de DS coletados
     * @return Resultado da validação (Secure/Insecure/Bogus/Indeterminate)
     */
    ValidationResult validateChain(
        const std::string& target_zone,
        const std::map<std::string, std::vector<DNSKEYRecord>>& dnskeys,
        const std::map<std::string, std::vector<DSRecord>>& ds_records
    );
    
    /**
     * Obter zona pai de uma zona
     * @param zone Zona atual (ex: "google.com")
     * @return Zona pai (ex: "com"), ou "." se TLD, ou "" se root
     */
    std::string getParentZone(const std::string& zone) const;
    
    /**
     * Calcula digest (hash) de uma DNSKEY
     * @param dnskey DNSKEY a hashear
     * @param zone Nome da zona (owner name)
     * @param digest_type 1=SHA-1, 2=SHA-256
     * @return Digest (20 bytes para SHA-1, 32 para SHA-256)
     * 
     * PUBLIC para permitir testes unitários
     */
    std::vector<uint8_t> calculateDigest(
        const DNSKEYRecord& dnskey,
        const std::string& zone,
        uint8_t digest_type
    );
    
    /**
     * Calcula key tag de uma DNSKEY (RFC 4034 Appendix B)
     * @param dnskey DNSKEY
     * @return Key tag (16 bits)
     * 
     * PUBLIC para permitir testes unitários
     */
    uint16_t calculateKeyTag(const DNSKEYRecord& dnskey);
    
    /**
     * Validar assinatura RRSIG de um RRset (Story 3.4)
     * @param rrset Conjunto de RRs a validar
     * @param rrsig Assinatura RRSIG
     * @param dnskey DNSKEY para verificação (já validada)
     * @param zone Nome da zona
     * @return true se assinatura válida
     */
    bool validateRRSIG(
        const std::vector<DNSResourceRecord>& rrset,
        const RRSIGRecord& rrsig,
        const DNSKEYRecord& dnskey,
        const std::string& zone
    );
    
private:
    const TrustAnchorStore& trust_anchors_;
    bool trace_enabled_;
    
    /**
     * Encode domain name no formato wire
     * @param domain Nome de domínio
     * @return Buffer no formato wire
     */
    std::vector<uint8_t> encodeDomainName(const std::string& domain) const;
    
    /**
     * Encode DNSKEY no formato wire para cálculo de digest
     * @param dnskey DNSKEY
     * @param zone Nome da zona (owner name)
     * @return Buffer: [owner name] + [DNSKEY RDATA]
     */
    std::vector<uint8_t> encodeDNSKEYForDigest(
        const DNSKEYRecord& dnskey,
        const std::string& zone
    );
    
    /**
     * Canonicaliza RRset conforme RFC 4034 §6.2 (Story 3.4)
     * @param rrset Conjunto de RRs
     * @param rrsig RRSIG correspondente
     * @return Buffer canonicalizado
     */
    std::vector<uint8_t> canonicalizeRRset(
        const std::vector<DNSResourceRecord>& rrset,
        const RRSIGRecord& rrsig
    );
    
    /**
     * Constrói buffer para verificação de assinatura (Story 3.4)
     * @param rrsig RRSIG
     * @param canonical_rrset RRset canonicalizado
     * @return Buffer: [RRSIG RDATA sem signature] + [canonical RRset]
     */
    std::vector<uint8_t> buildVerificationBuffer(
        const RRSIGRecord& rrsig,
        const std::vector<uint8_t>& canonical_rrset
    );
    
    /**
     * Verifica assinatura RSA/SHA-256 (algorithm 8) (Story 3.6)
     * @param public_key Chave pública RSA em formato DNSKEY
     * @param data Buffer a verificar
     * @param signature Assinatura
     * @return true se válida
     */
    bool verifyRSASignature(
        const std::vector<uint8_t>& public_key,
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& signature
    );
    
    /**
     * Verifica assinatura ECDSA P-256/SHA-256 (algorithm 13) (Story 3.6)
     * @param public_key Chave pública ECDSA em formato DNSKEY
     * @param data Buffer a verificar
     * @param signature Assinatura
     * @return true se válida
     */
    bool verifyECDSASignature(
        const std::vector<uint8_t>& public_key,
        const std::vector<uint8_t>& data,
        const std::vector<uint8_t>& signature
    );
    
    /**
     * Converte chave pública DNSKEY RSA para EVP_PKEY (Story 3.6)
     * @param public_key Chave pública em formato DNSKEY (RFC 3110)
     * @return EVP_PKEY* (deve ser liberado com EVP_PKEY_free)
     * @throws std::runtime_error se conversão falhar
     */
    void* convertDNSKEYToRSA(const std::vector<uint8_t>& public_key);
    
    /**
     * Converte chave pública DNSKEY ECDSA para EVP_PKEY (Story 3.6)
     * @param public_key Chave pública em formato DNSKEY (RFC 6605)
     * @return EVP_PKEY* (deve ser liberado com EVP_PKEY_free)
     * @throws std::runtime_error se conversão falhar
     */
    void* convertDNSKEYToECDSA(const std::vector<uint8_t>& public_key);
    
    /**
     * Converte string para lowercase (DNS canonical form)
     * @param str String
     * @return String em lowercase
     */
    std::string toLowercase(const std::string& str) const;
    
    /**
     * Log de trace (se trace_enabled_)
     * @param message Mensagem a logar
     */
    void traceLog(const std::string& message) const;
    
    /**
     * Formata bytes em hex (primeiros N bytes)
     * @param data Bytes
     * @param max_bytes Máximo de bytes a mostrar
     * @return String hex
     */
    std::string formatHexShort(const std::vector<uint8_t>& data, size_t max_bytes = 16) const;
};

} // namespace dns_resolver

