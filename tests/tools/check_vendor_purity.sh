#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_vendor_purity.sh - CI gate for GODS_LAWS.md L-07 EXCECAO No 1
# (third_party/khronos/ e a UNICA excecao de glintfx a lei de
# dependencia zero) e L-40 (piso de varredura nao-vazia).
#
# O QUE ESTE PORTAO PROVA: third_party/khronos/ contem SOMENTE os
# arquivos que a excecao nomeou - os dois vendorizados verbatim do
# Khronos Group (gl.xml, LICENSE-APACHE-2.0.txt, obrigacao 1) mais o
# README.md do proprio glintfx que registra a proveniencia (obrigacao
# 4; ver .gitattributes deste repo, que ja documenta que aquele
# diretorio "also holds README.md, which is OUR OWN prose") - nada
# mais. Sem este portao, nada impede alguem de acrescentar um arquivo
# ali e a excecao crescer sozinha, um arquivo por vez, ate virar uma
# dependencia de fato - o proprio motivo de L-07 existir.
#
# A LISTA FECHADA NAO MORA AQUI: mora em khronos_vendor_files.sh (mesmo
# diretorio), sourced abaixo - a MESMA enumeracao que check_spdx.sh usa
# para decidir se um arquivo sob third_party/khronos/ e isento do
# cabecalho AGPL. Item VENDOR-PURITY, TODO.md: duas listas que
# precisam concordar sem nada as obrigando e exatamente o defeito que
# esta onda ja consertou uma vez.
#
# Varre o DIRETORIO real (find, nao `git ls-files`): um arquivo
# intruso tem de ser pego mesmo ANTES de ser commitado - o mais cedo
# que a excecao poderia comecar a crescer, e antes que um `git add`
# atomico (CLAUDE.md, "Estado atual do repositorio") tenha a chance de
# empacota-lo junto de uma fatia legitima sem ninguem notar.
#
# Usage:
#   check_vendor_purity.sh <repo-root-directory>
#   check_vendor_purity.sh --selftest
#
# --selftest roda tres controles (positivo, negativo com arquivo
# intruso, varredura vazia) contra fixtures descartaveis sob mktemp,
# nunca contra a arvore rastreada real - os tres que GODS_LAWS.md L-40
# exige de todo portao.
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

# A enumeracao fechada e a funcao de match moram em khronos_vendor_files.sh
# - unica fonte, tambem sourced por check_spdx.sh (ver o cabecalho dele
# e o do arquivo sourced). Resolvido relativo a ESTE arquivo (dirname
# "$0"), nao ao diretorio de trabalho de quem chama.
# shellcheck source=./khronos_vendor_files.sh
. "$(dirname "$0")/khronos_vendor_files.sh"

# Caminho do diretorio vendorizado, relativo a raiz do repo - o unico
# lugar que este portao varre.
VENDOR_SUBDIR="third_party/khronos"

fail() {
    echo "check_vendor_purity.sh: $1" >&2
    exit 1
}

# --- enumeracao ------------------------------------------------------

# Lista todo arquivo dentro de $VENDOR_SUBDIR/, recursivo (find -type
# f, sem -maxdepth: um intruso escondido num subdiretorio novo tambem
# conta). Caminho devolvido relativo a raiz do repo, no MESMO formato
# que khronos_vendor_files.sh usa para comparar
# ("third_party/khronos/<nome>"). Diretorio ausente nao e erro de uso -
# vira lista vazia, e require_nonempty_scan (abaixo) reprova isso como
# varredura vazia, nunca como "nada para reprovar, entao passa".
scanned_files() {
    root="$1"
    dir="$root/$VENDOR_SUBDIR"
    [ -d "$dir" ] || return 0
    find "$dir" -type f | while IFS= read -r f; do
        printf '%s\n' "${f#"$root"/}"
    done
}

count_lines() {
    if [ -z "$1" ]; then
        echo 0
        return
    fi
    printf '%s\n' "$1" | wc -l | tr -d ' '
}

# GODS_LAWS.md L-40: 0 arquivos varridos (diretorio ausente, ou
# presente e vazio) e uma FALHA, nunca um sucesso silencioso - a classe
# exata de defeito que esta lei existe para proibir. Note a diferenca
# do que este portao reprova quando ENCONTRA algo: "0 varridos" e piso
# de varredura (este bloco); "N varridos, algum fora da lista" e
# violacao de pureza (check_vendor_purity, abaixo) - duas perguntas
# diferentes, nunca fundidas na mesma checagem.
require_nonempty_scan() {
    files="$1"
    if [ -z "$files" ]; then
        echo "check_vendor_purity.sh: varredura vazia (0 arquivos em $VENDOR_SUBDIR/)" >&2
        return 1
    fi
}

# --- checagem ----------------------------------------------------------

