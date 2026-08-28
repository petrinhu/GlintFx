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
# ORDER-DRIFT (27/08/2026, GODS_LAWS.md L-40/ISO-BASELINE): the real
# `wayland-container` CI job (runs-on ubuntu-latest) reproved the
# positive control the very first time it ran on a host other than the
# leader's Fedora 44 machine - not because any field's CONTENT
# differed from hostconfig_baseline.txt, but because MaskedPaths' four
# non-cpu entries ("/proc/sched_debug", "/proc/scsi", "/sys/firmware",
# "/sys/devices/virtual/powercap") came back in a different ORDER: same
# four strings, same count, shuffled. `docker inspect --format
# '{{json .HostConfig}}'` never promises a stable element order for
# any of its list-valued fields, and a per-field named exception
# ("MaskedPaths order does not matter") would be exactly the
# enumeration-blindness the whole-block compare above replaced -
# CapAdd, CapDrop, Dns, DnsOptions, DnsSearch, ExtraHosts, VolumesFrom,
# Links, GroupAdd, SecurityOpt, DeviceCgroupRules and ReadonlyPaths are
# the other flat string-array fields visible in this project's own
# baseline, every one exactly as order-unguaranteed as MaskedPaths. The
# fix is therefore generic, not a named-field list: sort_json_arrays()
# below canonicalizes the elements of every flat `[...]` span (one
# with no nested `[`, `]`, `{` or `}`) into sorted order before the
# compare, so no array-of-scalars field can reopen this bug under a
# different name. It deliberately never touches an array that holds
# JSON objects (Devices' `[{"PathOnHost":...}]` shape from FURO 1's own
# poisoned-device control, or Ulimits/Blkio*'s object shape), so a
# smuggled device or mount still breaks the match on content, exactly
# as before - only a field's own element ORDER stops being significant,
# never whether an element is present, absent, or new. See
# normalize_host_config() and sort_json_arrays() below.
#
# NULL-DRIFT (27/08/2026, GODS_LAWS.md L-40/ISO-BASELINE): the ORDER-
# DRIFT fix above was step one of a step, not the end of the story -
# the same `wayland-container` job reproved the positive control a
# second time, on a different runner, with `"Dns":null` in the live
# capture against `"Dns":[]` in the committed baseline - same meaning
# (no DNS override was ever set), different Go encoding/json spelling
# of a slice's zero value, the same underlying "docker inspect makes no
# promise about this spelling" cause as ORDER-DRIFT, one layer earlier
# (it does not promise element ORDER for a populated array, and it does
# not promise `null` over `[]` - or vice versa - for an empty one). This
# time the enumeration was done BEFORE writing the fix, per L-40's own
# instruction to enumerate a small space whole rather than search
# inside it: null-vs-empty-array is one of six ways two Docker/runner
# builds can spell an identical HostConfig value differently, and each
# of the other five was judged separately (observed and covered,
# observed and covered by a pre-existing check, or not observed and
# deliberately left uncovered rather than guessed at) - see
# normalize_host_config()'s own trailing comment block for the full
# six-item table and the reasoning behind each verdict.
#
# ISO-NULL-QUOTE (28/08/2026, GODS_LAWS.md L-09/L-40): the NULL-DRIFT
# fix above (":null" before "," or "}" rewritten to "[]") was written
# and reviewed as a plain sed pass over the raw JSON TEXT, with no
# notion of being inside or outside a quoted string - exactly the kind
# of blind substring match GODS_LAWS.md L-40's own closing warning
# exists to rule out. Adversarial review found the hole live, with a
# reproduction that needs no Docker at all:
# `{"Binds":["a:null,b"]}` - here "null" is CONTENT of a bind-mount
# string, not the JSON literal value, yet the substring ":null,"
# appears inside "a:null,b" regardless, and the old sed rewrote it into
# "a[],b", corrupting a real field's real content before it was ever
# compared to the baseline. A field whose actual live value differs
# from the baseline could be pushed toward looking identical by this
# same blind rewrite, which is precisely the thing this whole check
# exists to prevent ("null e [] sao o mesmo nada; mas [] e
# [\"/dev/uinput\"] nunca podem virar a mesma coisa", L-40). The fix,
# collapse_null_outside_strings() below, replaces the blind sed with a
# character-by-character scan that tracks whether the cursor is inside
# a JSON string (honoring backslash escapes, so a string containing an
# escaped quote does not flip the tracker early) and only ever rewrites
# a bare `null` token that sits immediately after a `:` OUTSIDE any
# string, exactly the position a real JSON value occupies. `null`
# appearing anywhere inside string content - a bind-mount path, an
# env var, a label - is left untouched, byte for byte, no matter what
# it is followed by.
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

