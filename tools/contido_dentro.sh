#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/contido_dentro.sh - the half of tools/contido.sh that runs INSIDE the containment
# (CI-SPLIT-PER-OS A5, PLANO-C1.md, C1b v2 by L34-RETRO.md R2-final). It is never called by hand.
#
# Usage: contido_dentro.sh <mode> <arg> <N> <S> <G> <dir> -- command [args...]
#   mode  proprio       contido.sh created the scope (Delegate=pids). <arg> is the unit name. Proof by the
#                       object: pids.max == N and the cgroup name == <arg>.scope. Then: mkdir supervisor/
#                       and carga/, move itself to supervisor/ (no internal process rule), `+pids` on the
#                       scope root, carga/pids.max = N, and the command runs in carga/. Kills carga/.
#         ninho         nested inside a DELEGATED contido scope. <arg> is that scope's cgroup path. Creates
#                       the SIBLING ninho-*/ of the outer carga/ with pids.max = N (the effective ceiling is
#                       the smaller, by the kernel hierarchy) and kills only it. A ninho under carga/ was
#                       measured and does not work (EOPNOTSUPP, P7B).
#         herdado-pgid  a numeric ceiling of somebody else's cgroup, without delegation (a container with
#                       --pids-limit): the command runs under setsid and the kill is by process group. A
#                       setsid grandchild ESCAPES the deadline here; the printed mode says so, always.
#   N the task ceiling asked for; S the deadline in seconds; G the grace between TERM and KILL, in seconds.
# The kill is ALWAYS aimed at a sub-cgroup this script created (carga/ or ninho-*/), never at the scope root
# nor at a cgroup systemd created: TERM to every direct pid of it, a wait UP TO G that ends as soon as
# cgroup.events says `populated 0` in ALL the targets (it sees the descendants; cgroup.procs does not), then
# cgroup.kill ALWAYS, even on an empty cgroup: skipping it leaves alive a descendant that ignores TERM. The mode
# proprio kills {carga/, every ninho-* under the scope root}, because a nested contido puts its ninho BESIDE
# carga/ and its own supervisor may have died. `populated 0` is monotonic here: a cgroup that is empty only fills
# again by migration, and only this supervisor or a nested contido that already died writes to its cgroup.procs.
# Exit codes: the command's own (128+n on a signal), 124 deadline, 71 wrong scope, a cgroup step that failed
# (nothing ran) or an invalid kill target, 72 broken clock, 2 usage. (73 is the caller's: this half died.)
# Always (one single way out): writes <dir>/fim (`rc=<n>`, `prazo` or `relogio`), prints the final
# `contido:` line on stderr, then kills what is left of the command.
#
# One single wait, no loop: the command runs in the background and reports its exit status through
# a pipe kept open on both sides; the deadline is ONE `read -t`. A read that returns with neither data
# nor timeout (broken descriptor) is a broken clock and fails closed.
#
# Identifiers are English (project L-21); the printed messages are Portuguese and fixed.

# Test hook: the selftest redefines it (in a subshell) to break the descriptor before the read.
contido_hook_before_read() { :; }

# contido_own_cgroup: prints the own cgroup path (line `0::` of /proc/self/cgroup) or nothing.
contido_own_cgroup() {
    local line
    while read -r line; do
        case "$line" in 0::*) echo "${line#0::}"; return 0 ;; esac
    done </proc/self/cgroup
    return 1
}

# contido_read_first <file>: the first word of a file, or `ilegivel`.
contido_read_first() {
    local value
    if read -r value <"$1" 2>/dev/null && [ -n "$value" ]; then
        echo "$value"
    else
        echo "ilegivel"
    fi
}

# contido_events_max <cgroup path>: the `max` counter of pids.events (forks refused), or `ilegivel`.
contido_events_max() {
    local key value
    while read -r key value; do
        [ "$key" = "max" ] && { echo "$value"; return 0; }
    done 2>/dev/null <"/sys/fs/cgroup$1/pids.events"
    echo "ilegivel"
}

# contido_pgrp_of <pid>: the process group of a pid (field 5 of /proc/<pid>/stat), or nothing.
contido_pgrp_of() {
    local line
    read -r line 2>/dev/null <"/proc/$1/stat" || return 1
    line="${line##*) }"
    read -r _ _ REPLY _ <<<"$line"
    echo "$REPLY"
}

# contido_scope_delegated <absolute scope dir>: 0 when the scope was delegated to us AND carries our layout: the
# root is writable, `pids` is listed in its cgroup.subtree_control and supervisor/ exists (the mark of contido).
# Otherwise 1, with the missing piece in REPLY (raiz-nao-gravavel | sem-pids | sem-supervisor).
contido_scope_delegated() {
    local line
    [ -w "$1/cgroup.subtree_control" ] || { REPLY="raiz-nao-gravavel"; return 1; }
    read -r line <"$1/cgroup.subtree_control" 2>/dev/null
    case " $line " in *" pids "*) ;; *) REPLY="sem-pids"; return 1 ;; esac
    [ -d "$1/supervisor" ] || { REPLY="sem-supervisor"; return 1; }
    return 0
}

# contido_setup_proprio <own cgroup path> <N>: the order is the one of probe P6 and cannot change. Sets TARGET.
contido_setup_proprio() {
    local abs="/sys/fs/cgroup$1"
    mkdir "$abs/supervisor" "$abs/carga" 2>/dev/null || return 1
    echo "$$" >"$abs/supervisor/cgroup.procs" 2>/dev/null || return 1
    echo "+pids" >"$abs/cgroup.subtree_control" 2>/dev/null || return 1
    echo "$2" >"$abs/carga/pids.max" 2>/dev/null || return 1
    TARGET="$1/carga"
}

# contido_setup_ninho <scope cgroup path> <N>: a sibling of the outer carga/. Sets TARGET.
contido_setup_ninho() {
    local abs="/sys/fs/cgroup$1" name
    contido_scope_delegated "$abs" || return 1
    name="ninho-$$-${EPOCHREALTIME//[.,]/}"
    mkdir "$abs/$name" 2>/dev/null || return 1
    echo "$2" >"$abs/$name/pids.max" 2>/dev/null || return 1
    TARGET="$1/$name"
}

# contido_populated <absolute cgroup dir>: 0 while cgroup.events says `populated 1` (live processes in the cgroup
# or in ANY descendant; cgroup.procs lists only the direct ones). 1 when it says `populated 0` or the cgroup is gone.
# Unreadable or without the line counts as populated: the wait then runs to G and the KILL follows (fail closed).
contido_populated() {
    local key value
    [ -d "$1" ] || return 1
    while read -r key value; do
        if [ "$key" = populated ]; then
            [ "$value" = 0 ] && return 1
            return 0
        fi
    done 2>/dev/null <"$1/cgroup.events"
    return 0
}

# contido_wait_bounded <G> <predicate> [args...]: waits UP TO G seconds, ending as soon as `predicate args...`
# returns 0. 0 when it did, 1 when G ran out (or the clock broke). A bounded loop of `read -t 0.1` on the clock
# descriptor (no process per turn, at most 10*G turns) with the rotation guard of L-11 6(a): three turns in a row
# that did not sleep end the wait (broken clock) and the caller goes on to what follows. The one wait of the file:
# the TERM grace, the wait after the KILL and the wait for a cgroup to empty are all this loop.
contido_wait_bounded() {
    local g="$1" turns=0 fast=0 t0 t1
    shift
    while [ "$turns" -lt $((g * 10)) ]; do
        "$@" && return 0
        t0="${EPOCHREALTIME//[.,]/}"
        read -r -t 0.1 -u "$CONTIDO_FD" _ 2>/dev/null
        t1="${EPOCHREALTIME//[.,]/}"
        if [ $((t1 - t0)) -lt 50000 ]; then
            fast=$((fast + 1))
            [ "$fast" -ge 3 ] && return 1
        else
            fast=0
        fi
        turns=$((turns + 1))
    done
    return 1
}

