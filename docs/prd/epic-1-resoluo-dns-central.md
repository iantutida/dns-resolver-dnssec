# EPIC 1: Resolução DNS Central
*Objetivo: Construir o motor de resolução DNS fundamental, capaz de realizar consultas recursivas básicas.*

**STORY 1.1:** Como um usuário, quero que o resolvedor construa e envie uma consulta DNS padrão via UDP.

**STORY 1.2:** Como um desenvolvedor, quero que o resolvedor receba e parseie a resposta DNS.

**STORY 1.3:** Como um usuário, quero que o resolvedor siga as delegações (registros NS) para realizar uma resolução recursiva completa.

**STORY 1.4:** Como um usuário, quero que o resolvedor siga corretamente os registros CNAME encadeados.

**STORY 1.5 (Atualizada):** Como um usuário, quero que o resolvedor interprete e exiba corretamente as respostas negativas.
- **Critérios de Aceitação:**
    - O campo `RCODE` no cabeçalho da resposta deve ser verificado.
    - Se `RCODE=3`, o programa deve informar ao usuário que o domínio é inexistente (NXDOMAIN).
    - Se `RCODE=0` (NoError) mas a seção de Resposta estiver vazia, o programa deve informar que não há registros do tipo solicitado (NODATA).

---
