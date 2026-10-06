<!-- Origem: auditoria-revisao-r2d-batch-final.md, md5 9a3500ba0b0918886a1749bf9c02bdc5. Corpo copiado byte a byte; so este cabecalho, o texto da secao 3 (sanitizer, B7-c) e a nota sobre o B7-K3 foram acrescentados (D-FECH-3). -->

# Revisão adversarial que executa: `R2D-BATCH` (B7 da W7-D)

**Quem:** o agente `api-review`, distinto do implementador (`impl-da14`) e do CTO que planejou (L-12 do projeto). **Pedido:** team-lead, 30/09/26 15:33. **Escopo:** `/var/tmp/cto-w7d/PLANO.md` (md5 `bd73502a64fa2b4c41d338ce4c09de1e`), linha B7 e "Fecha quando" da §4.B, e a errata `/var/tmp/cto-w7d/PLANO-errata.md` até o §27 (md5 `9306487e54d3cc4df78f4f005c8c328b`).

**O que foi revisado (rodada final):** o commit `62b78658a5af7e7f23438951ac9385b9d2e75b8d` (`62b7865`), extraído por `git archive` para `/var/tmp/cto-w7d/b7/f/src0` (intocada). Nenhuma mutação tocou a árvore do projeto (L-27 global). As mutações rodaram numa segunda cópia (`/var/tmp/cto-w7d/b7/f/mut`), restaurada da base a cada mutante e conferida igual à base no fim (`diff -rq`). A rodada anterior, sobre `c25cc08` (antes da refatoração L-17 `2c02b56`..`d9a9f90`), fica citada só onde a comparação ajuda; os números dela foram substituídos pelos desta rodada. Plano de origem: `/var/tmp/cto-w7d/plano-fechamento-w7d.md` (md5 `de6cbfd9651725ca4125d8885c99ca29`), §2.1, fatias B7-a e B7-b. Toda execução foi contida (`systemd-run --user --scope -p TasksMax=...`).

**Leis aplicadas:**
- L-12: revisor distinto; a revisão executa.
- L-17: cinco perguntas por unidade, e os números duros.
- L-20 e L-40: um mutante que sobrevive é lacuna de teste; varredura vazia reprova.
- L-27 global: mutação só em cópia, e verificação pelo blob.
- L-11 global e L-25 do projeto: um trabalho pesado por vez, com a janela pedida ao team-lead.
- L-09: nada abriu janela nem tocou GPU ou entrada nesta máquina.

---

## 1. Veredito

**APROVA COM RESSALVAS** (sem CRÍTICO e sem IMPORTANTE abertos), sobre `62b7865`.

- **B7-a:** 27 de 27 mutantes mortos (18 reancorados + 8 da B4 + `m_cap`), todos por teste unitário da camada `draw2d`, todos com a prova de que a mutação chegou ao binário. **Zero sobreviventes.** O controle (sem mutante) deu os 12 testes verdes.
- **B7-b:** direção de camada com 0 violações nas duas regras e os três controles achados de novo; `fnmetrics.py` com 0 funções fora dos limites (zero acima de 40 linhas, zero acima de 4 parâmetros) em `src/draw2d`, confirmado por um segundo medidor independente (`lizard`).
- **Os quatro IMPORTANTE da rodada anterior (B7-I1 a B7-I4) estão consertados** e medidos de novo (seções 6 e 7).
- **Ressalvas** (todas COSMÉTICO ou INFORMATIVO, seção 7): `pod_buffer.hpp` sem teste próprio (B7-K1); o plano dizia "11 testes" e são 12 desde `piece_refusal_test` (B7-K2); os mutantes de estado GL e de sRGB foram mortos por teste com GL de mentira, e a contraparte com GL real (`draw2d_parity_test`) não foi executada por mim (L-09; janela/placa fora desta revisão, B7-K3); o medidor `fnmetrics.py` é por chaves, declarado aproximado (B7-K4); o sanitizer (B7-c) é do orquestrador e está em branco na seção 3, **o veredito final só vale com ele verde**.

---

## 2. As mutações (as nove de `docs/plano-w7d.md` §4.5, as duas da D-W7D-18, mais uma do B0-C1 e as sete lacunas da errata §19)

