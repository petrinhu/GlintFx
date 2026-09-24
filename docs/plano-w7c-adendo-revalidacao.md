# Adendo ao plano da W7-C: revalidação contra a árvore depois da W7-B

**Autor:** Caetano (CTO), C-level `opus`, esforço alto, em modo autônomo (L-34 emendada em 22/09/2026). **Data real:** 24/09/26 - 00:05:14 (`date`). **Árvore conferida:** `023e1c7` em `main` no início da leitura; `c0fb96f` e `7d09889` (do main, durante a leitura) não mudam nenhum fato abaixo além do citado em A0. **Não reescreve** `docs/plano-w7c.md` (escrito sobre `da1e325`, 23/09 06:37); diz, item por item, o que mudou, o que já foi feito e o que ficou obsoleto. **Única escrita deste agente:** este arquivo. Nada executado além de leitura, `grep`, `git log`/`git show`, `gh run view` e importação somente-leitura de dois módulos Python do portão (para chamar funções de análise de texto, sem escrita).

**Leis aplicadas e como:** L-43 e L-44 (pesquisa onde a árvore mudou o problema, seção 2, com URL); L-18/L-27 (seção 1 separa FATO, com comando, de INFERÊNCIA, marcada `INF`); L-17 (gêmeos enumerados, não buscados: seções 1.C e 3); L-20/L-36/L-40 (cada sub-fatia nova com estreia vermelha e piso); L-24 (fechamento escrito antes do dado, seção 4); L-45 (o oráculo de camadas não roda aqui; tudo que depende dele se prova no servidor); L-11 global (um trabalho pesado por vez; nenhum desenho de um processo por item); L-04 (portão de texto roda nos dois sistemas); L-22 (terceira tentativa não se faz por palpite).

---

## 1. Revalidação dos fatos F1 a F28 do plano

Legenda da coluna Estado: **VALE** (remedido, igual), **MUDOU** (remedido, diferente, com o novo valor), **FEITO** (o que o fato pedia já está na árvore), **ERRADO** (o fato já estava errado em `da1e325`, medido agora), **OBSOLETO**.

| # | Estado | Remedição (comando) | Resultado hoje |
|---|---|---|---|
| F1 | MUDOU (só posição) | `grep -nE 'W7-C' TODO.md` em `023e1c7` | linhas 563, 564, 565, 648, 654, 655, 658, 659, 660; mesma ordem. **`LINK-PREFIX-RESIDUOS` (D6) nunca foi registrada**: `grep -c LINK-PREFIX-RESIDUOS TODO.md` = 0. `WL-RELAY-FAULTS` (D21) também não: 0 |
| F2 | VALE | contagem de `\|` não escapado por linha | 12 pedaços nas nove linhas |
| F3 | VALE (texto ainda velho) | `sed -n '564,565p' TODO.md \| grep -o 'aguardando'` | as duas linhas ✅ continuam dizendo "aguardando push/CI real"; a troca pela citação de `92d85a2` não foi feita |
| F4, F5 | VALE (fonte externa, não remedida) | não relido | o plano cita o ramo `master` do KWin e do wlroots; nada na árvore depende de nova leitura antes de A3 |
| F6 | VALE | `grep -n xdg_surface_ack_configure src/ -r` | `window_adapter.cpp:294` e `:303`, sem mudança |
| F7 | MUDOU (só linhas) | `grep -n '^FROM\|python3' tests/container/Containerfile` | estágio `arch-ports-builder` em `:230`, imagem final `FROM fedora:44` em `:1026`; a lista do `dnf` da final (`:1037-1053`) continua sem `python3`. D2 (relé em C++) continua valendo |
| F8 | FEITO | `grep -cvE '^\s*(#\|$)' tests/parity_aliases.txt`; `grep -n bilateral=` | 30 apelidos; os três bilaterais agora declarados (`:155`, `:156`, `:297`) |
| F9 | FEITO (com outro texto) | `grep -c 'nunca casaram'` dá 0, mas a contagem existe com outra frase | o servidor imprimiu, no run `35944510792` (SHA `6a63ff8`, trabalho `107463396193`): "30 apelido(s), 0 morto(s), 3 bilateral(is) declarado(s), 0 bilateral(is) sem declaracao, 0 bilateral(is) falso(s), 0 meio-morto(s)..." e "32 excecao(oes), 0 morta(s)" |
| F10 | MUDOU | `grep -c '^[a-z_0-9]*\|' tests/parity_exceptions.txt` | **32** registros (eram 28); o portão agora confere exceção morta (C3 entregue) |
| F11 | VALE | `git log --diff-filter=D -- tests/tools/check_readme_test_count.py` | apagado em `fccf3f8` |
| F12 | VALE | `ls tests/tools \| grep -i vocab` | vazio |
| F13 | VALE, com um fato que o plano não viu | `scanned_files` + `in_scope` + `is_exempt` de `check_dash_pubdoc.py`, importados | 57 arquivos, zero não rastreados. **34 dos 57 são `.txt`, e são código ou dado, não documento**: 21 `CMakeLists.txt` (o portão casa por extensão, `in_scope()` = `.md` ou `.txt`), 12 arquivos de dado de `tests/` e uma licença de terceiro (`third_party/khronos/LICENSE-APACHE-2.0.txt`) |
| F14 | **ERRADO desde `da1e325`** | a mesma forma de vocabulário que a seção 3.D do plano declara, aplicada aos 57 | **10 frases, não 4.** As seis que o plano não contou já existiam em `da1e325` (conferido com `git show da1e325:<arquivo> \| grep -c`): três falsas "`2 of 26`", "`1 of 26`", "`2 of 26`" (`tests/CMakeLists.txt:195,793,802`, são DATAS: "achado 2 of 26/08/2026"), uma viva "`43 of 53`" (`tests/CMakeLists.txt:2773`), uma saída de ferramenta entre aspas "`2 tests failed out of 216`" (`tests/package/CMakeLists.txt:31`), um parâmetro de desenho "`100 consecutive pairs`" (`docs/gl-loop-portability-matrix.md:46`). A de `parity_exceptions.txt` mudou de `:528` para `:636` |
| F15 | VALE (um desvio) | `git ls-files -z include \| xargs -0 grep -oiw <termo> \| wc -l` | 33 arquivos; `never` 378, `measured` 34, `always` 26, `identical` 16, `proved` 15, `proven` 9, `every platform` 4, `identically` 3; `all five` dá **3** (o plano: 2; a diferença é de fronteira de palavra do comando, não da árvore: `include/` não mudou desde `da1e325`, `git diff --stat da1e325..HEAD -- include` vazio) |
| F16 | VALE | `grep -niE 'WM_SIZE\|resize'` sobre `include/` | vazio |
| F17 | VALE | leitura | "the two intruders" em `cmake/glintfx.version:87` |
| F18 | VALE | `grep` em `check_plan_scope_diff.py`, `ci.yml`, `preci.sh` | regex `\d+[a-z]*` em `:110`; `columns[4]` fixo em `:337`; `--absences`/`--todo` com `default=None` (`:311`, `:315`); zero chamadas em `ci.yml` e `preci.sh`; reserva em `tests/tools/selftest_orphan_exceptions.txt:22` |
| F19 | **MUDOU e piorou** | cabeçalho de cada tabela de `docs/plano-*.md` (lista abaixo, em 1.B) | 16 planos versionados (eram 11); as formas divergem mais do que o plano previu |
| F20 | MUDOU | idem | identificadores novos: `S1..S4` com coluna `Ordem` antes (`plano-loop-callbacks.md`), `V-5c`, `L-5d` a `L-5h` citados em prosa, `~~V-6b~~` segue riscado |
| F21, F22 | VALE | `grep -nE 'friend struct\|struct [a-z_]*internal_access'` sobre `include/`; `grep -rln internal_access tests/` | quatro chaves (`display.hpp:105`, `window.hpp:179`, `context.hpp:168`, `loop.hpp:299`); os cinco testes de F22 continuam os únicos rastreados que as usam (`tests/container/_arch_ports_src/` é cópia encenada, ignorada por `.gitignore:85`) |
| F23 | VALE | `sed -n 44,62p src/platform/window/window_facade.cpp` | a prova negativa continua fora da árvore |
| F24 | MUDOU | `gh run view 35944510792 --json jobs` | **26** trabalhos (eram 25), 25 verdes, 1 pulado por desenho (`Versao == etiqueta (VERSION-TAG-SYNC)`) |
| F25 | VALE | `git for-each-ref --sort=-creatordate refs/tags` | `v0.5.0.0` (22/09) |
| F26 | VALE (continua pendente) | `grep -n 'adaptadores gr' TODO.md` | a entrada resolvida continua na INBOX (`TODO.md:281` em `023e1c7`) |
| F27 | MUDOU (só linha) | `grep -n static_assert include/glintfx/gfss/value.hpp`; `grep -nE 'sizeof\|alignof\|is_standard_layout'` | um `static_assert` só, em `:439`; os outros três continuam faltando |
| F28 | VALE | `grep -n xdg_wm_base_pong src/platform/wayland/shell_adapter.cpp` | `:38` |

