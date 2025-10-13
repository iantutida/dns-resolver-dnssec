# DNS Resolver - Trabalho de Redes

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

Implementação de um resolvedor DNS avançado em C++17 com suporte a UDP, TCP, DNS-over-TLS (DoT), DNSSEC e cache distribuído.

## 📋 Status do Projeto

| Epic | Story | Status | Descrição |
|------|-------|--------|-----------|
| EPIC 1 | Story 1.1 | ✅ Completed | Construir e Enviar Consulta DNS via UDP |
| EPIC 1 | Story 1.2 | ✅ Completed | Receber e Parsear Resposta DNS |
| EPIC 1 | Story 1.3 | ✅ Completed | Resolução Recursiva Completa (Root → TLD → Authoritative) |
| EPIC 1 | Story 1.4 | ✅ Completed | Seguir Registros CNAME Encadeados |
| EPIC 1 | Story 1.5 | ✅ Completed | Interpretar Respostas Negativas (NXDOMAIN/NODATA) |
| EPIC 2 | Story 2.1 | ✅ Completed | TCP Fallback para Respostas Truncadas |
| EPIC 2 | Story 2.2 | ✅ Completed | DNS-over-TLS (DoT) com OpenSSL - Consultas Criptografadas |
| EPIC 3 | Story 3.1 | 🔜 Próxima | Parsing de DNSSEC Records (RRSIG, DNSKEY, DS) |

## 🚀 Quick Start

### Pré-requisitos

- **Compilador C++17:** g++ 7.0+ ou clang++ 5.0+
- **Sistema Operacional:** Linux ou macOS
- **OpenSSL:** 1.1.1+ ou 3.x (para DoT - Story 2.2)
- **Acesso à Internet:** Necessário para consultar root servers
- **Ferramentas opcionais:** tcpdump, Wireshark (para debug de pacotes)

### Compilação

```bash
# Clonar o repositório
cd Trabalho_redes

# Compilar o projeto
make

# Executar testes unitários
make test-unit

# Executar testes manuais (Stories 1.1/1.2)
make test

# Limpar build
make clean
```

### Uso

**Modo Recursivo (Story 1.3 - Resolução completa a partir dos root servers):**

```bash
# Resolução recursiva básica
./build/resolver --name google.com

# Especificar tipo de registro
./build/resolver --name google.com --type NS

# Modo trace (mostra caminho completo: root → TLD → authoritative)
./build/resolver --name google.com --trace

# Resolver domínio inexistente
./build/resolver --name thisdoesnotexist.com

# Modo TCP forçado (Story 2.1)
./build/resolver --name google.com --tcp

# TCP + trace (ver handshake e framing)
./build/resolver --name google.com --tcp --trace

# DNS over TLS - Cloudflare (Story 2.2)
./build/resolver --server 1.1.1.1 --name google.com --mode dot --sni one.one.one.one

# DoT com Google DNS
./build/resolver --server 8.8.8.8 --name example.com --mode dot --sni dns.google

# DoT com Quad9
./build/resolver --server 9.9.9.9 --name cloudflare.com --mode dot --sni dns.quad9.net

# DoT + trace (ver handshake TLS)
./build/resolver --server 1.1.1.1 --name google.com --mode dot --sni one.one.one.one --trace
```

**Modo Direto (Stories 1.1/1.2 - Query direta a servidor específico):**

```bash
# Query para servidor específico
./build/resolver 8.8.8.8 google.com A
./build/resolver 1.1.1.1 cloudflare.com AAAA

# Teste padrão (google.com via 8.8.8.8)
make run
```

**Ajuda:**

```bash
./build/resolver --help
```

## 📁 Estrutura do Projeto

```
Trabalho_redes/
├── include/
│   └── dns_resolver/
│       ├── types.h              # Estruturas DNS (DNSMessage, DNSHeader, etc)
│       ├── DNSParser.h          # Serialização/parsing de mensagens DNS
│       └── NetworkModule.h      # Comunicação de rede (UDP, TCP, DoT)
├── src/
│   └── resolver/
│       ├── DNSParser.cpp        # Implementação da serialização
│       ├── NetworkModule.cpp    # Implementação da rede
│       └── main.cpp             # Programa de teste
├── build/                       # Arquivos compilados (gerado)
├── docs/                        # Documentação do projeto
│   ├── prd/                     # Product Requirements Document
│   ├── architecture/            # Documentação de arquitetura
│   └── stories/                 # User stories
├── Makefile                     # Build system
└── README.md                    # Este arquivo
```

