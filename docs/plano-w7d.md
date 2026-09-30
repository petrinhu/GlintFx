# Plano da onda W7-D: predefinição gráfica, desenho 2D em lote, entrada

**Autor:** C-level planejador (`opus`, esforço alto), sob modo autônomo (L-34 emendada em 22/09/2026). Eu planejo, não implemento.
**Data:** 23/09/2026, `HEAD` = `da1e325` (árvore com `TODO.md` e `tests/tools/check_layers.py` modificados por OUTROS agentes, não por mim).
**Única escrita deste agente:** este arquivo.

> **Como ler.** `FATO` = medido por mim nesta sessão, com comando ou `arquivo:linha`. `FONTE` = achado de pesquisa externa, com URL. `INFERÊNCIA` = julgamento meu, pode estar errado. `DECISÃO AUTÔNOMA` = decidi no lugar do líder (L-34), com as fontes que a sustentam; o main registra em `DECISOES_AUTONOMAS.md`.
>
> **Fontes internas que o implementador lê INTEIRAS (L-27 item 3, passar a fonte e não o resumo):**
> - `/home/petrus/IDrive/Documentos/projetos_claudebrain/Projects/GlintFx/docs/plano-w6b-fatias-5.md` §2 (D-W6b-34, 35, 44) e §5 inteira (fatia 5c): é o desenho de `GFX-PRESET`, válido com as correções da seção 3 abaixo.
> - `/var/tmp/glintfx-plan/entrada-teclado-ponteiro.md`: o plano parcial da entrada; o desenho D1 a D10 continua valendo, com as emendas da seção 5 abaixo.
> - `/home/petrus/.claude/projects/-home-petrus-IDrive-Documentos-projetos-claudebrain-Projects-GlintFx/memory/project_decisoes_api_desenho_2d.md`: as três decisões do líder sobre desenho 2D (não se reabrem).
> - `docs/api-conventions.md` (R1 a R10), `GODS_LAWS.md` do projeto (L-04, L-09, L-17, L-19, L-20, L-22, L-32, L-35, L-40), `/home/petrus/.claude/GODS_LAWS.md`.

---

## 0. Resumo executivo

| Item | Estado medido | O que este plano faz |
|---|---|---|
| `GFX-PRESET` | ⏳; mecanismo inexistente (FATO 1.1) | Executa o desenho já pronto de 06/09 (fatia 5c), com três correções; 6 sub-fatias. |
| `WL-WRITE-TIMEOUT-NAO-FATAL` | ✅ | Nada. |
| `R2D-BATCH` | ⏳; nenhuma linha de desenho existe (FATO 1.2) | Desenho completo da API pública de desenho 2D (porta de mão única), 8 sub-fatias, revisão de API dedicada como primeira. |
| `INPUT-EVENTS` | ⏳; a coluna lista 4 pré-requisitos (2 na W9, 2 na W11c) e há um CICLO com `WIN-INPUT` (FATO 1.3, re-medido após a objeção do team-lead de 23/09 06:31) | **DIVIDIDO pela dependência real** (D-W7D-01 refeita): tecla física, ponteiro, foco e estado vão para a W9 sem esperar o parser (decisão do líder de 22/09); o que de fato precisa do parser XKB (texto; modificador lógico com trava e latch) vira duas fatias novas na W11c; o ciclo se desfaz com um átomo interno próprio no começo da W9. |
| `CI-VERDE-W7D` | ⏳ | Definição de fechamento da onda (seção 6). |

**Porte:** 2 itens de conteúdo, 14 sub-fatias, 2 revisões de API dedicadas, 8 fatias novas criadas por completude ou para desfazer bloqueio (seção 8), 17 decisões autônomas (seção 9; a D-W7D-01 refeita em 23/09).

---

## 1. O que medi antes de decidir (FATO)

### 1.1 `GFX-PRESET`

- As três linhas de opção existem e são aceitas sem efeito: `include/glintfx/platform/gl/gfx_option.hpp:118-120` (`preset = 5`, `auto_choice_reason = 6`, `power_source = 7`); `src/platform/gl/gfx_option_registry.hpp` linhas da tabela com `preset` `choice`/`live` 0..4 e as duas outras `read_only`.
- Os adaptadores aceitam `preset` sem efeito: `src/platform/wayland/egl_context_adapter.cpp:908-911` e `src/platform/win32/wgl_context_adapter.cpp:658` ("accepted here with no adapter-side effect yet").
- `grep -rn suggested src include` não acha `suggested_preset`; `ls src/platform/port/` não tem `power_source_port.hpp`; não existe `gfx_preset_table`, `auto_preset_rule`, `preset_expansion`, `power_supply_rule`, `power_status_rule` nem adaptador de energia.
- A fachada guarda opções em `current_values` (`src/platform/gl/gl_context_facade.cpp:332,401,416`), exatamente como o F16 do plano de 06/09 descreve.
- **Mudou desde 06/09, e muda o valor do item:** o `frame_rate_cap` agora TEM efeito, pelo laço (`include/glintfx/platform/loop/loop.hpp`, promessa P7; `src/platform/loop/loop_context_view.hpp:44`). Logo `power_saving = {vsync=on, frame_rate_cap=30}` passa a ter efeito real e medível, o que em 06/09 não tinha.
- **Mudou desde 06/09, e invalida uma assinatura do plano antigo:** a regra R3 (`docs/api-conventions.md`, portão `noexcept_alloc_test`, 17-18/09) proíbe alocação que lança dentro de função `noexcept`. O plano de 06/09 desenhou `expand_preset(...) noexcept -> std::vector<...>` (§5.1). Isso reprova no portão. Correção na seção 3.
- Texto público: a matriz (`docs/gl-loop-portability-matrix.md:32`) já declara "Accepted, not refused - and currently a no-op"; **a wiki NÃO declara** (`docs/wiki/API-Graphics-Context.md:63-65` descreve `preset` como se funcionasse). O item exigia que o texto público dissesse; metade diz.

### 1.2 `R2D-BATCH`

- Pré-requisitos `GL-LOADER`, `GL-CONTEXT`, `CORE-MATH2D`, `CORE-COLOR` estão todos ✅ (memória `project_decisoes_api_desenho_2d`, medido em 19/09; conferido na tabela hoje pelo `awk` da sessão). **A célula de `R2D-BATCH` ainda diz "Bloqueado transitivamente enquanto GL-API não é decidido"** (`TODO.md:553`), e a de `DEMO-1` (`TODO.md:565`) também: texto podre, o bloqueio não existe. O main corrige as duas células com o motivo.
- Não existe uma linha de desenho: `ls src/render/` = `CMakeLists.txt gl_abi.hpp gl_proc_address.hpp`. O carregador GL gerado (`gl_function_table`, `load_gl_functions(gl_proc_address_fn) noexcept`, emitido por `tools/gl_registry_codegen/loader_codegen.cpp:88-100`) está ligado à biblioteca (`src/render/CMakeLists.txt:106`) e **nenhum código de produção o chama ainda**: o desenho será o primeiro usuário.
- **Armadilha medida:** `gl_proc_address_fn` é `void *(*)(const char *)` (`src/render/gl_proc_address.hpp`), sem contexto; a resolução pública é método (`gltfx_gl_context::proc_address(std::string_view)`, `include/glintfx/platform/gl/context.hpp:230`). O carregador precisa de um resolvedor que saiba de QUAL contexto: ou o gerador passa a emitir uma forma com `void *user`, ou há um trampolim. Decisão D-W7D-15.
- O Windows já resolve as funções do GL 1.1 que o `wglGetProcAddress` não devolve, caindo em `opengl32.dll` (`src/platform/win32/wgl_proc_address.cpp:81-96`). Armadilha conhecida já fechada.
- Tipos de núcleo: `gltfx_vec2_world` (double) e `gltfx_vec2_screen` (float) sem conversão implícita (`include/glintfx/core/vec2.hpp:98-107`); `gltfx_rgba` = quatro float, luz **linear**, alfa **reto** (`include/glintfx/core/color.hpp`, decisões 3 e 5 do líder de 26/08); `gltfx_mat3`, `gltfx_transform`, `gltfx_rect_world`/`_screen` existem.
- **A decisão de precisão já congelada diz como a matriz chega à placa** (`vec2.hpp`, bloco "THE DECISION OF THE PROJECT LEADER"): *"The matrix handed to the graphics card is born in SINGLE precision and is computed ONCE PER FRAME, never once per object."* O desenho tem de honrar isso junto com o mundo em double.
- Camadas: `render/` é exceção documentada do portão de camada (cabeçalho de `tests/tools/check_layers.py` no `HEAD`, linhas 21-23: "render/ is a documented, deliberate exception ... the GL loader"), e é usada por baixo pelos adaptadores de contexto (`egl_context_adapter.cpp`, `wgl_context_adapter.cpp`). **O desenho 2D fica ACIMA de `platform/gl`**; morar em `render/` misturaria a peça de baixo com a de cima. Decisão D-W7D-14.
- `check_layers.py` está sendo alterado AGORA por outro agente (`git diff --stat`: 448+/66-), pela fatia `LAYERS-GATE-GFSS-GFUI` (W7-B, 🔍). **Colisão de arquivo**, seção 7.
- O teste de paridade GL roda "live" nos dois sistemas (`docs/gl-loop-portability-matrix.md`, linha de abertura/apresentação: "proven identical, live, not by construction"). É o trilho em que a prova por leitura de pixel vai andar.
- `tests/container/Containerfile` tem 22 linhas `g++ -std=c++23` hoje (`grep -c`); cada executável novo de container acrescenta a sua e o portão `check_container_fixture_includes.py` a confere.

### 1.3 `INPUT-EVENTS`

