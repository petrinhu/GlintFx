# Plano da sub-fatia L-5 de `LAYERS-GATE-GFSS-GFUI`: oráculo diferencial do portão de camadas (só no CI do servidor)

**Autor:** C-level planejador (opus, esforço alto), 23/09/2026 (início 13:10 -03). **Código planejado:** HEAD `f189cf7` (portão em `tests/tools/check_layers.py` como deixado por `7018132`).
**Status deste arquivo:** PRONTO (23/09/2026). Três decisões condicionais para o orquestrador (seção 13); o resto é executável.

Legenda (L-27): **[FATO-CASA]** medido aqui, com o comando; **[FATO-FONTE]** de terceiro, com URL; **[INFERÊNCIA]** raciocínio meu, disputável; **[DECISÃO]** escolha deste plano, com a razão.

**O que rodou nesta máquina (L-45, L-09):** leitura de arquivos; um `import` do portão como módulo Python numa cópia no scratchpad para contar tabelas (nenhum compilador, nenhum autoteste, nenhum `check_layers()` chamado); **uma** medição de `g++ -H` num arquivo de três linhas (autorizada no briefing); `gh run view` (leitura de duração de trabalhos). Nada mais. Nenhum oráculo, nenhum compilador por fixture.

---

## 0. Resumo em uma tela

**O que o oráculo é:** um segundo script, `tests/tools/check_layers_oracle.py`, que pede ao próprio portão (uma chamada, modo novo `--export-fixtures`) as fixtures das suas tabelas de autoteste **e o veredito real do portão para cada uma**, pré-processa cada fixture com o compilador do trabalho de CI (`-E -H` no GCC e no Clang; `/P /showIncludes` no MSVC), reconstrói a árvore de inclusão, e reprova se **algum arquivo de camada pura puxou diretamente um cabeçalho fora da lista de permitidos e o portão, para aquela fixture, passou**.

**As sete respostas, em uma linha cada:**

1. **Como perguntar:** `-H` (não `-M`/`-MM`) no GCC/Clang, com `-E` e `LC_ALL=C`; `/showIncludes` com `/P` no MSVC, prefixo localizado aprendido por calibração. `-MM` **esconde** exatamente o que procuramos (cabeçalho de sistema), `-M` achata a profundidade (seção 3).
2. **Uma fonte só:** registro único `_CASE_TABLES` dentro de `check_layers.py`, consumido pelo autoteste **e** pelo `--export-fixtures`, com trava que enumera o módulo inteiro e reprova tabela fora do registro. O oráculo nunca importa o portão nem copia fixture: conversa com ele pela linha de comando e por um manifesto (seção 4).
3. **A promessa é uma direção só:** "compilador puxou proibido ⇒ portão reprovou" é **falha dura**. O contrário (portão reprovou, compilador não puxou) é a super-aproximação declarada: **relatada por categoria, com nome e contagem, nunca reprova** (seção 5).
4. **Onde:** partes compartilhadas dos quatro Linux (GCC), do `clang` e do `windows-debug` (MSVC). Local: registrado no `ctest` com `DISABLED`, visível como "Not Run (Disabled)", mais recusa em tempo de execução fora do GitHub Actions e trava contra a opção aparecer em `tools/preci.sh` (seção 6).
5. **Piso:** contagens por balde impressas sempre; zero comparadas reprova; calibração com sentinelas que reprovam o instrumento cego (seção 7).
6. **Fechamento e estreia vermelha:** critério na seção 8, escrito antes do código; estreia com o defeito do retorno de carro sozinho reintroduzido num ramo descartável, vermelho nomeado nos trabalhos GNU (seção 8.2).
7. **Custo:** GCC/Clang na casa de segundos por trabalho (medido: 0,01 s por pré-processamento pequeno); MSVC é o risco, e por isso vai no trabalho com folga (`windows-debug`, 9,6 min), com teto pré-registrado de 3 min para o oráculo (seção 9).

**Decisões que saem daqui para o orquestrador:** três (seção 13), nenhuma de produto; a do ramo descartável da estreia é ação visível em remoto e precisa de autorização.

---

## 1. Pesquisa (L-42/L-43), com fontes e licenças

### 1.1 O que cada compilador garante sobre "que cabeçalhos você puxou?"

- **[FATO-FONTE]** GCC, `-H`: *"Print the name of each header file used, in addition to other normal activities. Each name is indented to show how deep in the '#include' stack it is."* <https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html>. A documentação **não** diz em qual fluxo sai nem menciona a lista de guardas de inclusão.
- **[FATO-FONTE]** GCC, `-MM`: *"Like -M but do not mention header files that are found in system header directories, nor header files that are included, directly or indirectly, from such a header."* (mesma página). **Consequência:** `-MM` apaga `<fstream>`, `<windows.h>`, `<unistd.h>` da saída, que é exatamente o que o oráculo tem de ver. `-M` lista tudo, mas **achatado, sem profundidade**: não distingue "a camada pura puxou `sys/cdefs.h`" (proibido) de "`<cstdint>` puxou `sys/cdefs.h`" (normal). **[DECISÃO]** `-H`, nunca `-M`/`-MM`.
- **[FATO-FONTE]** Clang, `-H, --trace-includes`: *"Show header includes and nesting depth"*; `-E`: *"Only run the preprocessor"*. <https://clang.llvm.org/docs/ClangCommandLineReference.html>. O formato exato da saída do Clang **não** está documentado. **[INFERÊNCIA, não medido: o briefing só permite UM exemplo, e ele foi gasto no g++]** o Clang usa o mesmo formato de pontos; a calibração da seção 7 reprova o instrumento se não usar.
- **[FATO-CASA]** Medição única autorizada, g++ 16.2.1, `g++ -std=c++23 -x c++ -E -H -o /dev/null p.cpp` sobre `#include <cstdint>` + `#include "x.hpp"` duas vezes (`x.hpp` com `#pragma once` e `#include <cstddef>`), em `…/scratchpad/probe/`:
  - saída no **stderr**, stdout vazio, código 0, **0,01 s** de relógio;
  - formato `<pontos><espaço><caminho>`, um ponto por nível: `. /usr/include/c++/16/cstdint`, `.. /usr/include/c++/16/x86_64-redhat-linux/bits/c++config.h`, …, `. x.hpp`, `.. /usr/include/c++/16/cstddef`;
  - o caminho sai **como foi aberto**: absoluto para o de sistema, **relativo ao diretório de trabalho** para o de aspas (`x.hpp`);
  - o segundo `#include "x.hpp"` **não aparece** (guarda `#pragma once`): cada cabeçalho aparece na primeira abertura;
  - no fim vem a lista de guardas **traduzida pela localidade**: `Múltiplos include guards podem ser úteis para:` seguida de caminhos **sem pontos**. **Consequência:** o analisador aceita só linhas `^\.+ `, para na primeira que não casa, e o oráculo exporta `LC_ALL=C` para o compilador (determinismo, L-04);
  - na profundidade 5 aparece `/usr/include/sys/cdefs.h`, puxado pelo próprio `<cstdint>`. Confirma o achado de L-4 §1.2: **só os filhos diretos de arquivo de camada pura interessam**; o que os cabeçalhos padrão puxam por dentro não é da conta do portão.
