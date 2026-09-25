# Plano: CI por sistema no modelo do astrometrica, as distros novas, e a imagem de teste incremental

**Autor:** Caetano (CTO, `opus`), modo autônomo (L-34). **Data:** 24/09/2026, quarta versão (VL-1, VL-2, VL-3 e a frase do python3 decididos pelo líder; A7d e A7e destravadas); antes, terceira versão (A5, A6 e D-A7 a D-A9 acrescentadas depois da escolha do líder "Espírito do astrometrica", commit `7d064ff`); a segunda, das 20:27, trouxe o plano; a primeira, das 20:18, era um estudo "vale a pena?" e foi substituída pelas duas ordens do líder de 24/09, registradas na L-04). **Árvore lida:** GlintFx `main` local, HEAD `52962dc`; astrometrica (`petrinhu/astrofind`) na árvore local.
**Natureza:** só leitura. Nenhum build, container, suíte ou oráculo rodado (L-09, L-45). Medições vieram de `gh run view`/`gh api`, de leitura de arquivo e de índices públicos de pacotes (URL citada).
**Itens:** `CI-SPLIT-PER-OS` e `PLATFORMS-ASTRO-PARITY` (Parte II, um plano só, porque o segundo depende do primeiro) e `CONTAINER-BUILD-PER-FIXTURE-LAYERS` (Parte III, plano separado).
**Marcação (L-27):** `FATO` = arquivo:linha, saída de comando citada, ou fonte externa com URL. `INF` = inferência minha, sempre com o teste que a confirma ou derruba.

---

## 0. Resumo em dez linhas

1. **Estrutura, confirmada pelo líder ("Espírito do astrometrica", commit `7d064ff`):** um workflow só, com **um job de matriz por sistema** e o preparo de cada sistema num **arquivo próprio** (`tools/ci/env/<sistema>.sh`). O conserto de um sistema toca só o arquivo dele, como no astrometrica, e o `PARITY-GATE` continua recebendo tudo do mesmo run (D-A1).
2. **Achado que a ordem expõe:** o `PARITY-GATE` de hoje compara a **união** de todas as pernas Linux contra a união das pernas Windows (`ci.yml`, passo "Unir inventarios por sistema"). Um teste que some só no CachyOS passa despercebido. Com 12 sistemas Linux, a paridade tem de ser **por sistema** (fatia A1).
3. **Achado sobre os passos pulados:** no job `wayland-container` plain do run `36036115151`, um vermelho no passo 28 pulou **22 passos**, inclusive a publicação do inventário. O nome da fixtura só entra no inventário **depois** de ela passar (`ci.yml:1795-1796`). Então uma falha de teste chega ao `parity` como buraco de cobertura (fatia A2).
4. **Lista unida, sem duplicatas:** 13 alvos (seção 3.1). `ubuntu:latest` **não** é o Ubuntu 24: hoje ele é o 26.04 "resolute" (FATO, log do CI).
5. **Piso real:** GCC 14 **e CMake 4.1** (`README.md:78`, `CMakeLists.txt:33`). O CMake barra mais distros que o GCC.
6. **Com caminho para o piso, todas as 12 Linux:** Ubuntu 24.04, Mint 22, Pop!_OS 24.04 e Zorin OS 18 (`gcc-14` do universe do noble mais o tarball da Kitware), Debian 13 (`gcc-14` + `cmake` de `trixie-backports`) e Rocky 9 (`gcc-toolset-14` no AppStream mais o tarball da Kitware). As rolantes já atendem.
7. **Decididas pelo líder (seção 3.3):** Debian 13 no lugar do 12; imagens reais de Pop!_OS 24.04 e Zorin OS 18; análises só no Fedora. O astrometrica não testa Pop nem Zorin de verdade (roda `ubuntu:24.04` com outro rótulo), e o Pop real traz libwayland e Mesa próprios (FATO, 3.3).
8. **KWin existe em todas**, mas em duas famílias: 6.x (Fedora, Arch, CachyOS, Manjaro, Tumbleweed, Ubuntu 26.04) e 5.27 (Ubuntu 24.04, Mint 22, Pop!_OS 24.04, Zorin OS 18, Rocky 9 via EPEL); Debian 13 tem 6.3.6. Plano: compositor da própria distro, dentro do container da distro (D-A5).
9. **Tempo de fechamento por fatia (INF, a partir dos jobs de hoje):** de ~21-22 min para **~35-45 min** só com a ampliação. Com A5 (`ctest` paralelo calibrado), A6 (clang-tidy do Windows em 3 partes) e a imagem incremental, **~25-30 min**. A5 e A6 entram antes das distros. O gargalo passa a ser o teto de **20 jobs simultâneos** do plano Free.
10. **Imagem incremental:** plano de antes mantido. O que mudou: o Containerfile passa a aceitar a imagem base por parâmetro, e o teto de cache foi refeito para ~26 escopos (seção 10).

---

# Parte I: o que foi lido

## 1. O modelo do astrometrica (FATO)

- **Arquivos:** doze workflows: `build.yml` (runner `ubuntu-24.04`, sem container), nove `qa-<distro>.yml` com um job cada num container, `audit.yml` (matriz de 5, `fail-fast: false`, `audit.yml:94`), `secret-scan.yml`. Todos com o mesmo gatilho (`push` em `main`/`develop`, `pull_request` para `main`), sem `paths:`, sem `needs:` entre eles.
- **Imagens dos `qa-*`**, lidas agora: `archlinux:latest` (`qa-arch.yml:56`), `debian:12` (`qa-debian-12.yml:58`), `manjarolinux/base` (`qa-manjaro.yml:63`), `linuxmintd/mint22-amd64` (`qa-mint-22.yml:55`), `opensuse/tumbleweed` (`qa-opensuse-tw.yml:62`), **`ubuntu:24.04` em `qa-pop-os-22.yml:53`**, `rockylinux:9` (`qa-rocky-9.yml:64`), `ubuntu:24.04` (`qa-ubuntu-24.yml:46`), **`ubuntu:24.04` em `qa-zorin-17.yml:53`**.
- **Pop e Zorin são substitutos declarados:** `qa-pop-os-22.yml:1` *"QA · Pop!_OS stand-in (ubuntu:24.04 base)"*; `:12-17` *"There is no official Pop!_OS Docker image ... This job therefore runs on ubuntu:24.04 ... as a stand-in"*; `:19` *"LIMITATION: This is NOT Pop!_OS and NOT a 22.04 base."* O mesmo em `qa-zorin-17.yml:1`, `:11-20`.
- **`audit.yml`:** `fedora:44`, `cachyos/cachyos:latest`, `archlinux:latest`, `ubuntu:24.04`, `debian:12` (`:96-113`), com `tidy_gate` falso em Debian/Ubuntu (`:99-110`).
- **Resposta da sessão do astrometrica** (repassada pelo main, com arquivo:linha): o conserto por SO fica **só na instalação de dependências, nunca no código**; mesma suíte em todas as distros (`qa-arch.yml:194-197`); não há job que compare saídas entre distros; o rerun só é usado quando a falha veio **antes** de qualquer teste, no máximo uma vez; o ganho de tempo **nunca foi medido**.
- **Tempos dele:** cada `qa-*` leva ~4-5 min (run do commit `d2c0277`: todos começam 23:04:17Z e terminam entre 23:08:32Z e 23:09:25Z).

## 2. O CI do GlintFx hoje (FATO)

- **Um workflow**, 26 jobs, todos paralelos, `fail-fast: false` nas matrizes. Só `parity` espera: `needs: [linux, windows, wayland-container, lint]`, com `if: !cancelled()`.
- **Duração:** run `36036115151`: 22,5 min de parede, 25 jobs, **210 job-minutos** somados. Caminho crítico: `Windows - Lint`, 1345 s. Run `36023489188` tentativa 1: 20,7 min (caminho crítico `Windows - estatico`, 1226 s).
- **Dentro do job Linux** (`Fedora (primario) - compartilhado`, 640 s): build ~190 s; `ctest` **serial** 450 s (`ci.yml:534`, `:960`; `preci.sh:801`, sem `-j`), sete portões de consumo somam ~405 s.
- **Matriz Linux** (`ci.yml:140-330`): 5 distros × {compartilhado, estático}. A receita de cada distro está **dentro** da matriz, no campo `instalar`. Fedora, Ubuntu e Arch instalam `python3`/`python` explicitamente; CachyOS não.
- **`ubuntu:latest` hoje é o Ubuntu 26.04:** no log do job `Ubuntu - compartilhado` de `36036115151`, o apt baixa de `archive.ubuntu.com/ubuntu resolute`, e sai `g++-14 (Ubuntu 14.3.0-14ubuntu1)`, `cmake 4.2.3-2ubuntu2`, `wayland-client 1.24.0`, `wayland-protocols 1.47`, `python3 3.14`. O Fedora do mesmo run: `cmake 4.3.0`, `wayland-client 1.26.0`.
- **Paridade por união:** o passo "Unir inventarios por sistema" do job `parity` faz `cat inventarios/linux/*/parity_inventory.txt > /tmp/parity_linux_union.txt` e compara a união Linux contra a união Windows (`check_test_parity.py --compare`). **Nenhuma comparação por distro.** Os artefatos se chamam `parity-inv-linux-${{ strategy.job-index }}` (`ci.yml:561`): pelo nome, nem se sabe de que distro vieram.
- **O container é só Fedora:** o `Containerfile` constrói e roda tudo em `fedora:44` (`:230`, `:1128`; cabeçalho `:11-13`: *"Pinned on fedora:44"*). **Nenhuma das outras distros de hoje (Ubuntu, Arch, CachyOS) roda os testes de janela/EGL contra compositor.** A L-04 item 3 já pedia isso antes da ampliação. Observação: o cabeçalho ainda diz "pinned", e o `ESCOPO.md` §1 diz que todas as imagens da matriz passaram a `latest` em 27/08. O container não está na matriz; aponto a diferença e deixo a correção com o main.
- **Os passos pulados, medidos** (`gh api .../jobs`, job `Container Wayland isolado - plain` de `36036115151`): passo 28 (`GL-CONTEXT fatia 5 - erro de protocolo`) `failure`; passos **29 a 51 `skipped`**: 12 fixturas, o piso do inventário, o contador de alocação, a prova `ldd libasan`, a varredura do sanitizer, a coleta MEASURED, **a publicação do inventário (passo 47)** e os **quatro controles negativos de isolamento (48-51)**.
- **Por que o `parity` falha junto:** cada passo de fixtura grava o nome **depois** de rodar, sob `set -eu` (`ci.yml:1794-1796`: `exec_fixture.sh ... egl_protocol_error_smoke` e só então `echo "egl_protocol_error_smoke" >> parity_inventory.txt`). A publicação tem `if: matrix.perna == 'plain'`, sem `!cancelled()`/`always()` (`:2211-2217`). Resultado: o `parity` leva 7-8 s e falha por inventário incompleto, **mascarando** um vermelho de teste como buraco de paridade.
- **Portões que leem o `ci.yml`** (grep por `ci.yml` em `tests/tools/`): `check_ci_timeouts.py` (nomes e classes de job), `check_container_fixture_inventory.py` (job `wayland-container`, regex `echo "<nome>" >> parity_inventory.txt`), `check_container_kill_order.py`, `check_pkg_dep_coverage.py` (lê o `instalar` da matriz `linux`), `check_test_parity.py`, `check_measured_parity.py`, `check_dep_zero_trace.py` (`parity`), `check_env_sweep.py`, `check_readme_windows_build_anchors.py`, `check_windows_static_parallel.py`, e outros que só citam o arquivo.
- **Fila:** o plano Free permite **20 jobs simultâneos** em runner padrão (docs.github.com/en/actions/reference/limits). O repositório é público (`gh api`).

