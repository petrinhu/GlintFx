#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# run_example_capture_selftest.sh - DEMO-1 D3 (D-W8-138 of /var/tmp/cto-w8/plano-d3.md): the controls of
# run_example_capture.sh, loaded once with `.` by that driver's own --selftest (CAPTURE-DRIVER-SELFTEST-SPLIT, D-W8-137:
# a driver's controls live beside it, never inside it). It has no main and no dispatch of its own.
#
# No container and no docker daemon: the selftest puts DOUBLES of `docker` and of the example on PATH, and runs the REAL
# exec_fixture.sh, the REAL `timeout`, and the REAL tests/tools/raw_to_png.py and image_probe.py. The container
# functions of the relay session are stubbed in the wiring controls; they are proved by the real run in the CI (D6-red),
# never by these stubs. Expected values are LITERALS written here from the decisions (D-W8-72, D-W8-79, D-W8-81,
# D-W8-90).

SELFTEST_CHECKS=0

# run_control <control>: runs one control in a SUBSHELL, so a stub or a variable it leaves behind dies with it.
run_control() {
    ( "$@" )
}

selftest_check() {
    SELFTEST_CHECKS=$((SELFTEST_CHECKS + 1))
    if ! run_control "$@"; then
        echo "selftest: controle $SELFTEST_CHECKS FALHOU: $*" >&2
        exit 1
    fi
}

# -- doubles on PATH ---------------------------------------------------------------------------------------------------

# write_docker_double <bin-dir> <argv-log>: a `docker` that records its whole command line, then runs, HERE, what
# `docker exec [-e X ...] <container>` would run in the container.
write_docker_double() {
    cat >"$1/docker" <<EOF
#!/bin/sh
printf '%s\n' "\$*" >>"$2"
shift
while [ "\$1" = "-e" ]; do shift 2; done
shift
exec "\$@"
EOF
    chmod +x "$1/docker"
}

# write_example_double <bin-dir> <body>: a `first_window` whose body is the given sh text.
write_example_double() {
    printf '#!/bin/sh\n%s\n' "$2" >"$1/first_window"
    chmod +x "$1/first_window"
}

# with_doubles <root>: PATH gets <root>/bin first, and exec_fixture.sh appends its log under <root> (never in the
# working directory of the test run).
with_doubles() {
    mkdir -p "$1/bin"
    PATH="$1/bin:$PATH"
    GLINTFX_MEASURED_LOG="$1/measured.log"
    export PATH GLINTFX_MEASURED_LOG
}

# -- the frame and the probe files -------------------------------------------------------------------------------------

# write_good_frame <frames-dir>: a 2x2 ARGB8888 frame as the relay writes it (B, G, R, A in memory; pixels (R, G, B)
# (1,2,3) (5,6,7) / (9,10,11) (13,14,15), alpha 255), plus the no-frame marker of the second connection.
write_good_frame() {
    mkdir -p "$1"
    printf '\003\002\001\377\007\006\005\377\013\012\011\377\017\016\015\377' >"$1/conn1_surface1.raw"
    printf 'width=2\nheight=2\nstride=8\nformat=0\n' >"$1/conn1_surface1.meta"
    printf 'nenhum quadro\n' >"$1/conn2_no_frame.txt"
}

# write_probes <file> <count> <line>: <count> copies of one probe line.
write_probes() {
    : >"$1"
    i=0
    while [ "$i" -lt "$2" ]; do
        printf '%s\n' "$3" >>"$1"
        i=$((i + 1))
    done
}

# judge_setup: a fresh JUDGE_ROOT with the good frame under OUT_DIR/first_window/frames, a static file of 12 good probes
# and a frames30 file of 5 good probes.
judge_setup() {
    JUDGE_ROOT="$(mktemp -d)"
    OUT_DIR="$JUDGE_ROOT/out"
    write_good_frame "$OUT_DIR/first_window/frames"
    write_probes "$JUDGE_ROOT/static.txt" 12 "0 0 010203FF"
    write_probes "$JUDGE_ROOT/frames30.txt" 5 "1 1 0D0E0FFF"
}

# judge_on <size>: judge_example over JUDGE_ROOT's files, quiet; removes JUDGE_ROOT and returns the judge's status.
judge_on() {
    judge_example "$JUDGE_ROOT/static.txt" "$JUDGE_ROOT/frames30.txt" "$1" >/dev/null 2>&1
    rc=$?
    rm -rf "${JUDGE_ROOT:?}"
    return "$rc"
}

# -- the exit code -----------------------------------------------------------------------------------------------------

selftest_overrun_is_exit_3() {
    [ "$(example_exit_code 124 0 0)" = 3 ] && [ "$(example_exit_code 124 1 1)" = 3 ]
}