- **[FATO-FONTE]** MSVC, `/showIncludes`: *"When the compiler comes to an include file during compilation, a message is output ... `Note: including file: d:\MyDir\include\stdio.h`"*; *"Nested include files are indicated by an indentation, one space for each level of nesting"*. <https://learn.microsoft.com/cpp/build/reference/showincludes-list-include-files>. A página atual **não** diz o fluxo.
- **[FATO-FONTE]** Ninja, sobre o mesmo formato: *"the tool outputs specially-formatted lines to its stdout"*; e o prefixo **é traduzido**: *"`msvc_deps_prefix` ... defines the string which should be stripped from msvc's /showIncludes output. Only needed when `deps = msvc` and no English Visual Studio version is used."* <https://ninja-build.org/manual.html>. **[DECISÃO]** O prefixo não é escrito no código: é **aprendido na calibração** (seção 7), procurando a linha que termina no caminho de um arquivo conhecido.
- **[FATO-FONTE]** MSVC, `/E`: *"copies the preprocessed files to the standard output device"*; `/P` manda para arquivo. <https://learn.microsoft.com/cpp/build/reference/e-preprocess-to-stdout>. **Consequência:** com `/E`, o texto pré-processado e as notas de `/showIncludes` dividem o stdout, e o conteúdo de uma fixture poderia forjar uma linha "Note: including file:". **[DECISÃO]** `/P /Fi<arquivo descartável>`: o stdout fica só com as notas. **[INFERÊNCIA, não medido]** `/showIncludes` emite com `/P`; se a primeira rodada no CI mostrar que não, o substituto é `/Zs` (só sintaxe, mais lento). A calibração pega a diferença sozinha: sem notas, o instrumento reprova.
- **[FATO-FONTE]** MSVC tem dois pré-processadores; `/Zc:preprocessor` liga o conforme e `/Zc:preprocessor-` pede o tradicional explicitamente. <https://learn.microsoft.com/cpp/build/reference/zc-preprocessor>. **[FATO-CASA]** `grep -n 'Zc' cmake/*.cmake CMakeLists.txt`: vazio, o projeto não escolhe. **[DECISÃO]** O oráculo **não** passa `/Zc:preprocessor` em nenhum sentido: pergunta ao compilador no **mesmo modo em que o build compila**, porque é esse modo que o portão tem de acompanhar.
- **[FATO-FONTE]** GCC aceita três terminadores de linha: *"GCC accepts the ASCII control sequences LF, CR LF and CR as end-of-line markers."* <https://gcc.gnu.org/onlinedocs/cpp/Initial-processing.html>. É a base da estreia vermelha (seção 8.2). Para o MSVC, **não achei** documento que diga se CR sozinho termina linha; o comportamento é medido pelo oráculo no CI, não presumido.

### 1.2 O que o CI e o CTest garantem

- **[FATO-FONTE]** GitHub Actions: *"`GITHUB_ACTIONS`: Always set to `true` when GitHub Actions is running the workflow. You can use this variable to differentiate when tests are being run locally or by GitHub Actions."* <https://docs.github.com/en/actions/reference/workflows-and-actions/variables>.
- **[FATO-FONTE]** CTest, propriedade `DISABLED`: *"If set to `True`, the test will be skipped and its status will be 'Not Run'. A `DISABLED` test will not be counted in the total number of tests and its completion status will be reported to CDash as `Disabled`."* <https://cmake.org/cmake/help/latest/prop_test/DISABLED.html>. **O que a página NÃO garante:** o código de saída do `ctest` com teste desligado, e se `ctest -N` o lista. **[INFERÊNCIA]** sai 0 e lista; o implementador mede numa pasta de build descartável (configurar e listar localmente não executa o oráculo, é permitido) e registra.

### 1.3 Alguém já faz isso?

- **[FATO-FONTE]** O Ninja já trata o prefixo traduzido do `/showIncludes` como problema conhecido (acima): quem confiou no texto inglês quebrou em Visual Studio de outra língua.
- **[FATO-FONTE, herdado de L-4 §1.1]** O include-what-you-use documenta que ferramenta baseada no pré-processador do compilador só enxerga **uma configuração** (issue 1020). É exatamente por isso que o oráculo **não substitui** o portão e a direção 2 não pode ser promessa (seção 5): o portão vê todos os ramos, o compilador vê um.
- **Procurei e não achei:** ferramenta pública que faça "teste diferencial de um verificador de inclusão contra `-H`/`/showIncludes`". A técnica geral (teste diferencial contra implementação de referência) é conhecida; a forma específica, sem prior art achada. Declarado, não calado (L-43).

### 1.4 Licenças (L-29)

| Fonte lida | Licença | O que li |
|---|---|---|
| Manuais do GCC, do Clang, do CMake, do Ninja, Microsoft Learn, docs do GitHub | documentação | contratos citados; nenhum código lido nem copiado |

Nenhum código de terceiro foi aberto nesta pesquisa.

---
## 2. Desenho em uma página

```
ctest (CI, opção ligada)                         ctest (máquina do líder)
  layers_oracle_test ─┐                            layers_oracle_test  -> "Not Run (Disabled)"
                      │                            layers_oracle_selftest -> roda (zero compilador)
  check_layers_oracle.py
    0. recusa se GITHUB_ACTIONS != "true"   (antes de qualquer processo)
    1. lê compile_commands.json do build: tira SÓ as flags de família fechada (seção 3.3)
    2. 1 processo: check_layers.py --export-fixtures <scratch>
         -> árvores de fixture + manifest.json (veredito REAL do portão por caso)
    3. 1 processo: calibração (todos os cabeçalhos padrão permitidos num arquivo só,
         mais as sentinelas) -> mapa de caminhos padrão, prefixo do MSVC, instrumento OK?
    4. N processos, SEQUENCIAIS, um por arquivo de camada pura plantado pelo caso:
         compilador -E -H  |  cl /P /showIncludes
    5. árvore de inclusão -> filhos diretos de arquivo de camada pura -> permitido?
    6. compara com o veredito real do portão; imprime baldes; reprova se violação,
       zero comparadas, calibração falha, piso de puxadas proibidas não atingido
```

**Arquivos tocados (escopo fechado, seção 11):** `tests/tools/check_layers.py` (registro, modo de exportação, três controles), **novo** `tests/tools/check_layers_oracle.py`, `tests/CMakeLists.txt` (dois testes), `cmake/GlintfxOptions.cmake` (uma opção), `.github/workflows/ci.yml` (a opção em seis linhas de configuração), `TODO.md`.

**[DECISÃO] Arquivo separado, não modo dentro de `check_layers.py`.** O portão já tem 3215 linhas e L-17 (pergunta 5: o mesmo arquivo em todo diff). O oráculo faz outra coisa (roda compilador, lê saída de compilador), e a proibição de L-4 §5 contra "módulo irmão importado" continua respeitada: **o oráculo não importa o portão**, fala com ele pela linha de comando e por um manifesto JSON, como qualquer consumidor externo. Isso também impede que o oráculo reuse, por acidente, a lógica que ele existe para vigiar.

