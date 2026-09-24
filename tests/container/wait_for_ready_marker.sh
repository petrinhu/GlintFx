#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# wait_for_ready_marker.sh - WL-ACK-SMOKE-BLUNT A3e, passo 2 (achado
# do team-lead, 24/09/2026, GODS_LAWS.md L-09 rule 5/L-17). Ate aqui,
# quem sobe o container (.github/workflows/ci.yml, "Sobe o compositor
# limpo") esperava por uma sonda EXTERNA independente (loop de `docker
# exec ... wayland-info`), SEM nenhuma sincronia com a verificacao
# INTERNA do proprio entrypoint (run_compositor.sh's own wait_for_
# relay_ready()). As duas sao processos separados, com orcamentos de
# tempo proprios e sem se falar - achado real, medido ao vivo: o
# container podia ficar "pronto" pra sonda externa (o rele ja estava
# aceitando conexoes) e servir varias rodadas de teste com sucesso,
# enquanto a verificacao INTERNA ainda estava tentando por baixo do
# tapete, e eventualmente falhava e matava o PID1 - derrubando o
# container NO MEIO de um teste que ja estava passando havia segundos.
#
# Este script substitui a sonda externa: espera um ARQUIVO MARCADOR
# (RUNTIME_DIR/ready) que run_compositor.sh's own main() so publica
# DEPOIS que wait_for_relay_ready() ja passou de verdade - a mesma
# fonte de verdade que o entrypoint usa para decidir "estou pronto",
# em vez de uma sonda que so testa "consigo falar com o soquete agora".
#
# Usage: wait_for_ready_marker.sh <container-name>
#        wait_for_ready_marker.sh --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly RUNTIME_DIR="/run/glintfx-test"
readonly MARKER_PATH="${RUNTIME_DIR}/ready"

fail() {
    echo "wait_for_ready_marker.sh: $1" >&2
    exit 1
}

require_container_arg() {
    [ "$#" -eq 1 ] || fail "usage: wait_for_ready_marker.sh <container-name>"
}

# GLINTFX_READY_MARKER_WAIT_TRIES/_SLEEP: only --selftest below
# overrides these (to run its RED/GREEN cases in well under a second
# instead of the real budget) - production callers never set them.
: "${GLINTFX_READY_MARKER_WAIT_TRIES:=40}"
: "${GLINTFX_READY_MARKER_WAIT_SLEEP:=1}"

# The only point that actually talks to the real container - --selftest
# redefines this DENTRO da propria subshell, mesmo idioma que
# compositor_probe() em run_compositor.sh ja usa.
marker_probe() {
    container="$1"
    docker exec "$container" test -f "$MARKER_PATH"
}

wait_for_ready_marker() {
    container="$1"
    tries=0
    while [ "$tries" -lt "$GLINTFX_READY_MARKER_WAIT_TRIES" ]; do
        marker_probe "$container" >/dev/null 2>&1 && return 0
        tries=$((tries + 1))
        sleep "$GLINTFX_READY_MARKER_WAIT_SLEEP"
    done
    fail "marcador de prontidao ($MARKER_PATH) nao apareceu dentro do container '$container' em ${GLINTFX_READY_MARKER_WAIT_TRIES}x${GLINTFX_READY_MARKER_WAIT_SLEEP}s - GODS_LAWS.md L-40: sinal de que o proprio entrypoint (run_compositor.sh) ainda nao terminou wait_for_relay_ready(), ou nunca vai terminar"
}

# --- --selftest: prova o laco acima SEM Docker nenhum (GODS_LAWS.md
# L-35/L-36 - vermelho antes de verde), mesmo idioma que run_
# compositor.sh's own --selftest ja usa para compositor_probe().

# CASO VERMELHO: o marcador nunca aparece - tem que REPROVAR dentro do
# orcamento, nunca declarar pronto. fail() sai com `exit`, nao
# `return` - por isso este caso nao usa `if` (mesmo idioma ja
# documentado em run_compositor.sh's own selftest_case_never_ready).
selftest_case_marker_never_appears() (
    marker_probe() { return 1; }
    GLINTFX_READY_MARKER_WAIT_TRIES=3
    GLINTFX_READY_MARKER_WAIT_SLEEP=0
    wait_for_ready_marker "selftest-container" >/dev/null 2>&1
    exit 42
)

# CONTROLE POSITIVO: o marcador so aparece apos um atraso - o laco tem
# que ESPERAR por ele (nunca declarar pronto antes da 3a tentativa) e
# so entao suceder.
selftest_case_marker_appears_after_delay() (
    tries_seen=0
    marker_probe() {
        tries_seen=$((tries_seen + 1))
        [ "$tries_seen" -ge 3 ]
    }
    GLINTFX_READY_MARKER_WAIT_TRIES=10
    GLINTFX_READY_MARKER_WAIT_SLEEP=0
    if ! wait_for_ready_marker "selftest-container" >/dev/null 2>&1; then
        echo "SELFTEST FALHOU: marcador que aparece apos atraso nunca foi esperado" >&2
        exit 1
    fi
    if [ "$tries_seen" -lt 3 ]; then
        echo "SELFTEST FALHOU: declarou pronto antes da 3a tentativa (tries=$tries_seen)" >&2
        exit 1
    fi
    echo "selftest: marcador com atraso foi esperado corretamente (tries=$tries_seen) - OK"
)

run_selftest() {
    ok=0

    rc=0
    selftest_case_marker_never_appears || rc="$?"
    if [ "$rc" -eq 42 ]; then
        echo "SELFTEST FALHOU: marcador que nunca aparece foi declarado pronto" >&2
        ok=1
    else
        echo "selftest: marcador que nunca aparece reprovou o laco (codigo=$rc), como esperado - OK"
    fi

    selftest_case_marker_appears_after_delay || ok=1

    [ "$ok" -eq 0 ] || fail "selftest reprovou (ver mensagens acima)"
    echo "wait_for_ready_marker.sh --selftest: todos os casos passaram"
}

main() {
    if [ "$#" -eq 1 ] && [ "$1" = "--selftest" ]; then
        run_selftest
        exit 0
    fi
    require_container_arg "$@"
    wait_for_ready_marker "$1"
    echo "wait_for_ready_marker.sh: marcador de prontidao encontrado em '$1'"
}

main "$@"