# contido_all_empty <absolute cgroup dir...>: 0 when none of them is populated.
contido_all_empty() {
    local dir
    for dir in "$@"; do contido_populated "$dir" && return 1; done
    return 0
}

# contido_wait_empty <G> <absolute cgroup dir...>: waits UP TO G seconds, ending as soon as none is populated.
contido_wait_empty() {
    local g="$1"
    shift
    contido_wait_bounded "$g" contido_all_empty "$@"
}

# contido_live_in_group <pgrp> [proc root, default /proc: the selftest feeds it a fake tree]: sets CONTIDO_LIVE to how many LIVE members the process group has. Live means the
# state of /proc/<pid>/stat (the field after the last `)`) is not Z or X, and the group (field 5) is <pgrp>. A
# zombie is not live: it already died, and a container whose PID 1 does not reap keeps it forever (`kill -0 --
# -pgrp` sees it, this does not). ONE sweep over /proc/[0-9]*, entirely of shell builtins (a glob, `read`, a
# here-string): NO process per item swept (GODS_LAWS.md L-11: a design that spends a process per pid brought the
# machine down on 29/09/2026). A pid that vanishes between the glob and the read is skipped.
contido_live_in_group() {
    local dir line state ppid group root="${2:-/proc}"
    CONTIDO_LIVE=0
    for dir in "$root"/[0-9]*; do
        read -r line 2>/dev/null <"$dir/stat" || continue
        line="${line##*) }"
        read -r state ppid group _ <<<"$line"
        [ "$group" = "$1" ] || continue
        case "$state" in Z | X) ;; *) CONTIDO_LIVE=$((CONTIDO_LIVE + 1)) ;; esac
    done
}

# contido_group_has_no_live_member <pgrp>: the predicate of the wait after the KILL, in herdado-pgid mode; leaves
# CONTIDO_LIVE set. FAST PATH: `kill -0 -- -pgrp` (a builtin) failing means nobody is in the group, not even a
# zombie, so nothing is swept; the sweep runs only while kill -0 still answers (a live member OR a zombie: the sweep
# is what tells them apart). In the normal case that is zero sweeps.
contido_group_has_no_live_member() {
    if ! kill -0 -- "-$1" 2>/dev/null; then
        CONTIDO_LIVE=0
        return 0
    fi
    contido_live_in_group "$1"
    [ "$CONTIDO_LIVE" -eq 0 ]
}

# contido_group_is_gone <pgrp>: the predicate of the TERM grace, in herdado-pgid mode (`kill -0` still sees a
# zombie, which is fine there: the KILL follows either way).
contido_group_is_gone() {
    ! kill -0 -- "-$1" 2>/dev/null
}

# contido_cgroup_survivors <absolute cgroup dir...>: sets CONTIDO_SURVIVORS to how many processes are still in the
# cgroups that are still populated (the direct ones listed in cgroup.procs; a cgroup populated only through a
# descendant, or one whose cgroup.procs already reads empty while the kernel still tears it down, counts as 1).
contido_cgroup_survivors() {
    local dir pid found
    CONTIDO_SURVIVORS=0
    for dir in "$@"; do
        contido_populated "$dir" || continue
        found=0
        while read -r pid; do found=$((found + 1)); done 2>/dev/null <"$dir/cgroup.procs"
        [ "$found" -eq 0 ] && found=1
        CONTIDO_SURVIVORS=$((CONTIDO_SURVIVORS + found))
    done
}

# contido_matar <mode> <G> <target...>: the ONLY place that kills. proprio|ninho: the targets are cgroups this
# script created (carga/ or ninho-*/), TERM to every direct pid, a wait UP TO G for `populated 0` in ALL of them,
# then cgroup.kill ALWAYS (idempotent and cheap on an empty cgroup; skipping it leaves a descendant that ignores
# TERM alive). A target that is not carga or ninho-* (for instance the scope root) is refused with 71, and nothing
# is written. herdado-pgid: the target is the process group; TERM, wait for the group to vanish, KILL always.
# AFTER the KILL, both wait (bounded by max(1, G) s) until the tree is really over, because the KILL is
# asynchronous, and leave CONTIDO_SURVIVORS = how many are still there (0 in the normal case): D-C1b-6.
contido_matar() {
    local mode="$1" g="$2" target abs pid first dirs=() after=$(($2 > 1 ? $2 : 1))
    CONTIDO_SURVIVORS=0
    CONTIDO_AFTER_KILL="$after"
    shift 2
    case "$mode" in
        proprio|ninho)
            for target in "$@"; do
                case "$target" in */carga|*/ninho-*) ;; *) echo "contido: FALHA - alvo de matanca invalido '$target'; nada foi morto" >&2; return 71 ;; esac
            done
            for target in "$@"; do
                abs="/sys/fs/cgroup$target"
                [ -d "$abs" ] || continue
                dirs+=("$abs")
                if read -r first <"$abs/cgroup.procs" 2>/dev/null && [ -n "$first" ]; then
                    while read -r pid; do
                        [ "$pid" = "$$" ] || [ "$pid" = "$BASHPID" ] || kill -TERM "$pid" 2>/dev/null
                    done <"$abs/cgroup.procs"
                fi
            done
            contido_wait_empty "$g" "${dirs[@]}"
            for abs in "${dirs[@]}"; do
                echo 1 >"$abs/cgroup.kill" 2>/dev/null
            done
            # The KILL is asynchronous: cgroup.kill returns before the tasks are gone. Wait, bounded, for the
            # kernel's own word (`populated 0`, never cgroup.procs, which reads empty first), and only then
            # remove a ninho (rmdir of a populated cgroup fails and would leave it behind).
            contido_wait_empty "$after" "${dirs[@]}"
            contido_cgroup_survivors "${dirs[@]}"
            case "$mode" in ninho) for abs in "${dirs[@]}"; do rmdir "$abs" 2>/dev/null; done ;; esac
            return 0
            ;;
        herdado-pgid)
            # the group must be numeric, above 1 and different from the caller's own group
            case "$1" in ''|*[!0-9]*) return 1 ;; esac
            [ "$1" -gt 1 ] || return 1
            kill -TERM -- "-$1" 2>/dev/null && contido_wait_bounded "$g" contido_group_is_gone "$1"
            kill -KILL -- "-$1" 2>/dev/null
            # The KILL is asynchronous too (kill(2) returns once the signal is queued): wait, bounded, until
            # the group has no LIVE member, so that returning means the tree is over - or says how many are left.
            contido_wait_bounded "$after" contido_group_has_no_live_member "$1"
            contido_group_has_no_live_member "$1"
            CONTIDO_SURVIVORS="$CONTIDO_LIVE"
            return 0
            ;;
    esac
}

# contido_refuse <dir> <reason> <message>: a refusal of THIS half (nothing ran, or nothing that mattered): the marker
# says `recusa:<reason>` BEFORE the exit 71, so the caller maps the 71 by the marker and never by the bare number
# (a bare 71 with the unit still active is a dead supervisor, not a refusal).
contido_refuse() {
    printf 'recusa:%s\n' "$2" >"$1/fim" 2>/dev/null
    echo "$3" >&2
    return 71
}