### 1.A Fatos NOVOS que a W7-B trouxe e que mudam itens desta onda

| # | Fato | Comando | Item afetado |
|---|---|---|---|
| N1 | `PARITY-ALIAS-HYGIENE` entregou C1, C2, C3 e C4 do plano (P-1 `5d0c173`, P-2 `3c0e0a1`/`c77b215`/`745f808`, P-3 `f189cf7`, P-4 `78390d3`); 41 controles no `--selftest`; o quinto campo `PROVA-PARCIAL=` existe (`check_test_parity.py:273`) e **não foi usado** na linha do `gpu_kind_report_smoke`, com motivo escrito (`parity_exceptions.txt:396-409`: o gêmeo cita arquivo-fonte, não nome de `ctest`) | `git log --oneline da1e325..HEAD -- tests/tools/check_test_parity.py`; linha do item | C |
| N2 | **O portão de paridade teria reprovado o CI de fechamento da W7-B**: `WIN-RUNNER-PROPRIO` virou ✅ em `84877aa` e a exceção `gpu_kind_report_smoke` (`parity_exceptions.txt:406`) ainda apontava para ele; `validate_exceptions()` (`check_test_parity.py:708-731`) reprova isso. Medido importando o módulo e rodando `parse_todo_status_text` + `parse_exceptions_text` + `validate_exceptions` contra a árvore: 1 erro, exatamente esse. Avisado ao main às 23:59; consertado por ele em `c0fb96f` (dono trocado para `WIN-LAB-INSTALAR`) | a importação descrita | nova linha `PARITY-LOCAL-MIRROR` (3.I) |
| N3 | É a **segunda** ocorrência medida da mesma família: `eab26db` (CONT-WARMUP) já tinha consertado uma exceção que só o servidor acusou (INBOX `PRECI-LOCAL-NAO-RODA-CHECK-TEST-PARITY`). `grep -c check_test_parity tools/preci.sh` = 0; no `ctest` só existe o `--selftest` (`tests/CMakeLists.txt:4503`) | os dois `grep` | 3.I |
| N4 | O gêmeo L-17 de N3: `check_measured_parity.py` também tem regra de morte por item concluído (26 ocorrências de `conclu`) e também só roda contra dado real no servidor (`ci.yml:1289-1295`; `preci.sh` 0; `ctest` só `--selftest`, `tests/CMakeLists.txt:4889`). Varredura de todos os arquivos de exceção atrás de ID ✅ no campo de item: zero hoje (duas menções a ID ✅ em PROSA, `measured_exceptions.txt:172` e `parity_exceptions.txt:369`, fora do campo) | laço Python único sobre os cinco arquivos | 3.I |
| N5 | CONT-WARMUP reescreveu a subida do compositor: `run_compositor.sh` (283 linhas) só declara pronto quando `wayland-info` completa uma ida e volta real no soquete (`compositor_probe()`, `:95-99`, com `timeout` por chamada, `:84-94`), e tem `--selftest` próprio registrado (`container_run_compositor_selftest`, exceção `parity_exceptions.txt:656`) | leitura | A3 |
| N6 | `fatal_error_smoke.cpp` agora mede, de forma determinística, a ordem do fim de conexão: primeira leitura depois do `pkill` vê `POLLIN\|POLLHUP` e ainda drena um último pedaço legítimo; só a segunda trava o erro (cabeçalho, `:33-60`) | leitura | A5 |
| N7 | `tests/container/CMakeLists.txt` põe todo `tests/container/*.cpp` no banco de compilação com `file(GLOB ... "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")` (`:41-43`, **não recursivo**), e o portão `container_smokes_in_compile_db_test` enumera por `git ls-files 'tests/container/*.cpp'` (`check_container_smokes_in_compile_db.py:62`), cujo `*` **atravessa `/`** (medido: `git ls-files 'tests/*.cpp' \| grep -c /container/` = 21). Um relé em `tests/container/wire_relay/` seria visto pelo portão e não entraria no banco: vermelho alto, não silêncio | os dois `grep` e a medição do pathspec | A1 |
| N8 | O relatório do oráculo de camadas se extrai do log com `grep '^check_layers_oracle.py:'` em TRÊS cópias (`ci.yml:644` Linux, `:3247` Windows em PowerShell, `:3401` Clang), e o piso "linhas > 0" (`:647-650`) é satisfeito pelas linhas do `layers_oracle_selftest`, que escreve com o MESMO prefixo no MESMO `LastTest.log` (INBOX `LAYERS-ORACULO-RELATORIO-MISTURA-SELFTEST`). **Consequência que a entrada da INBOX não escreveu:** se o `layers_oracle_test` real deixar de rodar, o passo que diz existir para acusar "o oráculo não rodou" passa verde com o texto do autoteste | leitura de `ci.yml:630-665` | nova linha `LAYERS-ORACLE-REPORT` (3.J) |
| N9 | Nenhum portão Python registrado no `ctest` roda sem buffer: `grep -n PYTHONUNBUFFERED` em `tests/CMakeLists.txt`, `ci.yml`, `preci.sh` = 0. Sob `ctest`, a saída padrão do Python vai para um cano, e o manual diz que aí ela é "block-buffered", enquanto a de erro é "line-buffered" (fonte na seção 2). A inversão da INBOX `LAYERS-ORACULO-STDERR-ANTES-DO-STDOUT` é, portanto, de TODOS os portões Python, não só do oráculo | o `grep` | 3.J |
| N10 | **A contagem de reconciliação do portão de ligação cruzada Windows é 104 + 14 + 9 = 127, não 104 + 12 + 9 = 125** como diz a linha `WIN-CROSS-TESTS-LINK` do `TODO.md` (`:652`, "104 ligados + 12 excluídos `if(UNIX)` + 9 menções em comentário = 125 ocorrências brutas"). Medido importando `check_win32_test_link.py` (só leitura) e chamando `extract_win32_test_targets` e `_count_add_test_comment_mentions_in_text` sobre `git show <ref>:tests/CMakeLists.txt`: em `HEAD` (`7d09889`), `023e1c7` e `b634876` (X-4), 104 alvos, 14 excluídos (`{'UNIX': 14}`), 9 menções em comentário, bruto 127, soma 127. O bruto também por `grep -c 'glintfx_add_test(' tests/CMakeLists.txt` = 127. **O 125 nunca foi o número da árvore:** o bruto já era 127 em `f610af9` (X-3), `0b495ef` e `9f54422`, e o extrator de ANTES da X-4 (arquivo de `f610af9`, carregado de uma cópia fora da árvore, apagada depois) também dá 14 excluídos sobre a árvore de `f610af9`. `INF`: o "12" veio de uma contagem feita à mão, não do portão; a frase "conferidas por `grep` independente do orquestrador, batendo exatamente" é falsa como está escrita | a importação descrita e o `grep -c` | passo 0 (seção 5) |

