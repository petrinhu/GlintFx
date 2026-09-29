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
# Dois grupos (L-04: o que roda em todo sistema roda igual; o que depende de bwrap pula DECLARADO):
#  GRUPO 1 (nao chama o envoltorio com PATH normal, roda em todo Linux): bins registram e saem 97,
#    gates estaticos do envoltorio, "sem bwrap sai 77", GLINTFX_ARMADILHA_SIMULAR_SEM_BWRAP so' aqui.
#  GRUPO 2 (exige bwrap utilizavel): isolamento, sentinela, reflink, symlink, lab visivel, disco,
#    toca virsh, PATH, rc que atravessa.
#  Saida: 1 se o grupo 1 falhar ou (com bwrap) o grupo 2 falhar; 77 SO' com o grupo 1 verde e o grupo 2
#  pulado (o ctest mostra "Skipped", nunca verde); 0 so' com os dois verdes.
#  GLINTFX_ARMADILHA_SIMULAR_SEM_BWRAP=1 faz ESTE script tratar o bwrap como inutilizavel (so' para o
#  vermelho/verde da prova; nunca lida pelo run_com_armadilha.sh nem pelo dentro_da_sandbox.sh).
#
# Usage: armadilha_selftest.sh --selftest

set -u
DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WRAP="$DIR/run_com_armadilha.sh"

if [ "${1:-}" != "--selftest" ]; then
  echo "Uso: $0 --selftest" >&2
  exit 2
fi

falhas=0
falha() { echo "armadilha_selftest: FALHOU - $1" >&2; falhas=$((falhas + 1)); }
work="$(mktemp -d "${TMPDIR:-/var/tmp}/glintfx-armadilha-selftest.XXXXXX")" || exit 1
trap 'rm -rf -- "$work"' EXIT

tem_bwrap() {
  [ -z "${GLINTFX_ARMADILHA_SIMULAR_SEM_BWRAP:-}" ] || return 1
  command -v bwrap >/dev/null 2>&1 && bwrap --unshare-all --ro-bind / / true >/dev/null 2>&1
}

# ---- GRUPO 1: sem bwrap ------------------------------------------------------

g1_ferramentas_saem_97() {
  local ferramenta rc
  for ferramenta in virsh virt-install qemu-system-x86_64 qemu-system-aarch64 ssh scp; do
    : >"$work/log"
    GLINTFX_ARMADILHA_LOG="$work/log" "$DIR/bin/$ferramenta" -c test:///default duble >/dev/null 2>&1
    rc=$?
    [ "$rc" -eq 97 ] || falha "$ferramenta saiu $rc (esperado 97)"
    grep -q "^$ferramenta " "$work/log" || falha "$ferramenta nao registrou no log"
  done
}

g1_qemu_img_nao_e_armadilha() {
  [ ! -e "$DIR/bin/qemu-img" ] || falha "bin/qemu-img existe: a armadilha de qemu-img foi rejeitada (C-1), quem protege e' o isolamento"
}

g1_gates_estaticos_do_envoltorio() {
  grep -q '^bwrap "${args\[@\]}"' "$WRAP" || falha 'run_com_armadilha.sh nao executa bwrap - o comando rodaria sem isolamento'
  grep -q -- '--new-session' "$WRAP" || falha 'run_com_armadilha.sh nao usa --new-session (TIOCSTI: injecao de tecla na sessao do lider, L-50)'
  grep -q -- '--bind "$vt" /var/tmp' "$WRAP" || falha 'run_com_armadilha.sh nao liga um /var/tmp privado (--bind "$vt" /var/tmp)'
}

g1_simulacao_so_neste_script() {
  # GLINTFX_ARMADILHA_SIMULAR_SEM_BWRAP so' pode ser lida por este selftest, e so' move para o lado seguro
  if grep -q "GLINTFX_ARMADILHA_SIMULAR_SEM_BWRAP" "$WRAP" "$DIR/dentro_da_sandbox.sh"; then
    falha "GLINTFX_ARMADILHA_SIMULAR_SEM_BWRAP aparece no envoltorio ou na auto-prova: nenhum gancho de teste pode afrouxar o isolamento"
  fi
}

g1_sem_bwrap_sai_77() {
  local saida rc
  saida="$(env PATH=/nonexistente /usr/bin/bash "$WRAP" /usr/bin/true 2>&1)"; rc=$?
  [ "$rc" -eq 77 ] || falha "sem bwrap o envoltorio deveria sair 77, obteve $rc: $saida"
  printf '%s' "$saida" | grep -q "AUSENTE: bwrap (isolamento obrigatorio do laboratorio da VM, L-09/L-50)" || falha "sem bwrap a mensagem nao e a esperada: $saida"
}