# 137 is timeout -k's KILL, a death by signal that exec_fixture.sh reports: exit 1, never 3 (plano-d3.md:28).
selftest_exit_0_needs_every_part_clean() {
    [ "$(example_exit_code 0 0 0)" = 0 ] && [ "$(example_exit_code 1 0 0)" = 1 ] &&
        [ "$(example_exit_code 139 0 0)" = 1 ] && [ "$(example_exit_code 137 0 0)" = 1 ] &&
        [ "$(example_exit_code 0 1 0)" = 1 ] && [ "$(example_exit_code 0 0 1)" = 1 ]
}

selftest_decided_name_frames_and_budget() {
    [ "$EXAMPLE_NAME" = first_window ] && [ "$EXAMPLE_FRAMES" = 30 ] && [ "$EXAMPLE_BUDGET_SECONDS" = 60 ]
}

# -- the example's command, through the REAL exec_fixture.sh and timeout -----------------------------------------------

# The docker double records the command line exec_fixture.sh built; the example double records its own arguments.
selftest_example_command_reaches_the_container() {
    root="$(mktemp -d)" || return 1
    with_doubles "$root"
    write_docker_double "$root/bin" "$root/docker.argv"
    write_example_double "$root/bin" "printf '%s\\n' \"\$*\" >'$root/example.argv'; echo 'first_window: duble rodou'"
    CONTAINER=glintfx-teste
    run_example 60 >/dev/null 2>&1
    rc=$?
    argv="$(cat "$root/docker.argv" 2>/dev/null)"
    example_argv="$(cat "$root/example.argv" 2>/dev/null)"
    rm -rf "${root:?}"
    [ "$rc" -eq 0 ] || return 1
    case "$argv" in
        *" -e WAYLAND_DISPLAY=glintfx-cap-first_window glintfx-teste timeout -k 5 60 first_window --frames 30") ;;
        *) echo "$argv" >&2; return 1 ;;
    esac
    [ "$example_argv" = "--frames 30" ]
}

# -- the judge, with the REAL raw_to_png.py and image_probe.py ---------------------------------------------------------

# The positive control comes first: a good frame must PASS, or every negative case below would pass on a broken judge.
selftest_judge_passes_a_good_frame() {
    judge_setup
    judge_on 2x2
}

selftest_judge_rejects_zero_images() {
    judge_setup
    rm -f "$OUT_DIR/first_window/frames/conn1_surface1.raw" "$OUT_DIR/first_window/frames/conn1_surface1.meta"
    ! judge_on 2x2
}

selftest_judge_rejects_two_images() {
    judge_setup
    cp "$OUT_DIR/first_window/frames/conn1_surface1.raw" "$OUT_DIR/first_window/frames/conn1_surface2.raw"
    cp "$OUT_DIR/first_window/frames/conn1_surface1.meta" "$OUT_DIR/first_window/frames/conn1_surface2.meta"
    ! judge_on 2x2
}

# A 2x2 capture judged against the decided 640x480 is refused BEFORE any probe: no probe rc file is ever written.
selftest_judge_rejects_wrong_size_before_probes() {
    judge_setup
    static_rc="$OUT_DIR/first_window/static.rc"
    judge_example "$JUDGE_ROOT/static.txt" "$JUDGE_ROOT/frames30.txt" 640x480 >/dev/null 2>&1 && return 1
    [ ! -e "$static_rc" ]
    rc=$?
    rm -rf "${JUDGE_ROOT:?}"
    return "$rc"
}

selftest_judge_rejects_missing_probe_file() {
    judge_setup
    rm -f "$JUDGE_ROOT/static.txt"
    ! judge_on 2x2
}

selftest_judge_rejects_empty_probe_file() {
    judge_setup
    write_probes "$JUDGE_ROOT/static.txt" 0 "unused"
    ! judge_on 2x2
}

selftest_judge_rejects_a_wrong_probe_count() {
    judge_setup
    write_probes "$JUDGE_ROOT/static.txt" 11 "0 0 010203FF"
    ! judge_on 2x2
}

selftest_judge_rejects_a_failed_probe() {
    judge_setup
    write_probes "$JUDGE_ROOT/frames30.txt" 5 "1 1 0D0E10FF"
    ! judge_on 2x2
}

# -- the wiring of capture_example -------------------------------------------------------------------------------------