---

# Parte II: plano de `CI-SPLIT-PER-OS` + `PLATFORMS-ASTRO-PARITY`

## 3. A lista de alvos e o piso por distro

### 3.1 Lista exata, unida e sem duplicatas

| # | Alvo | Imagem | Origem | Duplicata resolvida |
|---|---|---|---|---|
| 1 | **Fedora** (primário) | `fedora:latest` | nosso | `fedora:44` do `audit.yml` dele: é a mesma versão hoje (FATO: o hospedeiro é `fc44`, e o log do Fedora do CI traz os mesmos `cmake 4.3.0`/`wayland 1.26.0`) |
| 2 | **Ubuntu** (atual) | `ubuntu:latest` (= 26.04 "resolute" hoje) | nosso | **Não** é o Ubuntu 24 dele |
| 3 | **Ubuntu 24.04 LTS** | `ubuntu:24.04` | dele (`qa-ubuntu-24`, `audit`, `build.yml`) | os três dele são a mesma distro |
| 4 | **Arch** | `archlinux:latest` | os dois | uma entrada |
| 5 | **CachyOS** | `cachyos/cachyos:latest` | nosso e o `audit` dele | uma entrada |
| 6 | **Debian 13** | `debian:13` | dele (`qa-debian-12`, `audit`), trocado para o 13 pelo líder (VL-1) | uma entrada |
| 7 | **Linux Mint 22** | `linuxmintd/mint22-amd64` | dele | a organização `linuxmintd` no Docker Hub também publica `mint22.1`/`22.2`/`22.3`; o astrometrica usa `mint22` (FATO, API do Docker Hub) |
| 8 | **openSUSE Tumbleweed** | `opensuse/tumbleweed` | dele | |
| 9 | **Manjaro** | `manjarolinux/base` | dele | |
| 10 | **Rocky Linux 9** | `rockylinux:9` | dele | |
| 11 | **Pop!_OS 24.04** | `ubuntu:24.04` + `apt.pop-os.org/release noble` no preparo | dele, imagem real pelo líder (VL-2) | distinto do Ubuntu 24.04: libwayland e Mesa próprios (3.3) |
| 12 | **Zorin OS 18** | `ubuntu:24.04` + PPAs `zorinos/*` no preparo | dele, imagem real pelo líder (VL-2) | distinto do Ubuntu 24.04 na identificação e no desktop (3.3) |
| 13 | **Windows** | `windows-latest` | nosso | |

Treze alvos, doze Linux: é o "~14" da ordem (a diferença é o `build.yml` dele contado à parte, e ele é o mesmo Ubuntu 24.04).

### 3.2 Piso por distro: GCC 14 e CMake 4.1

Piso, FATO: `README.md:78` (*"GCC 14 or newer; GCC 13's partial C++23 support is not enough, measured against Ubuntu 24.04's default toolchain"*, *"CMake 4.1 or newer"*); `CMakeLists.txt:33` `cmake_minimum_required(VERSION 4.1)`; o job Ubuntu usa `g++-14` (`ci.yml:258-275`). Precedente de trazer o CMake de fora no preparo: o job Windows baixa o zip oficial da Kitware (passo "Instalar CMake >= 4.1 (Windows)"), e o Ubuntu fazia o mesmo até 27/08 (comentário em `ci.yml:165-170`).

| Alvo | GCC padrão | Caminho para GCC 14 (fonte) | CMake | Caminho para CMake 4.1 | Veredito |
|---|---|---|---|---|---|
| Fedora | 16.2 (FATO, hospedeiro e docs do projeto) | já atende | 4.3.0 (log do CI) | já atende | ok |
| Ubuntu 26.04 | INF: 15 | `gcc-14` 14.3.0 no resolute (FATO, log e packages.ubuntu.com) | 4.2.3 (log) | já atende | ok (é o de hoje) |
| Ubuntu 24.04 | 13 (FATO, `qa-pop-os-22.yml:15-16` e `README.md:78`) | **`gcc-14` 14.2.0 no universe do noble** (packages.ubuntu.com/search?keywords=gcc-14) | INF: 3.28 (`README.md:78` diz que o apt do 24.04 fica abaixo do piso) | tarball oficial da Kitware, no preparo | ok com preparo |
| Mint 22 | 13 (base noble, INF) | o mesmo `gcc-14` do noble (INF: o Mint 22 usa o repositório do Ubuntu 24.04) | o mesmo do noble | Kitware | ok com preparo; a INF se confirma no primeiro job |
| Debian 13 | INF: 14 | `gcc-14` 14.2.0-19 em trixie (FATO, packages.debian.org) | 3.31.6 (FATO) | `trixie-backports` 4.3.4 (FATO) | ok com preparo |
| Rocky 9 | 11 (FATO, TODO.md:561) | **`gcc-toolset-14-gcc-c++-14.2.1-13.el9`** no AppStream (FATO, listagem de `dl.rockylinux.org/pub/rocky/9/AppStream/.../g/`; também existe o toolset 15) | **3.31.8** (FATO, mesmo espelho) | Kitware | ok com preparo |
| Tumbleweed | INF: 15 (rolante) | já atende (INF) | INF: 4.x | já atende (INF) | ok; o passo de piso confirma |
| Manjaro | INF: 15/16 (rolante, repositório do Arch com atraso) | já atende (INF) | INF: 4.x | já atende (INF) | ok; o passo de piso confirma |
| Arch, CachyOS | 16 (hoje no CI) | já atende | 4.x | já atende | ok |
| Pop!_OS 24.04 (base noble + repositório do Pop) | 13 (INF: o repositório do Pop não sobrepõe GCC) | `gcc-14` do noble | 3.28 (INF) | Kitware | ok com preparo |
| Zorin OS 18 (base noble + PPAs do Zorin, lançado em 14/10/2025, ubuntuhandbook.org) | 13 | `gcc-14` do noble | 3.28 (INF) | Kitware | ok com preparo |

**Nota sobre o `gcc-toolset` do Rocky (INF, a provar em A7c):** o toolset liga partes novas do `libstdc++` estaticamente (`libstdc++_nonshared`) e usa o `libstdc++.so.6` do sistema (GCC 11) para o resto. Um consumidor externo precisa do mesmo toolset. É comportamento de empacotamento da distro, não do GlintFx, e fica declarado no `PACKAGING.md` na mesma fatia.

**Nota sobre o Python (INF):** o Rocky 9 traz `python3` 3.9.25 (FATO, espelho BaseOS). O piso de versão dos portões Python **nunca foi medido**. O grep por `match`, anotação `X | None` e `tomllib` não achou nada, mas isso não prova ausência. A7c é a medida.

### 3.3 Decididas pelo líder (24/09/2026, `AskUserQuestion`, registradas na L-04 e em `DECISOES_AUTONOMAS.md`)

**VL-1, Debian:** *"Trocar por Debian 13 (Recomendado)"*. O alvo é `debian:13` ("trixie", a stable atual).
- FATO (packages.debian.org): `gcc-14` 14.2.0-19 em trixie; `cmake` 3.31.6 em trixie e **4.3.4 em `trixie-backports`**; `kwin-wayland` 6.3.6.
- O CMake vem do backports oficial da própria Debian, não da Kitware: preparo nativo da distro, preferido quando existe.

**VL-2, Pop!_OS e Zorin OS:** *"Imagem real de cada um (Recomendado)"*: base `ubuntu:24.04` mais o repositório oficial de cada distro.
- **Pop!_OS 24.04:** repositório oficial `https://apt.pop-os.org/release`, suíte `noble`, componente `main` (FATO: o `Release` responde `Description: Pop!_OS Release noble 24.04`, datado de 23/09/2026).
  - **Esse repositório sobrepõe a pilha que o GlintFx usa:** `libwayland-client0 1.23.1-3pop1` e Mesa `26.1.6-1pop0` (`libegl-mesa0`, `libgl1-mesa-dri`), FATO, lido do índice `main/binary-amd64/Packages`.
  - Ou seja, o Pop real roda outra libwayland e outro Mesa que o Ubuntu 24.04. É exatamente o que o substituto do astrometrica esconderia, e é a prova de que a decisão do líder muda o que se testa.
