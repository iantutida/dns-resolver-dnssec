/*
 * ----------------------------------------
 * Arquivo: TrustAnchorStore.cpp
 * Propósito: Implementação do gerenciamento de trust anchors DNSSEC
 * Autor: João Victor Zuanazzi Lourenço, Ian Tutida Leite, Tiago Amarilha Rodrigues
 * Data: 14/10/2025
 * Projeto: DNS Resolver Recursivo Validante com Cache e DNSSEC
 * ----------------------------------------
 */

#include "dns_resolver/TrustAnchorStore.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace dns_resolver {

TrustAnchorStore::TrustAnchorStore() {
    // Construtor vazio
}

void TrustAnchorStore::loadFromFile(const std::string& filepath, bool quiet) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open trust anchor file: " + filepath);
    }
    
    std::string line;
    int line_number = 0;
    int loaded = 0;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // Remover whitespace inicial/final
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        if (!line.empty()) {
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
        }
        
        // Ignorar linhas vazias
        if (line.empty()) {
            continue;
        }
        
        // Ignorar comentários (linhas começando com ; ou #)
        if (line[0] == ';' || line[0] == '#') {
            continue;
        }
        
        // Parsear DS record
        try {
            TrustAnchor ta = parseDSRecord(line);
            addTrustAnchor(ta);
            loaded++;
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to parse line " << line_number 
                      << ": " << e.what() << std::endl;
            std::cerr << "  Line: " << line << std::endl;
        }
    }
    
    if (loaded == 0) {
        throw std::runtime_error("No valid trust anchors found in file: " + filepath);
    }
    
    if (!quiet) {
        std::cerr << "Loaded " << loaded << " trust anchor(s) from " << filepath << std::endl;
    }
}

TrustAnchor TrustAnchorStore::parseDSRecord(const std::string& line) {
    std::istringstream iss(line);
    TrustAnchor ta;
    
    std::string class_str, type_str;
    int key_tag, algorithm, digest_type;
    std::string digest_hex;
    
    // Formato: . IN DS 20326 8 2 E06D44B8...
    iss >> ta.zone >> class_str >> type_str >> key_tag >> algorithm >> digest_type >> digest_hex;
    
    // Validar parsing básico
    if (iss.fail() && !iss.eof()) {
        throw std::runtime_error("Invalid DS record format");
    }
    
    // Validar classe
    if (class_str != "IN") {
        throw std::runtime_error("Expected class IN, got: " + class_str);
    }
    
    // Validar tipo
    if (type_str != "DS") {
        throw std::runtime_error("Expected type DS, got: " + type_str);
    }
    
    // Converter valores
    ta.key_tag = static_cast<uint16_t>(key_tag);
    ta.algorithm = static_cast<uint8_t>(algorithm);
    ta.digest_type = static_cast<uint8_t>(digest_type);
    
    // Validar algorithm
    if (!isValidAlgorithm(ta.algorithm)) {
        throw std::runtime_error("Unsupported algorithm: " + std::to_string(algorithm));
    }
    
    // Validar digest type
    if (!isValidDigestType(ta.digest_type)) {
        throw std::runtime_error("Unsupported digest type: " + std::to_string(digest_type));
    }
    
    // Converter digest de hex para bytes
    ta.digest = parseHexString(digest_hex);
    
    // Validar digest size
    size_t expected_size = (ta.digest_type == 2) ? 32 : 20;  // SHA-256=32, SHA-1=20
    if (ta.digest.size() != expected_size) {
        throw std::runtime_error("Digest size mismatch: expected " + 
                                 std::to_string(expected_size) + 
                                 ", got " + std::to_string(ta.digest.size()));
    }
    
    return ta;
}

std::vector<uint8_t> TrustAnchorStore::parseHexString(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("Hex string must have even number of characters");
    }
    
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    
    for (size_t i = 0; i < hex.size(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        try {
            uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
            bytes.push_back(byte);
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid hex character in digest: " + byte_str);
        }
    }
    
    return bytes;
}

bool TrustAnchorStore::isValidAlgorithm(uint8_t alg) const {
    // Algoritmos suportados (conforme será implementado em Stories 3.6)
    // 8 = RSA/SHA-256
    // 13 = ECDSA P-256/SHA-256
    return (alg == 8 || alg == 13);
}

bool TrustAnchorStore::isValidDigestType(uint8_t dt) const {
    // 1 = SHA-1 (legacy, mas ainda usado)
    // 2 = SHA-256 (preferencial)
    return (dt == 1 || dt == 2);
}

void TrustAnchorStore::addTrustAnchor(const TrustAnchor& ta) {
    anchors_[ta.zone].push_back(ta);
}

std::vector<TrustAnchor> TrustAnchorStore::getTrustAnchorsForZone(
    const std::string& zone
) const {
    auto it = anchors_.find(zone);
    if (it != anchors_.end()) {
        return it->second;
    }
    return std::vector<TrustAnchor>();
}

bool TrustAnchorStore::hasTrustAnchor(const std::string& zone) const {
    return anchors_.find(zone) != anchors_.end();
}

void TrustAnchorStore::loadDefaultRootAnchor(bool quiet) {
    // Root KSK 2024 (Key Tag 20326)
    // Fonte oficial: https://data.iana.org/root-anchors/root-anchors.xml
    // Válido a partir de 2024
    TrustAnchor root;
    root.zone = ".";
    root.key_tag = 20326;
    root.algorithm = 8;  // RSA/SHA-256
    root.digest_type = 2;  // SHA-256
    root.digest = parseHexString(
        "E06D44B80B8F1D39A95C0B0D7C65D08458E880409BBC683457104237C7F8EC8D"
    );
    
    addTrustAnchor(root);
    
    if (!quiet) {
        std::cerr << "Loaded default Root Trust Anchor (KSK 20326)" << std::endl;
    }
}

size_t TrustAnchorStore::count() const {
    size_t total = 0;
    for (const auto& pair : anchors_) {
        total += pair.second.size();
    }
    return total;
}

}  // namespace dns_resolver

