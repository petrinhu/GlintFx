# ESCOPO.md — o que o GlintFx É

> **Registro do produto, não da conduta.** `GODS_LAWS.md` continua sendo a autoridade sobre COMO o agente trabalha (agente especialista, TDD, `AskUserQuestion`, portões de qualidade). Este arquivo é a autoridade sobre O QUE o GlintFx É: nome, formato, plataforma, API pública, versão, licenciamento. **Nasceu em 26/08/2026, ordem do líder, verbatim:** *"As god laws são leis minhas para você executar ao fazer o projeto, não decisões sobre o projeto."*
>
> **Proveniência de cada seção:** cada decisão preserva data e citação de origem (número da lei em `GODS_LAWS.md`, ou linha do `TODO.md`, ou arquivo de planejamento) — L-27 (fato separado de inferência) vale aqui como em qualquer outro documento. Onde este arquivo diz "migrado", a lei de origem em `GODS_LAWS.md` ficou reduzida a um ponteiro.
>
> **Blocos marcados `<!-- DUP-BLOCK:ID:START/END -->` são DUPLICADOS por inteiro** — o texto entre as âncoras é idêntico, byte a byte, ao que está em `GODS_LAWS.md`. Dois motivos de duplicação, distintos e marcados como tal em cada bloco: (a) **decisão do líder** (L-05 Wayland, L-07 dependência zero — ele escolheu duplicar em vez de apontar, sabendo que duas cópias divergem se só uma for editada); (b) **dúvida de classificação do orquestrador, NÃO decisão do líder** (L-06, L-19, L-22, L-26, e a `LEI ZERO`) — a regra aplicada nestes cinco casos foi *"onde houver dúvida genuína, DUPLICA"*, para o líder poder desfazer depois sem que nada se perca enquanto isso. `tests/tools/check_dup_laws.sh` prova que as duas cópias de cada bloco continuam iguais; se um dia divergirem, o portão reprova.

---

## §1 — Identidade e distribuição

### O que é o GlintFx

**Origem:** `GODS_LAWS.md` L-02, 21/08/2026, via `AskUserQuestion`.

O GlintFx é **biblioteca/framework reutilizável**, não aplicação final. O entregável é a **API pública, os headers e o pacote CMake**. Domínio: framework 2D completo (janela, loop, render2d, input, gamepad, áudio, fonte, asset, math2d); o consumidor escreve só a lógica dele.

### C++23 + CMake

**Origem:** `GODS_LAWS.md` L-03 (migrada inteira), 21/08/2026, via `AskUserQuestion`.

**C++23 + CMake.** Sem exceção de linguagem sem ordem do líder.

### Cinco plataformas, Fedora 44 primário, CachyOS próprio

**Origem:** `GODS_LAWS.md` L-04, 21/08/2026, verbatim: *"Fedora, Ubuntu, CachyOs (proprio, nao um arch renomeado), Arch, Windows"* e, no mesmo dia, *"nosso OS principal é o fedora, na versao que eu uso"*.

Cinco alvos, **cinco entradas distintas na matriz de CI**. **Fedora 44 é o alvo primário**, por ser o sistema do líder: ⚠️ **O pino foi SOLTO em 27/08/2026, por ordem do líder** (verbatim: *"solte para latest estável"*, e depois *"ponha todos como latest e instale a ferramenta nos que precisarem"*): **todas as imagens da matriz passam a ser `latest`**. **A razão de o pino existir continua valendo** — o alvo primário tem de falhar quando a máquina dele falharia —, e ela **continua satisfeita**: foi medido em 27/08/2026 que `fedora:latest` **é a mesma versão da máquina dele**, com a mesma ferramenta de build na mesma versão. **O que muda é quem acompanha:** antes alguém tinha de subir o número à mão quando ele atualizasse; agora o alvo primário acompanha sozinho. ⚠️ **O risco que isso assume, declarado:** o dia em que o Fedora lançar uma versão nova, o CI passa a testá-la **antes** de a máquina dele subir — e o alvo primário deixa de espelhar a máquina dele naquele intervalo. **Se isso morder, o conserto é voltar a pinar**, e a decisão é dele. **CachyOS não é Arch renomeado** e não é coberto pelo job de Arch: toolchain, flags de otimização, kernel e empacotamento diferem.

### Wayland puro, sem X11 (decisão do líder de duplicar)

**Origem:** `GODS_LAWS.md` L-05, 21/08/2026. Duplicada por DECISÃO EXPLÍCITA do líder (não dúvida do orquestrador) — a opção apresentada a ele dizia que duas cópias divergem se só uma for editada, e ele escolheu duplicar mesmo assim.

<!-- DUP-BLOCK:L05-WAYLAND:START -->

**Data:** 21/08/2026, verbatim: *"no linux, usaremos apenas camada wayland, sem x11"*.

Sem backend X11, sem fallback por XWayland, sem Xlib, XCB, XTest ou framebuffer X11 no repositório, nem em produção nem em teste. Usuário em sessão X11 **não é público alvo**, por desenho.

**Aplicação:** exemplo de internet que usa X11 não serve aqui, nem "só para o teste". Se a única forma conhecida de fazer algo é X11, isso é assunto para o líder, não contorno.

<!-- DUP-BLOCK:L05-WAYLAND:END -->

### Dependência zero (decisão do líder de duplicar)

**Origem:** `GODS_LAWS.md` L-07, 21/08/2026. Duplicada por DECISÃO EXPLÍCITA do líder, mesmo caso de L-05 acima.

<!-- DUP-BLOCK:L07-DEPZERO:START -->

**Data:** 21/08/2026, via `AskUserQuestion`.

**Zero dependência além da biblioteca padrão de C++23 e das APIs do sistema operacional.** Sem gerenciador de pacote de terceiros, sem `FetchContent` de biblioteca externa, sem vendorizar.

Fronteira registrada: `libwayland-client` conta como API do sistema (mesma categoria de Win32); `libxkbcommon` **não** conta (ver L-06). Mudança nessa fronteira é do líder.

**Aplicação:** decode de imagem, rasterização de fonte, loader de GL, mixagem de áudio e decode de gamepad são escritos em casa. Ao topar com uma lacuna, **pare e pergunte**; não improvise dependência "só por enquanto".


**EXCEÇÃO Nº 1, aberta pelo líder em 26/08/2026 — o registro do OpenGL (`gl.xml`, Khronos Group).** Verbatim dele, escolhendo entre três saídas apresentadas: *"opcao A"*.

**O que entra, e o escopo é ESTREITO de propósito:** **um arquivo de DADO, lido em tempo de build por script nosso, nunca linkado ao binário.** Dele saem as ~341 declarações de função do OpenGL 3.3 core (número medido, não estimado). **Não é biblioteca, não é código de terceiro executando dentro do nosso, e a exceção não se estende a mais nada.**

**Por que precisou de exceção, e o precedente do Wayland NÃO se aplica:** o `xdg-shell.xml` **vem instalado com o sistema**, então nós o lemos de onde ele já está e **não redistribuímos nada**. O registro do OpenGL **não existe em pacote de nenhuma das cinco plataformas** — medido em 26/08/2026. Redistribuir é o que cria obrigação, e é exatamente essa a diferença entre os dois casos.

**As três saídas que o líder pesou:** (a) vendorizar o arquivo e gerar por máquina; (b) transcrever as 341 assinaturas à mão, sem exceção à lei, ao custo de erro humano que **só aparece como travamento em tempo de execução**; (c) ler o cabeçalho já instalado no Linux, que **não existe no Windows** e deixaria dois mecanismos para manter. Escolheu (a): a única que serve as cinco plataformas **e** gera mecanicamente.

**Parecer do CLO, verificado na fonte em 26/08/2026 — sem impedimento:** `SPDX-License-Identifier: Apache-2.0`, confirmado no cabeçalho do arquivo oficial. Compatível com AGPL-3.0 por **mecanismo duplo**, qualquer um bastando: a FSF declara a Apache-2.0 compatível com a GPLv3, e as obrigações dela cabem inteiras na §7 da AGPLv3; além disso, arquivo de dado de entrada de build configura **agregação** (§5), que não dispara copyleft. **O repositório do Khronos não tem `NOTICE`**, então a obrigação §4d não existe aqui — registrado para ninguém inventar um nem travar a fatia procurando o que não há.

**As quatro obrigações, que são pré-condição da implementação:** (1) o texto integral da Apache-2.0 entra junto do arquivo — **o `LICENSE` da raiz NÃO é tocado, continua AGPL-3.0 puro**; (2) o cabeçalho de copyright do arquivo fica **intacto**, nunca "limpo"; (3) o arquivo entra **verbatim, byte a byte**, e não se modifica — o que elimina por construção a obrigação de marcar modificação; (4) morada em `third_party/khronos/`, com proveniência escrita: URL, commit de origem, data e `sha256`.

⚠️ **A pergunta que mais preocupava, e a resposta:** a saída gerada **não contamina nada**. Não muda a licença da biblioteca, não põe obrigação no consumidor, não exige licenciamento duplo. **O fato que mais ajuda aqui é do próprio Khronos:** o cabeçalho que **eles** geram a partir do mesmo `gl.xml` sai rotulado **MIT** — licença diferente e mais permissiva que a do insumo. Quem detém o copyright do registro não trata a saída como algemada a ele. Ainda assim, pela postura conservadora e ao custo de três linhas, cada arquivo gerado leva no topo: que é gerado em build, a atribuição ao Khronos com o identificador da licença, e que o **gerador** é nosso.

**Onde a análise do CLO para, dito por ele:** orientação técnica, não parecer vinculante. Advogado seria necessário para **afirmar juridicamente que a saída gerada está livre de obrigação** — a questão de derivação de API não é assentada —, para licenciamento comercial duplo da biblioteca, ou para disputa de marca. **A recomendação contorna a questão em aberto; não a resolve.**

<!-- DUP-BLOCK:L07-DEPZERO:END -->

### Público, AGPL-3.0

**Origem:** `GODS_LAWS.md` L-08, 21/08/2026, via `AskUserQuestion`.

Repositório **público no GitHub, licença AGPL-3.0**.

### LEI ZERO — projeto para distribuição

**Origem:** `GODS_LAWS.md`, blocos-âncora fora da numeração das 40 leis, 21/08/2026. Duplicada por dúvida de classificação (candidatura levantada em 26/08/2026, não decisão explícita do líder — ver `/var/tmp/glintfx-plan/separacao-leis-escopo.md` §6).

<!-- DUP-BLOCK:LEI-ZERO:START -->

> **LEI ZERO, ACIMA DE TODAS: PROJETO PARA DISTRIBUIÇÃO.** O GlintFx é biblioteca pública sob AGPL-3.0, consumida por gente que não conhecemos, em cinco plataformas. **Nunca raciocine como se houvesse um consumidor único.** Ordem do líder em 21/08/2026, verbatim: *"onde está escrito que o consumidor é único? o projeto é para distribuir"*. Qualquer análise, corte de escopo, priorização ou decisão de API que se apoie na premissa de consumidor único está **errada por construção** e deve ser refeita.

<!-- DUP-BLOCK:LEI-ZERO:END -->

---

## §2 — Erro e API pública

### Nenhuma exceção cruza a API pública

**Origem:** `GODS_LAWS.md` L-22 (duplicada por dúvida de classificação, não decisão do líder).

<!-- DUP-BLOCK:L22-NOEXCEPT:START -->

**Data:** 21/08/2026, decisão do líder.

**Nenhuma exceção cruza a API pública.** Internamente exceção é permitida; na fronteira pública o erro sai como `std::expected` ou código de erro.

**Por quê, e o motivo é de ABI, não de gosto:** a biblioteca é compartilhada por padrão (L-19). Exceção atravessando `.so` exige RTTI compatível entre a lib e o consumidor, e quebra quando os dois foram compilados por compiladores ou bibliotecas padrão diferentes.

Continua valendo do `CONTRACT.md` §6.4: erro nunca é engolido em silêncio, sempre propagado ao chamador, e exceção não serve de fluxo de controle.

<!-- DUP-BLOCK:L22-NOEXCEPT:END -->

### Camadas, porta em compile-time, superfície pública opaca