### 1.B As formas das tabelas de fatia nos 16 planos versionados (F19 remedido)

Comando: para cada `docs/plano-*.md`, a linha anterior a cada separador `|---`. Só as tabelas que listam fatias:

| Plano | Cabeçalho da tabela de fatias | O portão desenhado em D17 (`#` + `Fatia`/`Sub-fatia`) acha? |
|---|---|---|
| `plano-w6a-janela` | `# \| Fatia \| ... \| Nasce / muda \| ...` | sim |
| `plano-w6b-placa-e-laco` | com "Teste vermelho de estreia" antes das provas | sim |
| `plano-w6-folha-de-estilo` | coluna "Prova" única | sim |
| `plano-fecho-w7b` | `# \| Sub-fatia \| Fechamento \| Estreia vermelha` (5 tabelas) | acha a linha, mas **não há coluna de entrega nem de prova nomeada**: nada a conferir |
| `plano-w7c` | o esquema de D20 (7 tabelas) | sim |
| `plano-w7d` | `# \| Sub-fatia \| Entrega \| Prova de estreia VERMELHA ... \| Fechamento` | acha; a coluna de entrega se chama `Entrega`, fora do vocabulário de D17 |
| `plano-loop-callbacks` | `Ordem \| Sub-fatia \| ID no TODO \| Nome \| Depende de`, e a entrega em tabelas separadas `Nasce / muda \| O que faz` | **não** (primeira coluna não é `#`; a entrega está em outra tabela) |
| `plano-layers-l4`, `-l4-adendo`, `-l5`, `-l5-adendo-calibracao`, `plano-w6b-fatias-5`, `-5b-revisao`, `-6-8`, `plano-conserto-fachadas-uaf`, `plano-conserto-vermelhos-w6b` | nenhuma tabela de fatia; entrega em prosa ou em tabelas `Caso \| ... \| Mata` | não, por forma |

**Conclusão medida:** das 16, só 5 têm tabela conferível pelo desenho de D17; uma sexta (`w7d`) com vocabulário a mais; as outras 10 ficam "fora por forma". `INF`: um portão que reconhece forma por heurística de cabeçalho vai continuar perseguindo a próxima forma inventada; o problema deixou de ser o regex e passou a ser a falta de um esquema declarado (decisão D-A7).

### 1.C Estado real de cada item aberto, em uma linha

| Item | Estado medido |
|---|---|
| `WL-ACK-SMOKE-BLUNT` | aberto, nada implementado; o que muda está em N5, N6, N7 |
| `PLAN-SCOPE-REGEX-BLIND`, `LINK-PREFIX-SUBSTRING` | ✅; só o texto velho de F3 |
| `LINK-PREFIX-RESIDUOS` | linha nunca registrada (F1); as duas entradas da INBOX que ela absorveria seguem lá (`TODO.md:165-167` e a `SELFTEST-MULTI-...`) |
| `PARITY-ALIAS-HYGIENE` | 🔍; C1-C4 entregues e vistos no servidor (F9); falta a revisão adversarial da P-4 |
| `DOCS-COUNT-VOCAB` | aberto; o universo e a calibração mudaram (F13, F14) |
| `CLAIM-CITATIONS` | aberto; fatos iguais; ganha o analisador léxico de CMake (seção 3.E) |
| `PLAN-SCOPE-COLUMNS` | aberto; o problema mudou de natureza (1.B) |
| `DISPLAY-PASSKEY-CONTROL` | aberto; fatos iguais |
| `CI-VERDE-W7C` | aberto; hoje 26 trabalhos, e a W7-C acrescenta passos (seção 4) |

---

## 2. Pesquisa nova (L-43, L-44), só onde a árvore mudou o problema

O resto da pesquisa do plano (seção 2 de `docs/plano-w7c.md`) continua valendo; não foi refeita.

- **Analisador léxico de CMake (entra em `CLAIM-CITATIONS`, seção 3.E).** Manual oficial, `cmake-language(7)` <https://cmake.org/cmake/help/latest/manual/cmake-language.7.html>: "A `#` not immediately followed by a bracket_open forms a line comment"; "A `#` immediately followed by a bracket_open forms a bracket comment" (`#[[...]]`, `#[=[...]=]`, com o mesmo número de `=` nos dois lados); "No evaluation of the enclosed content ... is performed" dentro de argumento colchete; argumento entre aspas tem sequência de escape e continuação de linha por barra invertida em número ímpar; e "A comment starts with a `#` character that is not inside a Bracket Argument, Quoted Argument, or escaped with `\`". **Lição:** as três leituras de `tests/CMakeLists.txt` que existem hoje (seção 3.E) erram cada uma um pedaço desta gramática; ela é curta e fechada, e se escreve uma vez.
- **Esquema declarado em vez de forma adivinhada (muda `PLAN-SCOPE-COLUMNS`, seção 3.F).** Sphinx-Needs, ferramenta de rastreabilidade de requisito em documentação como código: todo item tem identificador que "must match the regular expression ... needs_id_regex", e "the Sphinx build stops if the ID does not match"; `needs_warnings` define o que nenhum item pode fazer, e `-W` transforma aviso em erro <https://sphinx-needs.readthedocs.io/en/latest/directives/need.html>, <https://sphinx-needs.readthedocs.io/en/latest/configuration.html>. Doorstop, já citado no plano, guarda cada item como dado estruturado, não como prosa. **Lição:** as duas ferramentas maduras exigem um esquema e reprovam quem sai dele; nenhuma tenta reconhecer a forma que cada autor inventou. Medido em 1.B: a heurística de D17 alcança 5 de 16 planos.
- **Ordem das saídas de um programa Python sob `ctest` (entra na linha nova `LAYERS-ORACLE-REPORT`, seção 3.J).** Manual da biblioteca padrão, `sys.stdout` <https://docs.python.org/3/library/sys.html>: "When interactive, the stdout stream is line-buffered. Otherwise, it is block-buffered like regular text files." e "The stderr stream is line-buffered in both cases" (desde a 3.9). Linha de comando <https://docs.python.org/3/using/cmdline.html>: `-u` "Force the stdout and stderr streams to be unbuffered", e `PYTHONUNBUFFERED` não vazio equivale a `-u`. **Lição:** a inversão medida no run `35946636755` é o comportamento documentado, não defeito do oráculo; vale para todo portão Python cuja saída passa por cano (N9), e o conserto é uma variável de ambiente num lugar só.
- **Buscado e não achado:** prior art de relé de protocolo Wayland usado como testemunha de teste para bibliotecas cliente além dos já citados no plano (waypipe, blog do mstoeckl). Nada novo a acrescentar; o desenho de A continua o do plano, com os três ajustes da seção 3.A.
- **RmlUi e SDL3:** nenhum dos dois tem portão de documentação, esquema de plano ou relatório de oráculo; nada a aprender para os itens novos. Declarado, não silenciado.

---

## 3. O que muda, item por item

As tabelas de sub-fatia usam o mesmo cabeçalho do plano (`# | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão`), que é o esquema v1 da decisão D-A7. Nome entre crases em coluna de prova é nome de teste; "—" é ausência declarada com motivo. "Mutante que mata" está dentro da coluna de estreia: é a sabotagem, em cópia fora da árvore (L-27), que a prova tem de derrubar.

### 3.A `WL-ACK-SMOKE-BLUNT`: três ajustes, desenho mantido

O relé estrito (D1 a D5) continua certo; nada na árvore o tornou mais barato ou desnecessário. O que muda:

