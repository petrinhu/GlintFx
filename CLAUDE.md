> **PROJETO PARA DISTRIBUIÇÃO. NUNCA raciocine sobre ele como projeto de consumidor único.** O GlintFx é biblioteca pública sob AGPL-3.0, com base de consumidores **aberta e desconhecida**. Toda decisão de API, ABI, empacotamento, ordem de entrega e prioridade se julga pelo consumidor externo que ainda não conhecemos, nunca por um integrador específico. Premissa de consumidor único é **erro**, e já produziu um: em 21/08/2026 ela foi inferida sem estar escrita em lugar nenhum e quase amputou a lente de produto do planejamento.

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## LEIS CANÔNICAS: leia `GODS_LAWS.md` ANTES de agir

**[`GODS_LAWS.md`](GODS_LAWS.md) contém as ordens expressas do líder e tem precedência sobre este arquivo, sobre os manuais e sobre qualquer preferência sua.** Ele não existe para ser declarado, existe para ser **usado no momento da ação**.

**Como usar, sem exceção:**

1. Antes do primeiro comando de qualquer tarefa, confira se um dos gatilhos abaixo casa com o que você vai fazer. Casou: **abra `GODS_LAWS.md` e leia a lei inteira antes de agir**, não depois.
2. Ao despachar subagent, **cole no prompt da task** o texto das leis cujo gatilho casa com ela, mais o caminho absoluto de `GODS_LAWS.md`. Subagent não herda este contexto.
3. Ao relatar ao líder, diga qual lei aplicou e como.
4. Ordem nova do líder entra em `GODS_LAWS.md` **no instante em que ele a dá**, com data e o texto dele verbatim.
5. Agente nenhum revoga, flexibiliza ou reinterpreta lei. Só o líder.

**Gatilhos (dispara quando você vai...):**

| | Dispara ao... | Lei |
|---|---|---|
| ⚖ | procurar prior art, base de código ou "como era antes" | L-01 projeto do zero |
| ⚖ | desenhar API, escopo ou entregável | L-02 é biblioteca, não aplicação |
| ⚖ | criar build, escolher padrão de linguagem | L-03 C++23 + CMake |
| ⚖ | escrever CI ou declarar suporte de plataforma | L-04 cinco alvos, CachyOS próprio |
| ⚖ | tocar janela, input, display, ou copiar exemplo de internet | L-05 Wayland puro, sem X11 |
| ⚖ | implementar teclado, keymap ou texto digitado | L-06 parser XKB próprio |
| ⚖ | adicionar dependência, `FetchContent`, vendorizar | L-07 dependência zero |
| ⚖ | criar repo, `LICENSE`, cabeçalho, publicar qualquer coisa | L-08 público, AGPL-3.0 |
| ⚖ | rodar teste que abre janela, injeta input ou captura tela | L-09 container, nunca a sessão viva |
| ⚖ | escolher entre abordagens, decidir design ou arquitetura | L-10 opções via AskUserQuestion |
| ⚖ | `git push`, merge em `main`, criar tag, publicar release | L-11 push por onda, tag com aval |
| ⚖ | escrever ou revisar código de produto | L-12 agente especialista, papéis distintos |
| ⚖ | escrever qualquer mensagem ao líder | L-13 timestamp real |
| ⚖ | instalar, remover ou atualizar pacote de sistema | L-14 pedir autorização |
| ⚖ | fechar um marco ou notar a hora | L-15 nunca mandar descansar |
| ⚖ | abrir sessão, precisar de algo de outro projeto, receber ideia do Gus | L-16 bus `gusworld_ia_autocomm` |
| ⚖ | escrever função, arquivo, classe ou módulo novo | L-17 proibido monolito, cada função é um átomo |
| ⚖ | ir executar qualquer trabalho de produto | L-18 main só orquestra; fable audita e cria, sonnet implementa |
| ⚖ | criar módulo, tocar a fronteira do SO, desenhar API pública | L-19 camadas, portas em compile-time, fronteira opaca |
| ⚖ | escrever qualquer código com comportamento | L-20 TDD estrito, vermelho antes de verde |
| ⚖ | nomear qualquer coisa, escrever comentário ou commit | L-21 inglês e `snake_case` no código, commit em pt-br |
| ⚖ | projetar assinatura pública ou tratar erro | L-22 nenhuma exceção cruza a API pública |
| ⚖ | commitar, fechar uma fatia, ou dar push | L-23 portões de qualidade e `preci.sh` |
| ⚖ | pensar em cobertura, formatação ou auditoria | L-24 sem meta de cobertura, clang-format LLVM, dossiê na 1.0 |
| ⚖ | iniciar build pesado, ASan, teste de janela ou demo | L-25 armar o `watchcode` na janela e desarmar ao fim |
| ⚖ | criar tag, publicar release, ou mexer na versão | L-26 versão e tag `vA.B.C.D`, `SOVERSION` segue o `A` |
| ⚖ | escrever ordem de serviço, ou aceitar corte proposto por agente | L-27 fato separado de inferência; corte exige citação |
| ⚖ | tocar folha de estilo, tema, layout de UI ou parser de estilo | L-28 RCSS é o formato, implementado em casa, sem RmlUi |
| ⚖ | não saber como implementar algo, ou querer ver prior art | L-29 pode ler RmlUi e SDL3 para aprender; copiar é proibido |
| ⚖ | tocar mapa, grade, colisão, rota ou visibilidade | L-30 mapa é mecanismo da lib; o formato é nosso; conteúdo de jogo fica fora |
| ⚖ | tocar contexto gráfico, shader ou carregador de GL | L-31 OpenGL 3.3 core |
| ⚖ | escolher a próxima fatia, antes de a demo rodar | L-32 caminho principal sempre, mais no máximo UMA trilha paralela |
| ⚖ | tocar qualquer coisa de mapa | L-33 avisar o `mapeditor`; se fora do ar, pelo bus |
| ⚖ | começar QUALQUER fatia ou onda | L-34 brainstorm com o líder, fable planeja, main verifica, sonnet implementa |
| ⚖ | operar em modo autônomo, ou sair dele | L-34 o `fable` decide no lugar do líder; o main registra em `DECISOES_AUTONOMAS.md` ao vivo |
| ⚖ | tocar a superfície de entrada, ou a entrega de evento | L-35 entrega determinística, sem duplicar e sem reordenar; é promessa pública |
| ⚖ | tocar cursor, áudio, gamepad ou compose | L-36 as quatro decisões de escopo de 23/08; nenhuma é mais pergunta |
| ⚖ | o líder aprovar, rejeitar ou mudar algo, ou fechar item de alta prioridade | L-37 avisar o Gus Dragon sem ele perguntar |