**Origem:** `GODS_LAWS.md` L-19 (duplicada por dúvida de classificação, não decisão do líder — a promessa de opacidade da API é fato de produto, mas a arquitetura em camadas é conduta; não separei os dois com confiança, então duplicou inteira).

<!-- DUP-BLOCK:L19-OPAQUE:START -->

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

<!-- DUP-BLOCK:L19-OPAQUE:END -->

### `gltfx_err` / `gltfx_rslt` — as decisões do CORE-ERROR

**Origem:** `TODO.md`, linha `CORE-ERROR` (W2), 25/08/2026, `AskUserQuestion`. **Movida daquela linha em 26/08/2026** — a linha do `TODO.md` é reescrita a cada fatia (vinte linhas mudaram só em 26/08) e não é lar seguro para decisão permanente; este é o lar canônico.

**Nomes públicos, verbatim do líder:** `glintfx::gltfx_err` e `glintfx::gltfx_rslt` — prefixo curto dentro do espaço de nome, escolhidos por ele depois do CTO levantar que prefixar dentro do namespace produziria o nome repetido.

**As quatro decisões do líder:**

1. **Falta de memória é traduzida em código e devolvida — a lib NUNCA aborta o processo do consumidor**, porque motor interno de estúdio pode abortar mas biblioteca pública de consumidor desconhecido não tem o direito de matar processo alheio, que pode ser um editor com trabalho não salvo.
2. **Lista de códigos append-only** — publicado nunca muda de valor nem some, código novo é compatível e sobe `B`, renumerar sobe `A`.
3. e 4. **Os nomes públicos** `glintfx::gltfx_err` e `glintfx::gltfx_rslt` (citados acima).

**Dois consertos pós-revisão, decisão do líder:**

- **Guarda de precondição em depuração:** ler o valor errado passa a **parar citando o que o consumidor fez**, em vez de falha de memória confusa no código dele; em produção, custo zero.
- **Troca do armazenamento de `gltfx_rslt<void>`** para espelhar o molde primário (`std::variant`, não `std::optional`), decisão do líder recomendada pelo CTO — motivada por um achado real: lida indevidamente em produção, a forma antiga **não travava**, devolvia um erro fabricado a partir de memória não inicializada, e se copiado/destruído o ponteiro fantasma podia corromper a memória.

### Formatação de erro: vocabulário de token, nunca frase; sem catálogo de mensagem

**Origem:** `docs/api-conventions.md`, regra R7 (não movida do arquivo-fonte; o gabarito de revisão de API continua lá; esta seção é a cópia canônica da decisão, não o único lugar onde ela existe).

`gltfx_err_fields()` devolve pares `(name, value)` — `name` sempre um identificador estável, nunca uma frase. Campo ausente é **omitido** do resultado, nunca presente com valor vazio ou zero. `glintfx` **nunca** embarca um catálogo de mensagens em nenhuma língua, em nenhuma fatia futura — o consumidor lê os tokens e monta a mensagem no idioma dele. **Decisão do líder, permanente**, motivada pela base de consumidores aberta e desconhecida (LEI ZERO): a lib escolher a frase seria escolher o idioma do consumidor, o que só acerta por acidente.

---

### As 6 decisões de 26/08/2026 sobre o tipo de cor (`CORE-COLOR`) — porta de mão única

Por `AskUserQuestion` (L-10), com as opções escritas pelo CTO em `/var/tmp/glintfx-plan/core-color-opcoes.md` e os fatos verificados por ele na documentação pública dos padrões (seções citadas, não de memória).

| # | Decisão | Régua da L-26 |
|---|---|---|
| 40 | **Quatro números decimais de 32 bits, 16 bytes.** Não inteiros pequenos de 4 bytes. | congela `A` (binário) |
| 41 | **A transparência fica DENTRO do tipo.** | congela `A` (binário) |
| 42 | **Os números representam LUZ FÍSICA** (escala linear), não o valor codificado que a tela recebe. | ⚠️ **a mais perigosa** |
| 43 | **Nome público: `gltfx_rgba`.** | congela `A` (recompilação) |
| 44 | **Alfa cru, não pré-multiplicado**, como o padrão faz. | muda folha já escrita |
| 45 | **Só as conversões mínimas congelam:** ida e volta com o formato de 8 bits da tela, mais o auxiliar de multiplicação pela transparência. | fronteira mínima |

**O argumento que decidiu, e é o mais importante de registrar:** com quatro números pequenos, **o branco é teto absoluto** — somar duas luzes brancas dá branco, e a conta para no limite. Mas o **brilho intenso** (decisão 26) e a **soma de luz** (decisão 27), que o líder comprou nesta mesma sessão, precisam de valores **acima do branco**: é assim que uma luz forte estoura e vaza para fora do objeto. E o **`oklch()`** (decisões 7 e 8) descreve cores que a tela não mostra, trazidas ao alcance só na hora de pintar; com inteiros pequenos elas seriam **amputadas no nascimento**, antes de qualquer conta.

**Ou seja: a opção barata era cara exatamente onde o líder já tinha investido.** Custo de errar estrutural, não linear.

⚠️ **Por que a decisão 42 é a mais perigosa das seis:** trocar a escala depois **quebra a cor na tela de todo consumidor SEM quebrar compilação nenhuma**. Nada acusa, nada falha, e o desenho fica errado. Por isso o significado dos números vai **escrito no cabeçalho** e provado por teste de ida e volta com valores fixados, não deixado como convenção implícita.

**Fatos verificados na fonte pelo CTO em 26/08/2026** (L-27, com a seção citada): o padrão de cor guarda alfa **cru** e pré-multiplica só durante a interpolação, desfazendo ao final; somar luz de forma fisicamente correta exige espaço **linear em intensidade**; e o `oklch()` tem chroma teoricamente ilimitado, expressando cores **fora do alcance da tela**.

**Descartada com razão registrada:** números decimais de 16 bits. O fornecimento deles no padrão da linguagem é **condicional**, e o projeto não controla isso nas cinco plataformas da L-04.

### Duas decisões do líder de 27/08/2026 sobre o que o consumidor recebe

**Origem:** decisão dele por `AskUserQuestion`, depois de as duas opções lhe serem explicadas pelo efeito e não pela implementação (L-41).

#### 1. A leitura de cor entra na API pública COM o diagnóstico rico

**Verbatim da opção que ele escolheu:** *"do jeito rico"*.

Quando a leitura de uma cor falha, o consumidor recebe **em que linha e em que coluna** do arquivo está o problema, e **o que se esperava encontrar ali** — não apenas "deu errado". É isso que permite a quem integra a biblioteca mostrar ao usuário final dele uma mensagem útil em vez de um erro mudo.

**O custo, aceito de olhos abertos:** o consumidor passa a lidar com **duas formas** de receber erro na mesma biblioteca — a rica, na leitura de folha de estilo, e a geral, no resto. A alternativa era um mecanismo só, e ela foi **recusada**, porque caberia a informação de posição num molde que não foi feito para carregá-la, e a posição é justamente o que torna o erro acionável.

**Isto congela na entrada.** Mudar depois quebra quem já escreveu código em cima.

#### 2. Defeito interno NOSSO sai por CANAL SEPARADO

**Verbatim da opção que ele escolheu:** *"canal separado"*.

Quando a biblioteca detecta um bug **dela mesma** no meio de uma leitura — não um erro no arquivo do consumidor —, o aviso **não** sai pelo mesmo canal dos erros de sintaxe. Ele tem caminho próprio.

**O problema que isto resolve, e que motivou a pergunta:** o consumidor foi ensinado, pela filosofia de recuperação que o formato documenta, a ler o canal de sintaxe como *"o arquivo do usuário tem um erro, siga em frente"*. Um defeito nosso saindo por ali faria ele **culpar um arquivo que está correto**, e reportar ao usuário final um erro de sintaxe que não existe.

**O custo, declarado:** é um caminho a mais que o consumidor precisa lembrar de consultar. O líder pesou isso contra o risco de culpa trocada e escolheu a clareza.

⚠️ **Isto REVISA a decisão `D-W3-6`**, tomada em modo autônomo pelo C-level, que tinha escolhido usar o mesmo canal com um identificador que nomeia a biblioteca como culpada. A implementação atual segue aquele desenho e **precisa ser migrada**; o que não se reverte é o comportamento que ela substituiu, que devolvia texto plausível e falso em silêncio.


### Mais duas decisões do líder de 27/08/2026

#### 3. O relógio público é EXATO, com atalho de conveniência ao lado

**Verbatim da opção que ele escolheu:** *"Exato, com atalho"*.

A biblioteca informa o tempo decorrido como **contagem inteira de uma unidade minúscula**, que **nunca acumula erro**, mais uma **conversão pronta para segundos** para quem prefere conforto. A alternativa — segundos com casas decimais direto — foi **recusada**: é mais simples de usar e **acumula desvio ao longo de horas de execução**, que é exatamente o regime de um jogo.

**A razão de produto:** precisão para quem precisa dela, conveniência para quem não precisa, **sem obrigar ninguém a escolher entre as duas**.

#### 4. O carregamento de arquivo NÃO guarda nada para reaproveitar

**Verbatim da opção que ele escolheu:** *"Ratifico, sem guardar"*.

A parte pública de carregamento faz três coisas: lê o arquivo, resolve o caminho, e reporta erro pela via da L-22. **Não guarda o que já leu.**

**A razão de produto, e é o argumento que decidiu:** escolher **como** guardar seria **impor a nossa política a todo consumidor**, e cada um tem a sua — quem carrega uma vez no início não quer pagar por memória retida, quem recarrega em laço quer. Guardar vira **item próprio** no dia em que existir carga medida que justifique, nunca por especulação.


#### 5. As cinco naturezas de valor e as treze unidades ficam FINAIS

**Verbatim dele:** *"aceito todas"*, depois de pedir para **ver a lista antes** e de ela lhe ser apresentada item a item.

**As cinco naturezas, e elas NUNCA se convertem umas nas outras na leitura:** palavra-chave (incluindo as três universais de herdar, voltar ao valor de fábrica e desfazer); **número puro**, sem unidade, que não é medida de comprimento; **número inteiro**, distinto do número com casas; **comprimento**, que é número com unidade; e **porcentagem**, que **continua sendo porcentagem** e não vira comprimento na leitura.

⚠️ **A separação entre as três primeiras existe porque misturá-las esconde erro:** um lugar que espera contagem receberia um número com casas decimais **calado**.

**As treze unidades:** de tela (`px`, `dp`); relativas à letra (`em`, `rem`, `ex`); relativas à janela (`vw`, `vh`); e físicas (`in`, `cm`, `mm`, `pt`, `pc`), estas presas a uma **razão fixa** de 96 pontos de tela por polegada.

**O que ele decidiu com o fato na mão:** foi-lhe dito, antes da escolha, que **as unidades físicas não medem centímetro de verdade** — ninguém conhece o tamanho real da tela — e que recusá-las deixaria o formato menor e mais honesto ao custo de **rejeitar folhas escritas para o padrão**. **Ele escolheu aceitá-las**, sabendo disso.

#### 6. Os oito fatos que o consumidor responde sobre a árvore dele ficam FINAIS

**Verbatim dele:** *"aceito tudo"*, também depois de pedir para **ver a lista antes**.

**A biblioteca exige do consumidor, e só isto:** o nome do elemento; o identificador único, se houver; a lista de classes; consultar um atributo pelo nome; os cinco estados (mouse em cima, sendo clicado, com foco, com foco por teclado, marcado); quem é o pai; quem é o irmão anterior e o seguinte; quantos filhos tem e qual é o primeiro.

**A biblioteca NÃO exige, porque calcula sozinha:** a posição do elemento entre os irmãos, as contagens, e a posição contando só irmãos do mesmo tipo — tudo isso sai andando pela árvore com o que os três últimos itens já dão.

**O princípio que governa o corte, e é o que torna a lista defensável:** ⚠️ **pede-se ao consumidor apenas o que ninguém além dele pode responder.** Tudo que é derivável fica do nosso lado. É esse princípio, e não a contagem de itens, que deve ser aplicado se algum dia se cogitar mexer nela.

**A ausência deliberada:** a âncora de *"a partir daqui"* numa busca **não entra** no contrato do nó — ela pertence à consulta, não ao elemento.

