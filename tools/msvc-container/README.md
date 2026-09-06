# Container com o cl.exe REAL da Microsoft

**Ferramenta de desenvolvimento local. Não é dependência do produto**
(GODS_LAWS.md L-07: o GlintFx continua com dependência zero além da stdlib
e da API do SO). Isto nunca é linkado nem distribuído com a biblioteca, e
não substitui o CI real: o job `windows` do `.github/workflows/ci.yml`
continua sendo a única prova de execução de verdade, em `windows-latest`.

## O que isto é

O compilador `cl.exe` real da Microsoft (versão medida:
`19.51.36256`, toolset MSVC `14.51.36231`, Windows SDK `10.0.26100.0`),
rodando sob Wine dentro de um container Linux, via o projeto
[mstorsjo/msvc-wine](https://github.com/mstorsjo/msvc-wine). Os
componentes vêm do canal público de release da Microsoft (o mesmo que o
instalador do Visual Studio usa) e a licença deles é aceita no download
(`--accept-license`, ver "Licença" abaixo).

**Por que isto existe:** sem ele, todo defeito do lado Windows deste
projeto só aparece depois de uma rodada inteira no `windows-latest` do
GitHub Actions. Nesta onda (06/09/2026) foram três rodadas gastas no
mesmo teste, e o compilador cruzado que já existe nesta máquina
(`x86_64-w64-mingw32-g++`, do projeto GNU) aceita coisas que o
compilador real recusa ou avisa (ver a seção "A prova" abaixo).

## O que isto NÃO é, e nunca deve ser vendido como tal

- **Não abre janela, não minimiza, não restaura.** Não executa nenhum
  binário Windows, só compila (`/c`, sem link nem execução).
- **Não tem placa de vídeo real nem driver de verdade.** WGL sob Wine,
  quando roda, cai em Mesa/llvmpipe por software, igual ao `windows-latest`
  do CI, mas não é a mesma coisa que a máquina de um jogador de verdade.
- **`/analyze` (PREfast) não funciona nesta imagem**, falha com
  `warning C28297: ... File not found` e `fatal error C1253` porque o
  conjunto mínimo baixado (ver "Como foi construída") não inclui os
  arquivos de modelo do PREfast. Não investigado a fundo (custaria mais
  download). Sem `/analyze`, o `cl.exe` sozinho só emite os avisos nativos
  dele (a família C4xxx), nunca os checks de `clang-tidy`
  (`misc-misplaced-const`, `bugprone-exception-escape` etc.) que pegaram
  os dois achados reais desta onda, ver "O que isto NÃO pegou" abaixo.
- **A única prova de execução real continua sendo o job `windows` do CI**
  (`windows-latest`, GitHub Actions).

## Como usar (2 passos, um deles já feito)

### Passo 1: construir a imagem base (rápido, ~1,5 GB, sem licença
envolvida, só Debian + Wine + Python + o clone do msvc-wine)

```bash
docker build -t glintfx-msvc-base:latest -f tools/msvc-container/Dockerfile tools/msvc-container/
```

### Passo 2: baixar os componentes MSVC (feito nesta sessão; refazer só
se quiser atualizar a versão do MSVC, ou se a imagem final for apagada)

```bash
DIR=/algum/diretorio/de/trabalho   # fora do repo; ficou ~4 GB
mkdir -p "$DIR/opt-msvc" "$DIR/cache"

docker run --rm \
  -v "$DIR/opt-msvc:/opt/msvc" -v "$DIR/cache:/cache" \
  glintfx-msvc-base:latest \
  python3 /opt/msvc-wine/vsdownload.py --accept-license \
    --cache /cache --dest /opt/msvc \
    --architecture x64 \
    --with-workload no --with-msvc yes --with-sdk yes \
    --with-atl no --with-asan no --with-dia no \
    --with-msbuild no --with-devcmd no

docker run --rm -v "$DIR/opt-msvc:/opt/msvc" glintfx-msvc-base:latest \
  bash -c 'cd /opt/msvc-wine && ./install.sh /opt/msvc'

docker build -t glintfx-msvc:latest -f tools/msvc-container/Dockerfile.full "$DIR"
```

