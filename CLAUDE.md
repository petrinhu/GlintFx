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
| ⚖ | escrever função, arquivo, classe ou módulo novo, ou revisar fatia | L-17 proibido monolito **e** proibida fragmentação; números duros mais as cinco perguntas do revisor |
| ⚖ | ir executar qualquer trabalho de produto | L-18 main só orquestra; fable audita e cria, sonnet implementa |
| ⚖ | criar módulo, tocar a fronteira do SO, desenhar API pública | L-19 camadas, portas em compile-time, fronteira opaca |
| ⚖ | escrever qualquer código com comportamento | L-20 TDD estrito, vermelho antes de verde |
| ⚖ | nomear qualquer coisa, escrever comentário, commit **ou documento** | L-21 inglês e `snake_case` no código, commit em pt-br; **documento se decide pelo LEITOR** |
| ⚖ | projetar assinatura pública ou tratar erro | L-22 nenhuma exceção cruza a API pública |
| ⚖ | commitar, fechar uma fatia, ou dar push | L-23 portões de qualidade e `preci.sh` |
| ⚖ | pensar em cobertura, formatação ou auditoria | L-24 sem meta de cobertura, clang-format LLVM, dossiê na 1.0 |
| ⚖ | iniciar build pesado, ASan, teste de janela ou demo | L-25 armar o `watchcode` na janela e desarmar ao fim |
| ⚖ | criar tag, publicar release, ou mexer na versão | L-26 versão e tag `vA.B.C.D`, `SOVERSION` segue o `A` |
| ⚖ | escrever ordem de serviço, ou aceitar corte proposto por agente | L-27 fato separado de inferência; corte exige citação |
| ⚖ | tocar folha de estilo, tema, layout de UI ou parser de estilo | L-28 motor **`gfui`**, folha **`gfss`**, marcação **`gfml`**; **legibilidade humana é requisito — verboso vence curto**; em casa, sem RmlUi |
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
| ⚖ | nomear artefato de saída, extensão de arquivo ou formato próprio | L-38 dado é nosso e pode ter extensão própria; binário usa `.so` e `.dll` |
| ⚖ | ver QUALQUER coisa vinda do Gus Dragon, em qualquer canal | L-39 é prioridade e é SEMPRE respondida; o ack interrompe a onda |
| ⚖ | escrever, revisar ou confiar em qualquer portão de qualidade | L-40 piso de varredura não-vazia: contou zero, reprova |

---

## O que é o GlintFx

Biblioteca/framework **2D completo e reutilizável** em C++23. O consumidor (outro projeto) obtém janela, loop principal, render 2D, input, gamepad, áudio, fonte, carregamento de asset e math2d, e escreve apenas a lógica dele. O entregável é a **API pública + headers + pacote CMake**, não um binário final.

## Estado atual do repositório

> **Regra de manutenção desta seção, para o próximo editor tropeçar nela ANTES de violá-la:** esta seção **não carrega contador**. Todo número que muda por fatia (commits, casos de teste, arquivos rastreados, estado do remoto, resultado do CI) entra como **o comando que o mede**, nunca como o valor medido — comando não apodrece, número escrito sim. **Fato estrutural** (o que existe e por quê, não quanto) continua em prosa, datado. Esta regra existe porque a versão anterior (22/08/2026) gravava número, e uma verificação de 25/08/2026 reprovou o item `DOC-ESTADO` por isso: a seção dizia 56 commits/9 casos de teste quando o real já era 102/15 — o próprio item criado para consertar documentação obsoleta ficou obsoleto em 3 dias.

**Voláteis — meça, não leia:**

| O quê | Comando |
|---|---|
| Commits locais | `git rev-list --count HEAD` |
| Estado do remoto | `git ls-remote origin main` comparado com `git rev-parse HEAD` |
| Commits ainda não empurrados | `git log origin/main..HEAD --oneline` |
| Casos de teste por modo (shared/estático) | `ctest --test-dir build -N` e `ctest --test-dir build-static -N` |
| Arquivos rastreados | `git ls-files` |
| Camadas e backends que existem | `ls src/` e `ls src/platform/` |
| Resultado do último CI pushado | `gh run list --limit 1` (jobs de um run: `gh run view <id> --json jobs`) |

