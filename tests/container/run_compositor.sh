#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# run_compositor.sh - starts an isolated kwin_wayland compositor. Runs
# ONLY inside the container (GODS_LAWS.md L-09, TEST-WLCONT): it
# creates its OWN XDG_RUNTIME_DIR from scratch and never reads, nor
# expects, anything mounted in from the host. The container that ran
# this script is the whole isolation boundary; there is no host
# fallback path here to accidentally take.
#
# Usage: run_compositor.sh <socket-name>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly RUNTIME_DIR="/run/glintfx-test"

fail() {
    echo "run_compositor.sh: $1" >&2
    exit 1
}

require_socket_name_arg() {
    [ "$#" -eq 1 ] || fail "usage: run_compositor.sh <socket-name>"
}

# chmod 700 on a directory created fresh by this same process, inside
# this same container, is the private-runtime-dir half of the L-09
# isolation proof: nothing from a host XDG_RUNTIME_DIR is ever read
# here, so there is nothing to leak.
create_private_runtime_dir() {
    mkdir -p "$RUNTIME_DIR"
    chmod 700 "$RUNTIME_DIR"
}

export_runtime_env() {
    export XDG_RUNTIME_DIR="$RUNTIME_DIR"
    # GODS_LAWS.md L-05: Linux is Wayland-only in this project. This
    # session type is what this container's own compositor speaks, not
    # a request to fall back to X11.
    export XDG_SESSION_TYPE="wayland"
}

# Backgrounded, NOT `exec`'d into: this used to be
# `exec dbus-run-session -- kwin_wayland ...`, which makes dbus-run-
# session PID 1 of the container. dbus-run-session's own job is to run
# ONE command and exit once that command exits - so the moment the
# compositor died (WL-DISPLAY fatia C's fatal_error_smoke.cpp kills it
# on purpose, from INSIDE this same container, to prove the adapter
# survives a dead connection), PID 1 considered its job done and
# exited too, taking the whole container down with it before the test
# could even run its second half. Measured live, reproduced twice
# (adversarial review, GODS_LAWS.md L-36): `docker inspect` showed
# exit=137 and zero test output. Backgrounding it here decouples the
# compositor's lifetime from PID 1's.
start_compositor() {
    socket_name="$1"
    dbus-run-session -- kwin_wayland --virtual --socket "$socket_name" &
}

# A compositor that never starts has to fail this script LOUDLY
# (GODS_LAWS.md L-40: no silent pass) - `exec`'ing into it used to make
# that failure immediate and free (a bad exec just kills PID 1); now
# that start_compositor() above only backgrounds a job, that same
# fast-failure signal has to be rebuilt on purpose, with a bounded
# wait, instead of leaving every CALLER's own pgrep-polling loop as the
# only thing that will ever notice.
wait_for_compositor_ready() {
    tries=0
    while [ "$tries" -lt 30 ]; do
        pgrep -x kwin_wayland >/dev/null 2>&1 && return 0
        tries=$((tries + 1))
        sleep 1
    done
    fail "kwin_wayland nao subiu em 30s"
}

# PID 1 stays up on its own from here on, independent of whatever
# happens to the compositor backgrounded above - this is the whole
# point of the fix: fatal_error_smoke.cpp (or anything else) can kill
# the compositor process and this container keeps running, instead of
# tearing itself down with it.
stay_up_forever() {
    exec tail -f /dev/null
}

main() {
    require_socket_name_arg "$@"
    create_private_runtime_dir
    export_runtime_env
    start_compositor "$1"
    wait_for_compositor_ready
    stay_up_forever
}

main "$@"
