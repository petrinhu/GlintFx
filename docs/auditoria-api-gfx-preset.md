<!-- Origem: auditoria-api-gfx-preset.md, md5 0f60cbf6a1495a9781aefd2d2f243cd7. Corpo copiado byte a byte; so este cabecalho e o apendice final foram acrescentados (D-FECH-3). -->

# Parecer P0: revisão de API dedicada de `GFX-PRESET`

**Quem:** revisor de API dedicado da W7-D (agente `api-review`), distinto do CTO que planejou (`cto-review`) e do implementador (`impl-da14`), pela L-12 do projeto e da global. **Data real:** 29/09/26 - 23:05 (`date`). **Árvore:** os cabeçalhos foram lidos e o fixture montado a partir do blob `e39fcb021bf08d3d12fd8e6e991cf2a55e4464ad` (`git archive`, nunca a árvore de trabalho, que tinha trabalho de outro agente). **Nada no repositório foi editado.**

**Plano lido:** `/var/tmp/cto-w7d/PLANO.md`. O briefing mandava md5 `892f16ce331c8d43cb37fc5f7d30f085`; medi **`e923192a5aaaf72a5f6792a486df5e86`** às 22:46 (mtime 22:46, avisado ao team-lead no mesmo minuto) e li essa versão inteira. Às 23:03 o arquivo estava em **`bd73502a64fa2b4c41d338ce4c09de1e`**; o `diff` entre as duas toca só três linhas de redação (desambiguação da B3a do adendo, §0 item 3 e D-W7D-19), nada nas §3 e §4 que regem esta revisão.

**Fontes internas lidas inteiras ou na seção pedida:** `docs/plano-w7d.md` §3 e §9; `docs/plano-w7d-adendo-revalidacao.md` §3.A e §6; `docs/plano-w6b-fatias-5.md` §2 (D-W6b-34, 35, 44) e §5; `docs/api-conventions.md` R1 a R10 e a auditoria de colisão; `GODS_LAWS.md` do projeto L-12, L-17, L-19, L-21, L-22, L-26, L-27, L-28, L-29, L-34; `include/glintfx/platform/gl/gfx_option.hpp`, `context.hpp`; `src/platform/gl/gfx_option_registry.hpp`; `docs/wiki/API-Graphics-Context.md`.

**Leis aplicadas e como:** L-12 (sou o terceiro agente; não implementei nem planejei); L-21 (texto de cabeçalho em inglês internacional, parecer em pt-br); L-22 e R3 (nenhuma exceção cruza, tudo `noexcept`); L-27 (cada achado separa FATO, com `arquivo:linha` ou medição, de INF); L-29 (bibliotecas de terceiros lidas só em documentação ou cabeçalho público, licença conferida, nada copiado); L-34 com a emenda de 29/09 (cada dúvida no formato da lei, com fontes); L-11 global (um trabalho pesado por vez: esperei o `preci --fast` de outro agente terminar, `pgrep` vazio às 23:03, antes de rodar o portão); L-09 (nada abriu janela, GPU ou entrada).

---

## 1. Veredito

**APROVADO COM TRÊS EMENDAS, todas já aplicadas no texto congelado da seção 2.** Os três nomes do rascunho (`suggested_preset`, `gltfx_gfx_preset_row_count`, `gltfx_gfx_preset_row_at`) congelam. As emendas: (1) os números que as linhas de predefinição leem e escrevem passam a ter nome público (P0-I1); (2) o `row_at` fora da faixa degrada para uma entrada inofensiva, não para `{vsync, 0}` (P0-I2); (3) o parâmetro se chama `row_index`, não `index`, porque `index` colide medido com macro de sistema (P0-I3). Nenhum achado CRÍTICO. P3 pode começar quando o main aceitar este parecer.

**Contagem de achados:** 0 CRÍTICO, 5 IMPORTANTE, 2 COSMÉTICO.

---

## 2. O texto congelado

**A fonte única é ESTE parecer, versionado em `docs/auditoria-api-gfx-preset.md`** (D-W7D-R8: `/var/tmp` não sobrevive nem é visto no servidor); o diff abaixo é o que o fechamento compara. O fixture de onde o diff saiu, e que também foi a sonda do portão, é cópia de conveniência, não para commit: `/var/tmp/cto-w7d/api-review/fixture/include/glintfx/platform/gl/gfx_option.hpp`, md5 `f96baa372c9a338100dae975b9cfeefa`.