# ORDER-DRIFT (27/08/2026): canonicalizes the element order of every
# flat JSON array in `text` - a `[...]` span whose contents hold no
# nested `[`, `]`, `{` or `}` - by sorting its comma-separated elements
# under `LC_ALL=C`. Text-level on purpose, same reason as
# normalize_host_config() below (no JSON parser in this project,
# GODS_LAWS.md L-07): the restriction to bracket spans with no `{`/`}`
# inside is exactly what keeps this safe on an object-valued array
# (Devices, Ulimits, Blkio*) without needing to parse one - the regex
# below simply never matches into it, so its content (and therefore
# its presence/absence signal) passes through untouched. Piped through
# `awk`, not sed: this needs an actual sort, not a substitution, and
# `awk` is already this project's established host-tooling pattern for
# text logic sed cannot express (tests/tools/check_public_name_collision.sh's
# enumerate_names.awk) - it is host-side test tooling, not something
# the shipped library links against, so GODS_LAWS.md L-07 does not
# reach it. Plain POSIX awk only (`match`, `split`, string `>`): this
# script is proven with `shellcheck -s sh` across five platforms, and
# the CI job that runs it is Ubuntu, whose default `awk` is mawk, not
# gawk - no `asort()`, no gawk-only extension.
sort_json_arrays() {
    text="$1"
    printf '%s' "$text" | LC_ALL=C awk '
        function sort_csv(inner,    n, i, j, tmp, tok, out) {
            if (inner == "") return ""
            n = split(inner, tok, ",")
            for (i = 2; i <= n; i++) {
                tmp = tok[i]
                j = i - 1
                while (j >= 1 && tok[j] > tmp) {
                    tok[j + 1] = tok[j]
                    j--
                }
                tok[j + 1] = tmp
            }
            out = tok[1]
            for (i = 2; i <= n; i++) out = out "," tok[i]
            return out
        }
        {
            s = $0
            result = ""
            while (match(s, /\[[^][{}]*\]/)) {
                pre = substr(s, 1, RSTART - 1)
                grp = substr(s, RSTART, RLENGTH)
                inner = substr(grp, 2, length(grp) - 2)
                result = result pre "[" sort_csv(inner) "]"
                s = substr(s, RSTART + RLENGTH)
            }
            printf "%s", result s
        }
    '
}

# NULL-DRIFT (27/08/2026, GODS_LAWS.md L-40/ISO-BASELINE): the
# `wayland-container` CI job reproved the positive control a SECOND
# time, on a runner that had never shown the ORDER-DRIFT shape - this
# time `"Dns":null` on one side against `"Dns":[]` on the other, same
# meaning (no DNS override, the field was never set), different Go
# encoding/json spelling of "zero elements" for a slice field (`nil`
# marshals as `null`, a non-nil empty slice marshals as `[]`; which one
# a given Docker build/runner produces for an UNSET `[]string` field is
# not a promise `docker inspect` makes, same root cause as ORDER-DRIFT's
# array-order non-promise, one layer earlier). Per L-40's own
# instruction to enumerate the whole space instead of patching one
# field: this is NOT "Dns does this", it is "any field whose zero value
# is a slice can be spelled either way", so the fix is the generic sed
# step below, not a Dns-shaped one - see this function's own trailing
# comment for the six other spellings-of-the-same-value forms that were
# enumerated alongside this fix (lista vazia/null is this one; cadeia
# vazia/null, numero/cadeia numerica, 0/false, campo ausente/presente-
# e-nulo, maiusculas em valor de modo) and why each is or is not
# covered.
#
# ISO-NULL-QUOTE (28/08/2026): character-scan replacement for the old
# blind `sed -E 's#:null([,}])#:[]\1#g'`. Tracks JSON string boundaries
# (`in_str`) with backslash-escape handling, so the ":null,"/":null}"
# rewrite only ever fires when the cursor is OUTSIDE a string - the
# exact position a real JSON `null` value occupies, and never inside
# one, where "null" can only ever be literal string CONTENT. `prev`
# holds the character immediately before the cursor (in the ORIGINAL
# text, not the rewritten `out`), so the "preceded by a bare colon"
# requirement from the old regex survives unchanged; only the "must not
# be inside a string" guard is new. Plain POSIX awk, same reasoning as
# sort_json_arrays() above (no gawk-only extension, host-side test
# tooling so GODS_LAWS.md L-07 does not reach it, proven on mawk via
# the Ubuntu CI job that runs this script).
collapse_null_outside_strings() {
    text="$1"
    printf '%s' "$text" | LC_ALL=C awk '
        {
            s = $0
            n = length(s)
            out = ""
            in_str = 0
            prev = ""
            i = 1
            while (i <= n) {
                c = substr(s, i, 1)
                if (in_str) {
                    out = out c
                    if (c == "\\" && i < n) {
                        i++
                        nc = substr(s, i, 1)
                        out = out nc
                        prev = nc
                        i++
                        continue
                    }
                    if (c == "\"") in_str = 0
                    prev = c
                    i++
                    continue
                }
                if (c == "\"") {
                    in_str = 1
                    out = out c
                    prev = c
                    i++
                    continue
                }
                if (c == "n" && prev == ":" && substr(s, i, 4) == "null") {
                    nxt = substr(s, i + 4, 1)
                    if (nxt == "," || nxt == "}") {
                        out = out "[]"
                        prev = "]"
                        i += 4
                        continue
                    }
                }
                out = out c
                prev = c
                i++
            }
            printf "%s", out
        }
    '
}

