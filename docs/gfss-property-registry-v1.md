# Registro de propriedades da folha `gfss`, versão 1 — especificação de origem da fatia `GFSS-PROP-REGISTRY`

**Autor:** Caetano (CTO, modelo `fable`), em **modo autônomo** (L-34 do projeto: *"o `fable` decide no lugar do líder; o main registra em `DECISOES_AUTONOMAS.md` ao vivo"*). Ordem do líder de 05/09/2026, verbatim: *"eu já mandei modo autonomo! vc fica parando o tempo todo! deixe o clevel responder!"*.

**Data:** 05/09/2026, 15:06 (America/Recife). HEAD conferido: `70cfb61` (`git rev-parse --short HEAD`, no ramo `main`).

**Como ler a marca de cada decisão (L-27, fato separado de inferência):**

| Marca | Significa |
|---|---|
| **[LÍDER]** | decisão dele, com data; **não reaberta aqui** |
| **[CTO]** | decisão minha, tomada em modo autônomo, **a confirmar ou reverter por ele no registro** |
| **[PADRÃO]** | fato verificado na especificação pública, com a fonte citada na §9 |
| **[MEDIR]** | ponto em que faltou medição; decidido assim mesmo, com o que precisa ser medido depois |

**Fontes de origem desta lista, todas lidas antes de decidir:** `ESCOPO.md` §4 (as 5 decisões de 21/08, as 22 e as 8 e as 5 de 26/08, as 3 da fronteira de glifos), §8 (convenção de nome `gltfx-`), §9 (as seis decisões de 28/08 sobre ângulo e tempo, e a decisão "sempre no fim, nunca renumerar"); `/var/tmp/glintfx-plan/decisoes-27-08-tarde.md` (grupos A, B e C); as linhas do `TODO.md` que nomeiam propriedade (listadas na coluna "fatia que consome"); e a página publicada pela minha sessão anterior (`claude.ai/code/artifact/8ee11251-…`), que é o ponto de partida das 22 dúvidas.

**A frase-guarda, verbatim da L-21 global, vale para cada linha abaixo:** produto para distribuição, base de consumidores **aberta e desconhecida**; nenhuma propriedade sai porque "o jogo do líder não usa"; onde o padrão aceita, o produto aceita, salvo divergência **declarada** com razão.

---

## §1 — As três decisões estruturais

### E1 — O registro nasce COMPLETO, com dívida CONTADA e RUIDOSA  [CTO]

**Decisão:** o registro nasce com todas as propriedades da §6 (contagem derivada no fim deste arquivo), agrupadas por assunto na ordem da §6, e a partir daí **só cresce pelo fim** (regra do líder de 28/08/2026, *"Sempre no fim, nunca renumerar"*).

**O que resolve a tensão "aceita e ignora"**, que era a objeção real contra nascer completo. Três mecanismos, todos obrigatórios na fatia:

1. **Cada linha carrega a fatia que a lê** (coluna "fatia que consome" da §6). Isso vive na **tabela interna** do registro, não na superfície pública — nada a congelar.
2. **Cada linha carrega um estado interno `reserved` ou `applied`.** Nasce `reserved`. A fatia que a consome vira `applied` no mesmo commit em que passa a lê-la, e o teste dessa fatia afirma isso. **Enquanto uma propriedade estiver `reserved`, a leitura da folha (`GFSS-DECL-PARSE`) aceita a declaração — a folha é válida — mas emite um diagnóstico com linha, coluna e o identificador `property_reserved`**, dizendo ao autor que esta versão da biblioteca ainda não aplica aquela propriedade. Nunca silêncio: é exatamente o defeito que o líder mandou eliminar, tornado visível em vez de escondido.
3. **Portão pré-1.0:** o teste do registro enumera FECHADO a tabela e imprime `applied N / reserved M / total T` sempre (L-40, a contagem aparece mesmo quando passa). Antes da 1.0, `M > 0` é aceito e impresso; **na fatia de release 1.0, `M > 0` reprova**. Uma propriedade que ninguém lê não chega ao consumidor externo como promessa vazia.

**Por que não "quase vazio":** nome novo entra sempre no fim; se cada fatia trouxesse as suas, `margin-top` ficaria a vinte posições de `padding-top`, e o identificador público (contrato, L-26) refletiria ordem de chegada de fatias para sempre. Legibilidade da lista é requisito do formato (L-28, *"verboso vence curto"*), e o identificador é lido por quem depura folha.

**Custo de reverter:** baixo enquanto pré-1.0 (SOVERSION 0); depois da 1.0, renumerar sobe `A`.

### E2 — Palavra reservada de C++ ganha o sufixo `_keyword`, e a regra é geral, não uma lista de dois  [CTO]

**Decisão:** o identificador em código de um **valor** da folha é a própria palavra em `snake_case` (`block`, `flex_start`, `border_box`, `plus_lighter`). **Quando a palavra é reservada em C++ (ou é token alternativo: `and`, `or`, `not`, `xor`, `bitand`…), o identificador leva o sufixo `_keyword`**: `auto_keyword`, `static_keyword`. A palavra escrita na folha **não muda** (`width: auto`, `position: static`); a função `gltfx_gfss_keyword_name()` devolve a grafia da folha, sem sufixo.

**Por que sufixo só onde colide, e não em todos:** oitenta e tantos identificadores mais longos e feios para proteger de um erro que o compilador já acusa sozinho é custo sem ganho. **Por que uma REGRA e não "os dois que colidem":** a próxima palavra reservada (`inline`, `default`, `not`, `register`) chega sem precisar de decisão nova. A regra é verificável: o teste do registro enumera os identificadores e afirma que nenhum deles é palavra reservada (lista fechada de C++23), e que todo `_keyword` corresponde a uma.

**Conferido contra macro minúscula (L-40, enumeração fechada):** nenhum nome da §6 é igual a `min`, `max`, `near`, `far`, `small`, `hyper`, `interface`, `pascal`, `cdecl` (Windows) nem `linux`, `unix`, `major`, `minor`, `index` (GCC/Unix). `min_width` e `max_height` **contêm** `min`/`max` mas não são a macro (macro de função `min(a,b)` não casa com `min_width`). [MEDIR] o portão de colisão dos 6.217 cabeçalhos do Windows roda sobre a lista final na implementação, não sobre esta leitura.

### E3 — Das sete abreviações de fora, DUAS entram (`outline`, `flex-flow`) e CINCO ficam fora  [CTO]

**O que o líder já decidiu [LÍDER, 26/08/2026]:** nove abreviações entram (`margin`, `padding`, `border-width`, `border-style`, `border-color`, `border`, `border-radius`, `gap`, `overflow`); `flex` e `inset` ficam fora pela régua de legibilidade; a fronteira é *"shorthand posicional inventado por nós é proibido; os dos padrões entram por serem vocabulário público"*.

