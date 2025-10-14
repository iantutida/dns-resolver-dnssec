#include "dns_resolver/ResolverEngine.h"
#include "dns_resolver/ThreadPool.h"
#include <iostream>
#include <random>
#include <stdexcept>
#include <sstream>

namespace dns_resolver {

// ========== CONSTRUTOR ==========

ResolverEngine::ResolverEngine(const ResolverConfig& config)
    : config_(config) {
    // Validar configuração
    if (config_.root_servers.empty()) {
        throw std::invalid_argument("Root servers list cannot be empty");
    }
    
    if (config_.max_iterations < 1) {
        throw std::invalid_argument("max_iterations must be >= 1");
    }
    
    // Carregar trust anchors (Story 3.1)
    if (!config_.trust_anchor_file.empty()) {
        trust_anchors_.loadFromFile(config_.trust_anchor_file, config_.quiet_mode);
    } else {
        trust_anchors_.loadDefaultRootAnchor(config_.quiet_mode);
    }
    
    traceLog("Trust anchors loaded: " + std::to_string(trust_anchors_.count()));
    
    // STORY 4.2: Configurar cache client
    cache_client_.setTraceEnabled(config_.trace_mode);
}

// ========== MÉTODO PRINCIPAL: RESOLVE ==========

DNSMessage ResolverEngine::resolve(const std::string& domain, uint16_t qtype) {
    // Validar entrada
    if (domain.empty()) {
        throw std::invalid_argument("Domain cannot be empty");
    }
    
    // STORY 4.2: Consultar cache primeiro
    auto cached_response = cache_client_.query(domain, qtype);
    if (cached_response) {
        // Cache HIT - retornar diretamente
        return *cached_response;
    }
    
    // Cache MISS ou offline - continuar com resolução normal
    
    // Limpar cache de servidores consultados
    queried_servers_.clear();
    
    // STORY 3.3: Limpar registros DNSSEC coletados
    collected_dnskeys_.clear();
    collected_ds_.clear();
    
    // Selecionar root server aleatório
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, config_.root_servers.size() - 1);
    std::string root_server = config_.root_servers[dis(gen)];
    
    traceLog("========================================");
    traceLog("Starting resolution for " + domain + " (type " + std::to_string(qtype) + ")");
    traceLog("Initial root server: " + root_server);
    traceLog("========================================");
    
    // STORY 3.3: Coletar DNSKEY root no início (se DNSSEC ativo)
    if (config_.dnssec_enabled) {
        collectDNSKEY(".", root_server);
    }
    
    // Iniciar resolução iterativa
    try {
        DNSMessage result = performIterativeLookup(domain, qtype, root_server, 0);
        traceLog("========================================");
        traceLog("Resolution completed successfully");
        traceLog("========================================");
        
        // STORY 3.3: Validar cadeia DNSSEC se ativo
        if (config_.dnssec_enabled && !collected_dnskeys_.empty()) {
            traceLog("");
            DNSSECValidator validator(trust_anchors_, config_.trace_mode);
            ValidationResult validation = validator.validateChain(
                domain,
                collected_dnskeys_,
                collected_ds_
            );
            
            // STORY 3.5: Mapear ValidationResult → AD bit
            if (validation == ValidationResult::Secure) {
                traceLog("🔒 DNSSEC Status: SECURE");
                result.header.ad = true;
                traceLog("Setting AD=1 (authenticated data)");
            } else if (validation == ValidationResult::Insecure) {
                traceLog("⚠️  DNSSEC Status: INSECURE (zone not signed)");
                result.header.ad = false;
                traceLog("Keeping AD=0 (zone unsigned)");
            } else if (validation == ValidationResult::Bogus) {
                traceLog("❌ DNSSEC Status: BOGUS (validation failed - possible attack!)");
                result.header.ad = false;
                traceLog("Keeping AD=0 (bogus)");
                throw std::runtime_error("DNSSEC validation failed!");
            } else {
                traceLog("❓ DNSSEC Status: INDETERMINATE (insufficient data)");
                result.header.ad = false;
                traceLog("Keeping AD=0 (indeterminate)");
            }
        }
        
        // STORY 4.3: Armazenar resposta bem-sucedida no cache
        if (result.header.rcode == 0 && result.header.ancount > 0) {
            cache_client_.store(result, domain, qtype);
        }
        // STORY 4.4: Armazenar respostas negativas no cache
        else if (isNXDOMAIN(result)) {
            // NXDOMAIN - extrair TTL do SOA
            DNSResourceRecord soa = extractSOA(result);
            uint32_t ttl = (soa.type == DNSType::SOA) ? soa.rdata_soa.minimum : 300;
            cache_client_.storeNegative(domain, qtype, 3, ttl);  // RCODE=3
        }
        else if (isNODATA(result, qtype)) {
            // NODATA - extrair TTL do SOA
            DNSResourceRecord soa = extractSOA(result);
            uint32_t ttl = (soa.type == DNSType::SOA) ? soa.rdata_soa.minimum : 300;
            cache_client_.storeNegative(domain, qtype, 0, ttl);  // RCODE=0, NODATA
        }
        
        return result;
    } catch (const std::exception& e) {
        traceLog("========================================");
        traceLog(std::string("Resolution failed: ") + e.what());
        traceLog("========================================");
        throw;
    }
}

