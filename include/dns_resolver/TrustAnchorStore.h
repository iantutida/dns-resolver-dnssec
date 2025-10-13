#ifndef TRUST_ANCHOR_STORE_H
#define TRUST_ANCHOR_STORE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace dns_resolver {

/**
 * Trust Anchor (Âncora de Confiança) - representa um DS record
 * usado como ponto inicial da cadeia de confiança DNSSEC.
 */
struct TrustAnchor {
    std::string zone;              // Nome da zona (ex: "." para root)
    uint16_t key_tag;              // Identificador da chave (ex: 20326)
    uint8_t algorithm;             // Algoritmo criptográfico (8=RSA/SHA-256)
    uint8_t digest_type;           // Tipo de hash (2=SHA-256, 1=SHA-1)
    std::vector<uint8_t> digest;   // Hash da DNSKEY pública
};

/**
 * Gerencia trust anchors carregados de arquivo ou hardcoded.
 */
class TrustAnchorStore {
public:
    TrustAnchorStore();
    
    // Carregar trust anchors de arquivo DS record
    void loadFromFile(const std::string& filepath, bool quiet = false);
    
    // Adicionar trust anchor manualmente
    void addTrustAnchor(const TrustAnchor& ta);
    
    // Obter trust anchors para uma zona específica
    std::vector<TrustAnchor> getTrustAnchorsForZone(const std::string& zone) const;
    
    // Verificar se há trust anchor para zona
    bool hasTrustAnchor(const std::string& zone) const;
    
    // Carregar Root Trust Anchor padrão (KSK 2024)
    void loadDefaultRootAnchor(bool quiet = false);
    
    // Obter total de trust anchors carregados
    size_t count() const;
    
private:
    // Mapa: zona → lista de trust anchors
    std::map<std::string, std::vector<TrustAnchor>> anchors_;
    
    // Helpers internos
    TrustAnchor parseDSRecord(const std::string& line);
    std::vector<uint8_t> parseHexString(const std::string& hex);
    bool isValidAlgorithm(uint8_t alg) const;
    bool isValidDigestType(uint8_t dt) const;
};

}  // namespace dns_resolver

#endif  // TRUST_ANCHOR_STORE_H

