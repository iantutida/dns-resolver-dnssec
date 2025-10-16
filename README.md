# DNS Resolver - Trabalho de Redes

Implementação de um resolvedor DNS em C++17 com suporte a resolução iterativa, TCP fallback, DNS-over-TLS (DoT), validação DNSSEC, e cache distribuído.

---

## O que o projeto faz

Este é um resolvedor DNS completo que consegue resolver nomes de domínio de forma iterativa, seguindo o processo padrão: root servers -> TLD servers -> servidores autoritativos. Além disso, implementa varias funcionalidades avançadas como criptografia, validação de segurança e cache para melhorar a performance.

### Funcionalidades principais

**Resolução DNS Iterativa**
- Resolve domínios desde os root servers até os servidores autoritativos
- Segue automaticamente delegações (registros NS)
- Otimização com glue records
- Segue registros CNAME (incluindo cross-domain)
- Detecta respostas negativas (NXDOMAIN, NODATA)

**Segurança DNSSEC**
- Validação criptográfica completa (RSA/SHA-256, ECDSA P-256/SHA-256)
- Chain of trust (root -> TLD -> domain)
- Trust anchors (Root KSK 2024)
- Detecção de ataques (Bogus detection)
- AD bit (Authenticated Data)

**Comunicação Avançada**
- UDP: Protocolo padrão (rapido)
- TCP: Fallback automático para respostas truncadas
- DoT: DNS over TLS (porta 853, criptografado)
  - Validação de certificado (CN/SAN matching)
  - Suporte SNI
  - Servidores públicos: Cloudflare, Google, Quad9

**Cache Distribuído**
- Daemon persistente: Cache em background gerenciável via CLI
- Cache positivo: Respostas válidas (A, NS, CNAME, AAAA, etc)
- Cache negativo: NXDOMAIN e NODATA (RFC 2308)
- Performance: 100-300x mais rapido para queries repetidas (300ms -> 1ms)
- LRU policy: Eviction automática quando cheio
- TTL management: Expiração automática de entradas
- Fallback elegante: Funciona normalmente se daemon offline

**Interface CLI**
- Help completo e estruturado
- Aliases intuitivos (-n, -t, -h, -v, -q)
- Quiet mode (output minimal)
- Validação robusta de argumentos
- Parâmetros avançados (--timeout, --max-iterations)
- Mensagens de erro uniformes

**Desempenho e Concorrência**
- ThreadPool production-grade: Pool de threads para processamento paralelo
- Batch processing: Processamento de múltiplos domínios simultaneamente
- Fan-out paralelo: Consultas simultâneas a múltiplos nameservers
- Performance: 2.5-7.8x speedup em batch processing
- Latência reduzida: 10x mais rápido para queries com servidores lentos
- Thread-safe: Implementação segura para concorrência
- RAII: Gerenciamento automático de recursos
- Zero overhead: Funcionalidades opcionais sem impacto quando desabilitadas

--

## Como usar

### Pré-requisitos

- Compilador C++17: g++ 7.0+ ou clang++ 5.0+
- Sistema Operacional: Linux ou macOS
- OpenSSL: 1.1.1+ ou 3.x
- Acesso à Internet: Para consultar root servers

### Instalação

```bash
# Clonar o repositório
cd Trabalho_redes

# Compilar o projeto
make

# Executar testes (266 testes)
make test-unit

# Limpar build
make clean
```

### Resolução DNS Básica

```bash
# Query básica (tipo A - IPv4)
./build/resolver --name google.com
./build/resolver -n google.com

# Tipos específicos
./build/resolver -n google.com -t NS      # Nameservers
./build/resolver -n google.com -t MX      # Mail exchange
./build/resolver -n google.com -t AAAA    # IPv6
./build/resolver -n example.com -t SOA    # Start of Authority

# Quiet mode (output minimal)
./build/resolver -n google.com -q
```