// ========== ALGORITMO ITERATIVO ==========

DNSMessage ResolverEngine::performIterativeLookup(
    const std::string& domain,
    uint16_t qtype,
    const std::string& initial_server,
    int depth
) {
    // Proteção contra recursão profunda
    const int MAX_DEPTH = 5;
    if (depth > MAX_DEPTH) {
        throw std::runtime_error(
            "Recursion depth exceeded (max " + std::to_string(MAX_DEPTH) + ")"
        );
    }
    
    std::string current_server = initial_server;
    int iterations = 0;
    
    while (iterations < config_.max_iterations) {
        iterations++;
        
        traceLog("");
        traceLog("--- Iteration " + std::to_string(iterations) + " ---");
        traceLog("Querying: " + current_server + " for " + domain + 
                 " (type " + std::to_string(qtype) + ")");
        
        // Verificar se já consultamos este servidor (proteção contra loops)
        if (queried_servers_.count(current_server) > 0) {
            traceLog("WARNING: Already queried this server before (possible loop)");
        }
        queried_servers_.insert(current_server);
        
        try {
            // Enviar query e receber resposta
            DNSMessage response = queryServer(current_server, domain, qtype);
            
            // Verificar RCODE
            if (response.header.rcode != 0) {
                traceLog("Got RCODE " + std::to_string(response.header.rcode));
                
                if (response.header.rcode == 3) {
                    traceLog("Domain does not exist (NXDOMAIN)");
                    
                    // STORY 1.5: Extrair e mostrar SOA
                    DNSResourceRecord soa = extractSOA(response);
                    if (soa.type == DNSType::SOA) {
                        traceLog("SOA MINIMUM (negative cache TTL): " + 
                                 std::to_string(soa.rdata_soa.minimum) + " seconds");
                    }
                } else if (response.header.rcode == 2) {
                    traceLog("Server failure (SERVFAIL)");
                } else {
                    traceLog("Error response code");
                }
                
                return response;  // Retornar erro
            }
            
            // Verificar se tem resposta final (ANSWER section não vazia)
            if (response.header.ancount > 0) {
                traceLog("Got authoritative answer with " + 
                         std::to_string(response.header.ancount) + " record(s)");
                
                // Listar records recebidos
                for (const auto& rr : response.answers) {
                    std::ostringstream oss;
                    oss << "  Answer: " << rr.name << " TTL=" << rr.ttl << " Type=" << rr.type;
                    if (rr.type == DNSType::A && !rr.rdata_a.empty()) {
                        oss << " → " << rr.rdata_a;
                    } else if (rr.type == DNSType::NS && !rr.rdata_ns.empty()) {
                        oss << " → " << rr.rdata_ns;
                    } else if (rr.type == DNSType::CNAME && !rr.rdata_cname.empty()) {
                        oss << " → " << rr.rdata_cname;
                    }
                    traceLog(oss.str());
                }
                
                // STORY 1.4: Verificar se resposta contém CNAME que precisa ser seguido
                if (hasCNAME(response, qtype)) {
                    traceLog("Response contains CNAME without target type, following...");
                    return followCNAME(response, domain, qtype, depth);
                }
                
                return response;  // Sucesso!
            }
            
            // STORY 1.5: Verificar se é NODATA
            if (isNODATA(response, qtype)) {
                traceLog("Got NODATA response (domain exists, no records of this type)");
                
                // Extrair SOA para negative cache TTL
                DNSResourceRecord soa = extractSOA(response);
                if (soa.type == DNSType::SOA) {
                    traceLog("SOA MINIMUM (negative cache TTL): " + 
                             std::to_string(soa.rdata_soa.minimum) + " seconds");
                }
                
                return response;  // Retornar NODATA
            }
            
            // Verificar se é delegação
            if (isDelegation(response)) {
                std::vector<std::string> nameservers = extractNameservers(response);
                std::map<std::string, std::string> glue_records = extractGlueRecords(response);
                
                traceLog("Got delegation to " + std::to_string(nameservers.size()) + 
                         " nameserver(s):");
                for (const auto& ns : nameservers) {
                    traceLog("  NS: " + ns);
                }
                
                if (!glue_records.empty()) {
                    traceLog("Glue records available:");
                    for (const auto& glue : glue_records) {
                        traceLog("  " + glue.first + " → " + glue.second);
                    }
                }
                
                // STORY 3.3: Extrair zona delegada ANTES de mudar servidor
                std::string delegated_zone;
                for (const auto& rr : response.authority) {
                    if (rr.type == DNSType::NS && !rr.name.empty()) {
                        delegated_zone = rr.name;
                        break;
                    }
                }
                
                // STORY 3.3: Coletar DS DA ZONA DELEGADA do servidor PAI (antes de mudar)
                if (config_.dnssec_enabled && !delegated_zone.empty() && delegated_zone != ".") {
                    collectDS(delegated_zone, current_server);
                }
                
                // Selecionar próximo servidor (com ou sem fan-out)
                std::string next_server;
                
                // STORY 6.2: Fan-out se habilitado e múltiplos servidores com glue
                if (config_.fanout_enabled && glue_records.size() > 1) {
                    // Coletar IPs dos servers com glue
                    std::vector<std::string> server_ips;
                    for (const auto& ns : nameservers) {
                        auto it = glue_records.find(ns);
                        if (it != glue_records.end()) {
                            server_ips.push_back(it->second);
                        }
                    }
                    
                    if (server_ips.size() > 1) {
                        traceLog("Fan-out enabled: " + std::to_string(server_ips.size()) + " servers available");
                        // Consultar em paralelo (fan-out retorna primeira resposta válida)
                        response = queryServersFanout(server_ips, domain, qtype);
                        
                        // Usar primeiro servidor como "current_server" para logs
                        next_server = server_ips[0];
                    } else {
                        // Só 1 servidor com glue, usar seleção normal
                        next_server = selectNextServer(nameservers, glue_records, depth);
                    }
                } else {
                    // Fallback: seleção sequencial normal
                    next_server = selectNextServer(nameservers, glue_records, depth);
                }
                
                if (next_server.empty()) {
                    throw std::runtime_error("No usable nameserver found in delegation");
                }
                
                traceLog("Next server selected: " + next_server);
                
                // Atualizar servidor atual
                current_server = next_server;
                
                // STORY 3.3: Coletar DNSKEY DA ZONA DELEGADA do novo servidor
                if (config_.dnssec_enabled && !delegated_zone.empty()) {
                    collectDNSKEY(delegated_zone, current_server);
                }
                
                continue;
            }
            
            // Caso inesperado (não é answer, nem delegação, nem NODATA)
            throw std::runtime_error(
                "Unexpected response: no answer, not a delegation, and not NODATA"
            );
            
        } catch (const std::runtime_error& e) {
            std::string error_msg = e.what();
            traceLog("Error querying " + current_server + ": " + error_msg);
            
            // Se foi timeout, podemos tentar outro servidor
            // Por enquanto, propagar exceção
            throw;
        }
    }
    
    // Max iterations atingido
    throw std::runtime_error(
        "Max iterations (" + std::to_string(config_.max_iterations) + ") exceeded"
    );
}

