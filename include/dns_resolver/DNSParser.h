#pragma once

#include "types.h"
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace dns_resolver {

/**
 * Classe utilitária para serialização e parsing de mensagens DNS
 * Converte entre DNSMessage (estrutura C++) e formato binário (wire format)
 */
class DNSParser {
public:
    /**
     * Serializa uma mensagem DNS para formato binário (RFC 1035)
     * @param message A mensagem DNS a ser serializada
     * @return Buffer binário pronto para envio via rede
     * @throws std::invalid_argument se a mensagem for inválida
     */
    static std::vector<uint8_t> serialize(const DNSMessage& message);
    
    /**
     * Parse de uma mensagem DNS do formato binário (STORY 1.2)
     * @param buffer Buffer binário recebido da rede
     * @return Mensagem DNS parseada
     * @throws std::runtime_error se o buffer for inválido
     */
    static DNSMessage parse(const std::vector<uint8_t>& buffer);

private:
    // ========== FUNÇÕES DE SERIALIZAÇÃO (STORY 1.1) ==========
    
    /**
     * Codifica um nome de domínio no formato de labels DNS
     * Exemplo: "www.google.com" -> [3]www[6]google[3]com[0]
     * @param domain Nome de domínio (ex: "google.com")
     * @return Buffer com o nome codificado
     * @throws std::invalid_argument se o domínio for inválido
     */
    static std::vector<uint8_t> encodeDomainName(const std::string& domain);
    
    /**
     * Converte as flags do DNSHeader para o formato de 16 bits
     * @param header Cabeçalho DNS
     * @return Valor de 16 bits com todas as flags empacotadas
     */
    static uint16_t encodeFlags(const DNSHeader& header);
    
    // ========== FUNÇÕES DE PARSING (STORY 1.2) ==========
    
    /**
     * Parseia o cabeçalho DNS (12 bytes)
     * @param buffer Buffer com a mensagem DNS
     * @param pos Posição inicial (será atualizada)
     * @return Header parseado
     * @throws std::runtime_error se o buffer for inválido
     */
    static DNSHeader parseHeader(const std::vector<uint8_t>& buffer, size_t& pos);
    
    /**
     * Decodifica um nome de domínio do formato DNS (com suporte a compressão)
     * Suporta labels normais e ponteiros de compressão (RFC 1035 §4.1.4)
     * @param buffer Buffer com a mensagem DNS
     * @param pos Posição inicial (será atualizada para após o nome)
     * @param jump_limit Limite de saltos para evitar loops infinitos
     * @return Nome de domínio decodificado (ex: "www.google.com")
     * @throws std::runtime_error se houver erro de parsing ou loop detectado
     */
    static std::string parseDomainName(
        const std::vector<uint8_t>& buffer,
        size_t& pos,
        int jump_limit = 10
    );
    
    /**
     * Parseia uma question DNS
     * @param buffer Buffer com a mensagem DNS
     * @param pos Posição inicial (será atualizada)
     * @return Question parseada
     * @throws std::runtime_error se o buffer for inválido
     */
    static DNSQuestion parseQuestion(const std::vector<uint8_t>& buffer, size_t& pos);
    
    /**
     * Parseia um resource record DNS
     * @param buffer Buffer com a mensagem DNS
     * @param pos Posição inicial (será atualizada)
     * @return Resource record parseado (com RDATA interpretado por tipo)
     * @throws std::runtime_error se o buffer for inválido
     */
    static DNSResourceRecord parseResourceRecord(
        const std::vector<uint8_t>& buffer,
        size_t& pos
    );
    
    /**
     * Decodifica as flags do header (16 bits) para a estrutura DNSHeader
     * @param flags_value Valor de 16 bits com as flags
     * @return Header com flags decodificadas
     */
    static DNSHeader decodeFlags(uint16_t flags_value);
    
    /**
     * Lê um uint16_t do buffer em network byte order
     * @param buffer Buffer de dados
     * @param pos Posição de leitura
     * @return Valor em host byte order
     */
    static uint16_t readUint16(const std::vector<uint8_t>& buffer, size_t pos);
    
    /**
     * Lê um uint32_t do buffer em network byte order
     * @param buffer Buffer de dados
     * @param pos Posição de leitura
     * @return Valor em host byte order
     */
    static uint32_t readUint32(const std::vector<uint8_t>& buffer, size_t pos);
};

} // namespace dns_resolver

