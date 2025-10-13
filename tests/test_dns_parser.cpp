#include "../include/dns_resolver/DNSParser.h"
#include "../include/dns_resolver/types.h"
#include <iostream>
#include <cassert>
#include <stdexcept>

using namespace dns_resolver;

// ========== Testes de encodeDomainName ==========

void test_encodeDomainName_simple() {
    std::cout << "  [TEST] encodeDomainName - domínio simples... ";
    
    DNSMessage msg;
    msg.header.id = 0x1234;
    msg.header.qr = false;
    msg.header.rd = true;
    msg.header.qdcount = 1;
    msg.questions.emplace_back("google.com", DNSType::A, DNSClass::IN);
    
    auto buffer = DNSParser::serialize(msg);
    
    // Verificar que o nome está codificado corretamente após o header (12 bytes)
    // Formato esperado: 06 g o o g l e 03 c o m 00
    // Tamanho do nome: 1 + 6 + 1 + 3 + 1 = 12 bytes
    assert(buffer.size() == 28); // header(12) + nome(12) + qtype(2) + qclass(2)
    assert(buffer[12] == 0x06); // Tamanho "google"
    assert(buffer[13] == 'g');
    assert(buffer[19] == 0x03); // Tamanho "com"
    assert(buffer[20] == 'c');
    assert(buffer[23] == 0x00); // Terminador
    
    std::cout << "✅\n";
}

void test_encodeDomainName_subdomain() {
    std::cout << "  [TEST] encodeDomainName - subdomínio... ";
    
    DNSMessage msg;
    msg.header.qdcount = 1;
    msg.questions.emplace_back("www.example.com", DNSType::A, DNSClass::IN);
    
    auto buffer = DNSParser::serialize(msg);
    
    // Formato: 03 w w w 07 e x a m p l e 03 c o m 00
    // Tamanho: 1+3+1+7+1+3+1 = 17 bytes
    assert(buffer.size() == 33); // header(12) + nome(17) + qtype(2) + qclass(2)
    assert(buffer[12] == 0x03); // "www"
    assert(buffer[16] == 0x07); // "example"
    assert(buffer[24] == 0x03); // "com"
    assert(buffer[28] == 0x00); // Terminador
    
    std::cout << "✅\n";
}

void test_encodeDomainName_empty() {
    std::cout << "  [TEST] encodeDomainName - domínio vazio... ";
    
    DNSMessage msg;
    msg.header.qdcount = 1;
    msg.questions.emplace_back("", DNSType::A, DNSClass::IN);
    
    try {
        DNSParser::serialize(msg);
        assert(false && "Deveria lançar exceção para domínio vazio");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (exceção esperada)\n";
    }
}

void test_encodeDomainName_too_long() {
    std::cout << "  [TEST] encodeDomainName - domínio > 255 chars... ";
    
    // Criar domínio com múltiplos labels que somem > 255 chars
    // Cada label tem max 63 chars, então precisamos de múltiplos labels
    std::string long_domain;
    for (int i = 0; i < 5; i++) {
        if (i > 0) long_domain += ".";
        long_domain += std::string(50, 'a'); // 5 labels de 50 chars = 250 chars
    }
    // Total: 250 chars + 4 pontos = 254 chars (ainda válido)
    
    DNSMessage msg;
    msg.header.qdcount = 1;
    msg.questions.emplace_back(long_domain, DNSType::A, DNSClass::IN);
    
    // Isso deve funcionar (254 chars)
    auto buffer = DNSParser::serialize(msg);
    assert(buffer.size() > 12);
    
    // Agora tentar com 256+ chars
    long_domain += ".ab"; // Adicionar mais 3 chars (total 257)
    msg.questions[0].qname = long_domain;
    
    try {
        DNSParser::serialize(msg);
        assert(false && "Deveria lançar exceção para domínio > 255 chars");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (exceção esperada)\n";
    }
}

void test_encodeDomainName_label_too_long() {
    std::cout << "  [TEST] encodeDomainName - label > 63 chars... ";
    
    // Criar label com 64 caracteres
    std::string long_label(64, 'a');
    long_label += ".com";
    
    DNSMessage msg;
    msg.header.qdcount = 1;
    msg.questions.emplace_back(long_label, DNSType::A, DNSClass::IN);
    
    try {
        DNSParser::serialize(msg);
        assert(false && "Deveria lançar exceção para label > 63 chars");
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ (exceção esperada)\n";
    }
}

void test_encodeDomainName_max_valid_label() {
    std::cout << "  [TEST] encodeDomainName - label com 63 chars (válido)... ";
    
    // Criar label com exatamente 63 caracteres
    std::string max_label(63, 'a');
    max_label += ".com";
    
    DNSMessage msg;
    msg.header.qdcount = 1;
    msg.questions.emplace_back(max_label, DNSType::A, DNSClass::IN);
    
    auto buffer = DNSParser::serialize(msg);
    assert(buffer[12] == 63); // Tamanho do label
    
    std::cout << "✅\n";
}

// ========== Testes de Serialização de Header ==========

