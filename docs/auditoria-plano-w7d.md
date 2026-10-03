<!-- Origem: PLANO.md, md5 bd73502a64fa2b4c41d338ce4c09de1e; PLANO-errata.md, md5 9942bf8974e9f75585aebb1917a18266. Os dois corpos copiados byte a byte, o PLANO primeiro e a errata depois; so os comentarios de origem foram acrescentados (D-FECH-3). -->

# Plano da W7-D, versão de 29/09/2026: código do framework primeiro

**Autor:** CTO (C-level `opus`, modo autônomo, L-34 do projeto com a emenda de 29/09). **Data real:** 29/09/26 - 22:39 (`date`). **Árvore:** `main` local em `612db94`, com a C1b STAGED por outro agente (não tocada por mim).
**Não reescreve** `docs/plano-w7d.md` (23/09) nem `docs/plano-w7d-adendo-revalidacao.md` (24/09). **Substitui**, onde diverge, a ORDEM, o RECORTE e as tabelas de fatia dos dois. O desenho de produto deles (R-B1 a R-B8, D-W7D-01 a 17, D-W7D-R2, R4, R6, R8, R9) continua valendo, salvo onde este arquivo diz outra coisa.

Convenção (L-27): **FATO** vem com comando ou `arquivo:linha`; **INF** é inferência, marcada; **DECISÃO** é do CTO no lugar do líder, no formato da L-34 (pergunta, opções, fontes, escolha e porquê, porta de mão única, custo de reverter), confirmada retroativamente.

---

## 0. A ordem que manda neste plano

**FATO, decisão do líder de 29/09/2026, 22:33, por `AskUserQuestion`** (`DECISOES_AUTONOMAS.md`, commit `612db94`). A cobrança, verbatim: *"Já mais de um dia que você tá escrevendo teste!!! Onde fica o CÓDIGO do framework?!"*. Medido pelo main: os 109 commits desde 28/09 tocaram 0 arquivos de `src/` e `include/`, e o último commit que tocou produto é o `aa83f69`, de 24/09.
- Fecha-se só a C1b; depois vai-se direto ao framework, W7-D e em seguida W8.
- O resto da W7-C (C1c-C4, F2-F13, A6-A9, B0-B3, CI-CMP-5) vai para DEPOIS da W8.

**Régua deste plano, escrita antes (L-43 global):**
1. Toda fatia de execução cria ou muda código em `src/` ou `include/`, com o teste dela escrito vermelho antes (L-20).
2. Fatias sem produto são só as que a lei exige: as duas revisões de API dedicada (porta de mão única), as duas revisões adversariais e a prova viva de paridade.
3. **Ferramental novo não entra.** Os portões novos que os planos de 23 e 24/09 punham na W7-D (Q1, W0, a B3a DO ADENDO de 24/09, que era o portão de direção de camada, e a sonda B1 isolada; a B3a DESTE plano é outra coisa: é produto, o carregador GL) vão para a onda `INFRA-CI` (logo depois da W8), junto com o resto da W7-C.

**Contagem desta régua, para conferir no fim:** 11 fatias de produto, 7 de revisão ou prova, 1 de fechamento (seção 5).

---

## 1. Fatos medidos agora (29/09, 22:3x)

