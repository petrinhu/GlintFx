#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_spdx.sh - CI gate for repo-wide "SPDX-License-Identifier"
# header coverage (GODS_LAWS.md L-08: publico no GitHub, AGPL-3.0).
# Decisao do lider, onda W2, item SPDX-GATE, 25/08/2026: entre este
# portao e o de ancora morta nos manuais, ele escolheu SO este -
# arquivo nascido sem cabecalho enfraquece a licenca perante quem
# consome a lib de fora (LEI ZERO: consumidor desconhecido, nao um so),
# e o dano e juridico, nao cosmetico.
#
# ORIGEM: modelo herdado de /var/tmp/glintfx-plan/spdx-check/check_spdx.sh,
# deixado pela revisao adversarial de 22/08/2026 (rev-spdx.md), que
# auditou o item A10/e393279 e confirmou cobertura de 100% dos 35
# arquivos de codigo/build entao existentes, com os tres controles da
# GODS_LAWS.md L-40 (nao existia ainda como lei numerada naquele dia,
# mas o metodo ja era esse) ja provados contra aquele modelo. O achado
# 1 daquela revisao propos exatamente este arquivo como portao futuro -
# procurado antes de escrever do zero, como pedido.
#
# O QUE MUDOU em relacao ao modelo herdado: o modelo original recebia
# a LISTA de arquivos a varrer via stdin - a decisao de QUAIS arquivos
# entravam era tomada por fora, por busca dirigida (grep de extensao).
# Este arquivo enumera ele mesmo, com `git ls-files` (TODO arquivo
# rastreado, o universo fechado por construcao que GODS_LAWS.md L-40
# item 5 pede), e classifica cada um: por padrao EXIGE cabecalho, e so
# escapa quem estiver na lista curta e nomeada de excecoes abaixo (ver
# is_exempt). Extensao nova nascida amanha (.rs, .py, o que for) cai
# do lado "exige" sem precisar de outra edicao aqui - e exatamente o
# oposto do defeito (b) que check_no_x11.sh documenta no proprio
# cabecalho (lista de diretorios que nao cobria tools/ nem .github/).
#
# EXCECOES DECLARADAS, e por que cada uma (GODS_LAWS.md L-40: excecao
# nao declarada vira buraco):
#
#   *.md         - documentacao, nao codigo nem build (mesma
#                  classificacao da revisao de 22/08, rev-spdx.md).
#   LICENSE       - e o proprio texto legal da licenca; cabecalho SPDX
#                  dentro dele nao faz sentido (mesmo julgamento).
#   .gitignore     - configuracao declarativa, nao codigo/build (mesmo
#                  julgamento).
#   .bigtech-porte  - marcador de uma linha, sem logica, sem conteudo
#                  autoral relevante para copyright de codigo (mesmo
#                  julgamento; la registrado como achado de baixa
#                  prioridade, aqui formalizado no portao).
#   .claude/**      - configuracao do Claude Code em si (hooks, settings),
#                  nao codigo nem build do PRODUTO GlintFx.
#   *.json          - JSON padrao (RFC 8259) nao aceita comentario; nao
#                  ha sintaxe de cabecalho que nao quebre o arquivo.
#                  Unico caso hoje: .claude/settings.json, ja coberto
#                  pela excecao acima - mantida como regra propria
#                  porque um .json fora de .claude/ tambem cairia aqui
#                  por razao tecnica, nao por ser Claude Code.
#
# Cada excecao e conferida no CAMINHO EXATO relativo a raiz do repo,
# nunca por substring - "README.md" nao esconde "README.md.bak".
#
# Usage:
#   check_spdx.sh <repo-root-directory>
#   check_spdx.sh --selftest
#
# --selftest monta arvores descartaveis sob mktemp e as transforma em
# repositorio git de verdade (`git init` + `git add`) - a enumeracao
# real E `git ls-files`, entao a fixture PRECISA ser um repo, diferente
# das fixtures de check_no_x11.sh (que usa find puro sobre diretorio).
# Roda os tres controles que GODS_LAWS.md L-40 exige de todo portao:
# positivo (cabecalho presente passa, e arquivo isento sem cabecalho
# tambem passa), negativo (cabecalho ausente reprova e o caminho exato
# aparece na mensagem), e varredura vazia (repo git sem nenhum arquivo
# rastreado reprova, nunca passa em silencio - e o proprio motivo desta
# lei existir, GODS_LAWS.md L-40 caso "portao da lei do Wayland").
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

REQUIRED_HEADER='SPDX-License-Identifier: AGPL-3.0-or-later'

fail() {
    echo "check_spdx.sh: $1" >&2
    exit 1
}

# --- classificacao, fechada por construcao (GODS_LAWS.md L-40 item 5):
# excecao e uma lista curta e NOMEADA, nunca um padrao amplo tipo "todo
# arquivo sem extensao conhecida". Ver o cabecalho acima para a razao
# de cada linha. -----------------------------------------------------