### DNS over TLS (Queries Criptografadas)

```bash
# Cloudflare DoT
./build/resolver --server 1.1.1.1 --name google.com --mode dot --sni one.one.one.one

# Google DoT
./build/resolver --server 8.8.8.8 -n example.com --mode dot --sni dns.google

# Quad9 DoT
./build/resolver --server 9.9.9.9 -n cloudflare.com --mode dot --sni dns.quad9.net
```

### DNSSEC Validation

```bash
# Validar autenticidade criptográfica
./build/resolver -n cloudflare.com --dnssec
./build/resolver -n example.com --dnssec --trace

# Usar trust anchor customizado
./build/resolver -n example.com --dnssec --trust-anchor root.trust-anchor
```

**Resultado com DNSSEC:**
```
DNSSEC:
  Status: Secure (AD=1)
  Data authenticated via DNSSEC
```

### Cache Daemon

```bash
# Iniciar daemon de cache em background
./build/cache_daemon --activate

# Verificar status
./build/cache_daemon --status
# Daemon: Running (PID: 12345)
# Cache Daemon Status
# Positive: 5/50
# Negative: 2/50

# Configurar tamanho do cache
./build/cache_daemon --set positive 100
./build/cache_daemon --set negative 50

# Listar entradas
./build/cache_daemon --list all

# Limpar cache
./build/cache_daemon --flush

# Parar daemon
./build/cache_daemon --deactivate
```

**Performance com Cache:**
```
Query 1 (MISS):  ~300-500ms  (resolução completa)
Query 2 (HIT):   ~1-5ms      (100-300x mais rapido!)
```

### Modo Trace (Debug Detalhado)

```bash
# Ver resolução iterativa completa
./build/resolver -n google.com --trace

# Trace com DNSSEC
./build/resolver -n example.com --dnssec --trace

# Trace com TCP fallback
./build/resolver -n large-response.com --tcp --trace

# Trace com DoT
./build/resolver --server 1.1.1.1 -n google.com --mode dot --sni one.one.one.one --trace
```

### Parâmetros Avançados

```bash
# Timeout customizado (1-60 segundos)
./build/resolver -n slow-server.com --timeout 10

# Limitar iterações (1-50)
./build/resolver -n example.com --max-iterations 20

# Combinar parâmetros
./build/resolver -n google.com --timeout 10 --max-iterations 20 -q
```

---

## Estrutura do Projeto

```
Trabalho_redes/
├── include/dns_resolver/
│   ├── types.h                 # Estruturas DNS (DNSMessage, DNSHeader, RRs)
│   ├── DNSParser.h             # Serialização/parsing de mensagens DNS
│   ├── NetworkModule.h         # Comunicação (UDP, TCP, DoT)
│   ├── ResolverEngine.h        # Motor de resolução iterativa
│   ├── TrustAnchorStore.h      # Gerenciamento de trust anchors
│   ├── DNSSECValidator.h       # Validação DNSSEC (chain + RRSIG)
│   └── CacheClient.h           # Cliente IPC para cache daemon
│
├── src/resolver/
│   ├── main.cpp                # CLI e entrada do programa
│   ├── DNSParser.cpp           # Implementação parsing/serialização
│   ├── NetworkModule.cpp       # Implementação UDP/TCP/DoT
│   ├── ResolverEngine.cpp      # Implementação resolução iterativa
│   ├── TrustAnchorStore.cpp    # Implementação trust anchors
│   ├── DNSSECValidator.cpp     # Implementação DNSSEC validation
│   └── CacheClient.cpp         # Implementação IPC client
│
├── src/daemon/
│   ├── main.cpp                # CLI do cache daemon
│   ├── CacheDaemon.h           # Interface do daemon
│   └── CacheDaemon.cpp         # Implementação daemon + IPC server
│
├── tests/
│   ├── test_dns_parser.cpp     # Testes serialização/parsing
│   ├── test_network_module.cpp # Testes UDP/TCP/DoT
│   ├── test_resolver_engine.cpp# Testes resolução iterativa
│   ├── test_trust_anchor_store.cpp # Testes trust anchors
│   ├── test_dnssec_validator.cpp   # Testes DNSSEC validation
│   ├── test_dnssec_records.cpp     # Testes parsing DNSSEC RRs
│   ├── test_tcp_framing.cpp    # Testes TCP framing
│   └── test_dot.cpp            # Testes DNS over TLS
│
├── build/
│   ├── resolver                # Executável principal
│   ├── cache_daemon            # Daemon de cache
│   └── tests/                  # Executáveis de teste
│
├── Makefile                    # Build system
├── README.md                   # Este arquivo
└── root.trust-anchor           # Root KSK 2024 (exemplo)
```