**Por que este conjunto de flags e não o default do `vsdownload.py`:** o
default (`--with-workload yes` implícito) puxa o workload `VCTools`
inteiro (WebTools, SQLCommon, WebView2, IntelliSense de 12 idiomas):
569 pacotes, 2,2 GB de download, 6,7 GB instalado. O conjunto acima
(arquitetura só x64, sem ATL/ASAN/DIA/MSBuild/DevCmd) cai para **79
pacotes, 1,0 GB de download, 3,0 GB instalado**, e ainda assim compila
C++23 real do projeto (ver "A prova" abaixo). A maior fatia do download é
o próprio Windows SDK (`Win11SDK_10.0.26100`, 1,6 GB); não dá para cortar
mais sem perder `<windows.h>`/`<objbase.h>` que o backend `win32` do
GlintFx usa.

### Uso do dia a dia: a linha que importa

```bash
docker run --rm -v "$(pwd):/src:ro" glintfx-msvc:latest bash -c '
  cl /nologo /std:c++latest /Zc:__cplusplus /EHsc /W4 /c \
    /I /src/include /I /src/build-preci/generated/include /I /src/src \
    /DGLINTFX_LIBRARY_STATIC_DEFINE /D_WIN32=1 /DWIN32=1 /D_WIN32_WINNT=0x0A00 \
    /Fo/tmp/saida.obj \
    /src/src/platform/win32/<ARQUIVO>.cpp
'
```

Troque `<ARQUIVO>` pelo `.cpp` que quer checar. `/DGLINTFX_LIBRARY_STATIC_DEFINE`
evita um problema de ferramenta (não do produto): o `export.hpp` gerado em
`build-preci/generated/include` foi gerado por um `cmake` configurado com
GCC, e o `GenerateExportHeader` do CMake baixa `__attribute__((visibility))`
incondicionalmente nessa combinação; o `cl.exe` real não entende essa
sintaxe. Isto não é um defeito do GlintFx: um `cmake` configurado de
verdade com MSVC gera a variante `__declspec`. Passar a definição de build
estático evita as duas variantes e testa exatamente o que os arquivos do
projeto escrevem.

**Achado desta sessão sobre o nome do flag de padrão da linguagem, não
documentado nos manuais do msvc-wine:** este toolset (MSVC 14.51, VS 18.9)
aceita `/std:<c++14|c++17|c++20|c++latest>`. Não existe um `/std:c++23`
literal nele. `/std:c++23` é silenciosamente ignorado (sem erro nem
aviso visível), e o compilador cai no padrão default (pré-C++17), o que
produz uma cascata de erros que nada tem a ver com o código real (o
primeiro sintoma foi `error C2429: language feature
'nested-namespace-definition' requires compiler flag '/std:c++17'`). O
flag certo para C++23 nesta imagem é `/std:c++latest`, usado em todos os
comandos deste documento.

### Prova de que o cl.exe real compila código de verdade do projeto

```
$ docker run --rm -v "$(pwd):/src:ro" glintfx-msvc:latest bash -c '
    cl /nologo /std:c++latest /Zc:__cplusplus /EHsc /W4 /c \
      /I /src/include /I /src/build-preci/generated/include /I /src/src \
      /DGLINTFX_LIBRARY_STATIC_DEFINE /D_WIN32=1 /DWIN32=1 /D_WIN32_WINNT=0x0A00 \
      /Fo/tmp/wgl_extension_loader.obj \
      /src/src/platform/win32/wgl_extension_loader.cpp
    echo "EXIT_CODE=$?"
  '
wgl_extension_loader.cpp
EXIT_CODE=0
```

Versão exata usada (impressa por `cl` sem argumentos):

```
Microsoft (R) C/C++ Optimizing Compiler Version 19.51.36256 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.
```

## A prova: o que o real pega e o alternativo não

`tools/msvc-container/armadilhas/compare.sh` roda os dois lados
(`x86_64-w64-mingw32-g++`, já presente nesta máquina, e o `cl.exe` real
desta imagem) contra quatro arquivos mínimos, e imprime a saída literal
dos dois. Resultado medido (06/09/2026):

| Arquivo | Armadilha reproduzida | MinGW cruzado | cl.exe real (`/W4`) |
|---|---|---|---|
| `crt_inseguro.cpp` | `strcpy`/`sprintf` sem a variante `_s` | aceita, sem aviso nenhum (`-Wall -Wextra`) | avisa as duas (`C4996`: "Consider using strcpy_s/sprintf_s instead") |
| `setvbuf_borda.cpp` | `setvbuf(..., _IOLBF, 0)`, commit real `0628ad8`, matava o processo no runtime da Microsoft (tamanho 0 é inválido para `_IOLBF`/`_IOFBF`; o parâmetro inválido aciona um handler que ENCERRA o processo, em vez de devolver erro) | aceita, sem aviso | também aceita, sem aviso nenhum, mesmo com `/W4` |
| `const_mal_posicionado.cpp` | `const HINSTANCE module = ...`, commit real `babbd77`, achado por `clang-tidy misc-misplaced-const` no runner real | aceita, sem aviso | também aceita, sem aviso, mesmo com `/W4` |
| `noexcept_pode_lancar.cpp` | função `noexcept` que constrói `std::string` alocante, commit real `babbd77`, achado por `clang-tidy bugprone-exception-escape` no runner real | aceita, sem aviso | também aceita, sem aviso, mesmo com `/W4` |

