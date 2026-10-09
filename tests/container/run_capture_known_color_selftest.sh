#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# run_capture_known_color_selftest.sh - the controls of run_capture_known_color.sh, loaded once with `.` by that
# driver's own --selftest (CAPTURE-DRIVER-SELFTEST-SPLIT, D-W8-137: a driver's controls live beside it, never inside
# it). It has no main and no dispatch of its own; $0 is still the driver.

# -- selftest: the pure decisions, no container, no docker ------------------------------------------------------
SELFTEST_CHECKS=0

# The controls of run_check.sh live beside it (D-W8-157); they are registered in selftest_main below.
# shellcheck source=run_check_selftest.sh
. "$SCRIPT_DIR/run_check_selftest.sh"

# run_control <control>: runs one control in a SUBSHELL, so a stub or a variable it leaves behind dies with it and
# never reaches the next control (rev-d2bfix IMP-2: a convention in a comment protected one control, not the class).
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
    (
        CONTAINER=x
        in_container() { printf 'conn7_surface6.meta\nconn7_surface6.raw\n'; }
        message="$(CAPTURE_WAIT_TRIES=1 CAPTURE_WAIT_SLEEP=0 wait_for_capture_files off 2>&1)" && exit 1
        case "$message" in
            *conn1_*conn7_surface6.meta*) exit 0 ;;
            *) echo "$message" >&2; exit 1 ;;
        esac
    )
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

# The relay's ready line is read from its own log: only the exact line of the mode, whole, counts (RELAY-SOCKET-STALE).
selftest_relay_announced_accepts_exact_line() {
    printf '%s\n' "some noise" "wire_relay: listening on /run/glintfx-test/glintfx-cap-off, upstream /run/glintfx-test/glintfx-test-upstream" "more" |
        relay_announced off &&
        printf '%s\n' "wire_relay: listening on /run/glintfx-test/glintfx-cap-on, upstream /run/glintfx-test/glintfx-test-upstream" |
        relay_announced on
}

selftest_relay_announced_rejects_other_lines() {
    ! printf '' | relay_announced off &&
        ! printf '%s\n' "wire_relay: listening on /run/glintfx-test/glintfx-cap-on, upstream /run/glintfx-test/glintfx-test-upstream" | relay_announced off &&
        ! printf '%s\n' "wire_relay: listening on /run/glintfx-test/glintfx-capx-off, upstream /run/glintfx-test/glintfx-test-upstream" | relay_announced off &&
        ! printf '%s\n' "wire_relay: listening on /run/glintfx-test/glintfx-cap-off2, upstream /run/glintfx-test/glintfx-test-upstream" | relay_announced off &&
        ! printf '%s\n' "wire_relay: listening on /run/glintfx-test/glintfx-cap-off, upstream /run/glintfx-test/glintfx-test-upstream extra" | relay_announced off &&
        ! printf '%s\n' "wire_relay: listening on /run/glintfx-test/glintfx-cap-off, upstream /run/glintfx-test/glintfx-test-upstream" | relay_announced on
}

# A socket file left by an earlier run passes `test -S`; the wait must NOT pass on it. The stub's `test` branch says the
# file exists (that branch is what kills RS1, the old `test -S` wait), and the log names only the OTHER mode: the wait
# has to fail, and its message has to name the mode it waited for.
selftest_wait_rejects_stale_socket_without_announcement() {
    (
        CONTAINER=x
        SOCKET_WAIT_TRIES=1
        SOCKET_WAIT_SLEEP=0
        in_container() {
            case "$1" in
                test) return 0 ;;
                cat) printf '%s\n' "wire_relay: listening on /run/glintfx-test/glintfx-cap-on, upstream /run/glintfx-test/glintfx-test-upstream" ;;
            esac
        }
        message="$(wait_for_socket off 2>&1)" && exit 1
        case "$message" in
            *"glintfx-cap-off,"*) exit 0 ;;
            *) echo "$message" >&2; exit 1 ;;
        esac
    )
}

selftest_wait_accepts_announcement() {
    (
        CONTAINER=x
        SOCKET_WAIT_TRIES=1
        SOCKET_WAIT_SLEEP=0
        in_container() {
            case "$1" in
                cat) printf '%s\n' "wire_relay: listening on /run/glintfx-test/glintfx-cap-off, upstream /run/glintfx-test/glintfx-test-upstream" ;;
            esac
        }
        wait_for_socket off >/dev/null 2>&1
    )
}

selftest_wiring_bad_socket_wait_refused() {
    out="$(SOCKET_WAIT_TRIES=abc sh "$0" 2>&1)"
    rc=$?
    [ "$rc" -eq 2 ] || return 1
    case "$out" in
        *SOCKET_WAIT_TRIES*) return 0 ;;
    esac
    return 1
}

# CONTAINER is needed by every container call: empty, the library must refuse BEFORE docker runs (D-W8-117, I-1).
# The docker stub only echoes: if it is reached, its word shows up in the failure message.
selftest_empty_container_refused() {
    root="$(mktemp -d)" || return 1
    (
        docker() { echo docker-chamado; }
        CONTAINER=""
        OUT_DIR="$root"
        message="$(in_container true 2>&1)" && { echo "$message" >&2; exit 1; }
        case "$message" in *CONTAINER*) ;; *) echo "$message" >&2; exit 1 ;; esac
        message="$(copy_out off 2>&1)" && { echo "$message" >&2; exit 1; }
        case "$message" in *CONTAINER*) exit 0 ;; *) echo "$message" >&2; exit 1 ;; esac
    )
    rc=$?
    rm -rf "${root:?}"
    return "$rc"
}