// ========== HELPERS PARA DELEGAÇÕES ==========

bool ResolverEngine::isDelegation(const DNSMessage& response) const {
    // Uma delegação tem:
    // - ANSWER vazio (ancount == 0)
    // - AUTHORITY com NS records (nscount > 0)
    // - RCODE = 0 (NO ERROR)
    return (response.header.ancount == 0) &&
           (response.header.nscount > 0) &&
           (response.header.rcode == 0);
}

std::vector<std::string> ResolverEngine::extractNameservers(
    const DNSMessage& response
) const {
    std::vector<std::string> nameservers;
    
    for (const auto& rr : response.authority) {
        if (rr.type == DNSType::NS && !rr.rdata_ns.empty()) {
            nameservers.push_back(rr.rdata_ns);
        }
    }
    
    return nameservers;
}

std::map<std::string, std::string> ResolverEngine::extractGlueRecords(
    const DNSMessage& response
) const {
    std::map<std::string, std::string> glue_map;
    
    for (const auto& rr : response.additional) {
        if (rr.type == DNSType::A && !rr.rdata_a.empty()) {
            // Priorizar IPv4
            glue_map[rr.name] = rr.rdata_a;
        } else if (rr.type == DNSType::AAAA && !rr.rdata_aaaa.empty()) {
            // Aceitar IPv6 se não houver IPv4
            if (glue_map.count(rr.name) == 0) {
                glue_map[rr.name] = rr.rdata_aaaa;
            }
        }
    }
    
    return glue_map;
}

