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

# _fase_show <rotulo> <arquivo>: imprime o conteudo com os fins de linha visiveis (`cat -A`), ou o estado.
_fase_show() {
  echo "--- $1 ($2):"
  if [ -f "$2" ] && [ -r "$2" ]; then cat -A "$2"; else echo "(ausente, ilegivel ou nao e' arquivo)"; fi
}

# _fase_same_bytes <obtido> <esperado>: 0 = iguais byte a byte; 1 = diferentes; 2 = algum ausente, ilegivel ou
# DIRETORIO. Em toda falha imprime o esperado e o obtido em stderr (a licao do test_cmp do Git). Em bash puro,
# sem diffutils (as imagens Fedora e Arch do CI nao trazem). Contrato de cada guarda: sem o `[ -r ]`, dois
# ausentes dariam "iguais" (os dois `read` falham e deixam as variaveis vazias); sem o `[ -d ]`, um diretorio
# contra um arquivo vazio dava "iguais". Aceita `<(...)`, que o `cmake -E compare_files` nao aceita. Limite:
# `read -d ''` para no primeiro byte NUL; serve para texto (fixtures e saidas de FASE), nao para binario.
_fase_same_bytes() {
  local a="" b=""
  if [ -d "$1" ] || [ -d "$2" ] || [ ! -r "$1" ] || [ ! -r "$2" ]; then
    echo "fase.sh: nao comparei '$1' com '$2': algum e' ausente, ilegivel ou diretorio" >&2
    { _fase_show esperado "$2"; _fase_show obtido "$1"; } >&2
    return 2
  fi
  IFS= read -r -d '' a <"$1" || :   # `read -d ''` devolve 1 no EOF; sob `set -e` isso abortaria o chamador
  IFS= read -r -d '' b <"$2" || :
  [ "$a" = "$b" ] && return 0
  echo "fase.sh: '$1' difere de '$2'" >&2
  { _fase_show esperado "$2"; _fase_show obtido "$1"; } >&2
  return 1
}

