#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# run_capture_known_color.sh - QA-SCREEN-CAPTURE P3 (D-W8-20, D-W8-32, D-W8-33, GODS_LAWS.md L-09/L-36/L-40).
#
# THE DRIVER of the protocol-capture proof: for each srgb_framebuffer mode (off, on) it starts a SECOND wire
# relay inside the already-running test container, in front of the SAME KWin the container's own relay talks
# to, with a capture directory (the third argument of wire_relay_main, which this run is the first end-to-end
# test of); runs tests/parity/capture_known_color_smoke.cpp's binary through that relay; copies the capture
# and the client's own readback OUT of the container with `docker cp`; and judges, on the host:
#   1. the fixture's own exit code (it must have presented a frame);
#   2. tests/tools/raw_to_png.py: at least one image converted, nothing unaccounted for (zero reproves, L-40);
#   3. tests/tools/capture_vs_readback.py: the capture equals the readback pixel by pixel, alpha included (D-W8-33);
#   4. tests/tools/image_probe.py: the pixels are the known colors (tests/fixtures/capture_known_color_probes_<mode>.txt).
# One mode is one process is one connection is one captured surface, so the mode of every capture is known by
# construction (no pairing by guessing). A SECOND relay, not the container's own, so no other fixture pays for
# frame copies and run_compositor.sh does not change. The relay is a client of the compositor on the internal
# socket name run_compositor.sh already publishes; the capture directory lives INSIDE the container and leaves
# only through `docker cp` (nothing of the host is mounted, L-09).
#
# USAGE: run_capture_known_color.sh [--env NAME=VALUE ...] [--sabotage NAME] <container> <out-directory>
#        run_capture_known_color.sh --selftest   (no container: the pure decisions of this driver)
# --env is passed to exec_fixture.sh (the asan leg's runtime options). --sabotage is the debut proof (L-36): the
# fixture draws a wrong frame on purpose and the verdict MUST be a rejection; this script does not invert it.
# The fixture's exit 77 is its DECLARED ABSENCE (srgb_framebuffer=on refused by name): the Windows driver counts it,
# THIS driver treats it as a FAILURE (llvmpipe supports sRGB here, so an absence is a regression); mode_verdict
# below needs the fixture rc to be exactly 0. Exit 0 only when every mode passed all four checks. The verdict line of each mode and a final summary line
# are always printed.
#
# WHAT THIS DOES NOT PROVE (declared, L-43): the Windows capture (QA-SCREEN-CAPTURE C2b); the composition KWin
# does after the commit (the capture is the buffer the client hands over, D-W8-20); another driver or Mesa
# than the one this image carries.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
readonly SCRIPT_DIR REPO_ROOT
readonly CAPTURE_ROOT="/tmp/glintfx-capture"
readonly TOOLS_DIR="$REPO_ROOT/tests/tools"
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
    echo "run_capture_known_color: CAPTURE_WAIT_TRIES ('$CAPTURE_WAIT_TRIES') e inteiro positivo e CAPTURE_WAIT_SLEEP ('$CAPTURE_WAIT_SLEEP') inteiro nao negativo" >&2
    exit 2
fi

fail_usage() {
    echo "uso: run_capture_known_color.sh [--env NOME=VALOR ...] [--sabotage NOME] <container> <diretorio-de-saida>" >&2
    exit 2
}

# Words of the --env options, kept as one string of "--env X" pairs (POSIX sh has no arrays); values never
# contain spaces (ASAN_OPTIONS=a=1:b=2 style).
ENV_ARGS=""
SABOTAGE=""

parse_args() {
    while [ "$#" -gt 0 ]; do
        case "$1" in
            --env) [ "$#" -ge 2 ] || fail_usage; ENV_ARGS="$ENV_ARGS --env $2"; shift 2 ;;
            --sabotage) [ "$#" -ge 2 ] || fail_usage; SABOTAGE="$2"; shift 2 ;;
            -*) fail_usage ;;
            *) break ;;
        esac
    done
    [ "$#" -eq 2 ] || fail_usage
    CONTAINER="$1"
    OUT_DIR="$2"
}

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
    echo "run_capture_known_color: FAIL o rele de captura ($mode) nao abriu o socket em ${SOCKET_WAIT_TRIES}s" >&2
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
    echo "run_capture_known_color: FAIL ($mode) esperava um arquivo casando $PRESENTING_PATTERN em $CAPTURE_ROOT/$mode/frames em ${CAPTURE_WAIT_TRIES} tentativa(s); achei: $(printf '%s' "$listing" | tr '\n' ' ')" >&2
    return 1
}