| # | Fato | Fonte |
|---|---|---|
| G1 | Os 5 itens com `W7-D` na coluna de onda: `GFX-PRESET` ⏳, `WL-WRITE-TIMEOUT-NAO-FATAL` ✅, `R2D-BATCH` ⏳, `INPUT-EVENTS` ⏳, `CI-VERDE-W7D` ⏳ | `grep -nE '^\| [0-9]' TODO.md \| grep '\| W7-D \|'` (linhas 599, 609, 628, 629, 630) |
| G2 | Pré-requisitos de `R2D-BATCH` todos ✅: `GL-LOADER` (W2), `GL-CONTEXT` (W6b), `CORE-MATH2D` (W4), `CORE-COLOR` (W2); `LOOP-RUN` ✅ | `awk` sobre a coluna 8 do `TODO.md` |
| G3 | `INPUT-EVENTS` continua na W7-D com `WL-KEYBOARD`, `WL-POINTER` (W9) e `KEYMAP-MODSTATE`, `KEYMAP-UTF8` (W11c) na coluna. A arrumação D-W7D-01 (ratificada em bloco em 24/09 07:35, pelo adendo §3.C) **nunca foi aplicada à tabela** | `TODO.md:629`; nenhuma linha `INPUT-SEQUENCE-CORE` (`awk` vazio) |
| G4 | Nenhuma das linhas novas dos planos de 23/24/09 existe: `PLAN-SCOPE-COMPARE-ALL`, `TODO-DEP-ORDER`, `INPUT-SEQUENCE-CORE`, `INPUT-TEXT`, `MAP-STEP-RESOLVE` | `awk` vazio para cada uma |
| G5 | `src/render/` = `CMakeLists.txt`, `gl_abi.hpp`, `gl_proc_address.hpp`: nenhuma linha de desenho existe | `find src/render -type f` |
| G6 | `gltfx_input_event` está só declarada; `on_event` existe no layout congelado e `run()` o recusa | `include/glintfx/platform/loop/loop.hpp:139,162,226-230` |
| G7 | As opções `preset`, `auto_choice_reason` e `power_source` (ids 5-7) são aceitas sem efeito | `include/glintfx/platform/gl/gfx_option.hpp:118-120`; `src/platform/wayland/egl_context_adapter.cpp:1103-1105,1115,1123-1124` |
| G8 | `tests/parity_absences.txt` tem **10** linhas de `GFX-PRESET` (o adendo de 24/09 contava 9) | `grep -c GFX-PRESET tests/parity_absences.txt` |
| G9 | `origin/main` = `393ec30` (marca `v0.5.1.0`). O ramo de trabalho (`main` local, empurrado como `onda-w7c`) está **243** commits à frente de `origin/main`, **8** deles em `src/`/`include/` (a guarda de display morto do EGL, S1-S5c, e o E2 de citações) | `git rev-list --count origin/main..HEAD`, idem `-- src include` |
| G10 | `origin/onda-w7c` = `b168454`, CI `36653147581` verde (26 trabalhos: 25 success e 1 skipped por desenho). Local e não empurrados: `4f31c7a`, `8b21056`, **`d8641c9` (REPROVADO, D-A38)**, `90ff958`, `3dead3a`, `612db94`, mais a C1b STAGED no índice | `git log --oneline origin/onda-w7c..HEAD`; `git status --short` |
| G11 | Versão do projeto: `0.5.1.0` | `CMakeLists.txt:43` |
| G12 | A L-32 do projeto, extensão de 27/08: *"Enquanto a demo não estiver rodando, escopo NOVO não entra na fila de execução"*: é registrado e desenhado, não vira fatia disponível | `GODS_LAWS.md`, L-32 |
| G13 | `DEMO-1` (W8) depende só de `LOOP-RUN` ✅ e `R2D-BATCH` | `TODO.md:640` |
| G14 | `origin/main` (`393ec30`, o merge do PR #11) **NÃO é ancestral** do ramo de trabalho: `HEAD..origin/main` = 1 commit, o próprio merge. Os dois pais dele (`98035e8` e `032884f`) SÃO ancestrais, e a árvore dele é idêntica à de `032884f`. O merge da W7-C em `main` **não tem conflito** (`git merge-tree --write-tree origin/main HEAD` sai 0, sem tocar a árvore), mas **não é avanço rápido**: nasce um commit de merge, e a marca vai nele | `git merge-base --is-ancestor`, `git diff --stat 393ec30 032884f` (vazio), `git merge-tree` |
| G15 | A L-32 do projeto, emenda de 09/09: *"Não pule nenhuma fatia, faça na sequencia da tabela"*. Dentro da onda, a ordem é a das linhas do `TODO.md`: `GFX-PRESET` (linha 599) antes de `R2D-BATCH` (linha 628) | `GODS_LAWS.md`, L-32, "Emenda de 09/09/2026" |

---

## 2. Pesquisa (L-43 do projeto e L-34 com a emenda de 29/09)

A pesquisa de 23/09 (`docs/plano-w7d.md` §2: raylib, LÖVE, SDL, sokol_gp, bgfx, MonoGame, GLFW e Godot, com as dores R-B1 a R-B8 e R-I1, R-I2) e a de 24/09 (adendo §2) continuam valendo. **Acrescento o que a emenda de 29/09 manda procurar: o mais completo e o mais desejado pela comunidade, e como as bibliotecas semelhantes fazem.** Li só documentação e páginas públicas; nenhum código de terceiro (L-29).

1. **SDL3, o conjunto de desenho 2D:** pontos, linhas, retângulo vazio e cheio, `SDL_RenderGeometry` (lista de triângulos com cor por vértice, textura opcional e índices opcionais), textura inteira, girada, afim, ladrilhada e em 9 partes. https://wiki.libsdl.org/SDL3/CategoryRender
2. **`SDL_RenderGeometry` foi pedido da comunidade:** entrou no SDL 2.0.18 e foi portado logo por quem usa o SDL (Rust-SDL2 #1180, pygame #3006). O imgui_sdl declarou que ele tornou desnecessário o rasterizador próprio. https://github.com/Rust-SDL2/rust-sdl2/issues/1180 ; https://github.com/pygame/pygame/pull/3006 ; https://github.com/Tyyppi77/imgui_sdl ; https://wiki.libsdl.org/SDL3/SDL_RenderGeometry
3. **RmlUi 6.0, a interface de desenho:** `CompileGeometry(Span<const Vertex>, Span<const int>)`, depois `RenderGeometry(handle, translation, texture)`. A textura saiu do compile, o desenho imediato foi removido e a cor por vértice passou a vir em **alfa pré-multiplicado**. https://github.com/mikke89/RmlUi/releases/tag/6.0 ; https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces/render.html
   - **Consequência para nós:** o `gfui` (L-28, o motor de interface da casa) vai desenhar por geometria indexada. O desenho 2D que nascer só com quadrilátero obrigaria a reabrir o formato de vértice e o agrupador quando o `gfui` chegar.
4. **raylib, o módulo de formas:** retângulo (cheio, contorno, arredondado, degradê), linha e linha grossa, círculo, elipse, anel, triângulo, leque, faixa, polígono. https://deepwiki.com/raysan5/raylib/4.2-shapes-and-textures ; https://github.com/raysan5/raylib/blob/master/examples/shapes/shapes_basic_shapes.c

**Lição:** o mais completo e o mais pedido é um agrupador de **TRIÂNGULOS INDEXADOS** com cor por vértice pré-multiplicada. Retângulo, quadrilátero, linha grossa, contorno, círculo e polígono viram geometria por cima dele. Isso vale para os três lados medidos: SDL3 (geometria como primitiva universal), RmlUi (geometria indexada, pré-multiplicada) e raylib (todas as formas desenham triângulos). **RmlUi e SDL3 não têm nada a ensinar ao `GFX-PRESET`** (nenhum dos dois tem predefinição gráfica), e isso fica declarado, não silenciado.

---

## 3. Decisões novas (L-34, formato da lei)

### D-W7D-18. O agrupador do desenho 2D é por triângulo indexado, e as primitivas além de retângulo e quadrilátero entram como item próprio depois da demo

- **Pergunta que teria ido ao líder:** "O desenho 2D nasce agrupando só quadriláteros, como diz o item, ou por triângulos indexados? E as formas que a comunidade mais usa (geometria livre, linha grossa, contorno, círculo) entram agora?"
- **Opções:**
  - (a) agrupador de quadrilátero e API pública só com `fill_rect`/`fill_quad` (o plano de 23/09);
  - (b) agrupador de triângulo indexado por dentro, API pública v1 com `fill_rect`/`fill_quad`, e as formas mais pedidas desenhadas agora e executadas como item novo `R2D-SHAPES` logo depois da `DEMO-1`;
  - (c) todas as formas já na W7-D.
- **Fontes:** §2 itens 1 a 4 (SDL3, `SDL_RenderGeometry` com a demanda medida, RmlUi 6, raylib); G12 (a L-32).
- **Escolha: (b).**
  - É o mais completo que a lei deixa executar antes da demo.
  - O MECANISMO (triângulo indexado, vértice com posição, coordenada de textura e cor pré-multiplicada, índice de 32 bits) não é escopo novo: é como `R2D-BATCH` agrupa, e é o que evita reabrir o formato quando `R2D-SHAPES`, `R2D-TEXTURE` e o `gfui` chegarem.
  - (c) contraria a extensão de 27/08 da L-32 (G12): o escopo novo é registrado e desenhado, não executado antes de a janela desenhar. As leis do projeto vencem o desejo da comunidade (salvaguarda da emenda de 29/09).
  - (a) é o mais simples, e a emenda proíbe decidir por isso.
- **O que `R2D-SHAPES` desenha agora (sem executar):**
  - `fill_triangles` (geometria livre, com vértices de mundo e cor por vértice, índices opcionais: o par do `SDL_RenderGeometry`);
  - `stroke_line` (linha com espessura);
  - `stroke_rect` (contorno);
  - `fill_ellipse` (tesselação com número de segmentos derivado do raio NA TELA, determinístico).
  - A revisão B0 vê os nomes dessas quatro JUNTO com os da v1, para a família nascer coerente, mas **só congela os da v1**.
- **Porta de mão única:** o formato de vértice é interno (não); a API da v1 é (sim), na B0. **Custo de reverter:** barato agora, caro depois de B4.

### D-W7D-19. Recorte da W7-D: produto primeiro, ferramental novo depois da W8

- **Pergunta:** "Os planos de 23 e 24/09 abrem a W7-D com três portões novos (Q1 conferência de todas as linhas de plano, W0 ordem de onda na tabela, B3a do adendo, direção de camada) e uma sonda isolada (B1). Eles entram agora?"
- **Opções:** (a) manter como os planos dizem; (b) cortar para depois da W8 e aplicar a arrumação da tabela como edição de texto do main; (c) cortar e não registrar.
- **Fontes:** a ordem do líder de 22:33 (seção 0); a medição dele (0 arquivos de produto em 109 commits).
- **Escolha: (b).**
  - Q1, W0 e a B3a do adendo (portão de direção de camada; não confundir com a B3a de produto da §4.B) vão para a onda `INFRA-CI` (logo depois da W8), com linha própria na tabela (`PLAN-SCOPE-COMPARE-ALL`, `TODO-DEP-ORDER`, `LAYER-DIRECTION-GATE`).
  - B1 se dissolve em B5: as chaves `MEASURED` do estado de GL entram no próprio teste de paridade do desenho, que já roda nos dois sistemas.
  - A arrumação da cadeia de entrada (W0b) vira edição do `TODO.md` feita pelo main, sem portão.
- **Perdas declaradas:**
  1. até o `LAYER-DIRECTION-GATE` existir, "plataforma nunca inclui `draw2d`" e "`draw2d` nunca inclui cabeçalho do sistema" são conferidos pela revisão (B7, com as cinco perguntas da L-17 e um `grep` das diretivas no relatório), não por portão. Quando o portão chegar, ele varre a árvore inteira e pega qualquer violação que tenha passado;
  2. a arrumação da tabela fica sem trava contra a volta até o `TODO-DEP-ORDER`;
  3. a conferência por máquina de todas as linhas de plano fica para depois.
- **Porta de mão única:** não. **Custo de reverter:** barato (as linhas estão registradas).

### D-W7D-20. Onde a W7-D nasce

- **Pergunta:** "O ramo de trabalho está 243 commits à frente de `origin/main` (8 de produto), a W7-C não fecha pela definição original, e o líder mandou o resto dela para depois da W8. De onde nasce a W7-D?"
- **Opções:**
  - (a) do `origin/main` de hoje (`393ec30`): perde os 243 commits, inclusive a guarda de display morto do EGL e todo o ferramental que a W7-D usa (`preci`, `contido`, CI por sistema);
  - (b) em cima do ramo da W7-C, sem mesclá-la: o PR da W7-D arrastaria 243 commits de outra onda, o `main` continuaria em `v0.5.1.0` por mais uma onda, e a W7-C nunca fecharia;
  - (c) **fechar a W7-C agora, com o recorte que o líder fez**, mesclar no `main` e abrir a W7-D do `origin/main` novo.
- **INF, marcada:** "a W7-C fecha agora" é leitura minha da ordem do líder, não palavra dele. O verbatim de 22:33 manda o resto para depois da W8 e não diz "feche e mescle". A autoridade para o merge e a marca vem de dois textos dele: *"ligue modo autonomo. Autorizo push/tag/merge"* (`d04ffbd`) e o recorte de 22:33.
- **FATO novo (commit `e948847`, ordem do líder):** as 6 linhas PENDENTES da W7-C passaram a ter Onda = `INFRA-CI`, logo depois da W8: `CI-SPLIT-PER-OS`, `PLATFORMS-ASTRO-PARITY`, `WL-ACK-SMOKE-BLUNT`, `PLAN-SCOPE-COLUMNS`, `DISPLAY-PASSKEY-CONTROL` e **`CI-VERDE-W7C`**. As 9 concluídas continuam W7-C. **Consequência:** a W7-C fica com só itens ✅, e a linha de CI dela mudou de onda. Então o portão do merge NÃO é a célula `CI-VERDE-W7C`: é o servidor verde no commit de fechamento, lido direto (L-11 do projeto: merge com tudo verde; CI vermelho bloqueia).
- **Registro obrigatório:** a definição de fechamento escrita da W7-C (o plano dela e o adendo, inclusive o critério 10) NÃO está cumprida como foi escrita. Por isso o main grava no `DECISOES_AUTONOMAS.md` a **definição de fechamento nova** da W7-C: "as 9 linhas ✅ da W7-C; os 6 itens pendentes movidos para `INFRA-CI` por ordem do líder (`e948847`, ordem de 22:33 em `612db94`); servidor verde no commit de fechamento, lido de `gh run view --json conclusion,jobs`, com a lista de trabalhos conferida contra o `ci.yml`". Sem esse registro, o passo 5 não roda.
  - Nota para o main, sem decisão minha: a linha `CI-VERDE-W7C` agora vive em `INFRA-CI` com o nome da onda antiga. Se ela passa a ser a linha de fechamento da própria `INFRA-CI` (renomeada) ou se a `INFRA-CI` ganha uma linha própria, é arrumação de tabela do main.
- **Fontes:** G9, G10, G14; a ordem do líder de 22:33 (o recorte da W7-C é dele); a L-11 do projeto (merge e marca ao fim de onda com tudo verde, número pela L-26, ramo apagado ao mesclar); a L-24 global (concluir a onda antes da seguinte, pelo critério escrito); o adendo de 24/09, D-W7D-R10 (a W7-D abre do `origin/main` depois do merge da W7-C, pela marca, N15).
- **Escolha: (c)**, com esta sequência. O main executa; os passos 1 e 2 são do implementador.
  1. A C1b termina (a prova D0-D12 e os mutantes, depois o commit, pela revisão do CTO).
  2. **O conserto do `d8641c9` (D-W7D-21)**, em commit próprio.
  3. `tools/preci.sh --fast` contido; push de `onda-w7c`; CI verde lido de `gh run view --json conclusion,jobs`, com a lista de trabalhos conferida contra o `ci.yml` (L-36).
  4. **Feito pelo main em `e948847`:** os itens abertos da W7-C foram para a onda `INFRA-CI`. Falta só gravar a definição de fechamento nova (acima). O portão do merge é o CI do passo 3.
  5. PR `onda-w7c` → `main` e merge. **Não é avanço rápido (G14):** o merge cria um commit de merge sobre `393ec30`, sem conflito medido. A marca **`v0.5.2.0`** vai NESSE commit de merge, no `origin/main`, pela L-26 (a W7-C entrega correção de produto, a guarda do EGL, sem recurso novo nem quebra: sobe o C). O `project(... VERSION 0.5.2.0)` e o `VERSION-TAG-SYNC` ficam coerentes ANTES da marca, num commit do ramo, antes do PR.
     - **A marca exige a válvula do hook de publicação:** o modo autônomo tem a flag, e a ordem do líder "Autorizo push/tag/merge" está registrada em `d04ffbd`. O hook casa QUALQUER comando com a palavra de marca, até uma listagem (medido às 22:3x). Então o main prefixa a válvula também nas leituras, ou lê por `git ls-remote origin`.
     - Se o `git merge-tree` passar a mostrar conflito no dia (outro commit em `main`), a resolução é do main, conferida pelo CTO no blob do commit de merge, antes da marca.
  6. O ramo `onda-w7d` nasce do `origin/main` novo. Antes do primeiro commit, o main confere `git merge-base --is-ancestor v0.5.2.0 onda-w7d` (tem de sair 0).
- **Porquê:** (c) é a única em que cada onda fecha com PR, CI e marca próprios, o PR da W7-D só tem a W7-D, e o `main` recebe já o conserto de produto do EGL, que hoje só existe no ramo.
- **Porta de mão única:** a marca e o merge em `main` são irreversíveis na prática (L-25: nunca force-push em `main`). **Custo de reverter:** alto depois do passo 5; barato antes.

### D-W7D-21. O que fazer com o `d8641c9` (REPROVADO, D-A38)

- **Pergunta:** "O `d8641c9` está no histórico local, não empurrado, com um registro de ctest que dá verde falso (`PASS_REGULAR_EXPRESSION` ignora o rc, provado em 22:20). Como o ramo pode ser empurrado sem levar esse teste?"
- **Opções:**
  - (a) reescrever o histórico e tirar o commit: é destrutivo com agente ativo na árvore (L-25 global) e mexe no índice onde a C1b está STAGED;
  - (b) `git revert d8641c9`: seguro, mas desfaz também o que a revisão ACEITOU no mesmo commit (o controle de orientação do diagnóstico, as pistas do gêmeo cross, a INBOX);
  - (c) commit de conserto mínimo por cima: tirar o `PASS_REGULAR_EXPRESSION` do registro do `fase_sh_selftest`, o rc volta a ser o veredito, e o cinto `FAIL_REGULAR_EXPRESSION "FALHOU"` do projeto entra no lugar;
  - (d) o CI-CMP-5 inteiro agora (os 2 controles de `chmod 000` num ctest próprio, com `SKIP_RETURN_CODE 77`).
- **Fontes:** D-A38 e o §R1-Q4 de `/var/tmp/cto-a5-paralelo/L34-RETRO.md` (o ctest mínimo que mediu o verde falso; o manual do CMake: "The process exit code is ignored"); a ordem do líder de 22:33 (o CI-CMP-5 vai para depois da W8).
- **Escolha: (c).**
  - (d) seria a mais completa, mas a ordem do líder a adia pelo nome (CI-CMP-5), e a ordem dele vence.
  - (a) viola a L-25.
  - (b) joga fora o trabalho revisado e aceito.
  - (c) tira o verde falso e preserva o resto.
- **O que continua pendente, declarado:** a linha `pulados: <k>` continua impressa mas não é EXIGIDA por ninguém (é o estado do `b168454`, que eu já tinha classificado como IMPORTANTE). O conserto completo é o CI-CMP-5, depois da W8.
- **Critério de aceite do conserto, fixado agora (a régua do revisor):** o harness do ctest mínimo de 22:20, numa cópia fora da árvore, com o registro NOVO do blob:
  - "OK e depois return 1" dá **Failed**;
  - "OK e depois exit 3" dá **Failed**;
  - o original dá **Passed**;
  - "sem a parte `pulados:`" dá **Passed**. É a perda declarada acima, e tem de aparecer no relatório como tal, não como defeito novo.
  - Mais o `fase_sh_selftest` verde no fedora:latest como root e como uid 1000, e revisão do CTO no blob.
- **Ordem:** depois do commit da C1b, porque os dois tocam `tests/CMakeLists.txt` e a C1b está STAGED nele (memória `feedback_indice_compartilhado_engole_wip`).
- **Porta de mão única:** não. **Custo de reverter:** barato.

### D-W7D-22. `INPUT-EVENTS` sai da W7-D pela arrumação já ratificada, sem portão

- **Pergunta:** "A W7-D fecha com `INPUT-EVENTS` dentro?"
- **Escolha:** não, pela D-W7D-01, ratificada em 24/09. O main aplica a cadeia na tabela no PRIMEIRO commit do ramo `onda-w7d`:
  - `INPUT-EVENTS` → W9, logo antes de `CI-VERDE-W9`, com `WL-KEYBOARD, WL-POINTER, WIN-INPUT`;
  - `WIN-INPUT` com `WIN-WINDOW, INPUT-SEQUENCE-CORE`;
  - `WL-KEYBOARD` e `WL-POINTER` ganham `INPUT-SEQUENCE-CORE`;
  - linhas novas: `INPUT-SEQUENCE-CORE` (W9, primeira), `INPUT-TEXT` e `INPUT-KEY-LOGICAL` (W11c), `INPUT-ACTIONS` (W11c), `MAP-STEP-RESOLVE` (W11a), `R2D-BATCH-OPTIMIZER` (W9-B), `GFX-POWER-SAVER` (W9-B);
  - e as de D-W7D-18/19: `R2D-SHAPES` (W8, depois de `DEMO-1`) e `PLAN-SCOPE-COMPARE-ALL`, `TODO-DEP-ORDER` e `LAYER-DIRECTION-GATE` (na onda `INFRA-CI`);
  - corrigir as células podres de `R2D-BATCH` e `DEMO-1` ("Bloqueado transitivamente ... GL-API").
- **Conferência do main, sem portão novo:** um `awk` de processo único que lista toda dependência para onda posterior das linhas W7-D, W8, W9 e W11c. As 4 de `INPUT-EVENTS` têm de sumir, e nenhum ciclo `WIN-INPUT`/`INPUT-EVENTS` pode aparecer. A saída vai no relatório.
- **Porta de mão única:** não. **Custo de reverter:** barato.

---

## 4. Fatias da W7-D

Esquema v1 (as colunas que o portão de escopo lê), com os caminhos reais. **Uma implementação por vez na árvore** (todas tocam `tests/CMakeLists.txt`). As revisões P0 e B0 só leem e escrevem fora da árvore, e podem andar em paralelo com a implementação (teto de 4 agentes vivos, L-11 global). Um trabalho pesado por vez. Toda execução de teste de agente é contida (`systemd-run --user --scope -p TasksMax`, ou `tools/contido.sh` quando a C1b estiver commitada); o docker leva `--pids-limit` e `--pull=never`, com a fonte existindo antes do `-v`.

### 4.A `GFX-PRESET` (desenho: `docs/plano-w6b-fatias-5.md` §5 e o adendo §3.A, válidos)

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| P0 | Revisão de API dedicada da predefinição (C-level distinto do implementador e de mim) | comum | `docs/auditoria-api-gfx-preset.md` (parecer e o texto congelado de `suggested_preset` e dos dois acessores `gltfx_gfx_preset_row_count`/`gltfx_gfx_preset_row_at`) | Os três nomes do rascunho compilados contra o portão de colisão antes do parecer | `public_name_collision_test` | `public_name_collision_test` | `public_name_collision_selftest` |
| P1 | **PRODUTO:** átomos puros sem alocação | comum | `src/platform/gl/gfx_preset_table.{hpp,cpp}`, `src/platform/gl/auto_preset_rule.{hpp,cpp}`, `src/platform/gl/preset_expansion.{hpp,cpp}`, `src/platform/gl/CMakeLists.txt`, `tests/gfx_preset_table_test.cpp`, `tests/auto_preset_rule_test.cpp`, `tests/preset_expansion_test.cpp`, `tests/CMakeLists.txt`, `tests/parity_absences.txt` (apaga as linhas cujos caminhos esta fatia cria) | Os três testes antes do `.cpp`: 12 células em `gfx_preset_table_test`, 12 em `auto_preset_rule_test`, contagem impressa, zero reprova. Mutante: `preset_expansion` devolvendo `std::vector` sob `noexcept` faz o `noexcept_alloc_test` reprovar | `gfx_preset_table_test`, `auto_preset_rule_test`, `preset_expansion_test` | idem | `noexcept_alloc_test` |
| P2 | **PRODUTO:** leitura da fonte de energia nos dois sistemas | ambos | `src/platform/port/power_source_port.hpp`, `src/platform/wayland/power_supply_rule.{hpp,cpp}`, `src/platform/wayland/power_source_adapter.{hpp,cpp}`, `src/platform/wayland/selected_power_source_adapter.hpp`, `src/platform/wayland/selected_power_source_adapter_check.cpp`, `src/platform/win32/power_status_rule.{hpp,cpp}`, `src/platform/win32/power_source_adapter.{hpp,cpp}`, `src/platform/win32/selected_power_source_adapter.hpp`, `src/platform/win32/selected_power_source_adapter_check.cpp`, os três `CMakeLists.txt` de `src/platform/{port,wayland,win32}`, `tests/tools/check_port_privacy.sh`, `tools/ci/check-port-privacy-win.ps1`, `tests/power_supply_rule_test.cpp`, `tests/power_status_rule_test.cpp`, `tests/CMakeLists.txt`, `tests/parity_absences.txt` (apaga as restantes de `GFX-PRESET`) | 14 células no Linux (`scope`, `present`) e 5 no Windows (com a célula própria `255`), vermelhas antes; os dois portões de privacidade de porta reprovam a classe nova antes de a lista a conhecer | `power_supply_rule_test`, `power_status_rule_test`, `port_privacy_test` | `power_supply_rule_test`, `power_status_rule_test`, `port_privacy_win_test` | `port_privacy_selftest` |
| P3 | **PRODUTO:** fachada, registro e `suggested_preset` | comum | `include/glintfx/platform/gl/gfx_option.hpp`, `src/platform/gl/gfx_option_registry.{hpp,cpp}`, `src/platform/gl/gl_context_facade.cpp`, `src/platform/gl/gl_context_impl.hpp`, `src/platform/wayland/egl_context_adapter.{hpp,cpp}`, `src/platform/win32/wgl_context_adapter.{hpp,cpp}`, `tests/tools/check_gfx_option_ids.py`, `tests/gfx_option_registry_test.cpp` (o caso com "8" no nome, renomeado), `tests/container/Containerfile` (as fontes novas nas fixturas que ligam a fachada) | `static_assert` reprova o id 8 sem linha; o portão de ids reprova a linha sem registro; a fixtura do container sem as fontes novas não liga (visto) | `gfx_option_registry_test`, `gfx_option_ids_test`, `claim_citations_test` | idem | `container_fixture_inventory_test` |
| P4 | Prova viva e texto público verdadeiro | ambos | `tests/parity/gl_context_parity_test.cpp` (perguntar não grava; o rótulo é do consumidor; `automatic` grava o concreto; `power_saving` com o teto OBEDECIDO pelo laço; `MEASURED` de `power_source`, `auto_choice_reason` e `suggested_preset`), `tests/measured_exceptions.txt` (só pela regra D-W7D-R4), `docs/gl-loop-portability-matrix.md`, `docs/wiki/API-Graphics-Context.md`, `include/glintfx/platform/gl/context.hpp` | Na árvore de P3 sem a regra 7, reprova com `perguntar mudou preset`; com o teto não aplicado, reprova pela cadência. Os sete sítios de texto (adendo, D-W7D-R9) reescritos pelo efeito real | `gl_context_parity_test`, `loop_parity_test` | idem | `measured_parity_textual_test`, `claim_citations_test` |
| P5 | Revisão adversarial que executa (agente distinto) | comum | `docs/auditoria-revisao-gfx-preset.md` (as mutações do plano de 06/09 ainda aplicáveis, em cópia fora da árvore, com a saída vermelha; as cinco perguntas da L-17 por unidade criada) | Cada mutação vista vermelha; `tools/preci.sh --sanitizer-only` verde, com o código lido de variável | os de P1, P2 e `gl_context_parity_test` | `power_status_rule_test`, `gl_context_parity_test` | `noexcept_alloc_test` |

**Fecha quando:** o adendo §3.A, itens 1 a 6, com o item 5 corrigido para "de 10 linhas para 0" (G8). O item 7 (`--compare`) sai, porque o instrumento foi para depois da W8 (D-W7D-19).

### 4.B `R2D-BATCH` (desenho: `docs/plano-w7d.md` §4.1-4.3; o adendo §3.B; D-W7D-18)

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| B0 | Revisão de API dedicada do desenho 2D | comum | `docs/auditoria-api-draw2d.md` (R1 a R10; as seis perguntas do plano; os nomes da v1 CONGELADOS com o texto inteiro dos cabeçalhos; os nomes de `R2D-SHAPES` vistos para coerência e NÃO congelados, D-W7D-18) | Os nomes do rascunho (`gltfx_renderer_2d`, `fill_rect`, `fill_quad`, `flush`, `layer`, `gltfx_quad_world`, mais os quatro de `R2D-SHAPES`) contra o portão de colisão | `public_name_collision_test` | idem | `public_name_collision_selftest` |
| B2a | **PRODUTO:** mundo em double para pixel em float | comum | `src/draw2d/quad_vertices.{hpp,cpp}`, `src/draw2d/CMakeLists.txt`, `src/CMakeLists.txt`, `tests/quad_vertices_test.cpp`, `tests/CMakeLists.txt` | Uma peça em x = 16 777 217 com câmera em -16 777 216 sai em pixel 1,0; estreitando ANTES de transformar, sai 0,0 (o mutante que prova a decisão de precisão) | `quad_vertices_test` | idem | `noexcept_alloc_test` |
| B2b | **PRODUTO:** ordem total por (camada, submissão) | comum | `src/draw2d/draw_order.{hpp,cpp}`, `tests/draw_order_test.cpp`, `tests/CMakeLists.txt` | As 8 células {sem chave, igual, menor, maior} × {AB, BA} com contagem impressa, zero reprova; 1 000 peças de camada igual na ordem de submissão | `draw_order_test` | idem | `noexcept_alloc_test` |
| B2c | **PRODUTO:** montagem do lote por TRIÂNGULO INDEXADO (D-W7D-18) | comum | `src/draw2d/triangle_batch.{hpp,cpp}` (vértice de 32 bytes: posição float2 em pixel, coordenada de textura float2, cor float4 linear pré-multiplicada; índice u32; corridas por chave de estado), `tests/triangle_batch_test.cpp`, `tests/CMakeLists.txt` | Um quadrilátero gera 4 vértices e 6 índices; junta quando o estado é igual e separa quando difere (as duas direções da dor raylib#6110/#4849); a cor sai pré-multiplicada (alfa 0,5 sobre 1,0 dá 0,5 no canal); o alocador armado falha no meio, a peça é contada como descartada e nada lança | `triangle_batch_test` | idem | `noexcept_alloc_test` |
| B2d | **PRODUTO:** relato do quadro | comum | `src/draw2d/frame_report_tally.{hpp,cpp}`, `tests/frame_report_tally_test.cpp`, `tests/CMakeLists.txt` | Uma peça com NaN é contada como recusada com o token da PRIMEIRA razão; um quadro vazio relata zero; falta de memória vira contagem e um erro `out_of_memory` só no fim | `frame_report_tally_test` | idem | `noexcept_alloc_test` |
| B3a | **PRODUTO:** carregador GL com contexto (D-W7D-15) | comum | `tools/gl_registry_codegen/loader_codegen.cpp`, `tests/gl_loader_codegen_test.cpp`, `src/render/gl_proc_address.hpp` | O teste do gerador reprova a forma sem `void *user` antes de o gerador mudar | `gl_loader_codegen_test` | idem | `gl_codegen_host_leak_test` |
| B3b | **PRODUTO:** programa de sombreamento embutido | comum | `src/draw2d/embedded_program.{hpp,cpp}` (GLSL 330 core; uma falha dá `platform_failure` com `rejected_value` = `vertex_shader`/`fragment_shader`/`program_link` e o registro do driver como campo, R7), `src/platform/gl/gl_context_impl.hpp` (acessor interno do resolvedor e do tamanho da superfície), `tests/parity/draw2d_parity_test.cpp` (primeira célula: o programa compila e liga no contexto real), `tests/CMakeLists.txt`, `tests/container/Containerfile` | Um texto de sombreamento sabotado (cópia fora da árvore) faz a célula reprovar com o token certo | `draw2d_parity_test` | idem | `container_fixture_inventory_test` |
| B3c | **PRODUTO:** fluxo de vértices e contrato de estado GL | comum | `src/draw2d/vertex_stream.{hpp,cpp}`, `src/draw2d/gl_state_contract.{hpp,cpp}`, `tests/parity/draw2d_parity_test.cpp` (célula de estado hostil plantado: profundidade, mistura trocada, recorte de 1 pixel, descarte de face, máscara de cor; e as chaves `MEASURED` da antiga B1: estado após `make_current`, VAO no perfil core, `GL_FRAMEBUFFER_SRGB`, custo das duas técnicas de envio para 10 mil peças) | Sem `glDisable(GL_DEPTH_TEST)`/recorte na descarga, o pixel lido fica errado; zero chaves `MEASURED` reprova (L-40) | `draw2d_parity_test` | idem | `measured_parity_textual_test` |
| B4 | **PRODUTO:** fachada pública conforme B0 | comum | `include/glintfx/draw2d/renderer_2d.hpp`, `include/glintfx/draw2d/renderer_2d_desc.hpp`, `include/glintfx/draw2d/frame_2d_report.hpp`, `include/glintfx/draw2d/quad_world.hpp`, `src/draw2d/renderer_2d_facade.cpp`, `src/draw2d/renderer_2d_impl.hpp`, `tests/header_hygiene_test.cpp` | As células de B5 escritas contra o cabeçalho antes de a fachada existir; os cabeçalhos entram com o texto de B0 MENOS as alegações que citam B5 (D-W7D-R2); a contagem de cabeçalhos varridos pelos portões sobe e o número é conferido | `header_hygiene_test`, `public_name_collision_test`, `claim_citations_test`, `visibility_test` | `header_hygiene_test`, `public_name_collision_test`, `claim_citations_test`, `exports_win_test` | `facade_export_boundary_test` |
| B5 | Prova viva por leitura de pixel | ambos | `tests/parity/draw2d_parity_test.cpp` (as 8 células de ordem lidas no pixel; borda pré-multiplicada; `srgb_framebuffer` ligado e desligado; quadro vazio; dois retângulos sem fresta nem sobreposição; uma chamada de desenho para N peças de estado igual), os cabeçalhos de `include/glintfx/draw2d/` (só as alegações que B0 congelou citando este teste, byte a byte) | Cada célula vista vermelha contra uma cópia sabotada (as mutações de `docs/plano-w7d.md` §4.5) | `draw2d_parity_test` | idem | `claim_citations_test`, `measured_parity_textual_test` |
| B6 | Texto público do desenho 2D | comum | `docs/draw2d-portability-matrix.md`, `docs/wiki/API-Draw-2D.md`, `docs/api-conventions.md` (só se B0 criar a regra do relato único) | Os portões de documento reprovam o rascunho com contagem por número ou travessão | `docs_count_vocab_test`, `dash_pubdoc_test` | idem | `docs_count_vocab_selftest` |
| B7 | Revisão adversarial que executa (agente distinto) | comum | `docs/auditoria-revisao-r2d-batch.md` (as nove mutações de `docs/plano-w7d.md` §4.5 mais duas de D-W7D-18: o quadrilátero com índice trocado e a cor sem pré-multiplicar; as cinco perguntas da L-17 por unidade; o `grep` de diretivas que substitui o portão de direção de camada, D-W7D-19 perda 1) | Cada mutação vista vermelha; `tools/preci.sh --sanitizer-only` verde | os de B2a-B2d, `draw2d_parity_test` | `draw2d_parity_test` | `noexcept_alloc_test` |

**Fecha quando:** o adendo §3.B, itens 1 a 5, com duas mudanças: o item 6 sai (é o `--compare`, D-W7D-19), e entra um item novo:
- o `grep` de diretivas de B7 mostra zero `#include` de `draw2d` em `src/platform/`, **sem distinção de caixa** (o Windows abre `Draw2D/x.hpp`, a fuga que a B3a do adendo listava);
- e mostra zero cabeçalho do sistema (`<windows.h>`, `<wayland-client.h>`, `<EGL/...>`) em `src/draw2d/`;
- controles numa cópia: uma inclusão plantada em minúsculas, uma com caixa trocada e um cabeçalho do sistema plantado. Os três têm de ser achados; se algum não for, o `grep` está cego.

**Espelho por fatia (L-23, emenda de 24/09):**
- P1, B2a-B2d e B6: `tools/preci.sh --fast`;
- P2: `--fast` mais o estágio Windows do espelho;
- P3, B3b, B3c, B4 e B5: `tools/preci.sh` completo, porque mudam a ligação no container ou código de produto sob sanitizer e Windows;
- P5 e B7: `--sanitizer-only`.
- Sempre contido, e com o `watchcode` armado nos pesados (L-25 do projeto).

---

## 5. Ordem de execução

| Ordem | O quê | Tipo | Por que aqui |
|---|---|---|---|
| 0 | Fecho da W7-C: C1b, o conserto do `d8641c9`, push, CI verde, rotulagem, PR, merge, `v0.5.2.0` (D-W7D-20, D-W7D-21) | fechamento | a ordem do líder: fecha-se só a C1b |
| 1 | `onda-w7d` do `origin/main` novo; arrumação da tabela pelo main (D-W7D-22) | tabela | nada se despacha para linha que não existe |
| 2 | **P0 e B0 JÁ, em paralelo com o passo 0**, fora da árvore, por um C-level distinto (só leitura e parecer; os rascunhos são `docs/plano-w7d.md` §3.1 e §4.2 mais os nomes de D-W7D-18). O parecer entra por commit quando o ramo `onda-w7d` existir, com md5 conferido nas duas pontas (L-13 global) | revisão | não dependem de onde o ramo nasce, e são o único trabalho de framework que pode andar legitimamente durante o fecho da W7-C; há uma vaga no teto de 4 agentes (main, impl-da14 e eu) |
| 3 | P1 → P2 → P3 → P4 → P5 | 3 produto, 2 prova e revisão | ordem das linhas da tabela, L-32 emenda de 09/09 (G15). **Custo declarado:** o primeiro arquivo de `src/draw2d/` só nasce depois das cinco fatias de `GFX-PRESET`. Não atrasa a `DEMO-1`: a W8 só abre depois de a W7-D fechar (L-24 global), qualquer que seja a ordem dentro dela. E P1 a P3 são produto, então a cobrança do líder (código, não teste) é atendida desde a primeira fatia |
| 4 | B2a → B2b → B2c → B2d (podem começar em paralelo a P2-P3 SE houver um só implementador na árvore; senão, em fila) | 4 produto | átomos puros, sem GL |
| 5 | B3a → B3b → B3c → B4 → B5 → B6 → B7 (nada de B3b em diante antes de P3 commitado: colisão em `gl_context_impl.hpp`) | 4 produto, 3 prova, texto e revisão | encanamento, fachada, prova |
| 6 | `CI-VERDE-W7D`: servidor verde lido direto, lista de trabalhos contra o `ci.yml`, merge, marca **`v0.6.0.0`** (L-26: recurso novo compatível, sobe o B) | fechamento | sempre a última |

**Contagem:**
- 11 fatias de PRODUTO: P1, P2, P3, B2a, B2b, B2c, B2d, B3a, B3b, B3c, B4;
- 7 de revisão, prova ou texto: P0, P4, P5, B0, B5, B6, B7;
- 1 de fechamento.
- Ferramental novo: zero.

**Avisos no fechamento (continuam do plano de 23/09, §6 item 7):**
- ao Gus Dragon quando `R2D-BATCH` fechar (L-37), em linguagem para 11 anos e sem prometer data;
- ao `gusworld` pelo barramento, sobre onde os três requisitos de entrada foram parar;
- ao `mapeditor`, sobre `MAP-STEP-RESOLVE` (L-33).

---

## 6. O que NÃO medi

1. Se o executor Windows do servidor tem GL 3.3 capaz de leitura de pixel exata: as chaves `MEASURED` de B3c medem isso ANTES de B5 depender.
2. O custo real das técnicas de envio de vértice: B3c.
3. Se `flush`, `layer` e os nomes de `R2D-SHAPES` colidem com macro em algum alvo: a B0 roda o portão.
4. O nome da onda do ferramental adiado: resolvido pelo main, `INFRA-CI` (`e948847`).
5. Se a C1b fecha a prova D0-D12 sem achado bloqueante: o passo 0 depende disso, e a revisão do CTO decide.

## 7. Ataque a este plano

1. **"Fechar a W7-C incompleta viola a L-24 global."** Contenção: o recorte é ordem do líder (22:33); os itens não somem, mudaram de onda para `INFRA-CI` (`e948847`); o merge exige servidor verde no commit de fechamento, lido direto.
2. **"Cortar os portões novos deixa a camada `draw2d` sem trava."** Contenção: a perda está declarada (D-W7D-19 perda 1), o B7 confere por `grep` com controle plantado, e o portão, quando chegar, varre a árvore inteira.
3. **"Triângulo indexado é escopo novo escondido."** Contenção: nenhuma função pública nova além de `fill_rect`/`fill_quad` entra antes da demo; o que muda é o mecanismo interno. O custo medido é 2 índices a mais por quadrilátero (6 contra 4 vértices implícitos), e o que se ganha é não reabrir formato nem agrupador quando `R2D-SHAPES`, `R2D-TEXTURE` e o `gfui` chegarem.
4. **"A arrumação da tabela sem portão pode ficar pela metade."** Contenção: o `awk` de processo único do main, com a saída no relatório; e o `TODO-DEP-ORDER` registrado na `INFRA-CI`.
5. **Planejador e orquestrador são da mesma família** (L-34, emenda de 22/09): a re-verificação do main em P5 e B7 usa sabotagem de família diferente da do implementador e da minha.

<!-- Origem da errata abaixo: PLANO-errata.md, md5 9942bf8974e9f75585aebb1917a18266 -->

# Errata ao PLANO.md da W7-D (congelado em bd73502a64fa2b4c41d338ce4c09de1e)

**Autor:** CTO (quem planejou; C-level `opus`, modo autônomo, L-34 com a emenda de 29/09). **Data real:** 29/09/26, 23:2x. **Fonte:** o julgamento dos dois pareceres do `api-review`, que foram lidos inteiros:
- `/var/tmp/cto-w7d/auditoria-api-gfx-preset.md` (P0, md5 `0f60cbf6a1495a9781aefd2d2f243cd7`);
- `/var/tmp/cto-w7d/auditoria-api-draw2d.md` (B0, md5 `1006552f7bf0c982df6ee4a383320d72`).

**Regra de leitura:** onde esta errata diverge do PLANO.md ou dos pareceres, vale a errata. O texto congelado dos cabeçalhos é o dos pareceres (seção 2 de cada um), MAIS as emendas E1 e E2 abaixo.

---

## 1. Julgamento do P0 (`GFX-PRESET`)

**Veredito do CTO: ACEITO, com UMA emenda minha (E1) e UM achado promovido de INBOX a P3 (E2).**

| Decisão ou achado | Julgamento | Porquê |
|---|---|---|
| D-API-P0-1, os números das linhas ganham nome público (`k_gltfx_...`, `inline constexpr std::int64_t`) | **ACEITA** | O canal de valor é `std::int64_t`, congelado desde a W6b (D-W6b-17). Um `enum class` obrigaria `static_cast` em cada `set_option`; a constante não custa ABI e segue o precedente da casa (`k_gltfx_gpu_index_unknown`, `k_gltfx_max_frame_elapsed`) |
| D-API-P0-2, `row_at` fora da faixa degrada para `{preset, k_gltfx_preset_manual}` | **EMENDADA (E1)** | Ver E1 |
| D-API-P0-3, o parâmetro é `row_index` | **ACEITA** | Colisão MEDIDA com `X11/Xos.h:67` |
| P0-I4, `gltfx_gfx_option_at(std::size_t index)` tem a mesma colisão | **ACEITA, entra em P3** | Nome de parâmetro não é API nem ABI (custo zero para o consumidor), e P3 já edita esse cabeçalho. O novo nome é do implementador, desde que o portão e a sonda de parâmetro não o acusem |
| P0-I1, gêmeo: `vsync` e `gpu_preference` sem nome público | **`vsync` PROMOVIDO a P3 (E2)**, contra a recomendação do revisor; **`gpu_preference` fica na INBOX**, como o revisor recomendou | Ver E2 |
| P0-I5, o portão de citação aceita um bloco com UMA citação válida | **ACEITO, linha nova na INFRA-CI** (a mesma de B0-I7) | É ferramental; D-W7D-19 |
| P0-K1, "v1 ships exactly the eight ids" fica falso | **ACEITO, em P3**, sem número no texto | Regra `DOC-ESTADO` |
| P0-K2, sem função que devolva o token de dado (`"balanced"`) | **ACEITO, INBOX pós-demo** | É conveniência; o contrato é o número (L-32) |

### E1. Emenda do CTO à D-API-P0-2: `row_at` fora da faixa degrada para `{suggested_preset, k_gltfx_preset_manual}`

- **Pergunta que teria ido ao líder:** "Uma leitura fora da faixa devolve uma entrada que, aplicada por engano, faz o quê?"
- **Opções:**
  - (a) `{vsync, 0}`: desliga a sincronia em silêncio (recusada pelo próprio revisor);
  - (e) `{preset, manual}` (a do parecer): não muda valor de linha nenhuma, mas **reescreve em silêncio o rótulo do consumidor para `manual`**, e o rótulo é dele (regra 3 do bloco "SUGGESTED PRESET" que o próprio parecer congela);
  - (f) `{suggested_preset, k_gltfx_preset_manual}`: um id `read_only`.
- **Fontes:**
  - FATO: `set_option` com id `read_only` é RECUSADO com `invalid_argument` e `rejected_value` = nome da linha (`src/platform/gl/gfx_option_validation.cpp:42-44`);
  - R4 (a degradação nunca é comportamento indefinido e é distinguível: o precedente `gltfx_gfx_option_at` degrada para um `info` de nome vazio, reconhecível);
  - a regra 3 do bloco do parecer.
- **Escolha: (f).**
  - Aplicada por engano, é RECUSADA em voz alta com um erro que o consumidor vê: nada muda, nem valor nem rótulo.
  - Lida, é reconhecível sem constante sentinela nova: um id `read_only` nunca é linha de predefinição, por construção.
  - (e) troca uma falha silenciosa por outra menor, e (f) não tem falha silenciosa nenhuma.
- **O texto do cabeçalho** (o parágrafo "An out-of-range `preset` or `row_index` degrades ...") muda para dizer: *"degrades to the entry {suggested_preset, k_gltfx_preset_manual}: an id that is read_only, so handing it to set_option() by mistake is refused with invalid_argument and changes nothing - neither a row nor your preset label. It is never a default-constructed entry, because {vsync, 0} would silently turn vsync off."*
- **Prova em P1** (`gfx_preset_table_test`): a célula fora da faixa confere o par exato. **Prova em P4** (`gl_context_parity_test`): aplicar o par degradado dá `invalid_argument` e deixa `option(preset)` igual ao de antes.
  - Mutante: degradar para `{preset, manual}` faz a célula de P4 ver o rótulo mudar.
- **Porta de mão única:** sim. **Custo de reverter:** alto depois de P3; zero agora.

### E2. Os valores de `vsync` ganham nome público em P3; os de `gpu_preference` ficam na INBOX

- **Pergunta:** "O gêmeo que o revisor achou (P0-I1: `vsync` 0/1/2 e `gpu_preference` 0/1/2 sem nome público) é escopo novo, e portanto INBOX, ou é parte de `GFX-PRESET`?"
- **Correção minha, antes de gravar:** eu tinha escrito que as linhas de predefinição devolvem também `gpu_preference`. O próprio parecer P0 (bloco "SUGGESTED PRESET") e a D-W7D-07 dizem que HOJE as linhas só tocam `vsync` e `frame_rate_cap`; `gpu_preference` só entra nas linhas quando for honrada (`GL-GPU-PREFERENCE`, depois). Por isso o recorte abaixo separa os dois.
- **Fatos:** as linhas de predefinição que `gltfx_gfx_preset_row_at` DEVOLVE hoje são entradas de `vsync` e de `frame_rate_cap`; `gpu_preference` entra só quando for honrada (`docs/plano-w6b-placa-e-laco.md:285`; D-W7D-07). O consumidor que lê uma linha recebe `{vsync, 1}` e precisa saber o que é o `1`.
  - É exatamente o argumento que o próprio parecer usou para P0-I1 ("ler 2 e não saber que é balanced anula o item").
  - `frame_rate_cap` é `integer` (Hz), e não precisa de nome.
- **Opções:** (a) INBOX pós-demo para os dois (o parecer); (b) P3 para os dois; (c) P3 para `vsync` (que sai de `row_at` hoje) e INBOX para `gpu_preference` (que não sai).
- **Escolha: (c).** As constantes `k_gltfx_vsync_off/on/adaptive` (nomes finais do implementador, com os valores do registro interno) NÃO são escopo novo: é o que torna legível a saída de uma função que ESTA fatia publica. Sem elas, `row_at` entrega metade do contrato, e a L-17 global manda consertar o gêmeo da mesma frase.
  - `gpu_preference` não sai de `row_at` hoje: nomeá-lo agora seria escopo novo antes da demo (L-32), e ele entra junto com `GL-GPU-PREFERENCE`, quando a linha de predefinição passar a tocá-lo. INBOX.
- **Quem fixa os nomes exatos:** o implementador de P3, com os valores do registro. O `api-review` confere na revisão do commit de P3 (portão de colisão e sonda de nome). Não é uma segunda revisão dedicada: é a mesma família de constantes, com a mesma regra.
- **Porta de mão única:** sim (nomes públicos). **Custo de reverter:** alto depois de P3.

---

## 2. Julgamento do B0 (`R2D-BATCH`)

**Veredito do CTO: ACEITO INTEIRO.** Os dois CRÍTICOS e as dez decisões ficam como o parecer propôs.

### Os dois CRÍTICOS

- **B0-C1: ACEITO.**
  - FATO conferido por mim: `gltfx_mat3` é `std::array<float, 9>` (`include/glintfx/core/mat3.hpp:88-90`), e `float(16777217.0)` = `16777216.0` (medido, python `struct`).
  - `begin_batch(const gltfx_mat3 &)` estreitaria a câmera antes da subtração, e o teste de B2a do meu plano usava -2^24, que é exato em float: **o defeito é do MEU plano**, e o teste não o pegaria.
  - `begin_batch(gltfx_transform)` (double, `core/transform.hpp:57-61`: translação, rotação e escala) é o único tipo que honra a decisão de precisão do líder hoje sem escopo novo.
  - A matriz afim geral em double entra depois como SOBRECARGA (`R2D-TRANSFORM`, W10), acréscimo compatível.
- **B0-C2: ACEITO.**
  - Um `gltfx_rslt` é valor OU erro; a D-W7D-13 do plano de 23/09 prometia os dois juntos, o que o envelope não comporta.
  - `finish_frame() -> gltfx_rslt<gltfx_frame_2d_report>` mais `last_frame_report()`, legível também quando o quadro falhou, **SUBSTITUI a parte "erro com os campos de contagem" da D-W7D-13.**

### As dez decisões D-API

| Decisão | Julgamento | Porquê, em uma linha |
|---|---|---|
| D-API-01 `begin_batch(gltfx_transform)` | ACEITA | B0-C1 |
| D-API-02 `last_frame_report()` | ACEITA | B0-C2; a opção (c) é a única com o envelope único e sem perder as contagens |
| D-API-03 relatório com campos nomeados | ACEITA | A L-28 (legibilidade) vence o vetor indexado; a regra de ABI verdadeira fica escrita (B0-I2); a revisão da 1.0 reavalia |
| D-API-04 `gltfx_draw_layer layer = {}` (tipo forte e argumento padrão) | ACEITA | Metade da superfície exportada; o padrão 0 É "a submissão manda", a decisão do líder de 27/08 |
| D-API-05 `core/quad.hpp`, cantos por papel | ACEITA | Tipo de valor puro mora no núcleo (L-19), ao lado de `gltfx_rect_world`; o papel do canto serve à `R2D-TEXTURE` sem reabrir o tipo |
| D-API-06 o contexto vive mais que o desenhador (pré-condição) | ACEITA | É a regra da família toda (janela, contexto, laço). Mover o handle do contexto é inofensivo porque o desenhador guarda o interior (FATO: o texto congelado diz isso). A troca para comportamento definido pode vir depois sem quebrar ninguém |
| D-API-07 o desenhador torna corrente o próprio contexto | ACEITA | Com duas janelas, a pré-condição desenharia na errada em silêncio |
| D-API-08 quadro limpo por padrão, `std::nullopt` para não limpar | ACEITA | O `eglSwapBuffers` deixa o buffer indefinido sem `EGL_BUFFER_PRESERVED`; o padrão seguro, e a exceção de propósito |
| D-API-09 `reserve_pieces`; a regra de crescimento verdadeira escrita | ACEITA | `reserve_quads` nomeava o mecanismo que a D-W7D-18 trocou |
| D-API-10 a mensagem do driver vai ao registro (evento `draw2d_program_rejected`, campo `driver_log`) | ACEITA | R7 proíbe frase no erro, e reabrir `gltfx_err` reabriria um tipo congelado |

### Os IMPORTANTES e COSMÉTICOS de B0 que não viraram D-API

- **B0-I5 (a promessa do estado de GL na SAÍDA sem teste): ACEITO, entra em B5** (a célula que lê cada item da lista depois de `flush()`).
- **B0-I10 (tipo de valor por valor, descritor por `const &`): ACEITO.**
- **B0-I7: ACEITO, é a mesma linha da INFRA-CI de P0-I5.**
- **B0-K1 (`m_impl` contra a L-21, com 128 usos na árvore):**
  - o texto novo segue a lei (`impl`), e isso está aceito;
  - a árvore contradiz a L-21, o que é conflito de canon contra árvore (L-67 global). **Não é desta onda:** vai à INBOX como renomeação mecânica, e o main leva o conflito ao líder quando ele pedir o registro. Nenhum agente declara a regra velha, nem os 128 usos, como aceitáveis.
- **B0-K2, K3 e K4: ACEITOS, como registro.**
- **A seção 9 (`R2D-SHAPES`):** registrada; as quatro perguntas abertas (espessura em mundo ou em pixel, contorno dentro/fora/centrado, elipse ou círculo, e o tipo público de vértice) vão para a revisão dedicada de `R2D-SHAPES`, depois da `DEMO-1`.

---

## 3. O que muda no PLANO.md (as linhas da §4, sem tocar o arquivo congelado)

| Linha | Mudança |
|---|---|
| **P1** | `gfx_preset_table_test` ganha a célula da degradação E1: o par exato `{suggested_preset, k_gltfx_preset_manual}` |
| **P3** | Acrescenta ao "Nasce / muda": o diff do parecer P0 §2 com a emenda E1 no texto do `row_at`; as constantes E2 de `vsync`; o parâmetro de `gltfx_gfx_option_at` renomeado (P0-I4); a frase "eight ids" reescrita sem número (P0-K1). O `api-review` confere o commit de P3 contra o texto congelado mais E1 e E2 |
| **P4** | `gl_context_parity_test` ganha a célula E1: aplicar o par degradado dá `invalid_argument` e o rótulo continua o mesmo. As duas linhas [P4] do parecer entram byte a byte (P0 §7) |
| **B2a** | Ganha a célula de B0-C1: câmera em x = 16 777 217, peça em 16 777 218, pixel esperado **1,0**; o mutante "matriz em float antes de subtrair" tem de dar **2,0**. A célula antiga (-2^24) fica como controle de que o caso exato continua exato. A entrada de `quad_vertices` é `gltfx_transform` (double) |
| **B2d** | O relatório é a `struct` de campos nomeados de `frame_2d_report.hpp` (D-API-03); a falta de memória descarta, conta, e o erro sai por `finish_frame()` sem perder as contagens (D-API-02) |
| **B3b** | A falha do programa de sombreamento sai com `rejected_value` = `vertex_shader`/`fragment_shader`/`program_link`, e a mensagem do driver vai ao registro, como o evento `draw2d_program_rejected` com o campo `driver_log` (D-API-10). Nasce/muda acrescenta o uso do `core/log` (sem arquivo público novo) |
| **B4** | **SEIS** cabeçalhos, não quatro: `include/glintfx/core/quad.hpp` (NÃO `draw2d/quad_world.hpp`), `include/glintfx/draw2d/renderer_2d.hpp`, `renderer_2d_desc.hpp`, `frame_2d_desc.hpp`, `frame_2d_report.hpp`, `draw_layer.hpp`. O texto é o do parecer B0 §2, sem as linhas [B5] (B0 §7). `begin_batch(gltfx_transform)`, `last_frame_report()`, `reserve_pieces`, `gltfx_draw_layer layer = {}` e o `make_current` do próprio contexto |
| **B5** | Ganha a célula de B0-I5 (o estado de GL lido DEPOIS de `flush()`, item a item da lista do cabeçalho) e a de D-API-07 (duas janelas, dois contextos: cada desenhador desenha no seu, lido no pixel). As linhas [B5] do parecer entram byte a byte |
| **B7** | Acrescenta aos mutantes: `begin_batch` estreitando para float (B2a pega); `finish_frame` perdendo as contagens no erro (B2d pega); o desenhador sem `make_current` (a célula de duas janelas de B5 pega) |

**Critério novo no fechamento de P3/P4 e de B4/B5 (L-43, fixado agora):** o cabeçalho publicado comparado por máquina com o texto congelado do parecer, **mais** E1 e E2 aplicados. Diff vazio, ou divergência registrada por mim. É por essa comparação, e não pelo portão de citação (P0-I5 e B0-I7), que a ordem de entrada [P3]/[P4] e [B4]/[B5] fica garantida.

---

## 4. Destino dos itens que o revisor listou

| Item | Destino |
|---|---|
| O parâmetro `index` de `gltfx_gfx_option_at` colide com `X11/Xos.h` | **P3** (W7-D) |
| `vsync` sem nome público | **P3** (W7-D), E2 |
| `gpu_preference` sem nome público | **INBOX**, junto com `GL-GPU-PREFERENCE` (E2) |
| O portão de citação aceita um bloco com UMA citação (P0-I5 = B0-I7) | **INFRA-CI**, linha nova: "cada citação do bloco tem de existir" |
| O token de dado de um valor (P0-K2) | **INBOX pós-demo** |
| `m_impl` contra a L-21 (B0-K1) | **INBOX**, renomeação mecânica; o conflito de canon contra árvore fica registrado para o líder (L-67) |
| As quatro perguntas de `R2D-SHAPES` | Na linha `R2D-SHAPES` (W8, depois da `DEMO-1`), para a revisão dedicada dela |

---

## 5. Decisões da P1 (29/09, 23:5x), a pedido do implementador (L-34)

### D-P1-1. Onde nasce o tipo interno da fonte de energia

- **Pergunta:** "`choose_preset_automatically(gltfx_gpu_kind, <fonte de energia>)` é de P1, e o tipo da fonte de energia só nasceria em P2, no cabeçalho da porta. Onde ele nasce?"
- **Opções:**
  - (a) P1 cria `src/platform/port/power_source_port.hpp` só com o enum, e P2 acrescenta o concept (a recomendação dele);
  - (b) o enum em `auto_preset_rule.hpp`, e P2 o move;
  - (c) `int64` com constantes locais;
  - (d) **um cabeçalho PRÓPRIO do tipo de valor**, `src/platform/port/power_source.hpp` (só o enum, com valores numéricos EXPLÍCITOS 0/1/2 iguais aos do contrato de dado), criado em P1; o `power_source_port.hpp` de P2 o INCLUI e acrescenta o concept.
- **Fontes:** L-17 do projeto (um assunto por arquivo; o nome é o árbitro); L-19 (dependência numa direção só: a regra pura não deveria depender do cabeçalho de uma PORTA de sistema só para ter um tipo de valor); a memória "cópia em vez de fonte" (mover depois cria duas fontes durante uma fatia).
- **Escolha: (d).**
  - (a) mistura dois assuntos no mesmo arquivo (o valor e o contrato do adaptador), e a regra pura passaria a incluir o cabeçalho da porta.
  - (b) exige um MOVE em P2, com o risco das duas fontes no meio.
  - (c) é o mais simples e perde a checagem de tipo que o `switch` fechado da regra de 12 células precisa.
  - (d) dá um átomo com nome honesto (o tipo), usado por dois consumidores (a regra e a porta), sem move.
  - Em P3, um `static_assert` amarra cada valor do enum à constante pública `k_gltfx_power_source_*` (o contrato de dado).
- **Porta de mão única:** não (interno). **Custo de reverter:** barato.

### D-P1-2. A célula E1 (degradação fora da faixa) sai de P1 e vai para P3, e o átomo interno não degrada

- **Pergunta:** "A célula E1 precisa de `suggested_preset = 8` e de `k_gltfx_preset_manual`, que só ficam públicos em P3. Constantes internas temporárias em P1, ou a célula muda de fatia?"
- **Opções:**
  - (a) constantes internas em P1, trocadas em P3 com `static_assert` (a recomendação dele);
  - (b) a célula vai para P3;
  - (c) **(b) com a divisão de responsabilidade certa:** o átomo interno `gfx_preset_table` NÃO escolhe o que entregar ao consumidor fora da faixa, e responde a AUSÊNCIA honestamente (`std::optional<gltfx_gfx_option_entry>`, `noexcept`, sem alocação); a política pública E1 ("degrada para `{suggested_preset, k_gltfx_preset_manual}`") mora na função PÚBLICA `gltfx_gfx_preset_row_at` (P3), onde os nomes públicos existem.
- **Fontes:** a memória "cópia em vez de fonte" (a opção (a) cria a segunda cópia de um valor que tem dono, mesmo que por uma fatia); R4 (a degradação é contrato da função PÚBLICA, não do átomo); L-19 (a política pública na fachada, o fato no núcleo).
- **Escolha: (c).**
  - (a) cria exatamente a cópia temporária que já causou quatro defeitos neste projeto (09/09).
  - Com (c), P1 prova a ausência: `gfx_preset_table_test` confere que um índice fora da faixa e um preset fora do vocabulário dão `std::nullopt`.
  - P3 prova E1 (um caso novo junto do teste do registro ou do acessor), e P4 prova a recusa ao vivo.
- **Isto CORRIGE a linha P1 da §3 desta errata:** onde se lê "gfx_preset_table_test ganha a célula da degradação E1", leia "gfx_preset_table_test prova a AUSÊNCIA (`std::nullopt`) fora da faixa; a célula E1 do par exato é de P3".
- **Porta de mão única:** não (interno). **Custo de reverter:** barato.

### D-P1-3. `expand_preset`: CONTESTO a assinatura com `std::span` de saída

- **A dele:** `expand_preset(preset, explicit_entries, std::span<gltfx_gfx_option_entry> out) noexcept -> std::size_t`.
- **O problema:** o `span` de saída cria um caminho de falha que não existia: o chamador passa um `span` pequeno demais. A função passa então a ter de truncar em silêncio, recusar (com que envelope?) ou ter pré-condição. O degrau 1 da R3 é justamente "capacidade fixa, sem falha": a C1 do plano de 23/09 fixava `std::array<gltfx_gfx_option_entry, N>` MAIS contagem, com `N` = número de linhas do registro, amarrado por `static_assert`.
- **Escolha:** `expand_preset(std::int64_t preset, std::span<const gltfx_gfx_option_entry> explicit_entries) noexcept -> preset_expansion`, onde `preset_expansion` é um tipo de valor interno `{ std::array<gltfx_gfx_option_entry, k_gfx_option_row_count> entries; std::size_t count; }`, com o `static_assert` amarrando a capacidade ao número de linhas do registro.
  - Nenhum dimensionamento pelo chamador, nenhum ramo de "não coube": a capacidade é, por construção, o máximo possível (uma entrada por linha do registro).
  - O `span<const>` de ENTRADA fica como ele desenhou.
  - Controle do teste: a expansão de toda predefinição cabe; `count` confere com `row_count`; a entrada explícita da lista de abertura vence a expansão (regra de D-W6b-35).
- **Porta de mão única:** não (interno). **Custo de reverter:** barato.

---

## 6. D-P1-4 (30/09, 00:0x): a cauda que o `expand_preset` corta em silêncio

- **FATO relatado pelo implementador (P1, ainda fora do índice):** com uma lista explícita que repete uma opção, o `expand_preset` passa da capacidade e CORTA a cauda em silêncio.
- **FATO medido por mim:** a recusa de opção repetida na lista de abertura JÁ EXISTE no produto. `validate_gl_context_desc()` recusa o mesmo id duas vezes, nomeando a primeira duplicata (`src/platform/gl/gl_context_desc_validation.cpp:42-56`, "Reason 3"), e a fachada a chama em `open()` (`src/platform/gl/gl_context_facade.cpp:159`).
- **Pergunta:** "Basta a fachada de P3 recusar a repetição antes de expandir?"
- **Opções:**
  - (a) só a obrigação na P3 (a proposta dele): a fachada valida antes de expandir;
  - (b) (a) e mais o átomo tornado IMUNE: a saída do `expand_preset` é, por construção, no máximo uma entrada por id (a capacidade é o número de linhas do registro, então nunca estoura); com entrada repetida (inalcançável pela fachada), vale a PRIMEIRA ocorrência, a mesma regra de "a primeira duplicata é a que se nomeia" da validação, e o átomo o diz no cabeçalho.
- **Escolha: (b).**
  - Com (a) sozinha, a correção do átomo depende da ORDEM de chamadas de outra unidade. Um consumidor interno futuro (`set_option(preset)` ao vivo, ou `R2D`) que chame o átomo sem a validação volta a perder a cauda em silêncio.
  - (b) elimina o corte por construção, e a validação da fachada continua sendo a recusa em voz alta para o consumidor.
- **Provas:**
  - P1, `preset_expansion_test`:
    - a lista com id repetido dá uma entrada por id, com o valor da primeira ocorrência;
    - a contagem nunca passa de `k_gfx_option_row_count`;
    - mutante "corte na capacidade" (o comportamento de hoje): a entrada que vinha depois da repetição some e o caso reprova.
  - P3: `open()` com lista repetida E `preset` é RECUSADO nomeando a opção, e nada é expandido. Controle: a mesma lista sem repetição abre.
- **A linha P3 da errata §3 ganha:** "a fachada chama `validate_gl_context_desc()` ANTES de `expand_preset`, e o teste de P3 prova a recusa de lista repetida com `preset`".
- **Porta de mão única:** não (interno). **Custo de reverter:** barato.

---

## 7. D-P1-5 (30/09, 00:1x): os números de preset, vsync e razão antes de P3

- **Pergunta:** "As tabelas e a regra da P1 precisam dos NÚMEROS de `preset`, `vsync` e `auto_choice_reason`, e os `k_gltfx_*` públicos só nascem na P3. Onde eles moram entre a P1 e a P3?"
- **Opções:**
  - (i) constantes internas espalhadas em `gfx_preset_table.hpp` e `auto_preset_rule.hpp`, trocadas na P3 com `static_assert` de igualdade (o estado de hoje);
  - (ii) antecipar o bloco público de constantes para a P1;
  - (iii) literais soltos na tabela e na regra;
  - (iv) **UMA fonte interna única durante a P1/P2, APAGADA na P3.**
- **Fontes:** a memória "cópia em vez de fonte" (duas fontes do mesmo valor ao mesmo tempo é o defeito; uma fonte que muda de lugar num commit, não); a memória "teste que copia o valor da implementação" (o teste tem de afirmar o valor DECIDIDO, não o que o código diz); D-W6b-44 e `gfx_option_registry.hpp:79-98` (onde os números estão decididos hoje); L-17 (um assunto por arquivo).
- **Escolha: (iv).**
  1. Os números vão para UM cabeçalho interno só, `src/platform/gl/gfx_option_values.hpp` (os valores de `preset`, `vsync` e `auto_choice_reason`), incluído pela tabela e pela regra. Nada de constante espalhada nos dois átomos.
  2. **Os testes da P1 afirmam os números LITERAIS decididos** (`manual=0` ... `automatic=4`, `off=0/on=1/adaptive=2`, `none=0` ... `unknown_gpu=5`, da D-W6b-44 e do registro), nunca pelas mesmas constantes que o código usa. É isso que protege a troca na P3.
  3. **Na P3, o `gfx_option_values.hpp` é APAGADO**, e a tabela e a regra passam a usar os `k_gltfx_*` públicos. Não há `static_assert` de convivência, porque não há convivência: depois da P3 existe UMA fonte, a pública. Os testes literais da P1, que continuam rodando, provam que a troca não mudou número nenhum.
  4. O tipo `power_source` (D-P1-1) NÃO é cópia (é um tipo de valor, não uma constante): fica, e a P3 amarra os valores dele aos `k_gltfx_power_source_*` com `static_assert`.
- **Porquê não as outras:**
  - (i) espalha duas cópias parciais em dois arquivos e deixa a igualdade para um `static_assert` que só existe na P3;
  - (ii) publica uma parte do cabeçalho congelado fora da ordem, e a linha "Proved by: power_supply_rule_test ..." do bloco citaria um teste que só nasce na P2 (o portão de citação reprova);
  - (iii) é número mágico.
- **Mutante:** trocar um valor em `gfx_option_values.hpp` (ex.: `balanced = 3`) faz o teste literal da P1 reprovar; na P3, esquecer um uso interno do cabeçalho apagado quebra a compilação (é a prova de que ele sumiu).
- **Porta de mão única:** não (interno). **Custo de reverter:** barato.

---

## 8. D-P2-1 e D-P2-2 (30/09, 00:2x): a leitura de energia do Linux e a quebra da L-20

### D-P2-1. Os 6 sítios novos de família A: redesenho sem alocação, e NÃO a catraca crescendo

- **FATO (relato do implementador, P2 fora da árvore em `/var/tmp/p2-work`):** o `check_noexcept_alloc.py` (família A) reprova 6 sítios NOVOS de `std::string`/`std::vector`: `read_first_line`, `read_power_supply_entries` e `read_text` em `power_source_adapter.cpp`, e os campos `type`/`scope`/`status` em `power_supply_rule.hpp`. Não há família B (o `read()` tem `try/catch(...)` eficaz).
- **FATO (lido agora):**
  - a catraca nasceu para que o número NÃO CRESÇA (ESCOPO.md, Decisão 13; o cabeçalho do `tests/noexcept_alloc_family_a_baseline.txt`: "so IMPEDE QUE O NUMERO CRESCA");
  - a **Decisão 15** do líder (18/09, verbatim *"eu quero que conserte os 140"*) REVOGOU o "congelar": o destino da família A é CONSERTO;
  - a R3 põe o degrau 1 ("não alocar", capacidade fixa) acima do degrau 2 ("capturar e converter").
- **Pergunta:** "Seis sítios novos entram na linha de base, ou o leitor é redesenhado sem alocação?"
- **Opções:**
  - (i) seis linhas a mais na catraca (a recomendação dele);
  - (ii) redesenho sem `std::string`/`std::vector`: buffers de `char` de tamanho fixo e iteração das entradas por função chamada a cada uma.
- **Fontes:**
  - a ABI do kernel, `Documentation/ABI/testing/sysfs-class-power` (https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-class-power, já citada no adendo de 24/09): os valores lidos são palavras CURTAS de vocabulário fechado (`type`: Battery, UPS, Mains, USB, Wireless; `status`: Charging, Discharging, Not charging, Full, Unknown; `scope`: Unknown, System, Device; `online`/`present`: 0/1/2). Nenhum passa de uma dúzia de caracteres;
  - a POSIX `opendir`/`readdir` percorre `/sys/class/power_supply` sem contêiner;
  - a Decisão 15 e a R3 (acima).
- **Escolha: (ii).**
  - (i) é o caminho menos difícil (memória "o caminho menos difícil"): o código NOVO de uma fatia de produto contraria uma ordem vigente do líder (conserto, não congelamento) e faz crescer a lista que ele mandou encolher.
  - O precedente `drm_device_facts.hpp` está na linha de base como LEGADO a consertar, e não como licença.
  - E (ii) é barato, porque o vocabulário é curto e fechado.
- **Forma exigida:**
  1. um valor lido de arquivo sysfs cabe num `std::array<char, 32>` com comprimento. Um valor mais longo ou ilegível vira `unknown` na regra (falha fechada, nunca truncado em silêncio), com um caso de teste;
  2. as entradas do diretório são visitadas por `readdir` com uma função chamada por entrada (ou um laço que decide entrada a entrada), sem lista materializada;
  3. `power_supply_rule` recebe um tipo de valor sem contêiner (os campos como enum ou como `std::string_view` sobre o buffer do chamador, nunca `std::string` dono);
  4. o `read()` continua `noexcept` e, sem alocação, deixa de precisar do `try/catch(...)`;
  5. o gate de família A fica verde SEM linha nova na linha de base (critério de aceite).
- **Mutantes:**
  - um valor de 40 caracteres plantado na árvore falsa: a regra dá `unknown` (e não o prefixo truncado);
  - um `std::string` de volta num campo: o gate reprova.
- **Porta de mão única:** não (interno). **Custo de reverter:** barato.

### D-P2-2. A quebra da L-20 no teste do leitor: NÃO aceita como declarada; o redesenho da D-P2-1 refaz o vermelho de verdade

- **FATO:** o `tests/power_source_adapter_test.cpp` foi escrito DEPOIS do adaptador (quebra declarada pelo implementador), compensada com 5 mutantes. O de corte de espaços sobrevivia até o teste ser reforçado.
- **Pergunta:** "Aceita-se declarada, ou refaz-se o vermelho?"
- **Opções:** (a) aceitar declarada, com os mutantes; (b) o vermelho artificial (tirar o adaptador de uma cópia e ver o teste reprovar); (c) **como o adaptador vai ser REESCRITO pela D-P2-1, o teste da forma nova é escrito PRIMEIRO, contra a assinatura nova, e visto vermelho (compilação e depois asserção) antes da implementação.** A saída vermelha vai no relatório.
- **Escolha: (c).**
  - (b) prova só que o teste depende de um símbolo, não que ele mede o comportamento.
  - (a) aceitaria uma quebra que a própria fatia tem ocasião natural de desfazer.
  - Os 5 mutantes dele continuam valendo como cinto, reaplicados à forma nova.
  - O fato de o mutante do corte de espaços ter sobrevivido até o reforço É a evidência de que o teste escrito depois nasceu fraco.
- **Porta de mão única:** não. **Custo de reverter:** barato.

### Informativo aceito

- A tabela do Windows com 7 células (plano de 06/09 §5.2), no lugar das 5 do adendo: ACEITA, é a mais completa. O fechamento de P2 conta 7.
- A célula 4 da regra do Linux reforçada pelo mutante "scope aplicado às fontes": ACEITA, e o mutante entra no relatório.
- **Nada do Windows foi executado** (só `mingw -fsyntax-only`). O aceite de P2 exige o `power_status_rule_test` com as 7 células impressas no trabalho Windows do SERVIDOR, lido de `gh run view --json jobs` e do log. A VM Windows local NÃO é usada (fora de alcance nesta sessão).

---

## 9. D-C1b-6 (30/09, 01:0x, PRONTA ANTES da prova do implementador): espera limitada DEPOIS do KILL, em todos os modos

- **Contexto (FATO, do team-lead):** o CI do `da9086c`, no Arch estático, deu "D10b: the grandchild is dead: the grandchild survived" (1 de 59), com o caso de tempo do D10b passando. O implementador prova por mecanismo se é (a) o teste lendo cedo demais ou (b) o produto saindo sem esperar o efeito do KILL.
- **Leitura do CTO:** (a) e (b) são o MESMO fato visto de dois lados. O KILL é assíncrono: `kill()`/`cgroup.kill` voltam antes de o alvo morrer. Um teste que lê "morto" logo depois da volta do contido só lê cedo demais PORQUE o contido volta antes de a morte acontecer. Consertar só o teste (esperar no teste) esconderia uma propriedade que o produto deve prometer: **quando o contido volta, a árvore dele acabou, ou ele diz quantos sobraram.**
- **Pergunta que teria ido ao líder:** "Depois do KILL, o contido espera o alvo esvaziar antes de voltar? Por quanto tempo, e o que faz se sobrar alguém?"
- **Fontes:**
  - comunidade, o mesmo defeito e o mesmo conserto em projetos de execução de processo: "SIGKILL delivery and the process's teardown are asynchronous, so on a slow or loaded runner the /proc read can land while the process is still in S or R state" (https://github.com/gke-labs/kube-agents/issues/2128); o conserto "wait for the group to empty after SIGKILL" (https://github.com/gke-labs/kube-agents/pull/2135); "bound the wait that follows a group kill" (https://github.com/otto-nation/otto-workbench/issues/1456); "run-with-deadline accepts a process group still alive after a delivered SIGKILL" (https://github.com/evenfire-ai/evenfire/issues/811);
  - o systemd espera DEPOIS do SIGKILL final, com limite, e relata o que sobrou: "Processes still around after final SIGKILL. Entering failed mode." (https://github.com/systemd/systemd/issues/30863);
  - o kernel: o `cgroup.procs` pode aparecer vazio enquanto o cgroup ainda está `populated` (tarefas morrendo), e o que vale é o `populated` (https://lkml.iu.edu/hypermail/linux/kernel/2603.2/16110.html);
  - o nosso: `kill -0 -- -pgid` responde 0 enquanto houver ZUMBI no grupo (medido na revisão do 072936b).
- **Opções:**
  - (i) consertar só o teste (ele espera antes de ler);
  - (ii) o produto espera, com limite, o alvo esvaziar depois do KILL, e relata os sobreviventes;
  - (iii) (ii) sem limite.
- **Escolha: (ii).**
  1. **Modos `proprio` e `ninho`:** depois do `cgroup.kill`, espera ATÉ `max(1, G)` segundos pelo `populated 0` de TODOS os alvos (o mesmo `contido_wait_empty`, o mesmo relógio, a mesma guarda de rotação). NUNCA pelo `cgroup.procs` (o kernel mostra que ele esvazia antes).
  2. **Modo `herdado-pgid`:** depois do `kill -KILL -- -pgid`, espera ATÉ `max(1, G)` segundos até não restar membro VIVO do grupo. "Vivo" = presente em `/proc` com o campo 5 (pgrp) igual ao grupo e estado diferente de Z/X (a mesma leitura do `contido_alive`). É uma varredura de `/proc/[0-9]*/stat` por glob e `read` embutido, UMA por volta, SEM processo por item (L-11 §6). Um zumbi do grupo não conta: ele já morreu, só não foi colhido (e num container que não colhe, nunca será).
  3. **Sempre impresso:** a linha `contido:` ganha `sobreviventes_apos_kill=<n>` (0 no caso normal; ausência declarada E contada). Com n > 0: `AVISO: <n> processo(s) sobreviveram ao KILL por <s>s` em stderr. O rc do comando NÃO é mascarado; no modo `proprio`, a unidade ainda ativa cai na varredura final de sempre (o `systemctl stop` do de fora).
  - Por que não (i): esconde a promessa e deixa todo chamador futuro com a mesma corrida.
  - Por que não (iii): um processo em estado D (sono ininterruptível) pode não morrer nunca, e o contido não pode ficar preso nele (L-11 §6).
- **Provas:**
  - o D10b e as células "killed" do selftest passam a ler IMEDIATAMENTE depois da volta, sem espera no teste. Se o produto não esperar, elas voltam a ser intermitentes. O mutante "sem a espera pós-KILL" tem de reprovar de forma DETERMINÍSTICA, então o teste precisa de um alvo que demore a morrer depois do KILL. O implementador propõe o mecanismo (ex.: um neto em estado que atrase a entrega, ou medir o `sobreviventes_apos_kill` com um processo em D simulado); se não houver forma determinística, a estreia vermelha é declarada como medição de intermitência (N rodadas, contagem) e não vendida como determinística;
  - o zumbi do grupo não conta como sobrevivente (o `contido_fabricate_zombie` da da9086c serve);
  - `sobreviventes_apos_kill=0` aparece no caso normal (a linha sempre impressa).
- **Porta de mão única:** não (interno). **Custo de reverter:** barato.
- **Condição de validade:** esta decisão foi escrita ANTES da prova de mecanismo do implementador (L-43: critério antes do dado). Se a prova mostrar uma terceira causa (nem a entrega assíncrona, nem a leitura cedo), a decisão volta ao CTO.

---

## 10. B3a (30/09, 02:0x): o teste fora da lista e a forma sem `user` removida

- **D-B3a-1. `tests/gl_proc_address_assign_test.cpp` entra na linha B3a.** É arquivo EXISTENTE (o teste do helper) que ganha 3 casos do helper com `user`; sem ele o helper novo ficaria sem teste (L-20). A linha B3a da §4.B do PLANO passa a listar, em "Nasce / muda", `tests/gl_proc_address_assign_test.cpp`, e a coluna de prova passa a citar `gl_proc_address_assign_test` ao lado de `gl_loader_codegen_test`. Pela L-67, o plano muda antes de o código entrar; o `--compare` do `plan_scope_diff` é da INFRA-CI (D-W7D-19), e esta errata é o registro.
- **D-B3a-2. A remoção da forma SEM `user` do `load_gl_functions` gerado: ACEITA.** A D-W7D-15 (plano de 23/09) já permitia "a forma antiga continua para os testes que já a usam, ou é migrada no mesmo commit". FATO: o único consumidor da forma antiga era o `gl_loader_codegen_test`, que o mesmo patch migra (`git grep` sobre HEAD, sem outro chamador em `src/`, `include/`, `tools/`, `tests/` nem `.github/`), e emitir as duas formas duplicaria 344 funções geradas.
- **O que NÃO decido (L-67):** depois do patch, a sobrecarga ANTIGA do helper `try_assign_gl_function_pointer(out, gl_proc_address_fn, name)` e o tipo `gl_proc_address_fn` ficam sem consumidor de produção (só o próprio teste antigo). O implementador os manteve, e está certo em manter: declarar código morto é do líder (L-67 global). Vai à INBOX como pergunta, sem remoção.

---

## 11. B3b (30/09, 02:2x): a prova do programa embutido, o acessor e as escolhas declaradas

### D-B3b-1. A prova da B3b é um teste PURO com uma tabela de funções GL de mentira; a célula VIVA entra com a B4 pela API pública

- **FATO (do implementador):** os `tests/parity/*_parity_test.cpp` são só API pública, e no Windows o alvo liga `glintfx::glintfx` (`tests/CMakeLists.txt:2413`). O `embedded_program` é interno (sem `GLINTFX_API`, visibilidade oculta): a célula da §4.B B3b ("o programa compila e liga no contexto real", dentro do `draw2d_parity_test`) não se liga no Windows. **O defeito é do MEU plano:** pedi uma célula de paridade para uma peça que, nessa fatia, ainda não tem superfície pública.
- **Pergunta:** "Como se prova a B3b antes de a B4 publicar a fachada?"
- **Opções:**
  - (1) só o vermelho de compilação na B3b, e a célula viva com a B4;
  - (2) o alvo Windows do teste de paridade recompila `embedded_program.cpp` e `gl_functions.cpp` direto (o padrão "híbrido" que já existe em `loop_hidden_test`/`win32_facade_pin_test`);
  - (3) **um teste PURO, `tests/embedded_program_test.cpp`, que injeta uma `gl_function_table` de MENTIRA** (ponteiros para funções do teste que simulam compilação e ligação com sucesso e com falha), rodando nos cinco alvos sem contexto GL; e a célula VIVA (compila e liga no driver real) entra com a B4, pela `open()` pública, no `draw2d_parity_test`.
- **Escolha: (3).**
  - O desenho do implementador (o `embedded_program` recebe a tabela já carregada e nunca inclui `gl_context_impl.hpp`) é o que torna isso possível: a tabela É a costura.
  - (2) quebra a pureza do teste de paridade (ele deixaria de ser a visão do consumidor), e a L-19 põe o precedente híbrido como exceção documentada, não como molde.
  - (1) deixaria a B3b sem prova de comportamento por duas fatias.
- **O que o teste puro prova, célula por célula:**
  - o sucesso devolve o programa e apaga os objetos de sombreamento intermediários;
  - a falha de compilação do vértice dá `platform_failure` com `rejected_value` = `vertex_shader`; a do fragmento, `fragment_shader`; a da ligação, `program_link`;
  - em cada falha, TODOS os objetos criados até ali são apagados (contagem de `glCreate*` igual à de `glDelete*`: nenhum vazamento de objeto GL);
  - o evento `draw2d_program_rejected` chega ao registro com o campo `driver_log` = o texto do "driver" de mentira;
  - um texto do driver maior que a capacidade é truncado E dito (ver D-B3b-3).
- **A célula viva** vai para a linha B4/B5 da errata §3: a `open()` pública num contexto real (container Linux e trabalho Windows) devolve um desenhador aberto; o programa sabotado numa cópia dá o token certo.
- **Porta de mão única:** não. **Custo de reverter:** barato.

### D-B3b-2. O acessor do TAMANHO da superfície sai da B3b e entra na B3c, que é quem o consome

- **FATO:** o port (`gl_context_adapter_port.hpp`) não tem método de tamanho; o adaptador Wayland tem `egl_surface_pixel_size()` e o Win32 não tem equivalente. Compilar e ligar o programa não consome tamanho. O `glViewport` da B3c consome.
- **Escolha:** a B3b entrega só o acessor do RESOLVEDOR (função livre em `gl_context_impl.hpp`, na forma da B3a, `void *(void *user, const char *name)`). A B3c estende o port com o tamanho da superfície em pixel físico, com os dois adaptadores:
  - Wayland: o `egl_surface_pixel_size()` que já existe;
  - Win32: `GetClientRect` da janela do contexto;
  - e a linha B3c ganha, em "Nasce / muda", o port e os dois adaptadores.
- **Porquê:** a fatia que toca o port é a que precisa dele, e o Win32 decide a fonte do tamanho junto com o teste que a consome (L-20).
- **Porta de mão única:** não (interno). **Custo de reverter:** barato.

### D-B3b-3. As escolhas que o implementador declarou, julgadas

| Escolha dele | Julgamento |
|---|---|
| `driver_log` sem alocação (`std::array<char, N>` na pilha, truncamento declarado, nenhum sítio novo de família A) | **ACEITA, com um acréscimo:** o truncamento tem de ser VISÍVEL no evento. Um campo `driver_log_truncated` do tipo `boolean` (R9 tem o tipo, `core/log/value.hpp:52`), verdadeiro quando o texto do driver não coube. O teste puro prova os dois lados |
| evento: categoria `draw2d`, nome `draw2d_program_rejected`, campo `driver_log` (texto) | **ACEITA** (é o congelado na D-API-10) |
| severidade `warning` | **TROCADA para `err`** (o valor que `gltfx_log_severity` tem, `severity.hpp:96`; não existe `warning`). O programa da PRÓPRIA biblioteca recusado pelo driver é o motivo de a `open()` falhar: é erro, não aviso. O `gltfx_rslt` leva o erro ao chamador; o registro leva o detalhe |
| o `embedded_program` recebe a `gl_function_table` já carregada e nunca inclui `gl_context_impl.hpp` | **ACEITA**, e é o que viabiliza a D-B3b-1 |

---

## 12. B3c (30/09, 03:0x): o estado GL que o contrato não define, e os erros GL do consumidor

### D-B3c-1. O contrato de estado GL da B3c define TUDO de que o desenho depende (emenda E3 ao texto congelado do B0)

- **FATO:** o texto congelado de `renderer_2d.hpp` (parecer B0 §2) promete que o desenhador "sets everything it depends on". A lista de SAÍDA congelada cobre programa, VAO, buffers, textura, unidade 0, mistura (fator), profundidade, estêncil, recorte, descarte de face, máscara de cor, viewport e `GL_FRAMEBUFFER_SRGB`. O `gl_state_contract.cpp` da B3c (commit local `7cc51e1`) define exatamente isso, e NÃO define:
  - a EQUAÇÃO de mistura (`glBlendEquationSeparate`): um consumidor que usa mistura aditiva, `MAX` ou subtração deixa a equação trocada, e o nosso alfa pré-multiplicado sai errado;
  - o modo de polígono (`glPolygonMode`): o wireframe de depuração do consumidor desenharia os nossos quadriláteros em linhas;
  - `GL_RASTERIZER_DISCARD` ligado: nada seria desenhado;
  - `GL_COLOR_LOGIC_OP` ligado: a operação lógica SUBSTITUI a mistura;
  - `GL_SAMPLE_ALPHA_TO_COVERAGE` ligado: a cobertura muda com o alfa numa superfície com MSAA;
  - o framebuffer de DESENHO ligado: com um FBO do consumidor ligado, desenharíamos no FBO dele, e não na superfície que o contrato chama de "pixel direto".
- **Fontes:** a dor de estado vazado do NanoVG (nanovg#285, já no plano §2.1 R-B3); o contrato de descarga do SDL (`SDL_FlushRenderer`, idem).
- **Escolha:** a B3c define também, na ENTRADA:
  - a equação ADD para cor e alfa;
  - o polígono em FILL para as duas faces;
  - `GL_RASTERIZER_DISCARD`, `GL_COLOR_LOGIC_OP` e `GL_SAMPLE_ALPHA_TO_COVERAGE` desligados;
  - o framebuffer de desenho 0.
  - A lista de SAÍDA do cabeçalho congelado ganha os mesmos itens (EMENDA E3 ao texto do B0, que só acrescenta itens a uma lista em comentário e não mexe em API nem ABI). O `api-review` confere a E3 no commit da B4.
- **Provas:**
  - `gl_state_contract_test`: cada item novo é definido na entrada e deixado como declarado na saída; um mutante por item;
  - a célula viva de estado hostil (B5) planta também a equação `MAX`, o polígono `LINE`, o descarte do rasterizador, a operação lógica e um FBO do consumidor ligado, e o pixel continua certo.
- **Porta de mão única:** a lista de saída é contrato publicado (sim, na B4). **Custo de reverter:** baixo agora.

### D-B3c-2. Os erros GL: drenar os do consumidor ANTES, conferir TODOS depois, e mapear pelo código

- **FATO:** o `vertex_stream.cpp` lê um `glGetError()` depois da operação e trata QUALQUER código como falha própria: `out_of_memory` na criação e `platform_failure`/`vertex_upload` no envio. O `glGetError` devolve o PRIMEIRO erro da fila. Um erro deixado ANTES pelo consumidor seria atribuído a nós, e ainda seria apagado da fila dele.
- **Fonte:** o SDL faz exatamente o par "drenar antes, conferir todos depois" (`GL_ClearErrors` e `GL_CheckAllErrors` em `src/render/opengl/SDL_render_gl.c`, https://github.com/libsdl-org/SDL/blob/main/src/render/opengl/SDL_render_gl.c).
- **Pergunta (as escolhas de erro que ele pediu para julgar):** "Criação dá `out_of_memory` e envio dá `platform_failure`/`vertex_upload`: está certo?"
- **Escolha:**
  1. **Na ENTRADA** de `create_vertex_stream` e de `upload_batch`: drenar a fila, com limite (no máximo 16 leituras; o GL tem um número pequeno de bandeiras). Cada erro drenado vira um evento `draw2d_prior_gl_error` (severidade `warn`, campo `gl_error` inteiro sem sinal) no registro: nunca engolido calado, e nunca atribuído a nós. O SDL só drena; nós drenamos E dizemos.
  2. **Na SAÍDA:** ler TODOS os erros da operação, com limite, e mapear pelo código: `GL_OUT_OF_MEMORY` (0x0505) dá `out_of_memory`; qualquer outro dá `platform_failure`. Nos dois casos, o `rejected_value` é o token do passo (`vertex_array_create`, `vertex_upload`, `index_upload`) e o código GL vai em `with_os_error_code()`.
  3. **`glGen*` devolvendo 0 sem erro na fila** (contexto perdido ou não corrente): `platform_failure` com o token do passo, e não `out_of_memory` (o `glGen*` não aloca a memória do objeto; quem sinaliza falta de memória é o `glBufferData`, e isso vem pela fila).
- **Provas:** no teste de GL de mentira:
  - um erro "do consumidor" plantado antes da chamada é drenado, sai no evento e NÃO vira falha nossa;
  - `GL_OUT_OF_MEMORY` na saída dá `out_of_memory`;
  - `GL_INVALID_OPERATION` dá `platform_failure` com o código em `os_error_code`;
  - a drenagem para no limite quando o GL de mentira nunca esvazia (o relógio quebrado do GL).
  - Mutantes: sem a drenagem (o erro do consumidor vira falha nossa); tudo mapeado para `out_of_memory`; sem o evento.
- **Porta de mão única:** o nome do evento e do campo, sim (vocabulário de registro, R9). **Custo de reverter:** baixo.

### Aceitos sem mudança

- O tamanho da superfície no port, com os dois adaptadores (o Win32 por `GetClientRect`, compilado só no mingw, a prova no trabalho Windows do servidor).
- O CONSERTO da trava negativa `local_gl_adapter_missing_gpu`, que reprovava pelos movimentos defaulted e não pela falta do `gpu()`: é achado L-40 legítimo, e agora cada controle negativo falta exatamente um membro.

## 13. D-P2-CI (30/09, 04:0x): `power_supply_rule.cpp` no banco de compilação Windows

- **FATO** (run 36680416590, log /var/tmp/ci-w7d-109774414364.log:1786): o Windows Lint reprova com "Arquivo(s) de src/platform/wayland/ caiu(ram) no banco de compilacao Windows - impossivel por desenho: src/platform/wayland/power_supply_rule.cpp". O portão é `.github/workflows/ci.yml:3333`. A lista fechada `$waylandDominioNaoSo` (ci.yml:3276) nomeia os CINCO arquivos Wayland-DOMÍNIO-não-só, e `power_supply_rule.cpp` não está nela.
- **Pergunta:** onde mora a regra pura do Linux, que é vocabulário do sysfs mas não toca nenhum cabeçalho do SO?
- **Opções:**
  - (a) mover para um lugar neutro (`port/` ou `platform/gl/`);
  - (b) o teste dela só compila no Linux, e a prova sai do Windows;
  - (c) sexto nome na lista fechada do portão, no mesmo desenho dos cinco.
- **Fontes:**
  - o precedente interno, "Wayland-DOMAIN, not Wayland-ONLY": tests/CMakeLists.txt:1231-1276 (window_configure_sequence, frame_callback_sequence, drm_gpu_kind, egl_device_dedup, global_catalog, decididos em 03/09 e 06/09) e o texto do portão em ci.yml:3258-3275 (18/09, pedido nro 10);
  - o SDL3 guarda a leitura do power_supply em `src/power/linux/`, compilada só no Linux e sem teste de unidade multiplataforma. É a opção (b), mais fraca que a nossa L-04 ("provado em cada um").
- **Escolha: (c).**
  - O conhecimento ("Battery", "Not charging", scope "Device") é do domínio Linux, e mover para um diretório neutro mentiria sobre isso (a).
  - Tirar do Windows perde uma prova que o projeto já decidiu manter para os cinco irmãos (b).
  - A lista continua FECHADA por nome: qualquer OUTRO arquivo de wayland/ no banco Windows segue reprovando.
  - O gêmeo `src/platform/win32/power_status_rule.cpp` compilado no Linux não tem portão simétrico. Conferi por grep em ci.yml e preci.sh: nada a mudar lá.
- **O que muda:**
  - ci.yml:3276 ganha `'src/platform/wayland/power_supply_rule.cpp'`;
  - os textos "cinco"/"os outros quinze" do comentário e da razão passam a "seis" e ao número que a própria varredura imprimir. Não escreva número à mão: diga "os da lista acima";
  - o comentário do tests/CMakeLists.txt sobre os testes de energia cita o desenho "Wayland-DOMAIN, not Wayland-ONLY" e o ci.yml:3276.
- **Prova:**
  - o vermelho atual É a estreia do portão para este arquivo;
  - o próximo run tem de mostrar `power_supply_rule.cpp` em `no_banco` e o `fora_do_banco` sem ele;
  - antes do push, sabotagem local da cópia do bloco PowerShell com um SÉTIMO nome inventado de wayland/ no banco, que tem de reprovar (pwsh existe nesta máquina? se não, declarar o downgrade).
- **Porta de mão única:** não. **Custo de reverter:** uma linha.

## 14. Clang `-Wunused-const-variable` nos três testes da P1 (30/09, 04:0x)

- **FATO** (/var/tmp/ci-w7d-109774414581.log:1154-1196): o erro NÃO é só em gfx_preset_table_test.cpp:52-59. Pega também auto_preset_rule_test.cpp:46,50,51,52 e preset_expansion_test.cpp:50 (L-17: o gêmeo).
- Os três arquivos estão iguais desde a P1 (e0787bb). O vermelho nasceu na P1 e ninguém o viu, porque o preci não compila com Clang: a mesma família de "portão que nunca rodou no ambiente real".
- **Conserto:** APAGAR as constantes sem uso, arquivo por arquivo. Nunca `[[maybe_unused]]`, que guarda código morto.
- **Item novo na INBOX (onda INFRA-CI):** o preci não tem estágio de compilação com Clang, e o CI tem.

### §13, adendo (30/09, 04:2x): a pesquisa da L-34, feita depois, que CONFIRMA a escolha (c)

O team-lead cobrou a emenda do líder à L-34 ("pesquisar na web o que for mais completo e de desejo da comunidade. Depois nas bibliotecas semelhantes como elas fazem"). A pesquisa veio DEPOIS da primeira escolha. Registro isso aqui como falha minha de ordem.

- **Comunidade:** o padrão "functional core, imperative shell" (Google Testing Blog, 2025, https://testing.googleblog.com/2025/10/simplify-your-code-functional-core.html) põe a lógica em funções puras, testadas por unidade sem supor nada do ambiente, e deixa a casca de E/S para teste de integração. É o que a P2 já fez: power_supply_rule (núcleo) e power_source_adapter (casca).
- **SDL3** (https://github.com/libsdl-org/SDL/blob/main/src/power/linux/SDL_syspower.c):
  - o arquivo inteiro fica sob `#ifdef SDL_POWER_LINUX`;
  - leitura e interpretação estão MISTURADAS em `SDL_GetPowerInfo_Linux_sys_class_power_supply()`;
  - não há teste automático da interpretação.
  - É a opção (b), sem núcleo puro: a mais fraca.
- **GLFW** (https://github.com/glfw/glfw/blob/master/src/CMakeLists.txt):
  - cada backend compila sob `if (GLFW_BUILD_<X>)`;
  - a EXCEÇÃO deliberada é o conjunto `null_*`, compilado em toda plataforma sem guarda e listado por nome.
  - É o mesmo desenho da nossa lista fechada: exceção por nome, nunca por regra genérica.
- **raylib** (https://github.com/raysan5/raylib/tree/master/src/platforms): um `rcore_<plataforma>.c` por alvo, e não há teste automático do código de plataforma.
- **sokol** (https://github.com/floooh/sokol/tree/master/tests): testes funcionais sobre um backend de mentira, disparados por um script por sistema (test_linux.sh, test_win.cmd, test_macos.sh...). A prova roda em cada alvo.
- **Godot** (https://docs.godotengine.org/en/stable/engine_details/architecture/unit_testing.html): os testes de unidade rodam no Linux, no macOS e no Windows. O guia não fala de código de `platform/`.
- **Conclusão: (c) CONFIRMADA.**
  - A opção mais completa é o núcleo puro provado em todos os alvos (padrão da comunidade, sokol). Ela supera o SDL3 e o raylib, que não provam.
  - O jeito de conviver com o portão de "backend só no seu sistema" é a exceção NOMEADA, como o `null_*` do GLFW.
  - Mover o arquivo (a) não aparece em nenhuma das cinco bibliotecas como prática para lógica de domínio.
  - Tirar a prova do Windows (b) é o que SDL3 e raylib fazem, e é justamente o que nos deixaria sem prova.

## 15. D-CI2-LINT (30/09, entre 08:00:49 e 08:02:39, medido pelo mtime dos logs do clang-tidy e do roteiro da B3c; o "08:1x" anterior era estimado): Windows Lint sem o cabeçalho gerado

- **FATO** (run 36696277806, /var/tmp/ci-w7d2-109824909159.log:1837-1844 e 2355):
  - `src/draw2d/embedded_program.hpp:10:10: error: 'gl_functions.hpp' file not found`, mais 2 `misc-use-internal-linkage` em embedded_program.cpp:144 e :194;
  - o job "Windows - Lint" só CONFIGURA (ci.yml:3149-3154: "nunca precisa rodar cmake --build") e o gl_functions.hpp nasce no BUILD (src/render/CMakeLists.txt:287, `glintfx_gl_functions_generated`);
  - embedded_program.hpp é o PRIMEIRO arquivo rastreado que inclui um cabeçalho gerado no build (git grep no 39d939d).
- **FATO medido por mim:**
  - clang-tidy (misc-use-internal-linkage mais clang-diagnostic-*) no embedded_program.cpp do 39d939d SEM o diretório gerado: 1 not-found e 2 internal-linkage;
  - COM o diretório gerado: 0 e 0, rc 0;
  - os dois internal-linkage são CONSEQUÊNCIA do cabeçalho ausente.
- **Pergunta:** como o lint enxerga um cabeçalho que só existe depois do build?
- **Fontes:**
  - Chromium (https://chromium.googlesource.com/chromium/src/+/60.0.3082.3/docs/clang_tidy.md): "Build chrome normally" ANTES do clang-tidy;
  - CMake `CMAKE_CXX_CLANG_TIDY` (https://danielsieger.com/blog/2021/12/21/clang-tidy-cmake.html) roda o tidy DENTRO do build, com as dependências geradas satisfeitas;
  - o Meson (https://github.com/mesonbuild/meson/issues/7662) reprova o tidy em fonte que não compila no diretório de build.
  - Regra comum: o que o tidy analisa tem de estar gerado antes.
- **Opções:**
  - (a) o job Windows Lint constrói SÓ os alvos de geração antes do tidy;
  - (b) build completo antes do tidy (o que o preci Linux já faz, preci.sh:563);
  - (c) tirar o include do cabeçalho: não resolve, porque o .cpp inclui também;
  - (d) excluir o arquivo do lint Windows: enfraquece o portão.
- **Escolha: (a), com um ALVO AGREGADOR, nunca uma lista de nomes.**
  - Nasce `glintfx_generated_sources` (add_custom_target) que depende de TODO alvo de geração: hoje `glintfx_gl_functions_generated`, e no Unix `glintfx_wayland_xdg_shell_binding`.
  - O Windows Lint roda `cmake --build build-lint --target glintfx_generated_sources` entre o configure e o tidy, com a trava `$LASTEXITCODE` no próprio passo (G5).
  - Um gerador novo entra no agregador no mesmo commit em que nasce: comentário no próprio alvo.
  - (b) seria completo, mas custa um build MSVC inteiro num job cujo papel é analisar. (a) dá ao tidy exatamente o que o build daria para ele.
- **Piso (L-40):** depois do build de geração, o passo confere que `build-lint/generated/render/gl_functions.hpp` existe; se não existir, `exit 1`, com uma mensagem que nomeia o arquivo.
- **Prova:**
  - o vermelho do run 36696277806 É a estreia do defeito;
  - o próximo run tem de dar o Windows Lint verde, com embedded_program.cpp no_banco e analisado.
  - Sem pwsh local: downgrade declarado no commit.
- **Porta de mão única:** não.

## 16. D-CI2-SELFTEST (30/09, entre 08:00:49 e 08:02:39, medido pelo mtime dos logs do clang-tidy e do roteiro da B3c; o "08:1x" anterior era estimado): contido_dentro_selftest acima da régua de 40 s

- **FATO** (/var/tmp/ci-w7d2-109824909098.log:2847 e 2920): Ubuntu estático, 51,52 s. O ctest_aggregate.py regra 1 (HEAVY_LIMIT_S=40, tests/tools/ctest_aggregate.py:58) reprova "sem RESOURCE_LOCK glintfx_nested_build nem PROCESSORS - disputa os nucleos".
- **FATO (leitura de tools/contido_dentro.sh:496-784):** o tempo é de ESPERA, não de CPU. São prazos S=1..5 s e graças G=1..2 s (d10b 5/2, kill_tardio 5/2, kill_tardio_g0 5/0, n3 5/1, graca 1/2, relógio, locale e descritor), somados à espera pós-KILL max(1,G) da D-C1b-6. A régua usa o tempo de parede como proxy de "disputa os núcleos", e para este teste o proxy não distingue (memória "a régua que não distingue").
- **Fontes:**
  - ctest(1) (https://cmake.org/cmake/help/latest/manual/ctest.1.html): RESOURCE_LOCK e PROCESSORS descrevem RECURSO disputado, não duração;
  - scivision (https://www.scivision.dev/cmake-resource-lock-ctest/): RESOURCE_LOCK é para o recurso compartilhado de fato;
  - a prática comum para teste longo é partir em casos menores que rodam em paralelo e localizam melhor a falha.
- **Opções:**
  - (a) RESOURCE_LOCK glintfx_nested_build no selftest: satisfaz a régua MENTINDO sobre o recurso e serializa um teste ocioso contra os builds (piora o tempo total). É o caminho menos difícil, e está recusado;
  - (b) encurtar prazos e graças: foram dimensionados contra a intermitência do D10b sob carga. Encurtar reabre a intermitência. Recusado;
  - (c) PARTIR o selftest em grupos, cada um uma entrada do ctest;
  - (d) isentar por nome no agregador: o portão deixa de ver este teste se um dia ele ficar pesado de verdade.
- **Escolha: (c).**
  - `contido_dentro.sh --selftest <grupo>`, com os grupos definidos por MEDIÇÃO e o critério fixado ANTES (L-43): cada grupo com no máximo 20 s de parede nesta máquina, sem carga concorrente. É metade da régua, a folga para runner lento.
  - `--selftest` sem grupo continua rodando TODOS, para o uso manual e para o preci que o chame.
  - Cada grupo vira `add_test(NAME contido_dentro_selftest_<grupo> ...)` com LABELS selftest. Os inventários de paridade, as exceções e o registro do --blob recebem os nomes novos.
  - Critério de fechamento:
    - a soma dos casos por grupo é igual ao total atual (o selftest imprime a contagem por grupo e o total, sempre, mesmo zero);
    - nenhum caso some;
    - um grupo desconhecido dá rc 2 com mensagem.
  - Os prazos e as graças NÃO mudam.
- **Porta de mão única:** não. **Custo de reverter:** médio (o CMake e os inventários).
- **Nota:** o grupo é do domínio da INFRA-CI, mas o vermelho bloqueia o merge da W7-D (L-11 do projeto), por isso o conserto entra agora.

### §16, adendo (30/09, entre 08:02:39 e 08:16:58, medido pelo mtime do roteiro da B3c e da revisão do f6e2f4b; o "08:3x" anterior era estimado): o critério de grupo refeito pelos números do implementador

- **FATO** (impl-da14, medido): sem carga, 18 s (16 s antes das esperas da D-C1b-6); com 8 laços de CPU em 4 núcleos, 22,4 s com 78 casos (antes 12,6 s com 59); no CI, com 4 pesados em paralelo, 51,5 s. A inflação medida do CI sobre o local sem carga é 51,5/18 ≈ 2,86x.
- **O critério da §16 ("≤ 20 s sem carga") NÃO segura essa inflação:** 20 × 2,86 ≈ 57 s, acima dos 40. Corrijo ANTES de existir a medida por grupo (L-43): **cada grupo ≤ 12 s sem carga nesta máquina** (12 × 2,86 ≈ 34 s < 40) e, na prática, **pelo menos 3 grupos** para os 18 s de hoje (cerca de 6 s cada, uns 17 s no CI).
- **As opções dele:**
  - (a) `PROCESSORS 2`: o ctest(1) define PROCESSORS como o número de processadores que o teste USA, e este teste é espera e bash. Declarar 2 reserva núcleos que ele não usa só para ganhar prioridade no agendador; a partição resolve sem isso. Recusado;
  - (b) G=2 → 1 nos casos do KILL atrasado: o KILL chega em 0,6 s e a graça de 1 s deixaria 0,4 s de folga sob a mesma contenção que infla 2,86x, que é exatamente a família do D10b. Recusado.
- **O resto da §16 fica:** a partição, a contagem por grupo e o total sempre impressos, a soma igual ao total de hoje, grupo desconhecido com rc 2, e prazos e graças intocados.

## 17. E4 (30/09, cerca de 10:08 pelo `date`; o "11:1x" anterior era estimado, corrigido pela L-03): os tokens da D-B3c-2 entram no texto congelado do renderer_2d.hpp (só comentário)

- **FATO** (api-review, commit ba15813, src/draw2d/vertex_stream.hpp:42-57 e vertex_stream.cpp:162-236):
  - a B3c emite `vertex_array_create`, `vertex_upload` e `index_upload` como rejected_value;
  - o OOM da PLACA sai como out_of_memory, com o código GL em os_error_code();
  - o texto congelado do B0 §2 não lista `vertex_array_create` (em open()) nem `index_upload`, nem o OOM da placa (em finish_frame()), e cita um "draw" que talvez nem exista.
- **Regra:** R7 de docs/api-conventions.md. O token de erro é vocabulário público, e um token emitido sem estar escrito é contrato não escrito.
- **Opções:**
  - (a) emenda de comentário E4, que acrescenta os tokens e a frase do OOM da placa;
  - (b) mudar o código para caber no texto congelado, o que apaga a informação do passo e do código GL. Recusada.
- **Fonte:** o Vulkan separa OOM do host e OOM do dispositivo (VK_ERROR_OUT_OF_HOST_MEMORY e VK_ERROR_OUT_OF_DEVICE_MEMORY, https://registry.khronos.org/vulkan/specs/latest/man/html/VkResult.html), porque quem chama reage de forma diferente. Aqui a distinção vai SEM enum novo: o os_error_code é 0 para memória do processo e o código GL (0x0505) para a placa. Assim não há ABI nova.
- **Escolha: (a), APROVADA,** com o texto proposto pelo api-review:
  - open(): `vertex_array_create` em platform_failure e em out_of_memory;
  - finish_frame(): `index_upload`, mais "out_of_memory also when the graphics card itself ran out of memory, with the GL error code in os_error_code()", e o código GL em os_error_code() nos dois;
  - "draw" sai se a fachada não o emitir. Quem decide é o que a B4 de fato emite, conferido por grep no blob.
- **Prova:** o api-review confere a E4 junto com a E3 no diff da B4. Cada token do cabeçalho é emitido por algum caminho testado, e cada token emitido está no cabeçalho, nos dois sentidos.
- **Porta de mão única:** é texto de contrato, mas acrescenta sem remover (exceto o "draw", se não existir). Custo de reverter: baixo antes do merge.

## 18. Decisões da B4 (30/09, 10:12:48 pelo `date`), a pedido do impl-da14 (L-34)

**(a) Tokens de erro da fachada.**
- FATO (impl): o draw_batch não lê erros GL hoje, e o load_gl_functions devolve not_found com o nome da função.
- Escolha, que estende a E4 (§17):
  - a fachada LÊ os erros GL depois das chamadas de desenho, com o limite de 16 e o mesmo mapeamento da D-B3c-2: OOM vira out_of_memory com o código GL em os_error_code(); o resto vira platform_failure com rejected_value "draw" e o código GL. Com isso o "draw" do texto congelado passa a ser EMITIDO e fica no cabeçalho;
  - o not_found do carregador vira `unsupported`, mantendo o nome da função como rejected_value, como o texto congelado de open() já diz.
- Prova: uma célula com o GL de mentira pondo erro depois do glDrawElements, nos dois mapeamentos. O api-review confere os tokens nos dois sentidos.

**(b) Como o draw2d alcança o interior do contexto.**
- FATO (impl): o gl_context_impl.hpp inclui o adaptador selecionado, e no Win32 isso chega ao <windows.h>. A regra da W7-D é "draw2d nunca inclui cabeçalho do sistema" (docs/plano-w7d.md §4.4 B3; o portão só vem na INFRA-CI, D-W7D-19, e até lá quem confere é a revisão B7 por grep).
- Fontes: o SDL3 expõe `SDL_GL_GetProcAddress(const char*)` como função livre (https://wiki.libsdl.org/SDL3/SDL_GL_GetProcAddress), e o GLFW faz o mesmo com `glfwGetProcAddress`. O acesso ao GL é por função livre, sem expor o tipo da plataforma. Aqui leva o contexto por parâmetro, porque a lib tem mais de um contexto.
- Escolha: `src/platform/gl/gl_context_access.hpp`, com duas funções livres noexcept sobre `const/gltfx_gl_context&`: o resolvedor no formato do carregador (o `void *user` e a função) e o tamanho da superfície em pixels ({0,0} sem contexto). Elas são definidas em gl_context_facade.cpp, reusando os inline já existentes (gl_context_resolve_proc e gl_context_surface_pixel_size), sem segunda cópia.
- Condições:
  - o cabeçalho inclui SÓ o cabeçalho público do contexto e a stdlib;
  - o comentário de src/draw2d/CMakeLists.txt, que diz "never includes src/platform/", é corrigido para a lista FECHADA "de src/platform/ só gl_context_access.hpp";
  - a B7 prova por grep, com controle plantado, que nenhum arquivo de src/draw2d inclui outro cabeçalho de src/platform/ e que nenhum chega a um cabeçalho do SO: compilar a fachada com `-H` e listar os cabeçalhos que ela alcança.

**(c) `triangle_batch::reserve(std::size_t pieces) noexcept -> bool`.**
- Fonte, para comparar: o sokol_gl tem capacidade FIXA no setup (`sgl_desc_t.max_vertices` e `max_commands`, erro `vertices_full` e `commands_full`, https://github.com/floooh/sokol/blob/master/util/sokol_gl.h). O B0 congelou reserve_pieces com semântica de crescer, então a decisão aqui é só a forma interna.
- Escolha: aprovado como proposto, pela mesma regra de crescimento, nos três blocos (4 vértices, 6 índices e 1 corrida por peça).
- Condições:
  - guarda de estouro em `pieces*4` e `pieces*6`: devolve false sem tocar em nada;
  - crescer só alguns blocos numa falha é aceitável, porque muda capacidade e nunca conteúdo, e a célula confere o conteúdo intacto;
  - neste commit entra a célula do `grown = needed` (pedido de mais que o DOBRO numa chamada), que eu tinha deixado anotada na revisão da B2c, mais a célula do estouro.

**(d) Peças pendentes até o flush.**
- Escolha: EXTRAIR. `src/draw2d/pod_buffer.hpp` passa a ter o `pod_buffer<T>` e o `ensure_room`, com o static_assert de trivialmente copiável.
- É a 4ª ocorrência (vértices, índices, corridas, peças), e a L-33 manda extrair na 3ª. Uma 2ª cópia local seria WET contra a lei.
- O `piece_list` usa esse cabeçalho e o mesmo alocador realloc.
- Condições:
  - o teste de falta de memória em cada ponto de crescimento, como no triangle_batch;
  - os testes do triangle_batch continuam verdes SEM mudança nos arquivos de teste. A extração não muda comportamento, e o blob do teste é conferido idêntico.

## 19. Leitura adversarial da B4 na árvore, não commitada (30/09, 11:09:15 pelo `date`)

Base: a cópia da árvore em /var/tmp/cto-b4/o, com os md5 congelados em /var/tmp/cto-b4/congelado.md5.

**D-B4-1. A peça que não chegou à placa quebra a regra pública dos TRÊS destinos.**
- FATO: include/glintfx/draw2d/frame_2d_report.hpp:19-22 diz "Every piece submitted while a frame is open ends in exactly one of three places: pieces_submitted == pieces_drawn + pieces_refused + pieces_dropped_out_of_memory".
- FATO: `frame_tally::piece_not_drawn()` (novo) soma só em pieces_submitted, e o próprio teste renderer_2d_impl_test.cpp:549-551 confere "submitted, and no other end": 1 enviada, 0 desenhadas.
- Opções:
  - (i) um contador novo `pieces_not_drawn`, acrescentado NO FIM (a regra de crescimento do próprio cabeçalho), e a regra passa a QUATRO destinos;
  - (ii) somar em pieces_dropped_out_of_memory: mente quando a causa é o contexto que não ficou corrente;
  - (iii) não somar em pieces_submitted: esconde a peça.
- Escolha: (i).
  - O cabeçalho nasce nesta B4, então é o momento mais barato de acertar a porta de mão única.
  - O texto do campo diz o que é: "Pieces accepted while the frame was open that never reached the graphics card, because the context could not be made current or the card refused the data; the error of finish_frame() says why."
  - O api-review confere o texto e o layout.
  - Prova: as células de contexto falho e de erro no desenho e no envio passam a conferir a SOMA dos quatro destinos, não só submitted.

**D-B4-2. A ordem de pintura não pode depender de memória.**
- FATO: renderer_2d_impl.cpp:192-195 descarta o resultado (`(void)pieces.sort()`). Se o sort não conseguir guardar as chaves, a ordem por camada é abandonada EM SILÊNCIO, e o comentário dessas linhas ficou inacabado ("...said by the log of the sort? no: ...").
- FATO: o piece_list.reserve não reserva as chaves. Então o primeiro quadro com mais de uma camada aloca no flush, mesmo com a dica reserve_pieces.
- Fonte: `std::sort` ordena NO LUGAR sem alocar; `std::stable_sort` tenta alocar um buffer temporário (https://en.cppreference.com/w/cpp/algorithm/stable_sort). É a razão da D-W7D-10. A chave (camada, submissão) já está DENTRO de cada `pending_piece` e é única por construção.
- Escolha: ordenar o próprio `piece_store` no lugar pela chave (camada, submissão), com o `draw_key_before` do draw_order.
  - O sort não aloca e não pode falhar, e passa a devolver void.
  - O `key_store` e o `sorted` saem, e o `painted(i)` vira acesso direto.
  - O comentário inacabado sai.
- Prova: as células de ordem continuam verdes, e uma célula nova mostra que um alocador que recusa tudo DEPOIS do reserve não muda a ordem de um quadro com 3 camadas.

**Os 5 pontos do implementador:**
1. `gl_context_access.hpp` sobre o interior (`void*`), e não sobre `gltfx_gl_context&`: ACEITO. O texto congelado de open() diz "Moving the context handle while the renderer is alive is harmless: the renderer holds what the handle points to". A minha §18(b) dizia "sobre gltfx_gl_context&", e está CORRIGIDA por esta: vale o interior.
2. `srgb_framebuffer` lido uma vez no open(): ACEITO. A opção é `open_only` (src/platform/gl/gfx_option_registry.hpp:76-77) e não muda depois de aberto o contexto.
3. O open() real sem teste puro: aceito, com DUAS condições.
   - (a) O mapeamento not_found → unsupported, mantendo o nome, sai para uma função pura com célula própria.
   - (b) A B5 ganha a célula viva "contexto movido → invalid_argument 'context'", porque um gltfx_gl_context fechado só existe depois de um aberto, e isso exige o compositor.
4. not_found → unsupported com o nome: ACEITO. É o texto congelado de open(), e a prova é o (a) do ponto 3.
5. O draw2d_parity_test entra na B4, e não fica todo para a B5.
   - A L-20 e o plano (§4.4 B4: "Os testes de B5 escritos e compilados contra o cabeçalho antes da fachada existir") valem.
   - Escrever o teste só com a API pública, compilar e VER o vermelho de ligação sem o renderer_2d_facade.cpp, com o log guardado. Depois ele entra no MESMO commit da fachada, registrado, com os portões de paridade atualizados.
   - A B5 passa a ser a PROVA: o verde lido no container e no job Windows (gh run view), a verificação visual do qa-engineer e as contagens.

**Mutantes MEUS sobre a árvore** (13 tentados; 11 válidos; os dois inválidos foram refeitos):
- Mortos: sem o sort (:319); lista sempre de uma camada (:319); clear sem encode (:459); uniform de encode invertido (:488); sem contar o flush (:334).
- SOBREVIVEM, e são lacunas de teste (L-20). Cada uma precisa de célula:
  - (1) a PRIMEIRA falha do quadro é a devolvida: guardar a última passa;
  - (2) begin_batch() sem transformação volta ao pixel direto no meio do quadro: não zerar passa;
  - (3) o estado GL deixado depois do flush e do fim do quadro (a lista fechada do B0-I5): sem o leave passa;
  - (4) a precedência "erro do contexto ou da placa antes do OOM": invertida passa;
  - (5) a reserva padrão de 1024 quando reserve_pieces = 0: sem ela passa;
  - (6) a peça cujo desenho falhou NÃO é contada como desenhada: contá-la passa (vem junto da D-B4-1);
  - (7) o flush() refaz o contexto corrente (dois renderizadores em dois contextos nunca desenham no errado): sem refazer passa.
- Equivalente de custo: o clear que não volta o `one_layer` para true (só faz ordenar à toa).

### §19, adendo D-B4-1 (30/09, 11:11:48 pelo `date`): o nome, a posição e o texto, pelo api-review

- As três recomendações do api-review (/var/tmp/cto-w7d/conferencia-b4.md §8, md5 194558c3) estão ACEITAS e SUBSTITUEM o nome e a posição da §19.
- **Nome:** `pieces_dropped_graphics_failure`, e não `pieces_not_drawn`. Ao lado de pieces_drawn, "not_drawn" faria ler "submitted = drawn + not_drawn", o que é falso, porque recusadas e descartadas também não foram desenhadas. O nome segue a família `pieces_dropped_<causa>` que a struct já tem (L-28).
- **Posição:** logo depois de pieces_dropped_out_of_memory, junto dos outros destinos. A struct nasce nesta B4, e a regra "appended at the end" protege layout já publicado.
- **Texto:**
  - o OOM da PLACA (0x0505 no envio ou no desenho) cai no campo novo, e o erro do fim do quadro é out_of_memory com o código GL;
  - pieces_dropped_out_of_memory ganha "memory of its own", ou seja, a memória da própria lib;
  - sem isso, o consumidor veria o erro out_of_memory com o contador de memória em zero.
- **Arquivo pronto:** /var/tmp/cto-w7d/api-review/b4check/d_b4_1/frame_2d_report.hpp (md5 06a3e039). Compila com -Werror, e o portão de colisão dá 0.
- **Célula a mais:** o envio com 0x0505 conta em pieces_dropped_graphics_failure e deixa pieces_dropped_out_of_memory em zero. Somada às células da §19, a soma dos quatro destinos é conferida.

## 20. D-B4-3 (30/09, 11:57:32 pelo `date`): o noexcept_alloc acusa `.reserve` de um tipo NOSSO que é noexcept

- **FATO** (team-lead, lint da B4): família B, "renderer_2d_impl::create() é noexcept e aloca sem try eficaz (direto: renderer_2d_impl.cpp:71:reserve)".
- **FATO** (tests/tools/check_noexcept_alloc.py:282-285 e :693): a família B casa POR NOME qualquer `.reserve(`, sem olhar o tipo do receptor. O próprio arquivo diz que "não é inferência de tipo real". Aqui o receptor é `impl->batch` (`draw2d::triangle_batch`) e `impl->pieces` (`draw2d::piece_list`), e os dois `reserve` são declarados `noexcept` e não usam a STL (pod_buffer.hpp).
- **Precedente no MESMO portão:** check_noexcept_alloc.py:930-962 já resolve o receptor pelo tipo DECLARADO no caso do `.substr` de `std::string_view` (sv_names contra str_names), e o caso ambíguo fica acusado ("B-ambiguo").
- **Fonte:** o `bugprone-exception-escape` do clang-tidy decide pelo tipo e pela especificação `noexcept` DECLARADA de quem é chamado (https://clang.llvm.org/extra/clang-tidy/checks/bugprone/exception-escape.html), não pelo nome.
- **Opções:**
  - (a) renomear os `reserve` nossos para fugir do nome: é o caminho menos difícil, dobra o código ao defeito do portão e volta no próximo `insert` ou `append` nosso;
  - (b) resolver o receptor pelo tipo declarado, no molde do sv_names;
  - (c) linha na catraca ou exceção por sítio: proibido pela Decisão 13 do líder (catraca não cresce por conveniência).
- **Escolha: (b).**
  - Um receptor cujo nome é DECLARADO, na árvore varrida, SÓ com tipos do projeto (classe ou struct definida em src/ ou include/) cujo método de mesmo nome é declarado `noexcept` sai da família B.
  - Receptor declarado também com um tipo da STL, ou sem declaração achada, CONTINUA acusado. É o lado conservador, como o "B-ambiguo".
- **Prova (L-36/L-40), com fixtures novas no --selftest do portão:**
  1. `std::vector<int> v; v.reserve(n)` dentro de noexcept continua ACUSADO;
  2. um tipo do projeto com `bool reserve(std::size_t) noexcept` NÃO é acusado;
  3. um nome declarado com os DOIS tipos continua acusado;
  4. um tipo do projeto cujo `reserve` NÃO é noexcept continua acusado.
  A contagem das 4 aparece na saída. A árvore real passa a dar verde na B4 SEM linha nova em nenhuma baseline.
- **Porta de mão única:** não. **Custo de reverter:** baixo.
- **Alternativa se a prova não fechar:** se o implementador mostrar, por MECANISMO, que a resolução por tipo não cabe no desenho do portão (por nome, sem AST), a decisão volta para mim antes de qualquer rename.

## 21. B5: as 6 decisões pedidas, a B4-K5 e os commits intermediários da B4 (30/09, 12:55:00 pelo `date`)

Base: /var/tmp/cto-w7d/b5-desenho.md (md5 47a9d5e1).

**1. O fixture Linux: F1 agora, e a F2 como pergunta de onda.**
- FATO: o Containerfile registra CINCO ocorrências da "lista de fontes mantida à mão" (tests/container/Containerfile:145-200). Desde a quinta, o `check_container_fixture_link.py --exec` roda as linhas g++ reais antes do docker build, e o preci tem um estágio dele.
- A F2 (lib construída pelo CMake dentro da imagem e fixture ligado na libglintfx.so) é o desenho mais completo. Mas, adotada só para ESTE fixture, deixaria dois padrões convivendo no mesmo arquivo, e o problema da lista continuaria nos outros 22.
- Fontes (L-34):
  - o guia oficial do Docker para C++ (https://docs.docker.com/guides/cpp/multistage/) e o blog da equipe de C++ da Microsoft (https://devblogs.microsoft.com/cppblog/using-multi-stage-containers-for-c-development/) constroem no estágio construtor COM o sistema de build do projeto (CMake), e não com linhas de compilador à mão;
  - a F2 é o desejo da comunidade, e é por isso que ela vai para a INBOX como o destino, e não descartada.
- Porta de mão única: não (é fixture de teste). Custo de reverter F1 para F2 depois: médio (a migração dos 23).
- A F2 para TODOS os fixtures é uma migração de infraestrutura. Vai para a INBOX como item da INFRA-CI: "fixtures de container ligam na lib construída pelo CMake, matando a lista à mão". O team-lead anota.
- A B5 usa a F1, com três condições:
  - (a) a linha nova é gerada por script a partir da lista do gl_context_parity_test, mais os `src/draw2d/*.cpp` enumerados por `git ls-files`, e não escrita à mão;
  - (b) o carregador gerado no container sai do MESMO `gl_registry_codegen` sobre o MESMO xml, e o `check_container_fixture_includes.py` cobre o cabeçalho gerado;
  - (c) o `--exec` do portão de ligação roda sobre a linha nova ANTES do docker build, com "falharam: 0" lido.

**2. As tolerâncias:** aceitas.
- Peça opaca com valor EXATO.
- ±2 em 128 (sRGB desligado: branco codificado a 1,0 vezes alfa 0,5 = 127,5) e em 188 (sRGB ligado: 0,5 linear codificado dá 187,5).
- O valor cru sempre impresso como MEASURED.
- A célula do vermelho alfa 0,5 exige verde e azul em 0 EXATO: é o que prova a ausência da franja escura.

**3. C3 com dois contextos em sequência:** aceito, com um aperto.
- A ausência de suporte a `srgb_framebuffer` só é aceita se estiver DECLARADA por plataforma em tests/measured_exceptions.txt (item e motivo) e for impressa e contada.
- Ausência não declarada REPROVA. No llvmpipe e no Mesa do Windows o suporte é esperado; um "sem suporte" silencioso ali seria a célula nunca provando nada.

**4. C4: INCLUA o FBO plantado.**
- A E3 promete "o framebuffer de DESENHO 0", e só um FBO não zero plantado antes prova que a lib o troca.
- Plante tudo: os 5 estados do texto congelado, os da E3 (equação de mistura, polygon mode LINE, rasterizer discard, logic op, alpha-to-coverage e o FBO de desenho), mais a textura 2D e o programa.
- A releitura depois do flush() e do finish_frame() cobre a lista fechada inteira, incluindo os 6 itens da E3 (o cosmético do api-review na B4).

**5. Os mutantes por `docker cp` num container vivo:** aceito.
- Condições:
  - o binário é compilado com a MESMA linha g++ (as mesmas flags) do estágio do Containerfile, copiada por script;
  - o container sai da imagem construída, SEM montagem do host, com o check_isolation.sh verde ANTES do exec;
  - `--pids-limit` e `--pull=never`;
  - uma execução pesada por vez (L-11), com a liberação do docker pelo team-lead.
- O Windows fica no job do CI, lido por `gh run view` (L-49). A VM do laboratório só com liberação do team-lead.

**6. As 4 linhas "Proved by":** aceito como proposto. Commit próprio no FIM, depois de as células ficarem verdes nos dois sistemas, com o texto byte a byte o congelado e o api-review conferindo.

**B4-K5 (o assert de handle movido):** ENTRA NA B5, num commit próprio pequeno.
- Motivo: é o GÊMEO da família (L-17). display_facade.cpp:127 e window_facade.cpp:215 e seguintes já têm `assert(m_impl != nullptr && "... called on a moved-from ...")`.
- Forma: a mesma, em todo encaminhamento do gltfx_renderer_2d, exceto is_open, o destrutor e os moves.
- A família não tem teste de morte, então a prova é a B7: um grep que conta os encaminhamentos contra os asserts, lista fechada e contagem impressa.

**Os commits intermediários da B4 (pergunta do team-lead):** NÃO exijo a configuração e o build completos por commit.
- Motivo: medi os TUs que cada commit toca, compilando e rodando os testes deles no blob de cada commit:
  - 97e0963: frame_report_tally 2/2;
  - 0c7b5ea: piece_list 10/10, srgb 3/3 e gl_load_refusal 2/2;
  - 7515ccf: embedded_program 14/14 e vertex_stream 14/14.
- Risco residual, declarado: um portão de biblioteca inteira (por exemplo o exports_win, que compara o declarado com o exportado) pode reprovar num intermediário, porque o renderer_2d.hpp declara GLINTFX_API antes de a fachada existir.
- O projeto não tem regra de bissecção por portão, e o CI roda o HEAD. Três builds pesados em série custariam mais que esse risco.

## 22. D-B4-4: o noexcept_alloc ficou 40 vezes mais lento com a D-B4-3 (30/09, 13:09:24 pelo `date`)

- **FATO** (team-lead, CI 36740173106): o noexcept_alloc_test passou de 1,47 s para 88,8 s no Arch e de 0,64 s para 71,4 s no Ubuntu, e reprova a régua de 40 s do ctest_aggregate em 8 jobs. Todos os testes passaram.
- **FATO medido por mim** (cProfile na árvore do 13eba90, contido, /var/tmp/cto-perf/prof.txt):
  - o portão de hoje leva 21,0 s aqui; o de antes da D-B4-3 (05fc6c5) leva 0,51 s na mesma árvore, 41 vezes menos;
  - dos 21 s, 19,4 s estão em `receiver_is_project_noexcept`, e 18,5 s em `declared_types_of`: 100 chamadas, cada uma varrendo os 339 arquivos com uma regex por nome (42 699 buscas);
  - o analyze_tree roda 7 vezes por execução (a árvore mais as calibrações), e cada rodada refaz tudo;
  - numa rodada são 94 chamadas para só 34 nomes distintos.
  O custo é (rodadas × sítios B × arquivos), refeito do zero a cada nome. A INFERÊNCIA do team-lead estava certa, agora medida.
- **Opções:**
  - (a) ÍNDICE: em cada analyze_tree, uma passada por arquivo que registra as declarações de TODOS os nomes-receptores de uma vez, com uma regex só por arquivo sobre a alternância dos nomes. Os tipos DESCONHECIDOS (auto, decltype, structured binding, range-for, init-capture) entram no mesmo índice. Mais o cache de `class_method_is_noexcept` por (tipo curto, método);
  - (b) RESOURCE_LOCK de pesado: esconde uma regressão de 40 vezes e mente sobre o recurso (a mesma razão da §16). Recusada;
  - (c) procurar a declaração só no arquivo do sítio e nos que ele inclui: acha MENOS declarações, e menos declarações absolve mais. É o lado do falso negativo que a §20 fechou. Recusada.
- **Fonte:** a técnica é a do índice invertido. As próprias docs do `re` do Python lembram que cada `search` é uma varredura do texto: https://docs.python.org/3/library/re.html (search). Também o `functools.cache` para memoizar a pergunta por classe: https://docs.python.org/3/library/functools.html#functools.cache.
- **Escolha: (a).**
- **Critério, fixado AGORA, antes da medida do conserto (L-43):**
  1. VEREDITO IDÊNTICO:
     - na árvore real, o conjunto allocs_B e o conjunto inherited do conserto são IGUAIS aos do 3413e66, medidos por um diferencial de uma vez, com o log guardado;
     - o --selftest dá os mesmos 20 controles OK;
     - a minha sonda2 dá 11/11 (/var/tmp/cto-14fc/sonda2.py);
  2. TEMPO: a execução do portão na árvore real, nesta máquina, sem carga concorrente, fica em até 3 vezes a do portão de antes da D-B4-3 medido na mesma árvore, ou seja até 1,5 s. O número lido entra no log;
  3. nenhum RESOURCE_LOCK nem PROCESSORS novo no noexcept_alloc_test.
- **Porta de mão única:** não. **Custo de reverter:** baixo.

## 23. D-C1b-7: o caso "discount of elapsed time" passou 79 ms da janela no Arch estático (30/09, 13:27:44 pelo `date`)

- **FATO** (CI 36742575409, /var/tmp/ci-ac16-fail.log:1968): "FALHOU - discount of elapsed time, locale ambiente: total 1.9 s to 2.5 s: took 2579163 us", com o radix de ponto e o runner carregado (pkgconfig_test 97 s e embed_test 78 s ao mesmo tempo).
- **FATO** (tools/contido_dentro.sh:562-576): o caso roda S=2 com um gancho de 1 s antes da leitura. O correto termina em cerca de 2 s. O mutante do radix (`${t/./}`, que zera o tempo decorrido) terminaria em cerca de 3 s. O teto de 2,5 s é o ponto médio, então sobra só 0,5 s de folga para a sobrecarga do escalonador. A sobrecarga medida no servidor foi de 0,58 s (2,579 − 2,0).
- **O que o caso prova:** a SEPARAÇÃO entre "desconta" e "não desconta". Essa separação é igual à duração do gancho, e não depende da hora absoluta.
- **Fontes:**
  - uma asserção de ordem codificada como limiar de duração absoluta quebra sob o jitter de um runner carregado (https://github.com/lgtm-hq/py-lintro/issues/2278);
  - tolerância com margem, que ainda pega o defeito grande (https://maestro.dev/insights/how-to-avoid-flaky-tests-with-built-in-tolerance);
  - o Google mede a instabilidade e trata o teste frágil, não o esconde (https://testing.googleblog.com/2016/05/flaky-tests-at-google-and-how-we.html).
- **Opções:**
  - (a) ALARGAR A SEPARAÇÃO: gancho de 2 s e S=3. O correto dá cerca de 3 s, o mutante cerca de 5 s, e o teto no ponto médio (4,0 s) deixa 1,0 s de folga, 1,7 vezes a pior sobrecarga medida;
  - (b) subir só o teto (por exemplo para 2,9 s): encosta no mutante de 3 s, e sob a mesma carga o mutante também passaria. O caso deixaria de distinguir. Recusada;
  - (c) RESOURCE_LOCK no grupo: mente sobre o recurso e esconde a fragilidade. Recusada, como na §16 e na §22.
- **Escolha: (a).**
  - A regra que fica escrita no caso: folga ≥ 1,5 vezes a pior sobrecarga medida no CI, e gancho ≥ 2 × folga.
  - Piso de 2,9 s (S − 0,1) e teto de 4,0 s (S + gancho/2).
  - O mutante do radix continua caindo (cerca de 5 s).
  - Custo: +2 s no grupo prazos (os dois locales), de cerca de 7,5 s para cerca de 9,5 s, dentro dos 12 s da §16.
- **Prova:**
  - o mutante `${t/./}`, forçado num locale com vírgula na cópia, reprova com cerca de 5 s;
  - a árvore passa;
  - o grupo prazos medido sem carga fica em até 12 s, com o número lido;
  - no CI, o caso passa no Arch estático.
- **Porta de mão única:** não. **Custo de reverter:** baixo.
- É código da C1b (a contenção), que o líder mandou fechar. O vermelho bloqueia a W7-D (L-15), por isso o conserto entra agora.

## 24. O vermelho do "Windows - estático" no CI 36742575409: rodar de novo, com critério fixado antes (30/09, 13:37:06 pelo `date`)

- **FATOS medidos por mim** nos logs /var/tmp/ci-ac16-fail2.log (este run) e /var/tmp/ci-b4-109972206832.log (run 36740173106, mesmo job):
  - o tempo total dos testes DOBROU (817,63 s contra 403,08 s), mas o BUILD da MESMA base C++ subiu só 17% (329 s contra 280 s);
  - os lentos são todos testes que abrem processos e fazem cmake aninhado ou E/S: dep_zero_trace 120 s (timeout) contra 21,9; layers_selftest 61,8 contra 19,6; e blank_install_dir, libdir_mix, version_matches_tag e container_fixture_link;
  - o próprio noexcept_alloc_test, a única coisa que mudou (ac1654b), levou 4,39 s nesse job, e nenhum dos lentos o chama;
  - a MÁQUINA É OUTRA: Azure Region westus contra westcentralus, e Hosted Compute Agent 20260901.588 contra 20260828.587.
- **Leitura (INFERÊNCIA, ainda não provada):** variação do hospedeiro, concentrada em criação de processo e E/S (antivírus e disco do runner). Não é código nosso. A L-49 não deixa declarar isso sem a repetição.
- **Decisão:** rodar o CI de novo, e ele já é necessário para o 927c39f. Critério fixado ANTES:
  - (a) se o "Windows - estático" do novo run fica verde e o dep_zero_trace_selftest volta para menos de 40 s, classifica-se como variação do hospedeiro, com estes fatos registrados, e nada muda no código;
  - (b) se o MESMO job repetir o padrão (dep_zero_trace ≥ 60 s ou timeout, ou 3 ou mais dos 6 acima de 40 s), NÃO é hospedeiro: abre-se a medida por teste no próprio runner (tempo de criação de processo e de configure aninhado) antes de qualquer conserto, e nada de RESOURCE_LOCK ou TIMEOUT maior "para passar";
  - (c) se a lentidão aparecer em OUTRO job Windows, o mesmo que (b).

### §24, adendo (30/09, 13:39:18 pelo `date`): o "Windows - compartilhado" também estourou, e o fator comum medido

- **FATO** (/var/tmp/ci-ac16-fail3.log:3700-3701 e :4337):
  - win32_test_link_selftest levou 51,89 s e deu SKIPPED; container_fixture_link_selftest levou 62,96 s e passou; nos runs 36714800007 e 36740173106 os dois ficavam entre 7,4 e 14,4 s;
  - a região é westcentralus, a MESMA do run bom (o team-lead notou), e isso enfraquece a leitura "outra região".
- **FATO que eu medi agora, e que junta os dois jobs:** os DOIS jobs Windows vermelhos rodaram no Hosted Compute Agent 20260901.588 (fail2:"Version: 20260901.588", fail3 idem). O job bom do run 36740173106 rodou no 20260828.587. O fator comum não é a região, é a VERSÃO DO AGENTE ou da imagem do runner.
- **O que isso muda ANTES do resultado:**
  - o critério §24 (c) passa a nomear o "Windows - compartilhado": verde se win32_test_link_selftest e container_fixture_link_selftest ficarem abaixo de 40 s, e (b) se repetirem acima de 40 s;
  - a leitura do novo run ANOTA a versão do agente de cada job Windows;
  - agente .588 e lento de novo: a lentidão é PERSISTENTE da imagem nova, e não um acaso. Rodar de novo não resolve. Abre-se a medida no runner (criação de processo e configure aninhado, por teste) e decide-se com números, sem lock e sem timeout maior "para passar";
  - agente .588 e rápido: é variação, e fica registrada;
  - agente .587: o run não discrimina, e a leitura fica pendente.
- **Achado lateral, para a INBOX da INFRA-CI:** um teste que SAI COMO SKIPPED gastou 51,89 s para decidir pular (win32_test_link_selftest no Windows). Uma decisão de pular deveria ser barata e vir antes do trabalho caro. O item: "o win32_test_link_selftest decide o skip antes de qualquer trabalho caro; medir onde vão os 52 s".

## 25. D-B5-1: a montagem do fixture da B5 falhou 3 vezes pela mesma família, e a F1 CAI para este teste (30/09, 13:40:24 pelo `date`)

- **FATO** (team-lead): três montagens seguidas reprovaram pela mesma família, a lista de fontes montada fora do CMake:
  - (1) o caminho /build não foi reescrito depois de `|`;
  - (2) faltou src/core/color.cpp;
  - (3) faltou o cabeçalho GERADO glintfx/version_macros.hpp, que só o configure do CMake produz a partir do `project(VERSION)`.
  O Containerfile já registrava 5 ocorrências antes (Containerfile:145-200). Com estas, são 8.
- **FATO** (impl-da14): a 8ª tentativa exclui src/core/version.cpp, com motivo e trava contra apodrecer. Funciona, mas é a 4ª tentativa sobre a mesma família, e a "duas tentativas falhas proíbem a terceira por palpite" já tinha sido cruzada.
- **FATO que decide:** o draw2d_parity_test usa SÓ a API pública (a L-20 e a §19 ponto 5 exigiram isso). No Windows ele LIGA NA BIBLIOTECA: tests/CMakeLists.txt:2574, `target_link_libraries(draw2d_parity_test PRIVATE glintfx::glintfx)`. No Linux, a F1 RECOMPILA as fontes internas à mão. Os dois lados não provam o mesmo artefato, e isso fere a L-04 ("comportamento igual, provado em cada um") no próprio teste de paridade.
- **Fontes (L-22 e L-34):**
  - o guia do Docker para C++ (https://docs.docker.com/guides/cpp/multistage/) e o blog da equipe de C++ da Microsoft (https://devblogs.microsoft.com/cppblog/using-multi-stage-containers-for-c-development/) constroem no estágio construtor com o sistema de build do projeto;
  - o pkg-config é a interface de consumo que este projeto já publica e valida (cmake/glintfx.pc.in, GlintfxPkgConfigValidate.cmake).
- **Opções:**
  - (a) seguir na F1 com a exclusão declarada: é o 4º remendo da mesma família, e o artefato continua diferente do do Windows;
  - (b) F2 SÓ para os fixtures de API PÚBLICA: o estágio constrói a lib pelo CMake (estática, sem testes), instala num prefixo da imagem, e o fixture é ligado como um CONSUMIDOR, por `pkg-config --cflags --libs glintfx` sobre o .pc INSTALADO;
  - (c) F2 para todos os fixtures agora: amplia o escopo da B5, porque os fixtures de interior testam símbolos internos que não saem na lib instalada.
- **Escolha: (b).** A fronteira tem princípio e não é arbitrária:
  - fixture de API PÚBLICA liga na lib instalada, como no Windows e como o consumidor;
  - fixture de INTERIOR continua pela lista, gerada.
  - Ganhos: some a família da lista para este teste (o cabeçalho gerado, a camada core e os caminhos vêm do build real), o Linux passa a provar o MESMO artefato do Windows, e o .pc instalado é exercido por um consumidor de verdade.
- **Condições:**
  - o `dnf install` do estágio ganha cmake e ninja-build (é pacote da IMAGEM de teste, não do host nem da lib; não fere a L-07 nem a L-14); a imagem continua sem montagem do host;
  - o configure usa as MESMAS opções de compilação do build real (GLINTFX_WERROR=ON, Release), com testes desligados;
  - o prefixo de instalação fica dentro da imagem;
  - o fixture é ligado SÓ pelo .pc instalado (se o .pc não bastar, isso é defeito do .pc, que se conserta nele);
  - o `check_container_fixture_link.py` e o `_includes.py` aceitam essa forma: o fixture não tem lista de fontes, e o portão tem de tratá-lo como ligado na lib, com contagem impressa (L-40: não pode sumir da contagem);
  - a exclusão do version.cpp e o draw2d_fixture_sources.list SAEM.
- **Migrar o gl_context_parity_test** (também API pública, hoje pela lista, Containerfile:559) para a mesma forma: vai para a INBOX da INFRA-CI, junto da F2 geral da §21, e não para a B5.
- **Porta de mão única:** não. **Custo de reverter:** médio.
- **Custo:** mais tempo de docker build (a lib inteira) e uma imagem um pouco maior. Medir e registrar o tempo do estágio.

## 26. D-CI3: o container_fixture_link_selftest encosta na régua de 40 s no Windows (30/09, 14:27:50 pelo `date`)

- **FATO** (team-lead, CI 36745583418, a leitura pela §24): os dois jobs Windows rodaram no agente 20260901.588, e o compartilhado ficou VERDE. Pelo critério (a) da §24, a lentidão de antes foi VARIAÇÃO do hospedeiro.
- **O que sobra:** o container_fixture_link_selftest com 43,17 s no Windows estático. A série dele no Windows é 12,9 e 14,4 s (agente .587), 42,8 e 63,0 s (o run lento) e 20,8, 23,9 e 43,2 s (este run). Ele vive encostado na régua.
- **FATO medido por mim:** o mesmo --selftest leva 0,52 s nesta máquina Linux, com 10 controles executados e 0 pulados (/var/tmp/cto-cfl/st.log). No Windows, o custo NÃO é a lógica do portão; é o compilador e o ligador de VERDADE (MinGW g++ e ld) invocados por controle (check_container_fixture_link.py:280 casa g++.exe), onde criar processo e ligar são caros e sensíveis ao hospedeiro.
- **Diferente da §16 e da §22:** aqui o teste USA o recurso que o RESOURCE_LOCK glintfx_nested_build descreve. Ele roda o compilador e o ligador reais, é um build aninhado em miniatura. O projeto já classifica assim o gêmeo mais próximo: dep_zero_selftest, "o --selftest do check_dep_zero.py configura projetos CMake de fixtura", heavy com o lock (tests/CMakeLists.txt:3501 e seguintes), e outros onze (:2964 a :5720).
- **Fonte:** o ctest(1) diz que RESOURCE_LOCK garante que os testes com o mesmo recurso não rodam juntos (https://cmake.org/cmake/help/latest/prop_test/RESOURCE_LOCK.html). O recurso aqui é o conjunto de núcleos que o toolchain disputa.
- **Opções:**
  - (a) marcar heavy com o lock, no molde do dep_zero_selftest: comentário "glintfx-nested-build: heavy" com os números medidos (Linux 0,52 s aqui; Windows 12,9 a 63,0 s em 7 runs);
  - (b) cortar controles do selftest no Windows: perde a prova da forma MinGW (g++.exe e caminho C:\) que o próprio portão diz precisar. Recusada;
  - (c) SKIP no Windows: esconde. Recusada;
  - (d) TIMEOUT ou régua maiores: a régua é do projeto, e não se mexe nela para um teste. Recusada.
- **Escolha: (a).** Mais a instrumentação, que é barata: o selftest imprime o tempo de parede de CADA controle e o total, sempre. O próximo run no Windows diz onde vão os segundos (L-49, "instrumentar em vez de adivinhar").
- **Para a INBOX da INFRA-CI**, com os números impressos: medir por controle no Windows e decidir se o custo é o ld do MinGW, o antivírus do runner ou a criação de processo. É junto do SKIP-ANTES-DO-TRABALHO-CARO.
- **Prova:** o ctest --print-labels ou -N mostra o lock no teste; o próximo CI fica sem essa reprovação do agregador; e os tempos por controle aparecem no log do Windows.
- **Porta de mão única:** não. **Custo de reverter:** baixo.

## 27. D-INFRA-1: a cópia estagiada do Khronos que a F2 da B5 põe em tests/container/_arch_ports_lib (30/09, 15:26:07 pelo `date`)

- **FATO** (impl-infra, e conferido por mim): a F2 da §25 estagia a árvore do CMake em `tests/container/_arch_ports_lib/`, que é ignorada (.gitignore:85-86) e gerada pelo prepare_arch_ports_fixture.sh. Ela inclui `third_party/khronos/{gl.xml, LICENSE-APACHE-2.0.txt, README.md}`. O vendor_purity varre o sistema de arquivos (de propósito, e não o git ls-files) e acusa esses três como fora da lista fechada, então o portão fica VERMELHO na árvore do impl-da14 enquanto a pasta existir.
- **FATO medido:** os três arquivos estagiados são BYTE A BYTE iguais aos originais da lista fechada (cmp nos três).
- **Opções:**
  - (a) aceitar uma cópia estagiada SÓ se for byte a byte igual ao original da lista fechada, e SÓ dentro de uma raiz de estágio DECLARADA, numa lista fechada (hoje tests/container/_arch_ports_lib). A exceção é o CONTEÚDO, e não o caminho;
  - (b) mover o estágio para fora do repositório (o contexto do docker build em /var/tmp): muda o pipeline da B5, na área do impl-da14, no meio da fatia;
  - (c) podar as pastas ignoradas pelo git: esconde todo intruso que alguém ponha numa pasta ignorada, e enfraquece a L-07;
  - (d) podar pelo nome `_arch_ports_*`: esconde qualquer coisa ali. Mesmo defeito da (c), em menor escala.
- **Escolha: (a).** Ela não enfraquece nada:
  - um arquivo modificado (um byte que seja) é acusado;
  - um arquivo a mais dentro do third_party estagiado é acusado;
  - uma cópia idêntica fora das raízes declaradas é acusada;
  - só a cópia idêntica, no lugar declarado, passa, e a mensagem de sucesso CONTA as cópias aceitas (L-40).
- **Prova (L-36), com 4 fixtures no --selftest**, cada uma vista no estado certo contra o 50e27eb e depois do conserto:
  1. uma cópia idêntica numa raiz declarada: aceita;
  2. a mesma cópia com 1 byte trocado: acusada;
  3. um arquivo extra ao lado dela: acusado;
  4. uma cópia idêntica numa pasta NÃO declarada: acusada.
- **Cerca:** o arquivo é o check_vendor_purity.py (da trilha de infraestrutura). O estágio continua do impl-da14, que não muda nada.
- **Porta de mão única:** não.

## 28. D-SRGB-1: a "falta de sRGB no llvmpipe" era DEFEITO NOSSO no adaptador EGL (30/09, 15:32:42 pelo `date`)

- **FATO medido** (sonda do CTO, /var/tmp/cto-eglprobe/probe.c, rodada DENTRO da imagem glintfx-wltest:b5-f2, sem /dev/dri, sem rede, com LIBGL_ALWAYS_SOFTWARE=1 e a plataforma surfaceless, rc 0):
  - `EGL 1.5 vendor=Mesa Project`;
  - `EGL_KHR_gl_colorspace presente: sim`;
  - `eglChooseConfig SEM colorspace: ok=1 n=1`;
  - `eglChooseConfig COM EGL_GL_COLORSPACE_KHR: ok=0 n=0 err=0x3004`, que é EGL_BAD_ATTRIBUTE.
- **FATO no código:** src/platform/wayland/egl_context_adapter.cpp:558-560 põe `EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR` na lista de atributos do `eglChooseConfig`. Pela especificação (https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_gl_colorspace.txt, "New Tokens"), esse atributo é aceito SÓ por eglCreateWindowSurface, eglCreatePbufferSurface e eglCreatePixmapSurface, e nunca pelo eglChooseConfig. O Mesa recusa com EGL_BAD_ATTRIBUTE, o try_choose devolve false, e o adaptador conclui "srgb não suportado" (:595-610) em QUALQUER driver Mesa.
- **Consequência:** no Linux, `srgb_framebuffer=1` NUNCA funcionou. A "ausência medida" em tests/measured_exceptions.txt, a `gl_context_parity_test.srgb_support` (:132) e a nova `draw2d_parity_test.srgb_on_cells_absent` (:203), mediam o NOSSO defeito, e não o driver. O caminho sRGB ligado do Linux nunca foi exercido.
- **Conserto** (a forma que a especificação manda; L-34: a fonte é a própria especificação da Khronos):
  - (1) o eglChooseConfig volta a escolher só por RGBA8, stencil8 e samples;
  - (2) com a opção ligada: exigir `EGL_KHR_gl_colorspace` na lista de extensões do display. Sem ela, é `unsupported`/`srgb_framebuffer`, como hoje;
  - (3) criar a superfície com `{EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR, EGL_NONE}` na lista de atributos do eglCreateWindowSurface. Falha COM o atributo e sucesso sem ele é `unsupported`, e nunca uma superfície linear silenciosa com o rótulo sRGB;
  - (4) depois de criada, conferir com `eglQuerySurface(..., EGL_GL_COLORSPACE_KHR, ...)` que o valor é SRGB, e só então marcar m_srgb_supported = true.
- **Prova (L-20/L-36):**
  - uma célula de seam, com EGL de mentira ou o teste existente do adaptador: com a opção ligada, o eglChooseConfig NÃO recebe EGL_GL_COLORSPACE_KHR, e o eglCreateWindowSurface RECEBE. Vista vermelha contra o código de hoje;
  - no container: o draw2d_parity_test passa a RODAR as células do sRGB ligado no llvmpipe (srgb_on_cells_absent=0), e o gl_context_parity_test.srgb_support passa a 1;
  - as duas linhas de measured_exceptions.txt (:132 e :203) são REVISTAS pelo número novo: saem ou se estreitam, e nunca ficam por inércia;
  - o mutante do FRAMEBUFFER_SRGB, que antes "só o Windows podia exercer", tem de ficar VERMELHO no Linux.
- **Onde e quando:** é defeito de PRODUTO da camada de plataforma (W6b), que a B5 revelou. Entra na W7-D, ANTES do fechamento: a promessa de cor da API de desenho ("com srgb_framebuffer ligado a mistura é em luz linear") não está provada no Linux sem ele. Quem implementa: o impl-da14, que conhece o adaptador.
- **Os commits da B5 continuam aceitos:** o teste está CERTO, porque relatou com honestidade o que a lib dizia. O defeito está uma camada abaixo.
- **Porta de mão única:** não. **Custo de reverter:** baixo.

## 29. D-B7-1: os 4 achados IMPORTANTE da L-17 na B7 são consertados DENTRO da W7-D, antes dos mutantes (30/09, 15:40:02 pelo `date`)

- **FATO** (api-review, /var/tmp/cto-w7d/b7/auditoria-revisao-r2d-batch.md:77-107):
  - B7-I1: `renderer_2d_impl::send_pending` (renderer_2d_impl.cpp:179) com 60 linhas;
  - B7-I2: `create_embedded_program` (embedded_program.cpp:149) com 50 linhas (42 de código);
  - B7-I3: `send_to_buffer` (vertex_stream.cpp:68) com 7 parâmetros, e `renderer_2d_impl::create` (:49) com 5;
  - a frase de `frame_report_tally` precisa de "E": "conta os destinos do quadro E classifica o valor inválido da peça".
- **A lei:** a L-17 do projeto chama os limites do CONTRACT.md §6.2 de INEGOCIÁVEIS ("no máximo 40 linhas por função, no máximo 4 parâmetros (agrupe em struct)"), e a pergunta 2 do revisor (a frase sem "e") é reprovação. Não é estilo nem gosto, e não há escolha entre consertar e adiar para depois da onda. A ordem do líder de 29/09 ("Onde fica o CÓDIGO do framework?") também puxa para dentro.
- **Fonte das técnicas:** o catálogo de refatoração de Fowler, "Extract Function" (https://refactoring.com/catalog/extractFunction.html) e "Introduce Parameter Object" (https://refactoring.com/catalog/introduceParameterObject.html). A refatoração preserva o comportamento, e a rede de segurança são os testes que já existem.
- **Os átomos:**
  - **I1, send_pending:** as fases com nome próprio, cada uma com no máximo 40 linhas:
    - `drop_pending_without_context()` (o caminho do contexto não corrente);
    - `batch_pending_in_paint_order()` (sort, add_quad, contagem de OOM; devolve quantas entraram);
    - `upload_and_draw_batch()` (envio, estado, uniforms, desenho; devolve se desenhou e guarda o primeiro erro);
    - `settle_piece_counts(added, drawn)`;
    - o corpo de send_pending fica só com a SEQUÊNCIA e o leave e o clear do fim;
  - **I2, create_embedded_program:** `compile_both_shaders()` e `link_program(vertex, fragment)`, cada um com o seu caminho de erro, e a função de fora só encadeia;
  - **I3a, send_to_buffer:** um struct `buffer_upload {target, buffer, capacity&, data, bytes}` e a assinatura `send_to_buffer(gl, upload, technique)`, com 3 parâmetros;
  - **I3b, renderer_2d_impl::create:** um struct `renderer_2d_options {srgb_framebuffer, reserve_pieces, allocator}` e a assinatura `create(host, gl, options)`, com 3;
  - **I4, frame_report_tally:** `refusal_of_quad`/`refusal_of_rect` e o `refusal_of_values` vão para `src/draw2d/piece_refusal.{hpp,cpp}`, um assunto por arquivo. As células de recusa saem do frame_report_tally_test para um `piece_refusal_test.cpp` NOVO, MOVIDAS, com o texto das asserções idêntico.
- **Critério (L-43), fixado antes:**
  - o comportamento é idêntico: as ASSERÇÕES dos testes existentes não mudam. Só a assinatura das chamadas muda, nos pontos de chamada, e as células movidas mantêm o texto; o diff dos testes mostra isso;
  - todos os testes da draw2d verdes com g++ e clang++;
  - o fnmetrics.py do api-review (/var/tmp/cto-w7d/b7/fnmetrics.py), rodado de novo, dá ZERO funções acima de 40 linhas e ZERO acima de 4 parâmetros em src/draw2d;
  - a frase de cada arquivo sem "e".
- **A ordem em relação aos mutantes da B7:** a refatoração vem ANTES. Mutante que roda sobre código prestes a mudar é trabalho perdido, e a B7 tem de atestar o código que vai ficar. Então:
  1. a refatoração, uma unidade por commit (I1, I2, I3a, I3b, I4), sem suíte pesada: os testes da draw2d são leves e rodam contidos;
  2. o CTO reaplica os SEUS 8 mutantes da B4 sobre o código refatorado, e todos têm de continuar morrendo. É a prova de que a rede de segurança ainda segura depois da mudança de forma;
  3. só então os mutantes e o sanitizer da B7, na janela pesada.
- **Custo:** 5 commits pequenos, todos em src/draw2d e nos testes da draw2d, sem API pública nova: a porta de mão única não se move, porque os tipos novos são internos.

## 30. D-B5-2: o fixture de consumidor (F2) ganha o substituto de alocação como os outros 24 (30/09, 15:48:21 pelo `date`)

- **FATO** (CI 36759613878, /var/tmp/ci-c98-fail.log): "AVISO: draw2d_parity_test nao define o substituto de operator new (_Znwm)" e "substitutos_presentes: 24 de 25". O portão de relatório de alocação (tests/container/check_alloc_report.sh, CONTAINER-LEAK-COUNTER) reprova com razão, pela L-40.
- **FATO** (tests/container/Containerfile:316-319 contra :320-335): as linhas dos outros fixtures levam `/build/alloc_counter_classify.cpp /build/alloc_counter_hook.cpp`, a INSTRUMENTAÇÃO DE TESTE copiada para /build (:292-295). A linha do draw2d_parity_test ficou sem ela.
- **A distinção que decide:** a §25 matou a lista À MÃO das fontes DA BIBLIOTECA. O gancho de alocação NÃO é fonte da biblioteca, é o instrumento do lado do teste que TODO fixture carrega, assim como um consumidor liga o próprio código e a sua instrumentação contra a lib instalada. Acrescentá-lo não reabre a família da lista.
- **Opções:**
  - (a) as duas fontes do gancho na linha do fixture, como nos outros 24;
  - (b) um LD_PRELOAD de uma biblioteca do gancho: é um padrão novo só para ele, e o portão conta o símbolo DEFINIDO no binário, então o preload não contaria;
  - (c) uma exceção no portão para o fixture de consumidor: esconde o fixture do contador de vazamento, justo o que a camada nova precisa.
- **Escolha: (a).** A linha passa a ser: o fixture, os dois arquivos do gancho e o `$(pkg-config ... glintfx)`. O gancho é instrumento de teste; a biblioteca continua vindo só do .pc.
- **Prova:**
  - o `check_container_fixture_link.py --exec` e o `_includes.py` verdes, com a contagem de fixtures ligados na lib ainda 1/1;
  - no próximo CI, "substitutos_presentes: 25 de 25", e o relatório de alocação do draw2d_parity_test LIDO (os números de alocação do fixture impressos), e não só presente.
- **Porta de mão única:** não.

## 31. D-SRGB-2: o Windows anuncia sRGB e não codifica; a capacidade passa a ser LIDA DE VOLTA no framebuffer, num lugar só para os dois sistemas (30/09, 15:57:06 pelo `date`)

- **FATO** (CI 36759613878, /var/tmp/ci-c98-win.log, relatado pelo team-lead):
  - o draw2d_parity_test tem 51 células no "Windows - compartilhado" e 3 reprovadas, TODAS com [srgb=on];
  - srgb_on_cells_absent=0: o contexto ACEITOU a opção;
  - half_white_srgb_on=128 e half_red_srgb_on=128, onde o esperado é 188 ±2 (0,5 linear codificado dá 187,5); o modo desligado dá 127, que está certo;
  - a lista fechada (FRAMEBUFFER_SRGB = a opção, depois do flush e do fim) NÃO está entre as reprovadas, então o glEnable(GL_FRAMEBUFFER_SRGB) CHEGA.
- **O mecanismo, pela especificação** (ARB_framebuffer_sRGB, https://registry.khronos.org/OpenGL/extensions/ARB/ARB_framebuffer_sRGB.txt): a conversão acontece só se FRAMEBUFFER_SRGB estiver ligado E o FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING do anexo de destino for SRGB. Com o enable chegando e o pixel sem conversão, o buffer de trás do Windows NÃO tem codificação sRGB, embora o wglChoosePixelFormatARB tenha aceitado WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB (wgl_context_adapter.cpp:267-268 e :305-331). Ou o driver do runner (o opengl32 do Mesa instalado pelo tools/ci/install-mesa-opengl32.ps1) aceita o atributo e entrega um formato linear, ou o formato é "capable" e o driver não aplica. As duas hipóteses se separam por uma leitura, e não por palpite.
- **É o gêmeo da §28:** lá o EGL recusava errado; aqui o WGL ACEITA errado. A §28 já fixou a regra certa para o EGL (ler de volta, eglQuerySurface), e ela vale para TODO sistema.
- **Opções:**
  - (a) ler a capacidade de volta NO FRAMEBUFFER, depois de o contexto ficar corrente, num lugar só e agnóstico de sistema: a fachada gl_context_facade.cpp, pelo proc_address, chama `glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)` com o framebuffer 0 ligado (permitido no GL 3.x core para o framebuffer padrão). Se a opção foi pedida e a resposta não for GL_SRGB, fecha e devolve unsupported/"srgb_framebuffer". Isso fecha a promessa nos dois sistemas com o MESMO critério;
  - (b) consertar só no WGL, conferindo o atributo com wglGetPixelFormatAttribivARB: confere o que o formato DIZ, e não o que o framebuffer FAZ, e deixa a regra em dois lugares (L-17, o gêmeo);
  - (c) declarar o Windows como ausência: seria aceitar a mentira do anúncio.
- **Escolha: (a)**, mais a medição.
  - O draw2d_parity_test imprime `MEASURED draw2d_parity_test.back_buffer_color_encoding_srgb_<modo>` (a leitura pelo lado do TESTE, pela API pública e pelo proc_address), para o CI mostrar o valor nos dois sistemas.
  - A §28 continua: o EGL também passa por esta leitura, que é a mesma régua para os dois.
- **O que o próximo CI decide:**
  - Windows com encoding LINEAR: a lib agora RECUSA com honestidade, as células do sRGB ligado viram ausência DECLARADA no Windows (a linha de measured_exceptions estreitada para windows, com o motivo medido), e o caminho do sRGB ligado fica provado no Linux (a §28);
  - Windows com encoding SRGB e o pixel ainda em 128: é defeito do DRIVER do runner. O registro é por chave, com a evidência, e a VM Windows do laboratório (com a liberação do team-lead) mede com outro driver antes de qualquer conclusão sobre "o Windows".
- **Prova (L-20):** uma célula de seam na fachada com o GL de mentira: a opção pedida, com o GL de mentira respondendo LINEAR, tem de dar unsupported; respondendo SRGB, ok. Vista vermelha contra a fachada de hoje.
- **Porta de mão única:** não. O que muda para o consumidor é que um sistema passa a RECUSAR uma opção que antes aceitava sem cumprir. O texto público da opção já diz "unsupported quando o sistema não oferece".

## 32. D-INF-1: o selftest do ccache nunca rodou no script real; o setup_ccache recebe a raiz por parâmetro (30/09, 16:17:48 pelo `date`)

- **Fato, medido fora da árvore sobre o blob 63656b0:**
  - Os controles do ccache atribuem `ROOT_DIR=/selftest-root` (tools/preci.sh:2206, :2251, :2255 e :2263), mas o `ROOT_DIR` é `readonly` desde a linha 165.
  - No script real, o subshell morre com "ROOT_DIR: a variável permite somente leitura" e devolve vazio. O resultado é rc=1, tanto com ccache no PATH quanto sem ele.
  - O mesmo defeito existe desde o 6b48ef2, e continua no fc64b57.
  - A reprodução mínima: `bash -c 'readonly R=1; x=$( R=2 A=3; echo vivo ); echo "[$x]"'` imprime `[]`.
  - O `preci.sh --selftest` roda no job `lint` do CI (ci.yml:2924-2928), então entregar o fc64b57 deixaria o Lint vermelho. Os "5 OK" relatados vieram de uma execução isolada, e não do `--selftest` inteiro, que dependia da janela: era a condição pendente.
- **Opções:**
  - (a) o `setup_ccache` recebe a raiz como parâmetro (`setup_ccache "$ROOT_DIR"`), e o selftest passa `/selftest-root`;
  - (b) rodar os controles em `bash -c` com `declare -f`, o padrão da :1679;
  - (c) tirar o `readonly`.
- **Escolha: (a).**
  - É a menor mudança, e torna a dependência explícita.
  - A (c) apaga uma proteção que já tem controle próprio (PRECI-ROOTDIR-SC2155).
  - A (b) duplica o mecanismo só para testar.
- **Prova exigida:**
  - o `tools/preci.sh --selftest` INTEIRO roda verde no blob, com o rc lido de variável, duas vezes: com o ccache real, e com um PATH sem ccache (uma fazenda de links de /usr/bin sem o ccache);
  - um mutante que ignora o parâmetro tem de morrer;
  - o diretório do stub não pode vazar quando um controle reprova (hoje o `fail` sai antes do `rm`).
- **Entrega (a proposta A do impl-infra):** fc64b57 mais o conserto do selftest (o stub, o NÃO APLICÁVEL e esta §32). O C2 fica fora.
- **Porta de mão única:** não.

## 33. C4 (o modo --carga-servidor): desenho aprovado com cinco mudanças (30/09, 16:17:48 pelo `date`)

- **(a) A lista dos testes sensíveis a tempo:**
  - Sem os 5 nomes escritos no script. O plano manda gerar a lista pela marca, e uma lista fixa de nomes é cópia de um valor que já tem dono.
  - O piso é: a lista não está vazia E contém o grupo que a calibração usa. A contagem e os nomes são impressos.
  - Depois da edição dos labels, o `--blob-only` tem de mostrar o mesmo "N de M" de antes. O `blob_selftests.py` lê a forma `"selftest;tempo"`.
- **(b) O teto de 2,86x invalida a calibração.**
  - Não é um aviso. Se o degrau preciso para chegar a ≥2,0x, ou para o 10c antigo reprovar, passar de 2,86x, a carga é maior que a do servidor: rc 1, "calibrado além do servidor", e o número vai ao líder.
- **(c) A carga cobre toda a janela medida.**
  - O ctest da carga tem de estar vivo no começo e no fim de cada rodada medida. Se acabar (147 s frios é menos que a escada inteira), é reiniciado em laço.
  - Uma rodada medida com a carga morta é descartada e contada, nunca aproveitada.
- **(d) Os tetos de processos.** O TasksMax de 300 e 200 precisa do ok do team-lead, porque a regra da sessão é 64. Há um teto de processos somado, medido por /proc, e impresso.
- **(e) A prova vermelha.**
  - O 10c está no grupo prazos nos dois commits (contido_dentro.sh:562, "discount of elapsed time"); confirme pela lista de casos que o próprio grupo imprime.
  - Cada degrau abaixo, depois de o antigo não reprovar, reconfere o novo em 3 de 3 e o teto da (b).
  - Todas as tentativas são impressas.
- **O que já está medido:** o cgroup do usuário tem o controlador `cpu` delegado (`cgroup.controllers` = "cpu io memory pids"). A sonda leve do CPUQuota fica liberada.
