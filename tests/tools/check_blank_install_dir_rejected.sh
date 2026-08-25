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
# Usage: check_blank_install_dir_rejected.sh <glintfx-source-dir>
#
# Each function below does one thing (GODS_LAWS.md L-17).

set -eu

fail() {
    echo "check_blank_install_dir_rejected.sh: $1" >&2
    exit 1
}

require_args() {
    [ "$#" -eq 1 ] || fail "usage: check_blank_install_dir_rejected.sh <glintfx-source-dir>"
    [ -d "$1" ] || fail "glintfx source dir not found: $1"
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

    output="$(cmake -S "$glintfx_src" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        "-D${var_name}=${blank_value}" \
        -DGLINTFX_BUILD_TESTS=OFF 2>&1)" \
        && fail "configure with ${var_name}='${blank_value}' unexpectedly SUCCEEDED - the blank-value guard did not fire"

    case "$output" in
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

    scratch="$(make_scratch_workdir)"
    trap 'rm -rf "$scratch"' EXIT

    assert_configure_rejects_blank_var "$glintfx_src" "${scratch}/build-empty-libdir" "CMAKE_INSTALL_LIBDIR" ""
    assert_configure_rejects_blank_var "$glintfx_src" "${scratch}/build-blank-libdir" "CMAKE_INSTALL_LIBDIR" " "
    assert_configure_rejects_blank_var "$glintfx_src" "${scratch}/build-empty-includedir" "CMAKE_INSTALL_INCLUDEDIR" ""
    assert_configure_rejects_blank_var "$glintfx_src" "${scratch}/build-blank-includedir" "CMAKE_INSTALL_INCLUDEDIR" " "

    echo "ok: CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR are both rejected at configure time when empty or whitespace-only, with a message naming the offending variable."
}

main "$@"