**Fatos estruturais (confirmados em 25/08/2026, re-conferidos contra a árvore, não de memória):**

- **Harness de teste próprio** em `tests/harness/` (`check.hpp`/`.cpp`, macro `GLINTFX_CHECK`; `test_registry.hpp`/`.cpp`, macro `GLINTFX_TEST`). Existe porque a **L-07 proíbe Catch2 e GoogleTest** — são dependência de terceiro, e a lib tem dependência zero.
- **Camadas (L-19):** quais existem hoje, meça com `ls src/` (o backend ativo, com `ls src/platform/`) — `core/` (núcleo puro, não toca o SO) e `platform/` (a única camada que toca o SO). Dentro de `platform/`, o único backend é `platform/wayland/`; o próprio `src/platform/CMakeLists.txt` documenta que um backend novo (ex.: Windows) nasce como `add_subdirectory()` irmão, nunca misturado ao Wayland. `platform/wayland/` hoje só liga o binding gerado do `xdg-shell` ao alvo da lib (WL-PROTO) — nenhum código de conexão ou de evento ainda existe (isso é WL-DISPLAY, fatia futura).
- **`cmake/`:** um arquivo por assunto — opções, flags de compilação, definição do alvo da lib, instalação/empacotamento, harness de teste, geração do binding Wayland via `wayland-scanner`, template do `.pc` para empacotador externo. Quantidade cresce a cada fatia de infraestrutura; meça com `ls cmake/`.
- **`tests/tools/`:** gates de shell registrados como `add_test` condicionais, cobrindo consumo instalado, embutido (embed), exports, layout de instalação, violação de camada, colisão de nome de alvo CMake, nome do artefato de saída, cobertura de higiene de header público e o `.pc` gerado. Quantidade cresce a cada gate novo; meça com `ls tests/tools/`.
- **`tests/container/`:** nasceu na onda W1 (item `TEST-WLCONT`) para cumprir a L-09 — sobe `kwin_wayland` **dentro** de um container Docker, com `check_isolation.sh` provando, por `docker inspect`, que nenhuma montagem do host (`XDG_RUNTIME_DIR`, socket `wayland-0`, `/dev/uinput`) atravessa a fronteira, sempre antes de qualquer interação com o compositor.
- **Pin do alvo primário (L-04):** a imagem do job da matriz `linux` chamado `Fedora 44 (primario)` está fixada em `fedora:44`, nunca `:latest`.
- **Os portões que a onda W1 acrescentou (L-23):** `-Werror`/`/WX` ligado nos dois modos (shared e estático) dos jobs Linux e Windows via `-DGLINTFX_WERROR=ON`; três jobs de CI dedicados — `lint` (clang-format, clang-tidy, cppcheck), `sanitizer` (ASan/UBSan) e `gitleaks` (scan de segredo, `fetch-depth 0`, histórico inteiro) — todos rodando só no alvo primário (Fedora 44); e o script local `tools/preci.sh`, espelho do CI, com os modos `--fast`, `--lint-only`, `--sanitizer-only` e `--selftest` (este último provado contra fixtures em `tests/preci_fixtures/`, não contra a árvore real).
- **Os quatro jobs acrescentados pela onda W1 (`lint`, `sanitizer`, `gitleaks`, `wayland-container`) estrearam no GHA real em 25/08/2026, e um deles quebrou na estreia.** O `sanitizer` morreu no link com `cannot find libasan.so`: no Fedora, `libasan`/`libubsan` são pacotes **separados** do `gcc-c++`, e o job nunca os instalava — **passava localmente porque a máquina do líder os tem**. Consertado e verde. **A regra que fica, e vale para todo job novo:** portão que nunca rodou no ambiente real **não é portão** — o verde que ele exibe é o de rodada local, e rodada local compartilha a máquina com quem o escreveu. **Antes de confiar num job, compare as duas listas:** a local (leia o `.github/workflows/ci.yml`) contra a da última execução empurrada (`gh run view $(gh run list --limit 1 --json databaseId -q '.[0].databaseId') --json jobs -q '.jobs[].name'`). Se a segunda for menor, o que falta ainda não foi exercido.
- **Quarto componente de versão (item `VER-4C`, L-26):** `struct version` em `include/glintfx/core/version.hpp` ganhou `tweak_version` como quarto e último campo, casando com a tag `vA.B.C.D`. Reabertura de layout aceita só porque o projeto é pré-1.0 (`SOVERSION` 0, sem consumidor externo conhecido).

