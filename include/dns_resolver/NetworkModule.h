#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace dns_resolver {

/**
 * Módulo de rede responsável pela comunicação com servidores DNS
 * Suporta UDP, TCP e DoT (DNS over TLS)
 */
class NetworkModule {
public:
    /**
     * Envia uma query DNS via UDP e retorna a resposta
     * @param server Endereço IP do servidor DNS (ex: "8.8.8.8")
     * @param query Buffer com a query DNS serializada
     * @param timeout_seconds Timeout em segundos (padrão: 5)
     * @return Buffer com a resposta DNS
     * @throws std::runtime_error em caso de erro de rede ou timeout
     */
    static std::vector<uint8_t> queryUDP(
        const std::string& server,
        const std::vector<uint8_t>& query,
        int timeout_seconds = 5
    );
    
    /**
     * Envia uma query DNS via TCP (STORY 2.1)
     * Usado para respostas >512 bytes ou quando UDP está truncado
     * @param server Endereço IP do servidor DNS
     * @param query Buffer com a query DNS serializada
     * @param timeout_seconds Timeout em segundos (padrão: 10s, maior que UDP)
     * @return Buffer com a resposta DNS (sem framing)
     * @throws std::runtime_error em caso de erro de rede ou timeout
     */
    static std::vector<uint8_t> queryTCP(
        const std::string& server,
        const std::vector<uint8_t>& query,
        int timeout_seconds = 10
    );
    
    /**
     * Envia uma query DNS via DoT - DNS over TLS (STORY 2.2)
     * Criptografa a comunicação DNS usando TLS na porta 853
     * @param server Endereço IP do servidor DNS
     * @param query Buffer com a query DNS serializada
     * @param sni Server Name Indication para TLS (ex: "dns.google")
     * @param timeout_seconds Timeout em segundos (padrão: 15s)
     * @return Buffer com a resposta DNS (sem framing)
     * @throws std::runtime_error em caso de erro de rede, TLS ou validação
     */
    static std::vector<uint8_t> queryDoT(
        const std::string& server,
        const std::vector<uint8_t>& query,
        const std::string& sni,
        int timeout_seconds = 15
    );

private:
    // ========== HELPERS TCP (STORY 2.1) ==========
    
    /**
     * Adiciona framing TCP (2 bytes length prefix)
     * @param message Mensagem DNS sem framing
     * @return Mensagem com length prefix (big-endian)
     */
    static std::vector<uint8_t> addTCPFraming(const std::vector<uint8_t>& message);
    
    /**
     * Envia todos os bytes (loop até completar)
     * @param sockfd Socket file descriptor
     * @param buffer Buffer a enviar
     * @param length Número de bytes a enviar
     * @return true se sucesso, false se erro
     */
    static bool sendAll(int sockfd, const uint8_t* buffer, size_t length);
    
    /**
     * Recebe todos os bytes (loop até completar)
     * @param sockfd Socket file descriptor
     * @param buffer Buffer para receber
     * @param length Número de bytes a receber
     * @return true se sucesso, false se erro
     */
    static bool recvAll(int sockfd, uint8_t* buffer, size_t length);
    
    // ========== HELPERS TLS/DoT (STORY 2.2) ==========
    
    /**
     * Inicializa biblioteca OpenSSL e cria contexto SSL
     * @return Contexto SSL configurado
     * @throws std::runtime_error se inicialização falhar
     */
    static void* createSSLContext();
    
    /**
     * Valida certificado TLS do servidor
     * @param ssl Objeto SSL após handshake
     * @param expected_hostname Hostname esperado (SNI)
     * @return true se certificado válido
     */
    static bool validateCertificate(void* ssl, const std::string& expected_hostname);
    
    /**
     * Extrai Common Name (CN) do certificado
     * @param cert Certificado X509
     * @return CN do certificado
     */
    static std::string extractCertificateCN(void* cert);
    
    /**
     * Extrai Subject Alternative Names (SAN) do certificado
     * @param cert Certificado X509
     * @return Lista de SANs
     */
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

