# 🚀 Quick Start Guide - DNS Resolver

Este guia fornece instruções passo a passo para compilar, instalar e testar o DNS Resolver.

---

## 📋 Pré-requisitos

### Sistema Operacional
- **Linux** (Ubuntu 20.04+, Debian 11+, Fedora 35+, etc)
- **macOS** (10.15 Catalina ou superior)

### Compilador C++17
- **g++** 7.0 ou superior
- **clang++** 5.0 ou superior

### Bibliotecas Necessárias

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential g++ make libssl-dev
```

#### Fedora/RHEL/CentOS
```bash
sudo dnf install -y gcc-c++ make openssl-devel
```

#### macOS (via Homebrew)
```bash
brew install openssl@3
```

---

## 📦 Instalação

### 1. Clonar o Repositório

```bash
# Clone o repositório
git clone https://github.com/iantutida/dns-resolver-dnssec.git
cd dns-resolver-dnssec
```

### 2. Compilar o Projeto

```bash
# Compilar tudo (resolvedor + cache daemon)
make

# OU compilar componentes individualmente
make resolver    # Apenas o resolvedor
make daemon      # Apenas o cache daemon
```

**Output esperado:**
```
Compilando src/resolver/DNSParser.cpp...
Compilando src/resolver/NetworkModule.cpp...
...
✓ Resolvedor compilado: ./build/resolver
✓ Cache daemon compilado: ./build/cache_daemon
```

### 3. Verificar Compilação

```bash
# Verificar versão
./build/resolver --version

# Verificar help
./build/resolver --help
```

---

## 🧪 Executar Testes

### Testes Automatizados (272 testes)

```bash
# Executar todos os testes unitários
make test-unit
```

**Output esperado:**
```
==========================================
  EXECUTANDO TESTES UNITÁRIOS
==========================================

========================================
  Testes: DNSParser
========================================

TEST: buildDNSQuery - Header correto... ✓
TEST: buildDNSQuery - Question correto... ✓
...

==========================================
  ✅ TODOS OS TESTES UNITÁRIOS PASSARAM
==========================================
```

### Testes Manuais

```bash
# Teste 1: Resolução básica
./build/resolver -n google.com -q

# Teste 2: DNSSEC
./build/resolver -n cloudflare.com --dnssec

# Teste 3: DNS over TLS
./build/resolver --mode dot --server 1.1.1.1 --sni one.one.one.one -n example.com

# Teste 4: Batch processing
echo -e "google.com\ngithub.com\nstackoverflow.com" > test_domains.txt
./build/resolver --batch test_domains.txt --workers 4
```

---

## 🎯 Uso Básico

### Resolução DNS Simples

```bash
# Query tipo A (IPv4)
./build/resolver -n google.com

# Query tipo AAAA (IPv6)
./build/resolver -n google.com -t AAAA

# Query tipo MX (Mail Exchange)
./build/resolver -n google.com -t MX

# Quiet mode (output mínimo)
./build/resolver -n google.com -q
```

### Cache Daemon

```bash
# 1. Ativar daemon
./build/cache_daemon --activate

# 2. Verificar status
./build/cache_daemon --status

# 3. Usar resolvedor (cache automático)
./build/resolver -n google.com

# 4. Verificar cache
./build/cache_daemon --list all

# 5. Limpar cache
./build/cache_daemon --flush

# 6. Desativar daemon
./build/cache_daemon --deactivate
```

### DNSSEC Validation

```bash
# Validar domínio com DNSSEC
./build/resolver -n cloudflare.com --dnssec

# Usar trust anchor customizado
./build/resolver -n example.com --dnssec --trust-anchor root.trust-anchor
```

### DNS over TLS (DoT)

```bash
# Cloudflare DoT
./build/resolver --mode dot --server 1.1.1.1 --sni one.one.one.one -n example.com

# Google DoT
./build/resolver --mode dot --server 8.8.8.8 --sni dns.google -n example.com

# Quad9 DoT
./build/resolver --mode dot --server 9.9.9.9 --sni dns.quad9.net -n example.com
```

### Batch Processing (EPIC 6 - ThreadPool)

```bash
# Criar arquivo com domínios
cat > domains.txt << EOF
google.com
github.com
stackoverflow.com
reddit.com
wikipedia.org
EOF

# Processar com 4 workers (default)
./build/resolver --batch domains.txt

# Processar com 8 workers (mais rápido)
./build/resolver --batch domains.txt --workers 8

# Batch com tipo específico
./build/resolver --batch domains.txt --type MX
```

### Fan-out Paralelo (EPIC 6 - Nameservers)

```bash
# Ativar fan-out para consultar múltiplos NS em paralelo
./build/resolver -n example.com --fanout

# Fan-out com trace para ver processo
./build/resolver -n example.com --fanout --trace
```

---

## 🐛 Troubleshooting

### Erro: "Command not found: make"
```bash
# Ubuntu/Debian
sudo apt-get install make

# macOS
xcode-select --install
```

### Erro: "OpenSSL headers not found"
```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev

# macOS (Homebrew)
brew install openssl@3
```

### Erro: "undefined reference to SSL_*"
```bash
# Verificar se OpenSSL está instalado
openssl version

# Se macOS, adicionar ao PATH
export CPATH=/opt/homebrew/opt/openssl@3/include
export LIBRARY_PATH=/opt/homebrew/opt/openssl@3/lib

# Recompilar
make clean
make
```

### Cache Daemon não inicia
```bash
# Verificar se já está rodando
./build/cache_daemon --status

# Matar processo antigo se necessário
pkill -f cache_daemon

# Remover PID file antigo
rm -f /tmp/dns_cache.pid

# Tentar novamente
./build/cache_daemon --activate
```

### Query timeout
```bash
# Aumentar timeout (default: 5s)
./build/resolver -n example.com --timeout 10

# Verificar conectividade
ping -c 3 8.8.8.8

# Testar com servidor alternativo
./build/resolver --server 1.1.1.1 -n example.com
```

---

## 📚 Próximos Passos

1. **Ler o README completo:** `cat README.md`
2. **Explorar a CLI:** `./build/resolver --help`
3. **Testar DNSSEC:** `./build/resolver -n cloudflare.com --dnssec`
4. **Ativar cache:** `./build/cache_daemon --activate`
5. **Testar batch processing:** `./build/resolver --batch domains.txt --workers 8`
6. **Explorar documentação:** `ls docs/`

---

## 🆘 Suporte

Se encontrar problemas:
1. Verifique os pré-requisitos
2. Leia a seção de troubleshooting
3. Consulte o README.md completo
4. Verifique os logs de compilação
5. Execute `make clean && make` para recompilar do zero

---

## 📝 Notas

- **Root Access:** Não é necessário (roda como usuário normal)
- **Portas:** UDP/TCP 53 (DNS), TCP 853 (DoT)
- **Firewall:** Certifique-se de que as portas DNS estão abertas
- **Internet:** Necessário para consultar root servers

---

**Happy Resolving!** 🚀

