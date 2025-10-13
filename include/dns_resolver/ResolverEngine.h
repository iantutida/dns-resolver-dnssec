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

/**
 * Modo de comunicação DNS (STORY 2.1, 2.2)
 */
enum class QueryMode {
    UDP,   // UDP tradicional (padrão, rápido)
    TCP,   // TCP (fallback ou forçado)
    DoT    // DNS over TLS (porta 853, criptografado)
};

/**
 * Configuração do ResolverEngine
 */
struct ResolverConfig {
    std::vector<std::string> root_servers;  // Lista de root servers IPv4
    int max_iterations = 15;                // Máximo de iterações por resolução
    int timeout_seconds = 5;                // Timeout por query UDP
    bool trace_mode = false;                // Modo debug (similar a dig +trace)
    bool force_tcp = false;                 // Forçar uso de TCP (Story 2.1)
    QueryMode mode = QueryMode::UDP;        // Modo de comunicação (Story 2.2)
    std::string default_sni;                // SNI para DoT (Story 2.2)
    std::string trust_anchor_file;          // Arquivo trust anchor (Story 3.1)
    bool dnssec_enabled = false;            // Ativar validação DNSSEC (Story 3.2)
    bool quiet_mode = false;                // Modo quiet (Story 5.1 - suprimir logs)
    
    ResolverConfig() {
        // Root servers padrão (primeiros 5 dos 13 root servers)
        root_servers = {
            "198.41.0.4",      // a.root-servers.net
            "199.9.14.201",    // b.root-servers.net
            "192.33.4.12",     // c.root-servers.net
            "199.7.91.13",     // d.root-servers.net
            "192.203.230.10"   // e.root-servers.net
        };
    }
};

/**
 * Motor de resolução DNS com suporte a resolução iterativa
 * 
 * Implementa o algoritmo de resolução DNS seguindo delegações desde
 * root servers até servidores autoritativos (RFC 1034 §5.3.3)
 */
class ResolverEngine {
public:
    /**
     * Construtor
     * @param config Configuração do resolver (root servers, timeouts, etc)
     */
    explicit ResolverEngine(const ResolverConfig& config);
    
    /**
     * Resolve um domínio começando dos root servers
     * @param domain Nome de domínio a resolver (ex: "google.com")
     * @param qtype Tipo de registro DNS (1=A, 2=NS, etc)
     * @return Mensagem DNS com a resposta final
     * @throws std::runtime_error se resolução falhar
     */
    DNSMessage resolve(const std::string& domain, uint16_t qtype);
    
    // ========== MÉTODOS PARA RESPOSTAS NEGATIVAS (STORY 1.5) ==========
    
    /**
     * Verifica se resposta é NXDOMAIN (domínio não existe)
     * @param response Resposta DNS
     * @return true se RCODE=3 (NXDOMAIN)
     */
    bool isNXDOMAIN(const DNSMessage& response) const;
    
    /**
     * Verifica se resposta é NODATA (domínio existe, mas sem registros do tipo)
     * @param response Resposta DNS
     * @param qtype Tipo de registro solicitado
     * @return true se RCODE=0, ANSWER vazio, e não é delegação
     */
    bool isNODATA(const DNSMessage& response, uint16_t qtype) const;
    
    /**
     * Extrai registro SOA da seção AUTHORITY
     * @param response Resposta DNS
     * @return ResourceRecord do tipo SOA (type=0 se não encontrado)
     */
    DNSResourceRecord extractSOA(const DNSMessage& response) const;
    
    // ========== MÉTODOS PARA TCP FALLBACK (STORY 2.1) ==========
    
    /**
     * Verifica se resposta está truncada (bit TC=1)
     * @param response Resposta DNS
     * @return true se TC=1 (resposta truncada, deve usar TCP)
     */
    bool isTruncated(const DNSMessage& response) const;
    
private:
    /**
     * Algoritmo de resolução iterativa (coração do resolver)
     * @param domain Nome de domínio
     * @param qtype Tipo de registro
     * @param initial_server Servidor inicial (normalmente um root server)
     * @param depth Profundidade de recursão (proteção contra loops)
     * @return Mensagem DNS com resposta
     * @throws std::runtime_error em caso de erro
     */
    DNSMessage performIterativeLookup(
        const std::string& domain,
        uint16_t qtype,
        const std::string& initial_server,
        int depth = 0
    );
    
    /**
     * Verifica se uma resposta é uma delegação
     * @param response Resposta DNS recebida
     * @return true se é delegação (ANSWER vazio, AUTHORITY com NS)
     */
    bool isDelegation(const DNSMessage& response) const;
    
