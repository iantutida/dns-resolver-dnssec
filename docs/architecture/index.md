# Documento de Arquitetura: Resolvedor DNS Validante

## Table of Contents

- [Documento de Arquitetura: Resolvedor DNS Validante](#table-of-contents)
  - [1. Visão Geral da Arquitetura](./1-viso-geral-da-arquitetura.md)
  - [2. Descrição dos Componentes](./2-descrio-dos-componentes.md)
  - [3. Justificativas das Decisões de Arquitetura](./3-justificativas-das-decises-de-arquitetura.md)
  - [4. Design Detalhado e Implementação](./4-design-detalhado-e-implementao.md)
    - [4.1. Lógica de Parsing e Serialização (DNSParser.h)](./4-design-detalhado-e-implementao.md#41-lgica-de-parsing-e-serializao-dnsparserh)
    - [4.2. Estruturas de Dados Globais (types.h)](./4-design-detalhado-e-implementao.md#42-estruturas-de-dados-globais-typesh)
    - [4.3. Design das Classes Principais](./4-design-detalhado-e-implementao.md#43-design-das-classes-principais)
    - [4.4. Design do Daemon de Cache (CacheService.h)](./4-design-detalhado-e-implementao.md#44-design-do-daemon-de-cache-cacheserviceh)
  - [5. Estrutura do Projeto e Build](./5-estrutura-do-projeto-e-build.md)
    - [5.1. Estrutura de Diretórios](./5-estrutura-do-projeto-e-build.md#51-estrutura-de-diretrios)
    - [5.2. Makefile](./5-estrutura-do-projeto-e-build.md#52-makefile)
