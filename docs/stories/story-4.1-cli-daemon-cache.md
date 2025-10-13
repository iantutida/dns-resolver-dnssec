# STORY 4.1: CLI Completa para Daemon de Cache

**Epic:** EPIC 4 - Subsistema de Cache  
**Status:** ✅ Done  
**Prioridade:** Alta (Primeira Story EPIC 4 - Fundação do Cache)  
**Estimativa:** 3-4 dias  
**Tempo Real:** 1 hora  
**QA Score:** 5.0/5 ⭐⭐⭐⭐⭐ (Aprovado - ver `story-4.1-test-report-quinn.md`)

---

## User Story
Como um administrador, quero uma CLI completa para gerenciar o ciclo de vida e as políticas do daemon de cache, para que eu possa controlar o cache DNS de forma eficiente e flexível.

---

## Contexto e Motivação

### Por que um Daemon de Cache Externo?

**Problema:** Resolução DNS é lenta
- Cada query requer múltiplas iterações (root → TLD → authoritative)
- DNSSEC adiciona overhead (validação criptográfica)
- Latência total: ~300-500ms por query

**Solução:** Cache DNS
- Armazenar respostas recentes
- Reutilizar respostas válidas (dentro do TTL)
- Latência com cache: ~1-5ms (100x mais rápido!)

### Por que Daemon Separado?

```
┌──────────────────────────────────────────────────────┐
│  Resolvedor (Aplicação Cliente)                      │
│  ├─ Faz queries DNS                                  │
│  ├─ Valida DNSSEC                                    │
│  └─ Consulta cache via IPC                           │
└───────────────┬──────────────────────────────────────┘
                │ IPC (Unix Socket)
┌───────────────┴──────────────────────────────────────┐
│  Cache Daemon (Aplicação Servidora)                  │
│  ├─ Roda em background (persistente)                 │
│  ├─ Armazena respostas (positivas e negativas)       │
│  └─ Gerenciado via CLI (activate/deactivate/etc)     │
└──────────────────────────────────────────────────────┘
```

**Vantagens:**
- ✅ Cache persiste entre execuções do resolvedor
- ✅ Múltiplos processos podem compartilhar o cache
- ✅ Isolamento (crash do resolvedor não afeta cache)
- ✅ Gerenciamento independente (CLI dedicada)

---

## Objetivos

### Objetivo Principal
Criar aplicação daemon de cache com CLI completa para gerenciamento de ciclo de vida e políticas.

### Objetivos Secundários
- Implementar processo daemon (background, PID file)
- Criar comandos de ciclo de vida (activate, deactivate, status)
- Criar comandos de gerenciamento (set, purge, list, flush)
- Implementar armazenamento em memória (mapas thread-safe)
- Comunicação via Unix Domain Socket
- Configuração de tamanho de cache (positivo/negativo)

---

## Requisitos Funcionais

### RF1: Comandos de Ciclo de Vida

- **RF1.1:** `--activate`: Iniciar daemon em background
  - Fork processo
  - Criar PID file (`/tmp/dns_cache.pid`)
  - Iniciar listener IPC
  - Retornar sucesso

- **RF1.2:** `--deactivate`: Parar daemon
  - Ler PID file
  - Enviar SIGTERM
  - Remover PID file
  - Retornar sucesso

- **RF1.3:** `--status`: Verificar status do daemon
  - Verificar se PID file existe
  - Verificar se processo está rodando
  - Retornar: `Running (PID: XXXX)` ou `Not running`

- **RF1.4:** `--flush`: Limpar todo o cache
  - Enviar comando via IPC
  - Cache positivo e negativo limpos
  - Retornar número de entradas removidas

### RF2: Comandos de Gerenciamento de Cache

- **RF2.1:** `--set positive <N>`: Configurar tamanho do cache positivo
  - Alocar espaço para N entradas
  - Padrão: 50 entradas

- **RF2.2:** `--set negative <N>`: Configurar tamanho do cache negativo
  - Alocar espaço para N entradas
  - Padrão: 50 entradas

- **RF2.3:** `--purge positive`: Limpar apenas cache positivo

- **RF2.4:** `--purge negative`: Limpar apenas cache negativo

- **RF2.5:** `--purge all`: Limpar ambos (igual --flush)

- **RF2.6:** `--list positive`: Listar entradas do cache positivo

- **RF2.7:** `--list negative`: Listar entradas do cache negativo

- **RF2.8:** `--list all`: Listar todas as entradas

### RF3: Estrutura do Daemon

- **RF3.1:** Classe `CacheDaemon`:
  ```cpp
  class CacheDaemon {
  public:
      void run();  // Loop principal
      void stop();
      
  private:
      void handleClient(int client_socket);
      void processCommand(const std::string& cmd);
      
      // Armazenamento
      std::map<DNSQuestion, CacheEntry> positive_cache_;
      std::map<DNSQuestion, CacheEntry> negative_cache_;
      
      // Thread-safety
      std::mutex cache_mutex_;
      
      // Configuração
      size_t max_positive_entries_ = 50;
      size_t max_negative_entries_ = 50;
  };
  ```

