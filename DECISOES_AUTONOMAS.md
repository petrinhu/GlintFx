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

#### D5 — o `CLAUDE.md` passa a MANDAR MEDIR em vez de afirmar número  `[25/08/26 - 00:40:18]`

**Quem decidiu:** `fable` (CTO). **Fatia:** `DOC-ESTADO`, após eu reprová-lo na verificação.

**O que eu medi e que abriu a decisão:** o item existiu para consertar documentação que mentia sobre o estado do projeto, e **estava mentindo de novo em 24 horas** — dizia 56 commits contra 102 reais, 9 casos de teste contra 15, e apontava o remoto para um commit que já não era o topo. **O defeito não foi do agente**, que mediu tudo e declarou o comando de cada número: foi do **desenho** do item.

**A pergunta, como teria ido ao líder:** como o `CLAUDE.md` descreve o estado do projeto **sem apodrecer a cada onda**?

**Opções na mesa:**

- **(a) — recusada:** atualizar os números e aceitar que reapodrecem. A dívida volta na próxima onda, que neste projeto é amanhã.
- **(b) — ESCOLHIDA, com refinamento do CTO:** todo **contador volátil** sai do texto e vira **o comando que o mede**. E — este é o refinamento, e é melhor que a minha proposta — fica como **prosa datada** o **fato estrutural e lento**: por que o harness próprio existe, quais camadas existem, onde vivem os portões, o que cada decisão do líder congelou. **Esse é o contexto que uma sessão não re-deriva barato, e ele envelhece por MARCO, não por fatia.**
- **(c) — recusada:** retrato datado **mais** comandos. Mantém a esteira de manutenção que o incidente acabou de provar que falha em 24 horas, agora com **duas fontes para divergirem entre si**.

**Como o refinamento mata a minha própria ressalva:** eu havia argumentado que o leitor com pressa ficaria sem contexto. Não fica — **o que orienta uma sessão é o fato estrutural, e ele permanece**. O número volátil está a um comando de distância, e confiar num número gravado ontem **era exatamente o defeito**.

**Porta de mão única:** não. **Custo de reverter:** barato — recolocar números é um commit.

#### D5.1 — NÃO criar portão mecânico para isto  `[25/08/26 - 00:40:18]`

**Quem decidiu:** `fable` (CTO), com argumento que eu não tinha.

**Por que não:** com a opção (b), **o portão perde o objeto** — não sobra número no documento para conferir contra a realidade. A alternativa seria um portão **semântico**, decidindo se um dígito em prosa é contador volátil — e isso é **frágil por construção**: falso positivo em **toda data, versão e número de lei**, e falso negativo no contador escrito por extenso. **Portão que grita errado deixa de ser lido; é teatro de verificação.**

**O que fica no lugar, e são dois dentes baratos:** a própria seção **declara a regra de manutenção** no topo, de modo que o próximo editor tropeça nela antes de violar; e o revisor **enumera todos os dígitos** remanescentes, exigindo que cada um se justifique como data ou fato lento — **enumeração, não busca dirigida**, que é a técnica da casa.

**Porta de mão única:** não. **Custo de reverter:** criar o portão depois, se a regra declarada não bastar.

#### D6 — `PKG-DIST`: parar de PREVER a entrada e passar a VALIDAR a saída  `[25/08/26 - 01:46:42]`

**Quem decidiu:** `fable` (CTO), no lugar do líder, sob o modo autônomo da L-34. **Eu escalei o padrão em vez de mandar a quarta rodada de remendo.**

**O fato que motivou, e eu reproduzi cada linha:** três rodadas de revisão adversarial, **quatro formas** da mesma família — barra final, caminho absoluto, absoluto somado a prefixo divergente na instalação, e caminho vazio resolvendo para a **raiz do sistema**. Nas quatro, `pkg-config --exists` devolve **0**. Cada conserto foi seguido de uma forma nova que ninguém tinha imaginado.

**As opções que levei:** (A) continuar remendando; (B) validar a saída em vez de prever a entrada; (C) recusar o exótico no configure e documentar o suportado.

**Decisão: (B) como mecanismo primário, com (C) estreito, mais o cenário `DESTDIR` que faltava** — e a peça que alcança a máquina do empacotador vira **fatia própria fora da onda** (`PKG-VALIDATE`).

**O argumento que fecha a questão, e que eu não tinha:** o achado do prefixo divergente é **indecidível no configure** — o prefixo de instalação **ainda não existe** quando o cálculo roda. **Nenhuma rodada de previsão o alcança, jamais.** A família tem pelo menos um membro que a opção (A) nunca conseguiria pegar, o que a elimina por construção e não por preferência.

**O fato que ele mediu CONTRA a própria hipótese, e que eu reconferi:** ele ia afirmar que o Fedora passa o diretório de biblioteca **absoluto**, o que faria do caso absoluto o caminho do alvo primário. Foi ler `/usr/lib/rpm/macros.d/macros.cmake` e é **falso** — o Fedora **não passa esse diretório**, usa os legados e instala por **`DESTDIR`**. Ou seja: **o caminho que todo empacotador real usa é justamente o único que nenhuma das três rodadas testou.** Ele registrou o próprio quase-erro como o exemplo de por que essa pergunta se responde **medindo, não lembrando**.

**Onde ele me corrigiu:** eu enquadrei as rodadas 2 e 3 como gasto com hipótese de laboratório. **Não foram** — caminho absoluto é permitido e documentado pela convenção do CMake, e o código que o suporta **fica**. O gasto evitável era só a **quarta rodada de previsão**, e é essa que ele negou.

