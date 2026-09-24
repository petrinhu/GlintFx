#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_compositor.sh - starts an isolated kwin_wayland compositor. Runs
# ONLY inside the container (GODS_LAWS.md L-09, TEST-WLCONT): it
# creates its OWN XDG_RUNTIME_DIR from scratch and never reads, nor
# expects, anything mounted in from the host. The container that ran
# this script is the whole isolation boundary; there is no host
# fallback path here to accidentally take.
#
# Usage: run_compositor.sh <socket-name>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly RUNTIME_DIR="/run/glintfx-test"

fail() {
    echo "run_compositor.sh: $1" >&2
    exit 1
}

require_socket_name_arg() {
    [ "$#" -eq 1 ] || fail "usage: run_compositor.sh <socket-name>"
}

# chmod 700 on a directory created fresh by this same process, inside
# this same container, is the private-runtime-dir half of the L-09
# isolation proof: nothing from a host XDG_RUNTIME_DIR is ever read
# here, so there is nothing to leak.
create_private_runtime_dir() {
    mkdir -p "$RUNTIME_DIR"
    chmod 700 "$RUNTIME_DIR"
}

export_runtime_env() {
    export XDG_RUNTIME_DIR="$RUNTIME_DIR"
    # GODS_LAWS.md L-05: Linux is Wayland-only in this project. This
    # session type is what this container's own compositor speaks, not
    # a request to fall back to X11.
    export XDG_SESSION_TYPE="wayland"
}

# Backgrounded, NOT `exec`'d into: this used to be
# `exec dbus-run-session -- kwin_wayland ...`, which makes dbus-run-
# session PID 1 of the container. dbus-run-session's own job is to run
# ONE command and exit once that command exits - so the moment the
# compositor died (WL-DISPLAY fatia C's fatal_error_smoke.cpp kills it
# on purpose, from INSIDE this same container, to prove the adapter
# survives a dead connection), PID 1 considered its job done and
# exited too, taking the whole container down with it before the test
# could even run its second half. Measured live, reproduced twice
# (adversarial review, GODS_LAWS.md L-36): `docker inspect` showed
# exit=137 and zero test output. Backgrounding it here decouples the
# compositor's lifetime from PID 1's.
start_compositor() {
    socket_name="$1"
    dbus-run-session -- kwin_wayland --virtual --socket "$socket_name" &
}

# GLINTFX_COMPOSITOR_WAIT_TRIES/_SLEEP: only --selftest below overrides
# these (to run its RED/GREEN cases in well under a second instead of
# the real 30x1s budget) - production callers never set them, so the
# defaults always apply outside --selftest.
: "${GLINTFX_COMPOSITOR_WAIT_TRIES:=30}"
: "${GLINTFX_COMPOSITOR_WAIT_SLEEP:=1}"

# CONT-WARMUP C-1 (TODO.md, docs/plano-fecho-w7b.md D-10, GODS_LAWS.md
# L-40/L-51): only --selftest below overrides this (to a value small
# enough that its own fake-hang case returns in well under a second
# instead of really sleeping) - production callers never set it.
: "${GLINTFX_COMPOSITOR_PROBE_TIMEOUT:=5}"

# CONT-WARMUP (TODO.md, GODS_LAWS.md L-40/L-51): a REAL Wayland
# roundtrip against the socket - wayland-info is already installed in
# this image (dnf install list above) and tests/container/smoke.sh
# already calls it the exact same way against the exact same socket,
# so nothing new was added here to prove this. A compositor process
# that exists but has not bound/accepted on the socket yet fails this
# probe exactly like a compositor that never started at all - there is
# no third state this can mistake for "ready".
#
# CONT-WARMUP C-1 conserto (docs/plano-fecho-w7b.md D-10): `timeout`
# added around the `wayland-info` call itself - MEASURED (this fatia's
# own report), a socket that accepts the TCP-equivalent connection and
# then never answers hangs THIS SINGLE CALL forever, and
# wait_for_compositor_ready()'s own bounded `tries` loop below never
# gets a chance to apply its own budget, because it never regains
# control from a probe call that itself never returns - GLINTFX_
# COMPOSITOR_WAIT_TRIES stops meaning anything the moment a single
# probe hangs. Wrapping EACH CALL is what turns the loop's total
# budget into a genuine TIME bound (tries * (probe_timeout + sleep)),
# not just a call-count bound that assumes every call returns quickly.
compositor_probe() {
    socket_name="$1"
    WAYLAND_DISPLAY="$socket_name" timeout "$GLINTFX_COMPOSITOR_PROBE_TIMEOUT" wayland-info \
        >/dev/null 2>&1
}

