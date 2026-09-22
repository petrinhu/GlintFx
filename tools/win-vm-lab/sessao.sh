#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Motorista de sessao da VM Windows do laboratorio (item WIN-RUNNER-PROPRIO,
# sub-fatia V-2, GODS_LAWS.md deste projeto L-09/L-11, e L-35/L-36/L-40/L-45
# do arquivo global). Nao liga a maquina (isso e' V-5) nem altera a
# definicao persistente do dominio (isso e' V-4, com aval do lider) -- este
# script so cobre o que o plano em
# /var/tmp/glintfx-plan/win-runner-local.md secao 2 e 3 pede: a trava de
# exclusao mutua, a segunda verificacao independente por domstate, a
# sobreposicao descartavel que nasce e morre dentro da MESMA posse da
# trava, e a desmontagem incondicional por trap.
#
# DUAS VERIFICACOES INDEPENDENTES, E POR QUE NENHUMA SUBSTITUI A OUTRA:
#   1. flock sobre o arquivo de trava -- protege contra OUTRO sessao.sh,
#      porque so morde quem consulta o mesmo arquivo. E' convencao
#      cooperativa: um `virsh start` digitado a mao nao olha para ela.
#   2. `virsh domstate` -- protege contra QUALQUER OUTRA COISA que tenha
#      ligado a maquina, inclusive por fora deste script (a mao, por outra
#      ferramenta, por outra sessao que nao respeita a trava). E' a unica
#      das duas que enxerga o estado real do hypervisor.
# A trava sozinha nao prova que a maquina esta desligada; o domstate
# sozinho nao impede duas sessoes de tentarem a mesma hora. As duas juntas
# cobrem o que uma sozinha deixa passar (ver GODS_LAWS.md do projeto,
# secao 2.1 do plano).
#
# A SOBREPOSICAO NASCE E MORRE DENTRO DA MESMA POSSE DA TRAVA (plano, secao
# 3): por isso, antes de criar uma nova, este script recusa se achar uma
# sobreposicao ORFA no caminho de sempre -- sinal de que uma sessao
# anterior morreu sem passar pelo teardown (SIGKILL, que nenhum trap
# intercepta). Ausencia de sobreposicao nao prova sessao limpa por si so;
# e' a trava + o domstate + a ausencia de orfa, as tres, que autorizam
# prosseguir.
#
# Uso:
#   sessao.sh [--dom <nome>] [--connect <uri>] [--lock <caminho>]
#             [--base <qcow2>] [--overlay <qcow2>] [-- <comando...>]
#   sessao.sh --selftest
#
# Sem `-- <comando>`, a sessao prepara (trava, domstate, sobreposicao),
# imprime que esta pronta, e desfaz tudo em seguida (rodada de
# preparo/prova). Com `-- <comando>`, o comando roda DEPOIS do preparo e
# ANTES da desmontagem -- e' o ponto onde a sub-fatia V-5 pluga ligar a
# maquina, transferir e executar o trabalho.
set -u
set -o pipefail

# GATE-SELFTEST-ORFAO / GODS_LAWS.md L-40: mesma guarda que
# provar-isolamento.sh (xmlstarlet) e rodar-caminho.sh (jq) ja usam nesta
# mesma pasta. Sem ela, `qemu-img`/`flock` ausentes fariam as chamadas
# falharem em silencio dentro das funcoes, e um --selftest que nunca
# executa a logica real ainda assim poderia imprimir "OK" por acidente de
# fluxo -- a mesma familia de "afirma que mede e nao mede". `exit 77` e' o
# mesmo codigo de AUSENCIA DECLARADA que os dois vizinhos usam, lido pelo
# CTest como "Not Run" (SKIP_RETURN_CODE), nunca como falha nem como
# sucesso. NAO MEDIDO se as imagens de CI (nenhum workflow deste projeto
# instala qemu-img ou flock explicitamente) os tem — ausencia declarada,
# nao suposta.
for _ferramenta in qemu-img flock; do
  if ! command -v "$_ferramenta" >/dev/null 2>&1; then
    echo "AUSENTE: ${_ferramenta} nao encontrado no PATH - script pulado (declarado, nunca silencioso)." >&2
    exit 77
  fi
done
unset _ferramenta

DOM_DEFAULT="glintfx-win11-lab"
CONNECT_DEFAULT="qemu:///session"
LAB_DIR_DEFAULT="/var/tmp/glintfx-win-lab"
LOCK_FILE_DEFAULT="${LAB_DIR_DEFAULT}/.vm.lock"
BASE_DISK_DEFAULT="${LAB_DIR_DEFAULT}/${DOM_DEFAULT}.qcow2"
OVERLAY_DEFAULT="${LAB_DIR_DEFAULT}/${DOM_DEFAULT}.sessao-overlay.qcow2"