**O que a decisão abre mão, declarado e não escondido:** diretório vazio passa a ser **recusado**; absoluto somado a prefixo sobrescrito na instalação fica **não suportado, mas detectado**. E existe uma **janela**: até `PKG-VALIDATE` existir, um layout imprevisto produz arquivo errado em silêncio **na máquina de um empacotador fora do nosso CI**. Esse é o preço, aceito conscientemente, de fechar a onda W1 agora.

**Porta de mão única:** não. **Custo de reverter:** a assertion de saída é aditiva; o `FATAL_ERROR` do diretório vazio é a única coisa que restringe entrada, e sai num commit.

---

**Nota de método:** a ordem `HDR-FIX` → `VER-4C` e os lotes **não** entram neste registro. São planejamento do passo 2 da L-34, mandato próprio do CTO, não cadeira do líder. Ficaram no plano, para a minha verificação.

---

## Autorização de modo autônomo — 26/08/2026, 22h14

**Ordem do líder, verbatim:** *"quando acabar essa onda, siga em modo autonomo, push e inicie a onda seguinte e siga autonomo até o fim e push. Decisoes a criterio de um clevel. Vou dormir, até amanhã."*

**O que isto autoriza:**

1. Fechar a **W2** (revisão adversarial das 3 pendentes, correção do número no README, `preci.sh` e suíte verdes).
2. **Push da W2**, e conferência do CI **por `ls-remote` e `gh run`**, nunca pela mensagem do push.
3. Abrir e executar a **W3** inteira.
4. **Push da W3** ao fim.
5. **Decidir no lugar dele**, com um C-level assumindo a decisão e assinando — cada uma registrada aqui, ao vivo, para confirmação retroativa.

**O que isto NÃO relaxa** (as leis seguem inteiras):

- **L-12/L-18:** implementador, revisor e orquestrador continuam sendo agentes distintos. Revisão adversarial que **executa e muta**, nunca que lê.
- **L-20:** vermelho antes de verde, registrado com a saída de erro.
- **L-40:** piso de varredura não-vazia, contagem impressa.
- **CI vermelho BLOQUEIA.** Diagnosticar e consertar antes de seguir.
- **Relatório de agente não é prova.** O orquestrador re-verifica build, suíte e as alegações antes de aceitar.
- **Tag continua exigindo aval dele.** A autorização é de push, não de release.
- ⚠️ **Porta de mão única continua sendo dele.** Onde uma aparecer na W3, o C-level **não decide**: a fatia para, fica registrada aqui, e espera. Foi assim que a W2 tratou o tipo de cor e o carregador gráfico, e é a única leitura compatível com a L-10.

**Estado no momento da autorização:** W2 com 7 de 13 concluídos, 3 aguardando revisão, o carregador gráfico ainda escrevendo, 42 commits locais, último push em `4c35ddb`.

**A W3 tem 12 itens**, 6 deles bloqueados esperando pré-requisito. Os que abrem primeiro: relógio do núcleo, tipo de valor do `gfss`, contrato de nó, portas de arquitetura, carregamento de asset, análise de cor e o núcleo do seletor.

### Decisões tomadas em nome do líder nesta janela

*(cada uma com data, hora, o C-level que assinou, o que estava em jogo e o argumento — para confirmação retroativa)*

---

## Onda W3 — planejamento, decisões do CTO e o que fica parado  `[26/08/26 - 23:28:32]`

**Planejou:** `fable` (Caetano, CTO), read-only, a pedido do main. **Rascunho:** `/var/tmp/glintfx-plan/w3-plano.md`.

### Uma correção minha, antes das decisões dele

Eu levei ao CTO a afirmação de que **a W3 não tinha nenhum item de caminho principal**, e perguntei se era erro de composição. **A afirmação era falsa, e eu a reconferi contra a tabela depois de ele apontar:**

- `WL-DISPLAY` (W4) tem `ARCH-PORTS` (W3) como pré-requisito.
- `LOOP-RUN` (W6) tem `CORE-TIME` (W3) como pré-requisito.

Ou seja: **a W3 carrega dois elos do caminho principal**, e a composição está certa — é a cadeia esticada pelo conserto do CHK-07 (desvio 7 do `TODO.md`), que proíbe item de dividir onda com o próprio pré-requisito. **Nenhuma correção de composição é necessária**, e puxar `WL-DISPLAY` para a W3 teria reintroduzido justamente o defeito que o CHK-07 consertou.

O que prende o caminho principal nesta janela não é a onda: é o recorte da autorização, que deixa porta de mão única com o líder, caindo exatamente sobre esses dois elos.

### Classificação das cinco portas de mão única (achado do CTO, com citação)

| Item | Classificação | O que congela |
|---|---|---|
| `CORE-TIME` | **genuína** | representação pública de tempo/duração: value type visível, contrato de ABI (L-19) |
| `GFSS-VALUE` | **genuína, dupla** | layout do value type público (ABI) **e** a semântica do formato — número, comprimento e porcentagem são coisas distintas, e aqui quem perde é o arquivo do consumidor (terceira régua da L-26) |
| `ASSET-LOAD` | **genuína** | assinatura pública de carregamento e o erro da L-22 |
| `GFSS-NODE-VIEW` | **genuína, a de maior consequência** | é contrato que **o consumidor implementa**: exigência acrescentada depois quebra todo consumidor que já o implementou |
| `ARCH-PORTS` | ⚠️ **precaucionária** | o próprio item diz **"Não é ABI pública"**, e o que ele congelaria — concept C++23, um arquivo por plataforma escolhido pelo CMake, zero `#ifdef` em corpo de função — é **texto verbatim da L-19, que o líder já fechou em 21/08**. Adaptadores existentes hoje: **zero** |

