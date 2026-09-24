# Adendo ao plano da sub-fatia L-4 de `LAYERS-GATE-GFSS-GFUI`: construções que puxam cabeçalho fora de `#include`

**Autor:** C-level planejador (opus, esforço alto), 23/09/2026, modo autônomo (L-34 emendada: decido no lugar do líder depois de pesquisar; o orquestrador registra em `DECISOES_AUTONOMAS.md`).
**Base:** `docs/plano-layers-l4.md` (plano original) + implementação congelada `/var/tmp/glintfx-plan/check_layers-L4-congelado.py` (md5 `b2450996cfa6b9ea75981e71dae3e8d5`) + revisão `/var/tmp/glintfx-plan/revisao-layers-L4.md` (terceira reprovação do item). Árvore medida: `ab5db07`.
**Status deste arquivo:** PRONTO (23/09/2026). Quatro decisões autônomas (D-A1..D-A4, seção 4), nenhuma pendente com o líder além da D-2, que continua dele (seção 7).

Legenda (L-27), a mesma do plano original: **[FATO-CASA]** medido aqui, com o fixture/comando; **[FATO-FONTE]** de terceiro, com URL; **[INFERÊNCIA]** raciocínio meu, disputável; **[DECISÃO]** escolha deste adendo, com a razão.

Laboratório: `/var/tmp/glintfx-L4-adendo-lab/` (`git archive HEAD src include` + o congelado copiado para `tree/tests/tools/check_layers.py`, md5 conferido igual). Sondas em `/var/tmp/glintfx-L4-adendo-lab/probe/`. Nada rodou na árvore rastreada; nenhuma suíte, nenhum build. Compiladores: g++ 16.2.1, clang++ 22.1.8; Python 3.14.7.

Não-regressão de partida **[FATO-CASA]**: o congelado sobre a árvore de `ab5db07` imprime `violations: 0 in 133 files scanned, 3 skipped`, código 0.

---

## 0. Resumo em uma tela

O crítico do revisor (import partido em três linhas) é **um caso de uma família maior, e não o pior dela**. Medido **[FATO-CASA]** nesta sessão, no congelado **e** no portão da árvore de hoje (`HEAD:tests/tools/check_layers.py`, md5 `918b9eab...`: os três casos que conferi lá também passam, então a cegueira **não foi introduzida pela L-4, é herdada** e está no `main` agora):

1. **`import` fora da forma "uma linha, literal, `;` colado".** Onze formas passam limpas no portão; em **seis delas os DOIS compiladores concordam** que é importação de unidade de cabeçalho (o GCC tenta ler o módulo compilado de `/usr/include/c++/16/fstream`, o clang reclama que `<fstream>` não é unidade de cabeçalho conhecida). A mais barata: **`import <fstream> [[]];`, numa linha só**, porque a norma admite tokens entre o nome e o `;`.
2. **`import std;` e `module x;` passam por decisão** (`legacy_h_named_module_import_is_not_computed_include` espera "passa"). Mas **o módulo `std` exporta `<fstream>` e `<filesystem>` inteiros** (norma, [std.modules]), e `module glintfx_platform;` importa implicitamente a interface de outra camada. É uma porta de dependência que o portão declara aberta.
3. **Achado novo, fora do `import` e mais grave que ele:** o léxico da fase 3 do portão (`_translate_phases_2_and_3`, herdado de L-2, "sem mudança desta fatia") **dessincroniza do compilador em sete formas**, e em todas um `#include <fstream>` **comum, numa linha própria**, fica escondido dentro de um comentário que só o portão enxerga. Os dois compiladores puxam `<fstream>` na profundidade 1 (`-H`), **em quatro delas sem nenhum aviso nem com `-Wall -Wextra`**. A mais curta usa o separador de dígito do C++14 (`1'0`), que **a árvore real já usa** (`src/core/time.cpp:26`, `1'000'000'000.0`); a mais limpa não tem aspa nenhuma: `#if __has_include(<x/*y>)`.
4. **Pragmas:** `#pragma clang module import std` e `_Pragma("clang module import std")` fazem o clang procurar o módulo `std` (ele reconhece a forma) e passam no portão, que só procura `include_alias`. A lista de pragma é uma lista de proibidos, a mesma doença que a D-1 curou para nomes de cabeçalho.

**Desenho recomendado, em uma linha:** fechar o espaço pelo lado do **token**, não pelo lado da forma: nas camadas puras, **os identificadores `import`, `module`, `_Pragma` e `__pragma` são proibidos em qualquer posição de código** (fora de comentário e de literal), `#pragma` vira **lista de permitidos** (`once`, o único usado), e o léxico da fase 3 passa a modelar **número de pré-processamento** e **nome de cabeçalho** como a norma, com recusa fechada de literal não terminado na linha e de nome de cabeçalho com caractere de suporte condicional. Tudo isso sem chamar compilador.

---

## 1. O que foi medido nesta sessão ([FATO-CASA])

Cada sonda é um arquivo em `/var/tmp/glintfx-L4-adendo-lab/probe/`. "Portão" = `scan_file_directives()` do congelado sobre a sonda, com a raiz da árvore real (`run_gate.py` no laboratório); três delas também pela linha de comando do portão numa fixture limpa (`cli12/`), com o mesmo resultado (`violations: 0`, código 0). Compiladores: `clang++ -std=c++23 -fsyntax-only` e `g++ -std=c++23 -fmodules -fsyntax-only`; para as de `#include`, `-H` e a profundidade 1.

### 1.1 Família I: `import` de unidade de cabeçalho

| Sonda | Forma | clang++ 22.1.8 | g++ 16.2.1 `-fmodules` | Portão |
|---|---|---|---|---|
| P01 | `import` / `<fstream>` / `;` em três linhas (a do revisor) | importa `<fstream>` | **não** reconhece (`'fstream' was not declared`) | PASSA |
| P33 | `import` / `<fstream>;` em duas linhas | importa | não reconhece | PASSA |
| P18 | `import <fstream>` / `;` na linha seguinte | importa | **importa** (lê o módulo compilado de `c++/16/fstream`) | PASSA |
| **P02** | **`import <fstream> [[]];`** (uma linha) | importa | **importa** | PASSA |
| P03 | `export import <fstream> [[deprecated]];` | importa | importa | PASSA |
| P22 | `import "fstream" [[]];` | importa | importa | PASSA |
| **P04** | `#define HDR <fstream>` / `import HDR;` | importa | **importa** | PASSA |
| P05 | `#define IMP import` / `IMP <fstream>;` | importa | não reconhece | PASSA |
| P06 | `CAT(im,port) <fstream>;` (colagem `##`) | importa | não reconhece | PASSA |
| P23 | `#define X import <fstream>;` / `X` | importa (e reclama do `;` vindo de macro) | não reconhece | PASSA |
| P07 | `int x; import <fstream>;` (não no início da linha) | importa | não reconhece | PASSA |
| P29 | `import <fstream>;` | erro léxico (UCN de caractere básico) | erro léxico | PASSA |
| P13 | `@import std;` | erro de sintaxe | erro | PASSA |

**Leitura [INFERÊNCIA, apoiada em 2.1]:** o GCC segue a norma (a diretiva `import` é reconhecida no pré-processador, precisa começar a linha e ter o nome **na mesma linha lógica**), e o clang 22 reconhece `import` no analisador sintático, como declaração, em qualquer lugar e até vinda de macro. **Onde os dois concordam (P02, P03, P04, P18, P22), a forma é da norma e o portão está simplesmente errado.** Onde discordam (P01, P05, P06, P07, P23, P33), o clang é o permissivo, e o clang é compilador de alvo (Arch, CachyOS e o `clang-tidy` do job de lint usam clang; a matriz é dos cinco alvos, L-04). Nas duas situações o portão tem de reprovar.

### 1.2 Família II: módulos nomeados