---

## 3. Pergunta 1: como o oráculo pergunta "que cabeçalhos você puxou?"

### 3.1 Linha de comando por dialeto

O dialeto vem de `CMAKE_CXX_COMPILER_FRONTEND_VARIANT` (**[FATO-FONTE]** *"Identification string of the compiler frontend variant"*, desde 3.14, <https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_FRONTEND_VARIANT.html>), passado pelo CMake ao registrar o teste. Valor fora de `GNU`/`MSVC` reprova o oráculo, nomeado (enumeração fechada, L-40 item 5).

| Dialeto | Comando por arquivo | Fluxo lido | Ambiente |
|---|---|---|---|
| `GNU` (g++, clang++) | `<cxx> <flags §3.3> -x c++ -E -H -o <nulo> -I<raiz>/include -I<stubs> <arquivo>` (`-x c` para `.c`) | stderr | `LC_ALL=C` |
| `MSVC` (cl) | `<cl> /nologo <flags §3.3> /TP /P /Fi<scratch>\out.i /showIncludes /I<raiz>\include /I<stubs> <arquivo>` (`/TC` para `.c`) | stdout (e stderr juntado, por segurança) | herdado |

- `<nulo>` = `os.devnull` (nunca `/dev/null` escrito à mão: L-04).
- `-x c++`/`/TP`: força a língua para cabeçalho e para extensão que o compilador não conhece (`.hpp`, `.ipp`, `.tcc`, `.ixx`...). `.c` vai como C porque é assim que o build o compilaria; nenhum arquivo `.c` existe nas camadas puras (**[FATO-CASA]** pela contagem de L-4), e a fixture `D13f` é a única.
- Diretório de trabalho = pasta do arquivo compilado; todo caminho relativo impresso é resolvido contra ela e normalizado por `os.path.realpath`, e no dialeto MSVC também por `os.path.normcase` (NTFS não distingue caixa). A normalização é chamada com o dialeto como **argumento**, nunca por `sys.platform`, para o autoteste exercitar os dois ramos em qualquer sistema (L-04).
- `<stubs>` = pasta do scratch com os dois cabeçalhos gerados (`glintfx/export.hpp`, `glintfx/version_macros.hpp`), **vazios**, com os nomes lidos do manifesto (o portão exporta a própria `_GENERATED_HEADERS`: uma fonte só).
- Teto por invocação: 60 s. Estourou: **falha de instrumento**, reprova, nomeando a fixture. Nunca "pula".
- Execução **estritamente sequencial**: um compilador vivo por vez. Nada de pool de processos. A L-45 abre exceção para "um processo por item" no servidor, não para rajada paralela.

### 3.2 Leitura da saída: árvore, não lista

- **GNU:** cada linha que casa `^(\.+) (.+)$` vira nó de profundidade `len(pontos)`; a primeira linha que não casa **encerra** a leitura (é o começo da lista de guardas, medida traduzida em 1.1). Pai de um nó = o último nó de profundidade `d-1` visto antes dele; profundidade 1 tem como pai o arquivo compilado.
- **MSVC:** linha que começa pelo prefixo aprendido na calibração; profundidade = número de espaços entre o prefixo e o caminho (doc em 1.1: "one space for each level"). Mesmo algoritmo de pai.
- **Salto de profundidade** (um nó de profundidade `d+2` logo depois de um de `d`) é saída que o analisador não entende: **falha de instrumento**, reprova. Nunca adivinha.

### 3.3 Flags do build, lidas da fonte e nunca copiadas

**[DECISÃO]** O padrão da língua e o modo do pré-processador são os do **build real**, lidos de `compile_commands.json` (o projeto já o exporta em build de topo: **[FATO-CASA]** `cmake/GlintfxOptions.cmake:67-70`; **[FATO-FONTE]** *"generates a compile_commands.json file containing the exact compiler calls for all translation units of the project"*, só nos geradores Makefile e Ninja, <https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html>; o CI usa Ninja em todos os trabalhos). O oráculo pega a entrada de um arquivo de `src/core/` e extrai **só** tokens de uma família fechada:

| Família | GNU | MSVC |
|---|---|---|
| padrão da língua | `-std=...` | `/std:...`, `-std:...` |
| modo do pré-processador | (não existe) | `/Zc:preprocessor`, `/Zc:preprocessor-`, `/experimental:preprocessor` |
| conjunto de caracteres da fonte | `-finput-charset=...` | `/utf-8`, `/source-charset:...` |

Imprime o que achou em cada família e quantos tokens ignorou. **Padrão da língua ausente reprova** (sem ele o oráculo perguntaria a um compilador em C++17 sobre `#elifdef`). Os `-I`/`-D` do build **não** são copiados: apontam para a árvore real, e as fixtures têm raiz própria.

**Por que não `CMAKE_CXX23_STANDARD_COMPILE_OPTION`:** procurei documentação dessa variável e **não achei** (busca "CMAKE_CXX23_STANDARD_COMPILE_OPTION variable cmake documented", nenhum resultado em cmake.org). Variável interna do CMake não é contrato.

---

## 4. Pergunta 2: uma fonte só de fixtures

**[FATO-CASA]** Contagem feita importando uma cópia do portão no scratchpad: **20 tabelas de `Case` no nível do módulo, 309 casos**. `_SELFTEST_GROUPS` cita 16 delas diretamente; as outras quatro (`_FAMILY_B7_CASES` 14, `_FAMILY_C12_CASES` 105, `_FAMILY_F18_CASES` 8, `_FAMILY_D_EXTENSION_CASES` 10) só aparecem dentro de funções de autoteste. Os 334 do autoteste = 309 de tabela + os controles escritos como função (percurso, ligação, junção, FIFO, propriedade de normalização), que **não são perguntas de pré-processador** e ficam fora do oráculo por natureza (o compilador não vê atalho de pasta; quem vê é o sistema de arquivos).

### 4.1 No portão (`check_layers.py`)

1. **Registro único `_CASE_TABLES`**: tupla de `(rótulo, tabela, modo_oráculo)`, com `modo_oráculo ∈ {"compilar", "calibracao"}`. Só `C12` recebe `"calibracao"` (seção 7.1: o conteúdo de cada caso de C12 é `#include <nome>\n`, exatamente o que a calibração pré-processa de uma vez; compilar os 105 separados custaria 105 processos para repetir a mesma pergunta). O motivo fica escrito ao lado da entrada.
2. **Trava de completude, controle novo `X1`** no `--selftest`: enumera `vars()` do módulo, acha **toda** tupla não vazia cujos elementos são todos `Case`, e reprova se alguma não estiver em `_CASE_TABLES` (ou se o registro citar tabela que não existe). Enumeração fechada do módulo, não lista mantida à mão: tabela nova que alguém esquecer de registrar reprova sozinha. Rótulos repetidos também reprovam.
3. **Modo novo `--export-fixtures <pasta-vazia>`**: para cada caso de cada tabela do registro, chama **a mesma** `_plant_case_fixture()` do autoteste (a construção da fixture também é fonte única, não só o dado), depois **a mesma** `check_layers(raiz)` sob captura, e grava `manifest.json`:
   - por caso: `tabela`, `caso`, `modo_oraculo`, `raiz`, `alvos` (o arquivo plantado **mais** cada `extra_files` que caia numa das seis pastas puras), `tipo_do_alvo` pela **própria** classificação de sufixo do portão (fonte, não-fonte conhecido, desconhecido), `veredito_declarado` (o `verdict` da tabela, **só para agrupar relatório**), `veredito_real` (`passou`/`reprovou`, do retorno de `check_layers()`), `saida_real` (o texto capturado);
   - no topo: `stdlib_permitidos` (`_STDLIB_ALLOWED_NAMES`), `stdlib_banidos`, `gerados` (`_GENERATED_HEADERS`), `camadas_puras` (`_PURE_LAYER_DIR_SPECS`), `total_casos`.
   Pasta de destino não vazia reprova (nunca mistura exportação velha com nova). Escreve em binário; o conteúdo da fixture nunca passa por texto (L-4 R-8, CRLF).
