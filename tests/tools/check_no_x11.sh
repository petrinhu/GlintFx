#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_no_x11.sh - CI gate for GODS_LAWS.md L-05 (Linux e Wayland
# puro, sem X11) e L-06 (parser de keymap proprio; libxkbcommon fora).
#
# Nasceu da extracao do bloco inline que vivia no job `leis` de
# .github/workflows/ci.yml, reprovado por GODS_LAWS.md L-40 (piso de
# varredura nao-vazia) por tres motivos medidos:
#
#   (a) VARREDURA VAZIA SAIA VERDE. A versao anterior fazia
#       `[ -z "$dirs" ] && { echo "nada a varrer"; exit 0; }` - exatamente
#       a forma que a L-40 cita pelo nome como o sexto caso medido.
#       Aqui, varredura vazia REPROVA (ver require_nonempty_scan), com a
#       contagem na mensagem.
#
#   (b) A LISTA DE DIRETORIOS NAO COBRIA tools/ NEM .github/. A L-05
#       proibe X11 no repositorio INTEIRO, nao so no codigo de produto;
#       um script de ferramenta ou de CI e exatamente onde um exemplo
#       de internet copiado traz Xvfb/xdotool/XWayland. Agora entram
#       na varredura (ver code_dirs).
#
#   (c) A LISTA DE TERMOS PROIBIDOS ERA MAIS CURTA QUE A LEI. Cobria
#       cinco termos (Xlib.h, xcb/, XOpenDisplay, XTestFake,
#       xkbcommon); a L-05 cita nominalmente o fallback por XWayland, e
#       CLAUDE.md ("Isolamento obrigatorio de teste") lista Xvfb,
#       xvfb-run e xdotool como PROIBIDOS por nome neste repositorio.
#       X11/ cobre qualquer caminho de header X11 generico alem de
#       Xlib.h. Ver forbidden_pattern().
#
# AUTOEXCLUSAO, explicita e estreita (nao um buraco): este PROPRIO
# arquivo precisa conter, em texto puro, cada termo que ele proibe -
# sem isso nao ha o que varrer contra. Ele se auto-acusaria se fosse
# incluido na propria varredura. A exclusao e do CAMINHO EXATO deste
# arquivo (SELF_PATH_SUFFIX abaixo), nunca de um diretorio, nunca de
# outro arquivo. Nada mais escapa: um irmao no MESMO diretorio
# (tests/tools/) com um termo proibido continua reprovando - e o
# controle negativo "self_dir_sibling" do --selftest existe para
# provar exatamente isso, porque excluir o arquivo errado (ou de mais)
# e a forma mais facil de reabrir o buraco que este gate existe para
# fechar.
#
# Usage:
#   check_no_x11.sh <repo-root-directory>
#   check_no_x11.sh --selftest
#
# --selftest roda quatro controles (positivo, negativo, varredura
# vazia, e autoexclusao estreita) contra uma fixture descartavel sob
# mktemp, nunca contra a arvore rastreada real - GODS_LAWS.md L-40 exige
# os tres primeiros no autoteste de todo portao; o quarto e especifico
# deste gate, por causa da autoexclusao acima.
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# Caminho deste arquivo relativo a raiz do repo, usado tanto no modo
# real quanto nas fixtures do --selftest (ver make_self_excluded_fixture_file).
SELF_PATH_SUFFIX="tests/tools/check_no_x11.sh"

fail() {
    echo "check_no_x11.sh: $1" >&2
    exit 1
}

# --- padrao proibido, derivado da L-05/L-06 (GODS_LAWS.md), nao de
# memoria - ver o cabecalho acima para a origem de cada termo. --------

forbidden_pattern() {
    printf '%s' 'Xlib\.h|xcb/|XOpenDisplay|XTestFake|xkbcommon|XWayland|Xvfb|xvfb-run|xdotool|X11/'
}

# --- enumeracao ------------------------------------------------------

code_dirs() {
    root="$1"
    for candidate in src include tests examples demos cmake tools .github; do
        [ -d "$root/$candidate" ] && printf '%s\n' "$root/$candidate"
    done
}