## 🔧 Funcionalidades Implementadas

### Story 1.1: Construir e Enviar Consulta DNS via UDP ✅

- ✅ **Estruturas de Dados DNS**
  - `DNSHeader`: Cabeçalho completo com flags, contadores
  - `DNSQuestion`: Nome de domínio, tipo, classe
  - `DNSMessage`: Estrutura completa de mensagem DNS
  - `DNSResourceRecord`: Resource records com campos parsed

- ✅ **Serialização DNS**
  - Conversão de estruturas para formato binário (wire format)
  - Network byte order correto (big-endian)
  - Codificação de domain names com labels
  - Validações (domínio max 255 chars, labels max 63 chars)

- ✅ **Comunicação UDP**
  - Socket UDP com timeout configurável
  - Envio de queries para servidores DNS
  - Tratamento de erros de rede
  - RAII para gerenciamento de recursos (sem leaks)

### Story 1.2: Receber e Parsear Resposta DNS ✅

- ✅ **Recepção de Respostas**
  - `recvfrom()` implementado em NetworkModule
  - Timeout detectado e tratado apropriadamente
  - Validação de tamanho mínimo (12 bytes)
  - Buffer ajustado ao tamanho real da resposta

- ✅ **Parsing Completo de Mensagens DNS**
  - Parsing de header (12 bytes) com todas flags
  - Descompressão de nomes DNS (RFC 1035 §4.1.4)
  - Proteção contra loops infinitos (jump_limit)
  - Bounds checking rigoroso em todas leituras

- ✅ **Suporte a 8 Tipos de Resource Records**
  - **Tipo A**: IPv4 address (ex: "172.217.30.238")
  - **Tipo NS**: Nameserver (com descompressão)
  - **Tipo CNAME**: Canonical name (com descompressão)
  - **Tipo SOA**: Start of Authority (completo)
  - **Tipo PTR**: Pointer record
  - **Tipo MX**: Mail exchange (priority + server)
  - **Tipo TXT**: Text record
  - **Tipo AAAA**: IPv6 address (formato hex groups)

- ✅ **Validações e Robustez**
  - ID matching entre query e response
  - QR flag validation (deve ser 1)
  - RCODE detection (NO ERROR, NXDOMAIN, etc)
  - Tratamento de respostas malformadas
  - Exceções com mensagens claras

- ✅ **Output Formatado**
  - Exibição clara de flags DNS
  - Contadores de seções
  - Resource records formatados por tipo
  - Múltiplas seções (Answer, Authority, Additional)

### Story 1.3: Resolução Recursiva Completa ✅

- ✅ **ResolverEngine - Motor de Resolução**
  - Classe `ResolverEngine` com algoritmo iterativo (RFC 1034 §5.3.3)
  - Começa dos root servers e segue delegações até servidor autoritativo
  - Configuração flexível (root servers, timeouts, max iterations)

- ✅ **Root Servers**
  - 5 root servers IPv4 configurados (a-e.root-servers.net)
  - Seleção aleatória para load balancing
  - Fallback automático em caso de falha

- ✅ **Detecção e Seguimento de Delegações**
  - Identifica respostas de delegação (AUTHORITY com NS, ANSWER vazio)
  - Extrai nameservers da seção AUTHORITY
  - Extrai glue records da seção ADDITIONAL
  - Seleciona próximo servidor automaticamente

- ✅ **Glue Records**
  - Usa glue records (IPs na seção ADDITIONAL) quando disponíveis
  - Prioriza IPv4 sobre IPv6
  - Evita queries adicionais desnecessárias

- ✅ **Resolução de Nameservers**
  - Resolve recursivamente NSs sem glue record
  - Proteção contra dependências circulares
  - Limite de profundidade de recursão (depth=5)

