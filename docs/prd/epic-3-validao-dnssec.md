# EPIC 3: Validação DNSSEC
*Objetivo: Integrar a lógica de validação de segurança para garantir a autenticidade das respostas DNS.*

**STORY 3.1:** Como um usuário, quero que o resolvedor carregue as âncoras de confiança de um arquivo local.

**STORY 3.2:** Como um usuário, quero que o resolvedor solicite registros DNSKEY e DS durante a resolução.

**STORY 3.3:** Como um desenvolvedor, quero que o resolvedor valide a cadeia de confiança (DS -> KSK -> ZSK).

**STORY 3.4:** Como um usuário, quero que o resolvedor valide a assinatura (RRSIG) do conjunto de registros de resposta.

**STORY 3.5:** Como um usuário, quero que o bit `AD` (Authenticated Data) seja setado para `1` se a validação DNSSEC for bem-sucedida.

**STORY 3.6 (Nova):** Como um desenvolvedor, quero que o resolvedor suporte os algoritmos criptográficos necessários para validar as assinaturas RRSIG.
- **Critérios de Aceitação:**
    - A implementação deve ser capaz de validar assinaturas que usam RSA/SHA-256 (código 8).
    - A implementação deve ser capaz de validar assinaturas que usam ECDSA P-256/SHA-256 (código 13).
    - O resolvedor deve invocar a lógica de validação correta com base no campo "Algorithm" do registro RRSIG.

---
