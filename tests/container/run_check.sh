#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_check.sh - a POSIX sh LIBRARY of one function, loaded once with `.` by each capture driver that runs a
# command and judges its exit code. run_check runs one command, shows its output and keeps its
# exit code in a FILE (GODS_LAWS.md L-45). It left capture_relay_session.sh, which keeps only the relay session
# (RELAY-SESSION-SUBJECTS, D-W8-136). Its one shell option change: `set +e` around the command, then `set -e`, so a
# caller without -e gains it there. Loading it runs nothing.
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