1. **Onde o relé mora (N7, decisão D-A2).** Pasta `tests/container/wire_relay/`, mantida (um assunto, vários átomos), com uma SEGUNDA listagem explícita em `tests/container/CMakeLists.txt` para `wire_relay/*.cpp`. Não `GLOB_RECURSE`: a pasta `tests/container/_arch_ports_src/` existe no disco (encenação, `.gitignore:85`) com cópias de fontes do produto, e uma listagem recursiva as poria no banco de compilação em duplicata. **Estreia vermelha de graça:** antes de mexer no `CMakeLists`, o `container_smokes_in_compile_db_test` tem de reprovar citando os arquivos do relé.
2. **Subida em dois tempos (N5, D-A3).** `run_compositor.sh` passa a: subir o KWin num nome interno; `compositor_probe` contra esse nome; subir o relé no nome que as fixturas já usam; `compositor_probe` de novo, agora ATRAVÉS do relé. A segunda sonda é a primeira prova viva do modo transparente, e o relé imprime quantas mensagens decodificou dela (zero reprova). O `--selftest` do próprio script (`container_run_compositor_selftest`) ganha dois casos: compositor pronto e relé que nunca aceita, e relé pronto com compositor morto; os dois têm de reprovar.
3. **O fim de conexão não pode mudar de forma (N6, D-A4).** O relé propaga o fim do lado do compositor repassando TODO byte já lido e só então fechando a escrita para o cliente (`shutdown(SHUT_WR)`), nunca fechando antes de esvaziar. A5 roda `fatal_error_smoke` pelo relé e compara a sequência que a própria fixtura imprime (quantas leituras até a trava, com `POLLIN|POLLHUP` na primeira) com a da mesma fixtura direta no compositor; diferente, a fixtura fica fora do relé na lista fechada de A5, com o valor medido dos dois lados como motivo. **A expectativa da fixtura nunca se ajusta ao relé.**

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| A0 | Relé entra no banco de compilação | Wayland | `tests/container/CMakeLists.txt` (segunda listagem, `wire_relay/*.cpp`) | Com os arquivos de A1 rastreados e o `CMakeLists` intocado, `container_smokes_in_compile_db_test` reprova nomeando cada um; mutante que mata: trocar a listagem nova por `GLOB_RECURSE` faz o banco ganhar os arquivos de `_arch_ports_src/`, e o portão tem de reprovar por duplicata (se não reprovar, o furo vai para a INBOX com a medição) | `container_smokes_in_compile_db_test` | — (portão existe só em `if(UNIX)`, exceção `parity_exceptions.txt:718`) | inalterado |
| A3b | Subida em dois tempos | Wayland | `tests/container/run_compositor.sh` (`compositor_probe` antes e depois do relé), seu `--selftest` | Os dois casos novos do `--selftest` reprovam contra o script de hoje por ausência (não existem) e contra um mutante que pule a segunda sonda | `container_run_compositor_selftest` | — (idem, `:656`) | inalterado |
| A5b | Fim de conexão igual com e sem relé | Wayland | `tests/container/wire_relay/` (propagação do fim), `run_compositor.sh` (lista fechada de A5) | Mutante que mata: relé que fecha o cliente sem esvaziar o que leu; a sequência impressa por `fatal_error_smoke` muda (a leitura que drena some) e A5 tem de acusar a diferença | `fatal_error_smoke` | — (`:327`) | inalterado |

A1, A2, A3, A4, A5 e A6 do plano ficam como estão, com A0 antes de A1 entrar na árvore e A3b/A5b dentro de A3/A5. A1 e A2 podem nascer num laboratório FORA da árvore (seção 5), porque só tocam arquivos novos e o autoteste do relé roda com par de soquetes, sem compositor.

**Fecha quando** (acrescido ao do plano): A0 visto vermelho e verde; a sequência de `fatal_error_smoke` impressa pelas duas vias no mesmo relatório; o `--selftest` de `run_compositor.sh` com os casos novos vistos vermelhos.

### 3.B `LINK-PREFIX-RESIDUOS`: a linha nunca nasceu

D6 não foi executada (F1). **Ação do main, antes de despachar B:** registrar a linha logo depois de `LINK-PREFIX-SUBSTRING`, absorvendo `ANCORA-DE-BUILD-CEGA-A-IGUAL-E-ASPAS` e `SELFTEST-MULTI-NAO-DISTINGUE-CAUSA-DA-FALHA`, e trocar o texto velho "aguardando push/CI real" das duas linhas ✅ pela citação de `92d85a2` (F3). B1 e B2 do plano ficam como estão; `tests/tools/check_container_fixture_link.py` não mudou desde `da1e325` (`git log da1e325..HEAD -- tests/tools/check_container_fixture_link.py` vazio).

### 3.C `PARITY-ALIAS-HYGIENE`: o que o plano pedia está entregue

C1, C2, C3 e C4 estão na árvore (N1) e o servidor imprimiu as contagens (F9). **Falta só:** a revisão adversarial da P-4 (`78390d3`), que o próprio item pede, com a sabotagem de família diferente exigida pela emenda de 22/09 à L-34. **Fecha quando:** o relatório da revisão da P-4 está em disco, com os mutantes que ela executou; e a primeira execução do servidor DEPOIS de `c0fb96f` imprime "0 morta(s)" e nenhum erro de "concluido sem par" (é a primeira vez que a regra de morte por item concluído mordeu um caso real, N2, e a troca de dono precisa ser vista verde no servidor, não lida).

Nada novo entra neste item: o que N2 e N3 revelaram é outro defeito (o portão só morde no servidor) e vira linha própria (3.I), para C não ser reaberto depois de quatro sub-fatias.

### 3.D `DOCS-COUNT-VOCAB`: universo mantido, calibração refeita

**O que mudou:** F14 estava errado (10 frases, não 4), e F13 revelou que 34 dos 57 arquivos do universo são `.txt` de código ou dado. **D11 continua de pé** (reusar a enumeração do portão de travessão, nunca copiá-la): o universo ser mais largo que "documento ao consumidor" deixa o portão mais estrito, e "`43 of 53` apply to a Windows job" num comentário de `CMakeLists` apodrece do mesmo jeito que num `README` (o leitor é quem mantém o projeto). **Muda o vocabulário e a calibração (D-A9):**

- A forma `N of M` só é contagem quando `M` NÃO é seguido de `/`: "achado 2 of 26/08/2026" é data escrita com "of", e é o que produziu três falsos positivos em `tests/CMakeLists.txt` (`:195`, `:793`, `:802`). Esta regra se fixa ANTES de ligar o portão (L-43 do projeto) e tem controle próprio.
- **Nenhuma isenção nova.** Saída de ferramenta entre aspas (`tests/package/CMakeLists.txt:31`) se resolve passando o trecho para crases, que já é isenção; parâmetro de desenho copiado ("`100 consecutive pairs`", `docs/gl-loop-portability-matrix.md:46`) é cópia de uma constante que tem dono no teste (`time_test`) e se reescreve citando o teste, sem o número. Criar isenção "entre aspas" ou "parâmetro" abriria o furo que o portão existe para fechar.
- **O que D3 tem de resolver, medido hoje:** quatro frases vivas (`tests/CMakeLists.txt:2773`, `tests/package/CMakeLists.txt:31`, `docs/gl-loop-portability-matrix.md:46`, `tests/parity_exceptions.txt:636`); duas isentas por histórico (`CHANGELOG.md:160`, `:170`, dentro de `## [0.2.0.0]`, abaixo de `## [Unreleased]` em `:9`); uma isenta por citação (`docs/gl-loop-portability-matrix.md:38`, run `34975391524` e SHA `14427bc` no mesmo período); três que a regra da data tira. **Remedir na hora; estes números são deste adendo, não do portão.**

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| D2b | A regra da data no vocabulário | comum | `tests/tools/check_docs_count_vocab.py` (novo em D2) | Controle: "achado 2 of 26/08/2026" NÃO é contagem, "43 of 53 apply" É; mutante que mata: tirar a guarda do `/` faz o primeiro controle reprovar | `docs_count_vocab_selftest` | `docs_count_vocab_selftest` | mesmo nome |

D1, D2 e D3 do plano ficam; D3 passa a resolver as quatro vivas acima.

### 3.E `CLAIM-CITATIONS`: ganha o analisador léxico de CMake e absorve `WIN-CROSS-GATE-CMAKE-LEXER`

**Fato novo, enumerado (L-17):** três portões leem `tests/CMakeLists.txt` atrás de registro de teste, cada um com o próprio corte de comentário, e nenhum segue a gramática do manual (seção 2):