    /**
     * Extrai nameservers da seção AUTHORITY
     * @param response Resposta DNS
     * @return Lista de nomes de nameservers (ex: ["ns1.google.com", ...])
     */
    std::vector<std::string> extractNameservers(const DNSMessage& response) const;
    
    /**
     * Extrai glue records da seção ADDITIONAL
     * @param response Resposta DNS
     * @return Mapa nameserver → IP (ex: {"ns1.google.com": "216.239.32.10"})
     */
    std::map<std::string, std::string> extractGlueRecords(const DNSMessage& response) const;
    
    /**
     * Resolve um nameserver sem glue record
     * @param ns_name Nome do nameserver (ex: "ns1.google.com")
     * @param depth Profundidade atual de recursão
     * @return IP do nameserver
     * @throws std::runtime_error se não conseguir resolver
     */
    std::string resolveNameserver(const std::string& ns_name, int depth);
    
    /**
     * Seleciona próximo servidor da lista de nameservers
     * @param nameservers Lista de nameservers
     * @param glue_records Mapa de glue records disponíveis
     * @param depth Profundidade de recursão
     * @return IP do próximo servidor a consultar
     * @throws std::runtime_error se nenhum servidor disponível
     */
    std::string selectNextServer(
        const std::vector<std::string>& nameservers,
        const std::map<std::string, std::string>& glue_records,
        int depth
    );
    
    /**
     * Log de trace (similar a dig +trace)
     * @param message Mensagem a logar
     */
    void traceLog(const std::string& message) const;
    
    /**
     * Gera um transaction ID aleatório
     * @return ID de 16 bits
     */
    uint16_t generateTransactionID() const;
    
    /**
     * Envia uma query DNS e retorna a resposta
     * @param server IP do servidor
     * @param domain Domínio a consultar
     * @param qtype Tipo de registro
     * @return Resposta DNS parseada
     * @throws std::runtime_error em caso de erro de rede
     */
    DNSMessage queryServer(
        const std::string& server,
        const std::string& domain,
        uint16_t qtype
    );
    
    // ========== MÉTODOS PARA CNAME (STORY 1.4) ==========
    
    /**
     * Segue cadeia de CNAMEs até obter registro do tipo solicitado
     * @param initial_response Resposta inicial contendo CNAME
     * @param original_query Nome de domínio original da query
     * @param qtype Tipo de registro solicitado
     * @param depth Profundidade atual da cadeia CNAME
     * @return Mensagem DNS com todos CNAMEs + registro final
     * @throws std::runtime_error se cadeia for muito longa ou houver loop
     */
    DNSMessage followCNAME(
        const DNSMessage& initial_response,
        const std::string& original_query,
        uint16_t qtype,
        int depth
    );
    
    /**
     * Verifica se resposta contém CNAME que precisa ser seguido
     * @param response Resposta DNS
     * @param qtype Tipo de registro solicitado
     * @return true se há CNAME e não há registro do tipo solicitado
     */
    bool hasCNAME(const DNSMessage& response, uint16_t qtype) const;
    
    /**
     * Extrai o canonical name do primeiro registro CNAME
     * @param response Resposta DNS
     * @return Nome canônico (rdata_cname)
     */
    std::string extractCNAME(const DNSMessage& response) const;
    
    /**
     * Verifica se resposta contém registro do tipo solicitado
     * @param response Resposta DNS
     * @param qtype Tipo de registro
     * @return true se há pelo menos um registro do tipo
     */
    bool hasTargetType(const DNSMessage& response, uint16_t qtype) const;
    
    /**
     * Coleta DNSKEY para uma zona (Story 3.3)
     * @param zone Nome da zona
     * @param server Servidor authoritative da zona
     */
    void collectDNSKEY(const std::string& zone, const std::string& server);
    
    /**
     * Coleta DS para uma zona (Story 3.3)
     * @param zone Nome da zona
     * @param server Servidor que tem DS para essa zona (zona pai)
     */
    void collectDS(const std::string& zone, const std::string& server);
    
    // Constantes
    static const int MAX_CNAME_DEPTH = 10;  // Limite de saltos CNAME
    
    // Membros
    ResolverConfig config_;
    
    // Cache de servidores consultados (proteção contra loops)
    std::set<std::string> queried_servers_;
    
    // Trust anchors para validação DNSSEC (Story 3.1)
    TrustAnchorStore trust_anchors_;
    
    // STORY 3.3: Validador e coleta de registros DNSSEC
    std::map<std::string, std::vector<DNSKEYRecord>> collected_dnskeys_;
    std::map<std::string, std::vector<DSRecord>> collected_ds_;
    
    // STORY 4.2: Cliente de cache (IPC)
    CacheClient cache_client_;
};

} // namespace dns_resolver