⚠️ **Este é o único dos contratos que o CONSUMIDOR implementa.** Acrescentar exigência depois quebra todo consumidor que já a implementou, e por isso a lista foi mostrada inteira antes de ser fechada.


#### 7. A exigência mínima da ferramenta de build SOBE, e o leitor caseiro morre

**Verbatim dele:** *"opcao 2 e instala no ubuntu a versao necessaria e deixa a ressalva na documentacao"*. **Decisão de 27/08/2026**, tomada com o custo na mão e depois de uma busca na web feita sob a L-42 recém-criada.

**O que muda:** a leitura do descritor de empacotamento passa a usar o **mecanismo nativo da própria ferramenta de build**, que já existe pronto e testado por quem a mantém. **O leitor escrito em casa deixa de existir.**

**Por que, e o número que decidiu:** aquele leitor caseiro **falhou CINCO vezes em dois dias**, sempre da mesma família — um pedaço de caminho entrando de alguma fonte sem ser completado. A cada rodada um revisor achava outra ocorrência. **Não é falta de cuidado de quem escreveu: é superfície que não deveria ser nossa.**

**O custo, declarado ANTES da escolha e aceito:** o mecanismo nativo exige versão da ferramenta **mais nova que a nossa exigência de hoje**, e **a versão que o Ubuntu 24.04 entrega de fábrica é exatamente a nossa mínima atual**. Subir **corta o Ubuntu 24.04 de fábrica**, e ele é um dos cinco alvos da L-04.

**A saída que ele escolheu, e é a parte que evita o abandono do alvo:** ⚠️ **o nosso CI instala a versão necessária no Ubuntu**, e **a ressalva fica ESCRITA na documentação do empacotador** — quem estiver naquele sistema precisa de uma versão mais nova que a de fábrica, e o documento diz isso e diz como obter. **O alvo continua suportado; o que muda é que ele passa a ter um pré-requisito nomeado.**

⚠️ **A regra que fica para casos futuros da mesma forma:** cortar alvo em silêncio é proibido. **Alvo com pré-requisito é aceitável quando o pré-requisito está escrito onde o empacotador lê**, e quando o nosso próprio CI prova que a receita funciona.

#### 8. Caminho RELATIVO vira o caso principal exercitado pelos testes

**Verbatim da opção que ele escolheu:** *"Vire o padrão dos testes"*.

**O fato que decidiu, vindo da documentação oficial:** ela **recomenda caminho relativo em toda parte** e avisa que caminho completo **não funciona** com a opção de instalar num lugar escolhido. ⚠️ **Nós tratávamos relativo como caso de borda — ele é o caso recomendado.** Foi exatamente essa inversão que deixou cinco defeitos passarem: a suíte inteira exercitava o caso raro e nunca o comum.

**A partir de agora:** o caso principal dos testes é o **relativo**; o completo vira o caso extra, testado, mas não o padrão.


#### 9. O custo REAL da decisão 7, medido depois, e a ratificação com ele na mão

**Verbatim dele:** *"Mantenha, com a ressalva ampliada"*. **27/08/2026.**

⚠️ **A decisão 7 foi tomada com informação incompleta, e a falha é minha.** Eu apresentei o custo como *"corta o Ubuntu 24.04 de fábrica"*. **Ao medir, o custo real é muito maior:** a parte do mecanismo nativo de que precisamos só ficou completa numa versão **bem mais nova** do que eu supus.

**O que cada alvo entrega de fábrica, medido:**

| Alvo | Versão de fábrica | Atende a exigência nova? |
|---|---|---|
| **Fedora 44** (primário, o do líder) | 4.3 | **sim — o único** |
| Ubuntu 24.04 | 3.28 | não |
| Debian 12 | 3.25 | não |
| **A máquina de teste do Windows** | 3.31 | **não** |

⚠️ **Ou seja: a exigência só é atendida de fábrica pelo sistema do próprio líder.** Todos os outros, **Windows incluído**, precisam de instalação à mão — e isso significa que hoje **praticamente nenhum consumidor compila o GlintFx com a ferramenta que o sistema dele já tem**.

**Eu levei isso de volta a ele em vez de seguir**, porque decisão tomada com número errado não é decisão tomada. **Ele ratificou com o número certo na mão**, escolhendo manter e **ampliar a ressalva**.

**O que a ratificação obriga:** o documento do empacotador deixa de falar em *"o Ubuntu precisa de um passo extra"* e passa a dizer que **todos os sistemas, menos o Fedora, precisam instalar a ferramenta à mão** — **com a receita de cada um**. A regra da decisão 7 continua valendo e fica ainda mais exigente: **cortar alvo em silêncio é proibido**, e agora são quatro alvos com pré-requisito nomeado em vez de um.

**A prova de que a receita funciona, e não é só texto:** o pipeline inteiro foi rodado dentro de um Ubuntu real — instalar dependências, baixar a ferramenta oficial, configurar, compilar, e a suíte completa — **com todos os testes verdes**. A receita do Windows **não pôde ser provada aqui** (não há máquina Windows), e isso está declarado como tal, não vendido como verificado.


## §3 — Versionamento e ABI

### Versão `vA.B.C.D`, `SOVERSION`, e o terceiro contrato (DADO)

**Origem:** `GODS_LAWS.md` L-26 (duplicada por dúvida de classificação, não decisão do líder — a tabela A/B/C/D é contrato de produto, mas o "quando taggear" é protocolo de release já espelhado em L-11/L-18; não separei os dois com confiança).

<!-- DUP-BLOCK:L26-VERSION:START -->

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

<!-- DUP-BLOCK:L26-VERSION:END -->

### O quarto campo do `struct version`: `tweak_version`

**Origem:** `TODO.md`, linha `VER-4C` (W1), 22/08/2026, decisão do líder. **Movida em 26/08/2026.**

O value type `glintfx::version` ganha o **quarto campo** (`tweak_version`), para struct e string dizerem a mesma coisa — reabriu a porta de mão única do `HDR-HYGIENE` uma única vez, com revisão de API dedicada, na janela barata (pré-1.0, `SOVERSION` 0, sem consumidor externo). O campo entra **no fim**, sem reordenar os três existentes.

---

## §4 — `gfui` / `gfss` / `gfml`

**Origem:** `GODS_LAWS.md` L-28 (grande parte migrada; a lei manteve só a conduta — proibição de consultar `PORTING-RCSS.md`, critério de revisão, e a nota de licença dos padrões). Texto completo abaixo, verbatim, incluindo todas as decisões numeradas de escopo (as 5 de 21/08, as 22 e as 8 e as 5 de fecho e as 3 da fronteira de glifos, de 26/08).


**Data:** 21/08/2026, confirmado pelo líder. **Verbatim relatado:** *"usaremos rcss, é o que quis dizer com rcss, mas sem ligar rml"*.

**O GlintFx adota o RCSS como formato de folha de estilo, e o implementa em casa.** O **RmlUi está fora**: a lei de dependência zero (L-07) não abre exceção para ele, e adotar o formato não é adotar a biblioteca.

**De onde a especificação sai, e de onde NÃO sai.** A lista do que o parser precisa suportar vem do **formato RCSS** e das decisões de escopo do líder por `AskUserQuestion` (L-10), julgadas pelo **consumidor externo desconhecido** (LEI ZERO e L-02). **Nunca** dos mockups, do ui-kit ou das telas de um consumidor específico.


**Ratificação de 22/08/2026, pedida pelo próprio líder.** A sessão do `gusworld` relatou que ele queria decidir sobre esse `PORTING-RCSS.md` conosco, e a decisão dele, tomada por `AskUserQuestion`, foi **não abrir, sem exceção**. O argumento que a sustenta, e que vale como método para qualquer documento futuro do gênero: a lista de recursos que um consumidor precisou é, por construção, um **subconjunto** do que a documentação pública do formato já define, logo o documento **não pode ensinar nada que a fonte já não tenha**. O único acréscimo real dele seria **ordem de importância derivada de um consumidor**, que é exatamente o que a LEI ZERO proíbe. O risco não é copiar código: é a lista **parar a pergunta certa**, trocando "o que o formato define?" por "o que está na lista?". Precedente empírico registrado no mesmo dia: o escopo de RCSS foi fechado **sem** o documento, indo à documentação do formato, e nesse caminho o CTO **corrigiu três erros próprios** que a memória carregava.

### EMENDA de 26/08/2026 — o layout entra, a marcação espera, e os dois formatos ganham nome próprio

**Decisão do líder por `AskUserQuestion`, depois de eu argumentar CONTRA primeiro**, como a LEI DAS LEIS exige. **Verbatim dele:** *"Nosso \"css\" será chamado gfss e nossa marcacao se chamará gfml, com as propriedades de html."*

**O que muda:**

| | |
|---|---|
| **`gfss`** | o nome do nosso formato de folha de estilo. **Substitui o nome RCSS** em toda referência a formato. |
| **`gfml`** | o nome da nossa marcação, **com as propriedades de HTML**. **Ainda NÃO entra** — fica decidida e escrita, sem trilha aberta. |
| **layout** | **ENTRA no escopo.** A biblioteca passa a calcular caixa e posição, não só estilo computado. |

**O que motivou, e é reconhecimento de um argumento dele:** um motor de estilo sozinho entrega pouco ao consumidor externo desconhecido. *"Eu digo a cor do seu nó, você escreve o layout"* é proposta difícil de sustentar numa biblioteca 2D — e havia risco real de entregar 21 fatias que ninguém consegue usar sem escrever motor de layout próprio. **Layout resolve a inutilidade; marcação resolveria conveniência de autoria, e custa um parser inteiro a mais.** Por isso a divisão.

**O contra-argumento que apresentei antes, e que fica registrado para não ser redescoberto:** marcação mais estilo mais layout **é um motor de navegador**; a trilha de estilo sozinha já é a maior do projeto com 21 fatias, e marcação mais layout plausivelmente a igualariam, crescendo a tabela em cerca de um quinto. Pior, **a demo ainda não rodou** — janela, contexto gráfico, laço e demonstração seguem pendentes, e a **L-32** existe exatamente contra este cenário, que o próprio texto dela descreve: chegar com meia biblioteca pronta **e nenhuma janela na tela**. **O líder cortou pelo meio: pegou o que resolve o problema real, adiou o que era conveniência.**

**Risco de contaminação, que continua valendo com força redobrada:** o predecessor usava RmlUi, e **construir layout do zero é onde a tentação de "ver como o antigo fazia" fica mais forte**, porque é o trabalho mais difícil e o mais parecido com o que já existia. **A L-01 e a proibição concreta desta lei continuam inteiras.** Se em algum momento parecer mais barato consultar aquele material, **é exatamente o movimento proibido.**

**De onde a especificação sai agora — e isto é INFERÊNCIA minha sobre a decisão dele, não verbatim, sujeita a correção:** ao batizar os formatos de `gfss` e `gfml` e ao dizer *"com as propriedades de html"*, ele os torna **formatos nossos modelados no vocabulário dos padrões web**, e não mais o formato de terceiro que a versão original desta lei adotava. **A fonte da especificação passa a ser a documentação pública dos padrões (CSS e HTML), nunca um consumidor específico** — e a proibição de tirar a lista do que um jogo precisou **continua exatamente como está**, pelo mesmo argumento: a lista de um consumidor é subconjunto do que o padrão já define, e o risco não é copiar código, é **a lista parar a pergunta certa**.

**O que NÃO mudou:** as cinco decisões de escopo (árvore do consumidor apagada de tipo, folha imutável depois de parseada, seletor completo na v1, `@media` fora, animação e transição fora) **continuam de pé**. Layout entrar não as reabre.

### Nome do motor: `gfui` — decidido em 26/08/2026

**Ordem do líder, verbatim:** *"nosso motor se chamará gfui"*.

**O trio de nomes fica assim, e eles são coisas diferentes:**

| nome | o que é |
|---|---|
| **`gfui`** | **o motor** — a máquina que lê o estilo, calcula caixa e posição, e diz onde cada coisa fica |
| **`gfss`** | **o formato de folha de estilo** que ele lê |
| **`gfml`** | **o formato de marcação**, com as propriedades de HTML — **decidido, não aberto** |

