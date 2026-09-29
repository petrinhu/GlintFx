#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# dentro_da_sandbox.sh - metade de DENTRO de run_com_armadilha.sh (D-A27): roda sob bwrap.
# 1) PROVA o isolamento (senao sai 1): /tmp, /run e /home sao tmpfs privados e /var/tmp entra VAZIO (salvo o ponto de montagem da raiz); nao
#    existe /var/tmp/glintfx-win-lab nem /run/user; so' a interface lo; a raiz do repositorio e'
#    so' leitura. 2) poe bin/ (a armadilha) na frente do PATH e roda o comando. 3) log da
#    armadilha nao vazio reprova (rc 1), mesmo com o comando em rc 0.
#
# Usage: dentro_da_sandbox.sh <raiz-do-repositorio> <comando> [argumentos...]
set -u
raiz="$1"
shift
aqui="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

erros=()
for d in /tmp /run /home; do
  tipo="$(stat -f -c %T "$d" 2>/dev/null)"
  [ "$tipo" = "tmpfs" ] || erros+=("$d nao e' tmpfs privado (tipo: ${tipo:-?}) - e' o do hospedeiro?")
done
# /var/tmp entra vazio, EXCETO o componente de topo do caminho da raiz do repositorio quando ela mora
# sob /var/tmp (o --blob e os fake roots): o bind da raiz cria esse ponto de montagem. So' ele.
topo=""
case "$raiz" in /var/tmp/*) topo="/var/tmp/$(printf '%s' "${raiz#/var/tmp/}" | cut -d/ -f1)" ;; esac
extras="$(find /var/tmp -mindepth 1 -maxdepth 1 ! -path "$topo" 2>/dev/null)"
[ -z "$extras" ] || erros+=("/var/tmp nao esta VAZIO ao entrar - e' o do hospedeiro? conteudo: $(printf '%s' "$extras" | head -3 | tr '\n' ' ')")
[ ! -e /var/tmp/glintfx-win-lab ] || erros+=("/var/tmp/glintfx-win-lab VISIVEL")
[ ! -e /run/user ] || erros+=("/run/user VISIVEL")
[ -z "${XDG_RUNTIME_DIR:-}" ] || erros+=("XDG_RUNTIME_DIR definido: ${XDG_RUNTIME_DIR}")
if awk -F: 'NR>2 { gsub(/ /, "", $1); if ($1 != "lo") ruim = 1 } END { exit ruim }' /proc/net/dev; then :; else
  erros+=("ha interface de rede alem de lo")
fi
if ( : >"$raiz/.probe-escrita" ) 2>/dev/null; then
  rm -f -- "$raiz/.probe-escrita"
  erros+=("a raiz do repositorio ($raiz) e' GRAVAVEL")
fi
if [ "${#erros[@]}" -gt 0 ]; then
  echo "dentro_da_sandbox.sh: ISOLAMENTO NAO PROVADO - recuso rodar o comando:" >&2
  printf '  %s\n' "${erros[@]}" >&2
  exit 1
fi

log="$(mktemp /var/tmp/glintfx-armadilha-log.XXXXXX)" || exit 1
PATH="$aqui/bin:$PATH" GLINTFX_ARMADILHA_LOG="$log" "$@"
rc=$?
if [ -s "$log" ]; then
  echo "run_com_armadilha.sh: ARMADILHA DISPAROU - o comando tocou ferramenta de VM/rede (rc do comando: $rc):" >&2
  sed 's/^/  /' "$log" >&2
  exit 1
fi
exit "$rc"
