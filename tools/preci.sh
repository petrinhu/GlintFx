#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/preci.sh - local mirror of the CI (GODS_LAWS.md L-23; TESTES.md
# T15.2). Decision of the leader (L-23): NOT wired to a git hook, so
# pushing a documentation-only change is never slowed down by it.
#
# Usage:
#   tools/preci.sh                   full pipeline: format, configure
#                                     (-DGLINTFX_WERROR=ON), build,
#                                     clang-tidy, cppcheck, NOLINT
#                                     justification, gitleaks, ctest,
#                                     sanitizer stage.
#   tools/preci.sh --fast            same, minus the sanitizer stage -
#                                     for a documentation-only push.
#   tools/preci.sh --lint-only       format + configure + build +
#                                     clang-tidy + cppcheck + NOLINT
#                                     justification only (what the CI
#                                     `lint` job runs).
#   tools/preci.sh --sanitizer-only  the sanitizer stage only (what the
#                                     CI `sanitizer` job runs).
#   tools/preci.sh --selftest        proves the format/clang-tidy/cppcheck
#                                     stages against tests/preci_fixtures/
#                                     instead of the real tree: positive
#                                     control (clean fixture passes),
#                                     negative control (dirty fixture is
#                                     reproved), empty-scan control
#                                     (an empty directory is refused, not
#                                     silently approved), and a control
#                                     for the ROOT_DIR resolution itself
#                                     (a simulated `cd` failure proves the
#                                     old combined `readonly ROOT_DIR=$(...)`
#                                     form masks the error while the
#                                     current split form exits 1 with a
#                                     diagnostic). Registered as ctest
#                                     case `preci_selftest` when the
#                                     three tools are present (see
#                                     tests/CMakeLists.txt).
#
# GODS_LAWS.md L-25: running this script without --lint-only/--selftest
# triggers a heavy build and (outside --fast) a sanitizer build - both
# are a watchcode window. Arm before, disarm after.

set -euo pipefail

# This machine's /tmp is tmpfs (RAM-backed); a heavy C++ link can run it
# out of space. Disk-backed /var/tmp is the default unless the caller
# already set TMPDIR.
export TMPDIR="${TMPDIR:-/var/tmp}"

# fail()/log() moved above the ROOT_DIR resolution (they used to sit
# below it, forcing an inline `exit 1` there instead of `|| fail ...` -
# see the achado this comment replaces). Neither depends on ROOT_DIR.
log() {
    printf '== %s ==\n' "$1"
}

fail() {
    echo "preci.sh: $1" >&2
    exit 1
}

# GODS_LAWS.md INBOX achado (shellcheck SC2155): declare-and-assign
# separately, not a suppression. "readonly ROOT_DIR=$(cmd)" combined
# masks cmd's exit code behind the `readonly` builtin's own exit code,
# which is 0 even when cmd failed (measured live: `cd` to a
# non-existent directory left the old combined line silently swallowing
# the failure under `set -e`, ROOT_DIR ending up empty, and the script
# only failing later and uglier, at `cmake -S ""`). Assignment and its
# exit status are checked separately from `readonly`, and this fails
# loud and clear on the spot.
#
# Factored into its own function (achado IMPORTANTE de revisao
# adversarial, 25/08/2026) so that --selftest's simulated cd-failure
# control (run_selftest_rootdir_cd_failure_control) can import THIS
# exact function body into an isolated subshell via `declare -f`,
# instead of keeping a hand-copied duplicate that could silently drift
# from the real resolution logic below.
resolve_root_dir() {
    cd "$(dirname "$1")/.." && pwd
}

ROOT_DIR="$(resolve_root_dir "$0")" || {
    echo "preci.sh: nao foi possivel resolver ROOT_DIR (cd para o diretorio do script falhou)" >&2
    exit 1
}
readonly ROOT_DIR
readonly BUILD_DIR="${ROOT_DIR}/build-preci"
readonly SANITIZE_BUILD_DIR="${ROOT_DIR}/build-preci-sanitize"
readonly FIXTURES_DIR="${ROOT_DIR}/tests/preci_fixtures"

