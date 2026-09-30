#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/contido.sh - run any command inside a process-count ceiling (CI-SPLIT-PER-OS A5,
# PLANO-C1.md, CONTENCAO.md). C1a decides (contido_decide); C1b executes (contido_run_*).
#
# Usage:
#   tools/contido.sh [--tarefas N] [--tempo S] [--graca G] [--marcador M] -- command [args...]
#   tools/contido.sh --selftest
#     --tarefas N   integer >= 8, default 128: the task ceiling asked for
#     --tempo S     integer >= 1, default 600: the deadline in seconds
#     --graca G     integer >= 0, default 5: seconds between TERM and KILL at the end
#     --marcador M  [a-z0-9-]{1,32}, default avulso: names the unit
#
# Decision (contido_decide, a pure function; the table lives in PLANO-C1.md, C1a):
#   HERDA <P>   the own cgroup already has a numeric pids.max: inherit it, never wrap again
#   EMBRULHA    wrap in a new systemd scope under glintfx-agentes.slice
#   AUSENTE-CI  CI runner without systemd --user: run without containment
#   RECUSA: ... refuse, exit 70, nothing is run
# Exit codes: the command's own (signal death as 128+n), 2 usage, 70 refusal, 71 wrong scope or an invalid kill
# target, 72 broken clock, 73 supervisor dead (the inside half died without a marker), 124 deadline (the
# contido's own or the systemd's last-resort one). tools/contido_dentro.sh is the half that runs inside the scope.
# This is the ONLY versioned file that may call systemd-run (gate of C1c).
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

# contido_parse_args <args...>: validates before any fact is read; prints "N S M G" on stdout.
# The command itself stays in "$@" of the caller (everything after the first `--`).
contido_parse_args() {
    local n=128 s=600 m=avulso g=5
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
            --graca)
                [ "$#" -ge 2 ] || { echo "contido: --graca pede um valor" >&2; return 2; }
                case "$2" in ''|*[!0-9]*) echo "contido: --graca '$2' nao e' inteiro" >&2; return 2 ;; esac
                g="$2"; shift 2 ;;
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
                echo "$n $s $m $g"
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
    FACT_P="ilegivel"; FACT_U="ilegivel"; FACT_C_USER="ilegivel"; FACT_C_SLICE="ilegivel"; FACT_OWN=""
    FACT_CI="${GITHUB_ACTIONS:-}"
    while read -r line; do
        case "$line" in 0::*) own_path="${line#0::}" ;; esac
    done 2>/dev/null <"$cgroup_file"
    FACT_OWN="$own_path"
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

# contido_delegation <cgroup root> <own cgroup path>: sets REPLY to `ninho <scope root path>` when the own cgroup
# sits directly under a scope that contido delegated to itself, or to `pgid <reason>` otherwise. The proof is by
# the object, all four: the parent is named glintfx-*.scope; its cgroup.subtree_control is writable; it lists
# `pids`; and it holds supervisor/ (the mark of the layout contido builds). The NAME alone matches a scope systemd
# owns, such as a glintfx-preci-*.scope, where creating a sub-cgroup would write into something it owns. The
# reason names the first piece that is missing: cgroup-alheio | raiz-nao-gravavel | sem-pids | sem-supervisor.
# No process is created.
contido_delegation() {
    local root="$1" own="$2" parent control line
    REPLY="pgid cgroup-alheio"
    parent="${own%/*}"
    case "$own" in /*/*) ;; *) return 0 ;; esac
    case "${parent##*/}" in glintfx-*.scope) ;; *) return 0 ;; esac
    control="$root$parent/cgroup.subtree_control"
    [ -w "$control" ] || { REPLY="pgid raiz-nao-gravavel"; return 0; }
    read -r line <"$control" 2>/dev/null
    case " $line " in *" pids "*) ;; *) REPLY="pgid sem-pids"; return 0 ;; esac
    [ -d "$root$parent/supervisor" ] || { REPLY="pgid sem-supervisor"; return 0; }
    REPLY="ninho $parent"
}

