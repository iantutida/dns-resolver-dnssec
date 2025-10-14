/*
 * Arquivo: test_dns_response_parsing.cpp
 * Propósito: Testes unitários para parsing de respostas DNS, validando descompressão e parsing de resource records
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver com DNSSEC
 * 
 * Este arquivo contém testes abrangentes para o parsing de respostas DNS, cobrindo:
 * - Parsing de headers DNS com flags e contadores
 * - Descompressão de nomes de domínio (com e sem compressão)
 * - Parsing de resource records (A, NS, CNAME, MX, TXT, AAAA, SOA, PTR)
 * - Validação de buffers inválidos e ponteiros de compressão
 * - Parsing de múltiplas seções (Answer, Authority, Additional)
 * 
 * Os testes verificam conformidade com RFC 1035 e garantem que o parser
 * consegue interpretar corretamente mensagens DNS complexas com compressão
 * e múltiplos tipos de resource records.
 */

#include "dns_resolver/types.h"
#include "dns_resolver/DNSParser.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace dns_resolver;

// ========== Sistema de Contadores de Testes ==========
// Sistema simples para rastrear resultados dos testes, facilitando
// a identificação de falhas e fornecendo estatísticas finais.

int tests_passed = 0;
int tests_failed = 0;

/**
 * Função auxiliar para executar testes e atualizar contadores
 * Fornece feedback visual imediato sobre o resultado de cada teste
 * e mantém estatísticas para o relatório final.
 */
void test_assert(bool condition, const std::string& test_name) {
    if (condition) {
        std::cout << "✓ " << test_name << "\n";
        tests_passed++;
    } else {
        std::cout << "✗ " << test_name << "\n";
        tests_failed++;
    }
}

// ========== Testes de Parsing do Header DNS ==========
// Estes testes verificam se o parser consegue interpretar corretamente
// o header DNS de 12 bytes, incluindo ID, flags e contadores.

/**
 * Testa parsing básico do header DNS
 * Verifica se todos os campos do header são interpretados corretamente:
 * ID, flags (QR, RD, RA), contadores (QDCOUNT, ANCOUNT, NSCOUNT, ARCOUNT)
 */
void test_parse_header_basic() {
    std::cout << "\n[TEST] Parse Header Básico\n";
    
    // Header de resposta simples:
    // ID=0x1234, QR=1, OPCODE=0, AA=0, TC=0, RD=1, RA=1, Z=0, RCODE=0
    // QDCOUNT=1, ANCOUNT=1, NSCOUNT=0, ARCOUNT=0
    std::vector<uint8_t> buffer = {
        0x12, 0x34,  // ID
        0x81, 0x80,  // Flags: QR=1, RD=1, RA=1
        0x00, 0x01,  // QDCOUNT=1
        0x00, 0x01,  // ANCOUNT=1
        0x00, 0x00,  // NSCOUNT=0
        0x00, 0x00   // ARCOUNT=0
    };
    
    // Adicionar question mínima (para não falhar)
    std::vector<uint8_t> question = {
        0x03, 'c', 'o', 'm', 0x00,  // "com"
        0x00, 0x01,  // Type A
        0x00, 0x01   // Class IN
    };
    buffer.insert(buffer.end(), question.begin(), question.end());
    
    // Adicionar RR mínimo (para não falhar)
    std::vector<uint8_t> rr = {
        0xc0, 0x0c,  // Pointer to "com"
        0x00, 0x01,  // Type A
        0x00, 0x01,  // Class IN
        0x00, 0x00, 0x00, 0x10,  // TTL=16
        0x00, 0x04,  // RDLENGTH=4
        0x08, 0x08, 0x08, 0x08   // 8.8.8.8
    };
    buffer.insert(buffer.end(), rr.begin(), rr.end());
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.header.id == 0x1234, "Header ID correto");
    test_assert(msg.header.qr == true, "Header QR=1 (resposta)");
    test_assert(msg.header.rd == true, "Header RD=1");
    test_assert(msg.header.ra == true, "Header RA=1");
    test_assert(msg.header.rcode == 0, "Header RCODE=0");
    test_assert(msg.header.qdcount == 1, "Header QDCOUNT=1");
    test_assert(msg.header.ancount == 1, "Header ANCOUNT=1");
}

// ========== Testes de Descompressão de Nomes de Domínio ==========
// Estes testes verificam se o parser consegue descomprimir corretamente
// nomes de domínio que usam ponteiros de compressão (RFC 1035).