| Leitor | Onde | O que erra | Direção do erro |
|---|---|---|---|
| `extract_add_test_blocks` | `check_selftest_orphan.py:234` | não pula comentário nenhum; casa `add_test(` em qualquer lugar, inclusive dentro de `glintfx_add_test(` e de prosa | **verde falso possível**: um `add_test(... tools/x.py --selftest)` dentro de comentário conta como registro do autoteste. Hoje: 9 menções em comentário, todas `add_test()` vazias (`grep -nE '^\s*#.*add_test\(' tests/CMakeLists.txt \| grep -vcE 'add_test\(\)'` = 0), então inofensivo por acaso |
| `_strip_cmake_comments_from_text` | `check_win32_test_link.py:136` | corta no primeiro `#` da linha, sem ver aspas; não conhece comentário de colchete | vermelho falso (a própria INBOX mediu 0 casos hoje) |
| `strip_cmake_comments` + regex `^\s*(add_executable\|glintfx_add_test)\(` | `check_facade_export_boundary.py:172-204` | o mesmo corte ingênuo; linha de dentro de `#[[ ]]` que comece por `glintfx_add_test(` vira alvo | vermelho falso ou bloco engolido |

A regra de três (L-33) está cumprida por medição, não por previsão. **Decisão D-A8:** um átomo só, `tests/tools/cmake_lexer.py`, que devolve o texto com comentários (de linha e de colchete) apagados preservando o número de linha, e conhece argumento entre aspas (com escape e continuação) e argumento de colchete; `test_name_inventory.py` (E1) o consome, e os três leitores acima passam a consumi-lo. `WIN-CROSS-GATE-CMAKE-LEXER` sai da INBOX absorvida aqui.

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| E0 | Analisador léxico de CMake | comum | `tests/tools/cmake_lexer.py` (novo), `tests/CMakeLists.txt` (`cmake_lexer_selftest`) | Autoteste com `#[[ add_test(NAME fantasma) ]]`, `#[=[ ... ]=]` contendo `]]`, `message("glintfx_add_test(x) # nao e comentario")`, aspas com `\"` e continuação por barra: cada um tem saída esperada escrita antes; texto vazio reprova (L-40). Mutantes que matam: tratar `#[[` como comentário de linha; ignorar o número de `=`; tratar `#` dentro de aspas como comentário | `cmake_lexer_selftest` | `cmake_lexer_selftest` | mesmo nome |
| E1 | Inventário de nomes de teste | comum | `tests/tools/test_name_inventory.py` (novo, consome E0) | O do plano, mais: registro dentro de `#[[ ]]` NÃO entra no inventário | `claim_citations_selftest` | mesmo | mesmo |
| E1b | `check_selftest_orphan.py` consome E0 | comum | `tests/tools/check_selftest_orphan.py` (`extract_add_test_blocks`) | Controle novo: registro do autoteste SÓ dentro de comentário tem de reprovar como órfão; contra o código de hoje, passa (é o verde falso da tabela acima) | `check_selftest_orphan_selftest` (`tests/CMakeLists.txt:5304`) | mesmo | mesmo |
| E1c | `check_win32_test_link.py` e `check_facade_export_boundary.py` consomem E0 | comum | os dois arquivos (só o corte de comentário e a contagem bruta) | O caso da INBOX (`glintfx_add_test(` dentro de `message(...)` e de `#[[ ]]`) plantado no autoteste de cada um: hoje vira alvo fantasma; depois, não. **Regra do escoteiro (D-A10):** toda função das 16 de `WIN-CROSS-GATE-L17` que E1c tocar sai dentro dos tetos da L-17 no mesmo commit; as não tocadas ficam na INBOX | autotestes dos dois portões | mesmo | mesmo |

E2, E3 e E4 do plano ficam como estão. **Fecha quando** (acrescido): os três leitores importam `cmake_lexer`, e `grep -nE 'def _strip_cmake_comment|find\("#"\)' tests/tools/*.py` não acha mais corte de comentário de CMake fora dele (o comando é o piso: contar zero só vale se o mesmo comando, rodado antes, contou os três).

### 3.F `PLAN-SCOPE-COLUMNS`: esquema declarado em vez de forma adivinhada

**O que mudou:** 1.B. D17 reconhecia tabela por heurística de cabeçalho; medido, alcança 5 de 16 planos e já perdeu o `w7d` pelo nome de uma coluna. **Decisão D-A7 (substitui a parte de reconhecimento de D17; o resto de D17 fica):**

- **Esquema v1, por NOME de coluna, lista fechada:** obrigatórias `#`, `Fatia` (ou `Sub-fatia`), `Nasce / muda`, ao menos uma que comece por `Prova`, e `Par no portão`; opcionais `Lado` e a que comece por `Teste vermelho de estreia`. É a família que quatro planos já usam (`plano-w6a-janela.md:96`, `plano-w6b-placa-e-laco.md:117`, `plano-w6-folha-de-estilo.md:122`, `plano-w7c.md:105`), não uma forma nova.
- **Lista de legado fechada, que só encolhe:** `tests/plan_scope_legacy.txt` nasce com os planos de hoje que não têm tabela no esquema. Plano fora da lista sem tabela v1 reprova. Plano NA lista que passou a ter tabela v1 reprova como "legado morto" (a mesma regra de morte da paridade). A saída abre dizendo quantos planos conferiu, quantos são legado e quais.
- **Cada linha de legado tem de dizer a verdade, conferida por máquina (emenda do ataque de 24/09, `/var/tmp/glintfx-plan/ataque-w7c.md`).** É a doença que `PARITY-ALIAS-HYGIENE` pagou quatro sub-fatias para curar: a declaração `bilateral=<motivo>` passava calada até o motivo ser conferido. Forma da linha: `docs/plano-X.md|<categoria>|<motivo>`.
  - **Categoria, lista fechada, CONFERIDA contra o próprio plano:** `sem-tabela-de-fatia` (o plano não tem nenhuma tabela com coluna chamada `Fatia` ou `Sub-fatia`, em qualquer posição) ou `tabela-fora-do-esquema` (tem ao menos uma dessas tabelas, e nenhuma cumpre o v1; a saída imprime, por tabela, as colunas obrigatórias que faltam). Categoria que o conteúdo do plano desmente reprova como "legado com motivo falso", com o número da linha. Categoria fora da lista reprova. **Esta é a metade forte:** o que a máquina garante.
  - **Motivo, a metade fraca, com piso medível.** Reprova como **vazio** o campo sem nenhum caractere além de espaço. Reprova como **trivial**, e a definição se fixa aqui, antes do dado: (a) menos de cinco palavras depois de tirar pontuação (palavra = sequência de letras ou dígitos); ou (b) o motivo inteiro, sem acento, sem pontuação e em minúsculas, igual a um item da lista fechada de marcadores `legado`, `antigo`, `historico`, `n/a`, `na`, `todo`, `idem`, `ver acima`, `mesmo motivo`, `sem motivo`, `-`; ou (c) motivo idêntico, depois da mesma normalização, ao de outra linha do arquivo (cópia). O cabeçalho do arquivo e o do portão dizem o que isto NÃO pega: motivo de cinco palavras bem formado e falso passa; quem responde por ele é a categoria conferida.
- **`plano-w7d.md` se converte, não entra no legado:** a W7-D ainda não começou, e o plano dela vai ser conferido por máquina quando rodar.
- O identificador segue sendo a primeira célula, qualquer forma; `~~X~~` continua retirada contada.

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| F0 | Esquema v1 e lista de legado | comum | `tests/tools/check_plan_scope_diff.py`, `tests/plan_scope_legacy.txt` (novo), `docs/plano-w7d.md` (tabelas convertidas) | Autoteste: plano novo sem tabela v1 reprova; plano de legado que ganhou tabela v1 reprova como legado morto; tabela com `Entrega` no lugar de `Nasce / muda` reprova nomeando a coluna que falta; linha de legado com motivo vazio reprova; com motivo de quatro palavras reprova; com motivo `legado` reprova; duas linhas com o mesmo motivo reprovam; categoria `sem-tabela-de-fatia` num plano que tem tabela com coluna `Fatia` reprova como motivo falso; um motivo de cinco palavras e categoria verdadeira passa (controle positivo). Mutantes que matam: aceitar qualquer coluna que contenha "Prova" no meio do nome; trocar o piso de cinco palavras por um; pular a conferência da categoria contra o plano; comparar motivos sem normalizar acento e caixa | `plan_scope_diff_selftest` | mesmo | mesmo |

