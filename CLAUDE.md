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

---

## O que é o GlintFx

Biblioteca/framework **2D completo e reutilizável** em C++23. O consumidor (outro projeto) obtém janela, loop principal, render 2D, input, gamepad, áudio, fonte, carregamento de asset e math2d, e escreve apenas a lógica dele. O entregável é a **API pública + headers + pacote CMake**, não um binário final.

## Estado atual do repositório (21/08/2026)

**Projeto do zero.** O que existe hoje na raiz:

- 9 arquivos `.md`: os manuais canônicos do vault (ver "Autoridade documental" abaixo).
- Repositório git inicializado, **zero commits**, **sem remoto**.
- **Nenhum código, nenhum CMake, nenhum teste.**

Logo: não existe comando de build, lint ou teste para documentar aqui ainda. A seção "Comandos" abaixo é preenchida no mesmo commit em que o `CMakeLists.txt` nascer. Não invente comandos; se você precisa de um e ele não está listado, ele não existe.

## Decisões fechadas pelo líder (21/08/2026)

Estas foram tomadas explicitamente via AskUserQuestion e são o ponto de partida do projeto. Mudança em qualquer uma delas é decisão do líder, não de agente.

| Eixo | Decisão |
|---|---|
| Natureza | Biblioteca/framework reutilizável (não aplicação final) |
| Domínio | Framework 2D completo (janela, loop, render2d, input, gamepad, áudio, fonte, asset, math2d) |
| Linguagem e build | **C++23 + CMake** |
| Plataformas | **Fedora, Ubuntu, CachyOS, Arch, Windows**. No Linux, **apenas Wayland** |
| Dependências | **Zero além da stdlib e da API do SO** |
| Licença e visibilidade | **Público no GitHub, AGPL-3.0** |

### Plataformas: CachyOS é alvo próprio

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
| `AUDITORIAS.md` | Auditoria técnica (a Parte I é C++; classifica achado em CRÍTICO/IMPORTANTE/COSMÉTICO) |
| `AGILE.md` | Planejamento, cadência, ondas, priorização |
| `DEPLOY_CHECKLIST.md` | Qualquer operação irreversível (tag, release pública, rotação de chave) |
| `ORG.md`, `pipeline_release_1.0.md`, `lideranca_pipeline_release.md` | Quem lidera o quê, qual C-level ativar, as 12 fases |
| `TOOLING.md` | Qual ferramenta FOSS canônica usar por domínio antes de improvisar em shell |

Ao despachar um subagent, **inclua o caminho absoluto do manual no prompt da task** - subagents não herdam este contexto.

## Comandos

*(A preencher quando o build existir. Cada comando aqui deve ter sido executado com sucesso por quem o escreveu.)*

- Configurar: `<a definir>`
- Compilar: `<a definir>`
- Suíte completa: `<a definir>`
- Um único teste: `<a definir>`
- Lint / análise estática: `<a definir>`

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

## Restrições desta máquina que mordem este projeto

Estas não são conselhos genéricos; são armadilhas medidas nesta máquina, e um framework 2D bate em todas elas.

- **Build pesado vai para `/var/tmp`, com `export TMPDIR=/var/tmp`.** `/tmp` aqui é tmpfs (sai da RAM); build C++ grande enche o tmpfs e o link falha com "no space on device".
- **Espaço em disco se mede com `btrfs filesystem usage /`** (sem `sudo`), lendo `Device unallocated` e o `min` de `Free (estimated)`. O `df` mente em btrfs.
- **Teste que toca teclado, mouse ou tela roda em container, com compositor Wayland aninhado dentro dele.** Ver a seção "Isolamento obrigatório de teste" acima; é regra, não preferência. Injetor de input X11 ou de kernel está fora.
- **Verificação de entregável visual é do `qa-engineer`**, independente de quem implementou, e o orquestrador reconfere o relatório do QA.

## Nota sobre o predecessor (não é este projeto)

Existiu uma biblioteca homônima em `github.com/petrinhu/glintfx`, cuja árvore local foi descartada em 21/08/2026. **Ela não é base, referência nem canon deste repositório** - o líder determinou início do zero. Esta nota existe só para que a próxima sessão não gaste tempo investigando o achado, como já aconteceu uma vez.
