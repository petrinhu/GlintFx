# Plano da sub-fatia L-4 de `LAYERS-GATE-GFSS-GFUI` (portão de camada, `tests/tools/check_layers.py`)

**Autor:** C-level planejador (opus, esforço alto), 23/09/2026. **Código planejado:** `5d0c173` (HEAD `7beb354` só mudou `TODO.md`).
**Status deste arquivo:** PRONTO (23/09/2026). Três decisões esperam confirmação (seção 8); o resto é executável.

Legenda (L-27): **[FATO-CASA]** medido aqui, com o fixture/comando; **[FATO-FONTE]** de terceiro, com URL; **[INFERÊNCIA]** raciocínio meu, disputável; **[DECISÃO]** escolha deste plano, com a razão.

Laboratório: cópia fora da árvore em `/var/tmp/glintfx-L4-lab/` (`git archive HEAD src include tests/tools/check_layers.py`). Fixtures de sonda em `/var/tmp/glintfx-L4-lab/probe/` (`c01..c19*.cpp`, `u16*`, `u32*`, `nul_mid.cpp`). Compiladores: g++ 16.2.1, clang++ 22.1.8; Python 3.14.7. Nada rodou na árvore rastreada, nenhuma suíte, nenhum build.

---

## 0. Resumo em uma tela

O defeito crítico é maior do que o revisor mediu. **Não é `#if`/`#else`: é que a tabela de macros guarda UM valor por nome.** Com a mesma raiz, cinco formas passam limpas hoje, todas confirmadas contra g++ e clang++ (`-H`, cabeçalho puxado na profundidade 1):

| Fixture | Forma | Compiladores puxam | Portão em `5d0c173` |
|---|---|---|---|
| c01 | `#if 1` define `<fstream>`, `#else` define `<cstdint>`, `#include HDR` | `<fstream>` | PASSA |
| c02 | **sem `#if`**: `#define HDR <fstream>`, `#include HDR`, `#undef HDR`, `#define HDR <cstdint>` | `<fstream>` | PASSA |
| c17 | `#if 0` define `<cstdint>`; `#else` define `OTHER <fstream>` e `HDR OTHER` (cadeia) | `<fstream>` | PASSA |
| c03 | `#define F fstream`, `#define HDR <F>`, `#include HDR` (identificador dentro de `<...>` é expandido) | `<fstream>` | PASSA |
| c18 | `#define cstdint fstream`, `#define HDR <cstdint>`, `#include HDR` | `<fstream>` | PASSA |

E, fora da família de macro, a raiz do INBOX `PORTAO-DE-CAMADA-AGULHA-SO-MORDE-ANGULO` é **muito mais larga** que o item diz: a lista de agulhas é uma **lista de proibidos** (busca dirigida), e a L-40 item 5 manda **enumeração fechada**. Medido **[FATO-CASA]**: todos estes PASSAM hoje e todos puxam o cabeçalho de verdade nos dois compiladores, exceto os marcados "só Windows", que dependem de sistema de arquivos sem distinção de caixa:

`#include "fstream"` (c04), `<./fstream>` (c05), `<c++/16/fstream>` (c06), `"/usr/include/unistd.h"` (c07), `<bits/../fstream>` (c08), `#import <fstream>` (c10), `<linux/input.h>` (c11), `<pthread.h>`/`<dirent.h>`/`<poll.h>` (c12), e, só Windows mas na grafia canônica da documentação da Microsoft, **`<Windows.h>`, `<WinUser.h>`, `<gl/GL.h>` não mordem** (a agulha é sensível a caixa; conferido chamando `_FORBIDDEN_PATTERN.search` direto).

**Desenho recomendado:** trocar a lista de proibidos por uma **lista de permitidos fechada** (biblioteca padrão do C++23 menos `<fstream>`/`<filesystem>`, mais os cabeçalhos do próprio projeto que EXISTEM nas camadas puras); tabela de macros como **multimapa com semântica de união** (qualquer definição que puxe algo não permitido reprova; qualquer definição não-literal ou contaminada reprova como computada); **enumeração fechada dos nomes de diretiva**; **enumeração fechada dos tipos de entrada** da árvore (arquivo regular, pasta, ligação ou junção, outro); **decodificação pela marca de ordem de bytes** como o compilador da Microsoft faz; e os dois buracos de prova fechados. As duas lacunas do INBOX entram (2.4).

---

## 1. Pesquisa (L-42), com fontes e licenças

Separada do que foi medido em casa. Cada item diz de onde veio.

### 1.1 O problema é conhecido?

- **[FATO-FONTE]** Ferramenta que usa o pré-processador do compilador só enxerga **uma configuração**. O include-what-you-use documenta exatamente isso: rodando com `-DTARGET_A`, sugere remover `#include` que só `-DTARGET_B` precisa, porque não tem como saber da outra configuração. Issue 1020 do IWYU, <https://groups.google.com/g/include-what-you-use/c/9CXZpyOV4Ac>. (Li só a discussão, nenhum código.)
- **[FATO-FONTE]** A academia formalizou "todas as configurações ao mesmo tempo": o SuperC (Gazzillo e Grimm, PLDI 2012) resolve inclusão e macro **preservando** os condicionais, cada ramo com sua condição de presença. <https://dl.acm.org/doi/10.1145/2254064.2254103>, <https://paulgazzillo.com/papers/pldi12.pdf>. **[INFERÊNCIA]** A semântica de união deste plano é o caso degenerado desse modelo: toda condição de presença tratada como "pode ser verdadeira". Super-aproximação sólida (nunca perde um ramo), ao custo de falso positivo em ramo morto.
- **[FATO-FONTE]** O GCC documenta a forma computada: se a linha expande para `<` ... `>`, **os tokens entre eles são combinados** (portanto passaram por expansão de macro, que é o que c03 mede); se expande para uma cadeia, o conteúdo não é reexaminado. E o próprio GCC **recomenda usar só "uma única macro de objeto que expande para uma cadeia"**, por portabilidade. <https://gcc.gnu.org/onlinedocs/cpp/Computed-Includes.html>.
- **[FATO-FONTE]** No Windows, `os.walk(followlinks=False)` **segue junções** mesmo assim, e `os.path.islink()` devolve falso para junção. O defeito está aberto no CPython desde 2015, com remendos abandonados: <https://bugs.python.org/issue23407> → <https://github.com/python/cpython/issues/67596> (aberto, sem PR). `os.path.isjunction` só existe a partir do 3.12: <https://github.com/python/cpython/issues/99547>. **Consequência direta:** a recusa de atalho de L-3 (`_find_symlinked_directories`, baseada em `islink`) é **cega a junção no Windows**, e a junção é **percorrida** pelo `os.walk`: o conteúdo por trás dela é varrido no Windows e recusado no Linux. Comportamento diferente por sistema (L-04), e junção para um ancestral dá laço sem fim (L-11 global). Não é hipótese de laboratório: junção é o que o Bazel cria no Windows, justamente por não exigir privilégio.

