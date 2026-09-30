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

# WL-ACK-SMOKE-BLUNT A3e, passo 2c (achado do team-lead, 24/09/2026,
# GODS_LAWS.md L-17 "gemeo" - familia "copia em vez de fonte",
# memoria feedback_copia_em_vez_de_fonte.md): o caminho do marcador de
# prontidao existe em DUAS copias - o RUNTIME_DIR/nome de arquivo daqui
# (quem ESCREVE o marcador, de dentro do container) e o RUNTIME_DIR/
# MARKER_FILENAME de tests/container/wait_for_ready_marker.sh (quem
# ESPERA o marcador, de fora, via `docker exec`). Sao dois PROCESSOS
# SEPARADOS - um dentro do container, outro no host - entao nao ha como
# compartilhar uma variavel de ambiente entre eles; se um mudasse sem o
# outro, nenhum teste local reprovava, e o sintoma so apareceria no CI
# como um timeout de 40s esperando um arquivo que nunca aparece.
#
# Os dois literais abaixo (o valor default de RUNTIME_DIR e o nome do
# arquivo do marcador) sao comparados, TOKEN A TOKEN, contra o par
# marcado com o MESMO id em wait_for_ready_marker.sh, pelo gate ja
# existente tests/tools/check_sibling_lists.py (item GATE-SIBLING-LIST -
# reaproveitado aqui, nao reinventado: o mesmo mecanismo que ja prova
# tests/tools/check_port_privacy.sh contra tools/ci/check-port-privacy-
# win.ps1). Uma divergencia num dos dois lados agora reprova o
# `sibling_lists_test` do CI/preci, em vez de so aparecer 40s depois
# num container que nunca fica pronto.
# GLINTFX-SIBLING-LIST:ready-marker-path:START
readonly GLINTFX_READY_MARKER_RUNTIME_DIR_DEFAULT="/run/glintfx-test"
readonly GLINTFX_READY_MARKER_FILENAME="ready"
# GLINTFX-SIBLING-LIST:ready-marker-path:END

# WL-ACK-SMOKE-BLUNT A3e, passo 2b (achado do main, GODS_LAWS.md L-12):
# deixou de ser `readonly` para o --selftest poder provar a ORDEM real
# de bring_up() (abaixo) - um caso de selftest aponta RUNTIME_DIR para
# um diretorio temporario proprio ANTES de chamar bring_up(), o mesmo
# idioma `: "${VAR:=default}"` ja usado por GLINTFX_COMPOSITOR_WAIT_
# TRIES/_SLEEP e pelos demais GLINTFX_* deste arquivo. Producao nunca
# define RUNTIME_DIR no ambiente, entao o default abaixo sempre vale
# fora do --selftest - nenhum comportamento de producao muda.
: "${RUNTIME_DIR:=$GLINTFX_READY_MARKER_RUNTIME_DIR_DEFAULT}"

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

