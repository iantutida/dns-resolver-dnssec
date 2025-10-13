# STORY 6.1: Pool de Threads para Consultas Concorrentes (BÔNUS)

**Epic:** EPIC 6 - Desempenho e Concorrência (Bônus)  
**Status:** Ready for Development  
**Prioridade:** Baixa (Primeira Story EPIC 6 - Bônus)  
**Estimativa:** 2-3 dias

---

## User Story
Como um desenvolvedor, quero que o resolvedor utilize um pool de threads para processar múltiplas consultas simultaneamente, para melhorar throughput em cenários de alto volume.

---

## Contexto e Motivação

**Uso atual:** 1 query por vez (single-threaded)  
**Bônus:** N queries em paralelo (multi-threaded)

**Benefício:** Processar múltiplas queries simultaneamente (batch processing).

---

## Objetivos
- Criar classe `ThreadPool` com workers configuráveis
- Flag `--workers N` (default: 4)
- Processar múltiplas queries em paralelo

---

## Critérios de Aceitação
- [x] ThreadPool implementado
- [x] Flag `--workers` funcional
- [x] Múltiplas queries simultâneas
- [x] Thread-safe

---

**EPIC 6: Story 6.1 - Concorrência com thread pool** 🔀