# OUT_DIR empty: copy_out must refuse before it creates anything. Both stubs only echo, so a call that gets through
# shows its word in the failure message (the real mkdir of "/off/frames" never runs).
selftest_empty_out_dir_refused_by_copy_out() {
    (
        docker() { echo docker-chamado; }
        mkdir() { echo mkdir-chamado; }
        CONTAINER=x
        OUT_DIR=""
        message="$(copy_out off 2>&1)" && { echo "$message" >&2; exit 1; }
        case "$message" in
            *docker-chamado*|*mkdir-chamado*) echo "$message" >&2; exit 1 ;;
            *OUT_DIR*) exit 0 ;;
            *) echo "$message" >&2; exit 1 ;;
        esac
    )
}

# Loading the library without the caller's name must abort, and say so; with the name it must load (the positive case
# proves the refusal is about the missing name and not about a wrong path). A child shell is needed: the driver holds
# RELAY_SESSION_CALLER readonly, so a subshell of the driver cannot unset it.
selftest_library_refuses_load_without_caller() {
    lib="$SCRIPT_DIR/capture_relay_session.sh"
    message="$(env -u RELAY_SESSION_CALLER sh -c '. "$1"' _ "$lib" 2>&1)" && return 1
    case "$message" in
        *"defina RELAY_SESSION_CALLER"*) ;;
        *) return 1 ;;
    esac
    env RELAY_SESSION_CALLER=x sh -c '. "$1"' _ "$lib" >/dev/null 2>&1
}

# The caller's name is readonly in the driver: a subshell that reassigns it must fail.
selftest_caller_name_is_readonly() {
    ! (RELAY_SESSION_CALLER=outro) 2>/dev/null
}

# A relay that has not written its log yet (empty) or whose log does not exist (cat fails): the wait must fail and name
# the mode it waited for, never pass (rev-d2bfix IMP-1: the case right after the relay starts).
selftest_wait_rejects_empty_or_missing_log() {
    (
        CONTAINER=x
        SOCKET_WAIT_TRIES=1
        SOCKET_WAIT_SLEEP=0
        in_container() { return 0; }
        message="$(wait_for_socket off 2>&1)" && exit 1
        case "$message" in *"glintfx-cap-off,"*) ;; *) echo "$message" >&2; exit 1 ;; esac
        in_container() { return 1; }
        message="$(wait_for_socket off 2>&1)" && exit 1
        case "$message" in *"glintfx-cap-off,"*) exit 0 ;; *) echo "$message" >&2; exit 1 ;; esac
    )
}

# CONTAINER empty: both waits must refuse with the guard's OWN message before any docker call, never time out blaming the
# relay or the capture (rev-d2bfix IMP-3). -e is off here, as in one_mode's real context (called on the left of ||), so
# a guard silenced inside $(...) would let the wait run on: that is the defect this control sees.
selftest_waits_refuse_empty_container() {
    (
        set +e
        docker() { echo docker-chamado; }
        CONTAINER=""
        SOCKET_WAIT_TRIES=1
        SOCKET_WAIT_SLEEP=0
        CAPTURE_WAIT_TRIES=1
        CAPTURE_WAIT_SLEEP=0
        message="$(wait_for_socket off 2>&1)" && { echo "$message" >&2; exit 1; }
        case "$message" in
            *docker-chamado*|*"nao anunciou"*) echo "$message" >&2; exit 1 ;;
            *"CONTAINER vazio"*) ;;
            *) echo "$message" >&2; exit 1 ;;
        esac
        message="$(wait_for_capture_files off 2>&1)" && { echo "$message" >&2; exit 1; }
        case "$message" in
            *docker-chamado*|*esperava*) echo "$message" >&2; exit 1 ;;
            *"CONTAINER vazio"*) exit 0 ;;
            *) echo "$message" >&2; exit 1 ;;
        esac
    )
}

# run_control must contain what a control leaves behind: a stub defined inside one run_control never reaches the code
# after it (rev-d2bfix IMP-2). The expected answer is the literal name of the off socket, never read from the code.
selftest_run_control_contains_a_stub() {
    leak_a_stub() { relay_socket_name() { echo stub-vazado; }; }
    run_control leak_a_stub
    [ "$(relay_socket_name off)" = "glintfx-cap-off" ]
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
    selftest_check selftest_relay_announced_accepts_exact_line
    selftest_check selftest_relay_announced_rejects_other_lines
    selftest_check selftest_wait_rejects_stale_socket_without_announcement
    selftest_check selftest_wait_accepts_announcement
    selftest_check selftest_wiring_bad_socket_wait_refused
    selftest_check selftest_empty_container_refused
    selftest_check selftest_empty_out_dir_refused_by_copy_out
    selftest_check selftest_library_refuses_load_without_caller
    selftest_check selftest_caller_name_is_readonly
    selftest_check selftest_wait_rejects_empty_or_missing_log
    selftest_check selftest_waits_refuse_empty_container
    selftest_check selftest_run_control_contains_a_stub
    selftest_check selftest_run_check_restores_errexit
    selftest_check selftest_run_check_loading_changes_nothing
    selftest_gate_fails_a_failing_control
    rm -f "$CALLS_LOG"
    echo "selftest: $SELFTEST_CHECKS controles OK"
}

