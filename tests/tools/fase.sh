#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# fase.sh - helper de FASE do CI-SPLIT-PER-OS A5 (F1, docs/plano-ci-split-per-os.md, DESENHO.md 3.3).
# Todo teste pesado (que monta build aninhado) imprime `FASE configure|build|resto: X.XX s` e a linha
# `paralelismo aninhado: <n|ausente>` na saida padrao E as grava no arquivo LATERAL
# <GLINTFX_FASES_DIR>/<GLINTFX_FASES_TESTE>.txt: o ctest corta a saida de teste aprovado em 1024
# bytes (medido nos artefatos da calibracao), so' o arquivo lateral e' confiavel. O formato e' o MESMO
# nas tres linguagens (fase.py, tools/ci/fase.ps1): tests/tools/fixtures/fase/esperado*.txt.
# Relogio: /proc/uptime (CLOCK_BOOTTIME, monotonico), lido com `read` embutido, em centesimos.
#
# Uso:  source tests/tools/fase.sh
#         fase_inicio configure; ...; fase_fim configure
#         fase_paralelismo
#       bash tests/tools/fase.sh --selftest
# I-3 (CTO): o helper sempre ACRESCENTA ao lateral; a limpeza de <build>/fases/ antes do ctest e' de quem
# orquestra (preci e ci.yml), e quem le (F4/F9) reprova FASE repetida e mais de uma linha de paralelismo.
# FALHA FECHADA (I-1): erro ao gravar o lateral ou fim sem inicio ENCERRAM o script (exit 3 / exit 2, com a
# mensagem nomeando o arquivo ou a fase); GLINTFX_FASES_DIR ausente segue em silencio (so' a saida padrao).
declare -gA _FASE_T0=()

_fase_agora_cs() {
  local up
  read -r up _ </proc/uptime
  up="${up/./}"
  printf '%s\n' "$((10#$up))"
}

_fase_emitir() {
  printf '%s\n' "$1"
  if [ -n "${GLINTFX_FASES_DIR:-}" ] && [ -n "${GLINTFX_FASES_TESTE:-}" ]; then
    if ! { mkdir -p -- "$GLINTFX_FASES_DIR" && printf '%s\n' "$1" >>"$GLINTFX_FASES_DIR/$GLINTFX_FASES_TESTE.txt"; } 2>/dev/null; then
      echo "fase.sh: nao consegui gravar o arquivo lateral $GLINTFX_FASES_DIR/$GLINTFX_FASES_TESTE.txt" >&2
      exit 3
    fi
  fi
}

fase_registrar() {  # fase_registrar <nome> <centesimos>
  _fase_emitir "$(printf 'FASE %s: %d.%02d s' "$1" "$(($2 / 100))" "$(($2 % 100))")"
}

fase_inicio() {  # fase_inicio <nome>
  _FASE_T0["$1"]="$(_fase_agora_cs)"
}

fase_fim() {  # fase_fim <nome>
  local t0="${_FASE_T0[$1]:-}"
  if [ -z "$t0" ]; then
    echo "fase.sh: fase_fim '$1' sem fase_inicio" >&2
    exit 2
  fi
  fase_registrar "$1" "$(($(_fase_agora_cs) - t0))"
  unset "_FASE_T0[$1]"
}

fase_paralelismo() {
  _fase_emitir "paralelismo aninhado: ${CMAKE_BUILD_PARALLEL_LEVEL:-ausente}"
}

