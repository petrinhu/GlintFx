# GODS_LAWS.md

> Ordens expressas do líder (petrus). Este arquivo **não é declaração, é execução**: cada lei tem um **gatilho**, e o gatilho é conferido **no momento da ação**, não no fim.

## Protocolo de uso (obrigatório)

1. **Antes de agir**, varra a coluna "Gatilho" da tabela abaixo. Se algum gatilho casa com o que você está prestes a fazer, leia a lei inteira antes do primeiro comando, não depois.
2. **Ao despachar subagent**, cole no prompt da task o texto completo das leis cujo gatilho casa com aquela task, mais o caminho absoluto deste arquivo. Subagent **não herda** este contexto e não vai ler por conta própria.
3. **Ao relatar ao líder**, se você tocou uma área com lei, diga qual lei aplicou e como. Silêncio não é prova de conformidade.
4. **Lei nova entra aqui no instante em que o líder a dá**, com data e o texto dele verbatim entre aspas. Não espere "um momento melhor" para registrar.
5. **Nenhum agente revoga, flexibiliza ou reinterpreta lei.** Só o líder. Na dúvida sobre o alcance de uma lei, pergunte via `AskUserQuestion` antes de agir.
6. Conflito entre uma lei daqui e qualquer outro documento (manual, memória, hábito, preferência do agente): **a lei daqui vence**.

## Índice de gatilhos

