#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# prova_contido.sh - the DYNAMIC proof of tools/contido.sh (CI-SPLIT-PER-OS A5, PLANO-C1.md and
# L34-RETRO.md R2-final, C1b v2, proofs D0 to D13). It is NOT a ctest: creating a scope from inside a
# ctest would escape the ceiling (P2), and the proof needs the `proprio` and `ninho` modes.
#
# Usage: tests/tools/contencao/prova_contido.sh [--contido <path>]
#   --contido  the contido to prove; the default is tools/contido.sh of this tree. The mutants of
#              PLANO-C1 (M-g to M-m) and of R2-final (M-l', M-n, M-o, M-p) are copies outside the tree
#              proved through this option.
# Must run from a terminal OUTSIDE any scope with a numeric pids.max (else contido would inherit and the
# `proprio` mode would never be exercised): it refuses to run there, exit 2.
# Prints one line per proof and exits non-zero if any proof fails or none ran (L-40).
# It never calls systemd-run itself: the scopes are created by contido.sh, the only place allowed.
# Any control of a refused fork uses a process that does NOT retry the fork (bash retries on EAGAIN and
# would wait instead of failing).
#
# Identifiers are English (project L-21); the printed messages are Portuguese.

aqui="$(cd -- "${BASH_SOURCE[0]%/*}" && pwd)"
contido="$aqui/../../../tools/contido.sh"
if [ "${1:-}" = "--contido" ]; then
    contido="${2:?--contido pede um caminho}"
    shift 2
fi
[ -x "$contido" ] || { echo "prova_contido: '$contido' nao e' executavel" >&2; exit 2; }
dentro="${contido%/*}/contido_dentro.sh"

own_cgroup=""
while read -r line; do
    case "$line" in 0::*) own_cgroup="${line#0::}" ;; esac
done </proc/self/cgroup
read -r own_max <"/sys/fs/cgroup$own_cgroup/pids.max" 2>/dev/null
case "$own_max" in
    max) ;;
    *) echo "prova_contido: RECUSA - o cgroup proprio tem pids.max='$own_max'; rode de um terminal fora de qualquer escopo (o modo proprio nao seria exercido)" >&2; exit 2 ;;
esac

checks=0
failures=0
report() {  # report <id> <ok: 0|1> <detail>
    checks=$((checks + 1))
    if [ "$2" -eq 0 ]; then
        echo "$1 OK"
    else
        failures=$((failures + 1))
        echo "$1 FALHOU - $3"
    fi
}

agents_slice="/sys/fs/cgroup/user.slice/user-$UID.slice/user@$UID.service/glintfx.slice/glintfx-agentes.slice"
slice_current() {  # the pids.current of the agents slice; 0 when it does not exist
    local value
    if [ -r "$agents_slice/pids.current" ]; then read -r value <"$agents_slice/pids.current"; echo "$value"; else echo 0; fi
}

# one clock descriptor for the waits of this script (a read with a timeout, no `sleep` process)
exec {clock}<> <(:)
pause() { read -r -t "$1" -u "$clock" _ 2>/dev/null; return 0; }  # pause <seconds>

scratch="$(mktemp -d "${TMPDIR:-/var/tmp}/glintfx-prova.XXXXXX")"
cleanup() { case "$scratch" in "${TMPDIR:-/var/tmp}"/glintfx-prova.??????) rm -rf -- "$scratch" ;; esac; }
trap cleanup EXIT

# D0: the ceiling is really the one asked for, in the sub-cgroup the command runs in (carga/), and the
# scope above it has the same ceiling; everything sits under the agents slice
out="$("$contido" --tarefas 16 -- bash -c 'read l </proc/self/cgroup; p=${l#0::}; read m </sys/fs/cgroup$p/pids.max; read pm </sys/fs/cgroup${p%/*}/pids.max; echo "$m $pm $p"' 2>/dev/null)"
case "$out" in
    "16 16 "*glintfx-agentes.slice/*.scope/carga) report D0 0 "" ;;
    *) report D0 1 "esperado '16 16 <...glintfx-agentes.slice/...scope/carga>', obtido [$out]" ;;
esac

# D1: the exit status crosses end to end: 0, 1, 77 and death by signal (143)
for want in 0 1 77; do
    "$contido" -- bash -c "exit $want" >/dev/null 2>&1; got=$?
    report "D1-rc$want" "$([ "$got" -eq "$want" ] && echo 0 || echo 1)" "esperado $want, obtido $got"
done
"$contido" -- bash -c 'kill -TERM $$' >/dev/null 2>&1; got=$?
report D1-sinal "$([ "$got" -eq 143 ] && echo 0 || echo 1)" "esperado 143, obtido $got"

# D2 (E1): a literal ${HOME} and $$ reach the command intact (--expand-environment=no)
out="$("$contido" -- printf '%s' '${HOME}$$' 2>/dev/null)"
report D2 "$([ "$out" = '${HOME}$$' ] && echo 0 || echo 1)" "esperado o literal, obtido [$out]"

# D3 (E2): two contidos launched together both pass (the unit name is unique)
"$contido" --marcador d3a -- bash -c 'exit 0' >/dev/null 2>&1 &
pid_a=$!
"$contido" --marcador d3b -- bash -c 'exit 0' >/dev/null 2>&1 &
pid_b=$!
wait "$pid_a"; rc_a=$?
wait "$pid_b"; rc_b=$?
report D3 "$([ "$rc_a" -eq 0 ] && [ "$rc_b" -eq 0 ] && echo 0 || echo 1)" "rc a=$rc_a b=$rc_b"

# D4: a 2 s deadline over a `sleep 60` grandchild gives 124, and nothing is left alive
before="$(slice_current)"
"$contido" --tempo 2 --graca 1 -- bash -c 'sleep 60 & wait' >/dev/null 2>&1; got=$?
report D4-rc "$([ "$got" -eq 124 ] && echo 0 || echo 1)" "esperado 124, obtido $got"
pause 2
after="$(slice_current)"
report D4-limpo "$([ "$after" -le "$before" ] && echo 0 || echo 1)" "pids.current da fatia antes=$before depois=$after"

# D5 (nested = a real sub-cgroup): the inner contido creates a ninho-* SIBLING of the outer carga/, inside the
# outer scope; no new scope exists for it; its final line says modo=ninho
err="$scratch/d5.err"
out="$("$contido" --marcador d5fora -- "$contido" --marcador d5dentro -- bash -c 'read l </proc/self/cgroup; n=$(systemctl --user list-units --type=scope --no-legend 2>/dev/null | grep -c "^ *glintfx-d5dentro-"); echo "${l#0::} $n"' 2>"$err")"
read -r inner_cgroup inner_count <<<"$out"
report D5-ninho "$(grep -q 'contido: modo=ninho' "$err" && echo 0 || echo 1)" "a linha 'modo=ninho' nao apareceu: $(cat "$err")"
report D5-sem-escopo-novo "$([ "$inner_count" = 0 ] && echo 0 || echo 1)" "escopos glintfx-d5dentro=$inner_count"
case "$inner_cgroup" in
    */glintfx-d5fora-*.scope/ninho-*) report D5-cgroup 0 "" ;;
    *) report D5-cgroup 1 "cgroup da sonda '$inner_cgroup', esperado .../glintfx-d5fora-*.scope/ninho-*" ;;
