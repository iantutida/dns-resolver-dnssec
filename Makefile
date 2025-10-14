# Makefile para o Projeto Resolvedor DNS
# STORY 1.1: Construir e Enviar Consulta DNS via UDP

# Detectar OpenSSL path (macOS com Homebrew ou Linux)
OPENSSL_PREFIX := $(shell brew --prefix openssl 2>/dev/null || echo "/usr")

CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -I$(OPENSSL_PREFIX)/include -g -Wall -Wextra -Wpedantic
LDFLAGS = -L$(OPENSSL_PREFIX)/lib -lssl -lcrypto -lpthread

SRCDIR = src/resolver
DAEMONDIR = src/daemon
TESTDIR = tests
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj
DAEMONOBJDIR = $(BUILDDIR)/daemon_obj
TESTBINDIR = $(BUILDDIR)/tests

TARGET_RESOLVER = $(BUILDDIR)/resolver
TARGET_DAEMON = $(BUILDDIR)/cache_daemon
TARGET_TEST_PARSER = $(TESTBINDIR)/test_dns_parser
TARGET_TEST_NETWORK = $(TESTBINDIR)/test_network_module
TARGET_TEST_RESOLVER = $(TESTBINDIR)/test_resolver_engine
TARGET_TEST_RESPONSE = $(TESTBINDIR)/test_dns_response_parsing
TARGET_TEST_TCP_FRAMING = $(TESTBINDIR)/test_tcp_framing
TARGET_TEST_DOT = $(TESTBINDIR)/test_dot
TARGET_TEST_TRUST_ANCHOR = $(TESTBINDIR)/test_trust_anchor_store
TARGET_TEST_DNSSEC = $(TESTBINDIR)/test_dnssec_records
TARGET_TEST_VALIDATOR = $(TESTBINDIR)/test_dnssec_validator
TARGET_TEST_THREADPOOL = $(TESTBINDIR)/test_thread_pool

# Arquivos fonte (exceto main.cpp para permitir múltiplos targets depois)
SOURCES_LIB = $(SRCDIR)/DNSParser.cpp $(SRCDIR)/NetworkModule.cpp $(SRCDIR)/ResolverEngine.cpp $(SRCDIR)/TrustAnchorStore.cpp $(SRCDIR)/DNSSECValidator.cpp $(SRCDIR)/CacheClient.cpp
SOURCES_MAIN = $(SRCDIR)/main.cpp

# Arquivos fonte do daemon (Story 4.1)
SOURCES_DAEMON = $(DAEMONDIR)/CacheDaemon.cpp $(DAEMONDIR)/main.cpp

OBJECTS_LIB = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES_LIB))
OBJECTS_MAIN = $(OBJDIR)/main.o
OBJECTS_DAEMON = $(patsubst $(DAEMONDIR)/%.cpp, $(DAEMONOBJDIR)/%.o, $(SOURCES_DAEMON))

.PHONY: all clean run test test-unit help

all: $(TARGET_RESOLVER) $(TARGET_DAEMON)
	@echo "✓ Build completo!"
	@echo "  Resolvedor: ./$(TARGET_RESOLVER)"
	@echo "  Cache Daemon: ./$(TARGET_DAEMON)"

# Testes unitários
test-unit: $(TARGET_TEST_PARSER) $(TARGET_TEST_NETWORK) $(TARGET_TEST_RESPONSE) $(TARGET_TEST_RESOLVER) $(TARGET_TEST_TCP_FRAMING) $(TARGET_TEST_DOT) $(TARGET_TEST_TRUST_ANCHOR) $(TARGET_TEST_DNSSEC) $(TARGET_TEST_VALIDATOR) $(TARGET_TEST_THREADPOOL)
	@echo "\n=========================================="
	@echo "  EXECUTANDO TESTES UNITÁRIOS"
	@echo "==========================================\n"
	@./$(TARGET_TEST_PARSER)
	@./$(TARGET_TEST_NETWORK)
	@./$(TARGET_TEST_RESPONSE)
	@./$(TARGET_TEST_RESOLVER)
	@./$(TARGET_TEST_TCP_FRAMING)
	@./$(TARGET_TEST_DOT)
	@./$(TARGET_TEST_TRUST_ANCHOR)
	@./$(TARGET_TEST_DNSSEC)
	@./$(TARGET_TEST_VALIDATOR)
	@./$(TARGET_TEST_THREADPOOL)
	@echo "\n=========================================="
	@echo "  ✅ TODOS OS TESTES UNITÁRIOS PASSARAM"
	@echo "==========================================\n"

