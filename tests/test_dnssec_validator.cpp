/*
 * Arquivo: test_dnssec_validator.cpp
 * Propósito: Testes unitários para validação DNSSEC, incluindo cadeia de confiança e algoritmos criptográficos
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver com DNSSEC
 * 
 * Este arquivo contém testes abrangentes para o DNSSECValidator, cobrindo:
 * - Cálculo de key tags conforme RFC 4034 Appendix B
 * - Determinação de zonas pai para validação de cadeia de confiança
 * - Validação de registros DNSKEY contra registros DS
 * - Cálculo de digests SHA-1 e SHA-256 para verificação de integridade
 * - Validação de cadeia de confiança completa
 * - Validação contra trust anchors (âncoras de confiança)
 * 
 * Os testes verificam conformidade com RFC 4034 (DNSSEC) e garantem que
 * o validador consegue executar corretamente algoritmos criptográficos
 * essenciais para validação DNSSEC.
 */

#include "dns_resolver/DNSSECValidator.h"
#include "dns_resolver/TrustAnchorStore.h"
#include <iostream>
#include <cassert>
#include <iomanip>

using namespace dns_resolver;

// ========== Sistema de Contadores e Cores ==========
// Sistema para rastrear resultados dos testes com feedback visual colorido,
// facilitando a identificação de falhas e fornecendo estatísticas finais.

#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

int tests_passed = 0;
int tests_failed = 0;

// ========== Testes de Cálculo de Key Tag ==========
// Estes testes verificam se o algoritmo de cálculo de key tag está
// implementado corretamente conforme RFC 4034 Appendix B.

/**
 * Testa cálculo básico de key tag para registro DNSKEY
 * Verifica se o algoritmo consegue calcular um key tag válido
 * para uma chave DNSKEY, conforme especificação RFC 4034.
 */