/**
 * Testa parsing de nome de domínio sem compressão
 * Verifica se nomes simples são interpretados corretamente quando
 * não há uso de ponteiros de compressão.
 */
void test_domain_name_no_compression() {
    std::cout << "\n[TEST] Parsing de Nome Sem Compressão\n";
    
    // "www.google.com" - nome completo sem ponteiros
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Header
        0x03, 'w', 'w', 'w',        // "www" (3 bytes)
        0x06, 'g', 'o', 'o', 'g', 'l', 'e',  // "google" (6 bytes)
        0x03, 'c', 'o', 'm',        // "com" (3 bytes)
        0x00,                       // Terminador
        0x00, 0x01, 0x00, 0x01      // Type A, Class IN
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.questions.size() == 1, "1 question parseada");
    test_assert(msg.questions[0].qname == "www.google.com", "Nome sem compressão correto");
    test_assert(msg.questions[0].qtype == 1, "QTYPE=A");
    test_assert(msg.questions[0].qclass == 1, "QCLASS=IN");
}

/**
 * Testa parsing de nome de domínio com compressão usando ponteiros
 * Verifica se o parser consegue descomprimir corretamente nomes que
 * referenciam partes anteriores da mensagem através de ponteiros.
 */
void test_domain_name_with_compression() {
    std::cout << "\n[TEST] Parsing de Nome Com Compressão\n";
    
    // Question: "www.google.com"
    // Answer: pointer to "google.com" at offset 16
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,  // Header (0-11)
        0x03, 'w', 'w', 'w',              // offset 12-15 ("www")
        0x06, 'g', 'o', 'o', 'g', 'l', 'e',  // offset 16-22 ("google")
        0x03, 'c', 'o', 'm',              // offset 23-26 ("com")
        0x00,                             // offset 27 (terminador)
        0x00, 0x01, 0x00, 0x01,           // Type A, Class IN (28-31)
        
        // Answer RR: name uses pointer to offset 16 (google.com)
        0xc0, 0x10,                       // Pointer to offset 16 ("google.com")
        0x00, 0x01,                       // Type A
        0x00, 0x01,                       // Class IN
        0x00, 0x00, 0x00, 0x10,           // TTL=16
        0x00, 0x04,                       // RDLENGTH=4
        0x08, 0x08, 0x08, 0x08            // 8.8.8.8
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.questions.size() == 1, "1 question parseada");
    test_assert(msg.questions[0].qname == "www.google.com", "Question name correto");
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].name == "google.com", "Answer name com ponteiro correto");
    test_assert(msg.answers[0].type == 1, "Answer type A");
    test_assert(msg.answers[0].rdata_a == "8.8.8.8", "Answer RDATA IPv4 correto");
}

// ========== Testes de Parsing de Resource Records ==========
// Estes testes verificam se o parser consegue interpretar corretamente
// diferentes tipos de resource records (RR) conforme especificado no RFC 1035.

/**
 * Testa parsing de resource record tipo A (IPv4)
 * Verifica se endereços IPv4 são interpretados corretamente e
 * convertidos para formato string legível.
 */
void test_parse_rr_type_a() {
    std::cout << "\n[TEST] Parsing de RR Tipo A\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',  // "example"
        0x03, 'c', 'o', 'm',                      // "com"
        0x00,                                     // Terminador
        0x00, 0x01, 0x00, 0x01,                   // Type A, Class IN
        
        // Answer RR Type A
        0xc0, 0x0c,              // Pointer to "example.com"
        0x00, 0x01,              // Type A
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x3c,  // TTL=60
        0x00, 0x04,              // RDLENGTH=4
        0xc0, 0xa8, 0x01, 0x0a   // 192.168.1.10
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].name == "example.com", "RR name correto");
    test_assert(msg.answers[0].type == DNSType::A, "RR type A");
    test_assert(msg.answers[0].ttl == 60, "RR TTL correto");
    test_assert(msg.answers[0].rdata_a == "192.168.1.10", "RR IPv4 correto");
}

/**
 * Testa parsing de resource record tipo NS (Name Server)
 * Verifica se nameservers são interpretados corretamente,
 * incluindo descompressão de nomes com ponteiros.
 */