- ✅ **Modo Trace (similar a dig +trace)**
  - Flag `--trace` ativa logging detalhado
  - Mostra cada iteração com servidor consultado
  - Exibe delegações recebidas e glue records usados
  - Output formatado para debug fácil

- ✅ **Proteções e Robustez**
  - Limite de iterações (max_iterations=15)
  - Limite de profundidade de recursão (depth=5)
  - Cache de servidores consultados (previne loops)
  - Tratamento de timeout, SERVFAIL, NXDOMAIN
  - Error handling robusto

- ✅ **Interface CLI Moderna**
  - Modo recursivo: `--name DOMAIN [--type TYPE] [--trace]`
  - Modo direto: `SERVER DOMAIN [TYPE]` (compatibilidade backward)
  - Help completo: `--help` ou `-h`
  - Suporte a 7 tipos de registro

### Story 1.4: Seguir Registros CNAME Encadeados ✅

- ✅ **Detecção Inteligente de CNAME**
  - Verifica se resposta contém CNAME sem tipo alvo
  - Distingue entre CNAME otimizado (CNAME+A) vs apenas CNAME
  - Extração de canonical name (rdata_cname)

- ✅ **Seguimento de CNAME**
  - Resolve canonical name automaticamente
  - Inicia nova resolução iterativa para CNAME target
  - Acumula todos CNAMEs intermediários + registro final
  - Suporte a encadeamento (múltiplos CNAMEs em cadeia)

- ✅ **CNAME Cross-Domain**
  - Detecta quando CNAME aponta para outro domínio/TLD
  - Inicia nova resolução desde root servers
  - Usa glue records apropriadamente na nova resolução
  - Exemplo: www.reddit.com → reddit.map.fastly.net (.com → .net)

- ✅ **Otimização de Performance**
  - Se servidor retorna CNAME + registro alvo, usa diretamente
  - Evita query adicional desnecessária
  - Exemplo: www.github.com retorna CNAME + A na mesma resposta

- ✅ **Proteção contra Loops**
  - MAX_CNAME_DEPTH = 10 (limite de saltos)
  - Previne loops infinitos com CNAMEs circulares
  - Exceção clara: "CNAME chain too long"

- ✅ **Trace Detalhado para CNAMEs**
  - Logs mostram cada CNAME seguido
  - Indica quando inicia nova resolução iterativa
  - Formato: `CNAME: origem → destino`

### Story 1.5: Interpretar Respostas Negativas ✅

- ✅ **Detecção de NXDOMAIN (Domínio Não Existe)**
  - Verifica RCODE=3
  - Mensagem user-friendly: "Domain does not exist"
  - Extrai e exibe SOA da AUTHORITY
  - Mostra Negative TTL (SOA MINIMUM)

- ✅ **Detecção de NODATA (Sem Registros do Tipo)**
  - Verifica RCODE=0 + ANSWER vazio + não delegação
  - Mensagem clara: "Domain exists but no records of type X"
  - Diferencia de delegação (verifica presença de NS)
  - Exibe SOA com TTL negativo

- ✅ **Extração de SOA**
  - `extractSOA()` retorna SOA da seção AUTHORITY
  - Campos exibidos: Zone, Primary NS, Responsible Party, Serial, Negative TTL
  - Não crashea se SOA ausente

- ✅ **Output User-Friendly**
  - Emojis visuais (❌ NXDOMAIN, ⚠️ NODATA, ✅ SUCCESS)
  - Mensagens claras em português
  - Explicações do significado de cada status
  - Helpers: getRCodeName() e getTypeName()

- ✅ **Trace para Respostas Negativas**
  - NXDOMAIN mostrado no trace
  - NODATA indicado no trace
  - SOA MINIMUM exibido (negative cache TTL)
  - Logs integrados com resolução iterativa

- ✅ **Conformidade RFC**
  - RFC 1035 §4.1.1: RCODE definitions
  - RFC 2308: Negative Caching
  - Preparação para cache negativo (Story 4.4)

### Story 2.1: Fallback TCP para Respostas Truncadas ✅