check_vendor_purity() {
    root="$1"

    files="$(scanned_files "$root")"
    require_nonempty_scan "$files" || return 1
    file_count="$(count_lines "$files")"
    expected_count="$(known_khronos_vendor_files | wc -l | tr -d ' ')"

    intruders="$(printf '%s\n' "$files" | while IFS= read -r f; do
        is_known_khronos_vendor_file "$f" || printf '%s\n' "$f"
    done)"

    if [ -n "$intruders" ]; then
        intruder_count="$(count_lines "$intruders")"
        echo "check_vendor_purity.sh: PROIBIDO (GODS_LAWS.md L-07 EXCECAO No 1): $intruder_count arquivo(s) em $VENDOR_SUBDIR/ fora da lista fechada:" >&2
        printf '%s\n' "$intruders" | while IFS= read -r f; do
            echo "  $f" >&2
        done
        return 1
    fi

    echo "check_vendor_purity.sh: $file_count arquivo(s) varrido(s) em $VENDOR_SUBDIR/, $expected_count esperado(s) pela excecao - nenhum intruso"
}

# --- modo real -----------------------------------------------------------

require_root_dir_arg() {
    [ "$#" -eq 1 ] || fail "usage: check_vendor_purity.sh <repo-root-directory>"
    [ -d "$1" ] || fail "directory not found: $1"
}

real_main() {
    require_root_dir_arg "$@"
    check_vendor_purity "$1" || fail "arquivo fora da excecao encontrado em $VENDOR_SUBDIR/ (ver mensagem acima)"
}

# --- fixtures e controles do --selftest -----------------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-vendor-purity-selftest-XXXXXX"
}

# Arvore com EXATAMENTE os tres arquivos que a excecao nomeou -
# conteudo de fixture, nunca o gl.xml/LICENSE/README reais (nao precisa
# ser: o portao compara CAMINHO, nao conteudo nem sha256 - isso e
# trabalho do gate de integridade em tools/gl_registry_codegen, ja
# provado ali).
make_clean_fixture() {
    root="$1"
    mkdir -p "$root/$VENDOR_SUBDIR"
    printf '<comment>fixture, nao o gl.xml real</comment>\n' > "$root/$VENDOR_SUBDIR/gl.xml"
    printf 'Apache License 2.0 full text, fixture\n' > "$root/$VENDOR_SUBDIR/LICENSE-APACHE-2.0.txt"
    printf '# fixture, nao o README real\n' > "$root/$VENDOR_SUBDIR/README.md"
}

# Positive control: exatamente os dois arquivos nomeados. Esperado: passa.
selftest_positive_control() {
    scratch="$1"
    root="$scratch/positive"
    make_clean_fixture "$root"

    if output="$(check_vendor_purity "$root" 2>&1)"; then
        echo "selftest: controle POSITIVO OK (exatamente os tres arquivos nomeados, nada mais, aprovado)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Negative control: os tres arquivos legitimos MAIS um quarto,
# intruso, fora da lista fechada. Esperado: reprova, cita o caminho
# exato do intruso, nunca acusa nenhum dos tres legitimos.
selftest_negative_control() {
    scratch="$1"
    root="$scratch/negative"
    make_clean_fixture "$root"
    intruso="$root/$VENDOR_SUBDIR/mystery_vendor_file.dat"
    printf 'arquivo intruso, fora da excecao\n' > "$intruso"

    if output="$(check_vendor_purity "$root" 2>&1)"; then
        echo "selftest: controle NEGATIVO FALHOU (arquivo intruso deveria ter sido reprovado, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi

    ok=1
    if ! printf '%s\n' "$output" | grep -qF "third_party/khronos/mystery_vendor_file.dat"; then
        echo "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou o arquivo intruso pelo caminho exato)" >&2
        ok=0
    fi
    for legitimo in third_party/khronos/gl.xml third_party/khronos/LICENSE-APACHE-2.0.txt third_party/khronos/README.md; do
        if printf '%s\n' "$output" | grep -qF "$legitimo"; then
            echo "selftest: controle NEGATIVO FALHOU (acusou o arquivo legitimo '$legitimo')" >&2
            ok=0
        fi
    done

    if [ "$ok" -eq 1 ]; then
        echo "selftest: controle NEGATIVO OK (intruso citado pelo caminho exato, os tres legitimos intactos)"
        printf '%s\n' "$output" >&2
        return 0
    fi
    printf '%s\n' "$output" >&2
    return 1
}

# Empty-scan floor: $VENDOR_SUBDIR nem existe na fixture. Esperado:
# reprova com "varredura vazia" na mensagem - o proprio motivo desta
# lei existir (GODS_LAWS.md L-40), nunca "diretorio ausente, entao
# nada a reprovar, passa".
selftest_empty_scan_control() {
    scratch="$1"
    root="$scratch/empty"
    mkdir -p "$root"

    if output="$(check_vendor_purity "$root" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU ($VENDOR_SUBDIR/ ausente deveria ter sido recusado, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK ($VENDOR_SUBDIR/ ausente recusado, nunca presumido vazio-e-ok)"
    return 0
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control "$scratch" || overall=1
    selftest_empty_scan_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_vendor_purity.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_vendor_purity.sh --selftest: os tres controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