# CONT-WARMUP conserto: o antigo `pgrep -x kwin_wayland` provava que um
# PROCESSO com esse nome existe, nunca que ele ja ACEITA conexao -
# medido pelo orquestrador em 06/09/2026 (a primeira conexao real
# falhou com erro de plataforma; as seguintes passaram segundos
# depois, ja com o processo ha muito visivel ao pgrep). O que segurava
# ate aqui era a ORDEM acidental dos passos do job `wayland-container`
# (o passo de fumaca roda primeiro e absorve a janela de arranque) -
# garantia nao declarada, nao deste laco. Agora o laco so retorna
# quando compositor_probe() PROVA uma conexao real; enquanto isso, o
# script continua falhando LOUDLY (GODS_LAWS.md L-40) se o orcamento
# de tentativas esgotar, o mesmo dever que o `pgrep`-loop antigo tinha
# - so que agora medindo a coisa certa.
#
# CONT-WARMUP C-1: o ORCAMENTO TOTAL agora e' de TEMPO, nao so' de
# CONTAGEM - GLINTFX_COMPOSITOR_WAIT_TRIES sozinho nunca foi um limite
# de tempo (uma tentativa pendurada consumia tempo ilimitado sem gastar
# nenhuma "tentativa"); com compositor_probe() acima agora bounded por
# GLINTFX_COMPOSITOR_PROBE_TIMEOUT, o pior caso passa a ser genuinamente
# GLINTFX_COMPOSITOR_WAIT_TRIES * (GLINTFX_COMPOSITOR_PROBE_TIMEOUT +
# GLINTFX_COMPOSITOR_WAIT_SLEEP) segundos, nunca mais.
wait_for_compositor_ready() {
    socket_name="$1"
    tries=0
    while [ "$tries" -lt "$GLINTFX_COMPOSITOR_WAIT_TRIES" ]; do
        compositor_probe "$socket_name" && return 0
        tries=$((tries + 1))
        sleep "$GLINTFX_COMPOSITOR_WAIT_SLEEP"
    done
    fail "kwin_wayland nao aceitou conexao real em ${GLINTFX_COMPOSITOR_WAIT_TRIES}x${GLINTFX_COMPOSITOR_WAIT_SLEEP}s"
}

# --- --selftest: prova o laco acima SEM Docker/kwin_wayland nenhum
# (GODS_LAWS.md L-35/L-36 - vermelho antes de verde) - cada caso
# redefine compositor_probe() DENTRO da propria subshell (a sintaxe
# `nome() ( ... )` abaixo roda o corpo inteiro em processo filho), o
# mesmo idioma de isolamento que tests/container/exec_fixture.sh's own
# --selftest ja usa: o `exit`/fail() de dentro do caso nunca derruba o
# processo pai que esta rodando os OUTROS casos.

# CASO VERMELHO: uma sonda que NUNCA aceita conexao (o analogo do que
# o antigo `pgrep` deixava passar - um processo existe, mas nada
# aceita no soquete) tem que REPROVAR o laco dentro do orcamento,
# nunca declarar pronto. fail() (chamada por wait_for_compositor_ready
# quando o orcamento esgota) sai com `exit`, nao `return` - um `if
# wait_for_compositor_ready ...; then` em volta dela nunca alcancaria
# nenhum dos dois ramos, porque o processo ja morreu dentro da
# condicao. Por isso este caso nao usa `if`: deixa fail() terminar a
# subshell (o caminho DESEJADO), e so' chega ao `exit 42` abaixo se
# wait_for_compositor_ready RETORNOU normalmente - ou seja, se
# declarou pronto por engano, apesar da sonda acima nunca aceitar.
# run_selftest (abaixo) distingue os dois pelo codigo de saida.
selftest_case_never_ready() (
    compositor_probe() { return 1; }
    GLINTFX_COMPOSITOR_WAIT_TRIES=3
    GLINTFX_COMPOSITOR_WAIT_SLEEP=0
    wait_for_compositor_ready "selftest-never" >/dev/null 2>&1
    exit 42
)

# CONTROLE POSITIVO: uma sonda que falha um numero fixo de vezes e so
# entao aceita - o laco tem que ESPERAR por ela (nunca declarar pronto
# antes da sonda aceitar de verdade) e so entao suceder.
selftest_case_ready_after_delay() (
    selftest_probe_tries=0
    compositor_probe() {
        selftest_probe_tries=$((selftest_probe_tries + 1))
        [ "$selftest_probe_tries" -ge 3 ]
    }
    GLINTFX_COMPOSITOR_WAIT_TRIES=10
    GLINTFX_COMPOSITOR_WAIT_SLEEP=0
    if ! wait_for_compositor_ready "selftest-delay" >/dev/null 2>&1; then
        echo "SELFTEST FALHOU: sonda que aceita apos atraso nunca foi declarada pronta" >&2
        exit 1
    fi
    if [ "$selftest_probe_tries" -lt 3 ]; then
        echo "SELFTEST FALHOU: declarou pronto antes da 3a tentativa da sonda (tries=$selftest_probe_tries)" >&2
        exit 1
    fi
    echo "selftest: laco esperou a sonda aceitar (tries=$selftest_probe_tries) e so entao declarou pronto - OK"
)

