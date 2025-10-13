/**
 * Testes Unitários: DNSSEC Records (Story 3.2)
 * Parsing DNSKEY, DS e EDNS0
 */

#include "dns_resolver/DNSParser.h"
#include "dns_resolver/types.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace dns_resolver;

#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

int tests_passed = 0;
int tests_failed = 0;

//==============================================================================
// TESTES DE PARSING DNSKEY
//==============================================================================

void test_parse_dnskey_ksk() {
    std::cout << "  [TEST] Parsing DNSKEY KSK (flags=257)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header (ID=0x1234, QR=1, ANCOUNT=1)
        buffer.push_back(0x12); buffer.push_back(0x34);  // ID
        buffer.push_back(0x81); buffer.push_back(0x00);  // Flags (QR=1, RD=1)
        buffer.push_back(0x00); buffer.push_back(0x01);  // QDCOUNT=1
        buffer.push_back(0x00); buffer.push_back(0x01);  // ANCOUNT=1
        buffer.push_back(0x00); buffer.push_back(0x00);  // NSCOUNT=0
        buffer.push_back(0x00); buffer.push_back(0x00);  // ARCOUNT=0
        
        // Question: . DNSKEY IN
        buffer.push_back(0x00);  // Root name
        buffer.push_back(0x00); buffer.push_back(0x30);  // QTYPE=48 (DNSKEY)
        buffer.push_back(0x00); buffer.push_back(0x01);  // QCLASS=1 (IN)
        
        // Answer: . DNSKEY (flags=257 KSK, protocol=3, algorithm=8, key=8 bytes)
        buffer.push_back(0x00);  // Name (root)
        buffer.push_back(0x00); buffer.push_back(0x30);  // TYPE=48
        buffer.push_back(0x00); buffer.push_back(0x01);  // CLASS=1
        buffer.push_back(0x00); buffer.push_back(0x00);  // TTL
        buffer.push_back(0x00); buffer.push_back(0x64);  // TTL (100s)
        buffer.push_back(0x00); buffer.push_back(0x0C);  // RDLENGTH=12 (4+8)
        
        // RDATA: Flags=257, Protocol=3, Algorithm=8, Key=8 bytes
        buffer.push_back(0x01); buffer.push_back(0x01);  // Flags=257 (KSK)
        buffer.push_back(0x03);  // Protocol=3
        buffer.push_back(0x08);  // Algorithm=8 (RSA/SHA-256)
        // Public key (8 bytes)
        for (int i = 0; i < 8; i++) buffer.push_back(0xAA + i);
        
        DNSMessage msg = parser.parse(buffer);
        
        assert(msg.answers.size() == 1);
        assert(msg.answers[0].type == DNSType::DNSKEY);
        assert(msg.answers[0].rdata_dnskey.flags == 257);
        assert(msg.answers[0].rdata_dnskey.protocol == 3);
        assert(msg.answers[0].rdata_dnskey.algorithm == 8);
        assert(msg.answers[0].rdata_dnskey.public_key.size() == 8);
        assert(msg.answers[0].rdata_dnskey.isKSK() == true);
        assert(msg.answers[0].rdata_dnskey.isZSK() == false);
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

void test_parse_dnskey_zsk() {
    std::cout << "  [TEST] Parsing DNSKEY ZSK (flags=256)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header
        buffer.push_back(0x12); buffer.push_back(0x34);
        buffer.push_back(0x81); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        
        // Question
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x30);
        buffer.push_back(0x00); buffer.push_back(0x01);
        
        // Answer: DNSKEY ZSK
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x30);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x64);
        buffer.push_back(0x00); buffer.push_back(0x08);  // RDLENGTH=8 (4+4)
        
        // Flags=256 (ZSK), Protocol=3, Algorithm=8, Key=4 bytes
        buffer.push_back(0x01); buffer.push_back(0x00);  // Flags=256 (ZSK)
        buffer.push_back(0x03);
        buffer.push_back(0x08);
        for (int i = 0; i < 4; i++) buffer.push_back(0xBB);
        
        DNSMessage msg = parser.parse(buffer);
        
        assert(msg.answers[0].rdata_dnskey.flags == 256);
        assert(msg.answers[0].rdata_dnskey.isKSK() == false);
        assert(msg.answers[0].rdata_dnskey.isZSK() == true);
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

