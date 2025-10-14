# Examples - DNS Resolver

Este diretório contém exemplos e scripts para facilitar o uso e teste do DNS Resolver.

---

## 📁 Arquivos

### Listas de Domínios

- **`example_domains.txt`** - Lista de domínios populares para batch processing
- **`test_dnssec_domains.txt`** - Domínios com DNSSEC para testes de validação

### Scripts

- **`demo.sh`** - Script de demonstração das principais funcionalidades

---

## 🚀 Como Usar

### 1. Demo Script

Execute o script de demonstração para ver todas as funcionalidades:

```bash
cd examples
./demo.sh
```

O script demonstra:
1. Resolução DNS básica
2. Tipos diferentes (MX, NS, etc)
3. DNSSEC validation
4. DNS over TLS (DoT)
5. Batch processing com ThreadPool
6. Fan-out paralelo

### 2. Batch Processing

```bash
# Processar domínios de exemplo com 4 workers
../build/resolver --batch example_domains.txt --workers 4

# Processar com 8 workers (mais rápido)
../build/resolver --batch example_domains.txt --workers 8

# Modo quiet (apenas resultados)
../build/resolver --batch example_domains.txt --quiet
```

### 3. DNSSEC Testing

```bash
# Testar domínios com DNSSEC
../build/resolver --batch test_dnssec_domains.txt --dnssec

# Teste individual com trace
../build/resolver -n cloudflare.com --dnssec --trace
```

### 4. Criar Suas Próprias Listas

Crie um arquivo com um domínio por linha:

```bash
cat > my_domains.txt << EOF
example.com
test.com
mysite.org
EOF

# Processar
../build/resolver --batch my_domains.txt
```

---

## 📊 Performance Comparison

### Serial vs Parallel (Batch)

```bash
# Serial (1 worker) - baseline
time ../build/resolver --batch example_domains.txt --workers 1 -q

# Parallel (4 workers)
time ../build/resolver --batch example_domains.txt --workers 4 -q

# Parallel (8 workers)
time ../build/resolver --batch example_domains.txt --workers 8 -q
```

**Resultado Esperado:** 2.5-7.8x speedup com workers paralelos

### Com vs Sem Fan-out

```bash
# Sem fan-out (seleção sequencial de NS)
time ../build/resolver -n example.com -q

# Com fan-out (consulta múltiplos NS em paralelo)
time ../build/resolver -n example.com --fanout -q
```

**Resultado Esperado:** 10x redução de latência em casos de servidores lentos

---

## 🧪 Testes Específicos

### Teste 1: Comparar UDP vs DoT

```bash
# UDP (rápido, não criptografado)
../build/resolver -n example.com -q

# DoT (seguro, criptografado)
../build/resolver --mode dot --server 1.1.1.1 --sni one.one.one.one -n example.com -q
```

### Teste 2: Cache Performance

```bash
# Ativar cache
../build/cache_daemon --activate

# Primeira query (sem cache)
time ../build/resolver -n google.com -q

# Segunda query (com cache - deve ser muito mais rápida)
time ../build/resolver -n google.com -q

# Verificar cache
../build/cache_daemon --list all
```

### Teste 3: DNSSEC Chain Validation

```bash
# Ver cadeia de validação completa
../build/resolver -n cloudflare.com --dnssec --trace | grep -E "DNSKEY|DS|RRSIG|Trust"
```

---

## 💡 Dicas

1. **Use --quiet** para output mínimo em scripts
2. **Use --trace** para debugging detalhado
3. **Use --workers 8** para máxima performance em batch
4. **Use --fanout** quando há muitos nameservers disponíveis
5. **Ative o cache daemon** para queries repetidas

---

## 📚 Mais Informações

- **README principal:** `../README.md`
- **Quick Start:** `../QUICKSTART.md`
- **Help do resolver:** `../build/resolver --help`
- **Help do cache:** `../build/cache_daemon --help`

---

**Happy Resolving!** 🚀

