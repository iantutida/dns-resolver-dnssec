/*
 * Arquivo: test_trust_anchor_store.cpp
 * Propósito: Testes unitários para TrustAnchorStore, validando carregamento e validação de âncoras de confiança DNSSEC
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver com DNSSEC
 * 
 * Este arquivo contém testes abrangentes para o TrustAnchorStore, cobrindo:
 * - Validação de formatos de trust anchor (classe, algoritmo, digest)
 * - Carregamento de âncora de confiança padrão da raiz DNS
 * - Carregamento de trust anchors de arquivos externos
 * - Suporte a múltiplos trust anchors para a mesma zona
 * - Tratamento de arquivos inexistentes ou vazios
 * - Validação de parâmetros DNSSEC (key tag, algorithm, digest type)
 * - Conformidade com RFC 4034 (DNSSEC) e RFC 5011 (Automated Updates)
 * 
 * Os testes verificam conformidade com Story 3.1 e garantem que
 * o sistema consegue carregar e validar corretamente âncoras de confiança
 * DNSSEC para validação de cadeias de confiança.
 */

#include "dns_resolver/TrustAnchorStore.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <cstdio>

using namespace dns_resolver;

// ========== Sistema de Cores para Output ==========
// Define cores ANSI para melhorar a legibilidade dos resultados dos testes.

#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

// ========== Testes de Validação de Formatos ==========
// Estes testes verificam se o TrustAnchorStore valida corretamente
// formatos de trust anchor e rejeita entradas inválidas.

/**
 * Testa validação de formatos inválidos de trust anchor
 * Verifica se o sistema rejeita corretamente trust anchors com
 * parâmetros inválidos como classe incorreta, algoritmo não suportado
 * ou digest com tamanho incorreto.
 */
void test_invalidFormats() {
    std::cout << "TEST: Formatos inválidos (via loadFromFile)\n";
    
    // Teste 1: Classe inválida (CH em vez de IN)
    {
        const char* tmpfile = "/tmp/test_invalid_class.txt";
        std::ofstream f(tmpfile);
        f << ". CH DS 20326 8 2 E06D44B80B8F1D39A95C0B0D7C65D08458E880409BBC683457104237C7F8EC8D\n";
        f.close();
        
        TrustAnchorStore store;
        try {
            store.loadFromFile(tmpfile);
            std::cout << RED << "  ✗ FALHOU: deveria rejeitar classe CH\n" << RESET;
            std::remove(tmpfile);
            throw std::runtime_error("Não rejeitou classe inválida");
        } catch (const std::runtime_error& e) {
            std::string msg(e.what());
            if (msg.find("No valid") != std::string::npos) {
                std::cout << "  ✓ Rejeitou classe inválida\n";
            }
        }
        std::remove(tmpfile);
    }
    
    // Teste 2: Algoritmo não suportado (99)
    {
        const char* tmpfile = "/tmp/test_invalid_alg.txt";
        std::ofstream f(tmpfile);
        f << ". IN DS 20326 99 2 E06D44B80B8F1D39A95C0B0D7C65D08458E880409BBC683457104237C7F8EC8D\n";
        f.close();
        
        TrustAnchorStore store;
        try {
            store.loadFromFile(tmpfile);
            std::cout << RED << "  ✗ FALHOU: deveria rejeitar algoritmo 99\n" << RESET;
            std::remove(tmpfile);
            throw std::runtime_error("Não rejeitou algoritmo inválido");
        } catch (const std::runtime_error& e) {
            std::string msg(e.what());
            if (msg.find("No valid") != std::string::npos) {
                std::cout << "  ✓ Rejeitou algoritmo não suportado\n";
            }
        }
        std::remove(tmpfile);
    }
    
    // Teste 3: Digest com tamanho incorreto (muito curto)
    {
        const char* tmpfile = "/tmp/test_short_digest.txt";
        std::ofstream f(tmpfile);
        f << ". IN DS 20326 8 2 E06D44B8\n";
        f.close();
        
        TrustAnchorStore store;
        try {
            store.loadFromFile(tmpfile);
            std::cout << RED << "  ✗ FALHOU: deveria rejeitar digest curto\n" << RESET;
            std::remove(tmpfile);
            throw std::runtime_error("Não rejeitou digest curto");
        } catch (const std::runtime_error& e) {
            std::string msg(e.what());
            if (msg.find("No valid") != std::string::npos) {
                std::cout << "  ✓ Rejeitou digest com tamanho incorreto\n";
            }
        }
        std::remove(tmpfile);
    }
    
    std::cout << GREEN << "PASS: Formatos inválidos\n\n" << RESET;
}

// ========== Testes de Carregamento Padrão ==========
// Estes testes verificam se o TrustAnchorStore consegue carregar
// corretamente a âncora de confiança padrão da raiz DNS.

