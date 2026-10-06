# Ferramentas de apoio aos testes: versões, modos novos e limites

**Para que serve.** Registro das ferramentas que o GlintFx usa para construir, testar, analisar e isolar testes (Docker, CMake/CTest, compiladores, linters, sanitizers, ccache, gitleaks, gh, wine/MSVC, KWin, Mesa, Wayland, D-Bus, systemd). Nasceu da ordem do líder de 06/10/2026: *"Instalei plugin docker. Procure documentacao. E também leia a documentacao de todos os apps de apoio de testes que estamos usando aqui pois vários foram atualizados e você ainda não foi treinado, para ver se há novos modos e parâmetros de uso e registre tudo."*

**Quem lê.** Agentes e mantenedores deste projeto, antes de escrever ou revisar script de teste, CI ou Containerfile.

**Quando foi medido.** Todas as medições são de 06/10/2026, entre 11:00 e 11:20 (America/Recife), no host Fedora 44 do líder e na imagem `glintfx-wltest:m1`. Foram só leituras (`--version`, `--help`, `docker info`, arquivos, URLs). Nada foi instalado, atualizado nem executado como teste.

**Regra da casa: versão escrita apodrece.** Por isso cada versão abaixo vem com o COMANDO que a mede e a data da medição. Quem for confiar em um número deve medir de novo antes. O precedente é a seção "Estado atual" do `CLAUDE.md` do projeto, que proíbe gravar contador e manda gravar o comando que o mede.

**Convenção.** FATO = comando executado ou URL lida, citado. INFERÊNCIA = conclusão da leitura, marcada como tal. Novos modos e parâmetros listados como "oportunidade" são só registro: nenhum foi adotado, e adotar qualquer um é decisão do líder (L-01), com pacote novo dependendo de autorização (L-51). Os resumos de notas de versão vieram em parte por leitura resumida de página (WebFetch): tratar como pista e conferir na fonte antes de agir.

Relatórios brutos de origem (fora do repositório, em `/var/tmp`, podem sumir): `/var/tmp/ferramentas/docker.md`, `/var/tmp/ferramentas/construcao-teste-analise.md`, `/var/tmp/ferramentas/pilha-grafica.md`.

## 1. Tabela de versões

"Imagem/CI" é a versão no ambiente de teste ou de integração contínua quando se conhece; "n/m" significa não medida.

| Ferramenta | Host | Imagem de teste / CI | Comando que mede | Data |
|---|---|---|---|---|
| Docker Engine e CLI | 29.7.2 (API 1.55) | n/a; mais nova publicada: 29.8.2 (30/09/2026) | `docker version` | 06/10/2026 |
| containerd / runc | 2.3.5 / 1.5.1 | n/a; 29.8.2 traz 2.3.6 / 1.5.2 | `docker version` | 06/10/2026 |
| buildx | 0.37.1 | CI usa `docker/setup-buildx-action@v4`; mais nova: 0.37.2 | `docker buildx version` | 06/10/2026 |
| BuildKit (builder `default`) | 0.32.2 | n/m | `docker buildx ls` | 06/10/2026 |
| compose | 5.5.1 | não usado pelo projeto; mais nova: 5.6.0 | `docker compose version` | 06/10/2026 |
| Plugin Claude Code `docker-skills` | 0.3.1 | n/a | ver `plugin.json` em `~/.claude/plugins/marketplaces/docker/.claude-plugin/` | 06/10/2026 |
| `docker agent`, `sbx`, `docker debug`, `docker scout`, `docker model` | ausentes | n/a | `docker agent --help`, `which sbx` | 06/10/2026 |
| CMake / CTest | 4.3.0 | CI: `:latest` das distros (n/m); Windows instala 4.1.6 (`ci.yml:855`); piso do projeto 4.1; estável no site: 4.4.4 | `cmake --version`, `ctest --version` | 06/10/2026 |
| Ninja | 1.13.2 | n/m | `ninja --version` | 06/10/2026 |
| GCC | 16.2.1 20260819 | `fedora:latest` | `g++ --version` | 06/10/2026 |
| Clang, clang-tidy, clang-format | 22.1.8 | job lint: `fedora:latest` com `clang-tools-extra` | `clang --version`, `clang-tidy --version`, `clang-format --version` | 06/10/2026 |
| cppcheck | 2.22.0 | job lint: `dnf install cppcheck` | `cppcheck --version` | 06/10/2026 |
| ccache | 4.12.3 | só local (`tools/preci.sh`); mais nova no site: 4.14.1 | `ccache --version` | 06/10/2026 |
| gitleaks | 8.30.0 | job gitleaks: `apt-get install gitleaks` no runner (versão n/m) | `gitleaks version` | 06/10/2026 |
| gh | 2.97.0 | não usado no CI; mais nova listada: ~2.102 | `gh --version` | 06/10/2026 |
| python3 | 3.14.7 | pacote da distro; Windows: do runner | `python3 --version` | 06/10/2026 |
| pwsh | ausente | job Windows usa `shell: pwsh` | `pwsh --version` | 06/10/2026 |
| wine | 11.0 (Staging) | só no container msvc: Debian 12 `wine64` (versão n/m) | `wine --version` | 06/10/2026 |
| binutils `ld`, shellcheck, valgrind | 2.46.1, 0.11.0, 3.27.1 | n/m | `ld --version`, `shellcheck --version`, `valgrind --version` | 06/10/2026 |
| KWin | 6.7.5-1.fc44 | 6.7.5-1.fc44 (igual) | `rpm -q kwin` / `docker run --rm --entrypoint rpm glintfx-wltest:m1 -q kwin` | 06/10/2026 |
| Mesa (`mesa-dri-drivers`, libEGL, libgbm) | 26.2.3-1.fc44 | **26.1.8-1.fc44 (difere do host)** | `rpm -q mesa-dri-drivers` | 06/10/2026 |
| wayland / wayland-protocols | 1.26.0 / 1.49-1.fc44 | pacotes devel só no estágio de build | `pkg-config --modversion wayland-client wayland-protocols` | 06/10/2026 |
| systemd (busctl) | 259.9 | 259.8 | `rpm -q systemd` | 06/10/2026 |
| dbus-daemon | 1.16.2 | 1.16.2 | `rpm -q dbus-daemon` | 06/10/2026 |
| Qt (qt6-qtbase) | 6.11.2 | 6.11.2 | `rpm -q qt6-qtbase` | 06/10/2026 |