**Método.** O executor é `/var/tmp/cto-w7d/b7/mutants.py`.
- Para cada mutante, ele troca UM trecho (o texto velho tem de existir exatamente uma vez na base), recompila os testes unitários de `draw2d` e roda todos (12 em `62b7865`, 11 em `c25cc08`).
- O código de saída de cada teste é lido de `subprocess.returncode`, nunca da tela.
- **Controle:** a rodada SEM mutante tem de dar todos verdes, senão nenhum vermelho prova nada, e o executor para.
- (Rodada de `c25cc08`: executor `mutants.py`. Rodada final em `62b7865`: executor `f/run_mut.py`, logs em `/var/tmp/cto-w7d/b7/f/logs/`, resultado em `/var/tmp/cto-w7d/b7/f/mutants.json`.)

**Método (rodada final).** Executor: `/var/tmp/cto-w7d/b7/f/run_mut.py` (definições em `f/muts_a.py`, reancoradas; os 8 da B4 lidos de `/var/tmp/cto-b7r/muts.py`). Build: CMake + Ninja na cópia (`-DCMAKE_BUILD_TYPE=Release`), os 12 alvos de teste de `draw2d` reconstruídos a cada mutante, tudo sob `systemd-run --user --scope -p TasksMax=400`.
- **Reancoragem:** dos 18 textos-alvo de `mutants.py`, 15 casaram "exatamente uma vez" sem mudança; 3 mudaram de forma com a refatoração e foram reescritos para a MESMA mutação semântica:
  - **M8** (peça com NaN deixa de ser contada): a regra saiu de `frame_report_tally.cpp` para `piece_refusal.cpp` (D-B7-1, I4); mesmo trecho `if ((std::isnan(values) || ...))`; o teste que mata passa a ser `piece_refusal_test`.
  - **M11** (cor sem pré-multiplicar): o `premultiplied` deixou de ser variável local; a mutação é `write_quad(corners, color)` no lugar de `write_quad(corners, gltfx_rgba_premultiplied(color))`.
  - **S5** (sem a reserva padrão): `reserve_pieces` virou `options.reserve_pieces` (`renderer_2d_options`, B7-I3b); mesma mutação.
  Os 8 da B4 e o `m_cap` foram conferidos igualmente (1 ocorrência cada).
- **Controle:** a rodada sem mutante deu os **12** testes verdes (rc 0 lido de `subprocess.returncode`). O plano falava em 11; `piece_refusal_test` entrou depois (B7-K2).
- **Prova de que a mutação chegou ao binário:** md5 do executável de teste, base contra mutante, gravado em `f/mutants.json` (md5 completo). Em todo mutante o md5 do teste que o mata difere do da base. A primeira versão do executor restaurava o arquivo com `copy2` (preserva o mtime antigo) e o Ninja não recompilava: o mutante anterior vazava para o seguinte (S1 aparecia morto por `quad_vertices_test`). Detectado pelo md5 e consertado (`copyfile` mais `utime`); a tabela abaixo é da versão corrigida, e o cuidado é o do L-27 global (provar que a mutação chegou).
- **Restauração provada:** depois do último mutante o executor restaurou o fonte (`diff -rq` da cópia contra a base igual) e reconstruí os 12 alvos: os 12 executáveis voltaram ao md5 do CONTROLE e deram rc 0. A mutação muda o md5 e a restauração o devolve (L-27 global: rebuild após restaurar).
- **Vermelho real, não queda:** em todos os 27, cada teste que morre sai com rc 1 e casos `[FAIL]` impressos (nenhum crash, nenhuma falha de build). Logs em `f/logs/`.

**Resultado: 27 mutantes, 27 mortos, 0 sobreviventes, 0 falhas de build.** md5 abaixo são os 8 primeiros hex (base -> mutante), do executável do teste que mata.

