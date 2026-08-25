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
# Adversarial review of the first version of this script (TEST-WLCONT)
# found three holes, all fixed here:
#
#   FURO 1: `--device=/dev/uinput` never touches `.Mounts` - it is a
#     wholly separate HostConfig field (`.HostConfig.Devices`). The
#     first version only inspected `.Mounts`, so a passed-through
#     device sailed straight through and got printed as "isolamento
#     provado".
#   FURO 2, the serious one: `--pid=host` shares the host's PID
#     namespace with the container. `docker exec <container> pgrep -x
#     kwin_wayland` is normally scoped to the container's own
#     processes for free, simply by virtue of exec'ing INTO the
#     container's namespaces - but that scoping silently stops
#     existing exactly when the PID namespace is shared. Under the
#     reviewer's `--pid=host` test this returned five PIDs, the first
#     one being the LEADER'S OWN LIVE COMPOSITOR (`--socket
#     wayland-0`), and the script went on to run `lsof` against it and
#     report success. No amount of reading the code would have caught
#     this; only running it with `--pid=host` did.
#   FURO 3: the mount-pattern blacklist used "/run/user/" (with a
#     trailing slash), which does not match a mount whose path is
#     exactly "/run/user" (no trailing component).
#
# The fix replaces a blacklist of "known dangerous substrings" with an
# allow-list of "the exact HostConfig this project's own clean
# container run produces" (Mounts and Devices empty; Privileged,
# PidMode, IpcMode, NetworkMode, UsernsMode, CapAdd, CapDrop matching a
# baseline measured live against THIS image, not guessed) - a
# blacklist only ever catches what someone remembered to list, and a
# JSON substring blacklist scoped to the wrong field (.Mounts) cannot
# see a leak that lives in a sibling field (.HostConfig.Devices,
# .HostConfig.PidMode). Every one of these allow-list checks runs
# BEFORE the PID-based checks below, which is what makes calling
# `pgrep` afterwards safe: it is never reached unless PidMode has
# already been confirmed to be the container's own private namespace.
#
# Usage: check_isolation.sh <container-name> <socket-name>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# Measured live against a `docker create --cap-drop=ALL
# --cap-add=SYS_NICE <this image>` with no other flags (TEST-WLCONT
# adversarial review response), not guessed: this project's clean
# invocation needs exactly these HostConfig values, nothing shared
# with the host. Any other value is a namespace or privilege leak.
readonly EXPECTED_PRIVILEGED="false"
readonly EXPECTED_PID_MODE=""
readonly EXPECTED_IPC_MODE="private"
readonly EXPECTED_NETWORK_MODE="bridge"
readonly EXPECTED_USERNS_MODE=""
readonly EXPECTED_CAP_ADD='["CAP_SYS_NICE"]'
readonly EXPECTED_CAP_DROP='["ALL"]'

# Defense in depth for the ss/lsof TEXT output check further down
# (assert_output_has_no_forbidden_pattern) only - .Mounts and
# .HostConfig.Devices are now allow-listed to "exactly empty"
# (assert_empty_json_array), which is strictly stronger than any
# substring blacklist. FURO 3 fix: "/run/user" with no trailing slash,
# so it still matches a path that is exactly "/run/user".
readonly FORBIDDEN_PATH_PATTERNS="/run/user wayland-0 /dev/uinput /tmp/.X11-unix"

fail() {
    echo "check_isolation.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 2 ] || fail "usage: check_isolation.sh <container-name> <socket-name>"
}

# `docker inspect` failing outright (typo'd container name, daemon
# down) must never be read as "field absent, so it's fine": that is
# exactly the disguised-empty-scan shape the leader ordered a floor
# against.
inspect_field_or_fail() {
    container="$1"
    format="$2"
    label="$3"
    value="$(docker inspect --format "$format" "$container" 2>&1)" \
        || fail "docker inspect ($label) falhou para '$container': $value"
    printf '%s' "$value"
}

# Allow-list, not blacklist: the only value this check accepts for a
# JSON array field is the literal empty array "[]". Used for both
# .Mounts and .HostConfig.Devices (FURO 1: a --device flag never
# touches .Mounts at all, so a blacklist scoped only to .Mounts can
# never see it - a separate allow-listed field is required).
assert_empty_json_array() {
    container="$1"
    format="$2"
    label="$3"
    value="$(inspect_field_or_fail "$container" "$format" "$label")"
    [ "$value" = "[]" ] || fail "$label nao esta vazio, e deveria estar: $value"
    echo "check_isolation.sh: $label ok (vazio)"
}