# capture_example_with <budget> <socket-rc> <wait-rc> <judge-rc>: capture_example <budget> with the container
# collaborators replaced by stubs that record "<function> <args>" in CALLS_LOG. run_example is stubbed too (it records
# and returns STUB_EXAMPLE_RC, default 0), unless STUB_EXAMPLE_RC is "real". OUT_DIR is the caller's.
capture_example_with() {
    : >"$CALLS_LOG"
    (
        STUB_SOCKET_RC="$2"
        STUB_WAIT_RC="$3"
        STUB_JUDGE_RC="$4"
        start_capture_relay() { echo "start_capture_relay $1" >>"$CALLS_LOG"; }
        wait_for_socket() { echo "wait_for_socket $1" >>"$CALLS_LOG"; return "$STUB_SOCKET_RC"; }
        wait_for_capture_files() { echo "wait_for_capture_files $1" >>"$CALLS_LOG"; return "$STUB_WAIT_RC"; }
        stop_capture_relay() { echo "stop_capture_relay $1" >>"$CALLS_LOG"; }
        copy_out() { echo "copy_out $1" >>"$CALLS_LOG"; }
        judge_example() { echo "judge_example $*" >>"$CALLS_LOG"; return "$STUB_JUDGE_RC"; }
        if [ "${STUB_EXAMPLE_RC:-0}" != real ]; then
            run_example() {
                echo "run_example $1" >>"$CALLS_LOG"
                echo "duble do exemplo"
                return "${STUB_EXAMPLE_RC:-0}"
            }
        fi
        capture_example "$1"
    )
}

# The overrun, for real: the example double sleeps, the budget is 1 s, timeout kills it (124), and the driver exits 3
# with the budget's own message (D-W8-81, V-15).
selftest_overrun_exits_3_with_the_budget_message() {
    root="$(mktemp -d)" || return 1
    with_doubles "$root"
    write_docker_double "$root/bin" "$root/docker.argv"
    write_example_double "$root/bin" "echo 'first_window: duble dormindo'; exec sleep 30"
    # shellcheck disable=SC2034 # read by the REAL run_example, reached through capture_example_with
    CONTAINER=glintfx-teste
    OUT_DIR="$root/out"
    mkdir -p "$OUT_DIR"
    out="$(STUB_EXAMPLE_RC=real capture_example_with 1 0 0 0 2>&1)"
    rc=$?
    rm -rf "${root:?}"
    [ "$rc" -eq 3 ] || { echo "$out" >&2; return 1; }
    case "$out" in
        *"exemplo nao terminou em 1 s (--frames 30)"*) return 0 ;;
    esac
    echo "$out" >&2
    return 1
}

# expected_clean_calls: the calls of a clean run, in the driver's order, all with the run name first_window. The judge
# gets the two decided probe files and the decided size.
expected_clean_calls() {
    fixtures="$REPO_ROOT/tests/fixtures"
    printf '%s\n' "start_capture_relay first_window" "wait_for_socket first_window" "run_example 60" \
        "wait_for_capture_files first_window" "stop_capture_relay first_window" "copy_out first_window" \
        "judge_example $fixtures/first_window_probes.txt $fixtures/first_window_probes_frames30.txt 640x480"
}

selftest_clean_run_calls_in_order_and_passes() {
    OUT_DIR="$(mktemp -d)" || return 1
    out="$(capture_example_with 60 0 0 0)"
    rc=$?
    calls="$(cat "$CALLS_LOG")"
    rm -rf "${OUT_DIR:?}"
    [ "$rc" -eq 0 ] && [ "$calls" = "$(expected_clean_calls)" ] &&
        [ "$out" = "run_example_capture: exemplo=first_window frames=30 exemplo_rc=0 espera=0 julgamento=0 codigo=0" ]
}

# A relay that never announced: the example never runs; the relay is stopped and the log copied out, in that order.
selftest_socket_failure_skips_the_example() {
    OUT_DIR="$(mktemp -d)" || return 1
    out="$(capture_example_with 60 1 0 0)"
    rc=$?
    calls="$(cat "$CALLS_LOG")"
    rm -rf "${OUT_DIR:?}"
    [ "$rc" -eq 1 ] &&
        [ "$calls" = "$(printf '%s\n' "start_capture_relay first_window" "wait_for_socket first_window" \
            "stop_capture_relay first_window" "copy_out first_window")" ] &&
        [ "$out" = "run_example_capture: exemplo=first_window exemplo_rc=nao-rodou espera=socket-falhou codigo=1" ]
}

selftest_failed_example_fails_the_run() {
    OUT_DIR="$(mktemp -d)" || return 1
    out="$(STUB_EXAMPLE_RC=1 capture_example_with 60 0 0 0)"
    rc=$?
    rm -rf "${OUT_DIR:?}"
    [ "$rc" -eq 1 ] &&
        [ "$out" = "run_example_capture: exemplo=first_window frames=30 exemplo_rc=1 espera=0 julgamento=0 codigo=1" ]
}