- **Zorin OS 18:** os pacotes vêm dos PPAs oficiais no Launchpad, `ppa.launchpadcontent.net/zorinos/{stable,patches,apps,drivers}/ubuntu`, suíte `noble` (FATO: 231, 261, 358 e 21 pacotes nos índices `main/binary-amd64`).
  - O domínio `packages.zorinos.com` publica `Release` para `noble`, mas com índices de pacote **vazios** (FATO: `Packages` de 0 bytes em `stable`, `patches` e `apps`, lido também com cabeçalho de cliente APT).
  - Nos PPAs `stable` e `patches` não achei sobreposição de libwayland, Mesa, GCC ou CMake (FATO, grep nos índices); a diferença medida é de `base-files` (`13ubuntu10.4+zorin2`) e de configuração (`zorin-os-default-settings 18.1.9`).
  - INF: o Zorin 18 difere do Ubuntu 24.04 na camada de desktop, não na pilha que o GlintFx toca. O job próprio continua obrigatório (L-04 item 3), e barato.
- **Como a imagem nasce:** no preparo (`tools/ci/env/pop-os-24.sh`, `tools/ci/env/zorin-18.sh`), a partir de `ubuntu:24.04`: chave de assinatura do repositório conferida por impressão digital escrita no script (nunca `[trusted=yes]`), `apt-get update && apt-get dist-upgrade` (para as sobreposições valerem), e o pacote de identificação da distro (`pop-default-settings`/`zorin-os-default-settings`, se instalável sem desktop) para o `/etc/os-release` dizer o que é. O preparo **imprime** `/etc/os-release` e as versões de libwayland e Mesa, e reprova se elas forem as do Ubuntu puro (senão o job seria o substituto de novo, em silêncio).

**VL-3, análises:** *"Só no Fedora (Recomendado)"*. `lint`, `sanitizer`, `debug`, `clang` e `gl-codegen-host-cross` continuam só no primário. Toda distro roda a suíte `ctest` (compartilhado e estático) e o container nas duas pernas.

**Frase do `python3`:** *"Corrigir (Recomendado)"*. Corrigida pelo main em `GODS_LAWS.md:170` e no gêmeo `ESCOPO.md:80`.

## 4. O desenho

### 4.1 Um workflow com job de matriz por sistema, não um workflow por sistema (D-A1)

O que o astrometrica ganha com arquivos separados, e como cada ganho fica aqui:

| Propriedade (texto da L-04 de 24/09) | Como o astrometrica faz | Como fica aqui |
|---|---|---|
| Cada sistema independente e em paralelo | workflows separados | entradas de matriz, `fail-fast: false` (já é assim) |
| Um vermelho não esconde os passos seguintes do mesmo sistema | (não faz: um step vermelho pula os seguintes lá também) | A2: `if: ${{ !cancelled() && steps.<pré-requisito>.outcome == 'success' }}` em todo passo de teste, mais o resultado agregado |
| Conserto por sistema só no preparo | o diff mexe só no `qa-<distro>.yml` | `tools/ci/env/<sistema>.sh`: o diff do conserto mexe só nesse arquivo; um portão (A3) proíbe passo condicionado a sistema fora do preparo |
| Rerun só de falha de infraestrutura, no máximo uma vez | prática da sessão | A4: trava mecânica no primeiro passo de cada job |
| Paridade da L-04 e `PARITY-GATE` intactos | (não tem) | `needs:` no mesmo run, agora por sistema (A1) |

Por que não um arquivo por sistema:
1. O `parity` precisa dos inventários **do mesmo commit, no mesmo run**. Entre workflows, só com `workflow_run` (que dispara a partir do arquivo do ramo padrão) ou com download de artefato de outro run, localizado pelo SHA. Isso é um portão novo que pode mentir sobre o "mesmo commit".
2. Onze portões leem o `ci.yml` (2); em quinze arquivos, cada um teria de aprender a ler N arquivos.
3. O único ganho real de arquivos separados, o selo por distro no README, se reproduz com o resumo do job (`$GITHUB_STEP_SUMMARY`) e uma tabela por sistema no `parity`.

### 4.2 Passos que não se pulam, e o inventário que continua completo (A2)

- **Pré-requisito explícito:** cada job tem um passo de preparo (`id: prep`) e um de construção (`id: build`; no container, "imagem + compositor de pé"). Todo passo de teste leva `if: ${{ !cancelled() && steps.build.outcome == 'success' }}`: roda mesmo que um teste irmão tenha falhado e só pula se aquilo de que ele **precisa** falhou. Passos que independem do build (controles negativos de isolamento, publicação de artefato, resumo) levam `if: ${{ !cancelled() }}`.
- **O nome entra no inventário ANTES de o teste rodar:** `echo "<nome>" >> parity_inventory.txt` passa para a primeira linha do passo, seguida do teste. O teste grava `<nome>\t<código de saída>` em `results.tsv`, lendo o código de variável (memória `feedback_codigo_de_saida_de_variavel`). O inventário diz "este teste existe e foi chamado neste sistema" (paridade); `results.tsv` diz se passou (veredito do job). Hoje as duas coisas estão fundidas, e uma falha vira buraco de paridade.
- **Cabeçalho de completude:** o inventário ganha uma linha `STATUS: completo` ou `STATUS: incompleto (pre-requisito=<id do passo>)`, escrita pelo passo agregador. `check_test_parity.py` trata perna incompleta como **reprovação própria**, com o nome do pré-requisito, e não como N testes faltando. Assim o `parity` não mente sobre a causa.
- **Resultado agregado:** último passo do job, `if: ${{ !cancelled() }}`. Imprime `declarados: D, executados: E, passaram: P, falharam: F`, sempre, mesmo quando zero (L-40 do projeto, L-43 global). Reprova se `F > 0`, se `E ≠ D` ou se `D = 0`. O job já fica vermelho sozinho com um passo vermelho; o agregado existe para a contagem aparecer e para `E ≠ D` pegar um passo que não rodou sem ninguém ver.
- **Publicação do inventário:** `if: ${{ !cancelled() }}`, sempre.
- **Linux e Windows (matriz `linux`/`windows`):** `ctest` já roda todos os testes mesmo com falhas, e o `ctest -N` do inventário já é `if: always()` (comentário em `ci.yml:968`). Lá, A2 só troca `always()` por `!cancelled()`, conferido pelo portão, e acrescenta o agregado. O grosso da fatia é o job `wayland-container`.

### 4.3 Paridade por sistema (A1)

- Artefatos passam a se chamar pelo **sistema**: `parity-inv-<slug>-<modo>` e `parity-inv-<slug>-container`, no lugar de `strategy.job-index`.
- `check_test_parity.py` ganha o modo `--per-system`: o conjunto de referência é a união de todos os sistemas; **cada** sistema é comparado com ela, descontadas as exceções **com a chave do sistema** (o formato `windows|...|SEM-PENDENCIA` de `tests/parity_exceptions.txt` já tem chave de sistema, e passa a aceitar os slugs Linux). Um nome ausente num só sistema reprova com o nome do sistema.
- O modo `--compare` Linux × Windows continua, como leitura agregada.
- **Piso:** o número de sistemas com inventário tem de ser igual ao número da lista de alvos. Um sistema sem inventário reprova, nunca é pulado (L-40).
- `check_measured_parity.py` recebe o mesmo tratamento: presença do fato medido por sistema.

**Valor MEASURED entre N sistemas (A1b, D-A10).** Para cada chave, `check_measured_parity.py --per-system` monta a partição valor → sistemas (ex.: `1: arch,fedora,ubuntu,windows | 0: cachyos`) e classifica:
- **iguais:** todos os sistemas que medem a chave dão o mesmo valor.
- **divergentes:** qualquer diferença. Não há sistema de referência nem voto de maioria. A partição é impressa inteira, e nenhum lado é apontado como o certo.
- **instável no sistema X:** a mesma chave com valores diferentes dentro de UM sistema (pernas compartilhado/estático/container/asan). É seção própria, nunca colapsada escolhendo um valor.
- As seções de presença (herdada/obrigatória) continuam como estão.
- `tests/measured_exceptions.txt`, coluna `lado`, ganha vocabulário para N sistemas:
  - `todos`: o valor pode diferir entre quaisquer sistemas e dentro de um sistema (métrica do executor: tempo, contagem de dispositivo, texto de driver);
  - `familias`: os sistemas Linux têm de concordar entre si; só Linux × Windows pode diferir (mecanismo Wayland × Win32);
  - `linux`, `windows`, um slug (ex.: `rocky-9`) ou uma lista de slugs separada por vírgula: ausência esperada naquele(s) sistema(s).
  - `ambos` deixa de existir. As linhas que o usam hoje são migradas, uma por uma, no mesmo commit: `todos` quando a própria razão diz que o valor depende do executor; `familias` quando a razão é mecanismo de plataforma. O revisor confere cada uma contra a razão escrita. Depois da migração, `ambos` reprova como formato inválido.
- Com `familias`, uma divergência entre duas distros é **não declarada**.
- A regra de morte por chave (item concluído no TODO.md) passa a valer também no modo por sistema.
- Métrica de fechamento, por sistema: `divergentes não declaradas + instáveis não declaradas + obrigatória = 0`. O portão continua RELATÓRIO (só reprova pelo piso de varredura vazia e pela regra de morte), como o `--compare` de hoje; fechar é decisão da onda.
- A lista de slugs esperados vem da mesma fonte que o portão de uniformidade (A3) usa, nunca de uma tupla escrita no script (hoje `PER_SYSTEM_SLUGS_ESPERADOS`, check_measured_parity.py, ae5dd7e).
- Linha de escopo sempre impressa: `chaves: N; sistemas por chave: mín..máx; iguais/divergentes/instáveis/herdada/obrigatória: a/b/c/d/e`.