# Um find(1) por diretorio candidato (mesma razao de check_layers.sh:
# evita que um caminho com espaco seja quebrado por word-splitting).
# A autoexclusao acontece aqui, no unico ponto que enumera arquivo por
# arquivo - antes de qualquer grep.
scanned_files() {
    root="$1"
    self_path="$root/$SELF_PATH_SUFFIX"
    code_dirs "$root" | while IFS= read -r dir; do
        find "$dir" -type f
    done | while IFS= read -r f; do
        [ "$f" = "$self_path" ] && continue
        printf '%s\n' "$f"
    done
}

count_lines() {
    if [ -z "$1" ]; then
        echo 0
        return
    fi
    printf '%s\n' "$1" | wc -l | tr -d ' '
}

require_nonempty_scan() {
    files="$1"
    if [ -z "$files" ]; then
        echo "check_no_x11.sh: varredura vazia (0 arquivos)" >&2
        return 1
    fi
}

violations_in_file() {
    f="$1"
    pattern="$2"
    grep -nE "$pattern" "$f" 2>/dev/null | while IFS=: read -r line _; do
        printf '%s:%s\n' "$f" "$line"
    done
}

# --- checagem ----------------------------------------------------------

check_no_x11() {
    root="$1"

    files="$(scanned_files "$root")"
    require_nonempty_scan "$files" || return 1
    file_count="$(count_lines "$files")"

    pattern="$(forbidden_pattern)"
    violations="$(printf '%s\n' "$files" | while IFS= read -r f; do violations_in_file "$f" "$pattern"; done)"

    if [ -n "$violations" ]; then
        echo "check_no_x11.sh: PROIBIDO (GODS_LAWS.md L-05 Wayland puro, L-06 keymap proprio):" >&2
        printf '%s\n' "$violations" >&2
        return 1
    fi

    violation_count=0
    echo "check_no_x11.sh: $violation_count ocorrencia(s) em $file_count arquivo(s) varrido(s)"
}

# --- modo real -----------------------------------------------------------

require_root_dir_arg() {
    [ "$#" -eq 1 ] || fail "usage: check_no_x11.sh <repo-root-directory>"
    [ -d "$1" ] || fail "directory not found: $1"
}

real_main() {
    require_root_dir_arg "$@"
    check_no_x11 "$1" || fail "X11 ou libxkbcommon proibidos encontrados (ver mensagem acima)"
}

# --- fixtures e controles do --selftest -----------------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-no-x11-selftest-XXXXXX"
}

# Arvore minima com um diretorio varrido (src/) e um arquivo limpo.
make_clean_fixture() {
    root="$1"
    mkdir -p "$root/src"
    printf '// codigo limpo, sem termo proibido\n' > "$root/src/janela.cpp"
}