is_exempt() {
    path="$1"
    case "$path" in
        *.md) return 0 ;;
        LICENSE) return 0 ;;
        .gitignore) return 0 ;;
        .bigtech-porte) return 0 ;;
        .claude/*) return 0 ;;
        *.json) return 0 ;;
        *) return 1 ;;
    esac
}

# --- enumeracao -------------------------------------------------------

# `git ls-files` e o universo fechado por construcao: TODO arquivo
# rastreado, nada gerado, nada ignorado por .gitignore, nada de
# artefato de build - sem precisar manter, aqui, uma lista propria de
# diretorios a varrer (a lista propria e exatamente o defeito (b) que
# check_no_x11.sh documenta no proprio cabecalho). Uma falha real do
# git (raiz nao e repositorio, git ausente) NUNCA vira "nao achei nada,
# entao passa" - fica em `|| return 1`, nunca engolida.
scanned_files() {
    root="$1"
    git -C "$root" ls-files
}

require_nonempty_scan() {
    files="$1"
    if [ -z "$files" ]; then
        echo "check_spdx.sh: varredura vazia (0 arquivos rastreados)" >&2
        return 1
    fi
}

count_lines() {
    if [ -z "$1" ]; then
        echo 0
        return
    fi
    printf '%s\n' "$1" | wc -l | tr -d ' '
}

# Cada uma destas duas funcoes SO IMPRIME no stdout (nunca acumula em
# variavel dentro do corpo do loop) - o mesmo cuidado que
# check_no_x11.sh registra no proprio codigo (violations_in_file): um
# `| while read` roda em subshell sob `sh`, e variavel setada la dentro
# some ao sair do pipe. Capturar via `$(...)` e o unico jeito seguro.

required_files() {
    files="$1"
    printf '%s\n' "$files" | while IFS= read -r f; do
        [ -z "$f" ] && continue
        is_exempt "$f" && continue
        printf '%s\n' "$f"
    done
}

missing_headers() {
    root="$1"
    required="$2"
    printf '%s\n' "$required" | while IFS= read -r f; do
        [ -z "$f" ] && continue
        head -n 3 "$root/$f" 2>/dev/null | grep -qF "$REQUIRED_HEADER" && continue
        printf '%s\n' "$f"
    done
}

# --- checagem ----------------------------------------------------------

check_spdx() {
    root="$1"

    files="$(scanned_files "$root")" \
        || { echo "check_spdx.sh: 'git ls-files' falhou em '$root' (nao e repositorio git, ou git indisponivel) - varredura recusada, nunca presumida vazia" >&2; return 1; }
    require_nonempty_scan "$files" || return 1
    total_count="$(count_lines "$files")"

    required="$(required_files "$files")"
    required_count="$(count_lines "$required")"
    exempt_count=$((total_count - required_count))

    missing="$(missing_headers "$root" "$required")"

    if [ -n "$missing" ]; then
        missing_count="$(count_lines "$missing")"
        echo "check_spdx.sh: PROIBIDO (GODS_LAWS.md L-08, publico sob AGPL-3.0): $missing_count arquivo(s) sem '$REQUIRED_HEADER' nas 3 primeiras linhas:" >&2
        printf '%s\n' "$missing" | while IFS= read -r f; do
            echo "  $f" >&2
        done
        return 1
    fi

    echo "check_spdx.sh: 0 arquivo(s) sem cabecalho ($required_count exige(m), $exempt_count isento(s), $total_count rastreado(s) no total)"
}

# --- modo real -----------------------------------------------------------

require_root_dir_arg() {
    [ "$#" -eq 1 ] || fail "usage: check_spdx.sh <repo-root-directory>"
    [ -d "$1" ] || fail "directory not found: $1"
}

real_main() {
    require_root_dir_arg "$@"
    check_spdx "$1" || fail "cabecalho SPDX ausente em arquivo nao isento (ver mensagem acima)"
}

# --- fixtures e controles do --selftest -----------------------------------

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-spdx-selftest-XXXXXX"
}

# `git init` isolado e determinista: identidade de commit ficticia, sem
# tocar config global da maquina, para que --selftest nunca dependa de
# `git config --global user.*` estar setado no ambiente que o chama
# (CI ou local).
init_fixture_repo() {
    root="$1"
    mkdir -p "$root"
    git -C "$root" init -q
    git -C "$root" config user.email "selftest@check-spdx.invalid"
    git -C "$root" config user.name "check_spdx selftest"
}

track_all() {
    root="$1"
    git -C "$root" add -A
}

# Arvore com um arquivo de codigo COM cabecalho e um punhado de
# arquivos isentos SEM cabecalho - todos os seis ramos de is_exempt,
# um por um, para que o controle positivo prove que NENHUM deles e
# falsamente cobrado.
make_positive_fixture() {
    root="$1"
    mkdir -p "$root/src" "$root/.claude"
    printf '// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n' > "$root/src/foo.cpp"
    printf '# doc sem cabecalho\n' > "$root/README.md"
    printf 'texto legal, sem cabecalho\n' > "$root/LICENSE"
    printf 'build/\n' > "$root/.gitignore"
    printf 'porte=bigtech\n' > "$root/.bigtech-porte"
    printf '{\n  "chave": "valor"\n}\n' > "$root/.claude/settings.json"
    printf '{\n  "outra": true\n}\n' > "$root/config.json"
    track_all "$root"
}

selftest_positive_control() {
    scratch="$1"
    root="$scratch/positive"
    make_positive_fixture "$root"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle POSITIVO OK (arquivo com cabecalho passa, seis ramos de excecao sem cabecalho nao sao cobrados)"
        return 0
    fi
    echo "selftest: controle POSITIVO FALHOU" >&2
    printf '%s\n' "$output" >&2
    return 1
}

# Mesma arvore do positivo, mais um segundo arquivo de codigo SEM
# cabecalho. Esperado: reprova, cita o caminho exato do arquivo faltante
# e NAO cita nenhum dos seis arquivos isentos.
selftest_negative_control() {
    scratch="$1"
    root="$scratch/negative"
    make_positive_fixture "$root"
    printf 'int g();\n' > "$root/src/bar.cpp"
    track_all "$root"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle NEGATIVO FALHOU (src/bar.cpp sem cabecalho deveria ter sido reprovado)" >&2
        return 1
    fi

    ok=1
    if ! printf '%s\n' "$output" | grep -qF "src/bar.cpp"; then
        echo "selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou src/bar.cpp)" >&2
        ok=0
    fi
    for isento in README.md LICENSE .gitignore .bigtech-porte .claude/settings.json config.json; do
        if printf '%s\n' "$output" | grep -qF "$isento"; then
            echo "selftest: controle NEGATIVO FALHOU (cobrou arquivo isento '$isento' - excecao vazou)" >&2
            ok=0
        fi
    done

    if [ "$ok" -eq 1 ]; then
        echo "selftest: controle NEGATIVO OK (src/bar.cpp citado, nenhum arquivo isento cobrado)"
        printf '%s\n' "$output" >&2
        return 0
    fi
    printf '%s\n' "$output" >&2
    return 1
}

# Empty-scan floor: repositorio git valido, mas SEM nenhum arquivo
# rastreado (init sem add). Esperado: reprova com "varredura vazia" na
# mensagem - o proprio motivo desta lei existir (GODS_LAWS.md L-40).
selftest_empty_scan_control() {
    scratch="$1"
    root="$scratch/empty"
    init_fixture_repo "$root"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (repo git sem arquivo rastreado deveria ter sido recusado, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "varredura vazia"; then
        echo "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de VARREDURA VAZIA OK (repo git sem arquivo rastreado recusado)"
    return 0
}

# Controle extra, especifico deste gate (alem dos tres que a L-40
# exige): raiz que NAO e repositorio git nenhum. Esperado: git ls-files
# falha, e a falha vira reprovacao explicita - nunca "nao achei
# arquivo, entao passa" (a mesma falha de git que preci.sh's
# enumerate_untracked_cpp_hpp ja documenta e recusa).
selftest_not_a_repo_control() {
    scratch="$1"
    root="$scratch/not_a_repo"
    mkdir -p "$root/src"
    printf 'int h();\n' > "$root/src/baz.cpp"

    if output="$(check_spdx "$root" 2>&1)"; then
        echo "selftest: controle de NAO-E-REPO FALHOU (diretorio sem .git deveria ter sido recusado, mas passou)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    if ! printf '%s\n' "$output" | grep -qF "nao e repositorio git"; then
        echo "selftest: controle de NAO-E-REPO FALHOU (recusou, mas nao disse o motivo)" >&2
        printf '%s\n' "$output" >&2
        return 1
    fi
    echo "selftest: controle de NAO-E-REPO OK (diretorio sem .git recusado, nao presumido vazio)"
    return 0
}

selftest_main() {
    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    # positive/negative precisam de `git init` + `git config` antes de
    # `git add`; make_positive_fixture so cria arquivos, entao os dois
    # controles chamam init_fixture_repo primeiro.
    mkdir -p "$scratch/positive" "$scratch/negative"
    init_fixture_repo "$scratch/positive"
    init_fixture_repo "$scratch/negative"

    overall=0
    selftest_positive_control "$scratch" || overall=1
    selftest_negative_control "$scratch" || overall=1
    selftest_empty_scan_control "$scratch" || overall=1
    selftest_not_a_repo_control "$scratch" || overall=1

    if [ "$overall" -ne 0 ]; then
        echo "check_spdx.sh --selftest: FALHOU (ver acima)" >&2
        exit 1
    fi
    echo "check_spdx.sh --selftest: os quatro controles OK"
}

main() {
    if [ "${1:-}" = "--selftest" ]; then
        selftest_main
    else
        real_main "$@"
    fi
}

main "$@"