4. **Controle `X2`** (ida e volta): exporta para o scratch e confere que `total_casos` = soma dos tamanhos do registro, e que os bytes de cada alvo plantado são **idênticos** a `case.content`.
5. **Controle `X3`** (veredito real, não declarado): uma tabela **privada do autoteste**, fora do registro, com um `Case` cujo `verdict` é `"passes"` mas o conteúdo é `#include <fstream>\n`; exportada, o manifesto tem de dizer `veredito_real = reprovou`. Mata o mutante que grava o veredito declarado no lugar do real. (A trava X1 enumera só o nível do módulo; a tabela privada vive dentro da função de controle, de propósito.)

### 4.2 No oráculo

Lê só o manifesto e as pastas. **Não** tem lista de cabeçalhos, de camadas, de extensões nem de casos: tudo vem do manifesto. A única coisa que o oráculo sabe por conta própria é **ler saída de compilador**.

**[INFERÊNCIA] Por que isto não é tautologia:** o portão fornece o *dado* (fixture) e o *veredito dele*; o compilador fornece a *verdade*; o oráculo só compara. A lista de nomes permitidos vem do portão, sim, mas ela não é o que está sob teste aqui: ela já é conferida contra a norma pelo controle C12, e o oráculo mapeia nome para **caminho real** pela boca do compilador (seção 7.1), não pela lógica de leitura do portão.

---

## 5. Pergunta 3: o que o oráculo exige, e em qual direção

### 5.1 O que conta como "puxou proibido"

Na árvore da seção 3.2, um nó é **filho direto de arquivo de camada pura** quando o pai é o arquivo compilado ou outro nó cujo caminho real fica dentro de uma das seis pastas puras **da raiz da fixture**. Cada filho direto é classificado, nesta ordem:

1. caminho real dentro das seis pastas puras da fixture → **projeto, permitido**; os filhos dele também são examinados (é arquivo de camada pura);
2. caminho real dentro da pasta de stubs, com nome em `gerados` → **gerado, permitido** (folha);
3. caminho real no conjunto de caminhos padrão aprendido na calibração (7.1) → **padrão, permitido** (folha: o que `<cstdint>` puxa por dentro não interessa, medido em 1.1);
4. qualquer outra coisa → **proibido**.

Isto é lista de permitidos por **caminho**, fechada, no mesmo espírito da D-1 de L-4, mas com o mecanismo do compilador em vez do da leitura de texto.

### 5.2 As duas direções

| Compilador (filhos diretos) | Portão (`veredito_real`) | Balde | Efeito |
|---|---|---|---|
| puxou proibido | reprovou | **concorda-reprova** | ok |
| puxou proibido | **passou** | **VIOLAÇÃO** | **reprova o oráculo** |
| só permitidos, código 0 | passou | **concorda-passa** | ok |
| só permitidos, código 0 | reprovou | **super-aproximação** | relatado, nunca reprova |
| só permitidos, código ≠ 0 | qualquer | **recusado pelo compilador** | relatado com nome, nunca reprova |

**[DECISÃO] A promessa é a direção 1, e só ela** ("compilador puxou proibido ⇒ portão reprovou"). É o que a L-45 diz em letra: *"exige que o portão reprove sempre que o compilador puxar cabeçalho proibido"*. A direção 2 **não pode** ser promessa, por três razões de desenho, todas já declaradas em L-4 e no adendo:

- o portão vê **todos** os ramos (semântica de união); o compilador vê **um**. `#ifdef _WIN32` / `#include <windows.h>` é proibido no portão em todo sistema e só puxa no Windows (L-4 §1.2, c13);
- o portão recusa **formas** que não verifica (inclusão computada, `import`, `module`, `_Pragma`, pragma fora da lista, literal não terminado, codificação estranha) sem precisar que o compilador puxe nada;
- o compilador de um sistema **recusa** arquivos que o de outro aceita (UTF-16 só compila no MSVC, L-4 §1.2).

**[DECISÃO] O julgamento é pelo que foi puxado, nunca pelo código de saída.** Um arquivo que puxa `<fstream>` e depois tem erro de sintaxe **puxou**, e o portão tinha de reprovar. Descartar a saída de `-H` quando o código é ≠ 0 abriria exatamente o buraco que o oráculo fecha. (Controle `O-6` da seção 10.)

**[DECISÃO] O oráculo compara com o veredito REAL do portão, nunca com o declarado na tabela.** Um caso cuja tabela está errada **junto** com o portão (o defeito de memória "teste que copia o valor da implementação") é pego: o compilador não lê a tabela.

### 5.3 A super-aproximação é relatada, não calada

Todo caso no balde super-aproximação ou recusado é impresso com nome e com a **categoria declarada** (o `veredito_declarado`: `reproves_computed`, `reproves_token`, `reproves_directive`, `reproves_lexical`, `reproves_encoding`, `reproves_policy`), com contagem por categoria. O relatório final de fechamento põe as contagens dos seis trabalhos lado a lado. **[INFERÊNCIA]** Isso é o que permite, numa fatia futura, perguntar se um caso `reproves_policy` super-aproximado no Linux é concorda-reprova em algum outro sistema, sem inventar regra agora (seção 11, fora do escopo).

---

## 6. Pergunta 4: em quais trabalhos, e como o desligamento local fica visível

### 6.1 Onde liga

**[FATO-CASA]** Durações do último CI verde (`gh run view 35856728732`, commit `07499e3`): Windows compartilhado 13,0 min, Windows estático 13,2 min, Windows Debug 9,6 min; Fedora compartilhado 7,7, Ubuntu compartilhado 8,2, Arch compartilhado 11,4, CachyOS compartilhado 10,9, Clang compartilhado 8,3. A classe `compilacao` tem teto 40 min pela regra `teto >= 3 x pior caso` (**[FATO-CASA]** `ci.yml:88-117`).

**[DECISÃO]** Opção `-DGLINTFX_LAYERS_ORACLE=ON` em **seis** configurações, uma por compilador distinto, **só no modo compartilhado** (o modo de ligação não muda o pré-processador; rodar nos dois dobraria o custo sem informação nova):

