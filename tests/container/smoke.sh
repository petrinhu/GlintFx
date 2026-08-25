#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# smoke.sh - proves the compositor inside this container actually
# speaks Wayland (GODS_LAWS.md L-09, TEST-WLCONT). Runs INSIDE the
# container, after check_isolation.sh has already proven the boundary
# (L-09 rule 5: prove isolation before interacting) - this script is
# the interaction.
#
# Usage: smoke.sh <socket-name>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

readonly RUNTIME_DIR="/run/glintfx-test"

fail() {
    echo "smoke.sh: $1" >&2
    exit 1
}

require_socket_name_arg() {
    [ "$#" -eq 1 ] || fail "usage: smoke.sh <socket-name>"
}

wayland_info_output() {
    socket_name="$1"
    XDG_RUNTIME_DIR="$RUNTIME_DIR" WAYLAND_DISPLAY="$socket_name" wayland-info
}

# Matches the "interface: 'name',   version:  N, name:  N" line
# wayland-info prints per global - verified against the real binary
# output inside this project's own container, not guessed from the
# man page (the man page documents no exact format; the first attempt
# at this regex, missing the comma right after the closing quote,
# failed against a compositor that was genuinely up and correct).
# Anchored on the quotes and the trailing comma so a global whose name
# merely CONTAINS the searched string, e.g. a hypothetical
# wl_compositor_ext, can never satisfy a search for wl_compositor.
require_global() {
    output="$1"
    global="$2"
    printf '%s\n' "$output" | grep -qE "interface: *'${global}',[[:space:]]+version:" \
        || fail "global ausente: $global"
}

main() {
    require_socket_name_arg "$@"
    output="$(wayland_info_output "$1")"
    [ -n "$output" ] || fail "wayland-info nao imprimiu nada (varredura vazia)"
    require_global "$output" "wl_compositor"
    require_global "$output" "xdg_wm_base"
    echo "smoke.sh: ok - wl_compositor e xdg_wm_base presentes"
}

main "$@"
