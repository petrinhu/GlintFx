#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# run_capture_known_color.sh - QA-SCREEN-CAPTURE P3 (D-W8-20, D-W8-32, D-W8-33, GODS_LAWS.md L-09/L-36/L-40).
#
# THE DRIVER of the protocol-capture proof: for each srgb_framebuffer mode (off, on) it starts a SECOND wire
# relay inside the already-running test container, in front of the SAME KWin the container's own relay talks
# to, with a capture directory (the third argument of wire_relay_main, which this run is the first end-to-end
# test of); runs tests/container/capture_known_color_smoke.cpp's binary through that relay; copies the capture
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
# Exit 0 only when every mode passed all four checks. The verdict line of each mode and a final summary line
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
    wait_for_socket "$mode" || return 1
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
    ! mode_verdict 1 0 0 && ! mode_verdict 0 1 0 && ! mode_verdict 0 0 1 && ! mode_verdict 0 3 1
}

selftest_wait_settings_are_validated() {
    ( valid_wait_settings 30 1 ) && ( valid_wait_settings 1 0 ) &&
        ! ( valid_wait_settings 0 1 ) && ! ( valid_wait_settings abc 1 ) && ! ( valid_wait_settings 1 -1 ) &&
        ! ( valid_wait_settings "" 1 ) && ! ( valid_wait_settings 1 "" )
}

selftest_main() {
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
    echo "selftest: $SELFTEST_CHECKS controles OK"
}

if [ "${1:-}" = "--selftest" ]; then
    selftest_main
else
    main "$@"
fi
