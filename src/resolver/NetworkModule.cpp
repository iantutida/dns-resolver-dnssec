#include "dns_resolver/NetworkModule.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

namespace dns_resolver {

// ========== Implementação do SocketRAII ==========

NetworkModule::SocketRAII::SocketRAII(int fd) : fd_(fd) {}

NetworkModule::SocketRAII::~SocketRAII() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

NetworkModule::SocketRAII::SocketRAII(SocketRAII&& other) noexcept
    : fd_(other.fd_) {
    other.fd_ = -1;
}

NetworkModule::SocketRAII& NetworkModule::SocketRAII::operator=(SocketRAII&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) {
            close(fd_);
        }
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

int NetworkModule::SocketRAII::release() {
    int fd = fd_;
    fd_ = -1;
    return fd;
}

// ========== Implementação do queryUDP ==========

std::vector<uint8_t> NetworkModule::queryUDP(
    const std::string& server,
    const std::vector<uint8_t>& query,
    int timeout_seconds
) {
    // Validação de entrada
    if (server.empty()) {
        throw std::invalid_argument("Endereço do servidor vazio");
    }
    
    if (query.empty()) {
        throw std::invalid_argument("Query DNS vazia");
    }
    
    // Criar socket UDP
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error(
            std::string("Falha ao criar socket UDP: ") + strerror(errno)
        );
    }
    
    // RAII wrapper para garantir fechamento do socket
    SocketRAII socket_guard(sockfd);
    
    // Configurar timeout do socket
    struct timeval tv;
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        throw std::runtime_error(
            std::string("Falha ao configurar timeout do socket: ") + strerror(errno)
        );
    }
    
    // Preparar endereço do servidor DNS
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53); // Porta DNS padrão
    
    // Converter IP string para formato binário
    if (inet_pton(AF_INET, server.c_str(), &server_addr.sin_addr) <= 0) {
        throw std::invalid_argument("Endereço IP inválido: " + server);
    }
    
    // Enviar a query DNS
    ssize_t sent_bytes = sendto(
        sockfd,
        query.data(),
        query.size(),
        0,
        reinterpret_cast<struct sockaddr*>(&server_addr),
        sizeof(server_addr)
    );
    
    if (sent_bytes < 0) {
        throw std::runtime_error(
            std::string("Falha ao enviar query DNS: ") + strerror(errno)
        );
    }
    
    if (static_cast<size_t>(sent_bytes) != query.size()) {
        throw std::runtime_error(
            "Não foi possível enviar toda a query DNS (" +
            std::to_string(sent_bytes) + " de " + std::to_string(query.size()) + " bytes)"
        );
    }
    
    // ========== RECEBER RESPOSTA (STORY 1.2) ==========
    
    // Buffer para resposta
    // STORY 3.2: Com EDNS0, respostas podem ser até 4096 bytes
    std::vector<uint8_t> response(4096);
    socklen_t addr_len = sizeof(server_addr);
    
    ssize_t recv_bytes = recvfrom(
        sockfd,
        response.data(),
        response.size(),
        0,
        reinterpret_cast<struct sockaddr*>(&server_addr),
        &addr_len
    );
    
    if (recv_bytes < 0) {
        // Verificar se foi timeout
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            throw std::runtime_error(
                "Timeout ao aguardar resposta DNS (" + 
                std::to_string(timeout_seconds) + "s)"
            );
        }
        throw std::runtime_error(
            std::string("Falha ao receber resposta DNS: ") + strerror(errno)
        );
    }
    
    // Validação básica: resposta deve ter pelo menos 12 bytes (header)
    if (recv_bytes < 12) {
        throw std::runtime_error(
            "Resposta DNS muito pequena (" + std::to_string(recv_bytes) + " bytes, mínimo 12)"
        );
    }
    
    // Ajustar tamanho do vector ao número de bytes recebidos
    response.resize(static_cast<size_t>(recv_bytes));
    
    return response;
}

// ========== IMPLEMENTAÇÃO TCP (STORY 2.1) ==========

