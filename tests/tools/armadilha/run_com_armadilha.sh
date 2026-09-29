#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_com_armadilha.sh - roda um comando com a armadilha de ferramentas de VM NA FRENTE do
# PATH (D-A27, CI-SPLIT-PER-OS, L-09/L-50) e REPROVA se o log dela nao estiver vazio, mesmo que
# o comando tenha saido 0. O defeito que isto fecha: o selftest de um script que usa um stub
# de virsh por PATH="$stub:$PATH" passava com o stub furado, porque o virsh REAL fica logo
# atras do stub; agora atras do stub vem a armadilha (registra e sai 97).
# O rc do comando atravessa quando o log esta vazio (inclusive 77, o pulo do ctest).
#
# Usage: run_com_armadilha.sh <comando> [argumentos...]
set -u
dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [ "$#" -eq 0 ]; then
  echo "Uso: $0 <comando> [argumentos...]" >&2
  exit 2
fi
log="$(mktemp "${TMPDIR:-/var/tmp}/glintfx-armadilha-log.XXXXXX")" || exit 1
trap 'rm -f -- "$log"' EXIT
PATH="$dir/bin:$PATH" GLINTFX_ARMADILHA_LOG="$log" "$@"
rc=$?
if [ -s "$log" ]; then
  echo "run_com_armadilha.sh: ARMADILHA DISPAROU - o comando tocou ferramenta de VM/rede (rc do comando: $rc):" >&2
  sed 's/^/  /' "$log" >&2
  exit 1
fi
exit "$rc"
