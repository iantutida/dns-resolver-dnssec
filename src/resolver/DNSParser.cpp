/*
 * ----------------------------------------
 * Arquivo: DNSParser.cpp
 * Propósito: Implementação da serialização e parsing de mensagens DNS
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#include "dns_resolver/DNSParser.h"
#include <arpa/inet.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace dns_resolver {

std::vector<uint8_t> DNSParser::serialize(const DNSMessage& message) {
    std::vector<uint8_t> buffer;
    buffer.reserve(512); // Reservar espaço típico para mensagem DNS UDP
    
    // Serializar header DNS (12 bytes obrigatórios)
    
    // ID da transação (2 bytes, big-endian)
    buffer.push_back((message.header.id >> 8) & 0xFF);
    buffer.push_back(message.header.id & 0xFF);
    
    // Flags do header (2 bytes, big-endian)
    uint16_t flags = encodeFlags(message.header);
    buffer.push_back((flags >> 8) & 0xFF);
    buffer.push_back(flags & 0xFF);
    
    // Contadores de seções (8 bytes, big-endian)
    buffer.push_back((message.header.qdcount >> 8) & 0xFF);
    buffer.push_back(message.header.qdcount & 0xFF);
    
    buffer.push_back((message.header.ancount >> 8) & 0xFF);
    buffer.push_back(message.header.ancount & 0xFF);
    
    buffer.push_back((message.header.nscount >> 8) & 0xFF);
    buffer.push_back(message.header.nscount & 0xFF);
    
    buffer.push_back((message.header.arcount >> 8) & 0xFF);
    buffer.push_back(message.header.arcount & 0xFF);
    
    // Serializar seção de questions
    for (const auto& question : message.questions) {
        // Codificar nome de domínio usando formato DNS
        std::vector<uint8_t> encoded_name = encodeDomainName(question.qname);
        buffer.insert(buffer.end(), encoded_name.begin(), encoded_name.end());
        
        // Tipo da query (2 bytes, big-endian)
        buffer.push_back((question.qtype >> 8) & 0xFF);
        buffer.push_back(question.qtype & 0xFF);
        
        // Classe da query (2 bytes, big-endian)
        buffer.push_back((question.qclass >> 8) & 0xFF);
        buffer.push_back(question.qclass & 0xFF);
    }
    
    // Implementação de answers, authority e additional será feita
    // quando necessário para parsing completo de respostas
    
    // Suporte a EDNS0 (Extended DNS) se habilitado
    if (message.use_edns) {
        // Nome root (.) - apenas null byte
        buffer.push_back(0x00);
        
        // Tipo OPT (41) - 2 bytes, big-endian
        buffer.push_back(0x00);
        buffer.push_back(0x29);
        
        // Tamanho máximo de payload UDP - 2 bytes, big-endian
        uint16_t udp_size = message.edns.udp_size;
        buffer.push_back((udp_size >> 8) & 0xFF);
        buffer.push_back(udp_size & 0xFF);
        
        // TTL estendido: [Extended RCODE (1) + Version (1) + Flags (2)]
        uint8_t ext_rcode = 0;  // Extended RCODE sempre 0 por enquanto
        uint8_t version = message.edns.version;
        uint16_t flags = message.edns.dnssec_ok ? 0x8000 : 0x0000;  // Bit DO é bit 15
        
        buffer.push_back(ext_rcode);
        buffer.push_back(version);
        buffer.push_back((flags >> 8) & 0xFF);
        buffer.push_back(flags & 0xFF);
        
        // RDLENGTH: 0 (sem opções adicionais) - 2 bytes
        buffer.push_back(0x00);
        buffer.push_back(0x00);
        
        // Atualizar ARCOUNT no header para incluir OPT
        uint16_t arcount = message.header.arcount + 1;
        buffer[10] = (arcount >> 8) & 0xFF;
        buffer[11] = arcount & 0xFF;
    }
    
    return buffer;
}

// Implementação do parsing de mensagens DNS

DNSMessage DNSParser::parse(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 12) {
        throw std::runtime_error(
            "Resposta DNS muito pequena (" + std::to_string(buffer.size()) + 
            " bytes, mínimo 12)"
        );
    }
    
    DNSMessage message;
    size_t pos = 0;
    
    // Parsear header DNS (12 bytes)
    message.header = parseHeader(buffer, pos);
    
    // Parsear questions (QDCOUNT vezes)
    for (uint16_t i = 0; i < message.header.qdcount; i++) {
        message.questions.push_back(parseQuestion(buffer, pos));
    }
    
    // Parsear answers (ANCOUNT vezes)
    for (uint16_t i = 0; i < message.header.ancount; i++) {
        message.answers.push_back(parseResourceRecord(buffer, pos));
    }
    
    // Parsear authority (NSCOUNT vezes)
    for (uint16_t i = 0; i < message.header.nscount; i++) {
        message.authority.push_back(parseResourceRecord(buffer, pos));
    }
    
    // Parsear additional (ARCOUNT vezes)
    for (uint16_t i = 0; i < message.header.arcount; i++) {
        message.additional.push_back(parseResourceRecord(buffer, pos));
    }
    
    return message;
}

DNSHeader DNSParser::parseHeader(const std::vector<uint8_t>& buffer, size_t& pos) {
    if (pos + 12 > buffer.size()) {
        throw std::runtime_error("Buffer muito pequeno para header DNS");
    }
    
    DNSHeader header;
    
    // ID da transação (2 bytes)
    header.id = readUint16(buffer, pos);
    pos += 2;
    
    // Flags (2 bytes)
    uint16_t flags = readUint16(buffer, pos);
    pos += 2;
    
    // Decodificar flags individuais
    header.qr = (flags & 0x8000) != 0;
    header.opcode = (flags >> 11) & 0x0F;
    header.aa = (flags & 0x0400) != 0;
    header.tc = (flags & 0x0200) != 0;
    header.rd = (flags & 0x0100) != 0;
    header.ra = (flags & 0x0080) != 0;
    header.z = (flags >> 6) & 0x01;  // Bit reservado
    header.ad = (flags & 0x0020) != 0;  // Bit Authenticated Data
    header.rcode = flags & 0x0F;
    
    // Contadores de seções (8 bytes)
    header.qdcount = readUint16(buffer, pos);
    pos += 2;
    header.ancount = readUint16(buffer, pos);
    pos += 2;
    header.nscount = readUint16(buffer, pos);
    pos += 2;
    header.arcount = readUint16(buffer, pos);
    pos += 2;
    
    return header;
}

std::string DNSParser::parseDomainName(
    const std::vector<uint8_t>& buffer,
    size_t& pos,
    int jump_limit
) {
    std::string name;
    int jumps = 0;
    size_t original_pos = pos;
    bool jumped = false;
    
    while (true) {
        if (pos >= buffer.size()) {
            throw std::runtime_error(
                "Parsing de nome de domínio excedeu buffer (pos=" + 
                std::to_string(pos) + ", size=" + std::to_string(buffer.size()) + ")"
            );
        }
        
        uint8_t len = buffer[pos];
        
        // Verificar se é ponteiro de compressão (bits 11)
        if ((len & 0xC0) == 0xC0) {
            if (pos + 1 >= buffer.size()) {
                throw std::runtime_error("Ponteiro de compressão incompleto");
            }
            
            // Extrair offset de 14 bits
            uint16_t offset = ((buffer[pos] & 0x3F) << 8) | buffer[pos + 1];
            
            if (offset >= buffer.size()) {
                throw std::runtime_error(
                    "Ponteiro de compressão inválido: offset " + 
                    std::to_string(offset) + " >= buffer size " + 
                    std::to_string(buffer.size())
                );
            }
            
            if (!jumped) {
                original_pos = pos + 2;  // Salvar posição após ponteiro
                jumped = true;
            }
            
            pos = offset;
            jumps++;
            
            if (jumps > jump_limit) {
                throw std::runtime_error(
                    "Muitos saltos em nome de domínio (possível loop): " + 
                    std::to_string(jumps)
                );
            }
            continue;
        }
        
        // Fim do nome (len = 0)
        if (len == 0) {
            pos++;
            break;
        }
        
        // Validar tamanho do label
        if (len > 63) {
            throw std::runtime_error(
                "Label de nome de domínio muito longo: " + std::to_string(len)
            );
        }
        
        // Verificar se há bytes suficientes para o label
        if (pos + 1 + len > buffer.size()) {
            throw std::runtime_error(
                "Label de nome de domínio excede buffer"
            );
        }
        
        // Adicionar separador se não é o primeiro label
        if (!name.empty()) {
            name += ".";
        }
        
        // Copiar bytes do label
        name.append(
            reinterpret_cast<const char*>(&buffer[pos + 1]),
            len
        );
        
        pos += 1 + len;
    }
    
    // Restaurar posição após ponteiro se houve jump
    if (jumped) {
        pos = original_pos;
    }
    
    return name;
}

DNSQuestion DNSParser::parseQuestion(const std::vector<uint8_t>& buffer, size_t& pos) {
    DNSQuestion question;
    
    // Parsear nome de domínio
    question.qname = parseDomainName(buffer, pos);
    
    // Parsear type e class (4 bytes)
    if (pos + 4 > buffer.size()) {
        throw std::runtime_error("Question DNS incompleta");
    }
    
    question.qtype = readUint16(buffer, pos);
    pos += 2;
    
    question.qclass = readUint16(buffer, pos);
    pos += 2;
    
    return question;
}

DNSResourceRecord DNSParser::parseResourceRecord(
    const std::vector<uint8_t>& buffer,
    size_t& pos
) {
    DNSResourceRecord rr;
    
    // Parsear nome do registro
    rr.name = parseDomainName(buffer, pos);
    
    // Parsear metadados do registro (10 bytes)
    if (pos + 10 > buffer.size()) {
        throw std::runtime_error("Resource Record incompleto");
    }
    
    rr.type = readUint16(buffer, pos);
    pos += 2;
    
    rr.rr_class = readUint16(buffer, pos);
    pos += 2;
    
    rr.ttl = readUint32(buffer, pos);
    pos += 4;
    
    rr.rdlength = readUint16(buffer, pos);
    pos += 2;
    
    // Verificar se há bytes suficientes para RDATA
    if (pos + rr.rdlength > buffer.size()) {
        throw std::runtime_error(
            "RDATA excede buffer (rdlength=" + std::to_string(rr.rdlength) + 
            ", bytes restantes=" + std::to_string(buffer.size() - pos) + ")"
        );
    }
    
    // Copiar RDATA bruto
    rr.rdata.assign(buffer.begin() + pos, buffer.begin() + pos + rr.rdlength);
    
    // Parsear RDATA baseado no tipo específico
    size_t rdata_start = pos;
    size_t rdata_pos = rdata_start;
    
    switch (rr.type) {
        case DNSType::A:  // Registro A (IPv4)
            if (rr.rdlength == 4) {
                rr.rdata_a = std::to_string(buffer[rdata_pos]) + "." +
                             std::to_string(buffer[rdata_pos + 1]) + "." +
                             std::to_string(buffer[rdata_pos + 2]) + "." +
                             std::to_string(buffer[rdata_pos + 3]);
            }
            break;
        
        case DNSType::NS:  // Registro NS (nameserver)
            rr.rdata_ns = parseDomainName(buffer, rdata_pos);
            break;
        
        case DNSType::CNAME:  // Registro CNAME (canonical name)
            rr.rdata_cname = parseDomainName(buffer, rdata_pos);
            break;
        
        case DNSType::SOA:  // Registro SOA (start of authority)
            if (rr.rdlength >= 20) {  // Mínimo: 2 nomes + 5 uint32_t
                rr.rdata_soa.mname = parseDomainName(buffer, rdata_pos);
                rr.rdata_soa.rname = parseDomainName(buffer, rdata_pos);
                
                if (rdata_pos + 20 <= rdata_start + rr.rdlength) {
                    rr.rdata_soa.serial = readUint32(buffer, rdata_pos);
                    rdata_pos += 4;
                    rr.rdata_soa.refresh = readUint32(buffer, rdata_pos);
                    rdata_pos += 4;
                    rr.rdata_soa.retry = readUint32(buffer, rdata_pos);
                    rdata_pos += 4;
                    rr.rdata_soa.expire = readUint32(buffer, rdata_pos);
                    rdata_pos += 4;
                    rr.rdata_soa.minimum = readUint32(buffer, rdata_pos);
                }
            }
            break;
        
        case DNSType::PTR:  // Registro PTR (pointer)
            rr.rdata_ptr = parseDomainName(buffer, rdata_pos);
            break;
        
        case DNSType::MX:  // Registro MX (mail exchange)
            if (rr.rdlength >= 3) {
                uint16_t priority = readUint16(buffer, rdata_pos);
                rdata_pos += 2;
                std::string exchange = parseDomainName(buffer, rdata_pos);
                rr.rdata_mx = std::to_string(priority) + " " + exchange;
            }
            break;
        
        case DNSType::TXT:  // Registro TXT (text)
            if (rr.rdlength > 0) {
                // TXT começa com 1 byte de tamanho
                uint8_t txt_len = buffer[rdata_pos];
                if (rdata_pos + 1 + txt_len <= rdata_start + rr.rdlength) {
                    rr.rdata_txt.assign(
                        reinterpret_cast<const char*>(&buffer[rdata_pos + 1]),
                        txt_len
                    );
                }
            }
            break;
        
        case DNSType::AAAA:  // Registro AAAA (IPv6)
            if (rr.rdlength == 16) {
                // Formato IPv6 simplificado (grupos hex)
                std::ostringstream oss;
                for (int i = 0; i < 8; i++) {
                    if (i > 0) oss << ":";
                    uint16_t group = (buffer[rdata_pos + i * 2] << 8) | 
                                     buffer[rdata_pos + i * 2 + 1];
                    oss << std::hex << group;
                }
                rr.rdata_aaaa = oss.str();
            }
            break;
        
        case DNSType::DNSKEY:  // Registro DNSKEY (chave pública DNSSEC)
            if (rr.rdlength < 4) {
                throw std::runtime_error("DNSKEY RDATA muito pequeno (mínimo 4 bytes)");
            }
            rr.rdata_dnskey.flags = readUint16(buffer, rdata_pos);
            rdata_pos += 2;
            rr.rdata_dnskey.protocol = buffer[rdata_pos];
            rdata_pos += 1;
            rr.rdata_dnskey.algorithm = buffer[rdata_pos];
            rdata_pos += 1;
            
            // Chave pública: resto do RDATA
            if (rdata_pos < rdata_start + rr.rdlength) {
                rr.rdata_dnskey.public_key.assign(
                    buffer.begin() + rdata_pos,
                    buffer.begin() + rdata_start + rr.rdlength
                );
            }
            break;
        
        case DNSType::DS: {  // Registro DS (delegation signer)
            if (rr.rdlength < 4) {
                throw std::runtime_error("DS RDATA muito pequeno (mínimo 4 bytes)");
            }
            rr.rdata_ds.key_tag = readUint16(buffer, rdata_pos);
            rdata_pos += 2;
            rr.rdata_ds.algorithm = buffer[rdata_pos];
            rdata_pos += 1;
            rr.rdata_ds.digest_type = buffer[rdata_pos];
            rdata_pos += 1;
            
            // Digest: resto do RDATA
            if (rdata_pos < rdata_start + rr.rdlength) {
                rr.rdata_ds.digest.assign(
                    buffer.begin() + rdata_pos,
                    buffer.begin() + rdata_start + rr.rdlength
                );
            }
            
            // Validar tamanho do digest
            size_t expected_size = (rr.rdata_ds.digest_type == 2) ? 32 : 20;  // SHA-256=32, SHA-1=20
            if (rr.rdata_ds.digest.size() != expected_size) {
                std::cerr << "Warning: DS digest size mismatch (expected " 
                          << expected_size << ", got " << rr.rdata_ds.digest.size() << ")" << std::endl;
            }
            break;
        }
        
        case DNSType::RRSIG: {  // Registro RRSIG (assinatura DNSSEC)
            if (rr.rdlength < 18) {
                throw std::runtime_error("RRSIG RDATA muito pequeno (mínimo 18 bytes)");
            }
            
            // Tipo coberto (2 bytes)
            rr.rdata_rrsig.type_covered = readUint16(buffer, rdata_pos);
            rdata_pos += 2;
            
            // Algoritmo (1 byte)
            rr.rdata_rrsig.algorithm = buffer[rdata_pos];
            rdata_pos += 1;
            
            // Labels (1 byte)
            rr.rdata_rrsig.labels = buffer[rdata_pos];
            rdata_pos += 1;
            
            // TTL original (4 bytes)
            rr.rdata_rrsig.original_ttl = readUint32(buffer, rdata_pos);
            rdata_pos += 4;
            
            // Expiração da assinatura (4 bytes)
            rr.rdata_rrsig.signature_expiration = readUint32(buffer, rdata_pos);
            rdata_pos += 4;
            
            // Início da assinatura (4 bytes)
            rr.rdata_rrsig.signature_inception = readUint32(buffer, rdata_pos);
            rdata_pos += 4;
            
            // Key tag (2 bytes)
            rr.rdata_rrsig.key_tag = readUint16(buffer, rdata_pos);
            rdata_pos += 2;
            
            // Nome do signatário
            rr.rdata_rrsig.signer_name = parseDomainName(buffer, rdata_pos);
            
            // Assinatura (resto do RDATA)
            if (rdata_pos < rdata_start + rr.rdlength) {
                rr.rdata_rrsig.signature.assign(
                    buffer.begin() + rdata_pos,
                    buffer.begin() + rdata_start + rr.rdlength
                );
            }
            break;
        }
        
        default:
            // Tipo desconhecido - RDATA bruto já foi copiado
            break;
    }
    
    pos += rr.rdlength;
    return rr;
}

uint16_t DNSParser::readUint16(const std::vector<uint8_t>& buffer, size_t pos) {
    if (pos + 2 > buffer.size()) {
        throw std::runtime_error("Tentativa de ler uint16 além do buffer");
    }
    return (static_cast<uint16_t>(buffer[pos]) << 8) | buffer[pos + 1];
}

uint32_t DNSParser::readUint32(const std::vector<uint8_t>& buffer, size_t pos) {
    if (pos + 4 > buffer.size()) {
        throw std::runtime_error("Tentativa de ler uint32 além do buffer");
    }
    return (static_cast<uint32_t>(buffer[pos]) << 24) |
           (static_cast<uint32_t>(buffer[pos + 1]) << 16) |
           (static_cast<uint32_t>(buffer[pos + 2]) << 8) |
           buffer[pos + 3];
}

std::vector<uint8_t> DNSParser::encodeDomainName(const std::string& domain) {
    std::vector<uint8_t> encoded;
    
    // Validação básica do domínio
    if (domain.empty()) {
        throw std::invalid_argument("Nome de domínio vazio");
    }
    
    if (domain.length() > 255) {
        throw std::invalid_argument("Nome de domínio excede 255 caracteres");
    }
    
    // Caso especial: domínio root "."
    if (domain == ".") {
        encoded.push_back(0x00);  // Root é apenas null byte
        return encoded;
    }
    
    // Dividir domínio em labels (separados por '.')
    std::istringstream iss(domain);
    std::string label;
    
    while (std::getline(iss, label, '.')) {
        if (label.empty()) {
            // Ignorar labels vazios (ex: trailing dot em "example.com.")
            continue;
        }
        
        if (label.length() > 63) {
            throw std::invalid_argument("Label excede 63 caracteres: " + label);
        }
        
        // Adicionar tamanho do label
        encoded.push_back(static_cast<uint8_t>(label.length()));
        
        // Adicionar bytes do label
        for (char ch : label) {
            encoded.push_back(static_cast<uint8_t>(ch));
        }
    }
    
    // Adicionar terminador nulo
    encoded.push_back(0x00);
    
    return encoded;
}

uint16_t DNSParser::encodeFlags(const DNSHeader& header) {
    uint16_t flags = 0;
    
    // QR (bit 15)
    if (header.qr) {
        flags |= (1 << 15);
    }
    
    // OPCODE (bits 14-11)
    flags |= ((header.opcode & 0x0F) << 11);
    
    // AA (bit 10)
    if (header.aa) {
        flags |= (1 << 10);
    }
    
    // TC (bit 9)
    if (header.tc) {
        flags |= (1 << 9);
    }
    
    // RD (bit 8)
    if (header.rd) {
        flags |= (1 << 8);
    }
    
    // RA (bit 7)
    if (header.ra) {
        flags |= (1 << 7);
    }
    
    // Z (bit 6) - reservado, deve ser 0
    flags |= ((header.z & 0x01) << 6);
    
    // AD (bit 5) - Authenticated Data
    if (header.ad) {
        flags |= (1 << 5);
    }
    
    // CD (bit 4) - Checking Disabled (não usado por enquanto)
    // Mantido 0
    
    // RCODE (bits 3-0)
    flags |= (header.rcode & 0x0F);
    
    return flags;
}

} // namespace dns_resolver

