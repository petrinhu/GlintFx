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
# ISO-BASELINE (25-26/08/2026, GODS_LAWS.md L-40): the fix above
# replaced a substring blacklist with an allow-list of NINE NAMED
# fields (.Mounts, .HostConfig.Devices, Privileged, PidMode, IpcMode,
# NetworkMode, UsernsMode, CapAdd, CapDrop). That allow-list itself
# turned out to be the sixth of the six portoes L-40 was written
# about: it only ever asked about the fields someone remembered to
# name. Live proof of the gap, found while building this fatia:
# `docker create --cap-drop=ALL --cap-add=SYS_NICE --security-opt
# systempaths=unconfined <image>` touches NONE of the nine fields
# above - Privileged stays false, PidMode/IpcMode/NetworkMode/
# UsernsMode/CapAdd/CapDrop are untouched - yet it empties BOTH
# `.HostConfig.MaskedPaths` and `.HostConfig.ReadonlyPaths`, handing
# the container /proc/kcore (readable kernel memory), /proc/keys,
# /proc/scsi and the rest of Docker's default hidden paths. The nine-
# field version of this script printed "isolamento provado" for that
# container. Per the leader's decision (25/08/2026, AskUserQuestion,
# against keeping the named list and against a mixed approach), the
# fix this time is not "add a tenth named field": it is comparing
# `.HostConfig` AS A WHOLE against a live-measured baseline
# (tests/container/hostconfig_baseline.txt), so a field nobody thought
# to name shows up on its own the next time Docker adds one. See
# normalize_host_config() and assert_full_hostconfig_matches_baseline()
# below for what that costs and how the two genuinely host-dependent
# fields (ConsoleSize, and MaskedPaths' one-entry-per-CPU-core tail)
# are normalized without weakening the check - full reasoning is in
# hostconfig_baseline.txt's own header, since that is the file that
# has to be re-measured, not this script, when Docker's version moves.
#
# Usage: check_isolation.sh <container-name> <socket-name>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# fail() defined before the readonly constants below (not its usual
# spot further down with the other small helpers): BASELINE_DIR's
# resolution needs it to fail loud instead of "command not found" if
# `dirname` ever errored, same ordering reason as tools/preci.sh's own
# comment on moving fail()/log() above ROOT_DIR's resolution.
fail() {
    echo "check_isolation.sh: $1" >&2
    exit 1
}

# Measured live 26/08/2026 against Docker Server 29.7.2 (`docker
# version --format '{{.Server.Version}}'`), the same run that produced
# tests/container/hostconfig_baseline.txt - not guessed. Re-measure
# both together; a Docker version bump that changes the shape of
# `.HostConfig` will move this count too, and the point of this
# constant is exactly to say so with a dedicated message instead of
# leaving that discovery to the raw diff alone (GODS_LAWS.md L-40:
# "a contagem aparece na saida, mesmo quando passa").
readonly BASELINE_DOCKER_VERSION="29.7.2"
readonly EXPECTED_HOSTCONFIG_KEY_COUNT=63

# Declare-and-assign kept separate from `readonly` on purpose
# (shellcheck SC2155, same fix already applied in tools/preci.sh's
# ROOT_DIR resolution): combining them would mask a `dirname` failure
# behind `readonly`'s own always-0 exit status.
BASELINE_DIR="$(dirname "$0")" || fail "nao foi possivel resolver o diretorio deste script (dirname \"\$0\" falhou)"
readonly BASELINE_FILE="$BASELINE_DIR/hostconfig_baseline.txt"

# Defense in depth for the ss/lsof TEXT output check further down
# (assert_output_has_no_forbidden_pattern) only - the HostConfig this
# container was created with is now proven by
# assert_full_hostconfig_matches_baseline() below, which is strictly
# stronger than any substring blacklist. FURO 3 fix: "/run/user" with
# no trailing slash, so it still matches a path that is exactly
# "/run/user".
readonly FORBIDDEN_PATH_PATTERNS="/run/user wayland-0 /dev/uinput /tmp/.X11-unix"

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

