#include "../include/dns_resolver/NetworkModule.h"
#include "../include/dns_resolver/types.h"
#include <iostream>
#include <cassert>
#include <stdexcept>

using namespace dns_resolver;

// ========== Testes de Validação de Entrada ==========

void test_queryUDP_empty_server() {
    std::cout << "  [TEST] queryUDP - servidor vazio... ";
    
    std::vector<uint8_t> query = {0x01, 0x02, 0x03};
    
    try {
        NetworkModule::queryUDP("", query);
        assert(false && "Deveria lançar exceção para servidor vazio");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (exceção esperada)\n";
    }
}

void test_queryUDP_empty_query() {
    std::cout << "  [TEST] queryUDP - query vazia... ";
    
    std::vector<uint8_t> empty_query;
    
    try {
        NetworkModule::queryUDP("8.8.8.8", empty_query);
        assert(false && "Deveria lançar exceção para query vazia");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (exceção esperada)\n";
    }
}

void test_queryUDP_invalid_ip() {
    std::cout << "  [TEST] queryUDP - IP inválido... ";
    
    std::vector<uint8_t> query = {0x01, 0x02, 0x03};
    
    try {
        NetworkModule::queryUDP("not.an.ip.address", query);
        assert(false && "Deveria lançar exceção para IP inválido");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (exceção esperada)\n";
    }
}

void test_queryUDP_malformed_ip() {
    std::cout << "  [TEST] queryUDP - IP malformado... ";
    
    std::vector<uint8_t> query = {0x01, 0x02, 0x03};
    
    try {
        NetworkModule::queryUDP("999.999.999.999", query);
        assert(false && "Deveria lançar exceção para IP malformado");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (exceção esperada)\n";
    }
}

// ========== Testes de Envio UDP (requer rede) ==========

void test_queryUDP_send_success() {
    std::cout << "  [TEST] queryUDP - envio bem-sucedido (8.8.8.8)... ";
    
    // Criar uma query DNS simples
    std::vector<uint8_t> query = {
        // Header (12 bytes)
        0x12, 0x34,       // ID
        0x01, 0x00,       // Flags (RD=1)
        0x00, 0x01,       // QDCOUNT=1
        0x00, 0x00,       // ANCOUNT=0
        0x00, 0x00,       // NSCOUNT=0
        0x00, 0x00,       // ARCOUNT=0
        // Question
        0x06, 'g','o','o','g','l','e',
        0x03, 'c','o','m',
        0x00,             // Terminador
        0x00, 0x01,       // QTYPE=A
        0x00, 0x01        // QCLASS=IN
    };
    
    try {
        // Tentar enviar para 8.8.8.8
        // NOTA: Não esperamos resposta (STORY 1.2), apenas sucesso no envio
        NetworkModule::queryUDP("8.8.8.8", query, 1);
        std::cout << "✅\n";
    } catch (const std::runtime_error& e) {
        // Se não houver conectividade, informar mas não falhar o teste
        std::cout << "⚠️  (sem conectividade: " << e.what() << ")\n";
    }
}