//==============================================================================
// TESTES DE PARSING DS
//==============================================================================

void test_parse_ds_sha256() {
    std::cout << "  [TEST] Parsing DS SHA-256 (32 bytes)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header
        buffer.push_back(0x56); buffer.push_back(0x78);
        buffer.push_back(0x81); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        
        // Question: com DS IN
        buffer.push_back(0x03);  // Label length
        buffer.push_back('c'); buffer.push_back('o'); buffer.push_back('m');
        buffer.push_back(0x00);  // Null terminator
        buffer.push_back(0x00); buffer.push_back(0x2B);  // QTYPE=43 (DS)
        buffer.push_back(0x00); buffer.push_back(0x01);  // QCLASS=1
        
        // Answer: com DS
        buffer.push_back(0xC0); buffer.push_back(0x0C);  // Name pointer
        buffer.push_back(0x00); buffer.push_back(0x2B);  // TYPE=43
        buffer.push_back(0x00); buffer.push_back(0x01);  // CLASS=1
        buffer.push_back(0x00); buffer.push_back(0x00);  // TTL
        buffer.push_back(0x00); buffer.push_back(0x64);
        buffer.push_back(0x00); buffer.push_back(0x24);  // RDLENGTH=36 (4+32)
        
        // RDATA: KeyTag=19718, Alg=13, DigestType=2, Digest=32 bytes
        buffer.push_back(0x4D); buffer.push_back(0x06);  // KeyTag=19718
        buffer.push_back(0x0D);  // Algorithm=13 (ECDSA)
        buffer.push_back(0x02);  // DigestType=2 (SHA-256)
        for (int i = 0; i < 32; i++) buffer.push_back(0x8A + (i % 16));
        
        DNSMessage msg = parser.parse(buffer);
        
        assert(msg.answers.size() == 1);
        assert(msg.answers[0].type == DNSType::DS);
        assert(msg.answers[0].rdata_ds.key_tag == 19718);
        assert(msg.answers[0].rdata_ds.algorithm == 13);
        assert(msg.answers[0].rdata_ds.digest_type == 2);
        assert(msg.answers[0].rdata_ds.digest.size() == 32);
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

void test_parse_ds_sha1() {
    std::cout << "  [TEST] Parsing DS SHA-1 (20 bytes)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header
        buffer.push_back(0x56); buffer.push_back(0x78);
        buffer.push_back(0x81); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        
        // Question: com DS IN
        buffer.push_back(0x03);
        buffer.push_back('c'); buffer.push_back('o'); buffer.push_back('m');
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x2B);
        buffer.push_back(0x00); buffer.push_back(0x01);
        
        // Answer: com DS (SHA-1)
        buffer.push_back(0xC0); buffer.push_back(0x0C);
        buffer.push_back(0x00); buffer.push_back(0x2B);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x64);
        buffer.push_back(0x00); buffer.push_back(0x18);  // RDLENGTH=24 (4+20)
        
        // KeyTag=12345, Alg=8, DigestType=1 (SHA-1), Digest=20 bytes
        buffer.push_back(0x30); buffer.push_back(0x39);  // 12345
        buffer.push_back(0x08);
        buffer.push_back(0x01);  // SHA-1
        for (int i = 0; i < 20; i++) buffer.push_back(0xDD);
        
        DNSMessage msg = parser.parse(buffer);
        
        assert(msg.answers[0].rdata_ds.digest_type == 1);
        assert(msg.answers[0].rdata_ds.digest.size() == 20);
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

//==============================================================================
// TESTES DE EDNS0
//==============================================================================

