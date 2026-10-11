#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_isolation_selftest.sh - the controls of check_isolation.sh's HostConfig normalization (hostconfig_json.sh),
# loaded once with `.` by that script's own --selftest (D-W8-274; D-W8-137: a driver's controls live beside it,
# never inside it). No container, no docker, no socket: every input is derived by TEXT from
# hostconfig_baseline.txt. The poisoned variants are fixtures in text, never a real mount: mounting the host's
# XDG_RUNTIME_DIR, wayland-0 or /dev/uinput into a container is forbidden even when the container never starts
# (GODS_LAWS.md L-09, regra 2). It has no main and no dispatch of its own; $0 is still check_isolation.sh.
#
# The gold values below were measured BEFORE the split, with the pre-split functions of check_isolation.sh at
# HEAD 164c15fe (ouro, D-W8-274): the normalized text of each input hashed with md5sum. A control that compares
# against them proves the split changed no byte.

SELFTEST_CHECKS=0

# -- inputs: the baseline data line, six variants of it, and the ConsoleSize plant ----------------------------------

# (a): the baseline line as it is.
hc_variant_a() {
    printf '%s' "$1"
}

# (b): one device smuggled into HostConfig.Devices (the /dev/uinput shape of FURO 1).
hc_variant_b() {
    printf '%s' "$1" | sed 's#"Devices":\[\]#"Devices":[{"PathOnHost":"/dev/uinput","PathInContainer":"/dev/uinput","CgroupPermissions":"rwm"}]#'
}

# (c): PidMode host.
hc_variant_c() {
    printf '%s' "$1" | sed 's#"PidMode":""#"PidMode":"host"#'
}

# (d): a bind of the host's XDG_RUNTIME_DIR, as TEXT only (never mounted).
hc_variant_d() {
    printf '%s' "$1" | sed 's#"Binds":null#"Binds":["/run/user/1000:/run/user/1000"]#'
}

# (e): MaskedPaths emptied (the --security-opt systempaths=unconfined shape).
hc_variant_e() {
    printf '%s' "$1" | sed -E 's#"MaskedPaths":\[[^]]*\]#"MaskedPaths":[]#'
}

# (f): the elements of CapAdd and MaskedPaths in reverse order (the ORDER-DRIFT shape). CapAdd has one element,
# so only MaskedPaths really moves; the reversal of CapAdd is a no-op kept so the variant states its intent.
hc_variant_f() {
    hc_reverse_array "$(hc_reverse_array "$1" CapAdd)" MaskedPaths
}

# hc_reverse_array <text> <key>: reverses the elements of the flat array "<key>":[...] in plain text.
hc_reverse_array() {
    printf '%s' "$1" | awk -v key="$2" '
        {
            s = $0
            pre = "\"" key "\":["
            p = index(s, pre)
            if (p == 0) { printf "%s", s; exit }
            start = p + length(pre)
            rest = substr(s, start)
            q = index(rest, "]")
            n = split(substr(rest, 1, q - 1), parts, ",")
            rev = ""
            for (i = n; i >= 1; i--) rev = rev (i < n ? "," : "") parts[i]
            printf "%s", substr(s, 1, start - 1) rev substr(rest, q)
        }'
}

# The ConsoleSize plant: the live capture carries real numbers where the baseline carries "NORMALIZED".
hc_variant_a_console_planted() {
    printf '%s' "$1" | sed 's#"ConsoleSize":"NORMALIZED"#"ConsoleSize":[24,80]#'
}

# The variant names, in the order the controls use them.
HC_VARIANT_NAMES="hc_variant_a hc_variant_b hc_variant_c hc_variant_d hc_variant_e hc_variant_f hc_variant_a_console_planted"

# -- controls ----------------------------------------------------------------------------------------------------

# run_control <control>: runs one control in a SUBSHELL, so a variable or a stub it leaves behind dies with it.
run_control() {
    ( "$@" )
}

selftest_check() {
    SELFTEST_CHECKS=$((SELFTEST_CHECKS + 1))
    if ! run_control "$@"; then
        echo "selftest: controle $SELFTEST_CHECKS FALHOU: $*" >&2
        exit 1
    fi
}

# golden_matches <variant> <md5 of the gold>: the normalized text of the variant hashes to the gold.
golden_matches() {
    line="$(read_baseline_or_fail "$(baseline_file_for_variant ci)")"
    got="$(normalize_host_config "$("$1" "$line")" | md5sum | cut -d' ' -f1)"
    [ "$got" = "$2" ] || { echo "ouro $1: md5 $got, esperado $2" >&2; return 1; }
}

selftest_golden_a() {
    golden_matches hc_variant_a 4a11a53c59e2f59e694963385e0743df
}

selftest_golden_b() {
    golden_matches hc_variant_b 57f7c8d5963b5ce24d00f0262ba94c09
}

