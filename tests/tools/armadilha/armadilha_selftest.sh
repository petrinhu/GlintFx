#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# armadilha_selftest.sh - prova que o envoltorio de VM (D-A27, CI-SPLIT-PER-OS, L-09/L-50) MORDE:
#  - virsh, virt-install, qemu-system-* , ssh e scp (bin/) registram e saem 97;
#  - run_com_armadilha.sh roda o comando DENTRO de um bwrap (--unshare-all, /var/tmp, /run e
#    /home privados, repositorio so' leitura), PROVA o isolamento antes do comando, reprova se o
#    log da armadilha nao estiver vazio mesmo com rc 0, e sai 77 sem bwrap;
#  - o qemu-img NAO e' armadilha (nenhuma checagem de argv fecha `qemu-img commit` contra um
#    disco de verdade: C-1 do CTO); e' o ISOLAMENTO que o protege - o disco da VM nao existe la dentro.
# Toda ferramenta chamada aqui e' inofensiva mesmo se o envoltorio estivesse quebrado
# (test:///default, dominio falso, discos de 1 MiB criados por este proprio teste).
#
# Usage: armadilha_selftest.sh --selftest

set -u
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WRAP="$DIR/run_com_armadilha.sh"

if [ "${1:-}" != "--selftest" ]; then
  echo "Uso: $0 --selftest" >&2
  exit 2
fi
if ! command -v bwrap >/dev/null 2>&1 || ! bwrap --unshare-all --ro-bind / / true >/dev/null 2>&1; then
  echo "armadilha_selftest: SEM bwrap utilizavel neste host - pulado (77)"
  exit 77
fi

ok=1
falha() { echo "armadilha_selftest: FALHOU - $1" >&2; ok=0; }
work="$(mktemp -d "${TMPDIR:-/var/tmp}/glintfx-armadilha-selftest.XXXXXX")" || exit 1
trap 'rm -rf -- "$work"' EXIT

# 1. cada ferramenta da armadilha registra e sai 97; qemu-img nao e' armadilha
for ferramenta in virsh virt-install qemu-system-x86_64 qemu-system-aarch64 ssh scp; do
  : >"$work/log"
  GLINTFX_ARMADILHA_LOG="$work/log" "$DIR/bin/$ferramenta" -c test:///default duble >/dev/null 2>&1
  rc=$?
  [ "$rc" -eq 97 ] || falha "$ferramenta saiu $rc (esperado 97)"
  grep -q "^$ferramenta " "$work/log" || falha "$ferramenta nao registrou no log"
done
[ ! -e "$DIR/bin/qemu-img" ] || falha "bin/qemu-img existe: a armadilha de qemu-img foi rejeitada (C-1), quem protege e' o isolamento"

# 2. o envoltorio reprova comando que sai 0 mas tocou a armadilha, nomeando a ferramenta
saida="$("$WRAP" bash -c 'virsh -c test:///default domstate duble-dom >/dev/null 2>&1; exit 0' 2>&1)"; rc=$?
[ "$rc" -eq 1 ] || falha "comando que toca virsh e sai 0 deveria REPROVAR (rc=1), obteve $rc: $saida"
printf '%s' "$saida" | grep -q "ARMADILHA" || falha "a reprovacao nao diz ARMADILHA: $saida"
printf '%s' "$saida" | grep -q "virsh" || falha "a reprovacao nao nomeia virsh: $saida"

# 3. o rc do comando atravessa quando o log esta vazio (inclusive 77, o pulo do ctest)
"$WRAP" bash -c 'exit 0' >/dev/null 2>&1 || falha "comando limpo deveria passar"
"$WRAP" bash -c 'exit 77' >/dev/null 2>&1; rc=$?
[ "$rc" -eq 77 ] || falha "rc 77 do comando (pulo) deveria atravessar, obteve $rc"
"$WRAP" bash -c 'exit 3' >/dev/null 2>&1; rc=$?
[ "$rc" -eq 3 ] || falha "rc 3 do comando deveria atravessar, obteve $rc"

# 4. ISOLAMENTO: um caminho do hospedeiro (sentinela) e' AUSENTE dentro, e o /var/tmp de dentro e' privado
mkdir -p "$work/sentinela.d"
printf 'x' >"$work/sentinela.d/arquivo"
saida="$("$WRAP" bash -c "ls '$work/sentinela.d' 2>&1; echo rc-ls=\$?" 2>&1)"
printf '%s' "$saida" | grep -q "rc-ls=0" && falha "o caminho do hospedeiro $work/sentinela.d esta VISIVEL dentro do envoltorio: $saida"
printf '%s' "$saida" | grep -qiE "rc-ls=[1-9]" || falha "nao consegui ler o resultado do ls de ausencia: $saida"

# 4b. o /var/tmp de dentro e' privado MAS no mesmo sistema de arquivos do hospedeiro: o consolidar.sh
#     faz `cp --reflink=always` (btrfs/xfs); um /var/tmp tmpfs o quebraria (medido: preci 29/09,
#     "Operacao sem suporte"). So' vale onde o hospedeiro suporta reflink.
: >"$work/r-a"
if cp --reflink=always "$work/r-a" "$work/r-b" 2>/dev/null; then
  saida="$("$WRAP" bash -c 'cd /var/tmp && : > a && cp --reflink=always a b && echo reflink-ok' 2>&1)"
  printf '%s' "$saida" | grep -q "reflink-ok" || falha "dentro do envoltorio o /var/tmp nao suporta cp --reflink=always (o hospedeiro suporta): $saida"
