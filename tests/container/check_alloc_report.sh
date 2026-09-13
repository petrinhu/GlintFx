#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_alloc_report.sh - CONTAINER-LEAK-COUNTER sub-fatia S3 (/var/tmp/
# glintfx-plan/leak-counter.md sec. 5, GODS_LAWS.md L-20/L-35/L-40/L-43/
# L-45): QUEM APLICA O CRITERIO que a sub-fatia S2 (tests/container/
# alloc_counter_hook.cpp) so mede e relata. O gancho nunca reprova nada
# por si (o mesmo relatorio tem de sobreviver a uma fixture que ja
# falhou por outro motivo, sem esconder o `rc` real dela) - este script
# e' o UNICO atomo que decide "verde" ou "vermelho" a partir do que o
# gancho imprimiu, para o criterio morar num lugar so, com --selftest,
# em vez de espalhado por dezoito `main()`.
#
# ENTRADA: dois arquivos que o job `wayland-container` ja mantem -
# container_measured_raw.log (a saida crua e acumulada de cada fixture,
# tests/container/exec_fixture.sh's proprio `tee -a`) e
# parity_inventory.txt (um nome de fixture por linha, P-0). Um terceiro
# argumento OPCIONAL, o nome/id de um container Docker AINDA DE PE, so
# serve para resolver enderecos ALLOC_LEAK em arquivo:linha via
# `docker exec <container> addr2line -e /usr/local/bin/<fixture>
# <endereco>` - sem ele, o endereco cru ainda aparece no relatorio, so
# sem a resolucao (o --selftest nunca passa container: prova a logica
# de criterio, nunca faz `docker exec`).
#
# A SEIS CHAVES, SEMPRE AS MESMAS (leak-counter.md sec. 5 S2, formato
# exato que alloc_counter_hook.cpp's print_report() imprime):
# alloc_new_calls, alloc_delete_calls, alloc_live_ours,
# alloc_live_third_party, alloc_ours_overflow, alloc_foreign_delete.
# Uma fixture do inventario sem as SEIS linhas MEASURED <nome>.alloc_*
# e' "sem contador" - falta de linha reprova (o gancho nao rodou, ou
# morreu antes do atexit), nunca vira excecao silenciosa.
#
# CRITERIO, FIXADO ANTES DO DADO (leak-counter.md sec. 4, tabela; a
# mesma tabela que o plano registrou ANTES de qualquer fixture rodar
# contra este script):
#   alloc_live_ours       tem que ser 0 - qualquer outro valor e'
#                         vazamento NOSSO, com os enderecos citados.
#   alloc_new_calls       tem que ser > 0 - zero e' PISO DE VARREDURA
#   alloc_delete_calls    (GODS_LAWS.md L-40): o gancho existe no
#                         binario mas nunca foi alcancado, nunca "esta
#                         fixture nao aloca nada".
#   alloc_ours_overflow   tem que ser 0 - a tabela de blocos vivos
#                         `ours` estourou (4096 entradas).
#   alloc_foreign_delete  tem que ser 0 - `operator delete` recebeu um
#                         ponteiro que este gancho nunca alocou.
#   alloc_live_third_party NUNCA e' julgado aqui - so MEDIDO e
#                         publicado (leak-counter.md sec. 4: o
#                         julgamento do crescimento entre ciclos e' da
#                         sub-fatia S4, ver a chave seguinte).
#   third_party_growth_c2_c3 (S4, so alloc_cycle_growth_smoke.cpp
#                         imprime esta chave): quando a linha existe,
#                         tem que ser EXATAMENTE 0 - GODS_LAWS.md L-43,
#                         criterio fixado ANTES do dado. Diferente das
#                         seis chaves acima: nao e' "sem contador"
#                         quando ausente (nem toda fixture cicla), e a
#                         propria fixture escolhe o dono do prefixo, ao
#                         contrario das linhas ALLOC_LEAK.
#
# ATRIBUICAO DAS LINHAS ALLOC_LEAK AO DONO CERTO, SEM PREFIXO DE NOME:
# print_report() (S2) imprime "ALLOC_LEAK ours=<endereco>" SEM o nome
# da fixture - cada fixture e' o proprio processo de uma unica
# invocacao de `docker exec`, entao o bloco inteiro (as seis linhas
# MEASURED, seguidas de ate 32 linhas ALLOC_LEAK) e' contiguo no log,
# na ordem em que as fixtures rodaram. A linha "MEASURED <nome>.
# alloc_new_calls=" e' SEMPRE a primeira das seis (print_report() as
# imprime nessa ordem fixa) - o awk abaixo usa essa linha como marcador
# de INICIO DE BLOCO ("dono corrente"), e atribui toda linha ALLOC_LEAK
# seguinte ao dono corrente ate o proximo marcador aparecer. Nao ha
# outra forma de saber de quem e' um ALLOC_LEAK sem essa ordem - e' por
# isso que este script nunca reordena nem filtra o log antes de ler.
#
# GODS_LAWS.md L-45 (codigo de saida sempre de ARQUIVO, nunca so' de
# variavel de shell nem de notificacao): o `awk` roda sob `set +e`,
# escreve stdout/stderr em arquivos proprios, e o `rc` e' lido de volta
# do proprio comando antes de qualquer outra linha rodar - mesmo
# idioma que tests/container/exec_fixture.sh (S1's proprio irmao mais
# velho) ja usa para o mesmo motivo.
#
# POR QUE AWK, NAO PYTHON (diferente da maioria dos gates em tests/
# tools/): este script mora em tests/container/, ao lado de exec_
# fixture.sh e check_isolation.sh - os dois outros atomos deste
# diretorio que leem texto de log/inspecao sem Python, e ambos ja
# documentam a mesma razao (check_isolation.sh:298-306): plain POSIX
# awk, nunca uma extensao gawk-only, porque o job `wayland-container`
# roda em `ubuntu-latest`, cujo `awk` default e' mawk. Nenhuma extensao
# usada aqui alem de `split`/`sub`/`index`/`substr`/arrays com SUBSEP -
# todas POSIX, provadas em mawk.
#
# O QUE ISTO NAO PROVA (declarado, GODS_LAWS.md L-43): que a perna
# `asan` e a perna `plain` viram o MESMO conjunto de fixtures - este
# script roda uma vez por perna, contra o log daquela perna so'; quem
# compara as duas pernas e' quem le os dois jobs lado a lado (mesma
# lacuna que a "Varredura de relatorios do sanitizer" ja declara).