**A régua que aplico às sete, derivada das duas que ele derrubou:** abreviação do padrão entra quando a gramática dela é **livre de ordem por tipo** (cada pedaço se reconhece pelo que é, como `border: 2px solid red`) **ou** segue uma ordem posicional que ele **já aceitou** (topo-direita-baixo-esquerda; linha-coluna do `gap`). Fica fora quando dois pedaços do **mesmo tipo** só se distinguem pela posição, ou quando a abreviação **zera em silêncio** o que não foi escrito.

| Abreviação | Decisão | Razão pela régua |
|---|---|---|
| `outline` | **entra** | mesma forma de `border` (largura, estilo, cor, em qualquer ordem) |
| `flex-flow` | **entra** | direção e quebra são conjuntos de palavras disjuntos; ordem livre |
| `transition` | **fora** | dois `<time>` na mesma declaração: o primeiro é duração, o segundo é atraso, **só pela posição** — a pergunta de entrevista que veterano erra, igual ao `flex: 1` |
| `animation` | **fora** | mesmo defeito dos dois tempos, mais o nome da animação disputando com as palavras de curva |
| `background` | **fora** | zera em silêncio todo `background-*` não escrito; e posição/tamanho separados por barra posicional |
| `border-image` | **fora** | gramática de barras (`slice / width / outset`) posicional do mesmo tipo |
| `place-content`, `place-items`, `place-self` | **fora** | dois valores do mesmo tipo (alinhamento), o primeiro é o eixo transversal só pela posição |

**Mitigação obrigatória para as cinco de fora:** o diagnóstico de `GFSS-DECL-PARSE` para uma abreviação recusada **nomeia as longhands** ("esperava `transition-property`, `transition-duration`, …"). Quem copia da internet recebe erro **que ensina**, não erro mudo — é a régua do líder: *"erro de escrita se diagnostica com linha, coluna e o que se esperava"*.

**Custo de reverter, e é o argumento decisivo:** abreviação é assunto de leitura, não de registro. **Deixar fora é a direção reversível** (entra depois como acréscimo compatível, sobe `B`); pôr dentro é porta de mão única do formato (tirar quebra folha escrita). As onze abreviações vivem em `GFSS-SHORTHAND`, não na tabela da §6 — expandem para longhands na leitura e **não têm identificador público**.

---

## §2 — As cinco onde o padrão tem duas respostas

| # | Decisão | Marca | Razão |
|---|---|---|---|
| **A1** | `min-width`/`min-height` iniciam em **`auto`** | [CTO], [PADRÃO: css-sizing-3 §3.1.2, *"The initial value of auto is new; in CSS2 the initial value was zero"*] | É o que impede a caixa de encolher abaixo do próprio conteúdo — a mesma razão da decisão 14 do líder (i18n). Em fluxo de bloco `auto` resolve a 0, em item flex resolve ao mínimo do conteúdo; a distinção é de `LAYOUT-MIN-MAX`, e a doc do formato a declara. |
| **A2** | As três palavras iniciais são as **concretas**: `justify-content: flex-start`, `align-items: stretch`, `align-content: stretch`. **A palavra `normal` NÃO é aceita na v1** | [CTO], [PADRÃO: css-align-3 dá `normal`, css-flexbox-1 dá as concretas] | `normal` significa "depende do modo de layout" — é tabela de tradução ao lado, que a régua de legibilidade proíbe. Com só bloco e flex na v1, a concreta diz o que acontece. `normal` entra depois como acréscimo compatível se um segundo modo de layout (grid) chegar. As palavras `start`/`end` (sem `flex-`) também ficam fora da v1 pelo mesmo motivo: duas grafias para o mesmo efeito. |
| **A3** | `outline-color` inicia em **preto fixo**, igual à borda | [CTO], [PADRÃO: css-ui-4 diz `auto` → `currentColor`] | Uma regra só para toda a "casca" (borda, contorno, contorno de texto): cor própria, independente da cor do texto — é a decisão 4 do líder estendida à família inteira, não uma divergência nova de natureza diferente. **A palavra `auto` não é aceita** em `outline-color` (o significado dela depende de cor de destaque do sistema, que não temos). **Para quem quer o comportamento do padrão, entra a palavra-chave `currentColor` como valor de toda propriedade de cor** — ver D18. |
| **A4** | `line-height: normal` = **a métrica declarada pela própria fonte** (a que `FONT-MEASURE` já lê) | [CTO], [PADRÃO: css-inline deixa ao programa] | Não é divergência: é o que os programas fazem. Aceita também número puro (multiplicador, herdado como número), comprimento e porcentagem. [MEDIR] qual tabela da fonte é a fonte da métrica quando `hhea` e `OS/2` discordam — decisão de `FONT-MEASURE`, com paridade Linux/Windows provada, sem efeito na forma do registro. |
| **A5** | Quebra de linha pela forma **separada**: `text-wrap-mode: wrap \| nowrap`. **`white-space` (a palavra antiga) e `white-space-collapse` ficam FORA da v1** | [CTO], [PADRÃO: css-text-4 define `text-wrap-mode`, inicial `wrap`, e `white-space-collapse`, inicial `collapse`, como longhands de `white-space`] | `white-space` é uma palavra para cinco comportamentos — a régua proíbe. `white-space-collapse` fica fora porque sem marcação (`gfml` não está aberta) o texto chega literal pela API, e colapsar espaço é assunto de quem escreve marcação. `white-space: nowrap` copiado da internet recebe diagnóstico que nomeia `text-wrap-mode`. |

---

## §3 — As dezessete dúvidas, e uma décima oitava que elas criaram

### As três que paravam trabalho

**D3 — Como a folha aponta para uma imagem.** [CTO]
A folha usa **a forma do padrão, `url("…")`, e o texto entre aspas é guardado VERBATIM pelo leitor: a biblioteca não abre arquivo, não resolve caminho, não decodifica nada ao ler a folha.** A resolução é um **contrato opcional preenchido pelo consumidor** — a mesma forma da decisão 13 do líder para medir texto: a biblioteca **pergunta** a quem a usa "qual textura corresponde a esta referência?", e ele responde com o handle de textura de `R2D-TEXTURE`. Quem não responde continua funcionando: a imagem não pinta, e um diagnóstico de aplicação diz qual referência ficou sem resposta (nunca silêncio). O consumidor escolhe o esquema que quiser dentro das aspas (`url("hero.png")`, `url("atlas:hero")`); a biblioteca não interpreta.
*Por quê:* o líder ratificou em 27/08 que o carregamento **não guarda nada** porque *"escolher como guardar seria impor a nossa política a todo consumidor"*. Uma folha que carrega arquivo sozinha obriga a biblioteca a decidir quando carregar e quando soltar — exatamente a política que ele recusou. O contrato devolve essa escolha a quem a tem.
*O que cria:* um item PMU novo na trilha de render, **`R2D-IMAGE-RESOLVER`** (contrato público: referência → handle), dependência de `GFSS-OBJECT-FIT`, `R2D-NINEPATCH` e `LAYOUT-PSEUDO-BOXES` (para `content: url()`). Um resolvedor de conveniência ("carrega do disco relativo à folha, decodifica PNG, sobe textura, mantém enquanto a folha viver") fica **desenhado como ajuda opcional, separada**, não como comportamento embutido — o consumidor que a chama está escolhendo a política dela.
*Tipo no registro:* `<image>` = `none | url(<string>) | linear-gradient(…)` (o gradiente é a outra forma de `<image>` do padrão, e é de `R2D-GRADIENT`).

