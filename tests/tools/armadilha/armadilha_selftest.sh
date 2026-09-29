#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# armadilha_selftest.sh - prova que a armadilha de ferramentas de VM (D-A27, CI-SPLIT-PER-OS,
# L-09/L-50) MORDE: virsh, virt-install, qemu-system-*, ssh e scp registram e saem 97; qemu-img
# so' passa para caminho sob um diretorio de selftest; run_com_armadilha.sh reprova quando o
# log nao esta vazio, mesmo que o comando envolvido tenha saido 0 (o defeito real: selftests
# do consolidar.sh e do sessao.sh passavam COM o stub furado).
# Toda ferramenta chamada aqui e' inofensiva mesmo se a armadilha estivesse quebrada (URI
# test:///default e dominio falso).
#
# Usage: armadilha_selftest.sh --selftest

set -u
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WRAP="$DIR/run_com_armadilha.sh"

if [ "${1:-}" != "--selftest" ]; then
  echo "Uso: $0 --selftest" >&2
  exit 2
fi

ok=1
falha() { echo "armadilha_selftest: FALHOU - $1" >&2; ok=0; }
work="$(mktemp -d "${TMPDIR:-/var/tmp}/glintfx-armadilha-selftest.XXXXXX")" || exit 1
trap 'rm -rf -- "$work"' EXIT

# 1. cada ferramenta da armadilha registra e sai 97
for ferramenta in virsh virt-install qemu-system-x86_64 qemu-system-aarch64 ssh scp; do
  : >"$work/log"
  GLINTFX_ARMADILHA_LOG="$work/log" "$DIR/bin/$ferramenta" -c test:///default duble >/dev/null 2>&1
  rc=$?
  [ "$rc" -eq 97 ] || falha "$ferramenta saiu $rc (esperado 97)"
  grep -q "^$ferramenta " "$work/log" || falha "$ferramenta nao registrou no log"
done

# 2. o harness reprova comando que sai 0 mas tocou a armadilha, e nomeia a ferramenta
cat >"$work/toca.sh" <<'SCRIPT'
#!/usr/bin/env bash
virsh -c test:///default domstate duble-dom >/dev/null 2>&1
exit 0
SCRIPT
saida="$("$WRAP" bash "$work/toca.sh" 2>&1)"; rc=$?
[ "$rc" -eq 1 ] || falha "comando que toca virsh e sai 0 deveria REPROVAR (rc=1), obteve $rc"
printf '%s' "$saida" | grep -q "ARMADILHA" || falha "a reprovacao nao diz ARMADILHA: $saida"
printf '%s' "$saida" | grep -q "virsh" || falha "a reprovacao nao nomeia virsh: $saida"

# 3. comando limpo passa e devolve o rc do comando (inclusive 77)
"$WRAP" bash -c 'exit 0' >/dev/null 2>&1 || falha "comando limpo deveria passar"
"$WRAP" bash -c 'exit 77' >/dev/null 2>&1; rc=$?
[ "$rc" -eq 77 ] || falha "rc 77 do comando (pulo) deveria atravessar, obteve $rc"
"$WRAP" bash -c 'exit 3' >/dev/null 2>&1; rc=$?
[ "$rc" -eq 3 ] || falha "rc 3 do comando deveria atravessar, obteve $rc"

# 4. qemu-img: passa (real) sob diretorio de selftest, e' armadilha fora dele
cat >"$work/img_fora.sh" <<'SCRIPT'
#!/usr/bin/env bash
qemu-img info -- /var/tmp/naoexiste-armadilha/x.qcow2 >/dev/null 2>&1
exit 0
SCRIPT
saida="$("$WRAP" bash "$work/img_fora.sh" 2>&1)"; rc=$?
[ "$rc" -eq 1 ] || falha "qemu-img fora de dir de selftest deveria REPROVAR, obteve $rc"
printf '%s' "$saida" | grep -q "qemu-img" || falha "a reprovacao nao nomeia qemu-img: $saida"
cat >"$work/img_dentro.sh" <<'SCRIPT'
#!/usr/bin/env bash
d="$(mktemp -d /var/tmp/glintfx-y-selftest.XXXXXX)"
qemu-img create -f qcow2 -- "$d/a.qcow2" 1M >/dev/null 2>&1; rc=$?
rm -rf -- "$d"
exit $rc
SCRIPT
if command -v qemu-img >/dev/null 2>&1; then
  "$WRAP" bash "$work/img_dentro.sh" >/dev/null 2>&1 || falha "qemu-img sob dir de selftest deveria passar (usa o real)"
fi

# 5. a armadilha vai NA FRENTE do PATH mesmo com um virsh 'real' de mentira depois dela
mkdir -p "$work/real"
printf '#!/bin/sh\nexit 0\n' >"$work/real/virsh"; chmod +x "$work/real/virsh"
PATH="$work/real:$PATH" "$WRAP" bash -c 'virsh x' >/dev/null 2>&1; rc=$?
[ "$rc" -eq 1 ] || falha "virsh 'real' atras da armadilha deveria ser barrado (rc 1), obteve $rc"

if [ "$ok" -eq 1 ]; then
  echo "armadilha_selftest: OK - ferramentas registram e saem 97, harness reprova log nao vazio, rc do comando atravessa, qemu-img so' sob dir de selftest, armadilha na frente do PATH"
  exit 0
fi
exit 1