### RF4: Comunicação IPC

- **RF4.1:** Usar Unix Domain Socket (`/tmp/dns_cache.sock`)
- **RF4.2:** Protocolo simples baseado em texto:
  ```
  Cliente → Daemon:
    QUERY|qname|qtype|qclass
    SET_POSITIVE|N
    PURGE|positive
    LIST|all
  
  Daemon → Cliente:
    HIT|<serialized_response>
    MISS
    OK|<message>
    ERROR|<message>
  ```

### RF5: Estrutura de Cache Entry

- **RF5.1:** Struct `CacheEntry`:
  ```cpp
  struct CacheEntry {
      DNSMessage response;
      time_t timestamp;
      uint32_t ttl;
      
      bool isExpired() const {
          return (time(nullptr) - timestamp) > ttl;
      }
  };
  ```

### RF6: Interface CLI (cache_daemon)

- **RF6.1:** Executável separado: `build/cache_daemon`
- **RF6.2:** Aceitar comandos via argumentos:
  ```bash
  ./cache_daemon --activate
  ./cache_daemon --status
  ./cache_daemon --set positive 100
  ./cache_daemon --list all
  ./cache_daemon --deactivate
  ```

---

## Requisitos Não-Funcionais

### RNF1: Performance
- Lookup no cache: O(log N) com std::map
- Thread-safety: mutex para acesso concorrente
- IPC via Unix socket (mais rápido que TCP)

### RNF2: Robustez
- Daemon não deve crashear se cliente desconecta
- PID file garante apenas 1 instância
- Limpeza de recursos ao parar

### RNF3: Usabilidade
- Comandos intuitivos
- Mensagens de erro claras
- Status detalhado

---

## Critérios de Aceitação

### CA1: Ciclo de Vida
- [x] `--activate` inicia daemon em background
- [x] PID file criado
- [x] `--status` mostra status correto
- [x] `--deactivate` para daemon gracefully

### CA2: Gerenciamento
- [x] `--set positive N` altera tamanho
- [x] `--set negative N` altera tamanho
- [x] `--purge positive/negative/all` funciona
- [x] `--list positive/negative/all` exibe entradas

### CA3: Daemon
- [x] Daemon roda em background
- [x] Unix socket criado
- [x] Aceita conexões
- [x] Thread-safe (mutex)

### CA4: Testes
- [x] Iniciar e parar daemon
- [x] Alterar tamanho de cache
- [x] Listar cache vazio
- [x] Sem memory leaks

---

## Design Técnico

### Estrutura de Diretórios

```
dns_resolver/
├── src/
│   ├── resolver/        # Resolvedor (existente)
│   └── daemon/          # NOVO: Daemon de cache
│       ├── main.cpp
│       ├── CacheDaemon.cpp
│       └── CacheDaemon.h
└── build/
    ├── resolver         # Executável resolvedor
    └── cache_daemon     # NOVO: Executável daemon
```

### main.cpp do Daemon

```cpp
// src/daemon/main.cpp
#include "CacheDaemon.h"
#include <iostream>
#include <fstream>
#include <csignal>

const char* PID_FILE = "/tmp/dns_cache.pid";

void activate() {
    // Fork para background
    pid_t pid = fork();
    if (pid < 0) exit(1);
    if (pid > 0) exit(0);  // Pai termina
    
    // Processo filho (daemon)
    // Salvar PID
    std::ofstream pidfile(PID_FILE);
    pidfile << getpid();
    pidfile.close();
    
    // Iniciar daemon
    CacheDaemon daemon;
    daemon.run();
}

void deactivate() {
    std::ifstream pidfile(PID_FILE);
    if (!pidfile.is_open()) {
        std::cerr << "Daemon not running" << std::endl;
        exit(1);
    }
    
    pid_t pid;
    pidfile >> pid;
    pidfile.close();
    
    kill(pid, SIGTERM);
    unlink(PID_FILE);
    
    std::cout << "Daemon stopped" << std::endl;
}

void status() {
    std::ifstream pidfile(PID_FILE);
    if (!pidfile.is_open()) {
        std::cout << "Daemon: Not running" << std::endl;
        return;
    }
    
    pid_t pid;
    pidfile >> pid;
    
    // Verificar se processo existe
    if (kill(pid, 0) == 0) {
        std::cout << "Daemon: Running (PID: " << pid << ")" << std::endl;
    } else {
        std::cout << "Daemon: Not running (stale PID file)" << std::endl;
        unlink(PID_FILE);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: cache_daemon [--activate|--deactivate|--status|...]" << std::endl;
        return 1;
    }
    
    std::string cmd = argv[1];
    
    if (cmd == "--activate") {
        activate();
    } else if (cmd == "--deactivate") {
        deactivate();
    } else if (cmd == "--status") {
        status();
    }
    // ... outros comandos
    
    return 0;
}
```

---

