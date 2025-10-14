#include "dns_resolver/types.h"
#include "dns_resolver/DNSParser.h"
#include "dns_resolver/NetworkModule.h"
#include "dns_resolver/ResolverEngine.h"
#include "dns_resolver/ThreadPool.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>
#include <cstring>
#include <fstream>
#include <chrono>

using namespace dns_resolver;

// Versão do projeto
const char* VERSION = "1.0.0";

/**
 * Gera um ID de transação aleatório para a query DNS
 */
uint16_t generateTransactionID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dis(0, 65535);
    return dis(gen);
}

/**
 * Imprime um buffer em formato hexadecimal (para debug)
 */
void printHex(const std::vector<uint8_t>& buffer) {
    std::cout << "Buffer (hex): ";
    for (size_t i = 0; i < buffer.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(buffer[i]) << " ";
        if ((i + 1) % 16 == 0) std::cout << "\n              ";
    }
    std::cout << std::dec << "\n";
}

/**
 * Formata bytes em hex (para exibir digest/keys)
 */
std::string formatHex(const std::vector<uint8_t>& data, size_t max_bytes = 16) {
    std::ostringstream oss;
    size_t limit = std::min(data.size(), max_bytes);
    for (size_t i = 0; i < limit; i++) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') 
            << static_cast<int>(data[i]);
    }
    if (data.size() > max_bytes) {
        oss << "...";
    }
    return oss.str();
}

/**
 * Retorna nome legível de um RCODE
 */
std::string getRCodeName(uint8_t rcode) {
    switch (rcode) {
        case 0: return "NO ERROR";
        case 1: return "FORMAT ERROR";
        case 2: return "SERVER FAILURE";
        case 3: return "NXDOMAIN";
        case 4: return "NOT IMPLEMENTED";
        case 5: return "REFUSED";
        default: return "UNKNOWN (" + std::to_string(rcode) + ")";
    }
}

/**
 * Retorna nome legível de um tipo DNS
 */
std::string getTypeName(uint16_t type) {
    switch (type) {
        case DNSType::A: return "A";
        case DNSType::NS: return "NS";
        case DNSType::CNAME: return "CNAME";
        case DNSType::SOA: return "SOA";
        case DNSType::PTR: return "PTR";
        case DNSType::MX: return "MX";
        case DNSType::TXT: return "TXT";
        case DNSType::AAAA: return "AAAA";
        case DNSType::DS: return "DS";
        case DNSType::RRSIG: return "RRSIG";
        case DNSType::DNSKEY: return "DNSKEY";
        default: return "TYPE" + std::to_string(type);
    }
}

/**
 * Imprime um resource record formatado
 */
void printResourceRecord(const DNSResourceRecord& rr) {
    std::cout << "    " << rr.name << " ";
    std::cout << rr.ttl << "s ";
    
    // Tipo
    switch (rr.type) {
        case DNSType::A: std::cout << "A "; break;
        case DNSType::NS: std::cout << "NS "; break;
        case DNSType::CNAME: std::cout << "CNAME "; break;
        case DNSType::SOA: std::cout << "SOA "; break;
        case DNSType::PTR: std::cout << "PTR "; break;
        case DNSType::MX: std::cout << "MX "; break;
        case DNSType::TXT: std::cout << "TXT "; break;
        case DNSType::AAAA: std::cout << "AAAA "; break;
        case DNSType::DNSKEY: std::cout << "DNSKEY "; break;
        case DNSType::DS: std::cout << "DS "; break;
        case DNSType::RRSIG: std::cout << "RRSIG "; break;
        default: std::cout << "TYPE" << rr.type << " "; break;
    }
    
    // RDATA
    if (rr.type == DNSType::A && !rr.rdata_a.empty()) {
        std::cout << rr.rdata_a;
    } else if (rr.type == DNSType::NS && !rr.rdata_ns.empty()) {
        std::cout << rr.rdata_ns;
    } else if (rr.type == DNSType::CNAME && !rr.rdata_cname.empty()) {
        std::cout << rr.rdata_cname;
    } else if (rr.type == DNSType::PTR && !rr.rdata_ptr.empty()) {
        std::cout << rr.rdata_ptr;
    } else if (rr.type == DNSType::MX && !rr.rdata_mx.empty()) {
        std::cout << rr.rdata_mx;
    } else if (rr.type == DNSType::TXT && !rr.rdata_txt.empty()) {
        std::cout << "\"" << rr.rdata_txt << "\"";
    } else if (rr.type == DNSType::AAAA && !rr.rdata_aaaa.empty()) {
        std::cout << rr.rdata_aaaa;
    } else if (rr.type == DNSType::SOA) {
        std::cout << rr.rdata_soa.mname << " " << rr.rdata_soa.rname;
    } else if (rr.type == DNSType::DNSKEY) {
        std::cout << "Flags=" << rr.rdata_dnskey.flags << " ";
        std::cout << "(" << (rr.rdata_dnskey.isKSK() ? "KSK" : "ZSK") << ") ";
        std::cout << "Alg=" << static_cast<int>(rr.rdata_dnskey.algorithm) << " ";
        std::cout << "KeySize=" << rr.rdata_dnskey.public_key.size() << "B";
    } else if (rr.type == DNSType::DS) {
        std::cout << "KeyTag=" << rr.rdata_ds.key_tag << " ";
        std::cout << "Alg=" << static_cast<int>(rr.rdata_ds.algorithm) << " ";
        std::cout << "DigestType=" << static_cast<int>(rr.rdata_ds.digest_type) << " ";
        std::cout << "Digest=" << formatHex(rr.rdata_ds.digest, 16);
    } else if (rr.type == DNSType::RRSIG) {
        std::cout << getTypeName(rr.rdata_rrsig.type_covered) << " ";
        std::cout << "Alg=" << static_cast<int>(rr.rdata_rrsig.algorithm) << " ";
        std::cout << "KeyTag=" << rr.rdata_rrsig.key_tag << " ";
        std::cout << "Signer=" << rr.rdata_rrsig.signer_name;
    } else {
        std::cout << "[" << rr.rdlength << " bytes]";
    }
    
    std::cout << "\n";
}