**Decisão D-A10.** Unanimidade, sem árbitro. Uma chave MEASURED é "igual" só se TODOS os sistemas que a medem dão o mesmo valor; qualquer diferença é divergência; não há sistema de referência (nem Fedora, nem maioria). Opções: (a) maioria; (b) Fedora como referência; (c) unanimidade (escolhida). Contra (a): a maioria erra quando o defeito é comum a várias distros (o mesmo Mesa em 4 delas). Contra (b): o Fedora é primário para "falhar quando a máquina do líder falharia" (ESCOPO §1), não fonte de verdade de comportamento, e uma chave que falta no Fedora ficaria sem comparação. (c) é a leitura literal da L-04 e a prática do Interop do WPT (github.com/web-platform-tests/interop/blob/main/2024/README.md; wpt.fyi/interop-2024). Métrica que varia legitimamente é declarada com razão. Mão única: não. Reverter: barato.

### 4.4 Preparo por sistema e uniformidade (A3)

- `tools/ci/env/<slug>.sh`, um por alvo Linux, com três responsabilidades e nada mais: instalar (dnf/apt/pacman/zypper, `gcc-toolset`, Kitware), exportar `CC`/`CXX` para o passo seguinte (`$GITHUB_ENV`) e imprimir as versões. `python3` é instalado **explicitamente** em todos (3.4).
- A matriz passa a ter só `nome`, `slug`, `imagem`, `modo`, `builddir`, `shared_flag`, `oracle_flag`; `cc`/`c_compiler`/`instalar` saem da matriz e vão para o script.
- **Passo "Piso de ferramentas"** comum, depois do preparo: falha se `__GNUC__ < 14`, se `cmake --version` < 4.1, se não existe `python3`, e se falta `xdg-shell.xml`. Hoje o piso só aparece como erro de compilação lá na frente; o passo o antecipa com a mensagem certa.
- **Portão novo, `check_ci_system_uniformity.py`**, com autoteste:
  - (1) entradas de matriz dos jobs por sistema diferem só nas chaves permitidas;
  - (2) nenhum passo fora do preparo tem `if:` que cite sistema/slug/imagem;
  - (3) todo `tools/ci/env/*.sh` tem entrada de matriz, e vice-versa;
  - (4) todo alvo da lista do `ESCOPO.md` §1 tem entrada (L-04 item 3), com piso.
- `check_pkg_dep_coverage.py` passa a ler os scripts de preparo no lugar do campo `instalar`.

### 4.5 Política de rerun, mecânica (A4)

- Primeiro passo de todo job: `rerun_guard`. Com `github.run_attempt == 1`, não faz nada. Com `>= 3`, reprova ("no máximo uma reexecução"). Com `== 2`, consulta pela API do próprio GitHub (`GITHUB_TOKEN`, permissão `actions: read`) o job de mesmo nome na tentativa 1 e permite seguir **só** se o passo que falhou lá era de preparo (`prep`, inicialização de container, checkout). Se falhou num passo de teste, reprova: "a segunda falha é tratada como real, e falha de teste não se reexecuta".
- Classificação da falha em todo job vermelho: o agregado escreve `FALHA DE INFRAESTRUTURA (antes de teste)` ou `FALHA DE TESTE` no resumo, e é daí que o orquestrador decide se reexecuta.
- Autoteste `rerun_guard_selftest` com respostas da API gravadas em fixtura (falha de preparo → permite; falha de teste → reprova; tentativa 3 → reprova; resposta vazia da API → reprova, nunca "não achei, então pode").

### 4.6 O container por sistema (A8), depois da Parte III

- O `Containerfile` recebe `ARG BASE_IMAGE` e `ARG SYSTEM_SLUG`. O estágio de ferramentas passa a ser `FROM ${BASE_IMAGE}` e roda o **mesmo** `tools/ci/env/<slug>.sh` (uma receita só por sistema, L-17). O estágio final também é `FROM ${BASE_IMAGE}`, com o KWin e o Mesa daquela distro.
- **Compositor da própria distro (D-A5):** `kwin-wayland` existe em todas as distros com caminho para o piso. Fedora/Arch/CachyOS/Manjaro/Tumbleweed: Plasma 6. Ubuntu 26.04: 6.6.x (packages.ubuntu.com). Ubuntu 24.04, Mint 22, Pop!_OS 24.04 e Zorin OS 18: 5.27.11 do noble (o repositório do Pop não traz `kwin-wayland` próprio, FATO, grep no índice). Rocky 9: 5.27.12, via **EPEL 9** (FATO, listagem de `dl.fedoraproject.org/pub/epel/9/`). Debian 13: 6.3.6 (FATO, packages.debian.org).
- **Risco declarado (INF):** fixturas escritas contra KWin 6 podem divergir no 5.27 (ordem de `configure`, estados de `xdg_toplevel`). Uma divergência dessas é pergunta legítima de comportamento da biblioteca no sistema do usuário, e cai na L-04 como qualquer outra. `--virtual` existe no KWin 5 (INF; se não existir, a fatia para e decide).
- A topologia de isolamento (L-09) **não muda**: compositor e cliente no mesmo container, sem montagem do hospedeiro. Os controles negativos de `check_isolation.sh` rodam **em cada sistema** e têm de morder em cada um.

### 4.7 O Python (gêmeo L-44)

- **FATO:** `GODS_LAWS.md:170` e `ESCOPO.md:80` dizem *"`python3` está instalado de fábrica nos cinco alvos"* (o briefing citou `:169`; a linha é a 170). O próprio CI já contradiz isso para imagem de container: Fedora, Ubuntu e Arch **instalam** `python3` no `instalar` (`ci.yml:172-195`). "De fábrica" vale para instalação desktop, não para imagem.
- **Como o plano verifica:** todo `tools/ci/env/<slug>.sh` instala `python3` explicitamente; o passo "Piso de ferramentas" reprova sem `python3` e imprime a versão; o portão de uniformidade (4.4, item 3) reprova script de preparo sem `python3`. A versão mínima real se mede no Rocky 9 (3.9) em A7c.
- **Frase do cânone:** corrigida pelo main por decisão do líder (*"Corrigir (Recomendado)"*), em `GODS_LAWS.md:170` e `ESCOPO.md:80`.

### 4.8 `ctest` paralelo sem falsear a suíte (A5, parte de `CI-SPLIT-PER-OS` por ordem de 24/09, commit `7d064ff`)

**FATOS que limitam o desenho:**
- A suíte leva 450 s serial no Fedora e 468 s no Windows (seção 2).
- Os 7 portões mais lentos (~405 s) montam, cada um, um CMake + Ninja **inteiro** num `mktemp -d`. Cada um desses Ninja já usa todos os núcleos sozinho.
- Teto padrão por teste: **120 s** (`cmake/GlintfxTest.cmake:25-27`, `DART_TESTING_TIMEOUT 120`); só 4 testes têm teto próprio (2 × 300 s, 2 × 900 s). O `pkgconfig_test` levou **112,9 s** serial, e o comentário dele em `tests/CMakeLists.txt` registra que já estourou o teto no CachyOS.
- Um único teste tem `RUN_SERIAL` (`dep_zero_trace`, `:3403`, que reconfigura o próprio diretório de build). Onze registros recebem o diretório de build como argumento (`${PROJECT_BINARY_DIR}`/`${CMAKE_BINARY_DIR}` entre aspas em `tests/CMakeLists.txt`); quais **escrevem** nele ainda não foi medido.
- **A memória `feedback_carga_concorrente_falseia_suite`:** 43, 93 e 97 falhas em rodadas seguidas, todas falsas, com **outros agentes compilando ao mesmo tempo**; sozinha, a suíte fechou 193/193. O sintoma é **o número que muda**. Mecanismos plausíveis (INF): teto de tempo estourado por disputa de CPU, dois processos no mesmo caminho fixo, memória.

**O desenho, um mecanismo por causa:**
1. **Estado compartilhado, declarado e não deixado à sorte.** A fatia lê os 11 registros que recebem o diretório de build e classifica cada um: só lê ou escreve. Os que escrevem ganham `RESOURCE_LOCK glintfx_build_dir`, e o `RUN_SERIAL` do `dep_zero_trace` passa a esse mesmo trinco. Pela fonte (scivision.dev/cmake-resource-lock-ctest), `RESOURCE_LOCK` serializa só quem disputa o recurso, não a suíte inteira. Caminho fixo fora do build (nome em `/tmp` sem `mktemp`) é defeito do teste e se conserta nele.
2. **Disputa de CPU, resolvida por `PROCESSORS`.** Os testes que montam build aninhado declaram `PROCESSORS <n>`, e o `ctest` agenda pelo total declarado, sem empilhar quatro Ninja de quatro núcleos numa máquina de quatro. O `n` sai de medida (P2), nunca de palpite.
3. **Paralelismo = núcleos da máquina que roda, lido na hora e impresso:** `ctest --parallel "$(nproc)"` no Linux, `$env:NUMBER_OF_PROCESSORS` no Windows. No `tools/preci.sh`, limitado também pelo teto de CPU da L-11 global. O **número de testes** não depende disso (armadilha "contagem por núcleo" do `GATE-ENV-SWEEP`): o inventário vem do `ctest -N`, que não muda com `-j`.
4. **Teto de tempo recalibrado sob `-j`, com a folga fixada agora:** para todo teste, `TIMEOUT ≥ 3 × (maior duração sob -j em 5 rodadas)`, arredondado para cima em múltiplos de 30 s. Nenhum teto é **baixado** nesta fatia.
5. **Isolamento da rodada:** no CI, cada job é uma máquina própria, e a carga concorrente da memória não existe lá. No `tools/preci.sh`, a rodada paralela exige `docker ps` vazio e nenhum outro build do projeto no ar; havendo, o script **recusa** e diz por quê, nunca roda "assim mesmo" (item 5 da memória).
6. **Nada de repetição automática:** cada teste roda **uma vez**. `--repeat until-pass` e afins ficam proibidos por portão, porque esconderiam exatamente o "número que muda".