- **Cadeia de dependências, medida em 23/09 por `awk` sobre a coluna `Depende de` do `TODO.md` (re-medida depois da objeção do team-lead, que estava CERTA sobre a coluna):**

  ```
  INPUT-EVENTS        W7-D  dep = WL-KEYBOARD, WL-POINTER, KEYMAP-MODSTATE, KEYMAP-UTF8   ⏳  (TODO.md:554)
    WL-KEYBOARD       W9    dep = WL-SEAT (✅ W6a)                                         ⏳  (:538)
    WL-POINTER        W9    dep = WL-SEAT (✅ W6a)                                         ⏳  (:539)
    KEYMAP-MODSTATE   W11c  dep = KEYMAP-RESOLVE, KEYMAP-PARSE-COMPAT                      ⏳  (:506)
    KEYMAP-UTF8       W11c  dep = KEYMAP-RESOLVE                                           ⏳  (:502)
      KEYMAP-RESOLVE      W11c dep = KEYMAP-PARSE-CORE                                    ⏳  (:497)
      KEYMAP-PARSE-COMPAT W11c dep = KEYMAP-PARSE-CORE                                    ⏳  (:498)
      KEYMAP-PARSE-CORE   W11c dep = KEYMAP-LEX                                           ⏳  (:483)
      KEYMAP-LEX          W11c dep = FUND-4 (✅)                                          ⏳  (:433)
  WIN-INPUT           W9    dep = WIN-WINDOW (✅ W6a), INPUT-EVENTS                        ⏳  (:628)
  KEYMAP-COMPOSE      W12   dep = KEYMAP-MODSTATE, KEYMAP-UTF8                             ⏳  (:645)
  ```

  **Três fatos saem disso:** (1) a coluna de `INPUT-EVENTS` ainda carrega os dois `KEYMAP-*`, **mas a decisão do líder de 22/09 já os tirou**: `DECISOES_AUTONOMAS.md:3910`, verbatim do registro, *"a parte da tecla passa a funcionar sem esperar o parser XKB próprio ... `INPUT-EVENTS` deixa de depender de `KEYMAP-MODSTATE`/`KEYMAP-UTF8`"*; a coluna nunca foi atualizada (mesma família do texto podre de `R2D-BATCH`). (2) **O que os dois `KEYMAP-*` realmente entregam não pode ser feito sem o parser inteiro** (cinco elos, todos W11c, trilha paralela que a L-32 dá ao RCSS até a W10 por decisão do líder, e que ele mesmo moveu para W11 em 08/09): trazer a cadeia para antes é reverter duas decisões dele. (3) **Há um CICLO:** `WIN-INPUT` depende de `INPUT-EVENTS`, e `INPUT-EVENTS` não fecha sem o lado Windows (L-04). Mover `INPUT-EVENTS` para depois de `WIN-INPUT` dentro da W9 não resolve; a primeira versão deste plano não viu isso.