**O CTO recusou destravar `ARCH-PORTS` por conta própria**, e a recusa está certa: a autorização nomeia porta de mão única pelo rótulo, e reclassificar para si mesmo o direito de executar é exatamente o conserto que favorece quem o propõe. Vira pergunta ao líder, não decisão de agente.

### Decisões que o CTO assinou nesta janela

- **D-W3-1 — a onda é curta, e fica curta.** A W3 desta janela são as duas fatias de `gfss`: análise de cor e núcleo do seletor. Recusadas: destravar `ARCH-PORTS` por reclassificação própria (fora da cadeira dele) e puxar item de onda futura (violaria o CHK-07 e a L-32). **Porta de mão única:** não. **Custo de reverter:** zero, é aditivo.
- **D-W3-2 — sequencial, cor antes de seletor.** As duas editam o mesmo `src/gfss/CMakeLists.txt`, e paralelo só vale em arquivos disjuntos; mais um build pesado por vez. **Reverter:** zero.
- **D-W3-3 — nenhuma linha nova em `include/glintfx/` nesta onda.** Header novo nasce interno em `src/gfss/`; promover a público é ato de fatia com revisão de API dedicada. **Diverge de propósito do `GFSS-TOKEN`, que já é público:** congelar forma pública agora anteciparia decisão que é do líder. **Porta de mão única:** não — é o que **evita** uma. **Reverter:** promover header é um commit; o inverso seria quebra.
- **D-W3-4 — `oklch()` sai como erro com diagnóstico** apontando a fatia própria, e a lista de funções aceitas é enumerada fechada. Recusado aceitar-e-ignorar: linha aceita em silêncio é o defeito que o líder mandou eliminar. **Reverter:** a fatia de `oklch()` substitui o erro, já planejada.
- **D-W3-5 — o "cerca de 148" se resolve medindo.** O implementador deriva a tabela de cores nomeadas da especificação pública e **pina o número exato no teste**, com a contagem impressa igual ao total. Nenhum número de memória entra na ordem de serviço.

### O que fica parado esperando o líder

Cinco perguntas, na ordem em que o CTO recomenda apresentá-las. **A primeira sozinha destrava a W4 inteira.**

1. **`ARCH-PORTS`** — está rotulado porta de mão única, mas o próprio item diz que não é ABI pública, e o que ele congelaria é o padrão que a L-19 já fixou. Ratifica executá-lo sem revisão prévia sua, mantendo revisão adversarial dedicada e nada em `include/`, ou mantém como porta sua?
2. **`GFSS-VALUE`** — ratifica o desenho do value type (palavra-chave, número, inteiro, comprimento com enum fechado de unidade, porcentagem preservada) e o **layout** dele?
3. **`GFSS-NODE-VIEW`** — a lista de obrigações do contrato está **final**? Acrescentar depois quebra todo consumidor que já o implementou.
4. **`CORE-TIME`** — qual representação pública de tempo e duração?
5. **`ASSET-LOAD`** — modelo de resolução de caminho e assinatura pública?

**Contra-argumento que o CTO fez questão de registrar:** encher a onda violaria a autorização ou o teto da L-32, e o maior valor desta janela não é código — é deixar os cinco dossiês prontos para o líder decidir em minutos quando voltar.

### D-W2-FECHO — três decisões do fecho da W2  `[26/08/26 - 23:55:05]`

**Quem decidiu:** o main, como orquestrador, nas três abaixo. Nenhuma é porta de mão única; as três são reversíveis num commit.

**1 — Atualizar o capítulo 1 do `AUDITORIAS.md` para reconhecer a EXCEÇÃO Nº 1.**

**A pergunta, como teria ido ao líder:** um manual normativo contradiz uma lei que o líder abriu depois dele. Conserta-se o manual em modo autônomo, ou espera?

**O fato:** o capítulo 1 lista como 🔴 CRÍTICO *"nenhum diretório `vendor/`, `third_party/` ou `external/` no repositório"*, com um comando de auditoria escrito ao lado. Rodei o comando: ele **acha** o `third_party/` legítimo. Quem seguisse o manual reportaria um crítico falso.

**Escolhida: consertar.** A EXCEÇÃO Nº 1 é do líder e é posterior ao manual; alinhar o manual à lei é escrituração, não decisão de critério. **A regra não foi apagada** — diretório vendorizado segue CRÍTICO por padrão, e o que entrou foi a exceção **nomeada e enumerada fechada**, mais o comando corrigido (rodado de verdade) e o apontamento de onde está a prova de integridade que torna a exceção auditável.

**O limite que respeitei:** o agente enumerou o capítulo 1 inteiro e recebeu ordem de **só reportar**, nunca consertar, o que fosse **mudança de critério** em vez de envelhecimento contra lei posterior. Critério de auditoria é decisão do líder.

**2 — Abrir `VENDOR-PURITY` como item, em vez de consertar agora.**

O revisor achou que nenhum portão da casa prova a frase do próprio `README.md` do vendor. **Julgamento dele, que eu aceito:** a separação do `check_spdx.sh` está **certa** pela L-17 — ele faz uma pergunta só, e misturar "tem cabeçalho" com "está no lugar certo" o tornaria monolito. O que falta é a segunda pergunta **ter dono**. Não bloqueia o push: exige item nosso, com cabeçalho correto, aterrissando por engano numa pasta de nome enganoso.

**3 — WSJF dos dois itens novos é estimativa minha, e está declarado como tal na descrição de cada um.** Não passou pela lente de produto. Se o líder quiser a pontuação de verdade, ela sai do `product-manager`, não de mim. **Custo de reverter:** trocar dois números.

