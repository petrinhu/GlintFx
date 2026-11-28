#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/ubuntu.sh - CI-SPLIT-PER-OS A3b (docs/plano-ci-split-
# per-os.md secao 4.4): preparo do alvo Ubuntu. TRES responsabilidades
# e nada mais (secao 4.4): instalar, exportar CC/CXX ($GITHUB_ENV) e
# imprimir as versoes. A3c (D-A13): exporta tambem GLINTFX_PREP_SHA
# (sha256 do PROPRIO script) - o passo "Checkout e' repositorio git" do
# ci.yml confere esse valor contra o script real apos o checkout
# definitivo, provando que o script que rodou (via checkout de
# bootstrap, sem git) e' byte-a-byte o mesmo que o repo real tem.
# COMPORTAMENTO IDENTICO ao campo `instalar` que vivia na matriz do job
# `linux` antes desta fatia (ci.yml, entradas "Ubuntu - compartilhado/
# estatico") - mesmos pacotes, mesma ordem, so' o LOCAL mudou.
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

# CI-PIN-PKG-A (D-W8-2091, ordem do CTO): a onda inteira congela num
# instante so' (tools/ci/wave-stamp.env). Lido por "$(dirname "$0")", que
# funciona tambem do checkout de bootstrap, onde a arvore vem inteira.
. "$(dirname "$0")/../wave-stamp.env"

# ca-certificates vem ANTES do snapshot: sem ele o apt nao fala https com
# snapshot.ubuntu.com (o http redireciona para https). Ele e' lido do
# arquivo vivo do dia - lacuna declarada na D-W8-2093.
# `--yes` (e nao `-y`) de proposito: o portao de cobertura de pacote
# (tests/tools/check_pkg_dep_coverage.py) le a PRIMEIRA linha `apt-get
# install -y` como lista da distro; ca-certificates e' pre-requisito de
# transporte, nao pacote-alvo. Declarado no relatorio do CI-PIN-PKG-A.
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install --yes ca-certificates

printf 'APT::Snapshot "%s";\n' "$ONDA_STAMP" > /etc/apt/apt.conf.d/50snapshot

# O snapshot so' vale se o apt o usou de fato: a saida do update precisa
# citar snapshot.ubuntu.com, senao o congelamento falhou em silencio.
snapshot_log="$(apt-get update 2>&1)"
printf '%s\n' "$snapshot_log"
case "$snapshot_log" in
    *snapshot.ubuntu.com*) ;;
    *)
        echo "FALHOU: apt-get update nao citou snapshot.ubuntu.com - APT::Snapshot nao valeu" >&2
        exit 1
        ;;
esac

DEBIAN_FRONTEND=noninteractive apt-get install -y \
    g++-14 gcc-14 cmake ninja-build pkg-config git \
    libwayland-dev libwayland-bin wayland-protocols libegl-dev python3

{
    echo "CC=${CC_BIN}"
    echo "CXX=${CXX_BIN}"
    echo "GLINTFX_PREP_SHA=$(sha256sum "$0" | awk '{print $1}')"
} >> "$GITHUB_ENV"

"$CC_BIN" --version
"$CXX_BIN" --version
cmake --version

# Portao de manifesto (CI-PIN-PKG-A): o instalado tem de bater com
# tools/ci/wave-manifest/ubuntu.txt. Divergencia reprova o preparo e
# imprime as duas listas (L-40: manifesto vazio reprova).
python3 "$(dirname "$0")/../../../tests/tools/check_wave_manifest.py" --slug ubuntu