- **O que exatamente depende do parser, medido na fonte do protocolo e não presumido:** o evento de modificadores do Wayland entrega máscaras de bits (`mods_depressed`, `mods_latched`, `mods_locked`), e *"the interpretation of modifiers is keymap-specific"* ([Wayland Book, Keyboard input](https://wayland-book.com/seat/keyboard.html); [libxkbcommon, Keyboard State](https://xkbcommon.org/doc/current/group__state.html)): só as oito posições reais (Shift, Lock, Control, Mod1 a Mod5) têm ordem de convenção, e **qual `ModN` é Alt, NumLock ou Super é o keymap que diz**. Consequência: (i) o **texto** precisa de `KEYMAP-UTF8`; (ii) o **modificador lógico** (Alt como o leiaute define, trava de maiúsculas, latch de tecla de aderência de acessibilidade) precisa de `KEYMAP-MODSTATE`; (iii) **tecla física, repetição, foco, ponteiro e "quais teclas modificadoras FÍSICAS estão abaixadas"** (Shift esquerdo, Ctrl direito etc., derivado do próprio estado de teclas) **não precisam de nada do parser**.
- `ls src/platform/input/` = só `seat_capabilities.{hpp,cpp}`: nenhum adaptador de teclado ou ponteiro existe.
- `gltfx_input_event` está declarada e nunca definida (`loop.hpp:139`), `on_event` está no layout congelado (`loop.hpp:160,227`) e `run()` o recusa (`loop.hpp:223`, P9).
- **Decisão do líder 2 de 22/09** (`DECISOES_AUTONOMAS.md`, bloco "22/09/2026 - 09:20"): fechar as ondas antigas ANTES da onda de entrada. **A contradição medida:** W7-D é onda antiga, e o item de entrada dela não pode fechar sem a onda de entrada (W9) vir antes. Resolução: D-W7D-01.
- Consumidor pelo barramento (12/09, `TODO.md:554`): (1) duas direções ao mesmo tempo, entrada por estado; (2) eixo analógico contínuo; (3) teclado e gamepad pelo mesmo caminho. Ordem do líder `D-091201`: *"A quina é nossa"*, a biblioteca entrega o passo já resolvido; e a escolha "átomo de grade próprio ou esperar o mapa" ficou para o planejamento da fatia (`DECISOES_AUTONOMAS.md:3111`). Resolvida em D-W7D-02.
- Já existe na trilha de mapa a peça certa para a quina: `MAP-COLLIDE-GRID` (W11a, `TODO.md:521`, "consulta de colisão contra a grade ... rect vs células bloqueadas") e `MAP-PATH-ASTAR` já nasce "SEM corner cutting (caso diagonal-canto nasce como teste nomeado vermelho)" (`TODO.md:552`).
- Gamepad: `GP-MAP` em W11c (`TODO.md:558`).

### 1.4 Versão

- `CMakeLists.txt:43` = `0.5.0.0`; última marca `v0.5.0.0` (`git describe --tags --abbrev=0`).

---

## 2. Pesquisa (L-43, L-44, L-34 emenda de 09/09), por item

Licenças conferidas na fonte hoje (L-29), por `gh api repos/<r>/license`: SDL **Zlib**, sokol **Zlib**, sokol_gp **MIT-0**, bgfx **BSD-2-Clause**, raylib **Zlib**, NanoVG **Zlib**, GLFW **Zlib**, Godot **MIT**; MonoGame e LÖVE devolveram `NOASSERTION` pela API (MonoGame declara Ms-PL, LÖVE declara zlib no próprio repositório; **não conferido por mim na fonte**). **Li apenas documentação, páginas de issue e fóruns, nenhum código-fonte**; nenhuma linha foi copiada. Onde a licença não foi conferida, nada além de documentação pública foi lido.

### 2.1 `R2D-BATCH`: como o mercado agrupa desenho, e onde dói

**Como se faz:**

| Biblioteca | Técnica | Fonte |
|---|---|---|
| raylib (rlgl) | Acumula vértices; **troca de textura** fecha um "draw call"; buffer de tamanho fixo (`RL_DEFAULT_BATCH_BUFFER_ELEMENTS`) força descarga ao encher. | [rlgl.h](https://github.com/raysan5/raylib/blob/master/src/rlgl.h), [raylib#1320](https://github.com/raysan5/raylib/issues/1320) |
| LÖVE 11 | Agrupamento automático enquanto o estado não muda; mudança de estado (imagem, shader, canvas) descarrega; `flushBatch` explícito. | [love.graphics.flushBatch](https://love2d.org/wiki/love.graphics.flushBatch) |
| SDL (render) | Agrupamento sempre ligado no SDL3; contrato explícito: **quem chama GL cru chama `SDL_FlushRenderer` antes**, senão comportamento indefinido. | [SDL2 HINT_RENDER_BATCHING](https://wiki.libsdl.org/SDL2/SDL_HINT_RENDER_BATCHING), [SDL3 SDL_FlushRenderer](https://wiki.libsdl.org/SDL3/SDL_FlushRenderer), [SDL#8584](https://github.com/libsdl-org/SDL/issues/8584) |
| sokol_gp | Fila de comandos; otimizador olha os **8 últimos** comandos e só reordena para juntar se **não houver comando intermediário que sobreponha**; ganho medido de até 2,2x. | [sokol_gp README](https://github.com/edubart/sokol_gp) |
| bgfx | Chave de ordenação de 64 bits por chamada; modo **sequencial** (ordem de submissão) ou por profundidade, escolhido por "view". | [bgfx internals](https://bkaradzic.github.io/bgfx/internals.html), [Order your draw calls around!](https://realtimecollisiondetection.net/blog/?p=86) |
| MonoGame/XNA | `SpriteSortMode`: `Deferred` (submissão), `BackToFront`/`FrontToBack` por profundidade, `Texture` por textura. | [SpriteSortMode](https://docs.monogame.net/api/Microsoft.Xna.Framework.Graphics.SpriteSortMode.html) |
| GL 3.3 streaming | Órfão do buffer (`glBufferData` com nulo, ou `glMapBufferRange` com `INVALIDATE`) para não esperar a placa; ou anel de buffers. | [OpenGL Wiki, Buffer Object Streaming](https://wikis.khronos.org/opengl/Buffer_Object_Streaming) |

**As dores, cada uma virando requisito:**

1. **Ordenação instável troca a ordem de iguais entre quadros: pisca.** MonoGame `BackToFront` usa ordenação instável; sprites de mesma profundidade trocam de ordem entre quadros ([SpriteSortMode](https://learn.microsoft.com/en-us/previous-versions/windows/xna/bb199041(v=xnagamestudio.10)), [fórum](https://community.monogame.net/t/preserve-depth-across-multiple-spritebatches/14260)). → **R-B1: ordem total e determinística, empate preserva a submissão, provada célula a célula** (já é emenda do líder de 27/08).
2. **Agrupar junta o que não podia.** raylib: um desenho saiu com a textura do comando anterior porque o agrupador juntou comandos incompatíveis ([raylib#6110](https://github.com/raysan5/raylib/issues/6110)); e o inverso, peças da mesma textura que não agrupam ([raylib#4849](https://github.com/raysan5/raylib/issues/4849)). → **R-B2: a chave de estado é explícita e completa, e o teste prova as duas direções (junta quando pode, separa quando deve).**
3. **Estado de GL vazando entre a biblioteca e o consumidor.** NanoVG altera programa, mistura, recorte de face, alinhamento de desempacotamento, e quem desenha depois vê a cena sumir ([nanovg#285](https://github.com/memononen/nanovg/issues/285)). SDL precisou do contrato de descarga. → **R-B3: a biblioteca não confia em NENHUM estado de GL ao começar a descarregar (define tudo de que depende) e declara o estado em que deixa o GL; teste com estado hostil plantado antes.** E **R-B4: descarga explícita no meio do quadro**, com contrato escrito, para quem mistura GL cru (o nosso contexto EXPÕE `proc_address()`, então esse consumidor existe por construção).
4. **Buffer de tamanho fixo que estoura.** raylib: "Batch elements overflow" ([raylib#1401](https://github.com/raysan5/raylib/issues/1401)). → **R-B5: capacidade cresce; reserva opcional na abertura; falta de memória nunca derruba o processo (R3) e é contada.**
5. **Mapear buffer trava o driver.** Relatos de `glMapBufferRange` lento e de o driver esperar a placa antes de deixar escrever ([gamedev.net 666461](https://gamedev.net/forums/topic/666461-map-buffer-range-super-slow/), [Zach Bethel](https://zachbethel.wordpress.com/2013/03/20/buffer-streamin-opengl/)). → **R-B6: técnica de envio escolhida por MEDIÇÃO nos alvos, nunca prometida como número.**
6. **Franja escura em borda semitransparente.** Alfa reto misturado produz contorno escuro; alfa pré-multiplicado resolve ([Adrian Courrèges](https://www.adriancourreges.com/blog/2017/05/09/beware-of-transparent-pixels/)). → **R-B7: mistura pré-multiplicada por dentro** (transitória, que é exatamente a decisão 5 do líder sobre cor).
7. **Costura e sangria em atlas, meio pixel.** ([webglfundamentals](https://webglfundamentals.org/webgl/lessons/webgl-qna-how-to-prevent-texture-bleeding-with-a-texture-atlas.html)). É de `R2D-TEXTURE`, mas **o formato do vértice nasce aqui**; → **R-B8: a coordenada de textura e a convenção de pixel (origem no canto superior esquerdo, borda do retângulo na borda do pixel) ficam fixadas agora**, para a textura não reabrir o formato.

**O que a busca não achou, declarado:** nenhuma biblioteca C/C++ do tipo documenta, como cláusula de contrato, que a ordem é **total e reprodutível entre execuções**; bgfx no modo sequencial chega perto, sem promessa escrita. Nenhuma separa, como nós, mundo em double e tela em float; todas usam um tipo só.

### 2.2 `INPUT-EVENTS`: acréscimos à pesquisa do plano parcial

O plano parcial (`entrada-teclado-ponteiro.md` §2) já cobre fila × retorno de chamada (SDL3, GLFW, GLEQ), tecla presa ao perder foco (Waydroid, WSLg), `MAP_PRIVATE` do keymap, repetição no cliente Wayland (SDL#3119, SDL#12866), grupo do ponteiro até `frame`, carimbo de 32 bits. Acrescento o que muda o desenho:

1. **Toque curto perdido entre dois quadros.** raylib: `IsKeyPressed` perde tecla apertada e solta no mesmo quadro, comum em quadro lento e em teclado com firmware QMK ([raylib#4591](https://github.com/raysan5/raylib/issues/4591), [raylib#2827](https://github.com/raysan5/raylib/issues/2827)); GLFW documenta o mesmo e oferece "sticky keys" como remendo ([GLFW input guide](https://www.glfw.org/docs/3.3/input_guide.html)); SDL idem para `SDL_GetKeyboardState` ([SDL3 wiki](https://wiki.libsdl.org/SDL3/SDL_GetKeyboardState)); análise completa com a solução por fila em [Reg, "Lost clicks and key presses on low FPS"](https://dev.to/reg__/lost-clicks-and-key-presses-on-low-fps-3cka). → **R-I1: o retrato de estado por passo carrega, além de "pressionada agora", a CONTAGEM de descidas e subidas naquele passo. Um toque curto nunca some.** Isso é o requisito (1) do consumidor feito sem a armadilha.
2. **Tecla física × símbolo.** WASD num AZERTY vira ZQSD: jogo quer a posição física; atalho quer o símbolo ([SDL3 BestKeyboardPractices](https://wiki.libsdl.org/SDL3/BestKeyboardPractices)). O SDL usa o código de uso HID (página 7) como identificador físico. → **R-I2: identificador físico da tecla = uso HID de teclado (página 0x07 do USB-IF), padrão da indústria; o símbolo por leiaute vem depois, por FUNÇÃO de consulta, não por campo reservado** (acréscimo compatível, sem mexer no layout do evento).
3. **Texto não se monta com tecla.** SDL: *"do not try to build this yourself on top of individual keypress events"*. Já é decisão do líder (dois eventos).
4. **Alt-Tab no Windows deixa Alt preso** (a janela não recebe a soltura) ([Microsoft Learn, Keyboard Input Overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input)); emscripten/GLFW: estado preso após perder foco ([emscripten#5122](https://github.com/emscripten-core/emscripten/issues/5122)). Confirma o D6 do plano parcial nos DOIS sistemas.
5. **Diagonal mais rápida que o cardeal, e zona morta.** Godot resolve com `Input.get_vector` (normaliza e aplica zona morta, mesma ação para teclado e controle) ([Bugnet](https://bugnet.io/blog/fix-diagonal-movement-faster-than-cardinal-godot), [fórum Godot](https://forum.godotengine.org/t/how-is-the-deadzone-from-input-map-supposed-to-work/70685)). → é o requisito (2)+(3) do consumidor, e é uma CAMADA acima do evento: fatia nova `INPUT-ACTIONS` (seção 8).
6. **Trava e captura do ponteiro no Wayland** é dor grande (Minecraft/GLFW) e dependente de compositor ([pointer-constraints](https://wayland.app/protocols/pointer-constraints-unstable-v1), [SDL#2105](https://github.com/libsdl-org/SDL/issues/2105)). Fora do escopo de `INPUT-EVENTS`; **não existe item para isso na tabela** (FATO: `grep -n 'pointer-constraints\|relative' TODO.md` sem linha de item). Vai para a INBOX (seção 8).

### 2.3 `GFX-PRESET`: acréscimos à pesquisa de 06/09

A pesquisa de 06/09 (`docs/plano-w6b-fatias-5.md` §1: BatteryBoost limitando a 30 quadros, `scope` do sysfs, `SYSTEM_POWER_STATUS`) continua válida. Acrescento:

1. **Detecção automática erra muito, nos dois sentidos** (otimista demais e trava, ou pessimista demais) ([Escapist](https://forums.escapistmagazine.com/threads/auto-detect-video-settings-in-pc-games.262578/), [Steam](https://steamcommunity.com/app/784150/discussions/0/3757725715254631215/)). Confirma a ordem do líder: sugestão, nunca trava, e **a razão entregue junto** (`auto_choice_reason`), para o consumidor mostrar ao jogador por que.
2. **Quadro sem teto em menu gasta 19% a 33% da energia da placa e faz barulho** ([ROG](https://rog.asus.com/articles/guides/how-to-improve-your-gaming-laptops-performance-on-battery-power/), [SmoothFPS](https://smoothfps.com/guides/laptop-gaming-performance)). Confirma que `power_saving` com teto de quadro é o valor que mais importa, e que hoje (com P7) ele tem efeito.
3. **O modo de economia do sistema** (Windows: bit `SystemStatusFlag` de `SYSTEM_POWER_STATUS`; Linux: perfil de plataforma do kernel) é pedido do USUÁRIO, e jogo que o ignora é reclamação recorrente. O plano de 06/09 o deixou como opção futura (`power_saver_requested`). **Não entra em `GFX-PRESET`** (seria escopo novo antes da demo, proibido pela extensão de 27/08 da L-32); vira fatia nova pós-demo (seção 8).

---

## 3. `GFX-PRESET`

### 3.1 Desenho

**É o desenho da fatia 5c de `docs/plano-w6b-fatias-5.md` (§5 e decisões D-W6b-34, 35 reescrita, 44), sem reabrir nada**, com estas correções, todas por fato medido depois de 06/09:

- **C1 (R3, FATO 1.1):** `preset_expansion` não devolve `std::vector` sob `noexcept`. Degrau 1 da R3: capacidade fixa (`std::array<gltfx_gfx_option_entry, N>` mais contagem), com `N` = número de linhas do registro (`static_assert` amarrando os dois, na forma que `gfx_option_registry.hpp` já usa). Nenhuma alocação no caminho de `set_option(preset=...)`.
- **C2 (texto público, FATO 1.1):** a wiki (`docs/wiki/API-Graphics-Context.md`), o comentário de `context.hpp:84`, a matriz (`gl-loop-portability-matrix.md:32`) e os comentários dos dois adaptadores (`egl_context_adapter.cpp:908`, `wgl_context_adapter.cpp:658`, e os `.hpp` correspondentes) passam a descrever o efeito real. O portão de fechamento conta que não sobrou frase dizendo "no-op"/"no adapter-side effect" sobre `preset` (seção 3.3).
- **C3 (P7 existe agora):** o teste de paridade que prova `power_saving` confere que o LAÇO obedece ao teto de 30 (a mesma medição por prazo que o P7 já usa), não só que `option(frame_rate_cap)` devolve 30. Valor escrito que ninguém obedece seria o defeito "afirma que mede e não mede".

**O que congela na API pública (porta de mão única, append-only):** a linha `suggested_preset` (id 8, `choice`, `read_only`, valores 1..3 iguais aos de `preset`), o nome de dado `"suggested_preset"` (contrato de DADO, L-26), e dois acessores livres `gltfx_gfx_preset_row_count(std::int64_t preset) noexcept` e `gltfx_gfx_preset_row_at(std::int64_t preset, std::size_t index) noexcept -> gltfx_gfx_option_entry` (degradam a entrada padrão fora do intervalo, R4). **A classe `gltfx_gl_context` não cresce.** Exige revisão de API dedicada, pequena (sub-fatia P0).

**Semântica que fica pública e que a revisão confere:** as oito regras de D-W6b-35 (reescrita): rótulo `preset` é do consumidor e a biblioteca nunca o reescreve; `automatic` aplica a sugestão de agora uma vez e grava o concreto; perguntar a sugestão não grava nada; entrada explícita da lista de abertura vence a expansão; `balanced == performance` hoje, declarado no cabeçalho com o porquê.

### 3.2 Sub-fatias, em ordem (um implementador por vez: todas tocam `tests/CMakeLists.txt` e três tocam a fachada)

| # | Sub-fatia | Nasce / muda | Teste vermelho de estreia exigida antes do verde (L-20, L-40) | Prova | Par no portão | Fechamento |
| --- | --- | --- | --- | --- | --- | --- |
| **P0** | Revisão de API dedicada (C-level revisor distinto de mim e do implementador) | Parecer escrito sobre os três nomes (R6: colisão com macro nos cinco alvos), degradação R4, `[[nodiscard]]` estrutural, texto do cabeçalho | - | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Parecer com veredito; achado bloqueante resolvido antes de P3 |
| **P1** | Átomos puros: `gfx_preset_table`, `auto_preset_rule`, `preset_expansion` (sem alocação, C1) | Três átomos de núcleo, sem `#if` | Os três testes (`gfx_preset_table_test` 12 células, `auto_preset_rule_test` 12 células, `preset_expansion_test`) compilados antes do `.cpp` existir; cada um com contagem impressa e piso não-vazio; `noexcept_alloc_test` verde sobre os três | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Suíte verde em container; contagens impressas batendo com as do plano de 06/09 §5.2 |
| **P2** | Leitura de energia: porta `power_source_port`, regra Linux (sysfs, 14 células, `scope`/`present` [LENTE-2/6]), regra Windows (`GetSystemPowerStatus`, célula própria `255`), os dois adaptadores e os dois `selected_*` | Energia lida do SISTEMA nos dois lados | `power_supply_rule_test` e `power_status_rule_test` vermelhos antes; `check_port_privacy.sh` **e** `.ps1` reprovam a classe nova antes de a lista a conhecer (os dois no mesmo commit) | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Os dois testes puros verdes nos cinco alvos (são puros, sem guarda de plataforma) |
| **P3** | Fachada e registro: linha id 8, `static_assert` da tabela, `check_gfx_option_ids.py`, as regras 1 a 8 de D-W6b-35 em `gl_context_facade.cpp`, leitura no ato de `suggested_preset`/`auto_choice_reason`/`power_source`, os dois adaptadores devolvendo `read_only_here` para o id 8 | O mecanismo inteiro | `static_assert` reprova o id 8 sem linha; o portão de ids reprova a linha sem registro | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Build limpo nos dois modos |
| **P4** | Paridade e texto: `tests/parity/gl_context_parity_test.cpp` (fotografia antes/depois de perguntar; rótulo do consumidor; `automatic` gravando o concreto; `power_saving` com teto OBEDECIDO pelo laço, C3), `Containerfile`, `measured_exceptions.txt` (`power_source`), matriz, wiki, `context.hpp`, comentários dos adaptadores (C2) | Prova viva nos dois sistemas e texto público verdadeiro | Na árvore de P3 sem a regra 7, o teste reprova com `perguntar mudou preset: antes 0, depois 2`; com o teto não aplicado, reprova pela cadência medida | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Paridade verde no container Linux e no job Windows do servidor, lido de `gh run view --json jobs` |
| **P5** | Revisão adversarial que EXECUTA (agente distinto) + ASan/UBSan (a fatia muda estado de processo: memória `feedback_espelho_local_antes_do_commit`) | Relatório com as mutações 7.10 a 7.22 do plano de 06/09 (as ainda aplicáveis) reproduzidas em CÓPIA fora da árvore, cada uma vista vermelha | - | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Relatório gravado; `tools/preci.sh --sanitizer-only` verde, código lido de variável |

### 3.3 Definição de fechamento de `GFX-PRESET`

Todas, medidas, nenhuma lida de relatório:
1. P0 a P5 com commit próprio citando `GFX-PRESET` (L-11), `Status` → 🔍 no commit de P4, ✅ só depois de P5 e da re-verificação do main.
2. `grep -rnE "no-op|no adapter-side effect" docs src include` filtrado por `preset` devolve **zero** linhas sobre `preset`, e a mesma busca sobre `vsync` continua achando o que achava (controle de que a busca olha). Contagem impressa.
3. As 24 células puras (12 + 12) mais 14 + 5 de energia impressas; zero células reprova.
4. `noexcept_alloc_test` verde sem linha nova em `tests/noexcept_alloc_exceptions.txt`.
5. Paridade verde nos dois sistemas.

---

## 4. `R2D-BATCH`

### 4.1 O que já está decidido e NÃO se reabre (FATO: memória `project_decisoes_api_desenho_2d` e `TODO.md:553`)

- Posição de MUNDO + UMA transformação por lote; sem transformação, a posição é pixel direto; nenhuma câmera com estado.
- Falha reportada UMA vez, ao apresentar o quadro; chamadas de desenho não devolvem nada.
- Imagem na v1 recebe pixels prontos (é `R2D-TEXTURE`, W8; aqui só o formato que a acomoda).
- Chave de ordem opcional (camada, inteiro); sem chave, a submissão manda; ordenação estável, empate preserva submissão; enumeração fechada de 8 células com contagem.
- Cor linear, alfa reto no tipo; pré-multiplicação só transitória. Mundo double, tela float; matriz para a placa em float, uma vez por quadro.

### 4.2 Desenho (proposta para a revisão de API dedicada B0; porta de mão única)

**Um handle por assunto (L-19 armadilha 1):** `gltfx_renderer_2d` (nome proposto; a revisão pode trocar), PIMPL, movível, não copiável, `GLINTFX_API`, cabeçalho `include/glintfx/draw2d/renderer_2d.hpp`. Tipos de valor em cabeçalhos próprios do mesmo diretório (um assunto por arquivo, L-17).

| Operação | Forma proposta | Por quê |
|---|---|---|
| Abrir | `static gltfx_rslt<gltfx_renderer_2d> open(gltfx_gl_context &context, const gltfx_renderer_2d_desc &desc) noexcept` | Falível de verdade (programa de sombreamento que não compila, buffer que não se cria, função GL ausente): é o ÚNICO ponto além do fim de quadro onde erro aparece. `desc` leva só `reserve_quads` (dica de capacidade, R-B5); lista de campos cresce só por acréscimo. |
| Começar quadro | `void begin_frame(const gltfx_frame_2d_desc &desc) noexcept` | `desc`: cor de limpeza opcional (`clear`, default ligado, preto opaco) . O tamanho do alvo é LIDO do contexto (pixel físico da superfície), nunca informado pelo consumidor (não há como mentir). |
| Começar lote | `void begin_batch() noexcept` e `void begin_batch(const gltfx_mat3 &world_to_pixel) noexcept` | A decisão do líder, literal: sem argumento = pixel direto; com argumento = UMA transformação para o lote inteiro. Lote termina no próximo `begin_batch`, `flush` ou `finish_frame`. |
| Desenhar | `void fill_rect(const gltfx_rect_world &rect, const gltfx_rgba &color) noexcept`; `void fill_quad(const gltfx_quad_world &corners, const gltfx_rgba &color) noexcept`; as duas com sobrecarga que recebe `gltfx_draw_layer layer` (inteiro de 32 bits com sinal) | Sem retorno (decisão do líder). `fill_quad` (quatro cantos) dá retângulo girado sem abrir lote por peça, e é a forma geral que `R2D-TEXTURE`/`R2D-TRANSFORM` reusam. |
| Barreira | `void flush() noexcept` | R-B3/R-B4: tudo o que foi submetido vai à placa AGORA; depois dela o consumidor pode usar GL cru. A ordenação por camada NÃO atravessa a barreira (documentado; é o comportamento do SDL). |
| Fim de quadro | `gltfx_rslt<gltfx_frame_2d_report> finish_frame() noexcept` | O ponto ÚNICO de relato (decisão D-W7D-08). Descarrega o que falta e devolve o relatório: peças submetidas, desenhadas, recusadas por valor inválido (com a PRIMEIRA razão como token, R7), lotes, barreiras, chamadas de desenho emitidas. Erro (`gltfx_err`) só quando o QUADRO se perdeu: falta de memória que descartou peças, falha de envio à placa. |

**Convenções que congelam junto (R-B8):** pixel direto = pixel FÍSICO da superfície do contexto; origem no canto superior esquerdo; eixo y para baixo (a convenção de todo 2D levantado: SDL, raylib, LÖVE); borda do retângulo na borda do pixel, centro do pixel em +0,5; regras de rasterização do GL garantem que dois retângulos lado a lado não se sobrepõem nem deixam fresta (declarar e provar com leitura de pixel).

**O que a API NÃO promete (escrito no cabeçalho, para ninguém inferir de uma ausência):** número de quadros por segundo ou de peças por chamada (medido e impresso, nunca prometido); desenho de mais de uma linha de execução; sobreviver a perda de contexto do driver (GL 3.3 sem extensão de robustez não detecta; declarado); cor acima de 1,0 aparecer mais clara que branco numa superfície de 8 bits (declarado: é corte da superfície, não do tipo).

**Tempo de vida:** o contexto tem de viver mais que o desenhador (mesma promessa P10 do laço); a revisão B0 decide se o desenhador segura o interior do contexto (sobrevive a mover o handle do contexto) ou se mover o contexto é proibido enquanto houver desenhador. Recomendação: segurar o interior, que é o que torna o mover inofensivo.

### 4.3 Desenho interno (não congela; é o que o implementador constrói)

- **Transformação na CPU, em double, por vértice; para a placa vai só a matriz pixel→recorte, em float, uma vez por quadro (D-W7D-09).** É o que honra ao mesmo tempo "mundo em double" e "matriz em float uma vez por quadro": o mundo nunca é estreitado antes de a câmera ser subtraída, que é a técnica de origem flutuante. Custo: quatro multiplicações de `mat3` por peça, na CPU. **Consequência que vale ouro e vai para o cabeçalho:** como a transformação é aplicada antes da placa, **trocar de lote não quebra o agrupamento na placa** (lote é conceito de submissão, não de chamada de desenho).
- **Ordem (D-W7D-10):** cada peça gravada com chave composta `(camada, índice de submissão)`, única por construção; ordenação comum sobre chave única = ordem total, estável e reprodutível **sem** `std::stable_sort` (que pode alocar buffer temporário). Peça sem camada = camada 0.
- **Agrupamento:** depois de ordenar, peças CONSECUTIVAS com a mesma chave de estado (programa, textura, modo de mistura) viram uma chamada. Na v1 há um estado só (textura branca 1x1 interna), então todo quadro sem barreira é uma chamada por limite de índice. Reordenar para juntar mais (técnica sokol_gp) é fatia própria, pós-textura (seção 8), porque sem textura não há o que juntar.
- **Formato de vértice (R-B8):** posição float2 (pixel), coordenada de textura float2, cor float4 linear pré-multiplicada = 32 bytes. Índices de 32 bits (sem teto de 16 384 peças por chamada) ou de 16 com quebra: decide B1 por medição.
- **Envio (R-B6):** técnica escolhida por medição em B1 entre órfão por `glBufferData(nulo)` + `glBufferSubData` e `glMapBufferRange` com invalidação; o número medido é impresso, nunca asseverado.
- **Estado de GL (R-B3):** a cada descarga, o desenhador define TUDO de que depende (programa, VAO, buffers, mistura e função, teste de profundidade desligado, estêncil desligado, recorte desligado, descarte de face desligado, máscara de cor, viewport, `GL_FRAMEBUFFER_SRGB` conforme a opção, alinhamento de desempacotamento para quando houver textura). Ao terminar, deixa programa/VAO/buffers desligados (zero) e declara isso no cabeçalho.
- **Espaço de cor (D-W7D-12):** segue a opção `srgb_framebuffer` do contexto (já pública, `open_only`). Ligada: GL converte e a mistura é em luz linear (fisicamente correta). Desligada: o programa codifica para sRGB na saída e a mistura acontece no espaço codificado (o que SDL, raylib e o padrão do LÖVE fazem). Os dois caminhos documentados e provados por leitura de pixel.
- **Valor inválido** (NaN, infinito, tamanho negativo, quad degenerado): a peça é descartada e CONTADA no relatório com o token da primeira razão; o quadro segue. **Falta de memória ao gravar:** degrau 2 da R3 (captura dentro do `try`), a peça é descartada e contada, e `finish_frame()` devolve erro `out_of_memory` com os campos de contagem.
- **Carregador GL com contexto (D-W7D-15):** o gerador passa a emitir `load_gl_functions(resolver com void *user)`; a forma antiga continua para os testes que já a usam, ou é migrada no mesmo commit. É mudança interna (nada em `include/`).
- **Programa de sombreamento embutido:** texto GLSL 330 core dentro da biblioteca; falha de compilação ou ligação em `open()` devolve `platform_failure` com `rejected_value` = `vertex_shader`/`fragment_shader`/`program_link` e o registro do driver como campo (R7: token + campo, nunca frase nossa).

### 4.4 Sub-fatias, em ordem

| # | Sub-fatia | Nasce / muda | Teste vermelho de estreia | Prova | Par no portão | Fechamento |
| --- | --- | --- | --- | --- | --- | --- |
| **B0** | **Revisão de API dedicada** (C-level revisor distinto de mim e do implementador), sobre o cabeçalho RASCUNHO da 4.2, antes de qualquer linha pública entrar | Parecer: R1 a R10 um por um; R6 (colisão de `gltfx_renderer_2d`, `fill_rect`, `fill_quad`, `flush`, `layer`, `gltfx_quad_world` etc. com macro nos cinco alvos: `flush` e `layer` merecem atenção); legibilidade (L-28, "uma pessoa que nunca viu entende lendo uma vez?"); tempo de vida (4.2); o ponto único de relato; se `fill_quad` entra na v1 | - | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Parecer com veredito; cabeçalho congelado anexado; divergência implementador × revisor vai ao C-level decisor (eu, no modo autônomo) com registro |
| **B1** | **Medição** (não é produto): no container (`llvmpipe`) e no job Windows do servidor, imprimir o estado de GL que o contexto deixa após `make_current`; confirmar VAO obrigatório no perfil core; comportamento de `GL_FRAMEBUFFER_SRGB` nos dois; leitura de pixel do buffer de trás após `glFinish` funciona nos dois; custo das duas técnicas de envio para 10 mil peças | Relatório com números CRUS (L-43 global: números primeiro, critério depois) | - | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Relatório escrito ANTES de B3; a escolha da técnica de envio e do tamanho de índice registrada com o número |
| **B2** | **Átomos puros** (sem GL): `quad_vertices` (mundo double → pixel float por transformação), `draw_order` (chave composta), `draw_run_merge` (corridas por estado), `frame_report_tally` (contagem e primeira razão) | Núcleo do desenho, testável sem janela | (a) **precisão:** peça em x = 16 777 217 com câmera em −16 777 216 sai em pixel 1,0 exato (e o mesmo teste estreitando ANTES de transformar dá 0,0: é a mutação que prova a decisão de precisão); (b) **ordem:** as 8 células {sem chave, igual, menor, maior} × {AB, BA} com contagem impressa e zero reprova; (c) **empate:** 1 000 peças de camada igual saem na ordem de submissão; (d) **agrupamento:** junta quando o estado é igual e separa quando difere (as duas direções da dor raylib#6110/#4849); (e) **falta de memória:** alocador armado falha no meio, a peça é contada como descartada e nada lança | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Suíte verde nos cinco alvos (são puros); `noexcept_alloc_test` verde |
| **B3** | **Camada e encanamento GL:** diretório `src/draw2d/` e `include/glintfx/draw2d/`; portão de camada estendido (plataforma NUNCA inclui `draw2d`; `draw2d` nunca inclui cabeçalho do sistema operacional); carregador com contexto; programa embutido; fluxo de vértices | A peça que fala GL | O portão reprova um `#include "draw2d/..."` plantado em `src/platform/` e um `<windows.h>` plantado em `src/draw2d/`, antes de o portão conhecer a camada (controles positivo, negativo e varredura vazia, L-40) | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Portão com autoteste verde; build limpo nos dois modos |
| **B4** | **Fachada pública** conforme o cabeçalho congelado em B0 | `gltfx_renderer_2d` usável | Os testes de B5 escritos e compilados contra o cabeçalho antes da fachada existir | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Build limpo; `header_hygiene_test` e `public_name_collision_test` cobrindo os cabeçalhos novos (a contagem de cabeçalhos varridos sobe; conferir o número impresso, não confiar) |
| **B5** | **Prova viva por leitura de pixel**, `tests/parity/draw2d_parity_test.cpp` (só API pública, sem `#if`, forma de `gl_context_parity_test.cpp`), rodando no container Linux e no job Windows | O desenho CERTO na tela nos dois sistemas | Cada célula vista vermelha contra uma CÓPIA sabotada (seção 4.5): 8 células de ordem sobre dois quadrados sobrepostos lidos no pixel; borda pré-multiplicada; `srgb_framebuffer` ligado e desligado; **estado hostil plantado** (profundidade ligada, mistura trocada, recorte de 1 pixel, descarte de face frontal, máscara de cor desligada) antes de `finish_frame`, e o pixel continua certo; quadro vazio relata zero peças; dois retângulos lado a lado sem fresta nem sobreposição; contagem de chamadas de desenho = 1 para N peças de estado igual | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Verde nos dois sistemas, lido de `gh run view --json jobs`; contagens impressas; `Containerfile` com a linha nova e `check_container_fixture_includes.py` verde |
| **B6** | **Texto público** (inglês internacional, L-21): `docs/draw2d-portability-matrix.md` (linha por promessa, Linux × Windows × como se prova, forma das matrizes existentes), página de wiki `API-Draw-2D.md`, e em `docs/api-conventions.md` a regra nova se a revisão B0 a criar ("o relato de desenho é único, no fim do quadro") | Consumidor sabe o que tem | O portão de citação/licença/travessão de documentos que existir reprova antes do texto certo | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Portões de documento verdes |
| **B7** | **Revisão adversarial que EXECUTA** (agente distinto), ASan/UBSan, re-verificação do main (L-12: sabotagem de FAMÍLIA DIFERENTE da do implementador e da minha, porque planejador e orquestrador são ambos Opus, L-34 emenda de 22/09) | Relatório com as cinco perguntas da L-17 respondidas por unidade criada | - | — (fatia ainda não iniciada; nenhum teste registrado) | — (fatia ainda não iniciada; nenhum par de portão registrado) | Relatório; `preci.sh` e `--sanitizer-only` verdes com código lido de variável |

**E no fim:** o main corrige as células podres de `R2D-BATCH` e `DEMO-1` ("Bloqueado transitivamente ... GL-API") com o motivo.

### 4.5 Mutações obrigatórias (cada uma em CÓPIA fora da árvore, L-27 global; commitar antes de sabotar)

| Mutação | Onde | Quem tem de reprovar | Mensagem esperada (forma) |
|---|---|---|---|
| Estreitar para float ANTES de transformar | `quad_vertices` | B2 (a) | `pixel esperado 1.0, obtido 0.0` |
| Ignorar a camada (só submissão) | `draw_order` | B2 (b) e B5 (células de ordem) | célula nomeada, cor lida no pixel da sobreposição |
| Trocar desempate para ordem inversa | `draw_order` | B2 (c) | índice da primeira peça fora de ordem |
| Agrupar sem comparar textura | `draw_run_merge` | B2 (d) | contagem de corridas |
| Não definir `glDisable(GL_DEPTH_TEST)`/recorte na descarga | fachada | B5 estado hostil | pixel lido ≠ esperado |
| Mistura com alfa reto (`SRC_ALPHA`) | programa/estado | B5 borda pré-multiplicada | valor de canal fora da tolerância |
| Não ligar `GL_FRAMEBUFFER_SRGB` com a opção ligada | fachada | B5 célula sRGB | valor de canal |
| Deixar de contar peça com NaN | `frame_report_tally` | B2 e B5 | `recusadas esperado 1, obtido 0` |
| Planta `platform → draw2d` | árvore sabotada do portão | B3 | mensagem do próprio portão com o arquivo |

### 4.6 Definição de fechamento de `R2D-BATCH`

1. B0 a B7 com commit próprio citando `R2D-BATCH`; `Status` 🔍 no commit de B5, ✅ só depois de B7 e da re-verificação do main.
2. As 8 células de ordem impressas nos DOIS sistemas pelo teste de paridade (não só pelo átomo), zero reprova.
3. Toda linha da tabela 4.5 vista vermelha, com a saída colada no relatório de B7.
4. O cabeçalho publicado é byte a byte o congelado em B0 (diff vazio), ou a divergência tem registro do C-level decisor.
5. A linha de paridade Windows lida DIRETO de `gh run view <id> --json jobs`, nunca do painel (L-49 global).

---

## 5. `INPUT-EVENTS`: dividido pela dependência real, a parte sem parser vai para a W9

### 5.1 A decisão (D-W7D-01, REFEITA em 23/09 depois da objeção do team-lead)

**A primeira versão deste plano estava errada:** dizia "pré-requisitos todos na W9". A coluna lista quatro, dois deles na W11c, e há o ciclo com `WIN-INPUT` (FATO 1.3). Mover a linha para a W9 só trocaria um bloqueio por outro. Refiz com a cadeia medida.

**Insumo do líder que pesa, recebido pelo team-lead em 23/09 06:31 (a frase não está em nenhum arquivo que eu tenha achado com `grep`; proveniência: mensagem do team-lead, L-27):** *"Caminho principal: janela e entrada"*. A entrada é caminho principal do produto; empurrá-la para a W11c é custo de produto.

**As saídas reais, julgadas contra a cadeia:**

| Opção | O que faz | Veredito |
|---|---|---|
| (a) Mover `INPUT-EVENTS` inteiro para depois dos quatro (W11c ou depois) | Entrada chega ao consumidor só depois de W9, W9-B, W9-C, W10, W11, W11a, W11b | **Recusada.** Contraria a decisão do líder de 22/09 cuja consequência escrita era justamente *"a parte da tecla passa a funcionar sem esperar o parser"*, e contraria a entrada como caminho principal. E deixaria a W9 entregando adaptadores de teclado e ponteiro sem nenhum efeito para o consumidor (`on_event` continua recusado, `loop.hpp:223`), o mesmo defeito que `GFX-PRESET` existe para consertar. |
| (b) Trazer `KEYMAP-MODSTATE` e `KEYMAP-UTF8` para antes | Puxa a cadeia de CINCO elos do parser (LEX, PARSE-CORE, RESOLVE, PARSE-COMPAT, e os dois) | **Recusada.** Reverte duas decisões registradas do líder (slot único da L-32 com o RCSS até a W10, "rcss primeiro" de 22/08; mudança dos `KEYMAP-*` para W11 em 08/09), e é desnecessária: o que a tecla precisa não está nessa cadeia (FATO 1.3, item "o que exatamente depende do parser"). |
| (c) Dividir por DISPOSITIVO (ponteiro de um lado, teclado de outro) | Duas fatias congelando o mesmo tipo `gltfx_input_event` | **Recusada.** O teclado não depende do parser para a TECLA; dividir por dispositivo não desbloqueia nada que (d) não desbloqueie, e congelaria o mesmo tipo público duas vezes, em duas revisões de API, com risco de os dois lados divergirem. |
| **(d) Dividir pela DEPENDÊNCIA REAL** | W9: `INPUT-EVENTS` com tecla física, repetição, foco, ponteiro, estado por passo e modificadores físicos. W11c: duas fatias novas, `INPUT-TEXT` (evento de texto, depois de `KEYMAP-UTF8`) e `INPUT-KEY-LOGICAL` (modificador lógico com trava e latch, e símbolo da tecla pelo leiaute, depois de `KEYMAP-MODSTATE`). Mais um átomo interno `INPUT-SEQUENCE-CORE` no começo da W9 que desfaz o ciclo | **Escolhida.** É a leitura literal da decisão do líder de 22/09 (tecla e texto separados, a tecla não espera o parser) e de *"Caminho principal: janela e entrada"*. O que chega depois (texto, trava de maiúsculas, Alt pelo leiaute) chega por ACRÉSCIMO compatível: um tipo de evento novo por etiqueta (E5) e funções de consulta novas (E3), nunca campo novo no evento. |

**Como fica a cadeia depois de (d), sem ciclo:**

```
W9:   INPUT-SEQUENCE-CORE (novo; interno, núcleo puro; dep: nenhuma)
        -> WL-KEYBOARD, WL-POINTER          (dep: WL-SEAT ✅, INPUT-SEQUENCE-CORE)
        -> WIN-INPUT (as duas metades)      (dep: WIN-WINDOW ✅, INPUT-SEQUENCE-CORE; SAI "INPUT-EVENTS")
        -> INPUT-EVENTS (congela a API)     (dep: WL-KEYBOARD, WL-POINTER, WIN-INPUT; SAEM os dois KEYMAP-*)
        -> CI-VERDE-W9
W11c: KEYMAP-* (como está) -> INPUT-TEXT (dep: KEYMAP-UTF8, INPUT-EVENTS)
                           -> INPUT-KEY-LOGICAL (dep: KEYMAP-MODSTATE, INPUT-EVENTS)
W12:  KEYMAP-COMPOSE (como está; alimenta INPUT-TEXT com tecla morta e composição)
```

- **`INPUT-SEQUENCE-CORE`** é a S3 do plano parcial (`entrada-teclado-ponteiro.md` §5): os tipos de valor INTERNOS que os dois sistemas alimentam sem `#if`, mais o átomo `input_event_sequence` que carrega a L-35 (série sem furo, sem duplicar, sem reordenar, eventos sintetizados de perda de foco e repetição), mais o construtor do retrato de estado por passo (E1). **Nada público**: é o que os adaptadores precisam para existir antes de a API congelar. Ganha ID próprio porque `WIN-INPUT` precisa apontar para ALGO que não seja `INPUT-EVENTS`, senão o ciclo fica.
- **`WIN-INPUT`**: a coluna troca `INPUT-EVENTS` por `INPUT-SEQUENCE-CORE`. A atomização em duas metades anunciada em 02/09 e nunca criada (já está na INBOX) se resolve junto, na W9.
- **A primeira versão desta decisão (mover a linha inteira para a W9 sem tocar a coluna) fica APAGADA, não arquivada** (L-67 global).

**O que se perde com (d), declarado:** até a W11c, o consumidor que quer "o jogador digitou um nome" ou "a trava de maiúsculas está ligada" não tem como saber pela biblioteca; tem tecla física, e tem "Shift físico abaixado". Para jogo (movimento, atalho, clique) isso é tudo; para caixa de texto não é, e o cabeçalho de `INPUT-EVENTS` diz isso com essas palavras.

**Isto supera também a decisão autônoma 3 do plano parcial** ("`INPUT-EVENTS` NÃO entra na W9"; não ratificada), pela razão do veredito de (a). Não é reverter alocação do líder: `INPUT-EVENTS` foi à W7 por ajuste mecânico (CHK-07) e à W7-D pela decisão D1 autônoma de `onda-loop-w7.md`, ainda a ratificar.

### 5.2 Desenho da API de entrada (para a revisão de API dedicada da W9; porta de mão única)

Parte do plano parcial (D1 a D10, válidos) e da decisão do líder (tecla e texto separados). Acréscimos meus, cada um de uma dor da seção 2.2:

- **E1 (R-I1): entrega em DUAS formas que concordam.** (i) `on_event`, um por evento, depois do bombeamento e antes do `on_frame` (já congelado); (ii) um **retrato de estado por passo** consultável pelo consumidor dentro de `on_frame` (teclas físicas abaixadas agora, botões, posição do ponteiro, modificadores, e para cada tecla e botão a **contagem de descidas e subidas neste passo**). O retrato é derivado da MESMA sequência que alimentou o `on_event` do passo (um funil só; a L-35 se prova num ponto). Isto entrega o requisito (1) do consumidor (duas direções ao mesmo tempo) sem a armadilha do toque curto perdido que raylib, GLFW e SDL têm.
- **E2 (R-I2): identificador físico = uso HID de teclado, página 0x07** (enumeração append-only com valor numérico explícito, na forma de `gltfx_gfx_option`). Wayland entrega código evdev (conversão por tabela nossa, dado puro, sem `#if`); Windows entrega código de varredura com bit estendido (outra tabela). Tecla que não tem uso HID vai como `unknown` com o código cru do sistema num campo próprio, nunca descartada.
- **E3: símbolo por leiaute e modificador lógico = funções de consulta futuras** (`INPUT-KEY-LOGICAL`, W11c), não campo reservado no evento; o layout do evento não muda quando elas chegarem. **Na W9 o evento e o retrato carregam só os modificadores FÍSICOS** (quais das teclas Shift, Ctrl, Alt e Super, esquerda e direita, estão abaixadas), derivados do próprio estado de teclas, exatos e idênticos nos dois sistemas sem parser. O cabeçalho diz com todas as letras que trava de maiúsculas, latch de acessibilidade e "Alt como o leiaute define" chegam depois.
- **E4: botão de ponteiro = inteiro** (1 esquerdo, 2 meio, 3 direito, 4 voltar, 5 avançar, e os seguintes como o sistema numerar), atendendo à ordem do líder de 06/09 ("o máximo possível de botões que o SO registrar"); "quantos botões há" responde `unknown` onde o sistema não diz (Wayland não diz), nunca um número fixo.
- **E5: evento como tipo de valor com etiqueta de tipo e layout estável** (L-19: value type do núcleo, layout é o contrato), com tamanho fixo declarado por `static_assert`; tipos novos de evento entram por acréscimo de etiqueta.

**O que `INPUT-EVENTS` congela:** `gltfx_input_event` (definição), a enumeração de uso HID, a enumeração de etiqueta, o retrato de estado e o acessor dele, e a remoção da recusa de `on_event` (P9 muda: a recusa sai). **Revisão de API dedicada obrigatória** na W9, com a garantia da L-35 como item da revisão.

### 5.3 A quina e o caminho único teclado/gamepad (D-W7D-02, D-W7D-03)

- **A quina é da biblioteca (ordem do líder), e mora na trilha de mapa, não em `INPUT-EVENTS`.** Fatia nova `MAP-STEP-RESOLVE` (W11a, depois de `MAP-COLLIDE-GRID`): recebe o vetor de intenção de movimento e a grade, e devolve o passo JÁ resolvido (diagonal atravessa só se as duas vizinhas estiverem livres, regra configurável por um enum fechado, com o mesmo teste nomeado de canto que `MAP-PATH-ASTAR` já planeja). **Por que não um "átomo de grade próprio, menor" dentro de `INPUT-EVENTS`:** seria uma segunda grade na biblioteca, que muda por duas leis não relacionadas (entrada e mapa, L-17 régua qualitativa) e que `MAP-MODEL` teria de engolir ou duplicar depois (a regra de três da L-33 global não autoriza nem a segunda cópia). **O que se perde, declarado:** o passo resolvido só existe quando a trilha de mapa chegar (W11a); até lá o consumidor recebe estado e vetor, não o passo.
- **Teclado e gamepad pelo mesmo caminho, eixo contínuo:** fatia nova `INPUT-ACTIONS` (W11c, depois de `GP-MAP`): o consumidor declara uma ação de eixo 2D por ligações (teclas físicas, eixos e botões de controle), e recebe um vetor com zona morta circular e diagonal normalizada (a técnica que o Godot usa, aprendida, não copiada). Fica em W11c para nascer provada com as DUAS fontes de uma vez; nascer só com teclado seria meia paridade de fonte.

---

## 6. Definição de fechamento da ONDA W7-D

A onda fecha quando, e só quando, **tudo** abaixo foi medido:

1. `GFX-PRESET` ✅ pela seção 3.3; `R2D-BATCH` ✅ pela 4.6; `WL-WRITE-TIMEOUT-NAO-FATAL` ✅ (já está).
2. `INPUT-EVENTS` **movido** para a W9 no `TODO.md`, com o motivo escrito NA LINHA (L-32 emenda de 09/09), posição logo antes de `CI-VERDE-W9`, e a cadeia da seção 5.1 aplicada na tabela: coluna de `INPUT-EVENTS` = `WL-KEYBOARD, WL-POINTER, WIN-INPUT` (saem os dois `KEYMAP-*`, citando `DECISOES_AUTONOMAS.md:3910`); coluna de `WIN-INPUT` = `WIN-WINDOW, INPUT-SEQUENCE-CORE` (sai `INPUT-EVENTS`); `WL-KEYBOARD` e `WL-POINTER` ganham `INPUT-SEQUENCE-CORE`; linhas novas `INPUT-SEQUENCE-CORE` (W9, primeira da onda), `INPUT-TEXT` e `INPUT-KEY-LOGICAL` (W11c). **Portão desta arrumação, medido pelo main depois de editar:** nenhum item aponta, na coluna `Depende de`, para item de onda POSTERIOR à sua (varredura de todas as linhas das ondas W9 e W11c, contagem impressa, zero varridos reprova); e nenhum ciclo entre os itens de entrada (varredura do grafo desses itens). As outras fatias novas da seção 8 registradas na tabela ou na INBOX, conforme indicado.
3. O trabalho andou em ramo próprio `onda-w7d` (L-11 primeira e segunda emendas), nunca em `main`.
4. `CI-VERDE-W7D`: o commit que fecha a onda é empurrado ao ramo; o resultado do servidor lido DIRETO (`gh run view <id> --json conclusion,jobs`); **número de trabalhos verdes = total, os dois medidos**; a lista de nomes de trabalhos da última execução comparada com a do `ci.yml` local (regra do `CLAUDE.md`: se a da execução for menor, o que falta não foi exercido).
5. Só então: merge em `main` e marca, sem perguntar (L-11 terceira emenda). **Número pela L-26:** a onda entrega recurso novo compatível (linha `suggested_preset` + acessores; desenhador 2D) → sobe o **B**: **`v0.6.0.0`**. `project(... VERSION 0.6.0.0)` e `VERSION-TAG-SYNC` coerentes antes da marca.
6. `DECISOES_AUTONOMAS.md` com as decisões da seção 9, registradas ao vivo pelo main.
7. Avisos: Gus Dragon, por L-37, quando `R2D-BATCH` fechar (WSJF 17,67, alta), em linguagem para 11 anos e sem prometer data; o `gusworld` pelo barramento sobre onde os três requisitos de entrada foram parar (`INPUT-EVENTS` W9 com estado por passo, `INPUT-ACTIONS` W11c, `MAP-STEP-RESOLVE` W11a; texto em `INPUT-TEXT` W11c), sem classificação de prioridade (L-16); o `mapeditor` sobre `MAP-STEP-RESOLVE` (L-33: fatia nova de mapa é gatilho).

---

## 7. Arquivos que cada item toca (para o main saber o que pode andar em paralelo)

**Ordem obrigatória pela L-32 (emenda de 09/09): `GFX-PRESET` antes de `R2D-BATCH`** (linhas 529 e 553). Paralelismo possível só onde não há arquivo em comum.

| Item / sub-fatia | Arquivos |
|---|---|
| `GFX-PRESET` P1 | novos `src/platform/gl/{gfx_preset_table,auto_preset_rule,preset_expansion}.{hpp,cpp}`; `src/platform/gl/CMakeLists.txt`; `tests/{gfx_preset_table,auto_preset_rule,preset_expansion}_test.cpp`; `tests/CMakeLists.txt` |
| `GFX-PRESET` P2 | novos `src/platform/port/power_source_port.hpp`, `src/platform/wayland/{power_supply_rule,power_source_adapter,selected_power_source_adapter}*`, `src/platform/win32/{power_status_rule,power_source_adapter,selected_power_source_adapter}*`, `selected_*_check.cpp` dos dois; `src/platform/{port,wayland,win32}/CMakeLists.txt`; `tests/check_port_privacy.sh`, `tools/ci/check-port-privacy-win.ps1`; dois testes puros; `tests/CMakeLists.txt` |
| `GFX-PRESET` P3 | `include/glintfx/platform/gl/gfx_option.hpp`; `src/platform/gl/gfx_option_registry.{hpp,cpp}`; `src/platform/gl/gl_context_facade.cpp`; `src/platform/gl/gl_context_impl.hpp`; `src/platform/wayland/egl_context_adapter.{hpp,cpp}`; `src/platform/win32/wgl_context_adapter.{hpp,cpp}`; `tests/tools/check_gfx_option_ids.py` |
| `GFX-PRESET` P4 | `tests/parity/gl_context_parity_test.cpp`; `tests/container/Containerfile`; `tests/measured_exceptions.txt`; `docs/gl-loop-portability-matrix.md`; `docs/wiki/API-Graphics-Context.md`; `include/glintfx/platform/gl/context.hpp` (comentário) |
| `R2D-BATCH` B2 | novos `src/draw2d/{quad_vertices,draw_order,draw_run_merge,frame_report_tally}.{hpp,cpp}`, `src/draw2d/CMakeLists.txt`; `src/CMakeLists.txt`; testes puros; `tests/CMakeLists.txt` |
| `R2D-BATCH` B3 | `tests/tools/check_layers.py` (**COLIDE com o agente de `LAYERS-GATE-GFSS-GFUI`, que o está editando agora: B3 só começa depois de aquela fatia estar commitada**); `tools/gl_registry_codegen/loader_codegen.cpp` (+ teste do gerador); `src/render/gl_proc_address.hpp`; novos `src/draw2d/{gl_state_contract,vertex_stream,embedded_program}.*`; `src/platform/gl/gl_context_impl.hpp` + acessor interno de tamanho de superfície e de resolvedor (**COLIDE com `GFX-PRESET` P3**) |
| `R2D-BATCH` B4 | novos `include/glintfx/draw2d/*.hpp`, `src/draw2d/{renderer_2d_facade.cpp,renderer_2d_impl.hpp}`; a lista de cabeçalhos instalados/`export` que os portões de instalação conferem (medir com `ls cmake/` e `ls tests/tools/`, sem número escrito aqui); `tests/header_hygiene_test.cpp` |
| `R2D-BATCH` B5 | novo `tests/parity/draw2d_parity_test.cpp`; `tests/container/Containerfile`; `tests/CMakeLists.txt`; `.github/workflows/ci.yml` se o teste de paridade exigir passo novo |
| `R2D-BATCH` B6 | novos `docs/draw2d-portability-matrix.md`, `docs/wiki/API-Draw-2D.md`; `docs/api-conventions.md` (se B0 criar regra) |
| `INPUT-EVENTS` (nesta onda) | só `TODO.md` (mover a linha, corrigir quatro colunas `Depende de`, criar três linhas), pelo main; **fora da janela em que outro agente edita o `TODO.md`** (hoje ele aparece modificado na árvore) |

**O que PODE andar em paralelo, e só isto:** `R2D-BATCH` B0 (revisão, só leitura e parecer) e B1 (medição em container, escreve só em `/var/tmp`) podem rodar enquanto `GFX-PRESET` P1-P2 estão em implementação. `R2D-BATCH` B2 (diretório novo) pode rodar em paralelo a `GFX-PRESET` P1/P2 **exceto** por `tests/CMakeLists.txt`, que os dois tocam: serializar os commits nesse arquivo, ou um agente só por vez (memória `feedback_dois_agentes_mesma_arvore`). Nada de `R2D-BATCH` B3 em diante antes de `GFX-PRESET` P3 estar commitado. Respeitar o teto de 4 agentes vivos e 1 trabalho pesado por vez (L-11 global).

---

## 8. Fatias e ondas novas (criadas por completude, L-32 refinada em 22/09)

**Nenhuma entra na fila antes da demo** (extensão de 27/08 da L-32: escopo novo é registrado e desenhado, não executado antes de a janela desenhar).

| ID proposto | Onda | O que é | De onde veio |
|---|---|---|---|
| `INPUT-SEQUENCE-CORE` | W9, primeira linha da onda | Tipos de valor internos de entrada sem `#if`, átomo `input_event_sequence` (L-35), construtor do retrato de estado por passo com contagem de bordas. Núcleo puro, TDD sem container, nada público. É a S3 do plano parcial com ID próprio para desfazer o ciclo `WIN-INPUT` ↔ `INPUT-EVENTS`. | D-W7D-01 refeita |
| `INPUT-TEXT` | W11c, depois de `KEYMAP-UTF8` | Evento de texto (etiqueta nova do mesmo `gltfx_input_event`, acréscimo compatível), Wayland pelo parser próprio, Windows por `WM_CHAR` (com par de substitutos UTF-16), paridade provada. `KEYMAP-COMPOSE` (W12) o alimenta depois com tecla morta e composição. Porta de mão única. | D-W7D-01 refeita; decisão do líder de 22/09 |
| `INPUT-KEY-LOGICAL` | W11c, depois de `KEYMAP-MODSTATE` | Funções de consulta: modificador lógico (inclusive trava e latch) e símbolo da tecla física pelo leiaute ativo; Wayland pelo parser próprio, Windows por `GetKeyState`/`ToUnicodeEx`/`MapVirtualKeyEx` (API do sistema). Porta de mão única. | D-W7D-01 refeita; dor AZERTY (2.2 item 2) |
| `MAP-STEP-RESOLVE` | W11a, depois de `MAP-COLLIDE-GRID` | Passo de movimento já resolvido contra a grade, incluindo a quina (ordem do líder D-091201). Núcleo puro, TDD. Porta de mão única (regra da quina pública). | D-W7D-02 |
| `INPUT-ACTIONS` | W11c, depois de `GP-MAP` | Ação de eixo 2D por ligações de teclado e controle, zona morta circular, diagonal normalizada. Porta de mão única. | D-W7D-03, requisitos (2) e (3) do consumidor, dor Godot |
| `R2D-BATCH-OPTIMIZER` | W9-B, depois de `R2D-TEXTURE` | Reordenar para juntar chamadas, só entre peças que não se sobrepõem, olhando uma janela curta (técnica sokol_gp aprendida); pixel idêntico ao sem otimizador provado por leitura. | D-W7D-10, dor sokol_gp/raylib |
| `GFX-POWER-SAVER` | W9-B | Linha `read_only` nova: "o usuário pediu economia de energia ao sistema" (Windows por `SystemStatusFlag`; Linux por perfil de plataforma do kernel, `unknown` onde não existir), e a sugestão automática passa a considerá-la. | Dor 2.3 item 3; D-W7D-06 |
| INBOX: `POINTER-LOCK` | - | Trava e captura do ponteiro (Wayland `pointer-constraints`/`relative-pointer`, Win32 `ClipCursor`/entrada crua). Nenhum item existe. | Dor 2.2 item 6 |
| INBOX: `DRAW2D-RAW-GL-INTEROP-DOC` | - | Nada a construir: registrar que o exemplo de interoperação com GL cru (barreira `flush`) entra em `DEMO-1` ou num exemplo à parte, para o contrato R-B4 ter um uso real provado. | R-B4 |

---

## 9. Decisões no lugar do líder (DECISÃO AUTÔNOMA, uma por bloco)

Cada uma: pergunta que iria a ele, opções, escolha, porta de mão única, custo de reverter. Fontes consultadas nas seções 1 e 2; nenhuma saiu do "próprio aprendizado" sem fonte, exceto onde dito.

**D-W7D-01 (REFEITA em 23/09, depois da objeção medida do team-lead). Onde mora `INPUT-EVENTS`, dado que a coluna lista dois pré-requisitos na W11c e há ciclo com `WIN-INPUT`?** Opções: (a) mover inteiro para depois dos quatro (W11c ou depois); (b) trazer a cadeia `KEYMAP-*` (cinco elos) para antes; (c) dividir por dispositivo; (d) dividir pela dependência real: a parte sem parser na W9, `INPUT-TEXT` e `INPUT-KEY-LOGICAL` novas na W11c, e `INPUT-SEQUENCE-CORE` novo no começo da W9 desfazendo o ciclo. **Escolhida (d)** (seção 5.1). Fontes: `DECISOES_AUTONOMAS.md:3910` (a tecla não espera o parser, decisão do líder), a frase do líder *"Caminho principal: janela e entrada"* (via team-lead), Wayland Book e libxkbcommon (interpretação de modificador é do keymap), SDL3 BestKeyboardPractices (tecla física e texto são caminhos distintos). A primeira versão, errada, fica apagada. Porta de mão única: não a decisão de onde, mas o que `INPUT-EVENTS` congela na W9 (revisão de API de lá). Reverter: barato agora.

**D-W7D-02. A quina: átomo de grade próprio em `INPUT-EVENTS` ou esperar o mapa?** Opções: (a) grade mínima dentro da entrada; (b) fatia nova `MAP-STEP-RESOLVE` na trilha de mapa; (c) deixar a quina com o consumidor. **(c) está proibida pela ordem do líder.** **Escolhida (b)** (seção 5.3). Custo declarado: o passo resolvido só chega na W11a. Porta de mão única quando a fatia congelar a regra; a decisão de ONDE morar não é. Reverter: barato agora.

**D-W7D-03. Teclado e gamepad pelo mesmo caminho: em `INPUT-EVENTS` ou camada própria?** **Escolhida camada própria `INPUT-ACTIONS` em W11c**, depois do gamepad existir, para nascer provada com as duas fontes. Reverter: barato.

**D-W7D-04. Entrada por evento, por estado, ou as duas?** Opções: (a) só evento (GLFW); (b) só estado (raylib, perde toque curto); (c) as duas, derivadas do mesmo funil, com contagem de bordas por passo. **Escolhida (c)** (E1). Porta de mão única (decide-se na revisão de API da W9). Reverter: barato até a W9 congelar.

**D-W7D-05. Identificador físico de tecla.** Opções: (a) código evdev cru; (b) código de varredura do Windows; (c) uso HID página 0x07. **Escolhida (c)** (E2): o único que é padrão entre sistemas e o que o SDL adota. Porta de mão única na W9.

**D-W7D-06. O modo de economia do sistema entra em `GFX-PRESET`?** Opções: (a) entra agora; (b) fatia nova pós-demo. **Escolhida (b)** (`GFX-POWER-SAVER`, W9-B): entrar agora seria escopo novo antes da demo, que a extensão de 27/08 da L-32 proíbe. Reverter: barato.

**D-W7D-07. `balanced` e `performance` iguais hoje: diferenciar agora?** Opções: (a) `performance` com `vsync=off`; (b) manter iguais e declarar. **Escolhida (b)**, mantendo D-W6b-34: desligar a sincronia troca desempenho por rasgo de imagem, que é escolha de usuário separada da qualidade, e não é o que "desempenho" significa em nenhuma predefinição que a pesquisa achou. Diferenciam quando existirem opções de desenho ao vivo. Reverter: barato (é dado).

**D-W7D-08. Onde é o "ao apresentar o quadro" da decisão do líder sobre falha?** Opções: (a) `finish_frame()` chamado pelo consumidor no fim do `on_render`, imediatamente antes da apresentação, devolvendo o relatório único; (b) gancho interno dentro de `swap_buffers()`/`present()` que descarrega e devolve erro de desenho junto com o de apresentação. **Escolhida (a):** (b) mistura dois domínios de erro num `gltfx_rslt` só (o consumidor não saberia se o quadro foi apresentado quando o erro é de desenho), e poria a plataforma chamando a camada de cima. O que se perde com (a): o consumidor pode esquecer o `finish_frame()`; mitigação: `begin_frame()` de um quadro não terminado descarta o anterior e o próximo relatório conta isso num campo próprio. **Porta de mão única; vai à revisão B0 como ponto obrigatório.** Reverter: caro depois de publicado.

**D-W7D-09. Onde a transformação do lote é aplicada?** Opções: (a) na placa (matriz por lote como uniforme); (b) na CPU, em double, só a matriz pixel→recorte na placa. **Escolhida (b)** (4.3). É a única que honra juntas as duas decisões do líder (mundo em double; matriz em float uma vez por quadro) e ainda não quebra o agrupamento por troca de lote. Não é porta de mão única (interno).

**D-W7D-10. Algoritmo de ordem e agrupamento.** Opções: (a) `std::stable_sort` por camada; (b) chave composta única `(camada, submissão)` com ordenação comum; (c) ordenar por textura (MonoGame `Texture`). **Escolhida (b)**; (c) recusada porque reordena peças sobrepostas de mesma camada (quebra a promessa de submissão); (a) pode alocar sob `noexcept`. Reordenar para juntar fica na fatia nova `R2D-BATCH-OPTIMIZER`. Interno.

**D-W7D-11. Contrato de estado de GL.** Opções: (a) salvar e restaurar o estado do consumidor (caro, `glGet` síncrono); (b) não confiar em nada ao descarregar e declarar o estado de saída, com barreira `flush()` pública; (c) não dizer nada (NanoVG). **Escolhida (b)**. `flush()` é pública, portanto porta de mão única, na revisão B0.

**D-W7D-12. Espaço de cor da mistura.** Opções: (a) forçar superfície sRGB; (b) seguir a opção `srgb_framebuffer` e provar os dois caminhos; (c) sempre misturar em espaço codificado. **Escolhida (b)**: respeita a opção que o consumidor já escolhe e o tipo de cor já decidido pelo líder (linear), sem impor custo. Documentado nos dois. Reverter: médio (muda pixel observável).

**D-W7D-13. Peça inválida e falta de memória.** Escolhida: inválida é descartada e contada (o quadro segue); falta de memória descarta e faz `finish_frame()` devolver erro com as contagens. Fonte: R3 (degrau 2) e a decisão do líder "Devolve erro; o aplicativo decide". Na revisão B0.

**D-W7D-14. Onde o desenho 2D mora.** Opções: (a) `src/render/` junto do carregador; (b) camada nova `src/draw2d/` + `include/glintfx/draw2d/`, acima de `platform/gl`, com o portão de camada estendido (endurecimento, nunca afrouxamento). **Escolhida (b)**: o carregador está abaixo do contexto e o desenho está acima; um diretório com as duas coisas é camada sem direção. O nome do diretório público vai à revisão B0.

**D-W7D-15. Resolver funções GL com contexto.** Escolhida: o gerador emite forma com `void *user`; mudança interna, com teste do gerador vermelho antes. Reverter: barato.

**D-W7D-16. `fill_quad` na v1?** Opções: (a) só `fill_rect`; (b) `fill_rect` e `fill_quad`. **Escolhida (b)**, a mais completa: retângulo girado sem abrir lote por peça, e a forma que textura e transformação reusam. Vai à revisão B0 como ponto a confirmar. Porta de mão única.

**D-W7D-17. Ordem de execução e paralelismo dentro da onda.** `GFX-PRESET` P0..P5, depois `R2D-BATCH` B2..B7, com B0 e B1 adiantáveis em paralelo (seção 7). É execução da L-32, registrada por transparência.

---

## 10. O que continua exigindo o líder (não herdo)

1. **Nenhuma lei muda neste plano.** Se a revisão B0 quiser criar regra nova em `docs/api-conventions.md`, isso é documento de convenção pública, não lei; se virar lei, vai a ele.
2. **Porta de mão única** de `GFX-PRESET` (linha 8 + 2 acessores) e de `R2D-BATCH` (a API de desenho inteira) **só congelam depois da revisão de API dedicada** (P0 e B0). Autonomia não dispensa a revisão; o parecer é o que me autoriza a decidir divergências.
3. **Ratificação retroativa** das 17 decisões da seção 9, com destaque para D-W7D-01 refeita (dividir `INPUT-EVENTS` pela dependência real e desfazer o ciclo com `WIN-INPUT`; supera uma decisão autônoma anterior), D-W7D-02 (onde mora a quina que ele declarou nossa) e D-W7D-08 (onde é "ao apresentar").
4. **Instalação de pacote:** nenhuma exigida nesta onda (o container já tem `llvmpipe`, FATO pelo `CLAUDE.md` e pelos testes GL existentes). O pacote de entrada falsa do KWin continua sendo da W9 e continua declarado ao líder por lá.
5. **Marca:** autorizada ao fim de onda verde (L-11 terceira emenda); o número `v0.6.0.0` sai da L-26 e não é decisão minha.

---

## 11. O que eu NÃO medi, dito sem maquiagem

1. **Se o job Windows do servidor tem GL 3.3 por software capaz de leitura de pixel exata.** A matriz diz que a paridade de contexto roda "live" nos dois; **não li o log** de uma execução para saber qual implementação de GL o executor usa. B1 mede antes de B5 depender disso.
2. **Custo real das técnicas de envio de vértice** nos alvos: B1.
3. **A cadência que o laço mede com teto de 30** (C3 de `GFX-PRESET`): o teste do P7 já mede cadência; não li o número dele no servidor.
4. **Se `flush` e `layer` colidem com macro** em algum dos cinco alvos: o portão `public_name_collision_test` responde; eu não rodei.
5. **Licença de MonoGame e LÖVE na fonte:** a API devolveu `NOASSERTION`; li só documentação e páginas de fórum/issue deles.
