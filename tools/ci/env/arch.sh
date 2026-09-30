#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/arch.sh - CI-SPLIT-PER-OS A3b (docs/plano-ci-split-per-
# os.md secao 4.4): preparo do alvo Arch. TRES responsabilidades e
# nada mais (secao 4.4): instalar, exportar CC/CXX ($GITHUB_ENV) e
# imprimir as versoes. A3c (D-A13): exporta tambem GLINTFX_PREP_SHA
# (sha256 do PROPRIO script) - o passo "Checkout e' repositorio git" do
# ci.yml confere esse valor contra o script real apos o checkout
# definitivo, provando que o script que rodou (via checkout de
# bootstrap, sem git) e' byte-a-byte o mesmo que o repo real tem.
# COMPORTAMENTO IDENTICO ao campo `instalar` que vivia na matriz do job
# `linux` antes desta fatia (ci.yml, entradas "Arch - compartilhado/
# estatico") - mesmos pacotes, mesma ordem, so' o LOCAL mudou.
# GODS_LAWS.md L-04: CachyOS NAO reaproveita este script - imagem,
# repositorios e toolchain proprios, script proprio (cachyos.sh).
#
# python3 explicito (DEPZERO-TRACE): o nome do pacote no Arch e'
# "python" (pacman), nao "python3" - MEDIDO, 29/08/2026, o pacote
# "python" entrega o binario /usr/sbin/python3 tambem.
#
# glintfx-pkgconfig (D-A12 parte 4, revisao do CTO): ver o comentario
# em fedora.sh - mesma forma legivel por maquina, traducao propria
# desta distro.
# glintfx-pkgconfig: wayland-client=wayland
# glintfx-pkgconfig: wayland-egl=wayland
# glintfx-pkgconfig: egl=libglvnd

set -eu

CC_BIN="cc"
CXX_BIN="c++"

pacman -Syu --noconfirm gcc cmake ninja pkgconf git \
    wayland wayland-protocols libglvnd python

{
    echo "CC=${CC_BIN}"
    echo "CXX=${CXX_BIN}"
    echo "GLINTFX_PREP_SHA=$(sha256sum "$0" | awk '{print $1}')"
} >> "$GITHUB_ENV"

"$CC_BIN" --version
"$CXX_BIN" --version
cmake --version