| Lei | Gatilho: dispara quando você vai... | Resumo |
|---|---|---|
| [L-01](#l-01) | procurar prior art, base de código ou "como era antes" | Projeto é do zero; o glintfx anterior não é base |
| [L-02](#l-02) | desenhar API, escopo ou entregável | É biblioteca reutilizável, não aplicação final |
| [L-03](#l-03) | criar build, escolher padrão de linguagem | C++23 + CMake |
| [L-04](#l-04) | escrever CI, declarar suporte de plataforma | Cinco alvos; Fedora 44 primário; CachyOS é próprio, não Arch |
| [L-05](#l-05) | tocar janela, input, display ou exemplo de internet | Linux é Wayland puro, sem X11 |
| [L-06](#l-06) | implementar teclado, keymap, texto digitado | Parser XKB próprio; `libxkbcommon` está fora |
| [L-07](#l-07) | adicionar dependência, `FetchContent`, vendorizar | Zero além de stdlib e API do SO |
| [L-08](#l-08) | criar repo, `LICENSE`, cabeçalho de arquivo, publicar | Público no GitHub, AGPL-3.0 |
| [L-09](#l-09) | rodar teste que abre janela, injeta input ou captura tela | Container com compositor Wayland dentro; nunca a sessão viva |
| [L-10](#l-10) | escolher entre duas abordagens, decidir design ou arquitetura | Opções ao líder via `AskUserQuestion`, sem painel lateral |
| [L-11](#l-11) | `git push`, merge em `main`, criar tag, publicar release | Push por onda completa; merge e tag só com aval no contexto |
| [L-12](#l-12) | escrever ou revisar código de produto | Agente especialista executa; implementer, reviewer e orquestrador são distintos |
| [L-13](#l-13) | escrever qualquer mensagem ao líder | Timestamp real `[DD/MM/YY - HH:MM:SS]` obtido do `date` |
| [L-14](#l-14) | instalar, remover ou atualizar pacote de sistema | Pedir autorização; não instalar sozinho |
| [L-15](#l-15) | fechar um marco ou notar a hora | Nunca mandar o líder descansar, dormir ou parar |
| [L-16](#l-16) | abrir sessão, precisar de algo de outro projeto, ou receber ideia do Gus | Bus `gusworld_ia_autocomm`: como ler, enviar e responder |
| [L-17](#l-17) | escrever função, arquivo, classe ou módulo novo | Proibido monolito; cada função é um átomo |
| [L-18](#l-18) | ir executar qualquer trabalho de produto | Main só orquestra; agentes bigtech; fable audita e cria, sonnet implementa |

---

## L-01

**Data:** 21/08/2026. **Verbatim:** *"esse projeto é do zero"*.

O `Projects/GlintFx` nasce sem herança. Existiu uma biblioteca homônima em `github.com/petrinhu/glintfx`, cuja árvore local foi descartada em 21/08/2026: **não é base, não é referência, não é canon**. Não a clone, não a restaure da lixeira, não copie trecho dela, não a cite como "o que já funcionava".

**Aplicação:** ao encontrar rastro do predecessor (lixeira, remoto, memória de outra sessão), pare a investigação e siga do zero. Uma sessão já perdeu tempo escavando isso.

## L-02

**Data:** 21/08/2026, via `AskUserQuestion`.

O GlintFx é **biblioteca/framework reutilizável**, não aplicação final. O entregável é a **API pública, os headers e o pacote CMake**. Domínio: framework 2D completo (janela, loop, render2d, input, gamepad, áudio, fonte, asset, math2d); o consumidor escreve só a lógica dele.

**Aplicação:** toda decisão de API se julga pelo consumidor externo, não pela conveniência interna. Regra de aplicação específica nunca entra na lib.

## L-03

**Data:** 21/08/2026, via `AskUserQuestion`.

**C++23 + CMake.** Sem exceção de linguagem sem ordem do líder.

## L-04

**Data:** 21/08/2026, verbatim: *"Fedora, Ubuntu, CachyOs (proprio, nao um arch renomeado), Arch, Windows"* e, no mesmo dia, *"nosso OS principal é o fedora, na versao que eu uso"*.

Cinco alvos, **cinco entradas distintas na matriz de CI**. **Fedora 44 é o alvo primário**, por ser o sistema do líder: no CI a imagem fica **pinada em `fedora:44`**, nunca em `:latest`, para o alvo primário falhar quando a máquina dele falharia. Ao atualizar a versão dele, o pin sobe junto. **CachyOS não é Arch renomeado** e não é coberto pelo job de Arch: toolchain, flags de otimização, kernel e empacotamento diferem.

**Aplicação:** verde no Arch **não** autoriza declarar CachyOS suportado. Declaração de suporte exige job próprio verde.

## L-05

**Data:** 21/08/2026, verbatim: *"no linux, usaremos apenas camada wayland, sem x11"*.

Sem backend X11, sem fallback por XWayland, sem Xlib, XCB, XTest ou framebuffer X11 no repositório, nem em produção nem em teste. Usuário em sessão X11 **não é público alvo**, por desenho.

**Aplicação:** exemplo de internet que usa X11 não serve aqui, nem "só para o teste". Se a única forma conhecida de fazer algo é X11, isso é assunto para o líder, não contorno.

## L-06

**Data:** 21/08/2026, verbatim: *"usaremos nosso parser de keymap proprietario"*.

O compositor entrega o keymap em texto XKB por file descriptor. **Nós escrevemos o parser.** `libxkbcommon` está fora, mesmo instalado na máquina.

**Aplicação:** escopo mínimo do parser, para não subestimar a fatia: keycode para keysym, níveis e grupos de modificador, latch e lock, sequência de compose e tecla morta, keysym para UTF-8.

## L-07

**Data:** 21/08/2026, via `AskUserQuestion`.

**Zero dependência além da biblioteca padrão de C++23 e das APIs do sistema operacional.** Sem gerenciador de pacote de terceiros, sem `FetchContent` de biblioteca externa, sem vendorizar.

Fronteira registrada: `libwayland-client` conta como API do sistema (mesma categoria de Win32); `libxkbcommon` **não** conta (ver L-06). Mudança nessa fronteira é do líder.

**Aplicação:** decode de imagem, rasterização de fonte, loader de GL, mixagem de áudio e decode de gamepad são escritos em casa. Ao topar com uma lacuna, **pare e pergunte**; não improvise dependência "só por enquanto".

## L-08

**Data:** 21/08/2026, via `AskUserQuestion`.

Repositório **público no GitHub, licença AGPL-3.0**.

**Aplicação:** repo público significa que dado sensível se verifica no **histórico**, não na árvore (`git log --all -p | grep -ci <termo>`), e que screenshot ou frame derivado de captura de tela do líder passa por conferência antes de virar asset versionado.

## L-09

**Data:** 21/08/2026, verbatim: *"qualquer teste que toque minha superficie (teclado, mouse, tela) deve ser feito em docker/sandbox"*.

Cobre janela, fullscreen, foco, iconify, captura de input, cursor, hotkey global, screenshot e GL na tela. **Nunca na sessão viva do líder.** Já abriu janela na sessão dele por minutos e já travou o touchpad a ponto de exigir reboot.

**Aplicação:** o container é a fronteira externa e o compositor vive dentro dele (`kwin_wayland --virtual` para headless, `--socket <nome>` com dimensões quando precisar de janela). Três proibições absolutas: não montar o `XDG_RUNTIME_DIR` nem o socket `wayland-0` do host; **não montar `/dev/uinput` nem usar injetor baseado nele** (uinput injeta no kernel, não numa sessão, e a tecla chega na sessão real do líder mesmo dentro do container); nada de X11 (L-05). Provar o isolamento **antes** de interagir. Detalhe operacional em `CLAUDE.md`, seção "Isolamento obrigatório de teste".

## L-10

**Data:** regra permanente do líder, reafirmada neste projeto.

**Nenhum agente decide design ou arquitetura sozinho.** Diante de dúvida ou de mais de uma opção viável, apresentar 2 a 3 alternativas com prós, contras, impacto e esforço, e perguntar via `AskUserQuestion`, com a recomendada primeiro.

**Aplicação:** `AskUserQuestion` **sem painel lateral**, ou seja **sem o campo `preview`**; só `label` e `description`. Detalhe técnico longo vai no corpo da mensagem de chat, antes ou depois da pergunta. Decisão trivial e reversível com default óbvio segue o default e é informada.

**Dever de contra-argumentar:** se uma decisão do líder for destrutiva, violar princípio do projeto ou inviabilizar marco, o agente nomeia o problema, explica o risco concreto, propõe alternativa e devolve a decisão a ele. Silêncio passivo é má prática. Reafirmada a ordem, executa por inteiro.

## L-11

**Data:** regra permanente. **Verbatim:** *"commite cada fatia e push cada onda completada"*.

**Commit local** a cada fatia entregue, citando o ID do item na mensagem. **Push** quando a onda fecha, depois do review e com os gates verdes. **Merge em `main` por PR e criação de tag continuam exigindo aval explícito no contexto.**

**Aplicação:** a mensagem do `push` mente; confirme o SHA no remoto por `git ls-remote <url> <branch>` sempre que o push importar. Confira também `git diff --cached --stat` antes de commitar e `git show --stat` depois: `git add` é atômico e um pathspec inválido derruba o add inteiro em silêncio.

## L-12

**Data:** regra permanente.

**Toda alteração de produto ou de código é feita por agente especialista, nunca inline pelo orquestrador.** Implementer, reviewer e orquestrador são **agentes distintos**. O review adversarial **executa** o código, não só lê. O orquestrador **re-verifica** o entregável (build limpo, spot-check das afirmações arquivo:linha) antes de aceitar: relatório de agente não é prova.

**Aplicação:** verificação de entregável **visual ou renderizado** é do `qa-engineer`, independente do implementer, nunca do orquestrador inline e nunca do líder. Ao verificar trabalho de outro agente, leia o **blob commitado** (`git show <sha>:<arquivo>`), não a árvore de trabalho, que pode estar sob mutation testing.

## L-13

**Data:** regra permanente. **Verbatim:** *"você está esquecendo do timestamp nas suas mensagens. Formato [DD/MM/YY - HH:MM:SS]"*.

**Toda** mensagem ao líder começa com `[DD/MM/YY - HH:MM:SS]`. A hora tem de ser **real**, obtida de `date '+%d/%m/%y - %H:%M:%S'` a cada mensagem.

**Aplicação:** nunca estimar, inferir da conversa ou reaproveitar o timestamp anterior. Para situar evento no tempo, consultar fonte com data real (`git log --date=...`, journal, mtime), nunca a impressão de recência.

## L-14

**Data:** regra permanente.

**Agente não instala, remove ou atualiza pacote de sistema por conta própria.** Pedir autorização ao líder. Recusar e reportar "não executado" é resultado negativo honesto e vale mais que improvisar.

**Aplicação:** autorizado, rodar `--assumeno` antes para mostrar a transação, e conferir o resultado depois. Autorização para uma instalação **não** vale para a próxima.

## L-15

**Data:** regra permanente. **Verbatim:** *"eu tenho 46 anos, nao preciso de babá me mandando dormir"*.

**Nunca** dizer ao líder para descansar, dormir ou parar, nem comentar a hora dele, nem sugerir pausa pela hora. A cadência de trabalho é decisão dele.

**Aplicação:** ao fechar um marco, oferecer o próximo passo de forma neutra e simétrica ("sigo para X ou paramos?"), sem inclinar para a pausa. Se ele manda a próxima tarefa, continuar sem re-oferecer pausa.

## L-16

**Data:** 21/08/2026. **Verbatim:** *"a pasta [...] gusworld_ia_autocomm e o repo https://github.com/petrinhu/gusworld_ia_autocomm [...] serve de bus entre este projeto, GusWorld, Gus Dragon (meu filho [...] ele se comunica nesse bus via issues), o site de registro historico [...] e o gusworld_mapeditor"*.

O bus é o canal assíncrono entre as sessões do líder e o filho dele. Clone canônico: `~/IDrive/Documentos/projetos_claudebrain/gusworld_ia_autocomm/`. Repositório **privado**. Protocolo completo em `PROTOCOL.md` do clone; **leia o protocolo, não confie neste resumo**.

**Participantes.** Slugs de sessão: `gusworld` (o jogo), **`glintfx` (este projeto)**, `site` (a revista de registro histórico), `mapeditor` (o editor de mapas). Mais **Gus Dragon**, colaborador humano, filho do líder, handle GitHub `Dragon-Drv`, que manda ideias de jogo por **issue** ou por `.txt` na raiz de `inbox/`.

**Aplicação, leitura:** ao abrir a sessão ou quando o líder mandar, `git pull` no clone e ler o que estiver solto em `inbox/glintfx/`. Depois de ler e agir, `git mv` para `inbox/glintfx/archive/`, commit `read: <arquivo>`, push. A pasta `archive/` do topo do repo é convenção antiga, abandonada; não mande nada para lá.

**Aplicação, envio:** um arquivo `.md` por mensagem em `inbox/<destinatario>/`, nome `AAAAMMDD-HHMM-<de>-<slug-curto>.md`, com frontmatter `de`, `para`, `assunto`, `thread` opcional e `data`. Sempre `git pull` antes de enviar. Commit `msg: <de>-><para>: <assunto>`, push.

**Proibido classificar prioridade do outro** (ordem do líder, 03/08/2026): pedido pelo bus vai **sem** "urgente", "para agora", "quando der", "sem pressa", "bloqueia X". Quem recebe é quem enxerga o próprio roadmap e classifica. Entra na mensagem **o quê** se precisa, **para quê**, e fato datado quando houver. Duas exceções: o campo `prioridade:` que o **próprio Gus** põe na ideia dele, e **aviso operacional** (quebra, armadilha, correção de fato publicado), que não disputa fila.

**Ideia do Gus, o pipe completo:** (1) absorver; (2) **ack imediato e automático** na issue marcando `@Dragon-Drv`, sem esperar o líder, para a criança não ficar sem resposta; (3) discutir viabilidade e efeito dominó **com o líder**; (4) postar o resultado na issue, automático, **sempre honesto** e adequado a uma criança de 11 anos, sem inventar nada além do decidido; (5) arquivar, fechando a issue ou movendo o `.txt` para `respondidas_do_gus/`. Ideia do Gus entra na próxima onda, sem atropelar o que está em execução. **Nunca minta para ele.**

**Nunca versionar** nome de batismo de menor nem segredo, mesmo em repo privado. O filho do líder aparece só como **"Gus Dragon"**, e esse apelido pode ser citado em público.

## L-17

**Data:** 21/08/2026. **Verbatim:** *"projeto com proibicao de monolitos. Cada funcao é um átomo"*.

**Monolito é proibido em todo nível:** função que faz mais de uma coisa, arquivo que reúne assuntos sem relação, classe que cresce até virar dona de tudo, módulo sem fronteira declarada. **Cada função é um átomo:** faz **uma** coisa, inteira, e o nome dela diz exatamente qual.

**Aplicação, no momento de escrever:**

- Se você consegue extrair uma sub-função com **nome próprio e honesto**, ela não era um átomo. Extraia.
- Se o nome precisa de **"e"** para ser verdadeiro (`carrega_e_valida`, `parse_and_render`), são duas funções.
- Se um trecho precisa de comentário explicando **o que** faz, esse trecho é uma função sem nome. Dê o nome. Comentário existe para o **porquê**.
- Limites já normativos em `CONTRACT.md` §6.2, que esta lei torna inegociáveis: **no máximo 40 linhas** por função, **no máximo 4 parâmetros** (agrupe em struct), **no máximo 3 níveis** de aninhamento, com retorno antecipado em vez de aninhar.
- Arquivo é átomo de assunto: um assunto por arquivo, e o nome do arquivo diz qual.
- Módulo novo nasce **estreito**, com fronteira declarada. Assunto novo vira módulo próprio, nunca é acrescentado a um struct ou a uma classe existente porque "cabia lá".

**Aplicação, no momento de revisar:** função longa, arquivo com dois assuntos ou classe que virou dona de tudo são **achado de revisão**, não questão de gosto. O reviewer nomeia o átomo que faltou ser extraído.

**Aplicação, ao briefar agente:** esta lei entra no prompt de qualquer task de implementação. Agente que entrega monolito teve a task cumprida pela metade.

**O que a lei não é:** licença para fatiar em funções de uma linha sem sentido próprio, nem para criar helper genérico especulativo. `CONTRACT.md` §6 continua valendo: helper genérico só com **três ocorrências reais**. Átomo é a menor unidade **com significado**, não a menor unidade possível.

## L-18

**Data:** 21/08/2026. **Verbatim:** *"a partir de agora, é regra, decore: main apenas orquestra, delega agentes, avalia retorno de agentes. Usa apenas agentes bigtech. Auditoria e criacao de projetos: clevel fable sempre. Implementadores sonnet."*

**A thread principal (`main`) não executa trabalho de produto.** Ela faz exatamente três coisas: **orquestra**, **delega a agentes** e **avalia o retorno dos agentes**. Escrever código, escrever documento de produto, auditar, testar: nada disso é do `main`.

**Aplicação, quem chamar:**

| Tipo de trabalho | Agente | Modelo |
|---|---|---|
| Auditoria | C-level da constelação bigtech | **`fable`, sempre** |
| Criação de projeto | C-level da constelação bigtech | **`fable`, sempre** |
| Implementação | agente operacional bigtech (`backend-engineer`, `frontend-engineer`, `qa-engineer`, `technical-writer`, etc.) | **`sonnet`** |

**Somente agentes da constelação bigtech.** Nada de agente genérico, anônimo ou improvisado.

**O que continua sendo do `main`:** decidir o que delegar e em que ordem; escrever a ordem de serviço; **re-verificar o entregável** (build limpo, spot-check das afirmações arquivo:linha) porque relatório de agente não é prova (L-12); levar decisão ao líder (L-10); e falar com o líder (L-13).

**Aplicação ao briefar:** a ordem de serviço leva o caminho absoluto de `GODS_LAWS.md` e o texto das leis cujo gatilho casa com a task (L-16 do protocolo do bus vale o mesmo raciocínio: subagent não herda contexto).

**Papéis distintos permanecem (L-12):** o agente que implementa não é o que revisa, e nenhum dos dois é o `main` que re-verifica.