F1 do plano muda de "reconhecer pelo cabeçalho" para "reconhecer pelo esquema v1"; F2, F3 e F4 ficam. Este adendo é ele mesmo um `docs/plano-*.md`: as tabelas de fatia dele estão no esquema v1 e entram na primeira execução de F4.

### 3.G `DISPLAY-PASSKEY-CONTROL`: sem mudança

F21 a F23 valem. D18 fica (tipo incompleto para o consumidor, nos dois modos). A única coisa nova é de ordem: G continua por último.

### 3.I `PARITY-LOCAL-MIRROR` (linha NOVA, D-A5)

**Por quê, com fato:** N2 e N3. Duas vezes em dois dias a regra "exceção de item concluído reprova" só mordeu no servidor, e uma delas quase reprovou o fechamento de uma onda. O espelho local diz que espelha o servidor e não roda nenhum dos dois portões de paridade contra dado real. **Metade do que esses portões conferem não precisa de inventário nenhum:** a forma de cada linha, o item de cada exceção contra o `TODO.md`, o `SEM-PENDENCIA` com `gemeo=nenhum`, o quinto campo `PROVA-PARCIAL=`, a forma do apelido e da declaração `bilateral=`. Essa metade roda igual em qualquer máquina.

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| I1 | Modo textual do portão de paridade de teste | comum | `tests/tools/check_test_parity.py` (modo `--textual`, reusa `parse_exceptions_text`, `validate_exceptions`, `parse_todo_status_text` sem copiar), `tests/CMakeLists.txt` (`parity_textual_test`, sem guarda de sistema) | **Estreia contra o caso real:** o modo textual, rodado sobre `git show 023e1c7:tests/parity_exceptions.txt` e `git show 023e1c7:TODO.md`, tem de reprovar citando `gpu_kind_report_smoke` e `WIN-RUNNER-PROPRIO`; sobre a árvore depois de `c0fb96f`, passa. Mutante que mata: pular `validate_exceptions` no modo textual | `parity_textual_test` | `parity_textual_test` | mesmo nome |
| I2 | O gêmeo: modo textual do portão de medições | comum | `tests/tools/check_measured_parity.py`, `tests/CMakeLists.txt` (`measured_parity_textual_test`) | Cópia de `tests/measured_exceptions.txt` com uma linha apontando para item ✅ reprova; mutante que mata: o mesmo de I1, no outro arquivo | `measured_parity_textual_test` | mesmo | mesmo |

**Perda declarada, no cabeçalho dos dois modos:** lacuna de INVENTÁRIO (teste que existe num sistema e não no outro sem exceção, o caso de `eab26db`) continua só no servidor, porque o inventário do Windows não existe nesta máquina, e prever o inventário lendo o `CMakeLists` é a armadilha já medida ("`ctest -N` mede só metade"). **Fecha quando:** os dois testes rodam no `ctest` dos dois sistemas no servidor (nome visto na lista de testes executados de um trabalho Linux e de um Windows), e a estreia contra os blobs de `023e1c7` está registrada com a saída literal e o código de saída lido de variável. Absorve a INBOX `PRECI-LOCAL-NAO-RODA-CHECK-TEST-PARITY`.

### 3.J `LAYERS-ORACLE-REPORT` (linha NOVA, D-A11)

**Por quê, com fato:** N8 e N9. O passo do CI que existe para acusar "o oráculo não rodou" passa com as linhas do autoteste; a saída chega fora de ordem; e o tempo que o adendo de calibração (§5.1) promete no relatório nunca foi impresso (INBOX `LAYERS-ORACULO-SEM-TEMPO-IMPRESSO`). É exatamente a família desta onda: instrumento que afirma medir e não mede. Absorve as três entradas da INBOX (`...-RELATORIO-MISTURA-SELFTEST`, `...-STDERR-ANTES-DO-STDOUT`, `...-SEM-TEMPO-IMPRESSO`).

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| J1 | Extração do relatório num roteiro só, por bloco de teste | comum | `tests/tools/extract_oracle_report.py` (novo: lê o `LastTest.log`, separa por bloco de teste do próprio `ctest` e devolve só o do `layers_oracle_test`; bloco ausente ou vazio reprova), `.github/workflows/ci.yml` (as três cópias, `:644`, `:3247`, `:3401`, viram chamada ao roteiro), `tests/CMakeLists.txt` (`extract_oracle_report_selftest`) | **Controle que reproduz o furo:** registro de teste falso contendo SÓ o bloco do `layers_oracle_selftest`; o `grep` de hoje, aplicado a ele, devolve linhas (medir e registrar); o roteiro novo reprova. Mutante que mata: casar pelo prefixo `check_layers_oracle.py:` em vez de pelo bloco | `extract_oracle_report_selftest` | mesmo | mesmo |
| J2 | Saída sem buffer em todo portão Python | comum | `tests/CMakeLists.txt` (no fim do arquivo, `PYTHONUNBUFFERED=1` acrescentado à propriedade `ENVIRONMENT` de todos os testes do diretório, lidos da propriedade `TESTS`), `.github/workflows/ci.yml` (`env:` do fluxo) | Portão novo pequeno sobre `ctest --show-only=json-v1`: todo teste registrado carrega a variável; zero testes lidos reprova. Mutante que mata: acrescentar só aos testes cujo comando contém `python` (um teste que chama o Python por roteiro de shell fica de fora e o portão tem de acusar) | `python_unbuffered_test` | mesmo | mesmo |
| J3 | Tempo no relatório do oráculo | comum | `tests/tools/check_layers_oracle.py` (tempo de parede medido e impresso, sempre) | Só o servidor prova (L-45). Controle local no autoteste do oráculo: a linha de tempo existe e tem a forma fixada antes; mutante que mata: imprimir só quando reprova | `layers_oracle_selftest` | mesmo | mesmo |

**Fecha quando:** no servidor, o relatório de cada um dos seis trabalhos com o oráculo ligado contém só o bloco do caso real, em ordem, com a linha de tempo; e a estreia de J1 (o `grep` antigo aceitando o autoteste sozinho) está registrada.

---

## 4. Definição de fechamento da ONDA: o que muda na seção 4 do plano

Escrita antes de qualquer dado desta onda existir (L-43). Os oito critérios do plano ficam; mudam ou se acrescentam:

1. **Critério 1** passa a incluir as linhas novas `LINK-PREFIX-RESIDUOS`, `PARITY-LOCAL-MIRROR` e `LAYERS-ORACLE-REPORT`.
2. **Critério 5:** a contagem de trabalhos não se lê do plano ("hoje 25") nem da linha `CI-VERDE-W7C` ("hoje 22 de 22", texto velho): mede-se na execução de fechamento; em `35944510792` eram 26, com um pulado por desenho. A lista de trabalhos comparada com `ci.yml` tem de mostrar, rodando de fato: o autoteste do relé nas duas pernas do `wayland-container`; `parity_textual_test` e `measured_parity_textual_test` na lista de testes executados de um trabalho Linux E de um Windows (L-04); o roteiro de J1 nos seis trabalhos com oráculo.
3. **Novo 9:** a primeira execução do servidor depois de `c0fb96f` imprime zero erro de "concluido sem par" no passo de paridade (fecha o N2 por leitura do servidor, não do commit).
4. **Novo 10:** `docs/plano-w7c.md` e este adendo passam pelo portão de escopo de plano (F4) no esquema v1; o que ele achar ausente vira declaração com item aberto ou prova de entrega, antes do fechamento.
5. **Novo 11, contra a família desta onda:** para cada portão novo ou alterado, o relatório de fechamento traz a linha "o que ele NÃO vê", copiada do cabeçalho dele; portão sem essa linha não fecha.