**Por que a distinção importa e não é preciosismo:** `gfss` e `gfml` são **formatos de dado** — arquivos que o consumidor escreve, e que a **L-26 governa como o terceiro contrato de versão**, aquele em que a pergunta é *"quem perde o arquivo"*. **`gfui` é código** — módulo, superfície pública, símbolo exportado, governado pelos contratos de API e de ABI. **Confundir os três é confundir três réguas de compatibilidade diferentes**, e quebrar formato não custa o mesmo que quebrar assinatura.

### Legibilidade humana é REQUISITO, não gosto — 26/08/2026

**Ordem do líder, verbatim:** *"gfss e gfml DEVEM ser tão simpaticos a humanos quanto css e html. quero marcacoes verbosas"*.

**Os dois formatos são escritos e lidos POR PESSOAS, e o desenho se julga por isso.** Não são formato de máquina que por acaso alguém abre; são **linguagem de autoria**. Quem escreve uma folha `gfss` ou um documento `gfml` tem de conseguir **ler em voz alta e entender**, do mesmo jeito que se lê CSS e HTML.

**A régua concreta, para não virar frase bonita sem consequência:**

- **Verbosidade vence brevidade.** Onde houver escolha entre um nome curto e um nome que se explica, **ganha o que se explica**. `background-color` vence `bg`; `border-radius` vence `r`. **Digitar mais é barato; reler sem entender é caro**, e o texto é lido muitas vezes mais do que escrito.
- **Nada de sigla, abreviação inventada ou código numérico** onde uma palavra serve. O leitor não deve precisar de tabela de tradução ao lado.
- **Nada de sintaxe densa** — sem operador críptico, sem posicional obrigatório, sem "o terceiro campo é o alinhamento". **Se precisa contar vírgulas para saber o que uma linha faz, o desenho falhou.**
- **O vocabulário segue o dos padrões web** onde eles já resolveram o problema, porque **milhões de pessoas já sabem essas palavras** — e conhecimento que o consumidor já tem é o recurso mais barato que existe. Inventar sinônimo para o que já tem nome é custo puro.
- **Erro de escrita se diagnostica com linha, coluna e o que se esperava.** Formato amigável que falha com mensagem obscura **não é amigável** — é hostil na hora que mais importa.



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

### As 22 decisões de escopo de 26/08/2026 — `gfss`, layout, render e fonte

Todas por `AskUserQuestion` (L-10), na sessão em que o Caetano (CTO) derivou a lista de propriedades a partir da documentação pública dos padrões. **Não são mais perguntas.** Onde uma diverge do CSS, a divergência está declarada e vai para a documentação do formato — o consumidor não pode descobrir por acidente.

**Fato que governa as onze primeiras, e precisa ser lido antes delas:** com o `gfss` sendo **formato nosso** modelado no vocabulário dos padrões, o que antes era *fato herdado do RCSS* passou a ser **decisão nossa**. Valor inicial, presença ou ausência de recurso: tudo que a versão anterior desta lei registrava como "verificado na doc do formato" precisa de ratificação, e é isso que as decisões abaixo fazem.

#### Formato `gfss` — o que a folha significa

| # | Decisão | Diverge do CSS? |
|---|---|---|
| 1 | **Fluxo da v1: `block` + `flex`.** Grid fica para **discussão posterior** — não é veto, é adiamento. Fluxo inline e `float` ficam fora. | — |
| 2 | **`display` inicial = `block`.** O valor do CSS é `inline`, que depende de fluxo de texto e a v1 não tem. | **sim** |
| 3 | **`box-sizing` inicial = `border-box`.** O tamanho declarado é o total na tela, borda inclusa. Declarar `content-box` continua sempre possível. | **sim** |
| 4 | **Cor de borda inicial = cor própria fixa, independente da cor do texto.** O CSS parte de `currentColor`; aqui a borda nasce preta e só muda por ordem. Verbatim do líder: *"é obrigado a borda ser da cor do texto? devem ser idependentes"*. | **sim** |
| 5 | **Margens verticais vizinhas SOMAM.** Sem fusão (*margin collapsing*). 20px mais 30px dá 50px. **É de mão única:** mudar depois altera o desenho de folha já escrita. Motivo aceito: uma regra só no formato inteiro, igual à do flex, sem margem de filho vazando para o pai. | **sim** |
| 6 | **Os cinco valores iniciais que os padrões deixam em aberto ficam fixados:** cor do texto `black`; tamanho de fonte `16px`; alinhamento `left`; espaço entre itens `0`; espessuras `thin`/`medium`/`thick` = **1/3/5 px**. | — |
| 7 | **Cores: as ~148 nomeadas do padrão**, mais `#rrggbb`, `rgb()`, `hsl()` e **`oklch()`**. **Isto REVOGA** o default registrado da versão anterior desta lei, que punha as cores modernas fora da v1. | — |
| 8 | **`oklch()` em fatia própria**, logo depois da fatia das outras notações — ela exige conversão de espaço de cor perceptual e mapeamento de gamut, que as outras três não têm. | — |
| 9 | **`!important` entra na v1.** Isto **reescreve** `GFSS-CASCADE`, cujo texto dizia "sem `!important`". | — |
| 10 | **`inherit`, `initial` e `unset` entram na v1.** Isto **reescreve** `GFSS-INHERIT`, cujo texto dizia que o formato não tem a palavra-chave. | — |
| 11 | **Pseudo-elementos `::before` e `::after` entram na v1.** É o mais caro dos três: o motor passa a **fabricar caixas** que não existem na árvore do consumidor, o que é trabalho de layout, não de folha. Não confundir com as pseudo-classes de dois-pontos simples, que já estavam dentro nas cinco fatias de seletor. | — |

#### Layout — a trilha nova

| # | Decisão |
|---|---|
| 12 | **A trilha de layout é desenhada AGORA** (13 fatias `LAYOUT-*`), executada **depois da W10**, no **mesmo slot** da L-32 — layout consome estilo computado, é continuação da mesma trilha, não uma segunda disputando o teto. Precedente: as 14 fatias de mapa esperam desenhadas sem custo. |
| 13 | **Medir texto: interface separada e opcional.** A biblioteca pergunta o tamanho do texto a quem a usa; quem não responde continua funcionando, sem ajuste automático. **Não trava** o congelamento de `GFSS-NODE-VIEW` na W3. |
| 14 | **Ajuste automático de caixa ao conteúdo é REQUISITO, e o motivo é i18n.** Verbatim do líder: *"portugues e ingles e outras linguas tem palavras de tamanhos diferentes. no i18n, se a palavra em outra lingua for maior, 'estoura' e clipa as margens"*. Consequência direta: **`LAYOUT-MEASURE-CONTRACT` sobe para o começo da trilha**, antes de `LAYOUT-SIZE-RESOLVE`, porque resolver largura automática passa a depender dela. |
| 15 | **Texto que não cabe numa caixa que não pode crescer: transborda VISÍVEL, com diagnóstico.** Nunca cortar em silêncio. Erro de tradução que some sem aviso chega ao usuário final; erro que estoura na cara é consertado antes. Quebrar em linhas e reticências dependem do motor de texto e ficam para a trilha de fonte. |

#### Render — o que só existe quando houver o que desenhar

Critério transversal, e é ele que explica o bloco inteiro: **propriedade que a folha aceita e o motor ignora é o defeito que o líder mandou eliminar** ao dizer que biblioteca sozinha era inútil. Cada uma abaixo nasce ao lado do código que a pinta.

⚠️ **CORREÇÃO da mesma sessão, decidida depois destas seis (L-32):** as seis foram aprovadas para a W7, que vem **antes** da `DEMO-1` da W8 — e isso era exatamente o cenário que a L-32 existe para impedir, *meia biblioteca pronta e nenhuma janela na tela*. Medido no ato: `DEMO-1` carrega **31,00** de pontuação, a maior da tabela inteira, contra 20,00 do segundo colocado. **O bloco inteiro passa para DEPOIS da `DEMO-1`.** Verbatim da escolha do líder: *"Depois da demo"*. Nada foi cortado — só reordenado. Onde as linhas abaixo dizem W7, leia **pós-demo**.

| # | Decisão |
|---|---|
| 16 | **Gradiente: fatia ÚNICA, junto do render, na W7.** Gramática e pintura na mesma fatia, ao lado do shader interno de `R2D-BATCH`. Nada de linha aceita e ignorada. |
| 17 | **Sombra de caixa e filtros seguem o mesmo destino do gradiente** — mesmo shader, mesmo momento, uma decisão só em vez de três discussões idênticas. |
| 18 | **Moldura de nove pedaços (*nine-patch*, o `border-image` do CSS): fatia PRIORITÁRIA própria**, à frente das outras do bloco. Justificativa: é como praticamente toda caixa de diálogo, painel de inventário e balão de fala de jogo 2D é desenhado, e **não existia em lugar nenhum da tabela** — ausência medida, não lembrada. |
| 19 | **Pixel art não borrada (`image-rendering`) entra CEDO**, na W8 com a textura, sem esperar o bloco da W7. Custa quase nada no render, e sem ela todo sprite ampliado sai borrado — o que inviabiliza o estilo visual de metade dos jogos 2D. |
| 20 | **Transparência: a forma simples fica no eixo A** (funciona sem layout); **a de grupo** — painel inteiro translúcido como um todo, em vez de cada peça por si — **entra no bloco da W7**, porque exige desenhar fora da tela primeiro. A documentação diz qual é qual; ninguém descobre por surpresa. |
| 21 | **`transform` entra na W7 COM o teste de clique acompanhando a transformação.** Ligar entrada e render custa uma fatia a mais e evita o defeito concreto de um botão que gira na tela e continua clicável no lugar antigo. Registrado junto: elemento transformado **continua ocupando o lugar original no layout** — é o comportamento do padrão, e é surpreendente. |

#### Fonte — a trilha que faltava

| # | Decisão |
|---|---|
| 22 | **A trilha de fonte é COMPLETADA agora, no mesmo trabalho do layout.** |

⚠️ **Correção de fato, na mesma sessão, antes de a decisão 22 ser executada.** O CTO relatou que a trilha de fonte *"não tem uma única fatia na tabela hoje"*, e o orquestrador repetiu isso ao líder duas vezes antes de conferir. **É falso, e a medição desmente:** existem três — `FONT-TABLES` (W3, lê a estrutura do arquivo), `FONT-OUTLINE` (W4, extrai o contorno do glifo) e `FONT-RASTER` (W5, rasteriza com suavização). Aplicação direta da regra da casa de que **relatório de agente não é prova**, desta vez contra o próprio orquestrador, que relatou de segunda mão.

**O que a correção NÃO desfaz:** o buraco é real. As três param exatamente onde o layout precisa começar — **não existe fatia que MEÇA texto**, que é de onde o ajuste automático por i18n (decisão 14) depende, nem atlas, nem montagem de linha, nem quebra de linha. Elas entregam uma letra desenhada; o layout precisa saber quanto mede uma frase.

**O que a correção MUDA, e é substantivo:** a decisão sobre **como os glifos são guardados**, que eu apresentei ao líder como aberta, **já estava tomada** — `FONT-RASTER` diz *"rasterização scanline com AA"*, isto é, desenho direto, e **não** o método de guardar distância à borda. Consequência que ninguém escolheu explicitamente: contorno e sombra de texto saem pelo caminho caro, de vários desenhos por palavra, não de graça no shader. Fica registrado para revisão consciente quando a trilha for completada.

**A decisão do líder, com o fato corrigido na mesa:** completar a trilha inteira — medir texto, atlas, montar linha, quebrar linha — junto com o desenho do layout. Verbatim: *"Sim, completar a trilha"*.

#### O que a régua de legibilidade derrubou nesta mesma sessão

Aplicação concreta da seção anterior desta lei, registrada porque é o primeiro caso em que ela mudou uma recomendação técnica já escrita:

- **O shorthand `flex` fica FORA da v1.** `flex: 1` expande para `flex-grow: 1; flex-shrink: 1; flex-basis: 0` — a mudança silenciosa do `basis` é pergunta clássica de entrevista que veterano erra. É exatamente o "precisa de tabela de tradução ao lado" que a régua proíbe. Os três longhands **são** a forma verbosa que se explica, e ficam.
- **O shorthand `inset` fica fora** pelo mesmo motivo: a palavra não se explica; `top`, `right`, `bottom` e `left` se explicam.
- **Os demais shorthands do CSS ficam** (`margin`, `padding`, `border-width`, `border-style`, `border-color`, `border`, `border-radius`, `gap`, `overflow`): a ordem topo-direita-baixo-esquerda é vocabulário que milhões já sabem, e o longhand verboso existe sempre como alternativa. **A fronteira: shorthand posicional INVENTADO POR NÓS está proibido; os dos padrões entram por serem vocabulário público.**
- **O tipo de diagnóstico ganha um campo obrigatório: "o que se esperava"**, além de linha e coluna. Precisa entrar **antes** de o tipo congelar em revisão de API dedicada.



### As 8 decisões de 26/08/2026 sobre movimento e luz — animação REABERTA

Todas por `AskUserQuestion` (L-10), na mesma sessão, algumas horas depois das 22 anteriores. Nasceram de uma pergunta do líder — *"temos brilho? com fadein, fadeout, pulse, velocidade de fade, tempo de fade, propriedades predefinidas (heartbeat [velocidade], breath [velocidade], outros [sugira]"* — cuja resposta medida foi **não temos nada disso**: zero ocorrências de brilho, glow ou bloom na tabela inteira e nas leis, e a metade animada explicitamente fora da v1.

#### ⚠️ REVOGAÇÃO da decisão 5 de 21/08/2026

A quinta decisão de escopo desta lei dizia: *"Animação, transição e `@keyframes` ficam FORA da v1, ignorados com diagnóstico. Entram como escopo próprio quando o loop principal e o relógio existirem."*

**A condição que ela mesma escreveu está satisfeita, e foi medida:** `CORE-TIME` (W3, pontuação **24,00**, a terceira maior da tabela — relógio monotônico, delta, ritmo de quadro) e `LOOP-RUN` (W6, o loop principal público). **A partir da W6 a fundação existe**, e o líder reabriu.

**Consequência imediata na tabela:** `GFSS-SHEET-PARSE` (W6) descreve animação, transição e `@keyframes` como *"fora da v1, ignoradas com diagnóstico"* — **esse texto está revogado** e a fatia precisa ser reescrita.

| # | Decisão |
|---|---|
| 23 | **Animação REABRE**, com a trilha desenhada agora e **executada depois da `DEMO-1`** — mesma disciplina da L-32 aplicada ao bloco de render horas antes. A demo continua na frente de tudo. |
| 24 | **Tamanho: o conjunto COMPLETO** — transição de propriedade (ir de um valor a outro com duração e curva), presets nomeados, **e animação escrita à mão pelo autor da folha**, com quadros próprios. É a mais cara das três opções apresentadas: exige um sistema de linha do tempo **além** do de curvas. Registrado como custo aceito de olhos abertos, não como surpresa futura. |
| 25 | **Duração e curva são parâmetros SEPARADOS.** Duração é quanto tempo leva; curva é *como* leva — começa devagar, passa do alvo e volta, chega batendo. **A curva é o que faz um botão parecer mecânico ou vivo com a mesma duração**, e é o parâmetro que quase todo formato esquece de expor. Nenhum dos dois é opcional na superfície pública. |
| 26 | **Brilho: sombra MAIS uma peça própria de brilho intenso.** O halo simples vem de graça da sombra de caixa (sombra sem deslocamento e com cor viva **é** brilho); a peça própria é a que faz a luz vazar para fora do elemento do jeito que fogo e magia parecem em jogo. |
| 27 | **Mistura aditiva entra como CAPACIDADE GERAL**, não como detalhe interno do brilho. **É o que separa luz de tinta:** quando uma luz brilha sobre um fundo, ela **soma** com o que está atrás; sem isso o halo fica leitoso, como vidro fosco por cima, nunca luminoso. Serve a neon, fogo, magia, explosão, faísca e raio — decidir só para o neon seria resolver um caso de uma capacidade que serve a dez. **Não existia em lugar nenhum da tabela** — ausência medida, não lembrada. |
| 28 | **`neon` é PRESET com parâmetros**, não peça separada. A receita tem quatro camadas: **núcleo quase branco** (dessaturado — este é o segredo, e o erro comum), **halo curto e saturado**, **halo largo e fraco**, e **soma com o fundo**. Escrever o texto na cor do neon produz **adesivo colorido**; escrever quase branco com a cor no halo produz **luz**. Empacotar a receita evita que quase todo consumidor erre na primeira tentativa. Combinado com `flicker`, dá o letreiro com defeito sem nada novo. |
| 29 | **Presets com sorteio (`flicker`, `twinkle`, `ember`): semente NO CONTRATO.** Fixada a semente, a animação repete idêntica e o teste consegue verificá-la; sem fixar, varia normalmente. Nasce da tensão com a **L-35** (entrega determinística): animação com sorteio não é reproduzível em teste a menos que a semente seja parte do contrato público. **Nenhuma outra parte da biblioteca tem esse problema hoje** — este é o primeiro. |
| 30 | **A lista de presets da v1, treze:** `breath` (onda lenta, vivo em espera), `heartbeat` (dois pulsos e pausa, vida baixa), `pulse` (onda simples, atenção sem alarme), `throb` (sobe de golpe, desce devagar — impacto), `blink` (liga/desliga duro), `flicker` (irregular — tocha, néon quebrado), `shimmer` (faixa de luz varrendo — item raro), `twinkle` (picos curtos aleatórios — estrela), `ember` (variação lenta e fraca — brasa), `charge` (cresce ao pico e estoura), `alarm` (pulso duro e rítmico — perigo), `wave` (atraso crescente entre vizinhos — menu que acorda item a item) e `neon`. **Presets são baratos depois que o motor de curvas existir — são uma tabela.** ⚠️ **`wave` é o único que não age sobre UM elemento**, e sim coordena vários com atraso progressivo: mecanismo diferente dos outros doze, e por isso fatia própria. |

### As 5 decisões de fecho de 26/08/2026 — as perguntas que o fatiamento devolveu

Por `AskUserQuestion` (L-10), depois de o CTO entregar as 37 fatias e devolver cinco perguntas. **Não são mais perguntas.**

| # | Decisão |
|---|---|
| 31 | **A pontuação das 37 fatias novas passa pelo Capitolino (CPO).** O CTO pontuou **por analogia** com a escala do CPO, e quem criou a escala revisa quem a usou. Motivo aceito: a pontuação decide a **ordem de execução** de 37 fatias, e errar nela custa meses gastos na sequência errada. Parcelas em `/var/tmp/glintfx-plan/fatias-2608-caetano.md`. |
| 32 | **A escolha do método de guarda dos glifos é REABERTA**, antes de a trilha de fonte fechar. Ela estava tomada **por consequência** e não por decisão: `FONT-RASTER` especifica *"rasterização scanline com AA"*, o que exclui guardar distância à borda. ⚠️ **É de mão única no sentido mais literal:** só pode ser escolhida **antes de a primeira letra ser gravada**. O que está em jogo: guardar distância à borda entrega contorno, sombra e brilho de texto **de graça no shader** e escala sem borrar em qualquer tamanho; o desenho direto exige desenhar a palavra oito vezes deslocada para obter contorno, com quinas imperfeitas. |
| 33 | **Reticências em texto truncado: fatia futura**, desenhada e esperando a trilha de fonte entregar a montagem de linha. Até lá vale a decisão 15 sem alteração — **transborda visível com diagnóstico**. Não entra dentro da fatia de montagem de linha: truncar é política própria, e fatia que faz duas coisas viola a L-17. |
| 34 | **Filtros: os baratos numa fatia, o desfoque em outra.** Escurecer, clarear, tirar a cor, saturar, girar matiz, inverter e sépia são **uma conta por pixel** e cabem juntos; **o desfoque precisa de desenho fora da tela e dois passes**, mais memória. Misturar custos tão diferentes na mesma unidade é o que a L-17 proíbe. Uso ancorado em jogo: desfoque = fundo borrado atrás do menu de pausa; tirar a cor = item indisponível ou jogo pausado; clarear = botão sob o mouse. |
| 35 | **As duas violações de dependência da trilha de mapa são consertadas AGORA, com o dominó.** `MAP-OBJECTS` e `MAP-COLLIDE-GRID` estão na W3 dependendo de `CORE-MATH2D`, que está na W4 — logo são **impuxáveis mesmo com o slot livre**. Confirmado contra o histórico (`git show 7ec2190:TODO.md`): **é defeito pré-existente, não introduzido pelo fatiamento de 26/08**. Conserto: mover as duas para a W5 e empurrar o que vem depois. É trabalho de tabela, não de código, e se faz enquanto a trilha está parada — deixar quebrado é tropeço garantido para quem puxar a trilha um dia. |

### Decisão 36, de 26/08/2026 — o método de guarda dos glifos é HÍBRIDO

Fecha a decisão 32 (reabertura). Por `AskUserQuestion` (L-10), com as quatro opções na mesa e a comparação escrita do CTO (`/var/tmp/glintfx-plan/glifos-comparacao.md`) como insumo.

**A escolha do líder: HÍBRIDO desde o começo** — letra desenhada nos corpos pequenos, campo de distância nos grandes.

⚠️ **O líder escolheu contra a recomendação do CTO, e contra a forma como a opção lhe foi apresentada.** O CTO recomendou o campo de distância de 3 canais puro, e classificou o híbrido como *"nota lateral (INFERÊNCIA do CTO, não é terceira opção formal)"*, *"refinamento da B, decidível depois"*. **O líder o promoveu a decisão de partida.** Registrado assim, sem suavizar: quem ler depois precisa saber que a escolha foi deliberada e informada, não um mal-entendido sobre o que estava sendo oferecido.

**O que o híbrido resolve, e é o argumento a favor:** cada método tem exatamente um ponto fraco, e eles são opostos. A letra desenhada perde ao ampliar e cobra caro por efeito (contorno = desenhar a palavra oito vezes) e por tamanho novo; o campo de distância perde fidelidade em corpo pequeno — que é onde interface passa a maior parte do tempo. **O híbrido usa cada um onde ele é forte.** Adotado desde o começo, não depois, porque o depois exigiria reabrir uma porta de mão única.

**O que ele cobra, declarado:** **duas implementações** e **duas vias no depósito de letras**. Não é o caminho mais barato; é o que entrega o melhor resultado nas duas pontas.

**Três coisas que a escolha CRIA e que ninguém decidiu ainda** — registradas aqui para não virarem decisão silenciosa, como aconteceu com a própria escolha do método (a decisão 32 existiu porque `FONT-RASTER` tinha fechado a porta sem ninguém escolher):

1. **O corpo-limite** onde um método vira o outro é **parâmetro de contrato**, não detalhe de implementação: ele muda a aparência do texto e o conteúdo do depósito. Precisa ser decidido, documentado e — se ajustável pelo consumidor — congelado na revisão de API.
2. **A travessia do limite durante animação.** Com a decisão 24 (animação completa), um texto pode crescer de pequeno a grande **atravessando o corpo-limite no meio do movimento** e trocando de método no ar. Se as duas vias não casarem visualmente na fronteira, aparece um pulo. **Nenhum dos dois métodos puros tem este problema; é criado pelo híbrido.** Exige teste de fronteira dedicado, e a regra da casa vale aqui: teste na fronteira exata não basta, precisa também de um passo para fora dela.
3. **Qual variante do campo de distância** entra na metade grande — a simples, que arredonda quinas, ou a de 3 canais, que as preserva pagando um gerador bem mais delicado. O CTO recomendou a de 3 canais; o líder não foi perguntado sobre isso separadamente, e a pergunta segue aberta.

**Fatias afetadas** (as três que a decisão 32 já havia travado): `FONT-RASTER` (que passa a especificar os dois caminhos e o critério de escolha), `FONT-ATLAS` (duas vias no depósito) e `R2D-TEXT` (o desenho passa a ter dois caminhos). `FONT-MEASURE`, `FONT-LINE`, `FONT-WRAP`, `FONT-ELLIPSIS`, `FONT-TABLES`, `FONT-OUTLINE` e toda a trilha `LAYOUT-*` **não mudam** — métrica e layout independem de como a letra é guardada.

