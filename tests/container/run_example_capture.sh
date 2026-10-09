#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# run_example_capture.sh - DEMO-1 D3 (D-W8-74, D-W8-81, D-W8-90 of /var/tmp/cto-w8/plano-demo1-v2.md; D-W8-138 of
# /var/tmp/cto-w8/plano-d3.md): the DRIVER of the Linux capture of the example first_window.
#
# Inside the already-running test container it starts a capture relay (capture_relay_session.sh, the run named
# first_window), runs the INSTALLED example `first_window --frames 30` through it under `timeout` (an example that never
# exits is killed and reported, D-W8-81), copies the capture out with `docker cp`, and judges on the host:
#   1. the example's exit code (0; timeout's 124 is the overrun, the driver's exit 3);
#   2. tests/tools/raw_to_png.py: exactly ONE image, and its size is 640x480 (D-W8-72) BEFORE any probe;
#   3. tests/tools/image_probe.py over that image, twice: the 12 static probes (tests/fixtures/first_window_probes.txt)
#      and the 5 edge probes of the 30th drawn frame (tests/fixtures/first_window_probes_frames30.txt), each with
#      exactly that many probes reported and none failed.
# Output under <out-directory>/first_window/ (D-W8-90). The verdict line is always printed.
#
# USAGE: run_example_capture.sh [--env NAME=VALUE ...] <container> <out-directory>
#        run_example_capture.sh --selftest   (no container: run_example_capture_selftest.sh, loaded below)
# --env is passed to exec_fixture.sh (the asan leg's runtime options). EXIT: 0 passed; 3 the example did not exit within
# its budget; 1 any other failure; 2 bad usage.
#
# WHAT THIS DOES NOT PROVE (declared, L-43): the composition KWin does after the commit (the capture is the buffer the
# client hands over, D-W8-20); the Windows capture (run_example_capture_win32.py); another Mesa than the image's.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
readonly SCRIPT_DIR REPO_ROOT
readonly TOOLS_DIR="$REPO_ROOT/tests/tools"
# shellcheck disable=SC2034 # read by capture_relay_session.sh, loaded on the next lines: the name of this caller.
readonly RELAY_SESSION_CALLER="run_example_capture"

# shellcheck source=capture_relay_session.sh
. "$SCRIPT_DIR/capture_relay_session.sh"
# shellcheck source=run_check.sh
. "$SCRIPT_DIR/run_check.sh"

# The run's name: the relay socket glintfx-cap-first_window, the binary in the image, the output <out>/first_window.
readonly EXAMPLE_NAME="first_window"
readonly EXAMPLE_FRAMES=30             # D-W8-79: the 30th drawn frame is the one the edge probes read
readonly EXAMPLE_BUDGET_SECONDS=60     # D-W8-81, fixed before any measurement
readonly EXAMPLE_KILL_GRACE_SECONDS=5  # timeout -k: an example that ignores TERM is killed 5 s later
readonly EXIT_OVERRUN=3
readonly CAPTURE_SIZE="640x480"
readonly STATIC_PROBE_COUNT=12
readonly FRAMES_PROBE_COUNT=5
readonly STATIC_PROBES_FILE="$REPO_ROOT/tests/fixtures/first_window_probes.txt"
readonly FRAMES_PROBES_FILE="$REPO_ROOT/tests/fixtures/first_window_probes_frames30.txt"

fail_usage() {
    echo "uso: run_example_capture.sh [--env NOME=VALOR ...] <container> <diretorio-de-saida>" >&2
    exit 2
}

# Words of the --env options, kept as one string of "--env X" pairs (POSIX sh has no arrays); values never contain
# spaces (ASAN_OPTIONS=a=1:b=2 style).
ENV_ARGS=""

parse_args() {
    while [ "$#" -gt 0 ]; do
        case "$1" in
            --env) [ "$#" -ge 2 ] || fail_usage; ENV_ARGS="$ENV_ARGS --env $2"; shift 2 ;;
            -*) fail_usage ;;
            *) break ;;
        esac
    done
    [ "$#" -eq 2 ] || fail_usage
    CONTAINER="$1"
    OUT_DIR="$2"
}

# run_example <budget-seconds>: the example through exec_fixture.sh (the verdict line first, rc from a file), under
# timeout INSIDE the container. WAYLAND_DISPLAY is overridden AFTER the one exec_fixture.sh always sets, so the example
# connects to the capture relay.
run_example() {
    # shellcheck disable=SC2086 # ENV_ARGS is a list of "--env X" words, none with a space (parse_args)
    "$SCRIPT_DIR/exec_fixture.sh" $ENV_ARGS --env "WAYLAND_DISPLAY=$(relay_socket_name "$EXAMPLE_NAME")" \
        "$CONTAINER" timeout -k "$EXAMPLE_KILL_GRACE_SECONDS" "$1" "$EXAMPLE_NAME" --frames "$EXAMPLE_FRAMES"
}