std::vector<uint8_t> NetworkModule::queryTCP(
    const std::string& server,
    const std::vector<uint8_t>& query,
    int timeout_seconds
) {
    // Validação de entrada
    if (server.empty()) {
        throw std::invalid_argument("Endereço do servidor vazio");
    }
    
    if (query.empty()) {
        throw std::invalid_argument("Query DNS vazia");
    }
    
    // 1. Criar socket TCP
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error(
            std::string("Falha ao criar socket TCP: ") + strerror(errno)
        );
    }
    
    // RAII wrapper para garantir fechamento do socket
    SocketRAII socket_guard(sockfd);
    
    // 2. Configurar timeouts (send e receive)
    struct timeval tv;
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        throw std::runtime_error(
            std::string("Falha ao configurar timeout de recepção TCP: ") + strerror(errno)
        );
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        throw std::runtime_error(
            std::string("Falha ao configurar timeout de envio TCP: ") + strerror(errno)
        );
    }
    
    // 3. Preparar endereço do servidor DNS
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53); // Porta DNS padrão
    
    // Converter IP string para formato binário
    if (inet_pton(AF_INET, server.c_str(), &server_addr.sin_addr) <= 0) {
        throw std::invalid_argument("Endereço IP inválido: " + server);
    }
    
    // 4. Conectar ao servidor
    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&server_addr), 
                sizeof(server_addr)) < 0) {
        throw std::runtime_error(
            std::string("Falha ao conectar TCP ao servidor ") + server + ": " + 
            strerror(errno)
        );
    }
    
    // 5. Adicionar framing TCP (2 bytes length prefix)
    std::vector<uint8_t> framed_query = addTCPFraming(query);
    
    // 6. Enviar query completa (com framing)
    if (!sendAll(sockfd, framed_query.data(), framed_query.size())) {
        throw std::runtime_error(
            std::string("Falha ao enviar query TCP: ") + strerror(errno)
        );
    }
    
    // 7. Ler length prefix da resposta (2 bytes)
    uint8_t length_bytes[2];
    if (!recvAll(sockfd, length_bytes, 2)) {
        throw std::runtime_error(
            std::string("Falha ao receber length da resposta TCP: ") + strerror(errno)
        );
    }
    
    // Converter length de network byte order (big-endian)
    uint16_t response_length = (static_cast<uint16_t>(length_bytes[0]) << 8) | 
                                length_bytes[1];
    
    // Validação: resposta deve ter pelo menos 12 bytes (header)
    if (response_length < 12) {
        throw std::runtime_error(
            "Resposta TCP muito pequena (" + std::to_string(response_length) + 
            " bytes, mínimo 12)"
        );
    }
    
    // 8. Ler resposta DNS completa
    std::vector<uint8_t> response(response_length);
    if (!recvAll(sockfd, response.data(), response_length)) {
        throw std::runtime_error(
            std::string("Falha ao receber dados da resposta TCP: ") + strerror(errno)
        );
    }
    
    return response;
}

// ========== HELPERS PARA TCP ==========

std::vector<uint8_t> NetworkModule::addTCPFraming(const std::vector<uint8_t>& message) {
    std::vector<uint8_t> framed;
    
    // Validar tamanho (máximo 65535 bytes)
    if (message.size() > 65535) {
        throw std::invalid_argument("Mensagem DNS muito grande para TCP framing");
    }
    
    // Length em network byte order (big-endian)
    uint16_t length = static_cast<uint16_t>(message.size());
    framed.push_back((length >> 8) & 0xFF);  // High byte primeiro
    framed.push_back(length & 0xFF);          // Low byte segundo
    
    // Adicionar mensagem
    framed.insert(framed.end(), message.begin(), message.end());
    
    return framed;
}

bool NetworkModule::sendAll(int sockfd, const uint8_t* buffer, size_t length) {
    size_t total_sent = 0;
    
    while (total_sent < length) {
        ssize_t sent = send(sockfd, buffer + total_sent, length - total_sent, 0);
        
        if (sent < 0) {
            // Verificar se foi interrompido por sinal (retry)
            if (errno == EINTR) {
                continue;
            }
            // Verificar timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                errno = ETIMEDOUT;
            }
            return false;  // Erro real
        }
        
        if (sent == 0) {
            return false;  // Conexão fechada
        }
        
        total_sent += static_cast<size_t>(sent);
    }
    
    return true;
}