**Nota de método, porque foi um portão que me pegou:** nomeei o segundo item `AUD-CAP9`, e o `todo_audit` **reprovou** — o prefixo `AUD-*` é convenção da casa para auditoria e exige que o item declare, no pré-requisito, o que ele cobre. O item não é auditoria, é manutenção de documento: **o nome é que estava errado**, não o pré-requisito ausente. Renomeado para `DOC-AUDCAP9`; auditoria limpa, 17 checks, zero achados.

#### D-W3-6 — o que o consumidor recebe quando o tokenizador detecta defeito NOSSO  `[27/08/26 - 00:29:36]`

**Quem decidiu:** `fable` (Caetano, CTO). **Fatia:** `GFSS-TOKEN`, achado CRÍTICO que reprovou `95c0f20`.

**O que a revisão mediu**, e é o fato de onde tudo parte: com o guard disparando em publicação, o consumidor recebia `kind=number`, `lexeme="-"` e **`diagnostic.expected` vazio** — e vazio significa, pela R4, "sem erro". O conserto anterior trocou uma trava por um fluxo de tokens fabricado e indistinguível de dado bom.

**A pergunta, como teria ido ao líder:** o que uma função pública devolve quando detecta violação de contrato **interno nosso**?

**Opções na mesa, e por que três caíram:**

- **(a) avanço forçado mais identificador novo no vocabulário** — a proposta do revisor. **Recusada, e o motivo não é o identificador: é continuar produzindo tokens.** O consumidor foi treinado, pela filosofia de recuperação que o próprio `token.hpp` documenta, a ler diagnóstico não-vazio como *"o SEU arquivo tem erro de sintaxe, siga em frente"*. Ele atribuiria a um arquivo **correto** dele um erro que é **nosso**, reportaria ao usuário final um erro de sintaxe inexistente, e seguiria consumindo fluxo fabricado. Troca esconder o defeito por esconder o defeito com uma bandeirinha.
- **(b) canal separado do vocabulário do formato** — **recusada, mas o argumento fica registrado**: erro interno nosso de fato não é da mesma natureza que erro de sintaxe do arquivo do consumidor. Caiu porque canal que ninguém checa por convenção é sinal que ninguém recebe, e porque seria superfície nova a congelar por um caminho que nunca deveria disparar. A tensão que (b) aponta entra **nominalmente** no checklist da revisão de API dedicada.
- **(d) reverter** — **recusada**: devolve a trava no processo de um consumidor que se comportou corretamente, que foi o incidente medido que motivou o conserto.

**Escolhida: variante de (c) — encerramento terminal do fluxo, SEM `kind` novo.** O `assert()` fica (depuração inalterada). Em publicação, o cursor é pinado no fim da fonte, o token vira o **`eof` que já existe** carregando diagnóstico populado com linha e coluna do ponto da violação e `expected = internal_tokenizer_defect`, lexema vazio. Daí em diante toda chamada é `eof` genuíno: **o fluxo termina estruturalmente, em qualquer forma de laço**, não só no canônico.

**O que isso compra:** a falha volta a ser **alta e honesta** — fim prematuro, sinalizado, e **atribuível a nós** — sem voltar a ser trava. O consumidor lê o identificador, **sabe que o arquivo dele não é o culpado**, e reporta rio acima. O dano fica capado em "folha não estilizada com causa nomeada", nunca em "folha estilizada errada em silêncio".

**O nome:** `internal_tokenizer_defect`, e não `internal_progress_guard`. O do revisor nomeia **o nosso mecanismo**, que não diz nada a quem lê; o escolhido nomeia **a falta e o culpado**. É o único identificador do vocabulário que não nomeia um construto esperado — nomeia a biblioteca, e é exatamente essa a informação acionável.

**Porta de mão única: NÃO, e o CTO pesou os dois lados em vez de só o que lhe convinha.** Contra: o tipo de diagnóstico e o vocabulário dele **explicitamente não congelam agora** — o próprio `token.hpp` reserva o congelamento à revisão de API dedicada, que é do líder; projeto pré-1.0, `SOVERSION` 0, sem consumidor externo conhecido. A favor, e ele registrou: **é política de erro em função pública, a mesma classe que no `CORE-ERROR` foi ao líder.** Por isso a decisão não passa em silêncio: entra aqui para confirmação retroativa **e** entra nominalmente no checklist da revisão de API, onde o líder ratifica ou derruba **antes** de virar contrato. **Custo de reverter:** um identificador, um ramo e os testes, num commit, antes de qualquer congelamento.

**Julgamento sobre a alegação de escopo do commit reprovado, que se autoclassificava como "mecânico":** o revisor tem razão e o commit estava errado. O teste é objetivo — **a mudança alterou o que um consumidor observa de uma função pública exportada?** Antes: trava. Depois: tokens fabricados sem sinal. Isso é contrato observável, território da L-22, que já tinha convenção documentada e foi contradita por omissão.

⚠️ **A regra que nasce disto, e que vale daqui em diante:** toda mudança no comportamento observável de função pública **sob qualquer condição, inclusive as "impossíveis"** (violação de contrato interno, precondição quebrada, exaustão de recurso) é **decisão de produto**. Em modo normal vai ao líder; em modo autônomo sobe ao C-level e entra neste registro. **Nunca é autoclassificada como "mecânica" pelo agente que a implementa.**

**O sintoma que virou regra prática, e vale citar como está:** *"o commit precisou de um parágrafo inteiro para argumentar que não era decisão de produto. Mudança que precisa defender que não é decisão de produto, é decisão de produto."*

**Varredura "isolado ou padrão?", feita antes de fechar:** três pontos na superfície inteira. Os guards de precondição do `gltfx_rslt` são **classe diferente** (erro do chamador, comportamento decidido pelo líder e provado por portão próprio) — conformes. O `skip_comments`, que consome comentário não terminado **sem diagnóstico**, é da mesma família "a informação existe e não se propaga", mas é erro de **entrada**, está declarado em comentário como escolha da fatia, e vira **item próprio** em vez de ser embrulhado aqui. O achado em si é **isolado, não padrão** — com a ressalva honesta de que a superfície é jovem.

