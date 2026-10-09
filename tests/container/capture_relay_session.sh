#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# capture_relay_session.sh - the wire relay session of the known-color capture (DEMO-1, D2b).
#
# A POSIX sh LIBRARY, loaded once with `.` by a driver (tests/container/run_capture_known_color.sh). It has no main and
# no selftest dispatch of its own: loading it runs nothing except the wait settings below (their defaults and their
# validation). Its functions start a wire relay inside the running test container, wait for the relay socket and
# for the capture files of the presenting connection, stop the relay, copy the files out with docker cp, and clean the
# host directory copy_out writes to (clean_mode_output: copy_out lays files over what is there, so a run starts from an
# empty one). The mode-specific parts (fixture, verdict, modes) stay in the driver.
#
# CONTRACT with the caller (read by the functions below, never assigned here):
#   RELAY_SESSION_CALLER  readonly, set by the caller BEFORE loading; prefixes every message of this library.
#                         Loading without it aborts (the guard below).
#   CONTAINER             the test container name; set by the caller before any function here runs.
#   OUT_DIR               the host output directory; set by the caller before copy_out and clean_mode_output run.
#
# CAPTURE_WAIT_TRIES and CAPTURE_WAIT_SLEEP may come from the environment; their defaults and validation are below.
# load once: a second load aborts on the readonly values, which is what keeps a driver from reloading it over its own stubs.
# the container functions are proved by the real smoke (ci.yml), not by the selftest's stubs. start_capture_relay and
# stop_capture_relay have no guard of their own: both call in_container, whose guard fires first.
# The two waits call require_container FIRST: their in_container runs inside $(...), where its guard would only end
# that substitution and the wait would time out blaming the relay (rev-d2bfix IMP-3).

: "${RELAY_SESSION_CALLER:?capture_relay_session.sh: defina RELAY_SESSION_CALLER antes de carregar}"

readonly CAPTURE_ROOT="/tmp/glintfx-capture"
readonly RELAY_RUNTIME_DIR="/run/glintfx-test"
readonly RELAY_UPSTREAM_NAME="glintfx-test-upstream"
# the environment may override these; the selftest controls do so inline. Validated right below, once the helpers exist.
: "${CAPTURE_WAIT_TRIES:=30}"
: "${CAPTURE_WAIT_SLEEP:=1}"
: "${SOCKET_WAIT_TRIES:=30}"
: "${SOCKET_WAIT_SLEEP:=1}"
# The one pattern of "the files of the connection that presents the frame": the wait counts with it and its failure
# message quotes it, so the two cannot drift apart.
readonly PRESENTING_PATTERN='^conn1_(surface[0-9]+[.]meta|no_frame[.]txt)$'

# valid_wait_settings <tries> <sleep>: tries is a positive integer, sleep a non-negative one (digits only).
valid_wait_settings() {
    case "$1$2" in *[!0-9]*) return 1 ;; esac
    [ -n "$1" ] && [ -n "$2" ] && [ "$1" -ge 1 ]
}

if ! valid_wait_settings "$CAPTURE_WAIT_TRIES" "$CAPTURE_WAIT_SLEEP"; then
    echo "${RELAY_SESSION_CALLER}: CAPTURE_WAIT_TRIES ('$CAPTURE_WAIT_TRIES') e inteiro positivo e CAPTURE_WAIT_SLEEP ('$CAPTURE_WAIT_SLEEP') inteiro nao negativo" >&2
    exit 2
fi

if ! valid_wait_settings "$SOCKET_WAIT_TRIES" "$SOCKET_WAIT_SLEEP"; then
    echo "${RELAY_SESSION_CALLER}: SOCKET_WAIT_TRIES ('$SOCKET_WAIT_TRIES') e inteiro positivo e SOCKET_WAIT_SLEEP ('$SOCKET_WAIT_SLEEP') inteiro nao negativo" >&2
    exit 2
fi

# require_container: aborts the shell (or the substitution it runs in) when CONTAINER is empty, before any docker call.
require_container() {
    : "${CONTAINER:?capture_relay_session.sh: CONTAINER vazio; o chamador define antes de usar}"
}

# in_container <command...>: runs a command in the test container, with the runtime dir the compositor uses.
in_container() {
    require_container
    docker exec -e XDG_RUNTIME_DIR="$RELAY_RUNTIME_DIR" "$CONTAINER" "$@"
}

# relay_socket_name <mode>: the socket the capture relay of <mode> listens on AND the WAYLAND_DISPLAY the fixture
# connects to; one name, so the two cannot drift apart.
relay_socket_name() {
    printf 'glintfx-cap-%s\n' "$1"
}

# relay_ready_line <mode>: the exact line wire_relay prints AFTER listen() and fflush
# (tests/container/wire_relay/wire_relay_main.cpp, "wire_relay: listening on %s, upstream %s").
relay_ready_line() {
    printf 'wire_relay: listening on %s/%s, upstream %s/%s\n' "$RELAY_RUNTIME_DIR" "$(relay_socket_name "$1")" \
        "$RELAY_RUNTIME_DIR" "$RELAY_UPSTREAM_NAME"
}