# example_exit_code <example-rc> <waited> <judged>: the driver's exit code. 3 when the example overran its budget
# (timeout's 124), whatever else happened; 0 only when the example exited 0, the capture was waited for and the judge
# passed; 1 otherwise.
example_exit_code() {
    if [ "$1" -eq 124 ]; then
        echo "$EXIT_OVERRUN"
        return 0
    fi
    if [ "$1" -eq 0 ] && [ "$2" -eq 0 ] && [ "$3" -eq 0 ]; then
        echo 0
        return 0
    fi
    echo 1
}

# probe_png <png> <probe-file> <count> <rc-file>: image_probe.py over the PNG; 0 only when it exits 0 AND reports
# exactly <count> probes, none failed (the counts are fixed by D-W8-72: a shorter or longer file is a failure).
probe_png() {
    probe_rc="$(run_check "$4" python3 "$TOOLS_DIR/image_probe.py" "$1" "$2")"
    [ "$probe_rc" -eq 0 ] && grep -q -x "image_probe.py: probes=$3 failed=0" "$4.log"
}

# judge_example <static-probe-file> <frames30-probe-file> <size>: the host-side judge over what docker cp brought out.
# One image, of <size>, before any probe; then both probe files. Prints one line.
judge_example() {
    dir="$OUT_DIR/$EXAMPLE_NAME"
    png_rc="$(run_check "$dir/png.rc" python3 "$TOOLS_DIR/raw_to_png.py" "$dir/frames" "$dir/png")"
    images="$(grep -c '^png ' "$dir/png.rc.log" || true)"
    size="$(sed -n 's/^png .* \([0-9][0-9]*x[0-9][0-9]*\)$/\1/p' "$dir/png.rc.log" | head -n 1)"
    echo "run_example_capture: raw_to_png=$png_rc imagens=$images tamanho=${size:-nenhum} esperado=$3"
    if [ "$png_rc" -ne 0 ] || [ "$images" -ne 1 ] || [ "$size" != "$3" ]; then
        return 1
    fi
    png="$dir/png/$(sed -n 's/^png \([^ ]*\) .*/\1/p' "$dir/png.rc.log" | head -n 1)"
    static_ok=0
    probe_png "$png" "$1" "$STATIC_PROBE_COUNT" "$dir/static.rc" || static_ok=1
    frames_ok=0
    probe_png "$png" "$2" "$FRAMES_PROBE_COUNT" "$dir/frames30.rc" || frames_ok=1
    echo "run_example_capture: sondas_estaticas=$static_ok sondas_frames30=$frames_ok"
    [ "$static_ok" -eq 0 ] && [ "$frames_ok" -eq 0 ]
}

# report_overrun <example-rc> <budget-seconds>: the overrun's own message (D-W8-81), only for timeout's 124.
report_overrun() {
    [ "$1" -ne 124 ] || echo "run_example_capture: FAIL exemplo nao terminou em $2 s (--frames $EXAMPLE_FRAMES)" >&2
}

# capture_example <budget-seconds>: one capture of the example. Prints the verdict line and returns the driver's exit
# code (example_exit_code). A relay that never announced: the example never runs, the relay is stopped and the log
# copied out, in that order, so no relay is left in the container.
capture_example() {
    budget="$1"
    clean_mode_output "$EXAMPLE_NAME"
    start_capture_relay "$EXAMPLE_NAME"
    if ! wait_for_socket "$EXAMPLE_NAME"; then
        stop_capture_relay "$EXAMPLE_NAME"
        copy_out "$EXAMPLE_NAME"
        echo "run_example_capture: exemplo=$EXAMPLE_NAME exemplo_rc=nao-rodou espera=socket-falhou codigo=1"
        return 1
    fi
    example_rc="$(run_check "$OUT_DIR/$EXAMPLE_NAME.example.rc" run_example "$budget")"
    waited=0
    wait_for_capture_files "$EXAMPLE_NAME" || waited=1
    stop_capture_relay "$EXAMPLE_NAME"
    copy_out "$EXAMPLE_NAME"
    judged=0
    judge_example "$STATIC_PROBES_FILE" "$FRAMES_PROBES_FILE" "$CAPTURE_SIZE" || judged=1
    code="$(example_exit_code "$example_rc" "$waited" "$judged")"
    report_overrun "$example_rc" "$budget"
    echo "run_example_capture: exemplo=$EXAMPLE_NAME frames=$EXAMPLE_FRAMES exemplo_rc=$example_rc espera=$waited" \
        "julgamento=$judged codigo=$code"
    return "$code"
}

main() {
    parse_args "$@"
    mkdir -p "$OUT_DIR"
    code=0
    capture_example "$EXAMPLE_BUDGET_SECONDS" || code=$?
    exit "$code"
}

if [ "${1:-}" = "--selftest" ]; then
    # shellcheck source=run_example_capture_selftest.sh
    . "$SCRIPT_DIR/run_example_capture_selftest.sh"
    selftest_main
else
    main "$@"
fi