set -eu

SCRIPT_NAME="check_alloc_report.sh"

fail_usage() {
    cat >&2 <<'EOF'
uso: check_alloc_report.sh <log> <inventario> [container]
     check_alloc_report.sh --selftest
EOF
    exit 2
}

# --- o awk que aplica o criterio (leak-counter.md sec. 4/5 S3) ---------
#
# ATENCAO PARA QUEM EDITAR O BLOCO ABAIXO: ele inteiro vive dentro de UM
# UNICO literal de shell entre aspas simples - nenhum caractere apostrofo
# (nem o "e apostrofo" que o resto deste arquivo usa em comentario de
# shell comum) pode aparecer ali, ou o shell fecha a string no meio e o
# resto vira comando (foi visto ao vivo: "linha 125: o: comando nao
# encontrado" antes deste aviso existir). Escreva por extenso, sem
# elisao, dentro deste bloco.
#
# inv_file_name (passado via -v, nunca por FNR==NR): identifica qual dos
# dois arquivos de entrada e o inventario, comparando contra FILENAME.
# NUNCA usar o truque classico "FNR==NR" para essa distincao aqui: ele
# quebra exatamente no caso que este script existe para pegar
# (inventario VAZIO, GODS_LAWS.md L-40) - com o primeiro arquivo em zero
# linhas, NR nunca sai na frente de FNR, entao toda linha do LOG passaria
# a bater "FNR==NR" e seria lida como se fosse nome de inventario (medido
# ao vivo: seis linhas MEASURED de um log viraram seis fixtures falsas
# antes deste conserto). FILENAME nao depende de quantas linhas cada
# lado tem.
#
# Do arquivo de log, classifica cada linha: "MEASURED <owner>.<key>=
# <valor>" alimenta vals[owner,key] e seen[owner,key] quando <key> e uma
# das seis chaves reconhecidas, e marca current_owner quando <key> e
# "alloc_new_calls" (o marcador de inicio de bloco, ver o comentario do
# cabecalho deste arquivo); "ALLOC_LEAK ours=<endereco>" acrescenta o
# endereco a leak_addrs[current_owner]. O END aplica a tabela da secao 4
# fixture por fixture do inventario e imprime tanto o veredito humano
# (stderr, "ERRO: ...") quanto uma forma maquina ("ALLOC_LEAK_FIXTURE
# <fixture> <endereco>", stdout) que real_main() abaixo consome para
# resolver arquivo:linha quando um container foi passado.
ALLOC_REPORT_AWK='
BEGIN {
    n_keys = split("alloc_new_calls alloc_delete_calls alloc_live_ours alloc_live_third_party alloc_ours_overflow alloc_foreign_delete", keys, " ")
    exit_code = 0
    current_owner = ""
}
FILENAME == inv_file_name {
    line = $0
    sub(/\r$/, "", line)
    if (line != "") { inv[line] = 1; inv_count++ }
    next
}
{
    line = $0
    sub(/\r$/, "", line)
    if (line == "") next
    n = split(line, f, " ")
    if (n < 2) next
    if (f[1] == "MEASURED") {
        kv = f[2]
        eq = index(kv, "=")
        if (eq == 0) next
        ownerkey = substr(kv, 1, eq - 1)
        value = substr(kv, eq + 1)
        dot = index(ownerkey, ".")
        if (dot == 0) next
        owner = substr(ownerkey, 1, dot - 1)
        key = substr(ownerkey, dot + 1)
        if (key == "third_party_growth_c2_c3") {
            # S4 (leak-counter.md sec. 5 S4): not one of the six per-
            # fixture keys above (only alloc_cycle_growth_smoke prints
            # it) - captured separately, checked in END below, BEFORE
            # the is_alloc_key filter so it is never silently dropped.
            growth_seen[owner] = 1
            growth_val[owner] = value
            next
        }
        is_alloc_key = 0
        for (i = 1; i <= n_keys; i++) { if (key == keys[i]) { is_alloc_key = 1 } }
        if (!is_alloc_key) next
        seen[owner, key] = 1
        vals[owner, key] = value
        if (key == "alloc_new_calls") { current_owner = owner }
    } else if (f[1] == "ALLOC_LEAK") {
        kv = f[2]
        eq = index(kv, "=")
        if (eq == 0) next
        addr = substr(kv, eq + 1)
        if (current_owner != "") {
            leak_addrs[current_owner] = leak_addrs[current_owner] (leak_addrs[current_owner] == "" ? "" : " ") addr
        }
    }
}
END {
    if (inv_count == 0) {
        print "ERRO: inventario vazio - GODS_LAWS.md L-40, sinal de coleta quebrada, nunca de ausencia real de fixtures" > "/dev/stderr"
        exit_code = 1
    }
    with_contador = 0
    total_live_ours = 0
    for (name in inv) {
        missing = ""
        for (i = 1; i <= n_keys; i++) {
            if (!((name, keys[i]) in seen)) { missing = missing " " keys[i] }
        }
        if (missing != "") {
            printf "ERRO: %s: falta linha(s) MEASURED alloc_* exigida(s) -%s (fixture sem contador)\n", name, missing > "/dev/stderr"
            exit_code = 1
            continue
        }
        with_contador++
        new_calls = vals[name, "alloc_new_calls"] + 0
        delete_calls = vals[name, "alloc_delete_calls"] + 0
        live_ours = vals[name, "alloc_live_ours"] + 0
        live_third_party = vals[name, "alloc_live_third_party"] + 0
        ours_overflow = vals[name, "alloc_ours_overflow"] + 0
        foreign_delete = vals[name, "alloc_foreign_delete"] + 0
        printf "MEDIDO %s: new_calls=%d delete_calls=%d live_ours=%d live_third_party=%d ours_overflow=%d foreign_delete=%d\n", name, new_calls, delete_calls, live_ours, live_third_party, ours_overflow, foreign_delete
        if (new_calls == 0) {
            printf "ERRO: %s: alloc_new_calls=0 (piso de varredura, GODS_LAWS.md L-40 - gancho nao alcancado)\n", name > "/dev/stderr"
            exit_code = 1
        }
        if (delete_calls == 0) {
            printf "ERRO: %s: alloc_delete_calls=0 (piso de varredura, GODS_LAWS.md L-40 - gancho nao alcancado)\n", name > "/dev/stderr"
            exit_code = 1
        }
        if (ours_overflow != 0) {
            printf "ERRO: %s: alloc_ours_overflow=%d (tabela de blocos vivos ours estourou)\n", name, ours_overflow > "/dev/stderr"
            exit_code = 1
        }
        if (foreign_delete != 0) {
            printf "ERRO: %s: alloc_foreign_delete=%d (operator delete recebeu ponteiro que este gancho nunca alocou)\n", name, foreign_delete > "/dev/stderr"
            exit_code = 1
        }
        if (live_ours != 0) {
            total_live_ours += live_ours
            printf "ERRO: %s: alloc_live_ours=%d - vazamento NOSSO\n", name, live_ours > "/dev/stderr"
            n_addr = split(leak_addrs[name], addr_arr, " ")
            for (i = 1; i <= n_addr; i++) {
                printf "ALLOC_LEAK_FIXTURE %s %s\n", name, addr_arr[i]
            }
            exit_code = 1
        }
    }
    printf "fixtures_com_contador: %d de %d\n", with_contador, inv_count
    printf "vazamentos_ours: %d\n", total_live_ours

    # S4 (leak-counter.md sec. 5 S4, GODS_LAWS.md L-43: criterio fixado
    # ANTES do dado): quando a linha third_party_growth_c2_c3 existe,
    # tem que ser EXATAMENTE 0 - nem crescimento nem encolhimento, e
    # NUNCA uma tolerancia numerica decidida por este script (uma
    # arvore limpa que der diferente de 0 vai ao lider, nao vira um
    # `!= 0` frouxo aqui).
    for (owner in growth_seen) {
        growth = growth_val[owner] + 0
        printf "MEDIDO %s: third_party_growth_c2_c3=%d\n", owner, growth
        if (growth != 0) {
            printf "ERRO: %s: third_party_growth_c2_c3=%d - terceiro cresceu (ou encolheu) entre o ciclo 2 e o ciclo 3, esperado exatamente 0\n", owner, growth > "/dev/stderr"
            exit_code = 1
        }
    }

    exit exit_code
}
'