stop_capture_relay() {
    mode="$1"
    in_container sh -c "kill \$(cat $CAPTURE_ROOT/$mode/relay.pid) 2>/dev/null || true"
}

# run_fixture <mode>: through exec_fixture.sh (the verdict line first, rc from a file). WAYLAND_DISPLAY is
# overridden AFTER the one exec_fixture.sh always sets, so the fixture connects to the capture relay.
run_fixture() {
    mode="$1"
    set --
    [ -z "$SABOTAGE" ] || set -- "$SABOTAGE"
    # shellcheck disable=SC2086
    "$SCRIPT_DIR/exec_fixture.sh" $ENV_ARGS --env "WAYLAND_DISPLAY=glintfx-cap-$mode" \
        "$CONTAINER" capture_known_color_smoke "$mode" "$CAPTURE_ROOT/$mode/readback" "$@"
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

# judge_mode <mode>: the three host-side checks over what docker cp brought out. Prints one verdict line.
judge_mode() {
    mode="$1"
    dir="$OUT_DIR/$mode"
    png_rc="$(run_check "$dir/png.rc" python3 "$TOOLS_DIR/raw_to_png.py" "$dir/frames" "$dir/png")"
    cmp_rc="$(run_check "$dir/compare.rc" python3 "$TOOLS_DIR/capture_vs_readback.py" "$dir/frames" \
        "$dir/readback/readback_$mode.raw" "$dir/readback/readback_$mode.meta")"
    probe_rc=1
    png_count="$(find "$dir/png" -name '*.png' 2>/dev/null | wc -l | tr -d ' ')"
    if [ "$png_count" -eq 1 ]; then
        probe_rc="$(run_check "$dir/probe.rc" python3 "$TOOLS_DIR/image_probe.py" \
            "$(find "$dir/png" -name '*.png')" "$REPO_ROOT/tests/fixtures/capture_known_color_probes_$mode.txt")"
    else
        echo "run_capture_known_color: FAIL $mode: $png_count PNG(s), exatamente 1 esperado" >&2
    fi
    echo "$png_rc $cmp_rc $probe_rc" >"$dir/checks.rc"
    echo "run_capture_known_color: mode=$mode raw_to_png=$png_rc capture_vs_readback=$cmp_rc image_probe=$probe_rc"
    [ "$png_rc" -eq 0 ] && [ "$cmp_rc" -eq 0 ] && [ "$probe_rc" -eq 0 ]
}

# clean_mode_output <mode>: a reused out directory would hand the judge the PREVIOUS run's frames and readback
# (docker cp lays files over what is there): a fixture that does nothing then reads as a pass. Both expansions
# abort the shell when empty, so this can never become `rm -rf /` (GODS_LAWS.md L-53).
clean_mode_output() {
    rm -rf "${OUT_DIR:?}/${1:?}"
}

# mode_verdict <judged> <fixture-rc> <waited>: a mode passes only when the three host checks, the fixture AND the wait
# for the capture all came out clean (0). Pure, so the selftest can prove every part counts.
mode_verdict() {
    [ "$1" -eq 0 ] && [ "$2" -eq 0 ] && [ "$3" -eq 0 ]
}

# one_mode <mode>: returns 0 only when the fixture and the three checks passed.
one_mode() {
    mode="$1"
    clean_mode_output "$mode"
    start_capture_relay "$mode"
    if ! wait_for_socket "$mode"; then
        stop_capture_relay "$mode"
        copy_out "$mode"
        echo "run_capture_known_color: mode=$mode fixture=nao-rodou espera=socket-falhou"
        return 1
    fi
    fixture_rc="$(run_check "$OUT_DIR/$mode.fixture.rc" run_fixture "$mode")"
    waited=0
    wait_for_capture_files "$mode" || waited=1
    stop_capture_relay "$mode"
    copy_out "$mode"
    echo "run_capture_known_color: mode=$mode fixture=$fixture_rc espera=$waited"
    judged=0
    judge_mode "$mode" || judged=1
    mode_verdict "$judged" "$fixture_rc" "$waited"
}

# modes_are_exact <list>: the run covered EXACTLY the two srgb_framebuffer modes, each once. A count is not
# enough: `off off` has two entries and never runs `on`.
modes_are_exact() {
    # shellcheck disable=SC2086
    [ "$(printf '%s\n' $1 | sort | tr '\n' ' ')" = "off on " ]
}

main() {
    parse_args "$@"
    mkdir -p "$OUT_DIR"
    modes_seen=""
    modes_failed=0
    for mode in off on; do
        modes_seen="$modes_seen $mode"
        one_mode "$mode" || modes_failed=$((modes_failed + 1))
    done
    echo "run_capture_known_color: modos=$modes_seen reprovados=$modes_failed sabotagem=${SABOTAGE:-nenhuma}"
    modes_are_exact "$modes_seen" && [ "$modes_failed" -eq 0 ]
}

# -- selftest: the pure decisions, no container, no docker ------------------------------------------------------
SELFTEST_CHECKS=0

selftest_check() {
    SELFTEST_CHECKS=$((SELFTEST_CHECKS + 1))
    if ! "$@"; then
        echo "selftest: controle $SELFTEST_CHECKS FALHOU: $*" >&2
        exit 1
    fi
}

selftest_stale_output_is_removed() {
    root="$(mktemp -d)"
    mkdir -p "$root/off/frames"
    echo stale >"$root/off/frames/conn1_surface6.meta"
    echo keep >"$root/on.keep"
    OUT_DIR="$root" clean_mode_output off
    [ ! -e "$root/off" ] && [ -e "$root/on.keep" ]
    rc=$?
    rm -rf "${root:?}"
    return "$rc"
}

selftest_empty_out_dir_refuses() {
    ! (OUT_DIR="" clean_mode_output off) 2>/dev/null
}

selftest_modes_exact_accepts_both() {
    modes_are_exact " off on" && modes_are_exact " on off"
}

selftest_modes_exact_rejects_wrong_sets() {
    ! modes_are_exact " off off" && ! modes_are_exact " off" && ! modes_are_exact " on on" &&
        ! modes_are_exact "" && ! modes_are_exact " off on on"
}

selftest_wait_ignores_the_other_connection() {
    [ "$(printf 'conn2_no_frame.txt\n' | presenting_files_count)" -eq 0 ] &&
        [ "$(printf 'conn2_surface9.meta\nconn2_no_frame.txt\n' | presenting_files_count)" -eq 0 ] &&
        [ "$(printf '' | presenting_files_count)" -eq 0 ]
}

selftest_wait_sees_the_presenting_connection() {
    [ "$(printf 'conn2_no_frame.txt\nconn1_surface6.meta\nconn1_surface6.raw\n' | presenting_files_count)" -eq 1 ] &&
        [ "$(printf 'conn1_no_frame.txt\n' | presenting_files_count)" -eq 1 ]
}

selftest_wait_needs_the_meta_not_the_raw() {
    [ "$(printf 'conn1_surface6.raw\n' | presenting_files_count)" -eq 0 ]
}

# The numbering of connections changed (the presenting one is no longer conn1): the wait must FAIL, saying what it
# expected and what it found, never time out quietly.
selftest_wait_timeout_names_expected_and_found() {
    in_container() { printf 'conn7_surface6.meta\nconn7_surface6.raw\n'; }
    message="$(CAPTURE_WAIT_TRIES=1 CAPTURE_WAIT_SLEEP=0 wait_for_capture_files off 2>&1)" && return 1
    unset -f in_container
    case "$message" in
        *conn1_*conn7_surface6.meta*) return 0 ;;
        *) echo "$message" >&2; return 1 ;;
    esac
}