#### D-PKGWIN — o `glintfx.pc` e o validador dele são artefatos Unix; no Windows nenhum dos dois existe  `[27/08/26 - 04:16:25]`

**Quem decidiu:** `fable` (Caetano, CTO). **Fatia:** `PKG-WIN-SCOPE`, nascida do incidente em que **duas rodadas de CI reprovaram uma instalação CORRETA no Windows**.

**A pergunta, como teria ido ao líder:** no Windows, o `pkg-config` deve ter poder de veto sobre a instalação do consumidor — e o `.pc` sequer deve ser escrito lá?

**Os fatos que decidiram, e eu reconferi os quatro contra a árvore:**

1. **`PACKAGING.md:173` já dizia** *"pkg-config has no role in glintfx's Windows story today"*. A promessa escrita ao empacotador **já excluía** o Windows.
2. ⚠️ **O próprio validador já declarava falso vermelho estrutural no Windows**, no cabeçalho dele: *"this file's library-artifact glob would not match glintfx.lib and would report a false failure - accepted for now"*. **Isso mata por construção a saída de "consertar a conversa com o pkg-config"** — mesmo com a chamada consertada, o glob reprovaria a instalação correta duas funções adiante.
3. **Zero tags, local e remoto.** Pré-1.0, sem consumidor conhecido.
4. **O `.pc` era instalado no Windows por acidente de implementação:** as duas chamadas do `CMakeLists.txt` não têm guarda de plataforma, e **nenhuma decisão registrada jamais disse que haveria `.pc` no Windows**.

**Mais um, que fecha o caso:** o `.pc` do Windows traz `Libs: -lglintfx`, nomeando um artefato **inexistente** naquela cadeia (lá é `glintfx.lib`, L-38) — e **nenhum portão nosso jamais o validou lá**. Arquivo distribuído que faz uma alegação nunca provada.

**As opções e por que três caíram:**

- **(a) o veto vira aviso no Windows** — recusada, e o argumento é forte: é o pior dos dois mundos, porque **continua publicando** o arquivo órfão e **deixa de vigiá-lo**, mais cria assimetria de contrato na mesma função pública.
- **(b) consertar a conversa com o `pkg-config` do Windows, após medir** — recusada pelo fato 2: **não tem linha de chegada**.
- **(d) só desregistrar o validador, mantendo o `.pc`** — recusada sem hesitar: arquivo distribuído que **nenhum portão olha, nunca**, é a classe exata de defeito que a L-40 existe para eliminar.

**Escolhida: (c) — não instalar o `.pc` nem registrar o validador fora do Unix.** A decisão **alinha o código à promessa que o `PACKAGING.md` já fazia**; não muda contrato, conserta a implementação que o contradizia. Onde o `.pc` existe (os quatro alvos Unix), **o veto continua integral, sem rebaixamento nenhum**.

**Isto NÃO é abandono do quinto alvo.** O empacotador de Windows não fica com proteção menor: fica com proteção **pela via que a plataforma dele de fato usa** — `find_package`, com dois portões dedicados rodando a cada push. O que ele perde não é proteção, é um artefato órfão que o descrevia na sintaxe de outro mundo.

**Porta de mão única: não**, e o CTO pesou contra si: remover artefato de instalação normalmente é quebra, mas não há tag, release nem consumidor conhecido, e a promessa escrita nunca o incluiu. **Reverter é aditivo, um commit.**

**É mudança de comportamento observável de instalação, ou seja, decisão de produto pela régua do `D-W3-6`.** Por isso está aqui para **ratificação retroativa**, e **a promoção do texto ao `ESCOPO.md` §9 ESPERA o líder** — aquele arquivo registra decisão dele, não de agente.

**O que NÃO foi medido, declarado:** a causa exata de o `pkg-config` do executor recusar o arquivo. Um **passo permanente de diagnóstico** entra no job do Windows **mesmo com esta decisão**, justamente para que a próxima falha de Windows seja **leitura de log em vez da quarta adivinhação** — duas rodadas de CI já foram queimadas adivinhando.

**A pergunta que vai ao líder:** *"No Windows o consumo é `find_package`; o `.pc` (convenção Unix) deixou de ser instalado lá, e o validador dele não roda lá — os quatro alvos Unix mantêm o veto integral. Ratifica, e promovo o texto ao `ESCOPO.md` §9? Ou reverte, e aí decidimos juntos o que fazer com o validador que reprova instalação boa?"*

---

## RESPOSTAS DO LÍDER — ratificação retroativa  `[27/08/26 - 05:59:14]`

**Ele acordou e pediu, verbatim:** *"Mostre decisoes autonomas que foram tomadas em formato askuserquestion. registre minhas respostas e pause depois."* As oito foram apresentadas por `AskUserQuestion`, sem painel lateral. **Três foram DERRUBADAS.** Registro abaixo, decisão por decisão.

| Decisão do agente | Resposta do líder |
|---|---|
| `D-PKGWIN` — o `.pc` sai do Windows | ❌ **REVERTA** |
| `D-W3-6` — fim de fluxo com o culpado nomeado | ⏸ **QUERO DISCUTIR** |
| `D-W3-3` — nada novo em `include/` nesta onda | ❌ **QUERO PÚBLICO JÁ** |
| Conserto do capítulo 1 do `AUDITORIAS.md` | ✅ **certo, é escrituração** |
| `D-W3-4` — `oklch()` sai como erro com diagnóstico | ✅ **ratifico** |
| WSJF estimado por mim nos nove itens novos | ✅ **aceito a estimativa** |
| `D-W3-1` — onda W3 curta, sem encher | ✅ **ratifico** |
| Duas listas de vocabulário de diagnóstico | 🔁 **CONSOLIDE EM UMA SÓ** |