bool NetworkModule::recvAll(int sockfd, uint8_t* buffer, size_t length) {
    size_t total_received = 0;
    
    while (total_received < length) {
        ssize_t received = recv(sockfd, buffer + total_received, 
                                length - total_received, 0);
        
        if (received < 0) {
            // Verificar se foi interrompido por sinal (retry)
            if (errno == EINTR) {
                continue;
            }
            // Verificar timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                errno = ETIMEDOUT;
            }
            return false;  // Erro real
        }
        
        if (received == 0) {
            return false;  // Conexão fechada prematuramente
        }
        
        total_received += static_cast<size_t>(received);
    }
    
    return true;
}

// ========== IMPLEMENTAÇÃO DNS over TLS (STORY 2.2) ==========

std::vector<uint8_t> NetworkModule::queryDoT(
    const std::string& server,
    const std::vector<uint8_t>& query,
    const std::string& sni,
    int timeout_seconds
) {
    // Validação de entrada
    if (server.empty()) {
        throw std::invalid_argument("Endereço do servidor vazio");
    }
    
    if (query.empty()) {
        throw std::invalid_argument("Query DNS vazia");
    }
    
    if (sni.empty()) {
        throw std::invalid_argument("SNI (Server Name Indication) é obrigatório para DoT");
    }
    
    // 1. Inicializar OpenSSL e criar contexto
    SSL_CTX* ssl_ctx = static_cast<SSL_CTX*>(createSSLContext());
    
    // RAII para SSL_CTX
    struct SSLContextGuard {
        SSL_CTX* ctx;
        ~SSLContextGuard() { if (ctx) SSL_CTX_free(ctx); }
    } ctx_guard{ssl_ctx};
    
    // 2. Criar socket TCP (porta 853 para DoT, não 53)
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error(
            std::string("Falha ao criar socket para DoT: ") + strerror(errno)
        );
    }
    
    // RAII para socket
    SocketRAII socket_guard(sockfd);
    
    // 3. Configurar timeouts
    struct timeval tv;
    tv.tv_sec = timeout_seconds;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    // 4. Preparar endereço do servidor (porta 853)
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(853);  // DoT usa porta 853
    
    if (inet_pton(AF_INET, server.c_str(), &server_addr.sin_addr) <= 0) {
        throw std::invalid_argument("Endereço IP inválido: " + server);
    }
    
    // 5. Conectar ao servidor
    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&server_addr), 
                sizeof(server_addr)) < 0) {
        throw std::runtime_error(
            std::string("Falha ao conectar DoT ao servidor ") + server + ":853: " + 
            strerror(errno)
        );
    }
    
    // 6. Criar objeto SSL
    SSL* ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        throw std::runtime_error("Falha ao criar objeto SSL");
    }
    
    // RAII para SSL
    struct SSLGuard {
        SSL* ssl;
        ~SSLGuard() { if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); } }
    } ssl_guard{ssl};
    
    // 7. Configurar SNI (Server Name Indication)
    if (SSL_set_tlsext_host_name(ssl, sni.c_str()) != 1) {
        throw std::runtime_error("Falha ao configurar SNI: " + sni);
    }
    
    // 8. Associar SSL ao socket
    if (SSL_set_fd(ssl, sockfd) != 1) {
        throw std::runtime_error("Falha ao associar SSL ao socket");
    }
    
    // 9. Realizar handshake TLS
    if (SSL_connect(ssl) <= 0) {
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        throw std::runtime_error(
            std::string("Falha no handshake TLS: ") + err_buf
        );
    }
    
    // 10. Validar certificado
    if (!validateCertificate(ssl, sni)) {
        throw std::runtime_error(
            "Validação de certificado falhou para SNI: " + sni
        );
    }
    
    // 11. Enviar query (com framing TCP, igual Story 2.1)
    std::vector<uint8_t> framed_query = addTCPFraming(query);
    
    int sent = SSL_write(ssl, framed_query.data(), 
                         static_cast<int>(framed_query.size()));
    if (sent <= 0) {
        throw std::runtime_error("Falha ao enviar query DoT via SSL");
    }
    
    // 12. Receber length prefix (2 bytes)
    uint8_t length_bytes[2];
    int received = SSL_read(ssl, length_bytes, 2);
    if (received != 2) {
        throw std::runtime_error("Falha ao receber length da resposta DoT");
    }
    
    uint16_t response_length = (static_cast<uint16_t>(length_bytes[0]) << 8) | 
                                length_bytes[1];
    
    // Validação
    if (response_length < 12) {
        throw std::runtime_error(
            "Resposta DoT muito pequena (" + std::to_string(response_length) + 
            " bytes, mínimo 12)"
        );
    }
    
    // 13. Receber resposta completa
    std::vector<uint8_t> response(response_length);
    int total_received = 0;
    
    while (total_received < response_length) {
        int chunk = SSL_read(ssl, response.data() + total_received, 
                             response_length - total_received);
        if (chunk <= 0) {
            throw std::runtime_error("Falha ao receber dados da resposta DoT");
        }
        total_received += chunk;
    }
    
    return response;
}