# GODS_LAWS.md INBOX achado (rodada 2 da revisao do FUND-4, 24/08/2026):
# `ctest -L PATTERN` matches PATTERN as a *substring* of the label, not
# the whole label - a test labeled e.g. `nonunit` satisfies an
# unanchored `-L unit` (measured live: Total Tests: 1, should be 0).
# Not exploitable today (the only labels in this tree are unit, consume
# and selftest, none collide), but stage_sanitizer's own filter is
# exactly what the GODS_LAWS.md L-40 empty-scan floor exists to protect:
# if a future label ever contained "unit" as a substring, this
# unanchored filter would keep silently matching after the real `unit`
# label had vanished. Anchored once, here, and reused by both the real
# pipeline stage (stage_sanitizer below) and its selftest regression
# control (run_selftest_ctest_count_substring_control) so the two can
# never drift apart.
readonly CTEST_UNIT_LABEL_FILTER='^unit$'

# --- enumeration (GODS_LAWS.md L-23/L-24 spirit: a gate has to prove it
# looked at something). Each *_stage function below prints how many
# files it scanned; require_nonempty is the shared floor that refuses
# to call a stage "passed" when it scanned zero files ("varredura
# vazia") - a portal that scans nothing and prints green is the defect
# this project is explicitly building this gate to never ship again.

# Populates the global array FILES. Declared here (not `local` inside
# every stage) because bash cannot return an array from a function.
FILES=()

enumerate_tracked_cpp_hpp() {
    FILES=()
    while IFS= read -r f; do
        [ -n "$f" ] && FILES+=("$f")
    done < <(cd "$ROOT_DIR" && git ls-files -- '*.cpp' '*.hpp' ':!:tests/preci_fixtures/*')
}

enumerate_tracked_cpp() {
    FILES=()
    while IFS= read -r f; do
        [ -n "$f" ] && FILES+=("$f")
    done < <(cd "$ROOT_DIR" && git ls-files -- '*.cpp' ':!:tests/preci_fixtures/*')
}

# Plain directory walk (not git enumeration): used only by --selftest,
# which points this at tests/preci_fixtures/clean, .../dirty and a
# throwaway empty directory, none of which the real pipeline ever scans
# this way (enumerate_tracked_* above explicitly excludes the fixtures
# tree so the deliberately-broken dirty fixture never fails a real push).
enumerate_dir_cpp_hpp() {
    dir="$1"
    FILES=()
    while IFS= read -r f; do
        [ -n "$f" ] && FILES+=("$f")
    done < <(find "$dir" -type f \( -name '*.cpp' -o -name '*.hpp' \) 2>/dev/null | sort)
}

# Prints the file count and returns non-zero on empty - does NOT exit
# the process: --selftest's empty-directory control needs to observe
# this failure and keep running, so exiting here would be wrong for
# that caller. Real pipeline stages turn this into a hard stop
# themselves with `require_nonempty ... || fail ...`.
require_nonempty() {
    stage_name="$1"
    if [ "${#FILES[@]}" -eq 0 ]; then
        echo "$stage_name: varredura vazia (0 arquivos)" >&2
        return 1
    fi
    echo "$stage_name: ${#FILES[@]} arquivo(s) varrido(s)"
}

# Sibling floor to require_nonempty above, for the two stages whose
# unit of work is a TEST, not a file (stage_ctest, stage_sanitizer).
# `ctest` itself exits 0 and prints "No tests were found!!!" when a
# build has zero registered tests, or zero tests matching a `-L`
# filter - require_nonempty (file-count based) never covered this,
# and that gap is what reproved this fatia's first revision (adversarial
# review, FUND-4). `ctest -N` reports the count WITHOUT running
# anything, so counting is itself cheap.
count_ctest_tests() {
    build_dir="$1"
    shift
    ctest --test-dir "$build_dir" -N "$@" 2>/dev/null         | awk -F': ' '/^Total Tests:/ { print $2; found=1 } END { if (!found) print 0 }'
}