SCRIPT_PATH="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)/$(basename -- "${BASH_SOURCE[0]}")"

uso() {
  echo "Uso: $0 [--dom <nome>] [--connect <uri>] [--lock <caminho>] [--base <qcow2>] [--overlay <qcow2>] [-- <comando...>] | --selftest" >&2
  exit 2
}

# --- 1. trava de exclusao mutua ---------------------------------------------
# Abre o arquivo de trava em modo ANEXAR (>>), nunca truncar (>): duas
# sessoes abrindo o MESMO arquivo com `>` uma depois da outra apagariam o
# conteudo escrito pela primeira antes mesmo de disputar o flock. O registro
# de posse (quem segura) vive num arquivo IRMAO (<lock>.info), escrito so
# depois que a trava foi tomada de verdade -- nunca antes.
tomar_trava() {
  local lock_file="$1" info_file="$2" identificador="$3"
  mkdir -p -- "$(dirname -- "$lock_file")" 2>/dev/null

  exec 9>>"$lock_file"
  if ! flock -n 9; then
    echo "RECUSADO: a trava '${lock_file}' ja esta em posse de outro processo." >&2
    if [ -s "$info_file" ]; then
      echo "quem segura, pelo ultimo registro:" >&2
      sed 's/^/      /' "$info_file" >&2
    else
      echo "(sem registro de posse legivel -- primeira disputa desta trava)" >&2
    fi
    exec 9>&- 2>/dev/null || true
    return 1
  fi

  local tmp
  tmp="$(mktemp "${info_file}.XXXXXX")"
  {
    printf 'pid=%s\n' "$$"
    printf 'data=%s\n' "$(date '+%d/%m/%y - %H:%M:%S')"
    printf 'identificador=%s\n' "$identificador"
  } >"$tmp"
  mv -f -- "$tmp" "$info_file"
  return 0
}

soltar_trava() {
  exec 9>&- 2>/dev/null || true
}

# --- 2. segunda verificacao independente: domstate ---------------------------
verificar_domstate_livre() {
  local dom="$1" connect="$2"
  local estado rc

  estado="$(virsh -c "$connect" domstate -- "$dom" 2>&1)"
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: 'virsh -c ${connect} domstate ${dom}' falhou (codigo ${rc}). Saida:" >&2
    echo "$estado" >&2
    return 2
  fi

  echo "domstate lido: '${estado}'"
  if [ "$estado" != "desligado" ]; then
    echo "RECUSADO: a maquina '${dom}' nao esta desligada (domstate='${estado}')." >&2
    return 1
  fi
  return 0
}

# --- 3. sobreposicao descartavel ---------------------------------------------
detectar_sobreposicao_orfa() {
  local overlay="$1"
  if [ -e "$overlay" ]; then
    echo "RECUSADO: sobreposicao orfa encontrada em '${overlay}' -- uma sessao anterior nao passou pelo teardown (provavel SIGKILL)." >&2
    echo "Esta execucao ja segura a trava; o teardown dela remove a orfa antes de sair, entao a PROXIMA tentativa encontra o caminho limpo. Esta mesma tentativa recusa, e nao tenta reaproveitar um disco cujo conteudo nao e' confiavel." >&2
    return 1
  fi
  return 0
}

criar_sobreposicao() {
  local base="$1" overlay="$2" rc

  if [ ! -f "$base" ]; then
    echo "ERRO: disco base nao existe: ${base}" >&2
    return 1
  fi
  if [ -e "$overlay" ]; then
    echo "ERRO: sobreposicao ja existe, recuso sobrescrever: ${overlay}" >&2
    return 1
  fi

  qemu-img create -f qcow2 -b "$base" -F qcow2 -- "$overlay" >/dev/null
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: 'qemu-img create' da sobreposicao falhou (codigo ${rc})." >&2
    return 1
  fi
  echo "sobreposicao criada: ${overlay} (base=${base})"
  return 0
}

remover_sobreposicao() {
  local overlay="$1"
  [ -z "$overlay" ] && return 0
  if [ -e "$overlay" ]; then
    rm -f -- "$overlay"
    echo "sobreposicao removida: ${overlay}"
  fi
  return 0
}

# --- 4. desmontagem incondicional (trap) -------------------------------------
LOCK_TAKEN=0
TEARDOWN_OVERLAY=""

