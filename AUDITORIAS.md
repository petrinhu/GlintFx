# Auditorias Técnicas (GlintFx)

> **Precedência:** [`GODS_LAWS.md`](GODS_LAWS.md) vence este guia em qualquer conflito. Manuais
> irmãos: [`CONTRACT.md`](CONTRACT.md) (padrão de código), [`TESTES.md`](TESTES.md) (como testar;
> este documento não repete comando de teste, só diz **o que uma auditoria confere** e **como**).
> Nenhuma entrada abaixo aponta para outro repositório: o GlintFx audita a si mesmo.

Este documento foi reescrito do zero em 22/08/2026. A versão anterior tinha 12 seções no índice e
zero no corpo, prometia auditoria de MySQL, Qt6 e PHP — tecnologias que este projeto não usa e a
L-07 proíbe — e apontava wikilinks para outros projetos do vault. Era um portão que varre zero e
declara verde. Este documento só promete o que audita de fato.

**Quem audita (L-18, L-12):** sempre um C-level em modelo `fable` (o `internal-auditor`, ou o
CTO/CISO/CLO conforme o assunto), nunca a thread principal, nunca um agente genérico. O agente
que implementou a fatia não audita a própria fatia. O orquestrador reconfere o entregável do
auditor (build limpo, spot-check das afirmações contra arquivo e linha) antes de aceitar —
relatório de agente não é prova.

**Escala de severidade**, a única classificação usada em qualquer achado deste projeto:
**🔴 CRÍTICO · 🟠 IMPORTANTE · 🟢 COSMÉTICO**. Achado sem evidência (arquivo e linha, ou saída de
comando reproduzível) não entra no dossiê. 🔴 nunca fica sem plano de remediação.

**Estado do repositório em 22/08/2026, para calibrar o que segue:** existe fundação de build
(CMake, `src/core/version.hpp`/`.cpp`, harness de teste, e os gates de `tests/tools/`), mas
**nenhum backend de janela, mapa ou RCSS existe ainda**. Vários capítulos abaixo auditam código que
ainda não nasceu; eles ficam **pré-registrados** com o critério fixado agora (L-27: critério antes
do dado), para aplicar no primeiro commit da fatia correspondente, não depois.

---

## Índice

