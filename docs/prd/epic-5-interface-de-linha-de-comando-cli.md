# EPIC 5: Interface de Linha de Comando (CLI)
*Objetivo: Criar a interface de usuário final para interagir com todas as funcionalidades do resolvedor.*

**STORY 5.1:** Como um usuário, quero especificar o nome do domínio e o tipo de consulta (`--name`, `--qtype`).

**STORY 5.2:** Como um usuário, quero controlar o modo de operação do resolvedor (`--mode`).

**STORY 5.3 (Atualizada):** Como um usuário, quero poder fornecer todos os parâmetros de configuração e depuração.
- **Critérios de Aceitação:**
    - A CLI deve parsear os seguintes argumentos: `--ns`, `--sni`, `--trust-anchor`, `--fanout`, `--workers`, `--timeout` e `--trace`.
    - Valores padrão devem ser usados se os argumentos não forem fornecidos.

---