# Fetches the whole HostConfig block once, so every metadata check
# below (key count, MaskedPaths cardinality, full-text compare) reads
# the SAME snapshot instead of racing three separate `docker inspect`
# calls against a container nothing else is touching in between.
raw_host_config() {
    container="$1"
    inspect_field_or_fail "$container" '{{json .HostConfig}}' "HostConfig (bloco completo)"
}

# `docker inspect`'s Go template `range` over `.HostConfig` only ever
# yields the keys actually present in the decoded JSON - Docker's own
# `omitempty` struct tags mean a field with its zero value (like an
# unset .Mounts, only ever populated by `--mount`) is simply ABSENT
# from that map, not present-with-a-null-value. That is exactly what
# makes this count a real, independent signal: a `--mount` flag this
# project never uses would not touch any of Binds/Devices/PidMode/etc,
# but it inserts a whole new top-level key, and this count moves.
hostconfig_key_count() {
    container="$1"
    keys="$(inspect_field_or_fail "$container" '{{range $k, $v := .HostConfig}}{{println $k}}{{end}}' "HostConfig (contagem de campos)")"
    # `sed '/^$/d'` is load-bearing, not defensive filler: verified
    # live that `docker inspect --format` appends one more trailing
    # newline of its own on top of whatever the template body already
    # printed, so the naive `wc -l` (no blank-line strip) over-counts
    # by exactly one stray blank line regardless of how many real keys
    # exist - this is what produced 63 real keys but a naive count of
    # 64 while this baseline was being measured.
    printf '%s\n' "$keys" | sed '/^$/d' | wc -l
}

# GODS_LAWS.md L-40: the count is printed even when this passes -
# "distingue olhou e estava tudo bem de nao olhou" - and a mismatch
# gets a dedicated message pointing at the baseline file, instead of
# only ever showing up buried in the raw diff further down.
assert_hostconfig_key_count_matches_baseline() {
    container="$1"
    count="$(hostconfig_key_count "$container")"
    echo "check_isolation.sh: HostConfig tem $count campo(s) de topo (baseline: $EXPECTED_HOSTCONFIG_KEY_COUNT, medido com Docker $BASELINE_DOCKER_VERSION)"
    [ "$count" -gt 0 ] || fail "HostConfig nao retornou NENHUM campo de topo (varredura vazia disfarcada de OK)"
    [ "$count" -eq "$EXPECTED_HOSTCONFIG_KEY_COUNT" ] \
        || fail "HostConfig tem $count campo(s) de topo, o baseline esperava $EXPECTED_HOSTCONFIG_KEY_COUNT - campo novo apareceu (ex.: --mount populando .HostConfig.Mounts, que fica AUSENTE quando nao usado) ou o Docker mudou de versao: reconfira tests/container/hostconfig_baseline.txt (GODS_LAWS.md L-40/ISO-BASELINE)"
}

# Counts the "/sys/devices/system/cpu/cpuN/thermal_throttle" entries
# inside the raw HostConfig's MaskedPaths - one per CPU core for which
# THAT SYSFS DIRECTORY ACTUALLY EXISTS on the machine running dockerd,
# not something any `docker create`/`run` flag sets directly. This is
# the check that actually catches `--security-opt
# systempaths=unconfined` (or any other flag that empties
# MaskedPaths/ReadonlyPaths): it is the exact hole the nine-named-field
# version of this script had, because none of those nine fields move
# when systempaths is set to unconfined.
masked_paths_cpu_thermal_count() {
    raw="$1"
    printf '%s' "$raw" | grep -oE '/sys/devices/system/cpu/cpu[0-9]+/thermal_throttle' | sed '/^$/d' | wc -l
}