/**
 * Constrói, envia e parseia uma query DNS completa
 */
void testDNSQuery(const std::string& server, const std::string& domain, uint16_t qtype) {
    std::cout << "\n========== TESTE DE QUERY DNS ==========\n";
    std::cout << "Servidor: " << server << "\n";
    std::cout << "Domínio:  " << domain << "\n";
    std::cout << "Tipo:     " << qtype << " (";
    if (qtype == DNSType::A) std::cout << "A";
    else if (qtype == DNSType::NS) std::cout << "NS";
    else if (qtype == DNSType::AAAA) std::cout << "AAAA";
    else std::cout << "?";
    std::cout << ")\n\n";
    
    try {
        // 1. Construir a mensagem DNS
        DNSMessage query;
        
        // Configurar o header
        query.header.id = generateTransactionID();
        query.header.qr = false;                    // Query
        query.header.opcode = DNSOpcode::QUERY;     // Standard query
        query.header.aa = false;
        query.header.tc = false;
        query.header.rd = true;                     // Recursion desired
        query.header.ra = false;
        query.header.z = 0;
        query.header.rcode = DNSRCode::NO_ERROR;
        query.header.qdcount = 1;
        query.header.ancount = 0;
        query.header.nscount = 0;
        query.header.arcount = 0;
        
        // Adicionar a question
        query.questions.emplace_back(domain, qtype, DNSClass::IN);
        
        std::cout << "✓ Mensagem DNS construída\n";
        std::cout << "  Transaction ID: 0x" << std::hex << query.header.id << std::dec << "\n\n";
        
        // 2. Serializar a mensagem
        std::vector<uint8_t> query_buffer = DNSParser::serialize(query);
        
        std::cout << "✓ Query serializada (" << query_buffer.size() << " bytes)\n\n";
        
        // 3. Enviar via UDP e receber resposta
        std::cout << "Enviando query e aguardando resposta...\n";
        std::vector<uint8_t> response_buffer = NetworkModule::queryUDP(server, query_buffer, 5);
        
        std::cout << "✓ Resposta recebida (" << response_buffer.size() << " bytes)\n\n";
        
        // 4. Parsear a resposta
        DNSMessage response = DNSParser::parse(response_buffer);
        
        std::cout << "✓ Resposta parseada com sucesso!\n\n";
        
        // 5. Validar resposta
        if (response.header.id != query.header.id) {
            std::cerr << "⚠️  AVISO: ID da resposta não corresponde (query=0x" 
                      << std::hex << query.header.id << ", response=0x" 
                      << response.header.id << std::dec << ")\n\n";
        }
        
        if (!response.header.qr) {
            std::cerr << "⚠️  AVISO: QR=0 (esperado QR=1 para resposta)\n\n";
        }
        
        // 6. Mostrar informações da resposta
        std::cout << "========== RESPOSTA DNS ==========\n\n";
        
        // Flags
        std::cout << "Flags:\n";
        std::cout << "  QR=" << response.header.qr << " (query/response)\n";
        std::cout << "  AA=" << response.header.aa << " (authoritative)\n";
        std::cout << "  TC=" << response.header.tc << " (truncated)\n";
        std::cout << "  RD=" << response.header.rd << " (recursion desired)\n";
        std::cout << "  RA=" << response.header.ra << " (recursion available)\n";
        std::cout << "  RCODE=" << static_cast<int>(response.header.rcode);
        if (response.header.rcode == 0) std::cout << " (NO ERROR)";
        else if (response.header.rcode == 3) std::cout << " (NXDOMAIN)";
        std::cout << "\n\n";
        
        // Contadores
        std::cout << "Contadores:\n";
        std::cout << "  Questions: " << response.header.qdcount << "\n";
        std::cout << "  Answers: " << response.header.ancount << "\n";
        std::cout << "  Authority: " << response.header.nscount << "\n";
        std::cout << "  Additional: " << response.header.arcount << "\n\n";
        
        // Answers
        if (!response.answers.empty()) {
            std::cout << "ANSWER SECTION:\n";
            for (const auto& rr : response.answers) {
                printResourceRecord(rr);
            }
            std::cout << "\n";
        }
        
        // Authority
        if (!response.authority.empty()) {
            std::cout << "AUTHORITY SECTION:\n";
            for (const auto& rr : response.authority) {
                printResourceRecord(rr);
            }
            std::cout << "\n";
        }
        
        // Additional
        if (!response.additional.empty()) {
            std::cout << "ADDITIONAL SECTION:\n";
            for (const auto& rr : response.additional) {
                printResourceRecord(rr);
            }
            std::cout << "\n";
        }
        
        std::cout << "===================================\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERRO: " << e.what() << "\n";
        std::cerr << "=========================================\n";
    }
}

