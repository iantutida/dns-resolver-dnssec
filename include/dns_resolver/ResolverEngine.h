/*
 * ----------------------------------------
 * Arquivo: ResolverEngine.h
 * Propósito: Motor de resolução DNS iterativa com suporte a DNSSEC e cache
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#pragma once

#include "dns_resolver/types.h"
#include "dns_resolver/NetworkModule.h"
#include "dns_resolver/DNSParser.h"
#include "dns_resolver/TrustAnchorStore.h"
#include "dns_resolver/DNSSECValidator.h"
#include "dns_resolver/CacheClient.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace dns_resolver {

// Modo de comunicação DNS
enum class QueryMode {
    UDP,   // UDP tradicional (padrão)
    TCP,   // TCP (fallback ou forçado)
    DoT    // DNS over TLS (porta 853)
};

// Configuração do ResolverEngine
struct ResolverConfig {
    std::vector<std::string> root_servers;  // Lista de root servers IPv4
    int max_iterations = 15;                // Máximo de iterações por resolução
    int timeout_seconds = 5;                // Timeout por query UDP
    bool trace_mode = false;                // Modo debug
    bool force_tcp = false;                 // Forçar uso de TCP
    QueryMode mode = QueryMode::UDP;        // Modo de comunicação
    std::string default_sni;                // SNI para DoT
    std::string trust_anchor_file;          // Arquivo trust anchor
    bool dnssec_enabled = false;            // Ativar validação DNSSEC
    bool quiet_mode = false;                // Modo quiet
    bool fanout_enabled = false;            // Fan-out paralelo
    
    ResolverConfig() {
        // Root servers padrão
        root_servers = {
            "198.41.0.4",      // a.root-servers.net
            "199.9.14.201",    // b.root-servers.net
            "192.33.4.12",     // c.root-servers.net
            "199.7.91.13",     // d.root-servers.net
            "192.203.230.10"   // e.root-servers.net
        };
    }
};

// Motor de resolução DNS iterativa
// Implementa algoritmo de resolução DNS seguindo delegações desde
// root servers até servidores autoritativos
class ResolverEngine {
public:
    explicit ResolverEngine(const ResolverConfig& config);
    
    // Resolve um domínio começando dos root servers
    DNSMessage resolve(const std::string& domain, uint16_t qtype);
    
    // Métodos para respostas negativas
    bool isNXDOMAIN(const DNSMessage& response) const;
    bool isNODATA(const DNSMessage& response, uint16_t qtype) const;
    DNSResourceRecord extractSOA(const DNSMessage& response) const;
    
    // Métodos para TCP fallback
    bool isTruncated(const DNSMessage& response) const;
    
private:
    // Algoritmo de resolução iterativa (coração do resolver)
    DNSMessage performIterativeLookup(
        const std::string& domain,
        uint16_t qtype,
        const std::string& initial_server,
        int depth = 0
    );
    
    // Verifica se uma resposta é uma delegação
    bool isDelegation(const DNSMessage& response) const;
    
    // Extrai nameservers da seção AUTHORITY
    std::vector<std::string> extractNameservers(const DNSMessage& response) const;
    
    // Extrai glue records da seção ADDITIONAL
    std::map<std::string, std::string> extractGlueRecords(const DNSMessage& response) const;
    
    // Resolve um nameserver sem glue record
    std::string resolveNameserver(const std::string& ns_name, int depth);
    
    // Seleciona próximo servidor da lista de nameservers
    std::string selectNextServer(
        const std::vector<std::string>& nameservers,
        const std::map<std::string, std::string>& glue_records,
        int depth
    );
    
    // Log de trace (similar a dig +trace)
    void traceLog(const std::string& message) const;
    
    // Gera um transaction ID aleatório
    uint16_t generateTransactionID() const;
    
    // Envia uma query DNS e retorna a resposta
    DNSMessage queryServer(
        const std::string& server,
        const std::string& domain,
        uint16_t qtype
    );
    
    // Métodos para CNAME
    DNSMessage followCNAME(
        const DNSMessage& initial_response,
        const std::string& original_query,
        uint16_t qtype,
        int depth
    );
    
    bool hasCNAME(const DNSMessage& response, uint16_t qtype) const;
    std::string extractCNAME(const DNSMessage& response) const;
    bool hasTargetType(const DNSMessage& response, uint16_t qtype) const;
    
    // Coleta DNSKEY para uma zona
    void collectDNSKEY(const std::string& zone, const std::string& server);
    
    // Coleta DS para uma zona
    void collectDS(const std::string& zone, const std::string& server);
    
    // Consulta múltiplos servidores em paralelo (fan-out)
    DNSMessage queryServersFanout(
        const std::vector<std::string>& servers,
        const std::string& domain,
        uint16_t qtype
    );
    
    // Constantes
    static const int MAX_CNAME_DEPTH = 10;  // Limite de saltos CNAME
    
    // Membros
    ResolverConfig config_;
    
    // Cache de servidores consultados (proteção contra loops)
    std::set<std::string> queried_servers_;
    
    // Trust anchors para validação DNSSEC
    TrustAnchorStore trust_anchors_;
    
    // Validador e coleta de registros DNSSEC
    std::map<std::string, std::vector<DNSKEYRecord>> collected_dnskeys_;
    std::map<std::string, std::vector<DSRecord>> collected_ds_;
    
    // Cliente de cache (IPC)
    CacheClient cache_client_;
};

} // namespace dns_resolver