g1_auto_prova_recusa_raiz_gravavel() {
  # a auto-prova FORA da sandbox, com uma raiz GRAVAVEL: tem de recusar (rc 1), nomear GRAVAVEL, NAO rodar o
  # comando (o marcador nao pode existir) e NAO varrer o hospedeiro: um `find` ARMADILHA na frente do PATH
  # registra a chamada e sai 1, e o log dele tem de ficar VAZIO (as checagens baratas vem antes e saem 1).
  local saida rc
  mkdir -p "$work/raizw" "$work/trapfind"
  printf '#!/bin/sh\necho "find $*" >> "%s/find.log"\nexit 1\n' "$work" >"$work/trapfind/find"
  chmod +x "$work/trapfind/find"
  : >"$work/find.log"
  saida="$(PATH="$work/trapfind:$PATH" bash "$DIR/dentro_da_sandbox.sh" "$work/raizw" touch "$work/raizw/MARCADOR" 2>&1)"; rc=$?
  [ "$rc" -eq 1 ] || falha "a auto-prova com raiz gravavel deveria sair 1, obteve $rc: $saida"
  printf '%s' "$saida" | grep -q "GRAVAVEL" || falha "a auto-prova nao nomeou a raiz GRAVAVEL: $saida"
  [ ! -e "$work/raizw/MARCADOR" ] || falha "a auto-prova rodou o comando apesar de recusar (MARCADOR criado)"
  [ ! -s "$work/find.log" ] || falha "find chamado pela auto-prova FORA da sandbox (varreu o hospedeiro): $(cat "$work/find.log")"
}

g1_duble_e_envoltorio_com_o_mesmo_contrato() {
  # C-1 (CTO): o duble do blob e o envoltorio real (dentro_da_sandbox.sh) carregam a mesma mensagem e a
  # mesma forma de execucao do comando; divergencia de contrato nao passa calada
  local duble="$DIR/../fixtures/armadilha_sem_bwrap/run_com_armadilha.sh" alvo
  for alvo in "$duble" "$DIR/dentro_da_sandbox.sh"; do
    grep -q 'ARMADILHA DISPAROU - o comando tocou ferramenta de VM/rede' "$alvo" || falha "$alvo nao tem a mensagem 'ARMADILHA DISPAROU - o comando tocou ferramenta de VM/rede'"
    grep -q 'GLINTFX_ARMADILHA_LOG="$log" "$@"' "$alvo" || falha "$alvo nao executa o comando na forma GLINTFX_ARMADILHA_LOG=\"\$log\" \"\$@\""
  done
}

# ---- GRUPO 2: exige bwrap ----------------------------------------------------

g2_toca_virsh_reprova() {
  local saida rc
  saida="$("$WRAP" bash -c 'virsh -c test:///default domstate duble-dom >/dev/null 2>&1; exit 0' 2>&1)"; rc=$?
  [ "$rc" -eq 1 ] || falha "comando que toca virsh e sai 0 deveria REPROVAR (rc=1), obteve $rc: $saida"
  printf '%s' "$saida" | grep -q "ARMADILHA" || falha "a reprovacao nao diz ARMADILHA: $saida"
  printf '%s' "$saida" | grep -q "virsh" || falha "a reprovacao nao nomeia virsh: $saida"
}

g2_rc_atravessa() {
  local rc
  "$WRAP" bash -c 'exit 0' >/dev/null 2>&1 || falha "comando limpo deveria passar"
  "$WRAP" bash -c 'exit 77' >/dev/null 2>&1; rc=$?
  [ "$rc" -eq 77 ] || falha "rc 77 do comando (pulo) deveria atravessar, obteve $rc"
  "$WRAP" bash -c 'exit 3' >/dev/null 2>&1; rc=$?
  [ "$rc" -eq 3 ] || falha "rc 3 do comando deveria atravessar, obteve $rc"
}

g2_sentinela_do_hospedeiro_invisivel() {
  local saida
  mkdir -p "$work/sentinela.d"
  printf 'x' >"$work/sentinela.d/arquivo"
  saida="$("$WRAP" bash -c "ls '$work/sentinela.d' 2>&1; echo rc-ls=\$?" 2>&1)"
  printf '%s' "$saida" | grep -q "rc-ls=0" && falha "o caminho do hospedeiro $work/sentinela.d esta VISIVEL dentro do envoltorio: $saida"
  printf '%s' "$saida" | grep -qiE "rc-ls=[1-9]" || falha "nao consegui ler o resultado do ls de ausencia: $saida"
}

g2_lab_visivel_na_raiz_reprova() {
  # C-a (M1c do CTO): glintfx-win-lab em QUALQUER lugar visivel dentro da sandbox reprova a auto-prova
  local saida rc
  # a ~12 niveis de profundidade: a auto-prova nao pode ter limite de profundidade (I-1 do CTO)
  mkdir -p "$work/raizlab/tests/tools/armadilha" "$work/raizlab/a/b/c/d/e/f/g/h/i/j/k/l/glintfx-win-lab"
  cp -r "$DIR/." "$work/raizlab/tests/tools/armadilha/"
  saida="$("$work/raizlab/tests/tools/armadilha/run_com_armadilha.sh" /usr/bin/true 2>&1)"; rc=$?
  [ "$rc" -eq 1 ] || falha "glintfx-win-lab visivel dentro da raiz deveria REPROVAR a auto-prova (rc=1), obteve $rc: $saida"
  printf '%s' "$saida" | grep -q "glintfx-win-lab" || falha "a reprovacao nao nomeia glintfx-win-lab: $saida"
}