$(TARGET_TEST_PARSER): $(OBJECTS_LIB) $(TESTDIR)/test_dns_parser.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_dns_parser.cpp $(OBJECTS_LIB) $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_NETWORK): $(OBJECTS_LIB) $(TESTDIR)/test_network_module.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_network_module.cpp $(OBJECTS_LIB) $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_RESOLVER): $(OBJECTS_LIB) $(TESTDIR)/test_resolver_engine.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_resolver_engine.cpp $(OBJECTS_LIB) $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_RESPONSE): $(OBJECTS_LIB) $(TESTDIR)/test_dns_response_parsing.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_dns_response_parsing.cpp $(OBJECTS_LIB) $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_TCP_FRAMING): $(TESTDIR)/test_tcp_framing.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_tcp_framing.cpp $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_DOT): $(OBJECTS_LIB) $(TESTDIR)/test_dot.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_dot.cpp $(OBJECTS_LIB) $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_TRUST_ANCHOR): $(OBJECTS_LIB) $(TESTDIR)/test_trust_anchor_store.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_trust_anchor_store.cpp $(OBJECTS_LIB) $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_DNSSEC): $(OBJECTS_LIB) $(TESTDIR)/test_dnssec_records.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_dnssec_records.cpp $(OBJECTS_LIB) $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_VALIDATOR): $(OBJECTS_LIB) $(TESTDIR)/test_dnssec_validator.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_dnssec_validator.cpp $(OBJECTS_LIB) $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_TEST_THREADPOOL): $(TESTDIR)/test_thread_pool.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $(TESTDIR)/test_thread_pool.cpp $(LDFLAGS)
	@echo "✓ Teste compilado: $@"

$(TARGET_RESOLVER): $(OBJECTS_LIB) $(OBJECTS_MAIN)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Resolvedor compilado: $@"

# Cache Daemon (Story 4.1)
$(TARGET_DAEMON): $(OBJECTS_DAEMON) $(OBJECTS_LIB)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Cache daemon compilado: $@"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	@echo "✓ Compilado: $<"

$(DAEMONOBJDIR)/%.o: $(DAEMONDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	@echo "✓ Compilado (daemon): $<"

clean:
	@echo "Limpando diretório de build..."
	@rm -rf $(BUILDDIR)
	@echo "✓ Build limpo!"

run: $(TARGET_RESOLVER)
	@echo "\n========== EXECUTANDO TESTE PADRÃO ==========\n"
	./$(TARGET_RESOLVER)

test: $(TARGET_RESOLVER)
	@echo "\n========== TESTES MANUAIS - STORY 1.1 ==========\n"
	@echo "Teste 1: google.com (8.8.8.8)"
	./$(TARGET_RESOLVER) 8.8.8.8 google.com A
	@echo "\n\nTeste 2: cloudflare.com (1.1.1.1)"
	./$(TARGET_RESOLVER) 1.1.1.1 cloudflare.com A
	@echo "\n\n✓ Testes concluídos!"
	@echo "\nPara capturar pacotes, execute em outro terminal:"
	@echo "  sudo tcpdump -i any -n -X port 53"

help:
	@echo "Makefile - DNS Resolver"
	@echo ""
	@echo "Targets disponíveis:"
	@echo "  make           - Compila o projeto"
	@echo "  make run       - Compila e executa teste padrão"
	@echo "  make test      - Compila e executa múltiplos testes manuais"
	@echo "  make test-unit - Compila e executa testes unitários automatizados"
	@echo "  make clean     - Remove arquivos compilados"
	@echo "  make help      - Mostra esta ajuda"
	@echo ""
	@echo "Uso do resolver:"
	@echo "  ./build/resolver [servidor] [domínio] [tipo]"
	@echo ""
	@echo "Exemplos:"
	@echo "  ./build/resolver"
	@echo "  ./build/resolver 8.8.8.8 google.com A"
	@echo "  ./build/resolver 1.1.1.1 example.com AAAA"