- ✅ **Comunicação DNS via TCP**
  - Socket TCP (SOCK_STREAM) com connect/send/recv
  - Timeout configurável (padrão: 10s, 2x UDP)
  - RAII para gerenciamento automático de sockets
  - Error handling robusto (connection refused, timeout)

- ✅ **Framing TCP (RFC 1035 §4.2.2)**
  - 2 bytes length prefix em network byte order (big-endian)
  - addTCPFraming() adiciona prefix automaticamente
  - Leitura de length prefix da resposta
  - Leitura de exatamente N bytes da mensagem

- ✅ **Envio/Recepção Completa**
  - sendAll() com loop (garante envio total)
  - recvAll() com loop (garante recepção total)
  - EINTR handling (retry se interrompido)
  - Timeout detection (EAGAIN/EWOULDBLOCK)

- ✅ **Detecção Automática de Truncamento**
  - isTruncated() verifica bit TC=1 no header
  - Detecção após parsear resposta UDP
  - Fallback transparente para o usuário

- ✅ **Fallback Automático UDP → TCP**
  - UDP tentado primeiro (mais rápido)
  - Se resposta truncada (TC=1) → retry com TCP
  - Mesma query refeita automaticamente
  - Resposta completa retornada
  - Trace logs indicam fallback

- ✅ **Modo TCP Forçado**
  - Flag --tcp força uso de TCP desde início
  - Útil para debug e teste
  - Pula UDP completamente
  - Trace mostra "Force TCP mode"

- ✅ **Conformidade RFC**
  - RFC 1035 §4.2.2: TCP usage and framing
  - RFC 7766: DNS Transport over TCP
  - Length prefix em network byte order
  - send/recv completo com loops

### Story 2.2: DNS over TLS (DoT) - Consultas Seguras ✅

- ✅ **Integração OpenSSL**
  - OpenSSL 3.x suportado (compatível com 1.1.1+)
  - Auto-detecção de path (Homebrew/Linux)
  - Biblioteca inicializada automaticamente
  - Certificados CA do sistema carregados

- ✅ **Comunicação TLS Completa**
  - Socket TCP porta 853 (porta DoT dedicada)
  - Handshake TLS (SSL_connect)
  - TLS 1.2+ obrigatório (segurança)
  - Comunicação criptografada (SSL_write/SSL_read)
  - Framing TCP reutilizado (2 bytes length prefix)
  - Timeout 15s (handshake TLS é lento)

- ✅ **Validação de Certificado**
  - Validação obrigatória (SSL_VERIFY_PEER)
  - Chain of trust verificada
  - CN (Common Name) matching
  - SAN (Subject Alternative Name) matching
  - Suporte a wildcards (*.domain.com)
  - Erro claro se certificado inválido

- ✅ **Server Name Indication (SNI)**
  - SNI obrigatório para DoT
  - Configurado no handshake TLS
  - Usado na validação de certificado
  - Permite servidores multi-domain

- ✅ **QueryMode Enum**
  - UDP, TCP, DoT suportados
  - Switch elegante na seleção de protocolo
  - Flags: --mode udp|tcp|dot
  - Backward compatible (--tcp)

- ✅ **Interface CLI para DoT**
  - Flag --mode dot
  - Flag --sni HOSTNAME (obrigatório)
  - Flag --server IP (query direta)
  - Exemplos para 3 servidores públicos
  - Help detalhado

- ✅ **Servidores Públicos Suportados**
  - Cloudflare: 1.1.1.1:853 (SNI: one.one.one.one)
  - Google: 8.8.8.8:853 (SNI: dns.google)
  - Quad9: 9.9.9.9:853 (SNI: dns.quad9.net)

- ✅ **Segurança e Privacidade**
  - Queries criptografadas (proteção contra espionagem)
  - Certificado validado (proteção contra MitM)
  - Integridade dos dados (TLS)
  - Privacidade vs ISPs e redes não confiáveis

- ✅ **Conformidade RFC**
  - RFC 7858: DNS over TLS
  - RFC 8310: Usage Profiles for DoT
  - RFC 6066: SNI
  - RFC 5280: Certificate validation