## Testes

### Testes End-to-End

```bash
# Resolução básica
./build/resolver -n google.com

# TCP fallback
./build/resolver -n large-dns-response.com

# DNS over TLS
./build/resolver --server 1.1.1.1 -n example.com --mode dot --sni one.one.one.one

# DNSSEC validation
./build/resolver -n cloudflare.com --dnssec --trace

# Cache (daemon rodando)
./build/cache_daemon --activate
./build/resolver -n google.com  # MISS
./build/resolver -n google.com  # HIT (rapido!)
./build/cache_daemon --deactivate
```

---

## Arquitetura

### Componentes Principais

#### DNSParser
- **Responsabilidade:** Serialização e parsing de mensagens DNS
- **Funcionalidades:** Wire format, compression, 11 tipos de RR
- **Arquivos:** `DNSParser.h/cpp`

#### NetworkModule
- **Responsabilidade:** Abstração de comunicação de rede
- **Funcionalidades:** UDP, TCP, DoT (OpenSSL)
- **Arquivos:** `NetworkModule.h/cpp`

#### ResolverEngine
- **Responsabilidade:** Lógica de resolução iterativa
- **Funcionalidades:** Root -> Auth, delegations, glue, CNAME
- **Arquivos:** `ResolverEngine.h/cpp`

#### TrustAnchorStore
- **Responsabilidade:** Gerenciamento de trust anchors
- **Funcionalidades:** Load from file, default root KSK
- **Arquivos:** `TrustAnchorStore.h/cpp`

#### DNSSECValidator
- **Responsabilidade:** Validação DNSSEC completa
- **Funcionalidades:** Chain validation, RRSIG verification, crypto
- **Arquivos:** `DNSSECValidator.h/cpp`

#### CacheDaemon
- **Responsabilidade:** Daemon de cache persistente
- **Funcionalidades:** IPC server, LRU, TTL management
- **Arquivos:** `daemon/CacheDaemon.h/cpp`, `daemon/main.cpp`

#### CacheClient
- **Responsabilidade:** Cliente IPC para cache daemon
- **Funcionalidades:** query(), store(), serialização
- **Arquivos:** `CacheClient.h/cpp`

### Princípios de Design

1. **Modularidade:** Separação clara de responsabilidades
2. **RAII:** Gerenciamento automático de recursos (sockets, OpenSSL)
3. **Tipos Seguros:** `uint8_t`, `uint16_t`, `uint32_t`
4. **Error Handling:** Exceções com mensagens claras
5. **RFC Compliance:** Conformidade estrita com RFCs
6. **Thread-Safety:** Mutex em operações críticas (cache)
7. **Testabilidade:** 266 testes automatizados

---

## Casos de Uso

### 1. Resolução Básica
```bash
$ ./build/resolver -n google.com
    google.com 300s A 172.217.29.238
```

### 2. DNSSEC Validation
```bash
$ ./build/resolver -n example.com --dnssec
DNSSEC:
  Status: Secure (AD=1)
  Data authenticated via DNSSEC

    example.com 300s A 93.184.215.14
```