| Mutante | Mutação | md5 base -> mutante | Teste que matou |
|---|---|---|---|
| M1a | estreitar o PONTO para float antes de transformar | `38a6f41e`->`3cd1d8d0` | `quad_vertices_test` (rc=1, 1 caso(s) vermelho(s), ex.: `quad_vertices_precision_and_transform_cells`) |
| M1b | estreitar a TRANSLACAO (a camera) para float, o B0-C1 | `38a6f41e`->`f31bb835` | `quad_vertices_test` (rc=1, 1 caso(s) vermelho(s), ex.: `quad_vertices_precision_and_transform_cells`) |
| M2 | ignorar a camada (so a submissao) | `2f9dba14`->`8ec3ae02` / `490b1dd4`->`ac1d8fe0` / `d0c1a14e`->`84f8e7bf` | `draw_order_test` (rc=1, 2 caso(s) vermelho(s), ex.: `draw_order_closed_eight_cell_enumeration`); `piece_list_test` (rc=1, 6 caso(s) vermelho(s), ex.: `piece_list_paint_order_is_layer_then_submission`); `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_paint_order_is_layer_then_submission`) |
| M3 | desempate na ordem inversa | `2f9dba14`->`eb884238` / `490b1dd4`->`4ebdeb55` | `draw_order_test` (rc=1, 2 caso(s) vermelho(s), ex.: `draw_order_closed_eight_cell_enumeration`); `piece_list_test` (rc=1, 3 caso(s) vermelho(s), ex.: `piece_list_paint_order_is_layer_then_submission`) |
| M4 | agrupar sem comparar textura | `5b385155`->`4ddef5b2` / `55bfc0e1`->`df599152` | `triangle_batch_test` (rc=1, 1 caso(s) vermelho(s), ex.: `triangle_batch_run_cells`); `vertex_stream_test` (rc=1, 1 caso(s) vermelho(s), ex.: `vertex_stream_draws_one_call_per_run_with_the_run_offsets`) |
| M5 | sem glDisable de profundidade e de recorte | `8b02c1c1`->`8d05636f` / `d0c1a14e`->`ef81b598` | `gl_state_contract_test` (rc=1, 2 caso(s) vermelho(s), ex.: `gl_state_contract_setting_state_overrides_every_hostile_item`); `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_gl_state_left_after_a_flush_and_after_the_frame_is_the_closed_list`) |
| M6 | mistura com alfa reto (SRC_ALPHA) | `8b02c1c1`->`a8144d29` | `gl_state_contract_test` (rc=1, 2 caso(s) vermelho(s), ex.: `gl_state_contract_setting_state_overrides_every_hostile_item`) |
| M7 | nao ligar GL_FRAMEBUFFER_SRGB com a opcao ligada | `8b02c1c1`->`0a1ab3be` | `gl_state_contract_test` (rc=1, 2 caso(s) vermelho(s), ex.: `gl_state_contract_setting_state_overrides_every_hostile_item`) |
| M8 | deixar de contar a peca com NaN (a regra mora em piece_refusal.cpp apos a refatoracao) | `49fe85e1`->`771cd283` / `d0c1a14e`->`cf962a85` | `piece_refusal_test` (rc=1, 1 caso(s) vermelho(s), ex.: `piece_refusal_cells`); `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_a_refused_piece_is_counted_and_the_frame_is_still_ok`) |
| M10 | quadrilatero com o indice trocado (a outra diagonal) | `5b385155`->`5d8c2352` | `triangle_batch_test` (rc=1, 1 caso(s) vermelho(s), ex.: `triangle_batch_assembly_cells`) |
| M11 | cor sem pre-multiplicar | `5b385155`->`cbbeb548` | `triangle_batch_test` (rc=1, 1 caso(s) vermelho(s), ex.: `triangle_batch_assembly_cells`) |
| S1 | o erro devolvido e o ULTIMO, nao o primeiro | `d0c1a14e`->`e9a338b8` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_first_error_of_the_frame_is_the_one_returned`) |
| S2 | begin_batch() sem transformacao nao volta ao pixel direto | `d0c1a14e`->`756b9f84` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_begin_batch_without_a_transform_goes_back_to_pixels_mid_frame`) |
| S3 | sem deixar o estado GL depois do envio | `d0c1a14e`->`1f6dcb6b` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_gl_state_left_after_a_flush_and_after_the_frame_is_the_closed_list`) |
| S4 | precedencia invertida: OOM antes do erro do contexto ou da placa | `d0c1a14e`->`4538ad75` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_error_of_the_card_comes_before_out_of_memory`) |
| S5 | sem a reserva padrao quando reserve_pieces = 0 | `d0c1a14e`->`39b7dc22` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_default_reserve_holds_a_thousand_pieces_without_growing`) |
| S6 | peca cujo desenho falhou contada como desenhada | `d0c1a14e`->`b80d26e5` | `renderer_2d_impl_test` (rc=1, 4 caso(s) vermelho(s), ex.: `renderer_2d_impl_a_card_out_of_memory_in_the_draw_is_out_of_memory_with_the_token`) |
| S7 | flush() nao refaz o contexto corrente | `d0c1a14e`->`2c19a6e3` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_flush_makes_the_context_current_again`) |
| B4-1_erro_ultimo | B4-1_erro_ultimo | `d0c1a14e`->`e9a338b8` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_first_error_of_the_frame_is_the_one_returned`) |
| B4-2_batch_nao_zera | B4-2_batch_nao_zera | `d0c1a14e`->`756b9f84` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_begin_batch_without_a_transform_goes_back_to_pixels_mid_frame`) |
| B4-3_sem_leave | B4-3_sem_leave | `d0c1a14e`->`1f6dcb6b` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_gl_state_left_after_a_flush_and_after_the_frame_is_the_closed_list`) |
| B4-4_precedencia | B4-4_precedencia | `d0c1a14e`->`4538ad75` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_error_of_the_card_comes_before_out_of_memory`) |
| B4-5_sem_reserva_padrao | B4-5_sem_reserva_padrao | `d0c1a14e`->`39b7dc22` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_default_reserve_holds_a_thousand_pieces_without_growing`) |
| B4-6_falho_conta_desenhado | B4-6_falho_conta_desenhado | `d0c1a14e`->`65d7cefd` | `renderer_2d_impl_test` (rc=1, 4 caso(s) vermelho(s), ex.: `renderer_2d_impl_a_card_out_of_memory_in_the_draw_is_out_of_memory_with_the_token`) |
| B4-7_flush_sem_contexto | B4-7_flush_sem_contexto | `d0c1a14e`->`2c19a6e3` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_flush_makes_the_context_current_again`) |
| B4-8_sem_sort | B4-8_sem_sort | `d0c1a14e`->`b7beb339` | `renderer_2d_impl_test` (rc=1, 1 caso(s) vermelho(s), ex.: `renderer_2d_impl_the_paint_order_is_layer_then_submission`) |
| B4-cap | B4-cap vertex_stream: a capacidade nao e esquecida apos falha | `55bfc0e1`->`5d00941c` | `vertex_stream_test` (rc=1, 2 caso(s) vermelho(s), ex.: `vertex_stream_capacity_is_not_trusted_after_a_failed_growth`) |

