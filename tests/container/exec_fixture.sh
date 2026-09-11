#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# exec_fixture.sh - CONTAINER-LOG-SIGNAL-FIRST (TODO.md, GODS_LAWS.md
# L-45/L-49/L-40, /var/tmp/glintfx-plan/cauda-w6b.md secao 4.1).
#
# O DEFEITO QUE ISTO FECHA: os passos `docker exec ... | tee -a
# container_measured_raw.log` do job `wayland-container` (.github/
# workflows/ci.yml) escondem o veredito decisivo atras do ruido -
# quando uma fixture morre por sinal (SIGSEGV=139, SIGABRT=134,
# SIGKILL=137...), o log traz primeiro os avisos de driver do
# llvmpipe e um erro cosmetico de caminho alternativo, e so' depois o
# codigo de saida real. Quem le o comeco conclui falta de pacote; a
# causa esta no fim. Aconteceu de verdade com o orquestrador em
# 06/09/2026 (feedback_caminho_menos_dificil.md); um agente refutou
# so' depois de medir.
#
# O QUE ESTE ATOMO FAZ, UMA UNICA COISA (GODS_LAWS.md L-17): executa
# UMA fixture (via `docker exec`, ou via um runner alternativo so'
# para --selftest) e imprime o VEREDITO - fixture, codigo de saida,
# nome do sinal quando o codigo for >=128, tamanho da saida - ANTES
# de qualquer byte da saida capturada da fixture. Nunca decide se o
# resultado e' aceitavel; so' torna o veredito visivel em PRIMEIRO
# lugar, sempre.
#
# POR QUE O CODIGO DE SAIDA, NUNCA O TEXTO DO SERVICO: um processo
# morto por sinal sai com 128+numero-do-sinal (kernel POSIX); o texto
# de erro ("Segmentation fault", "Killed"...) e' impresso pelo SHELL
# que esperou o processo, nao pelo `docker exec`, e depende de locale
# e de qual shell fez a espera - nao e' garantia nenhuma. `kill -l
# <numero>` (builtin de todo shell POSIX, nada instalado -
# GODS_LAWS.md L-51) traduz o numero para o nome sem depender de
# texto nenhum.
#
# ARMADILHA MEDIDA (secao 2.1 do plano, e o motivo do desenho abaixo
# capturar em ARQUIVO antes de imprimir): com `docker exec ... | tee
# -a`, o `tee` comeca a escrever no arquivo/tela ao vivo, entao o
# texto da fixture SEMPRE chega antes do codigo de saida (que so' se
# conhece depois do processo morrer). Capturar em arquivo primeiro, e
# so' IMPRIMIR o veredito antes do conteudo, e' a unica forma de
# inverter essa ordem sem perder nenhum byte.
#
# CODIGO DE SAIDA SEMPRE DE ARQUIVO, NUNCA DE NOTIFICACAO NEM DE
# VARIAVEL PERDIDA (GODS_LAWS.md L-45): o comando roda com `set +e`
# (senao `set -eu` abortaria a linha ANTES de ler `$?`), o codigo e'
# escrito num arquivo `.rc` e relido de la' - o mesmo idioma que
# check_win32_test_link.py e o passo "Build e testes" do proprio
# ci.yml ja usam.
#
# CONTRATO:
#   exec_fixture.sh [--runner "<prefixo>"] [--env NOME=VALOR ...] \
#       <container> <fixture> [args...]
#   exec_fixture.sh --selftest
#
# `--runner "<prefixo>"` substitui o `docker exec -e ... <container>`
# default por um comando arbitrario (as palavras de <prefixo> mais o
# nome da fixture e os args extras) - existe SO' para o --selftest
# provar a logica de veredito/piso/sinal sem precisar de Docker; em
# uso real (dentro do job) nunca e' passado. Quando presente, o
# argumento <container> ainda e' exigido pela forma do contrato, mas
# e' descartado (o runner alternativo nao tem container nenhum).
#
# `--env NOME=VALOR` (repetivel) acrescenta variaveis de ambiente ao
# `docker exec` default, alem de XDG_RUNTIME_DIR/WAYLAND_DISPLAY que
# ja saem sempre - reservado para a fatia 2 (ASAN_OPTIONS/UBSAN_
# OPTIONS), sem uso nos passos desta fatia (meca a quantidade com
# `grep -c "tests/container/exec_fixture.sh glintfx-wltest-clean"
# .github/workflows/ci.yml`, descontando o controle negativo cn5).
# So' e' honrado no modo
# default; e' ignorado sob --runner (o runner alternativo do
# --selftest nao precisa de env de container nenhum).
#
# O QUE ISTO NAO PROVA (declarado, GODS_LAWS.md L-43): o comportamento
# do `trap` de TERM/INT sob cancelamento real de job - nenhum controle
# automatico o exercita; e nao serve ao espelho local (tools/preci.sh
# nunca roda fixture de container, grep -c wltest tools/preci.sh = 0).