### 3. DNS over TLS (Queries Criptografadas)
```bash
$ ./build/resolver --server 1.1.1.1 -n cloudflare.com --mode dot --sni one.one.one.one
    cloudflare.com 300s A 104.16.132.229
```

### 4. Cache (Performance Otimizada)
```bash
# Iniciar daemon
$ ./build/cache_daemon --activate

# Query 1 (MISS - ~300ms)
$ ./build/resolver -n google.com

# Query 2 (HIT - ~1ms, 300x mais rapido!)
$ ./build/resolver -n google.com

# Status
$ ./build/cache_daemon --status
Daemon: Running (PID: 12345)
Positive: 1/50
```

### 5. Debug com Trace
```bash
$ ./build/resolver -n example.com --dnssec --trace
;; Starting resolution for example.com (type 1)
;; DNSSEC enabled
;; Collecting DNSKEY and DS records...
;; Validating DNSSEC chain...
;; Chain validated: SECURE
;; Validating RRSIGs...
;;   ECDSA signature valid!
;; Setting AD=1

DNSSEC: Secure (AD=1)
```

---

## Segurança

### Proteção Contra Ataques

| Ataque | Proteção | Tecnologia |
|--------|----------|------------|
| **DNS Spoofing** | DNSSEC validation | RRSIG verification |
| **Man-in-the-Middle** | DNSSEC + DoT | TLS + crypto validation |
| **Cache Poisoning** | DNSSEC validation | Bogus detection |
| **Downgrade Attack** | DNSSEC validation | Chain of trust |
| **Eavesdropping** | DNS over TLS | TLS encryption |

### DNSSEC Chain of Trust

```
Root Zone (.)
    ↓ Trust Anchor (KSK 20326)
    ↓ DNSKEY validation
TLD (.com)
    ↓ DS -> DNSKEY matching
    ↓ RRSIG verification (crypto real)
Domain (example.com)
    ↓ DS -> DNSKEY matching
    ↓ RRSIG verification (crypto real)
    ↓
SECURE (AD=1)
```

---

## Performance

### Latência de Queries

| Cenário | Latência | Observações |
|---------|----------|-------------|
| Resolução iterativa (sem cache) | 300-500ms | Root -> TLD -> Auth |
| TCP fallback | 350-600ms | Overhead de handshake TCP |
| DNS over TLS | 400-700ms | Overhead de handshake TLS |
| **Cache HIT positivo** | **1-5ms** | **100x mais rapido!** |
| **Cache HIT negativo** | **1-5ms** | **300x mais rapido!** |

---

## Compilação e Build

### Makefile Targets

```bash
# Build completo
make                    # Compila resolver + cache_daemon

# Build individual
make resolver           # Apenas resolvedor
make daemon             # Apenas cache daemon
make tests              # Todos os testes

# Testes
make test-unit          # 266 testes automatizados
make test               # Testes manuais (legado)

# Limpeza
make clean              # Remove build/
make format             # (se clang-format disponível)

# Executar
make run                # Executa teste padrão
```

### Flags de Compilação

```makefile
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -O2 -I./include
LDFLAGS  = -lssl -lcrypto
```

### OpenSSL

**macOS (Homebrew):**
```bash
brew install openssl@3
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libssl-dev
```

---

## CLI Reference

### Opções Básicas

| Flag | Alias | Descrição | Default |
|------|-------|-----------|---------|
| `--name <domain>` | `-n` | Domínio a resolver (obrigatório) | - |
| `--type <type>` | `-t` | Tipo de registro | A |
| `--help` | `-h` | Mostrar ajuda | - |
| `--version` | `-v` | Mostrar versão | - |
| `--quiet` | `-q` | Output minimal | false |

### Opções de Conexão