---

## 5. Ordem de execução e paralelismo real

**Restrição de hoje, do pedido:** um implementador por vez na árvore; laboratório fora da árvore permitido. Mais o teto de quatro agentes vivos e um trabalho pesado por vez (L-11 global). Consequência: não há "duas pistas na árvore" como o plano previa (seção 8 dele); há uma fila na árvore e, em paralelo, um laboratório e revisores que só leem.

**Fila na árvore (um implementador por vez, nesta ordem):**

| Ordem | O quê | Por que nesta posição |
|---|---|---|
| 0 | **main**, só `TODO.md`: registrar `LINK-PREFIX-RESIDUOS`, `PARITY-LOCAL-MIRROR`, `LAYERS-ORACLE-REPORT`; trocar o texto velho de F3; tirar a entrada resolvida de F26; tirar das INBOX as entradas absorvidas (seção 7); corrigir na linha `WIN-CROSS-TESTS-LINK` a reconciliação para 104 + 14 + 9 = 127 e apagar "conferidas por `grep` independente ... batendo exatamente" (N10) | nada se despacha para linha que não existe |
| 1 | revisão adversarial da P-4 (só lê; muta em cópia fora da árvore) e fechamento de `PARITY-ALIAS-HYGIENE` | libera `check_test_parity.py` para I1 |
| 2 | `PARITY-LOCAL-MIRROR` (I1, I2) | protege cada commit de fechamento de fatia desta onda contra a mesma queda de N2, porque o `preci.sh --fast` passa a rodar os dois modos textuais |
| 3 | `LAYERS-ORACLE-REPORT` (J1, J2, J3) | J1 fecha um verde falso num portão que o líder mandou ser observável (D-L5h); a prova de J3 só existe no servidor, então quanto antes entrar, antes a primeira rodada da onda a exercita |
| 4 | `LINK-PREFIX-RESIDUOS` (B1, B2) | pequeno, arquivo isolado |
| 5 | `CLAIM-CITATIONS` E0, E1, E1b, E1c | E0/E1 são a fundação de D2 (o inventário não, mas a gramática de citação usa nome de teste) e de F2 |
| 6 | `DOCS-COUNT-VOCAB` (D1, D2, D2b, D3) | usa `citation_grammar.py`, que E3 depois estende |
| 7 | `CLAIM-CITATIONS` E2, E3, E4 | estende D1 |
| 8 | `PLAN-SCOPE-COLUMNS` (F0 a F4) | consome E1; confere este adendo e o plano |
| 9 | `WL-ACK-SMOKE-BLUNT` A0, A3 (com A3b), A4, A5 (com A5b), A6 | trabalho pesado em container, um de cada vez, com `watchcode` armado (L-25 do projeto); A6 mexe em `tests/parity_exceptions.txt`, que I1 já terá tornado conferível localmente |
| 10 | `DISPLAY-PASSKEY-CONTROL` (G1 a G4) | colide com E2 (cabeçalhos públicos) e com A (fixturas do container); último por construção |
| 11 | `CI-VERDE-W7C` | sempre a última |

**Em paralelo, fora da árvore:**

- **Laboratório do relé (A1, A2):** um implementador numa pasta fora da árvore RASTREADA: subpasta do projeto ignorada pelo git, como a L-55 global manda para cópia de trabalho (o `.gitignore` já tem essa forma para `tests/container/_arch_ports_src/`, `.gitignore:85`); o main escolhe o nome e confere com `git check-ignore -v` antes de despachar, só com arquivos novos e o autoteste por par de soquetes, sem compositor e sem container. Entra na árvore na posição 9, por cópia conferida por `md5` nas duas pontas (L-13 global).
- **Revisores:** só leem a árvore e mutam em cópia fora dela; um por fatia fechada, nunca o mesmo agente que implementou (L-12).
- **Pesado:** nenhum `preci.sh` completo, construção de imagem ou sanitizador enquanto outro estiver rodando, inclusive entre a fila e o laboratório.

---

## 6. Decisões novas, no formato de `DECISOES_AUTONOMAS.md`

Para o main registrar ao vivo. Quem decidiu: Caetano (CTO, `opus`, esforço alto), em modo autônomo. Nenhuma muda o que a biblioteca aceita ou entrega: todas são do instrumento de teste e da tabela. Todas: confirmar retroativamente.

**D-A1: a linha `LINK-PREFIX-RESIDUOS` é registrada agora, antes de despachar.**
Pergunta que teria ido ao líder: "a D6 do plano nunca foi executada; registramos a linha ou deixamos os resíduos na INBOX?" Opções: deixar na INBOX; **registrar agora (escolhida)**. Porquê: a D6 já foi decidida e ratificável; o que faltou foi execução, não decisão. Mão única: não. Reverter: barato. Fonte: F1 (medição).

**D-A2: o relé mora em `tests/container/wire_relay/` e entra no banco de compilação por listagem explícita, não recursiva.**
Opções: arquivos soltos em `tests/container/`; `GLOB_RECURSE`; **segunda listagem explícita (escolhida)**. Porquê: `GLOB_RECURSE` traria a pasta de encenação `_arch_ports_src/` (cópias de fontes do produto) em duplicata; arquivos soltos misturam o relé com as fixturas. Mão única: não. Reverter: barato. Fontes: N7 (medição do `pathspec` e da listagem); nenhuma externa necessária, a pergunta é da casa.

**D-A3: a subida do compositor passa a ter duas sondas, antes e através do relé.**
Opções: sonda só no relé; **as duas (escolhida)**. Porquê: sonda só no relé não distingue "compositor não subiu" de "relé quebrado"; a segunda sonda é a primeira prova viva do modo transparente. Mão única: não. Reverter: barato. Fonte: N5 (o desenho de prontidão que CONT-WARMUP já provou).

**D-A4: o fim de conexão tem de sair igual com e sem relé; se não sair, a fixtura fica fora do relé, nunca ajustada.**
Opções: aceitar a sequência nova; **exigir a mesma, com saída por lista fechada (escolhida)**. Porquê: `fatal_error_smoke` mede um comportamento do núcleo que o produto precisa tratar; um relé que o altere faria a fixtura medir o relé. Mão única: não. Reverter: barato. Fonte: N6.

**D-A5: linha nova `PARITY-LOCAL-MIRROR`, com modo textual dos dois portões de paridade no `ctest` dos dois sistemas.**
Pergunta: "o portão de paridade só morde no servidor; aceitamos, rodamos o portão inteiro local, ou rodamos localmente a metade que não depende de inventário?" Opções: aceitar; prever o inventário do Windows lendo o `CMakeLists`; **metade textual, com a outra metade declarada como perda (escolhida)**. Porquê: duas quedas medidas em dois dias (N2, N3); a previsão de inventário já foi medida enganosa ("`ctest -N` mede só metade"); a metade textual é idêntica em qualquer máquina. Mão única: não. Reverter: barato. Fontes: N2, N3, N4; ESLint e Rust (seção 2.C do plano: aviso é ignorado, reprovar é o remédio maduro) valem aqui também.

**D-A6: `PARITY-ALIAS-HYGIENE` fecha com a revisão da P-4, sem sub-fatia nova.**
Opções: acrescentar o modo local ao próprio item; **fechar e abrir linha própria (escolhida)**. Porquê: o item já passou por quatro sub-fatias e seus critérios estão cumpridos; o defeito novo é de outra natureza. Mão única: não. Reverter: barato.