**Calibração e prova (critério fixado agora, L-43):**

| Medida | Como | Critério |
|---|---|---|
| P1: serial × paralelo | por sistema e modo, 1 rodada serial + **5 rodadas `-j`** no próprio runner do CI (matriz de calibração só no PR da fatia) | **conjunto de aprovados idêntico nas 6 rodadas**, em todos os sistemas. Qualquer diferença reprova e vira investigação com nome, **nunca** reexecução |
| P2: folga de tempo | maior duração de cada teste nas 5 rodadas `-j` | todo teste abaixo de `TIMEOUT / 3`. Quem não estiver ganha teto novo pela regra 4, e a lista sai no relatório |
| P3: ganho | duração do passo de testes | Linux ≤ **50%** da serial (Fedora hoje: 450 s); Windows ≤ 50% (hoje: 468 s) |
| P4: estreia do trinco | cópia fora da árvore sem o `RESOURCE_LOCK` de um teste que escreve no build | o P1 dessa cópia tem de mostrar diferença. Se não mostrar, o teste não disputava nada e o trinco não fica "por via das dúvidas" |
| Trivialidade | `-j 1` | idêntico ao serial de hoje |
| Linha de escopo | sempre impressa | "testes: N, com PROCESSORS: a, com RESOURCE_LOCK: b, paralelismo: j" |

### 4.9 `clang-tidy` do Windows em partes (A6)

**FATOS:** passo "clang-tidy - src/" com 1293 s no `36036115151` e 952 s no `36023489188`. Já é paralelo dentro do job (`ForEach-Object -Parallel`, `ci.yml:2788`; grau `NUMBER_OF_PROCESSORS`, `:2774`). A lista de arquivos vem do `compile_commands.json` (`:2651`), e há piso de varredura sobre `src/platform/win32` (`:2552-2580`). O portão `check_windows_static_parallel.py` lê o job `windows-lint`.

**O desenho:**
- `windows-lint` vira matriz `parte: [1, 2, 3]` (INF: ~430-650 s cada; o número final sai de L1).
- A lista ordenada é **repartida por índice** (arquivo *i* vai para a parte `i mod 3`): determinístico, sem depender de nome nem de tamanho. Cada parte imprime a própria lista e publica `winlint-parte-<k>`.
- O job `windows-lint-uniao` (`needs: windows-lint`, `if: !cancelled()`) confere que **a união das partes é igual à lista inteira** do `compile_commands.json`, sem arquivo repetido, com total maior que zero (L-40 do projeto). Reprova se alguma parte não publicou a lista.
- O piso de `src/platform/win32` vale na **união**, não por parte: uma parte pode, legitimamente, não receber arquivo Win32.

| Medida | Critério (fixado agora) |
|---|---|
| L1: parte mais longa | ≤ **50%** do job de hoje (1345 s no `36036115151`) |
| L2: cobertura | união = lista inteira; 0 repetidos; 0 faltando |
| L3: mesmo achado | um defeito de clang-tidy plantado em cópia aparece com a mesma mensagem com e sem partes |
| Trivialidade | `parte: [1]` reproduz o job de hoje arquivo por arquivo |

## 5. Critérios fixados ANTES de medir (L-43)

| Métrica | Como mede | Critério |
|---|---|---|
| **C1: passos pulados com pré-requisito verde** (primária de A2) | PR com uma fixtura sabotada (cópia; sabotagem lida pelo blob comitado, L-27), `gh api .../jobs` | **0** (hoje: **22**, FATO) |
| **C2: buraco de paridade num sistema só** (primária de A1) | autoteste com inventário sem um nome só no CachyOS | reprova, citando `cachyos` (hoje passa, pela união) |
| **C3: cobertura de alvos** | contagem de sistemas com inventário no `parity` | igual à lista do `ESCOPO.md` §1, após as decisões do líder |
| **C4: falha de teste não vira falha de paridade** | o mesmo PR da C1 | `parity` **verde** em cobertura; job do sistema **vermelho**, com `falharam: 1` no agregado |
| **C5: rerun** | autoteste de A4 | 4 de 4 casos no veredito certo |
| **Trivialidade** | run sem falha nenhuma | agregado `declarados = executados = passaram`, com `declarados` ≥ contagem do run anterior por sistema |
| **C6: suíte paralela não falseia** (A5) | P1 de 4.8 | conjunto de aprovados idêntico em 1 serial + 5 paralelas, por sistema e modo |
| **C7: clang-tidy em partes** (A6) | L1-L3 de 4.9 | parte mais longa ≤ 50% de hoje; união = lista inteira |
| **Veto T: tempo** | parede de um run verde completo, e o atraso de fila de cada job (`started_at - created_at`) | parede ≤ **60 min**. Acima disso, **para** e volta ao líder com os números, sem cortar distro |
| **Veto Q: fila** | maior atraso de fila | informativo; entra no relatório de A9 |
| Linha de escopo | sempre impressa | "sistemas: N na lista, N com job, N com inventário, N verdes" |

## 6. Efeito no tempo de fechamento de cada fatia (INF, a partir dos jobs medidos)

| Cenário | Jobs | Job-minutos | Parede estimada |
|---|---|---|---|
| Hoje (FATO, `36036115151`) | 25 | 210 | 22,5 min (medida) |
| + 7 distros novas no `linux` (×2 modos, ~11 min cada) + container por sistema (12 × 2 pernas, ~7 min cada com a imagem de hoje) | ~67 | ~560 | **~35-45 min**: piso de 28 min só pela fila de 20 |
| + A5 (`ctest -j`: Linux de ~11 para ~6 min, Windows de ~13 para ~9, pelo P3) | ~67 | ~430 | ~28-32 min |
| + A6 (clang-tidy em 3 partes: caminho crítico isolado de ~22 para ≤ 11 min, 2 jobs a mais) | ~69 | ~435 | ~28-32 min (a fila manda) |
| + imagem incremental (container de ~7 para ~4 min) | ~69 | ~360 | **~25-30 min** |

- Com a ampliação, quem manda é a **fila de 20**, não um job isolado. A6 tira o `windows-lint` do caminho crítico, mas o ganho de parede só aparece quando a soma de job-minutos cai (A5 e a Parte III).
- **Ordem escolhida por isso:** A5 e A6 entram **antes** das distros novas (A7). Cada fatia de distro já fecha no CI mais barato.
- Um conserto que exige nova rodada custa a parede inteira de novo. O ganho do modelo é que o conserto começa **na primeira falha** e é **por sistema**.

## 7. As fatias (esquema v1)