## Casos de Teste

### CT1: Iniciar Daemon
```bash
./cache_daemon --activate
./cache_daemon --status
# Esperado: Daemon: Running (PID: XXXX)
```

### CT2: Parar Daemon
```bash
./cache_daemon --deactivate
./cache_daemon --status
# Esperado: Daemon: Not running
```

### CT3: Configurar Tamanho
```bash
./cache_daemon --activate
./cache_daemon --set positive 100
./cache_daemon --list positive
# Esperado: Cache positivo: 0/100 entradas
```

---

## Definition of Done (DoD)

- [x] Executável `cache_daemon` compilável
- [x] Comandos de ciclo de vida funcionando
- [x] Daemon roda em background
- [x] PID file criado/removido
- [x] Unix socket criado
- [x] Thread-safe (mutex)
- [x] Testes manuais: activate, status, deactivate

---

## 🚀 Início do EPIC 4!

Esta é a **primeira story do EPIC 4: Subsistema de Cache**!

Após EPIC 4, o resolvedor terá **performance otimizada** com cache distribuído! 🚀

---

## Dev Agent Record

### Tasks Checklist
- [x] Criar estrutura de diretórios src/daemon/
- [x] Implementar CacheDaemon.h (classe base)
- [x] Implementar CacheDaemon.cpp (armazenamento, IPC)
- [x] Implementar main.cpp do daemon (CLI activate/deactivate/status)
- [x] Atualizar Makefile para compilar cache_daemon
- [x] Testar activate e status
- [x] Testar deactivate
- [x] Testar comandos de gerenciamento (set, list, flush)

### Debug Log
Nenhuma alteração temporária necessária. Implementação direta sem bugs.

### Completion Notes

**Implementação:**

**1. Estrutura Criada:**
- `src/daemon/CacheDaemon.h`: Classe daemon com cache thread-safe
- `src/daemon/CacheDaemon.cpp`: Implementação com Unix socket e IPC
- `src/daemon/main.cpp`: CLI para gerenciamento do daemon
- Makefile atualizado com targets para cache_daemon

**2. Funcionalidades Implementadas:**

**Ciclo de Vida:**
- `--activate`: Fork para background, PID file, Unix socket
- `--deactivate`: SIGTERM, cleanup de PID file
- `--status`: Verificação de processo ativo + estatísticas

**Gerenciamento:**
- `--set positive/negative N`: Configurar tamanho de cache
- `--purge positive/negative/all`: Limpar cache específico
- `--list positive/negative/all`: Listar entradas
- `--flush`: Limpar todo o cache

**3. Características Técnicas:**
- Unix Domain Socket: `/tmp/dns_cache.sock`
- PID file: `/tmp/dns_cache.pid`
- Thread-safety: `std::mutex` para acesso concorrente
- Cache positivo e negativo separados
- Cleanup automático de entradas expiradas
- Protocolo IPC baseado em texto (COMMAND|args → OK|response)

**4. RAII e Robustez:**
- PKEYGuard para EVP_PKEY (Story 3.6, reutilizado)
- Cleanup de recursos no destrutor
- Detecção de PID file obsoleto
- Verificação de processo com kill(pid, 0)

**Testes Manuais:**
- ✅ `--activate`: Daemon iniciado (PID: 77104) ✓
- ✅ `--status`: Exibe PID e estatísticas ✓
- ✅ `--set positive 100`: Configuração aplicada ✓
- ✅ `--list all`: Lista cache vazio ✓
- ✅ `--flush`: Limpa cache (0 entradas) ✓
- ✅ `--deactivate`: Daemon parado ✓
- ✅ `--status` (após deactivate): "Not running" ✓

**Testes Automatizados:**
- ✅ 266 testes passando (100%)
- ✅ Compilação sem erros
- ✅ Sem regressões

**Estatísticas:**
- Arquivos criados: 3
  - `src/daemon/CacheDaemon.h` (149 linhas)
  - `src/daemon/CacheDaemon.cpp` (241 linhas)
  - `src/daemon/main.cpp` (274 linhas)
- Arquivos modificados: 1
  - `Makefile` (+11 linhas)
- Total de código: 675 linhas
  - CacheDaemon: 390 linhas
  - CLI main: 274 linhas
  - Makefile: 11 linhas
- Tempo real: 1 hora
- Complexidade: Média (IPC, fork, signals)

**Funcionalidades Prontas:**
- ✅ Daemon em background (fork)
- ✅ PID file management
- ✅ Unix socket IPC
- ✅ Thread-safe cache (mutex)
- ✅ Comandos de ciclo de vida completos
- ✅ Comandos de gerenciamento funcionais
- ✅ Cleanup automático de entradas expiradas

**Próximos Passos (EPIC 4):**
- Story 4.2: Integração do cache no ResolverEngine
- Story 4.3: Cache de respostas positivas
- Story 4.4: Cache de respostas negativas (NXDOMAIN/NODATA)

### Change Log
Nenhuma mudança de requisitos durante implementação.