set -eu

# --- mensagens ------------------------------------------------------

fail_usage() {
    cat >&2 <<'EOF'
uso: exec_fixture.sh [--runner "<prefixo>"] [--env NOME=VALOR ...] <container> <fixture> [args...]
     exec_fixture.sh --selftest
EOF
    exit 2
}

# --- modo real --------------------------------------------------------

real_main() {
    runner=""
    env_count=0

    while [ "$#" -gt 0 ]; do
        case "$1" in
            --runner)
                [ "$#" -ge 2 ] || fail_usage
                runner="$2"
                shift 2
                ;;
            --env)
                [ "$#" -ge 2 ] || fail_usage
                env_count=$((env_count + 1))
                eval "exec_fixture_env_${env_count}=\"\$2\""
                shift 2
                ;;
            --)
                shift
                break
                ;;
            -*)
                fail_usage
                ;;
            *)
                break
                ;;
        esac
    done

    [ "$#" -ge 2 ] || fail_usage
    container="$1"
    fixture_name="$2"
    shift 2

    # Args extras (raramente usados hoje - nenhum dos passos desta
    # fatia precisa deles, meca com o mesmo grep do cabecalho) guardados
    # em variaveis numeradas: POSIX sh
    # nao tem array, e "$@" ja vai ser reconstruido abaixo para montar
    # o comando final (docker exec ... ou o runner do --selftest).
    extra_count=0
    while [ "$#" -gt 0 ]; do
        extra_count=$((extra_count + 1))
        eval "exec_fixture_extra_${extra_count}=\"\$1\""
        shift
    done

    if [ -n "$runner" ]; then
        # --selftest: <container> e' descartado de proposito (o
        # runner alternativo nunca fala com Docker).
        # shellcheck disable=SC2086
        set -- $runner "$fixture_name"
    else
        set -- docker exec \
            -e XDG_RUNTIME_DIR=/run/glintfx-test \
            -e WAYLAND_DISPLAY=glintfx-test
        i=1
        while [ "$i" -le "$env_count" ]; do
            eval "v=\"\$exec_fixture_env_${i}\""
            # Falso positivo: "v" E' atribuida na linha `eval` acima,
            # mas shellcheck nao segue atribuicao feita por `eval`
            # (mesma construcao POSIX sh para simular lista/array usada
            # no restante deste arquivo, ver disable=SC2086 acima).
            # shellcheck disable=SC2154
            set -- "$@" -e "$v"
            i=$((i + 1))
        done
        set -- "$@" "$container" "$fixture_name"
    fi

    i=1
    while [ "$i" -le "$extra_count" ]; do
        eval "v=\"\$exec_fixture_extra_${i}\""
        set -- "$@" "$v"
        i=$((i + 1))
    done

    run_and_report "$fixture_name" "$@"
}

# --- o atomo: roda, captura, reporta -----------------------------------