# Same contract as require_nonempty, but for a test count instead of a
# file count: prints and returns non-zero on empty, never exits by
# itself (selftest's negative controls below need to observe the
# failure and keep running).
require_nonempty_tests() {
    stage_name="$1"
    count="$2"
    if [ "$count" -eq 0 ]; then
        echo "$stage_name: varredura vazia (0 testes)" >&2
        return 1
    fi
    echo "$stage_name: $count teste(s) varrido(s)"
}

# --- real-pipeline stages (operate on the tracked tree) ---

stage_format() {
    enumerate_tracked_cpp_hpp
    require_nonempty "format" || fail "estagio format recusado (varredura vazia)"
    clang-format --dry-run -Werror "${FILES[@]}"
}

# -DGLINTFX_BUILD_TESTS=ON forced explicitly (defense in depth, not a
# replacement for the count floor in stage_ctest below): the option's
# own default is ${PROJECT_IS_TOP_LEVEL} (cmake/GlintfxOptions.cmake),
# which stays ON for every fresh configure of this script's OWN build
# directories - but a build directory left over from a manual
# `-DGLINTFX_BUILD_TESTS=OFF` reconfigure would otherwise silently keep
# that cached value on the next `cmake -S -B` here, and stage_ctest
# would then find zero tests. Forcing it removes that path entirely for
# THIS script's own build dirs; a developer who genuinely wants a
# tests-off build uses plain `cmake` directly, not preci.sh, whose
# entire purpose is running the test suite.
stage_configure() {
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_WERROR=ON -DGLINTFX_BUILD_TESTS=ON
}

stage_build() {
    cmake --build "$BUILD_DIR"
}

stage_tidy() {
    enumerate_tracked_cpp
    require_nonempty "clang-tidy" || fail "estagio clang-tidy recusado (varredura vazia)"
    run-clang-tidy -p "$BUILD_DIR" -quiet "${FILES[@]}"
}

# --enable is deliberately warning,performance,portability, NOT `all`:
# --enable=all also turns on `style` (opinion-grade findings like
# "consider std::find_if instead of this loop" - not a bug) and
# `unusedFunction` (which false-positives on any function only used
# from a translation unit outside whatever file list gets passed in a
# single invocation, e.g. tests/embed_name_collision/dummy.cpp's
# dummy_symbol - it IS used, from a sibling TU cppcheck never sees
# here). CONTRACT.md's own gate philosophy is "0 security/bugprone
# errors", not zero style opinions; measured both ways building this
# slice before choosing this flag.
stage_cppcheck() {
    enumerate_tracked_cpp_hpp
    require_nonempty "cppcheck" || fail "estagio cppcheck recusado (varredura vazia)"
    cppcheck --enable=warning,performance,portability --inline-suppr \
        --error-exitcode=1 --suppress=missingIncludeSystem --std=c++20 \
        -I "$ROOT_DIR/include" -I "$BUILD_DIR/generated/include" \
        "${FILES[@]}"
}

# .clang-tidy's own header comment states the policy: every
# NOLINT/NOLINTNEXTLINE carries a `reason:` justification on the same
# line. A suppression with no recorded reason is not auditable later,
# which is exactly the failure mode a NOLINT is supposed to be an
# EXCEPTION to, not a habit.
stage_nolint_justification() {
    enumerate_tracked_cpp_hpp
    require_nonempty "NOLINT-justification" || fail "estagio NOLINT recusado (varredura vazia)"
    offenders=""
    for f in "${FILES[@]}"; do
        while IFS= read -r line; do
            case "$line" in
                *reason:*) ;;
                *) offenders="${offenders}${f}:${line}"$'\n' ;;
            esac
        done < <(grep -n 'NOLINT' "$ROOT_DIR/$f" 2>/dev/null || true)
    done
    if [ -n "$offenders" ]; then
        printf '%s' "$offenders" >&2
        fail "NOLINT sem justificativa 'reason:' na mesma linha (ver .clang-tidy)"
    fi
    echo "NOLINT-justification: ok"
}