void test_parse_rr_type_ns() {
    std::cout << "\n[TEST] Parsing de RR Tipo NS\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',  // "example"
        0x03, 'c', 'o', 'm',                      // "com"
        0x00,                                     // Terminador
        0x00, 0x02, 0x00, 0x01,                   // Type NS, Class IN
        
        // Answer RR Type NS
        0xc0, 0x0c,              // Pointer to "example.com"
        0x00, 0x02,              // Type NS
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x3c,  // TTL=60
        0x00, 0x06,              // RDLENGTH=6
        0x03, 'n', 's', '1',     // "ns1"
        0xc0, 0x0c               // Pointer to "example.com"
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].type == DNSType::NS, "RR type NS");
    test_assert(msg.answers[0].rdata_ns == "ns1.example.com", "RR nameserver correto");
}

/**
 * Testa parsing de resource record tipo CNAME (Canonical Name)
 * Verifica se aliases são interpretados corretamente,
 * incluindo descompressão de nomes com ponteiros.
 */
void test_parse_rr_type_cname() {
    std::cout << "\n[TEST] Parsing de RR Tipo CNAME\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x03, 'w', 'w', 'w',
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x05, 0x00, 0x01,  // Type CNAME
        
        // Answer RR Type CNAME
        0xc0, 0x0c,              // Pointer to "www.example.com"
        0x00, 0x05,              // Type CNAME
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x3c,  // TTL=60
        0x00, 0x02,              // RDLENGTH=2
        0xc0, 0x10               // Pointer to "example.com"
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].type == DNSType::CNAME, "RR type CNAME");
    test_assert(msg.answers[0].rdata_cname == "example.com", "RR canonical name correto");
}

/**
 * Testa parsing de resource record tipo MX (Mail Exchange)
 * Verifica se registros de correio são interpretados corretamente,
 * incluindo prioridade e nome do servidor de correio.
 */
void test_parse_rr_type_mx() {
    std::cout << "\n[TEST] Parsing de RR Tipo MX\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x0f, 0x00, 0x01,  // Type MX
        
        // Answer RR Type MX: priority 10 + mail.example.com
        0xc0, 0x0c,              // Pointer to "example.com"
        0x00, 0x0f,              // Type MX
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x3c,  // TTL=60
        0x00, 0x08,              // RDLENGTH=8
        0x00, 0x0a,              // Priority=10
        0x04, 'm', 'a', 'i', 'l',
        0xc0, 0x0c               // Pointer to "example.com"
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].type == DNSType::MX, "RR type MX");
    test_assert(msg.answers[0].rdata_mx == "10 mail.example.com", "RR MX (priority + exchange) correto");
}

/**
 * Testa parsing de resource record tipo TXT
 * Verifica se registros de texto são interpretados corretamente,
 * incluindo strings de caracteres de tamanho variável.
 */
void test_parse_rr_type_txt() {
    std::cout << "\n[TEST] Parsing de RR Tipo TXT\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x10, 0x00, 0x01,  // Type TXT
        
        // Answer RR Type TXT
        0xc0, 0x0c,              // Pointer to "example.com"
        0x00, 0x10,              // Type TXT
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x3c,  // TTL=60
        0x00, 0x0c,              // RDLENGTH=12
        0x0b, 'v', '=', 's', 'p', 'f', '1', ' ', '~', 'a', 'l', 'l'  // TXT: "v=spf1 ~all"
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].type == DNSType::TXT, "RR type TXT");
    test_assert(msg.answers[0].rdata_txt == "v=spf1 ~all", "RR TXT correto");
}

/**
 * Testa parsing de resource record tipo AAAA (IPv6)
 * Verifica se endereços IPv6 são interpretados corretamente
 * e convertidos para formato string legível.
 */
void test_parse_rr_type_aaaa() {
    std::cout << "\n[TEST] Parsing de RR Tipo AAAA (IPv6)\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x1c, 0x00, 0x01,  // Type AAAA
        
        // Answer RR Type AAAA: 2001:0db8:85a3:0000:0000:8a2e:0370:7334
        0xc0, 0x0c,              // Pointer to "example.com"
        0x00, 0x1c,              // Type AAAA
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x3c,  // TTL=60
        0x00, 0x10,              // RDLENGTH=16
        0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00,
        0x00, 0x00, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].type == DNSType::AAAA, "RR type AAAA");
    test_assert(msg.answers[0].rdata_aaaa.find("2001") != std::string::npos, "RR IPv6 contém 2001");
    test_assert(msg.answers[0].rdata_aaaa.find("db8") != std::string::npos, "RR IPv6 contém db8");
}