---

## O que é o GlintFx

Biblioteca/framework **2D completo e reutilizável** em C++23. O consumidor (outro projeto) obtém janela, loop principal, render 2D, input, gamepad, áudio, fonte, carregamento de asset e math2d, e escreve apenas a lógica dele. O entregável é a **API pública + headers + pacote CMake**, não um binário final.

## Estado atual do repositório (22/08/2026)

**Projeto em andamento desde a fundação de 21/08/2026 - já não é "do zero".** Esta seção foi reescrita porque a versão anterior (datada de 21/08/2026) afirmava "zero commits, sem remoto, nenhum código, nenhum CMake, nenhum teste" - falso desde o primeiro dia do projeto, e cada vez mais desatualizado depois. Todo número abaixo foi medido nesta data, com o comando indicado; nada veio de memória.

**Git e remoto:**

- `git rev-list --count HEAD`: **56 commits** no branch local `main`.
- `git ls-remote origin main`: o remoto `git@github.com:petrinhu/GlintFx.git` está em `6d3b506...` - **52 commits** (`git rev-list --count origin/main`). Os outros **4 são locais, ainda não empurrados** (L-11: push só ao fechar onda completa, com aval do líder). `git log origin/main..HEAD --oneline` lista exatamente esses 4.
- Último push, `git@github.com:petrinhu/GlintFx.git` @ `6d3b506` ("docs(testes): remove meta de cobertura instruida no T1..."), teve CI **verde nos 6 jobs** (`gh run view <id> --json jobs`): `Fedora 44 (primario)`, `Ubuntu`, `CachyOS`, `Arch`, `Windows`, `Gate das leis`. Os 4 commits locais ainda não passaram pelo CI, por não terem sido empurrados.

**Árvore real** (contada por `find`, não estimada):

