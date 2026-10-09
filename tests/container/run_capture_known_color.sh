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
#        run_capture_known_color.sh --selftest   (no container: the controls in run_capture_known_color_selftest.sh)
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
readonly TOOLS_DIR="$REPO_ROOT/tests/tools"
# shellcheck disable=SC2034 # read by capture_relay_session.sh, loaded on the next lines: the name of this caller.
readonly RELAY_SESSION_CALLER="run_capture_known_color"

# The relay session (wait settings, container and relay helpers, copy-out and run_check) lives in the library.
# shellcheck source=capture_relay_session.sh
. "$SCRIPT_DIR/capture_relay_session.sh"
# shellcheck source=run_check.sh
. "$SCRIPT_DIR/run_check.sh"

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

# run_fixture <mode>: through exec_fixture.sh (the verdict line first, rc from a file). WAYLAND_DISPLAY is
# overridden AFTER the one exec_fixture.sh always sets, so the fixture connects to the capture relay.
run_fixture() {
    mode="$1"
    set --
    [ -z "$SABOTAGE" ] || set -- "$SABOTAGE"
    # shellcheck disable=SC2086
    "$SCRIPT_DIR/exec_fixture.sh" $ENV_ARGS --env "WAYLAND_DISPLAY=$(relay_socket_name "$mode")" \
        "$CONTAINER" capture_known_color_smoke "$mode" "$CAPTURE_ROOT/$mode/readback" "$@"
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

if [ "${1:-}" = "--selftest" ]; then
    # shellcheck source=run_capture_known_color_selftest.sh
    . "$SCRIPT_DIR/run_capture_known_color_selftest.sh"
    selftest_main
else
    main "$@"
fi
