#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/contido.sh - run any command inside a process-count ceiling (CI-SPLIT-PER-OS A5,
# PLANO-C1.md, CONTENCAO.md). Slice C1a: this file only DECIDES; execution lands in C1b.
#
# Usage:
#   tools/contido.sh [--tarefas N] [--tempo S] [--marcador M] -- command [args...]
#   tools/contido.sh --selftest
#     --tarefas N   integer >= 8, default 128: the task ceiling asked for
#     --tempo S     integer >= 1, default 600: the deadline in seconds
#     --marcador M  [a-z0-9-]{1,32}, default avulso: names the unit
#
# Decision (contido_decide, a pure function; the table lives in PLANO-C1.md, C1a):
#   HERDA <P>   the own cgroup already has a numeric pids.max: inherit it, never wrap again
#   EMBRULHA    wrap in a new systemd scope under glintfx-agentes.slice
#   AUSENTE-CI  CI runner without systemd --user: run without containment
#   RECUSA: ... refuse, exit 70, nothing is run
# Exit codes: 2 usage, 70 refusal or not implemented yet.
#
# Every fact is read without creating a process (`read`, `printf -v`, the /proc/self/limits file),
# except the single `systemctl --user show` probe, which is skipped when the own cgroup already has
# a ceiling. The cgroup root and the cgroup file are literals passed by contido_main: no
# environment variable can forge "already contained". The readers take them as arguments only so
# the selftest can build a fake tree.
#
# Identifiers are English (project L-21); the printed messages are Portuguese and fixed.

# contido_decide P U C_user C_fatia N CI PROBE
#   P, U, C_user, C_fatia: an integer, `max` (P only) or `ilegivel`
#   CI: the value of GITHUB_ACTIONS; only the literal `true` counts as CI
#   PROBE: ok | falha | nao-consultada
# Prints the decision line on stdout; returns 70 when it is a refusal.
contido_decide() {
    local p="$1" u="$2" c_user="$3" c_slice="$4" n="$5" ci="$6" probe="$7"
    local ceiling used
    case "$p" in
        ''|*[!0-9]*)
            if [ "$p" != "max" ]; then
                echo "RECUSA: cgroup proprio ilegivel"
                return 70
            fi
            ;;
        *)
            if [ "$p" -gt "$n" ]; then
                echo "HERDA $p AVISO: herdado pids.max=$p maior que o pedido $n"
            else
                echo "HERDA $p"
            fi
            return 0
            ;;
    esac
    if [ "$probe" != "ok" ]; then
        if [ "$ci" = "true" ]; then
            echo "AUSENTE-CI"
            return 0
        fi
        echo "RECUSA: systemd --user indisponivel"
        return 70
    fi
    local fact name
    for name in U C_user C_fatia; do
        case "$name" in
            U) fact="$u" ;;
            C_user) fact="$c_user" ;;
            C_fatia) fact="$c_slice" ;;
        esac
        case "$fact" in
            ''|*[!0-9]*) echo "RECUSA: $name ilegivel"; return 70 ;;
        esac
    done
    ceiling=$((u / 2))
    used=$((c_user - c_slice))
    if [ "$used" -gt $((u - ceiling)) ]; then
        echo "RECUSA: sessao fora dos agentes usa $used de $u"
        return 70
    fi
    if [ $((n + c_slice)) -gt "$ceiling" ]; then
        echo "RECUSA: nao cabe na fatia ($c_slice em uso de $ceiling)"
        return 70
    fi
    echo "EMBRULHA"
}