# GODS_LAWS.md L-23 portao 4. `gitleaks detect` (no flags) scans the
# FULL git log by default on this machine's gitleaks 8.30.0, not just
# the working tree - verified live against a throwaway repo before
# wiring this stage in (a secret added in one commit and removed in the
# next was still found). That is a stronger guarantee than L-23's own
# text describes ("por padrao, olha a arvore e nao o historico") - a
# discrepancy reported alongside this slice, not silently absorbed.
# .gitleaksignore carries the two known documentation false positives
# (CONTRACT.md 7.1's deliberately-fake example key).
stage_gitleaks() {
    gitleaks detect --no-banner --source "$ROOT_DIR"
}

stage_ctest() {
    count="$(count_ctest_tests "$BUILD_DIR")"
    require_nonempty_tests "ctest" "$count" || fail "estagio ctest recusado (varredura vazia de testes)"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
}

# Sanitizer stage runs `ctest -L "$CTEST_UNIT_LABEL_FILTER"` (anchored
# `^unit$`) only, never the full suite: the shell-script consumption
# gates (tests/CMakeLists.txt, LABELS consume) link a plain,
# non-sanitized consumer against the sanitized library on purpose, which
# fails by construction under ASan (its runtime has to come first in the
# process) - see the comment on GLINTFX_SANITIZE in
# cmake/GlintfxOptions.cmake. Own build directory, own configure: this
# is a heavier, slower build than the normal one (GODS_LAWS.md L-23
# portao 2), so it never shares a build tree with stage_configure above.
stage_sanitizer() {
    cmake -S "$ROOT_DIR" -B "$SANITIZE_BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_WERROR=ON -DGLINTFX_BUILD_TESTS=ON \
        -DGLINTFX_SANITIZE=address,undefined
    cmake --build "$SANITIZE_BUILD_DIR"
    count="$(count_ctest_tests "$SANITIZE_BUILD_DIR" -L "$CTEST_UNIT_LABEL_FILTER")"
    require_nonempty_tests "ctest -L ${CTEST_UNIT_LABEL_FILTER} (sanitizer)" "$count" \
        || fail "estagio sanitizer recusado (varredura vazia de testes rotulados unit - o rotulo sumiu ou o build nao tem teste nenhum)"
    ctest --test-dir "$SANITIZE_BUILD_DIR" -L "$CTEST_UNIT_LABEL_FILTER" --output-on-failure
}

# --- selftest stages (operate on tests/preci_fixtures/<dir> or an
# ad hoc empty directory; never touch $BUILD_DIR) ---

selftest_stage_format() {
    enumerate_dir_cpp_hpp "$1"
    require_nonempty "format[$1]" || return 1
    clang-format --dry-run -Werror "${FILES[@]}"
}

# Standalone `-- -std=c++23`, no -p/build directory: the fixtures are
# self-contained (stdlib only, see their own header comments) precisely
# so the selftest never depends on the project's own build tree.
selftest_stage_tidy() {
    enumerate_dir_cpp_hpp "$1"
    require_nonempty "clang-tidy[$1]" || return 1
    status=0
    for f in "${FILES[@]}"; do
        clang-tidy "$f" -- -std=c++23 || status=1
    done
    return "$status"
}

selftest_stage_cppcheck() {
    enumerate_dir_cpp_hpp "$1"
    require_nonempty "cppcheck[$1]" || return 1
    cppcheck --enable=warning,performance,portability --inline-suppr \
        --error-exitcode=1 --suppress=missingIncludeSystem --std=c++20 \
        "${FILES[@]}"
}

run_selftest_positive_control() {
    log "selftest: controle positivo (fixture limpa)"
    selftest_stage_format "$FIXTURES_DIR/clean" \
        || fail "controle positivo FALHOU no estagio format (fixture limpa deveria passar)"
    selftest_stage_tidy "$FIXTURES_DIR/clean" \
        || fail "controle positivo FALHOU no estagio clang-tidy (fixture limpa deveria passar)"
    selftest_stage_cppcheck "$FIXTURES_DIR/clean" \
        || fail "controle positivo FALHOU no estagio cppcheck (fixture limpa deveria passar)"
    echo "selftest: controle positivo OK"
}