## 2. Docker (Engine, buildx, compose)

Fonte: `/var/tmp/ferramentas/docker.md`. Ambiente do daemon (FATO, `docker info`): Fedora 44, kernel 7.2.8, cgroup v2 com driver systemd, storage `overlayfs` com snapshotter containerd, runtime `runc`, segurança `seccomp (builtin)`, `selinux`, `cgroupns`; sem `rootless` e sem `userns`. CDI (interface padronizada de dispositivos) ativo com `nvidia.com/gpu` descoberto. O usuário está no grupo `docker`.

### 2.1 Como o projeto usa

Subcomandos (FATO, grep em `tools/`, `tests/`, `.github/`): `docker run` (preci, msvc, CI), `docker create` (controles negativos do CI), `docker exec` (fixtures), `docker inspect` (`tests/container/check_isolation.sh`), `docker build` (msvc), `docker rm -f`, `docker rmi`, `docker ps`. No CI: `docker/setup-buildx-action@v4` e `docker/build-push-action@v7` com `load: true`, `push: false`, `cache-from/cache-to: type=gha`. O projeto **não** usa compose. Flags de segurança do projeto: `--cap-drop=ALL --cap-add=SYS_NICE` (container real) e, só em controle negativo, `--security-opt systempaths=unconfined`, `--device=/dev/uinput`, `--pid=host`. Montagens com sufixo SELinux `:ro,z` (preci) e `:ro,Z`/`:Z` (msvc).

### 2.2 O que mudou (fonte: https://docs.docker.com/engine/release-notes/29/ e .../28/, lidas só em parte: 100 000 de 118 617 e de 108 792 caracteres)