# contido_parse_args <args...>: validates before any fact is read; prints "N S M" on stdout.
# The command itself stays in "$@" of the caller (everything after the first `--`).
contido_parse_args() {
    local n=128 s=600 m=avulso
    while [ "$#" -gt 0 ]; do
        case "$1" in
            --tarefas)
                [ "$#" -ge 2 ] || { echo "contido: --tarefas pede um valor" >&2; return 2; }
                case "$2" in ''|*[!0-9]*) echo "contido: --tarefas '$2' nao e' inteiro" >&2; return 2 ;; esac
                [ "$2" -ge 8 ] || { echo "contido: --tarefas minimo 8 (veio $2)" >&2; return 2; }
                n="$2"; shift 2 ;;
            --tempo)
                [ "$#" -ge 2 ] || { echo "contido: --tempo pede um valor" >&2; return 2; }
                case "$2" in ''|*[!0-9]*) echo "contido: --tempo '$2' nao e' inteiro" >&2; return 2 ;; esac
                [ "$2" -ge 1 ] || { echo "contido: --tempo minimo 1 (veio $2)" >&2; return 2; }
                s="$2"; shift 2 ;;
            --marcador)
                [ "$#" -ge 2 ] || { echo "contido: --marcador pede um valor" >&2; return 2; }
                case "$2" in
                    ''|*[!a-z0-9-]*) echo "contido: --marcador '$2' fora de [a-z0-9-]{1,32}" >&2; return 2 ;;
                esac
                [ "${#2}" -le 32 ] || { echo "contido: --marcador '$2' passa de 32 caracteres" >&2; return 2; }
                m="$2"; shift 2 ;;
            --)
                shift
                [ "$#" -gt 0 ] || { echo "contido: comando vazio depois do --" >&2; return 2; }
                echo "$n $s $m"
                return 0 ;;
            *) echo "contido: argumento desconhecido '$1' (o comando vem depois de --)" >&2; return 2 ;;
        esac
    done
    echo "contido: falta o -- antes do comando" >&2
    return 2
}

