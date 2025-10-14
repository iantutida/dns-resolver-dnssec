/*
 * Arquivo: test_dnssec_records.cpp
 * Propósito: Testes unitários para parsing de registros DNSSEC (DNSKEY, DS) e EDNS0
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver com DNSSEC
 * 
 * Este arquivo contém testes abrangentes para registros DNSSEC, cobrindo:
 * - Parsing de registros DNSKEY (KSK e ZSK) com diferentes algoritmos
 * - Parsing de registros DS (SHA-1 e SHA-256) para validação de cadeia de confiança
 * - Serialização e parsing de EDNS0 com bit DO (DNSSEC OK)
 * - Validação de tamanhos mínimos de RDATA e tratamento de erros
 * - Suporte a múltiplos registros DNSSEC na mesma resposta
 * 
 * Os testes verificam conformidade com RFC 4034 (DNSSEC) e RFC 6891 (EDNS0),
 * garantindo que o parser consegue interpretar corretamente registros criptográficos
 * essenciais para validação DNSSEC.
 */

#include "dns_resolver/DNSParser.h"
#include "dns_resolver/types.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace dns_resolver;

// ========== Sistema de Contadores e Cores ==========
// Sistema para rastrear resultados dos testes com feedback visual colorido,
// facilitando a identificação de falhas e fornecendo estatísticas finais.

#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

int tests_passed = 0;
int tests_failed = 0;

// ========== Testes de Parsing DNSKEY ==========
// Estes testes verificam se o parser consegue interpretar corretamente
// registros DNSKEY conforme especificado no RFC 4034.

/**
 * Testa parsing de registro DNSKEY KSK (Key Signing Key)
 * Verifica se chaves de assinatura são interpretadas corretamente,
 * incluindo flags (257 = KSK), protocolo (3), algoritmo e chave pública.
 */