| Trabalho | Compilador | Por quê |
|---|---|---|
| `linux` Fedora compartilhado | g++ do `fedora:latest` | alvo primário |
| `linux` Ubuntu compartilhado | g++-14 | outra versão do GCC |
| `linux` Arch compartilhado | g++ do Arch | alvo |
| `linux` CachyOS compartilhado | g++ do CachyOS | alvo próprio (não é "Arch renomeado") |
| `clang` compartilhado | clang++ | segundo compilador da casa |
| **`windows-debug`** | cl | **não** o `windows` compartilhado: este já está em 13,0 min, e 3 x 13,0 = 39 encosta no teto de 40; o `windows-debug` tem 9,6 min de pior caso, e a classe comporta até 13,3 |

**Consequência para a L-04:** os cinco sistemas perguntam ao próprio compilador, com o mesmo algoritmo; o que difere é o mecanismo (`-H` vs `/showIncludes`), não a pergunta.

### 6.2 Como fica desligado nesta máquina (três camadas, cada uma visível)

1. **Configuração:** `option(GLINTFX_LAYERS_ORACLE ... OFF)` em `cmake/GlintfxOptions.cmake`. O teste `layers_oracle_test` é registrado **sempre** (em todo sistema, sem guarda: paridade de inventário, L-04), e recebe `DISABLED TRUE` quando a opção está desligada. O `ctest` local mostra **"Not Run (Disabled)"** e lista o nome em "The following tests did not run" (doc em 1.2). Nenhum processo do oráculo nasce.
2. **Execução:** mesmo que alguém ligue a opção à mão, o oráculo **recusa** antes de qualquer processo quando `GITHUB_ACTIONS` não é `"true"` (doc em 1.2), com código ≠ 0 e a mensagem: *"oráculo de camadas só roda no CI do servidor (GODS_LAWS.md L-45); nesta máquina ele é desligado por lei"*. Recusar é **falha**, não pulo: ligar o oráculo aqui é violação, e violação é vermelha.
3. **Espelho local:** o `--selftest` do oráculo (que roda aqui, sem compilador) confere que `tools/preci.sh` **não contém** o nome `GLINTFX_LAYERS_ORACLE`. A L-45 diz "nem no `tools/preci.sh`"; aviso em comentário não é portão (memória "aviso no briefing não é portão"). Controle `O-13`.

O `layers_oracle_selftest` (autoteste do oráculo, seção 10) fica **ligado em todo lugar**: não chama compilador por construção, e sem ele o analisador de saída nunca seria exercido fora do servidor.

**Rótulo:** `consume` para o teste (o mesmo do `layers_test`); `selftest` para o autoteste. **Nunca `unit`**: os trabalhos de sanitizador rodam só `-L '^unit$'` e não devem pagar o oráculo.

**Teto do teste no CTest:** `TIMEOUT 900` (15 min), bem abaixo dos 40 do trabalho; travamento vira reprovação rápida, não seis horas.

---

## 7. Pergunta 5: piso de varredura (L-40) e calibração do instrumento

### 7.1 Calibração (um processo por trabalho, antes de qualquer fixture)

Um arquivo gerado no scratch, dentro de uma pasta pura de uma raiz de calibração, com:

- para **cada** nome de `stdlib_permitidos`: `#if __has_include(<nome>)` / `#include <nome>` / `#endif` (ausência de cabeçalho num compilador velho não mata a calibração inteira);
- `#include "sentinela_projeto.hpp"` (arquivo da mesma pasta pura, que por sua vez inclui `<cstdint>`).

Da árvore resultante saem: (a) o **conjunto de caminhos padrão** = caminho real dos filhos diretos cujo nome-base está em `stdlib_permitidos`; (b) no MSVC, o **prefixo** = o texto antes do caminho, na linha que termina no caminho real de `sentinela_projeto.hpp`.

**A calibração reprova o instrumento (falha, nunca pulo) se:**

- `sentinela_projeto.hpp` não aparecer como filho direto (o compilador não está falando, o formato mudou, a leitura quebrou);
- `<cstdint>` não aparecer como filho direto do arquivo de calibração **nem** como filho de `sentinela_projeto.hpp`;
- o conjunto de caminhos padrão tiver **menos de 60** nomes. **Número fixado aqui, antes de qualquer dado (L-43 global):** a norma lista 105 permitidos; nenhuma biblioteca padrão com C++23 minimamente útil fica abaixo de 60; um valor como 3 significa instrumento cego (por exemplo, `-I` quebrado ou `__has_include` falso sempre). O número encontrado é impresso sempre.

### 7.2 Sentinelas por fixture

Duas fixtures sintéticas do próprio oráculo, rodadas depois da calibração e antes das tabelas: `#include <fstream>` (tem de sair **puxou proibido**) e `#include <cstdint>` (tem de sair **só permitidos, código 0**). Qualquer outro resultado: falha de instrumento. É a "régua que distingue os dois estados", medida no próprio trabalho, a cada rodada.

### 7.3 Contagens e pisos

O oráculo imprime **sempre**, mesmo verde, por trabalho: casos exportados, casos por modo (`compilar`/`calibracao`), casos fora de escopo por desenho (alvo que o portão classifica como não-fonte, hoje só `D15_cmakelists_comment_not_scanned`, com nome), arquivos compilados, e a contagem de cada balde da tabela 5.2.

Reprova se:

1. **comparados = 0**;
2. **comparados + fora_de_escopo + calibracao ≠ exportados** (nenhum caso some calado);
3. **puxou proibido (concorda-reprova + violação) < 20** no trabalho. **Fixado agora, antes do dado:** as tabelas têm 62 casos `reproves_policy` e a maioria puxa `<fstream>` em qualquer compilador; 20 é piso folgado que só um instrumento quebrado não alcança;
4. qualquer **VIOLAÇÃO**;
5. qualquer falha de instrumento (calibração, sentinela, salto de profundidade, teto de 60 s, dialeto desconhecido, flag de padrão ausente, manifesto ilegível, pasta de exportação não vazia).

---
## 8. Pergunta 6: critério de fechamento (escrito antes do código, L-43 global) e estreia vermelha

### 8.1 A sub-fatia L-5 só fecha quando TODOS valerem, com a prova de cada um no relatório do implementador