- 29.0.0 (10/11/2025): containerd image store como padrão em instalação nova (o host já usa); cgroup v1 depreciado (o host está em v2); Docker Content Trust removido do CLI; `ulimit -n` do containerd caiu de 1048576 para 1024 (INFERÊNCIA: fixture que abra muitos descritores de arquivo pode notar).
- 29.3.0: opção de mount `bind-create-src`. 28.0.0: `--time` virou `--timeout` em `stop/restart`; mudanças de segurança de rede em portas publicadas. 28.2.0: CDI habilitado por padrão.
- 29.4.2 e 29.4.3: endurecimento do seccomp (AF_ALG, `socketcall`) por CVE-2026-31431, **com quebra conhecida de programas 32-bit** (contorno documentado: perfil `seccomp/v0.2.1`).
- 29.6.0: endpoint `GET /images/{name}/attestations` (provenance SLSA, SBOM SPDX). 29.7.0: opção de daemon `default-stop-timeout`.
- 29.8.0 (03/09/2026): flag `docker run --umask`, bloqueio de AF_VSOCK, perfil AppArmor customizável, BuildKit 0.33.0, runc 1.5.1. 29.8.2 (30/09/2026): 3 CVEs do Engine mais 7 do BuildKit.
- buildx 0.30 a 0.37 (via `gh api repos/docker/buildx/releases`): `docker buildx install/uninstall` depreciados (0.30); source policy em Rego (0.31, experimental; padrão para imagens do Docker em 0.34); depurador `buildx dap`/`buildx debug` estável (0.33); `--resource` de CPU e memória em `build` (0.35, exige BuildKit 0.31+ e Dockerfile 1.25+); `-o type=local,mode=delete` (0.35); 0.37.2 (30/09/2026) faz `bake --progress=rawjson` rejeitar entitlements não concedidos (dois advisories).
- compose: salto de 2.40 para 5.0.0 (02/12/2025); 5.5.0 pode recriar containers no primeiro `up`; 5.6.0 (02/10/2026). Não nos afeta hoje.
- Ações do Docker no CI: `setup-buildx-action` v4.4.1, `build-push-action` v7.4.0 (ambas com tag major flutuante no `ci.yml`).

### 2.3 O que usamos e ficou depreciado

Nenhuma flag do projeto aparece como depreciada nas notas lidas (FATO por leitura). Dois pontos de fragilidade, ambos INFERÊNCIA:
- `tests/container/hostconfig_baseline.txt` congela o `HostConfig` inteiro do `docker inspect`. O Engine 29.8 acrescenta `--umask`, provável campo novo no JSON; atualizar o Engine deve reprovar o portão `check_isolation.sh` até a linha de base ser recapturada. Registrado como `HOSTCONFIG-BASELINE-ENGINE-DRIFT` na INBOX do `TODO.md`.
- `tests/container/Containerfile:230` e `:1182` usam `FROM fedora:44`, enquanto a matriz do CI e o `CLAUDE.md` dizem `fedora:latest`. O comentário do próprio Containerfile (linhas 7 a 9) fala em nunca derivar para `:latest`. Pode ser exceção intencional; decisão do líder. Registrado como `CONTAINERFILE-FEDORA-PIN`.

### 2.4 Novos modos e parâmetros (oportunidade, nada adotado)

- `--pids-limit N`: teto de processos por container, complementa a L-11. Hoje aparece só em plano (`docs/auditoria-plano-w7d.md:955`) e, segundo a memória do projeto, reprova o baseline até recapturá-lo.
- `--memory`, `--cpus`, `--read-only` com `--tmpfs`: nenhum `docker run/create` do projeto os usa; mudariam `run_compositor.sh`, o CI e o baseline.
- BuildKit: `RUN --mount=type=cache`, `COPY --link` e secrets reduziriam o tempo do Containerfile de mais de 1100 linhas (hoje só há cache de camada via `type=gha`).
- `buildx build --call check`: lint nativo de Dockerfile; o repositório não tem hadolint (grep em `ci.yml` e `preci.sh`).
- `--provenance`, `--sbom`, `--attest`: atestados das imagens; o passo de build do CI não os pede.
- `buildx --policy` (Rego): verificar a origem das imagens base, hoje `FROM` com tag solta.
- `buildx dap`/`debug`: depurar o Containerfile passo a passo.
- `--resource` (buildx 0.35): limitar CPU/memória do passo de build, alinhado ao "um trabalho pesado por vez" da L-11.
- CDI (`--device nvidia.com/gpu=...`): forma declarativa de expor a GPU a um container (o host já lista os dispositivos). Tocar a placa de vídeo do líder continua regido pela L-09 (alerta 5 vezes e autorização).
- Docker rootless e `--userns=remap`: reduziriam o dano de uma fuga, mas trocariam store, imagens `glintfx-*` e donos de bind mount; é decisão de arquitetura do líder. O grupo `docker` equivale a root no host (INFERÊNCIA sobre o modelo de ameaça).
- `docker run --umask` (29.8.0) e `default-stop-timeout` (29.7.0): sem pedido concreto nos arquivos atuais.
- `--use-api-socket` (28.1.0, experimental): dá ao container acesso ao socket do daemon. **Contrário** à política de isolamento do projeto (L-09); registrado como anti-padrão.
- `sbx` (microVM) e `docker agent`: ausentes; ver seção 3.