Ordem: A0 → A1 → A1b → A2 → A3 → A4 → A5 → A6 → A7a/A7b/A7c/A7d/A7e (uma por vez) → **Parte III (B0-B3)** → A8 → A9. Um implementador por vez (todas tocam `ci.yml`). Nada de filtro por caminho (L-04 item 2). O oráculo de camadas continua ligado só nas pernas compartilhadas do servidor (L-45). **A1b roda antes de A3** (a fonte única de slugs que A3 cria) por decisão de sequência, não do plano original: `PER_SYSTEM_SLUGS_ESPERADOS` (check_measured_parity.py) segue como constante própria, DECLARADA como duplicata temporária da mesma forma que `SISTEMAS_PER_SYSTEM_ESPERADOS` (check_test_parity.py, ae5dd7e) já é - A3 herda a obrigação de varrer e unificar AS DUAS cópias na fonte única, não só a do script MEASURED.

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| A0 | Linha de base e critério | CI | Nenhum arquivo rastreado. `/var/tmp/glintfx-plan/ci-split-baseline.md`: parede, job-minutos e atraso de fila dos 5 últimos runs verdes; lista de passos pulados de cada job vermelho recente; a tabela 3.1 com o status de VL-1/VL-2 | O vermelho já está medido: 22 passos pulados no plain de `36036115151` (C1 > 0). Se o levantamento mostrar 0 pulados nos 5 runs vermelhos, a fatia para: A2 não teria o que consertar | `parity_textual_test` (continua verde) | `parity_textual_test` | inalterado |
| A1 | Paridade por sistema: presença | CI | `tests/tools/check_test_parity.py` (`--per-system`, perna incompleta como reprovação própria, piso de sistemas), `tests/tools/check_measured_parity.py` (`--per-system`, só presença: herdada/obrigatória por sistema, reusando `classify_unilateral()`), `.github/workflows/ci.yml` (nomes de artefato por slug; `parity` baixa por sistema), `tests/parity_exceptions.txt` (chave de sistema aceita os slugs) | Autoteste C2: inventário sem um nome só no `cachyos`. Contra o código de antes, **passa** (união), e isso é o vermelho. Depois reprova citando `cachyos`. Mutantes: voltar à união → C2 passa (tem de reprovar); piso de sistemas desligado → um sistema sem inventário passa calado | `test_parity_selftest`, `check_measured_parity_selftest`, `parity_textual_test` | `test_parity_selftest` (o lado Windows é um sistema da lista como os outros) | o próprio portão |
| A1b | Paridade por sistema: valor MEASURED por unanimidade (D-A10) | CI | `tests/tools/check_measured_parity.py` (`--per-system` com partição valor → sistemas, seções iguais/divergentes/instáveis, regra de morte por chave, slugs da fonte única de A3), `tests/measured_exceptions.txt` (`lado` com `todos`/`familias`/slug/lista; migração linha a linha de `ambos`), `.github/workflows/ci.yml` (o job `parity` chama o modo por sistema com `--measured-exceptions`/`--todo`) | Autoteste com 5 sistemas e a chave `k` = 1 em fedora, ubuntu, arch e windows, e 0 em cachyos, sem exceção. Contra ae5dd7e o modo por sistema **não diz nada** do valor (só presença), e isso é o vermelho. Depois: `k` em "divergentes não declaradas" com a partição `1: arch,fedora,ubuntu,windows \| 0: cachyos`. Mutantes que têm de morrer: (M1) voto de maioria → `k` vira "iguais"; (M2) Fedora como referência → uma 2ª fixtura em que o fedora NÃO mede `k` e ubuntu≠arch tem de sair divergente (M2 não compara nada); (M3) `familias` tratado como `todos` → chave declarada `familias` com ubuntu≠fedora tem de sair não declarada; (M4) `ambos` ainda aceito depois da migração → reprova como formato inválido; (M5) valores diferentes entre as pernas do MESMO sistema colapsados num só → fedora shared=1/static=0 tem de sair em "instável no sistema fedora"; (M6) piso: nenhuma chave em nenhum sistema → reprova (L-40) | `check_measured_parity_selftest`, `measured_parity_textual_test` | as mesmas fixturas incluem `windows` como um sistema da lista, sem caso especial | o próprio portão |
| A2 | Passos que não se pulam, inventário declarado antes, resultado agregado | CI | `.github/workflows/ci.yml` (todo passo de teste com `!cancelled()` + pré-requisito; `echo` antes do teste; `results.tsv`; passo agregado; publicação `!cancelled()`), `tests/tools/check_container_fixture_inventory.py` (aceita o `echo` antes do `exec`), portão novo `tests/tools/check_ci_step_independence.py` | C1 e C4 num PR com uma fixtura sabotada: hoje, 22 pulados e `parity` vermelho por "faltando"; depois, 0 pulados, agregado `falharam: 1`, `parity` verde em cobertura, job vermelho. Mutantes do portão novo: tirar `!cancelled()` de um passo → reprova; passo de teste sem pré-requisito nomeado → reprova; publicação do inventário sem `!cancelled()` → reprova | `ci_step_independence_selftest`, `container_fixture_inventory_selftest`, `container_fixture_inventory_test` | as mesmas regras nos jobs `windows*` (o portão lê o arquivo inteiro) | inalterado |
| A3 | Preparo por sistema em arquivo próprio, piso de ferramentas e uniformidade | CI | `tools/ci/env/fedora.sh`, `ubuntu.sh`, `arch.sh`, `cachyos.sh` (os 4 de hoje, **comportamento idêntico** ao `instalar` atual mais `python3` explícito), `.github/workflows/ci.yml` (matriz enxuta, passo "Piso de ferramentas"), portão novo `tests/tools/check_ci_system_uniformity.py`, `tests/tools/check_pkg_dep_coverage.py` (lê os scripts) | Autoteste de uniformidade com um passo `if: matrix.nome == 'Arch - compartilhado'` fora do preparo: nenhum portão de hoje o pega (vermelho); depois reprova. Piso de ferramentas: mutante que exige `__GNUC__ >= 99` reprova nos 4 sistemas; um preparo sem `python3` reprova. Os 4 sistemas de hoje com o mesmo número de testes por perna de A0 | `ci_system_uniformity_selftest`, `pkg_dep_coverage_selftest`, `pkg_dep_coverage_test` | o Windows ganha o passo de piso equivalente (MSVC/CMake 4.1) | inalterado |
| A4 | Rerun mecânico | CI | `tools/ci/rerun_guard.py` (primeiro passo de todo job), `.github/workflows/ci.yml` (`permissions: actions: read`, passo em todos os jobs), portão `tests/tools/check_ci_step_independence.py` ganha a regra "rerun_guard é o primeiro passo" | `rerun_guard_selftest` com 4 respostas gravadas: hoje nada bloqueia a reexecução de falha de teste (vermelho); depois, 4 de 4 no veredito certo (C5). Mutante: guarda que aceita tentativa 3 → reprova | `rerun_guard_selftest`, `ci_step_independence_selftest` | o mesmo passo nos jobs Windows (`pwsh`), com o autoteste rodando nos dois | inalterado |
| A5 | `ctest` paralelo calibrado (`CI-CTEST-PARALLEL`, agora dentro de `CI-SPLIT-PER-OS`) | CI + local | `tests/CMakeLists.txt` (`RESOURCE_LOCK glintfx_build_dir` nos registros que escrevem no build, medidos; `PROCESSORS` nos portões com build aninhado; `TIMEOUT` recalibrado pela regra 4 da seção 4.8), `.github/workflows/ci.yml` (`ctest --parallel` com o grau impresso, Linux e Windows), `tools/preci.sh` (mesmo grau, limitado pela L-11 e recusando rodar com `docker ps` não vazio), portão novo `tests/tools/check_ctest_parallel_policy.py` (proíbe `--repeat`; exige grau impresso; exige que todo registro que recebe o diretório de build declare "só lê" ou tenha o trinco) | P4: cópia sem o trinco mostra diferença no P1 (estreia do trinco). P1 com 1 serial + 5 paralelas por sistema e modo, conjunto de aprovados idêntico. Mutantes do portão: `--repeat until-pass` no `ci.yml` → reprova; registro novo que recebe o diretório de build sem declaração → reprova; grau não impresso → reprova | `ctest_parallel_policy_selftest`, `env_sweep_test` (o número de testes não varia com o grau), `check_ci_timeouts_test` | a mesma política no Windows (`$env:NUMBER_OF_PROCESSORS`), P1 no Windows nos dois modos | inalterado (`ctest -N` idêntico antes e depois) |
| A6 | `clang-tidy` do Windows em partes (`CI-WINLINT-SHARD`, agora dentro de `CI-SPLIT-PER-OS`) | CI | `.github/workflows/ci.yml` (`windows-lint` em matriz `parte`; job `windows-lint-uniao`), `tests/tools/check_windows_static_parallel.py` (entende as partes), `tests/tools/check_ci_timeouts.py` (classe do job de união) | L2 por sabotagem em cópia: uma parte que descarta um arquivo tem de fazer `windows-lint-uniao` reprovar citando o arquivo; hoje não existe união para reprovar (vermelho por ausência). L3: defeito plantado em cópia achado com e sem partes. L1 medido | `check_ci_timeouts_selftest`, `check_ci_timeouts_test` | `windows-lint-uniao` e as 3 partes verdes; L1 ≤ 50% de 1345 s | inalterado |
| A7a | Ubuntu 24.04 e Mint 22 | CI | `tools/ci/env/ubuntu-2404.sh`, `tools/ci/env/mint-22.sh`, entradas de matriz, `tests/parity_exceptions.txt` só se houver ausência **com motivo** | Estreia do piso: o preparo sem o `gcc-14` (só o GCC 13 padrão) tem de **reprovar** no passo "Piso de ferramentas"; com `gcc-14` + Kitware, verde. Depois, os jobs novos verdes com `ctest -N` igual à referência (A1) | `test_parity_selftest` e a suíte inteira nos dois modos, em cada sistema | não se aplica (o Windows não muda) | uma linha por sistema no `parity` |
| A7b | Tumbleweed e Manjaro | CI | `tools/ci/env/opensuse-tw.sh`, `tools/ci/env/manjaro.sh`, entradas de matriz | Rolantes já atendem o piso. A estreia é o mutante `__GNUC__ >= 99` reprovando nos dois, e o `pacman -Syu` do Manjaro (nunca `-Sy`, lição do astrometrica em `2c46be2`) conferido pelo portão de uniformidade | idem A7a | não se aplica | idem |
| A7c | Rocky 9 | CI | `tools/ci/env/rocky-9.sh` (CRB para `wayland-devel`/`ninja-build`; AppStream para `wayland-protocols-devel`, `mesa-libEGL-devel`, `gcc-toolset-14`; Kitware para o CMake), entrada de matriz, parágrafo sobre `gcc-toolset` em `PACKAGING.md` | Sem `gcc-toolset-14`, o piso reprova (GCC 11); com ele, verde. **Python 3.9**: a suíte inteira roda; qualquer portão que exija versão maior aparece aqui, e o conserto é no portão (biblioteca padrão apenas), nunca no preparo | idem A7a | não se aplica | idem |
| A7d | Debian 13 | CI | `tools/ci/env/debian-13.sh` (`gcc-14`; `cmake` de `trixie-backports`), entrada de matriz | Estreia do piso: o preparo sem backports (`cmake` 3.31.6 de trixie) tem de **reprovar** no passo "Piso de ferramentas"; com backports, verde. Depois, os jobs verdes com `ctest -N` igual à referência | `test_parity_selftest` e a suíte inteira nos dois modos | não se aplica | uma linha por sistema no `parity` |
| A7e | Pop!_OS 24.04 e Zorin OS 18, imagens reais | CI | `tools/ci/env/pop-os-24.sh`, `tools/ci/env/zorin-18.sh` (repositório oficial com chave conferida por impressão digital, `dist-upgrade`, pacote de identificação, `gcc-14` do noble, Kitware), entradas de matriz | **Estreia contra o substituto:** o preparo imprime `/etc/os-release` e as versões de libwayland/Mesa e reprova se forem as do Ubuntu puro. Rodado sem a linha do repositório (o substituto do astrometrica), tem de **reprovar**; com ela, o Pop mostra `libwayland-client0 1.23.1-3pop1` e Mesa `26.1.6-1pop0`, e o Zorin mostra `base-files ...+zorin2`. Mutante: chave com impressão digital errada → o preparo reprova (nunca `[trusted=yes]`). Depois, a suíte inteira verde nos dois modos | `test_parity_selftest` e a suíte inteira nos dois modos, em cada sistema | não se aplica | uma linha por sistema no `parity` |
| A8 | Container por sistema | container | `tests/container/Containerfile` (`ARG BASE_IMAGE`/`SYSTEM_SLUG`, preparo pelo mesmo `tools/ci/env/<slug>.sh`, KWin da distro), `.github/workflows/ci.yml` (matriz `wayland-container` = sistemas × {plain, asan}), `tests/container/check_isolation.sh` se o caminho do KWin mudar por distro | Por sistema, antes das fixturas: os controles negativos 1-5 têm de **morder** (montagem proibida, device, `--pid=host`, `systempaths`) em cada imagem nova; um controle que passa em alguma distro reprova a fatia. Depois, as fixturas: toda divergência KWin 5.27 × 6 vira achado com nome, nunca exceção silenciosa | `container_fixture_inventory_test`, `container_kill_order_test`, `container_run_compositor_selftest` e o `wayland-container` inteiro por sistema, nas duas pernas | não se aplica | uma linha `<slug>-container` por sistema no `parity` |
| A9 | Fila e cache | CI | Nenhum código se couber; senão, `.github/workflows/ci.yml` (`cache-to` por escopo) | Medida de 3 runs verdes completos: parede (veto T ≤ 60 min), atraso de fila, `gh cache list` total ≤ 10 GB sem despejo do escopo do Fedora. Estouro de qualquer um = volta ao líder com os números | `check_ci_timeouts_test` (as classes de tempo cobrem os jobs novos) | idem | inalterado |

