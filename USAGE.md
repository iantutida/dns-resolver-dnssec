# Usage Guide - DNS Resolver

Guia detalhado de uso do DNS Resolver com todos os comandos e opções disponíveis.

---

## 📋 Índice

1. [Resolução Básica](#resolução-básica)
2. [Tipos de Registro DNS](#tipos-de-registro-dns)
3. [Modos de Operação](#modos-de-operação)
4. [DNSSEC](#dnssec)
5. [Cache](#cache)
6. [Batch Processing](#batch-processing)
7. [Performance](#performance)
8. [Parâmetros Avançados](#parâmetros-avançados)

---

## 1. Resolução Básica

### Query Simples

```bash
# Sintaxe básica
./build/resolver --name <domain>
./build/resolver -n <domain>

# Exemplos
./build/resolver -n google.com
./build/resolver -n github.com
./build/resolver -n stackoverflow.com
```

### Tipos de Output

```bash
# Normal (verbose com trace)
./build/resolver -n google.com

# Quiet (apenas resultado)
./build/resolver -n google.com --quiet
./build/resolver -n google.com -q

# Com trace detalhado
./build/resolver -n google.com --trace
```

---

## 2. Tipos de Registro DNS

### Tipos Suportados

| Tipo | Número | Descrição | Exemplo |
|------|--------|-----------|---------|
| A | 1 | IPv4 address | `./build/resolver -n google.com -t A` |
| NS | 2 | Nameserver | `./build/resolver -n google.com -t NS` |
| CNAME | 5 | Canonical name | `./build/resolver -n www.github.com -t CNAME` |
| SOA | 6 | Start of Authority | `./build/resolver -n example.com -t SOA` |
| MX | 15 | Mail exchange | `./build/resolver -n google.com -t MX` |
| TXT | 16 | Text record | `./build/resolver -n google.com -t TXT` |
| AAAA | 28 | IPv6 address | `./build/resolver -n google.com -t AAAA` |

---

## 3. Modos de Operação

### Modo Recursive (Resolução Iterativa)

```bash
# Resolução iterativa desde root servers (default)
./build/resolver -n example.com

# Com trace para ver processo
./build/resolver -n example.com --trace
```

### Modo DNS over TLS (DoT)

```bash
# Cloudflare DoT
./build/resolver --mode dot --server 1.1.1.1 --sni one.one.one.one -n example.com

# Google DoT
./build/resolver --mode dot --server 8.8.8.8 --sni dns.google -n example.com
```

---

## 4. DNSSEC

### Validação DNSSEC

```bash
# Validar com DNSSEC
./build/resolver -n cloudflare.com --dnssec

# Com trace para ver cadeia de validação
./build/resolver -n cloudflare.com --dnssec --trace
```

---

## 5. Cache

### Usar Resolver com Cache

```bash
# 1. Iniciar daemon
./build/cache_daemon --activate

# 2. Usar resolver (cache automático)
./build/resolver -n google.com -q
```

---

## 6. Batch Processing

```bash
# Criar lista de domínios
cat > domains.txt << EOF
google.com
github.com
stackoverflow.com
EOF

# Processar em batch
./build/resolver --batch domains.txt --workers 4
```

---

## 7. Performance

### Fan-out Paralelo

```bash
# Consultar múltiplos NS em paralelo
./build/resolver -n example.com --fanout
```

---

## 8. Parâmetros Avançados

### Timeout e Iterations

```bash
# Timeout customizado
./build/resolver -n example.com --timeout 10

# Limite de iterações
./build/resolver -n example.com --max-iterations 20
```

---

**Para mais informações:**
- `README.md` - Documentação completa
- `QUICKSTART.md` - Guia de instalação
- `examples/` - Exemplos práticos

---

**Happy Resolving!** 🚀

