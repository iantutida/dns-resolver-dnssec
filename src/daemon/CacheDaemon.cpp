/*
 * ----------------------------------------
 * Arquivo: CacheDaemon.cpp
 * Propósito: Implementação do daemon de cache DNS distribuído com IPC
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#include "CacheDaemon.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <cstring>

namespace dns_cache {

const char* CacheDaemon::SOCKET_PATH = "/tmp/dns_cache.sock";

CacheDaemon::CacheDaemon() {
    // Construtor vazio - inicialização lazy
}

CacheDaemon::~CacheDaemon() {
    stop();
}

void CacheDaemon::run() {
    running_ = true;
    
    // Criar Unix Domain Socket para IPC
    server_socket_ = createSocket();
    if (server_socket_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }
    
    std::cout << "Cache daemon started" << std::endl;
    std::cout << "Socket: " << SOCKET_PATH << std::endl;
    std::cout << "Positive cache: 0/" << max_positive_entries_ << std::endl;
    std::cout << "Negative cache: 0/" << max_negative_entries_ << std::endl;
    
    // Loop principal do daemon
    while (running_) {
        // Aceitar conexão de cliente
        int client_socket = accept(server_socket_, nullptr, nullptr);
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
        
        // Processar comando do cliente
        handleClient(client_socket);
        close(client_socket);
        
        // Limpeza periódica de entradas expiradas
        cleanupExpiredEntries();
    }
    
    // Cleanup final
    close(server_socket_);
    unlink(SOCKET_PATH);
}

void CacheDaemon::stop() {
    running_ = false;
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
}

int CacheDaemon::createSocket() {
    // Remover socket antigo se existir
    unlink(SOCKET_PATH);
    
    // Criar Unix Domain Socket (AF_UNIX)
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    // Configurar endereço do socket
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    // Bind do socket ao arquivo
    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    // Colocar socket em modo listen
    if (listen(sock, 5) < 0) {
        close(sock);
        unlink(SOCKET_PATH);
        return -1;
    }
    
    return sock;
}

void CacheDaemon::handleClient(int client_socket) {
    // Ler comando do cliente (buffer simples)
    char buffer[4096];
    ssize_t n = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (n <= 0) {
        return;
    }
    
    buffer[n] = '\0';
    std::string command(buffer);
    
    // Processar comando e gerar resposta
    std::string response = processCommand(command);
    
    // Enviar resposta ao cliente
    send(client_socket, response.c_str(), response.size(), 0);
}

std::string CacheDaemon::processCommand(const std::string& command) {
    // Parsear comando (formato: COMMAND|arg1|arg2 ou COMMAND arg1)
    size_t delimiter_pos = command.find('|');
    std::string cmd;
    std::istringstream iss(command);
    
    if (delimiter_pos != std::string::npos) {
        // Formato com | (ex: QUERY|domain|type)
        cmd = command.substr(0, delimiter_pos);
    } else {
        // Formato com espaço (ex: SET_POSITIVE 100)
        iss >> cmd;
    }
    
    // QUERY - consultar cache 
    if (cmd == "QUERY") {
        // Parsear: QUERY|qname|qtype|qclass
        size_t pos = 0;
        std::vector<std::string> parts;
        std::string temp = command;
        
        while ((pos = temp.find('|')) != std::string::npos) {
            parts.push_back(temp.substr(0, pos));
            temp = temp.substr(pos + 1);
        }
        parts.push_back(temp);  // Último campo
        
        if (parts.size() < 4) {
            return "ERROR|Invalid QUERY format\n";
        }
        
        dns_resolver::DNSQuestion question;
        question.qname = parts[1];
        question.qtype = std::stoi(parts[2]);
        question.qclass = std::stoi(parts[3]);
        
        // Buscar no cache com lock thread-safe
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // Verificar cache positivo primeiro
        auto pos_it = positive_cache_.find(question);
        if (pos_it != positive_cache_.end() && !pos_it->second.isExpired()) {
            // HIT positivo - serializar e retornar
            std::string serialized = serializeMessage(pos_it->second.response);
            return "HIT|" + serialized + "\n";
        }
        
        // Verificar cache negativo
        auto neg_it = negative_cache_.find(question);
        if (neg_it != negative_cache_.end() && !neg_it->second.isExpired()) {
            // HIT negativo - retornar RCODE
            uint8_t rcode = neg_it->second.response.header.rcode;
            return "NEGATIVE|" + std::to_string(static_cast<int>(rcode)) + "\n";
        }
        
        // MISS em ambos os caches
        return "MISS\n";
    }
    
    // STORE - armazenar resposta positiva 
    if (cmd == "STORE") {
        // Parsear: STORE|qname|qtype|qclass|ttl|serialized_data
        // Parsear apenas os primeiros 5 campos, o resto é serialized_data
        std::vector<std::string> parts;
        size_t pos = 0;
        std::string temp = command;
        
        // Extrair primeiros 5 campos (STORE, qname, qtype, qclass, ttl)
        for (int i = 0; i < 5; i++) {
            pos = temp.find('|');
            if (pos == std::string::npos) {
                return "ERROR|Invalid STORE format\n";
            }
            parts.push_back(temp.substr(0, pos));
            temp = temp.substr(pos + 1);
        }
        
        // O resto é serialized_data (pode conter |)
        std::string serialized_data = temp;
        
        dns_resolver::DNSQuestion question;
        question.qname = parts[1];
        question.qtype = std::stoi(parts[2]);
        question.qclass = std::stoi(parts[3]);
        
        uint32_t ttl = std::stoi(parts[4]);
        
        // Deserializar resposta DNS
        dns_resolver::DNSMessage response = deserializeMessage(serialized_data);
        
        // Criar entrada de cache
        CacheEntry entry(response, ttl);
        
        // Adicionar ao cache positivo (com política LRU)
        addToCachePositive(question, entry);
        
        return "OK|Stored\n";
    }
    
    // STORE_NEGATIVE - armazenar resposta negativa 
    if (cmd == "STORE_NEGATIVE") {
        // Parsear: STORE_NEGATIVE|qname|qtype|qclass|ttl|rcode
        std::vector<std::string> parts;
        size_t pos = 0;
        std::string temp = command;
        
        for (int i = 0; i < 6; i++) {
            pos = temp.find('|');
            if (pos == std::string::npos && i < 5) {
                return "ERROR|Invalid STORE_NEGATIVE format\n";
            }
            if (pos != std::string::npos) {
                parts.push_back(temp.substr(0, pos));
                temp = temp.substr(pos + 1);
            } else {
                parts.push_back(temp);
            }
        }
        
        if (parts.size() < 6) {
            return "ERROR|Invalid STORE_NEGATIVE format\n";
        }
        
        dns_resolver::DNSQuestion question;
        question.qname = parts[1];
        question.qtype = std::stoi(parts[2]);
        question.qclass = std::stoi(parts[3]);
        
        uint32_t ttl = std::stoi(parts[4]);
        uint8_t rcode = std::stoi(parts[5]);
        
        // Criar DNSMessage mínimo com RCODE
        dns_resolver::DNSMessage negative_response;
        negative_response.header.qr = true;
        negative_response.header.rcode = rcode;
        negative_response.header.ancount = 0;
        
        // Criar entrada de cache negativo
        CacheEntry entry(negative_response, ttl);
        
        // Adicionar ao cache negativo (com política LRU)
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            
            // Limpar entradas expiradas primeiro
            for (auto it = negative_cache_.begin(); it != negative_cache_.end(); ) {
                if (it->second.isExpired()) {
                    it = negative_cache_.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Aplicar política LRU se cache cheio
            if (negative_cache_.size() >= max_negative_entries_) {
                auto oldest = negative_cache_.begin();
                for (auto it = negative_cache_.begin(); it != negative_cache_.end(); ++it) {
                    if (it->second.timestamp < oldest->second.timestamp) {
                        oldest = it;
                    }
                }
                negative_cache_.erase(oldest);
            }
            
            // Adicionar nova entrada
            negative_cache_[question] = entry;
        }
        
        return "OK|Stored negative\n";
    }
    
    // FLUSH - limpar todo o cache
    if (cmd == "FLUSH") {
        size_t removed = flushAll();
        return "OK|Flushed " + std::to_string(removed) + " entries\n";
    }
    
    // SET_POSITIVE - configurar tamanho do cache positivo
    if (cmd == "SET_POSITIVE") {
        size_t size;
        iss >> size;
        setMaxPositiveEntries(size);
        return "OK|Positive cache size set to " + std::to_string(size) + "\n";
    }
    
    // SET_NEGATIVE - configurar tamanho do cache negativo
    if (cmd == "SET_NEGATIVE") {
        size_t size;
        iss >> size;
        setMaxNegativeEntries(size);
        return "OK|Negative cache size set to " + std::to_string(size) + "\n";
    }
    
    // PURGE - limpar cache específico
    if (cmd == "PURGE") {
        std::string type;
        iss >> type;
        
        if (type == "positive") {
            size_t removed = purgePositiveCache();
            return "OK|Purged " + std::to_string(removed) + " positive entries\n";
        } else if (type == "negative") {
            size_t removed = purgeNegativeCache();
            return "OK|Purged " + std::to_string(removed) + " negative entries\n";
        } else if (type == "all") {
            size_t removed = flushAll();
            return "OK|Purged " + std::to_string(removed) + " total entries\n";
        }
        
        return "ERROR|Invalid purge type\n";
    }
    
    // LIST - listar entradas do cache
    if (cmd == "LIST") {
        std::string type;
        iss >> type;
        
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        if (type == "positive") {
            std::ostringstream oss;
            oss << "OK|Positive cache: " << positive_cache_.size() 
                << "/" << max_positive_entries_ << " entries\n";
            return oss.str();
        } else if (type == "negative") {
            std::ostringstream oss;
            oss << "OK|Negative cache: " << negative_cache_.size() 
                << "/" << max_negative_entries_ << " entries\n";
            return oss.str();
        } else if (type == "all") {
            std::ostringstream oss;
            oss << "OK|Total: " << (positive_cache_.size() + negative_cache_.size())
                << " entries ("
                << positive_cache_.size() << " positive, "
                << negative_cache_.size() << " negative)\n";
            return oss.str();
        }
        
        return "ERROR|Invalid list type\n";
    }
    
    // STATUS - informações do daemon
    if (cmd == "STATUS") {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        std::ostringstream oss;
        oss << "OK|Cache Daemon Status\n";
        oss << "Positive: " << positive_cache_.size() << "/" << max_positive_entries_ << "\n";
        oss << "Negative: " << negative_cache_.size() << "/" << max_negative_entries_ << "\n";
        return oss.str();
    }
    
    return "ERROR|Unknown command\n";
}

void CacheDaemon::setMaxPositiveEntries(size_t size) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    max_positive_entries_ = size;
}

void CacheDaemon::setMaxNegativeEntries(size_t size) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    max_negative_entries_ = size;
}

size_t CacheDaemon::purgePositiveCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    size_t count = positive_cache_.size();
    positive_cache_.clear();
    return count;
}

size_t CacheDaemon::purgeNegativeCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    size_t count = negative_cache_.size();
    negative_cache_.clear();
    return count;
}

size_t CacheDaemon::flushAll() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    size_t count = positive_cache_.size() + negative_cache_.size();
    positive_cache_.clear();
    negative_cache_.clear();
    return count;
}

size_t CacheDaemon::getPositiveCacheSize() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return positive_cache_.size();
}

size_t CacheDaemon::getNegativeCacheSize() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return negative_cache_.size();
}

void CacheDaemon::cleanupExpiredEntries() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Limpar cache positivo expirado
    for (auto it = positive_cache_.begin(); it != positive_cache_.end(); ) {
        if (it->second.isExpired()) {
            it = positive_cache_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Limpar cache negativo expirado
    for (auto it = negative_cache_.begin(); it != negative_cache_.end(); ) {
        if (it->second.isExpired()) {
            it = negative_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

// ==========  ARMAZENAMENTO E SERIALIZAÇÃO ==========

void CacheDaemon::addToCachePositive(
    const dns_resolver::DNSQuestion& question,
    const CacheEntry& entry
) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Limpar entradas expiradas primeiro
    for (auto it = positive_cache_.begin(); it != positive_cache_.end(); ) {
        if (it->second.isExpired()) {
            it = positive_cache_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Verificar tamanho e aplicar política LRU
    if (positive_cache_.size() >= max_positive_entries_) {
        // Encontrar entrada mais antiga (menor timestamp)
        auto oldest = positive_cache_.begin();
        for (auto it = positive_cache_.begin(); it != positive_cache_.end(); ++it) {
            if (it->second.timestamp < oldest->second.timestamp) {
                oldest = it;
            }
        }
        positive_cache_.erase(oldest);
    }
    
    // Adicionar nova entrada
    positive_cache_[question] = entry;
}

std::string CacheDaemon::serializeMessage(const dns_resolver::DNSMessage& msg) const {
    std::ostringstream oss;
    
    // Serializar header básico
    oss << "rcode:" << static_cast<int>(msg.header.rcode) << ";";
    
    // Serializar answers (formato: name|type|ttl|rdata##name|type|ttl|rdata)
    oss << "answers:";
    
    bool first = true;
    for (const auto& rr : msg.answers) {
        // Serializar apenas tipos suportados (pular DNSSEC por enquanto)
        if (rr.type == dns_resolver::DNSType::RRSIG || 
            rr.type == dns_resolver::DNSType::DNSKEY || 
            rr.type == dns_resolver::DNSType::DS ||
            rr.type == dns_resolver::DNSType::OPT) {
            continue;  // Pular registros DNSSEC
        }
        
        if (!first) {
            oss << "##";
        }
        first = false;
        
        oss << rr.name << "|" << rr.type << "|" << rr.ttl << "|";
        
        // Serializar RDATA baseado no tipo
        if (rr.type == dns_resolver::DNSType::A && !rr.rdata_a.empty()) {
            oss << rr.rdata_a;
        } else if (rr.type == dns_resolver::DNSType::NS && !rr.rdata_ns.empty()) {
            oss << rr.rdata_ns;
        } else if (rr.type == dns_resolver::DNSType::CNAME && !rr.rdata_cname.empty()) {
            oss << rr.rdata_cname;
        } else if (rr.type == dns_resolver::DNSType::AAAA && !rr.rdata_aaaa.empty()) {
            oss << rr.rdata_aaaa;
        } else {
            oss << "";
        }
    }
    
    return oss.str();
}

dns_resolver::DNSMessage CacheDaemon::deserializeMessage(const std::string& data) const {
    dns_resolver::DNSMessage msg;
    
    // Remover newline se existir
    std::string clean_data = data;
    if (!clean_data.empty() && clean_data.back() == '\n') {
        clean_data.pop_back();
    }
    
    // Parsear campos separados por ;
    std::istringstream iss(clean_data);
    std::string field;
    
    while (std::getline(iss, field, ';')) {
        size_t colon_pos = field.find(':');
        if (colon_pos == std::string::npos) continue;
        
        std::string key = field.substr(0, colon_pos);
        std::string value = field.substr(colon_pos + 1);
        
        if (key == "rcode") {
            msg.header.rcode = std::stoi(value);
            msg.header.qr = true;
        } else if (key == "answers") {
            // Parsear answers (formato: name|type|ttl|rdata##name|type|ttl|rdata)
            std::istringstream ans_stream(value);
            std::string rr_str;
            
            while (std::getline(ans_stream, rr_str, '#')) {
                if (rr_str.empty() || rr_str == "#") continue;
                
                // Parsear RR individual (name|type|ttl|rdata)
                std::istringstream rr_stream(rr_str);
                std::string rr_field;
                std::vector<std::string> rr_parts;
                
                while (std::getline(rr_stream, rr_field, '|')) {
                    rr_parts.push_back(rr_field);
                }
                
                if (rr_parts.size() >= 4) {
                    dns_resolver::DNSResourceRecord rr;
                    rr.name = rr_parts[0];
                    rr.type = std::stoi(rr_parts[1]);
                    rr.ttl = std::stoi(rr_parts[2]);
                    rr.rr_class = dns_resolver::DNSClass::IN;
                    
                    // Deserializar RDATA baseado no tipo
                    if (rr.type == dns_resolver::DNSType::A) {
                        rr.rdata_a = rr_parts[3];
                    } else if (rr.type == dns_resolver::DNSType::NS) {
                        rr.rdata_ns = rr_parts[3];
                    } else if (rr.type == dns_resolver::DNSType::CNAME) {
                        rr.rdata_cname = rr_parts[3];
                    } else if (rr.type == dns_resolver::DNSType::AAAA) {
                        rr.rdata_aaaa = rr_parts[3];
                    }
                    
                    msg.answers.push_back(rr);
                }
            }
        }
    }
    
    // Atualizar contadores do header
    msg.header.ancount = msg.answers.size();
    msg.header.qdcount = 1;  // Sempre 1 question
    
    return msg;
}

} // namespace dns_cache

