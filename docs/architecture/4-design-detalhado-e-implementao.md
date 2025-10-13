# 4. Design Detalhado e Implementação

Esta seção descreve a estrutura das classes C++ que implementarão a arquitetura.

## 4.1. Lógica de Parsing e Serialização (DNSParser.h)

Para encapsular a complexidade de converter mensagens DNS entre a estrutura de dados e o formato de fio binário (incluindo *endianness* e compressão de nomes), uma classe utilitária estática será usada.

```cpp
// include/dns_resolver/DNSParser.h
#include "types.h"
#include <vector>

class DNSParser {
public:
    static std::vector<uint8_t> serialize(const DNSMessage& message);
    static DNSMessage parse(const std::vector<uint8_t>& buffer);
};
```

## 4.2. Estruturas de Dados Globais (types.h)

As estruturas de dados centrais, com a sobrecarga de operador necessária para o cache.

```cpp
// include/dns_resolver/types.h
#include <vector>
#include <string>

struct DNSQuestion {
    std::string qname;
    uint16_t qtype;
    uint16_t qclass;

    // Sobrecarga do operador '<' para permitir o uso em std::map
    bool operator<(const DNSQuestion& other) const {
        if (qname != other.qname) return qname < other.qname;
        if (qtype != other.qtype) return qtype < other.qtype;
        return qclass < other.qclass;
    }
};

// ... outras estruturas como DNSHeader, ResourceRecord, DNSMessage ...
```

## 4.3. Design das Classes Principais

```cpp
// include/dns_resolver/NetworkModule.h
class NetworkModule {
public:
    std::vector<uint8_t> queryUDP(const std::string& server, const std::vector<uint8_t>& query);
    std::vector<uint8_t> queryTCP(const std::string& server, const std::vector<uint8_t>& query);
    std::vector<uint8_t> queryDoT(const std::string& server, const std::string& sni, const std::vector<uint8_t>& query);
};

// include/dns_resolver/ResolverEngine.h
class ResolverEngine {
public:
    ResolverEngine(const Config& config);
    DNSMessage resolve(const std::string& domain, uint16_t qtype);
private:
    DNSMessage performRecursiveLookup(const std::string& domain, uint16_t qtype, const std::string& server);
    // ... membros para os outros módulos
};
```

## 4.4. Design do Daemon de Cache (CacheService.h)

A classe principal do daemon, destacando a proteção de *thread-safety* com `std::mutex`.

```cpp
// src/daemon/CacheService.h
#include "dns_resolver/types.h"
#include <map>
#include <mutex>

class CacheService {
public:
    void run(); // Inicia o loop do servidor
private:
    void handleConnection(int client_socket);

    std::mutex cache_mutex; // Mutex para proteger o acesso concorrente

    // Estruturas de dados do cache
    std::map<DNSQuestion, DNSMessage> positiveCache;
    std::map<DNSQuestion, DNSMessage> negativeCache;
};
```