# ENV-DRIFT (27/08/2026, GODS_LAWS.md L-40/ISO-BASELINE): the first cut
# of this check compared `actual_count` against `nproc` - one masked
# path expected per logical CPU the scheduler reports. That is not what
# Docker promises: moby's own default-masked-paths setup
# (oci/defaults_linux.go) loops over CPU indices but appends the path
# only when `os.Stat` on it succeeds, exactly mirroring the `find`
# below - it does NOT add one per `nproc`-reported core unconditionally.
# Caught live in the real `wayland-container` CI job: a GitHub-hosted
# Ubuntu runner reports `nproc` > 0 while having ZERO
# thermal_throttle directories at all - its vCPUs are virtualized and
# never expose the Intel/AMD thermal-interrupt-status interface this
# path walks, so dockerd masked none of them, correctly. The previous
# version reproved that runner for not matching a number Docker never
# produces there. Measuring the same existence check dockerd itself
# performs - on THIS host, at check time, which is the same host
# dockerd ran on when it built the container being inspected - is the
# only expected value this check can legitimately hold `actual_count`
# against; a bare-metal host with every core exposing the interface
# (this project's own development machine, 16/16) and a virtualized CI
# runner with none (0/0) both compare true, and an attack that empties
# MaskedPaths on EITHER kind of host still shows up as a mismatch.
host_thermal_throttle_dir_count() {
    find /sys/devices/system/cpu -mindepth 1 -maxdepth 2 \
        -type d -name thermal_throttle 2>/dev/null | sed '/^$/d' | wc -l
}

assert_cpu_masked_path_count_matches_host() {
    raw="$1"
    actual_count="$(masked_paths_cpu_thermal_count "$raw")"
    expected_count="$(host_thermal_throttle_dir_count)"
    echo "check_isolation.sh: MaskedPaths tem $actual_count entrada(s) de cpuN/thermal_throttle (thermal_throttle presente neste host: $expected_count)"
    [ "$actual_count" = "$expected_count" ] \
        || fail "MaskedPaths tem $actual_count entrada(s) de cpuN/thermal_throttle, esperava $expected_count (contagem real de /sys/devices/system/cpu/cpuN/thermal_throttle neste host, GODS_LAWS.md L-40/ISO-BASELINE - nao 'nproc': maquina virtualizada pode ter nucleos sem essa interface, e dockerd so mascara o que existe): provavel --security-opt systempaths=unconfined (ou equivalente) esvaziando MaskedPaths/ReadonlyPaths sem tocar nenhum dos campos que este portao checava antes do ISO-BASELINE"
}

# Normalizes the three genuinely execution-dependent parts of a raw
# HostConfig capture before comparing it to the measured baseline -
# see hostconfig_baseline.txt's own header for the full reasoning on
# why exactly these three, and nothing else, are excused from an exact
# match. All three substitutions are text-level on purpose (no JSON
# parser in this project, GODS_LAWS.md L-07): Go's json.Marshal output
# for a fixed Docker version is a stable, deterministic single line,
# so a plain sed pass is enough as long as it is applied identically
# to the live capture and to how the committed baseline was produced.
#
# The third (OomKillDisable) was found live while proving this
# function's OWN positive control: the CI job's positive-control step
# uses `docker run -d` (a STARTED container), while its three negative
# controls use `docker create` (deliberately never started, so the
# poisoned ENTRYPOINT never runs - see ci.yml's own comment on that).
# Measured side by side on the identical clean invocation:
# `OomKillDisable` reads back as the literal `false` right after
# `docker create`, and as `null` once the same container is actually
# `docker run`-started - nothing else in the whole block moves between
# the two states. Neither value is a security loosening (both mean
# "OOM killer not explicitly disabled"); only `--oom-kill-disable`
# (which sets it to the literal `true`) is, and that is left
# unnormalized on purpose, so it still breaks the match.
normalize_host_config() {
    raw="$1"
    printf '%s' "$raw" \
        | sed -E 's#"ConsoleSize":\[[0-9]+,[0-9]+\]#"ConsoleSize":"NORMALIZED"#' \
        | sed -E 's#,"/sys/devices/system/cpu/cpu[0-9]+/thermal_throttle"##g; s#"/sys/devices/system/cpu/cpu[0-9]+/thermal_throttle",##g' \
        | sed -E 's#"OomKillDisable":(false|null)#"OomKillDisable":"NORMALIZED"#'
}