esac

# D6: the deadline of a ninho kills only the ninho: the outer carga stays alive until the end of its own
# command (proof that the kill never touches the outer cgroup), and the inner sleep is gone
before="$(slice_current)"
out="$("$contido" --tempo 30 --marcador d6fora -- bash -c '"$0" --tempo 2 --graca 1 --marcador d6dentro -- bash -c "sleep 60 & wait" 2>/dev/null; echo "D6-DENTRO-RC=$?"; echo D6-FORA-VIVO' "$contido" 2>/dev/null)"
case "$out" in *"D6-DENTRO-RC=124"*) report D6-rc 0 "" ;; *) report D6-rc 1 "esperado D6-DENTRO-RC=124, obtido [$out]" ;; esac
case "$out" in *"D6-FORA-VIVO"*) report D6-fora-vivo 0 "" ;; *) report D6-fora-vivo 1 "o escopo de fora morreu junto: [$out]" ;; esac
pause 1
after="$(slice_current)"
report D6-limpo "$([ "$after" -le "$before" ] && echo 0 || echo 1)" "pids.current da fatia antes=$before depois=$after"

# D7: the `proprio` mode checks the NAME of the scope, not only N. From inside a real contido scope
# (--tarefas 16), the inside half called with the right N but a false unit name refuses with 71 and
# runs nothing (without the name check it would run and then kill the scope it is in).
marker_dir="$scratch/d7"; mkdir "$marker_dir"
out="$("$contido" --tarefas 16 -- bash -c 'bash "$0" proprio unidade-falsa 16 5 1 "$1" -- touch "$1/marca" >/dev/null 2>"$1/err"; echo "D7-RC=$?"' "$dentro" "$marker_dir" 2>/dev/null)"
case "$out" in *"D7-RC=71"*) report D7-rc71 0 "" ;; *) report D7-rc71 1 "esperado D7-RC=71, obtido [$out]" ;; esac
# the refusal must be BY THE NAME: N alone matches here (16), and without the name check the run still ends in 71
# (the setup fails on a cgroup that has processes), so the rc alone would not tell the two apart
report D7-recusa-pelo-nome "$(grep -q 'FALHA - escopo errado (pids.max=16, cgroup=carga)' "$marker_dir/err" 2>/dev/null && echo 0 || echo 1)" "a recusa nao veio da conferencia do nome: $(cat "$marker_dir/err" 2>/dev/null)"
report D7-marcador "$([ "$(cat "$marker_dir/fim" 2>/dev/null)" = "recusa:escopo-errado" ] && echo 0 || echo 1)" "o marcador deveria dizer recusa:escopo-errado, disse [$(cat "$marker_dir/fim" 2>/dev/null)]"
report D7-nao-rodou "$([ ! -e "$marker_dir/marca" ] && echo 0 || echo 1)" "o comando rodou (a marca existe)"