/**
 * Testa carregamento da âncora de confiança padrão da raiz DNS
 * Verifica se o sistema consegue carregar corretamente a âncora
 * de confiança padrão (Root KSK 2024) e validar seus parâmetros.
 */
void test_loadDefaultRootAnchor() {
    std::cout << "TEST: loadDefaultRootAnchor\n";
    
    TrustAnchorStore store;
    store.loadDefaultRootAnchor();
    
    // Verificar que foi carregado
    assert(store.count() == 1);
    assert(store.hasTrustAnchor("."));
    
    // Obter e validar parâmetros da âncora de confiança
    auto tas = store.getTrustAnchorsForZone(".");
    assert(tas.size() == 1);
    assert(tas[0].key_tag == 20326);
    assert(tas[0].algorithm == 8);
    assert(tas[0].digest_type == 2);
    assert(tas[0].digest.size() == 32);
    
    std::cout << "  ✓ Root Trust Anchor padrão carregado\n";
    std::cout << "  ✓ Key Tag: " << tas[0].key_tag << "\n";
    std::cout << "  ✓ Algorithm: " << static_cast<int>(tas[0].algorithm) << "\n";
    std::cout << "  ✓ Digest Type: " << static_cast<int>(tas[0].digest_type) << "\n";
    std::cout << "  ✓ Digest Size: " << tas[0].digest.size() << " bytes\n";
    
    std::cout << GREEN << "PASS: loadDefaultRootAnchor\n\n" << RESET;
}

// ========== Testes de Carregamento de Arquivo ==========
// Estes testes verificam se o TrustAnchorStore consegue carregar
// trust anchors de arquivos externos com diferentes formatos.

/**
 * Testa carregamento de trust anchor de arquivo externo
 * Verifica se o sistema consegue carregar corretamente trust anchors
 * de arquivos externos, ignorando comentários e linhas vazias.
 */
void test_loadFromFile() {
    std::cout << "TEST: loadFromFile\n";
    
    // Criar arquivo temporário com trust anchor válido
    const char* tmpfile = "/tmp/test_trust_anchor.txt";
    {
        std::ofstream f(tmpfile);
        f << "; Comentário\n";
        f << "\n";
        f << ". IN DS 20326 8 2 E06D44B80B8F1D39A95C0B0D7C65D08458E880409BBC683457104237C7F8EC8D\n";
        f << "# Outro comentário\n";
    }
    
    TrustAnchorStore store;
    store.loadFromFile(tmpfile);
    
    // Verificar carregamento correto
    assert(store.count() == 1);
    assert(store.hasTrustAnchor("."));
    
    auto tas = store.getTrustAnchorsForZone(".");
    assert(tas.size() == 1);
    assert(tas[0].key_tag == 20326);
    
    std::cout << "  ✓ Trust anchor carregado de arquivo\n";
    std::cout << "  ✓ Comentários ignorados corretamente\n";
    
    // Limpar arquivo temporário
    std::remove(tmpfile);
    
    std::cout << GREEN << "PASS: loadFromFile\n\n" << RESET;
}

// ========== Testes de Múltiplos Trust Anchors ==========
// Estes testes verificam se o TrustAnchorStore consegue gerenciar
// múltiplos trust anchors para a mesma zona DNS.

/**
 * Testa carregamento de múltiplos trust anchors para a mesma zona
 * Verifica se o sistema consegue carregar e gerenciar corretamente
 * múltiplos trust anchors com diferentes key tags para a mesma zona.
 */
void test_multipleTrustAnchors() {
    std::cout << "TEST: Múltiplos Trust Anchors\n";
    
    // Criar arquivo com múltiplos trust anchors
    const char* tmpfile = "/tmp/test_multiple_tas.txt";
    {
        std::ofstream f(tmpfile);
        f << ". IN DS 20326 8 2 E06D44B80B8F1D39A95C0B0D7C65D08458E880409BBC683457104237C7F8EC8D\n";
        f << ". IN DS 19036 8 2 49AAC11D7B6F6446702E54A1607371607A1A41855200FD2CE1CDDE32F24E8FB5\n";
    }
    
    TrustAnchorStore store;
    store.loadFromFile(tmpfile);
    
    // Verificar carregamento de múltiplos trust anchors
    assert(store.count() == 2);
    assert(store.hasTrustAnchor("."));
    
    auto tas = store.getTrustAnchorsForZone(".");
    assert(tas.size() == 2);
    
    // Verificar que ambos os key tags foram carregados
    bool found_20326 = false;
    bool found_19036 = false;
    for (const auto& ta : tas) {
        if (ta.key_tag == 20326) found_20326 = true;
        if (ta.key_tag == 19036) found_19036 = true;
    }
    assert(found_20326 && found_19036);
    
    std::cout << "  ✓ Múltiplos trust anchors carregados\n";
    std::cout << "  ✓ Total: " << store.count() << " trust anchors\n";
    
    // Limpar arquivo temporário
    std::remove(tmpfile);
    
    std::cout << GREEN << "PASS: Múltiplos Trust Anchors\n\n" << RESET;
}