**Fecha quando:**
- A0 escrito;
- A1-A4 com cada vermelho visto antes do verde e cada mutante matando;
- A5 com P1-P4 cumpridos em todo sistema e modo existente, e A6 com L1-L3;
- A7a-A7e com o piso visto reprovando (e, em A7e, o substituto visto reprovando) e os sistemas verdes, cada um com o `ctest -N` igual à referência;
- A8 com os controles negativos mordendo em cada sistema;
- A9 dentro dos vetos;
- revisão adversarial por agente distinto do implementador, executando os mutantes (L-12).

**Paridade local × CI:** o `tools/preci.sh` continua espelho do **Fedora** (a máquina do líder). Para consertar um sistema localmente, `preci.sh --system <slug>` (sub-fatia de A3) roda a mesma receita do job daquele sistema **dentro de um container da imagem dele**: o mesmo `tools/ci/env/<slug>.sh`, o mesmo configure, a mesma suíte. É um trabalho pesado por vez (L-11 global), com `watchcode` (L-25), e nunca roda o oráculo (L-45). A comparação da lista de jobs local × último run empurrado (`CLAUDE.md`, "Os quatro jobs acrescentados pela onda W1") continua valendo, e fica maior.

---

# Parte III: plano de `CONTAINER-BUILD-PER-FIXTURE-LAYERS` (separado)

## 8. FATOS sobre a imagem

- `tests/container/Containerfile:230` `FROM fedora:44 AS arch-ports-builder`; `:236` `ARG GLINTFX_FIXTURE_SANITIZE=""`; `:237` `dnf install gcc-c++ ... libasan libubsan`.
- `:240` `COPY _arch_ports_src` (a árvore de produção inteira), `:241-260` COPY de **todas** as fixturas, `:279-294` cabeçalhos e o contador de alocação, **tudo antes** do `RUN g++` de `:303`. Pela regra de cache (docs.docker.com/build/cache/optimize/: o código que muda com frequência vai por último), qualquer fixtura alterada invalida o `RUN` inteiro.
- O `RUN` de `:303-~1080` encadeia **23 `g++` por `&&`** (em série, um núcleo); `:1114` é o 24º (`wire_relay_bin`).
- **647 citações de `.cpp` para 84 fontes distintas** (7,7×): os dois arquivos do contador 24 vezes cada; nove fontes de `wayland/` e `core/` 23 vezes cada; o grupo de `gl/`, `log/` e EGL 11 vezes cada.
- Flags iguais em todas: `-std=c++23 -O2 -g $GLINTFX_FIXTURE_SANITIZE -Wall -Wextra -Werror`, com dois grupos de `pkg-config --cflags`, **vazios os dois** no hospedeiro Fedora 44 (medido). Nenhum `-D` por fixtura.
- **CI:** `docker/build-push-action@v7`, `cache-from/cache-to: type=gha, mode=max, scope=wayland-container-<perna>` (`ci.yml:1412-1427`); um segundo build `target: arch-ports-builder` para inspecionar o compilador (`:1451-1462`); passo "Tempo de build da imagem" (`:1429`). O build levou 179 s no plain de `36036115151`.
- **Local:** buildx 0.37.1, builder `default`, driver `docker`, **BuildKit v0.32.2** (medido); 16 núcleos. Nada a instalar.
- **Portões que leem o Containerfile:** `check_container_fixture_includes.py` (`:85`, `:144`, `:173`), `check_container_fixture_link.py` (refaz os 23 `g++` fora do Docker, ~3m30s, `preci.sh:783`), `check_container_fixture_inventory.py` (`:95`), `check_container_smokes_in_compile_db.py`, e o passo do CI que exige o estágio `arch-ports-builder` com `gcc`.
- **Prova de instrumentação da perna asan:** `ldd <fixtura> | grep libasan` por fixtura (`ci.yml:2092-2120`).

## 9. Pesquisa (L-22, L-43; L-29 só para aprender)

- BuildKit constrói só os estágios de que o alvo depende (docs.docker.com/build/building/multi-stage/) e resolve dependências de forma concorrente (github.com/moby/buildkit). INF: estágios irmãos rodam em paralelo; medido em B2.
- *"BuildKit doesn't preserve cache mounts in the GitHub Actions cache by default"*; o contorno é uma ação de terceiro, `reproducible-containers/buildkit-cache-dance` (docs.docker.com/build/ci/github-actions/cache/). `ccache` por cache mount ajudaria só na máquina local e quebraria a paridade local × servidor.
- Cache do GitHub: **10 GB por repositório**, despejo do menos usado, 7 dias sem acesso (docs.github.com, dependency-caching reference).
- `ARG` vale só no estágio onde foi declarado; todo estágio que compila redeclara `GLINTFX_FIXTURE_SANITIZE` (a armadilha já está registrada em `Containerfile:231-235`).

## 10. O desenho

```
arch-ports-toolchain   FROM ${BASE_IMAGE} (fedora:44 até A8), preparo, xdg-shell .h/.c/.o
objs-core, objs-log, objs-wayland, objs-window, objs-gl, objs-alloc-counter
                       FROM toolchain; ARG redeclarado; COPY só do subdiretório do grupo + include/;
                       `g++ -c` uma vez por fonte; afirmação de instrumentação por .o
fx-<nome> (24)         FROM toolchain; ARG redeclarado; COPY só do .cpp daquela fixtura
                       (+ checked_stdio.hpp); COPY --from dos .o que ela usa; liga com a MESMA lista de hoje, em .o
arch-ports-builder     FROM toolchain (mantém o gcc que o passo do CI inspeciona);
                       COPY --from=fx-<nome> /build/<bin> /build/<bin>, 24 linhas
estágio final          inalterado (as 24 COPY --from=arch-ports-builder continuam iguais)
```

- **`arch-ports-builder` como agregador:** o estágio final e o portão de inventário ficam intactos.
- **Lista de ligação explícita, igual à de hoje, em `.o`:** equivalência provável (veto 3). Uma `.a` descartaria objeto com inicializador estático sem aviso; fica fora do v1 (D-B3).
- **Afirmação de instrumentação por objeto (D-B5):** um grupo `objs-*` compilado sem o `ARG` passaria na prova `ldd` de hoje, porque a fixtura ainda liga `libasan`. Cada `objs-*` termina conferindo: com o `ARG` preenchido, todo `.o` tem de ter um `__asan_` indefinido (`nm -u`); vazio, nenhum.
- **Preparado para A8:** `FROM ${BASE_IMAGE}` desde já, com o valor padrão igual ao de hoje, para que a Parte II não tenha de reabrir o arquivo inteiro.

## 11. Critério fixado ANTES de medir (L-43)

| Métrica | Como mede | Critério |
|---|---|---|
| **M1: uma fixtura mudada** (primária) | cache quente; uma linha de comentário no `pump_smoke.cpp` da cópia preparada; `docker build` 3 vezes, mediana; plain e asan | **≤ 60 s** nas duas pernas, com `CACHED` em **23 de 24** estágios `fx-*` |
| M2: uma fonte de produção mudada | idem, num `.cpp` de `src/platform/gl/` | ≤ 50% da mediana de antes |
| M3: M1 no servidor | "tempo de build da imagem" num push que só toca uma fixtura | ≤ 60 s nas duas pernas |
| Trivialidade | build sem mudança | ≤ 15 s antes e depois |
| **Veto 1:** build frio | `--no-cache` | ≤ 110% do frio de antes |
| **Veto 2:** memória | pico de RAM do build frio | dentro do teto da L-11 global; estourou, **para** e vai ao líder (D-B4) |
| **Veto 3:** equivalência | por binário (plain): `nm --defined-only` ordenado e tamanho de `.text` | 24 de 24 iguais |
| **Veto 4:** comportamento | `wayland-container` inteiro, duas pernas, local e servidor | mesma saída, mesmos códigos, `parity_inventory.txt` idêntico |
| **Veto 5:** cache do GitHub | `gh cache list` após dois pushes | ≤ **3 GB** para os 2 escopos de hoje. **Para A8** (~24 escopos): orçamento por escopo ≤ 10 GB / nº de escopos, e a medida entra em A9 |
| Linha de escopo | sempre impressa | "estágios fx: N encontrados, N construídos, N em cache" |