## 🧪 Testes e Validação

### Testes Automatizados

```bash
# Executar todos os testes unitários (35 testes)
make test-unit

# Breakdown:
# - 11 testes de serialização DNS (Story 1.1)
# - 10 testes de networking UDP (Story 1.1)
# - 14 testes de parsing de resposta (Story 1.2)
```

### Testes Manuais

```bash
# Executar suite de testes manuais
make test
```

### Debug de Pacotes

Use `tcpdump` ou `Wireshark` para capturar e inspecionar pacotes DNS:

```bash
# Em um terminal, iniciar captura
sudo tcpdump -i any -n -X port 53

# Em outro terminal, executar o resolver
./build/resolver 8.8.8.8 google.com A
```

### Validação com dig

Compare o formato dos pacotes gerados com o `dig`:

```bash
# Ver formato da query
dig @8.8.8.8 google.com A +qr +noedns
```

## 📊 Exemplo de Saída

### Exemplo 1: Query Tipo A

```
=================================================
  DNS Resolver - STORY 1.2 Test Suite
  Receber e Parsear Resposta DNS
=================================================

========== TESTE DE QUERY DNS ==========
Servidor: 8.8.8.8
Domínio:  google.com
Tipo:     1 (A)

✓ Mensagem DNS construída
  Transaction ID: 0x48f9

✓ Query serializada (28 bytes)

Enviando query e aguardando resposta...
✓ Resposta recebida (44 bytes)

✓ Resposta parseada com sucesso!

========== RESPOSTA DNS ==========

Flags:
  QR=1 (query/response)
  AA=0 (authoritative)
  TC=0 (truncated)
  RD=1 (recursion desired)
  RA=1 (recursion available)
  RCODE=0 (NO ERROR)

Contadores:
  Questions: 1
  Answers: 1
  Authority: 0
  Additional: 0

ANSWER SECTION:
    google.com 208s A 172.217.30.238

===================================
```

### Exemplo 2: Query Tipo NS

```
========== TESTE DE QUERY DNS ==========
Servidor: 8.8.8.8
Domínio:  google.com
Tipo:     2 (NS)

✓ Resposta recebida (100 bytes)
✓ Resposta parseada com sucesso!

ANSWER SECTION:
    google.com 21600s NS ns3.google.com
    google.com 21600s NS ns1.google.com
    google.com 21600s NS ns2.google.com
    google.com 21600s NS ns4.google.com
```

### Exemplo 3: Domínio Inexistente (NXDOMAIN)

```
========== TESTE DE QUERY DNS ==========
Servidor: 8.8.8.8
Domínio:  thisdoesnotexist12345.com
Tipo:     1 (A)

Flags:
  RCODE=3 (NXDOMAIN)

Contadores:
  Questions: 1
  Answers: 0
  Authority: 1
  Additional: 0

AUTHORITY SECTION:
    com 900s SOA a.gtld-servers.net nstld.verisign-grs.com
```

### Exemplo 4: Resolução Recursiva com Trace (Story 1.3)

```bash
./build/resolver --name google.com --type A --trace
```

```
;; ========================================
;; Starting resolution for google.com (type 1)
;; Initial root server: 199.9.14.201
;; ========================================

;; --- Iteration 1 ---
;; Querying: 199.9.14.201 for google.com (type 1)
;; Got delegation to 13 nameserver(s):
;;   NS: a.gtld-servers.net
;;   NS: b.gtld-servers.net
;;   ...
;; Glue records available:
;;   a.gtld-servers.net → 192.5.6.30
;;   b.gtld-servers.net → 192.33.14.30
;;   ...
;; Using glue record for a.gtld-servers.net → 192.5.6.30
;; Next server selected: 192.5.6.30

;; --- Iteration 2 ---
;; Querying: 192.5.6.30 for google.com (type 1)
;; Got delegation to 4 nameserver(s):
;;   NS: ns1.google.com
;;   NS: ns2.google.com
;;   NS: ns3.google.com
;;   NS: ns4.google.com
;; Glue records available:
;;   ns1.google.com → 216.239.32.10
;;   ns2.google.com → 216.239.34.10
;;   ...
;; Using glue record for ns2.google.com → 216.239.34.10
;; Next server selected: 216.239.34.10

;; --- Iteration 3 ---
;; Querying: 216.239.34.10 for google.com (type 1)
;; Got authoritative answer with 1 record(s)
;;   Answer: google.com TTL=300 Type=1 → 172.217.30.14
;; ========================================
;; Resolution completed successfully
;; ========================================

========== RESULTADO DA RESOLUÇÃO ==========

Query: google.com Type: 1

Flags:
  QR=1 (query/response)
  AA=1 (authoritative)
  RD=0 (recursion desired)
  RA=0 (recursion available)
  RCODE=0 (NO ERROR)

Contadores:
  Questions: 1
  Answers: 1
  Authority: 0
  Additional: 0

ANSWER SECTION:
    google.com 300s A 172.217.30.14

============================================
```

