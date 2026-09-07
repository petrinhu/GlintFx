# As três dúvidas represadas da INBOX, fechadas pela busca

STATUS: decidido, 06/09/2026 22:22:41 (hora real de `date`, America/Recife). Autor: Caetano (CTO, modelo `fable`). Medido contra a árvore em `abbec43` (HEAD no instante da escrita). **Só este arquivo foi escrito**: código, CMake, testes, workflows e `TODO.md` não foram tocados, nada foi commitado, nenhum build, container ou espelho local rodou.

**Método, que é ordem do líder e não escolha minha** (verbatim, 06/09/2026): *"Nas proximas decisoes, nao apra para me perguntar, busque na web a resposta, ou nas dores da comunidade, leve a um clevel e ele vai responder. Depois traga tudo para mim quando eu pedir. NUNCA escolha pelo mais fácil, priorize a comunidade, depois documentacao tecnica na web, busque nos repos rmlui e sdl3 apenas como exemplo (se nao buscou quebrou lei do projeto e reporte bug), depois mais eficaz."* Cada seção abaixo segue essa ordem de fontes: dor da comunidade, documentação técnica, os dois projetos de referência lidos como exemplo (L-29: aprender, nunca copiar), e só então a eficácia. Toda fonte externa vem com endereço e data; o que foi medido em casa vem separado, marcado **[FATO]** quando é arquivo:linha ou saída de comando, e **[INFERÊNCIA]** quando é conclusão minha (L-18/L-27).

**Declaração sobre o SDL3, para não parecer omissão:** o SDL3 não tem motor de folha de estilo, seletor nem identificador com escape. Para as duas primeiras dúvidas ele foi lido atrás do **princípio análogo** (como ele trata pedido que o sistema não sustenta; onde ele decodifica texto com escape), não de código equivalente, e cada seção diz o que se aprendeu e o que não existe lá. Para a terceira dúvida ele é exemplo direto (o CI dele roda portões versionados no próprio repositório).

---

## 1. `:placeholder-shown`

### 1.1 A dúvida, pelo efeito

Quem escreve uma folha de estilo pode hoje escrever uma regra para "campo de texto vazio mostrando a dica", a folha é aceita sem reclamar, e a regra **nunca se aplica a nada**, em silêncio, para sempre.

### 1.2 O que a árvore diz hoje **[FATO]**

- `src/gfss/selector_pseudo_vocabulary.hpp:82`: `"placeholder-shown"` está na lista fechada das 14 pseudo-classes simples que o leitor aceita.
- `src/gfui/compound_match.cpp:156-171`: das 14, cinco viram bit de estado (`hover`, `active`, `focus`, `focus-visible`, `checked`), sete são estruturais, e as duas que sobram (`placeholder-shown`, `scope`) marcam o composto como `deferred` ("adiado"), sem dono.
- `docs/node-view-and-matching.md:17` e `:31`: o quinto dos oito fatos que o consumidor responde sobre a árvore dele é `state`, **uma chamada** que devolve até cinco sinalizadores independentes de uma vez.
- `docs/node-view-and-matching.md:134`: o próprio documento público já declara que a regra "volta adiada, para sempre, para esta pseudo-classe específica" e que resolver isso "é decisão de produto".

### 1.3 A dor da comunidade (peso maior)

A pergunta que a comunidade responde é: **o que dói mais, regra que não funciona em silêncio, ou regra que passa a dar erro?** A resposta é unânime e antiga: o silêncio.

