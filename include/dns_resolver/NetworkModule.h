/*
 * ----------------------------------------
 * Arquivo: NetworkModule.h
 * Propósito: Módulo de comunicação de rede para DNS (UDP, TCP, DoT)
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace dns_resolver {

// Módulo de rede responsável pela comunicação com servidores DNS
// Suporta UDP, TCP e DoT (DNS over TLS)
class NetworkModule {
public:
    // Envia uma query DNS via UDP e retorna a resposta
    static std::vector<uint8_t> queryUDP(
        const std::string& server,
        const std::vector<uint8_t>& query,
        int timeout_seconds = 5
    );
    
    // Envia uma query DNS via TCP (para respostas >512 bytes)
    static std::vector<uint8_t> queryTCP(
        const std::string& server,
        const std::vector<uint8_t>& query,
        int timeout_seconds = 10
    );
    
    // Envia uma query DNS via DoT - DNS over TLS (criptografado)
    static std::vector<uint8_t> queryDoT(
        const std::string& server,
        const std::vector<uint8_t>& query,
        const std::string& sni,
        int timeout_seconds = 15
    );

private:
    // Helpers TCP
    static std::vector<uint8_t> addTCPFraming(const std::vector<uint8_t>& message);
    static bool sendAll(int sockfd, const uint8_t* buffer, size_t length);
    static bool recvAll(int sockfd, uint8_t* buffer, size_t length);
    
    // Helpers TLS/DoT
    static void* createSSLContext();
    static bool validateCertificate(void* ssl, const std::string& expected_hostname);
    static std::string extractCertificateCN(void* cert);
    static std::vector<std::string> extractCertificateSAN(void* cert);
    
    // RAII wrapper para gerenciar file descriptors de sockets
    class SocketRAII {
    public:
        explicit SocketRAII(int fd);
        ~SocketRAII();
        
        // Não permite cópia
        SocketRAII(const SocketRAII&) = delete;
        SocketRAII& operator=(const SocketRAII&) = delete;
        
        // Permite movimentação
        SocketRAII(SocketRAII&& other) noexcept;
        SocketRAII& operator=(SocketRAII&& other) noexcept;
        
        int get() const { return fd_; }
        int release();
        
    private:
        int fd_;
    };
};

} // namespace dns_resolver