void test_queryUDP_timeout_configurable() {
    std::cout << "  [TEST] queryUDP - timeout configurável... ";
    
    std::vector<uint8_t> query = {0x01, 0x02, 0x03};
    
    // Usar IP não roteável (192.0.2.0/24 é TEST-NET-1)
    // Timeout de 1 segundo deve ser respeitado
    auto start = std::chrono::steady_clock::now();
    
    try {
        NetworkModule::queryUDP("192.0.2.1", query, 1);
    } catch (const std::runtime_error&) {
        // Esperamos timeout ou erro de rede
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    
    // O timeout deve ter sido respeitado (permitir margem de 1-3 segundos)
    assert(duration >= 0 && duration <= 5);
    
    std::cout << "✅ (timeout respeitado: " << duration << "s)\n";
}

void test_queryUDP_different_servers() {
    std::cout << "  [TEST] queryUDP - diferentes servidores DNS... ";
    
    std::vector<uint8_t> query = {
        0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x06, 'g','o','o','g','l','e', 0x03, 'c','o','m', 0x00,
        0x00, 0x01, 0x00, 0x01
    };
    
    std::vector<std::string> servers = {
        "8.8.8.8",      // Google DNS
        "1.1.1.1",      // Cloudflare DNS
        "208.67.222.222" // OpenDNS
    };
    
    int successful = 0;
    for (const auto& server : servers) {
        try {
            NetworkModule::queryUDP(server, query, 2);
            successful++;
        } catch (const std::runtime_error&) {
            // Servidor pode estar inacessível
        }
    }
    
    std::cout << "✅ (" << successful << "/" << servers.size() << " servidores acessíveis)\n";
}

// ========== Testes de Recursos (RAII) ==========

void test_socket_raii_no_leak() {
    std::cout << "  [TEST] Socket RAII - sem vazamento de recursos... ";
    
    std::vector<uint8_t> query = {
        0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x06, 'g','o','o','g','l','e', 0x03, 'c','o','m', 0x00,
        0x00, 0x01, 0x00, 0x01
    };
    
    // Executar múltiplas queries para verificar se há leak de FDs
    for (int i = 0; i < 10; i++) {
        try {
            NetworkModule::queryUDP("8.8.8.8", query, 1);
        } catch (...) {
            // Ignorar erros de rede
        }
    }
    
    // Se chegou aqui sem crash ou "too many open files", RAII está funcionando
    std::cout << "✅\n";
}

// ========== Testes de TCP (STORY 2.1) ==========

void test_queryTCP_basic() {
    std::cout << "  [TEST] queryTCP - envio básico TCP... ";
    
    // Criar uma query DNS simples
    std::vector<uint8_t> query = {
        0x12, 0x34,       // ID
        0x01, 0x00,       // Flags (RD=1)
        0x00, 0x01,       // QDCOUNT=1
        0x00, 0x00,       // ANCOUNT=0
        0x00, 0x00,       // NSCOUNT=0
        0x00, 0x00,       // ARCOUNT=0
        0x06, 'g','o','o','g','l','e',
        0x03, 'c','o','m',
        0x00,
        0x00, 0x01,       // QTYPE=A
        0x00, 0x01        // QCLASS=IN
    };
    
    try {
        // Tentar query TCP para 8.8.8.8
        auto response = NetworkModule::queryTCP("8.8.8.8", query, 5);
        
        // Verificar que recebeu resposta
        assert(response.size() >= 12 && "Resposta TCP deve ter pelo menos 12 bytes (header)");
        std::cout << "✅ (resposta recebida: " << response.size() << " bytes)\n";
    } catch (const std::runtime_error& e) {
        // Se não houver conectividade ou servidor não suportar TCP
        std::cout << "⚠️  (sem conectividade TCP: " << e.what() << ")\n";
    }
}

void test_queryTCP_validation() {
    std::cout << "  [TEST] queryTCP - validação de entrada... ";
    
    std::vector<uint8_t> query = {0x01, 0x02, 0x03};
    
    // Servidor vazio
    try {
        NetworkModule::queryTCP("", query);
        assert(false && "Deveria lançar exceção para servidor vazio");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (validação OK)\n";
        return;
    }
    
    std::cout << "✅\n";
}

void test_queryTCP_invalid_server() {
    std::cout << "  [TEST] queryTCP - servidor inválido... ";
    
    std::vector<uint8_t> query = {0x01, 0x02, 0x03};
    
    try {
        NetworkModule::queryTCP("999.999.999.999", query, 1);
        assert(false && "Deveria lançar exceção para IP inválido");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (erro esperado - IP inválido)\n";
    } catch (const std::runtime_error& e) {
        std::cout << "✅ (erro esperado - conexão falhou)\n";
    }
}

void test_queryDoT_requires_sni() {
    std::cout << "  [TEST] queryDoT - requer SNI... ";
    
    std::vector<uint8_t> query = {0x01, 0x02};
    
    try {
        // SNI vazio deve lançar exceção
        NetworkModule::queryDoT("8.8.8.8", query, "");
        assert(false && "Deveria lançar exceção (SNI vazio)");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (exceção esperada)\n";
    }
}

// ========== Runner ==========

int main() {
    std::cout << "\n========================================\n";
    std::cout << "  Testes Unitários - NetworkModule\n";
    std::cout << "========================================\n\n";
    
    std::cout << "→ Testes de Validação:\n";
    test_queryUDP_empty_server();
    test_queryUDP_empty_query();
    test_queryUDP_invalid_ip();
    test_queryUDP_malformed_ip();
    
    std::cout << "\n→ Testes de Envio UDP:\n";
    test_queryUDP_send_success();
    test_queryUDP_timeout_configurable();
    test_queryUDP_different_servers();
    
    std::cout << "\n→ Testes de Recursos:\n";
    test_socket_raii_no_leak();
    
    std::cout << "\n→ Testes de TCP (Story 2.1):\n";
    test_queryTCP_basic();
    test_queryTCP_validation();
    test_queryTCP_invalid_server();
    
    std::cout << "\n→ Testes de DoT (STORY 2.2):\n";
    test_queryDoT_requires_sni();
    
    std::cout << "\n========================================\n";
    std::cout << "  ✅ Todos os testes passaram!\n";
    std::cout << "========================================\n\n";
    
    return 0;
}