# CONT-WARMUP C-1, CASO VERMELHO contra o codigo de HOJE (docs/plano-
# fecho-w7b.md D-10, estreia vermelha medida por este fatia's own
# report): esta sonda NAO redefine compositor_probe() como os dois
# casos acima - ela poe um `wayland-info` FALSO na FRENTE do PATH (um
# script que so' dorme) e deixa a funcao REAL deste arquivo chama-lo,
# exatamente como um compositor que aceitou a conexao e nunca respondeu
# faria. Isso e' o que prova o `timeout` DENTRO de compositor_probe() -
# uma redefinicao da funcao inteira (como os dois casos acima) nunca
# exercitaria esse `timeout`, so' o caminho de producao real exercita.
# MEDIDO antes deste conserto (relatorio desta fatia): contra o codigo
# de `de72850` (sem o `timeout`), este EXATO cenario nunca retornou
# sozinho - um `timeout` EXTERNO de 8s (GODS_LAWS.md L-40/L-50: nunca
# se deixa um comando pendurar de verdade, nem em teste) teve que matar
# o processo (rc=124). Com o conserto, a funcao real volta sozinha,
# dentro do orcamento, sem precisar de nenhum `timeout` externo.
selftest_case_probe_hangs_but_returns() (
    fake_bin_dir="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-XXXXXX")" || exit 1
    trap 'rm -rf "$fake_bin_dir"' EXIT
    cat >"$fake_bin_dir/wayland-info" <<'FAKE_WAYLAND_INFO'
#!/usr/bin/env sh
# Simula um soquete que aceitou a conexao e nunca respondeu - dorme
# bem mais que qualquer orcamento razoavel deste selftest, entao
# retornar aqui SO' prova que algo de fora desta sonda a cortou.
sleep 300
FAKE_WAYLAND_INFO
    chmod +x "$fake_bin_dir/wayland-info"
    PATH="$fake_bin_dir:$PATH"
    export PATH

    GLINTFX_COMPOSITOR_PROBE_TIMEOUT=1
    GLINTFX_COMPOSITOR_WAIT_TRIES=2
    GLINTFX_COMPOSITOR_WAIT_SLEEP=0
    wait_for_compositor_ready "selftest-hang" >/dev/null 2>&1
    # So' chega aqui (saindo com 99) se wait_for_compositor_ready
    # RETORNASSE em vez de fail() terminar a subshell primeiro - o
    # mesmo idioma de selftest_case_never_ready acima, pela mesma razao
    # (fail() usa `exit`, nunca `return`). A sonda falsa acima nunca
    # aceita de verdade, entao isto NUNCA deveria acontecer.
    exit 99
)

run_selftest() {
    ok=0

    rc=0
    selftest_case_never_ready || rc="$?"
    if [ "$rc" -eq 42 ]; then
        echo "SELFTEST FALHOU: sonda que nunca aceita conexao declarou o compositor pronto" >&2
        ok=1
    else
        echo "selftest: sonda que nunca aceita conexao reprovou o laco (codigo=$rc), como esperado - OK"
    fi

    selftest_case_ready_after_delay || ok=1

    started="$(date +%s)"
    rc=0
    selftest_case_probe_hangs_but_returns || rc="$?"
    elapsed=$(($(date +%s) - started))
    if [ "$rc" -eq 99 ]; then
        echo "SELFTEST FALHOU: sonda pendurada (fake wayland-info) declarou o compositor pronto" >&2
        ok=1
    elif [ "$elapsed" -gt 10 ]; then
        # Orcamento nominal: 2 tentativas * 1s de GLINTFX_COMPOSITOR_
        # PROBE_TIMEOUT = 2s; 10s de folga generosa contra jitter de
        # agendador, ainda MUITO abaixo dos 300s que a sonda pendurada
        # dormiria se o `timeout` de dentro de compositor_probe() nao a
        # cortasse (o cenario MEDIDO contra o codigo de hoje, ver o
        # comentario do caso acima).
        echo "SELFTEST FALHOU: laco levou ${elapsed}s com sonda pendurada (fake wayland-info) - GLINTFX_COMPOSITOR_PROBE_TIMEOUT nao esta cortando (codigo=$rc)" >&2
        ok=1
    else
        echo "selftest: sonda pendurada (fake wayland-info) cortada por GLINTFX_COMPOSITOR_PROBE_TIMEOUT, laco terminou em ${elapsed}s (codigo=$rc), dentro do orcamento - OK"
    fi

    [ "$ok" -eq 0 ] || fail "selftest reprovou (ver mensagens acima)"
    echo "run_compositor.sh --selftest: todos os casos passaram"
}

# PID 1 stays up on its own from here on, independent of whatever
# happens to the compositor backgrounded above - this is the whole
# point of the fix: fatal_error_smoke.cpp (or anything else) can kill
# the compositor process and this container keeps running, instead of
# tearing itself down with it.
stay_up_forever() {
    exec tail -f /dev/null
}

main() {
    if [ "$#" -eq 1 ] && [ "$1" = "--selftest" ]; then
        run_selftest
        exit 0
    fi
    require_socket_name_arg "$@"
    create_private_runtime_dir
    export_runtime_env
    start_compositor "$1"
    wait_for_compositor_ready "$1"
    stay_up_forever
}

main "$@"