**D-A7: o portão de escopo de plano confere um esquema v1 declarado por nome de coluna, com lista de legado fechada que só encolhe; substitui o reconhecimento heurístico de D17.**
Pergunta: "reconhecemos cada forma de tabela que os planos inventaram, reescrevemos os planos antigos, ou fixamos um esquema?" Opções: heurística (D17); reescrever os 16 planos; **esquema v1 + legado fechado + converter só o plano ainda não executado (`w7d`) (escolhida)**. Porquê: medido, a heurística alcança 5 de 16 e já perdeu um pela troca de uma palavra; plano executado é registro datado e não se reescreve; Sphinx-Needs e Doorstop mostram que a ferramenta madura exige esquema e reprova quem sai dele. Mão única: não. Reverter: barato. Fontes: 1.B; seção 2. **Emenda de 24/09 (ataque independente, achado IMPORTANTE):** cada linha da lista de legado traz categoria de lista fechada conferida contra o conteúdo do plano (`sem-tabela-de-fatia`, `tabela-fora-do-esquema`) e motivo com piso medível (vazio, menos de cinco palavras, marcador de lista fechada ou cópia de outra linha reprovam). Porquê: declaração em prosa não conferida é exatamente o defeito que `PARITY-ALIAS-HYGIENE` corrigiu em `bilateral=<motivo>`; a categoria conferida é a metade que a máquina garante, e o piso do motivo só barra o preenchimento vazio, declarado assim no cabeçalho.

**D-A8: um analisador léxico de CMake só (`tests/tools/cmake_lexer.py`), consumido pelo inventário novo e pelos três leitores que existem; absorve `WIN-CROSS-GATE-CMAKE-LEXER`.**
Opções: consertar só o leitor da INBOX; **átomo único e migração dos três (escolhida)**. Porquê: três leitores, três cortes de comentário diferentes, um deles com direção de verde falso (3.E); a regra de três está cumprida por medição. Mão única: não. Reverter: médio (três arquivos voltam ao corte próprio). Fonte: `cmake-language(7)`.

**D-A9: o portão de contagem mantém o universo do portão de travessão, ganha a regra da data e nenhuma isenção nova.**
Opções: estreitar o universo para `.md`; isentar texto entre aspas e parâmetro de desenho; **manter o universo, só a regra da data (escolhida)**. Porquê: estreitar afrouxa um portão; isenção por aspas é furo; a regra da data corrige um falso positivo medido sem abrir nada. Mão única: não. Reverter: barato. Fonte: F13, F14 (medição); Vale `existence` (seção 2.D do plano: vocabulário nomeado, exceção explícita).

**D-A10: `WIN-CROSS-GATE-L17` não entra na onda; a função das 16 que E1c tocar sai dentro dos tetos no mesmo commit.**
Opções: refatorar as 16 na W7-C; **regra do escoteiro só no que for tocado (escolhida)**; nada. Porquê: é débito de forma, sem defeito de medição (não é a família desta onda), num portão cujo autoteste mais pesado precisa do compilador real; a L-17 cobra a unidade tocada na revisão da fatia. Mão única: não. Reverter: barato. **Recomendação ao main, não decisão:** juntar `WIN-CROSS-GATE-L17` e `PARITY-GATE-DEBITO-L17-PRE-EXISTENTE` numa linha única de débito de forma dos portões, quando a tabela abrir espaço (congelamento de 27/08 até `DEMO-1`).

**D-A11: linha nova `LAYERS-ORACLE-REPORT`, absorvendo três entradas da INBOX, com o conserto do buffer para todo portão Python.**
Opções: consertar só o oráculo; deixar na INBOX; **linha própria, com o gêmeo de N9 incluído (escolhida)**. Porquê: J1 é verde falso no passo que existe para acusar ausência (N8); o buffer é o comportamento documentado do Python e atinge todos os portões (N9, L-17). Mão única: não. Reverter: barato. Fontes: `sys.stdout` e `-u` (seção 2); run `35946636755` (medido pela INBOX).

**D-A12: `LAYERS-ORACULO-MSVC-CR-SOLITARIO` e `CENSO-STDLIB-GXX14-ACIMA-DO-PISO` ficam fora da W7-C.**
Porquê: o primeiro é pergunta de desenho do portão de camadas com erro na direção segura: nos casos `M3` e `M15` (`check_layers.py:2891`, `:2964`), o portão reprova um `#include` que o MSVC nunca vê, porque ali o `\r` sozinho não quebra linha; é vermelho a mais no MSVC, nunca verde falso (`INF`, lido no código, não executado). O segundo é fato registrado "sem ação pendente" pela própria entrada. Mão única: não.

**D-A13: ordem da onda com um implementador por vez na árvore (seção 5).**
Opções: as duas pistas do plano; **fila única com laboratório do relé fora da árvore (escolhida)**. Porquê: é a restrição do pedido, e as colisões de arquivo medidas no plano (seção 8 dele) continuam valendo. Mão única: não. Reverter: barato.

---

## 7. INBOX desta noite: o que entra e o que não entra

| Entrada | Destino | Motivo |
|---|---|---|
| `WIN-CROSS-GATE-CMAKE-LEXER` | **entra**, absorvida em `CLAIM-CITATIONS` E0/E1c (D-A8) | mesma pergunta de três portões; a gramática do manual fecha as três |
| `WIN-CROSS-GATE-L17` | **não entra** (D-A10) | débito de forma, não medição falsa; escoteiro no que for tocado |
| `LAYERS-ORACULO-RELATORIO-MISTURA-SELFTEST` | **entra**, `LAYERS-ORACLE-REPORT` J1 (D-A11) | verde falso real no passo de ausência (N8) |
| `LAYERS-ORACULO-STDERR-ANTES-DO-STDOUT` | **entra**, J2, ampliada a todo portão Python | comportamento documentado, gêmeo em todos (N9) |
| `LAYERS-ORACULO-SEM-TEMPO-IMPRESSO` | **entra**, J3 | promessa do adendo de calibração não entregue |
| `LAYERS-ORACULO-MSVC-CR-SOLITARIO` | **não entra** (D-A12) | erro na direção segura; pergunta de desenho do portão de camadas |
| `CENSO-STDLIB-GXX14-ACIMA-DO-PISO` | **não entra** (D-A12) | fato sem ação |
| `PRECI-LOCAL-NAO-RODA-CHECK-TEST-PARITY` (não pedida, mas do tema e com segunda ocorrência hoje) | **entra**, `PARITY-LOCAL-MIRROR` (D-A5) | N2, N3 |

As absorções do plano (seção 7 dele) continuam: `REGEX-DE-FATIA-CEGO-A-MAIUSCULA` e `PLAN-SCOPE-SILENT-DEFAULT` em `PLAN-SCOPE-COLUMNS`; `PARIDADE-EXCECAO-SEM-FORMA-PARA-PROVA-PARCIAL` já foi resolvida por C4 e sai da INBOX; `ANCORA-DE-BUILD-CEGA-A-IGUAL-E-ASPAS` e `SELFTEST-MULTI-NAO-DISTINGUE-CAUSA-DA-FALHA` em `LINK-PREFIX-RESIDUOS`; a entrada de F26 sai por estar resolvida.

---

## 8. Ataque a este adendo

1. **O modo textual pode dar falsa segurança** ("passou local, logo a paridade passa"). Contenção: o cabeçalho e a saída dos dois modos dizem, sempre, que lacuna de inventário não é medida ali.
2. **J2 pode mudar a saída de algum portão que alguém lê por posição** (o relatório do oráculo, o `MEASURED`). Contenção: J2 vem depois de J1, que já lê por bloco; qualquer leitor por posição que quebrar é defeito de leitor, achado, não escondido.
3. **O esquema v1 pode travar a escrita de planos futuros.** Contenção: o esquema é a forma que quatro planos já usam; plano sem fatia (um adendo de calibração, por exemplo) entra no legado com motivo, e a lista só encolhe por conversão, nunca cresce sem commit que o diga.
4. **E1c mexe em três portões que funcionam.** Contenção: cada um ganha o controle do caso fantasma ANTES da troca, e a troca não pode mudar a contagem real sobre `tests/CMakeLists.txt` (medida antes e depois, impressa).
5. **Planejador e orquestrador são da mesma família** (L-34, emenda de 22/09): este adendo pede, como o plano, sabotagem de família diferente na verificação do main.