- **12 arquivos `.md` na raiz**: os 9 manuais canônicos do vault listados em "Autoridade documental" abaixo, mais este `CLAUDE.md`, `GODS_LAWS.md` e `TODO.md`.
- `LICENSE` (AGPL-3.0), `.gitignore`, `.github/workflows/ci.yml` (a matriz de 6 jobs acima), `.bigtech-porte` (`porte=bigtech`).
- `CMakeLists.txt` na raiz + `cmake/`: **7 arquivos** (`GlintfxOptions.cmake`, `GlintfxCompileOptions.cmake`, `GlintfxLibrary.cmake`, `GlintfxInstall.cmake`, `GlintfxTest.cmake`, `glintfx-config.cmake.in`, `version_macros.hpp.in`).
- `include/`: **1 header público**, `include/glintfx/core/version.hpp` (versão em runtime da lib; nenhuma exceção cruza a API pública, L-22).
- `src/`: `CMakeLists.txt` + **1 camada** (`core/`, com `version.cpp`) - o próprio comentário do CMake da camada diz "hoje só core/ existe" (L-19: cada camada é uma unidade CMake visível, a próxima camada que toca o SO nasce como entrada nova nesta lista, nunca misturada ao core).
- `tests/`: harness de teste **próprio** em `tests/harness/` (`check.hpp`/`check.cpp`, macro `GLINTFX_CHECK`; `test_registry.hpp`/`.cpp`, macro `GLINTFX_TEST`) - existe porque a **L-07 proíbe Catch2 e GoogleTest** (dependência externa). Casos de teste C++ (`version_test`) mais 8 gates de shell em `tests/tools/*.sh` (`check_consume.sh`, `check_embed.sh`, `check_exports.sh`, `check_install_includedir.sh`, `check_install_packager_layout.sh`, `check_layers.sh`, `check_no_target_collision.sh`, `check_output_name.sh`), registrados como `add_test` condicionais. Rodando a suíte agora (`ctest --test-dir build`): **9 casos passam em modo shared** (`version_test`, `visibility_test`, `layers_test`, `consume_test`, `embed_test`, `install_includedir_test`, `install_packager_layout_test`, `output_name_test`, `no_target_collision_test`) e **8 em modo estático** (os mesmos, menos `visibility_test`, que só existe com `BUILD_SHARED_LIBS=ON` em Unix).
- `tools/ci/`: 2 scripts PowerShell (`check-consume.ps1`, `diagnose-win-runtime.ps1`) para o job Windows.

A seção "Comandos" abaixo já era real; os comandos foram reexecutados nesta data e continuam funcionando (configure + build + ctest, shared e estático, `exit 0` nos três).

## Decisões fechadas pelo líder (21/08/2026)

Estas foram tomadas explicitamente via AskUserQuestion e são o ponto de partida do projeto. Mudança em qualquer uma delas é decisão do líder, não de agente.

| Eixo | Decisão |
|---|---|
| Natureza | Biblioteca/framework reutilizável (não aplicação final) |
| Domínio | Framework 2D completo (janela, loop, render2d, input, gamepad, áudio, fonte, asset, math2d) |
| Linguagem e build | **C++23 + CMake** |
| Plataformas | **Fedora 44 (primário)**, Ubuntu, CachyOS, Arch, Windows. No Linux, **apenas Wayland** |
| Dependências | **Zero além da stdlib e da API do SO** |
| Licença e visibilidade | **Público no GitHub, AGPL-3.0** |

### Plataformas: Fedora 44 é o alvo primário; CachyOS é alvo próprio

**Fedora 44 é o alvo primário**, por ser o sistema que o líder usa (ordem dele, 21/08/2026). No CI a imagem fica **pinada em `fedora:44`**, não em `:latest`: o alvo primário tem de falhar quando a máquina dele falharia. Quando ele atualizar de versão, o pin sobe junto. Os outros quatro alvos são de portabilidade.

**CachyOS não é "Arch renomeado" e não é coberto pelo job de Arch.** Ordem explícita do líder. Toolchain, flags de otimização, kernel e empacotamento do CachyOS diferem; a matriz de CI precisa de **cinco entradas distintas** (Fedora, Ubuntu, CachyOS, Arch, Windows), e um verde no Arch não autoriza declarar CachyOS suportado.

### Linux é Wayland puro, sem X11

Ordem do líder, 21/08/2026: **no Linux usamos apenas a camada Wayland, sem X11.** Não há backend X11, não há fallback por XWayland, e o projeto não roda em sessão X11 - por desenho, não por pendência. O backend Linux fala o protocolo Wayland direto (`wl_compositor`, `xdg-shell`, `wl_seat`, `wl_keyboard`, `wl_pointer`) contra `libwayland-client`.

Consequências que valem lembrar antes de escrever o primeiro backend:

- **Nada de Xlib, XCB, XTest, Xvfb ou `xdotool`** neste repositório, nem em produção nem em teste. Se um exemplo de internet usar, ele não serve aqui.
- Usuário em sessão X11 simplesmente não é público alvo. Isso é escolha do líder, registrada, não um bug a consertar.
- **Keymap: parser próprio, decidido pelo líder em 21/08/2026.** O compositor entrega o keymap em texto XKB (`xkb_keycodes`, `xkb_types`, `xkb_compat`, `xkb_symbols`) por file descriptor, e **nós escrevemos o parser em casa**. `libxkbcommon` está **fora** - a lei de dependência zero vence. Escopo real do parser, para dimensionar a fatia: keycode para keysym, níveis e grupos de modificador, latch/lock, sequência de compose e tecla morta, e conversão de keysym para UTF-8.
- Ferramental de protocolo instalado e conferido em 21/08/2026: `libwayland-client` **1.25.0**, `wayland-scanner`, e `wayland-protocols` **1.49** (`wayland-protocols-devel`, instalado neste dia a pedido do líder). O `xdg-shell.xml` está em `/usr/share/wayland-protocols/stable/xdg-shell/`, e o binding sai dele por `wayland-scanner` em passo de build, não é código escrito à mão nem vendorizado.
- `libwayland-client` conta como **API do sistema** para efeito da lei de dependência zero (mesma categoria de Win32), assim como o `libxkbcommon` **não** conta. Se essa fronteira mudar, é decisão do líder.

### Lei de dependência zero

Nada além da **biblioteca padrão de C++23** e das **APIs do sistema operacional** (Wayland, Win32, GL, evdev/XInput, PipeWire/ALSA/WASAPI, etc.; **no Linux, Wayland sem X11**). Sem gerenciador de pacote de terceiros, sem FetchContent de biblioteca externa, sem vendorizar dependência.

Consequência prática: decode de imagem, rasterização de fonte, loader de GL, mixagem de áudio e decode de gamepad **são escritos em casa**. Antes de trazer qualquer coisa de fora, pare e pergunte ao líder - a lei é dele e só ele a suspende.

## Autoridade documental

Os manuais na raiz são normativos e vencem qualquer preferência do agente, **e perdem para `GODS_LAWS.md`** em caso de conflito. **Leia o manual relevante ANTES de decidir**, não depois:

| Manual | Quando é obrigatório ler |
|---|---|
| `CONTRACT.md` | Antes de escrever, modificar ou revisar qualquer código (SOLID, camadas, clean code, regras C++) |
| `TESTES.md` | Antes de planejar ou executar teste, análise estática, fuzzing, sanitizer, auditoria de suíte |
| `AUDITORIAS.md` | Auditoria técnica: 10 capítulos ancorados nas leis deste projeto (dependência zero, camadas, ABI, os três contratos de versão, Wayland, isolamento de teste, licença, entrada hostil, portões de qualidade, dossiê pré-1.0); classifica achado em CRÍTICO/IMPORTANTE/COSMÉTICO |
| `AGILE.md` | Planejamento, cadência, ondas, priorização |
| `DEPLOY_CHECKLIST.md` | Qualquer operação irreversível (tag, release pública, rotação de chave) |
| `ORG.md`, `pipeline_release_1.0.md`, `lideranca_pipeline_release.md` | Quem lidera o quê, qual C-level ativar, as 12 fases |
| `TOOLING.md` | Qual ferramenta FOSS canônica usar por domínio antes de improvisar em shell |

Ao despachar um subagent, **inclua o caminho absoluto do manual no prompt da task** - subagents não herdam este contexto.

## Comandos

Comandos provados verdes em 21/08/2026 (FUND-3), na forma exata usada pelo job `linux` do CI (`.github/workflows/ci.yml`) para o modo shared (default). Para o modo estático, acrescente `-DBUILD_SHARED_LIBS=OFF` ao configurar.

- Configurar: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
- Compilar: `cmake --build build`
- Suíte completa: `ctest --test-dir build --output-on-failure`
- Um único teste: `ctest --test-dir build -R <nome> --output-on-failure` (ex.: `-R version_test`)
- Lint / análise estática: `<a definir>`

