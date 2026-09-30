#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# DUBLE do envoltorio de VM (D-A27), so' para o selftest do blob_selftests.py: poe a armadilha
# (bin/ ao lado, copiada para a raiz falsa) na frente do PATH, roda o comando e reprova se o log dela
# nao estiver vazio, SEM bwrap. Existe para o controle "selftest que sai 0 mas toca virsh reprova"
# provar a logica do --blob IGUAL em todo sistema (L-04), inclusive onde nao ha bwrap (o container do
# CI). O bwrap de verdade e' provado so' pelo armadilha_selftest.sh. Nunca e' usado fora da raiz falsa.
set -u
aqui="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
log="$(mktemp "${TMPDIR:-/var/tmp}/glintfx-armadilha-duble-log.XXXXXX")" || exit 1
trap 'rm -f -- "$log"' EXIT
PATH="$aqui/bin:$PATH" GLINTFX_ARMADILHA_LOG="$log" "$@"
rc=$?
if [ -s "$log" ]; then
  echo "run_com_armadilha.sh: ARMADILHA DISPAROU - o comando tocou ferramenta de VM/rede (rc do comando: $rc):" >&2
  sed 's/^/  /' "$log" >&2
  exit 1
fi
exit "$rc"
