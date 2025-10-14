/*
 * ----------------------------------------
 * Arquivo: CacheClient.cpp
 * Propósito: Implementação do cliente IPC para comunicação com daemon de cache DNS
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#include "dns_resolver/CacheClient.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>

namespace dns_resolver {

CacheClient::CacheClient(const std::string& socket_path)
    : socket_path_(socket_path) {
}

void CacheClient::setTraceEnabled(bool enabled) {
    trace_enabled_ = enabled;
}

std::unique_ptr<DNSMessage> CacheClient::query(
    const std::string& qname,
    uint16_t qtype,
    uint16_t qclass
) {
    // Se cache já foi detectado como indisponível, não tentar
    if (!daemon_available_) {
        return nullptr;
    }
    
    traceLog("Querying cache for " + qname + " (type " + std::to_string(qtype) + ")...");
    
    // Conectar ao daemon
    int sockfd;
    if (!connectToCache(sockfd, 1000)) {
        daemon_available_ = false;
        traceLog("⚠️  Cache daemon unavailable (will use full resolution)");
        return nullptr;
    }
    
    // RAII para socket
    struct SocketGuard {
        int fd;
        ~SocketGuard() { if (fd >= 0) close(fd); }
    } guard{sockfd};
    
    // Construir comando QUERY
    std::ostringstream oss;
    oss << "QUERY|" << qname << "|" << qtype << "|" << qclass << "\n";
    std::string command = oss.str();
    
    // Enviar comando
    if (!sendCommand(sockfd, command)) {
        traceLog("⚠️  Failed to send query to cache");
        return nullptr;
    }
    
    // Receber resposta
    std::string response = receiveResponse(sockfd);
    
    if (response.empty()) {
        traceLog("⚠️  Empty response from cache");
        return nullptr;
    }
    
    // Parsear resposta
    // Verificar se é MISS (primeiros 4 caracteres)
    if (response.size() >= 4 && response.substr(0, 4) == "MISS") {
        traceLog("Cache MISS - proceeding with full resolution");
        return nullptr;
    }
    
    // Verificar se é NEGATIVE (resposta negativa cacheada)
    if (response.size() >= 8 && response.substr(0, 8) == "NEGATIVE") {
        // Formato: NEGATIVE|rcode
        size_t delimiter_pos = response.find('|');
        if (delimiter_pos != std::string::npos) {
            std::string rcode_str = response.substr(delimiter_pos + 1);
            uint8_t rcode = std::stoi(rcode_str);
            
            std::string type = (rcode == 3) ? "NXDOMAIN" : "NODATA";
            traceLog("✅ Cache HIT (NEGATIVE): " + type);
            
            // Criar DNSMessage com resposta negativa
            auto negative_msg = std::make_unique<DNSMessage>();
            negative_msg->header.qr = true;
            negative_msg->header.rcode = rcode;
            negative_msg->header.ancount = 0;
            negative_msg->header.qdcount = 1;
            
            return negative_msg;
        }
    }
    
    // Verificar se é HIT (resposta positiva)
    if (response.size() >= 3 && response.substr(0, 3) == "HIT") {
        traceLog("✅ Cache HIT");
        // Extrair dados após "HIT|"
        size_t delimiter_pos = response.find('|');
        if (delimiter_pos != std::string::npos) {
            std::string data = response.substr(delimiter_pos + 1);
            return parseHitResponse(data);
        }
    }
    
    // Resposta inesperada
    traceLog("⚠️  Unexpected cache response: " + response.substr(0, 20));
    return nullptr;
}

bool CacheClient::isAvailable() const {
    int sockfd;
    if (!const_cast<CacheClient*>(this)->connectToCache(sockfd, 500)) {
        return false;
    }
    close(sockfd);
    return true;
}

bool CacheClient::connectToCache(int& sockfd, int timeout_ms) {
    // Criar socket
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return false;
    }
    
    // Configurar timeout
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    // Preparar endereço
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    
    // Conectar
    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sockfd);
        return false;
    }
    
    return true;
}

bool CacheClient::sendCommand(int sockfd, const std::string& command) {
    ssize_t sent = send(sockfd, command.c_str(), command.size(), 0);
    return (sent == static_cast<ssize_t>(command.size()));
}

std::string CacheClient::receiveResponse(int sockfd) {
    char buffer[4096];
    ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    
    if (n <= 0) {
        return "";
    }
    
    buffer[n] = '\0';
    return std::string(buffer);
}

std::unique_ptr<DNSMessage> CacheClient::parseHitResponse(const std::string& response) {
    try {
        DNSMessage msg = deserializeFromCache(response);
        return std::make_unique<DNSMessage>(msg);
    } catch (const std::exception& e) {
        traceLog("⚠️  Failed to parse cached response: " + std::string(e.what()));
        return nullptr;
    }
}

// ========== SERIALIZAÇÃO ==========

std::string CacheClient::serializeForCache(const DNSMessage& msg) const {
    std::ostringstream oss;
    
    // Header básico
    oss << "rcode:" << static_cast<int>(msg.header.rcode) << ";";
    
    // Answers (formato: name|type|ttl|rdata##name|type|ttl|rdata)
    oss << "answers:";
    
    bool first = true;
    for (const auto& rr : msg.answers) {
        // Serializar apenas tipos suportados (pular RRSIG por enquanto)
        if (rr.type == DNSType::RRSIG || rr.type == DNSType::DNSKEY || 
            rr.type == DNSType::DS || rr.type == DNSType::OPT) {
            continue;  // Pular registros DNSSEC
        }
        
        if (!first) {
            oss << "##";
        }
        first = false;
        
        oss << rr.name << "|" << rr.type << "|" << rr.ttl << "|";
        
        // RDATA baseado no tipo
        if (rr.type == DNSType::A && !rr.rdata_a.empty()) {
            oss << rr.rdata_a;
        } else if (rr.type == DNSType::NS && !rr.rdata_ns.empty()) {
            oss << rr.rdata_ns;
        } else if (rr.type == DNSType::CNAME && !rr.rdata_cname.empty()) {
            oss << rr.rdata_cname;
        } else if (rr.type == DNSType::AAAA && !rr.rdata_aaaa.empty()) {
            oss << rr.rdata_aaaa;
        } else {
            // Outros tipos: armazenar vazio
            oss << "";
        }
    }
    
    return oss.str();
}

DNSMessage CacheClient::deserializeFromCache(const std::string& data) const {
    DNSMessage msg;
    
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
            msg.header.qr = true;  // É uma resposta
        } else if (key == "ancount") {
            // Apenas para validação
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
                    DNSResourceRecord rr;
                    rr.name = rr_parts[0];
                    rr.type = std::stoi(rr_parts[1]);
                    rr.ttl = std::stoi(rr_parts[2]);
                    rr.rr_class = DNSClass::IN;
                    
                    // RDATA baseado no tipo
                    if (rr.type == DNSType::A) {
                        rr.rdata_a = rr_parts[3];
                    } else if (rr.type == DNSType::NS) {
                        rr.rdata_ns = rr_parts[3];
                    } else if (rr.type == DNSType::CNAME) {
                        rr.rdata_cname = rr_parts[3];
                    } else if (rr.type == DNSType::AAAA) {
                        rr.rdata_aaaa = rr_parts[3];
                    }
                    
                    msg.answers.push_back(rr);
                }
            }
        }
    }
    
    msg.header.ancount = msg.answers.size();
    msg.header.qdcount = 1;  // Sempre 1 question
    
    return msg;
}

bool CacheClient::store(
    const DNSMessage& response,
    const std::string& qname,
    uint16_t qtype
) {
    // Se cache indisponível, não tentar
    if (!daemon_available_) {
        return false;
    }
    
    // Conectar ao daemon
    int sockfd;
    if (!connectToCache(sockfd, 1000)) {
        daemon_available_ = false;
        return false;
    }
    
    struct SocketGuard {
        int fd;
        ~SocketGuard() { if (fd >= 0) close(fd); }
    } guard{sockfd};
    
    // Calcular TTL (menor TTL dos answers)
    uint32_t ttl = 300;  // Default 5 minutos
    if (!response.answers.empty()) {
        ttl = response.answers[0].ttl;
        for (const auto& rr : response.answers) {
            if (rr.ttl < ttl) {
                ttl = rr.ttl;
            }
        }
    }
    
    // Serializar resposta
    std::string serialized = serializeForCache(response);
    
    // Construir comando STORE
    std::ostringstream oss;
    oss << "STORE|" << qname << "|" << qtype << "|1|" << ttl << "|" << serialized << "\n";
    std::string command = oss.str();
    
    // Enviar comando
    if (!sendCommand(sockfd, command)) {
        return false;
    }
    
    // Receber confirmação
    std::string resp = receiveResponse(sockfd);
    
    if (resp.substr(0, 2) == "OK") {
        traceLog("Response stored in cache (TTL: " + std::to_string(ttl) + "s)");
        return true;
    }
    
    return false;
}

// ========== CACHE NEGATIVO ==========

bool CacheClient::storeNegative(
    const std::string& qname,
    uint16_t qtype,
    uint8_t rcode,
    uint32_t ttl
) {
    // Se cache indisponível, não tentar
    if (!daemon_available_) {
        return false;
    }
    
    // Conectar ao daemon
    int sockfd;
    if (!connectToCache(sockfd, 1000)) {
        daemon_available_ = false;
        return false;
    }
    
    struct SocketGuard {
        int fd;
        ~SocketGuard() { if (fd >= 0) close(fd); }
    } guard{sockfd};
    
    // Construir comando STORE_NEGATIVE
    std::ostringstream oss;
    oss << "STORE_NEGATIVE|" << qname << "|" << qtype << "|1|" << ttl << "|" << static_cast<int>(rcode) << "\n";
    std::string command = oss.str();
    
    // Enviar comando
    if (!sendCommand(sockfd, command)) {
        return false;
    }
    
    // Receber confirmação
    std::string resp = receiveResponse(sockfd);
    
    if (resp.substr(0, 2) == "OK") {
        std::string type = (rcode == 3) ? "NXDOMAIN" : "NODATA";
        traceLog(type + " stored in cache (TTL: " + std::to_string(ttl) + "s)");
        return true;
    }
    
    return false;
}

void CacheClient::traceLog(const std::string& message) const {
    if (trace_enabled_) {
        std::cerr << ";; " << message << std::endl;
    }
}

} // namespace dns_resolver