# contido_ceiling <mode> <N> <P>: sets REPLY to "<effective> <warn 0|1> <reason>". Always printed, warned when the
# effective ceiling differs from the one asked for. proprio: N. ninho: the smaller of N and P, by the kernel
# hierarchy (warns when N > P). herdado-pgid: P, there is nowhere to put N (warns when P is not N).
contido_ceiling() {
    local mode="$1" n="$2" p="$3"
    case "$mode" in
        proprio) REPLY="$n 0 -" ;;
        ninho)
            case "$p" in
                ''|*[!0-9]*) REPLY="$n 0 -" ;;
                *) if [ "$n" -gt "$p" ]; then REPLY="$p 1 hierarquia"; else REPLY="$n 0 -"; fi ;;
            esac
            ;;
        *)
            if [ "$p" = "$n" ]; then REPLY="$p 0 -"; else REPLY="$p 1 sem-delegacao:o-pedido-nao-e-aplicavel"; fi
            ;;
    esac
}

# contido_wrap_outcome <marker> <ActiveState> <Result> <rc of systemd-run>: sets REPLY to
# "<fim> <rc> <sweep nada|stop> <reset 0|1>". The marker <dir>/fim rules when it exists; without it the
# supervisor (the inside half, which IS the process systemd-run runs) died or hung: with Result=timeout it was
# HUNG (prazo-encosto, 124); otherwise it is DEAD (supervisor-morto, 73, raw rc kept apart). A refusal of the
# inside half (71) is told by its marker `recusa:<reason>`; a bare 71 counts only with the unit inactive. The sweep is a `stop` whenever the unit is still active; a reset-failed
# only when it is `failed`.
contido_wrap_outcome() {
    local marker="$1" state="$2" result="$3" rc="$4" sweep=nada reset=0
    # a state that is missing (no ActiveState= line) is NEVER read as inactive: fail closed, sweep with a stop
    case "$state" in ''|active|activating|deactivating|reloading) sweep=stop ;; failed) reset=1 ;; esac
    case "$marker" in
        rc=*) REPLY="$marker ${marker#rc=} $sweep $reset" ;;
        prazo) REPLY="prazo 124 $sweep $reset" ;;
        relogio) REPLY="relogio 72 $sweep $reset" ;;
        recusa:*) REPLY="$marker 71 $sweep $reset" ;;
        *)
            # a bare 71 (no marker: the inside half could not write one) is a refusal ONLY when the unit is
            # inactive; with the unit still active it is a supervisor that died after launching the load
            if [ "$rc" = 71 ] && [ "$sweep" = nada ]; then REPLY="recusa 71 $sweep $reset"
            elif [ "$result" = timeout ]; then REPLY="prazo-encosto 124 $sweep $reset"
            else REPLY="supervisor-morto 73 $sweep $reset"
            fi
            ;;
    esac
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
    case_decide "args defaults" "128 600 avulso 5" 0 contido_parse_args -- echo oi
    case_decide "args explicit" "16 30 meu-teste 5" 0 contido_parse_args --tarefas 16 --tempo 30 --marcador meu-teste -- echo oi
    case_decide "args N minimum 8" "8 600 avulso 5" 0 contido_parse_args --tarefas 8 -- echo oi
    case_decide "args grace default is 5" "128 600 avulso 5" 0 contido_parse_args -- echo oi
    case_decide "args grace explicit" "128 600 avulso 7" 0 contido_parse_args --graca 7 -- echo oi
    case_decide "args grace 0 accepted" "128 600 avulso 0" 0 contido_parse_args --graca 0 -- echo oi
    case_decide "args grace not a number" "" 2 contido_parse_args --graca abc -- echo oi
    case_decide "args grace negative" "" 2 contido_parse_args --graca -1 -- echo oi
    case_decide "args grace without value" "" 2 contido_parse_args --graca
    case_decide "args N=7 refused" "" 2 contido_parse_args --tarefas 7 -- echo oi
    case_decide "args N not a number" "" 2 contido_parse_args --tarefas abc -- echo oi
    case_decide "args S=0 refused" "" 2 contido_parse_args --tempo 0 -- echo oi
    case_decide "args S=1 accepted" "128 1 avulso 5" 0 contido_parse_args --tempo 1 -- echo oi
    case_decide "args marker with capital letter" "" 2 contido_parse_args --marcador Abc -- echo oi
    case_decide "args marker too long" "" 2 contido_parse_args --marcador aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa -- echo oi
    case_decide "args marker 32 accepted" "128 600 aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa 5" 0 contido_parse_args --marcador aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa -- echo oi
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
    # delegation (pure, on a fake tree): all four pieces or `pgid <the first missing one>`, one fault at a time
    local dtree="$tree/deleg" scope_path="/$uslice/user@$UID.service/glintfx.slice/glintfx-agentes.slice/glintfx-x-1-2.scope" ctl
    contido_delegation_echo() { contido_delegation "$@"; echo "$REPLY"; }
    ctl="$dtree$scope_path/cgroup.subtree_control"
    mkdir -p "$dtree$scope_path/carga" "$dtree$scope_path/supervisor" "$dtree${scope_path%/*}/foo.scope/supervisor"
    echo pids >"$ctl"
    case_decide "delegation: carga under a delegated scope" "ninho $scope_path" 0 contido_delegation_echo "$dtree" "$scope_path/carga"
    case_decide "delegation: ninho under a delegated scope" "ninho $scope_path" 0 contido_delegation_echo "$dtree" "$scope_path/ninho-a"
    echo "cpu pids memory" >"$ctl"
    case_decide "delegation: pids among other controllers" "ninho $scope_path" 0 contido_delegation_echo "$dtree" "$scope_path/carga"
    : >"$ctl"
    case_decide "delegation: empty subtree_control lacks pids" "pgid sem-pids" 0 contido_delegation_echo "$dtree" "$scope_path/carga"
    echo "cpu memory" >"$ctl"
    case_decide "delegation: other controllers without pids lack pids" "pgid sem-pids" 0 contido_delegation_echo "$dtree" "$scope_path/carga"
    echo "pidsx" >"$ctl"
    case_decide "delegation: a word that only starts with pids lacks pids" "pgid sem-pids" 0 contido_delegation_echo "$dtree" "$scope_path/carga"
    echo pids >"$ctl"
    rmdir "$dtree$scope_path/supervisor"
    case_decide "delegation: pids and writable but NO supervisor/ (not our layout)" "pgid sem-supervisor" 0 contido_delegation_echo "$dtree" "$scope_path/carga"
    mkdir "$dtree$scope_path/supervisor"
    rm "$ctl"
    case_decide "delegation: no subtree_control file is not writable" "pgid raiz-nao-gravavel" 0 contido_delegation_echo "$dtree" "$scope_path/carga"
    echo pids >"$dtree${scope_path%/*}/foo.scope/cgroup.subtree_control"
    case_decide "delegation: everything present but the scope is not glintfx-*" "pgid cgroup-alheio" 0 contido_delegation_echo "$dtree" "${scope_path%/*}/foo.scope/carga"
    echo pids >"$ctl"
    case_decide "delegation: the own cgroup IS the scope (parent is the slice)" "pgid cgroup-alheio" 0 contido_delegation_echo "$dtree" "$scope_path"
    case_decide "delegation: the root cgroup" "pgid cgroup-alheio" 0 contido_delegation_echo "$dtree" "/"
    if [ "${EUID:-1}" -ne 0 ]; then   # as root the file stays writable: this control only means something without root
        chmod 444 "$ctl"
        case_decide "delegation: pids listed but the file is not writable" "pgid raiz-nao-gravavel" 0 contido_delegation_echo "$dtree" "$scope_path/carga"
        chmod 644 "$ctl"
    fi

    # the effective ceiling and its warning, per mode (Q1: always printed, warned when it differs from the pedido)
    contido_ceiling_echo() { contido_ceiling "$@"; echo "$REPLY"; }
    case_decide "ceiling proprio: N" "16 0 -" 0 contido_ceiling_echo proprio 16 -
    case_decide "ceiling ninho: N below P" "16 0 -" 0 contido_ceiling_echo ninho 16 32
    case_decide "ceiling ninho: N equals P" "32 0 -" 0 contido_ceiling_echo ninho 32 32
    case_decide "ceiling ninho: N above P is capped, with the warning" "16 1 hierarquia" 0 contido_ceiling_echo ninho 64 16
    case_decide "ceiling herdado-pgid: P equals N" "128 0 -" 0 contido_ceiling_echo herdado-pgid 128 128
    case_decide "ceiling herdado-pgid: P above N warns" "128 1 sem-delegacao:o-pedido-nao-e-aplicavel" 0 contido_ceiling_echo herdado-pgid 16 128
    case_decide "ceiling herdado-pgid: P BELOW N warns too (the case the per-mode warning let pass)" "16 1 sem-delegacao:o-pedido-nao-e-aplicavel" 0 contido_ceiling_echo herdado-pgid 128 16

    # the outcome of a scope after systemd-run returned (R2-Q3): marker, then the unit's own state
    contido_wrap_outcome_echo() { contido_wrap_outcome "$@"; echo "$REPLY"; }
    case_decide "wrap: marker rc=0, unit gone" "rc=0 0 nada 0" 0 contido_wrap_outcome_echo rc=0 inactive success 0
    case_decide "wrap: marker rc=77 crosses" "rc=77 77 nada 0" 0 contido_wrap_outcome_echo rc=77 inactive success 77
    case_decide "wrap: marker prazo" "prazo 124 nada 0" 0 contido_wrap_outcome_echo prazo inactive success 124
    case_decide "wrap: marker relogio" "relogio 72 nada 0" 0 contido_wrap_outcome_echo relogio inactive success 72
    case_decide "wrap: marker prazo with the unit still ACTIVE gets the final sweep" "prazo 124 stop 0" 0 contido_wrap_outcome_echo prazo active success 124
    case_decide "wrap: marker relogio with the unit still ACTIVE gets the final sweep" "relogio 72 stop 0" 0 contido_wrap_outcome_echo relogio active success 72
    case_decide "wrap: marker but the unit is still active gets the final sweep" "rc=0 0 stop 0" 0 contido_wrap_outcome_echo rc=0 active success 0
    case_decide "wrap: no marker, Result=timeout is the systemd's deadline" "prazo-encosto 124 nada 1" 0 contido_wrap_outcome_echo "" failed timeout 143
    case_decide "wrap: no marker, dead supervisor with the unit still active" "supervisor-morto 73 stop 0" 0 contido_wrap_outcome_echo "" active success 137
    case_decide "wrap: no marker, dead supervisor with the unit gone" "supervisor-morto 73 nada 0" 0 contido_wrap_outcome_echo "" inactive success 137
    case_decide "wrap: no marker, a bare 71 with the unit INACTIVE is a refusal" "recusa 71 nada 0" 0 contido_wrap_outcome_echo "" inactive success 71
    case_decide "wrap: no marker, a bare 71 with the unit ACTIVE is a dead supervisor (stop, 73)" "supervisor-morto 73 stop 0" 0 contido_wrap_outcome_echo "" active success 71
    case_decide "wrap: marker recusa:<reason> maps to 71 by the marker" "recusa:escopo-errado 71 nada 0" 0 contido_wrap_outcome_echo recusa:escopo-errado inactive success 71
    case_decide "wrap: marker recusa with the unit still active also gets the sweep" "recusa:preparo-falhou 71 stop 0" 0 contido_wrap_outcome_echo recusa:preparo-falhou active success 71
    case_decide "wrap: a MISSING ActiveState is never inactive (fail closed: stop)" "supervisor-morto 73 stop 0" 0 contido_wrap_outcome_echo "" "" success 137
    case_decide "wrap: a missing ActiveState with a marker still gets the sweep" "rc=0 0 stop 0" 0 contido_wrap_outcome_echo rc=0 "" "" 0
    case_decide "wrap: a MISSING Result without a marker is a dead supervisor, never a timeout" "supervisor-morto 73 nada 0" 0 contido_wrap_outcome_echo "" inactive "" 137
    case_decide "wrap: failed unit is reset" "supervisor-morto 73 nada 1" 0 contido_wrap_outcome_echo "" failed exit-code 1
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