### As 3 decisões de 26/08/2026 sobre a fronteira dos glifos — busca na web, e a decisão 36 posta à prova

Por `AskUserQuestion` (L-10), depois de o líder mandar **buscar na web** o valor da fronteira. A busca não devolveu o número esperado; devolveu algo melhor e algo incômodo.

**FATO 1 — não existe número universal publicado.** O guia de referência sobre a técnica declara que *"cada fonte + faixa de distância se comporta diferente em tamanhos diferentes"* e recomenda **comparar com a própria fonte**. **O corpo-limite tem de ser MEDIDO nesta casa, não copiado.** Fonte: `redblobgames.com/articles/sdf-fonts/`.

**FATO 2 — a literatura já pensa em RAZÃO DE RESOLUÇÃO, não em pixels absolutos.** O autor da variante de três canais publica a fórmula da faixa mínima como **`2,5 + 1 ÷ escala`**, onde **`escala` = tamanho na tela ÷ tamanho gravado no depósito**. A pergunta do líder (*"podemos ter fronteira por resolução?"*) acertou o eixo que a própria literatura usa. Fonte: `github.com/Chlumsky/msdf-atlas-gen/discussions/69`.

**FATO 3 — o autor da técnica NÃO recomenda o híbrido.** Ele diz que corpo pequeno se conserta **alargando a faixa de distância** dentro da própria técnica, e cita amostragem múltipla quando a redução é grande. Em nenhum momento sugere cair para bitmap. **Isto é contra-argumento direto à decisão 36, tomada vinte minutos antes**, e foi levado ao líder como tal, com a ressalva honesta de que o autor tem interesse na própria técnica e de que alargar a faixa **não é grátis** (a mesma informação por ponto passa a cobrir mais distância, e a precisão perto da borda cai).

| # | Decisão |
|---|---|
| 37 | **O híbrido fica de pé, MAS ganha um passo obrigatório antes: MEDIR.** Comparar as duas saídas lado a lado em corpo pequeno **com a faixa já alargada pela fórmula do FATO 2**. ⚠️ **Se a medição mostrar que a faixa alargada basta, o caminho de bitmap se torna dispensável e a decisão 36 CAI sem ter custado nada** — por isso a medição vem antes de qualquer implementação do segundo caminho. Isto não revoga a 36; põe-na à prova com evidência, que é o oposto de decidir por autoridade (a do autor da técnica ou a minha). |
| 38 | **A fronteira se expressa como RAZÃO DE RESOLUÇÃO EFETIVA**, não como corpo declarado na folha: **tamanho real na tela ÷ tamanho gravado**, contando a escala do monitor **e** a escala de transformação acumulada. Motivo medido: `font-size: 14px` não significa 14 pixels na tela em nenhum dos três casos que o projeto já tem — tela HiDPI (a do líder é fracionária), `transform: scale()` (decisão 21) e animação de escala (decisão 24). **Ganho de brinde:** a travessia do limite durante animação deixa de ser degrau arbitrário e vira **função contínua do tamanho real** — o problema que a própria decisão 36 criou (registrado como consequência 2) fica muito menor. Dependências, conferidas na tabela e já na ordem certa: escala do monitor (`WL-SCALE`, W6), escala de transformação (`R2D-TRANSFORM`, W10), corpo-limite (`FONT-HYBRID-THRESHOLD`, W13). |
| 39 | **A variante do campo de distância é a de TRÊS CANAIS**, a que preserva quinas — fechando a pergunta que a decisão 36 deixou aberta. Argumento do custo de errar, que foi o eixo decisivo: **descer de três canais para o simples é trivial; subir reabre formato de depósito e shader já congelados.** Custo declarado, mantido da recomendação do CTO: a atribuição de arestas a canais é a peça mais delicada de todo o trabalho, e é exatamente onde a L-29 obriga a refazer diferente em vez de copiar. Fecha `FONT-FIELD-VARIANT` (W6), que sai de `🎨 Pendente design`. |

**Nota de método, que vale além deste caso:** a busca foi ordenada para achar um número e **não achou nenhum**. O que ela achou foi (a) a confirmação de que o eixo da pergunta do líder era o certo e (b) um argumento contra uma decisão dele tomada minutos antes. **Os dois foram relatados**, o segundo com a ressalva do viés da fonte. Resultado negativo de busca é resultado — e busca que só confirma o que já se queria fazer não teria valido a chamada.

---

## §5 — Mapa

**Origem:** `GODS_LAWS.md` L-30 (grande parte migrada; a lei manteve só a conduta — aviso de escopo sobre pathfinding, nota sobre o editor ser projeto à parte, e a lição de método). Texto completo abaixo, verbatim.


**Data:** 21/08/2026, decisão do líder.

**O GlintFx tem mecanismo de mapa.** Matriz `x,y` simples, objetos posicionados nela, hitbox, parede, porta e ponto de teleporte (escada, buraco e afins).

**O GlintFx é DONO do formato de arquivo de mapa.** A lib publica o formato e o carregador; o editor e o jogo são **consumidores** dele. Consequência que a decisão assume de olhos abertos: **o formato vira API pública e contrato de ABI** (L-19 e L-26), qualquer consumidor no mundo passa a depender dele, e mudá-lo depois quebra todos. Por isso o formato nasce com revisão de API dedicada e versionamento explícito.

**Escopo dentro da lib, decidido pelo líder:** matriz e objetos, hitbox e consulta de colisão, **mais busca de caminho e visibilidade**. Porta e teleporte entram como **marcador genérico com destino**.

**A fronteira que separa mecanismo de conteúdo, e ela é a parte que mais se esquece.** Cidade, dungeon, estrada e floresta são **conteúdo de um jogo**, não mecanismo. A lib entrega a **matriz e as consultas**; o que se põe dentro dela é do consumidor. **A lib nunca sabe o que é uma dungeon**, nunca sabe que uma escada é uma escada: ela sabe que existe um marcador com um destino, e quem dá sentido é quem consome (L-02 e LEI ZERO). Nomear tipo de cenário dentro da lib é o mesmo erro de premissa que a LEI ZERO existe para impedir, entrando por outra porta.



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

### EMENDA de 25/08/2026 — a fronteira nomeada substitui a proibição absoluta

**Decisão do líder por `AskUserQuestion`, depois de contra-argumento do CTO apresentado antes, como a LEI DAS LEIS exige.** Esta emenda **não apaga** o que está acima — ela corrige uma premissa falsa e troca uma proibição por uma fronteira. **Leia as duas partes juntas.**

**O fato novo, verbatim do líder:** *"QUero criptografado pois não quero ninguem editando saves e mapas canonicos. O mapeditor, é ferramenta interna, para me ajudar a fazer os mapas pois um modelo llm já provou (você) que os mapas ficam todos errados, como por exemplo um poste no meio da rua"*.

**O que isso derruba:** a frase que fechou a decisão acima dizia *"editar mapa num jogo que **distribui editor** é uso legítimo"*. **O jogo não distribui o editor** — ele é ferramenta interna. A premissa era falsa, e com ela cai a conclusão de que edição de mapa é uso legítimo.

**O que isso NÃO derruba, e é importante não confundir:**

- **O selo aberto continua no formato**, e continua certo. Ele serve o **consumidor externo desconhecido** que quer mapa em claro com detecção de corrupção, e esse consumidor não deixou de existir.
- **O corolário técnico continua fato:** *em projeto com o fonte publicado, DETECTAR alteração é alcançável e IMPEDIR não é.* Isso é propriedade do fonte publicado, não da premissa do editor — vale com editor interno ou distribuído.

**A fronteira que substitui a proibição absoluta:**

| | |
|---|---|
| ✅ **Mecanismo, sim** | a biblioteca pode fornecer **envelope selado** e **primitivas criptográficas padrão** implementadas em casa |
| ⛔ **Segredo, não** | a chave **sempre** vem de quem chama. A lib **não** guarda, não gera política, não deriva de identidade, não embarca chave |
| ⛔ **Impedimento, nunca prometido** | nenhuma documentação nossa dirá "impede". O que se entrega é **detecção garantida** e **encarecimento verificável** |

**Por que fronteira e não simplesmente apagar a linha** — o argumento é do CTO e o líder o acatou: a cláusula original existia para impedir que a lib prometesse o que não pode cumprir e para mantê-la fora do negócio de segredo. **Apagá-la sem substituta abriria precedente** para alguém, adiante, propor DRM de verdade citando esta emenda. A fronteira nomeada conserva a proteção e libera só o que foi decidido.

**O que continua sendo achado de revisão, e não feature:** DRM, ofuscação de binário, chave embarcada, assinatura com segredo dentro da biblioteca, algoritmo criptográfico **inventado** por nós, e qualquer "embaralhamento" vendido como cifra. **Ou padrão de verdade, testado contra vetores oficiais, ou nada.**

**Onde a cifra mora, e isto preserva tudo que já foi congelado:** o envelope fica **POR FORA** do formato de mapa. Um mapa cifrado é o envelope embrulhando os bytes do mapa. **O formato não muda um byte** — magic, taxonomia de bloco, bit safe-to-copy e selo aberto ficam exatamente como estão, e nenhuma porta de mão única reabre.

**A divisão de responsabilidade, que envelheceu e agora está corrigida:** o texto acima diz que *"a proteção contra trapaça mora no save do jogador, que é arquivo do consumidor, não nosso"*. **Não vale mais**, porque a lei do jogo consumidor obriga a criptografia a vir de nós, e o líder quer o mapa canônico protegido também. **A divisão correta é: mecanismo é da biblioteca; chave e política são do consumidor.**

**O que o líder decidiu sabendo, e fica escrito para ninguém redescobrir:** ele confirmou que **o jogo SERÁ distribuído publicamente**. Como a AGPL obriga quem nos linka a publicar o próprio fonte, **o caminho por onde a chave chega fica visível**. Contra edição casual — inclusive de uma criança que usa editor de texto e git — a cifra resolve **inteiro**. Contra quem tem depurador, **não resolve**, e quando **um** publica a ferramenta de trapaça, **todo curioso a herda**. Ele decidiu com os três níveis na mesa. **Isto tranca a porta de vidro, e a porta de vidro é a que estava sendo arrombada.**

### O requisito que veio de um consumidor humano, e o que ele virou

Em 21/08/2026 o **Gus Dragon** pediu, nomeando o GlintFx: *"GlintFx e Mapeditor façam blocos especiais pra isso"*. O mecanismo genérico que sustenta o pedido, e que **não** nomeia nada do jogo:

- **A célula carrega dois campos separados**, um com a semântica de arte ou terreno e outro com a **marcação opaca do autor do mapa**. São separados porque a marcação é independente da arte.
- **Toda consulta aceita uma máscara de travessia.** Célula bloqueante cuja marca casa com a máscara conta como transponível **só naquela consulta**, por ator, sem estado. A lib nunca sabe o que a marca significa.
- **"Parede rachada" e "porta do chefe" nunca aparecem na lib.** São bits que o consumidor nomeia. Propor bit nomeado dentro da biblioteca é violação desta lei e achado de revisão.

**Lição de método registrada:** necessidade descrita em palavras por um consumidor é insumo **legítimo**; copiar o que a lib antiga fazia continua **proibido** (L-01 e L-28). A diferença é a forma, não a origem.

---

## §6 — Render e gráfico

**Origem:** `GODS_LAWS.md` L-31, 21/08/2026, decisão do líder.

**A API gráfica do GlintFx é OpenGL 3.3 core.**

**Por quê, na razão que decidiu:** roda **nativo nas duas plataformas** (por EGL no Linux, por WGL no Windows), sem camada de tradução, o que preserva a lei de dependência zero (L-07). E o piso de placa de vídeo é amplo, o que importa quando a base de consumidores é **aberta e desconhecida** (LEI ZERO).

**O que foi recusado, e por quê:** OpenGL 4.x traz recursos que um render 2D de quadrados não exige e corta máquina mais velha de consumidor que não conhecemos. OpenGL ES 3.x **não existe no Windows** sem camada de tradução de terceiro, e isso colide de frente com a L-07.

---

