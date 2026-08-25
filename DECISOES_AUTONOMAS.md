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

#### D1 — o teste de higiene de header cobre também o Windows  `[24/08/26 - 22:29:00]`

**Quem decidiu:** `fable` (CTO), no replanejamento das cinco fatias restantes. **Fatia:** `HDR-HYGIENE-FIX`.

**A pergunta, como teria ido ao líder:** o teste que guarda a compilabilidade do header público deve cobrir só o caso Linux (`<sys/sysmacros.h>`) ou também o Windows (`<windows.h>` sem `NOMINMAX`, que define `min`/`max` como macro)?

**Opções na mesa:** (a) só Linux, que é onde a colisão original foi observada; (b) as duas plataformas, já que a propriedade guardada é "o header público continua compilável após headers de sistema agressivos", e o Windows é um dos cinco alvos.

**Escolhida: (b).** A propriedade não é sobre `sysmacros`, é sobre header público de biblioteca distribuída — e o `<windows.h>` é o caso mais famoso de header de sistema que polui o espaço de nomes com macro.

**Porta de mão única:** não. **Custo de reverter:** baixo — apagar a perna `_WIN32` do teste.

**Risco declarado pelo próprio CTO:** se a perna Windows reprovar no CI, é **achado genuíno**, não instabilidade de teste.

#### D2 — imagem de container de teste sem registry externo  `[24/08/26 - 22:29:00]`

**Quem decidiu:** `fable` (CTO). **Fatia:** `TEST-WLCONT`.

**A pergunta, como teria ido ao líder:** a imagem de container com o compositor Wayland dentro é publicada num registry e puxada pelo CI, ou construída sob demanda com cache?

**Opções na mesa:** (a) publicar num registry externo; (b) construir sob demanda, com cache, a partir do `Dockerfile` versionado.

**Escolhida: (b).** Mantém a imagem reproduzível a partir do repositório, sem credencial nem dependência de serviço de terceiro, e faz valer a regra de que a mesma imagem roda local e no CI.

**Porta de mão única:** não. **Custo de reverter:** baixo.

#### D3 — `PKG-DIST` vai por ÚLTIMO, depois do `WL-PROTO`  `[24/08/26 - 22:29:00]`

**Quem decidiu:** `fable` (CTO). **Fatia:** `PKG-DIST`.

**A pergunta, como teria ido ao líder:** o arquivo `.pc` nasce antes ou depois de a biblioteca passar a referenciar o `libwayland-client`?

**Opções na mesa:** (a) antes, na ordem do plano original; (b) depois do `WL-PROTO`.

**Escolhida: (b), e o motivo é concreto:** depois do `WL-PROTO`, a biblioteca estática referencia `libwayland-client`, e um consumidor que use `pkg-config --static` só linka se o `.pc` declarar `Libs.private: -lwayland-client`. Nascendo antes, o campo nasceria **vazio** e seria emendado na fatia seguinte — e o teste de consumo passaria a exercitar uma promessa que o arquivo não cumpre.

**Porta de mão única:** não, a ordem em si. **Custo de reverter:** baixo. **Observação:** o conteúdo do `.pc` **é** porta de mão única, e é justamente por isso que a ordem importa.

---

#### D4 — teste de higiene: blindagem preventiva DECLARADA, mais portão mecânico  `[24/08/26 - 22:49:21]`

**Quem decidiu:** `fable` (CTO). **Fatia:** `HDR-HYGIENE-FIX`, após a **segunda reprovação** em revisão adversarial.

**A pergunta, como teria ido ao líder:** o projeto aceita um teste de higiene que é **blindagem preventiva sem alvo vivo hoje**, desde que isso esteja declarado no próprio arquivo — ou exige que ele tenha alvo real?

**O que a revisão provou:** o teste **morde** onde faz asserção (renomear campo quebra compilação; quebrar o valor falha em execução; símbolo colidente exercitado falha no link). Mas remover a ordem hostil de includes **não muda nada**, porque nada em `version.hpp` usa `major`/`minor` em forma de chamada — o mecanismo que dá nome ao teste está **inerte** no único header público de hoje. E o comentário-contrato do topo não era aplicado por script nenhum.

**Opções na mesa:**

- **(a) — ESCOLHIDA:** declarar no próprio arquivo que a ordem hostil é blindagem **preventiva**, mais um **portão mecânico** que falha se um header público novo não entrar no arquivo, com os três controles da casa (positivo, negativo e **varredura vazia**).
- **(b) — recusada:** fabricar uma colisão de propósito no header, para o teste ter alvo. Recusa do CTO, com a razão: **código de produto existindo para servir a teste** contraria a lei dos átomos e a de biblioteca.
- **(c) — RECUSADA POR ESTAR FORA DA CADEIRA DELE:** remover o arquivo e mover as asserções para o teste de versão. Isso **reverteria decisão expressa do líder** (o teste **é escrito**, guardando o header inteiro). **O modo autônomo não dá a caneta das decisões que o líder já tomou.** Se um dia (c) parecer certa, é pergunta para ele.

**Porquê, em duas linhas:** o líder já aceitou blindagem sem dente contra as duas macros quando recusou o teste estreito; (a) executa a decisão dele e fecha o único buraco que a revisão provou — um contrato de cobertura que era **texto**, sem nada que o obrigasse.

**Porta de mão única:** não. **Custo de reverter:** barato — apagar o portão e o parágrafo de declaração; o arquivo de teste permanece.

**Anexo, e é lei e não opção (L-27):** o relatório do implementador dizia que ele **não replicou** as asserções do teste de versão; o revisor comparou linha a linha e achou **três checagens copiadas byte a byte**, mais uma de string. As asserções **ficam** — o mutante de valor prova que são necessárias —, mas passam a estar **documentadas como duplicação deliberada**, e o commit do conserto nomeia a divergência entre relato e arquivo. **O relato que não bate não vira punição de agente: vira comentário no código que impede o próximo revisor de reabrir a mesma dúvida.**

---

**Nota de método:** a ordem `HDR-FIX` → `VER-4C` e os lotes **não** entram neste registro. São planejamento do passo 2 da L-34, mandato próprio do CTO, não cadeira do líder. Ficaram no plano, para a minha verificação.