Sempre com `export TMPDIR=/var/tmp` antes de configurar/linkar nesta máquina (`/tmp` é tmpfs). No modo shared a suíte tem **9 casos** (`version_test`, `visibility_test`, `layers_test`, `consume_test`, `embed_test`, `install_includedir_test`, `install_packager_layout_test`, `output_name_test`, `no_target_collision_test`); no estático, **8** (os mesmos, menos `visibility_test` - só existe com `BUILD_SHARED_LIBS=ON` e sistema Unix, é `nm -D` numa `.a`, não faz sentido). Contagem medida em 22/08/2026 (`ctest --test-dir build` nos dois modos); a versão anterior desta linha dizia 3/2 e estava desatualizada desde a onda FIX-CONSUMO.

## Isolamento obrigatório de teste (teclado, mouse, tela)

**Ordem do líder, 21/08/2026: qualquer teste que toque a superfície dele - teclado, mouse ou tela - roda em Docker ou sandbox. Nunca na sessão viva.** Isto vale para toda a área que este projeto ocupa: janela, fullscreen, foco, iconify, captura de input, cursor, hotkey global, screenshot, GL na tela. Já houve incidente nesta máquina: uma sonda abriu janela na sessão viva por minutos, e um teste de input travou o touchpad do líder a ponto de exigir reboot.

Ferramental presente nesta máquina (verificado 21/08/2026):

| Ferramenta | Status |
|---|---|
| Docker | 29.7.2, daemon ativo, usuário no grupo `docker` (**sem `sudo`**) |
| distrobox | 1.8.2.5 |
| bubblewrap (`bwrap`) | 0.11.0 |
| podman | ausente |
| `kwin_wayland` | presente; **é o compositor de teste** (o mesmo da sessão do líder), tem `--virtual` (framebuffer headless) e `--socket <nome>` |
| `weston`, `sway`, `labwc` | ausentes |
| Xvfb, `xvfb-run`, injetor de input X11 | presentes no host, mas **PROIBIDOS aqui**: são X11, e o projeto é Wayland puro |
| injetor via `/dev/uinput` | presente e com acesso de escrita para o usuário, e por isso **PROIBIDO**: uinput injeta no kernel, ou seja **na sessão real do líder**, container ou não |
| `/dev/dri` | `card0`, `card1`, `renderD128`, `renderD129` |

Regras de execução:

1. **O container é a fronteira externa; o compositor vive dentro dele.** Suba `kwin_wayland` **dentro** do container (`--virtual` para headless de CI, `--socket <nome>` com `--width`/`--height` quando precisar de janela), e faça o app sob teste conectar nesse socket. Nada de Xvfb: é X11, e este projeto não tem backend X11.
2. **NUNCA monte no container o `XDG_RUNTIME_DIR` do host nem o socket `wayland-0` do host.** Montar devolve ao processo a superfície do líder e anula todo o isolamento. `wl_display_connect(NULL)` cai no nome embutido `wayland-0` sozinho e resolve dentro do `XDG_RUNTIME_DIR`; desfazer a variável de ambiente **não** protege, só a ausência do socket protege.
3. **NUNCA monte `/dev/uinput`, e não use injetor de input baseado nele.** uinput injeta evento no **kernel**, não numa sessão: o container não contém nada disso, e a tecla vai parar na sessão real do líder. Este é exatamente o caminho que já travou o touchpad dele. Input sintético de teste sai da **entrada virtual do compositor aninhado**, nunca do kernel.
4. **GPU só quando o teste exigir GL real.** Sem `--device /dev/dri`, o render sai por software (llvmpipe) e o resultado é reprodutível na matriz de CI. Passar `/dev/dri` é decisão consciente por teste, não default.
5. **Prove o isolamento antes de interagir**, não depois: `ss -xp` do processo sob teste deve apontar para o socket do compositor do container, e `lsof -p <pid>` não pode mostrar nenhum caminho do `XDG_RUNTIME_DIR` do host.
6. Quando o cenário não for testável ponta a ponta no ambiente disponível, **teste o seam de decisão e declare o downgrade**. Jamais venda teste de seam como e2e.
7. A mesma imagem de container usada localmente é a que roda na matriz de CI das cinco plataformas. Teste que só passa fora do container não conta como passando.

## Bus entre projetos: `gusworld_ia_autocomm`

Canal assíncrono entre as sessões do líder e o filho dele. Clone: `<vault>/gusworld_ia_autocomm/`, repo **privado** `petrinhu/gusworld_ia_autocomm`. **O protocolo canônico é o `PROTOCOL.md` do clone**; leia lá antes de usar, e trate a L-16 de `GODS_LAWS.md` como o resumo operacional.