## §7 — Entrada e periféricos

### Parser XKB próprio

**Origem:** `GODS_LAWS.md` L-06 (duplicada por dúvida de classificação, não decisão do líder — a lista de escopo mínimo do parser é fato de produto, mas também é dimensionamento de trabalho; não separei os dois com confiança).

<!-- DUP-BLOCK:L06-XKB:START -->

**Data:** 21/08/2026, verbatim: *"usaremos nosso parser de keymap proprietario"*.

O compositor entrega o keymap em texto XKB por file descriptor. **Nós escrevemos o parser.** `libxkbcommon` está fora, mesmo instalado na máquina.

**Aplicação:** escopo mínimo do parser, para não subestimar a fatia: keycode para keysym, níveis e grupos de modificador, latch e lock, sequência de compose e tecla morta, keysym para UTF-8.

<!-- DUP-BLOCK:L06-XKB:END -->

### Entrada determinística: promessa pública

**Origem:** `GODS_LAWS.md` L-35, 23/08/2026, decisão do líder. **Origem do pedido:** cobrança do Gus Dragon na discussion 7 do bus, item (1), endereçada nominalmente ao GlintFx e ao GusWorld.

**A entrega de evento de entrada é uma PROMESSA PÚBLICA do contrato: determinística, sem duplicação e sem reordenação.** Nasce com teste que a prova, antes de existir a implementação (L-20).

**O caso concreto que a originou, e ele não é teórico:** o jogador aperta confirmar duas vezes rápido numa tela de carregamento que dura vários quadros, e entra em dois saves ao mesmo tempo. O Gus Dragon descreveu o bug e escreveu que **a lib e o consumidor são os dois responsáveis** — e estava certo, com a divisão que o `gusworld` fez e que adotamos:

- **Do consumidor, e é a maior parte:** o guarda "estou carregando, não aceito outro carregar" é regra de jogo. Pedir que a lib impeça isso exigiria que ela entendesse save, config e tela de carregamento — conceito de jogo que a **L-02** recusa. **Ninguém está pedindo isso.**
- **Nossa, e é o que esta lei fixa:** se o mesmo evento chegar duas vezes, ou se a ordem variar entre quadros, **o guarda do consumidor deixa de ser suficiente** — ele estaria filtrando um comando duplicado que nunca deveria ter existido.

**Por que virou promessa escrita e não "qualidade de implementação":** promessa dá ao consumidor em que se apoiar ao desenhar a lógica dele, e dá a nós um teste guardando. E é **barato agora**, porque não existe uma linha de código de entrada escrita — depois, endurecer vira quebra e afrouxar vira perda silenciosa.

### Cursor, áudio, gamepad, compose — as quatro decisões de 23/08/2026

**Origem:** `GODS_LAWS.md` L-36 (migrada inteira), 23/08/2026, quatro decisões de escopo do líder por `AskUserQuestion`. **Não são mais perguntas** — os itens correspondentes saem de `🎨 Pendente design`.

1. **Cursor do ponteiro (`WL-POINTER`): as DUAS opções, e quem escolhe é o consumidor.** Ou a lib lê o **tema de cursor do sistema**, e o ponteiro fica igual ao resto do desktop do usuário (inclusive o tema grande de quem depende dele por acessibilidade), ou o **consumidor fornece a própria imagem**. Nenhuma das duas é imposta. A leitura do tema depende de como classificamos a biblioteca de cursor do Wayland sob a **L-07** — se ela não contar como API do SO, lemos o formato de arquivo em casa.
2. **Áudio no Linux (`AUD-BACKEND`): ALSA agora, PipeWire depois.** ALSA é a interface do próprio kernel e existe em **toda** máquina Linux, inclusive nas que não têm servidor de som moderno; o PipeWire intercepta ALSA, então o caminho funciona também no desktop atual. O PipeWire nativo entra **depois**, como caminho adicional escolhido em tempo de execução — **aditivo, nunca substituto**. Motivo que decidiu: base de consumidores aberta e desconhecida (LEI ZERO), e nenhuma máquina pode ficar de fora em nenhum momento.
3. **Gamepad (`GP-MAP`): só o cru e a dedução própria. NENHUM banco de dados.** A lib entrega eixos e botões numerados mais o identificador do aparelho, e por cima disso deduz um layout genérico do que o próprio dispositivo declara. **Banco de mapeamento não entra**, nem de terceiro nem nosso.
4. **Compose e tecla morta (`KEYMAP-COMPOSE`): ler o arquivo de dados do sistema.** A tabela padrão vive em pasta cujo nome carrega "X11" por história, e o líder decidiu que **ler um arquivo de texto não é usar X11** — a **L-05** proíbe o protocolo, a biblioteca e o backend, não um arquivo de dados.

---

## §8 — Nomes de efeitos e propriedades (`gltfx-`)

**Origem:** `/var/tmp/glintfx-plan/nomes-decididos-lider-no_mdash.md`, 26/08/2026. **Copiado sem editar, sem resumir e sem corrigir pontuação** — as descrições do mundo são texto do líder, verbatim. **Ordem documental do líder, respeitada abaixo:** *"essas descricoes poeticas devem entrar na documentacao antes da descricao do efeito, de maneira a cotextualizar o nome"* — a descrição do mundo vem primeiro, a técnica depois, dentro de cada entrada.

> ORDEM DOCUMENTAL DO LÍDER, verbatim: *"essas descricoes poeticas devem entrar na
> documentacao antes da descricao do efeito, de maneira a cotextualizar o nome."*
> Ou seja: na doc pública, a descrição do mundo vem PRIMEIRO, a descrição técnica depois.

### Convenção de nome, FECHADA pelo líder em 26/08/2026

| O quê | Forma | Exemplo |
|---|---|---|
| **Nossa propriedade** | `gltfx-` mais Iniciais_Maiusculas com sublinhado | `gltfx-Light_Shine` |
| **Nosso efeito** (valor de animação) | `gltfx-` mais minúsculas com sublinhado | `gltfx-living_pulse` |
| **Qualquer palavra do padrão web** | nome e forma DO PADRÃO, sem marca nenhuma | `background-color`, `oklch()`, `mix-blend-mode` |

**A maiúscula distingue propriedade de efeito** (verbatim do líder: *"gltfx-[Iniciais_Maiusculas] para diferenciar dos efeitos"*), ou seja: o que se escreve à ESQUERDA dos dois pontos leva maiúscula; o que se escreve à DIREITA fica minúsculo.

**Marca só no que o padrão não tem.** O líder considerou prefixar também as palavras recentes do padrão (`oklch`, `mix-blend-mode`, `image-rendering`) e **recusou**, mantendo a decisão anterior. Verbatim: *"Nenhuma, volto à decisão anterior"* e *"deixa o padrao"*. Motivo que pesou: trecho de CSS copiado da web continua funcionando numa folha `.gfss`.

**Prefixo é `gltfx-`**, o mesmo dos tipos públicos `gltfx_err` e `gltfx_rslt`, e NÃO `glintfx-`.

### Propriedades nossas (2)

#### gltfx-Light_Shine
O Brilho. Quando o programa é gravado no naipe, ele não cria luz do nada. Rouba. O mini-computador da carta abre um canal fino para o fluxo de mana-fibra que corre pelas muralhas do reino e captura apenas a porção mais limpa, a que ainda não foi contaminada por sombra ou por código sujo. Essa luz pura é então forçada a se condensar na superfície da carta, como orvalho digital que se recusa a se espalhar. O resultado é o brilho. Não o fogo. Não o clarão de explosão. O brilho controlado: a runa acende, as bordas do naipe ganham um contorno que parece molhado de prata viva, e qualquer objeto ou criatura que a carta toque herda o mesmo resplendor por alguns ciclos de execução. É o programa que os bardos usam quando querem que algo seja visto sem ser destruído, quando a magia precisa iluminar sem queimar. Na mesa, a carta não estala. Ela apenas começa a suar luz. Um suor frio, constante, que não consome a própria fonte. Enquanto o programa roda, o brilho permanece. Quando o ciclo termina, a luz some de uma vez, como se alguém tivesse fechado a tampa de um lanterna-runa. É o primeiro programa que quase todo aprendiz grava. Porque é simples. Porque é bonito. Porque ensina a carta a respirar luz sem pedir permissão ao sol.

#### gltfx-Chaos_Seed
A Semente do Caos Primevo. Não é um número. É o grão roubado do instante anterior à primeira instrução. Quando o programa é depositado na carta, ele enterra esse grão no registrador mais fundo. Dali brotam sequências que mudam se você piscar. Dados de combate, gotas de chuva digital, o lado em que a moeda-runa cai, tudo nasce dessa semente. Os bardos mais velhos dizem que ela cheira a ozônio e a folha queimada. Quando a carta ativa, um único ponto de luz irregular aparece no verso, como uma estrela que se recusa a ficar no mesmo lugar.

### FORA da folha (decidido)

- **Corpo-limite dos glifos**: NÃO entra na folha. É ajuste do motor, definido na inicialização da biblioteca.

### Presets de LUZ (12)

- **gltfx-living_pulse** — O Pulso Vivo. O foco não é só a luz, é o fato de que ela carrega vida. O brilho sobe e desce como batimento cardíaco distante, quase inaudível. Não é o brilho de magia ativa. É o brilho de presença. Quando a carta é ativada sobre algo que espera, o objeto ou a criatura passa a ter um leve pulsar de luz que diz: "ainda estou aqui". Útil para indicar estado de idle vivo, sem parecer que o programa está prestes a explodir.
- **gltfx-low_throb** — O Latejar Fraco. Aqui o foco é a fraqueza. Os dois pulsos rápidos são curtos, quase sem força. A luz mal sobe e já cai. Depois vem a pausa longa, pesada, como se o coração precisasse juntar coragem para bater de novo. Ideal para indicar vida baixa sem gritar. A carta não grita. Ela lateja. E quem entende o ritmo sabe que o tempo está acabando.
- **gltfx-gentle_call** — O Chamado Suave. O nome carrega a intenção. O brilho não é só visual: é convite. A onda é contínua, constante, um pouco mais viva que a respiração de espera. A carta parece dizer "venha ver" sem nunca levantar a voz. Ideal para tutoriais, pistas, objetos que o jogador deve notar sem que o mundo grite. A luz não pisca. Ela respira mais depressa, só isso.
- **gltfx-pain_fade** — O Desaparecer da Dor. O nome mais lento. O programa grava o golpe como uma subida brusca e depois entrega toda a atenção à descida. A luz sobe rápido e demora a voltar ao zero, como se a dor ainda estivesse se arrastando para fora do corpo. O silêncio depois do fade é quase tão forte quanto o clarão inicial. AJUSTE DECIDIDO: descida mais longa (o clarão fica contido, para não confundir com o alert_cut).
- **gltfx-alert_cut** — O Corte de Alerta. O foco está no corte. A luz não sobe: ela aparece. Não desce: ela some. Como se alguém tivesse cortado o fluxo de mana-fibra com uma faca. O ritmo é seco, absoluto. Qualquer coisa que a carta toque passa a piscar como aviso final. Não é convite. Não é lembrete. É o último sinal antes da ação.
- **gltfx-torch_tremor** — O Tremor da Tocha. Lembra o fogo de verdade: a chama que dança sem nunca repetir o mesmo movimento. A luz sobe e desce de forma imprevisível, com pequenas falhas e recuperações súbitas. Qualquer fonte de luz que a carta toque passa a tremer como tocha real. Os bardos dizem que este é o programa que os guardas das masmorras antigas usavam para fazer as tochas parecerem vivas, e um pouco perigosas. USA A SEMENTE.
- **gltfx-sweep_glow** — A Varredura de Brilho. O programa mais limpo. Uma faixa de luz nasce numa borda da superfície e caminha até a borda oposta, constante, sem pressa. Quando chega ao fim, pode recomeçar ou desaparecer. Ideal para itens raros, equipamentos novos, pontos de interesse que merecem ser vistos. A carta parece ser limpa por uma mão invisível de luz.
- **gltfx-dust_spark** — A Faísca da Poeira. O foco está na poeira mágica. Os picos de luz são pequenos, rápidos e imprevisíveis, como se partículas invisíveis estivessem sendo atingidas por um feixe de mana. A luz aparece, some, reaparece noutro lugar. A superfície ganha a aparência de poeira mágica flutuando e cintilando. Os bardos dizem que é o que os alquimistas antigos usavam para fazer seus pós brilharem sem precisar de fogo. USA A SEMENTE.
- **gltfx-residual_heat** — O Calor que Resta. O nome mais preciso. A luz varia tão devagar e tão pouco que parece quase estática. Mas quem presta atenção percebe a oscilação fraca: o último calor que ainda não se foi. Qualquer superfície ganha o aspecto de algo que queimou e ainda não esfriou por completo. Ideal para brasas, ou o rastro de magia de fogo que ainda não dissipou. USA A SEMENTE.
- **gltfx-power_rise** — A Subida de Poder. A luz não sobe de golpe. Ela sobe. Ciclo após ciclo, mais intensa, mais presente, até o pico. Quando estoura, a liberação é clara: o poder acumulado sai de uma vez. Quando a carta toca um personagem ou arma que está carregando um ataque, o brilho conta a história inteira: o esforço, a espera, e o momento em que tudo é liberado.
- **gltfx-ripple_step** — O Passo da Ondulação. O nome mais visual. A luz salta de elemento em elemento como uma ondulação que se afasta do ponto inicial, e cada novo passo demora um pouco mais. O atraso crescente cria a sensação de que a onda está se dissipando enquanto avança. A superfície parece respirar em sequência, de dentro para fora, cada vez mais lenta. Ideal para propagação suave ou revelação gradual. AGE SOBRE CONJUNTO, NÃO SOBRE UM ELEMENTO.
- **gltfx-volt_shine** — O Brilho de Volt. O foco está na voltagem. A luz é alta, cortante e artificial. Não aquece. Ilumina de forma agressiva, como se a superfície estivesse sob alta tensão. Qualquer objeto ganha o aspecto de algo ligado à rede: vivo de eletricidade, não de fogo. Os bardos das cidades baixas dizem que este é o programa que os grafiteiros de runas usam para fazer suas marcas brilharem de noite sem precisar de tocha. (É o que era chamado de neon; receita de 4 camadas embutida.)