### 1.2 A ferramenta já resolve isso?

- **[FATO-CASA]** `g++ -E`/`-M`/`-H` resolvem inclusão, macro e fase 4 de verdade, mas **para uma configuração só**. c13 (`#ifdef _WIN32` / `#include <windows.h>`): `g++ -E` no Linux não mostra nada, e o portão atual **reprova**, como deve (o job Windows ativa esse ramo). Trocar o portão pelo pré-processador perderia justamente o caso que a L-19 existe para barrar.
- **[FATO-CASA]** `-H` sobre um arquivo que só tem `#include <cstdint>` lista `/usr/include/sys/cdefs.h` na profundidade 5. Lista de dependências do compilador exige filtrar a profundidade 1, ou a agulha `sys/` morderia todo arquivo.
- **[INFERÊNCIA]** Usar o compilador também exigiria as mesmas `-I`/`-D` do build, um processo por arquivo varrido (a L-11 global proíbe "desenho que gasta um processo por item varrido") e três dialetos de saída. **[DECISÃO]** O portão continua lendo texto, sem chamar compilador. **O compilador entra como ORÁCULO de teste, não como motor**: proposta de sub-fatia própria, seção 7.
- **[FATO-FONTE]** O compilador da Microsoft "detecta a marca de ordem de bytes para decidir se o arquivo está em UTF-16 ou UTF-8; sem marca, usa a página de código do usuário, salvo `/utf-8` ou `/source-charset`" (<https://learn.microsoft.com/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8>) e aceita **"UTF-16 little endian com ou sem marca; UTF-16 big endian com ou sem marca; UTF-8 com marca"** (<https://learn.microsoft.com/cpp/build/reference/unicode-support-in-the-compiler-and-linker>). **[FATO-CASA]** Este projeto **não** passa `/utf-8` nem `/source-charset` (`grep` vazio em `cmake/` e `CMakeLists.txt`). **[FATO-CASA]** g++ e clang++ recusam UTF-16 e UTF-32, com e sem marca (clang: "UTF-16 (LE) byte order mark detected ... encoding is not supported"). Logo, a lacuna UTF-16 é **real e exclusiva do alvo Windows**, o único em que o arquivo compila.
- **[FATO-FONTE]** `#pragma include_alias("a.h", "b.h")` faz o compilador da Microsoft **substituir** o nome num `#include` posterior, por casamento exato de cadeia, nas duas formas (`"..."` e `<...>`): <https://learn.microsoft.com/cpp/preprocessor/include-alias>. `_Pragma` e `__pragma` produzem pragma dentro de macro: <https://learn.microsoft.com/cpp/preprocessor/pragma-directives-and-the-pragma-keyword>. **[INFERÊNCIA, não medido]** `#pragma include_alias(<cstdint>, <windows.h>)` seguido de `#include <cstdint>` puxa `windows.h` no MSVC e passa limpo no portão de hoje. Não havia compilador da Microsoft rodando nesta sessão; confirmar é tarefa do implementador (R-6).

### 1.3 O que a documentação garante, e o que não garante

- **[FATO-FONTE]** A norma: "um cabeçalho não é necessariamente um arquivo-fonte, nem as sequências entre `<` e `>` são necessariamente nomes de arquivo válidos" ([headers] de N4950, <https://timsong-cpp.github.io/cppwp/n4950/headers>). As tabelas "C++ library headers" e "C++ headers for C library facilities" de N4950 são a lista fechada de cabeçalhos padrão do C++23. **O implementador transcreve da norma, não de memória, e o autoteste imprime a contagem** (família C).
- **[FATO-FONTE]** O GCC trata como C++ os sufixos `.cc .cp .cxx .cpp .CPP .c++ .C` (fonte) e `.hh .H .hp .hxx .hpp .HPP .h++ .tcc` (cabeçalho), e `.h` como cabeçalho de C ou C++: <https://gcc.gnu.org/onlinedocs/gcc/Overall-Options.html>. Hoje o portão não conhece `.C`, `.CPP`, `.HPP`, `.H`, `.hp`, `.h++`, `.c++`, e compara sufixo com distinção de caixa.
- **[FATO-FONTE]** Marca UTF-16 é `FF FE`/`FE FF`; UTF-32 é `FF FE 00 00`/`00 00 FE FF` (<https://learn.microsoft.com/windows/win32/intl/using-byte-order-marks>). Ordem de teste obrigatória: UTF-32 antes de UTF-16 (os dois primeiros bytes coincidem).
- **O que NÃO achei garantido em documento nenhum:** a heurística exata com que o compilador da Microsoft reconhece UTF-16 **sem** marca. **[DECISÃO]** Não modelar a heurística: arquivo sem marca com caractere nulo é **recusado, nomeado** (fechado por padrão). Não depende de adivinhar o que a Microsoft faz.

### 1.4 Alguém já tentou e desistiu?

- **[FATO-FONTE]** O verificador de dependência do Chromium (`buildtools/checkdeps/cpp_checker.py`, licença **estilo BSD, The Chromium Authors**, conferida no cabeçalho do arquivo em <https://chromium.googlesource.com/chromium/src/buildtools/+/refs/heads/main/checkdeps/cpp_checker.py>): reconhece só `#include "..."`/`#import "..."` por expressão regular; **não** trata `<...>`, emenda de linha, comentário, macro nem `include_next`; pula blocos `#if 0`; **recusa barra invertida no caminho**. As regras de inclusão dele (`DEPS`, `include_rules`) são **lista de permitidos**: <https://chromium.googlesource.com/chromium/src/+/master/buildtools/checkdeps/rules.py>. Li o comportamento para aprender, **nada foi copiado** (L-29). **Lição:** a ferramenta de referência desistiu da exatidão léxica e compensou com lista de permitidos e com recusa de forma estranha. Este plano faz a mesma troca de política sem abrir mão da exatidão já conquistada em L-1..L-3.
- **[FATO-FONTE]** O CPython desistiu de consertar `os.walk` com junção (1.1). Quem precisa de exatidão escreve o próprio percurso com `os.scandir` e `lstat`.
- **[FATO-FONTE, já registrado no `TODO.md`]** O Clang teve o defeito da cadeia bruta no próprio varredor de dependência (PR #139504). A família "ferramenta lê diferente do compilador" é conhecida fora de casa.

### 1.5 Licenças conferidas (L-29)

| Fonte lida | Licença | Conferida onde | O que li |
|---|---|---|---|
| Chromium `checkdeps` | estilo BSD (The Chromium Authors) | cabeçalho de `cpp_checker.py` no googlesource | comportamento, sem copiar |
| include-what-you-use | não se aplica | só a discussão da issue 1020, nenhum código | limitação documentada |
| SuperC | artigo acadêmico | ACM DL e página do autor | modelo, sem código |
| GCC, Microsoft Learn, Python, CPython issues, N4950 | documentação | — | contratos citados |

---

## 2. O desenho, achado por achado

### 2.1 Achado CRÍTICO: macro-sombra, e as quatro formas irmãs

**Raiz [FATO-CASA]:** `_collect_object_macros()` (`check_layers.py:641`) monta `{nome: literal}`; cada `#define` posterior **sobrescreve** o anterior; definição não-literal do mesmo nome é **descartada** em vez de contaminar; corpo `<...>` é aceito como literal sem notar que o compilador expande identificadores ali dentro.

**A consistência que decide o desenho [FATO-CASA, c13 e c19]:** para inclusão DIRETA o portão **já** usa semântica de união: `#ifdef _WIN32 #include <windows.h>` e até `#if 0 #include <windows.h>` reprovam hoje, porque o portão nunca avaliou condicional. A macro era a **única** exceção. Consertar é torná-la coerente com o resto do portão, não inventar semântica nova.

**[DECISÃO] Multimapa com semântica de união, fechado por padrão:**

1. Coletar **toda** diretiva `define` do arquivo (objeto **ou** função, qualquer corpo, qualquer ramo) num multimapa `nome -> [definições]`. Macro de função se reconhece por `(` **colado** ao nome. `#undef` é ignorado: c02 prova que honrar `#undef` pela ordem do texto é exatamente o que dá errado.
2. `#include IDENT` (ou `#include_next IDENT`) resolve assim:
   - nenhuma definição de `IDENT` no arquivo → **computada não verificável** (como hoje);
   - **alguma** definição de `IDENT` é de função, ou tem corpo que não é um nome de cabeçalho literal → **computada** (mata c17: a cadeia `HDR OTHER` contamina);
   - algum corpo `<...>` contém um identificador (`[A-Za-z_]\w*`) que é **nome definido por qualquer `#define` do mesmo arquivo** → **computada** (mata c03 e c18: o identificador seria expandido);
   - senão, **cada** corpo literal é um candidato, e **qualquer** candidato recusado pela política de nomes (2.4) reprova a diretiva, citando a linha do `#include`.
3. Definição antes ou depois do uso: continua ignorado (simplificação já declarada em L-3; usar antes de definir não compila).

**Descartado, com a razão:**
- *Avaliar `#if` para escolher o ramo ativo:* exigiria as macros de cada um dos cinco alvos. É a limitação de configuração única do IWYU (1.1), e perderia c13.
- *Pré-processador do compilador como motor:* 1.2.
- *Pular `#if 0` como o Chromium:* nenhum `#if 0` existe nas camadas puras (**[FATO-CASA]**, `grep` vazio); ganho medido zero, custo de um reconhecedor de condição com armadilhas próprias (`#if false`, `#if (0)`, `#if 0 // x`). Super-aproximar é mais simples e sólido. Declarado em comentário.
- *Fechar TODA inclusão computada, sem resolver macro nenhuma:* também sólido, mas desfaz uma decisão revisada de L-3 (`case_b`: corpo limpo resolve e passa) sem medição que a derrube. Mantida a resolução, agora correta.

### 2.2 Buraco de prova: o laço de atalho em gfss/gfui nunca é exercitado

**[FATO-FONTE, relatório do revisor]** Apagar o segundo laço de `require_no_symlinked_layer_dirs()` não derruba nenhum dos 14 controles. **[FATO-CASA, lendo o código]** Além disso, `include/glintfx/core` também nunca é exercitado (o controle só planta em `src/core/linked`), nem o ramo "a própria raiz da camada é atalho" (`check_layers.py:898`).

**[DECISÃO]** O controle vira **tabela das seis pastas** (enumeração fechada, L-40 item 5): uma pasta-atalho plantada **em cada uma**, fixture própria por pasta, mais o caso da **raiz da camada** ser o atalho. A tabela deriva da mesma fonte que o código de produção usa (uma constante única com as seis pastas, substituindo `core_source_dirs` + `GFSS_GFUI_DIR_SPECS`), nunca redigitada; e o controle confere que o número de casos é igual ao número de pastas cobertas (mesma trava que `_NEEDLE_BITES_CASES` já usa).

**A junção do Windows (1.1) entra aqui**, porque é o mesmo buraco noutro sistema. O percurso deixa de usar `os.walk` e passa a ser **próprio, com `os.scandir` e `lstat`**, classificando cada entrada numa **enumeração fechada de quatro tipos**: arquivo regular; pasta comum; **ligação** (atalho, ou qualquer ponto de nova análise do Windows: `st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT`, disponível em todo Python 3 do Windows, sem depender do `isjunction` do 3.12); e **outro** (FIFO, dispositivo, soquete). Ligação e outro **reprovam por presença, nomeados, sem abrir nem entrar**. Nunca se desce numa ligação: laço é impossível por construção.

### 2.3 Os três achados menores

**(a) `#include_next`** caía no balde de computada por acidente (a âncora casa `include` como prefixo de `include_next`). **[DECISÃO] Enumeração fechada dos NOMES de diretiva:** depois de `#`/`%:` e espaço, lê-se o identificador inteiro e decide-se pelo nome exato:
- inclusão: `include`, `include_next`, com a mesma política de nome (`#include_next <cstdint>` passa, `<fstream>` reprova: mata o falso positivo e o falso negativo);
- permitidas, sem efeito sobre inclusão: `define`, `undef`, `if`, `ifdef`, `ifndef`, `elif`, `elifdef`, `elifndef`, `else`, `endif`, `line`, `error`, `warning`, `pragma` (esta com a regra de `include_alias` abaixo);
- **qualquer outro nome reprova, nomeado** ("diretiva não permitida em camada pura"). Cobre `#import` (**[FATO-CASA]** c10: GCC e Clang puxam `<fstream>`; no MSVC importa biblioteca de tipos COM, que é SO), `#using` (MSVC C++/CLI), `#embed` (**[FATO-CASA]** clang 22 aceita em C++ como extensão), `#assert`, `#ident` e o que ainda não existe.
- A diretiva nula (`#` sozinho) continua permitida.

**[FATO-CASA]** Nomes usados hoje nas camadas puras (varredura com as funções do próprio portão): `include` 540, `pragma` 82 (todos `once`), `define` 63, `undef` 63. Nenhum outro. A enumeração não reprova a árvore real.

**`include_alias`:** qualquer ocorrência do identificador `include_alias` numa linha lógica (diretiva `pragma`, `_Pragma(...)` ou `__pragma(...)`) reprova, nomeada. **[FATO-CASA]** zero ocorrências hoje. Super-aproximação declarada: a palavra dentro de uma cadeia de texto também reprova (cadeias sobrevivem à fase 3 neste portão).

**(b) Atalho de ARQUIVO quebrado** era contado e nunca reprovava. **[DECISÃO]** Resolvido pela classificação fechada de 2.2: atalho de arquivo (quebrado **ou não**) é "ligação" e reprova por presença, coerente com a decisão já tomada para pasta. E o `except OSError` de `violations_in_file()` deixa de devolver lista vazia: **arquivo regular que não abre reprova**, nomeado (a disciplina da L-40 aplicada a pasta ausente, agora também a arquivo).

**(c) Extensão maiúscula invisível.** **[DECISÃO] Enumeração fechada de TODO arquivo regular das seis pastas**, no lugar do filtro por sufixo:
- sufixo de C++ conhecido, comparado **sem distinção de caixa**: a lista atual unida à do GCC (1.3): `.c .cc .cp .cxx .cpp .c++ .h .hh .hp .hxx .hpp .h++ .tcc .ipp .tpp .inl .cppm .ixx .mpp .ccm .cxxm` → **varrido**;
- nome exato conhecido como não-fonte: `CMakeLists.txt` → **pulado e contado**;
- **qualquer outro arquivo** (sufixo desconhecido, sem sufixo) → **reprova, nomeado** ("arquivo de tipo desconhecido em camada pura").

**[FATO-CASA]** Hoje as seis pastas têm 136 arquivos: 51 `.cpp`, 82 `.hpp`, 3 `CMakeLists.txt`; nenhum atalho, nenhum nulo, todos UTF-8 válido. A saída passa a dizer `violations: 0 in 133 files scanned, 3 skipped (known non-source)`, e as duas parcelas somam o total de entradas. `.c` entra porque `#include "x.c"` existe no mundo real, e varrer um arquivo a mais não custa.

### 2.4 As duas lacunas do INBOX

#### `PORTAO-DE-CAMADA-AGULHA-SO-MORDE-ANGULO`: ENTRA, e o conserto é a lista de permitidos

Razão, medida: o item descreve seis agulhas com `<` embutido; o problema real é que **qualquer lista de proibidos por subcadeia é busca dirigida**, e as sondas da seção 0 acharam onze formas que passam (aspas, `./`, `..`, caminho absoluto, caminho versionado, caixa, cabeçalho de SO que ninguém listou). Trocar seis agulhas por doze só mudaria quais formas passam.

**[DECISÃO recomendada, D-1] Lista de permitidos fechada, com nomes comparados EXATOS e com distinção de caixa** (o mesmo veredito nos cinco sistemas, L-04):

1. **Biblioteca padrão:** as duas tabelas de [headers] de N4950 (C++23), **menos `fstream` e `filesystem`** (a proibição que existe desde ASSET-LOAD; nenhuma proibição nova é inventada aqui). Só na forma `<nome>` ou `"nome"`, sem nenhum `/`. Os `.h` de compatibilidade com C (`<stdint.h>`...) **não** entram: nenhum é usado hoje (**[FATO-CASA]**), e são a porta mais curta para cabeçalhos de SO de nome parecido.
2. **Cabeçalho público do projeto:** `glintfx/core/...`, `glintfx/gfss/...`, `glintfx/gfui/...` que **exista** como arquivo regular em `include/glintfx/<camada>/`, conferido componente a componente contra `os.listdir` (caixa exata mesmo no NTFS), depois de recusar `..`, `.`, componente vazio e barra invertida. `glintfx/platform/...`, e qualquer outra coisa sob `glintfx/`, reprova: a regra que dá nome ao portão passa a ser provada pela ausência na lista.
3. **Cabeçalhos gerados** do projeto, lista fechada citada do CMake: `glintfx/export.hpp` (`cmake/GlintfxLibrary.cmake:131`) e `glintfx/version_macros.hpp` (`cmake/GlintfxLibrary.cmake:140`). **[FATO-CASA]** os dois são incluídos hoje por `src/core` e pelos públicos.
4. **Inclusão entre aspas relativa:** `"x.hpp"` ou `"sub/x.hpp"` que resolve para arquivo regular **existente dentro das seis pastas puras**, procurando na pasta do arquivo que inclui e depois nas raízes de inclusão do projeto (`src/`, por `src/gfui/CMakeLists.txt:63`; `include/`, por `cmake/GlintfxLibrary.cmake:153`). **[FATO-CASA]** é a forma de `"anb.hpp"`, `"gfss/anb.hpp"` e `"core/log/emit.hpp"` na árvore. Se não resolver dentro do projeto, cai na regra 1, que é o que o compilador faz (aspas que não acham local caem na busca de sistema); fora dela, reprova.
5. **Tudo o mais reprova**, com mensagem que diz o nome e por quê.

Com isso, `_FORBIDDEN_PATTERN`, `OS_HEADER_NEEDLES` e `UPPER_LAYER_NEEDLE` **saem** (L-67), e `selftest_needle_bites_control` é substituído pela família C.

**[FATO-CASA] Não-regressão prevista:** todo argumento de inclusão encontrado hoje nas seis pastas (lista impressa na medição de 23/09) é biblioteca padrão fora de `fstream`/`filesystem`, cabeçalho `glintfx/{core,gfss,gfui}` existente, um dos dois gerados, ou relativo resolvível. A árvore real deve continuar com zero violações; isso é critério de fechamento (seção 4), não suposição.

*Alternativa, se D-1 for recusada:* lista de proibidos **normalizada**: nome sem delimitador, em minúsculas, `\` virando `/`, casado como prefixo de componente (`"/" + nome` contém `/fstream`, `/sys/`, `/unistd`...), mais as famílias ausentes (`linux/`, `pthread`, `dirent`, `poll`, `io.h`, `winsock`...). Fecha c04..c08 e a caixa; **não** fecha "o cabeçalho de SO que ninguém listou". Por isso não é a recomendada.

#### `PORTAO-DE-CAMADA-CEGO-A-UTF16`: ENTRA

Razão: 1.2 mostra que é real no alvo Windows (o compilador da Microsoft aceita UTF-16 com e sem marca, e o projeto não passa `/utf-8`), e a família inteira deste portão é "ler diferente do compilador". **[DECISÃO]** Leitura por bytes, e então:

1. `00 00 FE FF` ou `FF FE 00 00` (UTF-32) → **reprova, codificação não verificável** (a Microsoft só documenta UTF-16 e UTF-8; fechado por padrão). Testado **antes** de UTF-16.
2. `FF FE` / `FE FF` → decodifica UTF-16 LE/BE **estrito** (erro de decodificação reprova) e varre normalmente.
3. `EF BB BF` → UTF-8 sem a marca (o que `utf-8-sig` já faz hoje).
4. sem marca → UTF-8 com `errors="replace"` (como hoje).
5. **Depois de decodificar, qualquer caractere nulo reprova**, nomeado ("caractere nulo: possível UTF-16 sem marca, que o compilador da Microsoft aceita"). **[FATO-CASA]** zero arquivos com nulo hoje.

O comentário `LIMITACAO DECLARADA` sobre UTF-16 (`check_layers.py:753-764`) é apagado no mesmo commit (L-67), e as duas linhas do INBOX saem do `TODO.md`, absorvidas neste item com a data.

---

## 3. Controles novos do autoteste, cada um com o mutante que mata

**Forma dos controles [DECISÃO; L-17 e a regra de três do `CONTRACT.md` §6]:** hoje há **sete** cópias do mesmo laço de ~40 linhas ("planta o conteúdo, espera reprovar ou passar, confere que citou o arquivo"). As famílias novas somariam mais seis. Um único **executor de tabela** substitui todas, com cada caso descrito por dados: nome; **bytes** do conteúdo (bytes, não texto: UTF-16 e nulo exigem); pasta onde plantar; arquivos extras da fixture; **veredito esperado** (`passes`, `reproves_policy`, `reproves_computed`, `reproves_directive`, `reproves_entry`, `reproves_encoding`); e o texto que a mensagem tem de conter. O executor confere **o motivo certo**, não só "reprovou": um caso que reprova pelo motivo errado esconde o mutante. As tabelas antigas migram para ele sem perder caso (contagem antes e depois no relatório). **Tabela vazia reprova o autoteste** (L-40 aplicada ao próprio autoteste).

Todo caso "✓gcc/clang" é reconfirmado pelo implementador com `-H` (profundidade 1) antes de virar controle, e a linha de confirmação vai no comentário. Os marcados "docs MS" dizem isso no comentário, nunca "confirmado".

Notação: **M-xx** é o mutante, aplicado numa cópia fora da árvore (L-27); o `--selftest` tem de reprovar **nomeando o caso** listado.

### Família A: tabela de macros (achado crítico)

| Caso | Conteúdo (resumo) | Esperado | Prova | Mata |
|---|---|---|---|---|
| A1 `if1_forbidden_first` | c01 | reprova política | ✓gcc/clang | **M-A1** "último vence" (o código de hoje) |
| A2 `if0_forbidden_second` | `#if 0` `<cstdint>` `#else` `<fstream>` `#endif` `#include HDR` | reprova política | ✓ (ramo `#else` ativo) | **M-A2** "primeiro vence" |
| A3 `redefine_after_use` | c02 | reprova política | ✓gcc/clang | M-A1 e **M-A3** "honra `#undef` pela ordem" |
| A4 `chain_in_other_branch` | c17 | reprova computada | ✓gcc/clang | **M-A4** "ignora definição não-literal em vez de contaminar" |
| A5 `identifier_inside_angle_body` | c03 | reprova computada | ✓gcc/clang | **M-A5** "sem checagem de contaminação" |
| A6 `std_name_redefined` | c18 | reprova computada | ✓gcc/clang | M-A5, na forma com nome da biblioteca padrão |
| A7 `two_clean_branches` | `#if X` `<cstdint>` `#else` `<cstddef>` | **passa** | ✓ | **M-A7** "mais de uma definição vira computada" (rigor demais) |
| A8 `function_like_same_name` | `#if 0` `HDR(x) <fstream>` `#else` `HDR <cstdint>` | reprova computada | super-aproximação declarada | **M-A8** "ignora macro de função" |
| `case_a..h` de L-3 | — | mantidos | — | — |

### Família B: nomes de diretiva

| Caso | Conteúdo | Esperado | Prova | Mata |
|---|---|---|---|---|
| B1 `include_next_clean` | `#include_next <cstdint>` | **passa** | forma de c09 | **M-B1** "`include_next` não reconhecido" (volta o falso positivo) |
| B2 `include_next_forbidden` | `#include_next <fstream>` | reprova política | ✓ c09 | **M-B2** "`include_next` ignorado" |
| B3 `import_directive` | `#import <fstream>` | reprova diretiva | ✓ c10 | **M-B3** "`import` na lista de permitidas" |
| B4 `import_clean_still_refused` | `#import <cstdint>` | reprova diretiva | fechado por padrão | M-B3 |
| B5 `using_directive` | `#using <mscorlib.dll>` | reprova diretiva | docs MS | **M-B5** "nome desconhecido ignorado" |
| B6 `embed_directive` | `#embed "x"` | reprova diretiva | ✓clang (extensão) | M-B5 |
| B7 `every_allowed_name_passes` | um trecho válido para **cada** nome permitido (14) | **passa**, um caso por nome | — | **M-B7** "remove nome da lista"; trava: nº de casos = tamanho da lista |
| B8 `pragma_include_alias` | `#pragma include_alias(<cstdint>, <windows.h>)` + `#include <cstdint>` | reprova diretiva | docs MS | **M-B8** "`include_alias` não procurado" |
| B9 `pragma_operator_include_alias` | `_Pragma("include_alias(\"cstdint\", \"windows.h\")")` | reprova diretiva | docs MS | M-B8 restrito a `#pragma` |
| B10 `pragma_once_passes` | `#pragma once` | **passa** | — | **M-B10** "todo pragma reprova" |

### Família C: política de nome (lista de permitidos; supõe D-1 aprovada)

| Caso | Argumento | Esperado | Prova | Mata |
|---|---|---|---|---|
| C1 | `"fstream"` | reprova | ✓ c04 | **M-C1** "casa só a forma `<...>`" |
| C2 | `<./fstream>` | reprova | ✓ c05 | **M-C2** "normaliza `./` e aceita" |
| C3 | `<c++/16/fstream>` | reprova | ✓ c06 | **M-C3** "compara só o último componente" |
| C4 | `"/usr/include/unistd.h"` | reprova | ✓ c07 | M-C3 |
| C5 | `<bits/../fstream>` | reprova | ✓ c08 | **M-C5** "resolve `..`" |
| C6 | `<Windows.h>` | reprova | caixa (Windows) | **M-C6** "compara sem caixa" (junto com C10) |
| C7 | `<WinUser.h>`, `<gl/GL.h>` | reprova | idem | M-C6 |
| C8 | `<sys\stat.h>` | reprova | ✓ recusado no Linux; docs MS aceita `\` | **M-C8** "troca `\` por `/`" |
| C9 | `<linux/input.h>`, `<pthread.h>`, `<dirent.h>`, `<poll.h>` | reprova, um caso cada | ✓ c11/c12 | **M-C9** "volta a lista de proibidos" |
| C10 | `<FSTREAM>`, `<CSTDINT>` | reprova | fechado | **M-C10** "biblioteca padrão sem caixa" |
| C11 | `<cstdint>` e `"cstdint"` | **passa** | ✓ | **M-C11** "aspas nunca vão à biblioteca padrão" |
| C12 | **cada** nome da lista padrão menos os dois proibidos | **passa**, um caso por nome | — | **M-C12** "nome esquecido na transcrição"; imprime a contagem e compara com a das duas tabelas de N4950, escrita no código com a URL |
| C13 | `<fstream>`, `<filesystem>` | reprova | — | **M-C13** "proibidos saíram da exceção" |
| C14 | `<glintfx/core/vec2.hpp>` existente na fixture | **passa** | — | **M-C14** "cabeçalho próprio exige algo além de existir" |
| C15 | `<glintfx/core/../platform/window.hpp>` | reprova | — | M-C5 |
| C16 | `<glintfx/core/nao_existe.hpp>` | reprova | fechado | **M-C16** "aceita o prefixo sem conferir existência" |
| C17 | `<glintfx/CORE/vec2.hpp>` | reprova nos cinco sistemas | — | **M-C17** "existência por `os.path.exists`" (passaria no NTFS) |
| C18 | `"irmao.hpp"` existente na mesma pasta | **passa** | — | M-C16 invertido |
| C19 | `"faltando.hpp"` | reprova | cairia na busca de sistema | M-C16 |
| C20 | `"gfss/anb.hpp"` a partir de `src/gfui/`, com `src/gfss/anb.hpp` na fixture | **passa** | forma real da árvore | **M-C20** "sem a raiz `src/`" |
| C21 | `<glintfx/export.hpp>`, `<glintfx/version_macros.hpp>` | **passa** | citados do CMake | **M-C21** "gerados fora da lista" |
| C22 | `<glintfx/platform/window.hpp>` | reprova | a regra que dá nome ao portão | **M-C22** "`glintfx/` inteiro permitido" |

### Família D: entradas da árvore (buraco de prova, menores b e c, junção)

| Caso | Plantio | Esperado | Mata |
|---|---|---|---|
| D1..D6 | pasta-atalho para pasta externa suja, **uma em cada uma das seis pastas cobertas** | reprova, cita o atalho, **não** cita o conteúdo | **M-D1** o mutante do revisor (apaga o segundo laço); **M-D2** "só a primeira pasta de `core`" |
| D7 | a própria `src/gfss` é atalho para pasta externa | reprova, cita a raiz | **M-D7** "apaga a checagem da raiz" |
| D8 | atalho de ARQUIVO válido `x.hpp` → arquivo externo sujo | reprova por presença, não cita o alvo | **M-D8** "segue atalho de arquivo" |
| D9 | atalho de arquivo **quebrado** `x.hpp` | reprova (fim do "open refused" silencioso) | **M-D9** o `except OSError` que devolve lista vazia |
| D10 | FIFO chamado `x.hpp` (POSIX) | reprova **sem abrir** (abrir um FIFO trava; o caso roda com teto de tempo) | **M-D10** "abre tudo que se chama `.hpp`" |
| D11 | **junção** em `src/gfss` (Windows, `cmd /c mklink /J`) para pasta externa suja | reprova, cita a junção, não cita o conteúdo | **M-D11** "classifica ligação só por `islink`" |
| D12 | `DIRTY.HPP` com `#include <fstream>` | reprova política (foi varrido) | **M-D12** "sufixo com distinção de caixa" |
| D13 | `x.C`, `x.hp`, `x.h++`, `x.c++`, `x.tcc`, `x.c` sujos, um caso cada | reprova política | **M-D13** "lista de sufixos antiga" |
| D14 | `notas.xyz` e `impl` (sem sufixo) | reprova entrada desconhecida | **M-D14** "pula o desconhecido" |
| D15 | `CMakeLists.txt` com a linha `# include <fstream>` (comentário de CMake) | **passa**, contado como pulado | **M-D15** "varre `CMakeLists.txt`" (falso positivo) |

**Ausência declarada e contada (L-04, L-40):** D10 não existe no Windows (NTFS não tem FIFO); D11 não existe no Linux (não há junção). O executor imprime "não aplicável neste sistema: <motivo>" e conta; **nunca pula calado**. Cada sistema prova o mesmo comportamento observável sobre ligação (presença recusada, conteúdo nunca lido), por mecanismos diferentes, exatamente o que a L-04 item 1 admite.

### Família E: codificação

| Caso | Bytes | Esperado | Mata |
|---|---|---|---|
| E1 | `FF FE` + UTF-16LE de `#include <fstream>` | reprova **política** | **M-E1** "sem decodificar UTF-16" (reprovaria pelo nulo, motivo errado; o executor confere a mensagem) |
| E2 | `FE FF` + UTF-16BE, idem | reprova política | M-E1, lado BE |
| E3 | `FF FE` + UTF-16LE de `#include <cstdint>` | **passa** | **M-E3** "recusa todo UTF-16 em vez de decodificar" |
| E4 | UTF-16LE **sem** marca de `#include <fstream>` | reprova codificação (nulo) | **M-E4** "sem a checagem de nulo" |
| E5 | `FF FE 00 00` + UTF-32LE | reprova codificação | **M-E5** "testa UTF-16 antes de UTF-32" |
| E6 | UTF-8 com nulo no meio | reprova codificação | M-E4 |
| marca UTF-8, CRLF | — | mantidos | — |

**Toda fixture é gravada em binário** (a lição do CRLF triplicado de L-3): o executor recebe bytes e escreve bytes.

---

## 4. Critério de fechamento (escrito antes do código, L-43 global)

A sub-fatia L-4 só fecha quando **todos** valerem, com a prova de cada um no relatório do implementador:

1. **Cada achado do relatório `revisao-layers-L3-rev-layers-2.md` tem controle:** crítico → A1..A8; buraco de prova → D1..D7; `include_next` → B1/B2; atalho de arquivo quebrado → D9; maiúscula → D12. As duas lacunas do INBOX → família C inteira e família E.
2. **Cada mutante da seção 3 foi aplicado numa cópia fora da árvore e fez o `--selftest` reprovar nomeando o caso esperado.** Registro em `/var/tmp/glintfx-plan/mutacoes-layers-L4.md`: mutante, SHA-base, comando, saída que prova. **Mutante que sobrevive reprova a fatia.**
3. **Não-regressão na árvore real** (cópia limpa do commit final): `check_layers.py <raiz>` sai com código 0 **lido de variável**, e imprime `violations: 0 in N files scanned, S skipped`, com **N = 133 e S = 3** se nada foi criado nas seis pastas desde `5d0c173`; se foi, N e S recalculados por enumeração independente (`find` nas seis pastas) e batendo com a saída, com N + S = total de entradas.
4. **`--selftest` local verde**, com a contagem de casos por família impressa, nenhuma tabela vazia, e a soma de casos maior ou igual à de hoje (nenhum controle antigo perdido na migração para o executor).
5. **CI verde nos cinco alvos**, inclusive os três trabalhos Windows, com o log do Windows mostrando D11 **executado** e D10 declarado não aplicável, e o do Linux o inverso. A lista de trabalhos do último run empurrado comparada com a do `ci.yml`.
6. **Confirmação contra compilador:** todo caso "✓gcc/clang" reconfirmado com `-H` na profundidade 1, g++ e clang++, antes de virar controle. Casos "docs MS" (B5, B8, B9, C6..C8, E1..E5 no que toca o MSVC): confirmados no contêiner do compilador da Microsoft ou na VM Windows **se o orquestrador autorizar** (L-09); senão, o comentário diz "fonte: documentação da Microsoft, URL, não medido", nunca "confirmado".
7. **L-17 no código novo:** nenhuma função com mais de 40 linhas, 4 parâmetros ou 3 níveis de aninhamento; nenhum nome com "e". As cinco perguntas do revisor respondidas para cada unidade criada ou crescida.
8. **L-67 no mesmo commit:** apagados `OS_HEADER_NEEDLES`, `UPPER_LAYER_NEEDLE`, `_FORBIDDEN_PATTERN` e `_NEEDLE_BITES_CASES` (se D-1 aprovada), `_collect_object_macros`, `_find_symlinked_directories`/`_walk_header_files` (substituídos pelo percurso próprio), o comentário `LIMITACAO DECLARADA` de UTF-16, a nota de agulha com `<` em `case_c`, e as duas linhas do INBOX no `TODO.md`. O comentário do topo descreve o portão como ele é, não a história de cada sub-fatia em prosa crescente.
9. **`TODO.md`:** linha do item com L-4 registrada e `Status` 🔍 (nunca ✅ direto), no mesmo commit (L-63).
10. **Portões de qualidade (L-23)** que o orquestrador rodar sobre o commit: verdes.

---

## 5. O que o implementador NÃO pode fazer (escopo fechado)

- **Não** chamar compilador, processo ou rede de dentro do portão em modo real. A única chamada de processo permitida é `cmd /c mklink /J` no autoteste do Windows (D11), uma vez, com teto de tempo.
- **Não** seguir ligação, junção ou atalho de arquivo; **não** abrir entrada que não seja arquivo regular.
- **Não** avaliar condicional (`#if`), **não** pular `#if 0`, **não** modelar `#undef` pela ordem.
- **Não** acrescentar proibição nova à biblioteca padrão além de `fstream`/`filesystem` (`<cstdio>` vai ao INBOX, seção 7). **Não** verificar direção entre camadas puras (INBOX).
- **Não** dividir `check_layers.py` em módulos irmãos: seria o primeiro portão Python do projeto a importar módulo irmão, e há portões que enumeram `.py` em `tests/tools/` (ex.: `check_env_sweep.py`), efeito não medido. Proposta no INBOX.
- **Não** mudar o registro no `ctest` (`layers_test`, `layers_selftest`), outro portão, ou arquivo fora de `tests/tools/check_layers.py` e `TODO.md` (mais `DECISOES_AUTONOMAS.md`, se o orquestrador pedir).
- **Não** transcrever a lista da biblioteca padrão de memória: da tabela de N4950, com a URL no comentário.
- **Não** rodar suíte completa nem build (L-09): `python3 check_layers.py --selftest` e o modo real numa cópia fora da árvore são o que se roda.
- **Não** usar pacote de terceiro (L-07): só biblioteca padrão do Python.
- **Não** declarar "confirmado" o que veio de documentação.

---

## 6. Riscos

| # | Risco | Mitigação |
|---|---|---|
| R-1 | **Criar atalho no Windows exige privilégio** (modo desenvolvedor ou administrador). O runner do GitHub é administrador; a máquina de um colaborador pode não ser. | O caso **reprova com mensagem que explica** (nunca pula). Declarado em comentário. Junção (D11) não exige privilégio. |
| R-2 | **`os.walk` segue junção no Windows** (CPython #67596, aberto). | Percurso próprio com `os.scandir` + `lstat`, que nunca desce em ligação (2.2). |
| R-3 | Versão do Python do `windows-latest` não medida aqui; `os.path.isjunction` só existe no 3.12+. | Detecção por `st_file_attributes & FILE_ATTRIBUTE_REPARSE_POINT` (todo Python 3 do Windows). O implementador lê a versão no log do CI e registra. |
| R-4 | Com `core.symlinks=false`, o git do Windows grava atalho como arquivo de texto com o caminho. | Atalho **commitado** reprova nos jobs Linux, que rodam a mesma árvore. Declarado. |
| R-5 | Transcrição errada da tabela padrão → falso positivo ou negativo silencioso. | C12 enumera a tabela inteira e compara a contagem com a da norma, escrita no código com a URL. |
| R-6 | `include_alias`, UTF-16, barra invertida e caixa **não são medíveis** com g++/clang. | Critério 6: contêiner ou VM da Microsoft se autorizado; senão, rótulo "docs, não medido". A sub-fatia L-5 (seção 7) fecha isso no CI. |
| R-7 | Super-aproximações (ramo morto, `include_alias` dentro de cadeia, `#import` limpo, `.c` varrido) reprovam código que compilaria. | Todas declaradas em comentário; nenhuma ocorre na árvore (**[FATO-CASA]**). Reprovar é o lado seguro. |
| R-8 | Fim de linha do Windows nas fixtures (já mordeu em L-3). | Fixture em binário; o executor recebe **bytes**. |
| R-9 | Existência de cabeçalho próprio no NTFS sem distinção de caixa daria veredito diferente por sistema. | Conferência componente a componente contra `os.listdir` (C17). |
| R-10 | A migração dos sete laços antigos para o executor perde um caso em silêncio. | Critério 4: contagem por família antes e depois. |
| R-11 | O arquivo cresce. | O executor de tabela remove as sete cópias do laço; a divisão em módulos fica proposta (INBOX). |

---

## 7. O que fica fora, e para onde vai

- **Sub-fatia L-5 proposta: ORÁCULO DIFERENCIAL contra o compilador real, nos cinco alvos.** **[INFERÊNCIA]** A família inteira de defeitos deste portão (emenda, comentário, marca UTF-8, macro, dígrafo, CRLF, ramo condicional) foi achada por revisor humano comparando o portão com o compilador, sete vezes. Um teste `layers_oracle_test` que pega as fixtures do próprio autoteste (exportadas pelo script: **uma fonte só**), pré-processa cada uma com o compilador do build (`-H` no GCC/Clang, `/showIncludes /Zs` no MSVC: mecanismo diferente, mesma pergunta, L-04) e exige **"compilador puxou cabeçalho não permitido na profundidade 1 ⇒ portão reprovou"** transforma o compilador em revisor permanente, e é o único jeito de provar no CI o que é exclusivo da Microsoft (UTF-16, `include_alias`, barra invertida, caixa). Custo e dúvida: ~80 processos sequenciais do compilador por rodada (número fixo de fixtures, não proporcional à árvore), o que pode esbarrar na letra da L-11 global; por isso é **decisão D-2**, não do plano.
- **INBOX, itens novos para o orquestrador registrar:**
  - `PORTAO-DE-CAMADA-DIRECAO-ENTRE-PURAS`: o portão não verifica que `core` não inclui `gfss`/`gfui`, nem que `gfss` não inclui `gfui`. **[FATO-CASA]** a árvore de hoje respeitaria (core só inclui core; gfss inclui core e gfss; gfui inclui gfss e gfui).
  - `PORTAO-DE-CAMADA-CSTDIO`: `<cstdio>` e `<iostream>` fazem E/S de arquivo e não estão proibidos; a proibição de `fstream`/`filesystem` veio de ASSET-LOAD. Decisão de produto do líder.
  - `CHECK-LAYERS-MODULOS`: dividir `check_layers.py` por assunto (leitura, fases 2/3, diretivas, política de nome, percurso, autoteste). L-17 pergunta 5: o mesmo arquivo aparece no diff de L-1, L-2, L-3 e L-4. Precedente de casa.

---

## 8. Decisões que esperam confirmação (L-10 / L-34)

| # | Decisão | Recomendação | Por quê | Se recusada |
|---|---|---|---|---|
| **D-1** | Trocar a lista de proibidos por **lista de permitidos fechada** (2.4) | **Sim** | É a enumeração fechada da L-40 item 5; a lista de proibidos deixou passar onze formas medidas; é o que o Chromium usa. Muda **o que o portão promete detectar**, e por isso o INBOX a chamou de decisão de produto. | Lista de proibidos normalizada (alternativa em 2.4); família C encolhe (sai C9 e a enumeração C12). |
| **D-2** | Abrir **L-5, oráculo diferencial** | **Sim, depois de L-4** | Fecha a família na raiz e prova o lado Microsoft no CI. | L-4 fecha sozinha; casos da Microsoft ficam "docs, não medido". |
| **D-3** | Absorver as duas lacunas do INBOX em L-4 | **Sim** | Mesma raiz (ler diferente do compilador); a ordem do líder de 22/09 pede o mais completo. O INBOX de AGULHA a declarou decisão de produto. | L-4 fecha só o relatório do revisor; as duas ficam no INBOX. |

**Tudo que não está nesta tabela é decisão técnica deste plano**, com a razão escrita ao lado, e pode ser executado sem nova consulta.

**Porte (L-08 global, sem prazo):** um arquivo de produção (`tests/tools/check_layers.py`) mais `TODO.md`; cinco famílias de controle, ~75 casos novos, ~30 mutantes a provar; um executor de tabela no lugar de sete laços; três decisões acima.