# contido_marker_dir: creates the marker directory (fails closed) and prints it.
contido_marker_dir() {
    mktemp -d "${TMPDIR:-/var/tmp}/glintfx-contido.XXXXXX"
}

# contido_marker_cleanup <dir>: removes ONLY a directory made by contido_marker_dir.
contido_marker_cleanup() {
    case "$1" in
        "${TMPDIR:-/var/tmp}"/glintfx-contido.??????) rm -rf -- "$1" ;;
    esac
}

# contido_run_inherit <N> <P> <S> <G> <dir> <command...>: the own cgroup already has a numeric ceiling, so no
# systemd-run at all. Delegated contido scope above (FACT_OWN): mode ninho, a sub-cgroup of our own that a
# setsid grandchild cannot escape; anything else (somebody else's cgroup, e.g. a container): herdado-pgid.
contido_run_inherit() {
    local n="$1" p="$2" s="$3" g="$4" dir="$5"
    shift 5
    contido_delegation /sys/fs/cgroup "$FACT_OWN"
    case "$REPLY" in
        ninho\ *) bash "${BASH_SOURCE[0]%/*}/contido_dentro.sh" ninho "${REPLY#ninho }" "$n" "$s" "$g" "$dir" -- "$@" ;;
        *) bash "${BASH_SOURCE[0]%/*}/contido_dentro.sh" herdado-pgid - "$p" "$s" "$g" "$dir" -- "$@" ;;
    esac
}