# real_main <log> <inventario> [container]
real_main() {
    [ "$#" -ge 2 ] || fail_usage
    log_path="$1"
    inventory_path="$2"
    container="${3:-}"

    [ -f "$log_path" ] || { echo "$SCRIPT_NAME: log nao encontrado: $log_path" >&2; exit 2; }
    [ -f "$inventory_path" ] || { echo "$SCRIPT_NAME: inventario nao encontrado: $inventory_path" >&2; exit 2; }

    out_file="$(mktemp "${TMPDIR:-/tmp}/glintfx-alloc-report-XXXXXX")"
    err_file="${out_file}.err"
    trap 'rm -f "$out_file" "$err_file"' EXIT

    # GODS_LAWS.md L-45: `set +e` em volta do awk (senao `set -eu`
    # abortaria esta funcao ANTES da linha seguinte poder ler "$?"),
    # rc sempre relido de $? imediatamente, nunca inferido depois.
    set +e
    awk -v inv_file_name="$inventory_path" "$ALLOC_REPORT_AWK" "$inventory_path" "$log_path" >"$out_file" 2>"$err_file"
    rc=$?
    set -e

    cat "$err_file" >&2 || true

    # Reconstroi cada linha de $out_file, resolvendo ALLOC_LEAK_FIXTURE
    # em arquivo:linha quando um container foi passado. `read -r a b c`
    # com mais de tres campos poe o RESTANTE da linha em "$c" - e' o que
    # permite reimprimir uma linha "MEDIDO ..."/"fixtures_com_contador:
    # ..." exatamente como veio, sem reescrever um parser de campos
    # para cada formato distinto que o awk acima produz.
    while IFS=' ' read -r tag a b; do
        if [ "$tag" = "ALLOC_LEAK_FIXTURE" ]; then
            fixture="$a"
            addr="$b"
            if [ -n "$container" ]; then
                resolved="$(docker exec "$container" addr2line -e "/usr/local/bin/${fixture}" "$addr" 2>/dev/null || echo '??:0')"
                echo "  vazamento: ${fixture} ours=${addr} -> ${resolved}" >&2
            else
                echo "  vazamento: ${fixture} ours=${addr} (sem container para resolver arquivo:linha)" >&2
            fi
        else
            printf '%s %s %s\n' "$tag" "$a" "$b"
        fi
    done <"$out_file"

    trap - EXIT
    rm -f "$out_file" "$err_file"
    exit "$rc"
}