**Resultado negativo honesto, como pedido:** dos dois achados reais que
motivaram este pedido (a anotação de constante mal posicionada e a
exceção escapando de função `noexcept`), o `cl.exe` sozinho não pegou
nenhum dos dois, nem no `/W4` que o CI real usa. A tentativa de usar
`/analyze` para dar ao `cl.exe` a mesma capacidade de análise estática
falhou por falta dos arquivos de modelo do PREfast nesta imagem mínima
(ver "O que isto NÃO é" acima); não foi testado se instalar o pacote de
análise estática completo (mais download) resolveria, porque os dois
achados reais vieram do `clang-tidy`, uma ferramenta separada do `cl.exe`,
rodando no job `lint` do `windows-latest`, não do compilador em si. Esta
imagem prova o compilador; não substitui o `clang-tidy` do lado Windows,
que é quem efetivamente pegou os dois achados.

**O que ela prova de verdade:** o `cl.exe` real tem sua própria família
de avisos (C4996 acima é um exemplo medido) que o compilador cruzado GNU
nunca vai emitir, porque eles vêm de anotações dos cabeçalhos da própria
Microsoft (`_CRT_INSECURE_DEPRECATE`) que o MinGW não usa. Ela também
prova (achado da própria sessão, não pedido) que o nome do flag de padrão
de linguagem difere (`/std:c++latest`, não `/std:c++23`) e que erros de
sintaxe genuínos de C++ que o `cl.exe` recusa (mensagens `C2xxx`) agora
aparecem localmente, em vez de só depois de uma rodada inteira no
servidor.

Para reproduzir: `tools/msvc-container/armadilhas/compare.sh` (precisa da
imagem `glintfx-msvc:latest` já construída e do `x86_64-w64-mingw32-g++`
já presente nesta máquina).

## Tamanho medido (06/09/2026)

| O quê | Tamanho |
|---|---|
| `glintfx-msvc-base:latest` (Debian+Wine+Python, sem MSVC) | 1,52 GB |
| Download dos componentes MSVC (cache) | 974 MB |
| Componentes MSVC instalados (`/opt/msvc`) | 3,1 GB |
| `glintfx-msvc:latest` (imagem final, autocontida) | 6,63 GB |

Disco do host (`btrfs filesystem usage /`, nunca `df`, GODS_LAWS.md
L-11): teto declarado para esta tarefa era 15 GB antes de avisar; o uso
real ficou nos ~8 GB combinados das duas imagens Docker (`glintfx-msvc-base`
mais `glintfx-msvc`) mais os ~4 GB do cache/dest fora da imagem (que podem
ser apagados depois de construída a imagem final, já que o conteúdo dela
já está copiado para dentro; `rm -rf` desse diretório de trabalho, fora
do repo, é seguro e recupera o espaço).

## Licença

O README do `mstorsjo/msvc-wine`, citado verbatim: "Downloading and
installing it requires accepting the license" (aceita via
`--accept-license` acima, sob a licença pública da Microsoft para o canal
de release do Visual Studio) e "As Visual Studio isn't redistributable,
the installed toolchain isn't either." Consequência prática: as imagens
`glintfx-msvc-base` e `glintfx-msvc` são para uso local, desta máquina,
deste time, nunca publicadas em registry nem distribuídas a terceiros.

## O que fica para o líder decidir, se algum dia importar

- Se vale a pena baixar o pacote de análise estática completo do MSVC
  para o `/analyze` funcionar (mais download, tamanho não medido).
- Se vale fixar o `msvc-wine` num commit/tag específico em vez do
  `git clone --depth 1` da `master` (hoje reproduzível só enquanto a
  `master` de lá não mudar sob o agente, GODS_LAWS.md L-36, "CI local vs.
  execução real": mudança silenciosa a montante quebraria isto sem
  aviso).
