#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# registrar.sh - corpo unico da armadilha (D-A27): registra "<ferramenta> <argumentos>" em
# $GLINTFX_ARMADILHA_LOG e sai 97. Chamado pelos wrappers em bin/.
# Usage: registrar.sh <ferramenta> [argumentos...]
nome="$1"
shift
linha="$nome $*"
if [ -n "${GLINTFX_ARMADILHA_LOG:-}" ]; then
  printf '%s\n' "$linha" >>"$GLINTFX_ARMADILHA_LOG"
else
  printf 'ARMADILHA sem GLINTFX_ARMADILHA_LOG: %s\n' "$linha" >&2
fi
exit 97