O que P3 acrescenta ao cabeçalho publicado é EXATAMENTE o diff abaixo (gerado por máquina contra o blob de `e39fcb0`), com uma exceção de ordem de entrada: a linha etiquetada **[P4]** só entra no commit de P4 (seção 7). Nenhuma outra linha do cabeçalho muda, salvo o gêmeo obrigatório de P0-K1 e a correção recomendada de P0-I4.

````diff
--- a/include/glintfx/platform/gl/gfx_option.hpp
+++ b/include/glintfx/platform/gl/gfx_option.hpp
@@ -118,7 +118,56 @@
     preset = 5,
     auto_choice_reason = 6,
     power_source = 7,
-};
+    // GFX-PRESET (docs/auditoria-api-gfx-preset.md, P0): `choice`,
+    // `read_only`. The preset this library would suggest RIGHT NOW,
+    // computed at the moment it is read - see the "SUGGESTED PRESET"
+    // block below this enum for the full contract.
+    suggested_preset = 8,
+};
+
+// ============================================================
+// THE VALUES OF THE PRESET ROWS - DATA CONTRACT, FOREVER
+// ============================================================
+//
+// Every `choice` row reads and writes a plain std::int64_t (item 2 of
+// this header's own top comment). For the four rows below, the meaning
+// of each number is part of the DATA contract (a consumer's saved
+// settings file stores the number): once shipped, a number is never
+// reused for a different meaning and never renumbered. The named
+// constants exist so that no consumer has to write a bare number; the
+// constants and the numbers are the same thing, spelled two ways.
+//
+// `preset` (id 5, `live`) and `suggested_preset` (id 8, `read_only`)
+// share ONE vocabulary, on purpose: the value read from
+// `suggested_preset` can be written straight back into `preset`.
+// `suggested_preset` only ever reads power_saving, balanced or
+// performance - never manual, never automatic.
+inline constexpr std::int64_t k_gltfx_preset_manual = 0;
+inline constexpr std::int64_t k_gltfx_preset_power_saving = 1;
+inline constexpr std::int64_t k_gltfx_preset_balanced = 2;
+inline constexpr std::int64_t k_gltfx_preset_performance = 3;
+inline constexpr std::int64_t k_gltfx_preset_automatic = 4;
+
+// `auto_choice_reason` (id 6, `read_only`): WHY `suggested_preset`
+// reads what it reads. `none` is never produced by this version (every
+// suggestion has a reason); it stays in the vocabulary because the
+// vocabulary is append-only.
+inline constexpr std::int64_t k_gltfx_auto_choice_reason_none = 0;
+inline constexpr std::int64_t k_gltfx_auto_choice_reason_on_battery = 1;
+inline constexpr std::int64_t k_gltfx_auto_choice_reason_software_renderer = 2;
+inline constexpr std::int64_t k_gltfx_auto_choice_reason_shared_gpu = 3;
+inline constexpr std::int64_t k_gltfx_auto_choice_reason_dedicated_gpu = 4;
+inline constexpr std::int64_t k_gltfx_auto_choice_reason_unknown_gpu = 5;
+
+// `power_source` (id 7, `read_only`): what the operating system reports
+// about where the machine's power comes from. `unknown` is a
+// first-class answer on both systems (a machine with no battery and no
+// readable supply, such as most virtual machines, reads `unknown`).
+// Proved by: power_supply_rule_test (Linux rule, every cell of the
+// closed set) and power_status_rule_test (Windows rule, every cell).
+inline constexpr std::int64_t k_gltfx_power_source_unknown = 0;
+inline constexpr std::int64_t k_gltfx_power_source_mains = 1;
+inline constexpr std::int64_t k_gltfx_power_source_battery = 2;
 
 // The three shapes a value can take (D-W6b-16 (2)): `toggle` reads
 // 0/1, `choice` reads the numeric id of the chosen value (documented