fi

# 4c. raiz do repositorio SOB /var/tmp (o --blob e os fake roots vivem la): o bind da raiz cria o ponto de
#     montagem dentro do /var/tmp privado; a auto-prova aceita SO' esse componente e o comando roda
#     (achado do preci 29/09: "/var/tmp nao esta VAZIO ... conteudo: glintfx-blob-XXXX").
mkdir -p "$work/raizfake/tests/tools"
cp -r "$DIR" "$work/raizfake/tests/tools/armadilha"
"$work/raizfake/tests/tools/armadilha/run_com_armadilha.sh" /usr/bin/true >"$work/raizfake.out" 2>&1 \
  || falha "com a raiz sob /var/tmp o envoltorio deveria rodar (o ponto de montagem da raiz e' o unico conteudo permitido): $(cat "$work/raizfake.out")"

# 5. C-1 do CTO: nenhuma lista de ferramentas por nome fecha o disco real (qemu-img commit, qemu-io write,
#    qemu-nbd, guestmount...); o que fecha e' o disco NAO EXISTIR na sandbox. Aqui o "disco da VM" e'
#    um caminho .../glintfx-win-lab/ criado FORA do envoltorio: `qemu-img commit` (a forma do
#    consolidar.sh) e `qemu-io write` (a do consolidar.sh) pelo envoltorio dao "inexistente" e o disco
#    do hospedeiro fica byte a byte igual. Antes do conserto, a liberacao do qemu-img por prefixo de
#    caminho gravava 64 KiB no cabecalho do base com o log vazio.
if command -v qemu-img >/dev/null 2>&1 && command -v qemu-io >/dev/null 2>&1; then
  mkdir -p "$work/glintfx-win-lab"
  base="$work/glintfx-win-lab/base.qcow2"
  ov="$work/glintfx-win-lab/ov.qcow2"
  qemu-img create -f qcow2 -- "$base" 1M >/dev/null 2>&1
  qemu-img create -f qcow2 -b "$base" -F qcow2 -- "$ov" >/dev/null 2>&1
  antes="$(md5sum <"$base")"
  saida="$("$WRAP" bash -c "qemu-img commit -- '$ov'" 2>&1)"; rc=$?
  [ "$rc" -ne 0 ] || falha "qemu-img commit sobre disco do hospedeiro deveria FALHAR dentro da sandbox, obteve rc=0"
  printf '%s' "$saida" | grep -qiE "inexistente|No such file|Could not open" || falha "qemu-img commit pelo envoltorio nao deu 'arquivo inexistente': $saida"
  saida="$("$WRAP" bash -c "qemu-io -c 'write 0 4k' '$base'" 2>&1)"; rc=$?
  [ "$rc" -ne 0 ] || printf '%s' "$saida" | grep -qiE "inexistente|No such file|Could not open|failed" || falha "qemu-io write pelo envoltorio nao deu 'arquivo inexistente': $saida"
  depois="$(md5sum <"$base")"
  [ "$antes" = "$depois" ] || falha "qemu-img commit / qemu-io write pelo envoltorio ALTEROU o disco base do hospedeiro (C-1)"
fi

# 6. a armadilha esta na frente do PATH de dentro
"$WRAP" bash -c 'case "$(command -v virsh)" in */armadilha/bin/virsh) exit 0 ;; *) echo "virsh resolvido em: $(command -v virsh)" >&2; exit 1 ;; esac' >/dev/null 2>&1 \
  || falha "virsh dentro do envoltorio nao resolve na armadilha"

# 6b. gate estatico: o envoltorio tem de EXECUTAR bwrap (um mutante que o tirasse rodaria o comando no hospedeiro)
grep -q '^bwrap "${args\[@\]}"' "$WRAP" || falha 'run_com_armadilha.sh nao executa bwrap - o comando rodaria sem isolamento'
grep -q -- '--bind "$vt" /var/tmp' "$WRAP" || falha 'run_com_armadilha.sh nao liga um /var/tmp privado (--bind "$vt" /var/tmp)'

# 7. sem bwrap: o envoltorio sai 77 e diz o motivo
saida="$(env PATH=/nonexistente /usr/bin/bash "$WRAP" /usr/bin/true 2>&1)"; rc=$?
[ "$rc" -eq 77 ] || falha "sem bwrap o envoltorio deveria sair 77, obteve $rc: $saida"
printf '%s' "$saida" | grep -q "AUSENTE: bwrap (isolamento obrigatorio do laboratorio da VM, L-09/L-50)" || falha "sem bwrap a mensagem nao e a esperada: $saida"

if [ "$ok" -eq 1 ]; then
  echo "armadilha_selftest: OK - ferramentas registram e saem 97, envoltorio isola (bwrap), reprova log nao vazio, rc atravessa, disco do hospedeiro invisivel e imune a qemu-img commit, sem bwrap sai 77"
  exit 0
fi
exit 1