**D8 — Como a folha nomeia uma fonte.** [CTO]
`font-family` aceita a **gramática do padrão** (lista de nomes separados por vírgula, com ou sem aspas), e **cada nome é um apelido que o consumidor registrou** na biblioteca ao carregar o arquivo de fonte (bytes via `ASSET-LOAD`, estrutura via `FONT-TABLES`). O primeiro nome da lista que estiver registrado vence. Não existe enumeração de fontes do sistema: dependência zero (L-07) exclui `fontconfig`/DirectWrite, e o líder decidiu que a biblioteca não embarca fonte nenhuma (não há arquivo de fonte com licença compatível dentro do repositório).
*Valor inicial:* **`sans-serif`** — palavra do padrão, sem invenção. Resolve como qualquer outro nome: se o consumidor registrou algo sob `sans-serif`, usa; se não, usa **a primeira fonte registrada** (o registro de fontes tem uma fonte padrão, que é a primeira); se o registro estiver vazio, **texto não é desenhado e o diagnóstico de aplicação nomeia a família que faltou**. As famílias genéricas do padrão (`serif`, `monospace`, …) são nomes comuns aqui, sem mapeamento para fonte do sistema — divergência declarada na doc.
*Peso e itálico (`font-weight`, `font-style`):* **FORA da v1**. A trilha de fonte carrega um arquivo por apelido e não fabrica negrito; um consumidor que quer negrito registra a face em negrito sob outro apelido. Entram depois, como acréscimo compatível, quando o registro de fontes tiver faces por peso. `@font-face` (a forma do padrão de fazer esse registro na própria folha) fica **desenhada como acréscimo posterior**: hoje at-rule desconhecida já gera diagnóstico em `GFSS-SHEET-PARSE`, então nada é aceito em silêncio.

**D11 — Uma palavra, duas transparências.** [CTO — mantém a recomendação anterior]
A palavra do padrão, **`opacity`, fica com o significado do padrão: a de GRUPO** (o elemento e os filhos como um todo, consumida por `R2D-GROUP-OPACITY`). **A forma simples, por peça, NÃO é propriedade de folha na v1: é o parâmetro de transparência da própria API de desenho** (`R2D-BATCH`, o "eixo A" da decisão 20). Até `R2D-GROUP-OPACITY` existir, `opacity` fica `reserved` (E1): aceita, diagnosticada, **nunca pintada com o significado errado**. Se um dia a forma simples precisar de palavra na folha, ela nasce como propriedade `gltfx-` própria no fim da lista — nunca reaproveitando `opacity`.

### As outras catorze

| # | Decisão | Marca | Razão e consumidor |
|---|---|---|---|
| **D1** | `margin-*: auto` **entra** | [CTO] | É como se centraliza caixa em bloco (`margin-left`/`margin-right: auto`), e em item flex absorve o espaço livre no eixo, como no padrão. Em bloco, `auto` vertical vale 0. Consumidores: `LAYOUT-BLOCK-FLOW` e `LAYOUT-FLEX-MAIN`. Omitir seria armadilha para toda folha copiada. |
| **D2** | `overflow-x`/`overflow-y` com **`visible` e `hidden`**; `hidden` é pintado por **`GFSS-CLIP`** | [CTO] | `hidden` é recorte **ordenado pelo autor**, a mesma categoria de `clip-path` (a própria linha de `GFSS-CLIP` diz que corte ordenado não conflita com a decisão 15). Recorte retangular no `padding box` é o caso trivial do `inset()` que essa fatia já cria. `scroll` e `auto` ficam fora (sem área rolante na v1) com diagnóstico. **A linha de `GFSS-CLIP` no `TODO.md` precisa ganhar esta frase** — registro do main. |
| **D4** | `background-repeat` **entra**, inicial **`repeat`** | [CTO], [PADRÃO] | Valores `repeat`, `repeat-x`, `repeat-y`, `no-repeat` (a forma de dois valores, `space` e `round` ficam fora). Ficar fora faria o fundo nunca repetir, divergindo do padrão sem uma linha escrita. Consumidor: `GFSS-OBJECT-FIT` (a fatia que pinta imagem de fundo). |
| **D5** | `border-image-outset` **entra**, inicial 0 | [CTO] | É uma soma no retângulo que `R2D-NINEPATCH` já tem na mão; "moldura de jogo raramente usa" é argumento de consumidor único e não vale (L-21 global). Divergência por omissão custaria mais que as duas linhas de pintura. |
| **D6** | `outline-offset` **entra**, inicial 0 | [CTO], [PADRÃO: css-ui-4] | Consumidor `GFSS-OUTLINE`. Barata; sem ela o anel gruda na borda para sempre. |
| **D7** | `text-align: justify` **FORA da v1** | [CTO] | Exige distribuir o espaço sobrando na quebra de linha (`FONT-WRAP`), trabalho a mais na trilha de fonte. Entra depois como palavra nova (compatível). Valores v1: `left`, `right`, `center`; `start`/`end` fora (não há direção de escrita, decisão de 26/08). |
| **D9** | `letter-spacing` **entra**, inicial `normal`, herdada | [CTO], [PADRÃO] | Uma soma no avanço de cada glifo em `FONT-LINE`. |
| **D10** | A caixa de `::before`/`::after` **existe pelo `content`, como no padrão** — não pela presença do seletor | [CTO] | `content` entra com `normal \| none \| [<string> \| url(<string>)]+` (texto, imagem, ou a concatenação; `attr()` e contadores fora). `normal` vale `none` em pseudo-elemento. Razão: `::before { color: red }` sem `content` fabricar caixa vazia surpreende quem sabe CSS, e o padrão é o conhecimento que o consumidor já tem. Consumidor: `LAYOUT-PSEUDO-BOXES` (existência e texto) — **a linha dela no `TODO.md` deve dizer "existe quando `content` não é `none`"**, registro do main. A imagem de `content: url()` passa por `R2D-IMAGE-RESOLVER` (D3). |
| **D12** | `filter` aceita **as sete funções baratas do padrão**: `brightness()` (cobre escurecer E clarear), `contrast()`, `grayscale()`, `saturate()`, `hue-rotate()`, `invert()`, `sepia()` | [CTO], [PADRÃO: filter-effects-1] | Os sete efeitos do líder cabem em seis funções; `contrast()` ocupa a sétima vaga e é conta por pixel como as outras. `opacity()` como função de filtro fica **fora** (duplicaria a propriedade `opacity`, duas formas de dizer o mesmo). `blur()` é de `R2D-FILTER-BLUR`; `drop-shadow()` é de `R2D-DROPSHADOW`. A enumeração fechada de `R2D-FILTER-COLOR` continua **sete** — só troca "escurecer/clarear" por "brightness/contrast". |
| **D13** | `mix-blend-mode` aceita **`normal`, `plus-lighter`, `multiply`, `screen`** | [CTO], [PADRÃO: `plus-lighter` é de Compositing Level 2; `multiply`/`screen` de Level 1] | Critério verificável, não gosto: **são exatamente os modos que o OpenGL 3.3 core pinta com a mistura de função fixa, sem ler o fundo** (`ONE,ONE`; `DST_COLOR,ONE_MINUS_SRC_ALPHA`; `ONE,ONE_MINUS_SRC_COLOR`). Os demais (`overlay`, `hue`, …) exigem ler o pixel de trás e desenho fora da tela — custo de outra classe, fora da v1 com diagnóstico. Consumidor: `R2D-BLEND-ADDITIVE`, **cujo escopo cresce de "aditivo" para "os modos de função fixa"** — registro do main. [MEDIR] a fórmula exata de `multiply` com alfa cru (decisão 44: alfa não pré-multiplicado no tipo) precisa de teste numérico na fatia; a decisão de registro não depende disso. |
| **D14** | `gltfx-Velocity` **é herdada**; inicial 1; **faixa válida `[0, 3]`**, negativo recusado com diagnóstico | [CTO] — a faixa −3..3 da página anterior era **inferência minha**, não verbatim dele; o verbatim cobre só 0..3 | Herdar é a feature: uma linha põe um painel em câmera lenta. Semântica de herança do padrão (o filho que declara substitui, não multiplica). Inverter é `animation-direction: reverse`; velocidade negativa seria segunda forma de dizer o mesmo. |
| **D15** | `gltfx-Chaos_Seed`: `auto` é a palavra para "não fixa, varia"; inicial `auto`; **não herdada** | [CTO] | Herdar faria irmãos sortearem em sincronia (todas as tochas tremendo juntas). Quem quer reproduzir em teste fixa a semente no elemento. Consumidor: `ANIM-SEED`. |
| **D16** | `gltfx-Light_Shine` **NÃO entra no nascimento**; entra no fim da lista quando `ANIM-GLOW` fixar o tipo | [CTO] | Congelar tipo não desenhado é o erro que já custou reabertura (`VER-4C`). Ponto de partida registrado para `ANIM-GLOW`: `none \| <length> <color>` (raio do vazamento e cor da luz; intensidade acima do branco vem da própria cor, decisão 42). Como a lista é append-only, o custo de entrar depois é zero. |
| **D17** | = E3 | | |
| **D18** (criada por A3) | A palavra-chave **`currentColor` entra como valor de toda propriedade de cor** | [CTO], [PADRÃO: css-color-4] | É o que torna a divergência da cor de borda/contorno indolor: `border-color: currentColor` devolve o comportamento do padrão em uma linha. Resolve em `GFSS-INHERIT` (é o `color` computado do próprio elemento). **`GFSS-COLOR-PARSE` hoje não a tem** (medido: zero ocorrências em `src/gfss/named_colors.cpp`) — entra em `GFSS-DECL-PARSE` como palavra-chave de cor, não como cor literal. Registro do main. |