@@ -208,4 +257,65 @@
 [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_gfx_option>
 gltfx_gfx_option_by_name(std::string_view name) noexcept;
 
+// ============================================================
+// SUGGESTED PRESET - A SUGGESTION HANDED OVER, NEVER LIBRARY STATE
+// ============================================================
+//
+// The leader's order of 06/09/2026 (ESCOPO.md): the automatic choice
+// only HANDS OVER the best values for the consumer to use on their
+// own side; it locks nothing. What that means, rule by rule:
+//
+//   1. READING NEVER WRITES. option(suggested_preset) and
+//      option(auto_choice_reason) compute the suggestion at the moment
+//      they are called, from two facts the system reports (the kind of
+//      GPU this context runs on, and option(power_source)). Reading
+//      them changes no option and no label.
+//      Proved by: gl_context_parity_test (every row read before and
+//      after asking twice: identical, on a real context).
+//   2. APPLYING IS ALWAYS YOUR REQUEST. set_option({preset, P}) with a
+//      concrete P applies every row of P, all or nothing, once.
+//      set_option({preset, k_gltfx_preset_automatic}) is a shortcut for
+//      "apply the suggestion of right now": it applies the suggested
+//      concrete preset, once, and stores THAT concrete value as the
+//      label - option(preset) never reads `automatic`.
+//   3. THE LABEL IS YOURS. option(preset) reads the last preset you
+//      asked for (default `manual`), not a verdict on whether the
+//      current values still match it. Changing one row by hand after
+//      applying a preset never rewrites the label.
+//   4. NOTHING IS EVER REAPPLIED OR PERSISTED BY THE LIBRARY ON ITS
+//      OWN. A power cable plugged in or pulled out changes what
+//      option(suggested_preset) reads next time, and nothing else.
+//
+// Two readings in a row may disagree: the power source can change
+// between them. That window is real and is not hidden.
+//
+// `balanced` and `performance` hold the SAME rows in this version, on
+// purpose: the only rows a preset touches today are vsync and
+// frame_rate_cap, and turning vsync off trades tearing for speed, which
+// is a separate choice from "performance". They start to differ when
+// the library grows options that can actually be spent on quality.
+// Proved by: gfx_preset_table_test (every row of every preset, value by
+// value).
+
+// How many rows the preset `preset` sets, in THIS build. Zero for
+// k_gltfx_preset_manual, for k_gltfx_preset_automatic (it resolves to
+// a concrete preset first) and for any number outside the vocabulary
+// above. Never a literal a consumer hardcodes: the rows of a preset are
+// the library's current best values and may change in a later release.
+[[nodiscard]] GLINTFX_API std::size_t gltfx_gfx_preset_row_count(std::int64_t preset) noexcept;
+
+// The row at `row_index` (0 <= row_index < gltfx_gfx_preset_row_count(
+// preset)) of the preset `preset`: which option it sets, and to what.
+// Read the suggestion, read its rows, and decide on your side what you
+// apply (the leader's "best values for the consumer to use").
+//
+// An out-of-range `preset` or `row_index` degrades (docs/api-
+// conventions.md R4) to the entry {preset, k_gltfx_preset_manual}:
+// the one entry that, if applied by mistake, changes the value of no
+// row - it only sets the preset label to `manual` (rule 3 above). It
+// never degrades to a default-constructed entry, because {vsync, 0}
+// would silently turn vsync off.
+[[nodiscard]] GLINTFX_API gltfx_gfx_option_entry
+gltfx_gfx_preset_row_at(std::int64_t preset, std::size_t row_index) noexcept;
+
 } // namespace glintfx
````

**O que congela, dito em uma frase por item:** o id `suggested_preset = 8` (append-only, nunca renumerado); o nome de dado `"suggested_preset"` (contrato de DADO, L-26; entra na linha 9 do registro com `choice`, `read_only`, faixa 1 a 3, padrão 2, como D-W6b-44 já fixava); os catorze números nomeados das linhas 5 a 8 (contrato de DADO: os números já estavam congelados pelo registro desde a fatia 2a, `src/platform/gl/gfx_option_registry.hpp:79-98`; o que P0 congela a mais são só os NOMES deles); as duas assinaturas; a regra de degradação de cada uma; as quatro regras de semântica do bloco "SUGGESTED PRESET". **O que NÃO congela:** o conteúdo das linhas de cada predefinição (é dado da biblioteca, pode mudar numa versão B, e o cabeçalho manda o consumidor ler em vez de gravar número), nem a regra automática de 12 células (D-W6b-34, interna).

---

## 3. As regras de `docs/api-conventions.md`, uma a uma

