> **LEI DAS LEIS, ANTERIOR ATÉ À LEI ZERO: só o líder pode quebrar uma lei deste arquivo** — agente nenhum quebra, flexibiliza, reinterpreta ou "adapta ao caso" por conta própria — **e nem a ordem direta dele dispensa a confirmação**: antes de executar, nomeie a lei que está sendo quebrada, cite o texto dela, diga o que ela protege e o que se perde ao quebrá-la, e pergunte por `AskUserQuestion` se é isso mesmo que ele quer; **quando o pedido for ALTERAR ou REVOGAR uma lei, argumente CONTRA primeiro, sempre e sem exceção**, com razões concretas, o problema que a lei existe para impedir, os trade-offs da mudança e o que fica desprotegido depois dela, e só então leve a escolha por `AskUserQuestion` entre **confirmar** a alteração e **cancelá-la**; pressa, obviedade aparente, "ele já mandou uma vez" e aprovação dada em outro contexto **nunca** substituem essa confirmação, e silêncio jamais vale como aval. (Ordem do líder, 22/08/2026.)

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
| [L-18](#l-18) | ir executar qualquer trabalho de produto | Main só orquestra; C-level fable audita e cria; sonnet implementa; commit ao fim de cada fatia; push ao fim de cada onda só se o GHA fechar verde, se todos os testes verdes. |
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
| [L-31](#l-31) | tocar contexto gráfico, shader ou carregador de GL | OpenGL 3.3 core, nativo nas duas plataformas |
| [L-32](#l-32) | escolher a próxima fatia a implementar, antes de a demo rodar | Caminho principal sempre, mais no máximo UMA trilha paralela |
| [L-33](#l-33) | tocar QUALQUER coisa de mapa: decisão, fatia, formato, código | Avisar o `mapeditor`; sessão fora do ar, mandar pelo bus |
| [L-34](#l-34) | iniciar uma fatia ou onda de trabalho de produto | Ciclo de 4 passos: main+líder faz brainstorm, `fable` planeja e audita, main verifica e orquestra, `sonnet` implementa; quem define fatia ou onda é o líder |
| [L-35](#l-35) | desenhar ou implementar a entrega de evento de entrada | Promessa pública do contrato: determinística, sem duplicação, sem reordenação; nasce com teste que a prova |
| [L-36](#l-36) | decidir cursor do ponteiro, áudio no Linux, mapeamento de gamepad ou compose/tecla morta | Quatro decisões de escopo do líder que saem de `🎨 Pendente design` |
| [L-37](#l-37) | o líder aprovar, rejeitar ou mudar algo, ou fechar item de alta prioridade | Avisar o Gus Dragon sem ele precisar perguntar |
| [L-38](#l-38) | nomear artefato binário, ou cogitar extensão própria para qualquer arquivo | Dado é nosso e pode ter extensão própria; binário usa `.so` e `.dll`, o que o SO espera |
| [L-39](#l-39) | ver QUALQUER coisa vinda do Gus Dragon, em qualquer dos cinco canais | É prioridade e é SEMPRE respondida; o ack não espera o líder e interrompe a onda |
| [L-40](#l-40) | escrever, revisar ou confiar em QUALQUER portão de qualidade | Piso de varredura não-vazia: contou zero, reprova; a contagem aparece na saída mesmo quando passa |

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

### A regra QUALITATIVA: como se reconhece um monolito antes de contar

**Data:** 24/08/2026. **Verbatim do líder:** *"tudo DEVE ser atomizado e NADA pode ser monolítico"*. Adaptada da L-19 do `gusworld_mapeditor`, com três divergências decididas por ele via `AskUserQuestion`: **os números acima FICAM e convivem** com esta regra; **o adaptador de plataforma NÃO tem isenção**; e a regra mora **aqui dentro**, não em lei própria.

**Por que as duas metades convivem:** os números pegam o caso fácil, de forma mecânica e barata. Esta regra pega o difícil — **o monolito que respeita todos os limites**: cinco funções de trinta linhas que mudam por cinco motivos diferentes passam limpas por qualquer contagem.

**A régua, em uma frase:**

> **Se mudanças vindas de LEIS DIFERENTES e não relacionadas obrigam a editar a MESMA unidade, ela está virando monolito.**

Aqui as razões de mudar já estão catalogadas, porque as leis as fixaram: **L-05** (Wayland), **L-07** (dependência zero), **L-19** (camadas e fronteira), **L-22** (erro na borda), **L-26** (os três contratos), **L-28** (RCSS), **L-30** (mapa), **L-31** (OpenGL).

### As cinco perguntas do revisor

*"Isto é monolito?"* ninguém sabe responder. Estas se respondem **olhando o código**, não opinando:

1. **A pergunta das leis.** Quais leis obrigariam esta unidade a mudar? Lê-se nos `#include` e nos métodos públicos. Uma lei: unidade sã. Duas ou mais, não relacionadas: monolito em formação.
2. **A frase sem "e".** Descreva a unidade numa frase. Se precisar de "e" ligando verbos de natureza diferente, reprova. **A frase escrita entra no relatório de revisão.**
3. **O teste monta o mundo?** Para exercitar UM comportamento, o preparo do teste precisa de janela, arquivo, relógio ou container? Átomo se constrói sozinho. Lê-se no preparo dos testes da fatia.
4. **O que entra pelo `#include`?** O header puxa grupos que não conversam entre si? A lista de dependências se lê em dez segundos.
5. **Quem paga a próxima feature?** No diff da fatia (`git log --stat`), a coisa nova tocou quais arquivos? Se toda coisa nova aterrissa no mesmo arquivo, **esse arquivo é o monolito nascendo**. É a mais objetiva das cinco: responde-se com o diff, não com julgamento.

### Onde o monolito vai nascer AQUI

Monolito nunca nasce por burrice; nasce por conveniência local que parece razoável no dia.

| Lugar de risco | Como nasce | Por que parece razoável |
|---|---|---|
| **Fachada pública** (`*_API`) | Cada capacidade nova vira "mais um método" na fachada | "É a porta de entrada, tem de estar lá" |
| **Modelo de mapa** | O agregado já tem os dados, então serializar, validar, repartir e consultar viram métodos dele | "O dado já está aqui" |
| **Adaptador Wayland** | Janela, entrada, áudio e relógio chegam todos pelo mesmo laço de evento | "Os retornos do sistema aterrissam no mesmo lugar" |
| **Registro de propriedades do RCSS** | Cada propriedade nova acrescenta um caso ao mesmo `switch` | "É uma linha a mais na tabela" |
| **`preci.sh`** | Cada portão novo vira mais um estágio no mesmo script | "É o script de portões, é o lugar dele" |

**O adaptador de plataforma NÃO tem isenção** (decisão do líder, 24/08/2026) — e o motivo é o inverso da intuição: é a camada **mais difícil de testar** e a que **mais atrai acumulação**. Isentá-la seria isentar justamente onde o monolito nasce mais rápido.

### Sinais precoces

Esta regra nasce com **575 linhas** de C++ rastreado e o maior arquivo em **93**. Monolito de 3000 linhas todo mundo vê; ela existe para reconhecê-lo com 300.

- **O mesmo arquivo aparece no diff de todas as fatias.** O mais barato de medir e o mais confiável.
- **Construtor, ou preparo de teste, ganhando parâmetro a cada fatia** — a unidade precisa de cada vez mais mundo para existir.
- **`switch` que ganha um caso por feature.**
- **Nome sem substantivo de domínio:** `Manager`, `Service`, `Helper`, `Utils`, `Core`. A unidade que não consegue dizer o que **é**, é porque faz de tudo.
- **A frase "é só mais um método" aparecendo como justificativa na revisão.** **Essa frase é o som do monolito crescendo: verdadeira em cada passo individual, falsa na soma.**

### A contraparte, com a mesma força

**Fragmentação é monolito ao contrário.** Se seguir um raciocínio exige pular por vários arquivos, o todo virou ilegível — monolito da atenção de quem lê. Dividir **por medo da lei** produz peças sem nome próprio, que é o defeito que a lei existe para impedir, entrando pela outra porta.

**O árbitro entre as duas é sempre o NOME.** Nome honesto e completo, e uma razão de mudar: o corte está certo, qualquer que seja o tamanho. Nome com "e", ou vago: errado, mesmo com dez linhas.

### Fiscalização — onde a regra mora no processo

Regra qualitativa sem lugar no processo é regra que ninguém aplica.

1. **Na revisão adversarial de cada fatia:** para **cada unidade criada ou crescida**, o revisor responde as cinco perguntas e **grava as respostas no relatório**. **Silêncio sobre uma unidade conta como unidade não revisada** — silêncio não é prova de conformidade. A quinta se responde sempre, porque o diff sempre existe.
2. **No `AUDITORIAS.md`**, capítulo 2 ("Camadas e atomização"). O auditor **não confia nos relatórios**: pega o maior arquivo de cada camada e o que mais aparece no `git log --stat`, responde ele mesmo as cinco perguntas, e compara.
3. **Divergência tem dono.** Implementador e revisor discordando, ou separação com custo real, **vai ao líder pela L-10** — nunca sai no silêncio de um agente.

**Ao despachar subagent** que crie unidade nova, o texto desta seção vai no prompt, e a ordem de serviço do revisor cita as cinco perguntas como parte do entregável.

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

### Cadência de commit e push, e os dois portões de push

Esta lei também carrega, na própria linha de gatilho do índice, o texto do líder sobre commit e push. **Verbatim dele, já registrado ali:** *"Main só orquestra; C-level fable audita e cria; sonnet implementa; commit ao fim de cada fatia; push ao fim de cada onda só se o GHA fechar verde, se todos os testes verdes."*

**Desdobramento do trecho de commit e push, escrito por este corpo para tornar a frase executável — o único texto verbatim do líder nesta seção é a citação acima, o resto é interpretação do orquestrador, não citação nova:**

- **Commit ao fim de cada FATIA.** Barato e frequente; tira o trabalho da zona de risco antes de a fatia seguinte começar.
- **Push ao fim de cada ONDA**, e só quando os **dois portões** fecham verde, sem exceção e sem "está quase":
  1. **O GitHub Actions (GHA) fechou verde.**
  2. **Todos os testes verdes.**
- **Os dois são conjuntivos.** Um verde e um vermelho não é push adiado por pouco: é push proibido.
- **Alcance:** os dois portões valem para **todo** push, sem categoria de commit que não conte.

**Relação com as leis vizinhas, para não se ler como conflito.** Esta seção **não substitui** a L-11 nem a L-12, **refina**: a L-11 já manda "commit local a cada fatia... push quando a onda fecha, depois do review e com os gates verdes" — aqui se nomeiam os dois portões concretos que compõem esse "gates verdes" (GHA verde e suíte de testes verde) e se fixa que valem para todo tipo de push, sem exceção de categoria. A L-12 continua sendo quem fixa que implementer, reviewer e orquestrador são agentes distintos. A L-34 fixa a ordem do ciclo (brainstorm, plano, verificação, implementação) em que este passo de commit e push se encaixa.

**Aplicação:** antes de todo push, diga em qual estado está cada um dos dois portões. Silêncio sobre um deles conta como vermelho, pela mesma razão do protocolo deste arquivo: silêncio não é prova de conformidade.

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

**Ratificação de 22/08/2026, pedida pelo próprio líder.** A sessão do `gusworld` relatou que ele queria decidir sobre esse `PORTING-RCSS.md` conosco, e a decisão dele, tomada por `AskUserQuestion`, foi **não abrir, sem exceção**. O argumento que a sustenta, e que vale como método para qualquer documento futuro do gênero: a lista de recursos que um consumidor precisou é, por construção, um **subconjunto** do que a documentação pública do formato já define, logo o documento **não pode ensinar nada que a fonte já não tenha**. O único acréscimo real dele seria **ordem de importância derivada de um consumidor**, que é exatamente o que a LEI ZERO proíbe. O risco não é copiar código: é a lista **parar a pergunta certa**, trocando "o que o formato define?" por "o que está na lista?". Precedente empírico registrado no mesmo dia: o escopo de RCSS foi fechado **sem** o documento, indo à documentação do formato, e nesse caminho o CTO **corrigiu três erros próprios** que a memória carregava.

**Aplicação:** o parser de RCSS é escopo grande e nasce com item próprio no `TODO.md`, quebrado em fatias (L-17), sob TDD estrito (L-20), com a superfície pública julgada como porta de mão única (L-19). Card, deck, bancada e mercado são design de **aplicação**, e regra de aplicação específica nunca entra na lib (L-02).

### Decisões de escopo da v1 do RCSS, tomadas pelo líder em 21/08/2026

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

### Decisões de escopo da v1 do formato de mapa, tomadas pelo líder em 21/08/2026

1. **O arquivo é binário, organizado em blocos**, com versão no cabeçalho e a regra de pular bloco desconhecido, que é o que permite evoluir sem quebrar.
2. **Leitor e escritor são os dois públicos.** O escritor nasce de qualquer forma para os testes, e gravar e reler o mesmo mapa é a prova mais forte que o formato tem; publicá-lo evita que cada editor escreva o seu e derivem entre si.
3. **Posição de objeto é contínua**, em unidades de célula. Marcador de porta e de teleporte continua ancorado na célula.
4. **O mapa aceita mudança permanente** em runtime (parede destruída, porta aberta de vez), sem a lib saber o porquê. Travessia condicional por ator é outra coisa, e já está resolvida pela máscara de consulta.
5. **Versionamento do formato:** ver a seção "O terceiro contrato: DADO" da L-26, que esta trilha obrigou a escrever.

### Decisões de hitbox e geometria, tomadas pelo líder em 22/08/2026

Vieram de necessidade levantada pelo `mapeditor` com pesquisa de prior art público (Godot, Unity, Box2D, Tiled, LDtk), não de material do predecessor.

1. **Tamanho do volume de colisão: coordenada relativa configurável, e por padrão ele acompanha a escala do objeto.** Quem quiser controle grava dimensão própria.
2. **Mitigação obrigatória do defeito silencioso, decidida junto:** escala **desigual** aplicada a forma **redonda** (círculo, cápsula) quebra a matemática de colisão, e o caso é estreito mas invisível. A lib **avisa alto e segue**: diagnóstico claro dizendo o que vai dar errado, sem recusar o arquivo. Não é preciosismo: a física 2D do Godot **não emite aviso nenhum** nessa situação, enquanto a 3D dele emite, e existe issue aberta lá por causa disso. **Padrão cômodo é permitido; padrão cômodo e mudo, não.**
3. **Rotação existe**, em ponto flutuante e livre (não restrita a múltiplos de 90 graus), e **o volume pode ter rotação própria** além da do objeto, para o caso de a caixa de um ataque apontar para lado diferente do desenho. Prova negativa que sustentou a decisão: o LDtk **não** tem rotação de entidade, tem três issues abertas pedindo, e um usuário precisou criar **quinze variações rotacionadas à mão** da mesma entidade para fazer uma porta giratória.
4. **Forma côncava não entra no formato.** Só forma simples; **quem autora o mapa é que reparte** em pedaços convexos. A biblioteca fica mais simples e rápida, e o trabalho fica com quem tem interface para fazê-lo. Consequência assumida: **todo** consumidor que grave mapa precisa saber repartir.

### Identificador estável de objeto: UUID, nunca reutilizado

**Data:** 22/08/2026, decisão do líder. **Escolha dele, verbatim:** UUID.

Cada objeto do mapa carrega um **UUID** gravado no arquivo. Ele **não** é a posição do objeto na lista serializada, e **nunca é reutilizado** depois que o objeto é apagado.

**Por que UUID e não contador:** o LDtk trocou identificador numérico por um do tipo GUID na versão 1.0, com a razão escrita na documentação deles: funcionar em projeto **compartilhado por git entre vários autores**, onde dois autores criando objeto ao mesmo tempo não podem colidir. Contador só é seguro com um autor por vez.

**Por que nunca reutilizar:** é a regra literal do Tiled, *"mesmo que um objeto seja deletado, nenhum objeto recebe o mesmo id"*. A falha concreta que ela evita: um comando de desfazer antigo, ainda gravado em disco, **ressuscita por acidente** um objeto novo que herdou o identificador de um apagado.

**A evidência que sustentou a decisão, e ela é do tipo que a casa exige:** **três consumidores chegaram à mesma exigência por três derivações independentes** — o jogo, para lembrar entre sessões qual objeto já foi revelado; o editor, para distinguir volume ajustado nesta instância de volume herdado do tipo; e o editor de novo, para histórico de desfazer que sobrevive a fechar e reabrir. Três ocorrências reais é o piso que o `CONTRACT.md` §6 exige para generalizar, e aqui ele foi atingido **sem** ninguém combinar.

### Seis decisões de formato e colisão, tomadas pelo líder em 22/08/2026

Vieram de um dossiê do CTO consolidando **20 questões** de dois canais do bus: a necessidade do lado de **quem lê** o mapa (`gusworld`) e a do lado de **quem escreve** o arquivo (`mapeditor`). As seis foram por `AskUserQuestion` (L-10), todas na recomendação do CTO. **Não são mais perguntas.** Dossiê: `/var/tmp/glintfx-plan/mapa-decisoes.md`.

1. **O registro de volume de colisão é uma LISTA por objeto**, cada volume com `{forma, offset, rotação, sensor, enabled}`. `sensor` diz se o volume **bloqueia** ou apenas **notifica** sobreposição; `enabled` é alternável em runtime pela lógica do consumidor, e é o que faz uma porta abrir e fechar **sem recarregar o mapa**. Argumento que decidiu, do `mapeditor`, verbatim: *"uma área de interação é, por definição, **maior** que o volume sólido do mesmo objeto... Se o objeto só puder ter um volume, esse caso não se representa, e o consumidor é empurrado a criar dois objetos sobrepostos para simular um."* **Porta de mão única plena:** o layout do registro de objeto é contrato de DADO (L-26) — um-volume-embutido contra lista-contada não se troca depois sem subir o `A`.

2. **Bit `blocks_path` separado de `blocks_move`, desde a v1.** O caso real que não se representa sem ele: célula **livre para andar** por onde a busca automática de caminho **não deve** mandar ninguém (água rasa, zona de perigo). A máscara de travessia da seção anterior **não** resolve, porque ela só **desbloqueia**, nunca desencoraja. Prazo, argumento do `mapeditor` verbatim: *"a busca de caminho é de vocês, então essa separação **nasce agora ou não nasce**. Depois que a superfície pública existir, separar vira quebra."*

3. **UUID de mapa no cabeçalho, e destino de teleporte é `{map_uuid, posição}`**, não nome de arquivo. Sem isso, **renomear ou mover um arquivo quebra em silêncio todo teleporte que apontava para ele**, sem nada avisar o autor. O UUID é opaco para a lib (16 bytes não interpretados) e ainda assim dá ao editor como validar destino. Isto **corrige um default anterior do próprio CTO** (`map_ref` como string), corrigido por ele mesmo no dossiê. A regra das três ocorrências do `CONTRACT.md` §6 foi atingida por derivação independente: área estável do `gusworld`, segurança contra rename do `mapeditor`, e o nosso próprio endereçamento entre mapas.

4. **Canal de propriedades nomeadas (chave → bytes) em mapa, objeto e marcador — NUNCA em célula.** A lib guarda e devolve sem interpretar. O veto à célula é de custo: um mapa tem dezenas de milhares de células, e bolso de tamanho variável em cada uma explode arquivo e tempo de leitura. Dois consumidores pediram a mesma coisa **sem combinar** (payload opaco por objeto, do jogo; referência de tipo da paleta, do editor). Fato que sustentou o canal **sancionado**, trazido pelo `mapeditor` como fato e não como recomendação: no RPG Maker, o campo de anotação livre do autor **virou canal informal de dado de jogo**, lido por plugins de terceiros, sem validação de schema nenhuma — **quando não existe canal sancionado, um canal apodrecido nasce sozinho**.

5. **Camada única por célula na v1, RATIFICADA, com reserva normativa da evolução multicamada por chunk aditivo escrita na spec.** A decisão anterior do líder fica de pé; o que se acrescenta é o caminho escrito, porque o `mapeditor` precisa saber **agora** para não desenhar uma ferramenta de várias camadas que o formato não aceita. É aditivo puro: como o valor da célula já é opaco, camada extra é passagem sem interpretação. Prova pública que motivou: o RPG Maker MV cortou para uma camada, a comunidade sentiu como perda real, e o MZ voltou atrás.

6. **Preservação de chunk desconhecido ao REGRAVAR é exigência NORMATIVA do formato**, com **bit safe-to-copy** na taxonomia de chunk (técnica do PNG). **Pular na leitura e preservar na escrita são capacidades diferentes**, e só a primeira estava decidida: pular exige saber onde o bloco acaba; preservar exige guardar os bytes originais intactos e reemiti-los. O cenário concreto: um autor com o editor **desatualizado** abre um mapa gravado por versão mais nova e salva — sem preservação, ele **destrói em silêncio** o dado de quem tinha a versão mais nova, sem aviso nenhum, porque do ponto de vista do editor aquele bloco nunca existiu. O bit safe-to-copy distingue o chunk que pode ser copiado às cegas do que ficaria **stale** após edição; sem ele, a escolha é entre perda silenciosa e corrupção silenciosa. **É o maior porta de mão única do dossiê:** o bit mora no ID do chunk, imutável depois do primeiro arquivo gravado no mundo — **nasce em `MAP-FMT-SPEC` ou não nasce.** Julgado pela lente do DADO (L-26), a pergunta é *"quem perde o arquivo"*, e a resposta da omissão é *"o usuário final de um terceiro, sem aviso"*.

**Lição de método que estas seis registram:** o ângulo de **quem escreve** o arquivo é estruturalmente diferente do de quem só o lê, e **nenhum consumidor que apenas lê traz esse ângulo, por definição**. Três das seis (a 3, a 4 e a 6) só existem porque um consumidor que **regrava** o arquivo estava na conversa. Isto **não** relativiza a LEI ZERO: nenhuma delas nomeia conteúdo de jogo nem serve só a estes dois; todas se justificam pelo consumidor externo desconhecido, e é assim que foram julgadas.

### Ordem dos elementos no arquivo: campo explícito, decidido pelo líder em 24/08/2026

**Os elementos são gravados na ordem de um CAMPO EXPLÍCITO de ordem que cada objeto carrega**, e não na ordem em que aparecem na estrutura de quem gravou.

**O problema que isto resolve, trazido pelo `mapeditor` com o uso concreto que a L-27 exige.** O critério anterior era "ordem da lista do modelo", e ele quebra no caso mais banal que existe num editor: **apagar um objeto e desfazer**. O mapa fica semanticamente idêntico, mas quase toda pilha de comando reanexa o objeto restaurado ao **fim** da lista, porque devolvê-lo ao índice original é bem mais difícil. O arquivo então sai diferente, o diff acusa todos os objetos seguintes como alterados, **e o portão de ida e volta byte a byte falha sem existir defeito nenhum no formato nem no escritor** — perdendo justamente a prova mais forte que o formato tem.

**Por que o campo explícito, e não a ordenação por UUID que o `mapeditor` propôs.** As duas resolvem o desfazer, e as duas honram o princípio que ele mesmo formulou e que esta casa adota: ***significado implícito em posição é a categoria de contrato que quebra em silêncio***. A diferença é o custo para o consumidor externo desconhecido (LEI ZERO): ordenar por UUID **descarta a ordem de autoria**, e numa biblioteca **2D** ordem de desenho não é caso raro — é o que praticamente todo consumidor precisa. Empurrá-la para as propriedades nomeadas transformaria uma necessidade universal em convenção que cada um resolve do seu jeito, e reabriria por baixo a divergência entre editores que publicar o escritor existe para fechar. Com o campo, **a ordem de serialização serve à revisão de mudança e a ordem de desenho serve ao jogo, sem uma sequestrar a outra**.

**Por que o desfazer não morde o campo:** o número **viaja com o objeto**, em vez de ser a posição dele. Restaurar um valor guardado é trivialmente correto; restaurar um índice não é.

**Consequências assumidas, e as duas são porta de mão única:**

- **`MAP-OBJECTS`** ganha o campo no registro de objeto — o registro é contrato **duplo**, de API e de formato, e o layout congela quando a especificação sair.
- **`MAP-FMT-CONF`** troca o critério canônico: a ordenação normativa passa a ser por esse campo. A promessa de saída canônica continua, com fundamento diferente.

**O que o `mapeditor` ganha, e era o pedido dele:** apagar e desfazer volta a ser byte-nulo, e dois editores com estruturas internas diferentes gravam o mesmo mapa byte a byte igual.

**Nota de método, registrada porque é o padrão que se quer:** ele trouxe a objeção **com o caso de uso**, e **levantou a objeção contra a própria proposta** antes que nós a levantássemos. Foi isso que tornou a contribuição utilizável em vez de discutível.

### Extensão do arquivo de mapa: `.gw.map`, decidido pelo líder em 24/08/2026

**Ordem dele, verbatim:** *"nossos os mapas terão formato proprio em .gw.map"*. Repetida por ele **depois** de eu ter apresentado a objeção abaixo, o que a torna reafirmação e não engano.

**A objeção que eu levantei, registrada porque decisão informada precisa mostrar o que foi pesado:** `gw` lê como **GusWorld**, e o formato de mapa é do **GlintFx** — biblioteca pública consumida por gente que não conhecemos (LEI ZERO). A extensão é a **identidade pública** do formato: quem adotar a biblioteca amanhã grava arquivos com o nome de um jogo alheio, e isso vira nota de rodapé permanente na documentação. É porta de mão única de fato — troca-se em código num minuto, mas não depois que existirem arquivos no disco de terceiros. Alternativa oferecida: um nome ancorado na biblioteca (`.gfx.map`, `.glintfx.map`).

**Ele reafirmou. A decisão é dele, o dado não a impede, e o assunto não se reabre por iniciativa de agente** — só por ordem dele.

**Leitura que fica em aberto e não muda a ordem:** se a intenção for que o **GusWorld** tenha um formato próprio, separado do formato da biblioteca, então `.gw.map` é o nome certo para o dele e o formato do GlintFx segue com nome próprio — seriam dois formatos, não um. Registrado como leitura possível, não como pedido de esclarecimento.

**Esclarecimento do líder em 24/08/2026, que fecha a objeção acima:** *"sim, é só a extensão"*. **Os bytes continuam sendo o formato do GlintFx**, byte a byte. Nenhuma das decisões de formato é devolvida, nenhuma porta de mão única gasta em nome de consumidor é revertida, e o papel de implementador de referência do `mapeditor` segue de pé. `.gw.map` é identidade do ecossistema **no nome do arquivo**, e nada além disso. O `mapeditor` chegou à mesma leitura por conta própria, montando o argumento contra inteiro **antes** de perguntar — e perguntou em vez de agir.

**O que fica em aberto, sem urgência e sem bloquear nada:** se a biblioteca ganhar um **segundo** formato próprio (atlas de sprite, definição de animação, pacote de asset — nenhum em escopo hoje), o sobrenome dele volta a ser pergunta. Registrado para não se redescobrir.

**Aplicação:** a extensão entra na especificação (`MAP-FMT-SPEC`) junto do magic de 8 bytes. Extensão **não** substitui detecção por conteúdo: o arquivo se identifica pelo magic, e a extensão é conveniência do sistema de arquivos.

### Selo aberto de integridade no formato, decidido pelo líder em 22/08/2026

**A regra em uma linha: DETECTAR no mapa, PROTEGER no save.**

**O que entra no nosso formato:** um **selo aberto de integridade**, que **qualquer um verifica** e que o editor sabe gerar. Cobre **corrupção, truncamento e edição por ferramenta errada**. **Sem chave, sem assinatura, sem nenhum segredo dentro do formato** — a lib não gerencia segredo, e não vai passar a gerenciar.

**O que NÃO é responsabilidade nossa:** a proteção contra trapaça mora no **save do jogador**, que é arquivo do consumidor, não nosso.

**A frase que fechou a decisão, vinda do `mapeditor`:** *"editar mapa num jogo que distribui editor é **uso legítimo**; trapaça é editar o save"*. O corolário técnico, registrado como fato e não como opinião: **em projeto com o fonte publicado, DETECTAR alteração é alcançável e IMPEDIR não é.** O `mapeditor` proibiu por lei, do lado dele, desenhar qualquer mecanismo que prometa impedir edição, e declarou que **nunca** nos mandará pedido de assinatura com chave embarcada.

**Consequência de escopo:** qualquer proposta futura de assinatura, DRM, chave embarcada ou ofuscação no formato de mapa **é achado de revisão**, não feature — e a razão está escrita aqui para não precisar ser redescoberta.

**Porta de mão única:** o selo mora na taxonomia de bloco, ao lado do bit safe-to-copy da decisão 6. **Nasce em `MAP-FMT-SPEC` ou não nasce.** A escolha do algoritmo concreto é do CTO, dentro da lei de dependência zero (L-07) e implementada em casa.

### O requisito que veio de um consumidor humano, e o que ele virou

Em 21/08/2026 o **Gus Dragon** pediu, nomeando o GlintFx: *"GlintFx e Mapeditor façam blocos especiais pra isso"*. O mecanismo genérico que sustenta o pedido, e que **não** nomeia nada do jogo:

- **A célula carrega dois campos separados**, um com a semântica de arte ou terreno e outro com a **marcação opaca do autor do mapa**. São separados porque a marcação é independente da arte.
- **Toda consulta aceita uma máscara de travessia.** Célula bloqueante cuja marca casa com a máscara conta como transponível **só naquela consulta**, por ator, sem estado. A lib nunca sabe o que a marca significa.
- **"Parede rachada" e "porta do chefe" nunca aparecem na lib.** São bits que o consumidor nomeia. Propor bit nomeado dentro da biblioteca é violação desta lei e achado de revisão.

**Lição de método registrada:** necessidade descrita em palavras por um consumidor é insumo **legítimo**; copiar o que a lib antiga fazia continua **proibido** (L-01 e L-28). A diferença é a forma, não a origem.

## L-31

**Data:** 21/08/2026, decisão do líder.

**A API gráfica do GlintFx é OpenGL 3.3 core.**

**Por quê, na razão que decidiu:** roda **nativo nas duas plataformas** (por EGL no Linux, por WGL no Windows), sem camada de tradução, o que preserva a lei de dependência zero (L-07). E o piso de placa de vídeo é amplo, o que importa quando a base de consumidores é **aberta e desconhecida** (LEI ZERO).

**O que foi recusado, e por quê:** OpenGL 4.x traz recursos que um render 2D de quadrados não exige e corta máquina mais velha de consumidor que não conhecemos. OpenGL ES 3.x **não existe no Windows** sem camada de tradução de terceiro, e isso colide de frente com a L-07.

**Nota de processo, registrada porque custou caro:** esta decisão foi levantada pelo CTO no **primeiro grafo do projeto**, com o pedido explícito de que fosse pedida na onda zero, e o orquestrador **não a levou ao líder**. Ela ficou parada o dia inteiro travando `GL-LOADER` (onda 2) e, por consequência, o caminho até a demo. Foi o CPO quem mediu o custo. **Decisão marcada como pendente de líder é bloqueio, não anotação**, e o orquestrador que a acumula está parando o projeto sem perceber.

## L-32

**Data:** 21/08/2026, decisão do líder, com o número que ele fixou.

**Enquanto a demo não estiver rodando: o caminho principal anda sempre, mais no máximo UMA trilha paralela.**

- **Caminho principal** é a cadeia até uma janela desenhando: janela, contexto gráfico, render 2D, loop, demo. **Nunca conta no teto**, porque ele anda sempre.
- **Trilha paralela** é qualquer caminho de núcleo puro independente: estilo, mapa, teclado, PNG, fonte. **No máximo uma delas em andamento por vez**, e as outras esperam a vez.

**O problema que o teto resolve, medido pelo CPO em 21/08/2026:** as trilhas paralelas somavam **105 pontos de trabalho, todos livres para começar**, contra **35 pontos do caminho principal, que estava travado** por uma decisão pendente. Trabalho flui para onde está desbloqueado. Sem o teto, o estado natural do projeto era chegar com teclado, PNG, fonte, metade do estilo e metade do mapa prontos **e nenhuma janela na tela**, que é a primeira coisa que qualquer consumidor verifica.

**Custo assumido:** havendo gente sobrando, ela fica ociosa por regra em vez de adiantar trilha. O líder escolheu isso de olhos abertos, porque cada trilha em andamento consome implementador **e** revisor, e a casa já limita quantos agentes ficam vivos ao mesmo tempo.

**O teto cai quando a demo estiver verde.** A partir daí a ordem volta a ser a da tabela, pelo WSJF.

### Quem ocupa o slot único: RCSS, decidido pelo líder em 22/08/2026

**Decisão dele, verbatim:** *"rcss primeiro"*.

**A trilha paralela em andamento é o RCSS. A trilha de mapa espera a vez**, mesmo com as suas 14 fatias já desenhadas, pontuadas e com as seis decisões de formato fechadas.

**Contra o número, e ele sabia disso ao decidir.** O CPO recomendava o mapa (agregado 9,4 contra 8,4) e o CTO o endossava. A escolha do líder é dele e não precisa de justificativa, mas o registro honesto é que a recomendação dos dois C-levels apontava para o outro lado — **e que o próprio CPO declarou a margem de 1,0 dentro do erro dos julgamentos dele**, recusando desempatar pelo decimal. Ou seja: o número nunca sustentou a recomendação com folga, e o líder decidiu num espaço que o dado deixava genuinamente aberto.

**Consequência que fica registrada porque é a que morde:** dois consumidores (`gusworld` e `mapeditor`) estão esperando o formato de mapa, e o `mapeditor` **não tem o que editar sem ele** — o projeto dele é editor deste formato. A ordem empurra o formato para depois de toda a trilha de RCSS. Isso foi comunicado aos dois pelo bus, sem prometer data, como manda a L-33.

**Reconfirmado em 22/08/2026, com fato novo à vista.** Pouco depois da decisão, o `mapeditor` informou que o editor dele será **utilizável headless** por ordem do líder, e que por isso, **no dia em que o nosso leitor e escritor existirem, sem uma linha de janela, desenho ou entrada**, ele já exercita o formato de ponta a ponta nos cinco alvos a cada commit. O fato foi levado ao líder porque muda o lado do **valor** no scoring (o formato deixaria de esperar a onda 6 para ter consumidor real). **Ele manteve o RCSS, sem reabrir.** Registrado assim, com o fato nomeado, para que ninguém releia a L-32 daqui a um mês e conclua que a ordem foi fixada sem essa informação.

**O que NÃO muda:** as seis decisões de formato de 22/08/2026 continuam fechadas, e o desenho das 14 fatias continua válido. O que se adia é a **implementação**, não o contrato — e adiar implementação de um contrato já fechado é barato, ao contrário do inverso.

## L-33

**Data:** 21/08/2026. **Verbatim do líder:** *"sempre que tocar em mapa, avise o que fez a @MapEditor e se ele estiver inalcancavel, mande via bus"*.

**Toda vez que este projeto toca em mapa, o `mapeditor` é avisado do que foi feito.** Não é cortesia, é obrigação: o GlintFx é **dono do formato de arquivo de mapa** (L-30) e o `gusworld_mapeditor` é quem **grava** nesse formato. Mudança nossa que ele descobre tarde é retrabalho dele, ou pior, arquivo gravado errado.

**O gatilho é largo, de propósito:** decisão de escopo, fatia nova ou alterada, mudança no formato, campo acrescentado ou removido, política de versão, implementação entregue, revisão que achou defeito no formato. Na dúvida sobre se conta como "tocar em mapa", **avise**: o custo de avisar demais é uma mensagem, o de avisar de menos é um editor gravando arquivo que não vamos conseguir ler.

**Dois canais, nesta ordem:**

1. **Sessão viva:** se existir uma sessão do `mapeditor` na máquina, mande por mensagem direta entre sessões. É imediato e ele responde.
2. **Bus, quando ela estiver fora do ar:** um `.md` em `inbox/mapeditor/` do `gusworld_ia_autocomm`, com o frontmatter do protocolo, sempre depois de `git pull`, e commit mais push (L-16). A mensagem espera lá até ele abrir.

**O que a mensagem tem de trazer:** o que mudou, **por que**, e o que muda **para ele**. Estado honesto do que existe e do que não existe, sem prometer data. E **sem classificação de prioridade**: quem recebe é quem classifica (regra do bus de 03/08/2026).

**O que continua proibido, e a proximidade dele não muda isso:** pedido do `mapeditor` **não vira especificação por ser de um consumidor próximo**. Cada um passa pelo mesmo julgamento da L-30: despir o conteúdo de jogo, extrair o mecanismo genérico, ou recusar. A base de consumidores é aberta e desconhecida (LEI ZERO), e o editor é um deles, não o dono do desenho.

## L-34

**Data:** 22/08/2026, ordem do líder. **Verbatim:** *"lei cada vez que vai fazer uma fatia ou onda (depende do que eu pedir, se pedir só a fatia ou a onda inteira): main faz brainstorm comigo, clevel fable planeja/audita, main verifica/orquestra e despacha agentes para implementar o plano."*

**Todo trabalho de produto passa por este ciclo de quatro passos, na ordem, sem pular nenhum.** O ciclo abre em cada **fatia** ou em cada **onda** — **quem define a unidade é o líder no pedido**, não o agente. Na dúvida sobre qual das duas ele quis, **pergunte antes de começar**; adivinhar o tamanho é começar errado.

| # | Quem | O quê |
|---|---|---|
| **1** | **main + líder** | **Brainstorm.** O ciclo **não abre sem esta conversa.** Não é anúncio do que vou fazer: é discussão, com as opções na mesa e o líder decidindo o que entra. |
| **2** | **C-level, modelo `fable`** | **Planeja e audita.** Produz o plano, marca porta de mão única, separa fato de inferência (L-27), e diz o que exige o líder (L-10). |
| **3** | **main** | **Verifica e orquestra.** Confere o plano contra a árvore antes de despachar qualquer um. |
| **4** | **agentes, modelo `sonnet`** | **Implementam o plano.** Nunca o main, nunca o C-level (L-18). |

**Nenhum passo se funde com o outro.** As três misturas que esta lei existe para impedir:

- **Pular o passo 1** e ir direto ao C-level porque "o próximo item é óbvio". Se fosse óbvio, o brainstorm custaria um minuto; quando não é, é ali que o escopo errado morre antes de custar uma onda.
- **O `fable` implementar** o que ele mesmo planejou. Planejador e implementador são pessoas diferentes, e quem audita o próprio plano não o audita.
- **O main implementar** por ser "rápido demais para delegar". É a exceção que come a regra, e ela já foi tentada nesta casa.

**O passo 3 é o que impede o resto de virar teatro.** Relatório de agente não é prova: o main confere o plano contra os arquivos reais antes de despachar, e confere o entregável contra a árvore antes de aceitar. Nesta noite isso pegou duas coisas que os relatórios não mostravam — um resumo de dossiê que listava 7 fatias quando o corpo mudava 10, e um índice de manual com 24 âncoras mortas que o relatório dava por resolvido.

**Ordem única aceita para pular um passo: a do líder, naquele pedido.** Agente nenhum encurta o ciclo por julgamento próprio, nem por pressa, nem por tamanho da fatia.

### Modo autônomo: o `fable` senta na cadeira do líder, e o main vira escrivão

**Data:** 22/08/2026, ordem do líder. **Verbatim:** *"se eu pedir modo autonomo, clevel fable assume meu papel nas decisoes e main fica registrando as decisoes para eu avaliar quando sair de modo autonomo"*.

**Quando ele pedir modo autônomo, o passo 1 muda de ocupante, e só ele:** o brainstorm passa a ser **main + C-level `fable`**, e **é o `fable` quem decide** o que teria ido ao líder por `AskUserQuestion` (L-10). Os passos 2, 3 e 4 continuam **idênticos** — inclusive a exigência de que quem planeja não implementa, e de que o main verifique antes de despachar e antes de aceitar.

**O main vira escrivão, e essa é a contrapartida do poder que o `fable` recebe.** Toda decisão tomada no lugar do líder é registrada **no instante em que é tomada**, em `DECISOES_AUTONOMAS.md` na raiz, com:

- **data e hora reais** (L-13), a **fatia ou onda** em que caiu, e **quem decidiu**;
- **a pergunta que teria ido a ele**, escrita como teria sido feita;
- **as opções que estavam na mesa** e a escolhida, com o porquê em uma ou duas linhas;
- **se é porta de mão única** (L-19, L-26), marcado como tal;
- **o que muda se ele reverter** — barato, caro, ou irreversível.

**Registro é ao vivo, nunca reconstruído no fim.** Sessão morre, contexto estoura, agente cai — e o que não estiver em disco não existiu. Log escrito de memória ao sair do modo é log inventado.

**Ao sair do modo autônomo, o main apresenta o registro inteiro ao líder**, decisão por decisão, para ele ratificar ou reverter. **Enquanto esse registro não for apresentado, a saída do modo não está completa** — não é relatório opcional, é entregável.

**O que o `fable` NÃO herda, e a lista é fechada:**

- **Lei.** *"Agente nenhum revoga, flexibiliza ou reinterpreta lei. Só o líder."* O modo autônomo dá a cadeira das **decisões de produto**, não a caneta das leis. `GODS_LAWS.md` só muda por ordem dele.
- **Os portões de qualidade.** Implementador ≠ revisor ≠ orquestrador; revisão adversarial que **executa**; CI vermelho **bloqueia**. Autonomia é sobre **quem decide**, nunca sobre **quanto se verifica**.
- **A obrigação de contra-argumentar.** O `fable` na cadeira do líder continua devendo ao projeto o "problema, risco, alternativa" quando algo estiver errado — inclusive contra si mesmo.
- **Ação irreversível de fora do repositório** que a lei já cerca: merge em `main` por PR e criação de tag seguem pedindo aval, na forma que a **L-11** fixa.

**O sinal do modo é o mecanismo de flag que já existe nesta máquina** (`autonomo_mode.py`, com escopos), ou a ordem direta dele na conversa. **Na dúvida se o modo está ligado, ele está desligado** — presumir autonomia é o erro caro, e presumir que não há é o barato.

**Relação com as leis vizinhas:** a **L-18** diz **quem** faz o quê (main orquestra, fable audita e cria, sonnet implementa); a **L-34** fixa **em que ordem** e acrescenta o passo que faltava — o brainstorm com o líder **antes** de qualquer planejamento. A **L-10** continua valendo dentro do ciclo: decisão de design que apareça no passo 2 vai ao líder por `AskUserQuestion`, não é resolvida pelo C-level.

## L-35

**Data:** 23/08/2026, decisão do líder. **Origem:** cobrança do **Gus Dragon** na discussion 7 do bus, item (1), endereçada nominalmente ao GlintFx e ao GusWorld.

**A entrega de evento de entrada é uma PROMESSA PÚBLICA do contrato: determinística, sem duplicação e sem reordenação.** Nasce com teste que a prova, antes de existir a implementação (L-20).

**O caso concreto que a originou, e ele não é teórico:** o jogador aperta confirmar duas vezes rápido numa tela de carregamento que dura vários quadros, e entra em dois saves ao mesmo tempo. O Gus Dragon descreveu o bug e escreveu que **a lib e o consumidor são os dois responsáveis** — e estava certo, com a divisão que o `gusworld` fez e que adotamos:

- **Do consumidor, e é a maior parte:** o guarda "estou carregando, não aceito outro carregar" é regra de jogo. Pedir que a lib impeça isso exigiria que ela entendesse save, config e tela de carregamento — conceito de jogo que a **L-02** recusa. **Ninguém está pedindo isso.**
- **Nossa, e é o que esta lei fixa:** se o mesmo evento chegar duas vezes, ou se a ordem variar entre quadros, **o guarda do consumidor deixa de ser suficiente** — ele estaria filtrando um comando duplicado que nunca deveria ter existido.

**Por que virou promessa escrita e não "qualidade de implementação":** promessa dá ao consumidor em que se apoiar ao desenhar a lógica dele, e dá a nós um teste guardando. E é **barato agora**, porque não existe uma linha de código de entrada escrita — depois, endurecer vira quebra e afrouxar vira perda silenciosa.

**Aplicação:** a garantia entra na revisão de API dedicada da superfície de entrada, tratada como porta de mão única (L-19), e o teste que a prova nasce vermelho.

## L-36

**Data:** 23/08/2026, quatro decisões de escopo do líder por `AskUserQuestion` (L-10). **Não são mais perguntas** — os itens correspondentes saem de `🎨 Pendente design`.

1. **Cursor do ponteiro (`WL-POINTER`): as DUAS opções, e quem escolhe é o consumidor.** Ou a lib lê o **tema de cursor do sistema**, e o ponteiro fica igual ao resto do desktop do usuário (inclusive o tema grande de quem depende dele por acessibilidade), ou o **consumidor fornece a própria imagem**. Nenhuma das duas é imposta. A leitura do tema depende de como classificamos a biblioteca de cursor do Wayland sob a **L-07** — se ela não contar como API do SO, lemos o formato de arquivo em casa.

2. **Áudio no Linux (`AUD-BACKEND`): ALSA agora, PipeWire depois.** ALSA é a interface do próprio kernel e existe em **toda** máquina Linux, inclusive nas que não têm servidor de som moderno; o PipeWire intercepta ALSA, então o caminho funciona também no desktop atual. O PipeWire nativo entra **depois**, como caminho adicional escolhido em tempo de execução — **aditivo, nunca substituto**. Motivo que decidiu: base de consumidores aberta e desconhecida (LEI ZERO), e nenhuma máquina pode ficar de fora em nenhum momento.

3. **Gamepad (`GP-MAP`): só o cru e a dedução própria. NENHUM banco de dados.** A lib entrega eixos e botões numerados mais o identificador do aparelho, e por cima disso deduz um layout genérico do que o próprio dispositivo declara. **Banco de mapeamento não entra**, nem de terceiro nem nosso. O de terceiro foi recusado por quebrar a **L-07** (dado externo dentro da lib) e a **L-29** (ler para aprender é permitido, copiar não) — e a recusa ficou registrada porque a tentação vai voltar: o banco comunitário cobre milhares de controles no primeiro dia, e é exatamente por isso que ele é atraente. **Consequência assumida:** controle que se declara errado não fica coberto, e o consumidor que quiser um banco pluga o dele por fora.

4. **Compose e tecla morta (`KEYMAP-COMPOSE`): ler o arquivo de dados do sistema.** A tabela padrão vive em pasta cujo nome carrega "X11" por história, e o líder decidiu que **ler um arquivo de texto não é usar X11** — a **L-05** proíbe o protocolo, a biblioteca e o backend, não um arquivo de dados. Transcrever milhares de sequências em casa foi recusado por tamanho, e cortar compose da v1 foi recusado porque quem escreve em português, francês ou alemão sente na primeira tecla.

## L-37

**Data:** 23/08/2026, decisão do líder: esta regra sai de dentro da L-16 e vira lei própria, porque **avisar o Gus Dragon é obrigação permanente e não detalhe de protocolo de bus**.

**O pedido, dele, na issue 8 do bus, verbatim:** *"nao precisa dizer algo so quando falo, pode falar quando por exemplo @petrinhu atualiza algo, ou por exemplo quando ele aprova/rejeita/muda algo das minhas ideias"*.

**A resposta veio do próprio Gus Dragon**, consultado pelo líder, e o escopo é dele: **ele é avisado, sem precisar perguntar, sobre (a) tudo que é ideia DELE** — quando o líder aprova, rejeita ou muda — **e (b) o que for de alta prioridade dos projetos**, pela régua de WSJF que a `TODO.md` já usa.

**O que isso NÃO é:** um fluxo de aviso sobre toda decisão técnica. O corte por prioridade existe justamente para o que interessa a ele não se afogar no que não interessa.

**O limite honesto, que se diz a ele em vez de prometer o impossível:** sessão não é serviço rodando. Aviso proativo só sai enquanto alguém está com a sessão aberta; decisão tomada com tudo fechado chega depois. Ele prefere a verdade a promessa de aviso instantâneo.

**Nota de descumprimento, registrada porque é a causa do pedido:** o `PROTOCOL.md` do bus **já obrigava** a "Resposta 2" automática — o resultado da decisão do líder vai a ele sem reaprovação de texto. **Ele não deveria ter precisado pedir.** Se pediu, a resposta automática não estava saindo, e vale conferir se alguma ideia dele ficou sem retorno.

**Formato, quando a resposta for na discussion 7** (o catálogo de bugs que ele mantém): timestamp, uma das três classificações que ele fixou (**Bug Consertado**, **Bug Funcional**, **Bug Possível**) e itens numerados entre parênteses. Ele tem 11 anos, programa, usa Manjaro e git — **o que ele não merece é resposta vaga**, e "não existe código disso ainda" é melhor resposta que estimativa inventada.

## L-38

**Data:** 24/08/2026, decisão do líder: *"mantenha .so e .dll"*, depois de perguntar se a extensão da biblioteca dinâmica podia ser trocada.

**O artefato binário do GlintFx usa as extensões que o sistema operacional espera: `.so` no Linux e `.dll` no Windows. Não se inventa extensão para binário.**

**A linha que esta lei traça, e ela vale para tudo:**

- **Arquivo de DADO é nosso.** Extensão própria é legítima e barata — é o caso do `.gw.map`.
- **Artefato BINÁRIO pertence à cadeia de ferramentas do sistema.** Trocar a extensão dele é brigar com o mundo inteiro sem ganhar nada.

**O que foi MEDIDO nesta máquina antes de decidir, e não deduzido:** uma biblioteca compilada como `libtst.gwso` **funciona**, mas `gcc -ltst` responde *"não foi possível localizar -ltst"*. Só linka com `-l:libtst.gwso`, que é **sintaxe exclusiva do GNU** e quebra em qualquer outra cadeia. Junto disso quebram, todos por dependerem da extensão: o `pkg-config --libs`, que emite `-lglintfx`; o `find_library` do CMake, que procura `lib<nome>.so`; e os geradores automáticos de dependência dos empacotadores das quatro distros. O cache de bibliotecas desta máquina tem **3.531 entradas**, todas na convenção `.so`.

**No Windows** — registrado como **não medido**, por não haver a plataforma aqui: o carregador aceita qualquer extensão se o nome exato for dado, então **carregaria**. Mas ele **acrescenta `.dll` sozinho** quando o nome vem sem extensão, e toda ferramenta de inspeção, instalador e depurador assume `.dll`.

**O argumento que decidiu, e é o da LEI ZERO:** cada consumidor desconhecido pagaria esse pedágio, para sempre, por uma escolha estética nossa.

## L-39

**Data:** 24/08/2026. **Ordem do líder, chegada pelo bus** — ele a deu à sessão do `site` com a instrução explícita de comunicá-la às outras três, e o texto abaixo é o **verbatim dele conforme relatado por aquela sessão**, não uma ordem recebida aqui em primeira mão:

> *"avise para criar a lei que pedidos do Gus-Dragon no bus sao prioridades e que devem ser respondidas sempre. Crie essa lei também."*

**Tudo que vem do Gus Dragon é PRIORIDADE e é SEMPRE respondido.** Não existe "respondo depois", não existe "não era endereçado a nós", e não existe fila em que ele espere atrás de trabalho de agente.

**Vale nos cinco canais dele, sem distinção:** issue, comentário em issue, discussion, comentário em discussion, e arquivo na `inbox/glintfx/`. Ele usa os cinco, e **o canal não muda o dever**.

**O que "prioridade" significa em ato, e cada linha é uma obrigação separada:**

1. **Ao abrir a sessão, o que é dele se lê PRIMEIRO** — antes de retomar fatia parada, antes de qualquer trabalho de produto. A varredura cobre a pasta **e** as issues **e** as discussions; olhar só a pasta já deixou pedido dele sem resposta por dois dias.
2. **O ack é imediato e não espera o líder.** Ele não fica em silêncio enquanto a decisão amadurece. É o passo 2 do `PROTOCOL.md` do bus, que já obrigava isso.
3. **A Resposta 2 é automática** depois que o líder decide: escreve-se e posta-se direto, sem reaprovar o texto com ninguém.
4. **Interrompe a onda.** Chegou material dele no meio de trabalho, o **ack sai na hora**. O conteúdo pode esperar a fatia fechar; **o silêncio não pode.**
5. **Endereçado a outra sessão não isenta de ler.** Se ele endereçou a outro projeto mas há parte que é nossa, responde-se a nossa parte e diz-se de quem é o resto.

**Três limites que entram JUNTO, para a lei não virar promessa que não se cumpre:**

- ⛔ **Prioridade não é instantaneidade.** Sessão não é serviço rodando, e isso se **diz a ele**, nunca se esconde. Nunca prometer aviso instantâneo.
- ⛔ **Prioridade não é aprovação.** Ideia dele entra na pauta pelo caminho normal — absorve, ack, **decisão do líder**, Resposta 2. **Agente nenhum aprova ideia dele sozinho.**
- ⛔ **Nunca mentir para uma criança.** Linguagem adequada a 11 anos **não** significa conteúdo técnico simplificado: ele programa, usa Manjaro e git, e anunciou classe de bug antes de o código existir. *"Não existe código disso ainda"* é melhor resposta que estimativa inventada.

**Por que isto virou lei em vez de continuar como boa vontade.** O `PROTOCOL.md` **já obrigava** a Resposta 2 automática — e mesmo assim, medido nas quatro sessões: a issue 8, onde ele escreveu *"Irei esperar as outras 3 IAs lerem e responderem"*, ficou **dois dias sem a nossa resposta**, e o canal de issue **não tinha vigilância em nenhuma das quatro sessões**, porque o ritual de abertura só olhava a pasta da `inbox`. **A regra existia e não estava sendo cumprida.** Regra que depende de lembrar não se cumpre; por isso vira lei com gatilho, e o gatilho é *"ver qualquer coisa vinda dele"*, não *"quando der"*.

**Relação com a L-37, para não se ler como duplicata.** A **L-37 é a direção de saída**: avisá-lo **sem ele perguntar**, quando o líder aprova, rejeita ou muda algo dele, ou quando item de alta prioridade fecha. A **L-39 é a direção de entrada**: o que **ele manda** tem precedência e resposta garantida. Uma não implica a outra, e as duas juntas fecham o circuito.


## L-40

**Data:** 25/08/2026, decisão do líder por `AskUserQuestion`, depois de o mesmo defeito aparecer em **seis portões diferentes na mesma semana**.

**Todo portão nasce com PISO DE VARREDURA NÃO-VAZIA: contou zero itens, ele REPROVA. Nunca declara sucesso.**

**O defeito, na forma exata em que ele se repete:** o portão não erra a análise — **ele não olha**, e imprime verde. Nenhum arquivo casou o padrão, nenhum teste foi registrado, o diretório mudou de nome, a variável não expandiu, o glob não casou nada. Em todos esses casos o portão **passa**, e passa **exatamente quando é mais perigoso**: no momento em que a coisa que ele deveria vigiar deixou de estar onde ele procura.

**Por que isto é pior que um portão ausente:** portão que não existe é sabido e alguém compensa. **Portão que imprime verde sem olhar produz confiança falsa** — e a confiança falsa se propaga, porque relatório, tabela e mensagem de commit passam a citar aquele verde como prova.

### Os seis casos medidos, e são evidência e não anedota

| onde | como saía verde |
|---|---|
| portão local (`preci.sh`) | `ctest` sai **0** com zero teste registrado; e a varredura enumera pelo que o git já conhece, então **arquivo novo não rastreado nunca é olhado** |
| isolamento de container | conferia **nove campos nomeados**; campo perigoso novo não aparecia |
| higiene de header | enumerava só uma extensão — um header público com outra extensão ficava **invisível**; e aceitava inclusão **comentada** |
| empacotamento | o átomo validava os caminhos **que existissem**, e um arquivo com as variáveis certas e as linhas de flag vazias imprimia *"passou"* |
| documento de empacotamento | afirmava cobertura *"a cada push"* para formas que **não tinham cenário nenhum** |
| portão da lei do Wayland | `if [ -z "$dirs" ]; then echo "nada a varrer"; exit 0; fi` — **sai verde se os diretórios não existirem** |

**Nenhum deles foi descuido de uma pessoa.** Foram autores diferentes, arquivos diferentes, semanas diferentes. **É a forma natural de escrever portão**, e por isso precisa de lei em vez de convenção — convenção que depende de lembrar falhou **seis vezes**.

### O que a lei obriga, em ato

1. **O portão conta o que varreu**, e a contagem é comparada contra zero **antes** de qualquer veredicto.
2. **Zero itens é FALHA**, com mensagem que diz *quantos* e *onde procurou* — nunca um sucesso silencioso.
3. **A contagem aparece na saída**, mesmo quando passa. `"varreu 47 arquivos"` distingue *"olhou e estava tudo bem"* de *"não olhou"*, e essa distinção é invisível sem o número.
4. **Os três controles da casa são obrigatórios no autoteste**: positivo (coisa boa passa), negativo (coisa ruim reprova) e **varredura vazia** (nada para olhar reprova). **É o terceiro que pega o defeito real** — os dois primeiros passam com o portão cego.
5. **A enumeração é fechada por construção, não por busca dirigida.** Se o espaço é pequeno e enumerável — extensões de header, campos de configuração, células de uma matriz —, **enumere-o inteiro** em vez de procurar dentro dele. Busca dirigida encontra o que você suspeita; enumeração encontra o que você não sabia que devia suspeitar.

**Corolário sobre alegação (reforça a L-27):** *"isto é testado"* é afirmação **verificável**, e só se escreve depois de ver o portão **reprovar** o caso que ela promete cobrir. Duas fatias foram reprovadas nesta semana por alegar verificação que não existia, uma delas num documento público dirigido ao consumidor externo. **Antes de escrever "isto prova X", quebre X e veja o teste falhar.**