selftest_verdict_needs_every_part_clean() {
    mode_verdict 0 0 0
}

selftest_verdict_rejects_each_failed_part() {
    ! mode_verdict 1 0 0 && ! mode_verdict 0 1 0 && ! mode_verdict 0 0 1 && ! mode_verdict 0 3 1 &&
        ! mode_verdict 0 77 0
}

selftest_wait_settings_are_validated() {
    ( valid_wait_settings 30 1 ) && ( valid_wait_settings 1 0 ) &&
        ! ( valid_wait_settings 0 1 ) && ! ( valid_wait_settings abc 1 ) && ! ( valid_wait_settings 1 -1 ) &&
        ! ( valid_wait_settings "" 1 ) && ! ( valid_wait_settings 1 "" )
}

# CALLS_LOG (set by selftest_main): one line per call the stubs of one_mode_with received, "<collaborator> <mode>",
# in call order. one_mode_with empties it before every run (not each control, so no control can forget to), and a
# control reads only the calls of its own run.
reset_calls() {
    : >"$CALLS_LOG"
}

# one_mode_with <mode> <fixture-rc> <wait-rc> <judge-rc>: runs one_mode <mode> with every container collaborator
# replaced by a stub. Each stub records its call in CALLS_LOG and returns its code; wait_for_socket returns
# STUB_SOCKET_RC (0 unless the caller sets it in the prefix of the call). The stubs live in a subshell, so they never
# outlive this check (unset -f would erase the real function instead of restoring it). Prints the one_mode line and
# returns one_mode's own verdict.
one_mode_with() {
    reset_calls
    (
        mode="$1"
        OUT_DIR="$(mktemp -d)" || exit 1
        STUB_FIXTURE_RC="$2"
        STUB_WAIT_RC="$3"
        STUB_JUDGE_RC="$4"
        STUB_SOCKET_RC="${STUB_SOCKET_RC:-0}"
        start_capture_relay() { echo "start_capture_relay $1" >>"$CALLS_LOG"; }
        wait_for_socket() { echo "wait_for_socket $1" >>"$CALLS_LOG"; return "$STUB_SOCKET_RC"; }
        run_fixture() { echo "run_fixture $1" >>"$CALLS_LOG"; return "$STUB_FIXTURE_RC"; }
        wait_for_capture_files() { echo "wait_for_capture_files $1" >>"$CALLS_LOG"; return "$STUB_WAIT_RC"; }
        stop_capture_relay() { echo "stop_capture_relay $1" >>"$CALLS_LOG"; }
        copy_out() { echo "copy_out $1" >>"$CALLS_LOG"; }
        judge_mode() { echo "judge_mode $1" >>"$CALLS_LOG"; return "$STUB_JUDGE_RC"; }
        rc=0
        one_mode "$mode" || rc=1
        rm -rf "${OUT_DIR:?}"
        exit "$rc"
    )
}