# A reused out directory would hand the judge the previous run's frames: first_window/ is removed first, nothing else.
selftest_stale_output_is_removed_first() {
    OUT_DIR="$(mktemp -d)" || return 1
    mkdir -p "$OUT_DIR/first_window/frames"
    echo stale >"$OUT_DIR/first_window/frames/conn1_surface9.meta"
    echo keep >"$OUT_DIR/outro.keep"
    capture_example_with 60 0 0 0 >/dev/null
    [ ! -e "$OUT_DIR/first_window/frames/conn1_surface9.meta" ] && [ -e "$OUT_DIR/outro.keep" ]
    rc=$?
    rm -rf "${OUT_DIR:?}"
    return "$rc"
}

# The script refuses a bad command line with its usage line and rc 2 (no arguments: a child started with --selftest
# would run this selftest again and spawn itself without end).
selftest_no_arguments_is_usage() {
    out="$(sh "$0" 2>&1)"
    rc=$?
    [ "$rc" -eq 2 ] || return 1
    case "$out" in
        *"uso: run_example_capture.sh"*) return 0 ;;
    esac
    return 1
}

# run_control must contain what a control leaves behind: a stub defined inside one run_control never reaches the code
# after it (the class of rev-d2bfix IMP-2). The expected answer is the literal name of the socket, never read from code.
selftest_run_control_contains_a_stub() {
    leak_a_stub() { relay_socket_name() { echo stub-vazado; }; }
    run_control leak_a_stub
    [ "$(relay_socket_name first_window)" = "glintfx-cap-first_window" ]
}

# fake_failing_control and fake_passing_control: the two controls the gate is checked with (no selftest_ prefix, so
# they are never counted among the real ones).
fake_failing_control() { return 1; }
fake_passing_control() { return 0; }

# selftest_gate_fails_a_failing_control: the gate itself, checked OUTSIDE selftest_check, because a blind gate cannot
# vouch for itself (rev-d2bfix2 IMP-A): a failing control must make selftest_check exit non-zero and name it, and a
# passing one must not. It counts as one control.
selftest_gate_fails_a_failing_control() {
    SELFTEST_CHECKS=$((SELFTEST_CHECKS + 1))
    if out="$( (selftest_check fake_failing_control) 2>&1)"; then
        echo "selftest: controle $SELFTEST_CHECKS FALHOU: o portao aprovou um controle que falha" >&2
        exit 1
    fi
    case "$out" in
        *"FALHOU: fake_failing_control"*) ;;
        *) echo "selftest: controle $SELFTEST_CHECKS FALHOU: o portao nao nomeou o controle que falhou" >&2; exit 1 ;;
    esac
    if ! (selftest_check fake_passing_control) >/dev/null 2>&1; then
        echo "selftest: controle $SELFTEST_CHECKS FALHOU: o portao reprovou um controle que passa" >&2
        exit 1
    fi
}

selftest_main() {
    # selftest_main runs once per process (rev-d3a C-5): a second dispatch would run every control twice, and the
    # executor, which wants one exact OK line, would not see it. readonly, so no later line can disarm it
    # (rev-d2bfix3-d3afix I-4): a reset aborts the shell.
    [ -z "${SELFTEST_MAIN_STARTED:-}" ] || { echo "selftest: selftest_main chamado duas vezes" >&2; exit 1; }
    readonly SELFTEST_MAIN_STARTED=1
    CALLS_LOG="${TMPDIR:-/tmp}/run_example_capture.selftest.calls.$$"
    selftest_check selftest_overrun_is_exit_3
    selftest_check selftest_exit_0_needs_every_part_clean
    selftest_check selftest_decided_name_frames_and_budget
    selftest_check selftest_example_command_reaches_the_container
    selftest_check selftest_judge_passes_a_good_frame
    selftest_check selftest_judge_rejects_zero_images
    selftest_check selftest_judge_rejects_two_images
    selftest_check selftest_judge_rejects_wrong_size_before_probes
    selftest_check selftest_judge_rejects_missing_probe_file
    selftest_check selftest_judge_rejects_empty_probe_file
    selftest_check selftest_judge_rejects_a_wrong_probe_count
    selftest_check selftest_judge_rejects_a_failed_probe
    selftest_check selftest_overrun_exits_3_with_the_budget_message
    selftest_check selftest_clean_run_calls_in_order_and_passes
    selftest_check selftest_socket_failure_skips_the_example
    selftest_check selftest_failed_example_fails_the_run
    selftest_check selftest_stale_output_is_removed_first
    selftest_check selftest_no_arguments_is_usage
    selftest_check selftest_run_control_contains_a_stub
    selftest_gate_fails_a_failing_control
    rm -f "$CALLS_LOG"
    echo "selftest: $SELFTEST_CHECKS controles OK"
}