# D8: a grandchild that called setsid does NOT escape the deadline, in mode proprio and in mode ninho
# (the kill is by cgroup, not by process group)
for mode in proprio ninho; do
    pidfile="$scratch/d8-$mode.pid"
    if [ "$mode" = proprio ]; then
        "$contido" --tempo 2 --graca 1 -- bash -c 'setsid sleep 60 >/dev/null 2>&1 & echo $! >"$0"; wait' "$pidfile" >/dev/null 2>&1; got=$?
    else
        "$contido" --tempo 30 --marcador d8fora -- bash -c '"$0" --tempo 2 --graca 1 -- bash -c "setsid sleep 60 >/dev/null 2>&1 & echo \$! >\"\$0\"; wait" "$1" >/dev/null 2>&1; echo "D8-RC=$?"' "$contido" "$pidfile" 2>/dev/null >"$scratch/d8-$mode.out"
        got="$(sed -n 's/^D8-RC=//p' "$scratch/d8-$mode.out")"
    fi
    report "D8-$mode-rc" "$([ "$got" = 124 ] && echo 0 || echo 1)" "esperado 124, obtido [$got]"
    kill -0 "$(cat "$pidfile" 2>/dev/null)" 2>/dev/null
    report "D8-$mode-setsid-morto" "$([ $? -ne 0 ] && [ -s "$pidfile" ] && echo 0 || echo 1)" "o neto com setsid ainda vive (ou nao registrou o pid)"
done

# D9: a ninho with a SMALLER N gets exactly that ceiling; a LARGER N is capped by the hierarchy and the
# warning says so
out="$("$contido" --tarefas 32 --marcador d9fora -- "$contido" --tarefas 16 -- bash -c 'read l </proc/self/cgroup; read m </sys/fs/cgroup${l#0::}/pids.max; echo "$m"' 2>/dev/null)"
report D9-menor "$([ "$out" = 16 ] && echo 0 || echo 1)" "esperado pids.max=16 no ninho, obtido [$out]"
err="$scratch/d9.err"
"$contido" --tarefas 16 --marcador d9fora -- "$contido" --tarefas 64 -- true >/dev/null 2>"$err"
report D9-aviso-efetivo "$(grep -q 'AVISO: teto efetivo 16 difere do pedido 64 (hierarquia)' "$err" && echo 0 || echo 1)" "o aviso 'efetivo = 16' nao apareceu: $(cat "$err")"

# D10: the grace: TERM comes before the KILL, so the command cleans up (deadline 2 s, grace 3 s)
marca="$scratch/d10.marca"
"$contido" --tempo 2 --graca 3 -- bash -c 'trap "echo limpo >\"\$0\"; exit" TERM; sleep 60 & wait' "$marca" >/dev/null 2>&1; got=$?
report D10-rc "$([ "$got" -eq 124 ] && echo 0 || echo 1)" "esperado 124, obtido $got"
report D10-marca "$([ -s "$marca" ] && echo 0 || echo 1)" "o comando nao limpou (KILL sem TERM antes)"