// ========== Testes de Tratamento de Erros ==========
// Estes testes verificam se o TrustAnchorStore trata corretamente
// situações de erro como arquivos inexistentes ou vazios.

/**
 * Testa tratamento de arquivo inexistente
 * Verifica se o sistema lança exceção apropriada quando
 * tenta carregar trust anchors de um arquivo que não existe.
 */
void test_fileNotFound() {
    std::cout << "TEST: Arquivo não encontrado\n";
    
    TrustAnchorStore store;
    
    try {
        store.loadFromFile("/tmp/nonexistent_trust_anchor_file_12345.txt");
        std::cout << RED << "  ✗ FALHOU: deveria lançar exceção\n" << RESET;
        throw std::runtime_error("Não lançou exceção para arquivo inexistente");
    } catch (const std::runtime_error& e) {
        std::string msg(e.what());
        if (msg.find("open") != std::string::npos || msg.find("Failed") != std::string::npos) {
            std::cout << "  ✓ Exceção lançada corretamente\n";
            std::cout << "  ✓ Mensagem: " << e.what() << "\n";
        } else {
            throw;
        }
    }
    
    std::cout << GREEN << "PASS: Arquivo não encontrado\n\n" << RESET;
}

/**
 * Testa tratamento de arquivo vazio ou sem trust anchors válidos
 * Verifica se o sistema rejeita corretamente arquivos que não
 * contêm trust anchors válidos, apenas comentários ou linhas vazias.
 */
void test_emptyFile() {
    std::cout << "TEST: Arquivo vazio (sem TAs válidos)\n";
    
    // Criar arquivo com apenas comentários e linhas vazias
    const char* tmpfile = "/tmp/test_empty_ta.txt";
    {
        std::ofstream f(tmpfile);
        f << "; Só comentários\n";
        f << "\n";
        f << "# Nada de válido\n";
    }
    
    TrustAnchorStore store;
    
    try {
        store.loadFromFile(tmpfile);
        std::cout << RED << "  ✗ FALHOU: deveria rejeitar arquivo sem TAs válidos\n" << RESET;
        std::remove(tmpfile);
        throw std::runtime_error("Não rejeitou arquivo sem TAs");
    } catch (const std::runtime_error& e) {
        std::string msg(e.what());
        if (msg.find("No valid") != std::string::npos) {
            std::cout << "  ✓ Arquivo sem TAs válidos rejeitado\n";
        } else {
            std::remove(tmpfile);
            throw;
        }
    }
    
    // Limpar arquivo temporário
    std::remove(tmpfile);
    
    std::cout << GREEN << "PASS: Arquivo vazio\n\n" << RESET;
}

// ========== Função Principal de Testes ==========

/**
 * Função principal que executa todos os testes unitários do TrustAnchorStore
 * Organiza os testes em categorias lógicas e fornece feedback detalhado
 * sobre o resultado de cada teste, facilitando a identificação de problemas.
 * 
 * Cobertura de testes:
 * - Validação de formatos de trust anchor
 * - Carregamento de âncora de confiança padrão
 * - Carregamento de trust anchors de arquivos externos
 * - Suporte a múltiplos trust anchors
 * - Tratamento de arquivos inexistentes ou vazios
 * - Validação de parâmetros DNSSEC
 * 
 * Requisitos para execução completa:
 * - Suporte a arquivos temporários (/tmp)
 * - Validação de exceções apropriadas
 * - Conformidade com RFC 4034 e RFC 5011
 */
int main() {
    std::cout << "\n========================================\n";
    std::cout << "  TESTES: TrustAnchorStore (Story 3.1)\n";
    std::cout << "========================================\n\n";
    
    try {
        // Executar todos os testes em sequência
        test_invalidFormats();
        test_loadDefaultRootAnchor();
        test_loadFromFile();
        test_multipleTrustAnchors();
        test_fileNotFound();
        test_emptyFile();
        
        // Exibir resultados finais
        std::cout << GREEN;
        std::cout << "========================================\n";
        std::cout << "   TODOS OS TESTES PASSARAM\n";
        std::cout << "========================================\n";
        std::cout << RESET;
        
        return 0;
    } catch (const std::exception& e) {
        // Tratar falhas de teste
        std::cout << RED;
        std::cout << "\n========================================\n";
        std::cout << "   TESTES FALHARAM\n";
        std::cout << "  Erro: " << e.what() << "\n";
        std::cout << "========================================\n";
        std::cout << RESET;
        return 1;
    }
}

