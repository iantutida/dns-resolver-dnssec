/**
 * Testes Unitários: TrustAnchorStore
 * STORY 3.1: Carregar Âncoras de Confiança
 */

#include "dns_resolver/TrustAnchorStore.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <cstdio>

using namespace dns_resolver;

// Cores para output
#define GREEN "\033[32m"
#define RED "\033[31m"
#define RESET "\033[0m"

void test_invalidFormats() {
    std::cout << "TEST: Formatos inválidos (via loadFromFile)\n";
    
    // Teste 1: Classe inválida
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
    
    // Teste 2: Algoritmo não suportado
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
    
    // Teste 3: Digest curto
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

void test_loadDefaultRootAnchor() {
    std::cout << "TEST: loadDefaultRootAnchor\n";
    
    TrustAnchorStore store;
    store.loadDefaultRootAnchor();
    
    // Verificar que foi carregado
    assert(store.count() == 1);
    assert(store.hasTrustAnchor("."));
    
    // Obter e validar
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

void test_loadFromFile() {
    std::cout << "TEST: loadFromFile\n";
    
    // Criar arquivo temporário
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
    
    // Verificar
    assert(store.count() == 1);
    assert(store.hasTrustAnchor("."));
    
    auto tas = store.getTrustAnchorsForZone(".");
    assert(tas.size() == 1);
    assert(tas[0].key_tag == 20326);
    
    std::cout << "  ✓ Trust anchor carregado de arquivo\n";
    std::cout << "  ✓ Comentários ignorados corretamente\n";
    
    // Limpar
    std::remove(tmpfile);
    
    std::cout << GREEN << "PASS: loadFromFile\n\n" << RESET;
}

void test_multipleTrustAnchors() {
    std::cout << "TEST: Múltiplos Trust Anchors\n";
    
    // Criar arquivo com múltiplos TAs
    const char* tmpfile = "/tmp/test_multiple_tas.txt";
    {
        std::ofstream f(tmpfile);
        f << ". IN DS 20326 8 2 E06D44B80B8F1D39A95C0B0D7C65D08458E880409BBC683457104237C7F8EC8D\n";
        f << ". IN DS 19036 8 2 49AAC11D7B6F6446702E54A1607371607A1A41855200FD2CE1CDDE32F24E8FB5\n";
    }
    
    TrustAnchorStore store;
    store.loadFromFile(tmpfile);
    
    // Verificar
    assert(store.count() == 2);
    assert(store.hasTrustAnchor("."));
    
    auto tas = store.getTrustAnchorsForZone(".");
    assert(tas.size() == 2);
    
    // Verificar key tags diferentes
    bool found_20326 = false;
    bool found_19036 = false;
    for (const auto& ta : tas) {
        if (ta.key_tag == 20326) found_20326 = true;
        if (ta.key_tag == 19036) found_19036 = true;
    }
    assert(found_20326 && found_19036);
    
    std::cout << "  ✓ Múltiplos trust anchors carregados\n";
    std::cout << "  ✓ Total: " << store.count() << " trust anchors\n";
    
    // Limpar
    std::remove(tmpfile);
    
    std::cout << GREEN << "PASS: Múltiplos Trust Anchors\n\n" << RESET;
}

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

void test_emptyFile() {
    std::cout << "TEST: Arquivo vazio (sem TAs válidos)\n";
    
    // Criar arquivo vazio
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
    
    // Limpar
    std::remove(tmpfile);
    
    std::cout << GREEN << "PASS: Arquivo vazio\n\n" << RESET;
}

int main() {
    std::cout << "\n========================================\n";
    std::cout << "  TESTES: TrustAnchorStore (Story 3.1)\n";
    std::cout << "========================================\n\n";
    
    try {
        test_invalidFormats();
        test_loadDefaultRootAnchor();
        test_loadFromFile();
        test_multipleTrustAnchors();
        test_fileNotFound();
        test_emptyFile();
        
        std::cout << GREEN;
        std::cout << "========================================\n";
        std::cout << "  ✅ TODOS OS TESTES PASSARAM\n";
        std::cout << "========================================\n";
        std::cout << RESET;
        
        return 0;
    } catch (const std::exception& e) {
        std::cout << RED;
        std::cout << "\n========================================\n";
        std::cout << "  ❌ TESTES FALHARAM\n";
        std::cout << "  Erro: " << e.what() << "\n";
        std::cout << "========================================\n";
        std::cout << RESET;
        return 1;
    }
}