| Regra | Veredito | Por quê (FATO ou INF marcado) |
|---|---|---|
| R1, envelope único | **CONFORME** | Nenhuma função nova é falível: `row_count` e `row_at` são totais. A leitura falível continua sendo `gltfx_gl_context::option()`, que já devolve `gltfx_rslt<std::int64_t>` (`context.hpp`, bloco dos nove métodos). |
| R2, `[[nodiscard]]` estrutural | **CONFORME** | Não há `gltfx_rslt` novo. As duas funções levam `[[nodiscard]]` explícito, como as quatro irmãs do mesmo cabeçalho (`gfx_option.hpp:187-209` no blob). |
| R3, fronteira `noexcept` que não aloca | **CONFORME, com condição de P1** | As duas assinaturas são `noexcept` e devolvem valor pequeno. A condição: a implementação lê a tabela fixa (`k_gfx_preset_table`, `std::array`, degrau 1 da R3), nunca monta contêiner. A correção C1 do plano (`preset_expansion` sem `std::vector`) é o mesmo caso; `noexcept_alloc_test` verde sem linha nova em `tests/noexcept_alloc_exceptions.txt` é a prova (critério 4 do fechamento). |
| R4, ausência nunca é comportamento indefinido | **CONFORME COM EMENDA (P0-I2)** | `row_count` fora do vocabulário devolve zero. `row_at` fora da faixa devolvia, no rascunho, `gltfx_gfx_option_entry{}`, que é `{vsync, 0}` (FATO, `gfx_option.hpp:164-167`): uma entrada que PARECE real e, aplicada, desliga a sincronia. Emenda: degrada para `{preset, k_gltfx_preset_manual}`, a única entrada que, aplicada por engano, não muda o valor de linha nenhuma: só grava o rótulo `manual` (regra 3 de D-W6b-35). |
| R5, alocar e liberar do mesmo lado | **CONFORME** | Nada atravessa a fronteira além de dois inteiros e um par `{id, value}` trivial. As constantes são `inline constexpr`, vivem só no cabeçalho, não geram símbolo exportado. |
| R6, nome sem colisão nos cinco alvos | **CONFORME COM EMENDA (P0-I3), metade Windows NÃO EXECUTADA** | Portão rodado no texto congelado (seção 6): 0 colisão real. A sonda de parâmetros achou `index` colidindo com `/usr/include/X11/Xos.h:67`, por isso `row_index`. Metade Windows: conferida só à mão contra a lista de R6 (`min`, `max`, `interface`, `small`, `near`, `far`, `IN`, `OUT`, `CONST`, `VOID`, `TRUE`, `FALSE`, `ERROR`, `DELETE`, `STRICT`): nenhum nome novo casa. A prova real é o `header_hygiene_test` do trabalho Windows do servidor em P3. |
| R7, token, nunca frase | **CONFORME** | As funções devolvem números; o nome de cada número é identificador C++ (`k_gltfx_preset_balanced`). Não há texto de catálogo. Lacuna registrada como INBOX, não defeito: não existe função que devolva o token de DADO de um valor (`"balanced"`), só o número (P0-K2). |
| R8, retorno de chamada | **NÃO SE APLICA** | Nenhum retorno de chamada. |
| R9, evento de registro | **NÃO SE APLICA** | Nenhum evento novo. |
| R10, camadas de retorno de chamada | **NÃO SE APLICA** | Idem. |
| Disciplina de citação (`CLAIM-CITATIONS`) | **CONFORME, com a ordem de entrada da seção 7** | Simulado por máquina (seção 6.5): o texto com a linha [P4] retirada passa no estado de P3; com ela, passa no de P4. |

---

## 4. Achados

**P0-I1. IMPORTANTE. O consumidor não tem como saber o que significa o número que `suggested_preset` devolve.**
- FATO: os valores de `preset` (`manual=0` a `automatic=4`), de `auto_choice_reason` (`none=0` a `unknown_gpu=5`) e de `power_source` (`unknown=0`, `mains=1`, `battery=2`) só existem em comentário de `src/platform/gl/gfx_option_registry.hpp:79-98`, que não é instalado. `gfx_option.hpp:125` promete que cada `choice` tem os valores "documented per option", e nenhum cabeçalho público os documenta (`grep -rn 'power_saving\|on_battery\|mains' include/` vazio). A wiki, `docs/wiki/API-Graphics-Context.md:63-65`, também não.
- INF: com `suggested_preset`, a lacuna vira defeito de uso: ler "2" e não saber que é `balanced` anula o item inteiro ("entrega os melhores valores para o consumidor usar").
- Resolução congelada: catorze constantes `inline constexpr std::int64_t k_gltfx_...` (decisão D-API-P0-1).
- Gêmeo fora do escopo (L-17 global): `vsync` (0/1/2) e `gpu_preference` (0/1/2) têm a mesma lacuna. Recomendação: linha na INBOX, fora desta onda (escopo novo antes da demo, L-32).

