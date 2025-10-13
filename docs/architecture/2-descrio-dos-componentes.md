# 2. Descrição dos Componentes

  * **CLI Parser (main.cpp):** Ponto de entrada da aplicação. Responsável por analisar todos os argumentos da linha de comando e configurar o `ResolverEngine`.
  * **Resolver Engine (ResolverEngine):** O cérebro da aplicação. Orquestra o fluxo de resolução, gerenciando a lógica recursiva, seguindo delegações (NS) e CNAMEs. Utiliza os outros módulos para executar suas tarefas.
  * **Network Module (NetworkModule):** Abstrai toda a comunicação de rede de baixo nível (UDP, TCP, TLS). Isola o código de sockets do resto da aplicação. Depende da biblioteca `OpenSSL` para a implementação de TLS.
  * **DNSSEC Validator (DNSSECValidator):** Encapsula toda a lógica criptográfica para a validação da cadeia de confiança DNSSEC (DS -\> DNSKEY -\> RRSIG). Depende da biblioteca `OpenSSL` para as operações de hash e verificação de assinatura.
  * **Cache Client (CacheClient):** Módulo dentro do resolvedor principal que se comunica com o Daemon de Cache via IPC. Trata falhas de conexão de forma elegante, permitindo que o resolvedor funcione mesmo se o cache estiver offline.
  * **Concurrency Manager (ThreadPool):** Gerencia o pool de threads (`--workers`) para processar consultas simultâneas e implementa a lógica de `fan-out` para consultar múltiplos servidores de nomes em paralelo.
  * **Cache Daemon (Aplicação Separada):** Um processo de background persistente que fornece serviços de cache. É composto por:
      * **Daemon CLI:** Interface de linha de comando para gerenciar o daemon (`--activate`, `--set`, `--list`, etc.).
      * **IPC Listener:** Ouve por conexões do `CacheClient` em um socket de domínio Unix.
      * **Cache Core:** Implementa o armazenamento em memória para caches positivos e negativos, respeitando os TTLs e garantindo a segurança de threads (thread-safety).