# --- helpers de --selftest ---------------------------------------------

write_measured_lines() {
    fixture="$1"
    new_calls="$2"
    del_calls="$3"
    ours="$4"
    third="$5"
    overflow="$6"
    foreign="$7"
    printf 'MEASURED %s.alloc_new_calls=%s\n' "$fixture" "$new_calls"
    printf 'MEASURED %s.alloc_delete_calls=%s\n' "$fixture" "$del_calls"
    printf 'MEASURED %s.alloc_live_ours=%s\n' "$fixture" "$ours"
    printf 'MEASURED %s.alloc_live_third_party=%s\n' "$fixture" "$third"
    printf 'MEASURED %s.alloc_ours_overflow=%s\n' "$fixture" "$overflow"
    printf 'MEASURED %s.alloc_foreign_delete=%s\n' "$fixture" "$foreign"
}

selftest_positive_control() {
    log_file="$(mktemp "${scratch}/log-positive-XXXXXX")"
    inv_file="$(mktemp "${scratch}/inv-positive-XXXXXX")"
    i=1
    while [ "$i" -le 18 ]; do
        name="fixture_${i}"
        write_measured_lines "$name" 3 3 0 0 0 0 >>"$log_file"
        echo "$name" >>"$inv_file"
        i=$((i + 1))
    done
    if out="$(real_main "$log_file" "$inv_file" 2>&1)"; then
        if printf '%s\n' "$out" | grep -q 'fixtures_com_contador: 18 de 18' \
            && printf '%s\n' "$out" | grep -q 'vazamentos_ours: 0'; then
            echo "selftest: controle POSITIVO OK (18 fixtures limpas, 0 vazamento)"
            return 0
        fi
    fi
    echo "selftest: controle POSITIVO FALHOU" >&2
    printf '%s\n' "${out:-}" >&2
    return 1
}