### 2.5 Lacunas

Notas das versões 29.x e 28.x lidas só em parte; página de instalação do `sbx` (requisitos de SO e KVM) e changelog do `docker-agent` não foram lidos; as notas de buildx e compose vieram de `gh api`, não do site. O repositório `docker/sandboxes` devolveu 404.

## 3. O plugin Docker do Claude Code (docker-skills)

FATO: versão 0.3.1 (29/09/2026), autor Docker Inc., Apache-2.0, instalado em `/home/petrus/.claude/plugins/cache/docker/docker-skills/0.3.1/`. Traz 11 skills (arquivos `SKILL.md`, referências, modelos e verificações), sem binário e sem hook. Cada skill dispara pela própria descrição; não há skill de entrada. Cobertura de leitura da pesquisa: sete `SKILL.md` na íntegra; os três de sandbox e o de `env` só em corpo principal, títulos e amostragem (cerca de 235 KB de apoio, não lidos linha a linha).

**Servem a este projeto** (INFERÊNCIA sobre utilidade):

| Skill | O que ensina | Por que serve |
|---|---|---|
| `docker-build-strategies` | multi-stage, cache de camadas, `RUN --mount=type=cache/bind/secret/ssh`, `.dockerignore`, `COPY --link`, usuário não-root, pino por digest | `tests/container/Containerfile` e `tools/msvc-container/Dockerfile*` |
| `docker-destructive-guardrails` | `rm -f`, `prune`, `rmi`, `volume rm` só com declaração do que se perde e confirmação; container criado pelo próprio agente em teste pode sair sem pergunta | casa com a L-53; há cerca de 47 imagens `glintfx` no host que um `prune` varreria |
| `docker-compose-patterns` | healthcheck, `depends_on: service_healthy`, `develop.watch` | leitura de referência: o projeto não usa compose |
| `docker-project-foundations` | `.dockerignore`, `Dockerfile`, `compose.yaml`, portas só em loopback | leitura de referência: o projeto já tem Containerfile |

**Exigem binário ausente** (FATO: `docker agent`, `sbx`, `docker debug`, `docker scout`, `docker model` não existem no host):

| Skill | Depende de |
|---|---|
| `docker-agent-config` | plugin CLI `docker-agent` (agent.yaml, toolsets, MCP) |
| `docker-agent-run` | `docker agent run` (`--safety`, `--sandbox`, `--worktree`); `--sandbox` exige `sbx` |
| `docker-agent-deploy` | `docker agent serve/share/eval` |
| `docker-sandboxes-lifecycle` | CLI `sbx` (`run/create/ls/stop/rm/exec`, `--clone`, `--cpus`, `--memory`) |
| `docker-sandboxes-network-credentials` | `sbx policy`, `sbx secret` |
| `docker-sandboxes-env` | `sbx env` e `sbxenv.yaml` (experimental) |
| `docker-sandboxes-kits` | `sbx kit pack/push/pull/sign/verify` |

Observações (FATO, salvo onde marcado): as skills foram verificadas contra o Docker CLI 29.7.2 (o mesmo do host) e `sbx` v0.42.0-503. A documentação oficial do `sbx` diz que o uso local é gratuito e usa microVM, mas não especifica SO nem KVM (`/dev/kvm` existe no host). Se o `--worktree` do `docker agent` fosse adotado, conflitaria com a L-55 (INFERÊNCIA). Instalar `sbx` ou `docker agent` exige autorização (L-51).

## 4. Construção, teste e análise

Fonte: `/var/tmp/ferramentas/construcao-teste-analise.md`.

### 4.1 CMake e CTest

**Uso (FATO):** `cmake_minimum_required(VERSION 4.1)` (`CMakeLists.txt:33`); configure `-G Ninja -DCMAKE_BUILD_TYPE=Release -DGLINTFX_WERROR=ON` (`ci.yml:602`, `tools/preci.sh:559`); ctest com `--output-on-failure --output-junit ... --parallel N` (`ci.yml:619`), `ctest -N` e `--show-only=json-v1` (`ci.yml:640`, `:673`, `preci.sh:944`); propriedades `LABELS`, `WILL_FAIL`, `RUN_SERIAL` (sete ocorrências), `SKIP_RETURN_CODE 77` (`cmake/GlintfxOptions.cmake:190`), `DART_TESTING_TIMEOUT 120` (`cmake/GlintfxTest.cmake:26`). Não existe `CMakePresets.json`.