---

## §4 — Forma pública do registro (o que a revisão de API dedicada confere)

- **Espaço de nome e tipo:** `glintfx::style::gltfx_gfss_property`, `enum class` com base `std::uint16_t`, gerado por X-macro `GLINTFX_GFSS_PROPERTY_LIST(X)` — a mesma disciplina de `value.hpp` (contagem mecânica `gltfx_gfss_property_count`, tabela interna com `static_assert` que reprova nome sem linha). **O valor numérico do enumerador é o id público**, na ordem da §6, e **é append-only**: a guarda de estabilidade do teste tem uma tabela própria `(identificador, id esperado)` e reprova inserção no meio.
- **Nome na folha:** `gltfx_gfss_property_name(p)` devolve a grafia da folha (`"margin-top"`, `"gltfx-Text_Outline_Color"`), nunca frase (R7). O identificador em código é a grafia em `snake_case` minúsculo: `margin_top`, `gltfx_text_outline_color` (a marca `gltfx-Iniciais_Maiusculas` é da folha; em C++ a convenção da L-21 vence).
- **Palavras-chave:** `enum class gltfx_gfss_keyword`, base `std::uint16_t`, uma enumeração fechada para **todas** as palavras de valor da §6 (E2 para as reservadas), com `gltfx_gfss_keyword_name()`. Palavra que não pertence à propriedade é recusada em `GFSS-DECL-PARSE` com diagnóstico — a tabela interna diz quais palavras cada propriedade aceita.
- **Metadados por propriedade, na tabela interna (não na superfície pública):** tipo aceito, valor inicial (um `gltfx_gfss_value` ou uma palavra-chave), `inherited`, `reader` (fatia), `status` (`reserved`/`applied`). Só `inherited` e o `initial` são consultáveis publicamente, porque `GFSS-INHERIT` e o consumidor precisam deles: `gltfx_gfss_property_is_inherited(p)` e `gltfx_gfss_property_initial(p)`.
- **O que NÃO congela:** a gramática de cada tipo composto (`<shadow>`, `<transform-list>`, `<easing>`, `<filter-function-list>`, `<image>`, `<position>`) é assunto da fatia que a lê, e a representação computada dela vive fora deste registro — aqui só se congela **que a propriedade existe, qual id tem, o que ela aceita em nome, o inicial e se herda**.

---

## §5 — Regras transversais de valor, para não decidir duas vezes

1. **`inherit`, `initial`, `unset`** são aceitas em **toda** propriedade (decisão 10 do líder). Não aparecem na coluna "tipo" abaixo.
2. **`currentColor`** e **`transparent`** são aceitas em toda propriedade de cor (D18; `transparent` já existe em `GFSS-COLOR-PARSE`).
3. **Tempo sem unidade vale milissegundos** só nas propriedades de tempo (decisão 1 de 28/08 do líder — o padrão mora no esquema da propriedade, nunca na leitura).
4. **`animation-iteration-count: N / <time>`** é a extensão do líder (decisão 4 de 28/08); `N` sozinho é o padrão puro.
5. **`z-index` e `order` são naturezas inteiras**: decimal é recusado com linha, coluna e "o que se esperava".
6. **Espessura nomeada** `thin`/`medium`/`thick` = **1/3/5 px** (decisão 6 do líder), em borda e contorno.
7. **`transform-origin`** tem as duas escalas decididas pelo líder em 27/08: a do padrão (comprimento, %, palavras) e a dele, de −1 a 1, onde o zero puro é recusado com erro e o centro é a ausência de declaração.