**P0-I2. IMPORTANTE. A degradação do rascunho transforma um erro de índice em "desligar a sincronia" em silêncio.** FATO e resolução na linha R4 da tabela acima (D-API-P0-2).

**P0-I3. IMPORTANTE. `index` colide com macro de sistema, medido.**
- FATO: rodada B do portão (seção 6.2), `COLISAO ... /usr/include/X11/Xos.h:67:index`. A regra permanente de RSLT-ERR-RENAME (`docs/api-conventions.md:92`) vale para nome de parâmetro também, e o portão não varre parâmetro (cabeçalho do script, "function PARAMETER names are NOT scanned"): só a sonda achou.
- Resolução congelada: `row_index`.

**P0-I4. IMPORTANTE. O gêmeo já publicado: `gltfx_gfx_option_at(std::size_t index)` (`gfx_option.hpp:193` no blob) tem a mesma colisão.**
- FATO: a mesma rodada B. INF: é macro de função (`index(s,c)`), e `index)` sem parêntese depois não expande; o risco hoje é de regra, não de quebra medida.
- Recomendação: P3 já edita este cabeçalho; renomear o parâmetro para `index_in_table` (ou o nome que o implementador preferir, que não colida) no mesmo commit. Nome de parâmetro não é API nem ABI: custo zero para o consumidor. Não é decisão minha aplicar; é do main ao despachar P3.

**P0-I5. IMPORTANTE (do instrumento, gêmeo de B0-I7). O portão de citação aceita um bloco se UMA citação dele é válida, e isso esconde a linha [P4].**
- FATO, medido na simulação da seção 6.5: a linha "Proved by: gl_context_parity_test (...)" cita um teste que JÁ existe; o portão passa com ela no estado de P3, antes de a célula que ela promete existir. É a família "afirma que mede e não mede".
- Consequência: a ordem de entrada da seção 7 só é garantida pela comparação do texto publicado com este parecer (o critério de D-W7D-R2), não pelo portão.
- Recomendação: linha nova na onda `INFRA-CI` (o portão passar a exigir que CADA citação do bloco exista, não uma). Não é desta onda: ferramental novo está fora (D-W7D-19).

**P0-K1. COSMÉTICO, gêmeo obrigatório de P3.** `gfx_option.hpp:100` no blob diz "v1 ships exactly the eight ids". Com `suggested_preset` fica falso. P3 reescreve essa frase (por exemplo "the ids below, in the SAME order as their numeric values"), sem número, pela regra de `DOC-ESTADO` do `CLAUDE.md` (número escrito apodrece). O mesmo vale para "Eight rows" e `std::array<gfx_option_row, 8>` do registro interno, que o `static_assert` já obriga.

**P0-K2. COSMÉTICO, INBOX.** Não há função que devolva o token de DADO de um valor (`"power_saving"` para 1). Quem grava a configuração do jogador grava o número, que é o contrato; um token legível seria conveniência. Registrar como ideia pós-demo, não agora.

---

## 5. Decisões no formato da L-34 (confirmar retroativamente; quem decide no modo autônomo é o CTO)

**D-API-P0-1. Os números das linhas de predefinição ganham nome público.**
- Pergunta que iria ao líder: "O consumidor lê `suggested_preset = 2`. Publicamos o significado dos números? Como?"
- Opções: (a) só comentário no cabeçalho; (b) constantes `inline constexpr std::int64_t`; (c) `enum class gltfx_gfx_preset : std::int64_t`; (d) função que devolve o token (`"balanced"`).
- Fontes: `gfx_option.hpp:125` (a promessa "documented per option"); precedente da casa de constante pública com prefixo `k_gltfx_` (`k_gltfx_gpu_index_unknown`, `gpu.hpp`; `k_gltfx_max_frame_elapsed`, `loop.hpp`); SDL3 publica valores nomeados como `#define`/`enum` para toda propriedade (documentação `wiki.libsdl.org`, lida em 23/09 e 29/09, licença zlib); L-28 ("uma pessoa que nunca viu entende lendo uma vez?": `k_gltfx_preset_balanced` sim, `2` não).
- Escolha: **(b)**. (a) deixa número mágico no código do consumidor; (c) obriga um `static_cast` em cada `set_option` porque o canal de valor é `std::int64_t` (D-W6b-17, congelado), o que é pior de ler que o número; (d) é conveniência, fica em INBOX (P0-K2). (b) não custa ABI (cabeçalho só), e os números já estavam congelados como dado.
- Porta de mão única: **sim** (nomes públicos). Custo de reverter: alto depois de publicado; zero agora.