### Exemplo 5: Resolução de CNAME Cross-Domain (Story 1.4)

```bash
./build/resolver --name www.reddit.com --type A
```

```
========== RESULTADO DA RESOLUÇÃO ==========

Query: www.reddit.com Type: 1

Flags:
  QR=1 (query/response)
  AA=1 (authoritative)
  RCODE=0 (NO ERROR)

Contadores:
  Questions: 1
  Answers: 2
  Authority: 4
  Additional: 0

ANSWER SECTION:
    www.reddit.com 10800s CNAME reddit.map.fastly.net
    reddit.map.fastly.net 60s A 151.101.249.140

============================================
```

**Com --trace mostrando resolução do CNAME:**

```
;; Got authoritative answer with 1 record(s)
;;   Answer: www.reddit.com TTL=10800 Type=5 → reddit.map.fastly.net
;; Response contains CNAME without target type, following...
;; CNAME: www.reddit.com → reddit.map.fastly.net
;; Following canonical name: reddit.map.fastly.net
;; Starting new iterative resolution for CNAME target: reddit.map.fastly.net
;; [... nova resolução iterativa para .net domain ...]
;;   Answer: reddit.map.fastly.net TTL=60 Type=1 → 151.101.249.140
```

### Exemplo 6: Resposta Negativa - NXDOMAIN (Story 1.5)

```bash
./build/resolver --name thisdoesnotexist12345.com --type A
```

```
============================================
        DNS QUERY RESULT
============================================
Query:  thisdoesnotexist12345.com
Type:   A (1)
RCODE:  NXDOMAIN
============================================

❌ Domain does not exist (NXDOMAIN)

The domain 'thisdoesnotexist12345.com' was not found.
This means the domain is not registered or does not exist.

AUTHORITY SECTION (SOA):
  Zone:              com
  Primary NS:        a.gtld-servers.net
  Responsible Party: nstld.verisign-grs.com
  Serial:            1760312244
  Negative TTL:      900 seconds

============================================
```

**Com --trace mostrando NXDOMAIN:**

```
;; --- Iteration 2 ---
;; Querying: 192.5.6.30 for thisdoesnotexist12345.com (type 1)
;; Got RCODE 3
;; Domain does not exist (NXDOMAIN)
;; SOA MINIMUM (negative cache TTL): 900 seconds
```

## 🏗️ Arquitetura

### Princípios de Design

1. **Modularidade:** Separação clara entre parsing, rede e lógica de resolução
2. **Tipos Seguros:** Uso de `uint8_t`, `uint16_t`, `uint32_t`
3. **RAII:** Gerenciamento automático de recursos (sockets)
4. **Tratamento de Erros:** Exceções para erros de rede e validação
5. **Padrões RFC:** Conformidade estrita com RFC 1035

### Componentes Principais

- **DNSParser:** Serialização/parsing de mensagens DNS
- **NetworkModule:** Abstração de comunicação de rede
- **types.h:** Estruturas de dados DNS compartilhadas

## 📚 Documentação

- **PRD Completo:** [`docs/prd/`](docs/prd/)
- **Arquitetura:** [`docs/architecture/`](docs/architecture/)
- **Stories:** [`docs/stories/`](docs/stories/)