selftest_golden_c() {
    golden_matches hc_variant_c 6003674c9009fd4b3ffd9ddc7aca31bb
}

selftest_golden_d() {
    golden_matches hc_variant_d 1a4ef7e438d7b64013a174d874c50914
}

selftest_golden_e() {
    golden_matches hc_variant_e 9bf929f73f24be0ced39307e493e6930
}

selftest_golden_f() {
    golden_matches hc_variant_f 4a11a53c59e2f59e694963385e0743df
}

# MJ3: a live ConsoleSize must be normalized to the same gold as the baseline's "NORMALIZED".
selftest_console_planted_is_golden_a() {
    golden_matches hc_variant_a_console_planted 4a11a53c59e2f59e694963385e0743df
}

# Normalizing twice gives the same text as once, for every variant.
selftest_normalize_is_idempotent() {
    line="$(read_baseline_or_fail "$(baseline_file_for_variant ci)")"
    for v in $HC_VARIANT_NAMES; do
        once="$(normalize_host_config "$("$v" "$line")")"
        twice="$(normalize_host_config "$once")"
        [ "$once" = "$twice" ] || { echo "nao idempotente: $v" >&2; return 1; }
    done
}

# MJ1: the order of the elements does not change the result, and the reversed input really is different.
selftest_array_order_is_irrelevant() {
    line="$(read_baseline_or_fail "$(baseline_file_for_variant ci)")"
    straight="$(hc_variant_a "$line")"
    reversed="$(hc_variant_f "$line")"
    [ "$reversed" != "$straight" ] || { echo "controle de ordem sem efeito: a entrada invertida e igual a original" >&2; return 1; }
    [ "$(normalize_host_config "$reversed")" = "$(normalize_host_config "$straight")" ] \
        || { echo "ordem dos elementos mudou o resultado" >&2; return 1; }
}

# The four poisoned variants must NOT normalize to the gold: the normalizer may not hide an attack.
selftest_poisoned_entries_differ_from_golden() {
    line="$(read_baseline_or_fail "$(baseline_file_for_variant ci)")"
    gold="$(normalize_host_config "$(hc_variant_a "$line")")"
    for v in hc_variant_b hc_variant_c hc_variant_d hc_variant_e; do
        got="$(normalize_host_config "$("$v" "$line")")"
        [ "$got" != "$gold" ] || { echo "envenenada $v normalizou igual ao ouro (normalizacao cega)" >&2; return 1; }
    done
}

# ISO-NULL-QUOTE: a null inside a JSON string is content and stays; a bare null after a colon becomes [], also
# after a string that holds an escaped quote and a brace.
selftest_null_inside_string_is_kept() {
    input='{"Binds":["a:null,b"],"VolumesFrom":null,"Env":["x\"y:null}z"]}'
    expected='{"Binds":["a:null,b"],"VolumesFrom":[],"Env":["x\"y:null}z"]}'
    got="$(normalize_host_config "$input")"
    [ "$got" = "$expected" ] || { echo "null dentro de string reescrito: $got" >&2; return 1; }
}

# -- QA-COMPOSED-PATH G1b (D-W8-266): variantes por placa -------------------------------------------------------------
# The variant picks the baseline FILE; the comparison is the same whole-block compare as the CI's. Only
# assert_full_hostconfig_matches_baseline is called here (never the thermal_throttle count: it compares against the
# host, and these fixtures carry no such entries). The integrated capture is the baseline's own data line, the
# text the live `docker create` produced after the placeholders.

# hc_compare <variante> <texto>: the whole-block compare against the variant's baseline, in a subshell (its fail
# exits), output silenced. A name that does not map fails here too, so a refusal control never passes on a typo.
hc_compare() {
    file="$(baseline_file_for_variant "$1")" || return 1
    ( assert_full_hostconfig_matches_baseline selftest "$2" "$file" ) >/dev/null 2>&1
}

hc_must_accept() {
    hc_compare "$1" "$2" || { echo "variante $1 reprovou o que devia passar" >&2; return 1; }
}

hc_must_refuse() {
    if hc_compare "$1" "$2"; then echo "variante $1 aceitou o que devia reprovar" >&2; return 1; fi
}

integrated_line() {
    read_baseline_or_fail "$(baseline_file_for_variant gpu_integrated)"
}

# The argv rule: two arguments mean the CI default, three mean the variant. require_args accepts both shapes.
selftest_require_args_accepts_two_and_three() {
    ( require_args a b ) >/dev/null 2>&1 || { echo "require_args recusou dois argumentos" >&2; return 1; }
    ( require_args a b c ) >/dev/null 2>&1 || { echo "require_args recusou tres argumentos" >&2; return 1; }
    if ( require_args a ) >/dev/null 2>&1; then echo "require_args aceitou um argumento" >&2; return 1; fi
}