/**
 * Query DoT direta a servidor específico (Story 2.2)
 */
void queryDoTDirect(
    const std::string& server,
    const std::string& domain,
    uint16_t qtype,
    const std::string& sni,
    bool trace
) {
    std::cout << "\n=================================================\n";
    std::cout << "  DNS over TLS (DoT) - Query Direta\n";
    std::cout << "  Servidor: " << server << ":853\n";
    std::cout << "  SNI:      " << sni << "\n";
    std::cout << "=================================================\n\n";
    
    try {
        // Construir query
        DNSMessage query;
        query.header.id = generateTransactionID();
        query.header.qr = false;
        query.header.opcode = DNSOpcode::QUERY;
        query.header.rd = true;  // Pedir recursão
        query.header.qdcount = 1;
        query.questions.emplace_back(domain, qtype, DNSClass::IN);
        
        if (trace) {
            std::cerr << ";; Construindo query DNS...\n";
            std::cerr << ";; Transaction ID: 0x" << std::hex << query.header.id 
                      << std::dec << "\n";
        }
        
        // Serializar
        std::vector<uint8_t> query_bytes = DNSParser::serialize(query);
        
        if (trace) {
            std::cerr << ";; Query serializada (" << query_bytes.size() << " bytes)\n";
            std::cerr << ";; Iniciando handshake TLS com " << server << ":853\n";
            std::cerr << ";; SNI: " << sni << "\n";
        }
        
        // Enviar via DoT
        std::vector<uint8_t> response_bytes = NetworkModule::queryDoT(
            server,
            query_bytes,
            sni,
            15
        );
        
        if (trace) {
            std::cerr << ";; Resposta DoT recebida (" << response_bytes.size() 
                      << " bytes, criptografada)\n";
            std::cerr << ";; Parseando resposta...\n\n";
        }
        
        // Parsear resposta
        DNSMessage response = DNSParser::parse(response_bytes);
        
        // Exibir resultado (mesmo formato de resolveRecursive)
        std::cout << "\n";
        std::cout << "============================================\n";
        std::cout << "        DNS QUERY RESULT\n";
        std::cout << "============================================\n";
        std::cout << "Query:  " << domain << "\n";
        std::cout << "Type:   " << getTypeName(qtype) << " (" << qtype << ")\n";
        std::cout << "RCODE:  " << getRCodeName(response.header.rcode) << "\n";
        std::cout << "============================================\n\n";
        
        if (response.header.rcode != 0) {
            std::cout << "❌ Error: " << getRCodeName(response.header.rcode) << "\n\n";
        } else if (response.answers.empty()) {
            std::cout << "⚠️  No answer records\n\n";
        } else {
            std::cout << "✅ SUCCESS - Records found\n\n";
            std::cout << "ANSWER SECTION (" << response.answers.size() << " records):\n";
            for (const auto& rr : response.answers) {
                printResourceRecord(rr);
            }
            std::cout << "\n";
        }
        
        std::cout << "============================================\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERRO DoT: " << e.what() << "\n";
        std::cerr << "============================================\n";
    }
}

/**
 * Processa batch de domínios usando ThreadPool (Story 6.1)
 */
