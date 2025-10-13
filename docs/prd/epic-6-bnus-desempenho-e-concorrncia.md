# EPIC 6 (Bônus): Desempenho e Concorrência
*Objetivo: Implementar funcionalidades avançadas para melhorar a velocidade e a capacidade de resposta do resolvedor.*

**STORY 6.1 (Nova):** Como um desenvolvedor, quero que o resolvedor utilize um pool de threads para processar múltiplas consultas simultaneamente.
- **Critérios de Aceitação:**
    - O resolvedor deve gerenciar um pool de threads cujo tamanho é definido pelo argumento `--workers`.
    - O sistema deve ser capaz de lidar com várias solicitações de resolução ao mesmo tempo sem bloqueio.

**STORY 6.2 (Nova):** Como um usuário, quero que o resolvedor acelere a resolução consultando múltiplos servidores de nomes em paralelo (*fan-out*).
- **Critérios de Aceitação:**
    - Quando uma delegação com múltiplos registros NS é encontrada, o resolvedor deve enviar consultas para vários desses servidores simultaneamente.
    - A primeira resposta válida recebida deve ser usada para continuar o processo de resolução.
    - O comportamento de *fan-out* deve ser ativável/desativável com o flag `--fanout`.