# find_scope <unit marker>: the absolute cgroup dir of the running scope glintfx-<marker>-*.scope, or nothing
find_scope() {
    local dir
    for dir in "$agents_slice"/glintfx-"$1"-*.scope; do [ -d "$dir" ] && { echo "$dir"; return 0; }; done
    return 1
}
# wait_scope <marker>: up to 5 s (20 turns of 0.25 s, no process) for the scope to have a supervisor
wait_scope() {
    local i dir
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        dir="$(find_scope "$1")" && [ -r "$dir/supervisor/cgroup.procs" ] && { read -r _ <"$dir/supervisor/cgroup.procs" && { echo "$dir"; return 0; }; }
        pause 0.25
    done
    return 1
}

# D10b: the leader leaves at once and a grandchild ignores TERM: it must die by the KILL after ~G (the case of
# skipping the KILL because the leader is gone). Deadline is not involved: the command ends by itself.
pidfile="$scratch/d10b.pid"
t0="${EPOCHREALTIME//[.,]/}"
"$contido" --graca 2 -- bash -c '(trap "" TERM; exec sleep 30) >/dev/null 2>&1 & echo $! >"$0"; exit 0' "$pidfile" >/dev/null 2>&1; got=$?
t1="${EPOCHREALTIME//[.,]/}"
report D10b-rc "$([ "$got" -eq 0 ] && echo 0 || echo 1)" "esperado 0, obtido $got"
report D10b-tempo "$([ $((t1 - t0)) -ge 1900000 ] && [ $((t1 - t0)) -le 4500000 ] && echo 0 || echo 1)" "o KILL veio depois de $((t1 - t0)) us (esperado ~G=2 s)"
kill -0 "$(cat "$pidfile" 2>/dev/null)" 2>/dev/null
report D10b-morto "$([ $? -ne 0 ] && [ -s "$pidfile" ] && echo 0 || echo 1)" "o neto que ignora TERM sobreviveu"

# a normal end costs no grace: G = 30 and the whole run still takes well under 5 s
t0="${EPOCHREALTIME//[.,]/}"
"$contido" --graca 30 -- true >/dev/null 2>&1; got=$?
t1="${EPOCHREALTIME//[.,]/}"
report D10c-rapido "$([ "$got" -eq 0 ] && [ $((t1 - t0)) -lt 5000000 ] && echo 0 || echo 1)" "rc $got em $((t1 - t0)) us com G=30 (o termino normal nao espera a graca)"

# D11: the supervisor (contido_dentro, which IS the process systemd-run runs) is KILLED from outside while the
# command runs. The outer contido must sweep the scope, exit 73 with fim=supervisor-morto, leave no scope and
# no sleep behind, and take far less than the deadline (S = 30)
err="$scratch/d11.err"
before="$(slice_current)"
t0="${EPOCHREALTIME//[.,]/}"
"$contido" --tempo 30 --graca 1 --marcador d11 -- bash -c 'sleep 60 & wait' >/dev/null 2>"$err" &
outer_pid=$!
if scope_dir="$(wait_scope d11)"; then
    read -r dentro_pid <"$scope_dir/supervisor/cgroup.procs"
    kill -KILL "$dentro_pid" 2>/dev/null
fi
wait "$outer_pid"; got=$?
t1="${EPOCHREALTIME//[.,]/}"
report D11-rc "$([ "$got" -eq 73 ] && echo 0 || echo 1)" "esperado 73, obtido $got: $(cat "$err")"
report D11-fim "$(grep -q 'fim=supervisor-morto' "$err" && echo 0 || echo 1)" "'fim=supervisor-morto' nao apareceu: $(cat "$err")"
report D11-escopo-sumiu "$([ -z "${scope_dir:-}" ] || [ ! -d "$scope_dir" ] && echo 0 || echo 1)" "o escopo $scope_dir continua vivo"
pause 1
after="$(slice_current)"
report D11-limpo "$([ "$after" -le "$before" ] && echo 0 || echo 1)" "pids.current da fatia antes=$before depois=$after (o sleep sobrou?)"
report D11-tempo "$([ $((t1 - t0)) -lt 15000000 ] && echo 0 || echo 1)" "levou $((t1 - t0)) us, muito acima da varredura (G+2 s)"

# D11b: the supervisor HUNG (kill -STOP), not dead: the systemd's RuntimeMaxSec (S + G + 5 = 7 s) ends the scope
# and the outer contido reads Result=timeout: rc 124 and fim=prazo-encosto (no --collect, or the Result vanishes)
err="$scratch/d11b.err"
"$contido" --tempo 1 --graca 1 --marcador d11b -- sleep 60 >/dev/null 2>"$err" &
outer_pid=$!
if scope_dir="$(wait_scope d11b)"; then
    read -r dentro_pid <"$scope_dir/supervisor/cgroup.procs"
    kill -STOP "$dentro_pid" 2>/dev/null