## 12. As fatias (esquema v1)

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| B0 | Linha de base e equivalência | container | Nenhum arquivo rastreado. `/var/tmp/glintfx-plan/image-build-baseline.md`: M1, M2, trivialidade e frio (duas pernas), pico de RAM, `nm`/`.text` dos 24 binários, `gh cache list`, e o tempo de build dos 5 últimos runs | A M1 de hoje tem de sair **> 60 s** (TODO: ~235 s). Se sair ≤ 60 s, para e reporta | `container_fixture_link_selftest` | não se aplica (a imagem só existe no Linux) | inalterado |
| B1 | Portões entendem o formato novo, antes da imagem | container | `tests/tools/check_container_fixture_includes.py`, `tests/tools/check_container_fixture_link.py` (o `--exec` passa a compilar 84 fontes), `tests/tools/check_container_smokes_in_compile_db.py`, `tests/container/prepare_arch_ports_fixture.sh` se a chamada mudar; os dois formatos aceitos na transição | Fixturas de autoteste no formato novo que os portões de hoje reprovam ou contam errado. Mutantes: `fx-*` sem um `.o` necessário → ligação reprova; `.cpp` no estágio errado → includes reprova; zero `fx-*` → piso | `container_fixture_includes_selftest`, `container_fixture_link_selftest`, `container_smokes_in_compile_db_selftest` | não se aplica | inalterado |
| B2 | A imagem em estágios | container | `tests/container/Containerfile` (seção 10) | M1 > 60 s vira ≤ 60 s nas duas pernas, 23/24 `CACHED`, vetos 1-4. Mutantes: tirar o `ARG` de um `objs-*` → a afirmação derruba o build asan; tirar um `.o` → falha de link e B1 reprova antes; copiar fixtura no estágio de ferramentas → M1 volta a > 60 s | `container_fixture_inventory_test`, `container_fixture_link_selftest`, `wayland-container` nas duas pernas | não se aplica | inalterado |
| B3 | Servidor e limpeza | container | push só com uma fixtura mexida; os portões de B1 deixam de aceitar o formato velho | M3 > 60 s antes e ≤ 60 s depois; veto 5. Mutante: fixtura de autoteste no formato velho → reprova | `container_fixture_includes_selftest`, `container_fixture_link_selftest` | não se aplica | inalterado |

**Fecha quando:** B0 escrito com a M1 vermelha; B1 com vermelhos e mutantes; B2 com a M1 verde nas duas pernas, vetos 1-4 e mutantes matando; B3 com a M3 verde e o cache no teto; revisão adversarial por agente distinto (L-12).

**Paridade local × servidor:** mesmo `Containerfile`, mesmo `build-arg`, as mesmas duas pernas. Diferença declarada: driver `docker` local, `docker-container` com `type=gha` no servidor. O conjunto de binários tem de ser o mesmo (veto 4).

---

## 13. Decisões, no formato de `DECISOES_AUTONOMAS.md`

Quem decidiu: Caetano (CTO, `opus`), modo autônomo. Todas: confirmar retroativamente, exceto as marcadas **DO LÍDER**.

**D-A1: um workflow com job de matriz por sistema, preparo em arquivo por sistema.**
- Opções: (a) um workflow por sistema, como o astrometrica; (b) **matriz num workflow, com `tools/ci/env/<slug>.sh` (escolhida)**; (c) workflow reutilizável por sistema chamado pelo `ci.yml`.
- Porquê: (b) entrega as quatro propriedades da ordem (4.1) sem tirar o `parity` do mesmo run. (a) exige juntar inventários entre runs. (c) mantém o run único, mas espalha a receita em N arquivos YAML que onze portões teriam de aprender a ler.
- Mão única: não. Reverter: barato.
- Fontes: seções 1, 2 e 4.1.

**D-A2: paridade por sistema, não por união.**
- Porquê: a união esconde o buraco de uma distro só (FATO, seção 2). Com 12 Linux, a L-04 item 3 só se cumpre comparando cada sistema.
- Mão única: não. Fonte: passo "Unir inventarios por sistema" do `ci.yml`.

**D-A3: o nome entra no inventário antes do teste; resultado em arquivo separado; perna incompleta tem reprovação própria.**
- Porquê: hoje uma falha de teste chega ao `parity` como buraco de cobertura (FATO, `ci.yml:1794-1796`). Separar os dois fatos mantém o inventário completo com passos que não se pulam, e o `parity` diz a causa certa.
- Mão única: não.

**D-A4: rerun controlado por trava no próprio job, não só por prática.**
- Porquê: "no máximo uma vez" e "só infraestrutura" são contáveis. Regra sem trava é a lição da memória `feedback_aviso_no_briefing_nao_e_portao`.
- Custo: permissão `actions: read` no workflow. Mão única: não.

**D-A5: compositor da própria distro, dentro do container da distro.**
- Opções: (a) **KWin de cada distro (escolhida)**; (b) KWin do Fedora num container vizinho, com o cliente da distro ligado por volume nomeado.
- Porquê: (a) mantém a topologia de isolamento da L-09 intacta e testa o que o usuário daquela distro tem. (b) muda a topologia (dois containers, volume compartilhado) e exigiria provar o isolamento de novo.
- Risco declarado: divergência KWin 5.27 × 6 (4.6).
- Mão única: não.

**D-A6: a ampliação vem antes da imagem incremental só até A7; o container por sistema (A8) espera a Parte III.**
- Porquê: multiplicar por 12 uma imagem que recompila 7,7× a mesma coisa custaria ~26 builds de ~3-4 min por run.
- Mão única: não.

**D-A7: `ctest` paralelo calibrado por medida, com trinco declarado e sem repetição automática.**
- Opções: (a) `-j` puro; (b) **`-j` com `RESOURCE_LOCK`/`PROCESSORS` medidos e teto de tempo recalibrado sob carga (escolhida)**; (c) só separar os portões pesados num job próprio.
- Porquê: (a) é o cenário da memória `feedback_carga_concorrente_falseia_suite` dentro de um job só. (c) paralelizaria menos e deixaria os 405 s dos portões em série noutro lugar. (b) ataca cada causa de resultado que muda com um mecanismo, e P1 prova com 6 rodadas.
- Mão única: não. Reverter: `-j 1`.
- Fontes: seção 4.8; scivision.dev/cmake-resource-lock-ctest.

**D-A8: clang-tidy do Windows em 3 partes por índice, com job de união.**
- Opções: (a) partes por diretório; (b) **partes por índice na lista ordenada (escolhida)**; (c) analisar só os arquivos mudados.
- Porquê: (a) desequilibra (o `win32/` concentra o custo, INF). (c) enfraquece o portão, porque mudança de cabeçalho afeta arquivos não mudados. (b) é determinística, e a união prova que nada caiu (L-40).
- Mão única: não.
- Fonte: seção 4.9.

**D-A9: A5 e A6 antes das distros novas.**
- Porquê: cada fatia de distro (A7) fecha num CI mais barato. As duas não dependem da lista de sistemas.
- Mão única: não.

**VL-1, DECIDIDA PELO LÍDER: Debian 13 no lugar do 12.** Seção 3.3.

**VL-2, DECIDIDA PELO LÍDER: imagens reais de Pop!_OS 24.04 e Zorin OS 18.** Seção 3.3, com o repositório de cada uma lido e as sobreposições medidas.

**VL-3, DECIDIDA PELO LÍDER: análises só no Fedora.** Seção 3.3.

**D-B1: estágio por fixtura + objetos por grupo, sem `ccache`.**
- Porquê: tira a redundância de 7,7×, o acoplamento entre fixturas, e funciona igual no local e no servidor.
- Mão única: não.

**D-B2: `ccache` - DECIDIDA PELO LÍDER (24/09/2026, via main): não.** Nada a instalar. Coincide com a recomendação: o cache dele não chega ao servidor sem ação de terceiro.

**D-B3: lista explícita de `.o`, não `.a`.**

**D-B4 (DO LÍDER, se acontecer): paralelismo do BuildKit acima do teto de RAM.** Confirmado pelo líder (24/09/2026, via main) na forma escrita: se limitar o paralelismo exigir mexer na configuração do Docker da máquina, a fatia **para** e a questão volta ao líder.

**D-B5: afirmação de instrumentação por `.o` nasce em B2, com o mutante.**

**Revogada nesta versão (L-67, apagada e não arquivada):** a D-V1 da primeira versão ("não separar o CI por sistema"), substituída pela ordem do líder.

## 14. Leis aplicadas

- **L-04 (projeto):** lista unida por leitura do CI do astrometrica; nenhuma distro tirada por agente (Debian 12 → 13, Pop e Zorin reais: decisões do líder, 3.3); o substituto do astrometrica é recusado como suporte pelo item 3; a paridade fica mais forte (por sistema), nunca mais fraca; nada de filtro por caminho.
- **L-09, L-25, L-45:** nada rodado por mim. O container por sistema mantém a topologia de isolamento, com controles negativos por sistema. O oráculo continua só no servidor.
- **L-11 global:** fila de 20 e RAM entram como vetos; um trabalho pesado por vez no `preci.sh --system`.
- **L-14 (projeto) / L-51 global:** nada instalado nesta máquina. Instalação acontece só dentro das imagens do CI, pelo preparo de cada sistema, que é o que a ordem de 27/08 (*"instale a ferramenta nos que precisarem"*) e a da L-04 de 24/09 cobrem. `ccache` fica marcado como decisão do líder.
- **L-17:** uma receita por sistema, usada pelo job e pelo container; os onze portões que leem o `ci.yml` e os cinco que leem o Containerfile foram listados antes de fatiar.
- **L-22 / L-43 globais:** pesquisa antes do plano (índices de pacote, documentação do GitHub e da Docker, os dois CIs); critérios e vetos fixados antes de qualquer medida nova.
- **L-27:** FATO e INF separados. O "python3 de fábrica" foi tratado como afirmação a medir (L-44), com a linha correta (`GODS_LAWS.md:170`).
- **L-32:** `CI-CTEST-PARALLEL` e `CI-WINLINT-SHARD` deixaram de ser candidatos: por ordem do líder (commit `7d064ff`), entram no escopo de `CI-SPLIT-PER-OS` como A5 e A6, sem abrir segunda trilha.
- **L-40 (projeto):** piso em toda varredura nova (sistemas, passos, estágios, inventário declarado).
