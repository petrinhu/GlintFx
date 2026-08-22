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
- Nenhum diretório `vendor/`, `third_party/` ou `external/` no repositório. Comando de auditoria:
  `find . -iname "*vendor*" -o -iname "*third_party*" -o -iname "*external*"` fora de `.git/`.

🟠 **IMPORTANTE**

- Ferramenta de SBOM (`syft`, listada em `TOOLING.md` para `internal-auditor`) detecta dependência
  **declarada**; o risco real deste projeto é dependência escrita à mão sem declarar nenhuma
  (`#include` cru de header vendorizado no disco). SBOM é complemento, não substitui os três
  itens acima.

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

A L-23 fixa quatro portões, mais o `preci.sh`. Estado real em 22/08/2026, para não prometer o que
não existe:

| Portão | Exigido pela L-23 | Estado hoje |
|---|---|---|
| Zero aviso, `-Werror` no CI | sim | **pendente**, `FUND-4` no `TODO.md` |
| ASan/UBSan por fatia fechada | sim | **pendente**, `FUND-4` |
| `clang-tidy` + `cppcheck` no CI | sim | **pendente**, `FUND-4`; ferramentas já instaladas localmente (`TOOLING.md` ✓) |
| `gitleaks` no CI | sim | **pendente**, `FUND-4`; instalado localmente (`TOOLING.md` ✓) |
| `preci.sh` (espelho local do CI, antes do push) | sim | **não existe** — `CLAUDE.md` lista `Lint / análise estática: <a definir>` |

**Gates que já existem e rodam hoje**, em `tests/tools/`: `check_layers.sh` (capítulo 2),
`check_exports.sh` (capítulo 3), `check_embed.sh`, `check_consume.sh`,
`check_install_includedir.sh`, `check_install_packager_layout.sh`,
`check_no_target_collision.sh`, `check_output_name.sh` — todos exercitados por `ctest` via
`tests/CMakeLists.txt`. Comando real de hoje (`CLAUDE.md`): `ctest --test-dir build
--output-on-failure`.

**Não inventar comando para o que falta.** Quando `FUND-4` fechar, este capítulo é o primeiro a
ser atualizado com o comando real, testado e verde — não antes.

---

## 10. Dossiê formal pré-1.0 (L-24)

O `internal-auditor`, em modelo `fable`, consolida os nove capítulos acima num dossiê único antes
do release 1.0: achado, severidade, evidência (arquivo e linha, ou saída de comando reproduzível)
e remediação. Nenhum 🔴 fecha o dossiê sem plano de remediação e re-teste. Até a 1.0, os portões
automáticos do capítulo 9 — os que já existem e os que `FUND-4` ainda vai entregar — cobrem o dia
a dia.

**Sem meta numérica de cobertura de teste** (L-24): cobertura é consequência do TDD estrito
(L-20), não alvo de auditoria. Auditar cobertura como número é procurar a métrica errada.
