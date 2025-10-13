# 5. Estrutura do Projeto e Build

## 5.1. Estrutura de Diretórios

```
dns_resolver/
├── build/
├── docs/
│   ├── PRD.md
│   └── ARCHITECTURE.md
├── include/
│   └── dns_resolver/
│       ├── types.h
│       ├── ResolverEngine.h
│       └── ... (outros .h)
├── src/
│   ├── resolver/
│   │   └── ... (arquivos .cpp do resolvedor)
│   └── daemon/
│       └── ... (arquivos .cpp do daemon)
└── Makefile
```

## 5.2. Makefile

Um `Makefile` automatiza a compilação dos dois executáveis (`resolver` e `cache_daemon`), gerenciando as dependências e flags de compilação, incluindo o link com a biblioteca OpenSSL.

```makefile
# Makefile para o Projeto Resolvedor DNS

CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -g -Wall
LDFLAGS = -lssl -lcrypto -lpthread

SRCDIR_RESOLVER = src/resolver
SRCDIR_DAEMON = src/daemon
BUILDDIR = build

TARGET_RESOLVER = $(BUILDDIR)/resolver
TARGET_DAEMON = $(BUILDDIR)/cache_daemon

SOURCES_RESOLVER = $(wildcard $(SRCDIR_RESOLVER)/*.cpp)
OBJECTS_RESOLVER = $(patsubst $(SRCDIR_RESOLVER)/%.cpp, $(BUILDDIR)/%.o, $(SOURCES_RESOLVER))

SOURCES_DAEMON = $(wildcard $(SRCDIR_DAEMON)/*.cpp)
OBJECTS_DAEMON = $(patsubst $(SRCDIR_DAEMON)/%.cpp, $(BUILDDIR)/%.daemon.o, $(SOURCES_DAEMON))

.PHONY: all clean

all: $(TARGET_RESOLVER) $(TARGET_DAEMON)

$(TARGET_RESOLVER): $(OBJECTS_RESOLVER)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Resolvedor compilado com sucesso: $@"

$(TARGET_DAEMON): $(OBJECTS_DAEMON)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Daemon de cache compilado com sucesso: $@"

$(BUILDDIR)/%.o: $(SRCDIR_RESOLVER)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILDDIR)/%.daemon.o: $(SRCDIR_DAEMON)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@echo "Limpando diretório de build..."
	@rm -rf $(BUILDDIR)