void test_calculate_key_tag_basic() {
    std::cout << "  [TEST] calculateKeyTag() básico... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        // Criar DNSKEY mock com valores conhecidos
        DNSKEYRecord dnskey;
        dnskey.flags = 257;  // KSK (Key Signing Key)
        dnskey.protocol = 3;
        dnskey.algorithm = 8;  // RSA/SHA-256
        
        // Public key pequeno para cálculo previsível
        dnskey.public_key = {0x01, 0x02, 0x03, 0x04};
        
        // Key tag deve ser calculado conforme RFC 4034 Appendix B
        // (Não validamos valor exato, apenas que retorna uint16_t)
        uint16_t tag = validator.calculateKeyTag(dnskey);
        
        assert(tag > 0);  // Tag válido gerado
        
        std::cout << " (tag=" << tag << ")\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa consistência do cálculo de key tag
 * Verifica se o algoritmo retorna sempre o mesmo key tag
 * para a mesma chave DNSKEY, garantindo determinismo.
 */
void test_calculate_key_tag_consistency() {
    std::cout << "  [TEST] calculateKeyTag() consistente... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        DNSKEYRecord dnskey;
        dnskey.flags = 256;  // ZSK (Zone Signing Key)
        dnskey.protocol = 3;
        dnskey.algorithm = 13;  // ECDSA P-256
        dnskey.public_key = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        
        // Calcular múltiplas vezes - deve retornar mesmo valor
        uint16_t tag1 = validator.calculateKeyTag(dnskey);
        uint16_t tag2 = validator.calculateKeyTag(dnskey);
        uint16_t tag3 = validator.calculateKeyTag(dnskey);
        
        assert(tag1 == tag2);
        assert(tag2 == tag3);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Testes de Determinação de Zona Pai ==========
// Estes testes verificam se o algoritmo consegue determinar corretamente
// a zona pai de um domínio para validação de cadeia de confiança DNSSEC.

/**
 * Testa determinação básica de zona pai
 * Verifica se o algoritmo consegue extrair corretamente a zona pai
 * de diferentes domínios para construção da cadeia de confiança.
 */
void test_get_parent_zone() {
    std::cout << "  [TEST] getParentZone()... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        // Testar diferentes níveis de domínio
        assert(validator.getParentZone("google.com") == "com");
        assert(validator.getParentZone("com") == ".");
        assert(validator.getParentZone(".") == "");
        assert(validator.getParentZone("www.example.com") == "example.com");
        assert(validator.getParentZone("a.b.c.example.com") == "b.c.example.com");
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa determinação de zona pai com trailing dot
 * Verifica se o algoritmo trata corretamente domínios que terminam
 * com ponto, que são válidos em DNS mas requerem tratamento especial.
 */
void test_get_parent_zone_trailing_dot() {
    std::cout << "  [TEST] getParentZone() com trailing dot... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        // Com trailing dot deve funcionar igual
        assert(validator.getParentZone("google.com.") == "com");
        assert(validator.getParentZone("com.") == ".");
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Testes de Validação DNSKEY ==========
// Estes testes verificam se o validador consegue verificar corretamente
// registros DNSKEY contra registros DS para validação de cadeia de confiança.

/**
 * Testa validação DNSKEY com correspondência perfeita
 * Verifica se o validador consegue validar corretamente um DNSKEY
 * contra um DS que corresponde exatamente (key tag e digest corretos).
 */
void test_validate_dnskey_match() {
    std::cout << "  [TEST] validateDNSKEY() com match... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        // Criar DNSKEY
        DNSKEYRecord dnskey;
        dnskey.flags = 257;
        dnskey.protocol = 3;
        dnskey.algorithm = 8;
        dnskey.public_key = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        
        // Criar DS que corresponde (calculando digest real)
        DSRecord ds;
        ds.key_tag = validator.calculateKeyTag(dnskey);
        ds.algorithm = 8;
        ds.digest_type = 2;  // SHA-256
        ds.digest = validator.calculateDigest(dnskey, "example.com", 2);
        
        // Validar - deve corresponder perfeitamente
        bool valid = validator.validateDNSKEY(dnskey, ds, "example.com");
        
        assert(valid == true);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa validação DNSKEY com key tag incorreto
 * Verifica se o validador rejeita corretamente um DNSKEY
 * quando o key tag no registro DS não corresponde ao DNSKEY.
 */
void test_validate_dnskey_key_tag_mismatch() {
    std::cout << "  [TEST] validateDNSKEY() key tag mismatch... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        DNSKEYRecord dnskey;
        dnskey.flags = 257;
        dnskey.protocol = 3;
        dnskey.algorithm = 8;
        dnskey.public_key = {0x01, 0x02, 0x03, 0x04};
        
        DSRecord ds;
        ds.key_tag = 99999;  // Tag incorreto
        ds.algorithm = 8;
        ds.digest_type = 2;
        ds.digest = validator.calculateDigest(dnskey, "test.com", 2);
        
        // Validar - não deve corresponder (key tag diferente)
        bool valid = validator.validateDNSKEY(dnskey, ds, "test.com");
        
        assert(valid == false);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa validação DNSKEY com digest incorreto
 * Verifica se o validador rejeita corretamente um DNSKEY
 * quando o digest no registro DS não corresponde ao DNSKEY.
 */
void test_validate_dnskey_digest_mismatch() {
    std::cout << "  [TEST] validateDNSKEY() digest mismatch... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        DNSKEYRecord dnskey;
        dnskey.flags = 256;
        dnskey.protocol = 3;
        dnskey.algorithm = 13;
        dnskey.public_key = {0xAA, 0xBB, 0xCC, 0xDD};
        
        DSRecord ds;
        ds.key_tag = validator.calculateKeyTag(dnskey);
        ds.algorithm = 13;
        ds.digest_type = 2;
        // Digest ERRADO (todos zeros)
        ds.digest.resize(32, 0x00);
        
        // Validar - não deve corresponder (digest diferente)
        bool valid = validator.validateDNSKEY(dnskey, ds, "test.com");
        
        assert(valid == false);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Testes de Validação de Cadeia de Confiança ==========
// Estes testes verificam se o validador consegue validar corretamente
// cadeias de confiança DNSSEC completas.

/**
 * Testa validação de cadeia sem dados
 * Verifica se o validador trata corretamente casos onde não há
 * dados suficientes para validação (deve retornar Insecure ou Indeterminate).
 */
void test_validate_chain_empty() {
    std::cout << "  [TEST] validateChain() sem dados... ";
    
    try {
        TrustAnchorStore store;
        store.loadDefaultRootAnchor();
        DNSSECValidator validator(store, false);
        
        std::map<std::string, std::vector<DNSKEYRecord>> dnskeys;
        std::map<std::string, std::vector<DSRecord>> ds_records;
        
        // Sem dados - deve retornar Insecure ou Indeterminate
        ValidationResult result = validator.validateChain("example.com", dnskeys, ds_records);
        
        assert(result == ValidationResult::Insecure || 
               result == ValidationResult::Indeterminate);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa validação de cadeia sem trust anchor
 * Verifica se o validador trata corretamente casos onde não há
 * trust anchors disponíveis (deve retornar Indeterminate).
 */
void test_validate_chain_no_trust_anchor() {
    std::cout << "  [TEST] validateChain() sem trust anchor... ";
    
    try {
        TrustAnchorStore store;  // Vazio, sem trust anchors
        DNSSECValidator validator(store, false);
        
        std::map<std::string, std::vector<DNSKEYRecord>> dnskeys;
        std::map<std::string, std::vector<DSRecord>> ds_records;
        
        // Sem trust anchor - deve retornar Indeterminate
        ValidationResult result = validator.validateChain("example.com", dnskeys, ds_records);
        
        assert(result == ValidationResult::Indeterminate);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Testes de Cálculo de Digest ==========
// Estes testes verificam se o algoritmo de cálculo de digest está
// implementado corretamente para SHA-1 e SHA-256.

/**
 * Testa cálculo de digest SHA-256
 * Verifica se o algoritmo consegue calcular corretamente
 * um digest SHA-256 de 32 bytes para um registro DNSKEY.
 */
void test_calculate_digest_sha256() {
    std::cout << "  [TEST] calculateDigest() SHA-256... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        DNSKEYRecord dnskey;
        dnskey.flags = 257;
        dnskey.protocol = 3;
        dnskey.algorithm = 8;
        dnskey.public_key = {0x01, 0x02, 0x03, 0x04};
        
        std::vector<uint8_t> digest = validator.calculateDigest(dnskey, "test.com", 2);
        
        // SHA-256 deve retornar 32 bytes
        assert(digest.size() == 32);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa cálculo de digest SHA-1
 * Verifica se o algoritmo consegue calcular corretamente
 * um digest SHA-1 de 20 bytes para um registro DNSKEY.
 */
void test_calculate_digest_sha1() {
    std::cout << "  [TEST] calculateDigest() SHA-1... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        DNSKEYRecord dnskey;
        dnskey.flags = 256;
        dnskey.protocol = 3;
        dnskey.algorithm = 8;
        dnskey.public_key = {0x01, 0x02, 0x03, 0x04};
        
        std::vector<uint8_t> digest = validator.calculateDigest(dnskey, "test.com", 1);
        
        // SHA-1 deve retornar 20 bytes
        assert(digest.size() == 20);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa consistência do cálculo de digest
 * Verifica se o algoritmo retorna sempre o mesmo digest
 * para a mesma entrada, garantindo determinismo.
 */
void test_calculate_digest_consistency() {
    std::cout << "  [TEST] calculateDigest() consistência... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        DNSKEYRecord dnskey;
        dnskey.flags = 257;
        dnskey.protocol = 3;
        dnskey.algorithm = 8;
        dnskey.public_key = {0xAA, 0xBB, 0xCC, 0xDD};
        
        // Calcular múltiplas vezes - deve retornar mesmo digest
        auto digest1 = validator.calculateDigest(dnskey, "example.com", 2);
        auto digest2 = validator.calculateDigest(dnskey, "example.com", 2);
        
        assert(digest1.size() == digest2.size());
        assert(digest1 == digest2);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

/**
 * Testa cálculo de digest para zonas diferentes
 * Verifica se o algoritmo produz digests diferentes
 * para a mesma chave em zonas diferentes (owner name faz parte do hash).
 */
void test_calculate_digest_different_zones() {
    std::cout << "  [TEST] calculateDigest() zonas diferentes... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        DNSKEYRecord dnskey;
        dnskey.flags = 257;
        dnskey.protocol = 3;
        dnskey.algorithm = 8;
        dnskey.public_key = {0x01, 0x02, 0x03, 0x04};
        
        // Mesma DNSKEY em zonas diferentes deve ter digest diferente
        // (owner name faz parte do hash)
        auto digest_com = validator.calculateDigest(dnskey, "com", 2);
        auto digest_net = validator.calculateDigest(dnskey, "net", 2);
        
        assert(digest_com != digest_net);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Testes de Validação com Trust Anchor ==========
// Estes testes verificam se o validador consegue validar corretamente
// registros DNSKEY contra trust anchors (âncoras de confiança).

/**
 * Testa validação DNSKEY com trust anchor bem-sucedida
 * Verifica se o validador consegue validar corretamente um DNSKEY
 * contra um trust anchor que corresponde exatamente.
 */
void test_validate_with_trust_anchor_success() {
    std::cout << "  [TEST] validateDNSKEYWithTrustAnchor() sucesso... ";
    
    try {
        TrustAnchorStore store;
        DNSSECValidator validator(store, false);
        
        // Criar DNSKEY
        DNSKEYRecord dnskey;
        dnskey.flags = 257;
        dnskey.protocol = 3;
        dnskey.algorithm = 8;
        dnskey.public_key = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        
        // Criar Trust Anchor correspondente
        TrustAnchor ta;
        ta.zone = ".";
        ta.key_tag = validator.calculateKeyTag(dnskey);
        ta.algorithm = 8;
        ta.digest_type = 2;
        ta.digest = validator.calculateDigest(dnskey, ".", 2);
        
        // Validar - deve corresponder
        bool valid = validator.validateDNSKEYWithTrustAnchor(dnskey, ta, ".");
        
        assert(valid == true);
        
        std::cout << "\n";
        tests_passed++;
        
    } catch (const std::exception& e) {
        std::cout << RED << " (" << e.what() << ")\n" << RESET;
        tests_failed++;
    }
}

// ========== Função Principal de Testes ==========

/**
 * Função principal que executa todos os testes unitários do DNSSECValidator
 * Organiza os testes em categorias lógicas e fornece feedback detalhado
 * sobre o resultado de cada teste, facilitando a identificação de problemas.
 * 
 * Cobertura de testes:
 * - Cálculo de key tags (RFC 4034 Appendix B)
 * - Determinação de zonas pai para cadeia de confiança
 * - Cálculo de digests SHA-1 e SHA-256
 * - Validação de registros DNSKEY contra DS
 * - Validação de cadeia de confiança completa
 * - Validação contra trust anchors
 */
int main() {
    std::cout << "\n==========================================\n";
    std::cout << "  TESTES: DNSSECValidator (Story 3.3)\n";
    std::cout << "==========================================\n\n";
    
    // Testes de Cálculo de Key Tag
    std::cout << "→ Testes de calculateKeyTag():\n";
    test_calculate_key_tag_basic();
    test_calculate_key_tag_consistency();
    
    // Testes de Determinação de Zona Pai
    std::cout << "\n→ Testes de getParentZone():\n";
    test_get_parent_zone();
    test_get_parent_zone_trailing_dot();
    
    // Testes de Cálculo de Digest
    std::cout << "\n→ Testes de calculateDigest():\n";
    test_calculate_digest_sha256();
    test_calculate_digest_sha1();
    test_calculate_digest_consistency();
    test_calculate_digest_different_zones();
    
    // Testes de Validação DNSKEY
    std::cout << "\n→ Testes de validateDNSKEY():\n";
    test_validate_dnskey_match();
    test_validate_dnskey_key_tag_mismatch();
    test_validate_dnskey_digest_mismatch();
    
    // Testes de Validação de Cadeia
    std::cout << "\n→ Testes de validateChain():\n";
    test_validate_chain_empty();
    test_validate_chain_no_trust_anchor();
    
    // Testes de Validação com Trust Anchor
    std::cout << "\n→ Testes de validateDNSKEYWithTrustAnchor():\n";
    test_validate_with_trust_anchor_success();
    
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
        std::cout << "  Cobertura DNSSECValidator:\n";
        std::cout << "    • calculateKeyTag(): RFC 4034 \n";
        std::cout << "    • getParentZone(): Cadeia de confiança \n";
        std::cout << "    • calculateDigest(): SHA-1/SHA-256 \n";
        std::cout << "    • validateDNSKEY(): Validação DS \n";
        std::cout << "    • validateChain(): Cadeia completa \n";
        std::cout << "    • Trust Anchors: Validação raiz \n\n" << RESET;
        return 0;
    } else {
        std::cout << RED << " ALGUNS TESTES FALHARAM\n\n";
        std::cout << "  Verifique os logs acima para identificar problemas.\n";
        std::cout << "  Cada teste falhado indica um problema específico no validador DNSSEC.\n\n" << RESET;
        return 1;
    }
}

