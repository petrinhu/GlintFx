#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/ubuntu.sh - CI-SPLIT-PER-OS A3b (docs/plano-ci-split-
# per-os.md secao 4.4): preparo do alvo Ubuntu. TRES responsabilidades
# e nada mais (secao 4.4): instalar, exportar CC/CXX ($GITHUB_ENV) e
# imprimir as versoes. COMPORTAMENTO IDENTICO ao campo `instalar` que
# vivia na matriz do job `linux` antes desta fatia (ci.yml, entradas
# "Ubuntu - compartilhado/estatico") - mesmos pacotes, mesma ordem, so'
# o LOCAL mudou.
#
# gcc-14/g++-14 explicitos (PKG-NATIVE, 27/08/2026): o g++ padrao do
# 24.04 e' o 13.3 (C++23 parcial); o piso de toolchain do projeto e'
# GCC 14. python3 explicito pelo mesmo motivo de fedora.sh (DEPZERO-
# TRACE) - MEDIDO: Ubuntu tem pacote "python3" (apt), expondo /usr/
# bin/python3.
#
# glintfx-pkgconfig (D-A12 parte 4, revisao do CTO): ver o comentario
# em fedora.sh - mesma forma legivel por maquina, traducao propria
# desta distro.
# glintfx-pkgconfig: wayland-client=libwayland-dev
# glintfx-pkgconfig: wayland-egl=libwayland-dev
# glintfx-pkgconfig: egl=libegl-dev

set -eu

CC_BIN="gcc-14"
CXX_BIN="g++-14"

apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y \
    g++-14 gcc-14 cmake ninja-build pkg-config git \
    libwayland-dev libwayland-bin wayland-protocols libegl-dev python3

{
    echo "CC=${CC_BIN}"
    echo "CXX=${CXX_BIN}"
} >> "$GITHUB_ENV"

"$CC_BIN" --version
"$CXX_BIN" --version
cmake --version