**Sobre os "só o `draw2d_parity_test` mata" do plano (M5, M6, M7, estado hostil, mistura com alfa reto, sRGB não ligado, sem `make_current`).** Todos foram mortos aqui por teste unitário com tabela de GL de mentira (`gl_state_contract_test`, `renderer_2d_impl_test`), então não precisaram do container. Isso prova o contrato contra o GL falso; o efeito em pixel com GL real é do `draw2d_parity_test` (CI, D-A55), que **não executei** (B7-K3).

**`m_cap` (D-FECH-10, parte ii; pendência 3 do plano §6).** É um **mutante**, não um controle: `vertex_stream.cpp`, `upload.capacity = 0;` trocado por `(void)0;` em `upload_one_buffer` (a capacidade deixa de ser esquecida depois de um crescimento que falhou). Em `/var/tmp/cto-b7r/m_cap` o `renderer_2d_impl_test` deu "0 falhas" porque o `run.sh` da B4 só rodava esse teste; quem o mata é o `vertex_stream_test` (`rv.log` do mesmo diretório: 2 falhas, `vertex_stream_capacity_is_not_trusted_after_a_failed_growth` e `vertex_stream_index_capacity_is_not_trusted_after_a_failed_growth`). **Não é sobrevivente.** Reaplicado no `62b7865`: morto pelo `vertex_stream_test` (`B4-cap` na tabela). Os 8 mutantes da B4 (`m_1` a `m_8`) também reaplicados: 8 de 8 mortos pelo `renderer_2d_impl_test`, como na rodada de 30/09 (evidência da errata §29, passo 2).

**A mutação 9 do plano** ("planta `platform → draw2d`, o portão reprova") pertencia ao portão de direção de camada, adiado para a `INFRA-CI` (D-W7D-19, perda 1). No lugar dela está o `grep` de diretivas da seção 4, com os três controles plantados.

---

## 3. `preci.sh --sanitizer-only` (o espelho da B7)