**D-API-P0-2. `row_at` fora da faixa degrada para `{preset, k_gltfx_preset_manual}`.**
- Pergunta: "Um índice fora da faixa devolve o quê?"
- Opções: (a) `gltfx_gfx_option_entry{}` = `{vsync, 0}` (o rascunho); (b) devolver `gltfx_rslt<entry>`; (c) id sentinela fora da tabela; (d) `std::span` das linhas, sem índice; (e) `{preset, manual}`.
- Fontes: R4; R1 (função total não devolve envelope); precedente de `gltfx_gfx_option_at`, que degrada para um `info` de nome vazio, distinguível (`gfx_option.hpp:186-193`); D-W6b-35 regra 3.
- Escolha: **(e)**. (a) produz uma entrada que parece real e faz mal aplicada; (b) torna falível uma leitura de tabela, contra o molde das irmãs; (c) exige constante sentinela nova e depende de `set_option` recusá-la; (d) é idioma diferente das irmãs `count/at` no mesmo cabeçalho (a coerência da família venceu, e (e) resolve o risco sem trocar o idioma).
- Porta de mão única: **sim**. Custo de reverter: alto depois de publicado.

**D-API-P0-3. O parâmetro é `row_index`.** Pergunta, opções e escolha em P0-I3 e P0-I4: não é gosto, é colisão medida. Porta de mão única: não (nome de parâmetro não é contrato). Custo: zero.

---

## 6. Estreia: o portão de colisão, e a separação P3/P4 por máquina

Tudo rodou contido (`systemd-run --user --scope -p TasksMax=...`), com `TMPDIR=/var/tmp`, num fixture fora da árvore, depois de `pgrep -af '^bash tools/preci.sh'` sair vazio. Nada abriu janela, GPU ou entrada (L-09).

**6.1 Controle positivo (L-36: portão só conta depois de visto vermelho).** Um enumerador `major` plantado em `probe/control.hpp`.
- Primeira tentativa, com o `enum` numa linha só: o portão passou (rc=0). **O defeito era da minha sonda, não do portão:** o extrator dele lê enumeradores só nas linhas seguintes à abertura do `enum` (forma declarada no cabeçalho do script). Registrado para quem repetir.
- Segunda tentativa, em várias linhas: **rc=1**, `/usr/include/sys/sysmacros.h:60:major`. O portão lê o fixture.

**6.2 Sonda de nomes de parâmetro e de R2D-SHAPES** (enumeradores descartáveis, fora do texto congelado): **rc=1**, uma colisão real, `index` (`X11/Xos.h:67`). `clear` aparece só como NEUTRALIZADA (`curses.h:1307`, define e `#undef` no mesmo arquivo). Todos os outros limpos.

**6.3 O texto congelado sozinho:** **rc=0**. `366 nome(s) publico(s) verificados contra 15367 arquivo(s) de sistema (6 diretorio(s)), 0 colisao real (frontend: gcc)`. Contra o blob de `e39fcb0` (311 nomes), são **55 nomes novos** somando P0 e B0; os de P0: `suggested_preset`, `gltfx_gfx_preset_row_count`, `gltfx_gfx_preset_row_at` e as catorze constantes.

**6.4 Compilação dos rascunhos:** um TU que usa cada nome novo, com a ordem hostil de `sys/sysmacros.h` e `sys/types.h` antes, `-std=c++23 -fsyntax-only -Wall -Wextra -Wshadow -Wpedantic -Werror`: **g++ rc=0, clang++ rc=0**.

**6.5 Separação P3/P4 pelo portão de citação** (`check_claim_citations.py --check` sobre cópias do repositório; os testes de P1 e P2 simulados por `add_test` acrescentado à cópia):

| Estado | rc | Leitura |
|---|---|---|
| hoje, texto inteiro | 1 | a linha de `power_source` cita testes que ainda não existem: reprova, como deve |
| P3 (testes de P1/P2 registrados), sem a linha [P4] | **0** | 0 sem citação |
| P3 com a linha [P4] (controle) | 0 | **o portão NÃO reprova**, porque `gl_context_parity_test` já existe: é P0-I5 |
| P4, texto inteiro | 0 | |

**O que NÃO foi executado, dito sem maquiagem (L-06 global):** a metade Windows do portão de colisão (esta máquina só tem o frontend GCC; o SDK do Windows não foi varrido); o `header_hygiene_test` real (é de P3 e B4, na árvore); nenhum dos testes de produto (não existem).