# contido_run_wrap <N> <S> <G> <M> <T_fatia> <dir> <command...>: mode proprio, a new DELEGATED scope in the
# slice, WITHOUT --collect (with it a deadline of the systemd is indistinguishable: the unit vanishes), with
# RuntimeMaxSec = S + G + 5 as the last resort for a HUNG inside half and TimeoutStopSec = G for the `stop` sweep.
# The rc of the command comes from <dir>/fim. When systemd-run returns, ONE `systemctl show` reads the state and
# contido_wrap_outcome decides; a unit still active gets `systemctl --user stop` (the systemd's own TERM, G, KILL
# on the whole scope), and the final line prints varredura_final=<nada|stop>.
contido_run_wrap() {
    local n="$1" s="$2" g="$3" m="$4" ceiling="$5" dir="$6" unit rc fim="" out line state="" result="" stop_g
    shift 6
    if ! systemctl --user set-property --runtime glintfx-agentes.slice "TasksMax=$ceiling"; then
        echo "contido: RECUSA: nao consegui fixar TasksMax=$ceiling na fatia; nada foi executado" >&2
        return 70
    fi
    stop_g=$((g > 0 ? g : 1))   # TimeoutStopSec=0 means no timeout at all
    unit="glintfx-$m-$$-${EPOCHREALTIME//[.,]/}"
    systemd-run --user --scope --quiet --expand-environment=no \
        --slice=glintfx-agentes.slice --unit="$unit" -p Delegate=pids -p "TasksMax=$n" \
        -p "RuntimeMaxSec=$((s + g + 5))" -p "TimeoutStopSec=$stop_g" -- \
        bash "${BASH_SOURCE[0]%/*}/contido_dentro.sh" proprio "$unit" "$n" "$s" "$g" "$dir" -- "$@"
    rc=$?
    [ -r "$dir/fim" ] && read -r fim <"$dir/fim"
    out="$(systemctl --user show -p ActiveState -p Result "$unit.scope" 2>/dev/null)"
    while read -r line; do
        case "$line" in ActiveState=*) state="${line#ActiveState=}" ;; Result=*) result="${line#Result=}" ;; esac
    done <<<"$out"
    [ -n "$state" ] && [ -n "$result" ] || echo "contido: AVISO: o systemctl show nao trouxe ActiveState= e Result=; a unidade e' tratada como ATIVA (varredura com stop)" >&2
    contido_wrap_outcome "$fim" "$state" "$result" "$rc"
    local verdict sweep reset out_rc
    read -r verdict out_rc sweep reset <<<"$REPLY"
    [ "$sweep" = stop ] && systemctl --user stop "$unit.scope" 2>/dev/null
    [ "$reset" = 1 ] && systemctl --user reset-failed "$unit.scope" 2>/dev/null
    echo "contido: fim=$verdict, varredura_final=$sweep, rc_bruto=$rc" >&2
    return "$out_rc"
}