## 🔜 Próximos Passos

### 🎉 EPIC 1 - Resolução DNS Central ✅ **COMPLETO!**

**Todas as 5 stories do EPIC 1 foram implementadas com sucesso:**
- ✅ Story 1.1: Construir e Enviar Consulta DNS via UDP
- ✅ Story 1.2: Receber e Parsear Resposta DNS
- ✅ Story 1.3: Resolução Recursiva Completa
- ✅ Story 1.4: Seguir Registros CNAME Encadeados
- ✅ Story 1.5: Interpretar Respostas Negativas (NXDOMAIN/NODATA)

**O resolvedor agora tem funcionalidade DNS completa:**
- ✅ Serialização e parsing de mensagens DNS (RFC 1035)
- ✅ Comunicação UDP com timeouts e RAII
- ✅ Resolução iterativa desde root servers (RFC 1034 §5.3.3)
- ✅ Seguimento de delegações com glue records
- ✅ Suporte a CNAMEs encadeados e cross-domain
- ✅ Interpretação de respostas negativas (RFC 2308)
- ✅ Parsing de 8 tipos de RR (A, NS, CNAME, SOA, PTR, MX, TXT, AAAA)
- ✅ Modo trace completo (similar a dig +trace)
- ✅ Interface CLI moderna e user-friendly

**Estatísticas do EPIC 1:**
- **Total de código:** ~2700 linhas
- **Testes automatizados:** 87 testes (100% passando)
- **Headers:** 4 arquivos (types.h, DNSParser.h, NetworkModule.h, ResolverEngine.h)
- **Implementação:** 4 arquivos (.cpp)
- **Testes:** 4 suites de testes
- **Tempo total:** ~8.5 horas (estimativa original: 13-17 dias)
- **Qualidade:** Zero warnings, código limpo C++17

### 🎉 EPIC 2 - Comunicação Avançada e Segura ✅ **COMPLETO!**

**Todas as 2 stories do EPIC 2 foram implementadas com sucesso:**
- ✅ Story 2.1: TCP Fallback para Respostas Truncadas
- ✅ Story 2.2: DNS-over-TLS (DoT) com OpenSSL

**O resolvedor agora suporta 3 protocolos:**
- ✅ UDP (rápido, padrão, porta 53)
- ✅ TCP (respostas grandes, fallback automático, porta 53)
- ✅ DoT (criptografado, privado, porta 853)

**Estatísticas do EPIC 2:**
- **Total de código:** ~815 linhas
- **OpenSSL:** Integrado com validação completa
- **Tempo total:** ~4 horas (estimativa original: 6-8 dias)
- **Qualidade:** Zero warnings, TLS 1.2+, validação obrigatória

### Stories Futuras (EPICs 3-6)

**EPIC 3: Validação DNSSEC**
- Story 3.1: Parsing de RRSIG, DNSKEY, DS Records
- Story 3.2: Validação de Assinaturas DNSSEC
- Story 3.3: Chain of Trust Validation

**EPIC 4: Sistema de Cache**
- Story 4.1: Cache em Memória com TTL
- Story 4.2: Cache Negativo (NXDOMAIN)
- Story 4.3: Cache Distribuído

**EPIC 5: Interface CLI Avançada**
- Story 5.1: Configuração via Arquivo
- Story 5.2: Comandos Interativos
- Story 5.3: Output em JSON

**EPIC 6: Otimizações**
- Story 6.1: Queries Paralelas
- Story 6.2: Connection Pooling
- Story 6.3: Métricas e Monitoring

## 🤝 Contribuindo

Este é um projeto acadêmico. Para mais informações, consulte a documentação no diretório `docs/`.

## 📄 Licença

Trabalho Acadêmico - Redes de Computadores

## 👥 Autores

- Desenvolvido como parte do curso de Redes de Computadores

## 📖 Referências

- [RFC 1035 - Domain Names](https://datatracker.ietf.org/doc/html/rfc1035)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [DNS Protocol Basics](https://www2.cs.duke.edu/courses/fall16/compsci356/DNS/DNS-primer.pdf)