1. **Autoteste do portão verde localmente**, com os três controles novos (`X1`, `X2`, `X3`) presentes, e a contagem total de casos **maior ou igual** à de hoje (334) mais os novos. Nenhum controle antigo perdido.
2. **Autoteste do oráculo verde localmente** (seção 10): os três controles da casa (positivo, negativo, varredura vazia) e os controles `O-1`..`O-15`, **sem nenhum compilador executado**, provado pela guarda de construção da seção 10.1.
3. **Mutação**: cada mutante `M-X1..M-X3` e `M-O1..M-O15` aplicado numa **cópia fora da árvore** (L-27: commitar antes de sabotar, nunca arquivo rastreado), com o autoteste correspondente reprovando **e nomeando o controle esperado**. Registro em `/var/tmp/glintfx-plan/mutacoes-layers-L5.md` (mutante, SHA-base, comando, saída). **Mutante que sobrevive reprova a fatia.**
4. **Desligamento local provado, nas três camadas da seção 6.2**, sem executar o oráculo: (a) numa pasta de build descartável, `ctest -N` e `ctest -R layers_oracle_test` mostram o teste registrado e "Not Run (Disabled)", com o código de saída do `ctest` **lido de variável** (memória "código de saída lido de variável") e registrado, **junto com a resposta medida a "o `ctest -N` lista teste desligado?"** (risco R-1); (b) o controle `O-12` prova a recusa por `GITHUB_ACTIONS`; (c) o controle `O-13` prova a trava do `preci.sh`.
5. **CI verde nos seis trabalhos da seção 6.1**, com o log de cada um mostrando: comando exato usado, flags achadas por família (3.3), calibração (quantos caminhos padrão, prefixo no MSVC), as duas sentinelas, contagens por balde, e o tempo de relógio do oráculo. **E** a lista de trabalhos do último run empurrado comparada com a do `ci.yml` (memória: portão que nunca rodou no ambiente real não é portão).
6. **Zero VIOLAÇÃO nos seis.** Se aparecer violação legítima (o oráculo achou defeito real no portão), **a fatia não fecha "verde por ajuste"**: o defeito vira item novo no INBOX com a fixture e o compilador, e o orquestrador decide se conserta dentro de L-5 ou em L-6. Nunca se afrouxa o oráculo para passar.
7. **Estreia vermelha (8.2) executada e registrada**, com o vermelho esperado em cada trabalho GNU.
8. **Tempo (seção 9):** oráculo ≤ 1 min de relógio em cada trabalho GNU e ≤ 3 min no MSVC; e a duração total do `windows-debug` nesse run, vezes 3, ≤ 40. Estourou qualquer um: **não fecha**; vai ao orquestrador como decisão de CI (seção 13, D-L5-2).
9. **Meta-portões da casa verdes sobre o commit final**, rodados pelo espelho local: `check_selftest_orphan` (o oráculo tem `--selftest` registrado), `check_env_sweep` (o oráculo lê `GITHUB_ACTIONS` e pode citar `\r\n`: cada uso com a declaração exigida por esse portão), `check_spdx`, `check_ci_timeouts` (nenhum trabalho novo; só linhas de configuração mudam), `check_test_parity --selftest`, e o `tools/preci.sh` conforme a memória "espelho local antes do commit".
10. **L-17** no código novo: nenhuma função com mais de 40 linhas, 4 parâmetros ou 3 níveis de aninhamento; as cinco perguntas do revisor respondidas por unidade criada ou crescida.
11. **L-67**: o comentário do topo de `check_layers.py` troca "um oraculo diferencial ... (proposta L-5, D-2 do lider)" em "O QUE FICA FORA" pela descrição do que existe agora (o modo de exportação e para quem ele serve), sem prosa de história.
12. **`TODO.md`**: linha do item com L-5 registrada e `Status` 🔍, no mesmo commit (L-63). Itens de INBOX da seção 11 registrados.

### 8.2 A estreia vermelha (L-36: portão só conta depois de visto reprovando o que deve barrar)

**Mutante escolhido: o defeito do retorno de carro sozinho, `M-CR`.** Em `check_layers.py`, `_LINE_BREAK_SEQUENCE_PATTERN = re.compile(r"\r\n|\r")` vira `re.compile(r"\r\n")`: o portão volta a tratar CR sozinho como caractere comum, e não como fim de linha.

**Previsão escrita agora, antes do dado:**

- **[FATO-CASA, leitura da tabela]** `M3_lone_cr_is_a_real_break` tem conteúdo `auto a = 1;\r#include <fstream>\r`; `M15_three_breaks_lone_cr` tem `int x = 1;\rint y = 2;\r#include <fstream>\r`. Com o mutante, o portão lê cada um como **uma linha só**, que não começa por `#`: **passa**. **[FATO-FONTE]** o GCC aceita CR como fim de linha (1.1): o compilador vê `#include <fstream>` numa linha própria e **puxa**.
- **Esperado nos cinco trabalhos GNU (Fedora, Ubuntu, Arch, CachyOS, Clang):** `layers_oracle_test` **reprova**, com linha de VIOLAÇÃO que nomeia **pelo menos** `M3_lone_cr_is_a_real_break` **e** `M15_three_breaks_lone_cr`, cada uma com o caminho puxado terminando em `fstream`. O Clang está na previsão por **[INFERÊNCIA]** (não achei documento do Clang sobre CR sozinho); se ele não reprovar, isso é **dado** a registrar, não falha da estreia, e o critério 8.1-7 passa a exigir os quatro GCC.
- **`M4` e `M16` não estão na previsão:** `M4` (`\n\r#include`) continua reprovado pelo mutante, porque o `\s` do casamento de diretiva aceita o `\r` inicial; `M16` tem `\r\n` antes do `#include`. Registrar o que acontecer com eles, sem exigir.
- **MSVC (`windows-debug`):** **sem previsão**; o resultado é a primeira medida da casa sobre CR sozinho no compilador da Microsoft, registrada como fato.
- **O que mais fica vermelho, e é esperado:** `layers_selftest` (os casos `M3`, `M4`, `M15`, `M16` e a propriedade de normalização). A estreia prova o oráculo, **não** que o autoteste é cego; o que importa é o oráculo nomear os casos **sozinho**, pela boca do compilador, sem ler a tabela.

**Onde:** ramo descartável (nome sugerido `l5-estreia-cr`) a partir do commit final, com **só** o mutante; CI disparado por `workflow_dispatch` (existe: **[FATO-CASA]** `ci.yml:81`). Depois: log copiado para `/var/tmp/glintfx-plan/estreia-layers-L5.md` (id do run, linhas de violação por trabalho), ramo remoto apagado, `git ls-remote` provando o apagamento. **Ramo em remoto é ação visível: decisão D-L5-1, seção 13.**

**Segunda prova, local e sem compilador:** o controle `O-5` (seção 10) prova que o oráculo usa o veredito **real** e não o declarado; o `X3` prova que o manifesto grava o real. Juntos, fecham o caso "tabela errada junto com o portão", que a estreia no servidor não isola.

---

## 9. Pergunta 7: custo em tempo do CI

**Número de processos por trabalho:** 1 exportação + 1 calibração + 2 sentinelas + **N** fixtures, sequenciais. **[FATO-CASA]** das 309 fixtures, 105 são de C12 (vão pela calibração) e 1 é não-fonte por desenho (`D15`); os alvos extras em pasta pura somam poucos (8 casos de C e 1 de F-neg têm `extra_files`, nem todos em pasta pura). Logo **N ≈ 205** por trabalho. O implementador imprime o N exato.

**Estimativa GNU:** **[FATO-CASA]** 0,01 s de relógio para `-E -H` de um arquivo com `<cstdint>` e `<cstddef>` no g++ 16. **[INFERÊNCIA]** com `<fstream>` o pré-processamento cresce (a árvore do `iostream` é grande), mas continua sendo só pré-processamento: dezenas de milissegundos. ≈ 205 × 0,05 s + partida do Python ≈ **15 s** por trabalho. Teto pré-registrado: **1 min**.