O orquestrador rodou um único `tools/preci.sh --sanitizer-only` (D-FECH-5) sobre o HEAD `3cd237f`, cujo código é idêntico ao de `c831d05` (só docs mudaram entre os dois). Ele cobre B7-c e P5-b. Resultado: rc=0, lido de `/var/tmp/cto-w7d/san/sanitizer.rc`; "preci.sh --sanitizer-only: VERDE"; "100% tests passed, 0 tests failed out of 136". Canários de estreia: `mem_bug` e `ub_bug` REPROVARAM (exit=1) e `clean_case` PASSOU. Execução em 06/10/2026, das 02:45:22 às 02:47:39; log em `/var/tmp/cto-w7d/san/sanitizer.log`.

**Nota sobre o B7-K3.** A parte `make_current` do achado B7-K3 foi fechada pela célula de duas janelas: DW-a `9eebfe6`, DW-b `85a34d6` (emenda E5), com a sabotagem do teste confirmada (`DECISOES_AUTONOMAS.md`, seção "DW-a aceita"). O restante do B7-K3 (efeito em pixel com GL real, `draw2d_parity_test`) continua sendo de outro portão.

---

## 4. Direção de camada por leitura de diretiva (o substituto declarado do portão adiado)

**Instrumento:** `/var/tmp/cto-w7d/b7/directives.py`, um processo só, com contagem impressa, e reprova com varredura vazia (L-40).
- (a) `src/platform/**` nunca inclui `draw2d`, **sem distinção de caixa**;
- (b) `src/draw2d/**` só inclui, entre `<...>`, a biblioteca padrão de C++ (lista fechada no script) ou `<glintfx/...>`. Qualquer outro `<...>` é cabeçalho do sistema e reprova. É uma lista de PERMITIDOS, mais forte que procurar só os três nomes do plano (`<windows.h>`, `<wayland-client.h>`, `<EGL/...>`).

| Rodada | Resultado |
|---|---|
| `c25cc08` (rodada anterior) | (a) 163 arquivos, 655 diretivas, 0 violações; (b) 25 arquivos, 109 diretivas, 0 violações; rc=0 |
| **`62b7865` (final)** | saída real: `(a) src/platform: arquivos com diretiva=170 diretivas=686 violacoes=0` e `(b) src/draw2d:   arquivos com diretiva=27 diretivas=112 violacoes=0`; **rc=0** |
| **`62b7865` com os três controles plantados** (`f/ctl`) | rc=1; `(a)` 2 violações e `(b)` 1, **os três achados**: `src/platform/gl/gl_context_facade.cpp:664` `"draw2d/renderer_2d_impl.hpp"`; `src/platform/wayland/egl_context_adapter.cpp:1207` `"Draw2D/Piece_List.hpp"` (caixa trocada); `src/draw2d/piece_list.cpp:62` `<wayland-client.h>` |

`directives.py` md5 `47ba8633fe943cce2ed79b2522a79b7d` (o mesmo da rodada anterior, não foi alterado). Os arquivos novos (`piece_refusal.*`) estão dentro da varredura: 27 arquivos de `draw2d` com diretiva contra 25 antes.

**E o que `draw2d` inclui por ASPAS fora de si mesmo** (a regra (b) só olha `<...>`, então conferi à parte): `core/log/emit.hpp` (2), `gl_abi.hpp` (3), `gl_functions.hpp` (5) e `platform/gl/gl_context_access.hpp` (1, o acessor declarado do interior do contexto, D-API-06). Nenhum cabeçalho de backend (`platform/wayland`, `platform/win32`).

---

## 5. B4-K5: um `assert` de handle movido por encaminhamento (errata §21)

A §21 fixou a prova: um `grep` que conta os encaminhamentos da fachada contra os `assert`s, com lista fechada e contagem impressa.
- **Base (`c25cc08`; reconferido em `62b7865`: os mesmos 8, 0 sem `assert`, linhas 92, 97, 105, 114, 123, 128, 133, 138):** 8 encaminhamentos em `src/draw2d/renderer_2d_facade.cpp`, cada um com exatamente um `assert(impl != nullptr ...)`: `begin_frame`:91, `begin_batch()`:96, `begin_batch(transform)`:104, `fill_rect`:112, `fill_quad`:121, `flush`:127, `finish_frame`:132, `last_frame_report`:137. Ficam fora, pela regra da §21, `open`, `is_open`, o destrutor e os movimentos. **0 sem `assert`.**
- **Controle:** numa cópia, sem o `assert` do `flush()`, a contagem acusa `SEM ASSERT: flush@127`.
- **Nota de instrumento:** a primeira versão do meu contador dobrou a contagem, porque o texto de cada `assert` repete o nome do método (`"gltfx_renderer_2d::flush() called on a moved-from ..."`) e o corte lia isso como outra definição. A contagem válida é a ancorada no início da definição, que é a que está acima.