read_baseline_or_fail() {
    [ -f "$BASELINE_FILE" ] \
        || fail "baseline ausente: $BASELINE_FILE (portao sem baseline nao aprova por omissao, GODS_LAWS.md L-40)"
    content="$(grep -v '^#' "$BASELINE_FILE")"
    [ -n "$content" ] \
        || fail "baseline vazio (so tinha comentario) em $BASELINE_FILE - varredura vazia disfarcada de OK"
    printf '%s' "$content"
}

# ISO-BASELINE, the point of this whole fatia: compares the ENTIRE
# HostConfig block (all top-level fields, GODS_LAWS.md L-40) against a
# live-measured baseline, instead of a hand-picked list of named
# fields. A field nobody thought to check shows up here on its own,
# because it shows up in the raw text either way. Runs before
# assert_only_internal_socket, same as the old
# assert_host_config_matches_baseline did: PidMode is proven private
# here before `pgrep` is ever called inside the container (FURO 2).
assert_full_hostconfig_matches_baseline() {
    container="$1"
    raw="$2"
    normalized="$(normalize_host_config "$raw")"
    baseline="$(read_baseline_or_fail)"
    if [ "$normalized" = "$baseline" ]; then
        echo "check_isolation.sh: HostConfig completo bate com o baseline medido (Docker $BASELINE_DOCKER_VERSION)"
        return 0
    fi
    diff_dir="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-iso-baseline-XXXXXX")"
    trap 'rm -rf "$diff_dir"' EXIT
    # POSIX sh has no <(...) process substitution (bashism, dash on
    # Ubuntu's CI does not have it either): write both sides to real
    # files and diff those (same pattern as
    # tests/tools/check_dup_laws.sh). One field per line (split on
    # comma) so the diff is legible instead of a single 1500-byte line
    # - this IS the "diff cru" this fatia's own text names as the
    # accepted cost of comparing the whole block instead of nine named
    # fields.
    printf '%s\n' "$baseline" | tr ',' '\n' > "$diff_dir/baseline"
    printf '%s\n' "$normalized" | tr ',' '\n' > "$diff_dir/atual"
    echo "check_isolation.sh: HostConfig DIVERGE do baseline medido (um campo por linha, virgula quebrada para leitura):" >&2
    diff -u "$diff_dir/baseline" "$diff_dir/atual" >&2 || true
    fail "HostConfig nao bate com o baseline medido em $BASELINE_FILE (GODS_LAWS.md L-40/ISO-BASELINE)"
}

compositor_pid_in_container() {
    container="$1"
    # `-x` matches the exact process name (comm), not `-f` (full
    # command line): verified live that `pgrep -f kwin_wayland` also
    # matches PID 1, the `dbus-run-session -- kwin_wayland ...`
    # wrapper, whose command line merely MENTIONS kwin_wayland as an
    # argument. Trusting "the first PID this container's own pgrep
    # reports" is only safe because assert_full_hostconfig_matches_baseline
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
    # `raw` is fetched once and shared by the three checks below so
    # they all read the same snapshot.
    raw="$(raw_host_config "$container")"
    assert_hostconfig_key_count_matches_baseline "$container"
    assert_cpu_masked_path_count_matches_host "$raw"
    assert_full_hostconfig_matches_baseline "$container" "$raw"

    assert_only_internal_socket "$container" "$socket_name"

    echo "check_isolation.sh: isolamento provado para '$container' (GODS_LAWS.md L-09)"
}

main "$@"
