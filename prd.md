# Product Requirements Document (PRD): Resolvedor DNS Validante
**Versão:** 2.0
**Status:** Aprovado

## 1. Introdução
Este documento detalha os requisitos funcionais e técnicos para o Resolvedor DNS Validante. Ele servirá como o guia mestre para o desenvolvimento, garantindo que todas as funcionalidades sejam implementadas de acordo com as especificações do projeto acadêmico.

## 2. Requisitos Técnicos Globais
- **Restrição de Sockets:** Toda a comunicação de rede (UDP, TCP e TLS) deve ser implementada exclusivamente através de chamadas de sistema de sockets de baixo nível (ex: `socket`, `bind`, `sendto`, `recvfrom`). O uso de bibliotecas de alto nível que abstraem a resolução DNS ou a comunicação de sockets é estritamente proibido.

---

## EPIC 1: Resolução DNS Central
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

## EPIC 2: Comunicação Avançada e Segura
*Objetivo: Implementar os mecanismos de comunicação robustos, incluindo fallback para TCP e o protocolo DNS over TLS (DoT).*

**STORY 2.1:** Como um usuário, quero que o resolvedor refaça a consulta DNS usando TCP se a resposta UDP vier com o bit de truncamento (TC=1).

**STORY 2.2:** Como um usuário, quero poder realizar consultas DNS seguras usando DNS over TLS (DoT).

---

## EPIC 3: Validação DNSSEC
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

## EPIC 4: Subsistema de Cache
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

## EPIC 5: Interface de Linha de Comando (CLI)
*Objetivo: Criar a interface de usuário final para interagir com todas as funcionalidades do resolvedor.*

**STORY 5.1:** Como um usuário, quero especificar o nome do domínio e o tipo de consulta (`--name`, `--qtype`).

**STORY 5.2:** Como um usuário, quero controlar o modo de operação do resolvedor (`--mode`).

**STORY 5.3 (Atualizada):** Como um usuário, quero poder fornecer todos os parâmetros de configuração e depuração.
- **Critérios de Aceitação:**
    - A CLI deve parsear os seguintes argumentos: `--ns`, `--sni`, `--trust-anchor`, `--fanout`, `--workers`, `--timeout` e `--trace`.
    - Valores padrão devem ser usados se os argumentos não forem fornecidos.

---

## EPIC 6 (Bônus): Desempenho e Concorrência
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