# The wiring of one_mode, one case per mutant of the R4-A review (rev-p3.md). The positive control comes first: an
# all-clean run must PASS, or the negative cases below would also pass on a broken harness.
selftest_wiring_clean_mode_passes() {
    out="$(one_mode_with off 0 0 0)" && [ "$out" = "run_capture_known_color: mode=off fixture=0 espera=0" ]
}

# A timed-out wait fails the mode. Kills the two mutants that alter the path of the wait: mode_verdict given 0 in
# place of $waited, and `waited=0` in the failure branch of the wait.
selftest_wiring_timed_out_wait_fails_the_mode() {
    ! one_mode_with off 0 1 0 >/dev/null
}

selftest_wiring_failed_judge_fails_the_mode() {
    ! one_mode_with off 0 0 1 >/dev/null
}

# Exit 77 is the declared absence, which this driver counts as a failure (see the header).
selftest_wiring_absent_fixture_fails_the_mode() {
    ! one_mode_with off 77 0 0 >/dev/null
}

# valid_wait_settings runs at the top of the script, before any dispatch. The probe runs the script with a bad
# CAPTURE_WAIT_TRIES and NO arguments: a child started with --selftest would run this selftest again and spawn
# itself without end. The real script refuses with rc 2 and names the variable; with the call removed, it reaches
# main, which refuses with the usage line. The message tells the two apart, the rc alone does not.
selftest_wiring_bad_wait_settings_refused() {
    out="$(CAPTURE_WAIT_TRIES=abc sh "$0" 2>&1)"
    rc=$?
    [ "$rc" -eq 2 ] || return 1
    case "$out" in
        *CAPTURE_WAIT_TRIES*) return 0 ;;
    esac
    return 1
}