---

## §6 — A lista: as propriedades da v1, na ordem em que nascem (o id é a posição)

Colunas: **id** · **nome na folha** · **identificador no código** · **tipo aceito** · **valor inicial** · **herda** · **fatia que consome** · **marca** (`=` igual ao padrão; `≠` diverge, com a decisão; `fixa` fixa o que o padrão deixava aberto; `nossa` não existe no padrão).

### Grupo A — Caixa e fluxo

| id | na folha | no código | tipo | inicial | herda | fatia que consome | marca |
|---|---|---|---|---|---|---|---|
| 0 | `display` | `display` | `block \| flex \| none` | `block` | não | `LAYOUT-TREE` | ≠ (líder, dec. 2: padrão é `inline`) |
| 1 | `box-sizing` | `box_sizing` | `border-box \| content-box` | `border-box` | não | `LAYOUT-BOX-MODEL` | ≠ (líder, dec. 3) |
| 2 | `width` | `width` | comprimento, %, `auto` | `auto` | não | `LAYOUT-SIZE-RESOLVE` | = |
| 3 | `height` | `height` | comprimento, %, `auto` | `auto` | não | `LAYOUT-SIZE-RESOLVE` | = |
| 4 | `min-width` | `min_width` | comprimento, %, `auto` | `auto` | não | `LAYOUT-MIN-MAX` | = (A1) |
| 5 | `min-height` | `min_height` | comprimento, %, `auto` | `auto` | não | `LAYOUT-MIN-MAX` | = (A1) |
| 6 | `max-width` | `max_width` | comprimento, %, `none` | `none` | não | `LAYOUT-MIN-MAX` | = |
| 7 | `max-height` | `max_height` | comprimento, %, `none` | `none` | não | `LAYOUT-MIN-MAX` | = |
| 8 | `aspect-ratio` | `aspect_ratio` | `auto \| <razão>` | `auto` | não | `LAYOUT-ASPECT-RATIO` | = |
| 9 | `margin-top` | `margin_top` | comprimento, %, `auto` | `0` | não | `LAYOUT-BLOCK-FLOW` / `LAYOUT-FLEX-MAIN` | ≠ comportamento (líder, dec. 5: margens somam); `auto` por D1 |
| 10 | `margin-right` | `margin_right` | comprimento, %, `auto` | `0` | não | `LAYOUT-BLOCK-FLOW` / `LAYOUT-FLEX-MAIN` | idem |
| 11 | `margin-bottom` | `margin_bottom` | comprimento, %, `auto` | `0` | não | `LAYOUT-BLOCK-FLOW` / `LAYOUT-FLEX-MAIN` | idem |
| 12 | `margin-left` | `margin_left` | comprimento, %, `auto` | `0` | não | `LAYOUT-BLOCK-FLOW` / `LAYOUT-FLEX-MAIN` | idem |
| 13 | `padding-top` | `padding_top` | comprimento, % | `0` | não | `LAYOUT-BOX-MODEL` | = |
| 14 | `padding-right` | `padding_right` | comprimento, % | `0` | não | `LAYOUT-BOX-MODEL` | = |
| 15 | `padding-bottom` | `padding_bottom` | comprimento, % | `0` | não | `LAYOUT-BOX-MODEL` | = |
| 16 | `padding-left` | `padding_left` | comprimento, % | `0` | não | `LAYOUT-BOX-MODEL` | = |
| 17 | `position` | `position` | `static \| relative \| absolute` | `static` | não | `LAYOUT-POSITION` | = (conjunto menor: `fixed`/`sticky` fora, sem área rolante); `static` → `static_keyword` (E2) |
| 18 | `top` | `top` | comprimento, %, `auto` | `auto` | não | `LAYOUT-POSITION` | = |
| 19 | `right` | `right` | comprimento, %, `auto` | `auto` | não | `LAYOUT-POSITION` | = |
| 20 | `bottom` | `bottom` | comprimento, %, `auto` | `auto` | não | `LAYOUT-POSITION` | = |
| 21 | `left` | `left` | comprimento, %, `auto` | `auto` | não | `LAYOUT-POSITION` | = |
| 22 | `z-index` | `z_index` | inteiro, `auto` | `auto` | não | `GFSS-ZINDEX` | = |
| 23 | `visibility` | `visibility` | `visible \| hidden` | `visible` | **sim** | `GFSS-VISIBILITY` | = (`collapse` fora) |
| 24 | `overflow-x` | `overflow_x` | `visible \| hidden` | `visible` | não | `GFSS-CLIP` | = (D2; `scroll`/`auto` fora) |
| 25 | `overflow-y` | `overflow_y` | `visible \| hidden` | `visible` | não | `GFSS-CLIP` | = (D2) |

### Grupo B — Caixas lado a lado (flex)

| id | na folha | no código | tipo | inicial | herda | fatia que consome | marca |
|---|---|---|---|---|---|---|---|
| 26 | `flex-direction` | `flex_direction` | `row \| row-reverse \| column \| column-reverse` | `row` | não | `LAYOUT-TREE` | = |
| 27 | `flex-wrap` | `flex_wrap` | `nowrap \| wrap \| wrap-reverse` | `nowrap` | não | `LAYOUT-FLEX-WRAP` | = |
| 28 | `flex-grow` | `flex_grow` | número | `0` | não | `LAYOUT-FLEX-MAIN` | = |
| 29 | `flex-shrink` | `flex_shrink` | número | `1` | não | `LAYOUT-FLEX-MAIN` | = |
| 30 | `flex-basis` | `flex_basis` | comprimento, %, `auto` | `auto` | não | `LAYOUT-FLEX-MAIN` | = |
| 31 | `justify-content` | `justify_content` | `flex-start \| flex-end \| center \| space-between \| space-around \| space-evenly` | `flex-start` | não | `LAYOUT-FLEX-MAIN` | fixa (A2) |
| 32 | `align-items` | `align_items` | `stretch \| flex-start \| flex-end \| center \| baseline` | `stretch` | não | `LAYOUT-FLEX-CROSS` | fixa (A2) |
| 33 | `align-self` | `align_self` | `auto \| stretch \| flex-start \| flex-end \| center \| baseline` | `auto` | não | `LAYOUT-FLEX-CROSS` | = ; `auto` → `auto_keyword` (E2) |
| 34 | `align-content` | `align_content` | `stretch \| flex-start \| flex-end \| center \| space-between \| space-around \| space-evenly` | `stretch` | não | `LAYOUT-FLEX-WRAP` | fixa (A2) |
| 35 | `order` | `order` | inteiro | `0` | não | `LAYOUT-TREE` | = |
| 36 | `row-gap` | `row_gap` | comprimento, % | `0` | não | `LAYOUT-FLEX-WRAP` | fixa (líder, dec. 6: padrão diz `normal`) |
| 37 | `column-gap` | `column_gap` | comprimento, % | `0` | não | `LAYOUT-FLEX-MAIN` | fixa (líder, dec. 6) |