| Sonda | Forma | clang++ | g++ | Portão |
|---|---|---|---|---|
| P08 | `import std;` | procura o módulo `std` | procura o módulo `std` | PASSA (é o `legacy_h`, esperado "passa") |
| P36 | `export import std;` | idem | idem | PASSA |
| P35 | `import` / `std;` em duas linhas | procura o módulo `std` | não reconhece | PASSA |
| P34 | `module glintfx_platform;` | procura o módulo `glintfx_platform` | procura o módulo | PASSA |

### 1.3 Família III: pragma que carrega módulo

| Sonda | Forma | clang++ | g++ | Portão |
|---|---|---|---|---|
| P10 | `#pragma clang module import std` | procura o módulo `std` | ignora (pragma desconhecido) | PASSA |
| P11 | `_Pragma("clang module import std")` | procura o módulo `std` | ignora | PASSA |

### 1.4 Família IV (achado novo): o léxico da fase 3 esconde `#include` comum

Todas as sondas terminam com um `#include <fstream>` numa linha **própria**, sem truque nenhum nela. O truque está antes, e faz o portão abrir um comentário de bloco que o compilador não abre.

| Sonda | O que dessincroniza | g++ `-H` prof. 1 | clang++ `-H` prof. 1 | Aviso com `-Wall -Wextra` | Portão |
|---|---|---|---|---|---|
| **P12** | `auto a = 1'0; auto s = "'/*";` (separador de dígito) | `fstream` | `fstream` | nenhum | PASSA |
| Q04 | `0x1'f` (separador seguido de letra, também número) | `fstream` | `fstream` | nenhum | PASSA |
| **Q01** | `#if __has_include(<x/*y>)` (`/*` dentro do nome de cabeçalho) | `fstream` | `fstream` | nenhum | PASSA |
| Q02 | `#if __has_include(<x'y>)` | `fstream` | `fstream` | nenhum | PASSA |
| Q05 | `#if __has_include(<x"y>)` | `fstream` | `fstream` | nenhum | PASSA |
| Q03 | `#if 0` / `don't` / `#endif` (apóstrofo solto em grupo pulado) | `fstream` | `fstream` | GCC: `missing terminating '` (aviso); clang: nenhum | PASSA |
| Q06 | `#if 0` / `#error it's` / `#endif` | `fstream` | `fstream` | GCC: aviso; clang: nenhum | PASSA |

**A raiz, lida no código congelado:** (a) o estado de literal de caractere abre em **todo** `'` fora de literal, e o compilador não abre literal no `'` de um número (`pp-number ' digit` e `pp-number ' nondigit`, [lex.ppnumber]); (b) nome de cabeçalho `<...>` é um token só para o compilador (dentro dele `/*`, `'` e `"` não significam nada), e o portão o lê como texto comum; (c) o literal de caractere ou de cadeia do portão **atravessa a quebra de linha** até achar o fecho, e o do compilador nunca atravessa (fora da cadeia bruta, que o portão já trata). Com (a), (b) ou (c), o fecho do literal falso cai dentro de uma cadeia verdadeira (`"'/*"`), e o `/*` que o compilador lê como texto da cadeia o portão lê como início de comentário.

**Não-regressão exige atenção:** a árvore real **usa** separador de dígito (`src/core/time.cpp:26`) e colagem `##` (`src/gfss/diagnostic_vocabulary.hpp:350,370,391`, `src/gfss/color_diagnostic_vocabulary.hpp:111,131`). Nenhuma regra nova pode reprovar essas duas formas por si.

### 1.5 Medição da árvore real (seis camadas, 133 arquivos-fonte, depois da fase 3 do próprio portão)

| Token (identificador inteiro, fora de comentário) | Ocorrências | Onde |
|---|---|---|
| `import` | **0** | (nenhuma) |
| `module` | **0** | (nenhuma) |
| `_Pragma`, `__pragma` | **0** | (nenhuma) |
| `#pragma` | 82 | **todos `once`** |
| `export` | 20 | todas dentro de `#include <glintfx/export.hpp>` (nome de cabeçalho), nenhuma como palavra-chave |
| separador de dígito | 3 | `src/core/time.cpp:26` |
| `##` | 5 | os dois vocabulários de diagnóstico de `src/gfss/` |
| `__has_include`, `#if 0` | 0 | (o `#if 0` já medido no plano original) |

Script da medição: `/var/tmp/glintfx-L4-adendo-lab/measure_tokens.py`.

---

## 2. Pesquisa (L-42), com fontes e licenças

Feita **antes** de decidir, e separada do que foi medido em casa (seção 1). Nenhum código de terceiro foi lido: só a norma, artigos do comitê, notas de versão e documentação.

### 2.1 O que a norma diz

