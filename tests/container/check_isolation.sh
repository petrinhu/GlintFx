#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_isolation.sh - proves, BEFORE any interaction with the
# compositor, that a running glintfx test container cannot reach the
# leader's real session (GODS_LAWS.md L-09 rule 5, TEST-WLCONT). Runs
# on the HOST: it drives `docker inspect`/`docker exec`, which do not
# exist inside the container image itself (Containerfile never copies
# this script in).
#
# Two checks, both mandatory:
#   (a) the container's own mount table carries none of the four
#       paths that would puncture the boundary (GODS_LAWS.md L-09):
#       the host XDG_RUNTIME_DIR family, the host's wayland-0 socket,
#       /dev/uinput (kernel-level input injection - it does not stay
#       inside any container, it lands in the leader's real session),
#       and any X11 socket directory (L-05: this project has no X11
#       backend at all).
#   (b) the compositor process's own open sockets, read from ss/lsof
#       INSIDE the container, show the internal test socket and none
#       of the same four host paths.
#
# Usage: check_isolation.sh <container-name> <socket-name>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# Four host paths a leak would show up as (GODS_LAWS.md L-09). Kept as
# a single space-separated constant, never computed, so the loops
# below always iterate a known non-empty list - the leader-ordered
# floor against a silently-empty scan reporting a green it never
# earned (GODS_LAWS.md L-23 portao pattern, tools/preci.sh
# require_nonempty).
readonly FORBIDDEN_PATH_PATTERNS="/run/user/ wayland-0 /dev/uinput /tmp/.X11-unix"

fail() {
    echo "check_isolation.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 2 ] || fail "usage: check_isolation.sh <container-name> <socket-name>"
}

# `docker inspect` failing outright (typo'd container name, daemon
# down) must never be read as "zero forbidden mounts, pass": that is
# exactly the disguised-empty-scan shape the leader ordered a floor
# against. A real empty mount table still prints the two characters
# "[]", which satisfies this check and is the expected clean case.
mounts_json_or_fail() {
    container="$1"
    json="$(docker inspect --format '{{json .Mounts}}' "$container" 2>&1)" \
        || fail "docker inspect falhou para '$container': $json"
    [ -n "$json" ] || fail "docker inspect nao retornou nada para '$container' (varredura vazia disfarcada de OK)"
    printf '%s' "$json"
}

assert_mounts_have_no_forbidden_pattern() {
    mounts_json="$1"
    for pattern in $FORBIDDEN_PATH_PATTERNS; do
        case "$mounts_json" in
            *"$pattern"*)
                fail "montagem proibida encontrada (padrao '$pattern') em: $mounts_json"
                ;;
        esac
    done
}

assert_no_forbidden_mounts() {
    container="$1"
    mounts_json="$(mounts_json_or_fail "$container")"
    assert_mounts_have_no_forbidden_pattern "$mounts_json"
    echo "check_isolation.sh: mounts ok (nenhum dos 4 padroes proibidos em: $mounts_json)"
}

compositor_pid_in_container() {
    container="$1"
    # `-x` matches the exact process name (comm), not `-f` (full
    # command line): verified live that `pgrep -f kwin_wayland` also
    # matches PID 1, the `dbus-run-session -- kwin_wayland ...`
    # wrapper, whose command line merely MENTIONS kwin_wayland as an
    # argument - `head -n 1` would then have audited the wrapper
    # instead of the compositor itself. A substring match against the
    # wrong text is a false positive, not a smaller true positive.
    docker exec "$container" pgrep -x kwin_wayland | head -n 1
}

# Same empty-scan floor as mounts_json_or_fail: an empty ss/lsof
# output must fail loudly, never be read as "nothing forbidden was
# found, so it passed" - it could just as easily mean the command
# itself did not run.
command_output_or_fail() {
    label="$1"
    shift
    output="$("$@" 2>&1)" || fail "$label falhou: $output"
    [ -n "$output" ] || fail "$label nao retornou nada (varredura vazia disfarcada de OK)"
    printf '%s' "$output"
}

assert_internal_socket_present() {
    ss_output="$1"
    socket_name="$2"
    printf '%s\n' "$ss_output" | grep -q "$socket_name" \
        || fail "socket interno esperado nao apareceu em ss -xap: $socket_name"
}

assert_output_has_no_forbidden_pattern() {
    label="$1"
    output="$2"
    for pattern in $FORBIDDEN_PATH_PATTERNS; do
        printf '%s\n' "$output" | grep -q "$pattern" \
            && fail "$label mostra um caminho proibido (padrao '$pattern')"
    done
    return 0
}

assert_only_internal_socket() {
    container="$1"
    socket_name="$2"

    pid="$(compositor_pid_in_container "$container")"
    [ -n "$pid" ] || fail "kwin_wayland nao encontrado dentro de '$container' (varredura vazia)"

    # `-a` is load-bearing here, verified against the real container
    # (not assumed): plain `ss -xp` only lists ESTABLISHED unix
    # sockets, and the wayland listening socket this whole check
    # exists to find is in LISTEN state - without `-a` it never shows
    # up, and the "internal socket present" assertion below would
    # have failed even on a perfectly isolated container.
    ss_output="$(command_output_or_fail "ss -xap" docker exec "$container" ss -xap)"
    assert_internal_socket_present "$ss_output" "$socket_name"
    assert_output_has_no_forbidden_pattern "ss -xap" "$ss_output"

    lsof_output="$(command_output_or_fail "lsof -p $pid" docker exec "$container" lsof -p "$pid")"
    assert_output_has_no_forbidden_pattern "lsof -p $pid" "$lsof_output"

    echo "check_isolation.sh: ss/lsof ok (pid $pid so referencia o socket interno '$socket_name')"
}

main() {
    require_args "$@"
    container="$1"
    socket_name="$2"

    assert_no_forbidden_mounts "$container"
    assert_only_internal_socket "$container" "$socket_name"

    echo "check_isolation.sh: isolamento provado para '$container' (GODS_LAWS.md L-09)"
}

main "$@"
