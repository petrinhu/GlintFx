#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_dup_laws.sh - CI gate for the mitigation proposed in
# /var/tmp/glintfx-plan/separacao-leis-escopo.md section 5, applied in
# 26/08/2026 (fase 2 da separacao LEI/ESCOPO, ordem do lider: "As god
# laws sao leis minhas para voce executar ao fazer o projeto, nao
# decisoes sobre o projeto").
#
# GODS_LAWS.md e ESCOPO.md tem blocos DUPLICADOS por inteiro (nao
# ponteiro): as leis L-05 e L-07 por decisao explicita do lider (as
# duas opcoes apresentadas a ele diziam que duas copias divergem se so
# uma for editada, e ele escolheu duplicar mesmo assim), e L-06, L-19,
# L-22, L-26 e a LEI ZERO por duvida GENUINA de classificacao do
# orquestrador (regra aplicada: "onde houver duvida, DUPLICA", para o
# lider poder desfazer depois sem perder nada).
#
# Cada bloco duplicado e envolvido, nos DOIS arquivos, por um par de
# ancoras identicas:
#
#   <!-- DUP-BLOCK:ID:START -->
#   ...texto...
#   <!-- DUP-BLOCK:ID:END -->
#
# Este portao prova que as duas copias de cada ID continuam byte a
# byte iguais. Se um dia divergirem (alguem editou um lado e esqueceu
# do outro), ele reprova.
#
# Aplica GODS_LAWS.md L-40 (piso de varredura nao-vazia) em DUAS
# camadas: (a) zero blocos encontrados em QUALQUER um dos dois
# arquivos e' reprovacao, nunca sucesso silencioso; (b) o conjunto de
# IDs tem de ser o MESMO nos dois arquivos - um ID que existe so de um
# lado (ancora apagada, renomeada, ou copia esquecida ao criar bloco
# novo) tambem reprova, mesmo que os blocos que EXISTEM nos dois
# batam.
#
# Usage:
#   check_dup_laws.sh <repo-root-directory>
#   check_dup_laws.sh --selftest
#
# --selftest roda quatro controles (positivo, negativo, varredura
# vazia, e conjunto de IDs divergente) contra fixtures descartaveis
# sob mktemp, nunca contra a arvore rastreada real.
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_dup_laws.sh: $1" >&2
    exit 1
}

# --- extracao ----------------------------------------------------------

# Lista os IDs de bloco presentes num arquivo, um por linha, na ordem
# em que aparecem. Usa so as marcas de START - o casamento com o END
# correspondente e' verificado em block_text() abaixo.
block_ids() {
    file="$1"
    grep -oE '<!-- DUP-BLOCK:[A-Za-z0-9_-]+:START -->' "$file" 2>/dev/null \
        | sed -E 's/<!-- DUP-BLOCK:([A-Za-z0-9_-]+):START -->/\1/'
}

# Extrai o texto de UM bloco (entre as duas ancoras do mesmo ID),
# excluindo as proprias linhas de ancora. Reprova (saida vazia, chamador
# confere) se o ID nao existir ou se START/END nao casarem.
block_text() {
    file="$1"
    id="$2"
    sed -n "/<!-- DUP-BLOCK:${id}:START -->/,/<!-- DUP-BLOCK:${id}:END -->/p" "$file" | sed '1d;$d'
}

sorted_unique() {
    printf '%s\n' "$1" | sort -u
}

# --- checagem ------------------------------------------------------------

# Devolve 0 e imprime o resumo se os dois arquivos tiverem o MESMO
# conjunto de IDs e cada bloco bater byte a byte; devolve 1 e imprime o
# diagnostico caso contrario. Nunca aprova com zero blocos.
check_dup_laws() {
    gods_laws="$1"
    escopo="$2"

    ids_a="$(block_ids "$gods_laws")"
    ids_b="$(block_ids "$escopo")"

    if [ -z "$ids_a" ] && [ -z "$ids_b" ]; then
        echo "check_dup_laws.sh: varredura vazia (0 blocos DUP-BLOCK em qualquer um dos dois arquivos)" >&2
        return 1
    fi

    sorted_a="$(sorted_unique "$ids_a")"
    sorted_b="$(sorted_unique "$ids_b")"

    if [ "$sorted_a" != "$sorted_b" ]; then
        echo "check_dup_laws.sh: CONJUNTO DE IDs DIVERGENTE entre '$gods_laws' e '$escopo':" >&2
        echo "  IDs em $gods_laws:" >&2
        printf '%s\n' "$sorted_a" | sed 's/^/    - /' >&2
        echo "  IDs em $escopo:" >&2
        printf '%s\n' "$sorted_b" | sed 's/^/    - /' >&2
        return 1
    fi

    count=0
    mismatch=0
    while IFS= read -r id; do
        [ -z "$id" ] && continue
        count=$((count + 1))
        text_a="$(block_text "$gods_laws" "$id")"
        text_b="$(block_text "$escopo" "$id")"
        if [ "$text_a" != "$text_b" ]; then
            echo "check_dup_laws.sh: bloco '$id' DIVERGE entre os dois arquivos:" >&2
            diff -u <(printf '%s\n' "$text_a") <(printf '%s\n' "$text_b") >&2 || true
            mismatch=1
        fi
    done <<EOF
$sorted_a
EOF

    if [ "$mismatch" -ne 0 ]; then
        return 1
    fi

    echo "check_dup_laws.sh: $count bloco(s) duplicado(s) comparados, 0 divergencias"
}