fi
wait "$outer_pid"; got=$?
report D11b-rc "$([ "$got" -eq 124 ] && echo 0 || echo 1)" "esperado 124, obtido $got: $(cat "$err")"
report D11b-encosto "$(grep -q 'fim=prazo-encosto' "$err" && echo 0 || echo 1)" "'fim=prazo-encosto' nao apareceu: $(cat "$err")"

# D12: a setsid grandchild sits in a ninho whose supervisor was killed: the OUTER supervisor kills {carga,
# every ninho-*} at its end, so the grandchild dies WITHOUT the final `stop` sweep (varredura_final=nada)
err="$scratch/d12.err"
pidfile="$scratch/d12.pid"
"$contido" --tempo 60 --graca 1 --marcador d12 -- bash -c '"$0" --tempo 40 --graca 1 --marcador d12dentro -- bash -c "setsid sleep 60 >/dev/null 2>&1 & echo \$! >\"\$0\"; wait" "$1" >/dev/null 2>&1; echo INNER-END' "$contido" "$pidfile" >/dev/null 2>"$err" &
outer_pid=$!
if scope_dir="$(wait_scope d12)"; then
    inner_pid=""
    for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        for cmdline_file in /proc/[0-9]*/cmdline; do
            mapfile -d '' -t argv <"$cmdline_file" 2>/dev/null || continue
            case "${argv[*]}" in *contido_dentro.sh*ninho\ *) inner_pid="${cmdline_file#/proc/}"; inner_pid="${inner_pid%/cmdline}" ;; esac
        done
        [ -n "$inner_pid" ] && [ -s "$pidfile" ] && break
        pause 0.25
    done
    [ -n "$inner_pid" ] && kill -KILL "$inner_pid" 2>/dev/null
fi
wait "$outer_pid"; got=$?
report D12-rc "$([ "$got" -eq 0 ] && echo 0 || echo 1)" "esperado 0, obtido $got: $(cat "$err")"
kill -0 "$(cat "$pidfile" 2>/dev/null)" 2>/dev/null
report D12-setsid-morto "$([ $? -ne 0 ] && [ -s "$pidfile" ] && echo 0 || echo 1)" "o neto com setsid do ninho orfao sobreviveu (ou o pid nao foi registrado)"
report D12-sem-varredura "$(grep -q 'varredura_final=nada' "$err" && echo 0 || echo 1)" "a matanca do fim nao alcancou o ninho; o stop teve de varrer: $(cat "$err")"

# D13: a BARE 71 with the unit still ACTIVE is a dead supervisor, not a refusal. A stub of the inside half launches
# a load and leaves with 71 and no marker; the real contido must sweep the scope (stop) and exit 73. A contido that
# trusted the bare 71 would return 71 and leave the scope, and its load, behind. The stub sits beside a COPY of the
# contido under proof, so the mutants (copies of contido.sh) are proved through it too.
stubdir="$scratch/d13"; mkdir "$stubdir"
cp "$contido" "$stubdir/contido.sh"
printf '%s\n' '#!/usr/bin/env bash' 'sleep 60 >/dev/null 2>&1 &' 'exit 71' >"$stubdir/contido_dentro.sh"
chmod +x "$stubdir/contido.sh" "$stubdir/contido_dentro.sh"
err="$scratch/d13.err"
before="$(slice_current)"
"$stubdir/contido.sh" --marcador d13 --graca 1 -- true >/dev/null 2>"$err"; got=$?
report D13-rc "$([ "$got" -eq 73 ] && echo 0 || echo 1)" "esperado 73, obtido $got: $(cat "$err")"
report D13-fim "$(grep -q 'fim=supervisor-morto, varredura_final=stop' "$err" && echo 0 || echo 1)" "esperado fim=supervisor-morto com varredura_final=stop: $(cat "$err")"
pause 1
after="$(slice_current)"
report D13-limpo "$([ "$after" -le "$before" ] && echo 0 || echo 1)" "pids.current da fatia antes=$before depois=$after (a carga do stub sobrou?)"

if [ "$checks" -eq 0 ]; then
    echo "prova_contido: FALHOU - zero provas rodadas" >&2
    exit 1
fi
if [ "$failures" -gt 0 ]; then
    echo "prova_contido: FALHOU - $failures de $checks provas" >&2
    exit 1
fi
echo "prova_contido: OK - $checks provas (D0 a D13) contra $contido"
