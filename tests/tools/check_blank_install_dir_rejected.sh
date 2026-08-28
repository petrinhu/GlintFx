#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# check_blank_install_dir_rejected.sh - proves CMAKE_INSTALL_LIBDIR and
# CMAKE_INSTALL_INCLUDEDIR, when given empty ("") or whitespace-only
# (" "), FAIL configure with glintfx's OWN FATAL_ERROR
# (glintfx_require_nonblank_install_subdir, cmake/GlintfxInstall.cmake) -
# instead of silently slipping through GNUInstallDirs' own defaulting
# (confirmed live before this guard existed: `-DCMAKE_INSTALL_LIBDIR=`
# used to configure successfully and only fail LATER, deep inside
# cmake_install.cmake's own generated script, with "file cannot create
# directory: /cmake/glintfx" - because the empty variable collapsed an
# install() DESTINATION to the filesystem ROOT).
#
# Blank is never a legitimate layout - unlike a malformed-but-nonblank
# value (adversarial review achados 1/3, which each still name a real
# layout somewhere and needed careful path computation), an empty or
# whitespace-only value is always a mistake in how it was passed, so
# this is refused outright, not computed around.
#
# The second argument (compiler) exists because every configure below
# is a FULL, SECOND, independent configure of the glintfx tree
# (project(glintfx LANGUAGES CXX), CMakeLists.txt:18) - it does not
# inherit whatever -DCMAKE_CXX_COMPILER the CALLER's own build used,
# and CMake's own auto-detection only tries a short list of generic
# names (c++, g++, ...). A job that installs a versioned compiler
# package without that generic name in PATH (e.g. this project's own
# Ubuntu CI job: g++-14 only, no plain g++/c++) fails compiler
# detection before glintfx_require_nonblank_install_subdir ever runs -
# a false negative for THIS test, confirmed live in a throwaway
# ubuntu:24.04 container mirroring the CI job's own install line
# (GODS_LAWS.md L-14: container only, nothing installed on the
# developer's machine).
#
# Usage: check_blank_install_dir_rejected.sh <glintfx-source-dir> <cxx-compiler>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_blank_install_dir_rejected.sh: $1" >&2
    exit 1
}

# ASSERT-WRAP: undoes CMake's own message() text-wrapping so a `case`
# pattern that searches for a MULTI-WORD phrase does not miss it just
# because the wrap happened to land between two of that phrase's words.
# Same defect class already named and fixed for check_pkgconfig_validate.sh
# (PKG-VALIDATE-WRAP, commit d2110d9) - that fix's own commit message
# enumerated THIS file's "*${var_name}*empty or blank*" as the same
# structural shape, left unfixed "por prudencia" (out of that commit's
# scope) - this is that fix, applied here. CMake's message() wrapping
# never breaks INSIDE an unbroken run of non-whitespace (a var name or
# path with no spaces stays intact on one line, however long) - only
# BETWEEN words - so collapsing every run of whitespace (newline
# included) back to one space is sufficient to reconstruct the original
# phrase; no word is ever split mid-token.
normalize_wrapped_message() {
    printf '%s' "$1" | tr '\n' ' ' | tr -s ' '
}

require_args() {
    [ "$#" -eq 2 ] || fail "usage: check_blank_install_dir_rejected.sh <glintfx-source-dir> <cxx-compiler>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
    [ -n "$2" ] || fail "cxx-compiler argument is empty"
}

make_scratch_workdir() {
    mktemp -d "${TMPDIR:-/tmp}/glintfx-blankdir-XXXXXX"
}

# Configures with ONE of the two install-dir variables set to the
# given (empty or blank) value, and asserts configure FAILS with a
# message naming that exact variable as empty or blank - not just
# ANY failure (a wrong-but-still-failing message would hide a
# regression of the guard's own wording just as effectively as a
# silent pass would).
assert_configure_rejects_blank_var() {
    glintfx_src="$1"
    build_dir="$2"
    var_name="$3"
    blank_value="$4"
    cxx_compiler="$5"

    output="$(cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_CXX_COMPILER=${cxx_compiler}" \
        "-D${var_name}=${blank_value}" \
        -DGLINTFX_BUILD_TESTS=OFF 2>&1)" \
        && fail "configure with ${var_name}='${blank_value}' unexpectedly SUCCEEDED - the blank-value guard did not fire"

    normalized_output="$(normalize_wrapped_message "$output")"
    case "$normalized_output" in
        *"${var_name}"*"empty or blank"*) : ;;
        *)
            fail "configure with ${var_name}='${blank_value}' failed, but not with the expected message naming '${var_name}' as empty or blank. Got:
${output}"
            ;;
    esac
    echo "check_blank_install_dir_rejected.sh: ${var_name}='${blank_value}' correctly rejected at configure time"
}

main() {
    require_args "$@"
    glintfx_src="$1"
    cxx_compiler="$2"

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    assert_configure_rejects_blank_var "$glintfx_src" "${scratch}/build-empty-libdir" "CMAKE_INSTALL_LIBDIR" "" "$cxx_compiler"
    assert_configure_rejects_blank_var "$glintfx_src" "${scratch}/build-blank-libdir" "CMAKE_INSTALL_LIBDIR" " " "$cxx_compiler"
    assert_configure_rejects_blank_var "$glintfx_src" "${scratch}/build-empty-includedir" "CMAKE_INSTALL_INCLUDEDIR" "" "$cxx_compiler"
    assert_configure_rejects_blank_var "$glintfx_src" "${scratch}/build-blank-includedir" "CMAKE_INSTALL_INCLUDEDIR" " " "$cxx_compiler"

    echo "ok: CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR are both rejected at configure time when empty or whitespace-only, with a message naming the offending variable."
}

main "$@"