### O que cada resposta obriga

**1. `D-PKGWIN` REVERTIDA.** O `glintfx.pc` **volta a ser instalado no Windows**, e o validador dele volta ao alcance. ⚠️ **Isso reabre o problema original**, e o líder sabe disso ao decidir: o validador reprova instalação **correta** de empacotador Windows, e o cabeçalho dele já declara que o glob nunca casaria `glintfx.lib`. **O que fazer com o validador é decisão a tomar COM ele**, não de agente — foi essa a opção que ele escolheu, com estas palavras: *"O .pc volta a ser instalado no Windows, e decidimos juntos o que fazer com o validador que reprova instalação boa."* **Consequência imediata: o CI do Windows volta a ficar vermelho até essa conversa acontecer.** Reverter os três commits sem decidir o validador reintroduz o vermelho conhecido; **a fatia para aqui e espera o líder.**

**2. `D-W3-6` EM DISCUSSÃO.** O desenho do que o consumidor recebe sob defeito interno **não está ratificado**. O código atual (fim de fluxo terminal com o marcador que nomeia a biblioteca como culpada) **fica em pé enquanto a conversa não acontece** — não se reverte para o defeito anterior, que devolvia texto plausível e falso em silêncio, porque esse era o CRÍTICO. **Mas o desenho é assunto aberto, e o líder quer discutir antes de virar contrato.**

**3. `D-W3-3` DERRUBADA — o analisador de cor vai para a API PÚBLICA agora.** Verbatim da opção que ele escolheu: *"Prefere que o analisador de cor entre na API pública agora, e aí decidimos o formato."* ⚠️ **Duas consequências que o próximo executor precisa ter na frente:** (a) isso **congela forma pública**, que é exatamente o que a decisão derrubada evitava — logo **o formato de retorno é decisão DELE, e a fatia não anda sem essa resposta**; (b) o analisador de cor está com **revisão REPROVADA** por um CRÍTICO (estouro de expoente vira preto em publicação e aborta em depuração) — **publicar antes de consertar seria publicar o defeito na API pública.** A ordem correta é: consertar o CRÍTICO, decidir o formato com ele, e só então promover.

**4. Escrituração de manual AUTORIZADA como padrão.** Alinhar manual normativo a uma lei posterior do líder **não é decisão, é manutenção**, e segue sem consulta prévia. ⚠️ **O limite continua valendo:** o que for **mudança de critério** de auditoria, e não envelhecimento contra lei posterior, continua sendo dele — foi assim que o agente foi instruído e é assim que fica.

**5, 6, 7 ratificadas sem alteração.** `oklch()` como erro com diagnóstico; a estimativa de WSJF dos nove itens serve como está; e a onda curta é a lei aplicada, com as dez travadas esperando.

**8. As duas listas de vocabulário CONSOLIDAM EM UMA SÓ**, com teste varrendo a união atrás de palavra repetida. A revisão tinha provado que as duas já produzem a **mesma palavra** a partir de símbolos diferentes, e que nada detecta isso.

### O que ficou FORA desta rodada, e continua esperando

As **cinco portas de mão única da W3** — `ARCH-PORTS`, `GFSS-VALUE`, `GFSS-NODE-VIEW`, `CORE-TIME`, `ASSET-LOAD` — não foram apresentadas aqui porque o pedido dele foi sobre **decisões já tomadas**, e essas nunca foram tomadas: pararam, por desenho. **`ARCH-PORTS` sozinha destrava a W4 inteira**, e o CTO a classificou como precaucionária.

---

## FALHA DE REGISTRO MINHA, apontada por revisão adversarial  `[27/08/26 - 08:52:43]`

**Um revisor classificou como CRÍTICO que a reversão do arquivo de empacotamento tivesse ido além do que o líder autorizou.** Ele leu, aqui neste arquivo, a resposta do líder — *"decidimos juntos o que fazer com o validador que reprova instalação boa"* — e a minha própria frase logo abaixo dela: *"a fatia PARA aqui e espera o líder."* Depois viu o commit chegar **2h27min mais tarde** com o desenho do validador **decidido e implementado**, e **nenhuma entrada neste arquivo entre uma coisa e outra**.

⚠️ **A conclusão dele estava certa PARA A EVIDÊNCIA QUE ELE TINHA. O que faltava era registro meu.**

**O que de fato aconteceu, e que eu não gravei:** depois daquela resposta, o líder mandou uma segunda mensagem, listando o que fazer com cada pendência por número. **Verbatim dele:**

> *"1- conserte*
> *2- faça*
> *3- askuserquestion*
> *4- askuserquestion*
> *5- askuserquestion*
> *6- askuserquestion"*

O item **2** era, na lista que eu tinha acabado de apresentar a ele, exatamente: *"Reverter o `.pc` no Windows — **e junto, o que fazer com o validador que reprova instalação boa**."* Ou seja: **o "faça" cobria o validador**, e o desenho passou a ser trabalho de agente por ordem dele.

**A falha, e ela é minha, não do agente que implementou:** eu executei a ordem e **não a gravei aqui**. Este arquivo é a única fonte de verdade sobre o que foi autorizado em modo autônomo — e ele ficou dizendo *"a fatia para e espera o líder"* enquanto a fatia andava. **Qualquer pessoa auditando o repositório chegaria à mesma conclusão do revisor.**

**A regra que fica:** ordem do líder que **destrava** algo registrado aqui como travado **entra aqui no instante em que ele a dá**, com o texto dele verbatim — do mesmo jeito que a ordem que trava. **Registro que só anota o "não" e esquece o "sim" mente por omissão**, e mente exatamente contra quem confia nele.