run_selftest_negative_control() {
    log "selftest: controle negativo (fixture suja)"
    if selftest_stage_format "$FIXTURES_DIR/dirty" >/dev/null 2>&1; then
        fail "controle negativo FALHOU: clang-format aprovou a fixture suja"
    fi
    if selftest_stage_tidy "$FIXTURES_DIR/dirty" >/dev/null 2>&1; then
        fail "controle negativo FALHOU: clang-tidy aprovou a fixture suja"
    fi
    if selftest_stage_cppcheck "$FIXTURES_DIR/dirty" >/dev/null 2>&1; then
        fail "controle negativo FALHOU: cppcheck aprovou a fixture suja"
    fi
    echo "selftest: controle negativo OK (fixture suja reprovada nos 3 estagios)"
}

run_selftest_empty_scan_control() {
    log "selftest: controle de varredura vazia"
    empty_dir="$(mktemp -d "${TMPDIR}/glintfx-preci-empty.XXXXXX")"
    if selftest_stage_format "$empty_dir" >/dev/null 2>&1; then
        rm -rf "$empty_dir"
        fail "controle de varredura vazia FALHOU: o estagio passou com 0 arquivos"
    fi
    rm -rf "$empty_dir"
    echo "selftest: controle de varredura vazia OK (0 arquivos foi recusado, nao aprovado)"
}

# --- selftest control for the ROOT_DIR resolution mechanism itself
# (achado IMPORTANTE de revisao adversarial, 25/08/2026): o conserto
# PRECI-ROOTDIR-SC2155 (linhas 45-60 deste arquivo) foi reproduzido ao
# vivo pelo revisor - reverter o bloco no arquivo rastreado e rodar
# `preci.sh --selftest` e `ctest -R preci_selftest` continuavam os dois
# verdes, porque nada na suite simulava a falha do `cd`. Este controle
# fecha essa lacuna.
#
# ROOT_DIR resolve nas primeiras linhas do script, ANTES da maioria das
# funcoes existir - por isso as duas formas sao exercitadas cada uma
# num subprocesso bash isolado, contra um $0 cujo
# `cd "$(dirname "$0")/.."` falha de proposito (ENOENT: o diretorio-pai
# nem existe). Nenhuma das duas toca o ROOT_DIR real desta execucao.
#
# A forma NOVA nao e uma copia literal: `declare -f resolve_root_dir`
# exporta para o subshell a MESMA funcao de producao (ver topo deste
# arquivo), entao uma regressao na funcao real e pega por este
# controle sem precisar manter duas copias em sincronia.

# Caminho cujo dirname()/.. nao existe, garantindo que o `cd` falhe por
# ENOENT e nao por permissao. $$ evita colisao entre execucoes
# concorrentes.
rootdir_cd_failure_target() {
    printf '%s' "/glintfx-rootdir-selftest-nao-existe-$$/sub/script.sh"
}

# A forma ANTIGA, tal como existia neste arquivo antes do conserto
# PRECI-ROOTDIR-SC2155: `readonly ROOT_DIR=$(...)` combinado mascara o
# exit code do `cd` atras do exit code do proprio builtin `readonly`
# (sempre 0 quando a atribuicao e valida, mesmo com o lado direito
# vazio). Mantida como literal de proposito: e a forma DESCARTADA, que
# este arquivo nao possui mais em lugar nenhum para reusar por funcao.
selftest_rootdir_old_form() {
    fake0="$1"
    bash -c '
        set -eu
        readonly ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
        printf "ROOT_DIR=[%s]\n" "$ROOT_DIR"
    ' "$fake0"
}

# A forma NOVA: reusa a funcao resolve_root_dir() REAL deste arquivo
# via `declare -f`, e reproduz em volta dela apenas a atribuicao e o
# `|| { ...; exit 1; }` que tambem existem, literalmente, no bloco do
# topo (linhas logo apos a definicao de resolve_root_dir).
selftest_rootdir_new_form() {
    fake0="$1"
    bash -c "
        $(declare -f resolve_root_dir)
        set -eu
        ROOT_DIR=\"\$(resolve_root_dir \"\$0\")\" || {
            echo 'preci.sh: nao foi possivel resolver ROOT_DIR (cd para o diretorio do script falhou)' >&2
            exit 1
        }
        readonly ROOT_DIR
        printf 'ROOT_DIR=[%s]\n' \"\$ROOT_DIR\"
    " "$fake0"
}