| Flag | Descrição | Default |
|------|-----------|---------|
| `--server <ip>` | Servidor DNS específico (direct query) | - |
| `--mode <udp\|tcp\|dot>` | Modo de comunicação | udp |
| `--sni <hostname>` | SNI para DoT (obrigatório com DoT) | - |
| `--timeout <seconds>` | Timeout de query (1-60s) | 5 |
| `--tcp` | Shortcut para --mode tcp | - |

### Opções DNSSEC

| Flag | Alias | Descrição | Default |
|------|-------|-----------|---------|
| `--dnssec` | - | Ativar validação DNSSEC | false |
| `--trust-anchor <file>` | `-ta` | Arquivo de trust anchor | Root KSK 2024 |

### Opções de Debug

| Flag | Descrição |
|------|-----------|
| `--trace` | Trace detalhado (resolução iterativa) |

### Opções Avançadas

| Flag | Range | Default |
|------|-------|---------|
| `--max-iterations <n>` | 1-50 | 15 |

---

## Exemplos de Uso

### Básico
```bash
# IPv4
./build/resolver -n google.com

# IPv6
./build/resolver -n google.com -t AAAA

# Nameservers
./build/resolver -n google.com -t NS

# Mail servers
./build/resolver -n google.com -t MX
```

### DNSSEC
```bash
# Validar zona segura
./build/resolver -n cloudflare.com --dnssec
# DNSSEC: Secure (AD=1)

# Validar zona insegura
./build/resolver -n google.com --dnssec
# DNSSEC: Insecure/Unverified (AD=0)

# Com trace (debug)
./build/resolver -n example.com --dnssec --trace
```

### DNS over TLS
```bash
# Cloudflare
./build/resolver --server 1.1.1.1 -n google.com --mode dot --sni one.one.one.one

# Google
./build/resolver --server 8.8.8.8 -n example.com --mode dot --sni dns.google

# Quad9
./build/resolver --server 9.9.9.9 -n cloudflare.com --mode dot --sni dns.quad9.net
```

### Cache
```bash
# Gerenciamento daemon
./build/cache_daemon --activate     # Iniciar
./build/cache_daemon --status       # Status
./build/cache_daemon --flush        # Limpar
./build/cache_daemon --deactivate   # Parar

# Configuração
./build/cache_daemon --set positive 100
./build/cache_daemon --set negative 50

# Listagem
./build/cache_daemon --list all
./build/cache_daemon --list positive
./build/cache_daemon --list negative

# Purge seletivo
./build/cache_daemon --purge positive
./build/cache_daemon --purge negative
```

### Combinações Avançadas
```bash
# DNSSEC + Trace + Quiet
./build/resolver -n example.com --dnssec --trace -q

# DoT + DNSSEC + Trace
./build/resolver --server 1.1.1.1 -n cloudflare.com --mode dot --sni one.one.one.one --dnssec --trace

# Parâmetros customizados
./build/resolver -n slow-server.com --timeout 10 --max-iterations 20
```

---

## Disapro de erros

### Compilação

**Erro: OpenSSL not found**
```bash
# macOS
brew install openssl@3

# Linux
sudo apt-get install libssl-dev
```

**Erro: C++17 not supported**
```bash
# Atualizar compilador
g++ --version  # Requer 7.0+
clang++ --version  # Requer 5.0+
```

### Runtime

**Erro: "Failed to connect to server"**
- Verificar conectividade de rede
- Testar com ping: `ping 8.8.8.8`
- Firewall pode bloquear porta 53/853

**Erro: "Cache daemon unavailable"**
- Iniciar daemon: `./build/cache_daemon --activate`
- Verificar status: `./build/cache_daemon --status`
- Resolvedor funciona normalmente sem daemon (fallback)

**Erro: "DNSSEC validation failed! Possible attack!"**
- Zona realmente tem DNSSEC inválido (Bogus)
- Pode ser ataque real ou configuração errada
- Verificar com: `dig +dnssec example.com`

---


## Autores

João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues

Desenvolvido como parte do curso de Redes de Computadores.
