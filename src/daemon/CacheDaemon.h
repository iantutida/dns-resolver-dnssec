/*
 * ----------------------------------------
 * Arquivo: CacheDaemon.h
 * Propósito: Interface do daemon de cache DNS distribuído com IPC
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#pragma once

#include "dns_resolver/types.h"
#include <map>
#include <mutex>
#include <string>
#include <ctime>

namespace dns_cache {

// Entrada de cache com timestamp e TTL
struct CacheEntry {
    dns_resolver::DNSMessage response;
    time_t timestamp;
    uint32_t ttl;
    
    CacheEntry() : timestamp(0), ttl(0) {}
    
    CacheEntry(const dns_resolver::DNSMessage& resp, uint32_t t)
        : response(resp), timestamp(std::time(nullptr)), ttl(t) {}
    
    // Verifica se entrada expirou
    bool isExpired() const {
        return (std::time(nullptr) - timestamp) > static_cast<time_t>(ttl);
    }
    
    // Retorna tempo restante de vida em segundos
    uint32_t getRemainingTTL() const {
        time_t elapsed = std::time(nullptr) - timestamp;
        if (elapsed >= static_cast<time_t>(ttl)) {
            return 0;
        }
        return ttl - static_cast<uint32_t>(elapsed);
    }
};

// Daemon de cache DNS distribuído
// Roda em background e gerencia cache de respostas DNS
// Comunicação via Unix Domain Socket (/tmp/dns_cache.sock)
class CacheDaemon {
public:
    CacheDaemon();
    ~CacheDaemon();
    
    // Inicia o loop principal do daemon
    void run();
    
    // Para o daemon gracefully
    void stop();
    
    // Configura tamanho máximo do cache positivo
    void setMaxPositiveEntries(size_t size);
    
    // Configura tamanho máximo do cache negativo
    void setMaxNegativeEntries(size_t size);
    
    // Limpa cache positivo
    size_t purgePositiveCache();
    
    // Limpa cache negativo
    size_t purgeNegativeCache();
    
    // Limpa todo o cache
    size_t flushAll();
    
    // Retorna número de entradas no cache positivo
    size_t getPositiveCacheSize() const;
    
    // Retorna número de entradas no cache negativo
    size_t getNegativeCacheSize() const;

private:
    // Cria Unix Domain Socket
    int createSocket();
    
    // Processa conexão de cliente
    void handleClient(int client_socket);
    
    // Processa comando recebido
    std::string processCommand(const std::string& command);
    
    // Remove entradas expiradas
    void cleanupExpiredEntries();
    
    // Adiciona entrada ao cache positivo com política LRU
    void addToCachePositive(
        const dns_resolver::DNSQuestion& question,
        const CacheEntry& entry
    );
    
    // Serializa DNSMessage para IPC
    std::string serializeMessage(const dns_resolver::DNSMessage& msg) const;
    
    // Deserializa DNSMessage do IPC
    dns_resolver::DNSMessage deserializeMessage(const std::string& data) const;
    
    // Armazenamento
    std::map<dns_resolver::DNSQuestion, CacheEntry> positive_cache_;
    std::map<dns_resolver::DNSQuestion, CacheEntry> negative_cache_;
    
    // Thread-safety
    mutable std::mutex cache_mutex_;
    
    // Configuração
    size_t max_positive_entries_ = 50;
    size_t max_negative_entries_ = 50;
    
    // Estado do daemon
    bool running_ = false;
    int server_socket_ = -1;
    
    // Constantes
    static const char* SOCKET_PATH;
};

} // namespace dns_cache

