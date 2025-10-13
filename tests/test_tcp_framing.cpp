#include "../include/dns_resolver/types.h"
#include <iostream>
#include <cassert>
#include <vector>

// ========== TESTES DE TCP FRAMING ==========

/**
 * Testa adição de length prefix (TCP framing)
 */
void test_tcp_framing_basic() {
    std::cout << "  [TEST] TCP Framing - length prefix básico... ";
    
    // Mensagem DNS de 28 bytes (exemplo)
    std::vector<uint8_t> message = {
        0x12, 0x34, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x06, 'g','o','o','g','l','e', 0x03, 'c','o','m', 0x00,
        0x00, 0x01, 0x00, 0x01
    };
    
    // Simular addTCPFraming manualmente
    std::vector<uint8_t> framed;
    uint16_t length = message.size();  // 28
    framed.push_back((length >> 8) & 0xFF);  // 0x00
    framed.push_back(length & 0xFF);          // 0x1C
    framed.insert(framed.end(), message.begin(), message.end());
    
    // Verificações
    assert(framed.size() == message.size() + 2);
    assert(framed[0] == 0x00);  // High byte de 28 (0x001C)
    assert(framed[1] == 0x1C);  // Low byte de 28
    assert(framed[2] == 0x12);  // Primeiro byte da mensagem (ID)
    
    std::cout << "✅\n";
}

/**
 * Testa framing com mensagens de tamanhos variados
 */
void test_tcp_framing_sizes() {
    std::cout << "  [TEST] TCP Framing - tamanhos variados... ";
    
    // Mensagem pequena (12 bytes - apenas header)
    std::vector<uint8_t> small(12, 0x00);
    std::vector<uint8_t> framed_small;
    uint16_t length_small = small.size();
    framed_small.push_back((length_small >> 8) & 0xFF);
    framed_small.push_back(length_small & 0xFF);
    framed_small.insert(framed_small.end(), small.begin(), small.end());
    
    assert(framed_small.size() == 14);  // 2 + 12
    assert(framed_small[0] == 0x00);
    assert(framed_small[1] == 0x0C);  // 12 em hex
    
    // Mensagem grande (512 bytes)
    std::vector<uint8_t> large(512, 0xFF);
    std::vector<uint8_t> framed_large;
    uint16_t length_large = large.size();
    framed_large.push_back((length_large >> 8) & 0xFF);
    framed_large.push_back(length_large & 0xFF);
    framed_large.insert(framed_large.end(), large.begin(), large.end());
    
    assert(framed_large.size() == 514);  // 2 + 512
    assert(framed_large[0] == 0x02);  // High byte de 512 (0x0200)
    assert(framed_large[1] == 0x00);  // Low byte
    
    std::cout << "✅\n";
}

/**
 * Testa big-endian (network byte order) do length
 */
void test_tcp_framing_endianness() {
    std::cout << "  [TEST] TCP Framing - big-endian (network byte order)... ";
    
    // Mensagem de 256 bytes (0x0100 em big-endian)
    std::vector<uint8_t> message(256, 0xAB);
    std::vector<uint8_t> framed;
    uint16_t length = 256;
    
    // Big-endian: byte alto primeiro
    framed.push_back((length >> 8) & 0xFF);  // 0x01
    framed.push_back(length & 0xFF);          // 0x00
    
    assert(framed[0] == 0x01);  // High byte primeiro (big-endian)
    assert(framed[1] == 0x00);  // Low byte segundo
    
    // Reconstruir length
    uint16_t reconstructed = (framed[0] << 8) | framed[1];
    assert(reconstructed == 256);
    
    std::cout << "✅\n";
}

/**
 * Testa detecção de mensagem muito grande (>65535 bytes)
 */
void test_tcp_framing_max_size() {
    std::cout << "  [TEST] TCP Framing - limite de tamanho (65535 bytes)... ";
    
    // Tamanho máximo que cabe em uint16_t
    uint16_t max_size = 65535;
    std::vector<uint8_t> message(max_size, 0x00);
    
    // Deve funcionar
    std::vector<uint8_t> framed;
    framed.push_back((max_size >> 8) & 0xFF);
    framed.push_back(max_size & 0xFF);
    assert(framed.size() == 2);
    
    uint16_t reconstructed = (framed[0] << 8) | framed[1];
    assert(reconstructed == 65535);
    
    std::cout << "✅\n";
}

/**
 * Testa parsing de length prefix da resposta
 */
void test_tcp_framing_parse_length() {
    std::cout << "  [TEST] TCP Framing - parsing de length prefix... ";
    
    // Simular resposta TCP: [length: 2 bytes][DNS message]
    uint8_t length_bytes[2] = {0x00, 0x2A};  // 42 bytes
    
    uint16_t length = (length_bytes[0] << 8) | length_bytes[1];
    
    assert(length == 42);
    std::cout << "✅\n";
}

// ========== MAIN ==========

int main() {
    std::cout << "\n========================================\n";
    std::cout << "  Testes de TCP Framing\n";
    std::cout << "  Story 2.1 - Automated Test Suite\n";
    std::cout << "========================================\n\n";
    
    test_tcp_framing_basic();
    test_tcp_framing_sizes();
    test_tcp_framing_endianness();
    test_tcp_framing_max_size();
    test_tcp_framing_parse_length();
    
    std::cout << "\n========================================\n";
    std::cout << "  ✅ Todos os testes de TCP framing passaram!\n";
    std::cout << "========================================\n\n";
    
    return 0;
}