**O que mudou** (https://cmake.org/cmake/help/latest/release/4.0.html a `4.4.html`): 4.0 removeu compatibilidade com políticas anteriores a 3.5; 4.1 trouxe `ctest --schedule-random-seed`; 4.2 trouxe o gerador `Visual Studio 18 2026` e as políticas CMP0203 e CMP0204 (`_WINDLL`, `_MBCS` na ABI MSVC); 4.3 depreciou `CMAKE_ENABLE_EXPORTS` e trouxe presets schema 11; 4.4 trouxe `ctest -- <args>`, o comando `discover_tests()`, `add_test(... BUILD_DEPENDS)`, `ctest --source-dir`, presets schema 12, e renomeou `-Wdev` para `-Wauthor` e `--no-warn-unused-cli` para `-Wno-unused-cli`.

**Depreciado entre o que usamos:** nada identificado (FATO por leitura; o projeto não usa `CMAKE_ENABLE_EXPORTS` nem `-Wdev`).

**Oportunidades:** `ctest --schedule-random` com `--schedule-random-seed` (achar dependência implícita de ordem); `--repeat until-fail:N` (caça a teste instável); `--stop-on-failure` no modo `--fast` do preci; `TIMEOUT_SIGNAL_NAME` e `TIMEOUT_SIGNAL_GRACE_PERIOD` (SIGTERM antes de SIGKILL em teste com container ou compositor); `--resource-spec-file` e `--test-load` (expressar teto de recursos da L-11); `cmake --fresh` (preci.sh:559, :1317, :1446); `CMakePresets.json` com test presets para unificar CI e preci; `ctest -- <args>` e `BUILD_DEPENDS` (4.4).

**Lacuna:** a versão de CMake das imagens `:latest` do CI não é dedutível por leitura.

### 4.2 Ninja

1.13.2, passado só como `-G Ninja`. Notas de versão não lidas. CMake 4.4 acrescenta alvos `test_prep/<test>` ao gerador Ninja.

### 4.3 GCC 16.2.1

**Uso:** `-Wall -Wextra -Wpedantic -Werror`, `-fsanitize=address,undefined -fno-sanitize-recover=all` (`cmake/GlintfxCompileOptions.cmake:12-26`, `:120-128`).
**Mudou** (https://gcc.gnu.org/gcc-16/changes.html e `gcc-15/changes.html`, via resumo): GCC 16 muda o padrão C++ de `gnu++17` para `gnu++20`, traz reflexão e contratos do C++26, mensagens de erro aninhadas (voltar com `-fno-diagnostics-show-nesting` ou `-fdiagnostics-plain-output`) e **remove** `-fdiagnostics-format=json` (usar SARIF). GCC 15 pôs `-Wheader-guard` dentro de `-Wall` e trouxe `-fdiagnostics-add-output=`.
**Depreciado entre o que usamos:** nada identificado. INFERÊNCIA: `-Wheader-guard` em `-Wall` com `-Werror` poderia reprovar header com guarda incoerente (não verificado se os headers usam `#pragma once`); a hierarquia nova de erros pode afetar o parser do oráculo de compilador (`tests/tools/check_layers_oracle.py`, `extract_oracle_report.py`), a conferir com `-fdiagnostics-plain-output`.
**Oportunidade:** `-fdiagnostics-add-output=sarif:...` para anexar SARIF aos artefatos do CI.

### 4.4 Clang, clang-tidy, clang-format 22.1.8

**Uso (FATO):** `clang-format --dry-run -Werror` (`preci.sh:432`); `run-clang-tidy -p "$BUILD_DIR" -quiet` (`preci.sh:626`); `.clang-tidy` com `bugprone-*, clang-analyzer-*, performance-*, cert-*, misc-*` e `WarningsAsErrors: '*'`; `.clang-format` LLVM, recuo 4, limite 100 colunas.
**Mudou** (https://releases.llvm.org/22.1.0/tools/clang/tools/extra/docs/ReleaseNotes.html e `.../tools/clang/docs/ReleaseNotes.html`): checks novos no clang-tidy 22 (por exemplo `bugprone-invalid-enum-default-initialization`, `misc-override-with-different-visibility`); vários checks renomeados com alias (a lista completa está nas notas da versão acima); opções globais `IgnoreMacros` e `StrictMode` removidas; avisos de header aparecem por padrão. Clang 22: `-fsanitize=alloc-token`, `-Wincompatible-pointer-types` vira erro por padrão, novos `-Walloc-size`, `-Wshadow-header`, `-Wenum-compare-typo`.
**Depreciado entre o que usamos:** nada (o `.clang-tidy` lista `clang-analyzer-*` explicitamente, então a saída do conjunto padrão não o afeta; o `.clang-format` não usa as opções novas). INFERÊNCIA a conferir: NOLINT existentes que citem nome antigo de check renomeado, pois a validação do preci checa só o texto da justificativa.
**Oportunidades:** `-Wshadow-header` e `-Walloc-size` como portão extra; `clang-tidy --removed-arg`; `CustomChecks` por query.
**Lacuna:** notas do LLVM 20 e 21 não lidas.

### 4.5 cppcheck 2.22.0

**Uso:** `cppcheck --enable=warning,performance,portability --inline-suppr --error-exitcode=1 --suppress=missingIncludeSystem --std=c++20 ...` (`preci.sh:665-668`, `:1564`).
**Mudou** (https://github.com/danmar/cppcheck/releases, resumo): `--output-format=sarif|xml` (2.16), `--check-level=reduced` (2.17), `--exitcode-suppress` (2.21), checks novos em 2.22.
**FATO:** o `cppcheck --help` do host lista `--std` só até `c++20`; portanto `--std=c++20` é o teto da ferramenta, não esquecimento do projeto.
**Oportunidades:** `--output-format=sarif` no job lint; `--check-level=exhaustive` em varredura noturna.

### 4.6 ccache 4.12.3

**Uso:** só local, via `tools/preci.sh:2597-2630` (launcher por `-DCMAKE_{C,CXX}_COMPILER_LAUNCHER`, `CCACHE_BASEDIR`, `CCACHE_NOHASHDIR`, `CCACHE_DIR` e `CCACHE_MAXSIZE` em `/var/tmp`, teto de 3 GiB; `--print-stats` em `:917`). O CI não usa.
**Mudou** (https://ccache.dev/releasenotes.html, resumo): `--print-log-stats`, `--format`, `--inspect` (4.10); `base_dir` com vários diretórios (4.12); MSVC oficialmente suportado e `ccache.conf` por diretório (4.13); `@layout=local` (4.14).
**FATO e dúvida:** o `--help` do host ainda lista `--recompress-threads` e `--trim-recompress-threads`; a atribuição do resumo de que `--threads` os unificou em 4.12 pode estar errada (INFERÊNCIA). Conferir na fonte.
**Oportunidades:** `base_dir` múltiplo no lugar da reescrita de caminho de `preci.sh:2603-2611`; ccache no job MSVC e no CI Linux.
**Lacuna:** só 100 000 de 189 010 caracteres das notas foram lidos.

### 4.7 gitleaks 8.30.0

**Uso:** `gitleaks detect --no-banner --source "$ROOT_DIR"` (`tools/preci.sh:768`, `.github/workflows/ci.yml:4355`); `.gitleaksignore` na raiz; o job instala pelo apt do runner (`ci.yml:4333`).
**FATO depreciado:** no host, `gitleaks --help` lista `dir`, `git`, `stdin`, `completion`, `version`; `detect` **não aparece**, mas `gitleaks detect --help` ainda responde: comando legado oculto, funcional na 8.30.0. O equivalente atual é `gitleaks git` (INFERÊNCIA de equivalência, não testada). Risco: o apt do runner pode trazer outra versão (não medida). Registrado como `GITLEAKS-DETECT-LEGACY`.
**Novos parâmetros no host (FATO, `--help`):** `--diagnostics`, `--enable-rule`, `--max-archive-depth`, `--max-decode-depth`, `--ignore-gitleaks-allow`, `--exit-code`, `-b/--baseline-path`; em `git`: `--staged`, `--pre-commit`, `--log-opts`.
**Oportunidades:** `gitleaks git --staged` no hook local; `--baseline-path` no lugar de `.gitleaksignore`.

### 4.8 gh 2.97.0

**Uso:** `gh run list`, `gh run view <id> --json jobs -q ...`, `gh api repos/.../actions/runners`.
**Mudou** (https://github.com/cli/cli/releases, 100 000 de 111 837 caracteres, resumo): `gh pr checkout --worktree PATH` (2.98), `gh repo read-file` (2.95), `gh skill` e `gh discussion` (2.94).
**Oportunidades:** `--worktree` casaria com a L-55; `gh run view --exit-status` no portão pós-push (FATO: a opção existe no `--help` do host).

### 4.9 Demais

- python3 3.14.7: usado em `tests/tools/*.py` e `tools/*.py`; notas de versão não lidas.
- pwsh: ausente no host; só o job Windows do CI e `tools/ci/*.ps1` o usam (INFERÊNCIA: esses `.ps1` não são validados localmente).
- wine 11.0 Staging e MSVC em container: `tools/msvc-container/Dockerfile` (Debian 12, `wine64`, msvc-wine clonado de `mstorsjo/msvc-wine`), `win-wine-toolchain.cmake:77`. Notas do Wine 11 e do msvc-wine **não lidas**. As políticas CMP0203 e CMP0204 do CMake 4.2 afetariam o `cl.exe` se o piso de políticas subir.
- `ld`, `lld`, shellcheck, valgrind, gcov, llvm-cov: só `--version`; mold ausente.

## 5. Pilha gráfica e de sessão

Fonte: `/var/tmp/ferramentas/pilha-grafica.md`. Imagem medida: `glintfx-wltest:m1`.

**Uso (FATO):** `tests/container/run_compositor.sh:111` sobe `dbus-run-session -- kwin_wayland --virtual --socket "$socket_name"` em segundo plano (por causa do PID 1) e exporta `XDG_RUNTIME_DIR` e `XDG_SESSION_TYPE=wayland`. Nenhuma variável `KWIN_*`, `LIBGL_*`, `MESA_*` ou `GALLIUM_*` é definida em `tests/container/*.sh`, no Containerfile ou no `ci.yml`. O alvo é descrito como llvmpipe/swrast sem `/dev/dri` (`Containerfile:22`). O `Containerfile:296-300` roda `wayland-scanner client-header` e `private-code` sobre `xdg-shell.xml`. O KWin tem `cap_sys_nice=ep` (`Containerfile:1259-1262`), por isso o `docker run` concede `CAP_SYS_NICE`. `busctl`, `gdbus` e `qdbus` existem na imagem, mas nenhum arquivo de `tests/container/` os usa.

**O que mudou:**
- Mesa 26.2.0 (https://docs.mesa3d.org/relnotes/26.2.0.html, 100 000 caracteres): llvmpipe com cache unificado de variantes de shader; nada que crie modo novo para o KWin. **FATO:** a imagem tem Mesa 26.1.8 e o host 26.2.3; o llvmpipe do teste não é o do host. Notas do Mesa 26.1 não lidas.
- systemd 259 (https://raw.githubusercontent.com/systemd/systemd/v259/NEWS): `PassPIDFD=` e `AcceptFileDescriptors=` em sockets, opções novas de `systemd-run`; nada sobre `busctl` nos 100 000 caracteres lidos. Não afeta o projeto.
- Plasma 6.7 (https://kde.org/announcements/plasma/6/6.7.0/): só diz "Support for many more Wayland protocols and portals"; changelog detalhado não lido.
- wayland-protocols 1.46 a 1.49: conteúdo não obtido (gitlab bloqueou a leitura). dbus 1.16.2 e Qt 6.11.2: não pesquisados.

**Flags usadas por nós que mudaram:** nenhuma identificada. `--virtual` e `--socket` seguem presentes no `kwin_wayland` 6.7.5 (FATO, strings do binário). `--width`, `--height`, `--scale` e `--no-lockscreen` não foram verificados.

**Oportunidade:** `wayland-scanner -s` (modo estrito: falha em XML fora do DTD) em `Containerfile:296-300` e no CMake do binding (INFERÊNCIA; `-s` e `-c` constam do `--help` do scanner 1.26.0).

**Variáveis `KWIN_*` presentes na `libkwin` 6.7.5 (FATO, strings do binário):** `KWIN_COMPOSE`, `KWIN_RENDER_NODES`, `KWIN_DRM_DEVICES`, `KWIN_FORCE_SW_CURSOR`, `KWIN_DISABLE_VULKAN`, `KWIN_WAYLAND_NO_PERMISSION_CHECKS`, `KWIN_XKB_DEFAULT_KEYMAP`, `KWIN_LOG_PERFORMANCE_DATA`, entre outras. O significado de cada uma **não foi verificado** além do nome, exceto `KWIN_COMPOSE`. Não há variável que ligue GL por software no backend virtual.

## 6. Fatos que limitam a prova visual

Estes fatos explicam por que a captura de tela no container não tem atalho. Fonte: código do KWin ramo `Plasma/6.7` baixado em 06/10/2026 (`https://invent.kde.org/plasma/kwin/-/raw/Plasma/6.7/src/backends/virtual/virtual_backend.cpp`, cópia em `/var/tmp/ferramentas/kwin67_virtual_backend.cpp`). O ramo baixado pode diferir em detalhes do pacote 6.7.5 instalado (INFERÊNCIA).

**FATO por leitura de código:**
1. O backend virtual procura um nó DRM de renderização com `drmGetDevices2()`. Sem nenhum dispositivo, devolve nulo.
2. `supportedCompositors()` oferece composição OpenGL só se houver dispositivo de renderização; composição QPainter (software, em CPU) é oferecida sempre. Logo, sem `/dev/dri`, o KWin virtual só compõe por QPainter. Isso casa com a medição feita em 06/10/2026.
3. `KWIN_COMPOSE` só reconhece `O2` e `O2ES` no 6.7; a lista de compositores do backend virtual já exclui GL sem dispositivo, então `O2` não muda nada.
4. O plugin ScreenShot2 faz `dynamic_cast` do backend para `EglBackend` e devolve vazio se não for EGL; a interface D-Bus converte isso em `org.kde.KWin.ScreenShot2.Error.Cancelled` ("Screenshot got cancelled"). O cancelamento é por desenho com compositor QPainter, não erro de parâmetro.
5. O host tem os protocolos `ext-image-copy-capture-v1` e `ext-image-capture-source-v1` em `/usr/share/wayland-protocols/staging`, mas o KWin 6.7.5 **não os anuncia**: nenhuma string correspondente em `libkwin.so.6` nem em `kwin_wayland`. Só aparece `zkde_screencast_unstable_v1` (mais os plugins `screencast.so`, `screenshot.so`, `eis.so`).

**INFERÊNCIA (não testada):**
- O que satisfaria o código seria um nó DRM de software, como o módulo de kernel `vgem` (existe no host, `modinfo vgem`, com 0 carregado). Carregá-lo exige privilégio no host (L-51), o Mesa/GBM teria de aceitar o nó, e vgem não é a GeForce.
- O backend GL do virtual cria swapchain GBM, então precisa de GBM sobre um nó DRM; não há caminho EGL sem superfície (surfaceless) no backend virtual.
- Há um único atalho no código (`CI` definido e libdrm sem suporte a faux bus abre `/dev/dri/card1` fixo); provavelmente não é o nosso caso, pois o libdrm 2.4.134 parece ter faux bus.
- Uma busca web de 06/10/2026 concorda que o KWin não exporta `ext-image-copy-capture` nem no Plasma 6.6 (https://github.com/trycua/cua/issues/3972); não foi confirmado por fonte oficial do KDE.
- O screencast por PipeWire também depende de caminho de renderização; não foi testado.
- O que o relé (`wire_relay`) enxerga é o `wl_shm` do cliente, o que mantém a captura por relé como o caminho do `QA-SCREEN-CAPTURE`.

## 7. Lacunas: o que nenhuma pesquisa leu

Para não parecer completo, a lista do que ficou de fora:
- Notas completas do Docker Engine 28 e 29 (leitura parcial), página de instalação do `sbx`, changelog do `docker-agent`, o conteúdo linha a linha das referências do plugin docker-skills.
- Notas de versão do Ninja, do Python, do Wine 11, do msvc-wine, do shellcheck, do valgrind, do binutils e do lld.
- LLVM 20 e 21, GCC 14, Mesa 26.1 e anteriores, Plasma 6.7 em detalhe, wayland-protocols 1.46 a 1.49, dbus 1.16.2 e Qt 6.11.2.
- `kwin_wayland --help` não foi rodado; `--width`, `--height`, `--scale` e `--no-lockscreen` não foram verificados; o significado das variáveis `KWIN_*` além do nome.
- Versões reais dos pacotes dentro das imagens `:latest` do CI (só o log de uma execução real as mostra) e versão do gitleaks no runner Ubuntu.
- Uso, no `tools/preci.sh`, de shellcheck, valgrind e lld; conteúdo de `tests/tools/check_python_unbuffered.py`; CMake do binding Wayland em `cmake/`.
- Atualizações pendentes (Docker Engine 29.8.2 com 10 CVEs, buildx 0.37.2, compose 5.6.0, CMake 4.4.4, ccache 4.14.1, gh ~2.102) são fato de versão publicada; aplicá-las é decisão do líder (L-14) e está registrado como `TOOLCHAIN-UPDATES-PENDENTES` na INBOX.