_fase_selftest() {
  local aqui fx tmp ok=1 saida rc cs
  aqui="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
  tmp="$(mktemp -d "${TMPDIR:-/var/tmp}/glintfx-fase-selftest.XXXXXX")" || return 1
  # a fixture leva cabecalho SPDX (linhas iniciadas por #, L-08); so' o resto entra na comparacao
  fx="$tmp/fixture"
  mkdir -p "$fx"
  grep -v '^#' "$aqui/fixtures/fase/esperado.txt" >"$fx/esperado.txt"
  grep -v '^#' "$aqui/fixtures/fase/esperado_ausente.txt" >"$fx/esperado_ausente.txt"
  falha() { echo "fase.sh --selftest: FALHOU - $1" >&2; ok=0; }

  # 1. formato byte a byte contra a fixture unica, na saida padrao E no arquivo lateral
  (
    export GLINTFX_FASES_DIR="$tmp/fases" GLINTFX_FASES_TESTE=demo CMAKE_BUILD_PARALLEL_LEVEL=4
    fase_registrar configure 1234; fase_registrar build 3005; fase_registrar resto 110; fase_paralelismo
  ) >"$tmp/stdout.txt"
  cmp -s "$tmp/stdout.txt" "$fx/esperado.txt" || falha "a saida padrao nao bate byte a byte com esperado.txt: $(cat "$tmp/stdout.txt")"
  cmp -s "$tmp/fases/demo.txt" "$fx/esperado.txt" || falha "o arquivo lateral nao bate byte a byte com esperado.txt"

  # 1b. o filtro de `#` vale SO' para a FIXTURE: fixture com e sem linhas `#` (no topo, no meio e no fim)
  #     da a mesma comparacao; uma linha `#` no MEIO da saida REAL nao e' descartada (divergencia aparece)
  { echo "# a"; sed -n 1,2p "$fx/esperado.txt"; echo "# b"; sed -n '3,$p' "$fx/esperado.txt"; echo "# c"; } >"$tmp/com_hash.txt"
  cmp -s <(grep -v '^#' "$tmp/com_hash.txt") "$fx/esperado.txt" || falha "a fixture com linhas # deveria dar o mesmo conteudo que sem elas"
  { sed -n 1,2p "$fx/esperado.txt"; echo "# intruso"; sed -n '3,$p' "$fx/esperado.txt"; } >"$tmp/real_intruso.txt"
  ! cmp -s "$tmp/real_intruso.txt" "$fx/esperado.txt" || falha "uma linha # no MEIO da saida real foi descartada (o filtro so' pode valer para a fixture)"

  # 2. CMAKE_BUILD_PARALLEL_LEVEL ausente vira a palavra `ausente`
  (
    export GLINTFX_FASES_DIR="$tmp/fases" GLINTFX_FASES_TESTE=ausente
    unset CMAKE_BUILD_PARALLEL_LEVEL
    fase_paralelismo
  ) >"$tmp/stdout2.txt"
  cmp -s "$tmp/fases/ausente.txt" "$fx/esperado_ausente.txt" || falha "sem CMAKE_BUILD_PARALLEL_LEVEL o arquivo lateral deveria trazer 'ausente'"

  # 3. sem GLINTFX_FASES_DIR/TESTE: so' a saida padrao, nenhum arquivo
  (
    unset GLINTFX_FASES_DIR GLINTFX_FASES_TESTE; export CMAKE_BUILD_PARALLEL_LEVEL=2
    cd "$tmp" && fase_registrar x 5
  ) >"$tmp/stdout3.txt"
  [ "$(cat "$tmp/stdout3.txt")" = "FASE x: 0.05 s" ] || falha "saida padrao sem lateral: $(cat "$tmp/stdout3.txt")"
  [ -z "$(ls -A "$tmp" | grep -v -e '^stdout' -e '^fases$' -e '^fixture$' -e '^com_hash' -e '^real_intruso')" ] || falha "sem as variaveis, algo foi gravado: $(ls -A "$tmp")"

  # 4. o relogio: ~0,25 s medido por fase_inicio/fase_fim fica entre 0,20 s e 2,00 s
  saida="$(fase_inicio relogio; sleep 0.25; fase_fim relogio)"
  cs="$(printf '%s' "$saida" | sed -E 's/^FASE relogio: ([0-9]+)\.([0-9]{2}) s$/\1\2/')"
  if printf '%s' "$cs" | grep -qE '^[0-9]+$'; then
    [ "$((10#$cs))" -ge 20 ] && [ "$((10#$cs))" -le 200 ] || falha "0,25 s medido como '$saida' (fora de 0,20 a 2,00 s)"
  else
    falha "saida do relogio fora do formato: '$saida'"
  fi

  # 5. fase_fim sem fase_inicio reprova (rc 2)
  (fase_fim inexistente) >/dev/null 2>&1; rc=$?
  [ "$rc" -eq 2 ] || falha "fase_fim sem fase_inicio deveria sair 2, obteve $rc"

  # 5b. FALHA FECHADA (I-1): lateral nao gravavel => rc 3 com a mensagem nomeando o arquivo; fim sem inicio
  #     => rc 2 e a linha SEGUINTE nao executa. (Um ARQUIVO como pai do diretorio: nao grava em nenhum SO.)
  : >"$tmp/arquivo_pai"
  saida="$( (export GLINTFX_FASES_DIR="$tmp/arquivo_pai/sub" GLINTFX_FASES_TESTE=nao; fase_registrar x 5; echo SEGUINTE) 2>&1 )"; rc=$?
  [ "$rc" -eq 3 ] || falha "lateral nao gravavel deveria sair 3, obteve $rc"
  printf '%s' "$saida" | grep -q "nao consegui gravar o arquivo lateral $tmp/arquivo_pai/sub/nao.txt" || falha "a mensagem nao nomeia o arquivo lateral: $saida"
  printf '%s' "$saida" | grep -q SEGUINTE && falha "a linha seguinte executou depois do erro de gravacao"
  saida="$( (fase_fim inexistente; echo SEGUINTE) 2>&1 )"; rc=$?
  [ "$rc" -eq 2 ] || falha "fim sem inicio deveria sair 2, obteve $rc"
  printf '%s' "$saida" | grep -q "fase_fim 'inexistente' sem fase_inicio" || falha "a mensagem nao nomeia a fase: $saida"
  printf '%s' "$saida" | grep -q SEGUINTE && falha "a linha seguinte executou depois do fim sem inicio"

  rm -rf -- "$tmp"
  if [ "$ok" -eq 1 ]; then
    echo "fase.sh --selftest: OK - formato byte a byte (saida e lateral), ausente, sem lateral, relogio, fim sem inicio"
    return 0
  fi
  return 1
}

if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  if [ "${1:-}" = "--selftest" ]; then
    _fase_selftest
    exit $?
  fi
  echo "Uso: source $0 | $0 --selftest" >&2
  exit 2
fi