# Allow-list for the namespace/privilege fields the adversarial review
# named. Any value other than this project's own clean-container
# baseline means a namespace or privilege is shared with the host -
# FAIL, never a warning. This is FURO 2's actual fix: it runs before
# assert_only_internal_socket, so PidMode is proven private before
# `pgrep` is ever called inside the container.
assert_host_config_matches_baseline() {
    container="$1"
    privileged="$(inspect_field_or_fail "$container" '{{.HostConfig.Privileged}}' Privileged)"
    pid_mode="$(inspect_field_or_fail "$container" '{{.HostConfig.PidMode}}' PidMode)"
    ipc_mode="$(inspect_field_or_fail "$container" '{{.HostConfig.IpcMode}}' IpcMode)"
    network_mode="$(inspect_field_or_fail "$container" '{{.HostConfig.NetworkMode}}' NetworkMode)"
    userns_mode="$(inspect_field_or_fail "$container" '{{.HostConfig.UsernsMode}}' UsernsMode)"
    cap_add="$(inspect_field_or_fail "$container" '{{json .HostConfig.CapAdd}}' CapAdd)"
    cap_drop="$(inspect_field_or_fail "$container" '{{json .HostConfig.CapDrop}}' CapDrop)"

    [ "$privileged" = "$EXPECTED_PRIVILEGED" ] \
        || fail "Privileged fora do baseline: '$privileged' (esperado '$EXPECTED_PRIVILEGED')"
    [ "$pid_mode" = "$EXPECTED_PID_MODE" ] \
        || fail "PidMode fora do baseline: '$pid_mode' (esperado vazio - namespace de PID tem que ser privado do container, GODS_LAWS.md L-09 FURO 2)"
    [ "$ipc_mode" = "$EXPECTED_IPC_MODE" ] \
        || fail "IpcMode fora do baseline: '$ipc_mode' (esperado '$EXPECTED_IPC_MODE')"
    [ "$network_mode" = "$EXPECTED_NETWORK_MODE" ] \
        || fail "NetworkMode fora do baseline: '$network_mode' (esperado '$EXPECTED_NETWORK_MODE')"
    [ "$userns_mode" = "$EXPECTED_USERNS_MODE" ] \
        || fail "UsernsMode fora do baseline: '$userns_mode' (esperado vazio)"
    [ "$cap_add" = "$EXPECTED_CAP_ADD" ] \
        || fail "CapAdd fora do baseline: '$cap_add' (esperado '$EXPECTED_CAP_ADD')"
    [ "$cap_drop" = "$EXPECTED_CAP_DROP" ] \
        || fail "CapDrop fora do baseline: '$cap_drop' (esperado '$EXPECTED_CAP_DROP')"

    echo "check_isolation.sh: HostConfig ok (Privileged/PidMode/IpcMode/NetworkMode/UsernsMode/CapAdd/CapDrop batem com o baseline)"
}

compositor_pid_in_container() {
    container="$1"
    # `-x` matches the exact process name (comm), not `-f` (full
    # command line): verified live that `pgrep -f kwin_wayland` also
    # matches PID 1, the `dbus-run-session -- kwin_wayland ...`
    # wrapper, whose command line merely MENTIONS kwin_wayland as an
    # argument. Trusting "the first PID this container's own pgrep
    # reports" is only safe because assert_host_config_matches_baseline
    # already proved PidMode is not "host" - main() calls it first and
    # exits before reaching here otherwise (FURO 2).
    docker exec "$container" pgrep -x kwin_wayland | head -n 1
}

# Empty ss/lsof output must fail loudly, never be read as "nothing
# forbidden was found, so it passed" - it could just as easily mean
# the command itself did not run.
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

    # Metadata checks only (docker inspect, never docker exec) go
    # first and in full, on purpose: every one of them has to pass
    # before this script ever runs a command INSIDE the container.
    assert_empty_json_array "$container" '{{json .Mounts}}' ".Mounts"
    assert_empty_json_array "$container" '{{json .HostConfig.Devices}}' ".HostConfig.Devices"
    assert_host_config_matches_baseline "$container"

    assert_only_internal_socket "$container" "$socket_name"

    echo "check_isolation.sh: isolamento provado para '$container' (GODS_LAWS.md L-09)"
}

main "$@"