# run_and_report <nome-para-o-veredito> <comando...>
run_and_report() {
    report_fixture_name="$1"
    shift

    capture_file="$(mktemp "${TMPDIR:-/tmp}/glintfx-exec-fixture-XXXXXX")"
    rc_file="${capture_file}.rc"

    # GODS_LAWS.md L-53 (global, deletar sem lixeira) / L-09 (projeto,
    # isolamento de teste): limpeza sempre, em qualquer saida. O trap
    # de TERM/INT despeja a captura PARCIAL
    # antes de morrer - um `docker exec` cancelado a meio do job nao
    # engole a saida que ja tinha sido escrita no arquivo (declarado:
    # nenhum controle automatico exercita este ramo, ver cabecalho).
    trap 'rm -f "$capture_file" "$rc_file"' EXIT
    trap '
        echo "exec_fixture: interrompido, despejando saida parcial de fixture=$report_fixture_name" >&2
        cat "$capture_file" 2>/dev/null
        rm -f "$capture_file" "$rc_file"
        exit 143
    ' TERM INT

    # `set +e` em volta do comando: sob `set -eu`, uma fixture que sai
    # != 0 abortaria esta funcao ANTES da linha seguinte ler "$?" - o
    # mesmo idioma do passo "Build e testes" deste proprio ci.yml
    # (PNC-FASES-SNAPSHOT). Codigo de saida sempre de ARQUIVO
    # (GODS_LAWS.md L-45), nunca so' da variavel de shell.
    set +e
    "$@" >"$capture_file" 2>&1
    rc=$?
    set -e
    printf '%s' "$rc" >"$rc_file"
    rc="$(cat "$rc_file")"

    bytes="$(wc -c <"$capture_file" | tr -d '[:space:]')"

    signal="none"
    if [ "$rc" -ge 128 ]; then
        signal_num=$((rc - 128))
        if [ "$signal_num" -ge 1 ] && [ "$signal_num" -le 64 ]; then
            signal="$(kill -l "$signal_num" 2>/dev/null || echo desconhecido)"
        else
            signal="desconhecido"
        fi
    fi

    # A LINHA QUE TUDO ISTO EXISTE PARA GARANTIR: sempre primeiro,
    # sempre neste formato exato - e' o que o controle negativo 5 do
    # ci.yml (`head -1 ... | grep -q 'rc=139 signal=SEGV'`) confere.
    echo "exec_fixture: fixture=$report_fixture_name rc=$rc signal=$signal bytes=$bytes"

    # Anotacao do GitHub Actions (docs.github.com/actions workflow
    # commands): aparece no TOPO da pagina do trabalho e no resumo do
    # run, independente de onde no log foi impressa - "sinal antes do
    # ruido" tambem na INTERFACE, nao so' na linha do log.
    if [ "$rc" -ge 128 ]; then
        echo "::error title=${report_fixture_name} morreu por sinal::${signal} (rc=${rc})"
    fi

    # So' agora o conteudo capturado - e o mesmo `tee -a
    # container_measured_raw.log` que os 18 passos antigos faziam
    # (a colheita MEASURED continua identica, so' que agora depois do
    # veredito em vez de antes). Falha ao escrever no log nao pode
    # esconder um veredito ja impresso - por isso `|| true` aqui, o
    # codigo de saida final desta funcao vem so' de $rc/$bytes abaixo.
    #
    # ALVO DO LOG (achado de revisao adversarial, 11/09/2026): o passo
    # real do job (.github/workflows/ci.yml, MEASURED-COLLECTOR) roda
    # com cwd na raiz do checkout de proposito - e' o mesmo arquivo que
    # o passo seguinte publica como artefato, entao o default abaixo
    # PRECISA continuar relativo ao cwd. O --selftest sobrescreve
    # GLINTFX_MEASURED_LOG para um arquivo dentro de diretorio proprio
    # (ver selftest_main), senao os controles sinteticos escrevem e
    # sujam a raiz do repositorio de quem rodar `--selftest` local -
    # medido: reaparecia depois de apagado, sem entrar no .gitignore.
    cat "$capture_file" | tee -a "${GLINTFX_MEASURED_LOG:-container_measured_raw.log}" || true

    exit_code="$rc"
    if [ "$bytes" -eq 0 ]; then
        echo "ERRO: fixture=$report_fixture_name nao produziu nenhuma saida (bytes=0) - GODS_LAWS.md L-40: sinal de fixture quebrada, nunca sucesso silencioso" >&2
        if [ "$rc" -eq 0 ]; then
            exit_code=1
        fi
    fi

    trap - EXIT TERM INT
    rm -f "$capture_file" "$rc_file"
    exit "$exit_code"
}