void test_edns0_serialization_do_set() {
    std::cout << "  [TEST] EDNS0 serialização (DO=1)... ";
    
    try {
        DNSParser parser;
        DNSMessage message;
        
        message.header.id = 1234;
        message.header.rd = true;
        message.header.qdcount = 1;
        
        DNSQuestion q;
        q.qname = "example.com";
        q.qtype = DNSType::A;
        q.qclass = DNSClass::IN;
        message.questions.push_back(q);
        
        // Ativar EDNS0 com DO bit
        message.use_edns = true;
        message.edns.dnssec_ok = true;
        message.edns.udp_size = 4096;
        
        std::vector<uint8_t> buffer = parser.serialize(message);
        
        // Verificar ARCOUNT = 1 (OPT RR adicionado)
        uint16_t arcount = (buffer[10] << 8) | buffer[11];
        assert(arcount == 1);
        
        // OPT RR está nos últimos 11 bytes
        size_t opt_start = buffer.size() - 11;
        
        assert(buffer[opt_start] == 0x00);      // Root name
        assert(buffer[opt_start+1] == 0x00);    // Type OPT MSB
        assert(buffer[opt_start+2] == 0x29);    // Type OPT LSB (41)
        assert(buffer[opt_start+3] == 0x10);    // UDP size MSB
        assert(buffer[opt_start+4] == 0x00);    // UDP size LSB (4096)
        assert(buffer[opt_start+5] == 0x00);    // Ext RCODE
        assert(buffer[opt_start+6] == 0x00);    // Version
        assert(buffer[opt_start+7] == 0x80);    // DO bit set
        assert(buffer[opt_start+8] == 0x00);    // Flags LSB
        assert(buffer[opt_start+9] == 0x00);    // RDLENGTH MSB
        assert(buffer[opt_start+10] == 0x00);   // RDLENGTH LSB
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

void test_edns0_serialization_do_clear() {
    std::cout << "  [TEST] EDNS0 serialização (DO=0)... ";
    
    try {
        DNSParser parser;
        DNSMessage message;
        
        message.header.id = 5678;
        message.header.rd = true;
        message.header.qdcount = 1;
        
        DNSQuestion q;
        q.qname = "test.com";
        q.qtype = DNSType::A;
        q.qclass = DNSClass::IN;
        message.questions.push_back(q);
        
        // Ativar EDNS0 MAS sem DO bit
        message.use_edns = true;
        message.edns.dnssec_ok = false;
        
        std::vector<uint8_t> buffer = parser.serialize(message);
        
        // OPT RR está nos últimos 11 bytes
        size_t opt_start = buffer.size() - 11;
        
        // Flags devem ser 0x0000 (DO=0)
        assert(buffer[opt_start+7] == 0x00);  // DO bit clear
        assert(buffer[opt_start+8] == 0x00);
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

//==============================================================================
// TESTES DE VALIDAÇÃO
//==============================================================================

void test_dnskey_rdata_too_short() {
    std::cout << "  [TEST] DNSKEY RDATA muito curto (< 4 bytes)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header
        buffer.push_back(0x12); buffer.push_back(0x34);
        buffer.push_back(0x81); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        
        // Question
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x30);
        buffer.push_back(0x00); buffer.push_back(0x01);
        
        // Answer com RDATA muito curto
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x30);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x03);  // RDLENGTH=3 (< 4 mínimo)
        
        buffer.push_back(0x01);
        buffer.push_back(0x01);
        buffer.push_back(0x03);
        
        bool exception_thrown = false;
        try {
            parser.parse(buffer);
        } catch (const std::runtime_error& e) {
            std::string msg(e.what());
            if (msg.find("DNSKEY") != std::string::npos && 
                msg.find("too short") != std::string::npos) {
                exception_thrown = true;
            }
        }
        
        assert(exception_thrown);
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

void test_ds_rdata_too_short() {
    std::cout << "  [TEST] DS RDATA muito curto (< 4 bytes)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header
        buffer.push_back(0x56); buffer.push_back(0x78);
        buffer.push_back(0x81); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        
        // Question
        buffer.push_back(0x03);
        buffer.push_back('c'); buffer.push_back('o'); buffer.push_back('m');
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x2B);
        buffer.push_back(0x00); buffer.push_back(0x01);
        
        // Answer com RDATA muito curto
        buffer.push_back(0xC0); buffer.push_back(0x0C);
        buffer.push_back(0x00); buffer.push_back(0x2B);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x02);  // RDLENGTH=2 (< 4 mínimo)
        
        buffer.push_back(0x4D);
        buffer.push_back(0x06);
        
        bool exception_thrown = false;
        try {
            parser.parse(buffer);
        } catch (const std::runtime_error& e) {
            std::string msg(e.what());
            if (msg.find("DS") != std::string::npos && 
                msg.find("too short") != std::string::npos) {
                exception_thrown = true;
            }
        }
        
        assert(exception_thrown);
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

void test_dnskey_multiple_records() {
    std::cout << "  [TEST] Múltiplos DNSKEY (KSK + ZSK)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header com ANCOUNT=2
        buffer.push_back(0x12); buffer.push_back(0x34);
        buffer.push_back(0x81); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x02);  // ANCOUNT=2
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        
        // Question
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x30);
        buffer.push_back(0x00); buffer.push_back(0x01);
        
        // Answer 1: KSK
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x30);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x64);
        buffer.push_back(0x00); buffer.push_back(0x08);
        buffer.push_back(0x01); buffer.push_back(0x01);  // KSK
        buffer.push_back(0x03);
        buffer.push_back(0x08);
        for (int i = 0; i < 4; i++) buffer.push_back(0xAA);
        
        // Answer 2: ZSK
        buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x30);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x64);
        buffer.push_back(0x00); buffer.push_back(0x08);
        buffer.push_back(0x01); buffer.push_back(0x00);  // ZSK
        buffer.push_back(0x03);
        buffer.push_back(0x08);
        for (int i = 0; i < 4; i++) buffer.push_back(0xBB);
        
        DNSMessage msg = parser.parse(buffer);
        
        assert(msg.answers.size() == 2);
        assert(msg.answers[0].rdata_dnskey.isKSK() == true);
        assert(msg.answers[1].rdata_dnskey.isZSK() == true);
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