selftest_variant_defaults_to_ci() {
    [ "$(variant_of_args c s)" = ci ] || { echo "sem 3o argumento a variante nao e ci" >&2; return 1; }
}

selftest_variant_takes_the_third_argument() {
    [ "$(variant_of_args c s gpu_integrated)" = gpu_integrated ] || { echo "o 3o argumento nao virou a variante" >&2; return 1; }
}

# Each name reads its OWN file. A mutant that sends every name to the CI file breaks here, not only in the compare.
selftest_variant_map_names_its_own_file() {
    [ "$(baseline_file_for_variant ci)" = "$BASELINE_DIR/hostconfig_baseline.txt" ] || return 1
    [ "$(baseline_file_for_variant gpu_integrated)" = "$BASELINE_DIR/hostconfig_baseline_gpu_integrated.txt" ] || return 1
    [ "$(baseline_file_for_variant gpu_geforce)" = "$BASELINE_DIR/hostconfig_baseline_gpu_geforce.txt" ] || return 1
}

# Refused BY NAME. A bare failure is not enough: an open list also fails (for want of a file), so the message must
# name the variant.
selftest_unknown_variant_fails_by_name() {
    out="$( (baseline_file_for_variant xyz) 2>&1 )" && { echo "variante xyz aceita" >&2; return 1; }
    printf '%s' "$out" | grep -q "variante desconhecida 'xyz'" \
        || { echo "xyz reprovou sem o nome da variante: $out" >&2; return 1; }
}

selftest_integrated_capture_passes_its_baseline() {
    hc_must_accept gpu_integrated "$(integrated_line)"
}

selftest_integrated_capture_under_ci_is_refused() {
    hc_must_refuse ci "$(integrated_line)"
}

# The GeForce node in place of the integrated one: a different device, same shape.
selftest_renderd129_in_integrated_is_refused() {
    hc_must_refuse gpu_integrated "$(integrated_line | sed 's#/dev/dri/renderD128#/dev/dri/renderD129#g')"
}

# The integrated node plus one extra node (the /dev/uinput shape of FURO 1): one more device, refused.
selftest_renderd128_plus_uinput_is_refused() {
    hc_must_refuse gpu_integrated "$(integrated_line | sed 's#"Devices":\[{#"Devices":[{"PathOnHost":"/dev/uinput","PathInContainer":"/dev/uinput","CgroupPermissions":"rwm"},{#')"
}

# The host's XDG_RUNTIME_DIR as a bind, as TEXT only (never mounted; GODS_LAWS.md L-09 rule 2).
selftest_run_user_bind_is_refused() {
    hc_must_refuse gpu_integrated "$(integrated_line | sed 's#"Binds":null#"Binds":["/run/user/1000:/run/user/1000"]#')"
}

# Sem 3o argumento, a variante e ci, e a linha de base do CI passa contra o arquivo do CI.
selftest_ci_default_passes_its_baseline() {
    variant="$(variant_of_args c s)"
    line="$(read_baseline_or_fail "$(baseline_file_for_variant "$variant")")"
    hc_must_accept "$variant" "$line"
}

# A baseline that does not exist fails closed, never passes by omission (GODS_LAWS.md L-40).
selftest_missing_baseline_fails_closed() {
    if ( read_baseline_or_fail "$BASELINE_DIR/hostconfig_baseline_inexistente.txt" ) >/dev/null 2>&1; then
        echo "baseline ausente passou" >&2; return 1
    fi
}

selftest_main() {
    selftest_check selftest_golden_a
    selftest_check selftest_golden_b
    selftest_check selftest_golden_c
    selftest_check selftest_golden_d
    selftest_check selftest_golden_e
    selftest_check selftest_golden_f
    selftest_check selftest_console_planted_is_golden_a
    selftest_check selftest_normalize_is_idempotent
    selftest_check selftest_array_order_is_irrelevant
    selftest_check selftest_poisoned_entries_differ_from_golden
    selftest_check selftest_null_inside_string_is_kept
    selftest_check selftest_require_args_accepts_two_and_three
    selftest_check selftest_variant_defaults_to_ci
    selftest_check selftest_variant_takes_the_third_argument
    selftest_check selftest_variant_map_names_its_own_file
    selftest_check selftest_unknown_variant_fails_by_name
    selftest_check selftest_integrated_capture_passes_its_baseline
    selftest_check selftest_integrated_capture_under_ci_is_refused
    selftest_check selftest_renderd129_in_integrated_is_refused
    selftest_check selftest_renderd128_plus_uinput_is_refused
    selftest_check selftest_run_user_bind_is_refused
    selftest_check selftest_ci_default_passes_its_baseline
    selftest_check selftest_missing_baseline_fails_closed
    echo "selftest: $SELFTEST_CHECKS controles OK"
}