void test_parse_dnskey_ksk() {
    std::cout << "  [TEST] Parsing DNSKEY KSK (flags=257)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header DNS (ID=0x1234, QR=1, ANCOUNT=1)
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
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa parsing de registro DNSKEY ZSK (Zone Signing Key)
 * Verifica se chaves de zona são interpretadas corretamente,
 * incluindo flags (256 = ZSK), protocolo (3), algoritmo e chave pública.
 */
void test_parse_dnskey_zsk() {
    std::cout << "  [TEST] Parsing DNSKEY ZSK (flags=256)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header DNS
        buffer.push_back(0x12); buffer.push_back(0x34);
        buffer.push_back(0x81); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        
        // Question: . DNSKEY IN
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
        
        // RDATA: Flags=256 (ZSK), Protocol=3, Algorithm=8, Key=4 bytes
        buffer.push_back(0x01); buffer.push_back(0x00);  // Flags=256 (ZSK)
        buffer.push_back(0x03);
        buffer.push_back(0x08);
        for (int i = 0; i < 4; i++) buffer.push_back(0xBB);
        
        DNSMessage msg = parser.parse(buffer);
        
        assert(msg.answers[0].rdata_dnskey.flags == 256);
        assert(msg.answers[0].rdata_dnskey.isKSK() == false);
        assert(msg.answers[0].rdata_dnskey.isZSK() == true);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Testes de Parsing DS (Delegation Signer) ==========
// Estes testes verificam se o parser consegue interpretar corretamente
// registros DS conforme especificado no RFC 4034.

/**
 * Testa parsing de registro DS com SHA-256 (32 bytes)
 * Verifica se registros DS são interpretados corretamente,
 * incluindo KeyTag, algoritmo, tipo de digest e hash SHA-256.
 */
void test_parse_ds_sha256() {
    std::cout << "  [TEST] Parsing DS SHA-256 (32 bytes)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header DNS
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
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa parsing de registro DS com SHA-1 (20 bytes)
 * Verifica se registros DS com hash SHA-1 são interpretados corretamente,
 * incluindo KeyTag, algoritmo, tipo de digest e hash SHA-1.
 */
void test_parse_ds_sha1() {
    std::cout << "  [TEST] Parsing DS SHA-1 (20 bytes)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header DNS
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
        
        // RDATA: KeyTag=12345, Alg=8, DigestType=1 (SHA-1), Digest=20 bytes
        buffer.push_back(0x30); buffer.push_back(0x39);  // 12345
        buffer.push_back(0x08);
        buffer.push_back(0x01);  // SHA-1
        for (int i = 0; i < 20; i++) buffer.push_back(0xDD);
        
        DNSMessage msg = parser.parse(buffer);
        
        assert(msg.answers[0].rdata_ds.digest_type == 1);
        assert(msg.answers[0].rdata_ds.digest.size() == 20);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Testes de EDNS0 (Extension Mechanisms for DNS) ==========
// Estes testes verificam se o parser consegue serializar e interpretar
// corretamente registros OPT conforme especificado no RFC 6891.

/**
 * Testa serialização de EDNS0 com bit DO (DNSSEC OK) ativado
 * Verifica se o registro OPT é adicionado corretamente à mensagem DNS
 * quando DNSSEC é solicitado, incluindo flags e tamanho UDP.
 */
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
        
        // Ativar EDNS0 com bit DO (DNSSEC OK)
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
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa serialização de EDNS0 com bit DO (DNSSEC OK) desativado
 * Verifica se o registro OPT é adicionado corretamente à mensagem DNS
 * quando EDNS0 é usado mas DNSSEC não é solicitado.
 */
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
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Testes de Validação e Tratamento de Erros ==========
// Estes testes verificam se o parser trata corretamente casos inválidos
// e situações de erro em registros DNSSEC, garantindo robustez e segurança.

/**
 * Testa tratamento de RDATA DNSKEY muito curto (< 4 bytes)
 * Verifica se o parser rejeita corretamente registros DNSKEY que não contêm
 * nem mesmo os campos obrigatórios (flags, protocol, algorithm).
 */
void test_dnskey_rdata_too_short() {
    std::cout << "  [TEST] DNSKEY RDATA muito curto (< 4 bytes)... ";
    
    try {
        DNSParser parser;
        std::vector<uint8_t> buffer;
        
        // Header DNS
        buffer.push_back(0x12); buffer.push_back(0x34);
        buffer.push_back(0x81); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x01);
        buffer.push_back(0x00); buffer.push_back(0x00);
        buffer.push_back(0x00); buffer.push_back(0x00);
        
        // Question: . DNSKEY IN
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
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa tratamento de RDATA DS muito curto (< 4 bytes)
 * Verifica se o parser rejeita corretamente registros DS que não contêm
 * nem mesmo os campos obrigatórios (key_tag, algorithm, digest_type).
 */
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
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa parsing de múltiplos registros DNSKEY (KSK + ZSK)
 * Verifica se o parser consegue processar corretamente mensagens
 * que contêm tanto chaves de assinatura quanto chaves de zona.
 */
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
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa serialização de EDNS0 com tamanho UDP customizado
 * Verifica se o parser consegue usar tamanhos UDP diferentes do padrão
 * conforme especificado no registro OPT.
 */
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
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Função Principal de Testes ==========

/**
 * Função principal que executa todos os testes unitários de registros DNSSEC
 * Organiza os testes em categorias lógicas e fornece feedback detalhado
 * sobre o resultado de cada teste, facilitando a identificação de problemas.
 * 
 * Cobertura de testes:
 * - Parsing de registros DNSKEY (KSK e ZSK)
 * - Parsing de registros DS (SHA-1 e SHA-256)
 * - Serialização e parsing de EDNS0 com bit DO
 * - Validação de tamanhos mínimos de RDATA
 * - Suporte a múltiplos registros DNSSEC
 */
int main() {
    std::cout << "\n==========================================\n";
    std::cout << "  TESTES: DNSSEC Records (Story 3.2)\n";
    std::cout << "==========================================\n\n";
    
    // Testes de Parsing DNSKEY
    std::cout << "→ Testes de Parsing DNSKEY:\n";
    test_parse_dnskey_ksk();
    test_parse_dnskey_zsk();
    test_dnskey_multiple_records();
    
    // Testes de Parsing DS
    std::cout << "\n→ Testes de Parsing DS:\n";
    test_parse_ds_sha256();
    test_parse_ds_sha1();
    
    // Testes de EDNS0
    std::cout << "\n→ Testes de EDNS0:\n";
    test_edns0_serialization_do_set();
    test_edns0_serialization_do_clear();
    test_edns0_udp_size_custom();
    
    // Testes de Validação
    std::cout << "\n→ Testes de Validação:\n";
    test_dnskey_rdata_too_short();
    test_ds_rdata_too_short();
    
    // Resultados Finais
    std::cout << "\n==========================================\n";
    std::cout << "  RESULTADOS FINAIS\n";
    std::cout << "==========================================\n";
    std::cout << "  ✓ Testes passaram: " << tests_passed << "\n";
    std::cout << "  ✗ Testes falharam: " << tests_failed << "\n";
    std::cout << "==========================================\n\n";
    
    if (tests_failed == 0) {
        std::cout << GREEN << " TODOS OS TESTES PASSARAM!\n\n";
        std::cout << "  Total de testes: " << tests_passed << "\n";
        std::cout << "  Cobertura DNSSEC:\n";
        std::cout << "    • Parsing DNSKEY: KSK/ZSK \n";
        std::cout << "    • Parsing DS: SHA-1/SHA-256 \n";
        std::cout << "    • EDNS0: DO=0/1, UDP size \n";
        std::cout << "    • Validação: RDATA size \n";
        std::cout << "    • Múltiplos registros: \n\n" << RESET;
        return 0;
    } else {
        std::cout << RED << " ALGUNS TESTES FALHARAM\n\n";
        std::cout << "  Verifique os logs acima para identificar problemas.\n";
        std::cout << "  Cada teste falhado indica um problema específico no parser DNSSEC.\n\n" << RESET;
        return 1;
    }
}