A seção "Comandos" abaixo continua real; os comandos foram reexecutados em 25/08/2026 e continuam funcionando (configure + build + ctest, shared e estático, `exit 0` nos três).

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
| `docs/api-conventions.md` | Antes de fechar qualquer revisão de API dedicada (PMU) - as sete regras que CORE-ERROR estabeleceu (CE-8), cada uma com o teste que a prova |

Ao despachar um subagent, **inclua o caminho absoluto do manual no prompt da task** - subagents não herdam este contexto.

## Comandos

Comandos provados verdes em 21/08/2026 (FUND-3), na forma exata usada pelo job `linux` do CI (`.github/workflows/ci.yml`) para o modo shared (default). Para o modo estático, acrescente `-DBUILD_SHARED_LIBS=OFF` ao configurar.

- Configurar: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
- Compilar: `cmake --build build`
- Suíte completa: `ctest --test-dir build --output-on-failure`
- Um único teste: `ctest --test-dir build -R <nome> --output-on-failure` (ex.: `-R version_test`)
- Lint / análise estática: `<a definir>`

Sempre com `export TMPDIR=/var/tmp` antes de configurar/linkar nesta máquina (`/tmp` é tmpfs). **Número de casos por modo não é fixo aqui** — meça com `ctest --test-dir build -N` (shared) e `ctest --test-dir build-static -N` (estático); os dois modos diferem só por `visibility_test`, que exige `BUILD_SHARED_LIBS=ON` em Unix (é `nm -D` numa `.a`, não faz sentido no modo estático). Esta linha já gravou "3/2" e depois "9/8" como se fossem constantes; as duas versões apodreceram em dias — não repita o erro.

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

A tabela de pendências e planejamento do projeto está em `TODO.md` na raiz. **A contagem de itens não é fixa aqui** — meça com `grep -cE '^\| [0-9]' TODO.md`; a proveniência de cada faixa (os itens originais da fundação, mais as fatias do motor de RCSS e do mecanismo de mapa acrescentadas em 21/08 e 22/08/2026) está documentada, com data, na seção "Proveniência (L-27)" do próprio `TODO.md` — link vale mais que número copiado, porque a proveniência não muda a cada fatia fechada e o total muda. Schema de **10 colunas** da skill `tab_pendencias`, com **`WSJF` como primeira coluna**. As linhas estão na ordem de execução, e a coluna `Onda` marca os passos paralelizáveis. As parcelas do scoring que a coluna não carrega (valor, criticidade, redução de risco, CoD e tamanho) ficam em `/var/tmp/glintfx-plan/lente-produto.md`, do `product-manager`. Itens com porta de mão única trazem na descrição **o que congelam** e a exigência de revisão de API dedicada.

A trilha de mapa (14 fatias, grupo `Mapa`) está **desenhada e pontuada, com as seis decisões de formato fechadas**, mas nenhuma fatia está disponível para pull agora: o slot único de trilha paralela da L-32 é do **RCSS** hoje, por decisão do líder em 22/08/2026 ("rcss primeiro" - contra a recomendação do CPO/CTO, que apontavam o mapa pelo agregado, mas dentro de um espaço que o número deixava genuinamente aberto). As 14 linhas usam o status `💡 Decisão tomada` para marcar isso, e liberam quando o RCSS fechar (onda W10) ou o líder decidir de outra forma - ver a seção "A trilha de mapa" do próprio `TODO.md`.

## Nota sobre o predecessor (não é este projeto)

Existiu uma biblioteca homônima em `github.com/petrinhu/glintfx`, cuja árvore local foi descartada em 21/08/2026. **Ela não é base, referência nem canon deste repositório** - o líder determinou início do zero. Esta nota existe só para que a próxima sessão não gaste tempo investigando o achado, como já aconteceu uma vez.
