#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# hostconfig_json.sh - the text-level normalization of a captured `.HostConfig` (sort_json_arrays,
# collapse_null_outside_strings, normalize_host_config), moved out of check_isolation.sh by D-W8-274 (G1a) with no
# change of behaviour. It defines functions only and has no main: check_isolation.sh loads it with `.`, and so does
# check_isolation_selftest.sh. Its reasons to change (how Docker spells a JSON value, GODS_LAWS.md L-40/ISO-BASELINE)
# are not the reasons check_isolation.sh changes (what the isolation policy forbids, GODS_LAWS.md L-09), so the
# two live apart (GODS_LAWS.md L-17: one subject per file). Every function below does one thing.

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
# collapse_null_outside_strings(): the scan itself is split in four awk functions, each with its own shell
# function that prints it (GODS_LAWS.md L-17: one unit, one name). The shell side only assembles them.
collapse_null_outside_strings() {
    text="$1"
    printf '%s' "$text" | LC_ALL=C awk "$(null_collapse_awk_program)"
}

# The whole awk program, in the order the awk side needs it (awk does not care about order; the reader does).
null_collapse_awk_program() {
    null_collapse_awk_bare_null
    null_collapse_awk_string_step
    null_collapse_awk_outside_step
    null_collapse_awk_main_loop
}

# bare_null_at(): true only for a bare `null` token right after a colon and right before `,` or `}`.
null_collapse_awk_bare_null() {
    cat <<'AWK'
function bare_null_at(s, i, prev,    nxt) {
    if (prev != ":" || substr(s, i, 4) != "null") return 0
    nxt = substr(s, i + 4, 1)
    return nxt == "," || nxt == "}"
}
AWK
}

# string_step(): inside a JSON string, copies one character (or one backslash escape pair) and says where to go next.
null_collapse_awk_string_step() {
    cat <<'AWK'
function string_step(s, i,    c, nc) {
    c = substr(s, i, 1)
    if (c == "\\" && i < length(s)) {
        nc = substr(s, i + 1, 1)
        out = out c nc
        prev = nc
        return i + 2
    }
    out = out c
    prev = c
    if (c == "\"") in_str = 0
    return i + 1
}
AWK
}

# outside_step(): outside a string, opens a string, rewrites a bare null into [], or copies one character.
null_collapse_awk_outside_step() {
    cat <<'AWK'
function outside_step(s, i,    c) {
    c = substr(s, i, 1)
    if (c == "\"") {
        in_str = 1
        out = out c
        prev = c
        return i + 1
    }
    if (bare_null_at(s, i, prev)) {
        out = out "[]"
        prev = "]"
        return i + 4
    }
    out = out c
    prev = c
    return i + 1
}
AWK
}

# main loop: one line at a time, state reset per line exactly as before the split.
null_collapse_awk_main_loop() {
    cat <<'AWK'
{
    s = $0
    n = length(s)
    out = ""
    in_str = 0
    prev = ""
    i = 1
    while (i <= n) {
        if (in_str) i = string_step(s, i)
        else i = outside_step(s, i)
    }
    printf "%s", out
}
AWK
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