# Normalizes the four genuinely execution- or encoding-dependent parts
# of a raw HostConfig capture before comparing it to the measured
# baseline - see hostconfig_baseline.txt's own header for the fuller
# reasoning on the first three. The first three substitutions stay
# text-level sed on purpose (no JSON parser in this project, GODS_LAWS.md
# L-07): each targets a fixed, specific key name or path string
# (`"ConsoleSize":`, the literal `/sys/devices/system/cpu/cpuN/
# thermal_throttle` paths, `"OomKillDisable":`) that would require a
# deliberately adversarial field VALUE to spoof, and none has been
# observed to do so (GODS_LAWS.md L-20/L-27: normalize only what was
# measured, not every theoretical shape) - unlike the fourth, whose
# match text is the four bytes "null", plain enough to occur by
# coincidence inside perfectly ordinary string content, and which
# ISO-NULL-QUOTE (28/08/2026) proved actually does. That fourth step is
# now collapse_null_outside_strings() above, not a blind sed. A fifth
# step, sort_json_arrays() above, runs last: it is idempotent on
# already-normalized text (no ConsoleSize/cpuN/OomKillDisable/
# null-as-empty-array pattern survives to re-match), which is what lets
# assert_full_hostconfig_matches_baseline() below run this same
# function over the committed baseline file too, instead of requiring
# hostconfig_baseline.txt to be hand-edited into sorted order.
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
#
# The fourth (NULL-DRIFT, `:null` before a `,` or `}` boundary rewritten
# to `:[]` by collapse_null_outside_strings() above) is deliberately
# UNNAMED and UNSCOPED to any particular field, same reasoning as
# sort_json_arrays() being generic instead of a named-field list. It is
# provably safe to apply to every key, not just slice-typed ones,
# because of what it can and cannot ever equate: it only ever rewrites
# a bare, OUT-OF-STRING `null` token into `[]`, on BOTH the live capture
# and the baseline text (normalize_host_config() runs on both sides in
# assert_full_hostconfig_matches_baseline()) - it never touches a
# populated array, and it never touches ANY occurrence of the four bytes
# "null" that sits inside a JSON string, which is exactly the case
# ISO-NULL-QUOTE (28/08/2026) added the in-string guard for: the old,
# string-blind version of this step could and did rewrite "null" found
# inside ordinary field CONTENT (a bind-mount spec, an env value), which
# is a different failure from the type-confusion paragraph below - it is
# corrupting a real value's real text before the compare ever runs, not
# collapsing two spellings of the same nothing. A scalar field (an
# `*int64` like MemorySwappiness or PidsLimit) that is unset also reads
# back as a bare `null` OUTSIDE any string and would still pass through
# this rewrite into `[]`, which looks type-confused on paper but changes
# nothing about what the check can catch: Docker never serializes a SET
# int64 pointer as `[]` (it would show the real number instead), so an
# actual value on that field is untouched by this scan and still breaks
# the match exactly as before. In short: this step can only ever
# collapse two spellings of "nothing" into one, and only where "nothing"
# actually IS the JSON value at that position; it can never collapse
# "nothing" with "something", nor can it any longer touch "something"
# that merely happens to spell the word "null" as part of its own text -
# which is the one property GODS_LAWS.md L-40's own closing warning
# ("null e [] sao o mesmo nada; mas [] e [\"/dev/uinput\"] nunca podem
# virar a mesma coisa") requires of it.
normalize_host_config() {
    raw="$1"
    sed_normalized="$(printf '%s' "$raw" \
        | sed -E 's#"ConsoleSize":\[[0-9]+,[0-9]+\]#"ConsoleSize":"NORMALIZED"#' \
        | sed -E 's#,"/sys/devices/system/cpu/cpu[0-9]+/thermal_throttle"##g; s#"/sys/devices/system/cpu/cpu[0-9]+/thermal_throttle",##g' \
        | sed -E 's#"OomKillDisable":(false|null)#"OomKillDisable":"NORMALIZED"#')"
    sort_json_arrays "$(collapse_null_outside_strings "$sed_normalized")"
}

