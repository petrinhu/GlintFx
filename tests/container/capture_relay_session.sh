#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# capture_relay_session.sh - the wire relay session of the known-color capture (DEMO-1, D2b).
#
# A POSIX sh LIBRARY, loaded once with `.` by a driver (tests/container/run_capture_known_color.sh). It has no main,
# no `set` of its own and no selftest dispatch of its own: loading it runs nothing except the wait settings below
# (their defaults and their validation). Its functions start a wire relay inside the running test container, wait
# for the relay socket and for the capture files of the presenting connection, stop the relay, copy the files out
# with docker cp, and judge them with run_check. The mode-specific parts (fixture, verdict, modes) stay in the driver.
#
# CONTRACT with the caller (read by the functions below, never assigned here):
#   RELAY_SESSION_CALLER  readonly, set by the caller BEFORE loading; prefixes every message of this library.
#                         Loading without it aborts (the guard below).
#   CONTAINER             the test container name; set by the caller before any function here runs.
#   OUT_DIR               the host output directory; set by the caller before copy_out and clean_mode_output run.
#
# CAPTURE_WAIT_TRIES and CAPTURE_WAIT_SLEEP may come from the environment; their defaults and validation are below.

: "${RELAY_SESSION_CALLER:?capture_relay_session.sh: defina RELAY_SESSION_CALLER antes de carregar}"

readonly CAPTURE_ROOT="/tmp/glintfx-capture"
readonly SOCKET_WAIT_TRIES=30
# Only --selftest overrides these (a one-try, zero-sleep wait). Validated right below, once the helpers exist.
: "${CAPTURE_WAIT_TRIES:=30}"
: "${CAPTURE_WAIT_SLEEP:=1}"
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

# in_container <command...>: runs a command in the test container, with the runtime dir the compositor uses.
in_container() {
    docker exec -e XDG_RUNTIME_DIR=/run/glintfx-test "$CONTAINER" "$@"
}

# start_capture_relay <mode>: the relay's pid goes to a file INSIDE the container, so it is stopped by pid
# afterwards, never by a name pattern (pkill -f matches itself, feedback_pgrep_encontra_a_si_mesmo).
start_capture_relay() {
    mode="$1"
    # A previous run's files would be read as this run's capture (two surfaces, or a stale frame): start clean.
    in_container rm -rf "${CAPTURE_ROOT:?}/$mode"
    in_container mkdir -p "$CAPTURE_ROOT/$mode/frames" "$CAPTURE_ROOT/$mode/readback"
    docker exec -d -e XDG_RUNTIME_DIR=/run/glintfx-test "$CONTAINER" sh -c \
        "echo \$\$ > $CAPTURE_ROOT/$mode/relay.pid; exec wire_relay glintfx-test-upstream glintfx-cap-$mode $CAPTURE_ROOT/$mode/frames > $CAPTURE_ROOT/$mode/relay.log 2>&1"
}

wait_for_socket() {
    mode="$1"
    tries=0
    while [ "$tries" -lt "$SOCKET_WAIT_TRIES" ]; do
        if in_container test -S "/run/glintfx-test/glintfx-cap-$mode"; then
            return 0
        fi
        tries=$((tries + 1))
        sleep 1
    done
    echo "${RELAY_SESSION_CALLER}: FAIL o rele de captura ($mode) nao abriu o socket em ${SOCKET_WAIT_TRIES}s" >&2
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
    mode="$1"
    mkdir -p "$OUT_DIR/$mode/frames" "$OUT_DIR/$mode/readback"
    docker cp "$CONTAINER:$CAPTURE_ROOT/$mode/frames/." "$OUT_DIR/$mode/frames"
    docker cp "$CONTAINER:$CAPTURE_ROOT/$mode/readback/." "$OUT_DIR/$mode/readback"
    docker cp "$CONTAINER:$CAPTURE_ROOT/$mode/relay.log" "$OUT_DIR/$mode/relay.log"
}

# run_check <rc-file> <command...>: the command's output is shown on stderr, its exit code goes to a FILE and is
# read back (GODS_LAWS.md L-45); stdout carries only that code.
run_check() {
    rc_file="$1"
    shift
    set +e
    "$@" >"$rc_file.log" 2>&1
    echo "$?" >"$rc_file"
    set -e
    cat "$rc_file.log" >&2
    cat "$rc_file"
}

# clean_mode_output <mode>: a reused out directory would hand the judge the PREVIOUS run's frames and readback
# (docker cp lays files over what is there): a fixture that does nothing then reads as a pass. Both expansions
# abort the shell when empty, so this can never become `rm -rf /` (GODS_LAWS.md L-53).
clean_mode_output() {
    rm -rf "${OUT_DIR:?}/${1:?}"
}