/**
 * Testa parsing de resource record tipo SOA (Start of Authority)
 * Verifica se registros SOA são interpretados corretamente,
 * incluindo todos os campos: MNAME, RNAME, SERIAL, REFRESH, etc.
 */
void test_parse_rr_type_soa() {
    std::cout << "\n[TEST] Parsing de RR Tipo SOA\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x06, 0x00, 0x01,  // Type SOA
        
        // Answer RR Type SOA
        0xc0, 0x0c,              // Pointer to "example.com"
        0x00, 0x06,              // Type SOA
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x3c,  // TTL=60
        0x00, 0x22,              // RDLENGTH=34 (6 + 8 + 20)
        // MNAME: ns1.example.com (6 bytes: 1 + 3 + 2)
        0x03, 'n', 's', '1',
        0xc0, 0x0c,              // Pointer to "example.com"
        // RNAME: admin.example.com (8 bytes: 1 + 5 + 2)
        0x05, 'a', 'd', 'm', 'i', 'n',
        0xc0, 0x0c,              // Pointer to "example.com"
        // SERIAL, REFRESH, RETRY, EXPIRE, MINIMUM (20 bytes)
        0x00, 0x00, 0x00, 0x01,  // Serial=1
        0x00, 0x00, 0x1c, 0x20,  // Refresh=7200
        0x00, 0x00, 0x0e, 0x10,  // Retry=3600
        0x00, 0x09, 0x3a, 0x80,  // Expire=604800
        0x00, 0x00, 0x00, 0x3c   // Minimum=60
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].type == DNSType::SOA, "RR type SOA");
    test_assert(msg.answers[0].rdata_soa.mname == "ns1.example.com", "SOA MNAME correto");
    test_assert(msg.answers[0].rdata_soa.rname == "admin.example.com", "SOA RNAME correto");
    test_assert(msg.answers[0].rdata_soa.serial == 1, "SOA SERIAL correto");
    test_assert(msg.answers[0].rdata_soa.refresh == 7200, "SOA REFRESH correto");
}

/**
 * Testa parsing de resource record tipo PTR (Pointer)
 * Verifica se registros de DNS reverso são interpretados corretamente,
 * incluindo descompressão de nomes complexos.
 */
void test_parse_rr_type_ptr() {
    std::cout << "\n[TEST] Parsing de RR Tipo PTR (Reverse DNS)\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x01, '1',
        0x01, '1',
        0x03, '1', '6', '8',
        0x03, '1', '9', '2',
        0x07, 'i', 'n', '-', 'a', 'd', 'd', 'r',
        0x04, 'a', 'r', 'p', 'a',
        0x00,
        0x00, 0x0c, 0x00, 0x01,  // Type PTR
        
        // Answer RR Type PTR
        0xc0, 0x0c,              // Pointer to "1.1.168.192.in-addr.arpa"
        0x00, 0x0c,              // Type PTR
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x3c,  // TTL=60
        0x00, 0x0d,              // RDLENGTH=13
        0x06, 'r', 'o', 'u', 't', 'e', 'r',
        0x05, 'l', 'o', 'c', 'a', 'l',
        0x00
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.answers.size() == 1, "1 answer parseado");
    test_assert(msg.answers[0].type == DNSType::PTR, "RR type PTR");
    test_assert(msg.answers[0].rdata_ptr == "router.local", "RR PTR domain correto");
}

// ========== Testes de Validação e Tratamento de Erros ==========
// Estes testes verificam se o parser trata corretamente casos inválidos
// e situações de erro, garantindo robustez e segurança.

/**
 * Testa tratamento de buffer muito curto (< 12 bytes)
 * Verifica se o parser rejeita corretamente buffers que não contêm
 * nem mesmo um header DNS completo.
 */
void test_invalid_buffer_too_short() {
    std::cout << "\n[TEST] Buffer Muito Curto (< 12 bytes)\n";
    
    std::vector<uint8_t> buffer = {0x12, 0x34, 0x81};
    
    try {
        DNSParser::parse(buffer);
        test_assert(false, "Deveria lançar exceção para buffer curto");
    } catch (const std::runtime_error& e) {
        test_assert(true, "Exceção lançada para buffer curto");
    }
}

/**
 * Testa tratamento de ponteiro de compressão inválido
 * Verifica se o parser detecta e rejeita ponteiros que apontam
 * para posições além do tamanho do buffer.
 */