---

## 7. Ordem de entrada das linhas (D-W7D-R2)

- **[P3]** tudo o que o diff mostra, EXCETO o bloco abaixo. A citação de `power_supply_rule_test` e `power_status_rule_test` exige os testes de P2 registrados antes (a simulação 6.5 prova que passa assim).
- **[P4]** exatamente estas duas linhas, inseridas byte a byte no commit de P4, com a célula "perguntar não grava" do `gl_context_parity_test`:
```
//      Proved by: gl_context_parity_test (every row read before and
//      after asking twice: identical, on a real context).
```
O fechamento compara o cabeçalho publicado com o diff da seção 2 deste parecer, por máquina; diferença só com registro do C-level decisor.

---

## 8. FATO e INFERÊNCIA, separados

- **FATO** (medido nesta sessão, com comando ou `arquivo:linha`): tudo nas seções 3, 4 e 6 que traz linha ou saída de comando; o md5 do plano; o SHA do blob.
- **INF** (julgamento meu, pode estar errado): que ler "2" sem nome "anula o item" (P0-I1); que `index` hoje não quebra de fato por ser macro de função (P0-I4); que `vsync` e `gpu_preference` merecem a mesma correção (gêmeo recomendado, não medido como pedido de consumidor).
- **Nada foi decidido com base em uso de um consumidor** (L-21 global, frase-guarda): nenhum argumento deste parecer cita o GusWorld.
<!-- APENDICE-INICIO: saida de tools/auditoria_texto_congelado.py, nunca editar a mao -->

## Apêndice: texto final congelado

Gerado a partir das cercas da seção 2, com as emendas aplicadas (E1, E2). Cada hunk de diff aparece como o texto final (contexto e linhas acrescentadas, sem as removidas).

````cpp
// ---- @@ -118,7 +118,56 @@ ----
    preset = 5,
    auto_choice_reason = 6,
    power_source = 7,
    // GFX-PRESET (docs/auditoria-api-gfx-preset.md, P0): `choice`,
    // `read_only`. The preset this library would suggest RIGHT NOW,
    // computed at the moment it is read - see the "SUGGESTED PRESET"
    // block below this enum for the full contract.
    suggested_preset = 8,
};

// ============================================================
// THE VALUES OF THE PRESET ROWS - DATA CONTRACT, FOREVER
// ============================================================
//
// Every `choice` row reads and writes a plain std::int64_t (item 2 of
// this header's own top comment). For the four rows below, the meaning
// of each number is part of the DATA contract (a consumer's saved
// settings file stores the number): once shipped, a number is never
// reused for a different meaning and never renumbered. The named
// constants exist so that no consumer has to write a bare number; the
// constants and the numbers are the same thing, spelled two ways.
//
// `preset` (id 5, `live`) and `suggested_preset` (id 8, `read_only`)
// share ONE vocabulary, on purpose: the value read from
// `suggested_preset` can be written straight back into `preset`.
// `suggested_preset` only ever reads power_saving, balanced or
// performance - never manual, never automatic.
inline constexpr std::int64_t k_gltfx_preset_manual = 0;
inline constexpr std::int64_t k_gltfx_preset_power_saving = 1;
inline constexpr std::int64_t k_gltfx_preset_balanced = 2;
inline constexpr std::int64_t k_gltfx_preset_performance = 3;
inline constexpr std::int64_t k_gltfx_preset_automatic = 4;

// `vsync` (id 0, `live`): the numbers of the option, wherever it is
// read - including the entries gltfx_gfx_preset_row_at() hands back.
// Same data-contract rule as the rows above: never reused, never
// renumbered.
inline constexpr std::int64_t k_gltfx_vsync_off = 0;
inline constexpr std::int64_t k_gltfx_vsync_on = 1;
inline constexpr std::int64_t k_gltfx_vsync_adaptive = 2;

// `auto_choice_reason` (id 6, `read_only`): WHY `suggested_preset`
// reads what it reads. `none` is never produced by this version (every
// suggestion has a reason); it stays in the vocabulary because the
// vocabulary is append-only.
inline constexpr std::int64_t k_gltfx_auto_choice_reason_none = 0;
inline constexpr std::int64_t k_gltfx_auto_choice_reason_on_battery = 1;
inline constexpr std::int64_t k_gltfx_auto_choice_reason_software_renderer = 2;
inline constexpr std::int64_t k_gltfx_auto_choice_reason_shared_gpu = 3;
inline constexpr std::int64_t k_gltfx_auto_choice_reason_dedicated_gpu = 4;
inline constexpr std::int64_t k_gltfx_auto_choice_reason_unknown_gpu = 5;