void processBatch(
    const std::string& filename,
    uint16_t qtype,
    const ResolverConfig& config,
    size_t num_workers
) {
    // Ler domínios do arquivo
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open batch file: " << filename << "\n";
        return;
    }
    
    std::vector<std::string> domains;
    std::string line;
    while (std::getline(file, line)) {
        // Remover espaços em branco
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Ignorar linhas vazias e comentários
        if (!line.empty() && line[0] != '#') {
            domains.push_back(line);
        }
    }
    file.close();
    
    if (domains.empty()) {
        std::cerr << "Error: No domains found in batch file\n";
        return;
    }
    
    std::cout << "\n=================================================\n";
    std::cout << "  DNS Resolver - Batch Processing\n";
    std::cout << "  Workers:  " << num_workers << "\n";
    std::cout << "  Domains:  " << domains.size() << "\n";
    std::cout << "=================================================\n\n";
    
    // Criar ThreadPool
    ThreadPool pool(num_workers);
    
    // Enfileirar tarefas
    std::vector<std::future<std::pair<std::string, bool>>> results;
    auto start_time = std::chrono::steady_clock::now();
    
    for (const auto& domain : domains) {
        results.push_back(pool.enqueue([domain, qtype, &config]() -> std::pair<std::string, bool> {
            try {
                ResolverEngine resolver(config);
                DNSMessage response = resolver.resolve(domain, qtype);
                
                // Verificar se foi bem-sucedido
                bool success = !response.answers.empty() && response.header.rcode == 0;
                return {domain, success};
            } catch (const std::exception& e) {
                return {domain, false};
            }
        }));
    }
    
    // Coletar resultados
    size_t success_count = 0;
    size_t fail_count = 0;
    
    for (auto& result : results) {
        auto [domain, success] = result.get();
        if (success) {
            std::cout << "✓ " << domain << "\n";
            success_count++;
        } else {
            std::cout << "✗ " << domain << "\n";
            fail_count++;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\n=================================================\n";
    std::cout << "  Batch Processing Complete\n";
    std::cout << "=================================================\n";
    std::cout << "  Success:   " << success_count << "/" << domains.size() << "\n";
    std::cout << "  Failed:    " << fail_count << "/" << domains.size() << "\n";
    std::cout << "  Time:      " << duration.count() << " ms\n";
    std::cout << "  Avg/query: " << (domains.empty() ? 0 : duration.count() / domains.size()) << " ms\n";
    std::cout << "=================================================\n\n";
}

/**
 * Resolve um domínio usando resolução recursiva completa (Story 1.3)
 */
void resolveRecursive(
    const std::string& domain,
    uint16_t qtype,
    const ResolverConfig& config,
    bool quiet_mode = false
) {
    if (!quiet_mode) {
        std::cout << "\n=================================================\n";
        std::cout << "  DNS Resolver - Resolução Recursiva\n";
        
        // Exibir modo
        switch (config.mode) {
            case QueryMode::UDP:
                std::cout << "  Mode: UDP (TCP fallback if truncated)\n";
                break;
            case QueryMode::TCP:
                std::cout << "  Mode: TCP (forced)\n";
                break;
            case QueryMode::DoT:
                std::cout << "  Mode: DoT (DNS over TLS, encrypted)\n";
                std::cout << "  SNI:  " << config.default_sni << "\n";
                break;
        }
        
        std::cout << "=================================================\n\n";
    }
    
    try {
        ResolverEngine resolver(config);
        
        // Resolver
        DNSMessage response = resolver.resolve(domain, qtype);
        
        // STORY 1.5: Detectar e formatar respostas negativas
        if (!quiet_mode) {
            std::cout << "\n";
            std::cout << "============================================\n";
            std::cout << "        DNS QUERY RESULT\n";
            std::cout << "============================================\n";
            std::cout << "Query:  " << domain << "\n";
            std::cout << "Type:   " << getTypeName(qtype) << " (" << qtype << ")\n";
            std::cout << "RCODE:  " << getRCodeName(response.header.rcode) << "\n";
            std::cout << "============================================\n\n";
        }
        
        // NXDOMAIN (domínio não existe)
        if (resolver.isNXDOMAIN(response)) {
            if (quiet_mode) {
                std::cout << "Domain not found\n";
                return;
            }
            
            std::cout << "❌ Domain does not exist (NXDOMAIN)\n\n";
            std::cout << "The domain '" << domain << "' was not found.\n";
            std::cout << "This means the domain is not registered or does not exist.\n\n";
            
            // STORY 3.5: Exibir DNSSEC Status
            if (config.dnssec_enabled) {
                std::cout << "DNSSEC:\n";
                if (response.header.ad) {
                    std::cout << "  Status: Secure (AD=1)\n";
                    std::cout << "  🔒 Negative response authenticated via DNSSEC\n\n";
                } else {
                    std::cout << "  Status: Insecure/Unverified (AD=0)\n";
                    std::cout << "  ⚠️  Negative response not authenticated\n\n";
                }
            }
            
            // Exibir SOA se disponível
            DNSResourceRecord soa = resolver.extractSOA(response);
            if (soa.type == DNSType::SOA) {
                std::cout << "AUTHORITY SECTION (SOA):\n";
                std::cout << "  Zone:              " << soa.name << "\n";
                std::cout << "  Primary NS:        " << soa.rdata_soa.mname << "\n";
                std::cout << "  Responsible Party: " << soa.rdata_soa.rname << "\n";
                std::cout << "  Serial:            " << soa.rdata_soa.serial << "\n";
                std::cout << "  Negative TTL:      " << soa.rdata_soa.minimum << " seconds\n";
            }
            
            std::cout << "\n============================================\n\n";
            return;
        }
        
        // NODATA (domínio existe, mas sem registros do tipo)
        if (resolver.isNODATA(response, qtype)) {
            if (quiet_mode) {
                std::cout << "No data found\n";
                return;
            }
            
            std::cout << "⚠️  No data found (NODATA)\n\n";
            std::cout << "The domain '" << domain << "' exists,\n";
            std::cout << "but has no records of type " << getTypeName(qtype) << ".\n\n";
            std::cout << "This means:\n";
            std::cout << "  • The domain is valid and registered\n";
            std::cout << "  • But it doesn't have " << getTypeName(qtype) << " records configured\n\n";
            
            // STORY 3.5: Exibir DNSSEC Status
            if (config.dnssec_enabled) {
                std::cout << "DNSSEC:\n";
                if (response.header.ad) {
                    std::cout << "  Status: Secure (AD=1)\n";
                    std::cout << "  🔒 Negative response authenticated via DNSSEC\n\n";
                } else {
                    std::cout << "  Status: Insecure/Unverified (AD=0)\n";
                    std::cout << "  ⚠️  Negative response not authenticated\n\n";
                }
            }
            
            // Exibir SOA se disponível
            DNSResourceRecord soa = resolver.extractSOA(response);
            if (soa.type == DNSType::SOA) {
                std::cout << "AUTHORITY SECTION (SOA):\n";
                std::cout << "  Zone:         " << soa.name << "\n";
                std::cout << "  Primary NS:   " << soa.rdata_soa.mname << "\n";
                std::cout << "  Negative TTL: " << soa.rdata_soa.minimum << " seconds\n";
            }
            
            std::cout << "\n============================================\n\n";
            return;
        }
        
        // RESPOSTA POSITIVA
        if (quiet_mode) {
            // Modo quiet: apenas registros
            for (const auto& rr : response.answers) {
                printResourceRecord(rr);
            }
            return;
        }
        
        std::cout << "✅ SUCCESS - Records found\n\n";
        
        // Flags (apenas se não for resposta negativa)
        std::cout << "Flags:\n";
        std::cout << "  QR=" << response.header.qr << " (query/response)\n";
        std::cout << "  AA=" << response.header.aa << " (authoritative)\n";
        std::cout << "  RD=" << response.header.rd << " (recursion desired)\n";
        std::cout << "  RA=" << response.header.ra << " (recursion available)\n\n";
        
        // STORY 3.5: Exibir DNSSEC Status e AD bit
        if (config.dnssec_enabled) {
            std::cout << "DNSSEC:\n";
            if (response.header.ad) {
                std::cout << "  Status: Secure (AD=1)\n";
                std::cout << "  🔒 Data authenticated via DNSSEC\n";
            } else {
                std::cout << "  Status: Insecure/Unverified (AD=0)\n";
                std::cout << "  ⚠️  No DNSSEC authentication\n";
            }
            std::cout << "\n";
        }
        
        // Contadores
        std::cout << "Record Counts:\n";
        std::cout << "  Questions:  " << response.header.qdcount << "\n";
        std::cout << "  Answers:    " << response.header.ancount << "\n";
        std::cout << "  Authority:  " << response.header.nscount << "\n";
        std::cout << "  Additional: " << response.header.arcount << "\n\n";
        
        // Answers
        if (!response.answers.empty()) {
            std::cout << "ANSWER SECTION (" << response.answers.size() << " records):\n";
            for (const auto& rr : response.answers) {
                printResourceRecord(rr);
            }
            std::cout << "\n";
        }
        
        // Authority (para respostas positivas, geralmente vazio)
        if (!response.authority.empty()) {
            std::cout << "AUTHORITY SECTION (" << response.authority.size() << " records):\n";
            for (const auto& rr : response.authority) {
                printResourceRecord(rr);
            }
            std::cout << "\n";
        }
        
        // Additional
        if (!response.additional.empty()) {
            std::cout << "ADDITIONAL SECTION (" << response.additional.size() << " records):\n";
            for (const auto& rr : response.additional) {
                printResourceRecord(rr);
            }
            std::cout << "\n";
        }
        
        std::cout << "============================================\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ ERRO: " << e.what() << "\n";
        std::cerr << "============================================\n";
    }
}

/**
 * Mostra informações de versão
 */
void showVersion() {
    std::cout << "DNS Resolver v" << VERSION << "\n";
    std::cout << "Built with: C++17, OpenSSL 3.x\n";
    std::cout << "Features: UDP, TCP, DoT, DNSSEC, Cache\n";
}

/**
 * Mostra ajuda de uso
 */
void showHelp(const char* prog_name) {
    std::cout << "DNS Resolver - Advanced DNS Client with DNSSEC and Cache\n";
    std::cout << "Version: " << VERSION << "\n\n";
    
    std::cout << "USAGE:\n";
    std::cout << "  " << prog_name << " [OPTIONS]\n\n";
    
    std::cout << "BASIC OPTIONS:\n";
    std::cout << "  --name <domain>, -n <domain>   Domain name to resolve (required)\n";
    std::cout << "  --type <type>, -t <type>       Query type (default: A)\n";
    std::cout << "                                 Supported: A, NS, CNAME, SOA, MX, TXT, AAAA, PTR, DNSKEY, DS, RRSIG\n";
    std::cout << "  --help, -h                     Show this help message\n";
    std::cout << "  --version, -v                  Show version information\n";
    std::cout << "  --quiet, -q                    Minimal output\n\n";
    
    std::cout << "CONNECTION OPTIONS:\n";
    std::cout << "  --server <ip>                  Use specific DNS server (direct query)\n";
    std::cout << "  --mode <udp|tcp|dot>           Connection mode (default: udp)\n";
    std::cout << "  --sni <hostname>               SNI for DoT mode\n";
    std::cout << "  --tcp                          Shortcut for --mode tcp\n\n";
    
    std::cout << "ADVANCED OPTIONS:\n";
    std::cout << "  --timeout <seconds>            Query timeout in seconds (default: 5)\n";
    std::cout << "                                 Valid range: 1-60\n";
    std::cout << "  --max-iterations <n>           Maximum resolution iterations (default: 15)\n";
    std::cout << "                                 Valid range: 1-50\n";
    std::cout << "  --workers <n>                  Thread pool size for batch processing (default: 4)\n";
    std::cout << "                                 Valid range: 1-16\n";
    std::cout << "  --batch <file>                 Process multiple domains from file (one per line)\n";
    std::cout << "  --fanout                       Query multiple nameservers in parallel (reduces latency)\n\n";
    
    std::cout << "DNSSEC OPTIONS:\n";
    std::cout << "  --dnssec                       Enable DNSSEC validation\n";
    std::cout << "  --trust-anchor <file>, -ta <file>\n";
    std::cout << "                                 Trust anchor file (default: built-in root KSK)\n\n";
    
    std::cout << "DEBUG OPTIONS:\n";
    std::cout << "  --trace                        Show detailed resolution trace\n\n";
    
    std::cout << "OPERATION MODES:\n";
    std::cout << "  Recursive (default)            Iterative resolution from root servers\n";
    std::cout << "                                 Usage: --name <domain>\n";
    std::cout << "                                 Supports: --mode udp|tcp (DoT not supported for recursive)\n\n";
    std::cout << "  Direct Query                   Direct query to specific DNS server\n";
    std::cout << "                                 Usage: --server <ip> --name <domain>\n";
    std::cout << "                                 or: <server> <domain> [type]\n";
    std::cout << "                                 Supports: --mode udp|tcp|dot (DoT requires --sni)\n\n";
    
    std::cout << "EXAMPLES:\n";
    std::cout << "  # Recursive mode (default) - UDP\n";
    std::cout << "  " << prog_name << " --name google.com\n";
    std::cout << "  " << prog_name << " -n google.com -t MX\n\n";
    
    std::cout << "  # Recursive mode - TCP forced\n";
    std::cout << "  " << prog_name << " --name google.com --mode tcp\n";
    std::cout << "  " << prog_name << " -n google.com --tcp\n\n";
    
    std::cout << "  # Direct query mode - UDP/TCP\n";
    std::cout << "  " << prog_name << " --server 8.8.8.8 --name example.com\n";
    std::cout << "  " << prog_name << " 8.8.8.8 google.com A\n\n";
    
    std::cout << "  # Direct query mode - DNS over TLS (DoT)\n";
    std::cout << "  " << prog_name << " --server 1.1.1.1 --name google.com --mode dot --sni one.one.one.one\n";
    std::cout << "  " << prog_name << " --server 8.8.8.8 -n example.com --mode dot --sni dns.google\n\n";
    
    std::cout << "  # DNSSEC validation (recursive)\n";
    std::cout << "  " << prog_name << " --name cloudflare.com --dnssec --trace\n";
    std::cout << "  " << prog_name << " -n example.com --dnssec\n\n";
    
    std::cout << "  # Quiet mode (minimal output)\n";
    std::cout << "  " << prog_name << " --name google.com --quiet\n";
    std::cout << "  " << prog_name << " -n example.com -q\n\n";
    
    std::cout << "  # Advanced parameters\n";
    std::cout << "  " << prog_name << " --name google.com --timeout 10 --max-iterations 20\n";
    std::cout << "  " << prog_name << " -n example.com --timeout 3\n\n";
    
    std::cout << "  # Batch processing (BONUS - Story 6.1)\n";
    std::cout << "  " << prog_name << " --batch domains.txt --workers 8\n";
    std::cout << "  " << prog_name << " --batch domains.txt --type MX --workers 4\n\n";
    
    std::cout << "  # Fan-out parallel nameserver queries (BONUS - Story 6.2)\n";
    std::cout << "  " << prog_name << " --name google.com --fanout --trace\n";
    std::cout << "  " << prog_name << " -n example.com --fanout\n\n";
    
    std::cout << "For more information, see: README.md\n";
}

int main(int argc, char* argv[]) {
    // Sem argumentos: mostrar ajuda
    if (argc == 1) {
        showHelp(argv[0]);
        return 0;
    }
    
    // Configuração do resolver
    ResolverConfig config;
    bool use_recursive = false;
    bool quiet_mode = false;
    std::string domain;
    std::string server;
    std::string dot_server;  // Servidor DNS específico para DoT
    uint16_t qtype = DNSType::A;
    size_t num_workers = 4;  // Default workers para batch
    std::string batch_file;  // Arquivo batch
    
    for (int i = 1; i < argc; i++) {
        if ((std::strcmp(argv[i], "--name") == 0 || std::strcmp(argv[i], "-n") == 0) && i + 1 < argc) {
            domain = argv[++i];
            use_recursive = true;
        } else if ((std::strcmp(argv[i], "--type") == 0 || std::strcmp(argv[i], "-t") == 0) && i + 1 < argc) {
            std::string type_str = argv[++i];
            if (type_str == "A" || type_str == "1") qtype = DNSType::A;
            else if (type_str == "NS" || type_str == "2") qtype = DNSType::NS;
            else if (type_str == "CNAME" || type_str == "5") qtype = DNSType::CNAME;
            else if (type_str == "SOA" || type_str == "6") qtype = DNSType::SOA;
            else if (type_str == "PTR" || type_str == "12") qtype = DNSType::PTR;
            else if (type_str == "MX" || type_str == "15") qtype = DNSType::MX;
            else if (type_str == "TXT" || type_str == "16") qtype = DNSType::TXT;
            else if (type_str == "AAAA" || type_str == "28") qtype = DNSType::AAAA;
            else if (type_str == "DS" || type_str == "43") qtype = DNSType::DS;
            else if (type_str == "RRSIG" || type_str == "46") qtype = DNSType::RRSIG;
            else if (type_str == "DNSKEY" || type_str == "48") qtype = DNSType::DNSKEY;
            else {
                std::cerr << "Error: Unknown type '" << type_str << "'\n";
                std::cerr << "Supported types: A, NS, CNAME, SOA, PTR, MX, TXT, AAAA, DNSKEY, DS, RRSIG\n";
                std::cerr << "Try 'resolver --help' for more information\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            std::string mode_str = argv[++i];
            if (mode_str == "udp") {
                config.mode = QueryMode::UDP;
            } else if (mode_str == "tcp") {
                config.mode = QueryMode::TCP;
            } else if (mode_str == "dot") {
                config.mode = QueryMode::DoT;
            } else {
                std::cerr << "Error: Invalid mode '" << mode_str << "'\n";
                std::cerr << "Valid modes: udp, tcp, dot\n";
                std::cerr << "Try 'resolver --help' for more information\n";
                return 1;
            }
            use_recursive = true;
        } else if (std::strcmp(argv[i], "--sni") == 0 && i + 1 < argc) {
            config.default_sni = argv[++i];
            use_recursive = true;
        } else if (std::strcmp(argv[i], "--server") == 0 && i + 1 < argc) {
            dot_server = argv[++i];
            use_recursive = true;
        } else if (std::strcmp(argv[i], "--trace") == 0) {
            config.trace_mode = true;
            use_recursive = true;
        } else if (std::strcmp(argv[i], "--dnssec") == 0) {
            config.dnssec_enabled = true;
            use_recursive = true;
        } else if (std::strcmp(argv[i], "--tcp") == 0) {
            // Backward compatibility com Story 2.1
            config.mode = QueryMode::TCP;
            use_recursive = true;
        } else if ((std::strcmp(argv[i], "--trust-anchor") == 0 || std::strcmp(argv[i], "-ta") == 0) && i + 1 < argc) {
            config.trust_anchor_file = argv[++i];
            use_recursive = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            showHelp(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            showVersion();
            return 0;
        } else if (std::strcmp(argv[i], "--quiet") == 0 || std::strcmp(argv[i], "-q") == 0) {
            quiet_mode = true;
            use_recursive = true;
        } else if (std::strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            try {
                int timeout = std::stoi(argv[++i]);
                if (timeout < 1 || timeout > 60) {
                    std::cerr << "Error: --timeout must be between 1 and 60 seconds\n";
                    std::cerr << "Try 'resolver --help' for more information\n";
                    return 1;
                }
                config.timeout_seconds = timeout;
                use_recursive = true;
            } catch (const std::exception&) {
                std::cerr << "Error: --timeout requires a valid number\n";
                std::cerr << "Try 'resolver --help' for more information\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--max-iterations") == 0 && i + 1 < argc) {
            try {
                int max_iter = std::stoi(argv[++i]);
                if (max_iter < 1 || max_iter > 50) {
                    std::cerr << "Error: --max-iterations must be between 1 and 50\n";
                    std::cerr << "Try 'resolver --help' for more information\n";
                    return 1;
                }
                config.max_iterations = max_iter;
                use_recursive = true;
            } catch (const std::exception&) {
                std::cerr << "Error: --max-iterations requires a valid number\n";
                std::cerr << "Try 'resolver --help' for more information\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--workers") == 0 && i + 1 < argc) {
            try {
                int workers = std::stoi(argv[++i]);
                if (workers < 1 || workers > 16) {
                    std::cerr << "Error: --workers must be between 1 and 16\n";
                    std::cerr << "Try 'resolver --help' for more information\n";
                    return 1;
                }
                num_workers = static_cast<size_t>(workers);
            } catch (const std::exception&) {
                std::cerr << "Error: --workers requires a valid number\n";
                std::cerr << "Try 'resolver --help' for more information\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--batch") == 0 && i + 1 < argc) {
            batch_file = argv[++i];
            use_recursive = true;
        } else if (std::strcmp(argv[i], "--fanout") == 0) {
            config.fanout_enabled = true;
            use_recursive = true;
        } else if (i == 1 && argc >= 3) {
            // Modo direto: servidor domínio [tipo]
            server = argv[i];
            domain = argv[i + 1];
            if (i + 2 < argc) {
                std::string type_str = argv[i + 2];
                if (type_str == "A" || type_str == "1") qtype = DNSType::A;
                else if (type_str == "NS" || type_str == "2") qtype = DNSType::NS;
                else if (type_str == "CNAME" || type_str == "5") qtype = DNSType::CNAME;
                else if (type_str == "SOA" || type_str == "6") qtype = DNSType::SOA;
                else if (type_str == "MX" || type_str == "15") qtype = DNSType::MX;
                else if (type_str == "TXT" || type_str == "16") qtype = DNSType::TXT;
                else if (type_str == "AAAA" || type_str == "28") qtype = DNSType::AAAA;
                else if (type_str == "DS" || type_str == "43") qtype = DNSType::DS;
                else if (type_str == "DNSKEY" || type_str == "48") qtype = DNSType::DNSKEY;
            }
            break;
        }
    }
    
    // Modo batch tem precedência (não requer --name)
    if (!batch_file.empty()) {
        processBatch(batch_file, qtype, config, num_workers);
        return 0;
    }
    
    // Validação de argumentos
    if (domain.empty()) {
        std::cerr << "Error: --name is required (or use direct query: SERVER DOMAIN [TYPE])\n";
        std::cerr << "Try 'resolver --help' for more information\n";
        return 1;
    }
    
    // Validar combinações DoT
    if (config.mode == QueryMode::DoT && dot_server.empty() && server.empty()) {
        std::cerr << "Error: --mode dot requires --server\n";
        std::cerr << "DoT (DNS over TLS) is only supported for direct queries\n";
        std::cerr << "Try 'resolver --help' for more information\n";
        return 1;
    }
    
    if (!dot_server.empty() && config.mode == QueryMode::DoT && config.default_sni.empty()) {
        std::cerr << "Error: DoT mode requires --sni\n";
        std::cerr << "Try 'resolver --help' for more information\n";
        return 1;
    }
    
    if (!config.default_sni.empty() && config.mode != QueryMode::DoT) {
        std::cerr << "Warning: --sni specified but --mode is not 'dot'\n";
        std::cerr << "SNI is only used with DNS over TLS (--mode dot)\n";
        std::cerr << "Ignoring --sni flag\n\n";
    }
    
    // Executar
    if (!dot_server.empty() && config.mode == QueryMode::DoT) {
        // Modo DoT direto (Story 2.2)
        queryDoTDirect(dot_server, domain, qtype, config.default_sni, config.trace_mode);
    } else if (use_recursive || server.empty()) {
        // Modo recursivo (Story 1.3+)
        // Nota: DoT não é suportado em modo recursivo (root servers não suportam DoT)
        if (config.mode == QueryMode::DoT) {
            std::cerr << "Error: DoT is not supported in recursive mode\n";
            std::cerr << "Root servers do not support DNS over TLS\n";
            std::cerr << "Use: --server <IP> --mode dot --sni <hostname> --name <domain>\n";
            std::cerr << "Try 'resolver --help' for more information\n";
            return 1;
        }
        resolveRecursive(domain, qtype, config, quiet_mode);
    } else {
        // Modo direto (Stories 1.1/1.2)
        testDNSQuery(server, domain, qtype);
    }
    
    return 0;
}

