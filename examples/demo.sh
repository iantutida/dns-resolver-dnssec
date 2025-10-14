#!/bin/bash
#
# DNS Resolver - Demo Script
# Este script demonstra as principais funcionalidades do resolver

set -e

# Cores para output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

RESOLVER="../build/resolver"
CACHE_DAEMON="../build/cache_daemon"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║                                                                ║${NC}"
echo -e "${BLUE}║           DNS Resolver - Demo Script                          ║${NC}"
echo -e "${BLUE}║                                                                ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Verificar se resolver existe
if [ ! -f "$RESOLVER" ]; then
    echo -e "${YELLOW}[!] Resolvedor não encontrado. Compilando...${NC}"
    cd ..
    make resolver
    cd examples
fi

echo -e "${GREEN}[1] Resolução DNS Básica${NC}"
echo -e "    Comando: $RESOLVER -n google.com -q"
$RESOLVER -n google.com -q
echo ""

echo -e "${GREEN}[2] Resolução com Tipos Diferentes${NC}"
echo -e "    MX Records:"
$RESOLVER -n google.com -t MX -q
echo ""

echo -e "${GREEN}[3] DNSSEC Validation${NC}"
echo -e "    Comando: $RESOLVER -n cloudflare.com --dnssec -q"
$RESOLVER -n cloudflare.com --dnssec -q
echo ""

echo -e "${GREEN}[4] DNS over TLS (DoT)${NC}"
echo -e "    Comando: $RESOLVER --mode dot --server 1.1.1.1 --sni one.one.one.one -n example.com -q"
$RESOLVER --mode dot --server 1.1.1.1 --sni one.one.one.one -n example.com -q
echo ""

echo -e "${GREEN}[5] Batch Processing (ThreadPool)${NC}"
echo -e "    Processando 5 domínios com 4 workers..."
cat > /tmp/demo_domains.txt << EOF
google.com
github.com
stackoverflow.com
reddit.com
wikipedia.org
EOF
$RESOLVER --batch /tmp/demo_domains.txt --workers 4 --quiet
rm /tmp/demo_domains.txt
echo ""

echo -e "${GREEN}[6] Fan-out Paralelo${NC}"
echo -e "    Consultando múltiplos nameservers em paralelo..."
$RESOLVER -n example.com --fanout -q
echo ""

echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║                                                                ║${NC}"
echo -e "${BLUE}║           Demo Completo! ✅                                     ║${NC}"
echo -e "${BLUE}║                                                                ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Para mais informações: $RESOLVER --help"

