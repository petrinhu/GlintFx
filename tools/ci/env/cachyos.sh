#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/cachyos.sh - CI-SPLIT-PER-OS A3b (docs/plano-ci-split-
# per-os.md secao 4.4): preparo do alvo CachyOS. TRES responsabilidades
# e nada mais (secao 4.4): instalar, exportar CC/CXX ($GITHUB_ENV) e
# imprimir as versoes. A3c (D-A13): exporta tambem GLINTFX_PREP_SHA
# (sha256 do PROPRIO script) - o passo "Checkout e' repositorio git" do
# ci.yml confere esse valor contra o script real apos o checkout
# definitivo. GODS_LAWS.md L-04: CachyOS NAO reaproveita arch.sh -
# imagem, repositorios e toolchain proprios (medido, 21/08/2026),
# script proprio.
#
# python EXPLICITO (B1, revisao do CTO - conserto da decisao anterior
# desta mesma fatia): a medicao de 29/08/2026 (CachyOS ja traz python3
# 3.14.7 de fabrica) estava certa como FATO daquele dia, mas o plano
# (secao 4.7 e a linha da A3b) exige python3 explicito em TODO script
# de preparo, sem excecao - "de fabrica" depende da imagem de hoje, e
# nao e' garantia contra o dia em que ela mudar. O portao de
# uniformidade (check_ci_system_uniformity.py) passa a reprovar
# qualquer script de familia linux sem o pacote declarado.
#
# ROTACAO-ESPELHO-CACHYOS (GODS_LAWS.md L-42, pesquisado antes de
# consertar - segunda falha do mesmo motivo, runs 35937778448 e
# 35939021453: github.com/CachyOS/distribution#443 e o topico do forum
# CachyOS "Problems with pacman and CachyOS keyring" 27/09/2025, ambos
# com o MESMO sintoma). Reproduzido em container local (23-24/09/2026):
# "error: cachyos: signature from CachyOS <admin@cachyos.org> is
# invalid". Medido, nao suposto: o .db do repo cachyos e IDENTICO
# (mesmo md5sum) entre espelhos testados, mas cada espelho reassina em
# horario proprio, e o pacman NAO cai sozinho para o proximo espelho da
# lista quando a assinatura do banco de dados falha (so faz fallback em
# erro de download). SigLevel = Never/TrustAll fica PROIBIDO (calaria o
# portao de seguranca da L-02/L-40) - a defesa e rotacionar
# cachyos-mirrorlist e tentar de novo, o que basta porque a
# dessincronia e por espelho e transitoria. Movido de ci.yml (passo
# "Instalar toolchain") para dentro deste script nesta fatia (A3b) -
# mesma semantica, mesmo maximo de 10 tentativas: como este script SO
# roda para o alvo CachyOS, nao precisa mais do `if matrix.imagem ==
# cachyos/*` que existia quando o passo era compartilhado entre os
# quatro alvos Linux.
#
# glintfx-pkgconfig (D-A12 parte 4, revisao do CTO): ver o comentario
# em fedora.sh - mesma forma legivel por maquina, traducao propria
# desta distro.
# glintfx-pkgconfig: wayland-client=wayland
# glintfx-pkgconfig: wayland-egl=wayland
# glintfx-pkgconfig: egl=libglvnd

set -u

CC_BIN="cc"
CXX_BIN="c++"

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

maximo=10
tentativa=0
codigo=1
while [ "$tentativa" -lt "$maximo" ]; do
    tentativa=$((tentativa + 1))
    echo "=== instalar toolchain (CachyOS), tentativa $tentativa/$maximo ==="
    pacman -Syu --noconfirm gcc cmake ninja pkgconf git \
        wayland wayland-protocols libglvnd python >"$log" 2>&1
    codigo=$?
    cat "$log"
    if [ "$codigo" -eq 0 ]; then
        break
    fi
    if [ "$tentativa" -lt "$maximo" ] && grep -q "signature from" "$log"; then
        echo "::warning::assinatura do banco de dados CachyOS invalida (espelho dessincronizado), rotacionando cachyos-mirrorlist e tentando de novo"
        lista=/etc/pacman.d/cachyos-mirrorlist
        primeira=$(grep -n "^Server" "$lista" | head -1 | cut -d: -f1)
        if [ -n "$primeira" ]; then
            sed -n "${primeira}p" "$lista" >>"$lista"
            sed -i "${primeira}d" "$lista"
        fi
        continue
    fi
    echo "falha ao instalar toolchain CachyOS (codigo $codigo), sem sinal de assinatura invalida ou tentativas esgotadas"
    exit "$codigo"
done

if [ "$codigo" -ne 0 ]; then
    exit "$codigo"
fi

{
    echo "CC=${CC_BIN}"
    echo "CXX=${CXX_BIN}"
    echo "GLINTFX_PREP_SHA=$(sha256sum "$0" | awk '{print $1}')"
} >> "$GITHUB_ENV"

"$CC_BIN" --version
"$CXX_BIN" --version
cmake --version