selftest_leak_control() {
    log_file="$(mktemp "${scratch}/log-leak-XXXXXX")"
    inv_file="$(mktemp "${scratch}/inv-leak-XXXXXX")"
    write_measured_lines leaky_fixture 4 3 1 0 0 0 >"$log_file"
    echo 'ALLOC_LEAK ours=0x555555560010' >>"$log_file"
    echo "leaky_fixture" >"$inv_file"
    if out="$(real_main "$log_file" "$inv_file" 2>&1)"; then
        echo "selftest: controle de VAZAMENTO FALHOU (deveria ter reprovado, saiu 0)" >&2
        printf '%s\n' "$out" >&2
        return 1
    fi
    if printf '%s\n' "$out" | grep -q 'leaky_fixture ours=0x555555560010' \
        && printf '%s\n' "$out" | grep -q 'vazamentos_ours: 1'; then
        echo "selftest: controle de VAZAMENTO OK (alloc_live_ours=1 acusado, endereco citado)"
        return 0
    fi
    echo "selftest: controle de VAZAMENTO FALHOU (reprovou mas nao citou o endereco/contagem esperados)" >&2
    printf '%s\n' "$out" >&2
    return 1
}

selftest_missing_line_control() {
    log_file="$(mktemp "${scratch}/log-missing-XXXXXX")"
    inv_file="$(mktemp "${scratch}/inv-missing-XXXXXX")"
    {
        printf 'MEASURED partial_fixture.alloc_new_calls=2\n'
        printf 'MEASURED partial_fixture.alloc_delete_calls=2\n'
        printf 'MEASURED partial_fixture.alloc_live_ours=0\n'
    } >"$log_file"
    echo "partial_fixture" >"$inv_file"
    if out="$(real_main "$log_file" "$inv_file" 2>&1)"; then
        echo "selftest: controle de FIXTURE SEM LINHA FALHOU (deveria ter reprovado, saiu 0)" >&2
        printf '%s\n' "$out" >&2
        return 1
    fi
    if printf '%s\n' "$out" | grep -q 'partial_fixture' \
        && printf '%s\n' "$out" | grep -q 'fixture sem contador' \
        && printf '%s\n' "$out" | grep -q 'fixtures_com_contador: 0 de 1'; then
        echo "selftest: controle de FIXTURE SEM LINHA OK (contador incompleto acusado)"
        return 0
    fi
    echo "selftest: controle de FIXTURE SEM LINHA FALHOU (reprovou mas nao acusou o motivo esperado)" >&2
    printf '%s\n' "$out" >&2
    return 1
}