# WL-ACK-SMOKE-BLUNT A3e, passo 2 (achado do team-lead, 24/09/2026):
# publicado por bring_up() SO DEPOIS que wait_for_relay_ready() ja
# passou de verdade - a fonte de verdade que tests/container/wait_for_
# ready_marker.sh (chamado por quem sobe este container, ex.: ci.yml)
# passa a esperar, no lugar de uma sonda externa independente sem
# sincronia nenhuma com esta verificacao interna. O caminho e' sempre
# "${RUNTIME_DIR}/${GLINTFX_READY_MARKER_FILENAME}", calculado NA
# CHAMADA (dentro de bring_up(), abaixo) em vez de guardado num
# `readonly` proprio - RUNTIME_DIR deixou de ser fixo (ver comentario
# dela acima) exatamente para o --selftest poder trocar o valor antes
# de chamar bring_up(). Os dois pedacos do caminho (RUNTIME_DIR default
# e o nome do arquivo) sao os literais marcados no passo 2c (comentario
# deles acima) - a mesma fonte que wait_for_ready_marker.sh compara.
publish_ready_marker() {
    marker_path="$1"
    : >"$marker_path"
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

# WL-ACK-SMOKE-BLUNT A3b (docs/plano-w7c-adendo-revalidacao.md SS3.A,
# N5/D-A3): "subida em dois tempos" - KWin comes up on an INTERNAL
# socket name (never exposed to fixtures) first, THEN the relay takes
# over the EXTERNAL name every fixture already connects to, itself a
# client of the internal one. Backgrounded, same reasoning as
# start_compositor() above (this process must not become the thing a
# dead relay takes the container down with).
start_relay() {
    internal_socket_name="$1"
    external_socket_name="$2"
    log_file="$3"
    wire_relay "$internal_socket_name" "$external_socket_name" >"$log_file" 2>&1 &
}

# The name run_compositor.sh's own two-stage startup uses for KWin -
# never handed to a fixture, only to start_relay()'s own upstream arg.
# A fixed suffix, not a random one: --selftest's own cases below (and
# a human debugging a live container) can name it without reading this
# script's PID first.
internal_socket_name() {
    external_socket_name="$1"
    printf '%s-upstream\n' "$external_socket_name"
}

# WL-ACK-SMOKE-BLUNT A3e (achado do team-lead, 24/09/2026, GODS_LAWS.md
# L-17 "gemeo"): ate aqui isto so olhava a ULTIMA linha do log
# (`tail -n 1`) - correto quando o rele so servia um cliente por vez
# (A3b), mas com o rele multi-cliente (A3c) outras linhas de PRODUCAO
# podem legitimamente ser a ultima (control truncated, falha ao
# conectar montante, perror de accept/poll - catalogadas no relatorio
# desta fatia, tests/container/wire_relay/wire_relay_connection_set.cpp),
# sem que isso signifique "zero mensagens decodificadas". Varre o log
# INTEIRO atras de QUALQUER linha "connection closed - N message(s)
# from client" e devolve a primeira com N>0 - ignora silenciosamente
# qualquer outra linha (de producao ou de outra conexao), exatamente
# como antes so devolvia vazio pra qualquer linha que nao casasse o
# formato exato. Empty output (nenhuma linha com N>0 ainda) continua
# sendo um "not ready yet" legitimo, nunca um erro por si so - o retry
# loop do chamador e' quem transforma vazio repetido em falha.
relay_client_message_count() {
    log_file="$1"
    grep 'connection closed' "$log_file" 2>/dev/null |
        sed -n 's/.* - \([0-9][0-9]*\) message(s) from client.*/\1/p' |
        awk '$1 > 0 { print; exit }'
}

# GLINTFX_RELAY_LOG_WAIT_TRIES/_SLEEP: only --selftest below overrides
# these, same shape as GLINTFX_COMPOSITOR_WAIT_TRIES/_SLEEP above.
: "${GLINTFX_RELAY_LOG_WAIT_TRIES:=10}"
: "${GLINTFX_RELAY_LOG_WAIT_SLEEP:=1}"

# The probe (wayland-info) and the relay are TWO SEPARATE PROCESSES:
# wayland-info can exit(0) - compositor_probe() already returned - a
# moment before the relay, on its own schedule, notices the client's
# EOF, finishes serve_one_client() and flushes the "connection closed"
# line this depends on. A single read right after the probe returns
# races that write; this project has already measured concurrent load
# turning a one-shot check into a false failure elsewhere (memoria
# feedback_carga_concorrente_falseia_suite.md) - the same shape of bug,
# caught here by reasoning before it reproduced live. A short retry
# turns the race into a genuine wait, the same idiom wait_for_
# compositor_ready() already uses for the probe itself.
wait_for_relay_log_count() {
    log_file="$1"
    tries=0
    while [ "$tries" -lt "$GLINTFX_RELAY_LOG_WAIT_TRIES" ]; do
        count="$(relay_client_message_count "$log_file")"
        if [ -n "$count" ] && [ "$count" -gt 0 ]; then
            printf '%s\n' "$count"
            return 0
        fi
        tries=$((tries + 1))
        sleep "$GLINTFX_RELAY_LOG_WAIT_SLEEP"
    done
    return 1
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

# WL-ACK-SMOKE-BLUNT A3b: the SECOND compositor_probe() the plan
# demands - "de novo, agora ATRAVES do rele". Reuses wait_for_
# compositor_ready() itself for the TCP-equivalent-connects half (a
# probe through the relay is still just wayland-info against a
# socket), then adds the part specific to going through a relay: the
# relay's OWN stdout has to show it actually decoded something from
# that probe - GODS_LAWS.md L-40, "zero reprova" (docs/plano-w7c.md
# SS3.A, A3b's own red control - a relay that accepts the connection
# but never really parses the wire is indistinguishable from no relay
# at all, unless something checks past the socket accept).
wait_for_relay_ready() {
    external_socket_name="$1"
    log_file="$2"
    wait_for_compositor_ready "$external_socket_name"
    if ! wait_for_relay_log_count "$log_file" >/dev/null; then
        fail "rele aceitou a conexao mas decodificou zero mensagens do cliente (log: $(tail -n 3 "$log_file" 2>/dev/null | tr '\n' ' '))"
    fi
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

# WL-ACK-SMOKE-BLUNT A3b, CASO VERMELHO 1: "rele que nunca aceita" -
# a extensao especifica de wait_for_relay_ready() (a segunda metade,
# sobre wait_for_compositor_ready() que os casos acima ja cobrem). O
# mesmo idioma de selftest_case_never_ready: fail() sai com `exit`, o
# `exit 42` abaixo so e alcancado se a funcao voltasse normalmente por
# engano.
selftest_case_relay_never_accepts() (
    compositor_probe() { return 1; }
    GLINTFX_COMPOSITOR_WAIT_TRIES=3
    GLINTFX_COMPOSITOR_WAIT_SLEEP=0
    fake_log="$(mktemp "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-relaylog-XXXXXX")" || exit 1
    trap 'rm -f "$fake_log"' EXIT
    wait_for_relay_ready "selftest-relay-never" "$fake_log" >/dev/null 2>&1
    exit 42
)

# CASO VERMELHO 2: "rele pronto com compositor morto" - o soquete
# aceita (a sonda do lado de fora nunca saberia a diferenca sozinha),
# mas o proprio log do rele mostra zero mensagens decodificadas da
# sonda - o sintoma real de um rele cujo lado de cima (o KWin interno)
# morreu ou nunca respondeu nada de util. wait_for_relay_ready() tem
# que reprovar por ISSO, nao pela conexao em si (que aqui sempre
# aceita).
selftest_case_relay_ready_but_zero_messages() (
    compositor_probe() { return 0; }
    # Orcamento pequeno so' para este caso rodar rapido: o log ja' tem
    # a linha de "0 message(s)" ANTES de wait_for_relay_ready() ser
    # chamada, entao nao ha corrida real a esperar aqui - so' se quer
    # que o retry de wait_for_relay_log_count() (que agora existe por
    # causa da corrida real entre o processo do rele e o processo da
    # sonda) nao segure o selftest pelos 10x1s de producao.
    GLINTFX_RELAY_LOG_WAIT_TRIES=2
    GLINTFX_RELAY_LOG_WAIT_SLEEP=0
    fake_log="$(mktemp "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-relaylog-XXXXXX")" || exit 1
    trap 'rm -f "$fake_log"' EXIT
    printf 'wire_relay: connection closed - 0 message(s) from client, 0 from upstream, 0 violation(s)\n' \
        >"$fake_log"
    wait_for_relay_ready "selftest-relay-zero" "$fake_log" >/dev/null 2>&1
    exit 42
)

# CONTROLE POSITIVO do proprio retry (o par de selftest_case_ready_
# after_delay acima, agora para wait_for_relay_log_count()): a linha
# "connection closed" so' aparece no log DEPOIS de um pequeno atraso -
# a mesma corrida real entre o processo do rele e o processo da sonda
# que motivou o retry existir (comentario de wait_for_relay_log_count()
# acima). wait_for_relay_ready() tem que ESPERAR a linha aparecer, nao
# reprovar so' porque a primeira leitura veio vazia.
selftest_case_relay_log_arrives_late() (
    compositor_probe() { return 0; }
    GLINTFX_RELAY_LOG_WAIT_TRIES=20
    GLINTFX_RELAY_LOG_WAIT_SLEEP=0.05
    fake_log="$(mktemp "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-relaylog-XXXXXX")" || exit 1
    trap 'rm -f "$fake_log"' EXIT
    (
        sleep 0.2
        printf 'wire_relay: connection closed - 3 message(s) from client, 3 from upstream, 0 violation(s)\n' \
            >"$fake_log"
    ) &
    if ! wait_for_relay_ready "selftest-relay-late" "$fake_log" >/dev/null 2>&1; then
        echo "SELFTEST FALHOU: linha do rele chegando atrasada devia ter sido esperada, nao reprovada direto" >&2
        exit 1
    fi
    echo "selftest: rele que escreve a linha do log com atraso foi esperado (nao reprovado direto) - OK"
)

# WL-ACK-SMOKE-BLUNT A3e (achado do team-lead, 24/09/2026): a sonda
# antiga so olhava `tail -n 1` - com o rele multi-cliente (A3c), OUTRAS
# linhas de producao (control truncated, falha ao conectar montante,
# perror de accept/poll) podem legitimamente ser a ULTIMA linha do log
# sem que isso signifique "zero mensagens decodificadas" (GODS_LAWS.md
# L-17 "gemeo": tests/container/wire_relay/wire_relay_connection_set.cpp
# tem cinco pontos assim, catalogados no relatorio desta fatia).
#
# CASO VERMELHO (positivo): a linha BOA ("connection closed - N
# message(s) from client", N>0) esta no log, mas NAO e' a ultima - uma
# linha de ruido de producao ("control truncated from client, closing")
# veio depois. Contra o codigo de HOJE (tail -n 1), isto reprova por
# engano; depois do conserto (varrer o log inteiro atras de QUALQUER
# linha "connection closed" com N>0), tem que passar.
selftest_case_relay_good_line_not_last() (
    compositor_probe() { return 0; }
    GLINTFX_RELAY_LOG_WAIT_TRIES=2
    GLINTFX_RELAY_LOG_WAIT_SLEEP=0
    fake_log="$(mktemp "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-relaylog-XXXXXX")" || exit 1
    trap 'rm -f "$fake_log"' EXIT
    {
        printf 'wire_relay: connection closed - 7 message(s) from client, 12 from upstream, 0 violation(s)\n'
        printf 'wire_relay: control truncated from client, closing\n'
    } >"$fake_log"
    if ! wait_for_relay_ready "selftest-relay-good-not-last" "$fake_log" >/dev/null 2>&1; then
        echo "SELFTEST FALHOU: linha boa presente (nao na ultima posicao) devia ter sido aceita, foi reprovada" >&2
        exit 1
    fi
    echo "selftest: linha boa nao-ultima aceita (varredura do log inteiro, nao so tail -n 1) - OK"
)

# WL-ACK-SMOKE-BLUNT A3e, passo 1b (achado do main em revisao
# adversarial, 24/09/2026 - GODS_LAWS.md L-12, sabotagem de FAMILIA
# DIFERENTE da do implementador): mutante M1 (`awk '$1 > 0'` vira
# `awk '$1 >= 0'`, aceitando zero) SOBREVIVE ao caso acima e ao
# controle negativo, porque nenhum dos dois tem uma linha N=0 ANTES de
# uma linha N>0 no MESMO log - o cenario real que isso esconde: um
# cliente que conecta e sai sem mandar nada (ex.: uma sonda externa)
# ANTES da conexao boa. Com o mutante, `awk` para na PRIMEIRA linha
# (N=0, por causa do `exit` logo que a condicao bate) e nunca chega na
# boa - a sonda reprovaria por engano mesmo com uma conexao real ja
# tendo decodificado mensagens.
selftest_case_relay_zero_before_good_line() (
    compositor_probe() { return 0; }
    GLINTFX_RELAY_LOG_WAIT_TRIES=2
    GLINTFX_RELAY_LOG_WAIT_SLEEP=0
    fake_log="$(mktemp "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-relaylog-XXXXXX")" || exit 1
    trap 'rm -f "$fake_log"' EXIT
    {
        printf 'wire_relay: connection closed - 0 message(s) from client, 0 from upstream, 0 violation(s)\n'
        printf 'wire_relay: connection closed - 45 message(s) from client, 291 from upstream, 0 violation(s)\n'
    } >"$fake_log"
    if ! wait_for_relay_ready "selftest-relay-zero-before-good" "$fake_log" >/dev/null 2>&1; then
        echo "SELFTEST FALHOU: linha N=0 seguida de linha N>0 devia ter sido aceita (a boa vem depois), foi reprovada" >&2
        exit 1
    fi
    echo "selftest: linha N=0 antes da linha boa nao esconde a linha boa - OK"
)

# CONTROLE NEGATIVO (o par do caso acima, para a regua continuar
# distinguindo os dois estados): so ha linhas "connection closed" com
# N=0, mais ruido de producao (perror de poll) - tem que continuar
# reprovando SEMPRE, antes e depois do conserto.
selftest_case_relay_all_zero_with_perror_noise() (
    compositor_probe() { return 0; }
    GLINTFX_RELAY_LOG_WAIT_TRIES=2
    GLINTFX_RELAY_LOG_WAIT_SLEEP=0
    fake_log="$(mktemp "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-relaylog-XXXXXX")" || exit 1
    trap 'rm -f "$fake_log"' EXIT
    {
        printf 'wire_relay: connection closed - 0 message(s) from client, 0 from upstream, 0 violation(s)\n'
        printf 'wire_relay: poll: Interrupted system call\n'
        printf 'wire_relay: connection closed - 0 message(s) from client, 5 from upstream, 0 violation(s)\n'
    } >"$fake_log"
    # Mesmo idioma de selftest_case_relay_never_accepts acima: fail()
    # (chamada por wait_for_relay_ready quando reprova, o resultado
    # ESPERADO aqui) sai com `exit`, nao `return` - o `exit 42` so e'
    # alcancado se a funcao voltasse normalmente por engano (ou seja,
    # se a checagem passasse quando deveria reprovar).
    wait_for_relay_ready "selftest-relay-all-zero" "$fake_log" >/dev/null 2>&1
    exit 42
)

# WL-ACK-SMOKE-BLUNT A3e, passo 2: publish_ready_marker() cria o
# arquivo no lugar exato que tests/container/wait_for_ready_marker.sh
# espera (RUNTIME_DIR/ready) - roda numa subshell com RUNTIME_DIR
# apontado pra um diretorio temporario proprio, nunca o real
# /run/glintfx-test (este selftest nunca toca a sessao do lider). A
# ORDEM (marcador so depois de wait_for_relay_ready) e' garantida por
# main() chamar as duas em sequencia (leitura direta do codigo, GODS_
# LAWS.md L-44) - nao repetida aqui como teste de runtime assincrono.
selftest_case_publish_ready_marker() (
    tmp_runtime_dir="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-marker-XXXXXX")" || exit 1
    trap 'rm -rf "$tmp_runtime_dir"' EXIT
    tmp_marker_path="${tmp_runtime_dir}/ready"
    [ ! -e "$tmp_marker_path" ] || {
        echo "SELFTEST FALHOU: marcador ja existia antes de publish_ready_marker() rodar" >&2
        exit 1
    }
    publish_ready_marker "$tmp_marker_path"
    if [ ! -f "$tmp_marker_path" ]; then
        echo "SELFTEST FALHOU: publish_ready_marker() nao criou o arquivo em $tmp_marker_path" >&2
        exit 1
    fi
    echo "selftest: publish_ready_marker() cria o marcador no lugar certo - OK"
)

run_selftest_marker_cases() {
    ok=0
    selftest_case_publish_ready_marker || ok=1
    return "$ok"
}

# WL-ACK-SMOKE-BLUNT A3e, passo 2b (achado do main em revisao
# adversarial, 24/09/2026, GODS_LAWS.md L-12 - sabotagem de familia
# DIFERENTE da do passo 1b): selftest_case_publish_ready_marker() acima
# so' prova que publish_ready_marker() cria um arquivo - passaria com
# QUALQUER ordem de chamadas dentro de bring_up(). Os dois casos abaixo
# rodam bring_up() DE VERDADE (a funcao real, nao uma copia) com as
# quatro funcoes que ela chama sobrescritas por stubs e RUNTIME_DIR
# apontado pra um diretorio temporario - a propriedade provada e' a
# ORDEM: o stub de wait_for_relay_ready() confere, no INSTANTE em que
# roda, se o marcador AINDA NAO existe. Mutante (o que o main pediu para
# matar): trocar a ordem de `wait_for_relay_ready` e
# `publish_ready_marker` dentro de bring_up() - com a ordem trocada, o
# marcador ja existiria quando o stub roda, em QUALQUER um dos dois
# casos abaixo, e o stub reprova com o codigo 97 antes de qualquer outra
# coisa.

# CASO 1: o rele nunca fica pronto - bring_up() tem que reprovar (o
# stub chama fail(), mesmo idioma de selftest_case_never_ready/
# selftest_case_relay_never_accepts acima: fail() sai com `exit`, nunca
# `return`) SEM o marcador ter sido publicado antes. O stub confere isso
# ele mesmo, porque e' o UNICO ponto que roda depois de onde o marcador
# teria sido publicado se a ordem estivesse trocada.
selftest_case_bring_up_relay_never_ready_no_marker() (
    tmp_runtime_dir="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-bringup-XXXXXX")" || exit 1
    trap 'rm -rf "$tmp_runtime_dir"' EXIT
    RUNTIME_DIR="$tmp_runtime_dir"
    start_compositor() { :; }
    wait_for_compositor_ready() { :; }
    start_relay() { :; }
    wait_for_relay_ready() {
        if [ -e "${RUNTIME_DIR}/${GLINTFX_READY_MARKER_FILENAME}" ]; then
            echo "SELFTEST FALHOU: marcador ja existia quando wait_for_relay_ready() rodou, mesmo no caminho em que o rele nunca fica pronto - ordem trocada" >&2
            exit 97
        fi
        fail "selftest: rele nunca ficou pronto (simulado)"
    }
    bring_up "selftest-bringup-never-ready" >/dev/null 2>&1
    # So' chega aqui se bring_up() RETORNASSE normalmente por engano -
    # o stub acima sempre termina o processo (fail() ou exit 97).
    exit 42
)

# CASO 2 (controle positivo do caso 1): o rele fica pronto - bring_up()
# tem que publicar o marcador, e SO' depois que wait_for_relay_ready()
# ja' rodou (o stub confere a ausencia do marcador no proprio instante
# em que e' chamado, antes de devolver sucesso).
selftest_case_bring_up_marker_after_relay_ready() (
    tmp_runtime_dir="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-run-compositor-selftest-bringup-XXXXXX")" || exit 1
    trap 'rm -rf "$tmp_runtime_dir"' EXIT
    RUNTIME_DIR="$tmp_runtime_dir"
    start_compositor() { :; }
    wait_for_compositor_ready() { :; }
    start_relay() { :; }
    wait_for_relay_ready() {
        if [ -e "${RUNTIME_DIR}/${GLINTFX_READY_MARKER_FILENAME}" ]; then
            echo "SELFTEST FALHOU: marcador ja existia quando wait_for_relay_ready() rodou - ordem trocada" >&2
            exit 97
        fi
        return 0
    }
    bring_up "selftest-bringup-marker-order" >/dev/null 2>&1
    if [ ! -f "${RUNTIME_DIR}/${GLINTFX_READY_MARKER_FILENAME}" ]; then
        echo "SELFTEST FALHOU: bring_up() nao publicou o marcador depois do rele ficar pronto" >&2
        exit 1
    fi
    echo "selftest: bring_up() so publica o marcador DEPOIS de wait_for_relay_ready confirmar pronto (ausente durante a chamada, presente depois) - OK"
)

