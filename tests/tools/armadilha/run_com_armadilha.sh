#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_com_armadilha.sh - roda um comando DENTRO de um bwrap (D-A27, C-1 do CTO, CI-SPLIT-PER-OS,
# L-09/L-50): --unshare-all (sem rede, sem sessao, sem IPC), --die-with-parent, /tmp, /var/tmp, /run
# e /home privados (tmpfs), /var/tmp privado e VAZIO (diretorio descartavel no sistema de arquivos do hospedeiro: o consolidar.sh usa `cp --reflink=always`, que um tmpfs nao tem), /usr e /etc so' leitura, e a raiz do repositorio re-ligada so' leitura
# no MESMO caminho. Dentro, dentro_da_sandbox.sh PROVA o isolamento antes do comando, poe a
# armadilha de virsh/ssh/scp/qemu-system/virt-install na frente do PATH e reprova se o log dela nao
# estiver vazio. O disco e o socket da VM real nao EXISTEM la dentro: por isso o qemu-img nao e'
# armadilha (nenhuma checagem de argv o fecha).
# Sem bwrap utilizavel: sai 77 (pulo do ctest) e diz o motivo. O rc do comando atravessa.
#
# Usage: run_com_armadilha.sh <comando> [argumentos...]
if [ "$#" -eq 0 ]; then
  echo "Uso: $0 <comando> [argumentos...]" >&2
  exit 2
fi
# so' builtins ate aqui: sem PATH nao ha dirname/mktemp
if ! command -v bwrap >/dev/null 2>&1; then
  echo "AUSENTE: bwrap (isolamento obrigatorio do laboratorio da VM, L-09/L-50) - o envoltorio nao roda o comando sem isolar; pulado (77)" >&2
  exit 77
fi
if ! bwrap --unshare-all --ro-bind / / true >/dev/null 2>&1; then
  echo "AUSENTE: bwrap utilizavel (isolamento obrigatorio do laboratorio da VM, L-09/L-50): presente, mas sem namespace de usuario (container sem privilegio?) - pulado (77)" >&2
  exit 77
fi
self="${BASH_SOURCE[0]}"
aqui="$(cd -- "${self%/*}" && pwd -P)"
raiz="$(cd -- "$aqui/../../.." && pwd -P)"

args=(--unshare-all --die-with-parent --new-session --ro-bind /usr /usr --ro-bind /etc /etc)
for d in bin sbin lib lib32 lib64; do
  if [ -L "/$d" ]; then
    args+=(--symlink "$(readlink "/$d")" "/$d")
  elif [ -d "/$d" ]; then
    args+=(--ro-bind "/$d" "/$d")
  fi
done
vt="$(mktemp -d "${TMPDIR:-/var/tmp}/glintfx-armadilha-vt.XXXXXX")" || exit 1
trap 'rm -rf -- "$vt"' EXIT
args+=(--proc /proc --dev /dev --tmpfs /tmp --bind "$vt" /var/tmp --tmpfs /run --tmpfs /home)
args+=(--ro-bind "$raiz" "$raiz" --chdir "$raiz")
args+=(--setenv TMPDIR /var/tmp --setenv HOME /tmp --unsetenv XDG_RUNTIME_DIR --unsetenv DBUS_SESSION_BUS_ADDRESS)
bwrap "${args[@]}" bash "$aqui/dentro_da_sandbox.sh" "$raiz" "$@"
exit $?