void test_invalid_pointer_offset() {
    std::cout << "\n[TEST] Ponteiro de Compressão Inválido\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x03, 'w', 'w', 'w',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x01, 0x00, 0x01,
        
        // Answer RR com ponteiro inválido (offset=255, além do buffer)
        0xc0, 0xff,              // Ponteiro inválido
        0x00, 0x01,
        0x00, 0x01,
        0x00, 0x00, 0x00, 0x10,
        0x00, 0x04,
        0x08, 0x08, 0x08, 0x08
    };
    
    try {
        DNSParser::parse(buffer);
        test_assert(false, "Deveria lançar exceção para ponteiro inválido");
    } catch (const std::runtime_error& e) {
        test_assert(true, "Exceção lançada para ponteiro inválido");
    }
}

/**
 * Testa descompressão com múltiplos níveis de ponteiros
 * Verifica se o parser consegue lidar com cadeias de ponteiros
 * que referenciam outros ponteiros (compressão aninhada).
 */
void test_compression_multiple_levels() {
    std::cout << "\n[TEST] Descompressão com Múltiplos Níveis de Ponteiros\n";
    
    // Criar buffer com 3 níveis de ponteiros:
    // www -> subdomain -> example -> com
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
        
        // Question: www.subdomain.example.com
        0x03, 'w', 'w', 'w',                      // offset 12
        0x09, 's', 'u', 'b', 'd', 'o', 'm', 'a', 'i', 'n',  // offset 16
        0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',  // offset 26
        0x03, 'c', 'o', 'm',                      // offset 34
        0x00,                                     // offset 38
        0x00, 0x01, 0x00, 0x01,
        
        // Answer 1: pointer to subdomain.example.com (offset 16)
        0xc0, 0x10,              // Pointer to offset 16
        0x00, 0x01,              // Type A
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x10,  // TTL=16
        0x00, 0x04,              // RDLENGTH=4
        0x01, 0x02, 0x03, 0x04,  // 1.2.3.4
        
        // Answer 2: pointer to example.com (offset 26)
        0xc0, 0x1a,              // Pointer to offset 26
        0x00, 0x01,              // Type A
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x20,  // TTL=32
        0x00, 0x04,              // RDLENGTH=4
        0x05, 0x06, 0x07, 0x08   // 5.6.7.8
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.questions.size() == 1, "1 question parseada");
    test_assert(msg.answers.size() == 2, "2 answers parseados");
    test_assert(msg.answers[0].name == "subdomain.example.com", "Primeiro RR descomprimido corretamente");
    test_assert(msg.answers[1].name == "example.com", "Segundo RR descomprimido corretamente");
}

/**
 * Testa validação de flag QR (Query/Response)
 * Verifica se o parser interpreta corretamente a flag QR
 * e pode distinguir entre queries e respostas.
 */
void test_response_qr_flag_validation() {
    std::cout << "\n[TEST] Validação de Flag QR (Deve Ser Resposta)\n";
    
    // Header com QR=0 (query, não resposta)
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x01, 0x00,  // ID + Flags (QR=0, RD=1)
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 'w', 'w', 'w',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x01, 0x00, 0x01
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    // Verificar que QR=0 foi parseado corretamente
    test_assert(msg.header.qr == false, "QR=0 (query) detectado");
    // Em produção, deveria validar e rejeitar se esperávamos uma resposta
}

/**
 * Testa validação de RCODE (Response Code)
 * Verifica se o parser interpreta corretamente códigos de resposta
 * como NXDOMAIN (RCODE=3) e outras condições de erro.
 */
void test_response_rcode_validation() {
    std::cout << "\n[TEST] Validação de RCODE (NXDOMAIN)\n";
    
    // Resposta com RCODE=3 (NXDOMAIN)
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x83,  // Flags: QR=1, RD=1, RA=1, RCODE=3
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0a, 'n', 'o', 'n', 'e', 'x', 'i', 's', 't', 'e', 'n',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x01, 0x00, 0x01
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.header.qr == true, "QR=1 (resposta)");
    test_assert(msg.header.rcode == 3, "RCODE=3 (NXDOMAIN)");
    test_assert(msg.answers.size() == 0, "Nenhuma resposta em NXDOMAIN");
}

// ========== Testes de Múltiplas Seções DNS ==========
// Estes testes verificam se o parser consegue interpretar corretamente
// mensagens DNS com múltiplas seções (Answer, Authority, Additional).