- **Bastien Calou, "What happens when you write an invalid CSS selector? Bad things", dev.to, 07/12/2020.** Relato de autor de folha: combinou `:active` com `:focus-visible` num mesmo seletor e, em navegador sem `:focus-visible`, **as duas metades sumiram**, sem aviso: *"If any part of a selector fails, the whole selector fails."* A dor descrita é exatamente a nossa em espelho: o autor só descobre que a regra não vale **olhando a tela**, nunca por mensagem. (https://dev.to/bcalou/what-happens-when-you-write-an-invalid-css-selector-bad-things-4734)
- **stylelint, regra `selector-pseudo-class-no-unknown`, pedida na issue #1098 em 21/04/2016** e existente até hoje. A comunidade construiu uma ferramenta inteira para fazer **na leitura** o que o navegador não faz: avisar quando uma pseudo-classe não vai funcionar. A documentação da regra justifica: pseudo-classe desconhecida faz o navegador descartar a regra inteira em silêncio, e isso "é difícil de depurar". Que exista uma regra de lint só para isso, mantida por dez anos, é a medida da dor. (https://stylelint.io/user-guide/rules/selector-pseudo-class-no-unknown/ ; https://github.com/stylelint/stylelint/issues/1098; o corpo da issue em si é de uma linha, a motivação está na página da regra e no artigo acima, digo isso para não inflar a fonte)
- **CSS Working Group, issue #7676 (aberta 02/09/2022), resolvida em 07/12/2022:** *"RESOLVED: Make has unforgiving"* e *"RESOLVED: Limit forgiving behavior to :is and :where and remove it everywhere else"*. O grupo que escreve a linguagem **recuou** de uma decisão de tolerância silenciosa depois que ela quebrou consumidor real (o jQuery, que dependia de o seletor inválido **dar erro** para cair na rota própria dele; com a tolerância, `ul:has(li:contains('Item'))` passou a "casar zero elementos" em silêncio). A lição literal da ata: a lista do que é tolerado tem de ser **curta e fácil de entender**. (https://github.com/w3c/csswg-drafts/issues/7676)
- **Onde procurei e não achei:** dor específica sobre `:placeholder-shown` no Stack Overflow. A busca dirigida trouxe só tutoriais (dev.to, 2020-2021) cujas reclamações são de uso ("não funciona porque falta o atributo `placeholder`", "não funciona em `textarea` no Chrome/Safari"), não de motor. Isso é resultado legítimo: a pseudo-classe em si é bem aceita; a dor está no **silêncio**, não no nome.

**O que isso decide sozinho:** das três saídas, "nunca casa (declarado)" é a pior, porque é exatamente a tolerância silenciosa que a comunidade passou dez anos combatendo e que o CSSWG revogou. Sobram "virar estado de verdade" e "recusar na leitura".

### 1.4 Documentação técnica

- **Selectors Level 4, §12.1.3 (rascunho do editor, lido em 06/09/2026):** *"The :placeholder-shown pseudo-class matches an input element that is showing such placeholder text, whether that text is given by an attribute or a real element, or is otherwise implied by the UA."* A pseudo-classe está na seção **"Input Control States"**, ao lado de `:checked` e `:indeterminate`, e o exemplo remete à linguagem hospedeira (HTML) para dizer **quem** mostra placeholder. (https://drafts.csswg.org/selectors-4/#placeholder-shown-pseudo)
- **MDN:** disponível em todo navegador desde janeiro de 2020, sem prefixo de fabricante. (https://developer.mozilla.org/en-US/docs/Web/CSS/:placeholder-shown)

**O que a especificação garante de fato (L-43, item 4):** "placeholder visível" é um **estado do controle**, informado por quem hospeda o controle, não uma propriedade dedutível da folha nem um atributo. É a mesma natureza de `:checked`, que já modelamos como bit de estado. A saída "casar por atributo" não tem sustentação na especificação: presença de atributo `placeholder` não diz se o campo está vazio, e "vazio" não é atributo.

### 1.5 Os dois projetos de referência, lidos como exemplo (L-29)

**RmlUi** (lido pela web, sem clonar):

- `Source/Core/StyleSheetNode.cpp`, função `Match()`: pseudo-classe **não é lista fechada no motor**. O seletor guarda nomes, e o casamento é `element->IsPseudoClassSet(name)` para cada nome. Estruturais (`nth-child` etc.) são separadas numa lista própria (`structural_selectors`).
- `Source/Core/StyleSheetParser.cpp`: nome de pseudo-classe que não é estrutural é **aceito na leitura** e guardado como texto para casar em tempo de execução.
- `Source/Core/Elements/WidgetTextInput.cpp`, função `SetValueOrPlaceholder()`: **quem liga `placeholder-shown` é o próprio controle de texto**, não o motor de estilo: `const bool showing_placeholder = value.empty() && !placeholder.empty();` seguido de `parent->SetPseudoClass("placeholder-shown", showing_placeholder);`.
- Documentação RCSS (`pages/rcss/selectors.html`): `:placeholder-shown` está na lista pública de pseudo-classes suportadas.

**O que aprendi (técnica, não código):** o mercado resolve "placeholder visível" como **estado informado por quem é dono do controle**, e o motor só compara. No RmlUi o dono é a própria biblioteca (ela tem os controles); no GlintFx o dono é o consumidor, porque a árvore é dele (é a decisão dos oito fatos, `ESCOPO.md` §... "os oito fatos ficam finais", ratificada pelo líder). Portanto o lugar natural é o mesmo canal pelo qual ele já informa `hovered`/`checked`: o fato `state`.

**O que NÃO vamos fazer, e por quê:** o RmlUi aceita **qualquer** nome de pseudo-classe, e a consequência é a que a comunidade CSS condena: um erro de digitação (`:hovered`) vira regra que nunca casa, em silêncio. Nosso leitor tem vocabulário fechado de propósito; isso fica.

**SDL3** (lido pela web, sem clonar):

- Não existe análogo direto. O princípio análogo está em como o SDL3 trata pedido que a plataforma não sustenta: `SDL_SetWindowOpacity()` **devolve `false` e escreve o motivo em `SDL_GetError()`** quando opacidade não é suportada, em vez de fingir que aplicou. (https://wiki.libsdl.org/SDL3/SDL_SetWindowOpacity)

**O que aprendi:** recusa declarada é preferível a "aceitei e não fiz nada". Mesma lição da comunidade CSS, vinda de outro domínio.

### 1.6 A decisão, fechada

**`:placeholder-shown` vira estado de verdade: o sexto sinalizador do fato `state`, informado pelo consumidor na mesma chamada que já informa os outros cinco.**

Por quê esta e não as outras duas:

1. **"Nunca casa (declarado)" está eliminada pela comunidade e pelo CSSWG** (§1.3): é tolerância silenciosa, o defeito com mais dor registrada no domínio.
2. **"Recusar na leitura" está eliminada pela regra de paridade** (L-21 global: *"se o motor/biblioteca substituído aceita, o produto novo aceita também"*): o RmlUi aceita e casa `:placeholder-shown`. Recusar seria cortar capacidade que o motor de referência tem, e seria também a saída **mais fácil** (uma linha a menos na lista), que a ordem do líder proíbe escolher.
3. **"Estado de verdade" é o que a especificação descreve** (§1.4: "Input Control State", ao lado de `:checked`) **e o que o mercado faz** (§1.5: o dono do controle liga o estado; o motor compara).

**Forma que a fatia toma** (para o planejador, não é implementação): o valor devolvido por `state` ganha um sexto bit, `placeholder_shown`; a tabela que traduz nome de pseudo-classe em bit (`src/gfui/state_pseudo_class_table.hpp`) ganha a linha; `compound_match.cpp` deixa de marcar `placeholder-shown` como adiada; `docs/node-view-and-matching.md` deixa de dizer "cinco" e apaga o item 1 da seção de lacunas. **Nenhum fato novo, nenhuma chamada nova**: continuam oito fatos e uma chamada de estado.

**Portão que nasce junto (L-36/L-40), para o defeito não voltar com outro nome:** um teste que enumera **a lista inteira** das pseudo-classes simples aceitas pelo leitor e exige que cada nome tenha dono declarado: bit de estado, casador estrutural, ou a lista explícita e curta do que é adiado por desenho (hoje só `scope`, que pertence à consulta e não ao nó, `docs/node-view-and-matching.md:22`). Um 15º nome aceito sem dono **reprova**, em vez de virar "adiado para sempre" mais uma vez. É a lição literal do CSSWG: a lista do que é tolerado tem de ser curta e explícita.

**O que se perde:** o valor de `state` reabre de layout (cinco bits viram seis). Aceitável pelo mesmo critério que reabriu `version` no `VER-4C`: pré-1.0, `SOVERSION` 0, nenhum consumidor externo conhecido. Precisa fechar **antes** de `GFSS-API` (W10) congelar a superfície, como a linha da INBOX já exige.

**O que custa a quem já usa:** nada para um consumidor sem campo de texto: bit não ligado é zero, e zero é o que `state` já devolve hoje quando nada está ativo. Para um consumidor com campo de texto: ligar um bit que ele já sabe calcular (valor vazio e dica presente), na chamada que ele já implementa.

---

## 2. Identificador com escape

### 2.1 A dúvida, pelo efeito

Quem escreve `.a\:b` na folha para casar a classe `a:b` (nome com dois-pontos, comum em nomenclaturas de utilitários) nunca casa nada: a barra de escape chega ao casador dentro do nome, e `a\:b` não é `a:b`.

### 2.2 O que a árvore diz hoje **[FATO]**

- `include/glintfx/gfss/token.hpp:69-81`: o cabeçalho declara, **marcado como inferência da própria fatia (L-27)**, que `lexeme` é o trecho cru do texto-fonte, sem escape resolvido, e justifica: resolver escape seria "responder duas perguntas" na mesma unidade.
- `src/gfss/lexical_rules.cpp:64-71` e `:93-95`: o léxico já **reconhece e atravessa** escape válido (`is_valid_escape`, `consume_escaped_code_point`), só não **produz** o valor decodificado.
- `src/gfss/selector_parse.cpp:328`: o nome da classe entra no seletor como `tokens[name_index].lexeme`, cru.
- `src/gfui/compound_match.cpp`: comparação exata, byte a byte (INBOX, achado de 01/09/2026).

### 2.3 A dor da comunidade (peso maior)

- **csso (minificador), issue #428, 19/12/2020, e tailwindcss #3141 (mesmo caso):** o seletor gerado pelo Tailwind `.sm\:focus-within\:text-opacity-5:focus-within{}` **derruba o leitor** com `SyntaxError: Unexpected input`. O relator provou que o seletor é válido (outra ferramenta o lê). Em 06/09/2026 a issue continua aberta com rótulo "more details needed", ou seja, **a dor ficou sem conserto**: ferramenta que não decodifica escape na camada léxica quebra ou não casa. (https://github.com/css/csso/issues/428 ; https://github.com/tailwindlabs/tailwindcss/issues/3141)
- **Firefox, bug 883044:** `querySelector("#Tools:PrivateBrowsing")` falha com "not a valid selector", e a resposta do ecossistema foi criar **`CSS.escape()`**, função padrão só para produzir o escape correto. A comunidade paga esse custo do lado de **quem escreve** o seletor; em troca espera que **quem lê** decodifique sempre. Nós hoje aceitamos o escape e não decodificamos: cobramos o custo e não entregamos a contrapartida. (https://bugzilla.mozilla.org/show_bug.cgi?id=883044 ; https://developer.mozilla.org/en-US/docs/Web/API/CSS/escape_static)
- **Firefox, bug 543428, aberto em 01/02/2010, FIXED:** `selectorText`/`cssText` devolviam o seletor **sem** a barra de escape (*"It's then impossible to reuse the selector or the associated rule"*). Esta é a dor da **outra ponta** da nossa dúvida, a fidelidade ao texto original, e o conserto do mercado foi decisivo para nós: David Baron não guardou o texto cru; escreveu **"a function for serializing an identifier"**, que **re-escapa a partir do valor decodificado**. Ou seja, o mercado não preserva o texto original; ele o reconstrói. (https://bugzilla.mozilla.org/show_bug.cgi?id=543428)

### 2.4 Documentação técnica

- **CSS Syntax Module Level 3, §4.3.11 "Consume an ident sequence":** *"Let result initially be an empty string. Repeatedly consume the next input code point ... [if] the stream starts with a valid escape: Consume an escaped code point. Append the returned code point to result ... Return result."* E na definição dos tokens: `<ident-token>`, `<function-token>`, `<hash-token>`, `<string-token>`, `<url-token>` **têm um valor** composto de pontos de código, isto é, **decodificado**. A especificação decide o lugar: **o tokenizer**. (https://www.w3.org/TR/css-syntax-3/#consume-name)
- **CSS Syntax §4.3.7 "Consume an escaped code point":** 1 a 6 dígitos hexadecimais mais um espaço opcional viram o ponto de código; EOF vira U+FFFD; qualquer outro caractere é devolvido literalmente. Isto é o **escopo completo** do decodificador; menos que isso não é escape CSS.
- **CSSOM, "serialize an identifier":** a serialização parte do **valor** do identificador, caractere a caractere, re-escapando o que precisa. Confirma o bug 543428: o texto original não é o que se preserva. (https://drafts.csswg.org/cssom/#serialize-an-identifier)

**O que a especificação garante e o que só parece garantir (L-43, item 4):** ela garante que o **valor** do token é decodificado. Ela **não** exige que o texto cru se perca: a posição no texto-fonte continua disponível, e é assim que os motores reais recuperam o original quando precisam (§2.5).

### 2.5 Os dois projetos de referência, lidos como exemplo (L-29)

**Servo `rust-cssparser`, `src/tokenizer.rs` (o tokenizer do Firefox):** `consume_name()` tem **dois caminhos**. Sem escape, devolve **uma fatia do texto-fonte, sem cópia** (`return tokenizer.slice_from(start_pos).into()`). Ao encontrar `\` ou byte nulo, muda para o caminho lento: copia o que já leu, decodifica escape por escape (`consume_escape_and_write`) e devolve texto **próprio**. O tipo do valor é `CowRcStr`: emprestado quando não há escape, dono quando há. O texto cru continua recuperável por posição (`position()`/`slice_from()`). (https://github.com/servo/rust-cssparser/blob/master/src/tokenizer.rs)

**Blink (Chromium), `css_tokenizer.cc`:** mesma forma. Caminho rápido: *"Names without escapes get handled without allocations"*, devolve uma vista do buffer. Ao ver `'\0'` ou `'\\'`: *"We need escape-aware parsing"*, constrói string nova com `ConsumeEscape()`. (https://github.com/chromium/chromium/blob/main/third_party/blink/renderer/core/css/parser/css_tokenizer.cc)

**RmlUi, `Source/Core/StyleSheetParser.cpp`:** não tem tokenizer separado; o leitor de seletor anda caractere a caractere e chama `UnescapeSelectorToken()` para nome de elemento, id, classe, pseudo-classe e atributo. A função **só remove a barra e mantém o caractere seguinte** (`if (c == '\\' && (p + 1) != token.end()) { result += *(p + 1); ++p; }`). **Não decodifica a forma hexadecimal** (`\3A ` não vira `:`). A documentação RCSS não menciona escape em lugar nenhum (conferido em `pages/rcss/selectors.md` e `syntax.html`).

**SDL3, `src/SDL_utils.c`, `SDL_URIToLocal()`/`SDL_URIDecode()`:** decodifica `%XX` **uma vez, na fronteira onde o texto entra**, e entrega o valor decodificado para todo o resto; o texto codificado não é preservado; URI que não é local devolve erro (`-1`) em vez de resultado parcial.

**O que aprendi (técnica, não código):**

1. **Todos os leitores reais decodificam na camada léxica**, com a especificação. Nenhum deles empurra a decodificação para quem monta a regra, e o RmlUi mostra o preço de decodificar "perto do uso": ficou incompleto (sem forma hexadecimal), porque cada ponto de uso implementa o que lembra.
2. **A fidelidade ao texto original não se resolve guardando texto cru no valor; resolve-se guardando a posição** (Servo/Blink) e **re-serializando a partir do valor** quando alguém precisa do texto (CSSOM, bug 543428).
3. **Custo zero quando não há escape** é a norma: caminho rápido sem alocação, caminho lento só quando a barra aparece.

### 2.6 A decisão, fechada

**O escape é desfeito no leitor de símbolos (tokenizer), uma vez, para os tipos de token que a especificação diz que carregam valor (identificador, função, hash, string, url), e o token passa a carregar as duas coisas: o trecho cru (`lexeme`, como hoje) e o valor decodificado.** Quem monta a regra e quem casa consomem **só o valor**.

Por quê esta e não a outra:

1. **É o que a especificação prescreve** (§2.4) e **o que os quatro leitores lidos fazem** (§2.5). Decodificar em quem monta a regra é a saída do RmlUi, e o resultado medido lá é um decodificador incompleto; e foi o "cada consumidor precisa lembrar" que **já produziu o nosso defeito** (INBOX, 01/09/2026). Repetir o desenho que causou o defeito seria escolher o caminho mais curto, não o mais eficaz.
2. **A objeção "o tokenizer deixa de devolver o texto original" cai**: o token continua com `lexeme` cru (nada do que hoje o consome muda), e ganha o valor ao lado. É exatamente a forma Servo/Blink: cru por posição, decodificado por valor.
3. **A objeção do cabeçalho de `token.hpp` ("responder duas perguntas")** foi registrada como **inferência** da própria fatia, não como fato nem ordem do líder (L-27, texto do arquivo). Ela pesou quando o tokenizer era a primeira de 23 fatias e nada consumia valor; hoje o casador consome, e a especificação que o tokenizer diz seguir define o token **pelo valor**. A pergunta que o token responde continua uma: "que símbolo é este". O valor é o símbolo; o escape é grafia.

**Forma que a fatia toma** (para o planejador): o decodificador é o **completo** do §4.3.7 (1 a 6 dígitos hexadecimais mais espaço opcional; U+FFFD para EOF, zero e substitutos; literal para o resto), nunca o "tira a barra" do RmlUi. Caminho rápido: sem escape no trecho, o valor **é** o `lexeme` (mesma vista, sem cópia). Caminho lento: só quando há `\`, o valor ganha armazenamento próprio. **[INFERÊNCIA]** a linha `GFSS-ESCAPE-EOF-DIAG` já proposta para a W7 (escape cortado no fim do arquivo) é vizinha desta e deve ser planejada na mesma fatia ou logo depois, para o decodificador nascer com o caso de EOF testado, não remendado.

**Portão que nasce junto (L-36/L-40):** teste que **casa** `.a\:b` com a classe `a:b`, **casa** `.a\3A b` com a mesma classe, e prova por sabotagem (vermelho visto) que o casador lê o valor e não o `lexeme`. Um ident sem escape tem de provar que valor e `lexeme` apontam para os mesmos bytes (nenhuma cópia paga quando não há escape).

**O que se perde:** o token deixa de ser dois ponteiros (uma vista) quando há escape: passa a poder ter armazenamento próprio. Isso muda o layout do token, que `GFSS-API` (W10) pretende congelar; por isso a linha da INBOX manda decidir **antes**. Decidido agora, o layout congela já certo.

**O que custa a quem já usa:** nada para quem só lê `lexeme` (continua igual). Quem hoje compara nomes com `lexeme` (o casador, `selector_parse.cpp:328`) passa a comparar com o valor: é o conserto do defeito, não custo extra.

---

## 3. `DASH-PUBDOC`: o portão de travessão não enxerga a documentação pública deste projeto

### 3.1 A dúvida, pelo efeito

O leitor externo que abrir a documentação pública desta biblioteca vai encontrar o travessão longo que a regra da casa proíbe em texto voltado a ele (L-32 global), porque o único portão que existe para isso mora fora do repositório e não considera esses documentos "públicos".

### 3.2 O que a árvore e a máquina dizem hoje **[FATO]**

- Contagem de travessões (U+2014), arquivo a arquivo, medida em 06/09/2026 22:15 contando as ocorrências do ponto de código U+2014 em cada arquivo (`grep -o` do caractere, seguido de `wc -l`):

| arquivo | travessões | leitor declarado no próprio arquivo (L-21 do projeto) | primeiro commit |
|---|---|---|---|
| `docs/api-conventions.md` | **70** | consumidor externo (o `README.md` o cita quatro vezes como "the full contract") | 25/08/2026 |
| `docs/gl-loop-portability-matrix.md` | **15** | consumidor externo (*"Audience: a consumer of this library"*, linha 3; citado no `README.md:128`) | 06/09/2026 |
| `docs/gfss-property-registry-v1.md` | **66** | nós (pt-br, "especificação de origem da fatia") | 05/09/2026 |
| `docs/plano-w6b-placa-e-laco.md` | **46** | nós (pt-br, plano de onda) | 06/09/2026 |
| `docs/plano-conserto-fachadas-uaf.md` | 1 | nós | 06/09/2026 |
| `docs/plano-conserto-vermelhos-w6b.md` | 1 | nós | 06/09/2026 |
| `README.md`, `CHANGELOG.md`, `PACKAGING.md`, os outros 8 de `docs/` | 0 | (mistos) | |

  **85 travessões em dois documentos que se declaram escritos para o consumidor externo**, e a linha da INBOX (medida mais cedo em 06/09) dizia "70 num arquivo, zero nos outros cinco": a lacuna **cresceu no mesmo dia**, porque quatro dos seis arquivos com travessão nasceram em 05 e 06/09. O buraco não é estático.
- **O portão global tem duas encarnações, com a mesma classificação:** `~/.claude/hooks/no_mdash.py` (dispara nas ferramentas Write/Edit do agente; **não vê** escrita por Bash, `sed`, heredoc, que é como este próprio documento foi escrito) e `~/.claude/githooks/no_mdash_staged.py` (dispara no `git commit`, sobre o conteúdo staged, e **importa** `is_user_facing()` do primeiro para não duplicar a regra). Nenhum dos dois roda no servidor. A classificação de "público" é por nome (`README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`...) e por pasta (`/public/`, `/site/`, `/docs/site/`...): `docs/*.md` e `PACKAGING.md` **não casam**.
- **O que o repositório já tem para receber um portão de texto:** `tests/tools/check_spdx.py` (varre todo arquivo rastreado e não rastreado, conta o que varreu, tem `--selftest` com controles positivo, negativo e de varredura vazia, registrado como `ctest` sem guarda de sistema, roda nos jobs Linux e Windows do CI); `tools/git-hooks/pre-commit` (hook **versionado**, instalado por `install.sh`, que encadeia `check_dep_zero.py` e `check_spdx.py`); `tests/tools/check_precommit_hook_chain.py` (reprova se um portão obrigatório sumir do hook, lista `REQUIRED_GATES`); `tools/preci.sh` (espelho local do CI). O job `leis` do CI virou casca: os portões que ele rodava migraram para `ctest` para valerem nas cinco plataformas (comentários do próprio `ci.yml`, linhas 2346-2420).

### 3.3 A dor da comunidade (peso maior)

- **Hook local não é política, é conveniência.** Atlassian, tutorial de git hooks: *"local hooks affect only the repository in which they reside, and each developer can alter their own local hooks, so you can't use them as a way to enforce a commit policy."* (https://www.atlassian.com/git/tutorials/git-hooks)
- **Hook não viaja com o clone.** É a reclamação fundadora do projeto `pre-commit`: *"sharing our pre-commit hooks across projects is painful. We copied and pasted unwieldy bash scripts from project to project"*. E a resposta da mesma comunidade para "então como garantir?": *"adding `pre-commit run --all-files` as a CI step will ensure everything stays in tip-top shape"*, isto é, **o mesmo portão, versionado no repositório, disparado no commit E no servidor**. Também nasceu o serviço `pre-commit.ci` para *"enforce passing hooks on pull requests even if the committer does not have a local installation"*. (https://pre-commit.com/)
- **`core.hooksPath` global é a solução da comunidade para "mesma regra em todos os repositórios da máquina"**, e é exatamente o que esta máquina já faz; a comunidade a usa para regra **da pessoa**, não para regra **do projeto**, porque ela não existe em nenhuma outra máquina.

### 3.4 Documentação técnica

- **git, `githooks(5)`:** hooks vivem em `$GIT_DIR/hooks` ou em `core.hooksPath`; **não são copiados no clone** (`git init` pode copiar de um diretório-modelo, `git clone` não traz os do repositório de origem); e o `pre-commit` *"can be bypassed with the `--no-verify` option"*. (https://git-scm.com/docs/githooks)
- **Lei do projeto, `CLAUDE.md`, "Estado atual do repositório":** *"portão que nunca rodou no ambiente real não é portão; o verde que ele exibe é o de rodada local, e rodada local compartilha a máquina com quem o escreveu."* Escrita depois de um portão quebrar na estreia no servidor (25/08/2026).
- **L-40 global:** todo portão nasce com piso de varredura não-vazia, conta o que varreu, e **enumera o espaço fechado por construção** em vez de procurar dentro dele por suspeita.
- **L-32 e L-62 globais:** não modificar canon externo ao repositório sem copiar antes; atualizar X não autoriza mexer em Y vizinho. O portão global é canon de **outra** jurisdição (todos os projetos da máquina) e serve a outros repositórios; alargá-lo para este projeto muda comportamento onde este projeto não manda.

### 3.5 Os dois projetos de referência, lidos como exemplo (L-29)

**SDL3, `.github/workflows/generic.yml`, job "Check Sources":** roda `build-scripts/test-versioning.sh`, `python build-scripts/check_android_jni.py`, `python build-scripts/check_stdlib_usage.py`. Os portões de higiene do SDL **moram no repositório** (`build-scripts/`) e **rodam no servidor** a cada execução. Nenhum depende de configuração da máquina de quem escreveu. (https://github.com/libsdl-org/SDL/blob/main/.github/workflows/generic.yml)

**RmlUi, `.github/workflows/build.yml`:** sete jobs (Linux, sanitizers, legacy, macOS, Windows, MinGW, Emscripten), todos de compilação e teste. **Não há nenhum portão de formatação, lint ou documentação no CI**; a única trava de qualidade é `-DRMLUI_WARNINGS_AS_ERRORS=ON`. É exemplo do que **não** queremos: higiene dependendo da máquina de cada contribuidor.

**O que aprendi:** o projeto de referência que leva higiene a sério (SDL) a versiona e a roda no servidor; o que não a versiona (RmlUi) simplesmente não a tem no CI. Não existe no mercado o modelo "regra do projeto vive na configuração global da máquina do mantenedor".

### 3.6 A decisão, fechada

**Este projeto ganha portão próprio, versionado em `tests/tools/`, na forma exata de `check_spdx.py`: registrado como `ctest` sem guarda de sistema (roda nas cinco plataformas dentro dos jobs que já existem), encadeado em `tools/git-hooks/pre-commit`, listado em `REQUIRED_GATES` de `check_precommit_hook_chain.py`, e espelhado em `tools/preci.sh`. O portão global fica intocado.**

Por quê esta e não alargar o global:

1. **Alargar o global não produz portão**, pela lei do projeto: nunca roda no servidor, não existe em nenhum clone, e é contornável por `--no-verify`. Seria a saída **mais fácil** (uma linha numa lista em Python fora do repositório), o que a ordem do líder proíbe; e é a que a comunidade descarta há uma década (§3.3).
2. **O "um lugar só" que o global parece oferecer é falso para este projeto:** hoje já são **dois** lugares (o hook de ferramenta e o hook de commit), com definição de "público" que serve a todos os repositórios da máquina e por isso nunca vai conhecer a régua de leitor deste projeto (L-21 do projeto, tabela "quem lê"). E alargar o global muda comportamento em repositórios que não pedem isso (L-62).
3. **O repositório já tem a forma pronta e provada** (§3.2): um portão de texto que enumera a árvore inteira, conta, tem os três controles do autoteste, roda em `ctest` e no hook versionado. O custo marginal é um arquivo na mesma forma dos vizinhos.

**A régua de "público" do portão, decidida aqui para não ficar em aberto (L-40, item 5, enumeração fechada; L-02 global, fail-closed):** o portão varre **todo** `.md` e `.txt` da árvore (rastreado e não rastreado, como `check_spdx.py`) e reprova travessão em qualquer um, **exceto** os documentos cujo leitor a L-21 do projeto declara ser "nós", listados **explicitamente por caminho ou padrão fechado** dentro do portão: `GODS_LAWS.md`, `CONTRACT.md`, `TESTES.md`, `AUDITORIAS.md`, `AGILE.md`, `TODO.md`, `DECISOES_AUTONOMAS.md`, `CLAUDE.md`, `ORG.md`, `pipeline_release_1.0.md`, `lideranca_pipeline_release.md`, `TOOLING.md`, `DEPLOY_CHECKLIST.md`, `ESCOPO.md`, e em `docs/`: `plano-*.md`, `decisoes-*.md`, `auditoria-*.md`, e este arquivo. **Padrão negativo por padrão, exceção explícita**, nunca lista de "públicos": um documento público nascido amanhã com nome novo entra no portão sem ninguém lembrar de listá-lo, que é o modo exato pelo qual o global falhou. **[INFERÊNCIA]** `docs/gfss-property-registry-v1.md` cai hoje na exceção pelo próprio cabeçalho (pt-br, especificação de origem de fatia); quando `DOCS-PUB` traduzi-lo para o consumidor, o tradutor perde a exceção, e o portão morde. Se o líder preferir régua mais dura (nenhum `.md` do repositório público carrega travessão, porque o repositório inteiro é público), a exceção encolhe a zero e o desenho não muda; registro a alternativa, não a escolho, porque a L-21 do projeto traça a linha pelo leitor e a L-32 global fala de "texto user-facing".

**Portão nasce vermelho (L-36):** o autoteste prova os três controles (positivo, negativo, varredura vazia), e a **primeira rodada real contra a árvore reprova**, pelos 85 travessões dos dois documentos públicos (e pelos 46 do plano da W6b, se a régua dura for a escolhida). A fatia que cria o portão limpa esses documentos no mesmo commit ou no seguinte, com o vermelho visto e registrado antes: é a única prova de que o portão morde.

**O que se perde:** a definição de "travessão em texto público" passa a existir em duas jurisdições (a global da máquina e a deste repositório), que podem divergir. A divergência é **assimétrica e inofensiva**: o portão do repositório é o mais estrito e é o que vale aqui; o global continua sendo o que sempre foi, guarda de digitação do agente para todos os projetos. Nenhum caso em que o global aprove e o repositório reprove passa, porque o do repositório roda depois, no commit e no servidor.

**O que custa a quem já usa:** a quem clona o repositório, nada até rodar `ctest` (aí o portão roda como qualquer outro teste). A quem contribui documentação pública, um vermelho local claro antes do commit, com o arquivo e a contagem, em vez de um vermelho no servidor.

---

## 4. Nada ficou para o líder por falta de evidência

As três dúvidas fecharam com evidência de mercado que aponta uma saída só em cada caso. Nenhuma cai na exceção do item 6 do briefing. O que **é** dele, por natureza (L-10), e fica registrado como decisão autônoma para confirmação retroativa: a reabertura de layout do valor de `state` (decisão 1) e do token (decisão 2), ambas pré-1.0; e a régua de exceção do portão de travessão (decisão 3), que eu fixei pela L-21 do projeto e pode ser endurecida por ele com uma palavra.

## 5. Conformidade com as leis citadas no briefing

- **L-22 / L-43:** busca feita antes de decidir, com fonte e data; o que não achei está declarado (§1.3, "onde procurei e não achei"; §2.5 e §3.5, o que o SDL3 e o RmlUi não têm).
- **L-29:** RmlUi e SDL3 lidos pela web, sem clonar; citei arquivo e função e o que aprendi; nenhuma linha copiada, e o que decidimos **difere** do RmlUi nos três casos (vocabulário fechado, decodificador completo, portão no CI).
- **L-18 / L-27:** fato e inferência marcados; nenhum corte de escopo proposto.
- **L-39 / L-28:** nenhuma proposta encurta sintaxe; a decisão 2 aumenta o que o formato aceita corretamente.
- **L-41:** cada dúvida abre pelo efeito no consumidor; nomes de arquivo aparecem só nas seções de evidência e de forma da fatia, dirigidas ao planejador.
- **L-36 / L-40:** as três decisões nascem com portão que enumera o espaço fechado e prova vermelho antes de valer.
- **L-21 do projeto:** este documento é lido por nós, logo pt-br; e não carrega travessão, conferido por `grep` após a escrita.
