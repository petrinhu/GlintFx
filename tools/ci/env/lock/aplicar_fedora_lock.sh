#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/env/lock/aplicar_fedora_lock.sh - CI-PIN-PKG-B (ordem do CTO, D-W8-2090, item 3): aplica o lock Fedora da
# onda num ponto de instalacao, ANTES do `dnf -y install` do ponto. Chamado por tools/ci/env/fedora.sh, pelos cinco
# jobs Fedora do ci.yml (lint, sanitizer, gl-codegen-host-cross, debug, clang) e pelos dois estagios do
# tests/container/Containerfile. Faz tres coisas e nada mais:
#   1. copia o lock para /etc/dnf/versionlock.toml (o dnf5 le esse arquivo);
#   2. grava o repositorio updates-archive oficial com enabled=1 (versoes ja superadas, para o lock encontrar);
#   3. imprime o sha256 do lock e a contagem de entradas, e reprova se a contagem nao bater com a do dnf5.
#
# Uso: aplicar_fedora_lock.sh <pasta com fedora.versionlock.toml e fedora-updates-archive.repo>
#
# Piso (GODS_LAWS.md L-40): lock vazio, repositorio sem a linha enabled=1, ou contagem diferente reprovam.

set -eu

pasta="${1:?aplicar_fedora_lock: falta a pasta do lock}"
lock="$pasta/fedora.versionlock.toml"
archive="$pasta/fedora-updates-archive.repo"

fail() {
    echo "aplicar_fedora_lock: $1" >&2
    exit 1
}

[ -s "$lock" ] || fail "lock ausente ou vazio: $lock (L-40)"
[ -s "$archive" ] || fail "repositorio updates-archive ausente ou vazio: $archive (L-40)"

entradas="$(grep -c '^\[\[packages\]\]' "$lock" || true)"
[ "${entradas:-0}" -gt 0 ] || fail "lock sem nenhuma entrada (L-40)"

install -m 0644 "$lock" /etc/dnf/versionlock.toml
sed 's/^enabled=.*/enabled=1/' "$archive" > /etc/yum.repos.d/fedora-updates-archive.repo
grep -q '^enabled=1$' /etc/yum.repos.d/fedora-updates-archive.repo || fail "updates-archive sem enabled=1 (L-40)"

dnf_entradas="$(dnf5 --disablerepo='*' versionlock list 2>/dev/null | grep -c '^Package name:' || true)"
echo "aplicar_fedora_lock: lock sha256=$(sha256sum "$lock" | cut -c1-64) entradas=$entradas dnf5=$dnf_entradas"
[ "$dnf_entradas" -eq "$entradas" ] || fail "dnf5 le $dnf_entradas entrada(s) de $entradas no lock (L-40)"