---

## 6. L-17: os números duros e as cinco perguntas, por unidade

### 6.1 Os números (`CONTRACT.md:370-372`: no máximo 40 linhas, 4 parâmetros, 3 níveis)

**Nenhum portão mede isto no projeto** (`.clang-tidy` sem `readability-function-size`; nenhum script em `tests/tools`). A B7 é, hoje, a única conferência.

**Rodada final, `62b7865`.** Instrumento `/var/tmp/cto-w7d/b7/fnmetrics.py` (md5 `f675f8fdc184339eb26c0693f2cffa23`), rodado de dentro da cópia. Saída real:

```
funcoes medidas=93 fora_dos_limites=0
  maiores: ('src/draw2d/vertex_stream.cpp', 165, 'create_vertex_stream', 38, 1, 1)
  maiores: ('src/draw2d/renderer_2d_impl.cpp', 110, 'renderer_2d_impl::begin_frame', 31, 1, 2)
  maiores: ('src/draw2d/embedded_program.cpp', 168, 'link_program', 30, 2, 2)
  maiores: ('src/draw2d/renderer_2d_impl.cpp', 49, 'renderer_2d_impl::create', 29, 3, 1)
  maiores: ('src/draw2d/vertex_stream.cpp', 81, 'send_to_buffer', 28, 3, 1)
  maiores: ('src/draw2d/gl_state_contract.cpp', 38, 'set_common_state', 27, 4, 1)
  maiores: ('src/draw2d/pod_buffer.hpp', 46, 'ensure_room', 27, 3, 1)
  maiores: ('src/draw2d/renderer_2d_facade.cpp', 42, 'gltfx_renderer_2d::open', 27, 2, 1)
```

**Critério da errata §29: zero funções acima de 40 linhas e zero acima de 4 parâmetros em `src/draw2d`. Cumprido** (a maior tem 38 linhas; o máximo de parâmetros é 4, em `set_common_state`). Colunas: arquivo, linha, função, linhas, parâmetros, aninhamento.

**Controles do instrumento (L-36).**
- *Contra o SHA antigo:* o mesmo `fnmetrics.py` sobre `c25cc08` acusa `fora_dos_limites=7` (entre elas `send_pending` 60 linhas, `create_embedded_program` 50, `add_quad` 45, `upload_batch` 43 e os 7 e 5 parâmetros). Ou seja, ele sabe reprovar.
- *Plantado:* uma cópia de `src/draw2d` com `planted_long` (48 linhas) e `planted_params` (5 parâmetros) acrescentadas a `piece_list.cpp`: `fora_dos_limites=2`, as duas achadas.
- *Segundo medidor, independente:* `lizard src/draw2d src/platform/gl/gl_context_access.cpp -L 40 -a 4 -w` sobre `62b7865`: **nenhum aviso** (109 funções analisadas, a maior com `length` 38); sobre `c25cc08` o mesmo comando dá 6 avisos (`create_embedded_program` 50, `create` 5 parâmetros, `send_pending` 60, `add_quad` 46, `send_to_buffer` 7 parâmetros, `upload_batch` 45). Os dois medidores concordam nos dois SHAs.

**Os achados da rodada anterior, um por um:**

| Achado (em `c25cc08`) | Conserto | Medido em `62b7865` |
|---|---|---|
| B7-I1 `send_pending` 60 linhas, mais de uma coisa | `2c02b56`: quatro fases com nome próprio | relido à mão em `renderer_2d_impl.cpp:242-262`: `send_pending` só sequencia (`drop_pending_without_context`, `batch_pending_in_paint_order`, `upload_and_draw_batch`, `settle_piece_counts`, mais o `leave_gl_state_after_drawing` final); frase "sequencia as fases de um flush", sem "e". Resta uma nota COSMÉTICA: `upload_and_draw_batch` (linhas 205-229, 25 linhas) ainda junta envio e desenho numa unidade, dentro dos limites |
| B7-I2 `create_embedded_program` 50 linhas | `63e572d`: `compile_both_shaders` e `link_program` | 14 e 30 linhas; `read_driver_log` com 4 parâmetros |
| B7-I3 `send_to_buffer` 7 e `create` 5 parâmetros | `66b73aa` (`buffer_upload`) e `ee14622` (`renderer_2d_options`) | 3 e 3 parâmetros |
| (cosméticos) `add_quad` e `upload_batch` na borda | `614a679` (`write_quad`, `record_run`) e `66b73aa` (`upload_one_buffer`) | ambos fora da lista das oito maiores |
| B7-I4 `frame_report_tally` com dois assuntos | `34f0374`: a recusa da peça vai para `piece_refusal.*` | `frame_report_tally.hpp` inclui só `<cstdint>` e `frame_2d_report.hpp`, sem `refusal_of_*`; `piece_refusal.hpp` abre com a frase sem "e" |