selftest_floor_control() {
    log_file="$(mktemp "${scratch}/log-floor-XXXXXX")"
    inv_file="$(mktemp "${scratch}/inv-floor-XXXXXX")"
    write_measured_lines unreached_fixture 0 0 0 0 0 0 >"$log_file"
    echo "unreached_fixture" >"$inv_file"
    if out="$(real_main "$log_file" "$inv_file" 2>&1)"; then
        echo "selftest: controle de PISO FALHOU (deveria ter reprovado, saiu 0)" >&2
        printf '%s\n' "$out" >&2
        return 1
    fi
    if printf '%s\n' "$out" | grep -q 'alloc_new_calls=0'; then
        echo "selftest: controle de PISO OK (new_calls=0 acusado como gancho nao alcancado)"
        return 0
    fi
    echo "selftest: controle de PISO FALHOU" >&2
    printf '%s\n' "$out" >&2
    return 1
}

selftest_overflow_control() {
    log_file="$(mktemp "${scratch}/log-overflow-XXXXXX")"
    inv_file="$(mktemp "${scratch}/inv-overflow-XXXXXX")"
    write_measured_lines overflowing_fixture 5000 5000 0 0 1 0 >"$log_file"
    echo "overflowing_fixture" >"$inv_file"
    if out="$(real_main "$log_file" "$inv_file" 2>&1)"; then
        echo "selftest: controle de ESTOURO FALHOU (deveria ter reprovado, saiu 0)" >&2
        printf '%s\n' "$out" >&2
        return 1
    fi
    if printf '%s\n' "$out" | grep -q 'alloc_ours_overflow=1'; then
        echo "selftest: controle de ESTOURO OK (tabela de blocos vivos ours estourada acusada)"
        return 0
    fi
    echo "selftest: controle de ESTOURO FALHOU" >&2
    printf '%s\n' "$out" >&2
    return 1
}