| Slug | Projeto |
|---|---|
| `gusworld` | o jogo |
| **`glintfx`** | **este projeto** |
| `site` | `petrinhu/site_gusworld`, registro histórico do jogo |
| `mapeditor` | `petrinhu/gusworld_mapeditor` |
| Gus Dragon (`Dragon-Drv`) | colaborador humano; manda ideias por issue ou `.txt` |

Fluxo: ler o que está solto em `inbox/glintfx/`, agir, `git mv` para `inbox/glintfx/archive/`, commit `read: <arquivo>`, push. Enviar: um `.md` em `inbox/<destinatario>/` com frontmatter `de`/`para`/`assunto`/`data`, sempre depois de `git pull`. **Pedido vai sem classificação de prioridade** (quem recebe classifica).

Estado em 21/08/2026: `inbox/glintfx/` **vazia**, 28 mensagens no `archive/`. As mensagens antigas endereçadas ao `glintfx` são **deprecadas por ordem do líder** e não devem ser executadas: descrevem a biblioteca anterior (L-01). Existe um hook pronto no clone, `check-inbox-hook.sh <slug>`, que faz `pull` e injeta as não lidas no contexto.

**Divergência conhecida, não corrigida por mim:** o `PROTOCOL.md` descreve o `glintfx` como "a lib glintfx (RmlUi/GL)". Isso não vale mais: o projeto é do zero, Wayland puro, dependência zero. O arquivo é compartilhado com as outras sessões; a correção é decisão do líder.

## Restrições desta máquina que mordem este projeto

Estas não são conselhos genéricos; são armadilhas medidas nesta máquina, e um framework 2D bate em todas elas.

- **Build pesado vai para `/var/tmp`, com `export TMPDIR=/var/tmp`.** `/tmp` aqui é tmpfs (sai da RAM); build C++ grande enche o tmpfs e o link falha com "no space on device".
- **Espaço em disco se mede com `btrfs filesystem usage /`** (sem `sudo`), lendo `Device unallocated` e o `min` de `Free (estimated)`. O `df` mente em btrfs.
- **Teste que toca teclado, mouse ou tela roda em container, com compositor Wayland aninhado dentro dele.** Ver a seção "Isolamento obrigatório de teste" acima; é regra, não preferência. Injetor de input X11 ou de kernel está fora.
- **Verificação de entregável visual é do `qa-engineer`**, independente de quem implementou, e o orquestrador reconfere o relatório do QA.

## Pendências

A tabela de pendências e planejamento do projeto está em `TODO.md` na raiz: **92 itens em 13 ondas** (contado em 22/08/2026 por `grep -cE '^\| [0-9]' TODO.md`; eram 57 em 21/08/2026 - os 57 originais mais **21 fatias do motor de RCSS** e **14 fatias do mecanismo de mapa**, acrescentadas nesse mesmo dia), no schema de **10 colunas** da skill `tab_pendencias`, com **`WSJF` como primeira coluna**. As linhas estão na ordem de execução, e a coluna `Onda` marca os passos paralelizáveis. As parcelas do scoring que a coluna não carrega (valor, criticidade, redução de risco, CoD e tamanho) ficam em `/var/tmp/glintfx-plan/lente-produto.md`, do `product-manager`. Itens com porta de mão única trazem na descrição **o que congelam** e a exigência de revisão de API dedicada.

A trilha de mapa (14 fatias, grupo `Mapa`) está **desenhada e pontuada, com as seis decisões de formato fechadas**, mas nenhuma fatia está disponível para pull agora: o slot único de trilha paralela da L-32 é do **RCSS** hoje, por decisão do líder em 22/08/2026 ("rcss primeiro" - contra a recomendação do CPO/CTO, que apontavam o mapa pelo agregado, mas dentro de um espaço que o número deixava genuinamente aberto). As 14 linhas usam o status `💡 Decisão tomada` para marcar isso, e liberam quando o RCSS fechar (onda W10) ou o líder decidir de outra forma - ver a seção "A trilha de mapa" do próprio `TODO.md`.

## Nota sobre o predecessor (não é este projeto)

Existiu uma biblioteca homônima em `github.com/petrinhu/glintfx`, cuja árvore local foi descartada em 21/08/2026. **Ela não é base, referência nem canon deste repositório** - o líder determinou início do zero. Esta nota existe só para que a próxima sessão não gaste tempo investigando o achado, como já aconteceu uma vez.
