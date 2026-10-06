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
readonly CAPTURE_WAIT_TRIES=30

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

# The relay writes the capture when the client's connection closes, a moment AFTER the fixture exits; the
# .meta is written after its .raw, so a .meta (or the no-frame marker) means the files are complete.
wait_for_capture_files() {
    mode="$1"
    tries=0
    while [ "$tries" -lt "$CAPTURE_WAIT_TRIES" ]; do
        # shellcheck disable=SC2016
        found="$(in_container sh -c 'ls "$1" | grep -c -E "[.]meta$|_no_frame[.]txt$" || true' _ "$CAPTURE_ROOT/$mode/frames")"
        if [ "${found:-0}" -ge 1 ]; then
            return 0
        fi
        tries=$((tries + 1))
        sleep 1
    done
    echo "run_capture_known_color: FAIL nenhum arquivo de captura ($mode) em ${CAPTURE_WAIT_TRIES}s" >&2
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

# one_mode <mode>: returns 0 only when the fixture and the three checks passed.
one_mode() {
    mode="$1"
    start_capture_relay "$mode"
    wait_for_socket "$mode" || return 1
    fixture_rc="$(run_check "$OUT_DIR/$mode.fixture.rc" run_fixture "$mode")"
    wait_for_capture_files "$mode" || true
    stop_capture_relay "$mode"
    copy_out "$mode"
    echo "run_capture_known_color: mode=$mode fixture=$fixture_rc"
    judge_mode "$mode" && [ "$fixture_rc" -eq 0 ]
}

main() {
    parse_args "$@"
    mkdir -p "$OUT_DIR"
    modes_run=0
    modes_failed=0
    for mode in off on; do
        modes_run=$((modes_run + 1))
        one_mode "$mode" || modes_failed=$((modes_failed + 1))
    done
    echo "run_capture_known_color: modos=$modes_run reprovados=$modes_failed sabotagem=${SABOTAGE:-nenhuma}"
    [ "$modes_run" -eq 2 ] && [ "$modes_failed" -eq 0 ]
}

main "$@"
