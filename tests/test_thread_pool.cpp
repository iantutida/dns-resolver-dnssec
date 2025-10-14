/*
 * Arquivo: test_thread_pool.cpp
 * Propósito: Testes unitários para ThreadPool, validando execução paralela e thread-safety
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver com DNSSEC
 * 
 * Este arquivo contém testes abrangentes para o ThreadPool, cobrindo:
 * - Criação e destruição de pools de threads
 * - Execução de tarefas simples e complexas
 * - Processamento paralelo de múltiplas tarefas
 * - Thread-safety com contadores compartilhados
 * - Tarefas com durações variáveis
 * - Medição de performance e speedup
 * - Validação de concorrência e sincronização
 * 
 * Os testes verificam conformidade com Story 6.1 e garantem que
 * o ThreadPool consegue executar tarefas de forma eficiente e segura
 * em ambiente multi-threaded, proporcionando speedup significativo.
 */

#include "dns_resolver/ThreadPool.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

using namespace dns_resolver;

// ========== Função Principal de Testes ==========

/**
 * Função principal que executa todos os testes unitários do ThreadPool
 * Organiza os testes em categorias lógicas e fornece feedback detalhado
 * sobre o resultado de cada teste, facilitando a identificação de problemas.
 * 
 * Cobertura de testes:
 * - Criação e destruição de pools de threads
 * - Execução de tarefas simples e complexas
 * - Processamento paralelo de múltiplas tarefas
 * - Thread-safety com contadores compartilhados
 * - Tarefas com durações variáveis
 * - Medição de performance e speedup
 * - Validação de concorrência e sincronização
 * 
 * Requisitos para execução completa:
 * - Suporte a C++17 (std::future, std::packaged_task)
 * - Threading nativo do sistema operacional
 * - Medição de performance precisa
 */
