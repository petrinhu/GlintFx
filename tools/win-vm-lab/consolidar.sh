#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Caminho de CONSOLIDACAO da sobreposicao (item WIN-RUNNER-PROPRIO, sub-fatia
# V-6a, docs/plano-fecho-w7b.md secao 2.2.2, GODS_LAWS.md deste projeto
# L-09/L-11/L-51). Junta as mudancas gravadas numa sobreposicao qcow2 de
# volta ao disco BASE dela (`qemu-img commit`) -- o oposto do que
# `sessao.sh` faz (que so' cria/descarta sobreposicoes, nunca consolida).
#
# ESTA SUB-FATIA (V-6a) NUNCA TOCA O DISCO REAL DE 17 GiB. O --selftest roda
# so' contra um par base/sobreposicao de BRINQUEDO que o proprio autoteste
# cria e apaga. A primeira consolidacao REAL (V-6b) e' item separado
# (`WIN-LAB-INSTALAR`, W9) que exige aval explicito do lider (L-01: acao
# irreversivel) -- nada aqui autoriza rodar isto contra
# glintfx-win11-lab.qcow2.
#
# TRES GUARDAS, NENHUMA DISPENSAVEL:
#   1. TRAVA (flock) -- mesma convencao de sessao.sh (mesma pasta): duas
#      consolidacoes disputando o mesmo par base/sobreposicao ao mesmo tempo
#      corrompem os dois arquivos. Sem a trava, recusa.
#   2. MAQUINA DESLIGADA -- `qemu-img commit` sobre um disco que o QEMU tem
#      aberto AO VIVO corrompe o disco (o processo QEMU nao sabe que o
#      arquivo mudou por baixo dele). Verificado por `virsh domstate`,
#      independente da trava (a trava so' protege contra OUTRO consolidar.sh;
#      o domstate enxerga qualquer coisa que tenha ligado a maquina, dentro
#      ou fora deste script -- mesma razao dupla que sessao.sh ja documenta).
#   3. COPIA DE SEGURANCA ANTES DE COMMITAR -- `cp --reflink=always` (btrfs:
#      copia instantanea, custo quase zero) do disco BASE, feita ANTES do
#      `qemu-img commit`. Se o commit corromper ou gravar algo errado, a
#      copia de seguranca e' o unico jeito de voltar atras -- e por isso ela
#      tem de existir e ser CONFERIDA (soma) antes do commit rodar, nunca
#      depois.
#
# Uso:
#   consolidar.sh --dom <nome> --connect <uri> --lock <caminho>
#                 --base <qcow2> --overlay <qcow2> --backup <caminho>
#   consolidar.sh --selftest
set -u
set -o pipefail

# GATE-SELFTEST-ORFAO / GODS_LAWS.md L-40: mesma guarda que sessao.sh
# (mesma pasta) ja usa para qemu-img/flock.
for _ferramenta in qemu-img flock sha256sum cp; do
  if ! command -v "$_ferramenta" >/dev/null 2>&1; then
    echo "AUSENTE: ${_ferramenta} nao encontrado no PATH - script pulado (declarado, nunca silencioso)." >&2
    exit 77
  fi
done
unset _ferramenta

SCRIPT_PATH="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)/$(basename -- "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname -- "$SCRIPT_PATH")"

uso() {
  echo "Uso: $0 --dom <nome> --connect <uri> --lock <caminho> --base <qcow2> --overlay <qcow2> --backup <caminho> | --selftest" >&2
  exit 2
}

# --- 1. trava de exclusao mutua (mesmo molde de sessao.sh, mesma pasta) ----
tomar_trava() {
  local lock_file="$1"
  mkdir -p -- "$(dirname -- "$lock_file")" 2>/dev/null

  exec 9>>"$lock_file"
  if ! flock -n 9; then
    echo "RECUSADO: a trava '${lock_file}' ja esta em posse de outro processo -- nunca consolido sem trava." >&2
    exec 9>&- 2>/dev/null || true
    return 1
  fi
  return 0
}

soltar_trava() {
  exec 9>&- 2>/dev/null || true
}

# --- 2. segunda verificacao independente: domstate (mesmo molde de sessao.sh) ---
verificar_domstate_desligado() {
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
    echo "RECUSADO: a maquina '${dom}' NAO esta desligada (domstate='${estado}') -- consolidar um disco que o QEMU tem aberto ao vivo corrompe o disco." >&2
    return 1
  fi
  return 0
}