// ========== HELPERS TLS ==========

void* NetworkModule::createSSLContext() {
    // Inicializar biblioteca OpenSSL (compatível com 1.1.1+ e 3.x)
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
    #endif
    
    // Criar contexto SSL com TLS_client_method()
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        throw std::runtime_error("Falha ao criar contexto SSL");
    }
    
    // Configurar verificação de certificados (obrigatório)
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    
    // Carregar certificados CA do sistema
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        SSL_CTX_free(ctx);
        throw std::runtime_error(
            "Falha ao carregar certificados CA do sistema"
        );
    }
    
    // Configurar versão mínima TLS (TLS 1.2+)
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1) {
        SSL_CTX_free(ctx);
        throw std::runtime_error("Falha ao configurar versão mínima TLS");
    }
    
    return ctx;
}

bool NetworkModule::validateCertificate(
    void* ssl_ptr,
    const std::string& expected_hostname
) {
    SSL* ssl = static_cast<SSL*>(ssl_ptr);
    
    // 1. Verificar resultado da validação OpenSSL
    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        std::cerr << "Erro na verificação do certificado: " 
                  << X509_verify_cert_error_string(verify_result) << std::endl;
        return false;
    }
    
    // 2. Obter certificado do servidor
    X509* cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        std::cerr << "Servidor não apresentou certificado" << std::endl;
        return false;
    }
    
    // RAII para certificado
    struct X509Guard {
        X509* cert;
        ~X509Guard() { if (cert) X509_free(cert); }
    } cert_guard{cert};
    
    // 3. Verificar CN (Common Name)
    std::string cn = extractCertificateCN(cert);
    if (cn == expected_hostname) {
        return true;
    }
    
    // 4. Verificar SANs (Subject Alternative Names)
    std::vector<std::string> san_list = extractCertificateSAN(cert);
    for (const auto& san : san_list) {
        if (san == expected_hostname) {
            return true;
        }
        
        // Suportar wildcards (ex: *.google.com)
        if (!san.empty() && san[0] == '*' && san.length() > 1) {
            std::string domain_part = san.substr(1);  // Remove *
            
            // Verificar se expected_hostname termina com domain_part
            if (expected_hostname.length() >= domain_part.length()) {
                std::string host_suffix = expected_hostname.substr(
                    expected_hostname.length() - domain_part.length()
                );
                if (host_suffix == domain_part) {
                    return true;
                }
            }
        }
    }
    
    std::cerr << "Hostname mismatch: esperado '" << expected_hostname 
              << "', certificado tem CN='" << cn << "'" << std::endl;
    return false;
}

std::string NetworkModule::extractCertificateCN(void* cert_ptr) {
    X509* cert = static_cast<X509*>(cert_ptr);
    
    char cn[256] = {0};
    X509_NAME* subject = X509_get_subject_name(cert);
    X509_NAME_get_text_by_NID(subject, NID_commonName, cn, sizeof(cn));
    
    return std::string(cn);
}

std::vector<std::string> NetworkModule::extractCertificateSAN(void* cert_ptr) {
    X509* cert = static_cast<X509*>(cert_ptr);
    std::vector<std::string> san_list;
    
    // Obter SANs do certificado
    STACK_OF(GENERAL_NAME)* san_names = static_cast<STACK_OF(GENERAL_NAME)*>(
        X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr)
    );
    
    if (!san_names) {
        return san_list;
    }
    
    int san_count = sk_GENERAL_NAME_num(san_names);
    for (int i = 0; i < san_count; i++) {
        GENERAL_NAME* entry = sk_GENERAL_NAME_value(san_names, i);
        if (entry && entry->type == GEN_DNS) {
            const unsigned char* dns_name = ASN1_STRING_get0_data(entry->d.dNSName);
            if (dns_name) {
                san_list.push_back(reinterpret_cast<const char*>(dns_name));
            }
        }
    }
    
    sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);
    return san_list;
}

} // namespace dns_resolver