### 6.2 As cinco perguntas

**As frases e as colunas desta tabela são da rodada em `c25cc08`; só as linhas marcadas foram reconferidas em `62b7865`** (diretiva, comando e função por função na 6.1). Nada nos 8 commits de refatoração acrescentou arquivo de assunto novo além de `piece_refusal.*`, que nasceu de um achado. A régua é a da L-17:
1. quais leis obrigariam a unidade a mudar (lido nos `#include`);
2. a frase sem "e";
3. o teste monta o mundo? (janela, arquivo, relógio, container);
4. o que entra pelo `#include`;
5. quem paga a próxima feature: quantos dos 43 commits do `R2D-BATCH` tocaram o arquivo, por `git log --name-only --grep=R2D-BATCH c25cc08`.

| Unidade | (1) Leis | (2) A frase | (3) Monta o mundo? | (4) `#include` | (5) Commits | Veredito |
|---|---|---|---|---|---|---|
| `quad_vertices` | a decisão de precisão do líder | "leva o canto de mundo ao pixel pela transformação do lote, em double até o fim" | não (puro) | `cmath`, `array`, `core/transform`, `core/vec2` | 1 | são |
| `draw_order` | L-35 (ordem determinística) | "ordena as chaves (camada, submissão) no lugar" | não | `algorithm`, `span` | 1 | são |
| `piece_list` | L-35, R3 | "guarda as peças do quadro e as entrega em ordem de pintura" | não | `draw_order`, `pod_buffer`, `quad_vertices` | 1 | são |
| `triangle_batch` | L-31 (formato de vértice), R3 | "monta vértices e índices por corrida de estado" | não | `pod_buffer`, `quad_vertices`, `core/color` | 4 | são; é o mais tocado depois do `CMakeLists.txt` (o `reserve` e a capacidade), sem virar dono de assunto novo |
| `frame_report_tally` (em `c25cc08`; em `62b7865` é só a contagem) | R4 e R7 (relato) | "conta os destinos do quadro" | não | `cstdint`, `frame_2d_report` | 3 | **era IMPORTANTE (B7-I4), consertado em `34f0374`**: a regra de valor inválido virou `piece_refusal` ("decide qual peça é recusada e por quê", puro, 1 commit) |
| `srgb_encoding` | a decisão de cor (linear) | "codifica linear para sRGB" | não | `cmath`, `core/color` | 1 | são |
| `gl_load_refusal` | R7, L-22 | "traduz a falha do carregador para o erro público" | não | `core/err` | 1 | são |
| `gl_state_contract` | L-31, D-W7D-11, E3 | "define o estado GL do desenho e o deixa como a lista promete" | GL de mentira | `gl_abi`, `gl_functions` | 1 | são |
| `vertex_stream` | L-31, R3, R7, R9 | "cria os objetos de vértice e envia o lote, mapeando o erro GL" | GL de mentira | `gl_*`, `core/log`, `core/err`, `triangle_batch` | 3 | são no assunto; o número (B7-I3) foi consertado |
| `embedded_program` | L-31, R7, R9 | "compila e liga o programa embutido, e relata a falha do driver" | GL de mentira | `gl_*`, `core/log`, `core/err` | 3 | são no assunto; o número (B7-I2) foi consertado |
| `pod_buffer` | R3 | "cresce um bloco de elementos triviais sem lançar" | **sem teste próprio** | `cstdlib`, `limits`, `type_traits` | 2 | são; o teste é indireto (COSMÉTICO, B7-K1) |
| `renderer_2d_impl` | L-31, R3, R7, D-W7D-08 | "a máquina do quadro: abre, recebe, envia e relata" | GL de mentira | 12 cabeçalhos internos, e é o orquestrador | 1 | o arquivo é são como orquestrador. **`send_pending` não era** (em `c25cc08`; consertado em `2c02b56`): "drena o que o contexto não aceitou **e** ordena **e** monta **e** envia **e** define estado **e** desenha **e** conta **e** deixa o estado" (B7-I1) |
| `renderer_2d_facade` | L-19 (fachada que só encaminha), L-22 | "encaminha cada método ao interior; `open()` monta o interior" | teste vivo (B5) | a fachada pública, o interior, o acessor do contexto | 2 | são: cada método é um encaminhamento de uma linha mais o `assert` |
| `frame_2d_report.cpp` | R7 | "dá o token de cada recusa" | não | só o cabeçalho público | 1 | são |
| os 6 cabeçalhos públicos | L-19, L-26 | conferidos nas conferências de P0/B0/B4 | não | | 1 cada | são |