# Positive control: fixture limpa em src/. Esperado: passa.
selftest_positive_control() {
    scratch="$1"
    root="$scratch/positive"
    make_clean_fixture "$root"

    if output="$(check_no_x11 "$root" 2>&1)"; then
        echo "selftest: controle POSITIVO OK (fixture limpa aprovada)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Negative control: planta CADA termo proibido em tools/ e em .github/
# (os dois diretorios que a versao anterior NAO varria - achado (b) do
# cabecalho). Esperado: cada termo, em cada diretorio novo, reprova e
# aparece na mensagem.
selftest_negative_control() {
    scratch="$1"
    root="$scratch/negative"
    make_clean_fixture "$root"
    mkdir -p "$root/tools/ci" "$root/.github/workflows"

    overall=0
    for termo in 'Xlib.h' 'xcb/foo' 'XOpenDisplay' 'XTestFakeKeyEvent' 'xkbcommon' \
                 'XWayland' 'Xvfb' 'xvfb-run' 'xdotool' 'X11/Xutil.h'; do
        slug="$(printf '%s' "$termo" | tr -c 'a-zA-Z0-9' '_')"
        alvo="$root/tools/ci/plantado_${slug}.sh"
        printf '# %s\n' "$termo" > "$alvo"

        if output="$(check_no_x11 "$root" 2>&1)"; then
            echo "selftest: controle NEGATIVO FALHOU (termo '$termo' em tools/ nao foi pego)" >&2
            overall=1
        elif ! printf '%s\n' "$output" | grep -qF "$alvo"; then
            echo "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou $alvo)" >&2
            printf '%s\n' "$output" >&2
            overall=1
        fi
        rm -f "$alvo"
    done

    # Mesmo termo, agora em .github/workflows/ (o segundo diretorio novo).
    alvo="$root/.github/workflows/plantado.yml"
    printf 'run: xvfb-run ./demo\n' > "$alvo"
    if output="$(check_no_x11 "$root" 2>&1)"; then
        echo "selftest: controle NEGATIVO FALHOU (termo em .github/ nao foi pego)" >&2
        overall=1
    elif ! printf '%s\n' "$output" | grep -qF "$alvo"; then
        echo "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou $alvo)" >&2
        printf '%s\n' "$output" >&2
        overall=1
    fi
    rm -f "$alvo"

    [ "$overall" -eq 0 ] && echo "selftest: controle NEGATIVO OK (dez termos, tools/ e .github/, todos pegos e citados)"
    return "$overall"
}

# Empty-scan floor: nenhum dos diretorios varridos existe na fixture.
# Esperado: reprova com "varredura vazia" na mensagem - e o achado (a)
# do cabecalho, o proprio motivo desta lei existir.
selftest_empty_scan_control() {
    scratch="$1"
    root="$scratch/empty"
    mkdir -p "$root"

    if output="$(check_no_x11 "$root" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar raiz sem diretorio de codigo, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK (raiz sem diretorio de codigo recusada)"
    return 0
}

# Controle de autoexclusao ESTREITA: planta um termo proibido dentro do
# arquivo self-excluido (deve ser ignorado, senao o gate se acusa
# sozinho) E, no MESMO diretorio, num arquivo IRMAO (deve reprovar - a
# prova de que a exclusao e de um arquivo so, nao do diretorio inteiro).
selftest_self_exclusion_control() {
    scratch="$1"
    root="$scratch/self_exclusion"
    make_clean_fixture "$root"
    self_dir="$root/tests/tools"
    mkdir -p "$self_dir"

    # O proprio arquivo self-excluido, plantado com termo proibido:
    # tem de PASSAR, senao este script se acusaria a cada execucao real.
    printf '# contem Xvfb de proposito, e o alvo da autoexclusao\n' > "$self_dir/check_no_x11.sh"
    if ! output="$(check_no_x11 "$root" 2>&1)"; then
        echo "selftest: controle de AUTOEXCLUSAO FALHOU (o proprio arquivo excluido foi pego - o gate se acusaria sozinho em producao)" >&2
        printf '%s\n' "$output" >&2
        rm -f "$self_dir/check_no_x11.sh"
        return 1
    fi

    # Irmao no MESMO diretorio, termo diferente: tem de REPROVAR - prova
    # que a exclusao nao vazou para o diretorio tests/tools/ inteiro.
    alvo="$self_dir/outro_arquivo.sh"
    printf '# xdotool plantado no irmao, nao no arquivo excluido\n' > "$alvo"
    if output="$(check_no_x11 "$root" 2>&1)"; then
        echo "selftest: controle de AUTOEXCLUSAO FALHOU (irmao no mesmo diretorio do arquivo excluido escapou - a exclusao vazou para o diretorio inteiro)" >&2
        rm -f "$self_dir/check_no_x11.sh" "$alvo"
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "$alvo"; then
        echo "selftest: controle de AUTOEXCLUSAO FALHOU (reprovou o irmao, mas nao o citou na mensagem)" >&2
        printf '%s\n' "$output" >&2
        rm -f "$self_dir/check_no_x11.sh" "$alvo"
        return 1
    fi

    rm -f "$self_dir/check_no_x11.sh" "$alvo"
    echo "selftest: controle de AUTOEXCLUSAO OK (arquivo excluido passa, irmao no mesmo diretorio reprova)"
    return 0
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control "$scratch" || overall=1
    selftest_empty_scan_control "$scratch" || overall=1
    selftest_self_exclusion_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_no_x11.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_no_x11.sh --selftest: os quatro controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
