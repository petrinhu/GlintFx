#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# run_check_selftest.sh - the controls of run_check.sh (rev-d3a I-1 and C-1), loaded once with `.` by
# run_capture_known_color_selftest.sh and registered there with selftest_check. A file of its own because run_check.sh
# is shared by the capture drivers: the controls live beside the unit they prove (CAPTURE-SELFTEST-SUBJECTS). No main
# and no dispatch of its own; SCRIPT_DIR is the driver's.

# run_check keeps the caller's -e (rev-d3a I-1): it runs its command with -e off and turns it back on, so after
# run_check a failure still stops a shell with -e. A child shell, because the probe has to die.
selftest_run_check_restores_errexit() {
    root="$(mktemp -d)" || return 1
    out="$(sh -c 'set -e; . "$1"; run_check "$2/rc" false >/dev/null 2>&1; echo depois; false; echo chegou-ao-fim' \
        _ "$SCRIPT_DIR/run_check.sh" "$root" 2>&1)"
    rc=$?
    recorded="$(cat "$root/rc" 2>/dev/null)"
    rm -rf "${root:?}"
    [ "$rc" -ne 0 ] && [ "$out" = "depois" ] && [ "$recorded" = 1 ]
}

# Loading run_check.sh runs nothing (its header, rev-d3a C-1): a shell WITHOUT -e that loads it still goes past a
# failure.
selftest_run_check_loading_changes_nothing() {
    out="$(sh -c '. "$1"; false; echo seguiu' _ "$SCRIPT_DIR/run_check.sh" 2>&1)"
    [ "$out" = "seguiu" ]
}