# relay_announced <mode>: reads a relay log on stdin; 0 only when it holds the ready line of <mode>, whole and exact.
relay_announced() {
    grep -F -x -q -e "$(relay_ready_line "$1")"
}

# start_capture_relay <mode>: the relay's pid goes to a file INSIDE the container, so it is stopped by pid
# afterwards, never by a name pattern (pkill -f matches itself, feedback_pgrep_encontra_a_si_mesmo).
start_capture_relay() {
    mode="$1"
    # A previous run's files would be read as this run's capture (two surfaces, or a stale frame): start clean.
    in_container rm -rf "${CAPTURE_ROOT:?}/$mode"
    in_container mkdir -p "$CAPTURE_ROOT/$mode/frames" "$CAPTURE_ROOT/$mode/readback"
    docker exec -d -e XDG_RUNTIME_DIR="$RELAY_RUNTIME_DIR" "$CONTAINER" sh -c \
        "echo \$\$ > $CAPTURE_ROOT/$mode/relay.pid; exec wire_relay $RELAY_UPSTREAM_NAME $(relay_socket_name "$mode") $CAPTURE_ROOT/$mode/frames > $CAPTURE_ROOT/$mode/relay.log 2>&1"
}

# wait_for_socket <mode>: waits for the relay's OWN announcement, never for the socket file: a file left by an earlier
# run passes `test -S`, and so does a socket bound but not yet listening (RELAY-SOCKET-STALE).
wait_for_socket() {
    require_container
    mode="$1"
    tries=0
    relay_log=""
    while [ "$tries" -lt "$SOCKET_WAIT_TRIES" ]; do
        relay_log="$(in_container cat "$CAPTURE_ROOT/$mode/relay.log" 2>/dev/null)" || relay_log=""
        if printf '%s\n' "$relay_log" | relay_announced "$mode"; then
            return 0
        fi
        tries=$((tries + 1))
        sleep "$SOCKET_WAIT_SLEEP"
    done
    echo "${RELAY_SESSION_CALLER}: FAIL o rele de captura ($mode) nao anunciou '$(relay_ready_line "$mode")' em ${SOCKET_WAIT_TRIES} tentativa(s); log: $(printf '%s' "$relay_log" | tr '\n' ' ')" >&2
    return 1
}

# presenting_files_count: reads file names on stdin and prints how many of them are the files of the connection that
# PRESENTS the frame (conn1, measured in every run: the fixture's first Wayland connection is the display it draws
# on; the second one, opened by the EGL layer, never commits a buffer). The second connection closes FIRST, so its
# "nenhum quadro" marker is on disk before the frame is: waiting for ANY marker returned early, and the relay was
# stopped before it saved the frame.
presenting_files_count() {
    grep -c -E "$PRESENTING_PATTERN" || true
}

# The relay writes the capture when the client's connection closes, a moment AFTER the fixture exits; the
# .meta is written after its .raw, so the .meta (or the no-frame marker) of the presenting connection means
# its files are complete.
wait_for_capture_files() {
    require_container
    mode="$1"
    tries=0
    listing=""
    while [ "$tries" -lt "$CAPTURE_WAIT_TRIES" ]; do
        listing="$(in_container ls "$CAPTURE_ROOT/$mode/frames")"
        found="$(printf '%s\n' "$listing" | presenting_files_count)"
        if [ "${found:-0}" -ge 1 ]; then
            return 0
        fi
        tries=$((tries + 1))
        sleep "$CAPTURE_WAIT_SLEEP"
    done
    echo "${RELAY_SESSION_CALLER}: FAIL ($mode) esperava um arquivo casando $PRESENTING_PATTERN em $CAPTURE_ROOT/$mode/frames em ${CAPTURE_WAIT_TRIES} tentativa(s); achei: $(printf '%s' "$listing" | tr '\n' ' ')" >&2
    return 1
}

stop_capture_relay() {
    mode="$1"
    in_container sh -c "kill \$(cat $CAPTURE_ROOT/$mode/relay.pid) 2>/dev/null || true"
}

copy_out() {
    : "${OUT_DIR:?capture_relay_session.sh: OUT_DIR vazio; o chamador define antes de usar}"
    require_container
    mode="$1"
    mkdir -p "$OUT_DIR/$mode/frames" "$OUT_DIR/$mode/readback"
    docker cp "$CONTAINER:$CAPTURE_ROOT/$mode/frames/." "$OUT_DIR/$mode/frames"
    docker cp "$CONTAINER:$CAPTURE_ROOT/$mode/readback/." "$OUT_DIR/$mode/readback"
    docker cp "$CONTAINER:$CAPTURE_ROOT/$mode/relay.log" "$OUT_DIR/$mode/relay.log"
}

# clean_mode_output <mode>: a reused out directory would hand the judge the PREVIOUS run's frames and readback
# (docker cp lays files over what is there): a fixture that does nothing then reads as a pass. Both expansions
# abort the shell when empty, so this can never become `rm -rf /` (GODS_LAWS.md L-53).
clean_mode_output() {
    rm -rf "${OUT_DIR:?}/${1:?}"
}