std::string ResolverEngine::selectNextServer(
    const std::vector<std::string>& nameservers,
    const std::map<std::string, std::string>& glue_records,
    int depth
) {
    if (nameservers.empty()) {
        throw std::runtime_error("No nameservers provided");
    }
    
    // Estratégia 1: Tentar usar glue record primeiro
    for (const auto& ns : nameservers) {
        auto it = glue_records.find(ns);
        if (it != glue_records.end()) {
            traceLog("Using glue record for " + ns + " → " + it->second);
            return it->second;
        }
    }
    
    // Estratégia 2: Resolver primeiro nameserver sem glue
    traceLog("No glue records available");
    traceLog("Recursively resolving nameserver: " + nameservers[0]);
    
    try {
        std::string ns_ip = resolveNameserver(nameservers[0], depth);
        traceLog("Resolved " + nameservers[0] + " → " + ns_ip);
        return ns_ip;
    } catch (const std::exception& e) {
        // Se falhou, tentar próximo nameserver
        if (nameservers.size() > 1) {
            traceLog("Failed to resolve " + nameservers[0] + ", trying next...");
            std::vector<std::string> remaining(nameservers.begin() + 1, nameservers.end());
            return selectNextServer(remaining, glue_records, depth);
        }
        throw;  // Nenhum nameserver funcionou
    }
}

std::string ResolverEngine::resolveNameserver(const std::string& ns_name, int depth) {
    // IMPORTANTE: Usar um root server para evitar dependência circular
    // Exemplo: se resolvendo google.com e NS é ns1.google.com, não podemos
    // usar google.com para resolver ns1.google.com
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, config_.root_servers.size() - 1);
    std::string root_server = config_.root_servers[dis(gen)];
    
    traceLog("  [NS Resolution] Using root server " + root_server);
    
    // Resolver tipo A
    DNSMessage ns_response = performIterativeLookup(
        ns_name,
        DNSType::A,
        root_server,
        depth + 1
    );
    
    // Verificar se obtivemos resposta
    if (ns_response.answers.empty()) {
        throw std::runtime_error("Could not resolve nameserver: " + ns_name);
    }
    
    // Retornar primeiro IP tipo A encontrado
    for (const auto& rr : ns_response.answers) {
        if (rr.type == DNSType::A && !rr.rdata_a.empty()) {
            return rr.rdata_a;
        }
    }
    
    throw std::runtime_error("Nameserver has no A record: " + ns_name);
}

// ========== HELPERS AUXILIARES ==========

void ResolverEngine::traceLog(const std::string& message) const {
    if (config_.trace_mode) {
        std::cerr << ";; " << message << std::endl;
    }
}

uint16_t ResolverEngine::generateTransactionID() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dis(0, 65535);
    return dis(gen);
}