# expected_normal_calls <mode>: the calls a clean one_mode run makes, in the order the driver makes them. The order is
# part of the contract: the capture is waited for (wait_for_capture_files) before the relay is stopped, because the
# relay can close its connection before the frame is on disk (the comments above wait_for_capture_files).
expected_normal_calls() {
    printf '%s\n' "start_capture_relay $1" "wait_for_socket $1" "run_fixture $1" \
        "wait_for_capture_files $1" "stop_capture_relay $1" "copy_out $1" "judge_mode $1"
}

# expected_socket_calls <mode>: the calls of a run whose socket never came up. The fixture never runs; the relay is
# stopped and the capture copied out, in that order, so no relay is left orphaned in the container.
expected_socket_calls() {
    printf '%s\n' "start_capture_relay $1" "wait_for_socket $1" "stop_capture_relay $1" "copy_out $1"
}

# The clean run, one control per mode: the seven calls, in the driver's order, with that mode, and nothing else.
selftest_wiring_on_mode_calls_in_order() {
    one_mode_with on 0 0 0 >/dev/null && [ "$(cat "$CALLS_LOG")" = "$(expected_normal_calls on)" ]
}

selftest_wiring_off_mode_calls_in_order() {
    one_mode_with off 0 0 0 >/dev/null && [ "$(cat "$CALLS_LOG")" = "$(expected_normal_calls off)" ]
}

# A socket that never comes up, in BOTH modes: the run fails, the verdict line names this mode and the socket failure,
# and the calls are the expected ones in order (stop before copy). A branch that hardcodes one mode passes only one.
selftest_wiring_socket_failure_stops_relay_and_reports() {
    modes_run=""
    for m in on off; do
        modes_run="$modes_run $m"
        out="$(STUB_SOCKET_RC=1 one_mode_with "$m" 0 0 0)"
        rc=$?
        [ "$rc" -ne 0 ] || return 1
        [ "$out" = "run_capture_known_color: mode=$m fixture=nao-rodou espera=socket-falhou" ] || return 1
        [ "$(cat "$CALLS_LOG")" = "$(expected_socket_calls "$m")" ] || return 1
    done
    # The loop must have run both modes, each once, in order: a loop that drops or repeats one would pass every check
    # above. modes_are_exact checks the set and the count; the literal compare checks the order.
    modes_are_exact "$modes_run" && [ "$modes_run" = " on off" ]
}

selftest_main() {
    CALLS_LOG="${TMPDIR:-/tmp}/run_capture_known_color.selftest.calls.$$"
    selftest_check selftest_stale_output_is_removed
    selftest_check selftest_empty_out_dir_refuses
    selftest_check selftest_modes_exact_accepts_both
    selftest_check selftest_modes_exact_rejects_wrong_sets
    selftest_check selftest_wait_ignores_the_other_connection
    selftest_check selftest_wait_sees_the_presenting_connection
    selftest_check selftest_wait_needs_the_meta_not_the_raw
    selftest_check selftest_wait_timeout_names_expected_and_found
    selftest_check selftest_verdict_needs_every_part_clean
    selftest_check selftest_verdict_rejects_each_failed_part
    selftest_check selftest_wait_settings_are_validated
    selftest_check selftest_wiring_clean_mode_passes
    selftest_check selftest_wiring_timed_out_wait_fails_the_mode
    selftest_check selftest_wiring_failed_judge_fails_the_mode
    selftest_check selftest_wiring_absent_fixture_fails_the_mode
    selftest_check selftest_wiring_bad_wait_settings_refused
    selftest_check selftest_wiring_on_mode_calls_in_order
    selftest_check selftest_wiring_off_mode_calls_in_order
    selftest_check selftest_wiring_socket_failure_stops_relay_and_reports
    rm -f "$CALLS_LOG"
    echo "selftest: $SELFTEST_CHECKS controles OK"
}

if [ "${1:-}" = "--selftest" ]; then
    selftest_main
else
    main "$@"
fi