### Presets de MOVIMENTO (10)

- **gltfx-deny_jitter** — O Jitter da Negação. O nome mais nervoso. O tremor é rápido, irregular nas bordas, e termina com o retorno forçado à posição original. Parece que o elemento tentou escapar e foi puxado de volta. Qualquer tentativa inválida ou golpe recebido faz a superfície (ou o corpo) tremer de lado a lado antes de se recompor. Os bardos dizem que é o programa que os portões antigos usavam quando alguém tentava passar sem a chave certa: tremiam e recusavam.
- **gltfx-touch_spread** — A Expansão do Toque. O foco está no espalhamento. A onda circular parte do ponto tocado e cresce de forma contínua, levando consigo um leve brilho ou distorção. O movimento é claro: nasceu aqui, está indo para lá. Quando a carta toca um botão ou uma área interativa, a onda confirma o contato e se espalha até se dissipar. Útil para feedback de toque limpo e legível.
- **gltfx-leap_stretch** — O Esticar do Salto. O nome mais voltado para a partida. No momento em que o objeto deixa o solo, ele se alonga verticalmente, como se a força do impulso puxasse o corpo para cima. No pouso ele se achata, mas o momento mais marcado é o esticar. O salto ganha tensão visual: o corpo se estica na subida e se comprime na queda. Os bardos da animação antiga dizem que é o programa que faz o movimento parecer vivo em vez de mecânico.
- **gltfx-pop_scale** — O Pop de Escala. O programa mais limpo. O elemento aumenta de tamanho de forma brusca e imediatamente começa a voltar ao tamanho original. O movimento é curto, legível e satisfatório. Qualquer botão, ícone ou número que a receba "estoura" para fora e se recompõe. Ideal para feedback de clique, item coletado ou valor que sobe. A carta não explica. Ela só cresce e volta.
- **gltfx-drop_bounce** — O Quique da Queda. O foco está na chegada. O elemento parece ter sido solto e quica várias vezes, perdendo altura a cada impacto, até se estabilizar. O movimento carrega peso. Quando a carta toca um item que acaba de ser recebido ou um aviso que entra na tela, ele aterrissa com quiques sucessivos e cada vez menores. Útil para dar sensação física de "chegou".
- **gltfx-world_tremor** — O Tremor do Mundo. O nome mais amplo. O programa trata a tela como o próprio mundo visível. Tudo treme junto: personagens, cenários, interface. A sacudida é global. Qualquer explosão ou impacto pesado faz o jogador sentir que o chão sob os pés (e sob os olhos) cedeu por um instante. Os bardos das câmeras antigas dizem que é o programa que separa o golpe local do desastre verdadeiro. MECANISMO DIFERENTE DO deny_jitter: sacode tudo, não um elemento.
- **gltfx-reveal_in** — A Revelação. O programa mais limpo. O elemento começa invisível ou em escala zero e progride até ficar completamente visível e no tamanho final. O movimento é contínuo e legível. O painel se abre ou o item aparece como se estivesse sendo revelado pela primeira vez. Ideal para aberturas de interface, itens descobertos ou qualquer coisa que precise nascer na tela de forma clara.
- **gltfx-reveal_out** — A Revelação Invertida. O oposto direto de gltfx-reveal_in. O elemento começa inteiro e progride até desaparecer por completo: opacidade, escala ou ambos. O movimento é contínuo e legível. O painel se fecha ou o item some como se estivesse sendo recolhido de volta ao vazio. Ideal para fechamentos de interface, itens consumidos ou avisos que terminam.
- **gltfx-afterimage** — A Imagem Residual. O nome carrega o eco. As cópias não são apenas rastros: são imagens residuais, fantasmas do instante anterior. Mais fracas, mais transparentes, elas ficam para trás e se apagam em sequência. Quando a carta toca um personagem veloz ou um golpe, o movimento deixa um rastro de "ainda estava ali". Útil para reforçar a sensação de velocidade extrema ou de impacto que corta o ar.
- **gltfx-hover_idle** — A Espera em Suspensão. O foco está no estado de idle. O elemento paira no lugar com um movimento vertical suave, como se estivesse suspenso por uma força invisível. A oscilação é pequena e constante. Quando a carta toca um item colecionável ou um objeto que espera interação, ele ganha esse sobe-e-desce paciente. Útil para indicar "estou aqui, pode me pegar" sem gritar.

### Recusado

- **Alarme cadenciado** (pulso rápido e duro em ritmo fixo): NÃO ENTRA. O gltfx-alert_cut cobre.

---

## §9 — Empacotamento e artefatos

### Extensão de artefato: `.so`/`.dll` para binário, própria para dado

**Origem:** `GODS_LAWS.md` L-38 (migrada inteira), 24/08/2026, decisão do líder: *"mantenha .so e .dll"*, depois de perguntar se a extensão da biblioteca dinâmica podia ser trocada.

**O artefato binário do GlintFx usa as extensões que o sistema operacional espera: `.so` no Linux e `.dll` no Windows. Não se inventa extensão para binário.**

**A linha que esta lei traça, e ela vale para tudo:**

- **Arquivo de DADO é nosso.** Extensão própria é legítima e barata — é o caso do `.gw.map`.
- **Artefato BINÁRIO pertence à cadeia de ferramentas do sistema.** Trocar a extensão dele é brigar com o mundo inteiro sem ganhar nada.

**O que foi MEDIDO nesta máquina antes de decidir, e não deduzido:** uma biblioteca compilada como `libtst.gwso` **funciona**, mas `gcc -ltst` responde *"não foi possível localizar -ltst"*. Só linka com `-l:libtst.gwso`, que é **sintaxe exclusiva do GNU** e quebra em qualquer outra cadeia. O cache de bibliotecas desta máquina tem **3.531 entradas**, todas na convenção `.so`.

**No Windows** — registrado como **não medido**, por não haver a plataforma aqui: o carregador aceita qualquer extensão se o nome exato for dado, mas ele **acrescenta `.dll` sozinho** quando o nome vem sem extensão, e toda ferramenta de inspeção, instalador e depurador assume `.dll`.

### Embutimento no Windows: a lib garante o runtime encontrável

**Origem:** `TODO.md`, linha `EMBED-DLL` (W2), 25/08/2026, `AskUserQuestion`. **Movida em 26/08/2026.**

**A biblioteca põe o artefato de runtime onde o consumidor que a EMBUTE consegue encontrá-lo, no Windows** — mecanismo nosso, **ligado por padrão** quando embutida. Decisão do líder entre resolver pelo consumidor, só documentar a receita, oferecer opção, ou consertar só o teste. Ele foi na forma mais forte: **o consumidor desconhecido nunca deve ver este problema.**

**O fato que motivou:** quem faz `add_subdirectory` e constrói compartilhado no Windows **constrói, linka, e o programa não sobe** — o carregador do Windows só procura ao lado do executável, e no Windows não existe o equivalente ao caminho que o CMake embute sozinho no Linux.

#### EMENDA de 28/08/2026 à decisão 5: dezesseis unidades, e DUAS categorias novas

**Origem:** a fatia `GFSS-VALUE` foi implementar a decisão 5 e o agente reportou, sem que ninguém tivesse pedido, que **o texto dizia "treze" e a lista somava doze**. O líder pediu para **ver a lista** antes de decidir, conferiu item a item, e ampliou.

**O erro de contagem:** eram **doze** mesmo, e o "treze" era resquício de quando a porcentagem ainda contava como unidade, antes de ela virar categoria própria em 26/08. **O número está corrigido; a lista de doze que ele aprovou nunca esteve errada.**

**Quatro unidades ACRESCENTADAS por decisão dele em 28/08/2026, total agora DEZESSEIS:**

| Unidade | O que é | Por que entrou |
|---|---|---|
| `vmin`, `vmax` | o menor e o maior lado da janela | Mantêm a interface proporcional quando a janela muda de forma. Sem elas, todo consumidor refaz a conta à mão e erra em tela vertical. |
| `ch` | largura do algarismo zero da fonte | Largura medida em caracteres; casa com a estética de terminal do consumidor conhecido. |
| `lh` | a altura de uma linha de texto | Espaçamento em múltiplos de linha em vez de pixels soltos. |

**Recusadas, com razão declarada, para ele não gastar decisão de novo:** `cap`, `ic` e `Q` são de nicho tipográfico ou de impressão; a família de unidades de janela "pequena, grande e dinâmica" existe **só por causa da barra do navegador de celular**, que não temos.

**⚠️ E DUAS CATEGORIAS NOVAS, decididas por ele em 28/08/2026. Verbatim: _"entram agora"_.**

O achado que as trouxe: a decisão 5 fechava **cinco** categorias, mas o líder tem **vinte e dois efeitos nomeados**, dos quais **dez são de movimento**. Movimento exige **ângulo** (girar) e **tempo** (duração, atraso), e **nenhum dos dois é comprimento**. Eram categorias inteiras faltando, no mesmo nível de "número" e "porcentagem".

⚠️ **Por que não podia esperar:** categoria é **porta de mão única**. Acrescentar categoria depois **quebra folha já escrita** por quem consome. As unidades concretas de cada uma estão em desenho pelo CTO, sob a L-43 (busca antes de decidir).

**Sobre porcentagem virar unidade, cogitado e NÃO adotado:** reverteria a decisão dele de 26/08 de que porcentagem é categoria própria que **nunca vira comprimento na leitura**. A razão original continua de pé: **porcentagem de quê?** depende da propriedade, e guardar como comprimento obrigaria a resolver cedo demais, com a informação errada.

**Em desenho, por pedido dele nesta mesma conversa:** porcentagem como **operador que aninha com função** (poder escrever "metade da largura da janela" sem saber o número), e uma **função de composição de laço** (repetições mais intervalo). ⚠️ **As formas que ele escreveu são ILUSTRAÇÃO, não especificação** — ele mesmo marcou assim: *"essa 'sintaxe' foi exemplo, não sei o padrão"*.

**Impacto declarado:** `GFSS-VALUE` (`d47dff7`) foi entregue com **cinco** categorias e **doze** unidades. **Precisa ser emendada antes de fechar.**
