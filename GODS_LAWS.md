> **LEI ZERO, ACIMA DE TODAS: PROJETO PARA DISTRIBUIÇÃO.** O GlintFx é biblioteca pública sob AGPL-3.0, consumida por gente que não conhecemos, em cinco plataformas. **Nunca raciocine como se houvesse um consumidor único.** Ordem do líder em 21/08/2026, verbatim: *"onde está escrito que o consumidor é único? o projeto é para distribuir"*. Qualquer análise, corte de escopo, priorização ou decisão de API que se apoie na premissa de consumidor único está **errada por construção** e deve ser refeita.

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
| [L-19](#l-19) | criar módulo, tocar a fronteira do SO, ou desenhar API pública | Camadas, portas em compile-time, fronteira pública opaca |
| [L-20](#l-20) | escrever qualquer código com comportamento | TDD estrito: vermelho antes de verde, sem exceção |
| [L-21](#l-21) | nomear qualquer coisa, ou escrever comentário e commit | Identificador e comentário em inglês, `snake_case`; commit em pt-br |
| [L-22](#l-22) | projetar assinatura pública ou tratar erro | Nenhuma exceção cruza a API pública |
| [L-23](#l-23) | commitar, ou fechar uma fatia, ou dar push | Portões de qualidade e o `preci.sh` antes do push |
| [L-24](#l-24) | pensar em cobertura, formatação ou auditoria | Sem meta de cobertura; clang-format LLVM; dossiê só antes da 1.0 |
| [L-25](#l-25) | iniciar build pesado, ASan, teste de janela ou demo | Armar o `watchcode` na janela, e desarmar ao fim |
| [L-26](#l-26) | criar tag, publicar release, ou mexer na versão | Versão e tag são `vA.B.C.D`; `SOVERSION` segue o `A` |
| [L-27](#l-27) | escrever ordem de serviço, ou aceitar um corte proposto por agente | Fato separado de inferência; corte exige citação; passar a fonte, não o resumo |
| [L-28](#l-28) | tocar folha de estilo, tema, layout de UI ou parser de estilo | RCSS é o formato, implementado em casa, sem RmlUi |
| [L-29](#l-29) | não saber como se implementa algo, ou querer ver prior art | Pode LER RmlUi e SDL3 para aprender; copiar é proibido |
| [L-30](#l-30) | tocar mapa, grade, colisão, rota ou visibilidade | Mapa é mecanismo da lib; o formato é nosso; conteúdo de jogo fica fora |

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

O bus é o canal assíncrono entre as sessões do líder e o filho dele. Clone canônico: `<vault>/gusworld_ia_autocomm/`. Repositório **privado**. Protocolo completo em `PROTOCOL.md` do clone; **leia o protocolo, não confie neste resumo**.

**Participantes.** Slugs de sessão: `gusworld` (o jogo), **`glintfx` (este projeto)**, `site` (a revista de registro histórico), `mapeditor` (o editor de mapas). Mais **Gus Dragon**, colaborador humano, filho do líder, handle GitHub `Dragon-Drv`, que manda ideias de jogo por **issue** ou por `.txt` na raiz de `inbox/`.

> ⚠ **Estes nomes são CANAIS DE COMUNICAÇÃO, não a base de consumidores do GlintFx.** Que `gusworld` e `mapeditor` existam no bus **não** os torna os consumidores da biblioteca, nem os torna os únicos. O GlintFx é público e distribuível, e a sua base de consumidores é **aberta e desconhecida** (LEI ZERO, no topo deste arquivo). **Nunca derive público-alvo, prioridade, escopo ou decisão de API da lista acima.** Esta ressalva existe porque em 21/08/2026 um agente leu esta seção e concluiu "consumidor único futuro", e a conclusão quase amputou a lente de produto do planejamento.

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

## L-19

**Data:** 21/08/2026, decisão do líder após comparação de alternativas.

A arquitetura do GlintFx tem **três compromissos**, e nenhum deles é negociável por agente.

**1. Camadas são a espinha.** Núcleo puro (math2d, geometria, tempo, tipos) não conhece o sistema operacional. **Uma única camada toca o SO**: nela vivem Wayland, GL, áudio, gamepad e arquivo. Cada camada depende só das de baixo, nunca o contrário, e **um gate de CI reprova a violação** em vez de confiar na disciplina de quem escreve.

**2. Porta é `concept` de C++23, resolvida em compile-time.** A fronteira de plataforma é modelada como porta com adaptador por sistema, mas **sem despacho virtual no caminho quente**: o adaptador é escolhido na compilação. Teste substitui o adaptador por um falso, que é o benefício real que se queria da inversão de dependência. Nada de `virtual` por frame ou por draw.

**3. A superfície pública é opaca.** A biblioteca é compartilhada por padrão, então **toda classe pública com layout visível ou método virtual vira contrato de ABI**. A API pública expõe handle opaco ou PIMPL. Reescrever o interior não pode quebrar consumidor.

**Alcance da opacidade, confirmado pelo líder em 21/08/2026:** vale para **handle e subsistema com estado**. **Não** vale para *value type* do núcleo (`version`, `vec2`, `rect` e afins), onde o **layout estável é o próprio contrato**: esconder value type custaria alocação e indireção em caminho quente, e a lei já põe "tipos" dentro do núcleo puro. Value type do núcleo é visível, e mudá-lo é mudança de ABI assumida, sinalizada pela versão.

**Sobre hexagonal, para encerrar a dúvida:** o vocabulário de portas e adaptadores está adotado; a obrigação de interface virtual em runtime **não** está. Hexagonal clássico existe para proteger regra de negócio, que um framework 2D não tem, e cobra despacho virtual justamente onde este projeto não pode pagar.

**Aplicação:** assunto novo nasce em módulo estreito próprio, com fronteira declarada (ver L-17), e nunca é acrescentado a um tipo existente porque "cabia lá". Ao propor `virtual` em tipo público, pare e justifique contra este item; na dúvida, pergunte ao líder.

### A atomização (L-17) continua valendo, e esta arquitetura tem três armadilhas contra ela

Cada uma destas é **achado de revisão**, não questão de gosto:

1. **Handle opaco que vira dono de tudo.** O risco do PIMPL é a fachada que acumula: um `Context` com oitenta métodos é monolito com outro nome. **Um handle por assunto** (janela, render, input, áudio), cada método sendo **um átomo** que encaminha para **um** átomo do interior. Fachada que encaminha não é licença para fachada que decide.
2. **Porta gorda.** Um `concept` que exige vinte operações é interface inchada, com o mesmo defeito que o hexagonal clássico teria. **Concepts pequenos, compostos**: quem só precisa desenhar não deve satisfazer o requisito de áudio.
3. **`#ifdef` dentro de função.** Adaptador escolhido em compile-time significa **um arquivo por plataforma, selecionado pelo CMake**, não bloco de pré-processador dentro do corpo da função. `#ifdef` picotando uma função é monolito montado pelo pré-processador, e some da leitura de quem revisa.

Dito de forma direta: a arquitetura **reforça** a L-17 (camada e módulo estreito são atomização em escala maior), desde que estas três armadilhas sejam tratadas como violação.

## L-20

**Data:** 21/08/2026, decisão do líder.

**TDD estrito, a partir do primeiro módulo com comportamento.** Nenhuma linha de código de comportamento entra sem um teste que **falhava antes** de ela existir.

**O ciclo, na ordem, sem pular etapa:**

1. **Vermelho.** Escreva o teste. **Execute** e veja falhar. Teste que nunca foi visto falhando não prova nada: pode estar medindo outra coisa, ou nem estar sendo executado.
2. **Verde.** Escreva o mínimo que faz passar.
3. **Refatorar.** Limpe com a suíte verde como rede, aplicando a L-17.

**Aplicação ao delegar (L-18):** a ordem de serviço do agente de implementação exige **a saída real do teste falhando** antes da implementação, e a saída real dele passando depois. Relatório que só mostra o verde final não cumpriu a lei: falta a metade que prova que o teste morde.

**O que está fora:** adaptador que só encaminha chamada ao sistema operacional, onde o teste honesto é de integração e não unitário, e código gerado. Fundação de build também está fora, e por isso a fundação criada em 21/08/2026 nasceu sem TDD, legitimamente: não havia comportamento a especificar. **A partir do primeiro módulo de verdade, não há mais essa saída.**

**Cuidado registrado, que já custou tempo em outro projeto:** ao provar que um teste morde por mutação, **prove que a mutação chegou ao código executado**. Registro de callback capturado no import, binário desatualizado por falta de rebuild e arquivo não commitado produzem suíte verde com o mutante aplicado, e a conclusão errada é "meu teste é fraco" quando na verdade a mutação nunca rodou.

O manual `TESTES.md` continua normativo para **como** testar; esta lei fixa **quando**.

## L-21

**Data:** 21/08/2026, decisão do líder ao revisar o `CONTRACT.md` item a item.

**Identificador e comentário de código em inglês, no estilo da biblioteca padrão de C++: `snake_case`, sem prefixo `m_`.** Isto **substitui** o `CONTRACT.md` §6.1, que manda função em pt-br, membro com `m_` e constante em ALL_CAPS. Motivo: a lib é pública e AGPL, e um consumidor estrangeiro não deve precisar de tradução para ler a API nem para abrir o arquivo.

**Mensagem de commit continua em pt-br**, assim como a documentação do projeto e a conversa com o líder. O histórico é conversa dele com o projeto.

**Aplicação:** `runtime_version`, `frame_buffer`, `is_valid`. Nada de `buscarItem`, nada de `m_cache`. Nome revela intenção, sem abreviação que não seja universal (`id`, `url`, `http`), e nada de letra solta fora de contador de laço.

## L-22

**Data:** 21/08/2026, decisão do líder.

**Nenhuma exceção cruza a API pública.** Internamente exceção é permitida; na fronteira pública o erro sai como `std::expected` ou código de erro.

**Por quê, e o motivo é de ABI, não de gosto:** a biblioteca é compartilhada por padrão (L-19). Exceção atravessando `.so` exige RTTI compatível entre a lib e o consumidor, e quebra quando os dois foram compilados por compiladores ou bibliotecas padrão diferentes.

Continua valendo do `CONTRACT.md` §6.4: erro nunca é engolido em silêncio, sempre propagado ao chamador, e exceção não serve de fluxo de controle.

## L-23

**Data:** 21/08/2026, decisão do líder sobre a tabela de portões do `CONTRACT.md` §11.

**Quatro portões, todos adotados:**

1. **Zero aviso de compilação em todo commit.** `-Werror` no CI. Aviso não acumula.
2. **ASan e UBSan a cada fatia fechada.** Build separado, mais lento, e é o que salva C++ de defeito silencioso.
3. **Análise estática no CI:** `clang-tidy` e `cppcheck`.
4. **Scan de segredo no CI:** `gitleaks`. Aviso honesto que a lei registra: `gitleaks` **não** pega nome de projeto e, por padrão, olha a árvore e não o histórico. Para dado sensível em repo público, a verificação é `git log --all -p | grep -ci <termo>`, com `-i`, nunca `git grep`.

**`preci.sh` antes do push.** O `TESTES.md` T15 é adotado: existe um script local que espelha o CI (formatação, configure com avisos estritos, compilação, `clang-tidy`, `cppcheck`, `ctest`) e ele roda **antes** do push, não depois. Decisão do líder de **não** amarrá-lo a hook de pre-push, para não atrasar push de documentação. As ferramentas que ele exige são instalação de sistema, portanto passam pelo líder (L-14).

## L-24

**Data:** 21/08/2026, decisões do líder que ajustam os manuais.

- **Sem meta numérica de cobertura.** O `TESTES.md` T1 pede 70% nos módulos críticos; **não vale aqui**. Com a L-20, todo código de comportamento nasce de um teste que falhou, então cobertura é consequência, não alvo. Métrica de cobertura premia linha executada, não comportamento verificado, e perseguir número produz teste escrito para a métrica.
- **Formatação: `clang-format` com base LLVM, indentação de 4, colunas 100.** Formatação deixa de ser assunto de revisão.
- **Dossiê formal de auditoria (`AUDITORIAS.md`, e A2/A10 do `TESTES.md`) só antes da 1.0**, feito pelo `internal-auditor`, como portão de release. Até lá os portões automáticos da L-23 cobrem o dia a dia.

### O que dos manuais foi descartado por inaplicável (21/08/2026)

Registrado para ninguém reintroduzir por engano: tudo de **Qt** (§12.1), **T10 injeção de SQL** (não há banco), **§7 UI/UX e WCAG** (é manual de aplicação com formulário, não de biblioteca), o modelo de **camadas do §5** (frontend/middleware/backend/infra é desenho de aplicação; as camadas deste projeto estão na L-19), **T5 e T12**, scan de dependência e de CVE (dependência zero, L-07), e o piso de **C++20** do §12.1, superado pela L-03.

## L-25

**Data:** 21/08/2026, decisão do líder.

O `watchcode` é o daemon que varre o journal e os coredumps atrás de crash real (OOM, SIGSEGV, SIGABRT, coredump fora da linha de base). Ele fica armado **por janela, não permanentemente**.

**Armar antes de:** build pesado, rodada de ASan ou UBSan, teste de janela ou de input em container (L-09), e execução de demo. **Desarmar ao fim da janela**, emitindo o semáforo.

**Por que não deixar ligado sempre, e o motivo é das nossas próprias leis:** a L-20 manda **ver o teste falhar** antes de existir código, e a revisão adversarial da L-12 roda **mutação**, quebrando o código de propósito. As duas coisas **fabricam crash legítimo**. Vigia ligado o tempo todo grita a cada ciclo vermelho correto, e um alarme que grita sempre deixa de ser lido. Nas janelas acima ninguém está quebrando nada de propósito, então todo achado é sinal.

**Ao achar crash desta sessão:** apresentar o semáforo vermelho com o que foi capturado e **perguntar ao líder** antes de abrir o ciclo de diagnóstico. Não delegar C-level por conta própria.

**Fato técnico verificado em 21/08/2026:** o `core_pattern` desta máquina é `systemd-coredump`, e isso é do **kernel**, não do container. Logo, crash dentro do container de teste **aparece** no `coredumpctl` do hospedeiro, e o vigia cobre os testes de superfície da L-09.

**Ruído conhecido, a descartar sem investigar:** coredump de binário sob `/var/tmp/*mutation-sandbox*` ou equivalente é mutação deliberada da revisão adversarial. Três desses foram descartados em 21/08/2026, por decisão do líder.

## L-26

**Data:** 21/08/2026, decisão do líder.

**A versão do GlintFx tem quatro componentes, e a tag é `vA.B.C.D`.** Isto é escolha do líder e **substitui** o SemVer de três números.

| Componente | Sobe quando |
|---|---|
| **A** | quebra de API: código de consumidor que compilava deixa de compilar |
| **B** | recurso novo, compatível para trás |
| **C** | correção, sem recurso novo e sem quebra |
| **D** | build ou revisão de empacotamento, **sem mudança de código** |

**`SOVERSION` acompanha o `A`.** É o único número que o consumidor precisa ler para saber que o binário dele quebrou. No CMake, `project(... VERSION A.B.C.D)` usa os quatro (o quarto é o `TWEAK`), e a versão atual `0.1.0` passa a ser **`0.1.0.0`**.

**Antes da 1.0: `SOVERSION 0` e nada de estabilidade prometida.** O zero em `libglintfx.so.0` é a convenção Unix que avisa "a ABI pode quebrar a qualquer momento", e é o que permite romper à vontade até a 1.0 **sem enganar ninguém**. Depois da 1.0, a regra da tabela passa a valer integralmente.

**Aplicação:** `write_basic_package_version_file` precisa de política coerente com esta tabela. Antes da 1.0, `SameMinorVersion` é o correto, porque `B` é onde a quebra mora enquanto `A` é zero. **Ao chegar na 1.0, isso muda para `SameMajorVersion`** e a mudança não pode ser esquecida.

### O terceiro contrato: DADO

Acrescentado em 21/08/2026, depois que o CTO achou um furo real nesta lei ao desenhar o formato de arquivo de mapa (L-30).

Esta lei nasceu cobrindo **dois** contratos: **API** (código que compilava deixa de compilar) e **ABI** (binário incompatível). O **formato de arquivo é um terceiro**, e é o mais grave dos três: um `.so` incompatível manda o consumidor **recompilar**, mas **um arquivo salvo não se recompila**. Ele é dado do usuário final de quem consome a biblioteca, e ninguém o conserta com um rebuild.

**Política decidida pelo líder:**

- **Quebrar formato sobe o `A`**, do mesmo jeito que quebra de API, **e o leitor novo continua lendo os formatos antigos**. Ou seja: a lib nunca abandona um arquivo que ela mesma gravou.
- **Acréscimo compatível sobe o `B`**, com o número menor do formato incrementado.
- **Antes da 1.0**, o formato declara **versão zero, sem promessa**, espelhando honestamente o que o `SOVERSION 0` já faz com o binário.

**Aplicação:** todo formato de arquivo que a lib publicar segue esta política, não só o de mapa. Revisão de porta de mão única sobre formato de arquivo usa **lente de dado persistido**, que é diferente da lente de ABI: a pergunta não é "quem recompila", é **"quem perde o arquivo"**.

Tag e release continuam exigindo **aval explícito do líder no contexto** (L-11): esta lei fixa o formato, não autoriza taggear.

## L-27

**Data:** 21/08/2026, decisão do líder, nascida de um erro medido no mesmo dia.

**O erro que originou a lei:** um agente inferiu que o projeto teria "consumidor único futuro", inferência que **não estava escrita em lugar nenhum**, e a usou para cortar a lente de produto e a tabela de scoring do planejamento. O orquestrador repassou a inferência ao agente seguinte **como se fosse fato**, sem a marca de origem. O líder pegou com uma pergunta de uma linha: *"onde está escrito?"*.

**Três obrigações, todas do orquestrador:**

**1. Proveniência marcada em toda ordem de serviço.** O brief separa explicitamente **`FATO`** (com arquivo e linha, ou verbatim do líder) de **`INFERÊNCIA DE AGENTE ANTERIOR`**. Subagente não distingue: tudo que chega no prompt do orquestrador é premissa dada. Inferência sem marca vira spec em silêncio.

**2. Corte exige citação.** Toda premissa que sustenta um **corte** (de agente, de escopo, de item, de etapa) só é aceita com a fonte citada. Se a fonte não existir, o corte não acontece. Corte é onde premissa errada faz o estrago máximo, porque **o que foi cortado desaparece e não volta a ser questionado**.

**3. Passar a fonte, não o resumo dela.** Ao encadear agentes, o seguinte recebe o **caminho do arquivo** que o anterior produziu, e lê. Resumo do orquestrador perde exatamente o marcador de incerteza que distinguia dado de conclusão.

**Padrão-mãe, para reconhecer o próximo caso:** já existe registro de que **exemplo inventado pelo orquestrador numa explicação vira spec**. Esta lei é o mesmo mecanismo com outra roupa: ali a origem era o próprio orquestrador, aqui era outro agente. **A regra geral é que tudo que atravessa o orquestrador sem marca de origem chega ao destino com autoridade de fato.**

## L-28

**Data:** 21/08/2026, confirmado pelo líder. **Verbatim relatado:** *"usaremos rcss, é o que quis dizer com rcss, mas sem ligar rml"*.

**O GlintFx adota o RCSS como formato de folha de estilo, e o implementa em casa.** O **RmlUi está fora**: a lei de dependência zero (L-07) não abre exceção para ele, e adotar o formato não é adotar a biblioteca.

**De onde a especificação sai, e de onde NÃO sai.** A lista do que o parser precisa suportar vem do **formato RCSS** e das decisões de escopo do líder por `AskUserQuestion` (L-10), julgadas pelo **consumidor externo desconhecido** (LEI ZERO e L-02). **Nunca** dos mockups, do ui-kit ou das telas de um consumidor específico.

**Proibição concreta, com o caso que a originou.** Existe em `/var/tmp` um backup do GusWorld com ui-kit, mockups de tela, capturas e um `PORTING-RCSS.md` que se abre como *"notas do dev do glintfx revisando os componentes contra a versão vigente do glintfx"*. **Isso é o predecessor descrevendo o que já funcionava, e a L-01 o proíbe como base, referência ou canon.** Pior: usar aquele material como fonte da especificação é a premissa de **consumidor único** entrando por outra porta, já que a spec sairia do que **um jogo** precisava. Se em algum momento parecer mais barato "ver como o antigo fazia", **é exatamente o movimento proibido**.

**Aplicação:** o parser de RCSS é escopo grande e nasce com item próprio no `TODO.md`, quebrado em fatias (L-17), sob TDD estrito (L-20), com a superfície pública julgada como porta de mão única (L-19). Card, deck, bancada e mercado são design de **aplicação**, e regra de aplicação específica nunca entra na lib (L-02).

### Decisões de escopo da v1, tomadas pelo líder em 21/08/2026

Cinco decisões, por `AskUserQuestion` (L-10). **Não são mais perguntas.**

1. **Como o motor enxerga a árvore do consumidor: contrato preenchido pelo consumidor**, sem template na superfície pública. Motivo dele: não obriga quem já tem programa pronto a trocar o que tem, e não solda a biblioteca ao código dele. O motor funciona com qualquer árvore.
2. **Folha imutável após o parse.** A única dinâmica é o estado de pseudo-classe do nó, com recomputação sob demanda. Simples, seguro entre threads por construção, e cobre `:hover`, `:focus` e `:checked`.
3. **Seletor: o formato COMPLETO na v1.** Atributo com os sete operadores, `:not()` com lista de seletores complexos, os quatro combinadores, a família estrutural inteira, `:placeholder-shown` e `:scope`.
4. **`@media` fica FORA da v1**, ignorada com diagnóstico.
5. **Animação, transição e `@keyframes` ficam FORA da v1**, ignorados com diagnóstico. Entram como escopo próprio quando o loop principal e o relógio existirem.

**Defaults registrados para veto, que o líder não vetou:** namespace `glintfx::style` com IDs de item `RCSS-*`; as cores modernas `lab()`, `lch()`, `oklab()` e `oklch()` fora da v1; e, quanto a aspas no valor de seletor de atributo, seguir a convenção do CSS (aceitar identificador sem aspas e string com aspas), já que a documentação do formato não especifica.

**Fatos do formato, verificados na documentação pública sob a L-29 e não de memória:** o formato **não tem `!important`**, **não tem a palavra `inherit`** e **não tem folha de estilo embutida do próprio motor**, o que simplifica cascata e herança; **pseudo-elementos não existem** no formato; a especificidade de `:not()` é a do sub-seletor mais específico, sem contar a si mesma; as cores nomeadas são **19**, não a tabela completa do CSS.

## L-29

**Data:** 21/08/2026. **Verbatim do líder:** *"você pode LER sem clonar os repos de rmlui e SDL3 para aprender, memorizar adequadamente como fazer aqui. Nào copie, não quero plágio. Mas pode refazer mais eficiente ou de maneiras diferentes."*

**O que está PERMITIDO:** ler o código-fonte e a documentação do **RmlUi** e do **SDL3** para **aprender a técnica**: como um problema costuma ser atacado, que armadilhas existem, que estrutura de dados serve. Aprender de prior art é engenharia normal, e a lei de dependência zero (L-07) proíbe **depender**, não proíbe **saber**.

**Sem clonar.** Leitura pela web. Nada de `git clone`, nada de vendorizar, nada de baixar a árvore para dentro do repositório ou do disco de trabalho.

**O que está PROIBIDO, e o líder chamou pelo nome: plágio.**

- **Nada de código verbatim**, nem trecho, nem função, nem tabela de constantes.
- **Nada de porte linha a linha** com nomes trocados. Traduzir a mesma implementação continua sendo cópia.
- **Nada de copiar comentário, estrutura de arquivo ou organização interna** por decalque.

**O que se espera em troca:** **refazer melhor, ou diferente.** Aprendida a ideia, a implementação é nossa, julgada pelos nossos critérios: dependência zero, camadas da L-19, átomos da L-17, ABI da L-26, e o consumidor externo desconhecido da LEI ZERO. Se a nossa saída for igual à deles, ou não aprendemos nada, ou copiamos.

**Higiene de licença, que reforça a proibição em vez de relaxá-la.** RmlUi é **MIT** e SDL3 é **zlib** (verificado na fonte em 21/08/2026: o README do RmlUi diz "published under the MIT license"; o SDL declara zlib). As duas são permissivas, então **copiar seria legalmente possível mediante atribuição**, e é justamente isso que a lei recusa. Enquanto **nenhuma linha for copiada**, nenhuma obrigação de atribuição nasce, e o projeto segue AGPL-3.0 limpo (L-08). No instante em que alguém colar código, cria-se obrigação de aviso de licença **e** se viola esta lei.

**Como declarar, quando a leitura ajudar:** dizer no comentário ou no commit que a **técnica** veio de leitura de prior art, sem colar nada. Exemplo do tipo certo: *"abordagem de cascata por especificidade calculada em três dígitos, técnica comum em motores de CSS"*. Exemplo do tipo errado: colar o código deles e citar a fonte.

**Não confundir com a L-01.** Esta lei fala de **projeto de terceiro**. O **predecessor `glintfx`** continua totalmente proibido como base, referência ou canon, e o mesmo vale para o backup do GusWorld citado na L-28. A diferença é real: aprender como o mundo resolve um problema é legítimo, e ressuscitar o nosso próprio código morto é o que o líder mandou não fazer.

## L-30

**Data:** 21/08/2026, decisão do líder.

**O GlintFx tem mecanismo de mapa.** Matriz `x,y` simples, objetos posicionados nela, hitbox, parede, porta e ponto de teleporte (escada, buraco e afins).

**O GlintFx é DONO do formato de arquivo de mapa.** A lib publica o formato e o carregador; o editor e o jogo são **consumidores** dele. Consequência que a decisão assume de olhos abertos: **o formato vira API pública e contrato de ABI** (L-19 e L-26), qualquer consumidor no mundo passa a depender dele, e mudá-lo depois quebra todos. Por isso o formato nasce com revisão de API dedicada e versionamento explícito.

**Escopo dentro da lib, decidido pelo líder:** matriz e objetos, hitbox e consulta de colisão, **mais busca de caminho e visibilidade**. Porta e teleporte entram como **marcador genérico com destino**.

**A fronteira que separa mecanismo de conteúdo, e ela é a parte que mais se esquece.** Cidade, dungeon, estrada e floresta são **conteúdo de um jogo**, não mecanismo. A lib entrega a **matriz e as consultas**; o que se põe dentro dela é do consumidor. **A lib nunca sabe o que é uma dungeon**, nunca sabe que uma escada é uma escada: ela sabe que existe um marcador com um destino, e quem dá sentido é quem consome (L-02 e LEI ZERO). Nomear tipo de cenário dentro da lib é o mesmo erro de premissa que a LEI ZERO existe para impedir, entrando por outra porta.

**Aviso de escopo, registrado porque o líder decidiu com ele à vista:** busca de caminho e visibilidade são algoritmos com **muitas variantes**, e cada consumidor costuma querer a sua. Entram como escopo próprio, quebrados em fatias (L-17), com a superfície pública tratada como porta de mão única.

**O editor é projeto à parte.** `GusWorld_MapEditor` (`petrinhu/gusworld_mapeditor`) é o editor do formato e **não** faz parte deste repositório. Ele consome o formato como qualquer outro consumidor, e por decisão do líder a mudança foi comunicada pelo bus (L-16).

### Decisões de escopo da v1, tomadas pelo líder em 21/08/2026

1. **O arquivo é binário, organizado em blocos**, com versão no cabeçalho e a regra de pular bloco desconhecido, que é o que permite evoluir sem quebrar.
2. **Leitor e escritor são os dois públicos.** O escritor nasce de qualquer forma para os testes, e gravar e reler o mesmo mapa é a prova mais forte que o formato tem; publicá-lo evita que cada editor escreva o seu e derivem entre si.
3. **Posição de objeto é contínua**, em unidades de célula. Marcador de porta e de teleporte continua ancorado na célula.
4. **O mapa aceita mudança permanente** em runtime (parede destruída, porta aberta de vez), sem a lib saber o porquê. Travessia condicional por ator é outra coisa, e já está resolvida pela máscara de consulta.
5. **Versionamento do formato:** ver a seção "O terceiro contrato: DADO" da L-26, que esta trilha obrigou a escrever.

### O requisito que veio de um consumidor humano, e o que ele virou

Em 21/08/2026 o **Gus Dragon** pediu, nomeando o GlintFx: *"GlintFx e Mapeditor façam blocos especiais pra isso"*. O mecanismo genérico que sustenta o pedido, e que **não** nomeia nada do jogo:

- **A célula carrega dois campos separados**, um com a semântica de arte ou terreno e outro com a **marcação opaca do autor do mapa**. São separados porque a marcação é independente da arte.
- **Toda consulta aceita uma máscara de travessia.** Célula bloqueante cuja marca casa com a máscara conta como transponível **só naquela consulta**, por ator, sem estado. A lib nunca sabe o que a marca significa.
- **"Parede rachada" e "porta do chefe" nunca aparecem na lib.** São bits que o consumidor nomeia. Propor bit nomeado dentro da biblioteca é violação desta lei e achado de revisão.

**Lição de método registrada:** necessidade descrita em palavras por um consumidor é insumo **legítimo**; copiar o que a lib antiga fazia continua **proibido** (L-01 e L-28). A diferença é a forma, não a origem.