**Estimativa MSVC:** **[INFERÊNCIA, não medido]** a partida do `cl` é o custo dominante, algo como 0,2 a 0,8 s por invocação; ≈ 205 × 0,5 s ≈ **2 min**. Teto pré-registrado: **3 min** no oráculo, e **13,3 min** para o `windows-debug` inteiro (para 3 × pior caso continuar ≤ 40).

**Plano B, se o MSVC estourar (não implementar agora, só se o critério 8.1-8 reprovar):** o `cl` aceita vários arquivos de fonte numa invocação só; com `/showIncludes` a saída de cada arquivo vem depois de uma linha com o nome dele. **[INFERÊNCIA, não documentado na página de `/showIncludes`]**. Agrupar reduziria N a poucas invocações, mas a separação entre arquivos passaria a depender de um formato não prometido; por isso é plano B, e com controle próprio antes de usar.

**Por que isto cabe no `check_ci_timeouts.py`:** nenhum trabalho novo nasce (o portão casa trabalho por trabalho contra a tabela de classes, **[FATO-CASA]** cabeçalho de `check_ci_timeouts.py`); só seis linhas de configuração ganham uma opção. O teto de cada trabalho não muda. O que muda é o **pior caso observado**, e é por isso que o critério 8.1-8 mede o `windows-debug` inteiro, não só o oráculo.

---

## 10. Autoteste do oráculo (roda em todo lugar, inclusive aqui, sem compilador)

### 10.1 Construção que impede compilador no autoteste

O oráculo recebe o "executor de compilador" como **parâmetro** (injeção). O modo `--selftest` passa um executor **falso** que devolve saídas enlatadas; o executor real **levanta exceção** se for chamado com a marca de autoteste ativa. Controle `O-0`: chamar o executor real dentro do autoteste tem de reprovar. **As saídas enlatadas vêm de três fontes, cada uma rotulada no código:** a medição única de 1.1 (verbatim, com a lista de guardas traduzida); o formato documentado pelo GCC e pela Microsoft (1.1, citados com URL); e variações escritas à mão para os casos de borda (rotuladas "sintética").

### 10.2 Controles, cada um com o mutante que mata

| Controle | O que prova | Mutante morto |
|---|---|---|
| positivo | fixture limpa, compilador só puxa permitido, portão passa → verde | (nenhum: é o controle que prova que o oráculo não reprova tudo) |
| negativo | compilador puxa `<fstream>`, portão passou → VIOLAÇÃO | M-O-neg: comparação desligada |
| varredura vazia | manifesto com zero casos → reprova "0 comparados" | M-O7: zero comparadas passa |
| O-1 | lista de guardas traduzida no fim do `-H` é ignorada | M-O1: aceita linha sem pontos |
| O-2 | proibido na profundidade 2 **sob** cabeçalho padrão não conta; na 1, conta | M-O2: profundidade deslocada em um |
| O-3 | filhos de cabeçalho padrão não são examinados (`sys/cdefs.h` sob `<cstdint>`) | M-O3: pai perdido, tudo vira filho direto |
| O-4 | filho de cabeçalho **de camada pura** é examinado (`x.hpp` puro puxa `<fstream>`) | M-O4: só profundidade 1 literal |
| O-5 | manifesto com `veredito_declarado = reproves_policy` e `veredito_real = passou`, compilador puxa `<fstream>` → VIOLAÇÃO | M-O5: compara com o declarado |
| O-6 | código de saída ≠ 0 **com** `<fstream>` puxado, portão passou → VIOLAÇÃO | M-O6: descarta saída quando o código é ≠ 0 |
| O-7 | salto de profundidade (1 → 3) → falha de instrumento | M-O7b: salto aceito |
| O-8 | calibração sem a sentinela de projeto → falha de instrumento; com menos de 60 caminhos padrão → falha | M-O8: calibração não conferida |
| O-9 | MSVC com prefixo traduzido (sintético, ex.: francês) → lido pelo prefixo aprendido | M-O9: prefixo inglês fixo |
| O-10 | MSVC aninhado, um espaço por nível → profundidade certa | M-O10: conta espaço errado |
| O-11 | MSVC, caixa diferente entre calibração e fixture → mesmo caminho | M-O11: sem `normcase` no dialeto MSVC |
| O-12 | modo real sem `GITHUB_ACTIONS=true` → código ≠ 0, mensagem cita L-45, executor **nunca** chamado | M-O12: guarda removida |
| O-13 | `preci.sh` sintético contendo `GLINTFX_LAYERS_ORACLE` → reprova; o `tools/preci.sh` real → passa, com o caminho impresso | M-O13: trava removida |
| O-14 | portão reprovou, compilador só permitido → balde super-aproximação, **verde** | M-O14: direção 2 vira falha |
| O-15 | caso não-fonte e casos de calibração aparecem na contagem, com nome; soma bate com exportados | M-O15: caso some calado |
| O-16 | flags: `compile_commands` sem padrão da língua → reprova; com `/Zc:preprocessor` → repassado e impresso | M-O16: padrão ausente aceito |
| O-17 | dialeto desconhecido (`"Intel"`) → reprova, nomeado | M-O17: cai no GNU por omissão |

Mais os três controles do portão da seção 4 (`X1`, `X2`, `X3`) no `--selftest` de `check_layers.py`.

**Regra que vale para toda a bateria (memória "trava tautológica"):** nenhum número esperado é derivado da constante que ele confere. O piso de 60 caminhos e o de 20 puxadas são literais escritos à parte.

---

## 11. Escopo fechado: o que o implementador NÃO pode fazer

- **Não rodar o oráculo nesta máquina**, nem "só desta vez", nem em cópia, nem em contêiner local (L-45: *"O oráculo não roda aqui: nem no `tools/preci.sh`, nem em agente"*). Configurar e listar (`cmake`, `ctest -N`) numa pasta descartável é permitido; **executar** `layers_oracle_test` não é, nem com a opção ligada (a guarda 6.2-2 recusaria, e a recusa é a prova, não um convite).
- **Não chamar compilador em lugar nenhum desta máquina**, nem no autoteste (10.1). Saída enlatada é o único jeito.
- **Não paralelizar** as invocações no servidor.
- **Não ligar a opção** em nenhum trabalho além dos seis da 6.1, **nem** no `tools/preci.sh`, **nem** nos trabalhos que rodam o `preci.sh` no servidor (`lint`, `sanitizer`, `debug`).
- **Não mudar a lógica de leitura do portão** (fases 2/3, macros, política). L-5 só acrescenta registro, exportação e três controles. Violação que o oráculo achar vai ao INBOX (8.1-6).
- **Não copiar fixture para o oráculo** nem escrever lista de cabeçalhos nele.
- **Não usar `-M`/`-MM`/`/E`** (1.1).
- **Não usar pacote de terceiro** (L-07): `json`, `subprocess`, `os`, `re`, `tempfile` da biblioteca padrão bastam.
- **Não tocar** arquivo fora da lista da seção 2.
- **Não declarar "confirmado"** o que é [INFERÊNCIA] neste plano (formato do Clang, `/P` com `/showIncludes`, custo do MSVC, CR no MSVC) antes de ver no log do servidor.

