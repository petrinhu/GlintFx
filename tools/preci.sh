#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/preci.sh - local mirror of the CI (GODS_LAWS.md L-23; TESTES.md
# T15.2). Decision of the leader (L-23): NOT wired to a git hook, so
# pushing a documentation-only change is never slowed down by it.
#
# Usage:
#   tools/preci.sh                   guard (no new untracked *.cpp/*.hpp),
#                                     then full pipeline: format, configure
#                                     (-DGLINTFX_WERROR=ON), build,
#                                     clang-tidy, cppcheck, NOLINT
#                                     justification, gitleaks, ctest,
#                                     sanitizer stage, debug stage.
#   tools/preci.sh --fast            same, minus the sanitizer stage and
#                                     the debug stage - for a
#                                     documentation-only push.
#   tools/preci.sh --lint-only       guard, then format + configure +
#                                     build + clang-tidy + cppcheck +
#                                     NOLINT justification only (what
#                                     the CI `lint` job runs).
#   tools/preci.sh --sanitizer-only  guard, then the sanitizer stage
#                                     only (what the CI `sanitizer` job
#                                     runs).
#   tools/preci.sh --debug-only      guard, then the debug stage only
#                                     (what the CI `debug` job runs):
#                                     counts every real `assert()` in
#                                     the tracked product source tree
#                                     (reproves on zero, GODS_LAWS.md
#                                     L-40 - GATE-DEBUG), configures a
#                                     SEPARATE build directory with
#                                     -DCMAKE_BUILD_TYPE=Debug (CMake's
#                                     own default Debug flags do NOT
#                                     define NDEBUG, unlike this
#                                     project's normal
#                                     -DCMAKE_BUILD_TYPE=Release - see
#                                     stage_debug's own comment), builds
#                                     and runs the FULL ctest suite
#                                     against it. Before this stage
#                                     existed, no gate in this project
#                                     ever compiled a build where
#                                     NDEBUG is undefined, so no
#                                     product assert() had ever been
#                                     exercised as compiled code -
#                                     GODS_LAWS.md TODO.md item
#                                     GATE-DEBUG, decision D4 of
#                                     DECISOES_AUTONOMAS.md (a real
#                                     debug stage, not an alternate
#                                     mechanism that re-enacts the
#                                     preconditions another way).
#   tools/preci.sh --selftest        proves the format/clang-tidy/cppcheck
#                                     stages against tests/preci_fixtures/
#                                     instead of the real tree: positive
#                                     control (clean fixture passes),
#                                     negative control (dirty fixture is
#                                     reproved), empty-scan control
#                                     (an empty directory is refused, not
#                                     silently approved), a control for
#                                     the ROOT_DIR resolution itself
#                                     (a simulated `cd` failure proves the
#                                     old combined `readonly ROOT_DIR=$(...)`
#                                     form masks the error while the
#                                     current split form exits 1 with a
#                                     diagnostic), and a control for the
#                                     untracked-source guard itself
#                                     (positive: clean repo passes;
#                                     negative: a loose *.cpp is refused;
#                                     ignored: a .gitignore'd *.cpp under
#                                     build/ never blocks; git-failure: a
#                                     non-repo directory aborts loud,
#                                     never "found nothing, so pass" -
#                                     GODS_LAWS.md L-40). --selftest never
#                                     runs the guard against the REAL
#                                     tree - see stage_untracked_guard's
#                                     own comment for why. Registered as
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
readonly DEBUG_BUILD_DIR="${ROOT_DIR}/build-preci-debug"
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

# GATE-QUOTEPATH (01/09/2026), applies to every `git ls-files` call in
# this file: git's own default (core.quotepath=true) prints any tracked
# path with a byte >= 0x80 as a C-style octal-escaped, double-quoted
# string (e.g. "Wayl\303\244nd.cpp" instead of Waylând.cpp). The pathspec
# match itself ('*.cpp'/'*.hpp') still finds the file - the extension is
# plain ASCII - but the LISTED name is the garbled quoted form, which is
# not a real path on disk, so it never reaches clang-format/clang-tidy/
# cppcheck usably. `-c core.quotepath=false` makes git print the raw
# UTF-8 bytes instead, matching the real path on disk.
enumerate_tracked_cpp_hpp() {
    FILES=()
    while IFS= read -r f; do
        [ -n "$f" ] && FILES+=("$f")
    done < <(cd "$ROOT_DIR" && git -c core.quotepath=false ls-files -- '*.cpp' '*.hpp' ':!:tests/preci_fixtures/*')
}