# contido_inside_main <mode> <arg> <N> <S> <G> <dir> -- command [args...]
contido_inside_main() {
    local mode="${1:-}" arg="${2:-}" n="${3:-}" s="${4:-}" g="${5:-}" dir="${6:-}"
    case "$mode" in proprio|ninho|herdado-pgid) ;; *) echo "contido: modo desconhecido '$mode'" >&2; return 2 ;; esac
    case "$n" in ''|*[!0-9]*) echo "contido: N invalido '$n'" >&2; return 2 ;; esac
    case "$s" in ''|*[!0-9]*) echo "contido: S invalido '$s'" >&2; return 2 ;; esac
    case "$g" in ''|*[!0-9]*) echo "contido: G invalido '$g'" >&2; return 2 ;; esac
    [ -d "$dir" ] || { echo "contido: diretorio de marcador '$dir' nao existe" >&2; return 2; }
    [ "${7:-}" = "--" ] || { echo "contido: falta o -- antes do comando" >&2; return 2; }
    shift 7
    [ "$#" -gt 0 ] || { echo "contido: comando vazio" >&2; return 2; }

    local own_cgroup own_pids_max TARGET="" mode_label="$mode"
    own_cgroup="$(contido_own_cgroup)"
    case "$mode" in
        proprio)
            # proof by the object BEFORE moving anywhere: the ceiling and the name of the scope contido.sh just made
            own_pids_max="$(contido_read_first "/sys/fs/cgroup$own_cgroup/pids.max")"
            if [ "$own_pids_max" != "$n" ] || [ "${own_cgroup##*/}" != "$arg.scope" ]; then
                contido_refuse "$dir" escopo-errado "contido: FALHA - escopo errado (pids.max=$own_pids_max, cgroup=${own_cgroup##*/}); nada foi executado"
                return 71
            fi
            contido_setup_proprio "$own_cgroup" "$n" || { contido_refuse "$dir" preparo-falhou "contido: FALHA - nao consegui preparar supervisor/ e carga/ em $own_cgroup; nada foi executado"; return 71; }
            ;;
        ninho)
            contido_setup_ninho "$arg" "$n" || { contido_refuse "$dir" escopo-nao-delegado "contido: FALHA - escopo '$arg' nao e' delegado ou nao aceita subpasta; nada foi executado"; return 71; }
            ;;
        herdado-pgid)
            mode_label="herdado-pgid (sem delegacao: um setsid escapa do prazo, o teto do cgroup alheio continua valendo)"
            ;;
    esac

    # the pipe is made AFTER the move, so no process substitution child is born in the scope root
    local pipe_fd
    exec {pipe_fd}<> <(:)
    CONTIDO_FD="$pipe_fd"
    exec 3<&0

    local start_us="${EPOCHREALTIME//[.,]/}"
    # The command runs in the background and reports first its pid, then its exit status. In cgroup modes its
    # subshell first moves itself into TARGET (a failure reports `erro` and runs nothing); in herdado-pgid it
    # runs under setsid, so that pid is also its process group (`exec setsid` keeps the pid of the subshell).
    local wrapper='read -r l </proc/$$/stat; l="${l##*) }"; read -r _ _ g _ <<<"$l"; echo "pid $$ $g" >&"$0"; "$@" <&3; echo "rc $?" >&"$0"'
    if [ "$mode" = herdado-pgid ]; then
        ( exec setsid bash -c "$wrapper" "$pipe_fd" "$@" ) &
    else
        ( echo "$BASHPID" >"/sys/fs/cgroup$TARGET/cgroup.procs" 2>/dev/null || { echo "erro mover" >&"$pipe_fd"; exit 71; }
          exec bash -c "$wrapper" "$pipe_fd" "$@" ) &
    fi

    local tag value reported_pgrp pgrp="" outcome="" cmd_rc="" waited
    read -r -t "$s" -u "$pipe_fd" tag value reported_pgrp
    if [ "$tag" = erro ]; then
        contido_refuse "$dir" mover-falhou "contido: FALHA - o comando nao conseguiu entrar em $TARGET; nada foi executado"
        contido_matar "$mode" "$g" "$TARGET"
        return 71
    fi
    if [ "$tag" = pid ]; then
        pgrp="$value"
        if [ "$mode" = herdado-pgid ]; then
            # the command reported its own group before running anything: no race with its exit
            local own_pgrp
            own_pgrp="$(contido_pgrp_of "$$")"
            if [ "$reported_pgrp" != "$pgrp" ] || [ "$reported_pgrp" = "$own_pgrp" ]; then
                contido_refuse "$dir" pgrp-nao-novo "contido: FALHA - o comando nao ficou num grupo de processos novo (pgrp=$reported_pgrp, o do chamador=$own_pgrp); nada foi morto"
                return 71
            fi
        fi
    fi
    contido_hook_before_read
    waited=$(((${EPOCHREALTIME//[.,]/} - start_us) / 1000000))
    if [ "$waited" -ge "$s" ]; then
        outcome=prazo
    else
        read -r -t "$((s - waited))" -u "$pipe_fd" tag value
        local read_rc=$?
        if [ "$read_rc" -eq 0 ] && [ "$tag" = rc ]; then
            outcome="rc=$value"; cmd_rc="$value"
        elif [ "$read_rc" -gt 128 ]; then
            outcome=prazo
        else
            outcome=relogio
        fi
    fi

    local elapsed peak_path="$own_cgroup"
    elapsed=$(((${EPOCHREALTIME//[.,]/} - start_us) / 1000000))
    [ -n "$TARGET" ] && peak_path="$TARGET"
    printf '%s\n' "$outcome" >"$dir/fim"
    local final_line
    final_line="contido: modo=$mode_label, pico=$(contido_read_first "/sys/fs/cgroup$peak_path/pids.peak") de $n, recusas_de_fork=$(contido_events_max "$peak_path"), fim=$outcome, tempo=${elapsed}s"
    local kill_rc=0 scope_abs="/sys/fs/cgroup$own_cgroup" ninho
    case "$mode" in
        herdado-pgid) contido_matar "$mode" "$g" "$pgrp"; kill_rc=$? ;;
        # the supervisor of a scope also owns the ninhos nested contidos made under it: an inner supervisor
        # that died would leave one behind (they sit beside carga/, out of its reach)
        proprio)
            local targets=("$TARGET")
            for ninho in "$scope_abs"/ninho-*; do [ -d "$ninho" ] && targets+=("${ninho#/sys/fs/cgroup}"); done
            contido_matar "$mode" "$g" "${targets[@]}"; kill_rc=$? ;;
        *) contido_matar "$mode" "$g" "$TARGET"; kill_rc=$? ;;
    esac
    # printed AFTER the kill, so that it can say how many processes outlived it (0 in the normal case: the
    # absence is declared AND counted); the marker above was written before, so it survives a stuck kill
    echo "$final_line, sobreviventes_apos_kill=$CONTIDO_SURVIVORS" >&2
    if [ "$CONTIDO_SURVIVORS" -gt 0 ]; then
        echo "contido: AVISO - $CONTIDO_SURVIVORS processo(s) sobreviveram ao KILL por ${CONTIDO_AFTER_KILL}s; o codigo de saida do comando nao foi alterado" >&2
    fi
    [ "$kill_rc" -eq 71 ] && return 71
    case "$outcome" in
        rc=*) return "$cmd_rc" ;;
        prazo) return 124 ;;
        *) return 72 ;;
    esac
}

# contido_alive <pid>: 0 when the process exists and is NOT a zombie (state Z) or dead (X), read from
# /proc/<pid>/stat (the field after the last `)`). `kill -0` alone is not enough: a container whose PID 1 does not
# reap children (the job container of the GitHub Actions runs `tail -f /dev/null`) keeps every orphaned grandchild
# as a zombie after it died, and `kill -0` succeeds on a zombie. Used by the selftest only.
contido_alive() {
    local line
    case "$1" in ''|*[!0-9]*) return 1 ;; esac
    read -r line 2>/dev/null <"/proc/$1/stat" || return 1
    line="${line##*) }"
    case "${line%% *}" in Z|X) return 1 ;; esac
    return 0
}