- **[FATO-FONTE] Quem abre uma diretiva** ([cpp.pre], <https://eel.is/c++draft/cpp.pre>): o primeiro token é *"a # preprocessing token, or an import preprocessing token immediately followed on the same logical source line by a header-name, <, identifier, or : preprocessing token, or a module preprocessing token immediately followed on the same logical source line by an identifier, :, or ; preprocessing token, or an export preprocessing token immediately followed on the same logical source line by one of the two preceding forms"*, e *"the last preprocessing token in the sequence is the first preprocessing token within the sequence that is immediately followed by whitespace containing a new-line character"*. **Consequência:** pela norma, `import` sozinho numa linha (P01, P33, P35) **não** abre diretiva: é o que o GCC faz. O clang 22 aceita mesmo assim (1.1). Essa regra vem do P1703R1 (Kolpackov, "Recognizing Header Unit Imports Requires Full Preprocessing", <https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1703r1.html>) e do P1857R3 ("Modules Dependency Discovery", <https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1857r3.html>), escritos justamente para que uma ferramenta de dependência reconheça `import` **sem pré-processar tudo**.
- **[FATO-FONTE] As três formas de `import`** ([cpp.import], <https://eel.is/c++draft/cpp.import>): `export_opt import header-name pp-tokens_opt ; new-line`, `export_opt import header-name-tokens pp-tokens_opt ; new-line`, `export_opt import pp-tokens ; new-line`; e *"preprocessing tokens after the import preprocessing token in the import control-line are processed just as in normal text (i.e., each identifier currently defined as a macro name is replaced by its replacement list)"*. **Consequência:** o `pp-tokens_opt` entre o nome e o `;` é o que torna **P02/P03/P22 (atributo)** válidos, e a expansão de macro é o que torna **P04 (`import HDR;`)** válido. O `_IMPORT_DIRECTIVE_PATTERN` do congelado exige `;` colado ao nome e literal direto: cego às duas coisas por construção.
- **[FATO-FONTE] Onde nasce um nome de cabeçalho** ([lex.pptoken], <https://eel.is/c++draft/lex.pptoken>): só depois de `include`/`embed` numa diretiva, depois de `import` no início da linha lógica, ou dentro de `__has_include`/`__has_embed` seguido de `(`. E, no rascunho atual, *"if a U+0027 apostrophe, a U+0022 quotation mark ... matches the last category, the program is ill-formed"* (apóstrofo solto). **Consequência:** fora desses contextos `<` é operador e o léxico é o comum; dentro deles `<...>` é um token só.
- **[FATO-FONTE] O que um nome de cabeçalho pode conter** ([lex.header], <https://eel.is/c++draft/lex.header>): *"The appearance of either of the characters ' or \ or of either of the character sequences /* or // in a q-char-sequence or an h-char-sequence is conditionally-supported with implementation-defined semantics, as is the appearance of the character " in an h-char-sequence."* **Consequência:** as formas Q01/Q02/Q05/Q08 são de suporte condicional, e g++/clang as aceitam em silêncio dentro de `__has_include` (1.4).
- **[FATO-FONTE] Número de pré-processamento** ([lex.ppnumber], <https://eel.is/c++draft/lex.ppnumber>): inclui `pp-number ' digit` e `pp-number ' nondigit`. O `'` de `1'000` e de `0x1'f` **não** abre literal de caractere. É a raiz de P12/Q04/Q07.
- **[FATO-FONTE] O módulo `std`** ([std.modules], <https://eel.is/c++draft/std.modules>) exporta *"declarations in namespace std that are provided by the importable C++ library headers"*, e os importáveis são *"the headers listed in Table 24"* ([headers], <https://eel.is/c++draft/headers>), a mesma tabela de onde o congelado transcreveu `fstream` e `filesystem`. **`import std;` puxa exatamente os dois cabeçalhos que a ASSET-LOAD proíbe.** `std.compat` exporta o mesmo e mais o espaço global.
- **[FATO-FONTE] Importação implícita** ([module.unit], <https://eel.is/c++draft/module.unit>): *"A module-declaration that contains neither an export-keyword nor a module-partition implicitly imports the primary module interface unit of the module as if by a module-import-declaration."* **Consequência:** `module glintfx_platform;` num arquivo de `src/core` puxa a interface de outra camada **sem nenhum `import` escrito** (P34).

### 2.2 O que os compiladores fazem de fato

- **[FATO-CASA]** GCC 16.2.1 segue [cpp.pre]: reconhece `import` no pré-processador, só no início da linha e com o nome na mesma linha lógica (1.1: recusa P01/P05/P06/P07/P23/P33).
- **[FATO-CASA]** clang 22.1.8 reconhece `import` no analisador: em qualquer posição, através de linhas, vindo de macro e de colagem.
- **[FATO-FONTE]** O clang **mudou isso no 23.1**: as notas de versão dizem *"Clang now supports P1857R3 Modules Dependency Discovery"* (<https://releases.llvm.org/23.1.0/tools/clang/docs/ReleaseNotes.html>), depois de uma história de aplicar e reverter (issue <https://github.com/llvm/llvm-project/issues/56917>; PRs #107168, #173130 e a reversão #173549 nos arquivos do cfe-commits). **[INFERÊNCIA]** A discordância GCC × clang de 1.1 é de **versão**, não de desenho: o clang 22 é o permissivo, o 23 deve se comportar como o GCC. Os jobs Arch e CachyOS usam `:latest` e o `clang-tidy` do lint é clang: qual versão estará lá num dado dia é fato do ambiente que o portão **não pode** congelar (memória "portão que congela um fato do ambiente"). O portão tem de reprovar as formas das duas versões.
- **[FATO-FONTE]** O compilador da Microsoft só importa unidade de cabeçalho com `/headerUnit` ou `/translateInclude` (<https://learn.microsoft.com/cpp/build/reference/translateinclude>, <https://learn.microsoft.com/cpp/build/walkthrough-header-units>). **Não medido aqui** se o analisador dele aceita as formas partidas; e o projeto **não** passa `/Zc:preprocessor` (`grep` vazio em `cmake/` e `CMakeLists.txt`), então o pré-processador tradicional da Microsoft está ativo, com desvios da norma que **não modelei** (seção 8, R-A5).
- **[FATO-FONTE]** `#pragma clang module import <nome>` é extensão documentada do clang (<https://clang.llvm.org/docs/Modules.html>, e as versões arquivadas, p.ex. <https://releases.llvm.org/19.1.0/tools/clang/docs/Modules.html>). **[FATO-CASA]** P10/P11: o clang 22 procura o módulo nomeado.

### 2.3 Alguma ferramenta já trata isso?

- **[FATO-FONTE]** O CMake: *"Header units are not supported"*, e `import std` só com a porta experimental `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` (<https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html>). **[INFERÊNCIA]** Hoje nenhum build deste projeto compila `import <x>;` com sucesso; o portão não pode depender disso, porque o consumidor externo (e a distribuição que empacota a lib) escolhe o compilador e as opções, não nós.
- **[FATO-FONTE]** Toda ferramenta de dependência que reconhece `import` **pergunta ao compilador**: o formato P1689 é produzido pelo próprio compilador (`clang-scan-deps`, `/scanDependencies` da Microsoft, <https://learn.microsoft.com/cpp/build/reference/scandependencies>). O P1703 existe porque reconhecer `import` exigia pré-processamento completo. O verificador do Chromium (lido no plano original, §1.4) só vê `#include`/`#import` por expressão regular. **Ninguém resolve `import` por texto.**
- **Lição [INFERÊNCIA]:** um portão só de texto não tem como **entender** `import` nas duas versões do clang, no GCC e na Microsoft ao mesmo tempo. Tem como **recusar a palavra**. É a mesma troca que a D-1 fez com nome de cabeçalho: parar de entender a forma proibida e passar a exigir a forma permitida.

### 2.4 Licenças (L-29)

| Fonte | Natureza | O que li |
|---|---|---|
| Rascunho da norma (eel.is), P1703R1, P1857R3 | norma e artigos do comitê | texto normativo citado |
| Notas de versão do clang 23.1, issue #56917 do LLVM, títulos dos PRs no cfe-commits | documentação e rastreador | só o que foi citado; **nenhum código** |
| Documentação do clang (Modules), do CMake, da Microsoft | documentação | contratos citados |

---

## 3. Enumeração fechada: toda construção que traz conteúdo de fora para a unidade de tradução (L-40)

**Como a lista foi fechada [INFERÊNCIA, com a base citada]:** pela norma, conteúdo externo entra numa unidade de tradução por **cinco** portas e nenhuma outra: inclusão de arquivo-fonte ([cpp.include]), importação de unidade de cabeçalho e de módulo nomeado ([cpp.import], [module.import]), importação implícita pela declaração de módulo ([module.unit]) e `#embed` ([cpp.embed]). Os compiladores acrescentam extensões, e toda extensão cai numa de quatro **superfícies de sintaxe**: (i) um nome de diretiva depois de `#`; (ii) o conteúdo de um `#pragma`; (iii) um operador de pragma (`_Pragma`, `__pragma`); (iv) uma palavra-chave ou identificador no código (`import`, `module`, `asm`). Se cada uma das quatro superfícies for fechada por **lista de permitidos** (e não por lista de proibidos), extensão que ainda não existe cai no lado da recusa. A superfície (v), o **léxico**, decide o que é código, comentário e literal antes de todas as outras, e por isso entra também.

| # | Construção | Superfície | Puxa conteúdo? (fonte) | Portão congelado | Decisão do adendo |
|---|---|---|---|---|---|
| 1 | `#include`, `%:include` | (i) | sim, [cpp.include] | política D-1 | **mantida** |
| 2 | `#include_next` | (i) | sim, GCC/clang | política D-1 | **mantida** |
| 3 | `#import` | (i) | sim (GCC/clang; COM no MSVC) | reprova diretiva | **mantida** |
| 4 | `#using` (MSVC) | (i) | sim (metadados .NET) | reprova diretiva | **mantida** |
| 5 | `#embed` | (i) | sim (bytes), [cpp.embed]; g++ 16 aceita calado em C++ (P41) | reprova diretiva | **mantida** |
| 6 | `#__include_macros` (interno do `-imacros`) | (i) | sim | reprova diretiva (P42, medido) | **mantida** |
| 7 | `import <h>;`/`import "h";` em qualquer disposição, com atributo, por macro, por colagem, fora do início da linha | (iv) | sim, [cpp.import]; seis formas medidas com os dois compiladores concordando | **PASSA** 11 formas | **REPROVA pelo token `import`** (D-A1) |
| 8 | `export import ...` | (iv) | idem | PASSA nas formas partidas e com atributo | idem |
| 9 | `import std;`, `import std.compat;`, `import M;`, `import :parte;` | (iv) | sim: `std` exporta Table 24 inteira ([std.modules]) | **PASSA por decisão** (`legacy_h`) | **REPROVA**, `legacy_h` invertido (D-A1) |
| 10 | `module M;`, `module M:P;`, `export module M;`, `module;`, `module :private;` | (iv) | `module M;` importa a interface ([module.unit]) | PASSA | **REPROVA pelo token `module`** (D-A1) |
| 11 | `#pragma include_alias` (MSVC) | (ii) | redireciona o próximo `#include` | reprova por busca textual | **REPROVA pela lista de permitidos de pragma** (D-A3) |
| 12 | `#pragma clang module import/load/build/begin` | (ii) | sim (P10, clang 22) | **PASSA** | idem |
| 13 | `#pragma comment(lib, ...)` (MSVC) | (ii) | liga biblioteca (do SO, tipicamente); não é cabeçalho, é a mesma violação da L-19 | **PASSA** | idem |
| 14 | `#pragma GCC dependency "f"` | (ii) | lê a data do arquivo, não o conteúdo | PASSA | idem (fechado por padrão) |
| 15 | `#pragma push_macro`/`pop_macro` | (ii) | não puxa; mexe na tabela de macro | PASSA | idem |
| 16 | `_Pragma("...")`, `__pragma(...)` | (iii) | tudo o que (11)-(15) fazem, de dentro de macro (P11) | só se contiver `include_alias` | **REPROVA pelo token** (D-A1) |
| 17 | colagem `##`/`%:%:` que **forma** `import`, `module`, `_Pragma`, `__pragma`, `asm` | (iv) | sim no clang 22 (P06, P37) | **PASSA** | **REPROVA pela regra de colagem** (D-A2) |
| 18 | `asm(".incbin \"f\"")`, `asm(".include ...")`, `__asm__`, `__asm`, `_asm` | (iv) | **sim**: P39, g++ embute o arquivo na seção `.text` (medido com `objdump`) | **PASSA** | **REPROVA pelo token** (D-A1) |
| 19 | `__has_include`, `__has_include_next`, `__has_embed` | (v) | **não puxa**; só pergunta | permitido | **permitido**, mas o nome de cabeçalho dentro dele passa a ser lido como token (D-A4) |
| 20 | `#if #ifdef #ifndef #elif #elifdef #elifndef #else #endif` | (i) | não puxam | permitidos (semântica de união) | **mantido** |
| 21 | `#define #undef #line #error #warning` | (i) | não puxam | permitidos | **mantido** |
| 22 | `@import` (Objective-C) | (iv) | não é C++ (P13: os dois recusam) | PASSA | reprova pelo token `import` (efeito colateral, inofensivo) |
| 23 | `import` soletrado com nome de caractere universal (`import`) | (v) | **não**: C++23 proíbe UCN de caractere básico em identificador (P29: os dois recusam) | PASSA | **não é vetor**; declarado, sem controle |
| 24 | tradução de `#include` em `import` (`/translateInclude`, `-fmodules` implícito do clang) | opção de build | o `#include` escrito é o que conta | julgado como `#include` | **fora do portão de fonte**, declarado |
| 25 | inclusão forçada pela linha de comando (`-include`, `/FI`) e ligação por `target_link_libraries` num `CMakeLists.txt` das camadas puras | build | sim | `CMakeLists.txt` é **pulado** | **fora deste item**; vai ao INBOX (`PORTAO-DE-CAMADA-CMAKE`, seção 7) |
| 26 | dessincronia léxica (P12, Q01-Q08) | (v) | esconde um `#include` inteiro | **PASSA** | **REPROVA pelas regras léxicas** (D-A4) |

**Residual declarado [INFERÊNCIA]:** função embutida do compilador que leia arquivo por nome. Não achei nenhuma documentada utilizável pelo programador (o `__builtin_pp_embed` do clang não é aceito como código comum: P40, erro), e a árvore não tem nenhum `__builtin` (**[FATO-CASA]**, `grep` vazio). Não entra regra nova; o oráculo D-2 é quem fecharia isso de verdade (seção 7).

---

## 4. Decisões (modo autônomo, L-34)

### D-A1. Identificadores proibidos em código das camadas puras

**[DECISÃO]** Nas seis pastas, qualquer **token identificador** igual a `import`, `module`, `_Pragma`, `__pragma`, `asm`, `__asm`, `__asm__` ou `_asm`, em **qualquer** linha (inclusive dentro de `#define`, `#if 0` e diretivas), **fora** de comentário, de literal de caractere/cadeia (bruta incluída) e de nome de cabeçalho, reprova, nomeado ("identificador `import` proibido em camada pura: a importação de módulo e de unidade de cabeçalho não é verificável por texto"). Tipo de violação novo: `token`.

**É a inferência do orquestrador, e a evidência a sustenta:**
- **[FATO-CASA]** zero ocorrências dos oito na árvore (1.5): a regra não reprova nada que exista.
- **[FATO-FONTE]** entender `import` exige o compilador (2.3), e o comportamento muda entre clang 22 e 23 (2.2). Recusar a palavra fecha as onze formas de 1.1, as quatro de 1.2 e as que ainda não foram achadas, em todas as versões.
- A comparação é por **token inteiro**, não por subcadeia: `important`, `modules`, `import_asset`, `asm_x` passam (controles F-neg).

**Descartado:**
- *Juntar linhas até o `;` e casar a forma* (sugestão do revisor): fecha P01, não fecha P02 (atributo), P04 (macro), P06 (colagem) nem P07; e seria o quarto conserto de forma numa família que já teve dez. Busca dirigida (L-40).
- *Modelar [cpp.pre] exatamente como a norma:* reprovaria só o que o GCC aceita, e o clang 22 aceita mais (1.1). O portão tem de reprovar a união dos compiladores, não a norma.
- *Permitir `import` de módulo nomeado e só barrar unidade de cabeçalho:* `import std;` puxa `<fstream>` (2.1). A proibição de `fstream`/`filesystem` é por cabeçalho e não sobrevive a um módulo que exporta os dois; não há como expressá-la por texto sobre `import std;`.

**O que custa ao consumidor externo [INFERÊNCIA, com a fronteira medida]:**
- **Nada para quem usa a biblioteca.** O portão só lê as seis pastas **do próprio GlintFx**, nunca o código de quem consome. O consumidor continua livre para `import` em casa, inclusive `import <glintfx/core/vec2.hpp>;` como unidade de cabeçalho, se o compilador e o build dele permitirem.
- **O custo é do GlintFx:** enquanto a regra valer, **o GlintFx não pode publicar um módulo nomeado próprio** (`export module glintfx.core;`) escrito dentro das camadas puras, nem usar `import std;` nelas. Publicar `import glintfx;` é uma **decisão de produto** que hoje ninguém pediu; quando vier, a regra não some: vira lista de permitidos de **nomes de módulo** (`glintfx.core`, `glintfx.gfss`, `glintfx.gfui`), exatamente como a D-1 fez com nome de cabeçalho, e `std` continua fora enquanto a proibição de `fstream`/`filesystem` existir. Vai ao INBOX como `GLINTFX-MODULO-NOMEADO` (seção 7).
- Custo cosmético: `module` e `import` não podem ser nome de variável nas camadas puras. Zero hoje.
- As extensões de módulo (`.cppm .ixx .mpp .ccm .cxxm`) **continuam varridas**: um arquivo de módulo plantado numa camada pura reprova pelo conteúdo, que é o comportamento desejado.

**Consequências no congelado (L-67):** `_IMPORT_DIRECTIVE_PATTERN` e o ramo `import` de `_scan_one_logical_line` **saem** (a regra do token cobre tudo o que eles cobriam); `anchor_3_import_angle` e `anchor_4_export_import_angle` passam a esperar `reproves_token`; **`legacy_h_named_module_import_is_not_computed_include` é invertido** para `reproves_token` e renomeado (`legacy_h_named_module_import_reproves_by_token`), com a citação de [std.modules] no comentário. Essa inversão desfaz uma decisão de L-3; a razão é a medição de 2.1 e 1.2, não preferência.

### D-A2. Colagem de token que pode formar identificador proibido

**[FATO-CASA]** a árvore usa `##` em cinco lugares, todos na forma `k_expected_##name` / `k_color_expected_##name` (1.5). Proibir `##` quebraria a árvore; ignorar deixa P06/P37 passarem.

**[DECISÃO]** Para cada **cadeia** de colagem `o1 ## o2 ## ... ## ok` (também na grafia `%:%:`) numa linha lógica de `#define`, com o conjunto de parâmetros da macro lido da própria linha (mais `__VA_ARGS__` e `__VA_OPT__`), a cadeia é **segura** se valer ao menos uma das três:
1. `o1` é identificador fixo (não é parâmetro) e **nenhum** identificador proibido começa com ele; ou
2. `ok` é identificador fixo e nenhum identificador proibido termina com ele; ou
3. algum `oi` é identificador fixo que **não é subcadeia** de nenhum identificador proibido; ou `o1` é número (o resultado começa por dígito e nunca é identificador).

Senão, reprova, nomeada ("colagem que pode formar identificador proibido"). A cadeia inteira é a unidade, não cada `##`: `a##por##t` com `a` parâmetro forma `import` se `a` valer `im`, e uma regra por par deixaria passar (controle G12). `k_expected_##name` é segura pela regra 1.

### D-A3. `#pragma` vira lista de permitidos

**[DECISÃO]** Diretiva `pragma` só passa se os tokens depois de `pragma` forem **exatamente** `once` (comentário já virou espaço na fase 3). Qualquer outro conteúdo reprova, nomeado ("pragma não permitido em camada pura"). **[FATO-CASA]** os 82 pragmas da árvore são todos `once`. Com isso `_INCLUDE_ALIAS_PATTERN` e a busca textual **saem** (L-67): `#pragma include_alias` cai na lista de permitidos, `_Pragma(...)`/`__pragma(...)` caem na D-A1. A super-aproximação "a palavra `include_alias` dentro de uma cadeia reprova" desaparece junto, porque deixa de ser necessária. Ampliar a lista (p.ex. `GCC diagnostic`) é uma linha e um controle, quando alguém precisar.

**Descartado:** acrescentar `clang module` e `comment` à lista de proibidos de pragma. É a lista de proibidos que a D-1 aposentou para nomes de cabeçalho; pragma é espaço aberto por definição (a norma deixa o conteúdo à implementação).

### D-A4. O léxico da fase 3 passa a ler como o compilador em três pontos, e recusa o resto

**[DECISÃO]** `_translate_phases_2_and_3` ganha três regras, cada uma num auxiliar próprio (L-17: a função herdada já tem ~160 linhas e **não pode crescer**; ela é dividida em tratadores por estado no mesmo commit, mantendo o contrato de saída `(clean_text, clean_lines)` e acrescentando a lista de violações léxicas e as faixas de literal e de nome de cabeçalho, para a D-A1 excluir):

1. **Número de pré-processamento** ([lex.ppnumber]): um token que começa por dígito, ou por `.` seguido de dígito, **no início de token** (o caractere anterior não é de identificador), consome `[A-Za-z0-9_.]`, `e+ e- E+ E- p+ p- P+ P-` e **`'` seguido de caractere de identificador**. O `'` de dentro dele nunca abre literal. Mata P12, Q04, Q07, sem tocar `src/core/time.cpp:26`.
2. **Nome de cabeçalho nos contextos da norma** ([lex.pptoken]): depois de `#`/`%:` + `include`, `include_next`, `import` ou `embed`; depois de `import` (ou `export import`) no início da linha; e depois de `__has_include`, `__has_include_next` ou `__has_embed` seguido de `(`. Se o próximo caractere útil for `<` ou `"` e o fecho (`>` ou `"`) estiver **na mesma linha lógica**, o trecho é **um token só**, sem reconhecer comentário nem literal dentro. Se o conteúdo tiver `'`, `"`, `\`, `/*` ou `//` (os caracteres de suporte condicional de [lex.header]), **reprova**, nomeado ("nome de cabeçalho com caractere de suporte condicional"). Sem fecho na linha, o léxico comum segue (é o que o compilador faz). Mata Q01, Q02, Q05, Q08. **[FATO-CASA]** zero argumentos de inclusão com esses caracteres na árvore.
3. **Literal que não fecha na linha:** literal de caractere ou de cadeia (não bruta) que encontra uma quebra de linha **real** (não emenda) antes do fecho reprova, nomeado ("literal não terminado"). O compilador nunca deixa um literal comum atravessar linha; o portão deixava, e é isso que transforma qualquer apóstrofo solto num esconderijo. Mata Q03 e Q06 (o GCC só avisa; o clang aceita calado em grupo pulado) e qualquer apóstrofo solto que ainda não pensamos. **[FATO-CASA]** o protótipo (4.1) não reprova nenhum dos 133 arquivos da árvore.

**Não entra:** reprovar literal de caractere com mais de um caractere (`'ab'`). Com as três regras, o `'` só dessincroniza em número e em nome de cabeçalho, ambos modelados; dentro de uma linha, `'...'` é literal para os dois lados, inclusive em grupo pulado. Regra a mais sem mutante que a justifique.

### 4.1 Protótipo de laboratório (prova de que as regras são satisfazíveis, não é código de produção)

`/var/tmp/glintfx-L4-adendo-lab/proto/proto_lex.py` (176 linhas, escrito por mim neste laboratório; **não** é para copiar: o implementador escreve o dele sobre o congelado, dividido por L-17) implementa D-A1, D-A2, D-A3 e D-A4 sobre um tokenizador. **[FATO-CASA]** `python3 proto/run_proto.py tree`: **133 arquivos, 0 reprovados**. Sobre as sondas: reprova P01-P08, P10, P11, P13, P18, P22, P23, P33-P38, P43 e Q01-Q03, Q05, Q06, Q08; P12, Q04 e Q07 passam no protótipo **porque o protótipo não aplica a política de nome**, e ele **enxerga** a diretiva escondida (`#include <fstream>` listado na linha certa nas quatro), que a política D-1 já existente reprova. P29 passa (não é vetor, linha 23 da tabela).

---

**Nota sobre o protótipo e a D-A2:** o protótipo aplica a regra de colagem **por par** de `##`; a decisão é **por cadeia** (D-A2). A diferença só aparece em cadeia de três ou mais operandos (G12 abaixo), e é justamente o mutante M-G12.

---

## 5. Controles novos, cada um com o mutante que mata (L-36, L-12)

Mesmo executor de tabela do plano original, com duas extensões **[DECISÃO]**:
- **dois tipos de veredito novos:** `reproves_token` (D-A1, D-A2) e `reproves_lexical` (D-A4); pragma fora da lista reprova como `reproves_directive` **com a mensagem "pragma não permitido"** conferida (o motivo certo, não só "reprovou");
- **expectativa múltipla** (`also_expect`): um caso pode exigir mais de uma violação, cada uma com arquivo, linha e motivo. É o que mata M-C24/M-C25 abaixo, onde a presença da ligação já reprova a árvore e esconderia a falha da linha do `#include`.

Toda sonda "✓" deste adendo já foi rodada nesta sessão contra g++ e clang++ (seção 1); o implementador **reconfirma** com o mesmo comando e põe a linha de confirmação no comentário do caso. Todo mutante é aplicado numa cópia fora da árvore (L-27) e o `--selftest` tem de reprovar **nomeando o caso**.

### Família F: tokens proibidos (D-A1)

| Caso | Conteúdo | Esperado | Prova | Mata |
|---|---|---|---|---|
| F1 | `import` / `<fstream>` / `;` (P01, o crítico do revisor) | token | ✓clang | **M-F1** "procura `import` só por expressão de linha única" (o congelado) |
| F2 | `import <fstream> [[]];` (P02) | token | ✓gcc+clang | M-F1 |
| F3 | `export import <fstream> [[deprecated]];` (P03) | token | ✓gcc+clang | M-F1 |
| F4 | `import "fstream" [[]];` (P22) | token | ✓gcc+clang | M-F1 |
| F5 | `#define HDR <fstream>` / `import HDR;` (P04) | token | ✓gcc+clang | M-F1 |
| F6 | `import <fstream>` / `;` (P18) | token | ✓gcc+clang | M-F1 |
| F7 | `#define IMP import` / `IMP <fstream>;` (P05) | token (na linha do `#define`) | ✓clang | **M-F7** "pula linhas de diretiva ao procurar o token" |
| F8 | `int x; import <fstream>;` (P07) | token | ✓clang | **M-F8** "só no início da linha" |
| F9 | `#if 0` / `import <fstream>;` / `#endif` (P38) | token | união (plano original §2.1) | **M-F9** "respeita `#if 0`" |
| F10 | `import std;` (P08; o `legacy_h` invertido) | token | ✓gcc+clang procuram `std` | **M-F10** "só unidade de cabeçalho" |
| F11 | `import` / `std;` (P35) | token | ✓clang | M-F1 |
| F12 | `module glintfx_platform;` (P34) | token `module` | ✓gcc+clang | **M-F12** "`module` fora da lista" |
| F13 | `module;` / `#include <cstdint>` / `export module m;` (P43) | token `module` | forma da norma | M-F12 |
| F14 | `_Pragma("clang module import std")` (P11) | token `_Pragma` | ✓clang | **M-F14** "`_Pragma` fora da lista" |
| F15 | `__pragma(once)` | token `__pragma` | docs MS | M-F14, grafia MSVC |
| F16 | `asm(".incbin \"x\"");` (P39) | token `asm` | ✓gcc embute (objdump) | **M-F16** "`asm` fora da lista" |
| F17 | `__asm__("")`, `__asm {}`, `_asm {}` | token, um caso cada | grafias GCC/MSVC | M-F16 por grafia |
| F18 | um caso por identificador da lista (8) | token | (nenhuma) | **M-F18** "nome esquecido"; trava: nº de casos = tamanho da lista |
| F-neg1 | `int important = 0; int modules = 0; int import_asset = 0; int asm_x = 0;` | **passa** | (nenhuma) | **M-Fn1** "casa por subcadeia" (rigor demais) |
| F-neg2 | `const char* s = "import <fstream>;"; char c = 'm';` | **passa** | (nenhuma) | **M-Fn2** "não exclui literal" |
| F-neg3 | `R"(module x; import std;)"` em cadeia bruta de várias linhas | **passa** | (nenhuma) | **M-Fn3** "não exclui cadeia bruta" |
| F-neg4 | `// import <fstream>;` e `/* module x; */` | **passa** | (nenhuma) | **M-Fn4** "não exclui comentário" |
| F-neg5 | `#include <glintfx/core/module_map.hpp>` com o arquivo existente na fixture | **passa** | (nenhuma) | **M-Fn5** "não exclui nome de cabeçalho" (o token `module` dentro do nome) |

### Família G: colagem (D-A2)

| Caso | Conteúdo | Esperado | Prova | Mata |
|---|---|---|---|---|
| G1 | `#define CAT(a,b) a##b` / `CAT(im,port) <fstream>;` (P06) | token (colagem) | ✓clang | **M-G1** "sem regra de colagem" |
| G2 | `CAT(_Pra,gma)("clang module import std")` (P37) | token (colagem) | ✓clang | M-G1 |
| G3 | `#define X(n) k_expected_##n` (forma da árvore) | **passa** | árvore real | **M-G3** "toda colagem reprova" |
| G4 | `#define X(n) n##_suffix` | **passa** (regra 2) | (nenhuma) | **M-G4** "ignora o operando direito" |
| G5 | `#define X(n) im##n` | token | `im` é prefixo de `import` | **M-G5** "operando fixo sempre seguro" |
| G6 | `#define X(n) n##port` | token | `port` é sufixo | M-G5, lado direito |
| G7 | `#define X(a,b) a##b` (dois parâmetros) | token | (nenhuma) | **M-G7** "só olha colagem usada" |
| G8 | `#define X(a,b) a %:%: b` | token | dígrafo | **M-G8** "só a grafia `##`" |
| G9 | `#define X(n) 1##n` | **passa** (resultado começa por dígito) | (nenhuma) | **M-G9** "número como operando reprova" |
| G10 | `#define X(...) __VA_ARGS__##t` | token (`t` é sufixo de `import`) | (nenhuma) | **M-G10** "`__VA_ARGS__` visto como fixo" |
| G11 | `#define X im##port` (objeto, dois fixos) | token | (nenhuma) | M-G5 |
| G12 | `#define X(a) a##por##t` | token | `im`+`por`+`t` | **M-G12** "regra por par em vez de por cadeia" (o protótipo) |

### Família H: pragma (D-A3)

| Caso | Conteúdo | Esperado | Mata |
|---|---|---|---|
| H1 | `#pragma once`, `%:pragma once`, `#  pragma   once  // x` | **passa**, um caso cada | **M-H1** "compara o texto cru em vez dos tokens" |
| H2 | `#pragma clang module import std` (P10) | directive, "pragma não permitido" | **M-H2** "pragma volta a ser livre" |
| H3 | `#pragma include_alias(<cstdint>, <windows.h>)` + `#include <cstdint>` (o B8 antigo) | idem | M-H2 |
| H4 | `#pragma comment(lib, "ws2_32")` | idem | M-H2 |
| H5 | `#pragma GCC dependency "x.hpp"`, `#pragma push_macro("X")` | idem, um caso cada | M-H2 |
| H6 | `#pragma once extra` | idem | **M-H6** "aceita `once` como prefixo" |
| H7 | `#pragma` sozinho | idem | **M-H7** "pragma vazio passa" (fechado por padrão) |

O B9 antigo (`_Pragma("include_alias...")`) migra para a família F (F14, motivo token).

### Família L: léxico (D-A4)

| Caso | Conteúdo | Esperado | Prova | Mata |
|---|---|---|---|---|
| L1 | P12 (`1'0` + `"'/*"` + `#include <fstream>`) | policy **na linha 2** | ✓gcc+clang, sem aviso | **M-L1** "sem número de pré-processamento" |
| L2 | Q04 (`0x1'f`) | policy linha 2 | ✓gcc+clang | **M-L2** "separador só antes de dígito" |
| L3 | Q07 (`1'0_km`) | policy linha 2 | ✓gcc (o clang recusa o sufixo, não o léxico) | M-L1 |
| L4 | `constexpr double k = 1'000'000'000.0;` + `#include <cstdint>` (forma real de `time.cpp:26`) | **passa** | árvore real | **M-L4** "separador de dígito reprova" |
| L5 | `double d = 1.5e+3; double e = 0x1p-3; float f = .5f;` | **passa** | (nenhuma) | **M-L5** "`e+`/`p-` quebram o número" |
| L6 | Q01 (`__has_include(<x/*y>)`) | lexical, linha 1 **e** policy linha 3 | ✓gcc+clang, sem aviso | **M-L6** "sem nome de cabeçalho no `__has_include`" |
| L7 | Q02 (`<x'y>`), Q05 (`<x"y>`) | lexical | ✓gcc+clang | M-L6 |
| L8 | Q08 (`__has_include("x\")`) | lexical | ✓gcc (o clang recusa) | **M-L8** "barra invertida fora do conjunto" |
| L9 | `#include <x/*y>` na própria diretiva | lexical | forma da norma | **M-L9** "nome de cabeçalho só no `__has_include`" |
| L10 | `#if __has_include(<cstdint>)` / `#endif` | **passa** | (nenhuma) | **M-L10** "todo `__has_include` reprova" |
| L11 | Q03 (`#if 0` / `don't` / `#endif` + esconderijo) | lexical (literal não terminado) | ✓gcc avisa, clang calado | **M-L11** "literal atravessa linha" (o congelado) |
| L12 | Q06 (`#error it's` em `#if 0`) | lexical | ✓idem | M-L11 |
| L13 | `const char* s = "ab\`(emenda)`cd";` | **passa** | emenda não é quebra real | **M-L13** "emenda conta como quebra" |
| L14 | `u8'a'`, `L"x"`, `U'b'`, `u"y"` | **passa** | (nenhuma) | **M-L14** "prefixo de literal lido como identificador + apóstrofo solto" |
| L15 | os casos de cadeia bruta, emenda, comentário e CRLF de L-1..L-3 | mantidos | (nenhuma) | (não-regressão da divisão da função, R-A3) |

### Controles dos quatro mutantes sobreviventes do revisor (e do quinto que ele marcou menor)

| Caso | Plantio | Esperado | Mata |
|---|---|---|---|
| **C23** | `src/platform/window.hpp` existente; `src/gfui/escape.hpp` com `#include "platform/window.hpp"` (resolve pela raiz `src/`) | policy em `escape.hpp:1` | **MUT2** do revisor (checagem de fronteira apagada) |
| **C23b** | `include/glintfx_extra/x.hpp` existente; `src/core/a.cpp` com `#include "glintfx_extra/x.hpp"` (resolve pela raiz `include/`) | policy em `a.cpp:1` | MUT2, segunda raiz |
| **C24** | ligação `src/core/sub` → pasta externa **limpa** com `x.hpp`; `src/core/a.cpp` com `#include "sub/x.hpp"` | **as duas**: ligação `src/core/sub` **e** policy em `a.cpp:1` (`also_expect`) | **MUT3** do revisor (`islink` do meio apagado). A pasta externa é limpa de propósito: nenhuma outra violação cobre a linha do `#include` |
| **C25** | ligação `include/glintfx/core/sub` → pasta externa limpa; `#include <glintfx/core/sub/x.hpp>` | as duas, idem | MUT3 pelo caminho de `_project_header_verdict` |
| **C26** | `#include <glintfx/core>` e `#include <glintfx/gfss>` (camada sem arquivo) | policy, um caso cada | **MUT1** do revisor (`if not rest`) |
| **D16** | chamada direta de `_kind_from_lstat()` (a parte pura de `_classify_entry`, separada no mesmo commit) com dois registros sintéticos: modo de pasta **com** `FILE_ATTRIBUTE_REPARSE_POINT` → `"link"`; o mesmo **sem** o atributo → `"dir"` | os dois, **em todo sistema** | **MUT4** do revisor (bloco de junção apagado), agora morto no Linux também. **[FATO-CASA]** `stat.FILE_ATTRIBUTE_REPARSE_POINT` existe no Python do Linux (`0x400`), então o controle roda nos cinco alvos. D11 (junção real) continua no Windows: um prova a lógica em todo lugar, o outro o mecanismo onde ele existe (L-04) |
| **D17** (Windows) | **junção** `src/core/sub` → pasta externa limpa, `#include "sub/x.hpp"` | as duas, como C24 | **M-D17** "`_resolve_case_exact` usa `os.path.islink`", que é **falso para junção** (CPython #67596, plano original §1.1). **[DECISÃO]** `_resolve_case_exact` passa a perguntar ao mesmo classificador fechado (`_kind_from_lstat`) em vez de `islink`: hoje o meio do caminho **segue junção no Windows**, o mesmo defeito que a L-4 consertou no percurso e esqueceu na resolução. No Linux, "não aplicável: sem junção", contado |
| **E7** | `00 00 FE FF` + UTF-32BE de `#include <fstream>` | encoding, **mensagem contém "UTF-32"** | **MUT8** do revisor. Sem a marca BE na tupla, o arquivo cai na decodificação UTF-8 e reprova pelo **nulo**: a mensagem é que distingue |

---

## 6. Correção do aninhamento (L-17)

**[FATO-CASA]** medido com `/var/tmp/glintfx-L4-adendo-lab/l17_measure.py` (análise da árvore sintática do Python; a cadeia `if/elif/else` conta **um** nível, a convenção do revisor; `self`/`cls` não contam como parâmetro; função aninhada é medida como função própria) sobre o congelado: 60 funções, **duas acima do teto**:

- `_evaluate_include_argument` (linha 424): 39 linhas, 4 parâmetros, **aninhamento 5**. A do revisor.
- `_translate_phases_2_and_3` (linha 200): **160 linhas**, aninhamento 4. Herdada, e a D-A4 mexe nela.

**[DECISÃO]**
- `_evaluate_include_argument` vira três funções, cada uma fazendo uma coisa: `_macro_candidates(name, macros)` (devolve a lista de corpos ou o marcador "computada"; um nível para o `any(... "other")`), `_angle_body_is_contaminated(body, macros)` (um `any()` sobre `re.finditer`, sem laço aninhado) e `_first_refused_candidate(candidates, root, including_dir)`. A de cima fica com um `if` de primeiro nível. Os controles A1..A8 e `legacy_a..g` são a rede: nenhum muda de veredito.
- `_translate_phases_2_and_3` é dividida por **estado** do léxico (código, cadeia, caractere, comentário de bloco, comentário de linha, cadeia bruta, número, nome de cabeçalho): um tratador por estado, cada um devolvendo o próximo índice e o próximo estado, e um laço de despacho. As duas funções internas (`raw_prefix_length`, `raw_delimiter`) sobem para o nível de módulo. O contrato de saída é o mesmo, acrescido das violações léxicas e das faixas excluídas. **Não é refatoração além do pedido:** a D-A4 acrescenta três regras a ela, e a L-17 proíbe fazer crescer uma unidade que já passou do teto.
- **O critério usa o script, não a leitura.** A contagem à mão enganou o próprio revisor uma vez (ele registrou isso); o implementador roda `l17_measure.py` sobre o arquivo final e cola a saída no relatório, com `acima do teto: 0`.

---

## 7. O que fica fora, e para onde vai

- **D-2 (oráculo diferencial contra o compilador) volta ao líder, com evidência nova. Não decido; o prompt a reservou a ele.** O que muda desde o plano original: esta é a **terceira reprovação** do item, e nesta sessão mais **oito** dessincronias (P12, Q01-Q08) e **onze** formas de `import` foram achadas **em minutos**, por sonda contra o compilador, nenhuma por leitura. Toda a família "ler diferente do compilador" continua sendo achada pelo mesmo método, e o método é exatamente o que o oráculo automatiza. **Recomendo que o orquestrador leve a D-2 ao líder agora**, com esta contagem. As regras deste adendo fecham o que foi medido; o oráculo é o único que fecha o que ainda não foi (o pré-processador tradicional da Microsoft, as funções embutidas, a próxima versão do clang).
- **INBOX, itens novos para o orquestrador registrar:**
  - `GLINTFX-MODULO-NOMEADO`: publicar `import glintfx.core;` para o consumidor. Decisão de produto do líder. Quando vier, a D-A1 vira lista de permitidos de nomes de módulo; `import std;` continua fora enquanto a proibição de `fstream`/`filesystem` existir.
  - `PORTAO-DE-CAMADA-CMAKE`: o `CMakeLists.txt` das camadas puras é **pulado**, e é por ele que se liga biblioteca do SO (`target_link_libraries`) ou se força inclusão (`-include`, `/FI`). Mesma lei (L-19), outra superfície; o portão de fonte não a vê.
  - `PORTAO-DE-CAMADA-MSVC-TRADICIONAL`: o projeto não passa `/Zc:preprocessor`; o pré-processador tradicional da Microsoft tem desvios da norma não modelados aqui (R-A5).
- **Os três itens de INBOX do plano original** (`DIRECAO-ENTRE-PURAS`, `CSTDIO`, `CHECK-LAYERS-MODULOS`) continuam como estavam. O `CHECK-LAYERS-MODULOS` fica **mais urgente**: o arquivo passa de ~1900 linhas com este adendo, e a pergunta 5 da L-17 ("o mesmo arquivo aparece em todo diff") já tem quatro sub-fatias de resposta.

---

## 8. Riscos novos

| # | Risco | Mitigação |
|---|---|---|
| R-A1 | A regra de token reprova código legítimo de módulo no dia em que o GlintFx quiser publicar um. | Declarado; INBOX `GLINTFX-MODULO-NOMEADO` com o caminho (lista de permitidos de nomes de módulo). Hoje zero ocorrências. |
| R-A2 | A regra de colagem é conservadora: uma colagem inofensiva com dois parâmetros (`a##b`) reprova mesmo sem nunca formar nada proibido. | Declarado; a árvore só usa a forma com prefixo fixo (G3). Reprovar é o lado seguro. |
| R-A3 | Dividir `_translate_phases_2_and_3` (160 linhas, herdada de L-1..L-3) pode quebrar um comportamento revisado. | Todos os casos de L-1..L-3 (cadeia bruta, emenda, CRLF, marca UTF-8, dígrafo) continuam no autoteste e são a rede (L15); critério 4 do plano original (contagem antes e depois) vale para eles. |
| R-A4 | O léxico novo muda o `clean_lines` (linha física de cada caractere) e as violações passam a citar outra linha. | Os controles L1, L2, L6 conferem a **linha** da violação, não só o arquivo. |
| R-A5 | O pré-processador tradicional da Microsoft (sem `/Zc:preprocessor`) tem desvios da norma não modelados. | Declarado; INBOX; é da D-2. Nenhum caso deste adendo diz "confirmado" para a Microsoft. |
| R-A6 | Uma versão futura do clang aceitar forma nova de `import`. | A regra é pelo **token**, não pela forma: qualquer forma que use a palavra reprova. Só colagem e o token escapam da palavra literal, e os dois estão cobertos (D-A2). |
| R-A7 | O executor ganha `also_expect` e dois vereditos novos; um erro no executor esconde mutante. | Cada veredito novo tem ao menos um caso que **passa** e um que reprova pelo motivo errado conferido pela mensagem (F-neg, G3, H1, L4, L10). |

---

## 9. Critério de fechamento revisado (escrito antes do código, L-43 global)

Os dez critérios de `docs/plano-layers-l4.md` §4 **continuam valendo**, com as emendas abaixo. A sub-fatia L-4 só fecha quando **todos** valerem, com a prova de cada um no relatório do implementador.

1. **Base de trabalho:** o implementador parte do **congelado** (`/var/tmp/glintfx-plan/check_layers-L4-congelado.py`, md5 `b2450996cfa6b9ea75981e71dae3e8d5`, conferido no início), não da árvore. O md5 de partida vai no relatório.
2. **Cada achado da revisão `revisao-layers-L4.md` tem controle:** CRÍTICO-1 → F1..F11; IMPORTANTE-1 → C23, C23b; IMPORTANTE-2 → C24, C25, D17; IMPORTANTE-3 → D16 (e D11 no Windows); UTF-32 BE → E7; `<glintfx/core>` sem arquivo → C26; IMPORTANTE-4 → critério 7 abaixo.
3. **Cada achado deste adendo tem controle:** famílias F, G, H e L inteiras (seção 5).
4. **Mutação:** cada mutante da seção 5 (M-F*, M-G*, M-H*, M-L*, MUT1..MUT4, MUT8, M-D17) aplicado numa cópia fora da árvore, com o `--selftest` reprovando **e nomeando o caso esperado**. Registro no mesmo `/var/tmp/glintfx-plan/mutacoes-layers-L4.md` (mutante, SHA-base ou md5 da base, comando, saída que prova). **Os quatro mutantes do revisor são reaplicados literalmente como ele os descreveu** (relatório, tabela "Mutantes aplicados"), não reinterpretados. **Mutante que sobrevive reprova a fatia.**
5. **Todas as sondas desta sessão viram prova:** cada arquivo de `/var/tmp/glintfx-L4-adendo-lab/probe/` (P01-P08, P10-P13, P18, P22, P23, P29, P33-P43, Q01-Q08) é rodado contra o portão final pela **linha de comando**, numa fixture limpa, um de cada vez, com o código de saída **lido de variável**. Esperado: todos reprovam, exceto **P29 e P40**, que passam por não serem vetor (erro de compilação nos dois compiladores, tabela 3, linhas 23 e residual); P41 reprova por `#embed`, P42 por diretiva, P43 por token. A lista com o veredito de cada um vai no relatório. **Nenhuma sonda deste adendo pode passar calada sem estar nomeada como exceção aqui.**
6. **Não-regressão na árvore real:** `violations: 0 in 133 files scanned, 3 skipped`, código 0 lido de variável, sobre `git archive` do commit final (ou N e S recalculados por enumeração independente, como no plano original).
7. **L-17 medido por script:** `python3 /var/tmp/glintfx-L4-adendo-lab/l17_measure.py <arquivo final>` imprime `acima do teto: 0` (nenhuma função com mais de 40 linhas, 4 parâmetros ou 3 níveis). Saída colada no relatório. As cinco perguntas do revisor respondidas para `_evaluate_include_argument` (dividida), `_translate_phases_2_and_3` (dividida) e cada função nova.
8. **L-67 no mesmo commit:** saem `_IMPORT_DIRECTIVE_PATTERN`, o ramo `import` de `_scan_one_logical_line`, `_INCLUDE_ALIAS_PATTERN` e a busca textual de `include_alias`, o comentário que chama a decisão de `import` de "herdada, inalterada", e o texto do cabeçalho do arquivo que diz que `_translate_phases_2_and_3`/`_logical_lines` foram "reaproveitadas sem mudança". O `legacy_h` é invertido e renomeado com a citação de [std.modules].
9. **Confirmação contra compilador:** as sondas marcadas ✓ nas tabelas da seção 5 reconfirmadas pelo implementador com o comando da seção 1 (g++ e clang++); as marcadas "docs MS" ou "forma da norma" dizem isso no comentário, **nunca** "confirmado". Se o orquestrador autorizar o contêiner da Microsoft (`tools/msvc-container/`, L-09), F2, F5, H3, H4 e L6 rodam lá e o resultado entra no relatório; senão, "não medido".
10. **CI verde nos cinco alvos**, com o log do Windows mostrando D11 e **D17 executados** e o do Linux mostrando os dois "não aplicável", e D16 executado nos dois. Lista de trabalhos do último run empurrado comparada com a do `ci.yml`.
11. **`TODO.md`:** a linha do item registra L-4 com o adendo e `Status` 🔍 (nunca ✅), no mesmo commit (L-63); os três itens de INBOX novos da seção 7 entram no INBOX, uma linha cada.
12. **`DECISOES_AUTONOMAS.md`:** o orquestrador registra D-A1..D-A4 como decisões autônomas deste C-level, com a razão de uma linha de cada e o caminho deste arquivo, para confirmação retroativa do líder.

**Porte (L-08 global, sem prazo):** o mesmo arquivo de produção (`tests/tools/check_layers.py`) mais `TODO.md`; quatro decisões; quatro famílias novas de controle (F, G, H, L) com cerca de 65 casos e o mesmo número de mutantes; oito controles para os achados do revisor; duas funções divididas (uma delas a de 160 linhas); nenhuma dependência nova, nenhum processo novo no modo real (L-11 global: o portão continua lendo texto, um processo só para a árvore inteira).

---

## 10. Leis aplicadas neste adendo

- **L-42:** a pesquisa (seção 2) veio antes das decisões (seção 4), com fonte em cada fato; nenhum código de terceiro lido (L-29).
- **L-40:** a pergunta central foi respondida por enumeração fechada (seção 3), e todas as quatro superfícies de extensão passaram a lista de permitidos; o piso "contou zero, reprova" continua no executor (tabela vazia reprova).
- **L-36 global / L-12:** cada controle novo nomeia o mutante que mata; os quatro sobreviventes do revisor viraram controles com o mutante literal dele; os casos que passam existem para provar o outro sentido.
- **L-17:** medido por script, não por leitura; duas divisões decididas.
- **L-11 global:** nenhum processo por item varrido; o único processo do autoteste continua sendo o `mklink /J` do Windows (agora em D11 e D17).
- **L-09:** tudo rodou em `/var/tmp/glintfx-L4-adendo-lab/`, fora da árvore; compiladores só com `-fsyntax-only`/`-E`/`-H`/`-c` em arquivos de poucas linhas; nenhuma suíte, nenhum build, nada que toque tela ou entrada.
- **L-27:** fato, fonte, inferência e decisão marcados em cada item.
- **L-67:** o que as decisões tornam falso está listado para apagar (critério 8).
