/*
 * ----------------------------------------
 * Arquivo: CacheClient.h
 * Propósito: Cliente IPC para comunicação com daemon de cache DNS distribuído
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#pragma once

#include "dns_resolver/types.h"
#include <string>
#include <memory>

namespace dns_resolver {

// Cliente IPC para comunicação com cache daemon
// Responsável por consultar o daemon de cache antes da resolução
// Implementa fallback elegante se cache estiver offline
class CacheClient {
public:
    explicit CacheClient(const std::string& socket_path = "/tmp/dns_cache.sock");
    
    // Consulta o cache para um domínio
    std::unique_ptr<DNSMessage> query(
        const std::string& qname,
        uint16_t qtype,
        uint16_t qclass = DNSClass::IN
    );
    
    // Armazena resposta no cache
    bool store(
        const DNSMessage& response,
        const std::string& qname,
        uint16_t qtype
    );
    
    // Armazena resposta negativa no cache
    bool storeNegative(
        const std::string& qname,
        uint16_t qtype,
        uint8_t rcode,
        uint32_t ttl
    );
    
    // Verifica se cache está disponível
    bool isAvailable() const;
    
    // Habilita/desabilita trace logs
    void setTraceEnabled(bool enabled);

private:
    std::string socket_path_;
    mutable bool daemon_available_ = true;  // Assume disponível até falha
    bool trace_enabled_ = false;
    
    // Conecta ao daemon via Unix socket
    bool connectToCache(int& sockfd, int timeout_ms = 1000);
    
    // Envia comando ao daemon
    bool sendCommand(int sockfd, const std::string& command);
    
    // Recebe resposta do daemon
    std::string receiveResponse(int sockfd);
    
    // Parseia resposta HIT do daemon
    std::unique_ptr<DNSMessage> parseHitResponse(const std::string& response);
    
    // Serializa DNSMessage para formato IPC
    std::string serializeForCache(const DNSMessage& msg) const;
    
    // Deserializa DNSMessage do formato IPC
    DNSMessage deserializeFromCache(const std::string& data) const;
    
    // Log de trace (se enabled)
    void traceLog(const std::string& message) const;
};

} // namespace dns_resolver