// `power_source` (id 7, `read_only`): what the operating system reports
// about where the machine's power comes from. `unknown` is a
// first-class answer on both systems (a machine with no battery and no
// readable supply, such as most virtual machines, reads `unknown`).
// Proved by: power_supply_rule_test (Linux rule, every cell of the
// closed set) and power_status_rule_test (Windows rule, every cell).
inline constexpr std::int64_t k_gltfx_power_source_unknown = 0;
inline constexpr std::int64_t k_gltfx_power_source_mains = 1;
inline constexpr std::int64_t k_gltfx_power_source_battery = 2;

// The three shapes a value can take (D-W6b-16 (2)): `toggle` reads
// 0/1, `choice` reads the numeric id of the chosen value (documented
// ---- @@ -208,4 +257,65 @@ ----
[[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_gfx_option>
gltfx_gfx_option_by_name(std::string_view name) noexcept;

// ============================================================
// SUGGESTED PRESET - A SUGGESTION HANDED OVER, NEVER LIBRARY STATE
// ============================================================
//
// The leader's order of 06/09/2026 (ESCOPO.md): the automatic choice
// only HANDS OVER the best values for the consumer to use on their
// own side; it locks nothing. What that means, rule by rule:
//
//   1. READING NEVER WRITES. option(suggested_preset) and
//      option(auto_choice_reason) compute the suggestion at the moment
//      they are called, from two facts the system reports (the kind of
//      GPU this context runs on, and option(power_source)). Reading
//      them changes no option and no label.
//      Proved by: gl_context_parity_test (every row read before and
//      after asking twice: identical, on a real context).
//   2. APPLYING IS ALWAYS YOUR REQUEST. set_option({preset, P}) with a
//      concrete P applies every row of P, all or nothing, once.
//      set_option({preset, k_gltfx_preset_automatic}) is a shortcut for
//      "apply the suggestion of right now": it applies the suggested
//      concrete preset, once, and stores THAT concrete value as the
//      label - option(preset) never reads `automatic`.
//   3. THE LABEL IS YOURS. option(preset) reads the last preset you
//      asked for (default `manual`), not a verdict on whether the
//      current values still match it. Changing one row by hand after
//      applying a preset never rewrites the label.
//   4. NOTHING IS EVER REAPPLIED OR PERSISTED BY THE LIBRARY ON ITS
//      OWN. A power cable plugged in or pulled out changes what
//      option(suggested_preset) reads next time, and nothing else.
//
// Two readings in a row may disagree: the power source can change
// between them. That window is real and is not hidden.
//
// `balanced` and `performance` hold the SAME rows in this version, on
// purpose: the only rows a preset touches today are vsync and
// frame_rate_cap, and turning vsync off trades tearing for speed, which
// is a separate choice from "performance". They start to differ when
// the library grows options that can actually be spent on quality.
// Proved by: gfx_preset_table_test (every row of every preset, value by
// value).

// How many rows the preset `preset` sets, in THIS build. Zero for
// k_gltfx_preset_manual, for k_gltfx_preset_automatic (it resolves to
// a concrete preset first) and for any number outside the vocabulary
// above. Never a literal a consumer hardcodes: the rows of a preset are
// the library's current best values and may change in a later release.
[[nodiscard]] GLINTFX_API std::size_t gltfx_gfx_preset_row_count(std::int64_t preset) noexcept;

// The row at `row_index` (0 <= row_index < gltfx_gfx_preset_row_count(
// preset)) of the preset `preset`: which option it sets, and to what.
// Read the suggestion, read its rows, and decide on your side what you
// apply (the leader's "best values for the consumer to use").
//
// An out-of-range `preset` or `row_index` degrades (docs/api-
// conventions.md R4) to the entry {suggested_preset,
// k_gltfx_preset_manual}: an id that is read_only, so handing it to
// set_option() by mistake is refused with invalid_argument and
// changes nothing - neither a row nor your preset label. It is never
// a default-constructed entry, because {vsync, 0} would silently
// turn vsync off.
[[nodiscard]] GLINTFX_API gltfx_gfx_option_entry
gltfx_gfx_preset_row_at(std::int64_t preset, std::size_t row_index) noexcept;

} // namespace glintfx
````

<!-- APENDICE-FIM -->