void test_serialize_header_structure() {
    std::cout << "  [TEST] serialize - estrutura do header... ";
    
    DNSMessage msg;
    msg.header.id = 0xABCD;
    msg.header.qr = false;
    msg.header.opcode = DNSOpcode::QUERY;
    msg.header.rd = true;
    msg.header.qdcount = 1;
    msg.questions.emplace_back("test.com", DNSType::A, DNSClass::IN);
    
    auto buffer = DNSParser::serialize(msg);
    
    // Verificar tamanho mínimo
    assert(buffer.size() >= 12);
    
    // Verificar ID (big-endian)
    assert(buffer[0] == 0xAB);
    assert(buffer[1] == 0xCD);
    
    // Verificar QDCOUNT (big-endian)
    assert(buffer[4] == 0x00);
    assert(buffer[5] == 0x01);
    
    // Verificar outros contadores zerados
    assert(buffer[6] == 0x00 && buffer[7] == 0x00); // ANCOUNT
    assert(buffer[8] == 0x00 && buffer[9] == 0x00); // NSCOUNT
    assert(buffer[10] == 0x00 && buffer[11] == 0x00); // ARCOUNT
    
    std::cout << "✅\n";
}

void test_serialize_flags_encoding() {
    std::cout << "  [TEST] serialize - codificação de flags... ";
    
    DNSMessage msg;
    msg.header.id = 0x0001;
    msg.header.qr = false;  // 0
    msg.header.opcode = 0;  // 0000
    msg.header.aa = false;  // 0
    msg.header.tc = false;  // 0
    msg.header.rd = true;   // 1
    msg.header.ra = false;  // 0
    msg.header.z = 0;       // 000
    msg.header.rcode = 0;   // 0000
    msg.header.qdcount = 1;
    msg.questions.emplace_back("test.com", DNSType::A, DNSClass::IN);
    
    auto buffer = DNSParser::serialize(msg);
    
    // Flags esperadas: 0000000100000000 (RD=1, resto=0) = 0x0100
    // Em big-endian: 01 00
    assert(buffer[2] == 0x01);
    assert(buffer[3] == 0x00);
    
    std::cout << "✅\n";
}

void test_serialize_question_section() {
    std::cout << "  [TEST] serialize - seção de question... ";
    
    DNSMessage msg;
    msg.header.id = 0x0001;
    msg.header.qdcount = 1;
    msg.questions.emplace_back("example.com", DNSType::A, DNSClass::IN);
    
    auto buffer = DNSParser::serialize(msg);
    
    // Verificar QTYPE (após o nome)
    // Nome: 07 e x a m p l e 03 c o m 00 = 1+7+1+3+1 = 13 bytes
    // QTYPE começa no offset 12 + 13 = 25
    size_t qtype_offset = 12 + 13;
    assert(buffer[qtype_offset] == 0x00);
    assert(buffer[qtype_offset + 1] == 0x01); // A = 1
    
    // QCLASS
    assert(buffer[qtype_offset + 2] == 0x00);
    assert(buffer[qtype_offset + 3] == 0x01); // IN = 1
    
    std::cout << "✅\n";
}

void test_serialize_multiple_questions() {
    std::cout << "  [TEST] serialize - múltiplas questions... ";
    
    DNSMessage msg;
    msg.header.id = 0x0001;
    msg.header.qdcount = 2;
    msg.questions.emplace_back("google.com", DNSType::A, DNSClass::IN);
    msg.questions.emplace_back("example.com", DNSType::AAAA, DNSClass::IN);
    
    auto buffer = DNSParser::serialize(msg);
    
    // Verificar QDCOUNT
    assert(buffer[4] == 0x00);
    assert(buffer[5] == 0x02);
    
    // Verificar que o buffer contém ambos os nomes
    // google.com: 1+6+1+3+1 = 12 bytes
    // example.com: 1+7+1+3+1 = 13 bytes
    assert(buffer.size() == 12 + (12+4) + (13+4)); // header + question1 + question2 = 45
    
    std::cout << "✅\n";
}

// ========== Testes de Endianness ==========

void test_network_byte_order() {
    std::cout << "  [TEST] network byte order (big-endian)... ";
    
    DNSMessage msg;
    msg.header.id = 0x1234;
    msg.header.qdcount = 0x0005;
    msg.header.ancount = 0xABCD;
    
    auto buffer = DNSParser::serialize(msg);
    
    // Verificar big-endian
    assert(buffer[0] == 0x12 && buffer[1] == 0x34);
    assert(buffer[4] == 0x00 && buffer[5] == 0x05);
    assert(buffer[6] == 0xAB && buffer[7] == 0xCD);
    
    std::cout << "✅\n";
}

// ========== Runner ==========

int main() {
    std::cout << "\n========================================\n";
    std::cout << "  Testes Unitários - DNSParser\n";
    std::cout << "========================================\n\n";
    
    std::cout << "→ Testes de encodeDomainName:\n";
    test_encodeDomainName_simple();
    test_encodeDomainName_subdomain();
    test_encodeDomainName_empty();
    test_encodeDomainName_too_long();
    test_encodeDomainName_label_too_long();
    test_encodeDomainName_max_valid_label();
    
    std::cout << "\n→ Testes de Serialização:\n";
    test_serialize_header_structure();
    test_serialize_flags_encoding();
    test_serialize_question_section();
    test_serialize_multiple_questions();
    
    std::cout << "\n→ Testes de Endianness:\n";
    test_network_byte_order();
    
    std::cout << "\n========================================\n";
    std::cout << "  ✅ Todos os testes passaram!\n";
    std::cout << "========================================\n\n";
    
    return 0;
}