**Consequência prática, e o revisor tem razão nela também:** o `TODO.md` atribuiu o pacote inteiro a *"ordem do líder"* sem distinguir qual parte veio de qual ordem. Isso está corrigido junto com esta entrada.

---

## `ARCH-PORTS` LIBERADA pelo líder  `[27/08/26 - 09:42:59]`

**Decisão dele por `AskUserQuestion` em 27/08/2026, opção escolhida:** *"Libere e traga as outras 4"*.

**A fatia deixa de ser porta de mão única**, e as outras quatro devem ser apresentadas na sequência: relógio do núcleo, tipo de valor do formato de estilo, contrato do nó, e carregamento de arquivo.

**A recomendação que ele aceitou, e as razões, na ordem em que pesam para um produto distribuível:**

1. **Nada que o consumidor vê fica congelado.** As três coisas que quebram um consumidor de biblioteca são a forma de chamar, a compatibilidade do binário já compilado, e o formato dos arquivos de dado. **Esta fatia não toca nenhuma das três** — é o molde interno pelo qual cada sistema operacional se encaixa.
2. **Hoje existem zero encaixes, e esse número só cresce.** É o momento **mais barato da vida do projeto** para o molde estar errado: exatamente um sistema o exercita antes de existir um segundo. O contra-argumento — *"e se o molde estiver errado?"* — é real, **mas fica mais caro a cada mês, não mais barato**. Esperar não reduz o risco.
3. **Não fazer custa mais que fazer.** Uma biblioteca 2D que **não abre uma janela não tem consumidor nenhum a proteger**, e atrás desta fatia estão a janela, o teclado, o mouse, o desenho e a demonstração.

**A trava que permanece:** revisão adversarial dedicada, e **nada novo na parte pública** — assim a porta que de fato é de mão única continua fechada.

**Nota de método, porque o líder foi explícito:** ele pediu a recomendação *"não por facilidade, mas por ser o melhor num produto distribuível"*. As três razões acima são de produto, não de conveniência de execução — e a segunda delas, em particular, **argumenta contra esperar**, que seria o caminho mais confortável para mim.

---

## ONDA W3-B, sete decisões do CTO em modo autônomo  `[28/08/26 - 01:48:15]`

**Autorização do líder, verbatim:** *"termine a onda em modo autonomo. AO terminar, push. Depois, siga a onda seguinte, modo autonomo, mesmas recomendacoes. Duvidas a cargo do clevel não para resolver logo, mas para resolver com eficiência partindo da premissa que é um framework para distirbuicao."* Reconfirmada em seguida: *"siga autonomo até o push final da onda seguinte"*.

**Plano completo:** `/var/tmp/glintfx-plan/onda-seguinte.md` (386 linhas). As sete decisões abaixo são o resumo do §4; a razão longa e o custo de cada uma estão lá.

| | Decisão | Custo se o líder reverter |
|---|---|---|
| **D1** | A onda fecha a W3 inteira; nada da W4 entra. `WL-DISPLAY` é fatia de fronteira de SO com teste em container, a mais sensível a agente morrendo no meio, e merece palco limpo. | Zero. É ordenação, nada congela. |
| **D2** | `README-WIN` entra como **conserto de defeito**, não escopo novo: o congelamento da L-32 barra escopo NOVO, e isto é defeito em entregável já publicado. Documentação que ensina o primeiro comando quebrado é defeito de produto num framework distribuído. | Um commit de documentação. |
| **D3** | Segunda entrada de matriz do Windows **não entra**; vira item `CI-WIN-VSGEN` congelado até a demo. ⚠️ **Contraria a sugestão do orquestrador, com razão medida:** a máquina nova do servidor só tem o compilador de 2026, cujo gerador exige ferramenta **acima do nosso piso** declarado. O job provaria uma alegação mais fraca que a documentação honesta. A fatia C1 declara a lacuna ao consumidor em vez de escondê-la. | Barato. O líder manda criar o job quando quiser. |
| **D4** | `GATE-DEBUG` ganha estágio de depuração real no portão local mais um trabalho no alvo primário, em vez de mecanismo alternativo. Exercita as asserções reais em vez de reencená-las. | Barato: apagar um trabalho e um estágio. |
| **D5** | Três itens da W3 adiados: balanço de macro de cabeçalho, varredura de ambiente e mistura de caminhos no empacotamento. Razões individuais no §5 do plano. | Zero. |
| **D6** | `GFSS-VOCAB-PROD` fica onde está: o prazo declarado dele é o congelamento da interface pública, longe, e a fatia B4 desta onda ataca a mesma família pelo lado mecânico. | Zero. |
| **D7** | A colisão de palavra de diagnóstico segue para a revisão de API dedicada, como já estava decidido, **com trava anti-esquecimento**: a ordem de serviço de B1 proíbe o revisor de congelá-la em silêncio. | Zero. |

**O que estas decisões NÃO relaxam:** implementador, revisor e orquestrador continuam sendo agentes distintos; a revisão executa e muta o código; o orquestrador reverifica antes de aceitar; e servidor vermelho bloqueia.

**Nada mais espera o líder aqui.** A nota anterior desta linha dizia que a ratificação retroativa de `D-PKGWIN` seguia pendente; ela estava **vencida quando foi escrita**. O líder já havia decidido, na tabela acima (`D-PKGWIN` — o `.pc` sai do Windows | ❌ **REVERTA**), e a reversão foi executada em `fda17c0` (`fix(cmake): PKG-WIN-SCOPE -- reverte decisao de agente por ordem do lider, .pc volta ao Windows`), que devolveu o `glintfx.pc` ao Windows e trouxe o validador junto. Corrigido em 31/08/2026 pelo orquestrador, escrituração própria.