run_selftest_rootdir_cd_failure_control() {
    log "selftest: mascaramento de falha do cd na resolucao de ROOT_DIR"
    fake0="$(rootdir_cd_failure_target)"

    old_rc=0
    old_out="$(selftest_rootdir_old_form "$fake0" 2>&1)" || old_rc=$?
    [ "$old_rc" -eq 0 ] \
        || fail "controle do ROOT_DIR FALHOU: a forma ANTIGA deveria sair 0 mascarando a falha do cd (documentando o defeito ja conhecido), saiu $old_rc: $old_out"
    printf '%s\n' "$old_out" | grep -qF 'ROOT_DIR=[]' \
        || fail "controle do ROOT_DIR FALHOU: a forma ANTIGA deveria imprimir ROOT_DIR vazio, saida foi: $old_out"

    new_rc=0
    new_out="$(selftest_rootdir_new_form "$fake0" 2>&1)" || new_rc=$?
    [ "$new_rc" -eq 1 ] \
        || fail "controle do ROOT_DIR FALHOU: a forma NOVA (atual neste arquivo) deveria sair 1 quando o cd falha, saiu $new_rc: $new_out"
    printf '%s\n' "$new_out" | grep -qF 'nao foi possivel resolver ROOT_DIR' \
        || fail "controle do ROOT_DIR FALHOU: a forma NOVA saiu $new_rc mas sem o diagnostico esperado, saida foi: $new_out"

    echo "selftest: controle de mascaramento do cd (ROOT_DIR) OK (forma antiga sai 0 e mascara com ROOT_DIR vazio; forma atual sai 1 com diagnostico)"
}

# --- selftest controls for the two ctest-based stages (stage_ctest,
# stage_sanitizer): GODS_LAWS.md L-20, the finding that reproved this
# fatia's first revision (adversarial review, FUND-4). `ctest` exits 0
# and prints "No tests were found!!!" on a build with zero registered
# tests, or zero tests matching a `-L` filter - the file-count floor
# above never covered this, because the unit of work here is a TEST,
# not a file. These controls run against a throwaway CMake project
# (language NONE, tests that just run `${CMAKE_COMMAND} -E true`), not
# the real $BUILD_DIR, so --selftest stays cheap: no compiler is
# invoked, only `cmake -S -B` (configure only) and `ctest -N` (counts
# without running anything).

selftest_ctest_project_dir() {
    mktemp -d "${TMPDIR}/glintfx-preci-ctest-selftest.XXXXXX"
}

# $2 (mode): with-unit-label registers one test carrying LABELS unit
# (mirrors glintfx_add_test in cmake/GlintfxTest.cmake); no-tests
# registers none (mirrors GLINTFX_BUILD_TESTS=OFF, or an empty suite);
# wrong-label registers one test WITHOUT the `unit` label (mirrors the
# exact defect the review found: `set_tests_properties(... LABELS
# unit)` disappearing from GlintfxTest.cmake in a future refactor -
# stage_sanitizer's `ctest -L "$CTEST_UNIT_LABEL_FILTER"` then matches
# zero tests even though the build has one); substring-label registers
# one test labeled `nonunit` (mirrors the OTHER defect the INBOX achado
# found, rodada 2 do FUND-4: an unanchored `-L unit` matches `nonunit`
# by substring - see run_selftest_ctest_count_substring_control below).
selftest_write_ctest_project() {
    dir="$1"
    mode="$2"
    {
        echo 'cmake_minimum_required(VERSION 3.28)'
        echo 'project(preci_selftest_ctest_probe NONE)'
        echo 'include(CTest)'
        case "$mode" in
            with-unit-label)
                echo 'add_test(NAME dummy COMMAND ${CMAKE_COMMAND} -E true)'
                echo 'set_tests_properties(dummy PROPERTIES LABELS unit)'
                ;;
            wrong-label)
                echo 'add_test(NAME dummy COMMAND ${CMAKE_COMMAND} -E true)'
                echo 'set_tests_properties(dummy PROPERTIES LABELS consume)'
                ;;
            substring-label)
                echo 'add_test(NAME dummy COMMAND ${CMAKE_COMMAND} -E true)'
                echo 'set_tests_properties(dummy PROPERTIES LABELS nonunit)'
                ;;
            no-tests) ;;
        esac
    } > "$dir/CMakeLists.txt"
    cmake -S "$dir" -B "$dir/build" -G Ninja >/dev/null 2>&1
}