# --- modo real -----------------------------------------------------------

require_root_dir_arg() {
    [ "$#" -eq 1 ] || fail "usage: check_dup_laws.sh <repo-root-directory>"
    [ -d "$1" ] || fail "directory not found: $1"
}

real_main() {
    require_root_dir_arg "$@"
    root="$1"
    gods_laws="$root/GODS_LAWS.md"
    escopo="$root/ESCOPO.md"
    [ -f "$gods_laws" ] || fail "arquivo nao encontrado: $gods_laws"
    [ -f "$escopo" ] || fail "arquivo nao encontrado: $escopo"
    check_dup_laws "$gods_laws" "$escopo" || fail "blocos duplicados divergiram ou conjunto de IDs nao bate (ver mensagem acima)"
}

# --- fixtures e controles do --selftest -----------------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-dup-laws-selftest-XXXXXX"
}

# Positive control: dois arquivos com dois blocos idênticos cada.
# Esperado: passa, citando "2 bloco(s)".
selftest_positive_control() {
    scratch="$1"
    a="$scratch/positive_a.md"
    b="$scratch/positive_b.md"
    cat > "$a" <<'FIXTURE'
# doc A

<!-- DUP-BLOCK:ALPHA:START -->
texto identico alpha
<!-- DUP-BLOCK:ALPHA:END -->

<!-- DUP-BLOCK:BETA:START -->
texto identico beta
com duas linhas
<!-- DUP-BLOCK:BETA:END -->
FIXTURE
    cat > "$b" <<'FIXTURE'
# doc B

<!-- DUP-BLOCK:ALPHA:START -->
texto identico alpha
<!-- DUP-BLOCK:ALPHA:END -->

<!-- DUP-BLOCK:BETA:START -->
texto identico beta
com duas linhas
<!-- DUP-BLOCK:BETA:END -->
FIXTURE

    if output="$(check_dup_laws "$a" "$b" 2>&1)" && printf '%s\n' "$output" | grep -qF "2 bloco(s)"; then
        echo "selftest: controle POSITIVO OK (dois blocos identicos aprovados)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU" >&2
    printf '%s\n' "${output:-}" >&2
    return 1
}

# Negative control: mesmo par de IDs, um bloco diverge por uma palavra.
# Esperado: reprova, citando o ID divergente.
selftest_negative_control() {
    scratch="$1"
    a="$scratch/negative_a.md"
    b="$scratch/negative_b.md"
    cat > "$a" <<'FIXTURE'
<!-- DUP-BLOCK:GAMMA:START -->
versao original do texto
<!-- DUP-BLOCK:GAMMA:END -->
FIXTURE
    cat > "$b" <<'FIXTURE'
<!-- DUP-BLOCK:GAMMA:START -->
versao EDITADA do texto
<!-- DUP-BLOCK:GAMMA:END -->
FIXTURE

    if output="$(check_dup_laws "$a" "$b" 2>&1)"; then
        echo "selftest: controle NEGATIVO FALHOU (bloco divergente foi aprovado)" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "GAMMA"; then
        echo "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou o ID GAMMA)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle NEGATIVO OK (bloco divergente reprovado e citado)"
    return 0
}

# Empty-scan floor: nenhum dos dois arquivos tem qualquer DUP-BLOCK.
# Esperado: reprova com "varredura vazia" na mensagem.
selftest_empty_scan_control() {
    scratch="$1"
    a="$scratch/empty_a.md"
    b="$scratch/empty_b.md"
    printf '# doc sem blocos\n' > "$a"
    printf '# outro doc sem blocos\n' > "$b"

    if output="$(check_dup_laws "$a" "$b" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (dois arquivos sem bloco algum deveriam reprovar)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK"
    return 0
}

# Conjunto de IDs divergente: um bloco existe SO num dos dois arquivos
# (ancora apagada ou copia esquecida). Esperado: reprova, mesmo que o
# bloco que existe nos dois seja identico - este e' o controle que a
# comparacao "so o que existe nos dois" NUNCA pegaria sozinha.
selftest_id_set_mismatch_control() {
    scratch="$1"
    a="$scratch/mismatch_a.md"
    b="$scratch/mismatch_b.md"
    cat > "$a" <<'FIXTURE'
<!-- DUP-BLOCK:DELTA:START -->
bloco presente nos dois, identico
<!-- DUP-BLOCK:DELTA:END -->

<!-- DUP-BLOCK:SO_EM_A:START -->
este bloco so existe no arquivo A
<!-- DUP-BLOCK:SO_EM_A:END -->
FIXTURE
    cat > "$b" <<'FIXTURE'
<!-- DUP-BLOCK:DELTA:START -->
bloco presente nos dois, identico
<!-- DUP-BLOCK:DELTA:END -->
FIXTURE

    if output="$(check_dup_laws "$a" "$b" 2>&1)"; then
        echo "selftest: controle de CONJUNTO DE IDs FALHOU (bloco so-em-A deveria ter reprovado, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "SO_EM_A"; then
        echo "selftest: controle de CONJUNTO DE IDs FALHOU (reprovou, mas nao citou SO_EM_A)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de CONJUNTO DE IDs OK (bloco orfao detectado mesmo com o par comum identico)"
    return 0
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control "$scratch" || overall=1
    selftest_empty_scan_control "$scratch" || overall=1
    selftest_id_set_mismatch_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_dup_laws.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_dup_laws.sh --selftest: os quatro controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