# contido_main <args...>: decides, prints the fixed `contido:` line on stderr, then runs.
contido_main() {
    local parsed n s m g limit_slice decision rc dir
    if ! parsed="$(contido_parse_args "$@")"; then
        echo "uso: tools/contido.sh [--tarefas N] [--tempo S] [--graca G] [--marcador M] -- comando [args...]" >&2
        return 2
    fi
    read -r n s m g <<<"$parsed"
    while [ "$1" != "--" ]; do shift; done
    shift
    contido_read_facts /sys/fs/cgroup /proc/self/cgroup
    FACT_PROBE="nao-consultada"
    [ "$FACT_P" = "max" ] && contido_probe_systemd
    decision="$(contido_decide "$FACT_P" "$FACT_U" "$FACT_C_USER" "$FACT_C_SLICE" "$n" "$FACT_CI" "$FACT_PROBE")"; rc=$?
    case "$FACT_U" in ''|*[!0-9]*) limit_slice=ilegivel ;; *) limit_slice=$((FACT_U / 2)) ;; esac
    local mode_pred="" eff="-" warn=0 reason="-" p_show="-" deleg=""
    case "$FACT_P" in ''|*[!0-9]*) ;; *) p_show="$FACT_P" ;; esac
    case "$decision" in
        HERDA*)
            contido_delegation /sys/fs/cgroup "$FACT_OWN"; deleg="$REPLY"
            case "$deleg" in ninho\ *) mode_pred=ninho ;; *) mode_pred=herdado-pgid ;; esac
            contido_ceiling "$mode_pred" "$n" "$FACT_P"; read -r eff warn reason <<<"$REPLY"
            ;;
        EMBRULHA) contido_ceiling proprio "$n" "-"; read -r eff warn reason <<<"$REPLY" ;;
    esac
    echo "contido: decisao=${decision%% *}, pids.max_proprio=$FACT_P, N=$n, U=$FACT_U, C_user=$FACT_C_USER, C_fatia=$FACT_C_SLICE, T_fatia=$limit_slice, teto_efetivo=$eff (pedido=$n, herdado=$p_show), ci=${FACT_CI:-nao}, sonda=$FACT_PROBE" >&2
    [ "$warn" = 1 ] && echo "contido: AVISO: teto efetivo $eff difere do pedido $n ($reason)" >&2
    case "$deleg" in "pgid "*) echo "contido: delegacao ausente (${deleg#pgid }): sem escopo delegado a contido, o prazo nao alcanca um setsid" >&2 ;; esac
    if [ "$rc" -ne 0 ]; then
        echo "contido: $decision; nada foi executado" >&2
        return "$rc"
    fi
    case "$decision" in
        AUSENTE-CI)
            echo "contido: ausente-ci (GITHUB_ACTIONS=true, systemd --user indisponivel; runner descartavel)" >&2
            exec "$@"
            ;;
    esac
    dir="$(contido_marker_dir)" || { echo "contido: RECUSA: sem diretorio de marcador; nada foi executado" >&2; return 70; }
    case "$decision" in
        HERDA*) contido_run_inherit "$n" "$FACT_P" "$s" "$g" "$dir" "$@"; rc=$? ;;
        EMBRULHA) contido_run_wrap "$n" "$s" "$g" "$m" "$limit_slice" "$dir" "$@"; rc=$? ;;
    esac
    contido_marker_cleanup "$dir"
    return "$rc"
}

if [ "${1:-}" = "--selftest" ] && [ "$#" -eq 1 ]; then
    contido_selftest
    exit $?
fi
contido_main "$@"
exit $?