# contido_user_slice_path <own cgroup path> <uid>: sets REPLY to the path of user-<uid>.slice found by
# climbing the own cgroup path, or to `ilegivel`. The component is derived from the uid, never
# written out. No process is created (REPLY instead of a command substitution).
contido_user_slice_path() {
    local path="$1" uid="$2" want
    want="user-${uid}.slice"
    case "$path" in
        */"$want"|*/"$want"/*) REPLY="${path%%/"$want"*}/$want" ;;
        *) REPLY="ilegivel" ;;
    esac
}

# contido_read_fact <file> <variable name>: puts the first word of the file in the variable, or
# `ilegivel` (never 0, never empty). No process is created (printf -v, no command substitution).
contido_read_fact() {
    local value
    if read -r value 2>/dev/null <"$1" && [ -n "$value" ]; then
        printf -v "$2" '%s' "$value"
    else
        printf -v "$2" '%s' "ilegivel"
    fi
}

# contido_read_facts <cgroup root> <cgroup file of the process>: fills FACT_P, FACT_U, FACT_C_USER,
# FACT_C_SLICE and FACT_CI. The two paths are ARGUMENTS so the selftest can build a fake tree;
# contido_main passes the literals /sys/fs/cgroup and /proc/self/cgroup, and no environment
# variable can forge "already contained". The probe of systemd is separate (contido_probe_systemd).
contido_read_facts() {
    local root="$1" cgroup_file="$2" line own_path="" limits_line
    FACT_P="ilegivel"; FACT_U="ilegivel"; FACT_C_USER="ilegivel"; FACT_C_SLICE="ilegivel"
    FACT_CI="${GITHUB_ACTIONS:-}"
    while read -r line; do
        case "$line" in 0::*) own_path="${line#0::}" ;; esac
    done 2>/dev/null <"$cgroup_file"
    if [ -n "$own_path" ]; then
        contido_read_fact "$root$own_path/pids.max" FACT_P
        contido_user_slice_path "$own_path" "$UID"
        if [ "$REPLY" != "ilegivel" ]; then
            local user_slice="$REPLY" slice_dir
            contido_read_fact "$root$user_slice/pids.current" FACT_C_USER
            # the slice sits under user@<uid>.service; a slice that does not exist yet is zero,
            # but a slice that exists and cannot be read is `ilegivel`, never zero
            slice_dir="$root$user_slice/user@$UID.service/glintfx.slice/glintfx-agentes.slice"
            if [ -d "$slice_dir" ]; then
                contido_read_fact "$slice_dir/pids.current" FACT_C_SLICE
            else
                FACT_C_SLICE=0
            fi
        fi
    fi
    # U: the soft "Max processes" of /proc/self/limits (what `ulimit -u` prints), read without a process
    while read -r limits_line; do
        case "$limits_line" in
            "Max processes"*)
                set -- ${limits_line#Max processes}
                case "${1:-}" in ''|*[!0-9]*) ;; *) FACT_U="$1" ;; esac
                ;;
        esac
    done </proc/self/limits
}

# contido_probe_systemd: sets FACT_PROBE (ok | falha). The ONLY process of the decision, and it is
# skipped when the own cgroup already has a numeric ceiling (inheriting needs no probe).
contido_probe_systemd() {
    if systemctl --user show --property=Version >/dev/null 2>&1; then
        FACT_PROBE=ok
    else
        FACT_PROBE=falha
    fi
}

contido_selftest() {
    local cases=0 failures=0

    # case <name> <expected stdout> <expected rc> <function> <args...>
    case_decide() {
        local name="$1" want_out="$2" want_rc="$3"
        shift 3
        local got_out got_rc
        got_out="$("$@" 2>/dev/null)"; got_rc=$?
        cases=$((cases + 1))
        if [ "$got_out" != "$want_out" ] || [ "$got_rc" -ne "$want_rc" ]; then
            failures=$((failures + 1))
            echo "contido --selftest: FALHOU - $name: esperado rc=$want_rc saida=[$want_out], obtido rc=$got_rc saida=[$got_out]" >&2
        fi
    }

    # contido_decide P U C_user C_fatia N CI PROBE   (U fixtures: 1000, round numbers only)
    # row 1: numeric P inherits, whatever else is true (the probe is not consulted)
    case_decide "row1 inherit P=64 N=128" "HERDA 64" 0 contido_decide 64 1000 100 0 128 "" nao-consultada
    case_decide "row1 inherit ignores a failing probe" "HERDA 64" 0 contido_decide 64 1000 100 0 128 true falha
    case_decide "row1 inherit ignores unreadable facts" "HERDA 64" 0 contido_decide 64 ilegivel ilegivel ilegivel 128 "" nao-consultada
    case_decide "row1 inherit with a larger ceiling warns" "HERDA 256 AVISO: herdado pids.max=256 maior que o pedido 128" 0 contido_decide 256 1000 100 0 128 "" nao-consultada
    case_decide "row1 inherit P equal to N does not warn" "HERDA 128" 0 contido_decide 128 1000 100 0 128 "" nao-consultada
    # rows 2 to 5, 10: the probe and CI
    case_decide "row2 CI true, probe fails" "AUSENTE-CI" 0 contido_decide max 1000 100 0 128 true falha
    case_decide "row3 CI true, probe ok, fits" "EMBRULHA" 0 contido_decide max 1000 100 0 128 true ok
    case_decide "row4 local, probe fails" "RECUSA: systemd --user indisponivel" 70 contido_decide max 1000 100 0 128 "" falha
    case_decide "row5 local, probe ok, fits" "EMBRULHA" 0 contido_decide max 1000 100 0 128 "" ok
    case_decide "row10 CI=false is not CI" "RECUSA: systemd --user indisponivel" 70 contido_decide max 1000 100 0 128 false falha
    case_decide "row10 CI=1 is not CI" "RECUSA: systemd --user indisponivel" 70 contido_decide max 1000 100 0 128 1 falha
    case_decide "row10 CI=TRUE is not CI" "RECUSA: systemd --user indisponivel" 70 contido_decide max 1000 100 0 128 TRUE falha
    # row 6: session outside the agents uses more than U - floor(U/2); boundary
    case_decide "row6 boundary equal fits" "EMBRULHA" 0 contido_decide max 1000 500 0 128 "" ok
    case_decide "row6 one above refuses" "RECUSA: sessao fora dos agentes usa 501 de 1000" 70 contido_decide max 1000 501 0 128 "" ok
    case_decide "row6 subtracts the agents slice" "EMBRULHA" 0 contido_decide max 1000 600 100 128 "" ok
    case_decide "row6 odd U boundary equal fits" "EMBRULHA" 0 contido_decide max 1001 501 0 128 "" ok
    case_decide "row6 odd U one above refuses" "RECUSA: sessao fora dos agentes usa 502 de 1001" 70 contido_decide max 1001 502 0 128 "" ok
    # row 7: N + C_fatia > floor(U/2); boundary
    case_decide "row7 boundary equal fits" "EMBRULHA" 0 contido_decide max 1000 400 372 128 "" ok
    case_decide "row7 one above refuses" "RECUSA: nao cabe na fatia (373 em uso de 500)" 70 contido_decide max 1000 400 373 128 "" ok
    # row 8 and 9: unreadable facts refuse, never become zero
    case_decide "row8 own cgroup unreadable" "RECUSA: cgroup proprio ilegivel" 70 contido_decide ilegivel 1000 100 0 128 "" ok
    case_decide "row9 U unreadable" "RECUSA: U ilegivel" 70 contido_decide max ilegivel 100 0 128 "" ok
    case_decide "row9 C_user unreadable" "RECUSA: C_user ilegivel" 70 contido_decide max 1000 ilegivel 0 128 "" ok
    case_decide "row9 C_fatia unreadable is not zero" "RECUSA: C_fatia ilegivel" 70 contido_decide max 1000 100 ilegivel 128 "" ok
    # the probe comes before the arithmetic
    case_decide "order: probe failure beats a full slice" "RECUSA: systemd --user indisponivel" 70 contido_decide max 1000 900 0 128 "" falha

    # argument parsing: contido_parse_args <args...>; prints "N S M" on success
    case_decide "args defaults" "128 600 avulso" 0 contido_parse_args -- echo oi
    case_decide "args explicit" "16 30 meu-teste" 0 contido_parse_args --tarefas 16 --tempo 30 --marcador meu-teste -- echo oi
    case_decide "args N minimum 8" "8 600 avulso" 0 contido_parse_args --tarefas 8 -- echo oi
    case_decide "args N=7 refused" "" 2 contido_parse_args --tarefas 7 -- echo oi
    case_decide "args N not a number" "" 2 contido_parse_args --tarefas abc -- echo oi
    case_decide "args S=0 refused" "" 2 contido_parse_args --tempo 0 -- echo oi
    case_decide "args S=1 accepted" "128 1 avulso" 0 contido_parse_args --tempo 1 -- echo oi
    case_decide "args marker with capital letter" "" 2 contido_parse_args --marcador Abc -- echo oi
    case_decide "args marker too long" "" 2 contido_parse_args --marcador aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa -- echo oi
    case_decide "args marker 32 accepted" "128 600 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" 0 contido_parse_args --marcador aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa -- echo oi
    case_decide "args without --" "" 2 contido_parse_args --tarefas 16 echo oi
    case_decide "args empty command" "" 2 contido_parse_args --tarefas 16 --
    case_decide "args unknown option" "" 2 contido_parse_args --foo 1 -- echo oi
    case_decide "args option without value" "" 2 contido_parse_args --tarefas
    case_decide "args no args at all" "" 2 contido_parse_args

    # cgroup path helpers (pure): user slice of a cgroup path, uid fixture 2000
    contido_user_slice_echo() { contido_user_slice_path "$@"; echo "$REPLY"; }
    case_decide "user slice from an app scope" "/user.slice/user-2000.slice" 0 contido_user_slice_echo /user.slice/user-2000.slice/user@2000.service/app.slice/x.scope 2000
    case_decide "user slice from a session scope" "/user.slice/user-2000.slice" 0 contido_user_slice_echo /user.slice/user-2000.slice/session-5.scope 2000
    case_decide "user slice absent" "ilegivel" 0 contido_user_slice_echo /system.slice/foo.service 2000
    case_decide "user slice of another uid" "ilegivel" 0 contido_user_slice_echo /user.slice/user-3000.slice/x.scope 2000

    # the fact readers, on a FAKE cgroup tree (mktemp), root and cgroup file passed as arguments
    local tree cg_dir uslice
    tree="$(mktemp -d "${TMPDIR:-/var/tmp}/glintfx-contido-facts.XXXXXX")" || return 1
    uslice="user.slice/user-$UID.slice"
    cg_dir="$tree/$uslice/user@$UID.service/app.slice/x.scope"
    # case_facts <name> <expected "P|C_user|C_fatia"> <cgroup file line>
    case_facts() {
        local name="$1" want="$2" line="$3" got
        printf '%s\n' "$line" >"$tree/cgroup"
        contido_read_facts "$tree" "$tree/cgroup"
        got="$FACT_P|$FACT_C_USER|$FACT_C_SLICE"
        cases=$((cases + 1))
        if [ "$got" != "$want" ]; then
            failures=$((failures + 1))
            echo "contido --selftest: FALHOU - facts $name: esperado [$want], obtido [$got]" >&2
        fi
    }
    local own_line="0::/$uslice/user@$UID.service/app.slice/x.scope" slice_dir
    slice_dir="$tree/$uslice/user@$UID.service/glintfx.slice/glintfx-agentes.slice"
    mkdir -p "$cg_dir"
    echo max >"$cg_dir/pids.max"
    echo 100 >"$tree/$uslice/pids.current"
    case_facts "slice absent is zero" "max|100|0" "$own_line"
    mkdir -p "$slice_dir"
    case_facts "slice present without pids.current is ilegivel, never zero" "max|100|ilegivel" "$own_line"
    echo 7 >"$slice_dir/pids.current"
    case_facts "normal" "max|100|7" "$own_line"
    echo 64 >"$cg_dir/pids.max"
    case_facts "own ceiling numeric" "64|100|7" "$own_line"
    rm "$cg_dir/pids.max"
    case_facts "own pids.max absent is ilegivel" "ilegivel|100|7" "$own_line"
    echo max >"$cg_dir/pids.max"
    rm "$tree/$uslice/pids.current"
    case_facts "user slice pids.current absent is ilegivel" "max|ilegivel|7" "$own_line"
    case_facts "cgroup outside the user slice" "ilegivel|ilegivel|ilegivel" "0::/system.slice/foo.service"
    # cgroup v1: the path in the v1 line EXISTS in the fake tree, with every file readable, so a reader that
    # accepted the v1 line as its own path would answer 64|100|7 instead of ilegivel
    echo 64 >"$cg_dir/pids.max"
    echo 100 >"$tree/$uslice/pids.current"
    case_facts "no 0:: line (cgroup v1, path exists)" "ilegivel|ilegivel|ilegivel" "1:name=systemd:/$uslice/user@$UID.service/app.slice/x.scope"
    echo 300 >"$tree/pids.max"
    case_facts "0::/ (container) reads the root pids.max" "300|ilegivel|ilegivel" "0::/"
    case "$tree" in "${TMPDIR:-/var/tmp}"/glintfx-contido-facts.??????) rm -rf -- "$tree" ;; esac

    if [ "$cases" -eq 0 ]; then
        echo "contido --selftest: FALHOU - zero casos rodados (varredura vazia)" >&2
        return 1
    fi
    if [ "$failures" -gt 0 ]; then
        echo "contido --selftest: FALHOU - $failures de $cases casos" >&2
        return 1
    fi
    echo "contido --selftest: OK - $cases casos"
}

# contido_main <args...>: decides, prints the fixed `contido:` line on stderr, then (C1a) refuses to run.
contido_main() {
    local parsed n s m limit_slice decision rc
    if ! parsed="$(contido_parse_args "$@")"; then
        echo "uso: tools/contido.sh [--tarefas N] [--tempo S] [--marcador M] -- comando [args...]" >&2
        return 2
    fi
    read -r n s m <<<"$parsed"
    contido_read_facts /sys/fs/cgroup /proc/self/cgroup
    FACT_PROBE="nao-consultada"
    [ "$FACT_P" = "max" ] && contido_probe_systemd
    decision="$(contido_decide "$FACT_P" "$FACT_U" "$FACT_C_USER" "$FACT_C_SLICE" "$n" "$FACT_CI" "$FACT_PROBE")"; rc=$?
    case "$FACT_U" in ''|*[!0-9]*) limit_slice=ilegivel ;; *) limit_slice=$((FACT_U / 2)) ;; esac
    echo "contido: decisao=${decision%% *}, pids.max_proprio=$FACT_P, N=$n, U=$FACT_U, C_user=$FACT_C_USER, C_fatia=$FACT_C_SLICE, T_fatia=$limit_slice, ci=${FACT_CI:-nao}, sonda=$FACT_PROBE" >&2
    case "$decision" in
        *"AVISO: "*) echo "contido: AVISO: ${decision#*AVISO: }" >&2 ;;
    esac
    if [ "$rc" -ne 0 ]; then
        echo "contido: $decision; nada foi executado" >&2
        return "$rc"
    fi
    case "$decision" in
        EMBRULHA) echo "contido: embrulho ainda nao implementado (C1b); nada foi executado" >&2 ;;
        *) echo "contido: execucao ainda nao implementada (C1b); nada foi executado" >&2 ;;
    esac
    return 70
}

if [ "${1:-}" = "--selftest" ] && [ "$#" -eq 1 ]; then
    contido_selftest
    exit $?
fi
contido_main "$@"
exit $?