run_selftest_bring_up_cases() {
    ok=0

    rc=0
    selftest_case_bring_up_relay_never_ready_no_marker || rc="$?"
    case "$rc" in
        42)
            echo "SELFTEST FALHOU: bring_up() nao reprovou quando o rele nunca ficou pronto" >&2
            ok=1
            ;;
        97)
            echo "SELFTEST FALHOU: ordem trocada detectada no caminho em que o rele nunca fica pronto (ver mensagem acima)" >&2
            ok=1
            ;;
        *)
            echo "selftest: rele nunca pronto -> bring_up() reprova sem publicar o marcador antes (codigo=$rc), como esperado - OK"
            ;;
    esac

    selftest_case_bring_up_marker_after_relay_ready || ok=1

    return "$ok"
}

# GODS_LAWS.md L-17: run_selftest() e' o proprio exemplo do arquivo de
# onde monolito nasce por conveniencia (cada caso novo "e' so' mais um
# bloco" - a quinta pergunta do revisor). Dividida por LADO (compositor
# puro x rele, N5/D-A3): cada uma tem razao propria de crescer, e
# ambas usam `return` (nao `exit`) porque run_selftest() as chama numa
# chamada de funcao normal, nao numa subshell propria.
run_selftest_compositor_cases() {
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

    return "$ok"
}