g2_raiz_por_symlink_resolve_fisico() {
  local saida
  mkdir -p "$work/real/tests/tools"
  cp -r "$DIR" "$work/real/tests/tools/armadilha"
  ln -s "$work/real" "$work/lnk"
  saida="$("$work/lnk/tests/tools/armadilha/run_com_armadilha.sh" bash -c 'echo "AQUI=$PWD"' 2>&1)"
  printf '%s' "$saida" | grep -q "AQUI=$work/real\$" || falha "raiz por symlink deveria resolver para o caminho fisico $work/real: $saida"
}

g2_reflink_dentro() {
  # o /var/tmp de dentro e' privado MAS no FS do hospedeiro: o consolidar.sh faz `cp --reflink=always`;
  # tmpfs o quebraria (medido no preci de 29/09). So' vale onde o hospedeiro suporta reflink.
  local saida
  : >"$work/r-a"
  if cp --reflink=always "$work/r-a" "$work/r-b" 2>/dev/null; then
    saida="$("$WRAP" bash -c 'cd /var/tmp && : > a && cp --reflink=always a b && echo reflink-ok' 2>&1)"
    printf '%s' "$saida" | grep -q "reflink-ok" || falha "dentro do envoltorio o /var/tmp nao suporta cp --reflink=always (o hospedeiro suporta): $saida"
  fi
}

g2_raiz_sob_var_tmp() {
  # o --blob e os fake roots vivem sob /var/tmp: o bind da raiz cria o ponto de montagem dentro do
  # /var/tmp privado; a auto-prova aceita SO' esse componente e o comando roda
  mkdir -p "$work/raizfake/tests/tools"
  cp -r "$DIR" "$work/raizfake/tests/tools/armadilha"
  "$work/raizfake/tests/tools/armadilha/run_com_armadilha.sh" /usr/bin/true >"$work/raizfake.out" 2>&1 \
    || falha "com a raiz sob /var/tmp o envoltorio deveria rodar (o ponto de montagem da raiz e' o unico conteudo permitido): $(cat "$work/raizfake.out")"
}

g2_disco_do_hospedeiro_imune() {
  # C-1 do CTO: o "disco da VM" e' um caminho .../glintfx-win-lab/ criado FORA do envoltorio;
  # `qemu-img commit` e `qemu-io write` pelo envoltorio dao "inexistente" e o disco fica igual.
  local base ov antes depois saida rc
  command -v qemu-img >/dev/null 2>&1 && command -v qemu-io >/dev/null 2>&1 || return 0
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
}

g2_armadilha_na_frente_do_path() {
  "$WRAP" bash -c 'case "$(command -v virsh)" in */armadilha/bin/virsh) exit 0 ;; *) echo "virsh resolvido em: $(command -v virsh)" >&2; exit 1 ;; esac' >/dev/null 2>&1 \
    || falha "virsh dentro do envoltorio nao resolve na armadilha"
}

GRUPO1=(g1_ferramentas_saem_97 g1_qemu_img_nao_e_armadilha g1_gates_estaticos_do_envoltorio g1_simulacao_so_neste_script g1_sem_bwrap_sai_77 g1_auto_prova_recusa_raiz_gravavel g1_duble_e_envoltorio_com_o_mesmo_contrato)
GRUPO2=(g2_toca_virsh_reprova g2_rc_atravessa g2_sentinela_do_hospedeiro_invisivel g2_lab_visivel_na_raiz_reprova g2_raiz_por_symlink_resolve_fisico g2_reflink_dentro g2_raiz_sob_var_tmp g2_disco_do_hospedeiro_imune g2_armadilha_na_frente_do_path)

roda_grupo() {  # roda_grupo <funcoes...>: imprime nada; devolve em $ok_grupo quantos controles passaram
  local f antes
  ok_grupo=0
  for f in "$@"; do
    antes="$falhas"
    "$f"
    [ "$falhas" -eq "$antes" ] && ok_grupo=$((ok_grupo + 1))
  done
}

roda_grupo "${GRUPO1[@]}"
echo "armadilha_selftest: grupo 1: ${ok_grupo} controles OK (de ${#GRUPO1[@]}; nao exigem bwrap)"
[ "$falhas" -eq 0 ] || exit 1

if ! tem_bwrap; then
  echo "armadilha_selftest: grupo 2: PULADO: ${#GRUPO2[@]} controles que exigem bwrap (sem bwrap utilizavel neste host): ${GRUPO2[*]}"
  exit 77
fi
roda_grupo "${GRUPO2[@]}"
echo "armadilha_selftest: grupo 2: ${ok_grupo} controles OK (de ${#GRUPO2[@]}; com bwrap)"
[ "$falhas" -eq 0 ] || exit 1
echo "armadilha_selftest: OK - ferramentas registram e saem 97, envoltorio isola (bwrap), reprova log nao vazio, rc atravessa, disco do hospedeiro invisivel e imune a qemu-img commit, sem bwrap sai 77"
exit 0