1. [Dependência zero](#1-dependência-zero-l-07)
2. [Camadas e atomização](#2-camadas-e-atomização-l-17-l-19)
3. [Fronteira pública, ABI e erro](#3-fronteira-pública-abi-e-erro-l-19-l-22)
4. [Os três contratos de versão: API, ABI, DADO](#4-os-três-contratos-de-versão-api-abi-dado-l-26)
5. [Wayland puro, sem X11](#5-wayland-puro-sem-x11-l-05)
6. [Isolamento de teste de superfície](#6-isolamento-de-teste-de-superfície-l-09)
7. [Licença e proveniência](#7-licença-e-proveniência-l-08-l-29)
8. [Entrada hostil: mapa e RCSS](#8-entrada-hostil-mapa-e-rcss-l-28-l-30)
9. [Portões automáticos de qualidade: estado hoje](#9-portões-automáticos-de-qualidade-estado-hoje-l-23)
10. [Dossiê formal pré-1.0](#10-dossiê-formal-pré-10-l-24)

---

## 1. Dependência zero (L-07)

🔴 **CRÍTICO**

- Nenhum `FetchContent_Declare`/`FetchContent_MakeAvailable` em `CMakeLists.txt` nem em `cmake/`.
  Comando de auditoria: `grep -rn "FetchContent" CMakeLists.txt cmake/ src/*/CMakeLists.txt`.
- Nenhum `find_package` além do exigido pela L-05 (`Wayland`, via `pkg_check_modules` de
  `wayland-client`/`wayland-protocols`). Qualquer `find_package` novo é achado até o líder aprovar
  a exceção.
- Nenhum `#include` de header de terceiro fora de stdlib e API do SO em `src/` ou `include/`
  (nada de `<SDL.h>`, `<GL/glfw3.h>`, `<xkbcommon/xkbcommon.h>`, `<freetype/...>`). `libwayland`
  conta como API do SO (L-05/L-07); `libxkbcommon` **não conta**, mesmo instalado na máquina
  (L-06).
- Nenhum diretório `vendor/`, `third_party/` ou `external/` no repositório **fora da EXCEÇÃO Nº 1
  abaixo**. A regra continua valendo por padrão: diretório vendorizado é achado CRÍTICO, a menos
  que o conteúdo bata exatamente com a exceção enumerada.

**EXCEÇÃO Nº 1 (L-07, aberta pelo líder em 26/08/2026): `third_party/khronos/`.** Texto de
`GODS_LAWS.md` L-07, citado: *"um arquivo de DADO, lido em tempo de build por script nosso, nunca
linkado ao binário [...] e a exceção não se estende a mais nada"*. Os únicos caminhos que ela cobre,
enumerados um a um, não descritos por padrão de nome:

1. `third_party/khronos/gl.xml`: o arquivo vendorizado em si, o registro `gl.xml` do Khronos Group,
   licença Apache-2.0.
2. `third_party/khronos/LICENSE-APACHE-2.0.txt`: o texto integral da licença que acompanha o
   arquivo, obrigação 1 da exceção (`GODS_LAWS.md` L-07: "o texto integral da Apache-2.0 entra
   junto do arquivo").
3. `third_party/khronos/README.md`: a proveniência escrita (URL, commit de origem, data, `sha256`),
   obrigação 4 da exceção. Este arquivo não é conteúdo vendorizado, é texto AGPL-3.0-or-later do
   próprio projeto; entra na enumeração porque mora no mesmo diretório e o auditor precisa saber
   que ele também é esperado, não um achado.

Qualquer outro arquivo, seja dentro de `third_party/khronos/`, seja em qualquer outro diretório
vendorizado, continua CRÍTICO sem exceção nenhuma. **A exceção não é transitiva**: não autoriza um
segundo arquivo vendorizado nem um segundo diretório, mesmo que a justificativa pareça análoga; isso
é decisão do líder, de novo, não extrapolação de agente.

**Comando de auditoria corrigido**, que distingue os dois casos em vez de gritar nos dois. O comando
antigo (`find . -iname "*vendor*" -o -iname "*third_party*" -o -iname "*external*"`) achava só o
diretório `./third_party`, sem entrar nele; hoje ele reportaria um CRÍTICO falso para a exceção
legítima. Testado contra o repositório real em 26/08/2026, HEAD `95c0f20a3f3be9d3c245266d7783b88aca44a657`:

```bash
allowlist="third_party/khronos/gl.xml
third_party/khronos/LICENSE-APACHE-2.0.txt
third_party/khronos/README.md"
matched=$(find . -iname "*vendor*" -o -iname "*third_party*" -o -iname "*external*" 2>/dev/null \
    | grep -v '^\./\.git' || true)
files=""
for p in $matched; do
    if [ -d "$p" ]; then files="$files
$(find "$p" -type f)"; else files="$files
$p"; fi
done
files=$(printf '%s\n' "$files" | sed 's#^\./##' | grep -v '^$' | sort -u)
count=$(printf '%s\n' "$files" | grep -c . || true)
echo "varreu: $count arquivo(s)"
[ "$count" -eq 0 ] && { echo "REPROVADO: varredura vazia (L-40)"; exit 1; }
critico=0
while IFS= read -r f; do
    printf '%s\n' "$allowlist" | grep -qxF "$f" \
        && echo "OK (EXCECAO No 1): $f" \
        || { echo "CRITICO: $f"; critico=1; }
done <<EOF
$files
EOF
[ "$critico" -eq 1 ] && exit 1 || exit 0
```

Saída real, obtida rodando o comando acima contra a árvore, colada sem edição:

```
varreu: 3 arquivo(s)
OK (EXCECAO No 1): third_party/khronos/gl.xml
OK (EXCECAO No 1): third_party/khronos/LICENSE-APACHE-2.0.txt
OK (EXCECAO No 1): third_party/khronos/README.md
```

`exit 0`. Os três controles da L-40 foram testados neste mesmo dia contra uma árvore de fixture em
`/var/tmp` (fora deste repositório): positivo (só a exceção, aprova), negativo (um `vendor/alien.hpp`
estranho ao lado, reprova, `CRITICO: vendor/alien.hpp`), e varredura vazia (nenhum caminho casado,
reprova, `REPROVADO: varredura vazia`).

**Sobre reprovar em zero (L-40), e por que isso é uma escolha e não um acidente:** enquanto a
EXCEÇÃO Nº 1 estiver aberta, os três arquivos acima são o estado esperado da árvore; uma varredura
que não os encontra não é "dependência zero restaurada", é o build quebrado (o codegen de OpenGL
depende de `gl.xml` existir, `src/render/CMakeLists.txt`) ou a exceção sendo perdida em silêncio.
Por isso este comando reprova em zero, ao contrário do comando antigo, que tratava zero como
sucesso. Se o líder revogar a EXCEÇÃO Nº 1 algum dia, este item e este comando precisam de
atualização consciente no mesmo commit que remove `third_party/khronos/`, do mesmo jeito que a
L-26 já registra que `SameMinorVersion` vira `SameMajorVersion` na 1.0: mudança de critério datada,
nunca deriva silenciosa.

**Prova de integridade, para o auditor conferir que a exceção não é só palavra escrita no README:**
o `sha256` do `gl.xml` vendorizado é recomputado a cada `configure` e comparado contra o valor
gravado em `third_party/khronos/README.md`, em `src/render/CMakeLists.txt` (o `file(SHA256 ...)`
perto da linha 34, comparado contra `GLINTFX_GL_XML_EXPECTED_SHA256` perto da linha 45); diverge,
o `configure` falha. Ligado à suíte, não só ao build: `tests/CMakeLists.txt` roda os três controles
da L-40 no nível do processo, via `gl_registry_codegen` (código de saída, `WILL_FAIL TRUE` do CTest
para os dois que devem falhar):

  - `gl_registry_codegen_real_vendored_file_succeeds_test`: positivo, o arquivo real com o `sha256`
    real passa.
  - `gl_registry_codegen_sha_mismatch_reproves_test`: negativo, `--expect-sha256` forjado reprova
    (obrigação 3 da exceção: o arquivo é verbatim, uma edição muda o hash e o build para).
  - `gl_registry_codegen_empty_registry_reproves_test`: varredura vazia, um `gl.xml` bem formado que
    resolve zero funções OpenGL reprova, nunca passa em silêncio.

  Confirmado nesta data, lendo `tests/CMakeLists.txt`: os três existem e rodam. Não escrevo "isto é
  testado" sem antes ver o portão reprovar o caso que promete cobrir (L-40, corolário sobre
  alegação); aqui a leitura do arquivo é a evidência, e `WILL_FAIL TRUE` é a prova de que os dois
  negativos são esperados para falhar, não bugs.

🟠 **IMPORTANTE**

- Ferramenta de SBOM (`syft`, listada em `TOOLING.md` para `internal-auditor`) detecta dependência
  **declarada**; o risco real deste projeto é dependência escrita à mão sem declarar nenhuma
  (`#include` cru de header vendorizado no disco). SBOM é complemento, não substitui os três itens
  de `#include`/`find_package`/`FetchContent` acima, e não cobre a EXCEÇÃO Nº 1: `gl.xml` não é
  dependência declarada em gerenciador de pacote nenhum, é dado lido por um script nosso, e a prova
  de integridade dela é o par `sha256`/testes descrito acima, não SBOM.

---

## 2. Camadas e atomização (L-17, L-19)

🔴 **CRÍTICO**

- Gate existente e verificável hoje: `tests/tools/check_layers.sh <raiz>` reprova qualquer arquivo
  de `src/core/` ou `include/glintfx/core/` que inclua header de camada acima (`glintfx/platform/`,
  quando ela nascer) ou header de SO (Wayland, Win32, GL/EGL, POSIX de baixo nível). A saída
  **tem** de declarar `violations: N in M files scanned` com `M > 0` — o mesmo padrão de "portão
  que varre zero e imprime verde" que motivou reescrever este manual.
- `#ifdef`/`#if defined` fora do topo do arquivo, dentro do corpo de uma função (armadilha 3 da
  L-19): adaptador por plataforma é **um arquivo por plataforma escolhido pelo CMake**, nunca
  bloco de pré-processador picotando uma função.
- Assunto novo virando método a mais de um handle/classe já existente em vez de módulo próprio
  (armadilha 1 da L-19: "handle opaco que vira dono de tudo").

🟠 **IMPORTANTE**

- Limites de `CONTRACT.md` §6.2, tornados inegociáveis pela L-17: função ≤ 40 linhas, ≤ 4
  parâmetros, ≤ 3 níveis de aninhamento, retorno antecipado em vez de aninhar. **Sem ferramenta
  automática ainda** (`clang-tidy readability-function-size` entra com o gate do capítulo 9,
  pendente); até lá, checagem manual de arquivo e linha em cada fatia revisada.
- `concept` de porta (L-19, item 2) exigindo mais operações do que o consumidor mínimo precisa
  (armadilha 2, "porta gorda").
- Nome de função que precisa de "e" para ser verdadeiro (`carrega_e_valida`) — são duas funções.

---

## 3. Fronteira pública, ABI e erro (L-19, L-22)

🔴 **CRÍTICO**

- Gate existente e verificável hoje: `tests/tools/check_exports.sh <caminho-.so>` roda
  `nm -D --defined-only` e reprova símbolo exportado fora do namespace `glintfx::` (prefixo
  mangled `_ZN7glintfx`) ou fora do allowlist mínimo de runtime (`_init _fini _edata _end
  __bss_start`).
- Classe pública com layout visível ou método `virtual` fora do conjunto explicitamente permitido
  pela L-19 (*value type* do núcleo: `version`, `vec2`, `rect` e afins). Handle e subsistema com
  estado têm de ser opacos (handle opaco ou PIMPL).
- Exceção cruzando a fronteira pública. Auditoria manual: toda função de API pública que pode
  falhar retorna `std::expected` ou código de erro, nunca deixa `throw` escapar para o consumidor.
  Erro nunca é engolido em silêncio (`CONTRACT.md` §6.4).

🟠 **IMPORTANTE**

- Diffing formal de ABI (`abidiff`/`abi-compliance-checker`) ainda **não está adotado** —
  `TOOLING.md` não o lista para nenhum agente deste projeto. Não inventar o comando; se a
  necessidade aparecer, é decisão de trazer ferramenta nova (L-14/L-07), não algo a improvisar.

---

## 4. Os três contratos de versão: API, ABI, DADO (L-26)

**A pergunta de DADO não é "quem recompila", é "quem perde o arquivo".**

🔴 **CRÍTICO**

- `SOVERSION` do alvo compartilhado acompanha o componente **A** da versão `vA.B.C.D`. Hoje
  `0.1.0.0`, `SOVERSION 0` — checar em `CMakeLists.txt`/`cmake/GlintfxLibrary.cmake`.
- Todo formato de arquivo publicado pela lib (mapa, L-30; folha RCSS, L-28, quando existirem)
  declara versão no cabeçalho e tem regra de **pular bloco desconhecido** na leitura. Quebra de
  formato sobe o **A**, e o leitor novo **continua lendo os formatos antigos** — a lib nunca
  abandona um arquivo que ela mesma gravou.
- Quando o formato de mapa ganhar escrita (L-30, decisões de 22/08/2026): **preservar chunk
  desconhecido ao regravar**, não só pular na leitura — são capacidades diferentes, e a lei
  registra a preservação como a exigência mais fácil de esquecer.

🟠 **IMPORTANTE**

- Mecanismo de checar API pública contra a versão anterior do header (`v0.1` compila contra
  `v0.2`?) ainda **não existe** no CI. Registrar como pendente em vez de fingir cobertura.

**Estado hoje:** nenhum formato de arquivo existe ainda (nem mapa nem RCSS). Este capítulo fica
pré-registrado; aplica-se no primeiro commit que grava um arquivo, não depois.

---

## 5. Wayland puro, sem X11 (L-05)

🔴 **CRÍTICO**

- Nenhum rastro de X11 no repositório: `grep -rniE "xlib|xcb|xtest|<X11/|xvfb|xdotool" src include
  tests .github` tem de sair vazio, em produção **e** em teste.
- Nenhum backend X11 nem fallback por XWayland. O backend Linux fala Wayland direto
  (`wl_compositor`, `xdg-shell`, `wl_seat`, `wl_keyboard`, `wl_pointer`) contra
  `libwayland-client`.
- Exemplo de internet copiado que usa X11 é achado, mesmo "só para o teste".

**Estado hoje:** nenhum backend de janela existe ainda. Capítulo pré-registrado para a fatia que
abrir a primeira janela.

---

## 6. Isolamento de teste de superfície (L-09)

🔴 **CRÍTICO**

- Nenhum teste que toca janela, fullscreen, foco, iconify, input ou tela roda fora de container.
  O compositor de teste (`kwin_wayland --virtual`/`--socket`) vive **dentro** do container.
- Nenhuma montagem, em script de teste ou Dockerfile, do `XDG_RUNTIME_DIR` do host nem do socket
  `wayland-0` do host. Nenhuma montagem de `/dev/uinput`. Nenhum injetor de input baseado em
  uinput — ele escreve no kernel, não numa sessão, e atravessa o container.
- Prova de isolamento **antes** de interagir: `ss -xp` do processo sob teste aponta para o socket
  do compositor do container; `lsof -p <pid>` não mostra caminho do `XDG_RUNTIME_DIR` do host.

**Estado hoje:** nenhum teste de janela existe ainda (não há backend, capítulo 5). Pré-registrado
para quando a L-05/L-09 tiverem código a testar.

---

## 7. Licença e proveniência (L-08, L-29)

🔴 **CRÍTICO**

- `SPDX-License-Identifier: AGPL-3.0-or-later` em todo arquivo fonte. Comando de auditoria, e
  resultado medido em 22/08/2026 (verde, 4 de 4 arquivos): `grep -rL
  "SPDX-License-Identifier" --include='*.hpp' --include='*.cpp' --include='*.h' src include`.
  A ferramenta `reuse` (conformidade SPDX formal) está listada em `TOOLING.md` como **a baixar**
  (`⬇`), não instalada ainda — instalação passa pelo líder (L-14) antes de virar gate de CI.
- Nada de código verbatim, porte linha a linha ou estrutura de arquivo decalcada de RmlUi (MIT) ou
  SDL3 (zlib), mesmo sendo licenças permissivas (L-29). Aprender a técnica por leitura é permitido
  e declarável em comentário/commit; colar código não é, e nasce obrigação de aviso de licença que
  o projeto não tem hoje.
- Repositório público: dado sensível (segredo, caminho de máquina, nome de menor) se verifica no
  **histórico**, não na árvore: `git log --all -p | grep -ci <termo>`, sempre com `-i`, nunca só
  `git grep` — commit que "limpa" um nome ainda o expõe no próprio diff.

🟠 **IMPORTANTE**

- `gitleaks` (instalado, `TOOLING.md` ✓) ainda não roda no CI deste projeto (ver capítulo 9).
  Cobre a árvore por padrão, não o histórico, e não pega nome de projeto — não é substituto do
  item anterior.

---

## 8. Entrada hostil: mapa e RCSS (L-28, L-30)

🔴 **CRÍTICO**, quando os parsers existirem

- Arquivo de mapa ou folha RCSS malformada, truncada ou com bloco/chunk desconhecido nunca
  derruba o processo: falha explícita, tratada, propagada ao chamador — nunca `assert` sozinho,
  nunca leitura fora dos limites do buffer.
- Fuzzing dedicado do parser (T3 do `TESTES.md`) entra na suíte no mesmo commit que o parser.

**Estado hoje:** nenhum parser de mapa nem de RCSS existe ainda. Capítulo pré-registrado; o
critério acima é fixado agora para não nascer depois do código, sob a lente da L-27.

---

## 9. Portões automáticos de qualidade: estado hoje (L-23)

A L-23 fixa quatro portões, mais o `preci.sh`. **`FUND-4` fechou** (`TODO.md`, coluna Status da
linha `FUND-4`: `✅ Concluído`; a coluna Estado Auditado da mesma linha está vazia, traço, como em
todas as linhas da tabela hoje, o SHA `9b01b3d` da segunda rodada de revisão adversarial vem da
coluna Descrição Técnica, não daquela). A versão anterior deste capítulo, datada de
22/08/2026, descrevia os quatro portões como pendentes: isso apodreceu. Estado confirmado **por
leitura** de `.github/workflows/ci.yml` e de `tools/preci.sh` nesta revisão (28/08/2026), sem
construir nada, como manda esta fatia:

| Portão | Exigido pela L-23 | Estado, confirmado por leitura em 28/08/2026 |
|---|---|---|
| Zero aviso, `-Werror` no CI | sim | **presente.** `-DGLINTFX_WERROR=ON` nos `cmake -S`/`-B` dos jobs `linux`, `windows` e `clang`, confira com `grep -n GLINTFX_WERROR .github/workflows/ci.yml`. A opção nasce `OFF` por padrão em `cmake/GlintfxOptions.cmake` e só é ligada pelo CI e pelo `preci.sh`, nunca no `configure` cru do dia a dia. |
| ASan/UBSan por fatia fechada | sim | **presente.** Job `sanitizer` em `.github/workflows/ci.yml`, roda `tools/preci.sh --sanitizer-only` no alvo primário (Fedora). |
| `clang-tidy` + `cppcheck` no CI | sim | **presente.** Job `lint` em `.github/workflows/ci.yml`, roda `tools/preci.sh --lint-only` no alvo primário. |
| `gitleaks` no CI | sim | **presente.** Job `gitleaks` em `.github/workflows/ci.yml`, com `fetch-depth: 0` (histórico inteiro, não só a árvore; capítulo 7 já cobra essa distinção). |
| `preci.sh` (espelho local do CI, antes do push) | sim | **presente.** `tools/preci.sh`, com os modos `--fast`, `--lint-only`, `--sanitizer-only` e `--selftest`, confira com `grep -n -- '--fast\|--lint-only\|--sanitizer-only\|--selftest' tools/preci.sh`. |

**Como isto foi confirmado, e o que NÃO foi feito:** os seis jobs de `.github/workflows/ci.yml`
citados acima (`linux`, `windows`, `clang`, `sanitizer`, `lint`, `gitleaks`) foram lidos por
inteiro nesta revisão, e o texto de cada célula acima cita o nome exato do job e o comando que ele
roda. **Não rodei `preci.sh` nem nenhum build** para chegar a esta tabela, porque esta fatia é só
documentação (ver a ordem de serviço); então "presente" aqui significa "o job existe e está
programado para rodar isto", não "eu vi passar agora". Para confirmar que o job **passou** de
verdade na última execução real, o comando é `gh run list --limit 1` e, para o detalhe por job,
`gh run view <id> --json jobs`; este comando não foi rodado nesta revisão, e por isso esta tabela
não afirma resultado de execução, só existência do portão.

**Gates que já existem e rodam hoje, em `tests/tools/`.** A lista cresce a cada gate novo; a
versão anterior deste capítulo enumerava oito nomes e ficou pra trás. **Meça, não leia a lista:**
`ls tests/tools/`. Os que os capítulos 2 e 3 deste documento já citam nominalmente
(`check_layers.sh`, `check_exports.sh`) continuam entre eles. **Quase todos** são exercitados por
`ctest` via `tests/CMakeLists.txt`, com duas exceções: `check_spdx.sh` e `check_vendor_purity.sh`
não têm `add_test` nenhum lá (confira com `grep -n 'check_spdx\|check_vendor_purity'
tests/CMakeLists.txt`, que não retorna nada), e rodam só como passo direto do job `leis` em
`.github/workflows/ci.yml`. Comando real de hoje (`CLAUDE.md`): `ctest --test-dir build
--output-on-failure`.

**Não inventar comando para o que falta.** Este princípio continua valendo para o que ainda não
existe (diffing formal de ABI, ver capítulo 3; checagem de API pública contra versão anterior, ver
capítulo 4), mas já não se aplica aos cinco portões acima, que saíram de "a definir" para linha
de CI nomeada.

---

## 10. Dossiê formal pré-1.0 (L-24)

O `internal-auditor`, em modelo `fable`, consolida os nove capítulos acima num dossiê único antes
do release 1.0: achado, severidade, evidência (arquivo e linha, ou saída de comando reproduzível)
e remediação. Nenhum 🔴 fecha o dossiê sem plano de remediação e re-teste. Até a 1.0, os portões
automáticos do capítulo 9 — os que já existem e os que `FUND-4` ainda vai entregar — cobrem o dia
a dia.

**Sem meta numérica de cobertura de teste** (L-24): cobertura é consequência do TDD estrito
(L-20), não alvo de auditoria. Auditar cobertura como número é procurar a métrica errada.