run_selftest_ctest_count_positive_control() {
    dir="$(selftest_ctest_project_dir)"
    selftest_write_ctest_project "$dir" with-unit-label
    n_all="$(count_ctest_tests "$dir/build")"
    n_unit="$(count_ctest_tests "$dir/build" -L unit)"
    rm -rf "$dir"
    [ "$n_all" -eq 1 ] || fail "selftest: esperava 1 teste sem filtro, count_ctest_tests devolveu '$n_all'"
    [ "$n_unit" -eq 1 ] || fail "selftest: esperava 1 teste com -L unit, count_ctest_tests devolveu '$n_unit'"
    require_nonempty_tests "ctest[positivo]" "$n_all"         || fail "selftest: controle positivo (ctest) FALHOU - piso recusou 1 teste real"
    require_nonempty_tests "ctest[positivo -L unit]" "$n_unit"         || fail "selftest: controle positivo (ctest -L unit) FALHOU - piso recusou 1 teste rotulado unit"
    echo "selftest: controle positivo (ctest, com e sem -L unit) OK"
}

run_selftest_ctest_count_negative_control() {
    dir="$(selftest_ctest_project_dir)"
    selftest_write_ctest_project "$dir" no-tests
    n="$(count_ctest_tests "$dir/build")"
    rm -rf "$dir"
    [ "$n" -eq 0 ] || fail "selftest: esperava 0 testes em projeto sem nenhum add_test, count_ctest_tests devolveu '$n'"
    if require_nonempty_tests "ctest[negativo: zero testes]" "$n" 2>/dev/null; then
        fail "selftest: controle negativo (ctest) FALHOU - piso aprovou 0 testes"
    fi
    echo "selftest: controle negativo (ctest, projeto sem nenhum teste registrado) OK"
}

run_selftest_ctest_count_wrong_label_control() {
    dir="$(selftest_ctest_project_dir)"
    selftest_write_ctest_project "$dir" wrong-label
    n_all="$(count_ctest_tests "$dir/build")"
    n_unit="$(count_ctest_tests "$dir/build" -L unit)"
    rm -rf "$dir"
    [ "$n_all" -eq 1 ] || fail "selftest: esperava 1 teste sem filtro (rotulo errado), count_ctest_tests devolveu '$n_all'"
    [ "$n_unit" -eq 0 ] || fail "selftest: esperava 0 testes com -L unit (rotulo errado), count_ctest_tests devolveu '$n_unit'"
    if require_nonempty_tests "ctest[-L unit, rotulo ausente]" "$n_unit" 2>/dev/null; then
        fail "selftest: controle de rotulo ausente FALHOU - piso aprovou -L unit com 0 testes casando"
    fi
    echo "selftest: controle de rotulo ausente (ctest -L unit, 1 teste existe mas nenhum rotulado unit) OK"
}

