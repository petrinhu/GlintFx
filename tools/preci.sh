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
#                                     reproved) and empty-scan control
#                                     (an empty directory is refused, not
#                                     silently approved). Registered as
#                                     ctest case `preci_selftest` when the
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

readonly ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
readonly BUILD_DIR="${ROOT_DIR}/build-preci"
readonly SANITIZE_BUILD_DIR="${ROOT_DIR}/build-preci-sanitize"
readonly FIXTURES_DIR="${ROOT_DIR}/tests/preci_fixtures"

log() {
    printf '== %s ==\n' "$1"
}

fail() {
    echo "preci.sh: $1" >&2
    exit 1
}

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

# --- real-pipeline stages (operate on the tracked tree) ---

stage_format() {
    enumerate_tracked_cpp_hpp
    require_nonempty "format" || fail "estagio format recusado (varredura vazia)"
    clang-format --dry-run -Werror "${FILES[@]}"
}

stage_configure() {
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_WERROR=ON
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
    ctest --test-dir "$BUILD_DIR" --output-on-failure
}

# Sanitizer stage runs `ctest -L unit` only, never the full suite: the
# shell-script consumption gates (tests/CMakeLists.txt, LABELS consume)
# link a plain, non-sanitized consumer against the sanitized library on
# purpose, which fails by construction under ASan (its runtime has to
# come first in the process) - see the comment on GLINTFX_SANITIZE in
# cmake/GlintfxOptions.cmake. Own build directory, own configure: this
# is a heavier, slower build than the normal one (GODS_LAWS.md L-23
# portao 2), so it never shares a build tree with stage_configure above.
stage_sanitizer() {
    cmake -S "$ROOT_DIR" -B "$SANITIZE_BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DGLINTFX_WERROR=ON \
        -DGLINTFX_SANITIZE=address,undefined
    cmake --build "$SANITIZE_BUILD_DIR"
    ctest --test-dir "$SANITIZE_BUILD_DIR" -L unit --output-on-failure
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

run_selftest() {
    run_selftest_positive_control
    run_selftest_negative_control
    run_selftest_empty_scan_control
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
