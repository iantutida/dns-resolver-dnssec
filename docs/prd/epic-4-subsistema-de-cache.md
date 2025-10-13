# EPIC 4: Subsistema de Cache
*Objetivo: Desenvolver um daemon de cache externo para armazenar respostas DNS e reduzir a latência.*

**STORY 4.1 (Atualizada):** Como um administrador, quero uma CLI completa para gerenciar o ciclo de vida e as políticas do daemon de cache.
- **Critérios de Aceitação:**
    - O daemon deve rodar como um processo em background.
    - A CLI deve implementar os comandos: `--activate`, `--deactivate`, `--status`, `--flush`.
    - A CLI deve implementar os comandos de gerenciamento de cache: `--set positive <N>`, `--set negative <N>`, `--purge positive`, `--purge negative`, `--purge all`, `--list positive`, `--list negative`, `--list all`.
    - Por padrão, o daemon deve alocar espaço para 50 entradas na cache positiva e 50 na cache negativa.

**STORY 4.2 (Atualizada):** Como um desenvolvedor, quero que o resolvedor consulte o daemon de cache antes de uma resolução, tratando a indisponibilidade do cache de forma elegante.
- **Critérios de Aceitação:**
    - O resolvedor envia a pergunta ao daemon de cache.
    - Se o cache tiver uma resposta válida, ele a retorna imediatamente.
    - Se a tentativa de comunicação com o daemon de cache falhar, o resolvedor deve prosseguir com a resolução de nomes completa e não deve encerrar com erro.

**STORY 4.3:** Como um desenvolvedor, quero que o resolvedor envie as respostas bem-sucedidas para o daemon de cache para armazenamento.

**STORY 4.4 (Nova):** Como um desenvolvedor, quero que o daemon de cache armazene respostas negativas para otimizar o desempenho.
- **Critérios de Aceitação:**
    - O daemon deve armazenar respostas do tipo NXDOMAIN e NODATA.
    - O TTL para essas entradas negativas deve ser extraído do campo `MINIMUM` do registro SOA da zona autoritativa.

---