# contido_fabricate_zombie <dir>: makes a zombie DETERMINISTICALLY, for the selftest. Sets ZOMBIE_PARENT (a live
# process that has become a `sleep` and will never reap) and ZOMBIE_PID (its child, held alive until the parent has
# exec'ed and only then released, so it exits as the zombie of a parent that does not reap). Returns 0 when the child
# is in state Z, 1 otherwise (every wait is bounded, on the clock descriptor). The earlier form (`sleep 0 &` then
# `exec sleep`) raced: a child that exits BEFORE the parent's exec is reaped by the parent bash itself (its SIGCHLD
# handler), and no zombie is left - measured 30 of 30 when the parent is delayed 50 ms, and seen once in the CI.
# Needs CONTIDO_FD (the clock). The caller kills ZOMBIE_PARENT when done.
contido_fabricate_zombie() {
    local dir="$1" turn comm line state=""
    rm -f "$dir/zumbi.pid" "$dir/zumbi.go"
    ZOMBIE_PID=""
    bash -c 'exec {c}<> <(:); ( for ((i = 0; i < 100; i++)); do [ -e "$1" ] && break; read -r -t 0.05 -u "$c" _; done ) & echo $! >"$0"; exec sleep 5' "$dir/zumbi.pid" "$dir/zumbi.go" >/dev/null 2>&1 &
    ZOMBIE_PARENT=$!
    for turn in {1..50}; do
        read -r comm 2>/dev/null <"/proc/$ZOMBIE_PARENT/comm"
        [ "$comm" = sleep ] && break
        read -r -t 0.1 -u "$CONTIDO_FD" _ 2>/dev/null
    done
    [ "$comm" = sleep ] || return 1
    read -r ZOMBIE_PID 2>/dev/null <"$dir/zumbi.pid" || return 1
    : >"$dir/zumbi.go"
    for turn in {1..50}; do
        if read -r line 2>/dev/null <"/proc/$ZOMBIE_PID/stat"; then
            line="${line##*) }"
            state="${line%% *}"
            [ "$state" = Z ] && return 0
        fi
        read -r -t 0.1 -u "$CONTIDO_FD" _ 2>/dev/null
    done
    return 1
}

