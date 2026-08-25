<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Decisões tomadas em modo autônomo

Registro exigido pela **L-34**, seção "Modo autônomo". Enquanto o modo está ligado, o C-level `fable` senta na cadeira do líder para as decisões que teriam ido a ele por `AskUserQuestion` (L-10), e **o main é o escrivão**.

**Regras deste arquivo, que são a contrapartida do poder que o `fable` recebe:**

- Cada decisão é registrada **no instante em que é tomada**, nunca reconstruída ao sair do modo. Log escrito de memória é log inventado.
- Cada entrada traz: **data e hora reais** (L-13), a **fatia ou onda**, **quem decidiu**, **a pergunta como teria ido ao líder**, as **opções na mesa**, a escolhida e o porquê, se é **porta de mão única** (L-19/L-26), e **o custo de reverter**.
- **A saída do modo não está completa** enquanto este registro não for apresentado ao líder, decisão por decisão, para ratificar ou reverter.

**O que o `fable` NÃO herda:** a caneta das leis (só o líder muda `GODS_LAWS.md`), os portões de qualidade (autonomia é sobre **quem decide**, nunca sobre **quanto se verifica**), o dever de contra-argumentar, e as ações irreversíveis que a L-11 cerca.

---

## Sessão de 24/08/2026 — onda W1

**Modo ligado em** `24/08/2026 22:18:53` por linguagem natural, ordem do líder: *"siga até terminar a onda"*. Escopos: `push`, `clean`. Validade: 24h.

**Estado da onda ao ligar:** `FUND-4`, `CONSUME-MODES` e `SPDX` concluídos; `DOC-ESTADO` aguardando verificação; `HDR-HYGIENE` reprovado com conserto decidido e não feito; `VER-4C`, `WL-PROTO`, `PKG-DIST` e `TEST-WLCONT` não começados.

**Decisões do líder que já estavam tomadas antes do modo ligar, e que NÃO são autônomas** (ficam aqui só para não serem confundidas com decisão de agente): o quarto campo do value type de versão; o nome `glintfx` para o módulo pkg-config; a entrada do `gitleaks` no `FUND-4`; os nomes dos campos de versão mantidos; e o teste de higiene guardando o header inteiro.

### Registro

_(nenhuma decisão autônoma tomada ainda nesta sessão)_