enumerate_tracked_cpp() {
    FILES=()
    while IFS= read -r f; do
        [ -n "$f" ] && FILES+=("$f")
    done < <(cd "$ROOT_DIR" && git -c core.quotepath=false ls-files -- '*.cpp' ':!:tests/preci_fixtures/*')
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

# Lists NEW *.cpp/*.hpp files not yet known to git (git ls-files
# --others), scoped to the same two extensions the real-pipeline lint
# stages enumerate (enumerate_tracked_cpp_hpp/enumerate_tracked_cpp
# above). GODS_LAWS.md L-40's own measured defect, first row of its
# table: those stages walk ONLY what `git ls-files` already knows, so
# a brand-new .cpp/.hpp dropped in the tree and never `git add`ed is
# invisible to clang-format/clang-tidy/cppcheck/NOLINT-justification -
# the gate prints green having never looked at it (discovered live by
# WL-PROTO's implementer: same command, different result before and
# after `git add`).
#
# `--exclude-standard` makes this respect .gitignore exactly like
# `git status` does, so build output, scratch/, tmp/ and anything else
# the tree already excludes on purpose never blocks a push (see
# run_selftest_untracked_guard_ignored_control below) - a guard that
# refuses to run over a leftover build/ directory gets disabled within
# a week, which protects nobody.
#
# $1 is the directory to scan, an explicit parameter rather than the
# global ROOT_DIR: stage_untracked_guard (the real-pipeline caller)
# passes "$ROOT_DIR", but --selftest's controls below point this at a
# disposable throwaway repo instead - the REAL tree can legitimately
# have another agent's WIP untracked *.cpp mid-onda right now (not
# hypothetical: it does, as of this slice), and gating --selftest on
# that would make --selftest's result depend on who else is working,
# which is itself a variant of the L-40 defect (a portal whose
# verdict depends on something it never declares).
#
# A real failure of the underlying git command (not a git repository,
# git missing, permission denied) is NEVER treated as "found nothing,
# so pass" - that collapse is exactly what L-40 exists to forbid. It
# fails loud immediately, in every caller, real pipeline or selftest
# control alike (see run_selftest_untracked_guard_git_failure_control).
enumerate_untracked_cpp_hpp() {
    dir="$1"
    FILES=()
    listing="$(cd "$dir" && git -c core.quotepath=false ls-files --others --exclude-standard -- '*.cpp' '*.hpp' ':!:tests/preci_fixtures/*')" \
        || fail "untracked-guard: 'git ls-files --others' falhou em '$dir' (nao e um repositorio git, ou git indisponivel) - varredura recusada, nunca presumida vazia (GODS_LAWS.md L-40)"
    while IFS= read -r f; do
        [ -n "$f" ] && FILES+=("$f")
    done <<< "$listing"
    # A here-string always feeds at least one byte (a trailing newline),
    # even when $listing is "" - unlike the process-substitution form
    # enumerate_tracked_cpp_hpp/enumerate_tracked_cpp use above. That
    # means the loop body DOES run once on a legitimately empty scan,
    # its last command (`[ -n "$f" ]`, false) becomes the loop's own
    # exit status, and - because this was the function's own last
    # statement - `set -e` would kill the whole script the instant a
    # scan came back clean (measured live: --selftest died silently,
    # no diagnostic, exactly when FILES was correctly empty). `return 0`
    # decouples the function's exit status from the loop's, which is
    # the only thing this function promises: FILES is populated,
    # nothing about whether it ended up empty.
    return 0
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

# Sibling floor to require_nonempty/require_nonempty_tests, but
# INVERTED: for this one gate an EMPTY scan is the GOOD outcome
# (nothing untracked to block on) and a NON-empty scan is the failure.
# Prints the count and the offending paths, and returns non-zero on
# failure - never exits by itself (--selftest's negative control needs
# to observe the failure and keep running, same contract as its two
# siblings above).
require_no_untracked_source() {
    if [ "${#FILES[@]}" -gt 0 ]; then
        echo "untracked-guard: ${#FILES[@]} arquivo(s) *.cpp/*.hpp novo(s), fora do controle de versao:" >&2
        printf '  %s\n' "${FILES[@]}" >&2
        return 1
    fi
    echo "untracked-guard: 0 arquivo(s) *.cpp/*.hpp novo(s) fora do controle de versao"
}

# --- real-pipeline stages (operate on the tracked tree) ---

# GODS_LAWS.md L-40 / TODO.md PRECI-UNTRACKED: runs before every real
# stage below (wired in main()), never inside --selftest (which never
# touches $ROOT_DIR - see enumerate_untracked_cpp_hpp's own comment).
stage_untracked_guard() {
    enumerate_untracked_cpp_hpp "$ROOT_DIR"
    require_no_untracked_source \
        || fail "estagio untracked-guard recusado (arquivo *.cpp/*.hpp novo fora do 'git ls-files' acima - rode 'git add' antes de preci.sh; GODS_LAWS.md L-40, TODO.md PRECI-UNTRACKED)"
}

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

# --- GATE-DEBUG (GODS_LAWS.md TODO.md item, DECISOES_AUTONOMAS.md D4,
# L-40): -DCMAKE_BUILD_TYPE=Release (every OTHER stage above) defines
# NDEBUG, which compiles every assert() in the product source tree to
# nothing - zero cost, but also zero exercise. Before this stage
# existed, no gate in this project ever built a tree where NDEBUG is
# undefined, so if any product assert()'s condition were inverted,
# nothing here would notice. CMake's own built-in Debug flags
# (CMAKE_CXX_FLAGS_DEBUG, unset by this project - see
# cmake/GlintfxCompileOptions.cmake, which never touches
# CMAKE_CXX_FLAGS_DEBUG/_RELEASE) do NOT add -DNDEBUG; only Release,
# RelWithDebInfo and MinSizeRel do. -DCMAKE_BUILD_TYPE=Debug is
# therefore sufficient on its own, with no extra flag, to turn every
# assert() in the tree back into a real check - this is the standard
# CMake convention every C++ toolchain this project targets already
# implements, not a project invention (same shape of fact as the
# comment on include/glintfx/core/err.hpp's own debug-only precondition
# guard, which this stage now finally exercises for the FIRST time
# against the library's own compiled translation units, not just the
# header-only fixture check_rslt_precondition.sh already proved).

# count_real_asserts_in_file: counts assert() CALL lines in a single
# file - not every line containing the eight-byte substring "assert(".
# Two things that string appears in and must NOT count: a comment line
# that only DISCUSSES the mechanism (this project has several, e.g.
# src/gfss/token_progress_guard.hpp's own header, include/glintfx/core/
# err.hpp:84/99) and a static_assert(...) (compile-time, not the
# runtime NDEBUG-gated macro this gate is about). A comment line is
# recognized by grep -vE '^[[:space:]]*//' (only a comment that starts
# the line, after leading whitespace, is excluded - the same
# NOLINT-justification convention this file already uses elsewhere
# treats "starts the line" as the honest definition of "this line is a
# comment", not "the line contains // anywhere"). static_assert is
# excluded by the boundary class '(^|[^_[:alnum:]])' immediately before
# "assert(": the character right before "assert(" in "static_assert("
# is '_', which the class explicitly forbids, so the pattern never
# matches static_assert's occurrence in the first place. Deliberately
# `wc -l` on the filtered line count, NEVER `grep -c` (GODS_LAWS.md
# GATE-DEBUG's own service order: "grep -c zero sai com status 1" -
# `wc -l` exits 0 whether it counted 0 or N lines, `grep -c` exits 1 on
# 0, which under `set -e`/pipefail would abort this script the instant
# a file legitimately had zero matches - the common case, most files in
# this tree have no assert() at all). The whole capture is still
# `|| true`-guarded on top of that, because the FIRST grep in the pipe
# (the one that finds "assert(" at all) still exits 1 on zero matches,
# and pipefail propagates that through the pipeline even though the
# trailing `wc -l` itself succeeds.
count_real_asserts_in_file() {
    file="$1"
    # shellcheck disable=SC2126 # grep|wc -l is intentional, not an
    # oversight: `grep -c` exits 1 on a zero count (see comment above),
    # `wc -l` always exits 0 - the whole point of this shape is to
    # never trip `set -e`/pipefail on the ordinary case of a file with
    # no assert() at all.
    count="$(grep -E '(^|[^_[:alnum:]])assert\(' "$file" 2>/dev/null \
        | grep -vE '^[[:space:]]*//' 2>/dev/null \
        | wc -l)" || true
    echo "${count:-0}"
}

# Product scope only (GODS_LAWS.md L-40's own "enumeracao fechada":
# src/**/*.cpp and include/**/*.hpp are the two trees where product
# code lives - CONTRACT.md/L-19): tests/ is excluded (harness and test
# code are not the product under precondition), and third_party/ is
# excluded (the Khronos gl.xml-generated header, L-07 EXCECAO No 1, is
# machine-generated data, never hand-written product code with a
# precondition of ours to guard). Same '*.cpp' '*.hpp' glob style
# enumerate_tracked_cpp_hpp already uses above - git's pathspec glob
# matches across directories with a bare '*.cpp', no '**' needed.
enumerate_product_source_files() {
    FILES=()
    while IFS= read -r f; do
        [ -n "$f" ] && FILES+=("$f")
    done < <(cd "$ROOT_DIR" && git -c core.quotepath=false ls-files -- '*.cpp' '*.hpp' ':!:tests/*' ':!:third_party/*')
}

# Sums count_real_asserts_in_file across every product source file.
# This IS the "varredura" GODS_LAWS.md L-40 requires this gate to
# prove it performed - not the ctest test count below (a build with
# zero registered tests and a build with zero assert()s are two
# DIFFERENT empty-scan defects; L-40's own six measured cases are all
# distinct shapes of the same defect, never covered by a single floor).
count_product_asserts() {
    enumerate_product_source_files
    total=0
    for f in "${FILES[@]}"; do
        n="$(count_real_asserts_in_file "$ROOT_DIR/$f")"
        total=$((total + n))
    done
    echo "$total"
}

# Sibling floor to require_nonempty/require_nonempty_tests, same
# contract: prints and returns non-zero on empty, never exits by
# itself (--selftest's negative control needs to observe the failure
# and keep running).
require_nonempty_asserts() {
    stage_name="$1"
    count="$2"
    if [ "$count" -eq 0 ]; then
        echo "$stage_name: varredura vazia (0 assert() de produto encontrado)" >&2
        return 1
    fi
    echo "$stage_name: $count assert() de produto encontrado(s)"
}

# Own build directory, own configure (same reasoning as stage_sanitizer
# above): a Debug build is slower than the normal Release one
# (optimizations off), so it never shares a build tree with
# stage_configure. Runs the FULL suite (unlike stage_sanitizer's
# declared `-L unit` downgrade) - GATE-DEBUG's own decision (D4): there
# is no ASan-shaped reason to narrow it here, a plain Debug build has
# no runtime-interposition-must-come-first constraint, so the
# consumption gates (LABELS consume) run here exactly like every other
# non-sanitized build in this project.
stage_debug() {
    count="$(count_product_asserts)"
    require_nonempty_asserts "debug" "$count" \
        || fail "estagio debug recusado (varredura vazia de assert() de produto - GODS_LAWS.md L-40, GATE-DEBUG)"

    cmake -S "$ROOT_DIR" -B "$DEBUG_BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DGLINTFX_WERROR=ON -DGLINTFX_BUILD_TESTS=ON
    cmake --build "$DEBUG_BUILD_DIR"

    test_count="$(count_ctest_tests "$DEBUG_BUILD_DIR")"
    require_nonempty_tests "ctest (debug)" "$test_count" \
        || fail "estagio debug recusado (varredura vazia de testes)"
    ctest --test-dir "$DEBUG_BUILD_DIR" --output-on-failure
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

# --- selftest controls for stage_untracked_guard (GODS_LAWS.md L-40 /
# TODO.md PRECI-UNTRACKED). Every control below builds its OWN
# throwaway git repo and points enumerate_untracked_cpp_hpp at it via
# its directory parameter - never at $ROOT_DIR. Gating --selftest on
# the real tree would make its result depend on whatever another agent
# currently has in flight there (real, not hypothetical - see the
# comment on enumerate_untracked_cpp_hpp), which is itself the shape
# of defect L-40 exists to forbid: a verdict resting on something
# undeclared.

selftest_untracked_guard_repo_dir() {
    mktemp -d "${TMPDIR}/glintfx-preci-untracked-repo.XXXXXX"
}

# One committed .cpp, nothing else - the baseline every control below
# starts from.
selftest_write_untracked_guard_repo() {
    dir="$1"
    git -C "$dir" init -q
    git -C "$dir" config user.email "preci-selftest@glintfx.invalid"
    git -C "$dir" config user.name "preci selftest"
    printf 'int tracked_probe() { return 0; }\n' > "$dir/tracked.cpp"
    git -C "$dir" add tracked.cpp
    git -C "$dir" commit -q -m "selftest: tracked.cpp"
}

run_selftest_untracked_guard_positive_control() {
    log "selftest: guarda de untracked - controle positivo (nada novo)"
    dir="$(selftest_untracked_guard_repo_dir)"
    selftest_write_untracked_guard_repo "$dir"
    enumerate_untracked_cpp_hpp "$dir"
    require_no_untracked_source \
        || fail "controle positivo (guarda-untracked) FALHOU: repositorio so com tracked.cpp committed deveria passar, o guarda reprovou"
    [ "${#FILES[@]}" -eq 0 ] \
        || fail "controle positivo (guarda-untracked) FALHOU: esperava 0 arquivos, FILES tinha ${#FILES[@]}"
    rm -rf "$dir"
    echo "selftest: guarda de untracked - controle positivo OK"
}

run_selftest_untracked_guard_negative_control() {
    log "selftest: guarda de untracked - controle negativo (arquivo novo solto)"
    dir="$(selftest_untracked_guard_repo_dir)"
    selftest_write_untracked_guard_repo "$dir"
    printf 'int solto() { return 1; }\n' > "$dir/solto.cpp"
    enumerate_untracked_cpp_hpp "$dir"
    if require_no_untracked_source 2>/dev/null; then
        rm -rf "$dir"
        fail "controle negativo (guarda-untracked) FALHOU: solto.cpp esta untracked e o guarda aprovou mesmo assim"
    fi
    [ "${#FILES[@]}" -eq 1 ] && [ "${FILES[0]}" = "solto.cpp" ] \
        || fail "controle negativo (guarda-untracked) FALHOU: esperava FILES=[solto.cpp], teve: ${FILES[*]-vazio}"
    rm -rf "$dir"
    echo "selftest: guarda de untracked - controle negativo OK (solto.cpp reprovado)"
}

# The "não deve bloquear demais" half of the fatia: a *.cpp covered by
# .gitignore (the shape of every build/ directory in this tree) must
# NEVER block, or the guard gets disabled within a week.
run_selftest_untracked_guard_ignored_control() {
    log "selftest: guarda de untracked - controle de arquivo ignorado (nao deve bloquear)"
    dir="$(selftest_untracked_guard_repo_dir)"
    selftest_write_untracked_guard_repo "$dir"
    printf 'build/\n' > "$dir/.gitignore"
    mkdir -p "$dir/build"
    printf 'int gerado() { return 2; }\n' > "$dir/build/gerado.cpp"
    enumerate_untracked_cpp_hpp "$dir"
    require_no_untracked_source \
        || fail "controle de arquivo ignorado (guarda-untracked) FALHOU: build/gerado.cpp esta coberto por .gitignore e nao deveria bloquear, o guarda reprovou"
    [ "${#FILES[@]}" -eq 0 ] \
        || fail "controle de arquivo ignorado (guarda-untracked) FALHOU: esperava 0 (respeitar .gitignore), FILES tinha ${#FILES[@]}: ${FILES[*]}"
    rm -rf "$dir"
    echo "selftest: guarda de untracked - controle de arquivo ignorado OK (.gitignore respeitado, nao bloqueou)"
}

# The floor's own "empty scan" control (GODS_LAWS.md L-40 §4: "os tres
# controles da casa sao obrigatorios... e varredura vazia"), adapted to
# this floor's inverted semantics: here an empty FILES is normally the
# GOOD outcome, so the defect this floor could reproduce is a REAL git
# failure silently collapsing into "found nothing, so pass" instead of
# aborting loud. Runs enumerate_untracked_cpp_hpp in an isolated
# subshell (own process, like run_selftest_rootdir_cd_failure_control
# above) against a directory that is not a git repository at all.
run_selftest_untracked_guard_git_failure_control() {
    log "selftest: guarda de untracked - varredura vazia por falha real do comando (nao presumida)"
    nogit_dir="$(mktemp -d "${TMPDIR}/glintfx-preci-untracked-nogit.XXXXXX")"
    rc=0
    out="$(bash -c "
        $(declare -f fail)
        $(declare -f enumerate_untracked_cpp_hpp)
        FILES=()
        enumerate_untracked_cpp_hpp '$nogit_dir'
        echo 'NAO_DEVERIA_CHEGAR_AQUI: FILES=(\${FILES[*]-vazio})'
    " 2>&1)" || rc=$?
    rm -rf "$nogit_dir"
    [ "$rc" -ne 0 ] \
        || fail "controle de falha real de git (guarda-untracked) FALHOU: diretorio sem repositorio deveria abortar a varredura, mas o subshell saiu 0: $out"
    printf '%s\n' "$out" | grep -qF 'varredura recusada' \
        || fail "controle de falha real de git (guarda-untracked) FALHOU: saiu $rc mas sem o diagnostico esperado, saida foi: $out"
    echo "selftest: guarda de untracked - varredura vazia por falha real do comando OK (falha do git nunca vira aprovacao silenciosa)"
}

# GATE-QUOTEPATH (01/09/2026): git's own default (core.quotepath=true)
# prints any path with a byte >= 0x80 as a C-style octal-escaped,
# double-quoted string. Pathspec matching itself (git ls-files -- '*.cpp')
# still finds an accented-named file - the extension is plain ASCII - but
# the LISTED name is the garbled quoted form, not the real path on disk,
# so it never actually reaches clang-format/clang-tidy/cppcheck as a
# usable filename. This control asserts FILES holds the REAL path.
run_selftest_untracked_guard_accented_name_control() {
    log "selftest: guarda de untracked - controle de nome acentuado (GATE-QUOTEPATH)"
    dir="$(selftest_untracked_guard_repo_dir)"
    selftest_write_untracked_guard_repo "$dir"
    printf 'int acentuado() { return 3; }\n' > "$dir/Waylând.cpp"
    enumerate_untracked_cpp_hpp "$dir"
    if require_no_untracked_source 2>/dev/null; then
        rm -rf "$dir"
        fail "controle de nome acentuado (guarda-untracked) FALHOU: Waylând.cpp esta untracked e o guarda aprovou mesmo assim"
    fi
    [ "${#FILES[@]}" -eq 1 ] && [ "${FILES[0]}" = "Waylând.cpp" ] \
        || fail "controle de nome acentuado (guarda-untracked) FALHOU: esperava FILES=[Waylând.cpp] (caminho real, nao escapado por core.quotepath), teve: ${FILES[*]-vazio}"
    rm -rf "$dir"
    echo "selftest: guarda de untracked - controle de nome acentuado OK (Waylând.cpp reportado pelo caminho real, GATE-QUOTEPATH)"
}

run_selftest_untracked_guard_controls() {
    log "selftest: guarda de arquivo novo nao rastreado (stage_untracked_guard)"
    run_selftest_untracked_guard_positive_control
    run_selftest_untracked_guard_negative_control
    run_selftest_untracked_guard_ignored_control
    run_selftest_untracked_guard_git_failure_control
    run_selftest_untracked_guard_accented_name_control
}

# --- selftest controls for stage_debug's assert-count floor
# (GODS_LAWS.md L-40, GATE-DEBUG): count_real_asserts_in_file and
# require_nonempty_asserts are tested directly against throwaway
# fixture files, never against the real tree (same reasoning as every
# other selftest control in this script - enumerate_untracked_cpp_hpp's
# own comment explains why the real tree is off-limits to --selftest).
# The real tree's own count is proven live every time stage_debug /
# --debug-only actually runs - that IS the varredura this floor exists
# to protect, and --selftest is not where it gets re-proven.

selftest_assert_fixture_file() {
    mktemp "${TMPDIR}/glintfx-preci-assert-fixture.XXXXXX.cpp"
}

run_selftest_assert_count_positive_control() {
    log "selftest: contagem de assert() de produto - controle positivo (assert real)"
    f="$(selftest_assert_fixture_file)"
    printf '#include <cassert>\nvoid f(int x) {\n    assert(x > 0 && "x deve ser positivo");\n}\n' > "$f"
    n="$(count_real_asserts_in_file "$f")"
    rm -f "$f"
    [ "$n" -eq 1 ] || fail "controle positivo (contagem de assert) FALHOU: esperava 1 assert() real, contou '$n'"
    require_nonempty_asserts "assert-count[positivo]" "$n" \
        || fail "controle positivo (contagem de assert) FALHOU: o piso recusou 1 assert() real"
    echo "selftest: contagem de assert() de produto - controle positivo OK"
}

# Two things that must NOT count, in the same fixture: a comment line
# that only discusses the mechanism (starts the line, after leading
# whitespace, with //), and a static_assert (compile-time, boundary-
# excluded because the char right before "assert(" is '_').
run_selftest_assert_count_negative_control() {
    log "selftest: contagem de assert() de produto - controle negativo (so comentario e static_assert)"
    f="$(selftest_assert_fixture_file)"
    printf '// assert() eh usado em builds de depuracao\nstatic_assert(sizeof(int) >= 2, "int precisa de ao menos 16 bits");\n' > "$f"
    n="$(count_real_asserts_in_file "$f")"
    rm -f "$f"
    [ "$n" -eq 0 ] || fail "controle negativo (contagem de assert) FALHOU: esperava 0 (comentario e static_assert nao contam), contou '$n'"
    if require_nonempty_asserts "assert-count[negativo]" "$n" 2>/dev/null; then
        fail "controle negativo (contagem de assert) FALHOU: o piso aprovou 0 assert() de produto"
    fi
    echo "selftest: contagem de assert() de produto - controle negativo OK (0 reprovado, nao aprovado)"
}

run_selftest_assert_count_controls() {
    log "selftest: piso de contagem de assert() de produto (stage_debug)"
    run_selftest_assert_count_positive_control
    run_selftest_assert_count_negative_control
}

run_selftest() {
    run_selftest_positive_control
    run_selftest_negative_control
    run_selftest_empty_scan_control
    run_selftest_rootdir_cd_failure_control
    run_selftest_ctest_count_controls
    run_selftest_untracked_guard_controls
    run_selftest_assert_count_controls
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

run_debug_only() {
    log "estagio 8: debug (NDEBUG indefinido, assert() de produto ligado)"
    stage_debug
    echo "preci.sh --debug-only: VERDE"
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
        echo "preci.sh --fast: estagio 8 (debug) PULADO"
    else
        log "estagio 7: sanitizer (ASan/UBSan)"
        stage_sanitizer
        log "estagio 8: debug (NDEBUG indefinido, assert() de produto ligado)"
        stage_debug
    fi
    echo "preci.sh: TUDO VERDE"
}

# Argument validated FIRST, guard SECOND: an unknown flag fails on its
# own usage message, not on a guard result the caller never asked for.
# --selftest is the one mode that skips the guard entirely - it never
# touches $ROOT_DIR (see enumerate_untracked_cpp_hpp's own comment on
# why: the real tree can legitimately have another agent's WIP
# untracked *.cpp mid-onda, and --selftest has to stay usable by
# anyone, any time, regardless of who else is mid-fatia).
main() {
    mode="${1:-}"
    case "$mode" in
        ""|--fast|--lint-only|--sanitizer-only|--debug-only|--selftest) ;;
        *) fail "uso: preci.sh [--fast|--lint-only|--sanitizer-only|--debug-only|--selftest]" ;;
    esac

    if [ "$mode" != "--selftest" ]; then
        log "estagio 0: guarda de arquivo novo nao rastreado"
        stage_untracked_guard
    fi

    case "$mode" in
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
        --debug-only)
            run_debug_only
            ;;
        --selftest)
            run_selftest
            ;;
    esac
}

main "$@"