# --- --selftest: quatro controles, sem Docker --------------------------
#
# Os quatro cobrem exatamente as quatro celulas que a funcao acima
# decide: sucesso limpo (P), morte por sinal (N1), reprovacao comum
# sem sinal (N2), e o piso de varredura vazia (V) - GODS_LAWS.md L-40.
#
# DESVIO DELIBERADO do comando literal do plano para o controle P
# (/var/tmp/glintfx-plan/cauda-w6b.md sec. 4.1 cita `true`): `true`
# produz ZERO bytes de saida, e o piso (bytes=0 reprova) tornaria P e
# V INDISTINGUIVEIS - um contradiz o outro se P usasse `true` ao pe'
# da letra (P exige "sai 0", V exige "REPROVA com bytes=0", os dois
# com rc=0 e saida vazia). O piso e' intencional e universal (item 5
# do contrato, GODS_LAWS.md L-40: nenhuma fixture real desta casa
# sai muda mesmo quando bem-sucedida). A troca de comando resolve a
# contradicao sem enfraquecer nenhum dos dois controles - decisao de
# implementacao, documentada aqui e no relatorio ao orquestrador, nao
# submetida ao lider (GODS_LAWS.md L-01: escolha de comando dentro de
# um selftest interno, nao design de API nem arquitetura).
selftest_positive_control() {
    if output="$(real_main --runner 'sh -c' x 'echo ok' 2>&1)"; then
        first_line="$(printf '%s\n' "$output" | head -1)"
        if printf '%s' "$first_line" | grep -q 'rc=0 signal=none' \
            && ! printf '%s' "$first_line" | grep -q 'bytes=0'; then
            echo "selftest: controle POSITIVO OK (rc=0 signal=none, saida nao-vazia)"
            return 0
        fi
    fi
    echo "selftest: controle POSITIVO FALHOU" >&2
    printf '%s\n' "${output:-}" >&2
    return 1
}

# selftest_check_one_signal <nome-do-sinal> <rc-esperado>
#
# ACHADO DE REVISAO ADVERSARIAL (11/09/2026): com um UNICO sinal
# testado (so' SEGV), uma mutacao que troca a conversao real
# (`kill -l "$signal_num"`) por um texto fixo "SEGV" passa sem ser
# pega - acerta por acidente. Testar um SEGUNDO sinal, de numero
# diferente, reprova qualquer mapeamento que nao seja a conversao de
# verdade (GODS_LAWS.md L-40: cobertura estreita e' a mesma familia
# de portao que nao olha).
selftest_check_one_signal() {
    sig_name="$1"
    expected_rc="$2"

    if output="$(real_main --runner 'sh -c' x "kill -${sig_name} \$\$" 2>&1)"; then
        echo "selftest: controle NEGATIVO (sinal ${sig_name}) FALHOU (deveria ter reprovado, saiu 0)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if printf '%s\n' "$output" | head -1 | grep -q "rc=${expected_rc} signal=${sig_name}"; then
        echo "selftest: controle NEGATIVO (sinal ${sig_name}) OK (rc=${expected_rc} signal=${sig_name} na primeira linha)"
        return 0
    fi
    echo "selftest: controle NEGATIVO (sinal ${sig_name}) FALHOU (primeira linha nao casou 'rc=${expected_rc} signal=${sig_name}')" >&2
    printf '%s\n' "$output" >&2
    return 1
}