### Grupo C — Borda, fundo e casca

| id | na folha | no código | tipo | inicial | herda | fatia que consome | marca |
|---|---|---|---|---|---|---|---|
| 38 | `border-top-width` | `border_top_width` | comprimento, `thin \| medium \| thick` | `medium` | não | `R2D-BORDER` | fixa (líder, dec. 6: 1/3/5 px) |
| 39 | `border-right-width` | `border_right_width` | idem | `medium` | não | `R2D-BORDER` | fixa |
| 40 | `border-bottom-width` | `border_bottom_width` | idem | `medium` | não | `R2D-BORDER` | fixa |
| 41 | `border-left-width` | `border_left_width` | idem | `medium` | não | `R2D-BORDER` | fixa |
| 42 | `border-top-style` | `border_top_style` | `none \| solid` | `none` | não | `R2D-BORDER` | = (conjunto menor) |
| 43 | `border-right-style` | `border_right_style` | `none \| solid` | `none` | não | `R2D-BORDER` | = |
| 44 | `border-bottom-style` | `border_bottom_style` | `none \| solid` | `none` | não | `R2D-BORDER` | = |
| 45 | `border-left-style` | `border_left_style` | `none \| solid` | `none` | não | `R2D-BORDER` | = |
| 46 | `border-top-color` | `border_top_color` | cor | preta | não | `R2D-BORDER` | ≠ (líder, dec. 4: padrão é `currentColor`) |
| 47 | `border-right-color` | `border_right_color` | cor | preta | não | `R2D-BORDER` | ≠ (líder, dec. 4) |
| 48 | `border-bottom-color` | `border_bottom_color` | cor | preta | não | `R2D-BORDER` | ≠ (líder, dec. 4) |
| 49 | `border-left-color` | `border_left_color` | cor | preta | não | `R2D-BORDER` | ≠ (líder, dec. 4) |
| 50 | `border-top-left-radius` | `border_top_left_radius` | comprimento, % | `0` | não | `R2D-BORDER` | = |
| 51 | `border-top-right-radius` | `border_top_right_radius` | comprimento, % | `0` | não | `R2D-BORDER` | = |
| 52 | `border-bottom-right-radius` | `border_bottom_right_radius` | comprimento, % | `0` | não | `R2D-BORDER` | = |
| 53 | `border-bottom-left-radius` | `border_bottom_left_radius` | comprimento, % | `0` | não | `R2D-BORDER` | = |
| 54 | `background-color` | `background_color` | cor | `transparent` | não | `R2D-BORDER` (fundo sólido, conforme a própria linha da fatia) | = |
| 55 | `background-image` | `background_image` | `none \| url(<string>) \| linear-gradient(…)` | `none` | não | `GFSS-OBJECT-FIT` (url, via `R2D-IMAGE-RESOLVER`) / `R2D-GRADIENT` (gradiente) | = (D3) |
| 56 | `background-repeat` | `background_repeat` | `repeat \| repeat-x \| repeat-y \| no-repeat` | `repeat` | não | `GFSS-OBJECT-FIT` | = (D4) |
| 57 | `background-size` | `background_size` | `auto`, comprimento, %, `contain \| cover` | `auto` | não | `GFSS-OBJECT-FIT` | = |
| 58 | `background-position` | `background_position` | `<position>` | `0% 0%` | não | `GFSS-OBJECT-FIT` | = |
| 59 | `border-image-source` | `border_image_source` | `none \| url(<string>)` | `none` | não | `R2D-NINEPATCH` (via `R2D-IMAGE-RESOLVER`) | = (D3) |
| 60 | `border-image-slice` | `border_image_slice` | número ou % (1 a 4), `fill` opcional | `100%` | não | `R2D-NINEPATCH` | = |
| 61 | `border-image-width` | `border_image_width` | comprimento, %, número, `auto` (1 a 4) | `1` | não | `R2D-NINEPATCH` | = |
| 62 | `border-image-outset` | `border_image_outset` | comprimento ou número (1 a 4) | `0` | não | `R2D-NINEPATCH` | = (D5) |
| 63 | `border-image-repeat` | `border_image_repeat` | `stretch \| repeat` | `stretch` | não | `R2D-NINEPATCH` | = (conjunto menor: `round`/`space` fora) |
| 64 | `outline-width` | `outline_width` | comprimento, `thin \| medium \| thick` | `medium` | não | `GFSS-OUTLINE` | fixa (1/3/5 px) |
| 65 | `outline-style` | `outline_style` | `none \| solid` | `none` | não | `GFSS-OUTLINE` | = (conjunto menor; `auto` fora) |
| 66 | `outline-color` | `outline_color` | cor | preta | não | `GFSS-OUTLINE` | ≠ (A3; padrão é `auto` → `currentColor`) |
| 67 | `outline-offset` | `outline_offset` | comprimento | `0` | não | `GFSS-OUTLINE` | = (D6) |
| 68 | `image-rendering` | `image_rendering` | `auto \| pixelated` | `auto` | **sim** | `R2D-IMAGE-RENDERING` | = (conjunto menor); `auto` → `auto_keyword` |

### Grupo D — Texto

| id | na folha | no código | tipo | inicial | herda | fatia que consome | marca |
|---|---|---|---|---|---|---|---|
| 69 | `color` | `color` | cor | preta | **sim** | `R2D-TEXT` | fixa (líder, dec. 6) |
| 70 | `font-family` | `font_family` | lista de nomes (apelidos registrados) | `sans-serif` | **sim** | `R2D-TEXT` (registro de fontes) | ≠ por resolução (D8: apelido, não fonte do sistema) |
| 71 | `font-size` | `font_size` | comprimento, % | `16px` | **sim** | `GFSS-RESOLVE` → `R2D-TEXT` | fixa (líder, dec. 6: padrão diz `medium`) |
| 72 | `line-height` | `line_height` | `normal`, número, comprimento, % | `normal` | **sim** | `FONT-LINE` | = (A4: `normal` = métrica da fonte) |
| 73 | `letter-spacing` | `letter_spacing` | `normal`, comprimento | `normal` | **sim** | `FONT-LINE` | = (D9) |
| 74 | `text-align` | `text_align` | `left \| right \| center` | `left` | **sim** | `FONT-LINE` | ≠ (líder, dec. 6 + 26/08: padrão é `start`); `justify` fora (D7) |
| 75 | `text-wrap-mode` | `text_wrap_mode` | `wrap \| nowrap` | `wrap` | **sim** | `FONT-WRAP` | = (A5; `white-space` fora) |
| 76 | `text-overflow` | `text_overflow` | `clip \| ellipsis` | `clip` | não | `FONT-ELLIPSIS` | = (só age com `overflow: hidden`) |
| 77 | `text-shadow` | `text_shadow` | `none \| <shadow>#` | `none` | **sim** | `R2D-TEXT-SHADOW` | = |
| 78 | `gltfx-Text_Outline_Color` | `gltfx_text_outline_color` | cor | preta | **sim** | `R2D-TEXT-OUTLINE` | **nossa** (líder, C1 de 27/08; nome por CTO, convenção §8) |
| 79 | `gltfx-Text_Outline_Width` | `gltfx_text_outline_width` | comprimento | `0` | **sim** | `R2D-TEXT-OUTLINE` | **nossa** (idem) |
| 80 | `content` | `content` | `normal \| none \| [<string> \| url(<string>)]+` | `normal` | não | `LAYOUT-PSEUDO-BOXES` (existência e texto); imagem via `R2D-IMAGE-RESOLVER` | = (D10) |

