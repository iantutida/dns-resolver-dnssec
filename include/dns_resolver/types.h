/*
 * ----------------------------------------
 * Arquivo: types.h
 * Propósito: Definições de estruturas e tipos DNS fundamentais
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dns_resolver {

// Cabeçalho DNS padrão (12 bytes)
struct DNSHeader {
    uint16_t id;          // ID da transação
    
    // Flags do cabeçalho DNS
    bool qr;              // Query/Response (0=query, 1=response)
    uint8_t opcode;       // Código de operação (4 bits)
    bool aa;              // Authoritative Answer
    bool tc;              // Truncation flag
    bool rd;              // Recursion Desired
    bool ra;              // Recursion Available
    uint8_t z;            // Reservado (3 bits)
    bool ad;              // Authenticated Data (DNSSEC)
    uint8_t rcode;        // Response code (4 bits)
    
    // Contadores de seções
    uint16_t qdcount;     // Número de questions
    uint16_t ancount;     // Número de answer RRs
    uint16_t nscount;     // Número de authority RRs
    uint16_t arcount;     // Número de additional RRs
    
    DNSHeader()
        : id(0), qr(false), opcode(0), aa(false), tc(false),
          rd(false), ra(false), z(0), ad(false), rcode(0),
          qdcount(0), ancount(0), nscount(0), arcount(0) {}
};

// Estrutura de uma pergunta DNS
struct DNSQuestion {
    std::string qname;    // Nome de domínio
    uint16_t qtype;       // Tipo de registro (A, AAAA, etc)
    uint16_t qclass;      // Classe (IN = Internet)
    
    DNSQuestion() : qtype(0), qclass(0) {}
    
    DNSQuestion(const std::string& name, uint16_t type, uint16_t cls)
        : qname(name), qtype(type), qclass(cls) {}
    
    // Operador para uso em std::map (cache)
    bool operator<(const DNSQuestion& other) const {
        if (qname != other.qname) return qname < other.qname;
        if (qtype != other.qtype) return qtype < other.qtype;
        return qclass < other.qclass;
    }
};

// Estrutura para dados SOA (Start of Authority)
struct SOARecord {
    std::string mname;      // Primary nameserver
    std::string rname;      // Admin email (com . no lugar de @)
    uint32_t serial;        // Serial number
    uint32_t refresh;       // Refresh interval
    uint32_t retry;         // Retry interval
    uint32_t expire;        // Expire time
    uint32_t minimum;       // Minimum TTL
    
    SOARecord()
        : serial(0), refresh(0), retry(0), expire(0), minimum(0) {}
};

// Estrutura para DNSKEY record (DNSSEC)
struct DNSKEYRecord {
    uint16_t flags;                  // 256 (ZSK) ou 257 (KSK)
    uint8_t protocol;                // Sempre 3 para DNSSEC
    uint8_t algorithm;               // 8 (RSA/SHA-256), 13 (ECDSA)
    std::vector<uint8_t> public_key; // Chave pública
    
    DNSKEYRecord() : flags(0), protocol(0), algorithm(0) {}
    
    // Verifica se é Key Signing Key
    bool isKSK() const { return (flags & 0x0001) != 0; }
    bool isZSK() const { return !isKSK(); }
};

// Estrutura para DS record (Delegation Signer)
struct DSRecord {
    uint16_t key_tag;              // ID da DNSKEY referenciada
    uint8_t algorithm;             // Algoritmo da DNSKEY
    uint8_t digest_type;           // 1 (SHA-1) ou 2 (SHA-256)
    std::vector<uint8_t> digest;   // Hash da DNSKEY
    
    DSRecord() : key_tag(0), algorithm(0), digest_type(0) {}
};

// Estrutura para opções EDNS0
struct EDNSOptions {
    uint16_t udp_size = 4096;      // Tamanho máximo de resposta UDP
    uint8_t version = 0;            // EDNS version
    bool dnssec_ok = false;         // DO bit (DNSSEC OK)
};

// Estrutura para RRSIG record (assinatura DNSSEC)
struct RRSIGRecord {
    uint16_t type_covered;         // Tipo do RRset assinado
    uint8_t algorithm;             // 8 (RSA/SHA-256), 13 (ECDSA P-256)
    uint8_t labels;                // Número de labels no owner name
    uint32_t original_ttl;         // TTL original do RRset
    uint32_t signature_expiration; // Unix timestamp
    uint32_t signature_inception;  // Unix timestamp
    uint16_t key_tag;              // ID da DNSKEY usada
    std::string signer_name;       // Nome da zona assinante
    std::vector<uint8_t> signature;  // Assinatura criptográfica
    
    RRSIGRecord() 
        : type_covered(0), algorithm(0), labels(0), original_ttl(0),
          signature_expiration(0), signature_inception(0), key_tag(0) {}
};

// Estrutura de um Resource Record DNS
struct DNSResourceRecord {
    std::string name;
    uint16_t type;
    uint16_t rr_class;
    uint32_t ttl;
    uint16_t rdlength;
    std::vector<uint8_t> rdata;  // Dados brutos
    
    // Campos parsed específicos por tipo
    std::string rdata_a;         // Tipo A: endereço IPv4
    std::string rdata_aaaa;      // Tipo AAAA: endereço IPv6
    std::string rdata_ns;        // Tipo NS: nameserver
    std::string rdata_cname;     // Tipo CNAME: canonical name
    std::string rdata_ptr;       // Tipo PTR: pointer
    std::string rdata_mx;        // Tipo MX: mail exchange
    std::string rdata_txt;       // Tipo TXT: texto
    SOARecord rdata_soa;         // Tipo SOA: start of authority
    DNSKEYRecord rdata_dnskey;   // Tipo DNSKEY: chave pública DNSSEC
    DSRecord rdata_ds;           // Tipo DS: delegation signer
    RRSIGRecord rdata_rrsig;     // Tipo RRSIG: assinatura
    
    DNSResourceRecord()
        : type(0), rr_class(0), ttl(0), rdlength(0) {}
};

// Estrutura completa de uma mensagem DNS
struct DNSMessage {
    DNSHeader header;
    std::vector<DNSQuestion> questions;
    std::vector<DNSResourceRecord> answers;
    std::vector<DNSResourceRecord> authority;
    std::vector<DNSResourceRecord> additional;
    
    // Suporte EDNS0
    bool use_edns = false;          // Se true, adiciona OPT RR
    EDNSOptions edns;                // Opções EDNS0
    
    DNSMessage() {}
};

// Constantes DNS comuns
namespace DNSType {
    constexpr uint16_t A = 1;        // IPv4 address
    constexpr uint16_t NS = 2;       // Name server
    constexpr uint16_t CNAME = 5;    // Canonical name
    constexpr uint16_t SOA = 6;      // Start of authority
    constexpr uint16_t PTR = 12;     // Pointer record
    constexpr uint16_t MX = 15;      // Mail exchange
    constexpr uint16_t TXT = 16;     // Text record
    constexpr uint16_t AAAA = 28;    // IPv6 address
    constexpr uint16_t OPT = 41;     // EDNS0 OPT pseudo-RR
    constexpr uint16_t DS = 43;      // Delegation Signer
    constexpr uint16_t RRSIG = 46;   // RRSIG Signature
    constexpr uint16_t DNSKEY = 48;  // DNS Key
}

namespace DNSClass {
    constexpr uint16_t IN = 1;       // Internet
}

namespace DNSOpcode {
    constexpr uint8_t QUERY = 0;     // Standard query
}

namespace DNSRCode {
    constexpr uint8_t NO_ERROR = 0;  // No error
    constexpr uint8_t FORMAT_ERROR = 1;
    constexpr uint8_t SERVER_FAILURE = 2;
    constexpr uint8_t NAME_ERROR = 3;
}

// Resultado de validação DNSSEC
enum class ValidationResult {
    Secure,         // Cadeia DNSSEC válida
    Insecure,       // Zona não tem DNSSEC (aceitável)
    Bogus,          // DNSSEC presente mas inválido
    Indeterminate   // Não foi possível validar
};

} // namespace dns_resolver