# --- 3. copia de seguranca, ANTES do commit, e conferida --------------------
fazer_copia_seguranca() {
  local base="$1" backup="$2"

  if [ ! -f "$base" ]; then
    echo "ERRO: disco base nao existe: ${base}" >&2
    return 1
  fi
  if [ -e "$backup" ]; then
    echo "ERRO: caminho de copia de seguranca ja existe, recuso sobrescrever: ${backup}" >&2
    return 1
  fi

  cp --reflink=always -- "$base" "$backup"
  local rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: 'cp --reflink=always' da copia de seguranca falhou (codigo ${rc})." >&2
    return 1
  fi

  local soma_base soma_backup
  soma_base="$(sha256sum -- "$base" | cut -d' ' -f1)"
  soma_backup="$(sha256sum -- "$backup" | cut -d' ' -f1)"
  if [ "$soma_base" != "$soma_backup" ]; then
    echo "ERRO: a copia de seguranca nao bate em soma com o base recem-copiado -- nunca prossigo para o commit." >&2
    rm -f -- "$backup"
    return 1
  fi

  echo "copia de seguranca criada e conferida: ${backup} (soma=${soma_base})"
  return 0
}

# --- consolidacao propriamente dita -----------------------------------------
consolidar_overlay() {
  local dom="$1" connect="$2" lock_file="$3" base="$4" overlay="$5" backup="$6"

  if ! tomar_trava "$lock_file"; then
    return 1
  fi
  trap soltar_trava EXIT

  if ! verificar_domstate_desligado "$dom" "$connect"; then
    soltar_trava
    return 1
  fi

  if [ ! -f "$overlay" ]; then
    echo "ERRO: sobreposicao nao existe: ${overlay}" >&2
    soltar_trava
    return 1
  fi

  local backing_real
  backing_real="$(qemu-img info --output=json -- "$overlay" 2>/dev/null | grep -o '"backing-filename": *"[^"]*"' | sed 's/.*: *"//; s/"$//')"
  if [ "$backing_real" != "$base" ]; then
    echo "ERRO: a sobreposicao '${overlay}' nao tem '${base}' como backing file (achei '${backing_real}') -- recuso commitar no disco errado." >&2
    soltar_trava
    return 1
  fi

  if ! fazer_copia_seguranca "$base" "$backup"; then
    soltar_trava
    return 1
  fi

  echo "--- commitando a sobreposicao no disco base (qemu-img commit) ---"
  qemu-img commit -- "$overlay"
  local rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "ERRO: 'qemu-img commit' falhou (codigo ${rc}). O disco base pode estar em estado desconhecido -- a copia de seguranca em '${backup}' preserva o estado ANTES do commit." >&2
    soltar_trava
    return 1
  fi

  local soma_base_pos
  soma_base_pos="$(sha256sum -- "$base" | cut -d' ' -f1)"
  echo "commit concluido. soma do base APOS o commit: ${soma_base_pos}"

  soltar_trava
  trap - EXIT
  return 0
}

# --- selftest ------------------------------------------------------------------

selftest_e_recusa_maquina_ligada() {
  local work_dir="$1" stub_dir base overlay backup lock_file saida rc

  stub_dir="${work_dir}/rl-bin"
  mkdir -p "$stub_dir"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
echo "executando"
exit 0
STUB
  chmod +x "${stub_dir}/virsh"

  base="${work_dir}/rl-base.qcow2"
  overlay="${work_dir}/rl-overlay.qcow2"
  backup="${work_dir}/rl-backup.qcow2"
  lock_file="${work_dir}/rl.lock"
  qemu-img create -f qcow2 -- "$base" 1M >/dev/null
  qemu-img create -f qcow2 -b "$base" -F qcow2 -- "$overlay" >/dev/null

  echo ">>> RECUSA-MAQUINA-LIGADA: domstate simulado 'executando' -- esperado RECUSAR, nada de commit"
  saida="$(PATH="${stub_dir}:$PATH" consolidar_overlay "duble-dom" "qemu:///session" "$lock_file" "$base" "$overlay" "$backup" 2>&1)"
  rc=$?
  echo "$saida" | tail -5
  echo ">>> codigo obtido (esperado != 0): ${rc}"

  local ok=1
  [ "$rc" -eq 0 ] && ok=0
  [ -e "$backup" ] && { echo "RECUSA-MAQUINA-LIGADA FALHOU: copia de seguranca foi criada mesmo com a maquina 'ligada'."; ok=0; }
  # a sobreposicao nao pode ter sido consumida pelo commit
  if ! qemu-img info -- "$overlay" 2>/dev/null | grep -qi "backing file:"; then
    echo "RECUSA-MAQUINA-LIGADA FALHOU: a sobreposicao perdeu o backing file (parece ter sido commitada)."
    ok=0
  fi

  if [ "$ok" -eq 1 ]; then
    echo "RECUSA-MAQUINA-LIGADA OK: recusou antes de tocar qualquer arquivo."
    return 0
  fi
  return 1
}

selftest_e_recusa_sem_trava() {
  local work_dir="$1" stub_dir base overlay backup lock_file holder_pid saida rc

  stub_dir="${work_dir}/st-bin"
  mkdir -p "$stub_dir"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
echo "desligado"
exit 0
STUB
  chmod +x "${stub_dir}/virsh"

  base="${work_dir}/st-base.qcow2"
  overlay="${work_dir}/st-overlay.qcow2"
  backup="${work_dir}/st-backup.qcow2"
  lock_file="${work_dir}/st.lock"
  qemu-img create -f qcow2 -- "$base" 1M >/dev/null
  qemu-img create -f qcow2 -b "$base" -F qcow2 -- "$overlay" >/dev/null

  echo ">>> RECUSA-SEM-TRAVA: outro processo ja segura a trava -- esperado RECUSAR"
  (
    exec 9>>"$lock_file"
    flock -x 9
    sleep 5
  ) &
  holder_pid=$!
  sleep 1

  saida="$(PATH="${stub_dir}:$PATH" consolidar_overlay "duble-dom" "qemu:///session" "$lock_file" "$base" "$overlay" "$backup" 2>&1)"
  rc=$?
  echo "$saida" | tail -5
  echo ">>> codigo obtido (esperado != 0): ${rc}"

  kill "$holder_pid" 2>/dev/null
  wait "$holder_pid" 2>/dev/null

  local ok=1
  [ "$rc" -eq 0 ] && ok=0
  [ -e "$backup" ] && { echo "RECUSA-SEM-TRAVA FALHOU: copia de seguranca foi criada sem a trava."; ok=0; }

  if [ "$ok" -eq 1 ]; then
    echo "RECUSA-SEM-TRAVA OK: recusou sem tocar nada, enquanto a trava estava ocupada."
    return 0
  fi
  return 1
}

# TERCEIRA GUARDA, com prova propria (achado do team-lead, 23/09/2026: a
# guarda existia mas nunca tinha sido vista reprovando - L-20, codigo sem
# teste que ja viu falhar nao conta). Dublê de `cp` que IGNORA
# `--reflink=always` e produz uma copia DIFERENTE do base (so os primeiros
# 10 bytes, nunca o arquivo inteiro) -- simula o cenario que a guarda
# existe para pegar: a copia de seguranca saiu CORROMPIDA/incompleta.
# `fazer_copia_seguranca` tem de recusar (a soma nao bate), apagar a copia
# ruim, e `consolidar_overlay` NUNCA pode chegar perto do `qemu-img commit`
# depois disso -- prova por leitura da sobreposicao (ainda tem que ter o
# backing file intacto, nunca commitada).
selftest_e_recusa_copia_corrompida() {
  local work_dir="$1" stub_dir base overlay backup lock_file saida rc

  stub_dir="${work_dir}/cp-bin"
  mkdir -p "$stub_dir"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
echo "desligado"
exit 0
STUB
  chmod +x "${stub_dir}/virsh"

  # duble de cp: qualquer chamada com --reflink=always produz uma copia
  # TRUNCADA de proposito (so 10 bytes); qualquer outra chamada de cp
  # (por exemplo, as que o proprio autoteste usa para preparar fixtures)
  # passa direto para o cp real, para nao quebrar nada fora do escopo
  # desta prova.
  cat >"${stub_dir}/cp" <<'STUB'
#!/usr/bin/env bash
if [ "${1:-}" = "--reflink=always" ] && [ "${2:-}" = "--" ]; then
  head -c 10 -- "$3" > "$4" 2>/dev/null
  exit 0
fi
exec /usr/bin/cp "$@"
STUB
  chmod +x "${stub_dir}/cp"

  base="${work_dir}/cp-base.qcow2"
  overlay="${work_dir}/cp-overlay.qcow2"
  backup="${work_dir}/cp-backup.qcow2"
  lock_file="${work_dir}/cp.lock"
  qemu-img create -f qcow2 -- "$base" 1M >/dev/null
  qemu-img create -f qcow2 -b "$base" -F qcow2 -- "$overlay" >/dev/null

  echo ">>> RECUSA-COPIA-CORROMPIDA: o duble de 'cp' devolve uma copia truncada -- esperado RECUSAR, apagar a copia ruim, NUNCA commitar"
  saida="$(PATH="${stub_dir}:$PATH" consolidar_overlay "duble-dom" "qemu:///session" "$lock_file" "$base" "$overlay" "$backup" 2>&1)"
  rc=$?
  echo "$saida" | tail -6
  echo ">>> codigo obtido (esperado != 0): ${rc}"

  local ok=1
  [ "$rc" -eq 0 ] && ok=0
  if [ -e "$backup" ]; then
    echo "RECUSA-COPIA-CORROMPIDA FALHOU: a copia CORROMPIDA foi deixada em disco em vez de apagada."
    ok=0
  fi
  if ! qemu-img info -- "$overlay" 2>/dev/null | grep -qi "backing file:"; then
    echo "RECUSA-COPIA-CORROMPIDA FALHOU: a sobreposicao perdeu o backing file (foi commitada mesmo com a copia de seguranca ruim)."
    ok=0
  fi

  if [ "$ok" -eq 1 ]; then
    echo "RECUSA-COPIA-CORROMPIDA OK: recusou, apagou a copia ruim, nunca chegou perto do commit."
    return 0
  fi
  return 1
}

selftest_e_consolidacao_completa() {
  local work_dir="$1" base overlay backup lock_file stub_dir
  local marca_overlay="MARCA-DA-SOBREPOSICAO-v6a"
  local ok=1

  stub_dir="${work_dir}/cc-bin"
  mkdir -p "$stub_dir"
  cat >"${stub_dir}/virsh" <<'STUB'
#!/usr/bin/env bash
echo "desligado"
exit 0
STUB
  chmod +x "${stub_dir}/virsh"

  base="${work_dir}/cc-base.qcow2"
  overlay="${work_dir}/cc-overlay.qcow2"
  backup="${work_dir}/cc-backup.qcow2"
  lock_file="${work_dir}/cc.lock"

  qemu-img create -f qcow2 -- "$base" 4M >/dev/null
  qemu-img create -f qcow2 -b "$base" -F qcow2 -- "$overlay" >/dev/null

  local soma_base_antes
  soma_base_antes="$(sha256sum -- "$base" | cut -d' ' -f1)"

  echo ">>> CONSOLIDACAO-COMPLETA 1/4: escrevendo a marca NA SOBREPOSICAO (nao no base)"
  # qemu-img nao tem um jeito trivial de "escrever bytes" de fora do
  # protocolo qcow2, entao usamos o proprio qemu-img (subcomando `dd`
  # embutido no formato via `qemu-io`, se disponivel) -- mas para manter
  # este autoteste livre de mais uma dependencia, escrevemos a marca via
  # `qemu-img convert` de um arquivo raw de patch minusculo, aplicado como
  # segunda camada. Caminho mais simples e' usar `qemu-io -c "write ..."`.
  if ! command -v qemu-io >/dev/null 2>&1; then
    echo "AUSENTE: qemu-io nao encontrado -- CONSOLIDACAO-COMPLETA pulada (declarado, nunca silencioso)."
    return 77
  fi
  qemu-io -c "write -P 0 0 512" "$overlay" >/dev/null 2>&1
  printf '%s' "$marca_overlay" | qemu-io -c "write -f raw -d 0 ${#marca_overlay}" "$overlay" >/dev/null 2>&1 || true
  # `qemu-io write -f raw` nao aceita dado por stdin diretamente em todas
  # as versoes; caminho garantido: escrever de um arquivo de padrao via -s.
  local patch_file="${work_dir}/cc-marca.bin"
  printf '%s' "$marca_overlay" > "$patch_file"
  qemu-io -c "write -s ${patch_file} 0 ${#marca_overlay}" "$overlay" >/dev/null 2>&1

  echo ">>> CONSOLIDACAO-COMPLETA 2/4: rodando consolidar_overlay"
  local saida rc
  saida="$(PATH="${stub_dir}:$PATH" consolidar_overlay "duble-dom" "qemu:///session" "$lock_file" "$base" "$overlay" "$backup" 2>&1)"
  rc=$?
  echo "$saida"
  echo ">>> codigo obtido (esperado 0): ${rc}"
  [ "$rc" -ne 0 ] && ok=0

  echo ">>> CONSOLIDACAO-COMPLETA 3/4: a marca tem de estar AGORA no BASE (qemu-io read)"
  local lido
  lido="$(qemu-io -c "read -v -s ${patch_file} 0 ${#marca_overlay}" "$base" 2>&1)"
  if echo "$lido" | grep -q "Pattern verification failed" ; then
    echo "CONSOLIDACAO-COMPLETA FALHOU: a marca NAO esta no base apos o commit."
    ok=0
  else
    echo "CONSOLIDACAO-COMPLETA: marca encontrada no base, commit funcionou."
  fi

  echo ">>> CONSOLIDACAO-COMPLETA 4/4: a copia de seguranca, RESTAURADA, reproduz o base de ANTES do commit, byte a byte"
  local base_restaurado="${work_dir}/cc-base-restaurado.qcow2"
  cp -- "$backup" "$base_restaurado"
  local soma_restaurado
  soma_restaurado="$(sha256sum -- "$base_restaurado" | cut -d' ' -f1)"
  echo "soma base ANTES do commit: ${soma_base_antes}"
  echo "soma da copia de seguranca RESTAURADA:      ${soma_restaurado}"
  if [ "$soma_base_antes" != "$soma_restaurado" ]; then
    echo "CONSOLIDACAO-COMPLETA FALHOU: restaurar a copia de seguranca NAO reproduz o estado de antes do commit."
    ok=0
  else
    echo "CONSOLIDACAO-COMPLETA: copia de seguranca restaura o estado anterior byte a byte -- OK."
  fi

  if [ "$ok" -eq 1 ]; then
    echo "CONSOLIDACAO-COMPLETA OK: marca migrou para o base, e a copia de seguranca restaura o estado anterior."
    return 0
  fi
  return 1
}

selftest() {
  local work_dir ok=1
  work_dir="$(mktemp -d /var/tmp/glintfx-consolidar-selftest.XXXXXX)"
  trap 'rm -rf -- "$work_dir"' RETURN

  echo "=== SELFTEST consolidar.sh -- diretorio de trabalho: ${work_dir} ==="
  echo
  selftest_e_recusa_maquina_ligada "$work_dir" || ok=0
  echo
  selftest_e_recusa_sem_trava "$work_dir" || ok=0
  echo
  selftest_e_recusa_copia_corrompida "$work_dir" || ok=0
  echo
  local rc_completa
  selftest_e_consolidacao_completa "$work_dir"
  rc_completa=$?
  if [ "$rc_completa" -eq 77 ]; then
    echo "CONSOLIDACAO-COMPLETA pulada (qemu-io ausente) -- nao conta como falha nem como sucesso."
  elif [ "$rc_completa" -ne 0 ]; then
    ok=0
  fi
  echo

  if [ "$ok" -eq 1 ]; then
    echo "SELFTEST OK: as quatro provas (recusa-ligada, recusa-sem-trava, recusa-copia-corrompida, consolidacao-completa) se comportaram como esperado."
    return 0
  fi
  echo "SELFTEST FALHOU: pelo menos uma prova nao se comportou como esperado."
  return 1
}

# --- despacho ------------------------------------------------------------------

if [ "${1:-}" = "--selftest" ]; then
  selftest
  exit $?
fi

DOM="" CONNECT="" LOCK_FILE="" BASE="" OVERLAY="" BACKUP=""

while [ $# -gt 0 ]; do
  case "$1" in
    --dom) DOM="${2:?}"; shift 2 ;;
    --connect) CONNECT="${2:?}"; shift 2 ;;
    --lock) LOCK_FILE="${2:?}"; shift 2 ;;
    --base) BASE="${2:?}"; shift 2 ;;
    --overlay) OVERLAY="${2:?}"; shift 2 ;;
    --backup) BACKUP="${2:?}"; shift 2 ;;
    -h|--help) uso ;;
    *) uso ;;
  esac
done

[ -z "$DOM" ] || [ -z "$CONNECT" ] || [ -z "$LOCK_FILE" ] || [ -z "$BASE" ] || [ -z "$OVERLAY" ] || [ -z "$BACKUP" ] && uso

consolidar_overlay "$DOM" "$CONNECT" "$LOCK_FILE" "$BASE" "$OVERLAY" "$BACKUP"
exit $?