# _fase_differs <a> <b>: 0 = os dois sao legiveis e DIFEREM (rc 1 do comparador, exato); 1 ou 2 = nao. O `-eq 1`
# e' de proposito: um `-ne 0` deixaria um arquivo apagado (rc 2) passar como "diferente" numa comparacao negada.
_fase_differs() {
  _fase_same_bytes "$1" "$2" 2>/dev/null
  [ "$?" -eq 1 ]
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
  _fase_same_bytes "$tmp/stdout.txt" "$fx/esperado.txt" || falha "a saida padrao nao bate byte a byte com esperado.txt: $(cat "$tmp/stdout.txt")"
  _fase_same_bytes "$tmp/fases/demo.txt" "$fx/esperado.txt" || falha "o arquivo lateral nao bate byte a byte com esperado.txt"

  # 1b. o filtro de `#` vale SO' para a FIXTURE: fixture com e sem linhas `#` (no topo, no meio e no fim)
  #     da a mesma comparacao; uma linha `#` no MEIO da saida REAL nao e' descartada (divergencia aparece)
  { echo "# a"; sed -n 1,2p "$fx/esperado.txt"; echo "# b"; sed -n '3,$p' "$fx/esperado.txt"; echo "# c"; } >"$tmp/com_hash.txt"
  _fase_same_bytes <(grep -v '^#' "$tmp/com_hash.txt") "$fx/esperado.txt" || falha "a fixture com linhas # deveria dar o mesmo conteudo que sem elas"
  { sed -n 1,2p "$fx/esperado.txt"; echo "# intruso"; sed -n '3,$p' "$fx/esperado.txt"; } >"$tmp/real_intruso.txt"
  _fase_differs "$tmp/real_intruso.txt" "$fx/esperado.txt" || falha "uma linha # no MEIO da saida real foi descartada (o filtro so' pode valer para a fixture)"

  # 1c. controles do comparador (rc 0 = iguais; 1 = diferentes; 2 = ausente, ilegivel ou diretorio)
  printf 'x\n' >"$tmp/c_a"; printf 'x\n' >"$tmp/c_b"; printf 'x' >"$tmp/c_semquebra"; printf 'y\n' >"$tmp/c_y"
  printf '*\n' >"$tmp/c_glob"; : >"$tmp/c_vazio"; mkdir "$tmp/c_dir" "$tmp/c_dir2"
  ctl() {  # ctl <rc esperado> <descricao> <obtido> <esperado>
    _fase_same_bytes "$3" "$4" 2>/dev/null; rc=$?
    [ "$rc" -eq "$1" ] || falha "comparador: $2 deveria dar $1, deu $rc"
  }
  ctl 0 "arquivos iguais" "$tmp/c_a" "$tmp/c_b"
  ctl 1 "diferenca so' na quebra final" "$tmp/c_a" "$tmp/c_semquebra"
  ctl 1 "1 byte diferente" "$tmp/c_a" "$tmp/c_y"
  ctl 2 "um arquivo ausente" "$tmp/c_a" "$tmp/c_ausente"
  ctl 2 "DOIS arquivos ausentes (nunca 0)" "$tmp/c_ausente" "$tmp/c_ausente2"
  ctl 2 "diretorio contra arquivo vazio (dava 'iguais')" "$tmp/c_dir" "$tmp/c_vazio"
  ctl 2 "arquivo vazio contra diretorio" "$tmp/c_vazio" "$tmp/c_dir"
  ctl 2 "dois diretorios" "$tmp/c_dir" "$tmp/c_dir2"
  ctl 1 "'x' contra '*' (glob nao casa; mata [[ \$a == \$b ]] sem aspas)" "$tmp/c_a" "$tmp/c_glob"
  ctl 0 "'*' contra '*'" "$tmp/c_glob" "$tmp/c_glob"
  ctl 0 "substituicao de processo igual" <(printf 'x\n') "$tmp/c_a"
  # como root, chmod 000 nao impede a leitura (os containers do CI rodam como root): os 2 controles so' valem
  # sem root, e a contagem dos PULADOS e' impressa SEMPRE (0 fora de root), exigida pelo ctest
  local skipped=0
  if [ "${EUID:-1}" -ne 0 ]; then
    : >"$tmp/c_000"; chmod 000 "$tmp/c_000"
    ctl 2 "arquivo ilegivel (chmod 000)" "$tmp/c_000" "$tmp/c_vazio"
    ctl 2 "DOIS arquivos ilegiveis (chmod 000)" "$tmp/c_000" "$tmp/c_000"
  else
    skipped=2
  fi
  # sob `set -e`, arquivos iguais NAO abortam o chamador
  saida="$( (set -e; _fase_same_bytes "$tmp/c_a" "$tmp/c_b"; echo VIVO) 2>&1 )"
  [ "$saida" = "VIVO" ] || falha "comparador sob set -e abortou com arquivos iguais: '$saida'"
  # diagnostico esperado x obtido em TODA falha: rc 1 (diferentes) e rc 2 (ausente). A ORIENTACAO conta: o que
  # vem depois de "--- esperado" e' o conteudo do esperado, depois de "--- obtido" o do obtido, com o `$` do
  # `cat -A` (mata rotulos trocados e `cat -A` trocado por `cat`)
  printf 'obtido-unico\n' >"$tmp/c_obt"; printf 'esperado-unico\n' >"$tmp/c_esp"
  saida="$(_fase_same_bytes "$tmp/c_obt" "$tmp/c_esp" 2>&1)"
  local esp_seg obt_seg
  esp_seg="${saida#*--- esperado}"; esp_seg="${esp_seg%%--- obtido*}"; obt_seg="${saida#*--- obtido}"
  case "$esp_seg" in *'esperado-unico$'*) ;; *) falha "rc 1: depois de '--- esperado' falta 'esperado-unico\$': '$saida'" ;; esac
  case "$esp_seg" in *obtido-unico*) falha "rc 1: o conteudo do obtido apareceu sob '--- esperado' (rotulos trocados): '$saida'" ;; esac
  case "$obt_seg" in *'obtido-unico$'*) ;; *) falha "rc 1: depois de '--- obtido' falta 'obtido-unico\$': '$saida'" ;; esac
  saida="$(_fase_same_bytes "$tmp/c_obt" "$tmp/c_ausente" 2>&1)"
  case "$saida" in *obtido-unico*) ;; *) falha "falha rc 2 sem o obtido na mensagem: '$saida'" ;; esac
  # a comparacao negada (linha 85): so' rc 1 exato conta como "diferem"; um arquivo APAGADO tem de reprovar
  _fase_differs "$tmp/c_a" "$tmp/c_y" || falha "_fase_differs: arquivos diferentes deveriam dar 0"
  _fase_differs "$tmp/c_a" "$tmp/c_b" && falha "_fase_differs: arquivos iguais deveriam dar nao-zero"
  _fase_differs "$tmp/c_ausente" "$tmp/c_a" && falha "_fase_differs: arquivo APAGADO passou como diferente (o real_intruso.txt apagado nao pode passar)"
  _fase_differs "$tmp/c_dir" "$tmp/c_a" && falha "_fase_differs: diretorio passou como diferente"

  # 2. CMAKE_BUILD_PARALLEL_LEVEL ausente vira a palavra `ausente`
  (
    export GLINTFX_FASES_DIR="$tmp/fases" GLINTFX_FASES_TESTE=ausente
    unset CMAKE_BUILD_PARALLEL_LEVEL
    fase_paralelismo
  ) >"$tmp/stdout2.txt"
  _fase_same_bytes "$tmp/fases/ausente.txt" "$fx/esperado_ausente.txt" || falha "sem CMAKE_BUILD_PARALLEL_LEVEL o arquivo lateral deveria trazer 'ausente'"

  # 3. sem GLINTFX_FASES_DIR/TESTE: so' a saida padrao, nenhum arquivo
  (
    unset GLINTFX_FASES_DIR GLINTFX_FASES_TESTE; export CMAKE_BUILD_PARALLEL_LEVEL=2
    cd "$tmp" && fase_registrar x 5
  ) >"$tmp/stdout3.txt"
  [ "$(cat "$tmp/stdout3.txt")" = "FASE x: 0.05 s" ] || falha "saida padrao sem lateral: $(cat "$tmp/stdout3.txt")"
  [ -z "$(ls -A "$tmp" | grep -v -e '^stdout' -e '^fases$' -e '^fixture$' -e '^com_hash' -e '^real_intruso' -e '^c_')" ] || falha "sem as variaveis, algo foi gravado: $(ls -A "$tmp")"

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
    echo "fase.sh --selftest: OK - formato byte a byte (saida e lateral), ausente, sem lateral, relogio, fim sem inicio; pulados: $skipped (root: chmod 000 nao impede leitura)"
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
