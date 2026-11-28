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

# CI-PIN-PKG-A (D-W8-2091, ordem do CTO): a onda inteira congela num
# instante so' (tools/ci/wave-stamp.env). A data do Arch sai do mesmo
# ONDA_STAMP por fatiamento - nao ha segunda variavel.
. "$(dirname "$0")/../wave-stamp.env"
arch_date="${ONDA_STAMP:0:4}/${ONDA_STAMP:4:2}/${ONDA_STAMP:6:2}"

# Uma linha so', no mirrorlist: o ALA por data. $repo e $arch ficam entre
# aspas simples para o pacman expandi-los, nao o shell.
printf '%s\n' 'Server = https://archive.archlinux.org/repos/'"$arch_date"'/$repo/os/$arch' \
    > /etc/pacman.d/mirrorlist

# -Syy força o banco a vir do ALA: o cache da imagem pode ser mais novo que
# o arquivo datado, e o pacman nao baixaria de novo. A linha seguinte
# (-Syu com a lista) e' a instalacao de sempre, so' que sobre o banco certo.
pacman -Syy
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

# Portao de manifesto (CI-PIN-PKG-A): o instalado tem de bater com
# tools/ci/wave-manifest/arch.txt. Divergencia reprova o preparo e imprime
# as duas listas (L-40: manifesto vazio reprova).
python3 "$(dirname "$0")/../../../tests/tools/check_wave_manifest.py" --slug arch
