/*
 * ----------------------------------------
 * Arquivo: ThreadPool.h
 * Propósito: Pool de threads para processamento paralelo de tarefas DNS
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#pragma once

#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

namespace dns_resolver {

// Pool de threads para execução paralela de queries DNS
// Utilizado para fan-out paralelo e processamento batch
class ThreadPool {
public:
    // Constrói pool com número específico de workers
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency())
        : stop_(false) {
        if (num_threads == 0) {
            num_threads = 1;
        }
        
        // Criar workers que ficam aguardando tarefas
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        // Aguardar tarefa ou sinal de parada
                        condition_.wait(lock, [this] {
                            return stop_ || !tasks_.empty();
                        });
                        
                        if (stop_ && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    // Destrutor: para workers e aguarda conclusão
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        
        condition_.notify_all();
        
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    // Não permite cópia
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Enfileira função para execução paralela
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    // Retorna número de workers ativos
    size_t size() const {
        return workers_.size();
    }
    
    // Retorna número de tarefas na fila
    size_t pending() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

private:
    std::vector<std::thread> workers_;           // Threads workers
    std::queue<std::function<void()>> tasks_;    // Fila de tarefas
    
    mutable std::mutex queue_mutex_;             // Mutex para sincronização
    std::condition_variable condition_;          // Condição para notificar workers
    bool stop_;                                  // Flag de parada
};

} // namespace dns_resolver