### Grupo E — Pintura e efeitos

| id | na folha | no código | tipo | inicial | herda | fatia que consome | marca |
|---|---|---|---|---|---|---|---|
| 81 | `opacity` | `opacity` | número em [0, 1] | `1` | não | `R2D-GROUP-OPACITY` | = (D11: significado de grupo, o do padrão) |
| 82 | `mix-blend-mode` | `mix_blend_mode` | `normal \| plus-lighter \| multiply \| screen` | `normal` | não | `R2D-BLEND-ADDITIVE` | = (D13; conjunto menor) |
| 83 | `filter` | `filter` | `none \| <filter-function-list>` | `none` | não | `R2D-FILTER-COLOR` (7 baratas), `R2D-FILTER-BLUR` (`blur()`), `R2D-DROPSHADOW` (`drop-shadow()`) | = (D12; `opacity()` fora) |
| 84 | `backdrop-filter` | `backdrop_filter` | `none \| <filter-function-list>` | `none` | não | `R2D-BACKDROP-BLUR` | = |
| 85 | `box-shadow` | `box_shadow` | `none \| <shadow>#` | `none` | não | `R2D-BOXSHADOW` | = |
| 86 | `clip-path` | `clip_path` | `none \| inset() \| circle() \| ellipse()` | `none` | não | `GFSS-CLIP` | = (`polygon()` fora, já decidido) |
| 87 | `transform` | `transform` | `none \| <transform-list>` | `none` | não | `R2D-TRANSFORM` | = (inclinar e matriz entraram em 27/08) |
| 88 | `transform-origin` | `transform_origin` | as duas escalas (§5, item 7) | centro (ausência) | não | `R2D-TRANSFORM` | ≠ (líder, 27/08: zero puro recusado na escala dele) |
| 89 | `pointer-events` | `pointer_events` | `auto \| none` | `auto` | **sim** | `INPUT-HIT-ORDER` | = (conjunto menor); `auto` → `auto_keyword` |

### Grupo F — Animação

| id | na folha | no código | tipo | inicial | herda | fatia que consome | marca |
|---|---|---|---|---|---|---|---|
| 90 | `transition-property` | `transition_property` | `none \| all \| <nome de propriedade>#` | `all` | não | `ANIM-TRANSITION` | = |
| 91 | `transition-duration` | `transition_duration` | `<time>#` | `0s` | não | `ANIM-TRANSITION` | ≠ (líder, 28/08: sem unidade = ms) |
| 92 | `transition-timing-function` | `transition_timing_function` | `<easing>#` | `ease` | não | `ANIM-EASING` | = |
| 93 | `transition-delay` | `transition_delay` | `<time>#` | `0s` | não | `ANIM-TRANSITION` | ≠ (sem unidade = ms) |
| 94 | `animation-name` | `animation_name` | `none \| <nome de @keyframes> \| gltfx-<preset>` | `none` | não | `ANIM-TIMELINE` (blocos) / `ANIM-PRESETS` e irmãs (receitas) | ≠ (líder: receitas `gltfx-*` como valor) |
| 95 | `animation-duration` | `animation_duration` | `<time>#` | `0s` | não | `ANIM-TIMELINE` | ≠ (sem unidade = ms) |
| 96 | `animation-timing-function` | `animation_timing_function` | `<easing>#` | `ease` | não | `ANIM-EASING` | = |
| 97 | `animation-delay` | `animation_delay` | `<time>#` | `0s` | não | `ANIM-TIMELINE` | ≠ (sem unidade = ms) |
| 98 | `animation-iteration-count` | `animation_iteration_count` | `<número> \| infinite`, com `/ <time>` opcional | `1` | não | `ANIM-TIMELINE` | ≠ (líder, 28/08: pausa entre repetições; valor único é o padrão puro) |
| 99 | `animation-direction` | `animation_direction` | `normal \| reverse \| alternate \| alternate-reverse` | `normal` | não | `ANIM-TIMELINE` | = |
| 100 | `animation-fill-mode` | `animation_fill_mode` | `none \| forwards \| backwards \| both` | `none` | não | `ANIM-TIMELINE` | = |
| 101 | `animation-play-state` | `animation_play_state` | `running \| paused` | `running` | não | `ANIM-TIMELINE` | = |
| 102 | `gltfx-Velocity` | `gltfx_velocity` | número em [0, 3] | `1` | **sim** | `ANIM-VELOCITY` | **nossa** (líder, 27/08; herança e faixa por D14) |
| 103 | `gltfx-Chaos_Seed` | `gltfx_chaos_seed` | inteiro, `auto` | `auto` | não | `ANIM-SEED` | **nossa** (líder, 26/08; D15); `auto` → `auto_keyword` |

**Contagem derivada da tabela (comando, não número de cabeça):** `grep -cE '^\| [0-9]+ \|' /var/tmp/glintfx-plan/gfss-registro-v1.md`. O resultado no momento da escrita está no relatório ao main, não aqui, para este arquivo não gravar número que apodrece.

### As onze abreviações (expandem na leitura; não têm id)

`margin`, `padding`, `border-width`, `border-style`, `border-color`, `border`, `border-radius`, `gap`, `overflow` [LÍDER, 26/08] e `outline`, `flex-flow` [CTO, E3]. Vivem em `GFSS-SHORTHAND`; a enumeração fechada daquela fatia passa a contar **onze**.

---

## §7 — O que fica FORA da v1, com razão, para não ser redescoberto