selftest_empty_inventory_control() {
    log_file="$(mktemp "${scratch}/log-empty-inv-XXXXXX")"
    inv_file="$(mktemp "${scratch}/inv-empty-XXXXXX")"
    write_measured_lines whatever_fixture 1 1 0 0 0 0 >"$log_file"
    : >"$inv_file"
    if out="$(real_main "$log_file" "$inv_file" 2>&1)"; then
        echo "selftest: controle de INVENTARIO VAZIO FALHOU (deveria ter reprovado, saiu 0)" >&2
        printf '%s\n' "$out" >&2
        return 1
    fi
    if printf '%s\n' "$out" | grep -q 'inventario vazio'; then
        echo "selftest: controle de INVENTARIO VAZIO OK (piso de varredura, GODS_LAWS.md L-40)"
        return 0
    fi
    echo "selftest: controle de INVENTARIO VAZIO FALHOU" >&2
    printf '%s\n' "$out" >&2
    return 1
}

selftest_growth_zero_control() {
    log_file="$(mktemp "${scratch}/log-growth-zero-XXXXXX")"
    inv_file="$(mktemp "${scratch}/inv-growth-zero-XXXXXX")"
    write_measured_lines alloc_cycle_growth_smoke 3 3 0 5 0 0 >"$log_file"
    printf 'MEASURED alloc_cycle_growth_smoke.third_party_growth_c2_c3=0\n' >>"$log_file"
    echo "alloc_cycle_growth_smoke" >"$inv_file"
    if out="$(real_main "$log_file" "$inv_file" 2>&1)"; then
        if printf '%s\n' "$out" | grep -q 'third_party_growth_c2_c3=0'; then
            echo "selftest: controle de CRESCIMENTO ZERO OK (third_party_growth_c2_c3=0 aceito)"
            return 0
        fi
    fi
    echo "selftest: controle de CRESCIMENTO ZERO FALHOU (deveria ter aceito com growth=0)" >&2
    printf '%s\n' "${out:-}" >&2
    return 1
}

selftest_growth_nonzero_control() {
    log_file="$(mktemp "${scratch}/log-growth-nonzero-XXXXXX")"
    inv_file="$(mktemp "${scratch}/inv-growth-nonzero-XXXXXX")"
    write_measured_lines alloc_cycle_growth_smoke 3 3 0 5 0 0 >"$log_file"
    printf 'MEASURED alloc_cycle_growth_smoke.third_party_growth_c2_c3=3\n' >>"$log_file"
    echo "alloc_cycle_growth_smoke" >"$inv_file"
    if out="$(real_main "$log_file" "$inv_file" 2>&1)"; then
        echo "selftest: controle de CRESCIMENTO NAO-ZERO FALHOU (deveria ter reprovado, saiu 0)" >&2
        printf '%s\n' "$out" >&2
        return 1
    fi
    if printf '%s\n' "$out" | grep -q 'third_party_growth_c2_c3=3'; then
        echo "selftest: controle de CRESCIMENTO NAO-ZERO OK (growth=3 acusado)"
        return 0
    fi
    echo "selftest: controle de CRESCIMENTO NAO-ZERO FALHOU (reprovou mas nao citou o valor esperado)" >&2
    printf '%s\n' "$out" >&2
    return 1
}

selftest_main() {
    scratch="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-alloc-report-selftest-XXXXXX")"
    trap 'rm -rf "$scratch"' EXIT

    exercitados=0
    reprovados=0

    exercitados=$((exercitados + 1))
    selftest_positive_control || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_leak_control || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_missing_line_control || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_floor_control || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_overflow_control || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_empty_inventory_control || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_growth_zero_control || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_growth_nonzero_control || reprovados=$((reprovados + 1))

    echo "controles: ${exercitados} exercitados, ${reprovados} reprovados"

    if [ "$reprovados" -ne 0 ]; then
        echo "check_alloc_report.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_alloc_report.sh --selftest: os oito controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
