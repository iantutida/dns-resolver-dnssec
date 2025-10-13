#pragma once

#include "dns_resolver/types.h"
#include <string>
#include <memory>

namespace dns_resolver {

/**
 * Cliente IPC para comunicação com cache daemon (Story 4.2)
 * 
 * Responsável por consultar o daemon de cache antes da resolução.
 * Implementa fallback elegante se cache estiver offline.
 */
class CacheClient {
public:
    /**
     * Construtor
     * @param socket_path Caminho do Unix socket
     */
    explicit CacheClient(const std::string& socket_path = "/tmp/dns_cache.sock");
    
    /**
     * Consulta o cache para um domínio
     * @param qname Nome de domínio
     * @param qtype Tipo de registro
     * @param qclass Classe (padrão: IN=1)
     * @return DNSMessage se HIT, nullptr se MISS ou cache offline
     */
    std::unique_ptr<DNSMessage> query(
        const std::string& qname,
        uint16_t qtype,
        uint16_t qclass = DNSClass::IN
    );
    
    /**
     * Armazena resposta no cache (Story 4.3)
     * @param response Resposta DNS a armazenar
     * @param qname Nome de domínio original
     * @param qtype Tipo de registro original
     * @return true se armazenado com sucesso
     */
    bool store(
        const DNSMessage& response,
        const std::string& qname,
        uint16_t qtype
    );
    
    /**
     * Armazena resposta negativa no cache (Story 4.4)
     * @param qname Nome de domínio
     * @param qtype Tipo de registro
     * @param rcode RCODE (3=NXDOMAIN, 0=NODATA)
     * @param ttl TTL do SOA MINIMUM
     * @return true se armazenado com sucesso
     */
    bool storeNegative(
        const std::string& qname,
        uint16_t qtype,
        uint8_t rcode,
        uint32_t ttl
    );
    
    /**
     * Verifica se cache está disponível
     * @return true se daemon responde
     */
    bool isAvailable() const;
    
    /**
     * Habilita/desabilita trace logs
     */
    void setTraceEnabled(bool enabled);

private:
    std::string socket_path_;
    mutable bool daemon_available_ = true;  // Assume disponível até falha
    bool trace_enabled_ = false;
    
    /**
     * Conecta ao daemon via Unix socket
     * @param sockfd Socket descriptor (output)
     * @param timeout_ms Timeout em milissegundos
     * @return true se conectou com sucesso
     */
    bool connectToCache(int& sockfd, int timeout_ms = 1000);
    
    /**
     * Envia comando ao daemon
     * @param sockfd Socket descriptor
     * @param command Comando a enviar
     * @return true se enviou com sucesso
     */
    bool sendCommand(int sockfd, const std::string& command);
    
    /**
     * Recebe resposta do daemon
     * @param sockfd Socket descriptor
     * @return String com resposta (vazia se erro)
     */
    std::string receiveResponse(int sockfd);
    
    /**
     * Parseia resposta HIT do daemon
     * @param response String com dados serializados
     * @return DNSMessage parseado ou nullptr se erro
     */
    std::unique_ptr<DNSMessage> parseHitResponse(const std::string& response);
    
    /**
     * Serializa DNSMessage para formato IPC (Story 4.3)
     * @param msg Mensagem DNS
     * @return String serializado
     */
    std::string serializeForCache(const DNSMessage& msg) const;
    
    /**
     * Deserializa DNSMessage do formato IPC (Story 4.3)
     * @param data String serializado
     * @return DNSMessage parseado
     */
    DNSMessage deserializeFromCache(const std::string& data) const;
    
    /**
     * Log de trace (se enabled)
     */
    void traceLog(const std::string& message) const;
};

} // namespace dns_resolver