| Fora | Razão | Direção |
|---|---|---|
| `font-weight`, `font-style` | um arquivo por apelido; sem síntese de negrito (D8) | entra depois, compatível |
| `text-decoration-*`, `text-transform` | sem fatia que pinte sublinhado nem que mude caixa de letra; **ausência consciente, provável de fazer falta** — primeira candidata a acréscimo pós-1.0 | entra depois |
| `cursor` | o líder deu o cursor à API, não à folha (23/08) | decidido |
| `float`, `clear`, `display: inline*`, `grid-*` | fluxo da v1 é bloco + flex; grid adiado, não vetado (líder, dec. 1) | grid entra depois |
| `position: fixed \| sticky`, `overflow: scroll \| auto`, `scroll-*` | sem área rolante | entra depois |
| `direction`, `unicode-bidi`, `writing-mode`, `start`/`end` | sem direção de escrita; é onde `text-align: left` se apoia — se entrarem, aquela decisão volta à mesa | entra depois, com a decisão reaberta |
| `will-change`, `contain`, `content-visibility` | dicas de otimização recusadas pelo líder (*"nós controlamos o nosso"*) | decidido |
| `perspective`, `backface-visibility`, `transform-style` | 3D fora, decisão do líder | decidido |
| `white-space`, `white-space-collapse`, `word-break`, `hyphens` | A5; sem marcação, texto é literal | entra depois |
| `opacity()` como função de filtro; `mix-blend-mode` além dos quatro; `filter: url()` | D12, D13 | entra depois |
| `gltfx-Light_Shine` | D16: nasce em `ANIM-GLOW`, no fim da lista | entra depois |
| tabela, lista, coluna, `resize`, `user-select`, `appearance`, `caret-color` | sem correspondente numa biblioteca 2D sem marcação | decidido |
| `animation-composition`, `animation-timeline`, `transition-behavior` | fora do padrão estável que milhões conhecem; sem fatia | entra depois |

---

## §8 — O que este arquivo obriga o main a registrar (não é código; é `DECISOES_AUTONOMAS.md` e linhas do `TODO.md`)

1. **Item novo [PMU]:** `R2D-IMAGE-RESOLVER` — contrato público opcional "referência de imagem → handle de textura", pré-requisito de `GFSS-OBJECT-FIT`, `R2D-NINEPATCH` e `LAYOUT-PSEUDO-BOXES` (D3). Posição na fila: logo após `R2D-TEXTURE` (W8).
2. **`GFSS-CLIP`** ganha `overflow-x/-y: hidden` = recorte retangular no padding box (D2).
3. **`LAYOUT-PSEUDO-BOXES`**: a caixa existe quando `content` não é `none`/`normal` (D10).
4. **`R2D-BLEND-ADDITIVE`**: escopo cresce para os quatro modos de função fixa (D13).
5. **`R2D-FILTER-COLOR`**: a enumeração fechada de sete passa a ser `brightness, contrast, grayscale, saturate, hue-rotate, invert, sepia` (D12).
6. **`GFSS-SHORTHAND`**: onze abreviações, não nove (E3).
7. **`GFSS-DECL-PARSE`**: emite `property_reserved` (E1), recusa abreviação de fora nomeando as longhands (E3), aceita `currentColor` como palavra-chave de cor (D18).
8. **`GFSS-INHERIT`**: resolve `currentColor` (D18).
9. **`GFSS-PROP-REGISTRY`**: a prova (L-40) imprime `applied/reserved/total`; portão de release 1.0 reprova `reserved > 0` (E1); teste de palavra reservada de C++ (E2); guarda de estabilidade por tabela `(identificador, id)`.
10. **`ANIM-GLOW`**: fixa o tipo de `gltfx-Light_Shine` e a acrescenta no fim (D16).
11. **`ANIM-VELOCITY`**: faixa `[0, 3]`, herdada (D14) — a página anterior dizia −3..3 por inferência minha.

---

## §9 — Fontes verificadas nesta sessão (L-27/L-43: fato de terceiro com fonte, separado do que foi medido em casa)

- `https://www.w3.org/TR/css-sizing-3/` — `min-width`/`min-height` inicial `auto`; nota *"in CSS2 the initial value was zero"* (A1).
- `https://www.w3.org/TR/css-ui-4/` — `outline-color: auto` (→ `currentColor` salvo `outline-style: auto`), `outline-style: none`, `outline-width: medium`, `outline-offset: 0` (A3, D6).
- `https://www.w3.org/TR/css-text-4/` — `text-wrap-mode: wrap | nowrap`, inicial `wrap`; `white-space-collapse`, inicial `collapse`, ambas longhands de `white-space` (A5).
- `https://www.w3.org/TR/css-align-3/` — `justify-content`/`align-content` inicial `normal`; para item flex, `normal` *"behaves as stretch"* (A2).
- `https://www.w3.org/TR/compositing-1/` — `mix-blend-mode` inicial `normal`; `plus-lighter` **não** está no Level 1 (é do Level 2), `multiply` e `screen` estão (D13).
- `https://www.w3.org/TR/css-fonts-4/` — `font-family` inicial *"depends on user agent"*, herdada; `font-weight`/`font-style` inicial `normal` (D8).
- **Medido em casa:** `src/gfss/named_colors.cpp` tem `transparent` e **não** tem `currentColor` (D18); `include/glintfx/gfss/value.hpp` fixa a disciplina de X-macro e contagem mecânica que o registro herda (§4); as linhas do `TODO.md` citadas na coluna "fatia que consome" foram lidas uma a uma em `70cfb61`.

## §10 — O que ficou sem medição (decidido assim mesmo, com o que conferir depois)

1. **Colisão de macro no Windows** para os nomes novos (`content`, `filter`, `order`, `screen`, `multiply`…): a leitura acima não achou colisão minúscula; o portão dos 6.217 cabeçalhos roda na implementação (E2).
2. **Fórmula de `multiply`/`screen` com alfa cru** no OpenGL 3.3 de função fixa: decidido pelo critério "expressável sem ler o fundo"; o valor numérico se prova por amostragem de pixel em `R2D-BLEND-ADDITIVE` (D13).
3. **`line-height: normal`**: qual tabela da fonte (`hhea` × `OS/2`) e paridade do resultado entre Linux e Windows — `FONT-MEASURE` (A4).
4. **Ruído do diagnóstico `property_reserved`** para quem escreve folha antes da 1.0: sem consumidor externo hoje, não há como medir incômodo. **Correção, GFSS-DECL-PARSE (D-DP-2), 09/09/2026:** esta linha estava errada sobre o tipo — `gltfx_gfss_diagnostic` (`include/glintfx/gfss/token.hpp`) **não tem** campo de severidade; tem `line`, `column`, `expected` e, desde D-DP-3, `detail` (identificadores, nunca severidade). O remédio ao ruído que GFSS-DECL-PARSE efetivamente construiu foi outro: o resultado do bloco (`declaration_list_parse_result`) carrega **duas listas separadas** — `declarations` (aceitas, cada uma podendo levar o aviso `property_reserved` preso a ela) e `rejected` (as descartadas) — em vez de um campo de severidade dentro do próprio tipo de diagnóstico. Se um nível de severidade genérico ainda fizer falta, continua em aberto para `GFSS-API` (wave W10) decidir; não é mais descrito aqui como "campo que já existe".
5. **Se `font-family: sans-serif` como inicial produz surpresa** quando o consumidor registra uma única fonte com outro nome: a regra "cai na primeira registrada" cobre; confere-se no teste de `R2D-TEXT` com registro de um nome só (D8).