DNSMessage ResolverEngine::queryServer(
    const std::string& server,
    const std::string& domain,
    uint16_t qtype
) {
    // Construir query
    DNSMessage query;
    query.header.id = generateTransactionID();
    query.header.qr = false;
    query.header.opcode = DNSOpcode::QUERY;
    query.header.rd = false;  // NÃO pedir recursão (resolução iterativa)
    query.header.qdcount = 1;
    
    DNSQuestion question(domain, qtype, DNSClass::IN);
    query.questions.push_back(question);
    
    // STORY 3.2: Configurar EDNS0 se DNSSEC ativo
    if (config_.dnssec_enabled) {
        query.use_edns = true;
        query.edns.dnssec_ok = true;
        query.edns.udp_size = 4096;
        
        traceLog("EDNS0 enabled (DO=1, UDP=4096)");
    }
    
    // Serializar
    std::vector<uint8_t> query_bytes = DNSParser::serialize(query);
    
    std::vector<uint8_t> response_bytes;
    DNSMessage response;
    
    // STORY 2.2: Escolher método de comunicação baseado no modo
    switch (config_.mode) {
        case QueryMode::TCP:
            // Modo TCP forçado (Story 2.1)
            traceLog("Using TCP mode (forced)");
            
            response_bytes = NetworkModule::queryTCP(
                server,
                query_bytes,
                config_.timeout_seconds * 2  // TCP timeout maior
            );
            
            response = DNSParser::parse(response_bytes);
            traceLog("TCP response received (" + 
                     std::to_string(response_bytes.size()) + " bytes)");
            break;
        
        case QueryMode::DoT:
            // Modo DoT - DNS over TLS (Story 2.2)
            if (config_.default_sni.empty()) {
                throw std::runtime_error(
                    "DoT mode requires SNI (use --sni hostname)"
                );
            }
            
            traceLog("Using DoT mode (DNS over TLS)");
            traceLog("TLS handshake with " + server + ":853 (SNI: " + 
                     config_.default_sni + ")");
            
            response_bytes = NetworkModule::queryDoT(
                server,
                query_bytes,
                config_.default_sni,
                15  // DoT timeout maior (handshake TLS é lento)
            );
            
            response = DNSParser::parse(response_bytes);
            traceLog("DoT response received (" + 
                     std::to_string(response_bytes.size()) + " bytes, encrypted)");
            break;
        
        case QueryMode::UDP:
        default:
            // Modo UDP padrão (Story 1.1)
            // Com fallback TCP se truncado (Story 2.1)
            response_bytes = NetworkModule::queryUDP(
                server,
                query_bytes,
                config_.timeout_seconds
            );
            
            response = DNSParser::parse(response_bytes);
            
            // STORY 2.1: Verificar se resposta está truncada (TC=1)
            if (isTruncated(response)) {
                traceLog("Response truncated (TC=1), retrying with TCP...");
                traceLog("UDP response size: " + std::to_string(response_bytes.size()) + " bytes");
                
                // Refazer query via TCP
                response_bytes = NetworkModule::queryTCP(
                    server,
                    query_bytes,
                    config_.timeout_seconds * 2  // TCP timeout maior (10s)
                );
                
                response = DNSParser::parse(response_bytes);
                
                traceLog("TCP response received (" + 
                         std::to_string(response_bytes.size()) + " bytes, complete)");
            }
            break;
    }
    
    return response;
}

// ========== IMPLEMENTAÇÃO TCP FALLBACK (STORY 2.1) ==========

bool ResolverEngine::isTruncated(const DNSMessage& response) const {
    return response.header.tc;
}

// ========== IMPLEMENTAÇÃO DE RESPOSTAS NEGATIVAS (STORY 1.5) ==========

bool ResolverEngine::isNXDOMAIN(const DNSMessage& response) const {
    return response.header.rcode == 3;
}

bool ResolverEngine::isNODATA(const DNSMessage& response, [[maybe_unused]] uint16_t qtype) const {
    // NODATA: RCODE=0, sem ANSWER, não é delegação
    if (response.header.rcode != 0) {
        return false;
    }
    
    if (response.header.ancount > 0) {
        return false;
    }
    
    // Verificar se não é delegação (Story 1.3)
    // Se tem NS records na AUTHORITY, é delegação, não NODATA
    for (const auto& rr : response.authority) {
        if (rr.type == DNSType::NS) {
            return false;  // É delegação
        }
    }
    
    // Se chegou aqui: RCODE=0, sem ANSWER, sem NS na AUTHORITY
    // Isso é NODATA
    return true;
}

