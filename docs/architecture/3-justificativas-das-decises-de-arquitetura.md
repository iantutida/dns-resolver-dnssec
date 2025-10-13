# 3. Justificativas das Decisões de Arquitetura

  * **Cache como Daemon Externo:** Desacopla o ciclo de vida do cache do resolvedor, permitindo persistência e isolamento. Múltiplos processos de resolvedor poderiam, em teoria, compartilhar o mesmo cache central.
  * **Comunicação via Sockets de Domínio Unix:** Escolhido por ser o método de IPC mais performático e seguro para comunicação na mesma máquina, evitando o overhead da pilha de rede TCP/IP.
  * **Módulo de Rede Dedicado:** Isola o código complexo e propenso a erros de sockets de baixo nível, tornando o resto da aplicação mais limpo e focado na lógica de DNS.
  * **Dependência da OpenSSL:** Implementar protocolos como TLS e algoritmos criptográficos como RSA e ECDSA do zero é inviável e inseguro. Utilizar uma biblioteca padrão da indústria como a OpenSSL é a prática correta para garantir segurança e robustez.