# ENUMERATION (27/08/2026, GODS_LAWS.md L-40's own instruction: an
# enumerable space gets enumerated whole, not searched inside). Six
# forms in which two Docker/runner builds can spell the identical
# HostConfig meaning differently were named at the same time this
# fatia was opened. This table is the record of that enumeration, not
# just the one fix above:
#
#   1. lista vazia contra null (`[]` vs `null`) - the bug this fatia
#      fixes. OBSERVED live (Dns, this file's own header). COBERTA by
#      the `:null([,}])` -> `:[]\1` step above, applied to every key.
#   2. cadeia vazia contra null (`""` vs `null`) - every string-typed
#      HostConfig field visible in this project's own baseline
#      (NetworkMode, VolumeDriver, PidMode, IpcMode, UTSMode,
#      UsernsMode, Cgroup, CgroupParent, CgroupnsMode, Runtime,
#      Isolation, CpusetCpus, CpusetMems, ContainerIDFile) is a plain
#      Go `string`, never a `*string` - encoding/json emits a non-
#      pointer string's zero value as `""`, and can only ever emit
#      `null` for a pointer or interface type. NAO PODE OCORRER neste
#      bloco, and NAO COBERTA on purpose: equating `""` with `null`
#      would be speculative code with no red test to prove it is
#      needed (GODS_LAWS.md L-20), and unlike the array case there is
#      no Go-typing argument that makes it provably safe in general -
#      only "not observed here yet" for this specific field set.
#   3. numero contra cadeia numerica (`0` vs `"0"`) - every numeric
#      HostConfig field visible here (ShmSize, OomScoreAdj, CpuShares,
#      Memory, NanoCpus, and the rest of the Cpu*/Blkio*/Memory* family)
#      is a plain Go integer type, always emitted bare, never quoted,
#      by the same encoding/json rule as case 2. NAO OBSERVADA, NAO
#      COBERTA, same reasoning as case 2: no evidence, so no code.
#   4. `0` contra `false` - a real type change (bool field becoming an
#      int, or vice versa) between Docker versions, not something this
#      project's own history has produced (the one boolean-shaped drift
#      actually measured, OomKillDisable's `false`/`null`, is a
#      DIFFERENT pair and is already normalized above). NAO OBSERVADA,
#      NAO COBERTA. If it ever happens, this check fails CLOSED (a real
#      mismatch reported), same as case 2 and 3 - never silently open -
#      and gets diagnosed and normalized the same way OomKillDisable
#      was: by measuring both sides side by side, never by guessing.
#   5. campo ausente contra campo presente-e-nulo - a key missing from
#      the JSON entirely versus the same key present with value `null`.
#      JA COBERTA, and not by anything in this function:
#      assert_hostconfig_key_count_matches_baseline() (above,
#      predating this fatia) counts the keys actually present before
#      any content comparison runs, so a field that disappears or
#      appears moves that count independently of what
#      normalize_host_config() does to the values.
#   6. maiusculas em valor de modo (`"bridge"` vs `"Bridge"`, `"no"` vs
#      `"No"`) - every mode-like string field here (NetworkMode,
#      IpcMode, UsernsMode, CgroupnsMode, RestartPolicy.Name, Isolation)
#      is Docker's own enum spelling, not user input, and has not been
#      observed to vary in case across this project's Docker Server
#      29.7.2 measurements. NAO OBSERVADA, NAO COBERTA, same
#      fails-closed reasoning as case 4.
#
# Cases 1 and 5 are covered because they were OBSERVED (this fatia and
# the pre-existing key-count check respectively). Cases 2, 3, 4 and 6
# are deliberately left uncovered, not overlooked: GODS_LAWS.md L-20
# and L-27 both forbid writing normalization code to a form nobody has
# seen this Docker version actually produce - the failure mode of doing
# that is a normalizer with a hole nobody tested, which is the exact
# genus L-40 exists to close, not to reopen with an "obviously safe"
# guess. The floor stays fail-closed for all four: an unnormalized
# genuine equivalence produces a spurious HostConfig mismatch (loud,
# investigated, safe) never a silent pass (quiet, exploitable, unsafe).

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
#
# ORDER-DRIFT (27/08/2026): `normalize_host_config()` runs on BOTH
# sides here, not just on the live capture - hostconfig_baseline.txt
# is committed as the raw text captured on the leader's machine
# (array elements in whatever order that Docker build happened to
# produce), and running it through sort_json_arrays() too is what
# makes the compare order-independent without hand-editing that file.
# This is safe only because normalize_host_config() is idempotent on
# already-normalized text (see its own comment) - a baseline file that
# somehow already went through this same pipeline once would compare
# identically either way.
assert_full_hostconfig_matches_baseline() {
    container="$1"
    raw="$2"
    normalized="$(normalize_host_config "$raw")"
    baseline_raw="$(read_baseline_or_fail)"
    baseline="$(normalize_host_config "$baseline_raw")"
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