# --- selftest -----------------------------------------------------------------------------
# The cases run in mode `herdado-pgid`, which kills by process group and never touches cgroup.kill, so it is
# safe in whatever cgroup the caller is in (the ctest, the preci). Mode `ninho` runs only when the own cgroup
# is a child of a DELEGATED contido scope (otherwise the skipped count is printed, ALWAYS); mode `proprio`
# is exercised by the dynamic proof (tests/tools/contencao/prova_contido.sh), outside the ctest.
# contido_inside_selftest [group]: with no group, every group runs (manual use and the preci); with one, only
# that one (a ctest entry each, D-CI2-SELFTEST: a long test of WAITS is split, never shortened - the deadlines
# and graces were sized against the D10b intermittency). The groups were fixed by measurement BEFORE the split
# (each at most 20 s of wall time on an idle machine): basico (exit codes, streams, the marker, refusals),
# prazos (deadlines, locales, the discount of elapsed time), graca (grace, the bounded wait, the KILL that
# lands late), varredura (the /proc sweep, the positive controls of contido_alive, the ninho). The count of
# cases per group and the total are ALWAYS printed, zero included.
contido_inside_selftest() {
    local want_group="${1:-}" k_groups="basico prazos graca varredura"
    if [ -n "$want_group" ]; then
        case " $k_groups " in
            *" $want_group "*) ;;
            *) echo "contido_dentro --selftest: grupo desconhecido '$want_group' (validos: $k_groups)" >&2; return 2 ;;
        esac
    fi
    local self dir cases=0 failures=0 got want rc line own_pgrp cmd_pgrp t0 t1 tw0 tw1 g_start=0 skipped=0
    local -A group_cases=([basico]=0 [prazos]=0 [graca]=0 [varredura]=0)
    self="$(cd -- "${BASH_SOURCE[0]%/*}" && pwd)/${BASH_SOURCE[0]##*/}"
    dir="$(mktemp -d "${TMPDIR:-/var/tmp}/glintfx-contido-selftest.XXXXXX")" || return 1

    check() {  # check <name> <ok: 0|1> <detail>
        cases=$((cases + 1))
        if [ "$2" -ne 0 ]; then
            failures=$((failures + 1))
            echo "contido_dentro --selftest: FALHOU - $1: $3" >&2
        fi
    }
    selftest_wants() { [ -z "$want_group" ] || [ "$want_group" = "$1" ]; }
    # the clock descriptor every group's waits use (a pipe nobody writes to: `read -t` on it is a sleep)
    exec {CONTIDO_FD}<> <(:)
    # run_inside <marker dir name> <deadline> <cmd...>: runs the script itself in herdado-pgid mode
    run_inside() {
        local sub="$1" s="$2"
        shift 2
        mkdir -p "$dir/$sub"
        bash "$self" herdado-pgid - 64 "$s" 1 "$dir/$sub" -- "$@" >"$dir/$sub/out" 2>"$dir/$sub/err"
    }

    if selftest_wants basico; then
    g_start=$cases
    # 1. the exit status crosses: 0, 1, 2, 77 and 127 (command not found)
    for want in 0 1 2 77; do
        run_inside "rc$want" 5 bash -c "exit $want"; rc=$?
        check "rc $want crosses" "$([ "$rc" -eq "$want" ] && echo 0 || echo 1)" "got $rc"
    done
    run_inside rc127 5 bash -c "exec contido-no-such-command-xyz"; rc=$?
    check "rc 127 crosses" "$([ "$rc" -eq 127 ] && echo 0 || echo 1)" "got $rc"
    # 2. death by signal crosses as 128+n
    run_inside sig 5 bash -c 'kill -TERM $$'; rc=$?
    check "SIGTERM crosses as 143" "$([ "$rc" -eq 143 ] && echo 0 || echo 1)" "got $rc"
    # 3. stdin crosses
    mkdir -p "$dir/stdin"
    got="$(printf abc | bash "$self" herdado-pgid - 64 5 1 "$dir/stdin" -- cat 2>/dev/null)"
    check "stdin crosses" "$([ "$got" = abc ] && echo 0 || echo 1)" "got [$got]"
    # 4. argv reaches the command literally (no expansion of ${HOME} or $$)
    run_inside argv 5 printf '%s' '${HOME}$$'
    got="$(cat "$dir/argv/out")"
    check "argv literal" "$([ "$got" = '${HOME}$$' ] && echo 0 || echo 1)" "got [$got]"
    # 5. the marker file records the outcome
    got="$(cat "$dir/rc77/fim" 2>/dev/null)"
    check "fim records rc=77" "$([ "$got" = "rc=77" ] && echo 0 || echo 1)" "got [$got]"
    # 6. the final line is printed on stderr, with the mode and the outcome
    grep -q '^contido: modo=herdado-pgid (sem delegacao.*fim=rc=0, tempo=' "$dir/rc0/err"
    check "final line for a normal end" "$?" "stderr: $(cat "$dir/rc0/err")"
    # 7. the command runs in a NEW process group, different from the caller's
    read -r line <"/proc/$$/stat"; line="${line##*) }"; read -r _ _ own_pgrp _ <<<"$line"
    run_inside pgrp 5 bash -c 'read -r l </proc/$$/stat; l="${l##*) }"; read -r _ _ g _ <<<"$l"; echo "$g"'
    cmd_pgrp="$(cat "$dir/pgrp/out")"
    check "command pgrp differs from the caller's" "$([ -n "$cmd_pgrp" ] && [ "$cmd_pgrp" != "$own_pgrp" ] && echo 0 || echo 1)" "own=$own_pgrp command=$cmd_pgrp"
    group_cases[basico]=$((group_cases[basico] + cases - g_start))
    fi
    if selftest_wants prazos; then
    g_start=$cases
    # 8. deadline: sleep 3 with S=1 gives fim=prazo, rc 124, and the sleep is dead afterwards
    run_inside prazo 1 bash -c 'sleep 3 >/dev/null 2>&1 & echo $! >"$0"; wait' "$dir/prazo/pid"; rc=$?
    check "deadline rc 124" "$([ "$rc" -eq 124 ] && echo 0 || echo 1)" "got $rc"
    got="$(cat "$dir/prazo/fim" 2>/dev/null)"
    check "deadline fim=prazo" "$([ "$got" = prazo ] && echo 0 || echo 1)" "got [$got]"
    check "deadline left a pid to check" "$([ -s "$dir/prazo/pid" ] && echo 0 || echo 1)" "no pid file"
    contido_alive "$(cat "$dir/prazo/pid" 2>/dev/null)"
    check "deadline killed the sleep" "$([ $? -ne 0 ] && [ -s "$dir/prazo/pid" ] && echo 0 || echo 1)" "the sleep is still alive"
    # 9. a grandchild left behind is killed at the end, even on a normal exit
    run_inside neto 5 bash -c 'sleep 3 >/dev/null 2>&1 & echo $! >"$0"; exit 0' "$dir/neto/pid"; rc=$?
    check "grandchild case rc 0" "$([ "$rc" -eq 0 ] && echo 0 || echo 1)" "got $rc"
    check "grandchild left a pid to check" "$([ -s "$dir/neto/pid" ] && echo 0 || echo 1)" "no pid file"
    contido_alive "$(cat "$dir/neto/pid" 2>/dev/null)"
    check "grandchild killed at the end" "$([ $? -ne 0 ] && [ -s "$dir/neto/pid" ] && echo 0 || echo 1)" "the grandchild is still alive"
    # 10. broken clock: the descriptor closed before the read gives fim=relogio, rc 72, in at most 1 s
    mkdir -p "$dir/relogio"
    t0="${EPOCHREALTIME//[.,]/}"
    (
        contido_hook_before_read() { exec {CONTIDO_FD}>&-; }
        contido_inside_main herdado-pgid - 64 30 0 "$dir/relogio" -- sleep 20
    ) >/dev/null 2>&1; rc=$?
    t1="${EPOCHREALTIME//[.,]/}"
    check "broken clock rc 72" "$([ "$rc" -eq 72 ] && echo 0 || echo 1)" "got $rc"
    got="$(cat "$dir/relogio/fim" 2>/dev/null)"
    check "broken clock fim=relogio" "$([ "$got" = relogio ] && echo 0 || echo 1)" "got [$got]"
    check "broken clock is fast (<= 1 s)" "$([ $((t1 - t0)) -le 1000000 ] && echo 0 || echo 1)" "took $((t1 - t0)) us"
    # 10b. the deadline measures ~1 s in BOTH radix locales (PLANO-C1 amendment 8): $EPOCHREALTIME uses the
    #      locale radix (a COMMA under pt_BR), and a `${t/./}` would turn it into garbage. One case with
    #      the environment's locale, one with LC_ALL=C forced; each must take between 0,9 s and 2,5 s.
    local radix_note="ponto" locale_case
    case "$EPOCHREALTIME" in *,*) radix_note="virgula" ;; esac
    for locale_case in ambiente C; do
        mkdir -p "$dir/loc_$locale_case"
        t0="${EPOCHREALTIME//[.,]/}"
        if [ "$locale_case" = C ]; then
            LC_ALL=C bash "$self" herdado-pgid - 64 1 0 "$dir/loc_$locale_case" -- sleep 20 >/dev/null 2>&1; rc=$?
        else
            bash "$self" herdado-pgid - 64 1 0 "$dir/loc_$locale_case" -- sleep 20 >/dev/null 2>&1; rc=$?
        fi
        t1="${EPOCHREALTIME//[.,]/}"
        check "deadline S=1 with locale $locale_case: rc 124" "$([ "$rc" -eq 124 ] && echo 0 || echo 1)" "got $rc"
        check "deadline S=1 with locale $locale_case: between 0.9 s and 2.5 s" "$([ $((t1 - t0)) -ge 900000 ] && [ $((t1 - t0)) -le 2500000 ] && echo 0 || echo 1)" "took $((t1 - t0)) us"
    done
    # 10c. the time already spent before the wait is discounted: S=2 with a 1 s pause before the read
    #      (the hook) ends in about 2 s in total, not 3 s. This is the case that bites `${t/./}`: under a
    #      comma radix the elapsed time would count as zero and the total would be ~3 s.
    for locale_case in ambiente C; do
        mkdir -p "$dir/desc_$locale_case"
        t0="${EPOCHREALTIME//[.,]/}"
        (
            [ "$locale_case" = C ] && export LC_ALL=C
            contido_hook_before_read() { sleep 1; }
            contido_inside_main herdado-pgid - 64 2 0 "$dir/desc_$locale_case" -- sleep 20
        ) >/dev/null 2>&1; rc=$?
        t1="${EPOCHREALTIME//[.,]/}"
        check "discount of elapsed time, locale $locale_case: rc 124" "$([ "$rc" -eq 124 ] && echo 0 || echo 1)" "got $rc"
        check "discount of elapsed time, locale $locale_case: total 1.9 s to 2.5 s" "$([ $((t1 - t0)) -ge 1900000 ] && [ $((t1 - t0)) -le 2500000 ] && echo 0 || echo 1)" "took $((t1 - t0)) us"
    done
    echo "contido_dentro --selftest: radix do ambiente: $radix_note (so com virgula o caso do ambiente exercita o mutante \${t/./})" >&2

    group_cases[prazos]=$((group_cases[prazos] + cases - g_start))
    fi
    if selftest_wants basico; then
    g_start=$cases
    # 11. mode proprio with the wrong unit refuses with 71 BEFORE running (and before any kill)
    mkdir -p "$dir/errado"
    bash "$self" proprio unidade-falsa 64 5 1 "$dir/errado" -- touch "$dir/errado/marca" >/dev/null 2>"$dir/errado/err"; rc=$?
    check "proprio with the wrong scope refuses 71" "$([ "$rc" -eq 71 ] && echo 0 || echo 1)" "got $rc"
    check "proprio refusal did not run the command" "$([ ! -e "$dir/errado/marca" ] && echo 0 || echo 1)" "the marker exists"
    grep -q 'FALHA - escopo errado' "$dir/errado/err"
    check "proprio refusal message" "$?" "stderr: $(cat "$dir/errado/err")"
    got="$(cat "$dir/errado/fim" 2>/dev/null)"
    check "proprio refusal writes the marker recusa:escopo-errado" "$([ "$got" = "recusa:escopo-errado" ] && echo 0 || echo 1)" "got [$got]"
    # 12. invalid mode or a missing -- refuses with 2
    bash "$self" bogus - 64 5 1 "$dir" -- true >/dev/null 2>&1; rc=$?
    check "unknown mode refuses 2" "$([ "$rc" -eq 2 ] && echo 0 || echo 1)" "got $rc"
    bash "$self" herdado-pgid - 64 5 1 "$dir" true >/dev/null 2>&1; rc=$?
    check "missing -- refuses 2" "$([ "$rc" -eq 2 ] && echo 0 || echo 1)" "got $rc"

    # 12b. an invalid grace refuses with 2
    bash "$self" herdado-pgid - 64 5 abc "$dir" -- true >/dev/null 2>&1; rc=$?
    check "grace not a number refuses 2" "$([ "$rc" -eq 2 ] && echo 0 || echo 1)" "got $rc"
    group_cases[basico]=$((group_cases[basico] + cases - g_start))
    fi
    if selftest_wants graca; then
    g_start=$cases
    # 12c. the grace: TERM comes BEFORE the kill, so the command can clean up (deadline S=1, grace G=2)
    mkdir -p "$dir/graca"
    bash "$self" herdado-pgid - 64 1 2 "$dir/graca" -- bash -c 'trap "echo limpo >\"\$0\"; exit" TERM; sleep 60 & wait' "$dir/graca/marca" >/dev/null 2>&1; rc=$?
    check "grace: deadline rc 124" "$([ "$rc" -eq 124 ] && echo 0 || echo 1)" "got $rc"
    check "grace: the command cleaned up on TERM before the KILL" "$([ -s "$dir/graca/marca" ] && echo 0 || echo 1)" "the marker file is missing (KILL came without a TERM)"
    # 12d. mode ninho refuses with 71, running nothing, when the scope is not delegated
    mkdir -p "$dir/ninho_recusa"
    bash "$self" ninho /nao/existe 64 5 1 "$dir/ninho_recusa" -- touch "$dir/ninho_recusa/marca" >/dev/null 2>&1; rc=$?
    check "ninho on a scope that is not delegated refuses 71" "$([ "$rc" -eq 71 ] && echo 0 || echo 1)" "got $rc"
    check "ninho refusal did not run the command" "$([ ! -e "$dir/ninho_recusa/marca" ] && echo 0 || echo 1)" "the marker exists"
    got="$(cat "$dir/ninho_recusa/fim" 2>/dev/null)"
    check "ninho refusal writes the marker recusa:escopo-nao-delegado" "$([ "$got" = "recusa:escopo-nao-delegado" ] && echo 0 || echo 1)" "got [$got]"
    # an invalid marker directory is a usage error (2): no marker can exist and nothing runs
    bash "$self" herdado-pgid - 64 5 1 "$dir/nao-existe" -- touch "$dir/marca-dir-invalido" >/dev/null 2>&1; rc=$?
    check "invalid marker directory refuses 2, running nothing" "$([ "$rc" -eq 2 ] && [ ! -e "$dir/marca-dir-invalido" ] && echo 0 || echo 1)" "rc $rc"
    # 12e. the populated reader and the bounded wait, on a fake cgroup dir (no cgroup is touched)
    local fake="$dir/fake"
    mkdir -p "$fake"
    printf 'populated 1\nfrozen 0\n' >"$fake/cgroup.events"
    contido_populated "$fake"; check "populated 1 counts as populated" "$?" "rc $?"
    printf 'frozen 0\npopulated 0\n' >"$fake/cgroup.events"
    contido_populated "$fake"; rc=$?
    check "populated 0 (after another line) is empty" "$([ "$rc" -eq 1 ] && echo 0 || echo 1)" "rc $rc"
    : >"$fake/cgroup.events"
    contido_populated "$fake"; check "no populated line counts as populated (fail closed)" "$?" "rc $?"
    rm "$fake/cgroup.events"
    contido_populated "$fake"; check "unreadable cgroup.events counts as populated (fail closed)" "$?" "rc $?"
    contido_populated "$dir/nao-existe"; rc=$?
    check "a cgroup that is gone is empty" "$([ "$rc" -eq 1 ] && echo 0 || echo 1)" "rc $rc"
    printf 'populated 0\n' >"$fake/cgroup.events"
    tw0="${EPOCHREALTIME//[.,]/}"; contido_wait_empty 5 "$fake"; rc=$?; tw1="${EPOCHREALTIME//[.,]/}"
    check "wait on an empty cgroup returns at once (< 0.3 s, rc 0)" "$([ "$rc" -eq 0 ] && [ $((tw1 - tw0)) -lt 300000 ] && echo 0 || echo 1)" "rc $rc took $((tw1 - tw0)) us"
    printf 'populated 1\n' >"$fake/cgroup.events"
    : >"$fake/cgroup.procs"
    tw0="${EPOCHREALTIME//[.,]/}"; contido_wait_empty 1 "$fake"; rc=$?; tw1="${EPOCHREALTIME//[.,]/}"
    check "a populated cgroup with an EMPTY cgroup.procs (a live descendant) keeps the wait until G (0.9 s to 1.6 s, rc 1)" "$([ "$rc" -eq 1 ] && [ $((tw1 - tw0)) -ge 900000 ] && [ $((tw1 - tw0)) -le 1600000 ] && echo 0 || echo 1)" "rc $rc took $((tw1 - tw0)) us"
    tw0="${EPOCHREALTIME//[.,]/}"
    ( CONTIDO_FD=99; contido_wait_empty 5 "$fake" ) 2>/dev/null; rc=$?   # fd 99 is not open: the read fails at once
    tw1="${EPOCHREALTIME//[.,]/}"
    check "rotation guard: a broken clock ends the wait fast (< 1 s, rc 1)" "$([ "$rc" -eq 1 ] && [ $((tw1 - tw0)) -lt 1000000 ] && echo 0 || echo 1)" "rc $rc took $((tw1 - tw0)) us"
    contido_matar proprio 1 /user.slice/x.scope >/dev/null 2>&1; rc=$?
    check "the kill refuses a target that is not carga or ninho-* (71)" "$([ "$rc" -eq 71 ] && echo 0 || echo 1)" "rc $rc"
    # 12f. a normal end costs no grace (< 1 s in total), and a leader that leaves at once while a grandchild
    #      ignores TERM ends by the KILL after ~G (the mergetrain case)
    mkdir -p "$dir/rapido" "$dir/d10b"
    tw0="${EPOCHREALTIME//[.,]/}"
    bash "$self" herdado-pgid - 64 5 3 "$dir/rapido" -- true >/dev/null 2>&1; rc=$?
    tw1="${EPOCHREALTIME//[.,]/}"
    check "a normal end is fast (< 1 s with G=3)" "$([ "$rc" -eq 0 ] && [ $((tw1 - tw0)) -lt 1000000 ] && echo 0 || echo 1)" "rc $rc took $((tw1 - tw0)) us"
    tw0="${EPOCHREALTIME//[.,]/}"
    bash "$self" herdado-pgid - 64 5 2 "$dir/d10b" -- bash -c '(trap "" TERM; exec sleep 30) >/dev/null 2>&1 & echo $! >"$0"; exit 0' "$dir/d10b/pid" >/dev/null 2>"$dir/d10b/err"; rc=$?
    tw1="${EPOCHREALTIME//[.,]/}"
    check "D10b: the leader leaves, a TERM-ignoring grandchild dies by the KILL after ~G (1.9 s to 3.5 s)" "$([ "$rc" -eq 0 ] && [ $((tw1 - tw0)) -ge 1900000 ] && [ $((tw1 - tw0)) -le 3500000 ] && echo 0 || echo 1)" "rc $rc took $((tw1 - tw0)) us"
    contido_alive "$(cat "$dir/d10b/pid" 2>/dev/null)"
    check "D10b: the grandchild is dead" "$([ $? -ne 0 ] && [ -s "$dir/d10b/pid" ] && echo 0 || echo 1)" "the grandchild survived"
    grep -q 'sobreviventes_apos_kill=0$' "$dir/d10b/err"
    check "D10b: the final line says sobreviventes_apos_kill=0" "$?" "stderr: $(cat "$dir/d10b/err")"
    # 12f2. D-C1b-6: the KILL is asynchronous, so the product WAITS for its effect. Deterministic: `kill -KILL`
    #       is replaced (in this subshell only) by one that delivers the real KILL 0.6 s LATE, the way a loaded
    #       machine does; the read right after the return must already find the grandchild dead.
    # (the stubs read a variable of THEIR OWN name: `dir` would be shadowed by the one of contido_inside_main)
    local selftest_scan_log="$dir/kill_tardio/scans"
    # The stubs COUNT the passes and then run the REAL sweep (kept under another name): a stub that forced "1 live"
    # would call a zombie a survivor, which is exactly what happens in a container whose PID 1 does not reap (the
    # dead grandchild stays a zombie of the group, `kill -0` keeps answering, and the sweep is reached).
    eval "selftest_real_live_in_group() $(declare -f contido_live_in_group | tail -n +2)"
    mkdir -p "$dir/kill_tardio"
    (
        contido_live_in_group() { echo scan >>"$selftest_scan_log"; selftest_real_live_in_group "$@"; }
        kill() {
            if [ "$1" = -KILL ]; then
                ( sleep 0.6; command kill -KILL -- "$3" ) >/dev/null 2>&1 &
                return 0
            fi
            command kill "$@"
        }
        contido_inside_main herdado-pgid - 64 5 2 "$dir/kill_tardio" -- bash -c '(trap "" TERM; exec sleep 30) >/dev/null 2>&1 & echo $! >"$0"; exit 0' "$dir/kill_tardio/pid"
    ) >/dev/null 2>"$dir/kill_tardio/err"; rc=$?
    contido_alive "$(cat "$dir/kill_tardio/pid" 2>/dev/null)"
    check "D-C1b-6: a KILL that lands 0.6 s late is waited for (the grandchild is dead at the return)" "$([ $? -ne 0 ] && [ -s "$dir/kill_tardio/pid" ] && echo 0 || echo 1)" "the grandchild was still alive when the contido returned"
    check "D-C1b-6: the late-KILL case keeps rc 0" "$([ "$rc" -eq 0 ] && echo 0 || echo 1)" "got $rc"
    grep -q 'sobreviventes_apos_kill=0$' "$dir/kill_tardio/err"
    check "D-C1b-6: the late-KILL case ends with sobreviventes_apos_kill=0" "$?" "stderr: $(cat "$dir/kill_tardio/err")"
    # 12f2b. the FLOOR of the wait after the KILL: with G = 0 (no TERM grace) the wait is still max(1, G) = 1 s, so
    #        the same late KILL is waited for. A wait of G would be zero turns and would return at once.
    mkdir -p "$dir/kill_tardio_g0"
    (
        kill() {
            if [ "$1" = -KILL ]; then
                ( sleep 0.6; command kill -KILL -- "$3" ) >/dev/null 2>&1 &
                return 0
            fi
            command kill "$@"
        }
        contido_inside_main herdado-pgid - 64 5 0 "$dir/kill_tardio_g0" -- bash -c '(trap "" TERM; exec sleep 30) >/dev/null 2>&1 & echo $! >"$0"; exit 0' "$dir/kill_tardio_g0/pid"
    ) >/dev/null 2>"$dir/kill_tardio_g0/err"; rc=$?
    contido_alive "$(cat "$dir/kill_tardio_g0/pid" 2>/dev/null)"
    check "D-C1b-6: with G=0 the wait after the KILL still has a floor of 1 s (the grandchild is dead at the return)" "$([ $? -ne 0 ] && [ -s "$dir/kill_tardio_g0/pid" ] && [ "$rc" -eq 0 ] && echo 0 || echo 1)" "rc $rc, or the grandchild was alive when the contido returned"
    # the fast path: while the group still answers kill -0 the sweep runs (the counting stub above records that it
    # is REACHED); in the normal case below (the group is already gone) it must not run at all
    check "D-C1b-6: the sweep runs while the group still answers kill -0" "$([ -s "$dir/kill_tardio/scans" ] && echo 0 || echo 1)" "no sweep happened"
    mkdir -p "$dir/rapido_varredura"
    selftest_scan_log="$dir/rapido_varredura/scans"
    (
        contido_live_in_group() { echo scan >>"$selftest_scan_log"; selftest_real_live_in_group "$@"; }
        contido_inside_main herdado-pgid - 64 5 1 "$dir/rapido_varredura" -- true
    ) >/dev/null 2>"$dir/rapido_varredura/err"; rc=$?
    check "D-C1b-6: fast path, a group that is already gone is NOT swept" "$([ ! -e "$dir/rapido_varredura/scans" ] && [ "$rc" -eq 0 ] && echo 0 || echo 1)" "rc $rc scans: $(cat "$dir/rapido_varredura/scans" 2>/dev/null)"
    grep -q 'sobreviventes_apos_kill=0$' "$dir/rapido_varredura/err"
    check "D-C1b-6: fast path ends with sobreviventes_apos_kill=0" "$?" "stderr: $(cat "$dir/rapido_varredura/err")"
    # 12f3. D-C1b-6: when somebody does outlive the KILL, the line SAYS how many, a warning goes to stderr, the
    #       wait is bounded (max(1, G) s) and the command's own exit status is not masked.
    mkdir -p "$dir/sobrevive"
    tw0="${EPOCHREALTIME//[.,]/}"
    (
        contido_group_has_no_live_member() { CONTIDO_LIVE=2; return 1; }
        contido_inside_main herdado-pgid - 64 5 1 "$dir/sobrevive" -- bash -c 'exit 7'
    ) >/dev/null 2>"$dir/sobrevive/err"; rc=$?
    tw1="${EPOCHREALTIME//[.,]/}"
    check "D-C1b-6: survivors do not mask the command's rc (7)" "$([ "$rc" -eq 7 ] && echo 0 || echo 1)" "got $rc"
    grep -q 'sobreviventes_apos_kill=2$' "$dir/sobrevive/err"
    check "D-C1b-6: the line counts the survivors (2)" "$?" "stderr: $(cat "$dir/sobrevive/err")"
    grep -q 'AVISO - 2 processo(s) sobreviveram ao KILL' "$dir/sobrevive/err"
    check "D-C1b-6: a warning names the survivors" "$?" "stderr: $(cat "$dir/sobrevive/err")"
    check "D-C1b-6: the wait after the KILL is bounded (0.9 s to 2.5 s with G=1)" "$([ $((tw1 - tw0)) -ge 900000 ] && [ $((tw1 - tw0)) -le 2500000 ] && echo 0 || echo 1)" "took $((tw1 - tw0)) us"
    group_cases[graca]=$((group_cases[graca] + cases - g_start))
    fi
    if selftest_wants varredura; then
    g_start=$cases
    # 12f4. the sweep itself, on a FAKE /proc: live members counted, a zombie and a dead one not, another group not,
    #       a name with spaces and parentheses parsed after the LAST `)`, a non-numeric entry ignored.
    local fakeproc="$dir/fakeproc"
    mkdir -p "$fakeproc/100" "$fakeproc/101" "$fakeproc/102" "$fakeproc/103" "$fakeproc/104" "$fakeproc/105" "$fakeproc/self" "$fakeproc/107"
    echo '100 (sleep) S 1 4242 4242 0 -1' >"$fakeproc/100/stat"
    echo '101 (worker) R 1 4242 4242 0 -1' >"$fakeproc/101/stat"
    echo '102 (gone) Z 1 4242 4242 0 -1' >"$fakeproc/102/stat"
    echo '103 (dead) X 1 4242 4242 0 -1' >"$fakeproc/103/stat"
    echo '104 (other) S 1 9999 9999 0 -1' >"$fakeproc/104/stat"
    echo '105 (a b) (c) D 1 4242 4242 0 -1' >"$fakeproc/105/stat"
    echo '107 (never) S 1 4242 4242 0 -1' >"$fakeproc/107/stat"; rm "$fakeproc/107/stat"
    echo '1 (init) S 0 4242 4242 0 -1' >"$fakeproc/self/stat"
    contido_live_in_group 4242 "$fakeproc"
    check "the sweep counts the live members of the group (S, R and D = 3; not Z, not X, not the other group, not a non-numeric entry, not a pid that vanished)" "$([ "$CONTIDO_LIVE" -eq 3 ] && echo 0 || echo 1)" "counted $CONTIDO_LIVE"
    contido_live_in_group 5555 "$fakeproc"
    check "the sweep of a group nobody is in counts 0" "$([ "$CONTIDO_LIVE" -eq 0 ] && echo 0 || echo 1)" "counted $CONTIDO_LIVE"
    contido_live_in_group 4242 "$dir/nao-existe"
    check "the sweep of a root that does not exist counts 0 (nothing to read)" "$([ "$CONTIDO_LIVE" -eq 0 ] && echo 0 || echo 1)" "counted $CONTIDO_LIVE"
    # ... and on the REAL /proc: a live group of one is 1, and 0 once it is killed and reaped or a zombie
    local live_pid live_group turn
    ( exec setsid sleep 30 ) >/dev/null 2>&1 &
    live_pid=$!
    for turn in {1..50}; do
        read -r line 2>/dev/null <"/proc/$live_pid/stat" && { line="${line##*) }"; read -r _ _ live_group _ <<<"$line"; [ "$live_group" = "$live_pid" ] && break; }
        read -r -t 0.1 -u "$CONTIDO_FD" _ 2>/dev/null
    done
    contido_live_in_group "$live_pid"
    check "the sweep on the real /proc finds the live group of one" "$([ "$CONTIDO_LIVE" -eq 1 ] && echo 0 || echo 1)" "counted $CONTIDO_LIVE (group $live_group)"
    kill -KILL -- "-$live_pid" 2>/dev/null
    wait "$live_pid" 2>/dev/null
    contido_wait_bounded 5 contido_group_has_no_live_member "$live_pid"
    check "the sweep on the real /proc finds the group dead after the KILL (or only a zombie)" "$?" "still live: $CONTIDO_LIVE"
    # 12f5. L-11: no process per item swept. (a) the body of the sweep has no command substitution, no
    #       process substitution, no backtick and none of the external tools that used to be reached for;
    #       (b) with strace, a real sweep of this machine's /proc makes no clone/fork/vfork at all.
    local body
    body="$(declare -f contido_live_in_group)"
    local probe="${body//'$(('/}"   # arithmetic expansion forks nothing; only a command substitution does
    case "$probe" in
        *'$('* | *'`'* | *'<('* | *'>('*) got=1 ;;
        *) got=0 ;;
    esac
    if [ "$got" -eq 0 ] && printf '%s' "$probe" | grep -Eqw 'cat|awk|sed|grep|ps|pgrep|cut|head|tail|tr|wc|xargs|find'; then got=1; fi
    check "the sweep is written with builtins only (no substitution, no external tool)" "$got" "found a construct that costs a process per item"
    if command -v strace >/dev/null 2>&1 && strace -f -o "$dir/strace.probe" -e trace=execve true >/dev/null 2>&1 && grep -q execve "$dir/strace.probe"; then
        local forks
        strace -f -o "$dir/strace.sweep" -e trace=clone,clone3,fork,vfork bash -c "$body; contido_live_in_group 1" >/dev/null 2>&1
        forks="$(grep -c -E '^[0-9]+ +(clone|clone3|fork|vfork)\(' "$dir/strace.sweep" || true)"
        check "strace: a sweep over the real /proc makes no clone, fork or vfork" "$([ "${forks:-x}" = 0 ] && echo 0 || echo 1)" "counted $forks process creation(s)"
    else
        echo "contido_dentro --selftest: strace ausente ou sem permissao de ptrace: o controle de zero fork por strace foi PULADO (1 caso pulado); o controle do corpo da funcao rodou" >&2
    fi
    # 12g. contido_alive has POSITIVE controls too: every other use only asks "is it dead?", so a helper that
    #      always answered "dead" would pass them all. (1) this very shell is alive; (2) a live sleep is alive
    #      BEFORE the kill; (3) a ZOMBIE made on purpose (contido_fabricate_zombie: the child exits only AFTER its
    #      parent became a `sleep` that never reaps) still answers to `kill -0` but is NOT alive: this case fails on
    #      any host if the helper is a bare `kill -0`, without needing a container that does not reap.
    contido_alive "$$"
    check "contido_alive: this shell is alive" "$?" "rc $?"
    sleep 30 &
    local live_pid=$!
    contido_alive "$live_pid"
    check "contido_alive: a live sleep is alive before the kill" "$?" "rc $?"
    kill "$live_pid" 2>/dev/null
    wait "$live_pid" 2>/dev/null
    contido_fabricate_zombie "$dir"; rc=$?
    check "zombie made on purpose reached state Z (deterministic, bounded waits)" "$rc" "no zombie after the bounded wait"
    kill -0 "$ZOMBIE_PID" 2>/dev/null
    check "a zombie still answers to kill -0" "$?" "kill -0 failed on the zombie"
    contido_alive "$ZOMBIE_PID"; rc=$?
    check "contido_alive says a zombie is NOT alive" "$([ "$rc" -eq 1 ] && echo 0 || echo 1)" "rc $rc"
    kill "$ZOMBIE_PARENT" 2>/dev/null
    wait "$ZOMBIE_PARENT" 2>/dev/null

    # 13. mode ninho, only when the own cgroup is a child of a delegated contido scope
    local own_cg parent_cg
    own_cg="$(contido_own_cgroup)"; parent_cg="${own_cg%/*}"
    if [ "${parent_cg##*/}" != "" ] && case "${parent_cg##*/}" in glintfx-*.scope) true ;; *) false ;; esac && contido_scope_delegated "/sys/fs/cgroup$parent_cg"; then
        mkdir -p "$dir/n1" "$dir/n2" "$dir/n3"
        bash "$self" ninho "$parent_cg" 16 5 1 "$dir/n1" -- bash -c 'exit 77' >/dev/null 2>&1; rc=$?
        check "ninho: rc 77 crosses" "$([ "$rc" -eq 77 ] && echo 0 || echo 1)" "got $rc"
        bash "$self" ninho "$parent_cg" 16 5 1 "$dir/n2" -- bash -c 'read l </proc/self/cgroup; echo "${l##*/}"' >"$dir/n2/out" 2>/dev/null
        case "$(cat "$dir/n2/out")" in ninho-*) check "ninho: the command runs in a ninho-* cgroup" 0 "" ;; *) check "ninho: the command runs in a ninho-* cgroup" 1 "got [$(cat "$dir/n2/out")]" ;; esac
        bash "$self" ninho "$parent_cg" 16 5 1 "$dir/n3" -- bash -c 'setsid sleep 30 >/dev/null 2>&1 & echo $! >"$0"; exit 0' "$dir/n3/pid" >/dev/null 2>&1
        contido_alive "$(cat "$dir/n3/pid" 2>/dev/null)"
        check "ninho: a setsid grandchild is killed at the end" "$([ $? -ne 0 ] && [ -s "$dir/n3/pid" ] && echo 0 || echo 1)" "the setsid grandchild is still alive"
    else
        skipped=3
    fi

    group_cases[varredura]=$((group_cases[varredura] + cases - g_start))
    fi

    case "$dir" in "${TMPDIR:-/var/tmp}"/glintfx-contido-selftest.??????) rm -rf -- "$dir" ;; esac
    local group
    for group in $k_groups; do
        selftest_wants "$group" && echo "contido_dentro --selftest: grupo $group: ${group_cases[$group]} caso(s)" >&2
    done
    if [ "$cases" -eq 0 ]; then
        echo "contido_dentro --selftest: FALHOU - zero casos rodados (varredura vazia)" >&2
        return 1
    fi
    if [ "$failures" -gt 0 ]; then
        echo "contido_dentro --selftest: FALHOU - $failures de $cases casos" >&2
        return 1
    fi
    echo "contido_dentro --selftest: OK - $cases casos"
    echo "contido_dentro --selftest: pulados: $skipped (sem delegacao: o cgroup proprio nao e' filho de um escopo glintfx-* delegado; o ninho se exercita na prova dinamica)"
}

if [ "${1:-}" = "--selftest" ] && [ "$#" -le 2 ]; then
    contido_inside_selftest "${2:-}"
    exit $?
fi
contido_inside_main "$@"
exit $?