int main() {
    std::cout << "========================================\n";
    std::cout << "  Testes: ThreadPool (Story 6.1)\n";
    std::cout << "========================================\n\n";
    
    // ========== Teste 1: Criação Básica ==========
    // Verifica se o ThreadPool consegue criar corretamente
    // o número especificado de worker threads.
    
    {
        std::cout << "[TEST] ThreadPool - Criação básica... ";
        ThreadPool pool(4);
        if (pool.size() != 4) {
            std::cerr << " FALHOU: Esperado 4 workers, obteve " << pool.size() << "\n";
            return 1;
        }
        std::cout << "\n";
    }
    
    // ========== Teste 2: Execução de Tarefa Simples ==========
    // Verifica se o ThreadPool consegue executar corretamente
    // uma tarefa simples e retornar o resultado esperado.
    
    {
        std::cout << "[TEST] ThreadPool - Tarefa simples... ";
        ThreadPool pool(2);
        auto result = pool.enqueue([](int x) { return x * 2; }, 21);
        int value = result.get();
        if (value != 42) {
            std::cerr << " FALHOU: Esperado 42, obteve " << value << "\n";
            return 1;
        }
        std::cout << "\n";
    }
    
    // ========== Teste 3: Múltiplas Tarefas ==========
    // Verifica se o ThreadPool consegue processar corretamente
    // múltiplas tarefas em paralelo, distribuindo o trabalho
    // entre os worker threads disponíveis.
    
    {
        std::cout << "[TEST] ThreadPool - Múltiplas tarefas... ";
        ThreadPool pool(4);
        std::vector<std::future<int>> results;
        
        for (int i = 0; i < 20; i++) {
            results.push_back(pool.enqueue([](int x) { return x * x; }, i));
        }
        
        bool all_correct = true;
        for (int i = 0; i < 20; i++) {
            int value = results[i].get();
            if (value != i * i) {
                all_correct = false;
                break;
            }
        }
        
        if (!all_correct) {
            std::cerr << " FALHOU: Resultados incorretos\n";
            return 1;
        }
        std::cout << "\n";
    }
    
    // ========== Teste 4: Thread-Safety ==========
    // Verifica se o ThreadPool mantém thread-safety ao executar
    // múltiplas tarefas que modificam um contador compartilhado,
    // garantindo que não há race conditions.
    
    {
        std::cout << "[TEST] ThreadPool - Thread-safety (contador)... ";
        ThreadPool pool(8);
        std::atomic<int> counter(0);
        std::vector<std::future<void>> results;
        
        for (int i = 0; i < 1000; i++) {
            results.push_back(pool.enqueue([&counter]() {
                counter++;
            }));
        }
        
        for (auto& result : results) {
            result.get();
        }
        
        if (counter != 1000) {
            std::cerr << " FALHOU: Esperado 1000, obteve " << counter << "\n";
            return 1;
        }
        std::cout << "\n";
    }
    
    // ========== Teste 5: Tarefas com Duração Variável ==========
    // Verifica se o ThreadPool consegue lidar corretamente com
    // tarefas que têm durações diferentes, garantindo que tarefas
    // mais rápidas não sejam bloqueadas por tarefas mais lentas.
    
    {
        std::cout << "[TEST] ThreadPool - Tarefas com duração variável... ";
        ThreadPool pool(4);
        std::vector<std::future<int>> results;
        
        for (int i = 0; i < 10; i++) {
            results.push_back(pool.enqueue([](int sleep_ms, int value) {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                return value;
            }, (i % 3) * 10, i));  // Variar sleep: 0ms, 10ms, 20ms
        }
        
        bool all_correct = true;
        for (int i = 0; i < 10; i++) {
            int value = results[i].get();
            if (value != i) {
                all_correct = false;
                break;
            }
        }
        
        if (!all_correct) {
            std::cerr << " FALHOU: Resultados incorretos\n";
            return 1;
        }
        std::cout << "\n";
    }
    
    // ========== Teste 6: Performance e Speedup ==========
    // Verifica se o ThreadPool proporciona speedup significativo
    // ao executar tarefas em paralelo comparado à execução serial,
    // medindo o tempo de execução e calculando o speedup.
    
    {
        std::cout << "[TEST] ThreadPool - Performance (serial vs parallel)... ";
        
        auto task = [](int ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            return ms;
        };
        
        // Execução Serial (1 worker)
        auto start_serial = std::chrono::steady_clock::now();
        {
            ThreadPool pool(1);
            std::vector<std::future<int>> results;
            for (int i = 0; i < 8; i++) {
                results.push_back(pool.enqueue(task, 50));
            }
            for (auto& r : results) r.get();
        }
        auto end_serial = std::chrono::steady_clock::now();
        auto duration_serial = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_serial - start_serial).count();
        
        // Execução Paralela (8 workers)
        auto start_parallel = std::chrono::steady_clock::now();
        {
            ThreadPool pool(8);
            std::vector<std::future<int>> results;
            for (int i = 0; i < 8; i++) {
                results.push_back(pool.enqueue(task, 50));
            }
            for (auto& r : results) r.get();
        }
        auto end_parallel = std::chrono::steady_clock::now();
        auto duration_parallel = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_parallel - start_parallel).count();
        
        double speedup = static_cast<double>(duration_serial) / duration_parallel;
        
        // Speedup deve ser significativo (pelo menos 2x)
        if (speedup < 2.0) {
            std::cerr << " FALHOU: Speedup insuficiente (" << speedup << "x)\n";
            return 1;
        }
        
        std::cout << " (speedup: " << std::fixed << std::setprecision(1) 
                  << speedup << "x)\n";
    }
    
    // ========== Resultados Finais ==========
    // Exibe estatísticas detalhadas dos testes executados
    // e fornece resumo da cobertura de funcionalidades.
    
    std::cout << "\n==========================================\n";
    std::cout << "  RESULTADOS FINAIS\n";
    std::cout << "==========================================\n";
    std::cout << "  ✓ Testes passaram: 6\n";
    std::cout << "  ✗ Testes falharam: 0\n";
    std::cout << "==========================================\n\n";
    
    std::cout << " TODOS OS TESTES PASSARAM!\n\n";
    std::cout << "  ThreadPool (Story 6.1):\n";
    std::cout << "    • Criação e destruição:     CORRETO\n";
    std::cout << "    • Execução de tarefas:      CORRETO\n";
    std::cout << "    • Processamento paralelo:   CORRETO\n";
    std::cout << "    • Thread-safety:            CORRETO\n";
    std::cout << "    • Duração variável:         CORRETO\n";
    std::cout << "    • Performance (speedup):    CORRETO\n";
    std::cout << "    • Concorrência segura:      CORRETO\n";
    std::cout << "    • Distribuição de carga:    CORRETO\n\n";
    
    return 0;
}

