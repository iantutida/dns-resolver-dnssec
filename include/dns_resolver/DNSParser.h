/*
 * ----------------------------------------
 * Arquivo: DNSParser.h
 * Propósito: Serialização e parsing de mensagens DNS entre formato binário e estruturas C++
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#pragma once

#include "types.h"
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace dns_resolver {

// Classe utilitária para serialização e parsing de mensagens DNS
// Converte entre DNSMessage (estrutura C++) e formato binário (wire format)
class DNSParser {
public:
    // Serializa uma mensagem DNS para formato binário
    static std::vector<uint8_t> serialize(const DNSMessage& message);
    
    // Parse de uma mensagem DNS do formato binário
    static DNSMessage parse(const std::vector<uint8_t>& buffer);

private:
    // Funções de serialização
    static std::vector<uint8_t> encodeDomainName(const std::string& domain);
    static uint16_t encodeFlags(const DNSHeader& header);
    
    // Funções de parsing
    static DNSHeader parseHeader(const std::vector<uint8_t>& buffer, size_t& pos);
    static std::string parseDomainName(
        const std::vector<uint8_t>& buffer,
        size_t& pos,
        int jump_limit = 10
    );
    static DNSQuestion parseQuestion(const std::vector<uint8_t>& buffer, size_t& pos);
    static DNSResourceRecord parseResourceRecord(
        const std::vector<uint8_t>& buffer,
        size_t& pos
    );
    static DNSHeader decodeFlags(uint16_t flags_value);
    static uint16_t readUint16(const std::vector<uint8_t>& buffer, size_t pos);
    static uint32_t readUint32(const std::vector<uint8_t>& buffer, size_t pos);
};

} // namespace dns_resolver