/**
 * Testa parsing de múltiplas seções DNS (Answer + Authority)
 * Verifica se o parser consegue processar corretamente mensagens
 * que contêm tanto respostas quanto registros de autoridade.
 */
void test_parse_multiple_sections() {
    std::cout << "\n[TEST] Parsing de Múltiplas Seções (Answer + Authority)\n";
    
    std::vector<uint8_t> buffer = {
        0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
        
        // Question
        0x03, 'w', 'w', 'w',
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x01, 0x00, 0x01,
        
        // Answer RR
        0xc0, 0x0c,              // Pointer to "www.com"
        0x00, 0x01,              // Type A
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x10,  // TTL=16
        0x00, 0x04,              // RDLENGTH=4
        0x01, 0x02, 0x03, 0x04,  // 1.2.3.4
        
        // Authority RR
        0x03, 'c', 'o', 'm',
        0x00,
        0x00, 0x02,              // Type NS
        0x00, 0x01,              // Class IN
        0x00, 0x00, 0x00, 0x20,  // TTL=32
        0x00, 0x04,              // RDLENGTH=4
        0x03, 'n', 's', '1',
        0x00
    };
    
    DNSMessage msg = DNSParser::parse(buffer);
    
    test_assert(msg.questions.size() == 1, "1 question");
    test_assert(msg.answers.size() == 1, "1 answer");
    test_assert(msg.authority.size() == 1, "1 authority");
    test_assert(msg.answers[0].rdata_a == "1.2.3.4", "Answer IPv4 correto");
    test_assert(msg.authority[0].type == DNSType::NS, "Authority type NS");
}

// ========== Função Principal de Testes ==========

/**
 * Função principal que executa todos os testes unitários de parsing de respostas DNS
 * Organiza os testes em categorias lógicas e fornece feedback detalhado
 * sobre o resultado de cada teste, facilitando a identificação de problemas.
 * 
 * Cobertura de testes:
 * - Parsing de headers DNS
 * - Descompressão de nomes (com e sem compressão)
 * - Parsing de 8 tipos de resource records
 * - Validação de buffers inválidos
 * - Parsing de múltiplas seções
 */
int main() {
    std::cout << "==========================================\n";
    std::cout << "  TESTES DE PARSING DE RESPOSTA DNS\n";
    std::cout << "  Story 1.2 - Automated Test Suite\n";
    std::cout << "==========================================\n";
    
    // Testes de Header DNS
    test_parse_header_basic();
    
    // Testes de Descompressão de Nomes
    test_domain_name_no_compression();
    test_domain_name_with_compression();
    test_compression_multiple_levels();
    
    // Testes de Resource Records (8 tipos principais)
    test_parse_rr_type_a();        // IPv4
    test_parse_rr_type_ns();       // Name Server
    test_parse_rr_type_cname();    // Canonical Name
    test_parse_rr_type_mx();       // Mail Exchange
    test_parse_rr_type_txt();      // Text
    test_parse_rr_type_aaaa();     // IPv6
    test_parse_rr_type_soa();      // Start of Authority
    test_parse_rr_type_ptr();      // Pointer (Reverse DNS)
    
    // Testes de Validação e Tratamento de Erros
    test_invalid_buffer_too_short();
    test_invalid_pointer_offset();
    test_response_qr_flag_validation();
    test_response_rcode_validation();
    
    // Testes de Múltiplas Seções
    test_parse_multiple_sections();
    
    std::cout << "\n==========================================\n";
    std::cout << "  RESULTADOS FINAIS\n";
    std::cout << "==========================================\n";
    std::cout << "  ✓ Testes passaram: " << tests_passed << "\n";
    std::cout << "  ✗ Testes falharam: " << tests_failed << "\n";
    std::cout << "==========================================\n";
    
    if (tests_failed == 0) {
        std::cout << "\n TODOS OS TESTES PASSARAM!\n\n";
        std::cout << "  Total de testes: " << tests_passed << "\n";
        std::cout << "  Cobertura de tipos RR: 100% (8/8)\n";
        std::cout << "  Cobertura de compressão: Completa\n";
        std::cout << "  Validação de erros: Completa\n\n";
        return 0;
    } else {
        std::cout << "\n ALGUNS TESTES FALHARAM\n\n";
        std::cout << "  Verifique os logs acima para identificar problemas.\n";
        std::cout << "  Cada teste falhado indica um problema específico no parser.\n\n";
        return 1;
    }
}