# GODS_LAWS.md INBOX achado (rodada 2 da revisao do FUND-4, 24/08/2026):
# proves the substring-match danger CTEST_UNIT_LABEL_FILTER exists to
# close. A project whose only test carries LABELS nonunit - contains
# the word "unit" but is not the label `unit` - is the exact shape the
# achado measured live (unanchored `-L unit` returned Total Tests: 1
# for it, should have been 0). n_unanchored below documents that the
# danger is real; n_anchored uses the SAME constant stage_sanitizer
# uses, so a future edit that widens CTEST_UNIT_LABEL_FILTER back to a
# bare `unit` fails this control instead of shipping silently.
run_selftest_ctest_count_substring_control() {
    dir="$(selftest_ctest_project_dir)"
    selftest_write_ctest_project "$dir" substring-label
    n_unanchored="$(count_ctest_tests "$dir/build" -L unit)"
    n_anchored="$(count_ctest_tests "$dir/build" -L "$CTEST_UNIT_LABEL_FILTER")"
    rm -rf "$dir"
    [ "$n_unanchored" -eq 1 ] \
        || fail "selftest: esperava que '-L unit' (sem ancora) casasse por substring o rotulo 'nonunit' (o bug que o achado mediu ao vivo), count_ctest_tests devolveu '$n_unanchored'"
    [ "$n_anchored" -eq 0 ] \
        || fail "selftest: controle de substring FALHOU - '-L ${CTEST_UNIT_LABEL_FILTER}' casou o rotulo 'nonunit', deveria ser 0 (a ancora nao esta protegendo), count_ctest_tests devolveu '$n_anchored'"
    if require_nonempty_tests "ctest[-L ${CTEST_UNIT_LABEL_FILTER}, rotulo nonunit]" "$n_anchored" 2>/dev/null; then
        fail "selftest: controle de substring FALHOU - o piso aprovou 0 testes casando com o filtro ancorado"
    fi
    echo "selftest: controle de substring de rotulo (nonunit vs -L unit / -L ${CTEST_UNIT_LABEL_FILTER}) OK"
}

run_selftest_ctest_count_controls() {
    log "selftest: piso de contagem de testes (stage_ctest / stage_sanitizer)"
    run_selftest_ctest_count_positive_control
    run_selftest_ctest_count_negative_control
    run_selftest_ctest_count_wrong_label_control
    run_selftest_ctest_count_substring_control
}

run_selftest() {
    run_selftest_positive_control
    run_selftest_negative_control
    run_selftest_empty_scan_control
    run_selftest_rootdir_cd_failure_control
    run_selftest_ctest_count_controls
    echo "preci.sh --selftest: TODOS OS CONTROLES PASSARAM"
}

# --- pipelines ---

run_lint_only() {
    log "estagio 1: clang-format"
    stage_format
    log "estagio 2: configure (-Werror)"
    stage_configure
    log "estagio 3: build"
    stage_build
    log "estagio 4: clang-tidy"
    stage_tidy
    log "estagio 5: cppcheck"
    stage_cppcheck
    log "estagio 5b: justificativa de NOLINT"
    stage_nolint_justification
    echo "preci.sh --lint-only: VERDE"
}

run_sanitizer_only() {
    log "estagio 7: sanitizer (ASan/UBSan)"
    stage_sanitizer
    echo "preci.sh --sanitizer-only: VERDE"
}

run_full_pipeline() {
    fast="$1"
    log "estagio 1: clang-format"
    stage_format
    log "estagio 2: configure (-Werror)"
    stage_configure
    log "estagio 3: build"
    stage_build
    log "estagio 4: clang-tidy"
    stage_tidy
    log "estagio 5: cppcheck"
    stage_cppcheck
    log "estagio 5b: justificativa de NOLINT"
    stage_nolint_justification
    log "estagio 5c: gitleaks"
    stage_gitleaks
    log "estagio 6: ctest completo"
    stage_ctest
    if [ "$fast" = "yes" ]; then
        echo "preci.sh --fast: estagio 7 (sanitizer) PULADO"
    else
        log "estagio 7: sanitizer (ASan/UBSan)"
        stage_sanitizer
    fi
    echo "preci.sh: TUDO VERDE"
}

main() {
    case "${1:-}" in
        "")
            run_full_pipeline "no"
            ;;
        --fast)
            run_full_pipeline "yes"
            ;;
        --lint-only)
            run_lint_only
            ;;
        --sanitizer-only)
            run_sanitizer_only
            ;;
        --selftest)
            run_selftest
            ;;
        *)
            fail "uso: preci.sh [--fast|--lint-only|--sanitizer-only|--selftest]"
            ;;
    esac
}

main "$@"
