#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/fedora.sh - CI-SPLIT-PER-OS A3b (docs/plano-ci-split-
# per-os.md secao 4.4): preparo do alvo Fedora (primario, GODS_LAWS.md
# L-04). TRES responsabilidades e nada mais (secao 4.4): instalar,
# exportar CC/CXX para o passo seguinte ($GITHUB_ENV) e imprimir as
# versoes. A3c (D-A13): exporta tambem GLINTFX_PREP_SHA (sha256 do
# PROPRIO script) - o passo "Checkout e' repositorio git" do ci.yml
# confere esse valor contra o script real apos o checkout definitivo,
# provando que o script que rodou (via checkout de bootstrap, sem git,
# GLINTFX-BOOTSTRAP-NOGIT) e' byte-a-byte o mesmo que o repo real tem.
# COMPORTAMENTO IDENTICO ao campo `instalar` que vivia na
# matriz do job `linux` antes desta fatia (ci.yml, entradas "Fedora
# (primario) - compartilhado/estatico") - mesmos pacotes, mesma ordem,
# so' o LOCAL mudou.
#
# python3 explicito (DEPZERO-TRACE, GODS_LAWS.md L-07/L-40): tests/
# CMakeLists.txt exige find_program(...NAMES python3 python REQUIRED)
# - sem o interprete, o CONFIGURE inteiro cai. MEDIDO ao vivo,
# 29/08/2026: Fedora tem pacote "python3" (dnf), expondo /usr/sbin/
# python3.
#
# glintfx-pkgconfig (D-A12 parte 4, revisao do CTO): declara, em forma
# legivel por MAQUINA (tests/tools/check_pkg_dep_coverage.py le estas
# linhas, nunca executa o script), qual pacote instalado acima fornece
# cada modulo pkg-config REQUIRED que cmake/*.cmake exige. A traducao
# modulo->pacote e' dado da PROPRIA distro - mora aqui, no script que
# de fato instala, nunca numa tabela solta em Python.
# glintfx-pkgconfig: wayland-client=wayland-devel
# glintfx-pkgconfig: wayland-egl=wayland-devel
# glintfx-pkgconfig: egl=libglvnd-devel

set -eu

CC_BIN="cc"
CXX_BIN="c++"

# CI-PIN-PKG-B (D-W8-2090, ordem do CTO): aplica o lock Fedora da onda ANTES do dnf: copia o lock para
# /etc/dnf/versionlock.toml e grava o updates-archive oficial com enabled=1. A lista de pacotes abaixo continua a
# mesma; o lock e' que trava a versao de cada um, e o portao de manifesto (check_wave_manifest.py) confere o resultado.
sh "$(dirname "$0")/lock/aplicar_fedora_lock.sh" "$(dirname "$0")/lock"

dnf -y install gcc-c++ cmake ninja-build pkgconf-pkg-config git \
    wayland-devel wayland-protocols-devel libglvnd-devel python3

# Portao de manifesto (CI-PIN-PKG-B): divergencia do instalado para o lock reprova o preparo.
python3 "$(dirname "$0")/../../../tests/tools/check_wave_manifest.py" --slug fedora

{
    echo "CC=${CC_BIN}"
    echo "CXX=${CXX_BIN}"
    echo "GLINTFX_PREP_SHA=$(sha256sum "$0" | awk '{print $1}')"
} >> "$GITHUB_ENV"

"$CC_BIN" --version
"$CXX_BIN" --version
cmake --version