### D8 — Regra da casa para conversão numérica: **função de matemática é TOTAL**  `[28/08/26 - 02:44:39]`

**Decisão do CTO em modo autônomo**, disparada por dois críticos de comportamento indefinido que a revisão adversarial de `CORE-TIME` reproduziu contra o binário real. Texto completo em `/var/tmp/glintfx-plan/decisao-core-time.md`.

**A regra, que vale para todo o projeto e não só para esta fatia:**

> Função pura de matemática ou conversão é **total**: determinística, saturante (direção preservada; não-número vai a zero), **nunca comportamento indefinido, nunca falível**. O diagnóstico de entrada inválida é trabalho da **fronteira de ingestão** (leitor, carregador), que é quem devolve pelo canal de erro.

**Como isso se aplica aos dois defeitos:**

| | Decisão |
|---|---|
| Diferença entre instantes estourando | Subtração em tipo sem sinal e conversão de volta. Comportamento definido por norma nos dois passos, para **qualquer** par. A garantia documentada não muda. |
| Conversão a partir de segundos devolvendo lixo | **Saturante total**, assinatura intacta, sem canal de erro. Fora de faixa e infinito saturam **na direção do sinal**; não-número vira **zero, documentado como contrato**. |

⚠️ **O detalhe que faz a decisão funcionar:** a verificação de faixa é feita **por comparação, ANTES** de qualquer arredondamento. Não-número reprova toda comparação e cai no ramo do zero, então **a biblioteca matemática do sistema nunca recebe entrada inválida**. Isso mata, por construção, o agravante de que aquela biblioteca **não é instrumentada e fica invisível ao sanitizer** — problema que continuaria existindo mesmo depois de `GATE-ASAN-HALT`.

**O que o CTO RECUSOU, e por quê:** canal de erro na conversão (criaria duas regras para a mesma classe, já que a cor limita sem canal de erro); precondição verificada (em modo de produção continua sendo comportamento indefinido, e **hoje nenhum portão desta casa a exercitaria**); valor-sentinela no retorno (roubaria um valor legítimo do domínio).

**Custo se o líder reverter:** trocar saturante por falível é **quebra de assinatura**, ou seja o componente de maior peso da regra de versão. **Grátis antes da 1.0**, caro depois. O mesmo vale para trocar o mapeamento do não-número, que é contrato documentado.

**Nada volta ao líder:** a assinatura pública não muda, e a regra é **generalização de um precedente que ele já aprovou** na fatia de cor. Registrado para confirmação retroativa.

---

## Sessão autônoma de 31/08/2026 — fechamento da onda de dependência zero  `[31/08/26 - 23:31:37]`

**Autorização do líder, verbatim:** *"modo autonomo. termine essa onda. pode depois seguir as próximas, só indo para a proxima apos tudo verde"*.

**O que a autorização cobre, pela L-15:** ondas sem parar a cada passo; `push` ao fim de onda; decisões que iriam a `AskUserQuestion` registradas aqui como decisão autônoma para confirmação retroativa. **O que ela não relaxa:** implementador, revisor e orquestrador continuam sendo agentes distintos; a revisão executa e muta o código; o orquestrador reverifica antes de aceitar; e servidor vermelho bloqueia. A própria ordem dele põe o portão explícito: *"só indo para a proxima apos tudo verde"*.

**Estado medido na abertura, não lembrado:** ramo `depzero-gate` em `da5a85e`, idêntico ao remoto; execução 33256549290 do servidor **verde nos 18 trabalhos** nesse mesmo identificador; `dep_zero_trace` e `dep_zero_trace_selftest` (24 controles) executados nas cinco plataformas, conferidos nos registros dos trabalhos de Fedora e de Windows. Isso satisfaz o pré-requisito declarado de `DEPZERO-SHALLOW` — o oráculo profundo já mordeu no ambiente real.

**Escrituração corrigida na abertura:** a nota vencida sobre `D-PKGWIN` (o líder já havia decidido; a reversão saiu em `fda17c0`).

### A última fatia da onda de dependência zero foi aberta em modo autônomo  `[31/08/26 - 23:41:03]`

**Fatia:** `DEPZERO-SHALLOW` — rebaixar o interpretador de texto a rede rasa declarada, agora que o oráculo profundo (`dep_zero_trace`) existe e já mordeu no servidor real.

**Pré-requisito conferido antes de abrir, não presumido:** execução 33256549290 verde nos 18 trabalhos em `da5a85e`, com `dep_zero_trace` e `dep_zero_trace_selftest` (24 controles) executados nas cinco plataformas — li os registros dos trabalhos de Fedora e de Windows, não a cor do painel.

**Papéis, como a lei manda:** o CTO planejou, um agente especialista implementa, um revisor adversarial independente executa e muta o código depois, e eu reverifico antes de aceitar. Nenhum dos três é o outro.

**A decisão do líder que rege o contrato desta fatia já estava tomada** (28/08/2026, verbatim: *"Deixa passar avisando que o servidor decide"*): forma ambígua passa o gancho com aviso impresso, e o servidor decide. Nada aqui é decisão nova minha.

**Nuance declarada pelo CTO, que eu confirmei na árvore:** o contrato de aviso também vale no modo de árvore, onde produz **2 avisos permanentes hoje** — as duas chamadas multi-linha reais em `cmake/GlintfxWaylandProtocols.cmake` e `cmake/GlintfxPkgConfigValidateInstalled.cmake.in`. É inócuo porque o oráculo profundo roda na mesma suíte e é a autoridade declarada, mas fica registrado para o líder poder discordar.