selftest_negative_control_signal() {
    # Dois sinais de numero diferente (SEGV=11/rc=139, ABRT=6/rc=134):
    # os dois tem de casar, senao um mapeamento fixo que so' acerta um
    # deles passaria. Ambas as chamadas rodam antes de decidir o
    # resultado - reprovar cedo esconderia qual delas falhou.
    ok=0
    selftest_check_one_signal SEGV 139 || ok=1
    selftest_check_one_signal ABRT 134 || ok=1
    return "$ok"
}

selftest_negative_control_plain_failure() {
    if output="$(real_main --runner 'sh -c' x 'echo falhou; exit 1' 2>&1)"; then
        echo "selftest: controle NEGATIVO (falha comum) FALHOU (deveria ter reprovado, saiu 0)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if printf '%s\n' "$output" | head -1 | grep -q 'rc=1 signal=none'; then
        echo "selftest: controle NEGATIVO (falha comum) OK (rc=1 signal=none na primeira linha)"
        return 0
    fi
    echo "selftest: controle NEGATIVO (falha comum) FALHOU (primeira linha nao casou 'rc=1 signal=none')" >&2
    printf '%s\n' "$output" >&2
    return 1
}

selftest_empty_output_floor_control() {
    if output="$(real_main --runner 'sh -c' x 'exit 0' 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (saida vazia com rc=0 deveria ter sido recusada, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if printf '%s\n' "$output" | grep -q 'bytes=0'; then
        echo "selftest: controle de VARREDURA VAZIA OK (bytes=0 recusado)"
        return 0
    fi
    echo "selftest: controle de VARREDURA VAZIA FALHOU (reprovou, mas nao citou bytes=0)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

selftest_main() {
    # ISOLAMENTO DO LOG MEASURED (achado de revisao adversarial,
    # 11/09/2026): os quatro controles abaixo chamam real_main, que por
    # sua vez chama run_and_report - e run_and_report SEMPRE escreve em
    # GLINTFX_MEASURED_LOG (default: container_measured_raw.log no
    # cwd). Sem este diretorio proprio, rodar `--selftest` da raiz do
    # repositorio cria/suja container_measured_raw.log ali - medido por
    # experimento controlado (apagar o arquivo, rodar o selftest,
    # reaparece). GODS_LAWS.md L-53: sem lixeira, entao a limpeza no
    # `trap EXIT` abaixo e' obrigatoria, nao so' desejavel.
    selftest_log_dir="$(mktemp -d "${TMPDIR:-/tmp}/glintfx-exec-fixture-selftest-XXXXXX")"
    trap 'rm -rf "$selftest_log_dir"' EXIT
    GLINTFX_MEASURED_LOG="${selftest_log_dir}/container_measured_raw.log"
    export GLINTFX_MEASURED_LOG

    exercitados=0
    reprovados=0

    exercitados=$((exercitados + 1))
    selftest_positive_control || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_negative_control_signal || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_negative_control_plain_failure || reprovados=$((reprovados + 1))

    exercitados=$((exercitados + 1))
    selftest_empty_output_floor_control || reprovados=$((reprovados + 1))

    echo "controles: ${exercitados} exercitados, ${reprovados} reprovados"

    if [ "$reprovados" -ne 0 ]; then
        echo "exec_fixture.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "exec_fixture.sh --selftest: os quatro controles OK"
}

# --- despacho ------------------------------------------------------
#
# real_main termina em `exit` (dentro de run_and_report) - os
# controles de --selftest acima chamam real_main so' dentro de "$(...)"
# (substituicao de comando), que ja roda em SUBSHELL propria por
# definicao POSIX: o `exit` la' dentro encerra so' aquela subshell,
# nunca o processo do selftest. Em uso real, main() chama real_main
# direto (sem subshell) de proposito - o `exit` final tem que ser o
# codigo de saida do PROPRIO processo do script.

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