**Quinta pergunta, a mais objetiva.** Dos 43 commits do `R2D-BATCH`, nenhum arquivo de código aparece em quase todos. O `CMakeLists.txt` de `src/draw2d` (9) é o registro que cresce por natureza. Não há sinal de monolito nascendo por acúmulo.

---

## 7. Achados

| ID | Classe | Achado | Estado |
|---|---|---|---|
| B7-I1 a B7-I4 | IMPORTANTE (rodada em `c25cc08`) | `send_pending` longo e com várias fases; `create_embedded_program` com 50 linhas; `send_to_buffer` com 7 e `create` com 5 parâmetros; `frame_report_tally` com dois assuntos | **FECHADOS** em `2c02b56`..`d9a9f90`, medidos em `62b7865` (6.1) |
| B7-K1 | COSMÉTICO | `pod_buffer.hpp` (`ensure_room` etc.) não tem teste próprio: é coberto de forma indireta por `triangle_batch_test` e `piece_list_test`. Não rodei mutante do crescimento em si (fora da lista do plano) | ABERTO; sugestão: um caso direto de falha de crescimento sem lançar |
| B7-K2 | INFORMATIVO | O plano (§2.1 B7-a) manda "os 11 testes da `draw2d`"; em `62b7865` são **12** (`piece_refusal_test` entrou na refatoração B7-I4). O M8 mudou de arquivo e de teste que mata (`frame_report_tally_test` -> `piece_refusal_test`). O texto do plano e `mutants.py` antigo ficam defasados | ABERTO (ajustar o plano/relatório ao versionar; `mutants.py` antigo não deve ser reutilizado: o `copy2` dele deixa o Ninja com mutante velho) |
| B7-K3 | INFORMATIVO | M5, M6, M7, S3 e S7 (estado hostil, mistura, sRGB, estado deixado, `make_current`) foram mortos por teste com GL de mentira; o efeito com GL real (`draw2d_parity_test`) não foi executado por mim (L-09: janela/placa fora do escopo desta revisão). O verde do parity nos dois sistemas é pré-condição separada (D-A55) | ABERTO, de outro portão |
| B7-K4 | INFORMATIVO | `fnmetrics.py` é leitura por chaves (declarado: não é parser de C++), 93 funções; o `lizard` vê 109 (conta lambdas e funções anônimas). Os dois dão zero fora do limite, então a conclusão não depende do medidor; o nível de aninhamento só vem do `fnmetrics.py` | ABERTO, sem ação |
| B7-K5 | COSMÉTICO | Nenhum portão mede `readability-function-size` no projeto (`.clang-tidy`), então a regressão da L-17 volta sem aviso; a B7 é a única conferência | ABERTO, item de infra (`INFRA-CI`) |

**Nenhum CRÍTICO e nenhum IMPORTANTE aberto.** Nenhum mutante sobreviveu.

---

## 8. FATO e INFERÊNCIA

- **FATO:** tudo o que tem `arquivo:linha`, SHA, md5 ou código de saída lido de variável.
- **INF:** as frases da pergunta 2 são minhas, escritas lendo o código; que `add_quad` e `upload_batch` "passam se o comentário não conta" depende de como o `CONTRACT.md` é lido, e o texto dele não diz.
- **Frase-guarda (L-21 global):** nada aqui se julgou pelo uso de um consumidor específico.
