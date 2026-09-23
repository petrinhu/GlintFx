# Plano da onda W7-C (drenagem): portões que afirmam medir e não medem

**Autor:** C-level `opus`, esforço alto, sentado na cadeira do líder em modo autônomo (L-34 emendada em 22/09/2026). **Data real:** 23/09/26 - 06:37:43 (`date`). **Árvore conferida:** `da1e325` em `main`, com `TODO.md` e `tests/tools/check_layers.py` modificados por outro implementador (não tocados aqui). **Única escrita deste agente:** este arquivo, mais as medições brutas em `/var/tmp/glintfx-plan/w7c-medidas/`.

**Leis aplicadas e como:** L-43/L-22 (pesquisa feita ANTES do desenho, seção 2, com URL); L-29 (fontes lidas para aprender; licença conferida: SDL e GLFW Zlib, wlroots MIT, KWin GPL, Doorstop LGPL-3.0; nada ligado, clonado ou copiado); L-18/L-27 (seção 1 separa FATO, com o comando que o mediu, de INFERÊNCIA); L-32 refinada em 22/09 (vence o mais completo, medido pela dor da comunidade; o que a completude exige vira fatia; o congelamento de escopo de 27/08 continua valendo porque `DEMO-1` está `⏳ Pendente`); L-36/L-40 (cada sub-fatia nasce com estreia vermelha e piso de varredura não-vazia); L-17 (gêmeos enumerados, não buscados); L-24 (a onda fecha pela definição da seção 4); L-11 emendada em 22/09 (ramo próprio, merge e marca ao fim verde); L-26 (número da marca); L-09 (tudo que executa roda em container); L-14/L-51 e L-07 (o que isso me proíbe decidir está na seção 6).

---

## 1. Fatos medidos (L-18/L-27)

Os números abaixo valem para a árvore `da1e325` e para o último CI verde (run `35808601262`, ramo `onda-w7b`). Todo número se remede na execução; aqui fica o comando.

| # | Fato | Comando que mediu | Resultado |
|---|---|---|---|
| F1 | A sequência da onda na tabela é a mesma do pedido | `grep -nE 'W7-C' TODO.md` | linhas 530, 531, 532, 615, 621, 622, 625, 626, 627, na ordem `WL-ACK-SMOKE-BLUNT`, `PLAN-SCOPE-REGEX-BLIND`, `LINK-PREFIX-SUBSTRING`, `PARITY-ALIAS-HYGIENE`, `DOCS-COUNT-VOCAB`, `CLAIM-CITATIONS`, `PLAN-SCOPE-COLUMNS`, `DISPLAY-PASSKEY-CONTROL`, `CI-VERDE-W7C` |
| F2 | Todas as linhas da onda têm 12 pedaços, sem barra crua | contagem de `\|` por linha, descontando `\\|` | 12 em todas as sete abertas |
| F3 | `PLAN-SCOPE-REGEX-BLIND` e `LINK-PREFIX-SUBSTRING` estão entregues e verdes no servidor | `git log --grep`, `git branch -r --contains` | `0e1a6c8` e `13398e2` estão em `origin/main`; `92d85a2` registra "servidor verde nos 24". **A descrição das duas linhas ainda diz "aguardando push/CI real": texto velho, status certo** |
| F4 | O KWin marca a configuração como aceita ao ENVIAR, e não confere o número de série da confirmação | leitura de `kwin/src/wayland/xdgshell.cpp` (ramo `master`) | `isConfigured = true` logo depois de `send_configure(serial)`; `ack_configure` só guarda o número, sem validar |
| F5 | O wlroots marca como configurada só ao RECEBER a confirmação, e recusa número de série desconhecido | leitura de `types/xdg_shell/wlr_xdg_surface.c` | `configured = true` dentro do tratador de `ack_configure`; número desconhecido dispara erro `invalid_surface_state` ("wrong configure serial") |
| F6 | Existem DOIS sítios de confirmação no produto, não um | `grep -n xdg_surface_ack_configure src/` | `window_adapter.cpp:294` (primeira configuração) e `:303` (`ack_pending_configure`, configurações seguintes). O item cita só o primeiro |
| F7 | A imagem final do container NÃO lista `python3` | `sed -n 948,980p tests/container/Containerfile` | a lista do `dnf` não tem `python3`; se ele vem na base `fedora:44` **não foi medido** |
| F8 | A tabela de apelidos tem 30 linhas; nenhuma está morta; TRÊS rodam dos dois lados | inventários baixados do run `35808601262` (`gh run download -p 'parity-inv-*'`), cruzados linha a linha (`w7c-medidas/linux.txt` 246 nomes, `windows.txt` 225) | vivas 30/30; bilaterais: `dep_zero_binary_test`, `dep_zero_binary_selftest` **e `frame_callback_sequence_test`** (este terceiro NÃO está no texto do item) |
| F9 | `grep -c 'nunca casaram' tests/tools/check_test_parity.py` | o comando | 0: a lacuna (a) do item continua aberta |
| F10 | O arquivo de exceções de paridade tem 28 registros, todos vivos | mesmo cruzamento de F8 | 28/28 casam uma lacuna real hoje; **o portão não confere isso** (gêmeo do apelido morto, L-17) |
| F11 | `check_readme_test_count.py` não existe mais | `git log --diff-filter=D` | apagado em `fccf3f8` (06/09); a motivação do `DOCS-COUNT-VOCAB` mudou para "nenhum outro documento cita contagem" (`docs/plano-conserto-vermelhos-w6b.md:48`) |
| F12 | Não existe portão de vocabulário | `ls tests/tools \| grep -i vocab` | vazio |
| F13 | O portão de travessão já enumera o texto público, e essa enumeração cobre 57 arquivos `.md`/`.txt` | `check_dash_pubdoc.py` (`EXEMPT_EXACT`, `EXEMPT_DOCS_PREFIXES`), reproduzido com `git ls-files` | 57; inclui `docs/lente-*.md` (internos, em português) e os `.txt` de `tests/` |
| F14 | Frases com forma de contagem no texto público hoje: quatro | `grep -noiE` do vocabulário candidato (seção 3.D) | `CHANGELOG.md:160` ("21 of 21", seção lançada 0.2.0.0), `CHANGELOG.md:170` ("70 cases", mesma seção), `docs/gl-loop-portability-matrix.md:38` ("23 of 23", **com run `34975391524` e SHA `14427bc` citados**), `tests/parity_exceptions.txt:528` ("18 fixtures") |
| F15 | Vocabulário de alegação nos cabeçalhos públicos | `grep -rnoiE` sobre `include/` (33 arquivos rastreados) | `never` 378, `measured` 34, `always` 26, `identical` 16, `proved` 15, `proven` 9, `every platform` 4, `identically` 3, `all five` 2; 24 linhas com forma de "igual entre sistemas" |
| F16 | A promessa falsa de 06/09 (mensagem de redimensionamento na criação) não está mais em `include/` | `grep -rniE 'WM_SIZE\|resize' include/` | vazio: ou foi consertada, ou morava fora de `include/`. **Reenumerar, nunca herdar o "seis"** |
| F17 | A citação viva "the two intruders" continua em `cmake/glintfx.version:87` | leitura | presente |
| F18 | O portão de escopo de plano: uma coluna só, posição fixa, identificador por regex, ausências opcionais, ninguém o roda | `check_plan_scope_diff.py:110` (`\d+[a-z]*`), `real_main` (`columns[4]` fixo; `--absences`/`--todo` com `default=None`); `grep -c check_plan_scope_diff` em `ci.yml` e `preci.sh` | 0 e 0; em `tests/CMakeLists.txt` só aparece em comentário; `tests/tools/selftest_orphan_exceptions.txt` reserva o órfão para `PLAN-SCOPE-COLUMNS` |
| F19 | As tabelas de fatia dos planos versionados têm formas diferentes | cabeçalhos de `docs/plano-*.md` | `plano-w6a`: `Nasce / muda` é a 4ª coluna; `plano-w6b-placa-e-laco` insere "Teste vermelho de estreia" antes das provas; `plano-w6-folha` tem uma coluna "Prova" só; `plano-fecho-w7b` usa `Sub-fatia`/`Fechamento`/`Estreia vermelha` |
| F20 | Identificadores de fatia reais | extração da 1ª célula das tabelas `# \| Fatia` e `# \| Sub-fatia` | `1..14`, `2a 2b 5b 5c`, `3b`, **`L-1 V-5a C-1 H-0 X-1` e `~~V-6b~~` (riscado)**: o regex atual `\d+[a-z]*` é cego ao plano INTEIRO do `fecho-w7b`, não só a `5C` maiúsculo |
| F21 | Há QUATRO chaves internas de acesso, não uma | `grep -rnE 'friend struct' include/` | `display_internal_access`, `window_internal_access`, `gl_context_internal_access`, `loop_internal_access`, todas com a definição completa no cabeçalho público |
| F22 | Testes caixa-branca usam as chaves | `grep -rn internal_access tests/` | `tests/container/gpu_kind_report_smoke.cpp`, `tests/container/loop_hidden_test.cpp`, `tests/loop_hidden_test.cpp`, `tests/win32_facade_pin_test.cpp`, `tests/container/facade_pin_smoke.cpp` |
| F23 | A prova negativa da chave de display vive fora da árvore | comentário em `src/platform/window/window_facade.cpp:48-60` | sonda descartável em `tests/container/_arch_ports_src/`; nenhum controle permanente |
| F24 | O último CI tem 25 trabalhos: 24 verdes, 1 pulado por desenho | `gh run view 35808601262 --json jobs` | pulado: "Versao == etiqueta (VERSION-TAG-SYNC)", que só roda em marca |
| F25 | Última marca | `git for-each-ref --sort=-creatordate refs/tags` | `v0.5.0.0` |
| F26 | Item da INBOX já resolvido | `docs/gl-loop-portability-matrix.md:17` | "Both of those have since landed": a entrada da INBOX sobre o documento que negava os adaptadores gráficos está **resolvida na árvore** e continua listada |
| F27 | `GFSS-VALUE-LAYOUT-ASSERT` (W10) está meio entregue | `include/glintfx/gfss/value.hpp:427-440` | existe `static_assert(std::is_trivially_copyable_v<...>)`; faltam `sizeof`/`alignof`/`is_standard_layout`, que o próprio item pede |
| F28 | `xdg_wm_base.ping` já é respondido e testado | `src/platform/wayland/shell_adapter.cpp:29`, `tests/container/shell_smoke.cpp` | sim; fora desta onda |

