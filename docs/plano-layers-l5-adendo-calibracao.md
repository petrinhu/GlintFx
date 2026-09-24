# Adendo ao plano L-5 de `LAYERS-GATE-GFSS-GFUI`: calibração, sombra de inclusão e raiz das fixtures

**Autor:** Caetano (CTO, C-level `opus`, esforço alto), 23/09/2026, 19:23 -03, em modo autônomo (L-34: decide no lugar do líder depois de pesquisar; o main registra).
**Emenda:** `docs/plano-layers-l5.md` (não editado). Onde este adendo e o plano divergem, vale este adendo; onde ele silencia, vale o plano.
**Código lido:** HEAD `25579ed`, `tests/tools/check_layers_oracle.py` e `tests/tools/check_layers.py`.
**Insumos:** a pesquisa `/var/tmp/glintfx-plan/pesquisa-calibracao-l5.md`; o diagnóstico `/var/tmp/glintfx-plan/fix-layers-L5-calibracao-windows.md`; os logs `/var/tmp/job-fedora-l5c.log` (run 35915733879, commit `25579ed`) e `/var/tmp/job-windbg-l5b.log` (run 35912952114, commit `07317a4`).

Legenda (L-27): **[FATO-CASA]** lido ou medido aqui, com arquivo:linha ou comando; **[FATO-LOG]** do log do servidor, com linha; **[FATO-FONTE]** de terceiro, com URL; **[INFERÊNCIA]** raciocínio, disputável; **[DECISÃO]** escolha deste adendo, com a razão.

**O que rodou nesta máquina (L-45):** leitura de arquivos e logs; `python3 tests/tools/check_layers_oracle.py --selftest` (rc 0, 24 controles, nenhum compilador); dois `ast.parse` estáticos de `check_layers.py` num script de leitura (não importa o módulo, não chama o portão, não chama compilador). Nenhum oráculo, nenhum compilador, nenhum `--export-fixtures`.

---

## 0. Resumo em uma tela

1. **Falso negativo por sombra: é real, NÃO invalida o oráculo hoje, e fecha por construção** trocando, só na hora de julgar os casos, cada cabeçalho padrão permitido por um **arquivo vazio e sem guarda** (seção 1). Com isso nenhum cabeçalho permitido abre nada, e um proibido nunca pode já estar aberto quando o arquivo de camada pura o inclui. Um **sentinela de sombra** mede, em cada trabalho e a cada rodada, que o fenômeno existe naquele compilador (configuração real: cego) e que a construção o fecha (configuração de caso: vê).
2. **Quarto defeito, achado agora ao ler o código:** o oráculo aponta `-I` para a pasta `include/` do **repositório**, não da **fixture**, e não passa a pasta `src/` que o build real passa. Consequência prevista: o caso `C14_own_header_exists` sairia como **VIOLAÇÃO falsa** assim que a calibração passasse, e `C20`, `C23`, `C23b`, `Fneg5` cairiam em "recusado" sem comparar nada (seção 2). O ataque independente achou que a primeira versão do conserto, com a raiz do código-fonte testada antes da pasta de build, derrubaria em silêncio `C21a` e `C21b` (os dois cabeçalhos gerados); emendado (seções 2 e 8).
3. **Os três consertos da pesquisa (C-1, C-2, C-3) ficam**, com ajustes: C-1 como proposto; C-3 endurecido (a folha e o `<cstdint>` sob a sentinela passam a ser exigências duras, sem a válvula "direto"); C-2 deixa de alimentar a classificação e vira **censo** da biblioteca real, com o piso de 60 **inalterado**, medido no universo certo (seção 3).
4. **Quatro sub-fatias, L-5d a L-5g**, cada uma com escopo, critério escrito antes do dado, controle com mutante e estreia vermelha (seção 4). Numeração dos controles continua em `O-20`.
5. **A estreia do oráculo inteiro (D-L5-1)** só acontece depois de uma rodada verde nos seis trabalhos com as quatro sub-fatias; a previsão escrita muda pouco e está na seção 5.
6. **Sete decisões para o main registrar** (seção 7).

---

## 1. Decisão sobre o falso negativo por sombra (antes de qualquer outra coisa)

### 1.1 O mecanismo, separado em fato e inferência

