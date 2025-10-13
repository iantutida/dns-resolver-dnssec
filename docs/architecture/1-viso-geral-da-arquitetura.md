# 1. Visão Geral da Arquitetura

O sistema foi projetado com uma **arquitetura modular e orientada a componentes** para promover a separação de preocupações, testabilidade e manutenibilidade.

Ele é composto por duas aplicações principais: o **Resolvedor DNS (Aplicação Cliente)** e o **Daemon de Cache (Aplicação Servidora)**. A comunicação entre eles é feita via Comunicação Inter-Processos (IPC), utilizando **Sockets de Domínio Unix** для máxima eficiência e segurança em ambiente local.

```mermaid
graph TD
    subgraph "Resolvedor DNS (Aplicação Cliente)"
        CLI[1. CLI Parser] --> Engine(2. Resolver Engine)
        Engine --> Net[3. Network Module]
        Engine --> DNSSEC[4. DNSSEC Validator]
        Engine --> Cache[5. Cache Client]
        Engine --> Concurrency[6. Concurrency Manager]
    end

    subgraph "Daemon de Cache (Aplicação Servidora)"
        DaemonCLI[7a. Daemon CLI] --> DaemonCore(7. Cache Daemon Core)
        DaemonCore --> IPCServer(7b. IPC Listener)
    end

    Cache -- "IPC via Unix Socket" --> IPCServer
```