**Fora desta fatia, para o INBOX (o orquestrador registra):**

- `ORACULO-CAMADAS-DIRECAO-2-ENTRE-SISTEMAS`: cruzar, no trabalho de paridade, os casos `reproves_policy` super-aproximados num sistema com os concorda-reprova de outro. Hoje só relatado (5.3).
- `ORACULO-CAMADAS-EMBED-INCBIN`: o instrumento só vê **inclusão de cabeçalho**. `#embed`, `.incbin` de `asm` e unidade de cabeçalho por `import` puxam conteúdo sem aparecer em `-H`/`/showIncludes` (**[INFERÊNCIA]** para `#embed`; os três já são barrados pelo portão por diretiva ou por token, adendo D-A1/B). Ponto cego declarado do oráculo, não do portão.
- `ORACULO-CAMADAS-CL-LOTE`: o plano B da seção 9, se o custo pedir.

---

## 12. Riscos

| # | Risco | Mitigação |
|---|---|---|
| R-1 | **`ctest -N` pode não listar teste desligado.** O trabalho `parity` une os inventários `ctest -N` por sistema (**[FATO-CASA]** `ci.yml:1073-1130`: `parity-inv-linux-*` e `parity-inv-windows-*`, que vêm dos trabalhos `linux`, `windows`, `wayland-container` e `lint`, **não** do `windows-debug`). Se desligado some do `-N`, o Linux teria o nome (ligado) e o Windows não (desligado nos trabalhos que entram no inventário) → **paridade vermelha**. | Medir primeiro (8.1-4a). Se some: ligar o MSVC no `windows` compartilhado em vez do `windows-debug` **e** trazer a decisão de teto ao orquestrador (D-L5-2), ou registrar o par em `tests/parity_aliases.txt`/exceção com o motivo. Não decidir às cegas. |
| R-2 | Formato do `-H` do Clang diferente do GCC. | Calibração e sentinelas reprovam o instrumento; nunca passa calado. |
| R-3 | `/showIncludes` não emite com `/P`. | Mesma calibração; troca por `/Zs` (1.1). |
| R-4 | Custo do MSVC acima do previsto. | Critério 8.1-8 e plano B (9). |
| R-5 | Compilador de uma distribuição sem algum cabeçalho C++23 (`<flat_map>`, `<stdfloat>`...). | `__has_include` na calibração; caso de fixture que o use cai em "recusado", relatado. |
| R-6 | Caminho com caractere fora do ASCII na saída do `cl` (página de código do console). | Scratch com nome só ASCII (conferido e recusado se não for); decodificação com substituição, nunca exceção. |
| R-7 | O oráculo acha violação real na primeira rodada. | É o motivo de existir. 8.1-6: vira item, não afrouxamento. |
| R-8 | Alguém copia a linha de configuração de um trabalho com a opção para o `preci.sh`. | Controle `O-13` roda em todo `ctest` local e no servidor. |
| R-9 | Exportação e autoteste divergem na construção da fixture. | Os dois chamam a mesma `_plant_case_fixture()`; `X2` confere byte a byte. |
| R-10 | A trava `X1` acha tupla de `Case` que não é tabela de autoteste (ex.: a privada do `X3`). | `X1` enumera só o nível do módulo; tabela privada vive dentro de função, de propósito, e o comentário diz por quê. |

---

## 13. Decisões

**Técnicas deste plano (executáveis sem nova consulta), com a razão ao lado de cada uma no corpo:** arquivo separado (2); `-E -H` e `/P /showIncludes` (1.1, 3.1); flags lidas de `compile_commands.json` por família fechada (3.3); registro `_CASE_TABLES` com trava de enumeração (4.1); C12 pela calibração (4.1); direção 1 dura e direção 2 relatada (5.2); julgamento pelo puxado, não pelo código de saída (5.2); comparação com o veredito real (5.2); seis trabalhos, só modo compartilhado (6.1); três camadas de desligamento (6.2); pisos 60 e 20 (7); estreia com `M-CR` (8.2).

**Para o orquestrador decidir (nenhuma é de produto; nenhuma muda o que a biblioteca aceita ou entrega):**

| # | Decisão | Recomendação | Por quê | Se recusada |
|---|---|---|---|---|
| **D-L5-1** | Autorizar o **ramo descartável em remoto** para a estreia vermelha (8.2), apagado em seguida | **Sim** | L-36 exige ver o portão reprovar; o único lugar onde o oráculo pode rodar é o servidor (L-45); pôr o mutante no `main` é inaceitável | A estreia fica só local (controles `O-*` com saída enlatada), e o critério 8.1-7 cai para "não provado no servidor", declarado |
| **D-L5-2** | Se o critério de tempo (8.1-8) estourar: plano B (`cl` em lote) **ou** reclassificar o teto do `windows-debug` (CI-TIMEOUTS) | Plano B primeiro | Não mexe em teto calibrado por medição | Oráculo MSVC desligado até decidir; os cinco GNU continuam |
| **D-L5-3** | Se o R-1 se confirmar: MSVC no `windows` compartilhado (e aí o teto aperta) **ou** declarar o par no mecanismo de paridade | Declarar no mecanismo de paridade | O `windows` compartilhado está a 1 min do teto | Idem D-L5-2 |

---

## 14. Leis aplicadas neste plano

- **L-45 (projeto), inteira:** nada do oráculo rodou aqui; o desenho o desliga em três camadas visíveis (6.2) e ainda prova o desligamento no `ctest` local; a exceção à L-11 global é usada só no servidor e sem paralelismo (3.1).
- **L-40:** contagens sempre impressas; zero comparadas reprova; soma que fecha; pisos literais; controle de varredura vazia no autoteste; enumeração fechada de dialeto, de família de flag e de tabelas do módulo.
- **L-42/L-43:** pesquisa antes do plano, cada fato com URL, separada do que foi medido aqui; o que não achei está declarado (Clang sem formato documentado, variável interna do CMake, CR no MSVC, prior art).
- **L-29:** nenhum código de terceiro lido; só documentação.
- **L-04:** mesma pergunta nos cinco sistemas, mecanismo por compilador; normalização por argumento, não por `sys.platform`; teste registrado sem guarda de sistema.
- **L-07:** só biblioteca padrão do Python; compilador que já está no CI.
- **L-17:** arquivo separado em vez de inchar o portão; limites de função no critério.
- **L-27:** fato, fonte, inferência e decisão rotulados; mutação em cópia fora da árvore.
- **L-36:** estreia vermelha no servidor, com previsão escrita antes.
- **L-43 global:** critério e pisos escritos antes de qualquer dado.

**Porte (L-08, sem prazo):** um arquivo novo (o oráculo) e cinco tocados; no portão, um registro, um modo de exportação e três controles; no oráculo, leitura de duas saídas de compilador, calibração, comparação e relatório, com os três controles da casa mais `O-0` a `O-17` no autoteste, cada um com o mutante que mata (tabela 10.2); seis linhas de configuração no CI; uma estreia no servidor; três decisões condicionais para o orquestrador.