void test_edns0_udp_size_custom() {
    std::cout << "  [TEST] EDNS0 UDP size customizado... ";
    
    try {
        DNSParser parser;
        DNSMessage message;
        
        message.header.id = 9999;
        message.header.rd = true;
        message.header.qdcount = 1;
        
        DNSQuestion q;
        q.qname = "test.com";
        q.qtype = DNSType::A;
        q.qclass = DNSClass::IN;
        message.questions.push_back(q);
        
        // EDNS com UDP size diferente
        message.use_edns = true;
        message.edns.udp_size = 1232;  // Tamanho customizado
        message.edns.dnssec_ok = false;
        
        std::vector<uint8_t> buffer = parser.serialize(message);
        
        size_t opt_start = buffer.size() - 11;
        
        // UDP size deve ser 1232 = 0x04D0
        assert(buffer[opt_start+3] == 0x04);  // MSB
        assert(buffer[opt_start+4] == 0xD0);  // LSB
        
        std::cout << "✅\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << "❌ (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

//==============================================================================
// MAIN
//==============================================================================

int main() {
    std::cout << "\n==========================================\n";
    std::cout << "  TESTES: DNSSEC Records (Story 3.2)\n";
    std::cout << "==========================================\n\n";
    
    // Parsing DNSKEY
    std::cout << "→ Testes de Parsing DNSKEY:\n";
    test_parse_dnskey_ksk();
    test_parse_dnskey_zsk();
    test_dnskey_multiple_records();
    
    // Parsing DS
    std::cout << "\n→ Testes de Parsing DS:\n";
    test_parse_ds_sha256();
    test_parse_ds_sha1();
    
    // EDNS0
    std::cout << "\n→ Testes de EDNS0:\n";
    test_edns0_serialization_do_set();
    test_edns0_serialization_do_clear();
    test_edns0_udp_size_custom();
    
    // Validação
    std::cout << "\n→ Testes de Validação:\n";
    test_dnskey_rdata_too_short();
    test_ds_rdata_too_short();
    
    // Resultados
    std::cout << "\n==========================================\n";
    std::cout << "  RESULTADOS\n";
    std::cout << "==========================================\n";
    std::cout << "  ✓ Testes passaram: " << tests_passed << "\n";
    std::cout << "  ✗ Testes falharam: " << tests_failed << "\n";
    std::cout << "==========================================\n\n";
    
    if (tests_failed == 0) {
        std::cout << GREEN << "🎉 TODOS OS TESTES PASSARAM!\n\n";
        std::cout << "  Total de testes: " << tests_passed << "\n";
        std::cout << "  Cobertura DNSSEC:\n";
        std::cout << "    • Parsing DNSKEY: KSK/ZSK ✅\n";
        std::cout << "    • Parsing DS: SHA-1/SHA-256 ✅\n";
        std::cout << "    • EDNS0: DO=0/1, UDP size ✅\n";
        std::cout << "    • Validação: RDATA size ✅\n\n" << RESET;
        return 0;
    } else {
        std::cout << RED << "❌ ALGUNS TESTES FALHARAM\n\n" << RESET;
        return 1;
    }
}