# WL-ACK-SMOKE-BLUNT A3b: os tres casos que so' existem por causa do
# rele (os dois do plano, N5/D-A3, mais o controle positivo do proprio
# retry de wait_for_relay_log_count() acima).
run_selftest_relay_cases() {
    ok=0

    rc=0
    selftest_case_relay_never_accepts || rc="$?"
    if [ "$rc" -eq 42 ]; then
        echo "SELFTEST FALHOU: rele que nunca aceita conexao foi declarado pronto" >&2
        ok=1
    else
        echo "selftest: rele que nunca aceita conexao reprovou o laco (codigo=$rc), como esperado - OK"
    fi

    rc=0
    selftest_case_relay_ready_but_zero_messages || rc="$?"
    if [ "$rc" -eq 42 ]; then
        echo "SELFTEST FALHOU: rele com zero mensagens decodificadas foi declarado pronto" >&2
        ok=1
    else
        echo "selftest: rele que aceita mas decodifica zero mensagens reprovou (codigo=$rc), como esperado - OK"
    fi

    selftest_case_relay_log_arrives_late || ok=1

    selftest_case_relay_good_line_not_last || ok=1

    selftest_case_relay_zero_before_good_line || ok=1

    rc=0
    selftest_case_relay_all_zero_with_perror_noise || rc="$?"
    if [ "$rc" -eq 42 ]; then
        echo "SELFTEST FALHOU: log so com N=0 (mais ruido de perror) deveria ter reprovado, passou" >&2
        ok=1
    else
        echo "selftest: log so com N=0 continua reprovando (controle negativo intacto, codigo=$rc) - OK"
    fi

    return "$ok"
}

