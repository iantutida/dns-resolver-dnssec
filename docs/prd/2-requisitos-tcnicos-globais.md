# 2. Requisitos Técnicos Globais
- **Restrição de Sockets:** Toda a comunicação de rede (UDP, TCP e TLS) deve ser implementada exclusivamente através de chamadas de sistema de sockets de baixo nível (ex: `socket`, `bind`, `sendto`, `recvfrom`). O uso de bibliotecas de alto nível que abstraem a resolução DNS ou a comunicação de sockets é estritamente proibido.

---