- **[FATO-FONTE]** GCC, documentação interna do cpplib: na segunda inclusão de arquivo com guarda detectada, o pré-processador *"doesn't preprocess or even re-open the file a second time"*; *"Subsequent calls to `stack_include_file` result in no buffer being pushed"*. <https://gcc.gnu.org/onlinedocs/cppinternals/Guard-Macros.html>
- **[FATO-FONTE]** Clang documenta o fenômeno ao criar a opção que o contorna: *"#include files may be "skipped" due to include guard optimization or #pragma once. This flag makes -H show also such includes."* (`-fshow-skipped-includes`, <https://clang.llvm.org/docs/ClangCommandLineReference.html>, conferido hoje, sem nota de descontinuação na página).
- **[FATO-FONTE]** MSVC: com `#pragma once`, o compilador não reabre o arquivo depois da primeira inclusão na unidade (<https://learn.microsoft.com/en-us/cpp/preprocessor/once>); estudo do `cl.exe` por Geoff Chappell: *"remembers not to open the file again and therefore does not issue a note for /showIncludes"* (<https://www.geoffchappell.com/studies/msvc/cl/cl/options/showincludes.htm>).
- **[FATO-LOG]** Os três compiladores do CI já mostraram o efeito: GCC/Clang omitem da profundidade 1 dezenas de nomes com guarda que a sonda inclui e que já tinham sido abertos por um anterior (pesquisa §1, `job-clang-l5b.log:1688`); `wordsize.h`, sem guarda, aparece repetido (`job-fedora-l5c.log:1759-1768`); o MSVC mostrou 58 nós e nenhum `<cstdint>` (`job-windbg-l5b.log:2169-2221`).
- **[INFERÊNCIA, direta dos fatos acima]** Num caso real, se um cabeçalho padrão permitido `S` abre por dentro um arquivo proibido `F` (por exemplo `<cstdint>` abre `bits/c++config.h` no libstdc++, e `yvals.h` no STL da Microsoft), e o arquivo de camada pura depois escreve `#include <F>`, a segunda inclusão não produz linha: o oráculo não vê a diretiva proibida. Se o portão também errar essa diretiva (é para isso que o oráculo existe), o caso sai "concorda-passa". **Falso negativo na direção 1, calado.**

### 1.2 Ele invalida o oráculo como prova?

**Hoje, não; como garantia, sim, e por isso fecha agora.**

- **[FATO-CASA]** Cada caso já é **uma unidade de tradução por arquivo-alvo**: `_judge_one_case` (`check_layers_oracle.py:639-657`) faz uma invocação por alvo do tipo fonte, e o alvo é o próprio arquivo de camada pura (`_plant_case_fixture`, `check_layers.py:1557-1566`). Isso não fecha a sombra: ela acontece **dentro** de um arquivo, entre duas diretivas.
- **[FATO-CASA]** Varredura estática das tabelas de `check_layers.py` (`ast.parse`, sem executar): 186 chamadas literais de `Case`, mais as três famílias geradas por compreensão (`B7`, `C12`, `F18`) e as seis de codificação (`E1`..`E7`, uma diretiva cada). **Nenhuma** fixture tem um cabeçalho permitido seguido de um proibido no mesmo arquivo; as que citam duas vezes `include` são `H3` (`#pragma include_alias` + um `#include`) e as `L6`..`L8b` (`__has_include` + um `#include <fstream>`). A estreia `M-CR` (`M3`, `M15`) tem um `#include` só.
- **Consequência:** nenhum veredito do oráculo até aqui, nem a estreia prevista, depende do buraco. Mas a tabela cresce a cada fatia, e um oráculo que passa calado quando a forma aparecer viola a L-40. A garantia tem de ser de construção, não de sorte da tabela.

### 1.3 O desenho escolhido: cabeçalhos padrão substituídos por arquivos vazios, só no julgamento dos casos

**[DECISÃO] D-L5d-1.** Na **configuração de caso** (sentinelas de caso e todos os casos do modo `compilar`), o oráculo cria uma pasta `std_stubs/` com **um arquivo vazio, sem guarda e sem `#pragma once`, para cada nome** de `stdlib_permitidos` do manifesto, e a acrescenta como **último** `-I`/`/I`, depois das pastas da fixture e da pasta dos gerados.

Por que fecha por construção:

- **[FATO-FONTE]** GCC: `-I` é procurado *"after the current directory (for the quote form of the directive) and ahead of the standard system directories"* (<https://gcc.gnu.org/onlinedocs/cpp/Search-Path.html>). MSVC: a forma com ângulo procura *"1) Along the path that's specified by each /I compiler option. 2) ... the INCLUDE environment variable"*; a forma com aspas procura antes a pasta do arquivo que inclui (<https://learn.microsoft.com/en-us/cpp/preprocessor/hash-include-directive-c-cpp>). Logo, `#include <cstdint>` resolve para o arquivo vazio nos três compiladores, e um nome **não** permitido continua resolvendo para o arquivo real do sistema.
- Arquivo vazio não abre nada. Então, numa unidade de caso, só três tipos de nó podem ter filhos: arquivo de camada pura (é descido, e o que ele abre aparece), arquivo proibido (o caso já "puxou proibido") e nada mais. **Não sobra caminho para um proibido ser aberto antes, sem ser visto, por um permitido.** O que fica de fora está em 1.5.
- A classificação "padrão" passa a ser **o conjunto de caminhos dos arquivos vazios**, conhecido por construção a partir do manifesto (L-40 item 5: enumeração fechada), e não mais o conjunto aprendido na calibração.
- **Não cria violação falsa.** **[FATO-CASA]** O portão tem semântica de união: toda diretiva de todo ramo conta (`check_layers.py:33-35`, `:847` em diante). O conteúdo dos cabeçalhos padrão só muda o que o compilador faz através de macro e de `__has_include`, isto é, de qual ramo fica ativo; qualquer diretiva que o compilador execute num ramo é uma diretiva que o portão também julga. Diretiva que o compilador executa e o portão deixa passar só acontece por **divergência de leitura**, que é exatamente o defeito que o oráculo procura.
- **Custo:** zero invocação extra por caso; uma escrita de uns cem arquivos vazios por trabalho.

**Prior art (L-43, L-44), técnica aprendida, nada copiado (L-29, só documentação lida):**

- **[FATO-FONTE]** pycparser distribui cabeçalhos falsos da libc (`utils/fake_libc_include`) justamente para pré-processar código sem depender do conteúdo dos cabeçalhos reais do sistema (<https://github.com/eliben/pycparser/blob/main/README.rst>; <https://eli.thegreenplace.net/2015/on-parsing-c-type-declarations-and-fake-headers>).
- **[FATO-FONTE]** Os testes do clang-tidy usam *"a set of simulated system header files"* em `checkers/Inputs/Headers` (<https://clang.llvm.org/extra/clang-tidy/Contributing.html>).
- É a forma que o mercado usa quando quer saber o que **o código do usuário** faz, sem que a biblioteca do sistema interfira.

### 1.4 Alternativas descartadas, com o motivo

| Alternativa | Motivo do descarte |
|---|---|
| Deixar como está e só declarar o risco | Passa calado quando a forma aparecer na tabela. Viola a L-40. |
| "Uma unidade por arquivo de caso" | **Já é assim** ([FATO-CASA] acima). Não fecha a sombra entre duas diretivas do mesmo arquivo. |
| Uma unidade por diretiva de inclusão | Exige separar o arquivo em diretivas, que é exatamente a leitura de texto que está sob teste. Circular. |
| `-fshow-skipped-includes` | Existe e faz o que precisa **só no Clang** ([FATO-FONTE] 1.1). Não há equivalente no GCC nem no MSVC. Quebraria a regra de mesma pergunta nos cinco sistemas (L-04). |
| `-dI` (GCC e Clang) | Existe nos dois: GCC *"Output '#include' directives in addition to the result of preprocessing"* (<https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html>); Clang *"Print include directives in -E mode in addition to normal output"* (URL do Clang acima). Descartado porque: não existe no MSVC; e a saída sai misturada ao texto pré-processado no stdout, onde **[INFERÊNCIA]** uma macro de objeto que expande para `# include <x>` forja uma linha igual à do compilador. |
| Arquivo de dependência (`-MD`/`-MF`) e `/sourceDependencies` | Lista plana, sem pai ([FATO-FONTE] <https://learn.microsoft.com/en-us/cpp/build/reference/sourcedependencies>): mostra que `F` foi aberto, não **quem** o incluiu. Não atribui a diretiva ao arquivo de camada pura. |
| Cabeçalhos-casca por arquivo sombreado (uma casca sem guarda com o nome de `F`, que inclui o `F` real por caminho absoluto) | Frágil: o mesmo nome relativo existe em mais de uma pasta do sistema, e `#include_next` dentro do real muda de comportamento quando ele é aberto por caminho absoluto. Mexe no conteúdo que se quer medir. |

### 1.5 O que continua de fora, declarado

- **Pré-inclusão implícita.** **[FATO-FONTE]** No GNU/Linux, o GCC pré-inclui `stdc-predef.h` no início de toda compilação (<https://github.com/bminor/glibc/blob/master/include/stdc-predef.h>, comentário do próprio arquivo), e o Clang faz o mesmo desde <https://reviews.llvm.org/D34158>. **[INFERÊNCIA]** Um `#include <stdc-predef.h>` num arquivo de camada pura é sombreado por essa pré-inclusão e não aparece. É **um** nome, que o portão reprova pela lista de permitidos. Vira item de INBOX `ORACULO-CAMADAS-PREINCLUDE`. O MSVC não pré-inclui nada sem `/FI` (que o filtro de flags não deixa passar, `check_layers_oracle.py:200-222`).
- **Arquivo do projeto incluído duas vezes.** Irrelevante para a direção 1: o que ele abre já foi visto e julgado na primeira vez.
- **Proibido incluído depois de outro proibido que o abre por dentro.** Irrelevante: o caso já "puxou proibido" (o veredito é por caso, OU entre alvos, `check_layers_oracle.py:645-657`).

### 1.6 As duas travas que provam a construção em cada rodada

- **Trava de folha vazia** (**[DECISÃO]**): na configuração de caso, qualquer nó cujo pai seja um arquivo vazio de `std_stubs/` ou da pasta de gerados é **falha de instrumento**, nomeada. É impossível pela construção; se aparecer, a construção quebrou.
- **Sentinela de sombra** (**[DECISÃO]**): da árvore **real** da calibração, o oráculo escolhe `X` = o primeiro filho do `<cstdint>` real cujo nome-base **não** está em `stdlib_permitidos` (previsto: `bits/c++config.h` no libstdc++, **[FATO-CASA]** `/usr/include/c++/16/cstdint:40`; `yvals.h` ou `yvals_core.h` no STL da Microsoft, **[INFERÊNCIA]**). Monta o arquivo `#include <cstdint>` + `#include "<caminho absoluto de X>"` e o roda duas vezes:
  - **configuração real** (sem `std_stubs/`): resultado **impresso sempre**, nunca reprova: "sombra presente neste compilador: sim" se `X` não aparecer como filho direto, "não" se aparecer. É a régua que mostra, rodada a rodada, que o instrumento antigo era cego aqui;
  - **configuração de caso** (com `std_stubs/`): `X` **tem de** aparecer como filho direto e sair "proibido". Senão, falha de instrumento.
  - Sem candidato `X` (o `<cstdint>` real não abriu nenhum arquivo de nome não permitido): falha de instrumento (L-40: medição vazia reprova).

---

## 2. O quarto defeito: a raiz das inclusões é a do repositório, não a da fixture

- **[FATO-CASA]** `check_layers_oracle.py:828`: `include_dirs = (os.path.join(source_root, "include"),)`, isto é, a pasta `include/` do **repositório**, a mesma para todos os casos. Nenhuma pasta `src/`.
- **[FATO-CASA]** O build real compila `src/core` com `${PROJECT_SOURCE_DIR}/include` e a pasta dos gerados (`cmake/GlintfxLibrary.cmake:153-157`) e com `${PROJECT_SOURCE_DIR}/src` (`src/gfui/CMakeLists.txt:63`, no mesmo alvo `glintfx_library`).
- **[FATO-CASA]** O plano dizia outra coisa: `-I<raiz>/include` com a raiz da fixture (`docs/plano-layers-l5.md` §3.1), e "os `-I`/`-D` do build não são copiados: apontam para a árvore real, e as fixtures têm raiz própria" (§3.3). A implementação divergiu do plano.
- **[FATO-CASA]** `C14_own_header_exists` (`check_layers.py:1818-1822`) escreve `#include <glintfx/core/vec2.hpp>`, planta o cabeçalho na fixture, e o portão passa. O repositório também tem `include/glintfx/core/vec2.hpp` (`ls include/glintfx/core/`).
- **[INFERÊNCIA, previsão]** Com o código de hoje, o compilador abre o `vec2.hpp` **do repositório**, cujo caminho não está em nenhuma pasta pura da fixture: "proibido" com o portão passando = **VIOLAÇÃO falsa**, nos seis trabalhos, assim que a calibração passar. `C20` e `C23` (inclusão entre aspas "pela raiz de `src`", `check_layers.py:1851-1876`), `C23b` e `Fneg5` não acham o arquivo e caem em "recusado", sem comparação nenhuma.

**[DECISÃO] D-L5g-1.** As pastas de inclusão são **lidas da mesma entrada de `compile_commands.json`** de onde já saem as flags (plano §3.3, "lidas da fonte e nunca copiadas") e **remapeadas por uma enumeração fechada, aplicada NESTA ORDEM, a regra mais específica primeiro, e a primeira que casar decide** (emenda do ataque de 23/09, seção 8):

1. pasta dentro da **pasta de build**: substituída pela pasta dos gerados vazios (uma vez só, sem repetir);
2. pasta dentro da **raiz do código-fonte** (e fora da pasta de build): **rebaseada** para a raiz da fixture (mesmo caminho relativo);
3. qualquer outra: mantida como está.

**Por que a pasta de build vem primeiro, e não é detalhe:** **[FATO-CASA]** os seis trabalhos com o oráculo configuram com `cmake -S . -B <pasta>` e nome relativo (`ci.yml:470`, `:798`, `:3095`, `:3183`), ou seja, a pasta de build nasce **dentro** do código-fonte; e a pasta dos gerados é `${PROJECT_BINARY_DIR}/generated/include` (`CMakeLists.txt:73`), dentro das duas. Com a raiz do código-fonte testada primeiro, os gerados seriam rebaseados para um caminho que não existe na fixture. **[INFERÊNCIA, do ataque, conferida por leitura]** `C21a_generated_export` e `C21b` (`check_layers.py:1857-1858`) morreriam em "arquivo não encontrado", que é erro fatal e para o pré-processamento; sairiam "recusado", balde que nunca reprova, e ninguém veria a cobertura sumir. A precedência é decidida por **contenção**, não por posição na lista: a implementação testa "está dentro da pasta de build?" antes de "está dentro do código-fonte?", seja qual for a relação entre as duas pastas (aninhadas como no CI, ou separadas como num build fora da árvore).

Tudo impresso com contagem por regra. A ordem da linha de comando é preservada; `std_stubs/` entra por último. Famílias reconhecidas (fechadas): GNU `-I`, `-isystem`, `-iquote`, `-idirafter`, colados ou em token separado; MSVC `/I`, `-I`, `/external:I`, `-external:I`. Presença de `-include`, `-imacros` ou `/FI` é **falha de instrumento nomeada**: é pré-inclusão que o oráculo não modela.

---

## 3. Os três consertos da pesquisa, julgados

### C-1: o leitor do `-H` lê todas as linhas (adotado como proposto)

- **[FATO-CASA]** `parse_gnu_dash_h_tree` para na primeira linha fora do formato (`check_layers_oracle.py:260-263`).
- **[FATO-LOG]** `job-fedora-l5c.log:1752-1754`: 49 nós de profundidade 1, o último é `backward/strstream`, e `returncode da calibracao: 0`. O compilador terminou; quem parou foi o leitor.
- **[FATO-CASA]** `/usr/include/c++/16/backward/strstream:50` inclui `backward_warning.h`, cujas linhas 31-37 emitem `#warning` quando `__DEPRECATED` está definido. **[INFERÊNCIA forte, não vista no log]** é a linha desse aviso, no mesmo fluxo do `-H`, que parou o leitor: o log só mostra as 20 últimas linhas fora da árvore, todas do bloco final de guardas. A L-5d faz o oráculo imprimir as primeiras também, e a próxima rodada confirma ou refuta.
- **Por que não basta `-w` ou `-Wno-deprecated`:** tapa este aviso e deixa aberto o próximo (qualquer `#warning` de qualquer cabeçalho ou de uma fixture). Prior art: o leitor do Ninja percorre todas as linhas e repassa as que não são de inclusão, nunca encerra por conteúdo (pesquisa §P5, <https://ninja-build.org/manual.html>).
- **Forja:** **[INFERÊNCIA]** nenhuma linha de diagnóstico do GCC/Clang começa por pontos seguidos de espaço (começam pelo caminho do arquivo, por "In file included from", ou por espaço no eco da linha de código); o bloco final de guardas tem caminhos sem pontos. O controle O-20 cobre as três formas.

### C-2: conjunto padrão na árvore inteira (adotado, com outro papel)

Com a seção 1, o conjunto real **não classifica mais nada** nos casos. Ele continua servindo a duas coisas: o **censo** que justifica contar os 105 casos de `C12` "pela calibração" (plano §4.1) e a escolha do `X` da sentinela de sombra.

- Censo = nomes-base de `stdlib_permitidos` presentes **em qualquer profundidade** da árvore real, cujo diretório é o de pelo menos um filho direto da sonda com nome permitido (barra homônimos em `tr1/`, `experimental/`, `ext/`).
- **Piso de 60, inalterado.** **[DECISÃO]** O número não muda; muda o **universo** em que é contado, porque o antigo (só profundidade 1) era inatingível por mecanismo documentado ([FATO-FONTE] 1.1), não por limiar mal escolhido. Isto é consertar o instrumento, não afrouxar o critério depois de ver o dado (L-43).
- Os nomes permitidos **ausentes** do censo são impressos por nome em toda rodada (L-40).

### C-3: sentinela primeiro, com folha própria (adotado e endurecido)

- `#include "sentinela_projeto.hpp"` passa a ser a **primeira** linha da sonda; a sentinela inclui `<cstdint>` e depois `"folha_projeto.hpp"` (arquivo irmão, nome único, vazio).
- **Exigências duras**, todas: a sentinela é o **primeiro** nó de profundidade 1; `folha_projeto.hpp` aparece como filho dela; `<cstdint>` aparece como filho dela. **A válvula "ou `<cstdint>` direto" sai** (`check_layers_oracle.py:491-499`): ela só existia porque a sentinela vinha por último.
- No MSVC, o prefixo continua aprendido na linha da sentinela (`learn_msvc_prefix`, `:277-294`); agora é a primeira linha de nota.

---

## 4. Sub-fatias

Ordem: **L-5d, L-5e, L-5f, L-5g**, commit local por sub-fatia, **um push só** depois das quatro (cada push custa um CI inteiro, e a rodada do servidor é a estreia comum das quatro). Laboratório fora da árvore, como na L-5b (memória "dois agentes na mesma árvore"). Todas seguem L-17 (40 linhas, 4 parâmetros, 3 níveis por função) e L-20 (o controle é escrito e **visto vermelho** antes do código).

Regra comum de mutação (L-27): mutante aplicado em **cópia extraída fora da árvore**, sobre um SHA já commitado; o autoteste tem de reprovar **e nomear o controle esperado**; registro em `/var/tmp/glintfx-plan/mutacoes-layers-L5d-g.md` (mutante, SHA-base, comando, saída). Mutante que sobrevive reprova a sub-fatia.

### L-5d: o leitor do `-H` não para em linha estranha (C-1)

- **Escopo:** `check_layers_oracle.py:parse_gnu_dash_h_tree` (lê todas as linhas; devolve também a contagem de linhas fora do formato); `_parse_tree` (repassa a contagem); `run_oracle`/`_final_report` (imprimem o total de linhas fora da árvore por trabalho, sempre); `_format_calibration_diagnostics` (acrescenta as **20 primeiras** linhas fora da árvore, além das 20 últimas).
- **Critério de fechamento (antes do dado):** O-20 verde; O-1 e O-7 continuam verdes sem mudar a asserção; `M-O20a` e `M-O20b` mortos; `--selftest` com 25 controles ou mais.
- **Controle O-20:** saída enlatada **sintética** com: nós; uma linha `…/backward/backward_warning.h:32:2: warning: #warning This file includes …`; mais nós depois dela (inclusive a sentinela); uma linha `./src/core/x.cpp:1:2: warning: …`; o bloco de guardas com um caminho que começa por `../`. Exige: todos os nós depois do aviso lidos, com pai certo; nenhuma das três linhas estranhas vira nó; contagem de linhas fora da árvore = valor literal escrito no controle (memória "trava tautológica").
- **Mutantes:** `M-O20a` recoloca o `break`; `M-O20b` zera a contagem.
- **Estreia vermelha:** local, O-20 escrito primeiro e visto falhando no código de `25579ed`. No servidor, o vermelho já existe e está registrado: `job-fedora-l5c.log:1752` (49 nós, parada em `strstream`, código 0). **Previsão para a rodada do push:** nos cinco trabalhos GNU, a lista de profundidade 1 da calibração passa de `strstream`; e, **[INFERÊNCIA]**, a primeira linha fora da árvore contém `#warning`. Se não contiver, registrar como fato; o conserto não depende disso.

### L-5e: calibração com sentinela primeiro e censo na árvore inteira (C-3, C-2)

- **Escopo:** `build_calibration_fixture` (sentinela na primeira linha; folha irmã; sentinela inclui `<cstdint>` e a folha); `evaluate_calibration` (as três exigências duras da seção 3; censo com filtro de diretório; piso 60 sobre o censo; nomes ausentes devolvidos); `run_calibration` (imprime censo, ausentes e, no MSVC, o prefixo, **sempre**, não só na falha); O-8 atualizado.
- **Critério de fechamento:** O-21 e O-22 verdes; O-8 e O-19 verdes na forma nova; mutantes `M-O21a`, `M-O21b`, `M-O22a`, `M-O22b` mortos.
- **Controle O-21:** (a) lê o arquivo de sonda que `build_calibration_fixture` grava e exige que a primeira diretiva seja a da sentinela; (b) árvore enlatada com sentinela primeiro e `<cstdint>` e folha sob ela: passa; (c) sem a folha: falha nomeada; (d) `<cstdint>` só como filho direto da sonda, fora da sentinela: falha nomeada; (e) sentinela presente mas não como primeiro nó: falha nomeada.
- **Controle O-22:** árvore enlatada em que 60 nomes permitidos aparecem **só** em profundidade 2 ou maior, dentro de um diretório que tem ao menos um filho direto permitido: censo = 60, passa; um homônimo em `…/tr1/tuple`, cujo diretório não tem filho direto: **não** conta (o controle confere o número exato, literal); 59: falha.
- **Mutantes:** `M-O21a` volta a sentinela para o fim; `M-O21b` tira a exigência da folha; `M-O22a` conta só a profundidade 1; `M-O22b` tira o filtro de diretório.
- **Estreia vermelha:** o vermelho do servidor já está registrado: MSVC `job-windbg-l5b.log:2169-2221` (sentinela achada, `<cstdint>` ausente) e, na leitura da pesquisa §3, o piso inatingível na profundidade 1. **Previsão para a rodada do push:** calibração passa nos seis; censo de **60 ou mais** em cada um; a lista de ausentes é impressa (pode ser vazia; vazia é resultado, não falha).

### L-5f: construção anti-sombra (seção 1)

- **Escopo:** função nova que grava `std_stubs/` (um arquivo vazio por nome do manifesto) e devolve o conjunto de caminhos normalizados; `run_oracle` (a configuração de caso passa a usar `std_stubs/` como último diretório e `ctx.standard_paths` = caminhos dos vazios; a calibração continua **sem** `std_stubs/`); `find_forbidden_pulls` ou função irmã (trava de folha vazia); `run_sentinels` (as sentinelas negativa `<fstream>` e positiva `<cstdint>` rodam na **configuração de caso**, com os vazios, e a negativa mede assim, a cada rodada, que um proibido real que tropeça nos vazios ainda aparece como aberto; acrescenta a sentinela de sombra nas duas configurações, com escolha de `X` a partir da árvore real da calibração). O contexto guarda as duas listas de diretórios (real e de caso) de forma explícita; nenhuma função descobre a configuração por efeito colateral.
- **Critério de fechamento:** O-23, O-24 e O-25 verdes; mutantes `M-O23a`, `M-O23b`, `M-O23c`, `M-O24`, `M-O25a`, `M-O25b` mortos; positivo, negativo, O-2 a O-6 e O-14 continuam verdes.
- **Controle O-23:** manifesto sintético com N nomes: `std_stubs/` tem **exatamente** N arquivos, todos com zero byte, conjunto de nomes igual ao do manifesto (comparação de conjunto, não de contagem); o comando de caso tem `std_stubs/` como **último** diretório de inclusão, depois dos da fixture e dos gerados; o comando de calibração **não** o tem.
- **Controle O-24:** árvore enlatada de caso com um nó filho de um arquivo de `std_stubs/`: falha de instrumento nomeada; o mesmo com filho de um gerado vazio: falha.
- **Controle O-25:** árvore real enlatada em que o `<cstdint>` tem os filhos `stdint.h` (permitido) e depois `bits/c++config.h`: `X` escolhido é o segundo (o controle confere o nome-base literal); saída enlatada da configuração real sem `X`: imprime "sombra presente neste compilador: sim" e **não** reprova; com `X`: imprime "não" e não reprova; saída da configuração de caso sem `X`: falha de instrumento; com `X` como filho direto: passa.
- **Mutantes:** `M-O23a` tira `std_stubs/` do comando; `M-O23b` põe `std_stubs/` em primeiro; `M-O23c` deixa de gravar o último nome; `M-O24` tira a trava; `M-O25a` tira a exigência da configuração de caso; `M-O25b` escolhe o primeiro filho sem filtrar os permitidos.
- **Estreia vermelha:** local, pelos mutantes. No servidor, a própria sentinela de sombra é a estreia a cada rodada: **previsão para a rodada do push**, nos seis trabalhos: "sombra presente neste compilador: **sim**" (base: [FATO-LOG] 1.1, os três compiladores já omitiram cabeçalhos reabertos) e a configuração de caso vendo `X`. Um "não" em algum trabalho é dado a registrar, não falha.

### L-5g: pastas de inclusão lidas do build e rebaseadas na fixture (seção 2)

- **Escopo:** `oracle_main` (lê as pastas da mesma entrada de `compile_commands.json`); função nova de extração das pastas por família fechada; função nova de remapeamento pelas três regras; `_judge_one_case`, `run_calibration` e `_judge_sentinel` passam a montar os diretórios **por raiz** (fixture, calibração, sentinela), nunca uma tupla fixa do trabalho.
- **Critério de fechamento:** O-26 verde; O-16 continua verde; mutantes `M-O26a`, `M-O26b`, `M-O26c` mortos.
- **Controle O-26:** três conjuntos de entradas sintéticas, GNU e MSVC, com `-I<fonte>/include`, `-I` e `<fonte>/src` em tokens separados, `-isystem /opt/x`, `/I`, `-external:I`:
  - (a) **pasta de build ANINHADA**, como no CI: `<build>` = `<fonte>/build-debug`, com `-I<fonte>/build-debug/generated/include`. Saída exigida: `<raiz>/include`, `<raiz>/src`, pasta dos gerados vazios (uma vez), `/opt/x` mantido, na ordem original. O controle confere **explicitamente** que nenhuma saída começa por `<raiz>/build-debug`;
  - (b) **pasta de build SEPARADA** (`<build>` fora de `<fonte>`): mesma saída;
  - (c) `-include foo.h` ou `/FIfoo.h`: falha nomeada.
  Em (a) e (b), a contagem por regra é impressa e conferida contra valores literais escritos no controle.
- **Mutantes:** `M-O26a` não rebaseia (usa a raiz do repositório); `M-O26b` mantém a pasta real do build; `M-O26c` ignora `-include`; **`M-O26d` inverte a precedência (código-fonte antes da pasta de build): tem de reprovar em (a)**. Se `M-O26d` passar em (a), o controle está medindo o universo errado (memória "contagem certa do universo errado") e a sub-fatia não fecha.
- **Estreia vermelha:** local, pelos mutantes (o `M-O26a` é exatamente o código de hoje). **Previsão para a rodada do push, [INFERÊNCIA]:** nos seis trabalhos, `C14_own_header_exists` sai "concorda-passa"; `C20` "concorda-passa"; `C21a_generated_export` e `C21b` "concorda-passa"; `C23` e `C23b` "concorda-reprova". Se algum desses sair "recusado" ou "violação", é defeito do remapeamento até prova em contrário, e a estreia não roda.

---

## 5. A nova estreia vermelha do oráculo inteiro (D-L5-1)

### 5.1 O que precisa estar verde ANTES

1. L-5d a L-5g fechadas pelos critérios da seção 4, com o registro de mutação.
2. **Uma rodada do CI, com as quatro empurradas, com `layers_oracle_test` verde nos seis trabalhos** (plano §6.1), e o log de cada um mostrando: comando usado; flags por família; pastas de inclusão por regra (L-5g); censo, ausentes e prefixo do MSVC (L-5e); linhas fora da árvore (L-5d); as quatro sentinelas (negativa, positiva, sombra real com o "sim/não", sombra de caso); baldes; `puxou_proibido` de 20 ou mais; soma que fecha; tempo do oráculo dentro dos tetos do plano §9.
3. **Nenhum "recusado" inesperado** entre os casos nomeados na previsão de L-5g (`C14`, `C20`, `C21a`, `C21b`, `C23`, `C23b`): arquivo não encontrado é erro fatal e esconde o resto do arquivo (seção 8), então "recusado" num deles é defeito do instrumento até prova em contrário.
4. **Zero VIOLAÇÃO** nessa rodada. Se aparecer violação, vale o plano §8.1-6: item de INBOX com fixture e compilador, decisão do orquestrador, **nunca** afrouxar. A estreia espera.
5. `layers_selftest`, `layers_oracle_selftest` e os meta-portões do plano §8.1-9 verdes nos seis.

### 5.2 A previsão, escrita de novo

O mutante continua o mesmo, `M-CR` (`_LINE_BREAK_SEQUENCE_PATTERN` de `r"\r\n|\r"` para `r"\r\n"`), num ramo descartável a partir do commit verde de 5.1-2.

- **Nos quatro trabalhos GCC (Fedora, Ubuntu, Arch, CachyOS):** `layers_oracle_test` **reprova**, com VIOLAÇÃO nomeando **pelo menos** `M3_lone_cr_is_a_real_break` e `M15_three_breaks_lone_cr`. **Nada muda com a seção 1:** `<fstream>` não é permitido, não tem arquivo vazio, e é aberto de verdade.
- **Clang:** mesma previsão, ainda **[INFERÊNCIA]** (nenhum documento do Clang sobre CR sozinho achado); se não reprovar, é dado e o critério passa a exigir os quatro GCC (plano §8.2, inalterado).
- **Acrescentado por este adendo:** **nenhuma outra VIOLAÇÃO** além das que o `M-CR` explica (casos com CR sozinho). Violação a mais é sinal de que a rodada verde de 5.1-2 não era estável, e anula a estreia.
- **Acrescentado:** as quatro sentinelas passam no ramo da estreia exatamente como na rodada verde (o mutante está no portão, não no instrumento).
- **MSVC (`windows-debug`):** sem previsão, como no plano; o resultado é a primeira medida da casa sobre CR sozinho no compilador da Microsoft.
- **Esperado também vermelho:** `layers_selftest` (plano §8.2, inalterado).

Registro em `/var/tmp/glintfx-plan/estreia-layers-L5.md`; ramo remoto apagado e o apagamento provado por `git ls-remote` (plano §8.2, inalterado).

---

## 6. Critério de fechamento de `LAYERS-GATE-GFSS-GFUI`, reescrito

O item fecha (✅, L-63: só depois da verificação, nunca direto da implementação) quando **todos** valerem:

1. As sub-fatias L-1 a L-4 fechadas como já registrado na linha do item em `TODO.md` (o main confere a linha, este adendo não afirma o estado delas).
2. **L-5 fechada**: os doze itens do plano §8.1, com três emendas: o item 2 passa a exigir também `O-20` a `O-26`; o item 3 passa a exigir também os mutantes `M-O20a` a `M-O26d`; o item 5 passa a exigir, no log de cada trabalho, as linhas novas listadas em 5.1-2.
3. A estreia de 5.2 executada e registrada, com o resultado comparado à previsão, item por item.
4. **Revisão independente adversarial** de L-5d a L-5g (L-12; e, pela emenda de 22/09 da L-34, com sabotagem de família de modelo **diferente** da do planejador e do implementador), que **executa** o autoteste e aplica mutantes próprios, não só os deste adendo.
5. Os itens de INBOX desta cadeia registrados em `TODO.md`: os três do plano §11, mais `ORACULO-CAMADAS-PREINCLUDE` (1.5).
6. Nenhuma VIOLAÇÃO pendente sem decisão: toda violação real que o oráculo achar tem item próprio e decisão registrada.

---

## 7. Decisões para o main registrar em `DECISOES_AUTONOMAS.md`

Formato do arquivo: título com data e hora reais, "Quem decidiu", pergunta, opções, escolha e porquê, porta de mão única, custo de reverter, fontes. Um bloco por decisão; nenhuma muda o que a biblioteca aceita ou entrega (todas são do instrumento de teste).

**D-L5d-1: o falso negativo por sombra fecha por construção, com cabeçalhos padrão vazios só no julgamento dos casos.**
Quem decidiu: Caetano (CTO, `opus`). Pergunta que teria ido ao líder: "o oráculo pode deixar passar uma inclusão proibida escrita depois de uma permitida que já a abriu por dentro; aceitamos o risco, fechamos só no Clang, ou fechamos nos três compiladores?" Opções: declarar e aceitar; `-fshow-skipped-includes` só no Clang; `-dI` no GCC e Clang; cascas por arquivo; **cabeçalhos padrão vazios na configuração de caso (escolhida)**. Porquê: única que fecha nos três com o mesmo mecanismo (L-04) e custo zero por caso; a semântica de união do portão impede violação falsa. Não é porta de mão única; reverter é barato (um diretório de inclusão). Fontes: cpplib Guard-Macros, Clang Command Line Reference, Microsoft `once`, pycparser, clang-tidy Contributing (URLs na seção 1).

**D-L5d-2: resíduo declarado, `stdc-predef.h`.**
Quem decidiu: Caetano. Pergunta: "a pré-inclusão implícita do GCC e do Clang no Linux esconde um nome; fechamos agora ou declaramos?" Opções: `-ffreestanding` (muda o compilador sob teste); **declarar e registrar `ORACULO-CAMADAS-PREINCLUDE` (escolhida)**. Porquê: um nome só, já reprovado pela lista de permitidos do portão; mudar o modo do compilador falsearia o resto. Reverter: barato.

**D-L5d-3: o leitor do `-H` lê todas as linhas e conta as estranhas (C-1), em vez de silenciar avisos.**
Quem decidiu: Caetano. Opções: `-w`/`-Wno-deprecated`; **ler tudo e contar (escolhida)**; as duas juntas. Porquê: silenciar tapa um aviso e deixa o próximo; é o que o Ninja faz. Reverter: barato.

**D-L5e-1: o piso de 60 da calibração não muda; muda o universo em que é contado.**
Quem decidiu: Caetano. Pergunta: "o piso fixado antes do dado se mostrou inatingível; baixamos o número ou consertamos o que ele conta?" Opções: baixar para caber no que a profundidade 1 mostra; **manter 60 e contar na árvore inteira com filtro de diretório (escolhida)**. Porquê: o mecanismo documentado dos três compiladores torna a profundidade 1 incompleta por desenho; baixar o número seria ajustar critério depois de ver resultado (L-43). Reverter: barato.

**D-L5e-2: a calibração exige sentinela primeiro, folha e `<cstdint>` sob a sentinela, e perde a válvula "`<cstdint>` direto".**
Quem decidiu: Caetano. Opções: só a folha (proposta do main); **folha e `<cstdint>`, sem válvula (escolhida)**. Porquê: a folha prova a atribuição de pai sem depender da biblioteca; o `<cstdint>` prova que um padrão sob arquivo do projeto aparece; a válvula só existia porque a sentinela vinha por último. Reverter: barato.

**D-L5g-1: as pastas de inclusão vêm do `compile_commands.json`, remapeadas por regra fechada com a pasta de build antes da raiz do código-fonte, e `-include`/`/FI` reprovam.**
Quem decidiu: Caetano, emendado depois do ataque independente de 23/09 (`/var/tmp/glintfx-plan/ataque-adendo-l5.md`, achado 1). Pergunta: "o oráculo procura cabeçalhos na pasta do repositório em vez da fixture, e sem a pasta `src/` do build; escrevemos as duas pastas no código ou lemos do build?" Opções: fixar `<raiz>/include` e `<raiz>/src` no código; **ler do build e remapear por regra fechada, a mais específica primeiro: pasta de build (vira a pasta dos gerados vazios), depois raiz do código-fonte (rebaseada na fixture), depois o resto (mantido) (escolhida)**. Porquê: segue o princípio do plano §3.3 (lido da fonte, nunca copiado) e acompanha sozinho qualquer pasta nova do build. A precedência existe porque, no CI, a pasta de build fica dentro do código-fonte (`ci.yml:470`, `CMakeLists.txt:73`); na ordem inversa, os dois cabeçalhos gerados sumiriam da comparação em silêncio. Prova: controle O-26 com build aninhado e mutante `M-O26d`. Reverter: barato.

**D-L5-4: as quatro sub-fatias vão num push só, e a estreia de D-L5-1 espera uma rodada verde delas.**
Quem decidiu: Caetano. Opções: um push por sub-fatia; **um push para as quatro (escolhida)**. Porquê: a rodada do servidor é a estreia comum das quatro, e o laboratório já prova cada uma por mutante; quatro rodadas custariam quatro CIs para o mesmo dado. Reverter: barato. A autorização de ramo descartável da D-L5-1 continua a mesma, sem ampliação.

---

## 8. Resultado do ataque independente (23/09/2026) e as três incertezas que eu declarei

**Ataque:** `/var/tmp/glintfx-plan/ataque-adendo-l5.md`, revisor de família diferente (sonnet), só leitura. **Veredito: aprova com emendas.** Um achado IMPORTANTE (a ordem das regras de D-L5g-1), emendado nas seções 2, 4 (L-5g), 5.1 e 7. O ataque confirmou, por leitura: a ordem de busca `-I` para os arquivos vazios nos três compiladores; a semântica de união do portão (não lê o conteúdo dos cabeçalhos padrão, então os vazios não mudam o veredito dele); o defeito da raiz de inclusão; `compile_commands.json` presente nos seis trabalhos (todos com gerador Ninja, `ci.yml:3095`); nenhum mutante amostrado parece equivalente.

**As três incertezas que declarei ao main antes do ataque, uma por uma:**

| # | Incerteza | O ataque | Estado agora |
|---|---|---|---|
| 1 | Forma com aspas no MSVC (`#include "cstdint"`) procura antes nas pastas dos arquivos abertos: poderia achar um arquivo real antes do vazio? | **Confirmou em parte.** Conferiu a forma com ângulo contra a documentação dos três compiladores; a forma com aspas no MSVC não foi examinada. | **Fechada por leitura minha, [FATO-FONTE]** <https://learn.microsoft.com/en-us/cpp/preprocessor/hash-include-directive-c-cpp>: aspas procuram (1) a pasta do arquivo que inclui, (2) as pastas dos arquivos abertos, (3) os `/I` em ordem, (4) `INCLUDE`. Num caso, os arquivos abertos são só os da fixture, e as pastas do STL real estão só em `INCLUDE`, depois dos `/I`. Logo um nome permitido entre aspas resolve para um arquivo da fixture com o mesmo nome (igual ao build real) ou para o vazio, nunca para o STL real. Aspas dentro de um cabeçalho proibido real não importam: ele é folha e não é descido. |
| 2 | Um proibido real (`<fstream>`) passa a achar os vazios nos próprios `#include <istream>`: o pré-processamento chega ao fim? | **Não examinou.** | **Continua [INFERÊNCIA]**, mas **não é mais calada**: a linha do `-H`/`/showIncludes` sai quando o arquivo é aberto, antes de qualquer erro dentro dele, e o julgamento é pelo que foi aberto, não pelo código de saída (O-6). **Emenda desta rodada:** a sentinela negativa (`<fstream>`) passa a rodar **na configuração de caso** (com os vazios), o que a L-5f já implicava e agora fica escrito. Ela mede exatamente esta incerteza a cada rodada, nos seis trabalhos, e a falha é de instrumento, alta, nunca silenciosa. |
| 3 | O `X` da sentinela de sombra pode ser um arquivo que se recusa a ser incluído sozinho (`#error` de "não inclua direto"). | **Não examinou.** | **Continua [INFERÊNCIA]**, medida a cada rodada: a linha do nó sai na abertura, antes do `#error` (no GCC o `#error` não para o pré-processamento; no MSVC o erro fatal C1189 vem depois da nota de inclusão). Se a previsão falhar, a sentinela de sombra da configuração de caso reprova como falha de instrumento, nomeando `X`. Aí a correção é escolher o próximo candidato, **nunca** relaxar a exigência. |

**Ligação entre o achado do ataque e a incerteza 2:** o ataque lembrou que "arquivo não encontrado" é erro **fatal** e para o pré-processamento antes das diretivas seguintes. Então um caminho de inclusão errado não só tira o cabeçalho que falta: **esconde tudo o que vem depois dele no mesmo arquivo**, e o caso sai "recusado", balde que nunca reprova. É mais um motivo para as previsões de L-5g listarem os casos por nome (`C14`, `C20`, `C21a`, `C21b`, `C23`, `C23b`), e para um "recusado" inesperado num deles barrar a estreia (5.1).

## 9. Leis aplicadas

- **L-45 (projeto):** nada do oráculo rodou aqui; todo controle novo usa saída enlatada; o único comando executado foi o `--selftest`.
- **L-40:** a sombra deixa de ser silêncio (trava de folha vazia, sentinela de sombra); linhas fora da árvore, censo e ausentes impressos sempre; enumerações fechadas (famílias de inclusão, arquivos vazios por nome do manifesto).
- **L-43 (global) e L-34 (emenda de 09/09):** manual oficial primeiro (GCC, Clang, Microsoft), depois dor e prática da comunidade (Ninja, pycparser, clang-tidy); cada critério escrito antes do dado da rodada que o mede; o piso de 60 mantido.
- **L-36 (global):** estreia vermelha por sub-fatia (mutante local e vermelho do servidor já registrado ou medido a cada rodada) e estreia do oráculo inteiro depois da rodada verde.
- **L-04:** mesmo mecanismo nos três compiladores; `-fshow-skipped-includes` e `-dI` descartados por serem de um lado só.
- **L-17, L-20, L-27:** funções pequenas e com nome, controle visto vermelho antes do código, mutação em cópia fora da árvore; fato separado de inferência em todo o texto.
- **L-29:** só documentação lida; nenhum código de terceiro aberto.

**Porte (L-08, sem prazo):** quatro sub-fatias, um arquivo tocado (`tests/tools/check_layers_oracle.py`), sete controles novos (`O-20` a `O-26`), dois atualizados (`O-8`, `O-19`), dezesseis mutantes, uma rodada do CI verde, uma estreia no servidor, sete decisões para registrar.
