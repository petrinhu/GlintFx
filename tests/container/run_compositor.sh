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

# `exec` replaces this shell so the compositor becomes the container's
# own foreground process: signals (docker stop) reach it directly, and
# `docker run -d` stays up for exactly as long as kwin_wayland does.
# dbus-run-session wraps it because kwin_wayland expects a session bus
# (GODS_LAWS.md L-09 risk list: "o kwin pedindo seat ou logind dentro
# do container"); dbus-run-session starts a throwaway private bus, not
# the host's.
start_compositor() {
    socket_name="$1"
    exec dbus-run-session -- kwin_wayland --virtual --socket "$socket_name"
}

main() {
    require_socket_name_arg "$@"
    create_private_runtime_dir
    export_runtime_env
    start_compositor "$1"
}

main "$@"