DNSResourceRecord ResolverEngine::extractSOA(const DNSMessage& response) const {
    for (const auto& rr : response.authority) {
        if (rr.type == DNSType::SOA) {
            return rr;
        }
    }
    
    // Retornar RR vazio se não encontrado
    DNSResourceRecord empty;
    empty.type = 0;
    return empty;
}

// ========== IMPLEMENTAÇÃO DE CNAME (STORY 1.4) ==========

bool ResolverEngine::hasCNAME(const DNSMessage& response, uint16_t qtype) const {
    // CNAME só é válido se:
    // 1. Há pelo menos um registro na seção ANSWER
    // 2. Pelo menos um dos registros é CNAME
    // 3. Não há registro do tipo solicitado (se houver, CNAME é irrelevante)
    
    if (response.answers.empty()) {
        return false;
    }
    
    bool has_cname = false;
    bool has_target = false;
    
    for (const auto& rr : response.answers) {
        if (rr.type == DNSType::CNAME) {
            has_cname = true;
        }
        if (rr.type == qtype) {
            has_target = true;
        }
    }
    
    // Se já tem o tipo solicitado, não precisa seguir CNAME
    // (servidor otimizou e retornou CNAME + registro final)
    return has_cname && !has_target;
}

std::string ResolverEngine::extractCNAME(const DNSMessage& response) const {
    for (const auto& rr : response.answers) {
        if (rr.type == DNSType::CNAME && !rr.rdata_cname.empty()) {
            return rr.rdata_cname;
        }
    }
    return "";
}

bool ResolverEngine::hasTargetType(const DNSMessage& response, uint16_t qtype) const {
    for (const auto& rr : response.answers) {
        if (rr.type == qtype) {
            return true;
        }
    }
    return false;
}

DNSMessage ResolverEngine::followCNAME(
    const DNSMessage& initial_response,
    const std::string& original_query,
    uint16_t qtype,
    int depth
) {
    // Proteção contra loops de CNAME
    if (depth >= MAX_CNAME_DEPTH) {
        throw std::runtime_error(
            "CNAME chain too long (depth > " + std::to_string(MAX_CNAME_DEPTH) + ")"
        );
    }
    
    // Verificar se já temos o registro do tipo solicitado
    if (hasTargetType(initial_response, qtype)) {
        traceLog("Response already includes target type " + std::to_string(qtype) + 
                 " (server optimization)");
        return initial_response;
    }
    
    // Extrair canonical name
    std::string cname = extractCNAME(initial_response);
    if (cname.empty()) {
        throw std::runtime_error("No CNAME found in response");
    }
    
    traceLog("CNAME: " + original_query + " → " + cname);
    traceLog("Following canonical name: " + cname);
    
    // Fazer nova query para o canonical name
    // Começar nova resolução iterativa (pode estar em outro domínio)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, config_.root_servers.size() - 1);
    std::string root_server = config_.root_servers[dis(gen)];
    
    traceLog("Starting new iterative resolution for CNAME target: " + cname);
    
    DNSMessage cname_response = performIterativeLookup(
        cname,
        qtype,
        root_server,
        depth + 1
    );
    
    // Verificar se a nova resposta também contém CNAME (encadeamento)
    if (hasCNAME(cname_response, qtype)) {
        traceLog("Found chained CNAME, following recursively...");
        cname_response = followCNAME(cname_response, cname, qtype, depth + 1);
    }
    
    // Combinar respostas (manter todos os CNAMEs + registro final)
    DNSMessage accumulated_response = initial_response;
    
    // Adicionar registros da resposta CNAME (podem incluir mais CNAMEs ou registro final)
    for (const auto& rr : cname_response.answers) {
        accumulated_response.answers.push_back(rr);
    }
    
    // Atualizar contador
    accumulated_response.header.ancount = 
        static_cast<uint16_t>(accumulated_response.answers.size());
    
    return accumulated_response;
}

// ========== STORY 3.3: Coleta de Registros DNSSEC ==========

