#pragma once

#include "dns_resolver/types.h"
#include <map>
#include <mutex>
#include <string>
#include <ctime>

namespace dns_cache {

/**
 * Entrada de cache com timestamp e TTL
 */
struct CacheEntry {
    dns_resolver::DNSMessage response;
    time_t timestamp;
    uint32_t ttl;
    
    CacheEntry() : timestamp(0), ttl(0) {}
    
    CacheEntry(const dns_resolver::DNSMessage& resp, uint32_t t)
        : response(resp), timestamp(std::time(nullptr)), ttl(t) {}
    
    /**
     * Verifica se entrada expirou
     * @return true se expirada
     */
    bool isExpired() const {
        return (std::time(nullptr) - timestamp) > static_cast<time_t>(ttl);
    }
    
    /**
     * Retorna tempo restante de vida em segundos
     * @return TTL restante (0 se expirado)
     */
    uint32_t getRemainingTTL() const {
        time_t elapsed = std::time(nullptr) - timestamp;
        if (elapsed >= static_cast<time_t>(ttl)) {
            return 0;
        }
        return ttl - static_cast<uint32_t>(elapsed);
    }
};

/**
 * Daemon de cache DNS (Story 4.1)
 * 
 * Roda em background e gerencia cache de respostas DNS.
 * Comunicação via Unix Domain Socket (/tmp/dns_cache.sock).
 */
class CacheDaemon {
public:
    /**
     * Construtor
     */
    CacheDaemon();
    
    /**
     * Destrutor - limpa recursos
     */
    ~CacheDaemon();
    
    /**
     * Inicia o loop principal do daemon
     * Bloqueia até receber sinal de parada
     */
    void run();
    
    /**
     * Para o daemon gracefully
     */
    void stop();
    
    /**
     * Configura tamanho máximo do cache positivo
     * @param size Número máximo de entradas
     */
    void setMaxPositiveEntries(size_t size);
    
    /**
     * Configura tamanho máximo do cache negativo
     * @param size Número máximo de entradas
     */
    void setMaxNegativeEntries(size_t size);
    
    /**
     * Limpa cache positivo
     * @return Número de entradas removidas
     */
    size_t purgePositiveCache();
    
    /**
     * Limpa cache negativo
     * @return Número de entradas removidas
     */
    size_t purgeNegativeCache();
    
    /**
     * Limpa todo o cache
     * @return Número total de entradas removidas
     */
    size_t flushAll();
    
    /**
     * Retorna número de entradas no cache positivo
     */
    size_t getPositiveCacheSize() const;
    
    /**
     * Retorna número de entradas no cache negativo
     */
    size_t getNegativeCacheSize() const;

private:
    /**
     * Cria Unix Domain Socket
     * @return Socket descriptor ou -1 em caso de erro
     */
    int createSocket();
    
    /**
     * Processa conexão de cliente
     * @param client_socket Socket do cliente
     */
    void handleClient(int client_socket);
    
    /**
     * Processa comando recebido
     * @param command Comando a processar
     * @return Resposta a enviar ao cliente
     */
    std::string processCommand(const std::string& command);
    
    /**
     * Remove entradas expiradas
     */
    void cleanupExpiredEntries();
    
    /**
     * Adiciona entrada ao cache positivo com política LRU (Story 4.3)
     * @param question Question DNS
     * @param entry Entrada de cache
     */
    void addToCachePositive(
        const dns_resolver::DNSQuestion& question,
        const CacheEntry& entry
    );
    
    /**
     * Serializa DNSMessage para IPC (Story 4.3)
     */
    std::string serializeMessage(const dns_resolver::DNSMessage& msg) const;
    
    /**
     * Deserializa DNSMessage do IPC (Story 4.3)
     */
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