**Inferências, marcadas como tal:** (I1) a lacuna do KWin (F4) explica, sem precisar de outra hipótese, por que o `window_smoke` passa com a confirmação removida: é construção do compositor, não defeito da fixture. (I2) O terceiro apelido bilateral (F8) provavelmente legitima um teste de integração do Windows com um teste unitário puro que roda nos dois lados; confirmar lendo os dois é trabalho da sub-fatia C2.

---

## 2. Pesquisa, por item (L-43, L-34 emenda de 09/09: manual oficial, dor da comunidade, web, bibliotecas semelhantes)

### 2.A `WL-ACK-SMOKE-BLUNT`

- **Manual oficial.** xdg-shell: `xdg_surface.error.unconfigured_buffer = 3` ("attaching a buffer to an unconfigured surface"), `invalid_serial = 4`; `ack_configure` "consome o número enviado e todos os anteriores desta superfície", ou seja, confirmar só o último de vários é legal. <https://wayland.app/protocols/xdg-shell>
- **Os compositores divergem, e é aí que mora a dor.** KWin marca configurado no envio (F4). wlroots (sway, wayfire, labwc, cage) só na confirmação, e recusa número desconhecido (F5). <https://invent.kde.org/plasma/kwin/-/raw/master/src/wayland/xdgshell.cpp>, <https://raw.githubusercontent.com/swaywm/wlroots/master/types/xdg_shell/wlr_xdg_surface.c>
- **Dor da comunidade, medida em projetos grandes:** o Zed morria no Wayfire com `xdg_surface@26: error 3: xdg_surface has never been configured` porque comitava antes de confirmar <https://github.com/zed-industries/zed/issues/10976>; o SDL comitava buffer do tamanho velho depois de confirmar um redimensionamento, e o Weston matava a conexão <https://github.com/libsdl-org/SDL/issues/4563>; o Chromium precisou garantir "sequência de configuração confirmada antes de mapear" <https://groups.google.com/a/chromium.org/g/ozone-reviews/c/u2Wl_Oepxxo>; o VLC teve o mesmo conserto <https://vlc-devel.videolan.narkive.com/jJ3HYZJ0/patch-xdg-shell-needs-to-be-fully-configured>. **Padrão:** o programa funciona no KWin/GNOME e morre nos compositores estritos. É exatamente o nosso ambiente de teste: só KWin.
- **Bibliotecas semelhantes (para aprender):** SDL3 (`SDL_waylandwindow.c`, Zlib) confirma na hora quando não está redimensionando, e durante redimensionamento interativo adia e confirma SÓ a mais recente no próximo aviso de quadro, para não atrasar; GLFW (`wl_window.c`, Zlib) confirma sempre dentro do tratador. **Lição:** as duas estratégias são legais; o oráculo não pode exigir "uma confirmação por configuração", tem de aceitar "confirmar a última".
- **Técnica de retransmissor (relé) no fio:** o protocolo é "parcialmente autodescritivo", dá para analisar só as mensagens de interesse e repassar o resto; o descritor passado por `SCM_RIGHTS` só se associa à mensagem analisando-a; mensagens ficam abaixo de 4096 bytes; e rastrear identidade de objeto exige cuidado com criação e reuso <https://mstoeckl.com/notes/gsoc/blog.html>, <https://github.com/neonkore/waypipe>. Armadilha do núcleo: excesso de descritores em trânsito dá `ETOOMANYREFS` <https://github.com/feschber/lan-mouse/pull/491>.
- **Alternativas conferidas e descartadas (seção 5, D1):** `WAYLAND_DEBUG` (prova só o que o cliente ENVIOU; nos compositores a saída do lado servidor mistura conexões sem identificar qual <https://github.com/wmww/wayland-debug/issues/5>); servidor falso sobre `libwayland-server` (fronteira da L-07); segundo compositor estrito no container (pacote novo, L-14).
- **RmlUi:** não tem backend de protocolo próprio (delega janela a SDL/GLFW); procurado, nada a aprender aqui. Declarado, não silenciado.

### 2.B `LINK-PREFIX-RESIDUOS` (linha nova, seção 7)

- Expressões regulares do Python: olhar-para-trás de largura fixa, classe negada para "não é continuação de caminho" <https://docs.python.org/3/library/re.html>. O resto da pesquisa é interno: as duas entradas da INBOX (`ANCORA-DE-BUILD-CEGA-A-IGUAL-E-ASPAS`, `SELFTEST-MULTI-NAO-DISTINGUE-CAUSA-DA-FALHA`) já trazem mecanismo e conserto medidos.

### 2.C `PARITY-ALIAS-HYGIENE`

- **A comunidade já apanhou deste defeito e deu nome a ele: supressão velha.** O ESLint passou a avisar por padrão, na v9, sobre diretiva de desligar regra que não desliga mais nada <https://eslint.org/blog/2024/04/eslint-v9.0.0-released/>, e há pedido aberto para que seja ERRO, porque aviso é ignorado <https://github.com/eslint/eslint/issues/18665>. O Rust criou `#[expect]` (RFC 2383) exatamente porque `#[allow]` apodrecia calado; expectativa não cumprida vira diagnóstico `unfulfilled_lint_expectations` <https://github.com/rust-lang/rust/issues/85549>, <https://doc.rust-lang.org/rustc/lints/levels.html>. **Lição:** o apelido morto é a mesma doença, e o remédio maduro é reprovar, não avisar.

### 2.D `DOCS-COUNT-VOCAB`

- Linter de prosa por vocabulário fechado: o Vale (regra `existence`, fichas com limite de palavra, `raw` para regex, `exceptions`) <https://docs.vale.sh/checks/existence>. Lição: vocabulário nomeado, uma ficha por forma, exceção explícita.
- Histórico datado é imutável por convenção: seções lançadas do `CHANGELOG` não se editam, só a `[Unreleased]` é viva <https://keepachangelog.com/en/1.1.0/>. Lição: "21 of 21 jobs" dentro da seção 0.2.0.0 é fato datado, não alegação viva.

### 2.E `CLAIM-CITATIONS`

- Alegação presa a teste: `doctest` do Python e testes de documentação do Rust mantêm a prosa honesta executando-a <https://docs.python.org/3/library/doctest.html>, <https://doc.rust-lang.org/rustdoc/write-documentation/documentation-tests.html>. A convenção da casa já existe: "Proved by: `arquivo` (ctest `nome`)" em `docs/api-conventions.md:5`.

### 2.F `PLAN-SCOPE-COLUMNS`

- Rastreabilidade requisito-teste com validação de ligação: Doorstop (LGPL-3.0) valida que cada item liga a um teste existente e marca ligação suspeita <https://github.com/doorstop-dev/doorstop>. Lição: a ligação se confere contra o inventário real, nunca contra a prosa.

### 2.G `DISPLAY-PASSKEY-CONTROL`

- Controle de acesso em C++ protege contra Murphy (engano), não contra Maquiavel (fraude deliberada) — Herb Sutter, GotW #76 <http://www.gotw.ca/gotw/076.htm>. Lição: o objetivo honesto é "o consumidor não alcança por acidente em NENHUM dos dois modos", não "é impossível".
- Teste de compilação que TEM de falhar: `PASS_REGULAR_EXPRESSION` **ignora o código de saída** <https://cmake.org/cmake/help/latest/prop_test/PASS_REGULAR_EXPRESSION.html>, e `WILL_FAIL` aceita QUALQUER falha <https://cmake.org/cmake/help/latest/prop_test/WILL_FAIL.html>; o padrão maduro casa o diagnóstico esperado <https://ibob.bg/blog/2022/10/04/testing-build-failure-with-cmake/>. Lição: exigir as duas coisas (saída diferente de zero E diagnóstico certo) mais um gêmeo positivo, senão é a família "reprovou pelo motivo errado".

---

## 3. Por item: estado, desenho, sub-fatias, fechamento, estreia vermelha

As tabelas de sub-fatia usam de propósito o esquema que o portão de `PLAN-SCOPE-COLUMNS` vai reconhecer (cabeçalho `# | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão`), para este plano ser conferível por máquina quando for versionado em `docs/plano-w7c.md` (D20). Nome entre crases na coluna de prova é nome de teste; "—" é ausência declarada, com motivo na própria célula.

### 3.A `WL-ACK-SMOKE-BLUNT` (linha 530)

**Estado medido:** aberto. F4, F5 e F6. O `window_smoke` continua afirmando no cabeçalho uma captura que não acontece; comentários de produto repetem a mesma alegação (`window_adapter.hpp:100-130`, `window_adapter.cpp:285-291`, `egl_context_adapter.cpp:785,833`); a exceção de paridade do `window_smoke` aponta para este item (`tests/parity_exceptions.txt:291`).

**Desenho (D1-D5):** um **relé estrito** escrito em casa, dentro do container, entre o programa sob teste e o KWin. Ele repassa os bytes e os descritores sem alterá-los, e em paralelo lê o fio e aplica as regras que os compositores estritos aplicam. Quando uma regra quebra, ele faz o que o wlroots faria: entrega ao cliente o erro de protocolo com o objeto e o código certos, e fecha. Assim o caminho de erro do produto (a conexão morta que chega ao consumidor com o nome `xdg_surface`) é exercido de verdade, pelos dois caminhos (memória compartilhada e gráfico), sem instalar compositor nenhum.

Regras, lista fechada, cada uma com a fonte:
- **R1 (protocolo):** comit com buffer numa superfície de papel `xdg` antes de qualquer confirmação → `xdg_surface.error 3 unconfigured_buffer` (xdg-shell; comportamento do wlroots).
- **R2 (protocolo):** confirmação com número nunca enviado, ou já consumido por uma confirmação posterior → erro de número inválido (xdg-shell `invalid_serial`; o wlroots usa `xdg_wm_base.invalid_surface_state`; o relé usa o do xdg-shell e o cabeçalho dele diz por quê). Confirmar só o último de vários é **aceito** (lição do SDL3).
- **R3 (regra da CASA, não do protocolo, e declarada assim no cabeçalho e na saída):** depois de uma configuração injetada pelo relé, comit com buffer sem confirmar essa configuração ou uma posterior = "estado nunca aplicado". Existe porque os compositores estritos só aplicam o estado na confirmação; sem ela a janela fica presa no tamanho velho (a dor do SDL #4563).

Arquitetura interna (L-17, L-19, átomos com nome próprio): transporte (laço de repasse, uma conexão a montante por cliente, fim de arquivo propagado nos dois sentidos, controle de descritores dimensionado para o teto do núcleo e `MSG_CTRUNC` reprovando alto); decodificador do fio (cabeçalho de 8 bytes, mensagem partida entre leituras, teto de 4096); tabela de objetos (só as interfaces que importam: `wl_display`, `wl_registry` com o nome da interface lido do `bind`, `wl_compositor`, `wl_surface`, `xdg_wm_base`, `xdg_surface`, `xdg_toplevel`, `wl_shm`, `wl_shm_pool`, `wl_buffer`, `wl_callback`; `delete_id` aposenta o número; objeto desconhecido é repassado, contado e nunca analisado); motor de regras; injetor (erro e configuração sintética). A separação transporte/regras deixa a porta pronta para os modos de falha da linha futura `WL-RELAY-FAULTS` (D21) **sem construí-los agora**.

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| A1 | Transporte e decodificador do relé, modo transparente (sem regra) | Wayland | `tests/container/wire_relay/` (novo; um arquivo por átomo), `tests/container/Containerfile` (compila no estágio `arch-ports-builder`, sem pacote novo), `tests/container/prepare_arch_ports_fixture.sh` | Autoteste com par de soquetes e montante falso: mensagem partida em duas leituras, descritor que chega com o pedaço certo, controle truncado, interface desconhecida, e **zero mensagens decodificadas reprova** (L-40); antes do átomo existir, o autoteste não compila | `wire_relay_selftest` | — (o fio Wayland não existe no Windows, ausência permanente por desenho) | exceção `SEM-PENDENCIA`, `gemeo=nenhum` |
| A2 | Motor de regras R1/R2 e injetor de erro | Wayland | `tests/container/wire_relay/` | Autoteste: sequência com confirmação passa; comit antes da confirmação recebe `error 3` no objeto certo; número inventado recebe erro de número; confirmar só a última de duas configurações passa; varredura vazia reprova | `wire_relay_selftest` | — (idem) | idem |
| A3 | Ligar o relé na frente do KWin para os dois caminhos | Wayland | `tests/container/run_compositor.sh` (KWin sobe num soquete a montante, o relé ocupa o nome que as fixturas já usam), `tests/container/check_isolation.sh` (a prova de isolamento passa a exigir a cadeia inteira dentro do container: cliente, relé, KWin), `.github/workflows/ci.yml` (passo do autoteste + linha no inventário de paridade), `tools/preci.sh` (espelho do mesmo passo) | **Mutação real, as duas, em cópia extraída (L-27):** confirmação removida em `window_adapter.cpp:294` tem de reprovar o `window_smoke` com `rejected_value == "xdg_surface"` E o `gl_context_parity_test` do container; sem o relé, as duas mutações passam (é o estado de hoje, F4) | `window_smoke`, `gl_context_parity_test` | o `gl_context_parity_test` do Windows continua como está (sem conceito de confirmação) | inalterado |
| A4 | Configuração injetada: exercer o segundo sítio de confirmação | Wayland | `tests/container/wire_relay/` (injetor de configuração com número próprio, cuja confirmação o relé consome em vez de repassar, porque o KWin não conhece esse número); fixture existente ganha um cenário, ou fixture nova `window_reconfigure_smoke` | Mutação: `ack_pending_configure()` (`window_adapter.cpp:303`) vazio → R3 reprova; hoje nenhuma fixture passa por esse sítio (a saída do relé imprime quantas configurações por superfície; **1 por superfície reprova o cenário**, porque não exerceu nada) | `window_reconfigure_smoke` (ou o cenário novo, nome fixado pelo implementador e citado de volta aqui) | — (idem) | `SEM-PENDENCIA` |
| A5 | Todas as fixturas do container pelo relé estrito | Wayland | `tests/container/run_compositor.sh`; lista fechada de exceção, cada uma com motivo, dentro do próprio script, impressa a cada execução | O relé imprime por fixtura "N mensagens, M objetos desconhecidos, K violações"; uma fixtura que passar pelo relé com N = 0 reprova (não passou por ele); `fatal_error_smoke` (mata o compositor) e `two_displays_test` (duas conexões) exercem o fim de arquivo e o multi-cliente | todas as fixturas de `ci.yml` do trabalho `wayland-container`, nas duas pernas (`plain`, `asan`) | — | inalterado |
| A6 | Verdade escrita e paridade | Wayland | cabeçalho de `tests/container/window_smoke.cpp` (a alegação passa a citar o relé e a regra R1), comentários de `window_adapter.hpp/.cpp` e `egl_context_adapter.cpp` (a frase "o compositor reclama sozinho" é FALSA para o KWin: consertar), `tests/parity_exceptions.txt:291` (a linha do `window_smoke` vira `SEM-PENDENCIA`, D5), `tests/parity_exceptions.txt` (linha nova do `wire_relay_selftest`) | O portão de paridade reprova se a linha do `window_smoke` continuar apontando para este item no commit que o fecha (regra "concluído sem par") | `window_smoke` | — | `SEM-PENDENCIA` |

**Fecha quando:** as duas mutações (sítios `:294` e `:303`) reprovam no container com o nome `xdg_surface` pelos dois caminhos, com a saída literal e o código de saída lidos de variável registrados; A5 imprime a contagem por fixtura e nenhuma tem zero; o autoteste do relé tem os três controles da casa; os comentários falsos saíram; o trabalho `wayland-container` do servidor roda o autoteste nas duas pernas.

### 3.B `PLAN-SCOPE-REGEX-BLIND` e `LINK-PREFIX-SUBSTRING` (linhas 531, 532)

**Estado medido (F3):** ✅ entregues e verdes no servidor. Nada a implementar. **Ação do orquestrador:** o texto "aguardando push/CI real" das duas linhas está velho; trocar pela citação de `92d85a2`. Os resíduos medidos pelo orquestrador em 19/09 viram a linha nova da seção 3.B2, e o de `PLAN-SCOPE-REGEX-BLIND` entra em `PLAN-SCOPE-COLUMNS` (D17).

### 3.B2 `LINK-PREFIX-RESIDUOS` (linha NOVA, logo depois da 532; D6)

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| B1 | Âncora de `/build` por "não é continuação de caminho" em vez de "espaço antes" | comum | `tests/tools/check_container_fixture_link.py` (`rewrite_build_prefix`) | Controles novos no `--selftest` com `--out=/build/x`, `'/build/x'` e `-I/build/inc`: hoje saem intactos (medido pelo orquestrador); depois, reescritos; os dois controles negativos atuais (`/var/tmp/builds/`, prosa inglesa) continuam intactos | `container_fixture_link_selftest` | `container_fixture_link_selftest` (portão de texto, roda nos dois, L-04) | mesmo nome |
| B2 | O controle `MULTI` exige o motivo certo da falha | comum | mesmo arquivo (`selftest_accumulates_multiple_failures`) | Plantar compilador inexistente: hoje o controle diz OK; depois, reprova porque o texto não traz `undefined reference` | idem | idem | idem |

**Fecha quando:** os dois controles novos foram vistos vermelhos contra o código de antes e verdes depois, com a varredura das 663 ocorrências de `/build` em `tests/container/` reimpressa (contagem, não número copiado daqui).

### 3.C `PARITY-ALIAS-HYGIENE` (linha 615)

**Estado medido:** aberto (F8, F9, F10). **Desenho (D7-D10):** o portão passa a imprimir, sempre, "N apelidos, M mortos, K bilaterais declarados, J bilaterais sem declaração" e "N exceções, M mortas", e reprova em M > 0 e J > 0. O formato ganha a forma de dizer a verdade que falta: apelido bilateral declarado num terceiro campo com motivo; exceção de "prova parcial" num quinto campo opcional, em que o gêmeo TEM de existir no inventário do outro lado (conferido por máquina, coisa que a forma atual nunca confere).

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| C1 | Apelido morto conta e reprova | comum | `tests/tools/check_test_parity.py` (`apply_aliases` devolve também o que nunca casou; `run_comparison` conta e reprova), cabeçalho de `tests/parity_aliases.txt` | Autoteste com apelido cujo nome não está em inventário nenhum: hoje passa calado; depois reprova citando a linha | `test_parity_selftest` (nome real a conferir contra `tests/CMakeLists.txt` pelo implementador) | mesmo, roda nos dois | mesmo nome |
| C2 | Apelido bilateral só com declaração | comum | mesmo portão; `tests/parity_aliases.txt` (terceiro campo `bilateral=<motivo>` nas três linhas de F8, depois de LER os dois lados de cada par; se o par do `frame_callback_sequence_test` não se sustentar, a linha sai e vira exceção honesta) | Autoteste com apelido bilateral sem declaração reprova; com declaração, conta e imprime | idem | idem | idem |
| C3 | Exceção morta conta e reprova (gêmeo L-17 do apelido morto) | comum | mesmo portão (`validate_exceptions` passa a receber os inventários) | Autoteste com exceção cujo teste existe dos dois lados, e outra cujo teste não existe de lado nenhum: hoje passam; depois reprovam | idem | idem | idem |
| C4 | Forma `PROVA-PARCIAL` | comum | mesmo portão (`parse_exceptions_text` aceita quinto campo opcional); cabeçalho de `tests/parity_exceptions.txt`; a linha do `gpu_kind_report_smoke` (`:317`) passa a usá-la **se, e só se,** o gêmeo nomeado existir no inventário do Windows; se não existir, a linha é uma ausência e fica como está | Autoteste: `PROVA-PARCIAL` com gêmeo ausente do outro inventário reprova; com gêmeo presente passa; regra de morte por item concluído continua valendo | idem | idem | idem |

**Fecha quando:** a execução real no servidor imprime as duas contagens (esperado hoje: 30 apelidos, 0 mortos, 3 bilaterais declarados; 28 exceções, 0 mortas — **remedir, não ler daqui**) e os quatro controles novos foram vistos vermelhos antes do conserto.

### 3.D `DOCS-COUNT-VOCAB` (linha 621)

**Estado medido:** aberto (F11-F14). O "seis documentos" do item apodreceu: a wiki pública não existia em 06/09. **Desenho (D11-D14):**
- **Universo:** a MESMA enumeração do portão de travessão (`check_dash_pubdoc.py`, importada, nunca copiada). Ela é mais larga que "documentos ao consumidor" (F13), então o portão novo é mais estrito, nunca mais frouxo.
- **Vocabulário fechado e nomeado**, uma ficha por forma, no cabeçalho: algarismo seguido de até dois modificadores e de um substantivo de contagem (`tests`, `test cases`, `cases`, `controls`, `checks`, `jobs`, `gates`, `fixtures`, `assertions`, `scenarios`, `pairs`); `N of N`; `N/N` seguido de substantivo de contagem; **número por extenso só na forma "palavra-número + até dois modificadores + substantivo de contagem"** (pega "ten renamed pairs", "forty controls"), calibrado antes de existir (D13).
- **Isenções, lista fechada e impressa:** trecho entre crases; citação `L-NN`; frase que carrega citação de medição datada no MESMO período (número de run do servidor ou SHA entre crases), que é o caso legítimo de F14 (`gl-loop-portability-matrix.md:38`); seções JÁ LANÇADAS do `CHANGELOG.md` (cabeçalho `## [A.B.C.D] - data`), contadas e impressas como "histórico datado", enquanto `[Unreleased]` é varrida sem isenção.
- **Gramática de citação num módulo só** (`tests/tools/citation_grammar.py`), que `CLAIM-CITATIONS` estende e `PLAN-SCOPE-COLUMNS` reusa (D14).

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| D1 | Gramática de citação (forma "medição datada") | comum | `tests/tools/citation_grammar.py` (novo) | Autoteste do módulo: run sem número, SHA fora de crase e data sozinha NÃO são citação; `gh run view 34975391524` e `` `14427bc` `` são | `docs_count_vocab_selftest` | mesmo | mesmo |
| D2 | Portão de vocabulário | comum | `tests/tools/check_docs_count_vocab.py` (novo), `tests/CMakeLists.txt` (`docs_count_vocab_test` e `_selftest`, sem guarda de sistema) | **Controle negativo = a ponte do revisor reconstruída** a partir das frases que motivaram tudo (`"Linux's 90"`, `"88"`, `"ten renamed pairs"`, `"forty controls"`, `"has N registered cases"`) mais `N of N jobs`: todas reprovam; positivo: os documentos reais passam; varredura vazia reprova | `docs_count_vocab_test` | mesmo | mesmo |
| D3 | Calibração contra a árvore real ANTES de ligar (L-43 do projeto) | comum | relatório impresso no commit; se aparecer contagem viva sem citação num documento, o documento se conserta no mesmo commit (hoje, pela medição de F14, `tests/parity_exceptions.txt:528` precisa de decisão do implementador: é texto interno em `.txt`, citar a medição ou reescrever sem número) | Saída com "varreu N arquivos, M frases com forma de contagem, K isentas por citação, H por histórico, 0 vivas" | idem | idem | idem |

**Fecha quando:** o cabeçalho do portão diz o que ele NÃO vê (número por extenso fora da forma nomeada; número em imagem; paráfrase livre), os três controles da casa foram vistos vermelhos, e a calibração está no commit.

### 3.E `CLAIM-CITATIONS` (linha 622)

**Estado medido:** aberto (F15-F17). **Desenho (D14, D15, D22):**
- **Inventário de nomes de teste num módulo só** (`tests/tools/test_name_inventory.py`): nomes de `add_test(NAME ...)` e `glintfx_add_test(...)` de `tests/CMakeLists.txt` (reusando `extract_add_test_blocks` de `check_selftest_orphan.py`), fixturas do container (reusando `parse_containerfile_fixtures` de `check_container_fixture_inventory.py`), e casos `GLINTFX_TEST`. Importa, não copia.
- **Vocabulário fechado:** igualdade entre sistemas (`identical`, `identically`, `same on both/all`, `both systems`, `both platforms`, `all five`, `every platform`) e medição (`measured`, `proved`, `proven`). **Fora, perda declarada no cabeçalho:** `never` e `always` (378 e 26 ocorrências, quase todas contrato de função, e não alegação de medida).
- **Citação aceita** no mesmo bloco de comentário: `Proved by:`/`proven by`/`see` seguido de nome que EXISTE no inventário; `static_assert` nomeado; ou a categoria declarada "by construction", aceita mas **contada à parte** e impressa ("N alegações por construção, sem teste").
- **`tests/claim_exceptions.txt`**, mesma forma e mesma regra de morte da paridade: `arquivo | trecho-âncora | item`; morre quando o item fecha. Primeira linha prevista: a alegação de layout de `value.hpp:55-59` → `GFSS-VALUE-LAYOUT-ASSERT` (W10, meio entregue, F27).

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| E1 | Inventário de nomes de teste | comum | `tests/tools/test_name_inventory.py` (novo) | Autoteste: nome inventado não está; nome registrado só no bloco `if(WIN32)` está; lista vazia reprova | `claim_citations_selftest` | mesmo | mesmo |
| E2 | Reenumerar e consertar as alegações dos cabeçalhos | comum | `include/glintfx/**/*.hpp` (só comentários), `cmake/glintfx.version:87` (trocar "os dois intrusos" pelo comando que mede: `nm -D` sobre a biblioteca construída em `Debug`, filtrando os padrões do próprio roteiro) | A lista impressa pelo portão (E3) contra a árvore de hoje é o vermelho; cada linha se resolve citando teste, reescrevendo sem a promessa, ou em `claim_exceptions.txt` com item aberto | idem | idem | idem |
| E3 | Portão | comum | `tests/tools/check_claim_citations.py` (novo), `tests/claim_exceptions.txt` (novo), `tests/tools/citation_grammar.py` (forma "teste citado"), `tests/CMakeLists.txt` | Cópia com `measured` sem citação reprova; citação de teste que não existe reprova; exceção apontando para item concluído reprova; varredura vazia reprova | `claim_citations_test` | mesmo | mesmo |
| E4 | A convenção escrita onde o consumidor lê | comum | `docs/api-conventions.md` (a regra de citação vale também para cabeçalho, com o portão como prova) | O portão de travessão e o de vocabulário (D2) passam sobre o texto novo | idem | idem | idem |

**Regra de passagem para o português nos cabeçalhos (D16):** toda linha que E2 reescrever sai em inglês corrido; a entrada `CABECALHO-PUBLICO-EM-PORTUGUES` da INBOX NÃO é absorvida, e o commit imprime a contagem do comando dela antes e depois, só como medida.

**Fecha quando:** a enumeração inteira foi impressa e resolvida linha a linha, e os quatro controles foram vistos vermelhos.

### 3.F `PLAN-SCOPE-COLUMNS` (linha 625)

**Estado medido:** aberto (F18-F20). O defeito é maior que o registrado: o portão é cego ao plano inteiro do `fecho-w7b`, não só a `5C`. **Desenho (D17, absorve `REGEX-DE-FATIA-CEGO-A-MAIUSCULA` e `PLAN-SCOPE-SILENT-DEFAULT`):**
- Tabela de fatias reconhecida pelo **cabeçalho**, nunca pela posição: primeira coluna `#`, segunda `Fatia` ou `Sub-fatia`. Colunas por nome, vocabulário fechado: `Nasce / muda` → caminhos; `Prova Linux`, `Prova Windows`, `Prova`, `Par no portão`, `Teste vermelho de estreia`, `Estreia vermelha` → nomes de teste entre crases.
- Identificador = a primeira célula da linha, qualquer forma; `~~X~~` = fatia retirada, contada e impressa, nunca conferida.
- Nome de teste conferido contra `test_name_inventory.py` (E1), sem heurística nova; piso por coluna.
- `--absences` e `--todo` com caminho fixo do projeto por padrão; arquivo ausente reprova; nunca roda cego.
- A saída abre dizendo o próprio escopo: planos varridos, planos sem tabela de fatias (fora por forma, listados), colunas de prosa que NÃO varre.

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| F1 | Reconhecer tabela e coluna pelo cabeçalho, identificador pela célula | comum | `tests/tools/check_plan_scope_diff.py` (`find_row_line`, `split_row_columns`, `real_main`) | Autoteste com `5C`, `V-5a` e `~~V-6b~~`: hoje não acha nenhuma das três; depois acha as duas primeiras e imprime a terceira como retirada; tabela com coluna a mais antes das provas (forma do `plano-w6b-placa-e-laco`) lida certo | `plan_scope_diff_selftest` | mesmo | mesmo |
| F2 | Colunas de prova e de par contra o inventário | comum | mesmo arquivo, reusando E1 | Autoteste: nome inexistente na coluna de prova reprova; coluna de prova sem nenhum nome reprova pelo piso | idem | idem | idem |
| F3 | Ausência nunca opcional | comum | mesmo arquivo | Autoteste: rodar sem `tests/parity_absences.txt` presente reprova; o caso medido em 06/09 (`plano-w6a`, `W-D' headers públicos`) passa com a declaração existente | idem | idem | idem |
| F4 | Registrar e rodar mecanicamente | comum | `tests/CMakeLists.txt` (`plan_scope_diff_test` com rótulo `consume`, e o `_selftest`), `tools/preci.sh`, `tests/tools/selftest_orphan_exceptions.txt` (apagar a linha reservada), `tests/parity_absences.txt` (declarar, com item dono, cada ausência real que a primeira execução contra os planos versionados achar) | A primeira execução contra `docs/plano-*.md` é o vermelho; cada ausência vira declaração com item aberto, ou prova de que a fatia foi entregue | `plan_scope_diff_test` | mesmo | mesmo |

**Fecha quando:** o portão roda no `ctest` dos dois sistemas contra todos os planos versionados, a saída declara o próprio escopo, e a linha reservada em `selftest_orphan_exceptions.txt` sumiu no mesmo commit.

### 3.G `DISPLAY-PASSKEY-CONTROL` (linha 626)

**Estado medido:** aberto (F21-F23). O item nomeia uma chave e há quatro. **Desenho (D18, D19):** as quatro chaves saem do cabeçalho público; o cabeçalho público mantém só `friend struct X;`, e a definição passa para um cabeçalho interno em `src/`. Para o consumidor o tipo fica **incompleto**: chamar `X::get()` deixa de compilar nos DOIS modos, compartilhado e estático, e a pergunta aberta do item (o modo estático alcança?) desaparece por construção em vez de ser medida e aceita. Fraude deliberada (redefinir o tipo no próprio programa) fica fora da promessa, declarada, como o GotW #76 ensina.

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| G1 | Controle negativo primeiro (L-20: vermelho antes do conserto) | comum | `tests/tools/check_passkey_unreachable.py` (novo), sondas de consumidor em `tests/passkey_probe/` (uma por chave, mais o gêmeo positivo sem a chamada), `tests/CMakeLists.txt` | Contra a árvore de hoje: as quatro sondas COMPILAM (é o defeito); no modo estático também LIGAM. O portão exige saída diferente de zero **E** diagnóstico de tipo incompleto (GCC/Clang `incomplete type`, MSVC `C2027`) **E** gêmeo positivo compilando | `passkey_unreachable_test` (nos dois modos) | mesmo, com MSVC | mesmo nome |
| G2 | Mover as quatro definições | comum | `include/glintfx/platform/window/display.hpp`, `window/window.hpp`, `gl/context.hpp`, `loop/loop.hpp` (só o `friend`), cabeçalho interno novo em `src/platform/`, `display_facade.cpp`, `window_facade.cpp`, `gl_context_facade.cpp`, `loop_facade.cpp`, os cinco testes de F22, `tests/container/prepare_arch_ports_fixture.sh` (encenar o cabeçalho interno) | G1 fica verde; a suíte inteira dos dois modos continua verde | `passkey_unreachable_test` | mesmo | mesmo |
| G3 | Enumeração fechada que impede a quinta chave de nascer no lugar errado | comum | mesmo portão: todo `friend struct *_internal_access` em `include/` não pode ter definição completa em `include/` | Cópia com uma quinta chave definida no cabeçalho público reprova; zero chaves achadas reprova | idem | idem | idem |
| G4 | Convenção e verdade escrita | comum | `docs/api-conventions.md` (regra nova: costura interna é tipo incompleto para o consumidor, com o portão como prova), comentário de `window_facade.cpp:48-60` (a prova deixa de viver fora da árvore) | E3 aceita a citação nova | idem | idem | idem |

**Fecha quando:** o controle roda nos dois modos e nos dois sistemas, foi visto vermelho contra as quatro chaves de hoje, e a promessa escrita é a mesma nos dois modos.

### 3.H `CI-VERDE-W7C` (linha 627)

Ver a seção 4. É a última linha, por construção.

---

## 4. Definição de fechamento da ONDA (L-24; CI verde não basta sozinho)

1. Todas as linhas de onda `W7-C` estão ✅, incluindo a nova `LINK-PREFIX-RESIDUOS`; a passagem para ✅ só depois da verificação do main (L-12), **com sabotagem de família diferente da do planejador e da do implementador** (contrapeso obrigatório da emenda de 22/09 à L-34, porque planejador e orquestrador são da mesma família).
2. Cada sub-fatia tem a estreia vermelha registrada: saída literal, código de saída lido de variável, e o SHA contra o qual se verificou (L-27).
3. As duas mutações de confirmação (`window_adapter.cpp:294` e `:303`) reprovam no container, pelos dois caminhos, e voltam a passar sem a mutação.
4. O espelho local (`tools/preci.sh`) inteiro verde, em container, antes de cada push (memória `feedback_espelho_local_antes_do_commit`); mudança em estado de processo passa também por `--sanitizer-only`.
5. Ramo `onda-w7c` empurrado; `gh run view <id> --json conclusion,jobs` lido direto: número de trabalhos verdes igual ao total menos os pulados por desenho, e **cada pulado nomeado** (hoje, só "Versao == etiqueta (VERSION-TAG-SYNC)"); a lista de trabalhos da execução comparada com a de `ci.yml`, para provar que os passos novos (autoteste do relé nas duas pernas, os portões novos no `ctest`) rodaram de fato no servidor.
6. Na execução do servidor, o portão de paridade imprime as contagens novas, e o de escopo de plano imprime o próprio escopo.
7. INBOX atualizada: as entradas absorvidas saem de lá com o destino escrito; as linhas novas estão na tabela (seção 7).
8. Merge em `main` e marca pela L-11 emendada; número pela L-26 (D19): sobe o `B` em relação à última marca existente no instante do fechamento, porque G muda o que um consumidor consegue compilar.

---

## 5. Decisões tomadas no lugar do líder (uma por bloco; o main registra em `DECISOES_AUTONOMAS.md`)

**D1. Como provar que a confirmação chegou, se o KWin não confere.**
- Opções: (a) relé estrito em casa, dentro do container; (b) ler o registro de protocolo do próprio cliente (`WAYLAND_DEBUG`); (c) servidor falso sobre `libwayland-server`; (d) segundo compositor estrito dentro do container.
- Escolhida: **(a)**. Recebe e assevera no fio, com o compositor real atrás (o caminho gráfico continua funcionando), e reproduz o comportamento dos compositores onde a comunidade se machuca. (b) prova só o envio e depende do formato do registro de uma versão da biblioteca do sistema; (c) mexe na fronteira da L-07 (decisão do líder); (d) é pacote novo (L-14/L-51).
- Fonte: F4, F5; Zed #10976; SDL #4563; blog do mstoeckl; waypipe. Mão única: não. Reverter: barato (o relé sai do `run_compositor.sh`).

**D2. Linguagem do relé.** Opções: C++ no estágio de construção do container, ou Python. Escolhida: **C++**, porque a presença do `python3` na imagem final não foi medida (F7) e acrescentá-lo seria pacote novo. Mão única: não. Reverter: barato.

**D3. Só as duas fixturas, ou todas pelo relé.** Escolhida: **todas (A5)**, com lista fechada de exceção. É o que resolve a dor de fato: toda violação que o KWin engole passa a aparecer em qualquer fixtura, não só nas duas que já sabemos. Custo: tempo de CI e risco de o relé ter defeito, controlado por A1 e A2 antes de A5. Mão única: não.

**D4. Exercer o segundo sítio de confirmação com configuração injetada, e a regra R3 declarada como regra da casa.** Sem isso o `:303` fica sem teste nenhum (F6). R3 não se vende como protocolo: o cabeçalho e a saída dizem "regra da casa". Fonte: xdg-shell (consumo de números), SDL3 (confirma a última). Mão única: não.

**D5. O `window_smoke` se aposenta?** (a pergunta pendente D-W6b-12). Escolhida: **não se aposenta**; ele é a prova do caminho de memória compartilhada, e o `gl_context_parity_test` a do gráfico; o item responde pelos dois. A exceção de paridade dele vira ausência permanente por desenho (`SEM-PENDENCIA`): no Windows não existe confirmação de configuração. Mão única: não.

**D6. Resíduos de um item já concluído.** Opções: reabrir `LINK-PREFIX-SUBSTRING`; linha nova na mesma onda; deixar na INBOX. Escolhida: **linha nova `LINK-PREFIX-RESIDUOS` logo depois da 532**. Reabrir apagaria o registro de que a primeira entrega foi verificada verde; deixar na INBOX deixa o gêmeo do defeito vivo no mesmo arquivo. Completude de um item da própria onda, permitida pelo refinamento de 22/09 da L-32. Mão única: não.

**D7. Apelido morto: avisar ou reprovar.** Escolhida: **reprovar**. Fonte: ESLint #18665 (aviso é ignorado), Rust `#[expect]`. Mão única: não.

**D8. Apelido bilateral (a parte (b) que o item deixou aberta).** Opções: só avisar; reprovar sempre; reprovar salvo declaração com motivo. Escolhida: **reprovar salvo declaração**. Reprovar sempre obrigaria a desregistrar testes que rodam com valor dos dois lados; só avisar é o defeito do ESLint antigo. Fonte: F8. Mão única: não.

**D9. Exceção morta.** Escolhida: **contar e reprovar**, gêmeo L-17 do apelido morto (F10). Mão única: não.

**D10. Forma para "gêmeo existe, um passo de prova não roda naquele sistema"** (a entrada da INBOX dizia "decisão do líder"). Opções: a barata (cabeçalho nomeia um item canônico); a forte (sentinela novo). Escolhida: **a forte, como quinto campo opcional**, com a exigência nova de o gêmeo existir no outro inventário, que é conferência a mais, não a menos. Mão única: não.

**D11. Universo do portão de contagem.** Opções: lista dos "seis documentos"; classificador novo por leitor; a enumeração do portão de travessão. Escolhida: **reusar a enumeração do travessão** (fonte única, e mais larga). Uma lista nova seria cópia de algo que já tem dono (memória `feedback_copia_em_vez_de_fonte`). Mão única: não.

**D12. Número legítimo em documento.** Escolhida: número com forma de contagem só vale com citação de medição datada no mesmo período; seções lançadas do `CHANGELOG` contam como histórico datado, contadas e impressas. Fonte: Keep a Changelog; F14. Mão única: não.

**D13. Número por extenso.** Em 06/09 o CTO o excluiu (quatro falsos positivos com palavra-número solta). Escolhida: **incluir só na forma "palavra-número + até dois modificadores + substantivo de contagem"**, calibrada antes de ligar; se a calibração achar falso positivo, cai de volta na decisão de 06/09, com a perda declarada. Mais completo, com recuo definido. Mão única: não.

**D14. Dois módulos compartilhados** (`citation_grammar.py`, `test_name_inventory.py`) em vez de três implementações da mesma pergunta. A regra de três (L-33) está cumprida: três portões fazem a mesma pergunta nesta onda. Mão única: não.

**D15. Vocabulário de alegação em cabeçalho.** Fechado nas formas de igualdade entre sistemas e de medição; `never`/`always` fora, com perda declarada; "by construction" aceito como categoria contada à parte; exceções com regra de morte. Mão única: não.

**D16. Jargão em português nos cabeçalhos** (`CABECALHO-PUBLICO-EM-PORTUGUES`). Escolhida: **não absorver**. O próprio texto da entrada reserva ao líder a entrada na tabela, e o congelamento de escopo de 27/08 vale até `DEMO-1`. Só as linhas que E2 reescrever saem em inglês. Mão única: não.

**D17. Portão de escopo de plano.** Tabela e colunas pelo cabeçalho, identificador pela célula, retirada contada, ausências obrigatórias, registro no `ctest` contra todos os planos versionados. Absorve `REGEX-DE-FATIA-CEGO-A-MAIUSCULA` e `PLAN-SCOPE-SILENT-DEFAULT` (mesmo portão, completude do item). Fonte: F18-F20; Doorstop. Mão única: não.

**D18. Chaves internas.** Opções: (a) controle de ligação só para a de display, aceitando que o modo estático alcança; (b) controle nos dois modos para as quatro, aceitando o estático medido; (c) tirar as quatro definições do cabeçalho público (tipo incompleto) e provar a falha de COMPILAÇÃO nos dois modos e nos dois sistemas. Escolhida: **(c)**. A mesma promessa nos dois modos, as quatro chaves cobertas (L-17), e a pergunta do modo estático some por construção. Fonte: GotW #76; documentação do CMake sobre `WILL_FAIL`/`PASS_REGULAR_EXPRESSION`. **Mão única: não**; tira superfície (devolver é acréscimo). Reverter: barato.

**D19. Número da marca.** G faz um programa de consumidor que compilava deixar de compilar. Antes da 1.0 a quebra sobe o `B` (L-26). Escolhida: **subir o `B`** sobre a última marca existente no fechamento (hoje `v0.5.0.0`, então `v0.6.0.0` se nada marcar antes). Mão única: a marca publicada é.

**D20. Versionar este plano em `docs/plano-w7c.md`**, com as tabelas no esquema que o portão de F reconhece, para que ele mesmo seja conferido por máquina. Mão única: não.

**D21. Modos de falha do relé** (compositor pendurado, compositor que para de ler, desconexão no meio do quadro; INBOX "o compositor existe mas não responde", mais a prova ponta a ponta do lado de escrita). Dor real: libwayland que desconecta cliente lento ("Data too big for buffer", <https://github.com/labwc/labwc/issues/3399>, <https://github.com/swaywm/sway/pull/8532>); cliente que bloqueia em despacho <https://groups.google.com/g/fltkcoredev/c/yJ16NzSdLJY>. Escolhida: **linha nova `WL-RELAY-FAULTS`, desenhada aqui e registrada DEPOIS de `DEMO-1`** (congelamento de escopo); o relé nasce com transporte separado das regras para aceitá-la sem retrabalho, mas o modo não é construído agora. Mão única: não.

**D22. `GFSS-VALUE-LAYOUT-ASSERT` fica na W10**; a alegação dele entra em `claim_exceptions.txt` apontando para ele. Mão única: não.

---

## 6. O que continua exigindo o líder

1. **Segundo compositor estrito no container** (Weston headless ou um baseado em wlroots), como segunda testemunha além do relé: é pacote novo na imagem (L-14/L-51). Recomendação: vale a pena depois de `DEMO-1`; não bloqueia esta onda.
2. **`libwayland-server` como ferramenta de teste**: muda a fronteira da L-07. Não usada neste plano.
3. **O portão de travessão classifica documentos internos em português (`docs/lente-*.md`) como públicos** (F13). Corrigir estreitaria um portão, e afrouxar portão não passa para o C-level. Só se registra na INBOX.
4. **Ratificação retroativa de D1 a D22** ao sair do modo autônomo (L-34).
5. **Marca e merge**: autorizados pela emenda de 22/09 à L-11 ao fim verde; o gancho local P2 exige a válvula de modo autônomo; é o main quem aciona.

---

## 7. Linhas novas e destino da INBOX

| Linha | Onda | Onde entra | Motivo |
|---|---|---|---|
| `LINK-PREFIX-RESIDUOS` | W7-C | logo depois de `LINK-PREFIX-SUBSTRING` | D6; absorve `ANCORA-DE-BUILD-CEGA-A-IGUAL-E-ASPAS` e `SELFTEST-MULTI-NAO-DISTINGUE-CAUSA-DA-FALHA` |
| `WL-RELAY-FAULTS` | pós-`DEMO-1` (onda decidida pelo main na hora de registrar; nasce `💡 Decisão tomada`, congelada) | depois de `DEMO-1` | D21; absorve "o compositor existe mas não responde" e a prova ponta a ponta de `poll_and_dispatch_with_budget` |

**Absorvidas em itens existentes:** `REGEX-DE-FATIA-CEGO-A-MAIUSCULA` e `PLAN-SCOPE-SILENT-DEFAULT` → `PLAN-SCOPE-COLUMNS` (D17); `PARIDADE-EXCECAO-SEM-FORMA-PARA-PROVA-PARCIAL` → `PARITY-ALIAS-HYGIENE` (D10).

**Não absorvidas, com motivo:** `CABECALHO-PUBLICO-EM-PORTUGUES` (D16); `FIXTURE-CAMINHO-DE-FALHA` (o relé passa a forçar falha real em A3/A4, o que atende metade; a medição de ramos por fixtura é escopo novo, congelado); `TABELA-DE-PENDENCIAS-NAO-TEM-PORTAO-DE-FORMA` e `CONTAGEM-DOCUMENTADA-NAO-CONTA-SEIS-LINHAS` (assunto é a tabela do `TODO.md`, não esta onda).

**Resolvida na árvore, a tirar da INBOX pelo main:** a entrada sobre o documento que negava os adaptadores gráficos (F26).

---

## 8. Paralelismo: quem toca o quê

| Item | Arquivos que toca |
|---|---|
| A `WL-ACK-SMOKE-BLUNT` | `tests/container/wire_relay/*` (novos), `tests/container/Containerfile`, `tests/container/prepare_arch_ports_fixture.sh`, `tests/container/run_compositor.sh`, `tests/container/check_isolation.sh`, `tests/container/window_smoke.cpp`, possível fixtura nova em `tests/container/`, `src/platform/wayland/window_adapter.{hpp,cpp}` (comentários), `src/platform/wayland/egl_context_adapter.cpp` (comentários), `.github/workflows/ci.yml`, `tools/preci.sh`, **`tests/parity_exceptions.txt`**, **`TODO.md`** |
| B `LINK-PREFIX-RESIDUOS` | `tests/tools/check_container_fixture_link.py`, **`TODO.md`** |
| C `PARITY-ALIAS-HYGIENE` | `tests/tools/check_test_parity.py`, `tests/parity_aliases.txt`, **`tests/parity_exceptions.txt`** (cabeçalho e linha `:317`), **`TODO.md`** |
| D `DOCS-COUNT-VOCAB` | `tests/tools/citation_grammar.py` (novo), `tests/tools/check_docs_count_vocab.py` (novo), **`tests/CMakeLists.txt`**, possivelmente `tests/parity_exceptions.txt:528`, **`TODO.md`** |
| E `CLAIM-CITATIONS` | `tests/tools/test_name_inventory.py` (novo), `tests/tools/check_claim_citations.py` (novo), `tests/tools/citation_grammar.py`, `tests/claim_exceptions.txt` (novo), `include/glintfx/**/*.hpp` (comentários), `cmake/glintfx.version`, `docs/api-conventions.md`, **`tests/CMakeLists.txt`**, **`TODO.md`** |
| F `PLAN-SCOPE-COLUMNS` | `tests/tools/check_plan_scope_diff.py`, `tests/parity_absences.txt`, `tests/tools/selftest_orphan_exceptions.txt`, `tools/preci.sh`, **`tests/CMakeLists.txt`**, **`TODO.md`** |
| G `DISPLAY-PASSKEY-CONTROL` | `include/glintfx/platform/{window/display.hpp,window/window.hpp,gl/context.hpp,loop/loop.hpp}`, cabeçalho interno novo em `src/platform/`, `src/platform/window/{display_facade,window_facade}.cpp`, `src/platform/gl/gl_context_facade.cpp`, `src/platform/loop/loop_facade.cpp`, `tests/passkey_probe/*` (novos), `tests/tools/check_passkey_unreachable.py` (novo), `tests/container/{gpu_kind_report_smoke,loop_hidden_test,facade_pin_smoke}.cpp`, `tests/loop_hidden_test.cpp`, `tests/win32_facade_pin_test.cpp`, `tests/container/prepare_arch_ports_fixture.sh`, `docs/api-conventions.md`, **`tests/CMakeLists.txt`**, **`TODO.md`** |

**Colisões:** A×C em `tests/parity_exceptions.txt`; D×E×F×G em `tests/CMakeLists.txt`; E×G nos quatro cabeçalhos públicos e em `docs/api-conventions.md`; A×G em `prepare_arch_ports_fixture.sh` e nas fixturas do container; D→E→F dependem dos módulos compartilhados; todos em `TODO.md`.

**Recomendação: duas pistas, no máximo dois implementadores vivos.**
- **Pista 1 (container):** A1 → A2 → A3 → A4 → A5; A6 por último, só depois de C ter comitado a sua mudança em `tests/parity_exceptions.txt`.
- **Pista 2 (portões de texto):** B → C → D → E → F, em série (módulos compartilhados e `tests/CMakeLists.txt`).
- **G depois das duas pistas fecharem** (colide com A e com E).
- **`TODO.md` só o main edita**, em série, no fechamento de cada fatia.
- **Trabalho pesado (L-11):** construção de imagem de container, suíte completa e compilação de sanitizador são um de cada vez entre as duas pistas; armar o `watchcode` na janela de cada um (L-25 do projeto). A pista 2 é leve até rodar a suíte inteira.
- **Ordem da tabela (emenda de 09/09 à L-32):** as pistas começam juntas, mas nenhuma fatia se pula, e a ordem dos commits segue a ordem das linhas onde há colisão.

---

## 9. Ataque ao próprio plano (onde ele pode estar errado)

1. **O relé pode ter defeito e reprovar fixtura boa.** Contenção: A1 prova o modo transparente com as fixturas atuais passando ANTES de ligar regra; A5 vem por último e com contagem por fixtura.
2. **O caminho gráfico pode usar interface que o relé não conhece** (buffer por `dmabuf` do Mesa). O comit em `wl_surface` com número de buffer diferente de zero basta para R1; o relé não precisa entender a origem do buffer. Se a primeira execução mostrar comit com buffer que ele não enxerga, A3 reprova pelo piso (zero comits com buffer vistos no caminho gráfico), nunca passa calado.
3. **A configuração injetada (A4) pode confundir o KWin.** O relé consome a confirmação do número que ele mesmo inventou, sem repassar. O KWin não confere número (F4), mas o relé também não deve depender disso: se o compositor mudar, o cenário quebra alto.
4. **G é a única fatia que muda o que o consumidor compila.** Se a revisão de API achar consumidor legítimo das chaves (nenhum conhecido: o texto delas diz "não é para você"), D18 volta ao C-level antes do merge.
5. **Planejador e orquestrador são da mesma família.** A emenda de 22/09 torna obrigatória a sabotagem de família diferente na verificação do main, e ela entra no critério 1 da seção 4.