run_selftest() {
    # WL-ACK-SMOKE-BLUNT A3e, achado proprio (revisao adversarial do
    # main pegou o sintoma - mutante M1 sobrevivendo - mas a causa raiz
    # era esta): `ok` NAO e local a nenhuma das run_selftest_*_cases()
    # abaixo (POSIX sh nao tem escopo de funcao sem `local`, que nao e
    # portavel) - cada uma delas faz `ok=0` na PROPRIA primeira linha e
    # usa o MESMO NOME. Chamar `run_selftest_relay_cases || ok=1` e
    # DEPOIS `run_selftest_marker_cases || ok=1` significa que o
    # `ok=0` que roda dentro de run_selftest_marker_cases() SOBRESCREVE
    # o `ok=1` que acabou de ser setado pela chamada anterior - se a
    # ULTIMA passar (mesmo com uma ANTERIOR tendo reprovado), a
    # reprovacao inteira desaparecia em silencio (medido ao vivo:
    # --selftest devolvia rc=0 com um caso reprovando de verdade por
    # baixo). `overall_ok`, nome diferente do `ok` que as sub-funcoes
    # usam, elimina a colisao.
    overall_ok=0
    run_selftest_compositor_cases || overall_ok=1
    run_selftest_relay_cases || overall_ok=1
    run_selftest_marker_cases || overall_ok=1
    run_selftest_bring_up_cases || overall_ok=1
    [ "$overall_ok" -eq 0 ] || fail "selftest reprovou (ver mensagens acima)"
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

# WL-ACK-SMOKE-BLUNT A3e, passo 2b (achado do main em revisao
# adversarial, 24/09/2026 - GODS_LAWS.md L-12): extraida de main() para
# que um selftest possa provar a ORDEM real das chamadas - o marcador
# so' e' publicado DEPOIS que o rele confirma pronto de verdade, nunca
# antes. Ate aqui isso so' estava garantido por LEITURA do codigo
# (comentario antigo, GODS_LAWS.md L-44: "leitura direta do codigo" nao
# e' prova); um mutante que trocasse a ORDEM das duas ultimas linhas
# abaixo (publicar antes de esperar) nao reprovava nenhum selftest
# existente. bring_up() e' a MESMA sequencia que main() chamava direto -
# nao uma copia reescrita para o teste (GODS_LAWS.md L-27: o selftest
# tem que exercitar o codigo real).
bring_up() {
    require_socket_name_arg "$@"
    create_private_runtime_dir
    export_runtime_env
    external_socket_name="$1"
    upstream_socket_name="$(internal_socket_name "$external_socket_name")"
    relay_log_file="$RUNTIME_DIR/wire_relay.log"
    start_compositor "$upstream_socket_name"
    wait_for_compositor_ready "$upstream_socket_name"
    start_relay "$upstream_socket_name" "$external_socket_name" "$relay_log_file"
    wait_for_relay_ready "$external_socket_name" "$relay_log_file"
    publish_ready_marker "${RUNTIME_DIR}/${GLINTFX_READY_MARKER_FILENAME}"
}

main() {
    if [ "$#" -eq 1 ] && [ "$1" = "--selftest" ]; then
        run_selftest
        exit 0
    fi
    bring_up "$@"
    stay_up_forever
}

main "$@"