void ResolverEngine::collectDNSKEY(const std::string& zone, const std::string& server) {
    if (!config_.dnssec_enabled) {
        return;  // DNSSEC desabilitado
    }
    
    traceLog("Collecting DNSKEY for zone: " + zone + " from " + server);
    
    try {
        DNSMessage response = queryServer(server, zone, DNSType::DNSKEY);
        
        // Extrair DNSKEYs da resposta
        int ksk_count = 0;
        int zsk_count = 0;
        
        for (const auto& rr : response.answers) {
            if (rr.type == DNSType::DNSKEY) {
                collected_dnskeys_[zone].push_back(rr.rdata_dnskey);
                
                if (rr.rdata_dnskey.isKSK()) {
                    ksk_count++;
                } else {
                    zsk_count++;
                }
            }
        }
        
        if (ksk_count > 0 || zsk_count > 0) {
            traceLog("  Collected " + std::to_string(ksk_count) + " KSK(s) and " +
                     std::to_string(zsk_count) + " ZSK(s)");
        } else {
            traceLog("  No DNSKEY records found");
        }
        
    } catch (const std::exception& e) {
        traceLog("  DNSKEY query failed: " + std::string(e.what()));
        // Não é fatal - zona pode não ter DNSSEC
    }
}

void ResolverEngine::collectDS(const std::string& zone, const std::string& server) {
    if (!config_.dnssec_enabled) {
        return;  // DNSSEC desabilitado
    }
    
    traceLog("Collecting DS for zone: " + zone + " from " + server);
    
    try {
        DNSMessage response = queryServer(server, zone, DNSType::DS);
        
        // Extrair DS da resposta
        for (const auto& rr : response.answers) {
            if (rr.type == DNSType::DS) {
                collected_ds_[zone].push_back(rr.rdata_ds);
            }
        }
        
        if (!collected_ds_[zone].empty()) {
            traceLog("  Collected " + std::to_string(collected_ds_[zone].size()) + " DS record(s)");
        } else {
            traceLog("  No DS records found (zone may not be signed)");
        }
        
    } catch (const std::exception& e) {
        traceLog("  DS query failed: " + std::string(e.what()));
        // Não é fatal - zona pode não ter DNSSEC
    }
}

/**
 * STORY 6.2: Fan-out paralelo - consulta múltiplos NS simultaneamente
 */
DNSMessage ResolverEngine::queryServersFanout(
    const std::vector<std::string>& servers,
    const std::string& domain,
    uint16_t qtype
) {
    if (servers.empty()) {
        throw std::runtime_error("No servers provided for fan-out query");
    }
    
    // Se apenas 1 servidor, não usar fan-out
    if (servers.size() == 1) {
        return queryServer(servers[0], domain, qtype);
    }
    
    traceLog(";; Fan-out: Querying " + std::to_string(servers.size()) + " servers in parallel...");
    
    // Criar ThreadPool (reutilizar workers)
    ThreadPool pool(std::min(servers.size(), static_cast<size_t>(8)));
    
    // Lançar queries em paralelo
    std::vector<std::future<std::pair<bool, DNSMessage>>> futures;
    
    for (const auto& server : servers) {
        futures.push_back(pool.enqueue([this, server, domain, qtype]() 
            -> std::pair<bool, DNSMessage> {
            try {
                DNSMessage response = queryServer(server, domain, qtype);
                // Resposta válida se RCODE=0 ou delegação válida
                bool valid = (response.header.rcode == 0);
                return {valid, response};
            } catch (const std::exception&) {
                // Servidor falhou
                return {false, DNSMessage()};
            }
        }));
    }
    
    // Aguardar primeira resposta válida
    DNSMessage first_valid_response;
    bool got_valid = false;
    size_t responded = 0;
    
    for (auto& future : futures) {
        auto [valid, response] = future.get();
        responded++;
        
        if (valid && !got_valid) {
            first_valid_response = response;
            got_valid = true;
            traceLog(";; Fan-out: Got first valid response (" + 
                     std::to_string(responded) + "/" + std::to_string(servers.size()) + ")");
            // Não quebrar loop - aguardar todas as threads terminarem
        }
    }
    
    if (!got_valid) {
        throw std::runtime_error("All servers in fan-out failed to respond");
    }
    
    traceLog(";; Fan-out: Complete (" + std::to_string(responded) + " servers responded)");
    
    return first_valid_response;
}

} // namespace dns_resolver