teardown() {
  local rc=$?
  remover_sobreposicao "$TEARDOWN_OVERLAY"
  if [ "$LOCK_TAKEN" -eq 1 ]; then
    soltar_trava
    LOCK_TAKEN=0
  fi
  return "$rc"
}

sinal_recebido() {
  echo "sinal recebido, encerrando a sessao..." >&2
  exit 143
}

# --- orquestracao -------------------------------------------------------------
rodar_sessao() {
  local dom="$1" connect="$2" lock_file="$3" base="$4" overlay="$5"
  shift 5
  local info_file="${lock_file}.info"

  TEARDOWN_OVERLAY="$overlay"
  trap teardown EXIT
  trap sinal_recebido TERM INT HUP

  if ! tomar_trava "$lock_file" "$info_file" "pid-$$"; then
    return 1
  fi
  LOCK_TAKEN=1

  if ! verificar_domstate_livre "$dom" "$connect"; then
    return 1
  fi

  if ! detectar_sobreposicao_orfa "$overlay"; then
    return 1
  fi

  if ! criar_sobreposicao "$base" "$overlay"; then
    return 1
  fi

  echo "SESSAO PRONTA: trava tomada, maquina confirmada desligada, sobreposicao descartavel criada."
  if [ $# -gt 0 ]; then
    echo "rodando comando sob a sessao: $*"
    "$@"
    return $?
  fi
  echo "nenhum comando passado -- rodada de preparo/prova; desfazendo em seguida."
  return 0
}

# --- selftest ------------------------------------------------------------------

selftest_e1() {
  local work_dir="$1" lock_file info_file rc2 holder_pid

  lock_file="${work_dir}/e1.vm.lock"
  info_file="${lock_file}.info"

  echo ">>> E1 1/2: primeiro processo toma a trava e a mantem aberta"
  (
    exec 9>>"$lock_file"
    flock -x 9
    {
      printf 'pid=%s\n' "$$"
      printf 'data=%s\n' "$(date '+%d/%m/%y - %H:%M:%S')"
      printf 'identificador=%s\n' "processo-A-do-teste"
    } >"$info_file"
    sleep 5
  ) &
  holder_pid=$!
  sleep 1

  echo ">>> E1 2/2: segundo processo tenta tomar a MESMA trava (esperado: recusa)"
  tomar_trava "$lock_file" "$info_file" "processo-B-do-teste"
  rc2=$?
  echo ">>> codigo obtido do segundo processo: ${rc2}"

  kill "$holder_pid" 2>/dev/null
  wait "$holder_pid" 2>/dev/null

  if [ "$rc2" -ne 0 ]; then
    echo "E1 OK: a segunda tentativa foi recusada (codigo ${rc2}) enquanto a primeira segurava a trava."
    return 0
  fi
  echo "E1 FALHOU: a segunda tentativa deveria ter sido recusada e nao foi (codigo ${rc2})."
  return 1
}

selftest_e2() {
  local work_dir="$1" stub_dir saida rc

  stub_dir="${work_dir}/e2-bin"
  mkdir -p "$stub_dir"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
# duble: sempre diz que o dominio esta executando, mesmo com a trava livre
echo "executando"
exit 0
STUB
  chmod +x "${stub_dir}/virsh"

  echo ">>> E2: domstate simulado 'executando' com a trava LIVRE (esperado: recusa mesmo assim)"
  saida="$(PATH="${stub_dir}:$PATH" verificar_domstate_livre "duble-dom" "qemu:///session" 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido: ${rc}"

  if [ "$rc" -ne 0 ]; then
    echo "E2 OK: recusado com a maquina simulada ligada, mesmo a trava estando livre."
    return 0
  fi
  echo "E2 FALHOU: deveria ter recusado e nao recusou (codigo ${rc})."
  return 1
}

selftest_e7() {
  local work_dir="$1" base overlay lock_file stub_dir pid rc

  base="${work_dir}/e7-base.qcow2"
  overlay="${work_dir}/e7-overlay.qcow2"
  lock_file="${work_dir}/e7.vm.lock"
  stub_dir="${work_dir}/e7-bin"
  mkdir -p "$stub_dir"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
echo "desligado"
exit 0
STUB
  chmod +x "${stub_dir}/virsh"
  qemu-img create -f qcow2 -- "$base" 10M >/dev/null

  echo ">>> E7a 1/3: sobe a sessao em segundo plano, com 'sleep 10' como comando"
  (
    PATH="${stub_dir}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
      --lock "$lock_file" --base "$base" --overlay "$overlay" -- sleep 10
  ) &
  pid=$!
  sleep 2

  echo ">>> E7a 2/3: sobreposicao deveria existir agora (sessao em andamento):"
  ls -la "$overlay" 2>&1

  echo ">>> E7a 3/3: mando SIGTERM no meio da sessao"
  kill -TERM "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null
  rc=$?
  echo ">>> codigo de saida do motorista apos SIGTERM: ${rc}"

  local e7a_ok=1
  if [ -e "$overlay" ]; then
    echo "E7a FALHOU: sobreposicao ainda existe depois do SIGTERM (${overlay})."
    e7a_ok=0
  else
    echo "E7a: sobreposicao removida apos SIGTERM -- OK."
  fi
  if PATH="${stub_dir}:$PATH" bash -c "exec 9>>'$lock_file'; flock -n 9"; then
    echo "E7a: trava livre depois do SIGTERM -- OK."
  else
    echo "E7a FALHOU: trava continua presa depois do SIGTERM."
    e7a_ok=0
  fi

  echo
  echo ">>> E7b (espelho SIGKILL) 1/3: sobe outra sessao e mata com SIGKILL (trap NAO roda)"
  (
    PATH="${stub_dir}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
      --lock "$lock_file" --base "$base" --overlay "$overlay" -- sleep 10
  ) &
  pid=$!
  sleep 2
  echo ">>> E7b 2/3: sobreposicao antes do SIGKILL:"
  ls -la "$overlay" 2>&1
  kill -KILL "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null

  local e7b_ok=1
  if [ ! -e "$overlay" ]; then
    echo "E7b FALHOU: a sobreposicao sumiu sozinha -- o cenario nao provou nada (precisa sobrar orfa apos SIGKILL)."
    e7b_ok=0
  else
    echo "E7b: sobreposicao ORFA confirmada em disco apos SIGKILL (esperado -- nenhum trap intercepta SIGKILL)."
  fi

  echo ">>> E7b 3/3: proxima execucao tem de RECUSAR por achar a sobreposicao orfa"
  PATH="${stub_dir}:$PATH" "$SCRIPT_PATH" --dom duble-dom --connect qemu:///session \
    --lock "$lock_file" --base "$base" --overlay "$overlay" -- true
  rc=$?
  echo ">>> codigo da execucao seguinte: ${rc}"
  if [ "$rc" -ne 0 ]; then
    echo "E7b: a execucao seguinte recusou por causa da sobreposicao orfa -- OK (estado sujo detectado, nunca escondido)."
  else
    echo "E7b FALHOU: a execucao seguinte deveria ter recusado e nao recusou."
    e7b_ok=0
  fi
  rm -f -- "$overlay"

  if [ "$e7a_ok" -eq 1 ] && [ "$e7b_ok" -eq 1 ]; then
    return 0
  fi
  return 1
}

selftest() {
  local work_dir ok=1
  work_dir="$(mktemp -d /var/tmp/glintfx-sessao-selftest.XXXXXX)"
  trap 'rm -rf -- "$work_dir"' RETURN

  echo "=== SELFTEST sessao.sh (E1, E2, E7) -- diretorio de trabalho: ${work_dir} ==="
  echo
  selftest_e1 "$work_dir" || ok=0
  echo
  selftest_e2 "$work_dir" || ok=0
  echo
  selftest_e7 "$work_dir" || ok=0
  echo

  if [ "$ok" -eq 1 ]; then
    echo "SELFTEST OK: E1, E2 e E7 se comportaram como esperado."
    return 0
  fi
  echo "SELFTEST FALHOU: pelo menos um cenario nao se comportou como esperado."
  return 1
}

# --- despacho ------------------------------------------------------------------

if [ "${1:-}" = "--selftest" ]; then
  selftest
  exit $?
fi

DOM="$DOM_DEFAULT"
CONNECT="$CONNECT_DEFAULT"
LOCK_FILE="$LOCK_FILE_DEFAULT"
BASE_DISK="$BASE_DISK_DEFAULT"
OVERLAY_PATH="$OVERLAY_DEFAULT"
COMANDO=()

while [ $# -gt 0 ]; do
  case "$1" in
    --dom) DOM="${2:?}"; shift 2 ;;
    --connect) CONNECT="${2:?}"; shift 2 ;;
    --lock) LOCK_FILE="${2:?}"; shift 2 ;;
    --base) BASE_DISK="${2:?}"; shift 2 ;;
    --overlay) OVERLAY_PATH="${2:?}"; shift 2 ;;
    --)
      shift
      COMANDO=("$@")
      break
      ;;
    -h|--help) uso ;;
    *) uso ;;
  esac
done

rodar_sessao "$DOM" "$CONNECT" "$LOCK_FILE" "$BASE_DISK" "$OVERLAY_PATH" "${COMANDO[@]}"
exit $?
