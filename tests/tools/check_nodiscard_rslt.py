#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_nodiscard_rslt.py - CE-4 of CORE-ERROR (TODO.md): proves the
# [[nodiscard]] on glintfx::gltfx_rslt<T> is not decoration. The class
# template being tagged is a claim - "a caller cannot silently drop a
# fallible call's result" - and a claim about a COMPILER DIAGNOSTIC is
# only proven by making the compiler actually emit one, on EVERY
# compiler this project ships a public API for, not just the one that
# happens to be running the session that writes the test.
#
# RSLT-PARITY-WIN (TODO.md, GODS_LAWS.md L-04, ordem do lider
# 03/09/2026): this file REPLACES check_nodiscard_rslt.sh, which was
# POSIX sh and registered only under if(UNIX) - "Windows... does not
# have `sh` by default" (this project's own declared downgrade,
# tests/CMakeLists.txt, embed_test's comment). R1 of docs/api-
# conventions.md documents gltfx_rslt<T>'s single-return-type contract
# as something every public fallible function carries; a gate that
# never runs the compiler this contract's OWN consumers on Windows
# actually use is not proof for that platform, it is an assumption.
# Same precedent this project already set for the identical problem
# (check_dep_zero_trace.py's own header, decision of the leader,
# 28/08/2026, verbatim: "Trocar o mecanismo: perguntar ao CMake") -
# here the mechanism traded away is not CMake-text-vs-trace, it is
# POSIX-sh-vs-a-language-that-runs-everywhere. Python was already the
# authorized tool for exactly this class of job (GODS_LAWS.md L-07's
# own "ferramenta de build" clarification of 28/08/2026), and
# GLINTFX_PYTHON3_EXECUTABLE (tests/CMakeLists.txt, resolved via
# find_program(NAMES python3 python REQUIRED) for dep_zero_trace) is
# reused here rather than re-resolved a second time.
#
# TWO COMPILERS, TWO DIAGNOSTICS, ONE CONTRACT: GCC/Clang warn
# "unused"/"discard" (checked with the same case-insensitive substring
# match the old .sh used); MSVC's own diagnostic for this exact
# situation is C4834 ("discarding return value of function with
# 'nodiscard' attribute" - learn.microsoft.com/cpp/error-messages/
# compiler-warnings/c4834, level 1 warning since Visual Studio 2017
# version 15.7, escalated to a build failure the same way -Werror
# does on the other four platforms: this project's own /WX,
# cmake/GlintfxCompileOptions.cmake). compiler_id (CMAKE_CXX_
# COMPILER_ID, passed in by tests/CMakeLists.txt) selects which
# command line and which diagnostic substring apply - never guessed
# from the compiler executable's name.
#
# Compilation only (no link): gltfx_err's trivial constructor is
# inline (err.hpp), so building a code-only gltfx_err needs no symbol
# from libglintfx at all; the copy constructor and destructor ARE
# out-of-line (GLINTFX_API, err.cpp), but a compile-only step stops
# before the link step ever resolves them, so no library artifact is
# needed here, on either compiler.
#
# STD-FLAG-WIN (estreia real no Windows, run 33832169390, GODS_LAWS.md
# L-44/L-49): this file used to hardcode the literal "/std:c++23" on
# the MSVC branch below. That literal is not a flag any MSVC version
# has ever accepted - the estreia's own log proved it live: `cl :
# Command line warning D9002 : ignoring unknown option '/std:c++23'`,
# cl.exe silently fell back to its pre-C++17 default, and the fixture
# then failed with cascading C2039 ("'string_view'/'variant' is not a
# member of 'std'") - the C2238 first reported was downstream of THAT,
# not of GLINTFX_API or the generated export header (both were found
# and worked correctly; the SAME job's real "Build" step, using the
# SAME cl.exe, already compiles this exact public header successfully
# through CMake's own CXX_STANDARD 23 property). The cxx-standard-flag
# argument below is CMAKE_CXX23_STANDARD_COMPILE_OPTION, resolved by
# CMake itself for whatever compiler/version tests/CMakeLists.txt is
# actually configuring against (on this MSVC toolset that resolves to
# "-std:c++latest", never a literal "/std:c++23" this file or CMake
# would otherwise have to guess) - the exact flag that already proved
# it builds the real target, reused here instead of invented a second
# time per-compiler.
#
# Usage: check_nodiscard_rslt.py <include-dir> <generated-include-dir> <cxx-compiler> <compiler-id> <cxx-standard-flag>
#
# Each function below does one thing (GODS_LAWS.md L-17).

import pathlib
import subprocess
import sys
import tempfile


def fail(message):
    print(f"check_nodiscard_rslt.py: {message}", file=sys.stderr)
    sys.exit(1)


def is_msvc(compiler_id):
    return compiler_id == "MSVC"


def require_args(argv):
    if len(argv) != 6:
        fail(
            "usage: check_nodiscard_rslt.py <include-dir> <generated-include-dir> "
            "<cxx-compiler> <compiler-id> <cxx-standard-flag>"
        )
    include_dir, generated_include_dir, cxx, compiler_id, cxx_standard_flag = argv[1:]
    if not pathlib.Path(include_dir).is_dir():
        fail(f"include dir not found: {include_dir}")
    if not pathlib.Path(generated_include_dir).is_dir():
        fail(f"generated include dir not found: {generated_include_dir}")
    if not cxx_standard_flag:
        fail("cxx-standard-flag is empty - CMAKE_CXX23_STANDARD_COMPILE_OPTION did not resolve")
    return include_dir, generated_include_dir, cxx, compiler_id, cxx_standard_flag


# The two example fallible functions, shared verbatim by both fixtures
# below - only main() differs (discards vs. consumes the results). Not
# part of glintfx's public API: illustrative only, for this gate.
def example_functions_source():
    return """#include <glintfx/core/err.hpp>

#include <string_view>

glintfx::gltfx_rslt<int> parse_positive_int(std::string_view text) noexcept {
    if (text.empty()) {
        return glintfx::gltfx_rslt<int>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::invalid_argument));
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return glintfx::gltfx_rslt<int>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::parse_failure));
        }
        value = (value * 10) + (c - '0');
    }
    return glintfx::gltfx_rslt<int>::ok(value);
}

glintfx::gltfx_rslt<void> validate_non_empty(std::string_view text) noexcept {
    if (text.empty()) {
        return glintfx::gltfx_rslt<void>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::invalid_argument));
    }
    return glintfx::gltfx_rslt<void>::ok();
}
"""


def write_discard_fixture(path):
    path.write_text(
        example_functions_source()
        + """
int main() {
    parse_positive_int("42");
    validate_non_empty("not empty");
    return 0;
}
""",
        encoding="utf-8",
    )


def write_consume_fixture(path):
    path.write_text(
        example_functions_source()
        + """
int main() {
    const auto parsed = parse_positive_int("42");
    const auto validated = validate_non_empty("not empty");
    return (parsed.has_value() ? 0 : 1) + (validated.has_value() ? 0 : 1);
}
""",
        encoding="utf-8",
    )


def compile_fixture(src, includedir, generated_includedir, cxx, compiler_id, cxx_standard_flag, obj):
    if is_msvc(compiler_id):
        # /W4 /WX: the same warning level and warnings-as-errors this
        # project's own MSVC target already builds under
        # (cmake/GlintfxCompileOptions.cmake) - C4834 is a level 1
        # warning, so it is already covered by /W4, and /WX is what
        # turns it into the compile FAILURE this gate needs, the same
        # role -Werror plays for GCC/Clang below. /EHsc: this
        # translation unit's real headers use exceptions internally
        # (R3, docs/api-conventions.md); without it MSVC warns C4530.
        # cxx_standard_flag (CMAKE_CXX23_STANDARD_COMPILE_OPTION, see
        # this file's own STD-FLAG-WIN header comment) replaces the
        # literal "/std:c++23" no MSVC version actually accepts.
        command = [
            cxx,
            "/nologo",
            cxx_standard_flag,
            "/W4",
            "/WX",
            "/EHsc",
            f"/I{includedir}",
            f"/I{generated_includedir}",
            "/c",
            str(src),
            f"/Fo{obj}",
        ]
    else:
        command = [
            cxx,
            cxx_standard_flag,
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I",
            includedir,
            "-I",
            generated_includedir,
            "-c",
            str(src),
            "-o",
            str(obj),
        ]
    return subprocess.run(command, capture_output=True, text=True)


def assert_discard_fixture_fails_naming_nodiscard(
    src, includedir, generated_includedir, cxx, compiler_id, cxx_standard_flag
):
    obj = src.with_suffix(".obj" if is_msvc(compiler_id) else ".o")
    result = compile_fixture(src, includedir, generated_includedir, cxx, compiler_id, cxx_standard_flag, obj)
    combined = result.stdout + result.stderr

    if result.returncode == 0:
        fail(
            "discard fixture compiled CLEANLY - [[nodiscard]] on gltfx_rslt<T> "
            f"is not enforced: {combined}"
        )

    if is_msvc(compiler_id):
        needle_found = "c4834" in combined.lower()
        expected_desc = "C4834 (learn.microsoft.com/cpp/error-messages/compiler-warnings/c4834)"
    else:
        needle_found = any(needle in combined.lower() for needle in ("nodiscard", "discard", "unused"))
        expected_desc = "nodiscard/discard/unused"

    if not needle_found:
        fail(
            f"discard fixture failed to compile, but not citing {expected_desc} - "
            f"output: {combined}"
        )

    print("check_nodiscard_rslt.py: discard fixture correctly REFUSED to compile ([[nodiscard]] fired)")


def assert_consume_fixture_compiles_cleanly(
    src, includedir, generated_includedir, cxx, compiler_id, cxx_standard_flag
):
    obj = src.with_suffix(".obj" if is_msvc(compiler_id) else ".o")
    result = compile_fixture(src, includedir, generated_includedir, cxx, compiler_id, cxx_standard_flag, obj)

    if result.returncode != 0:
        fail(f"consume fixture (result actually used) FAILED to compile: {result.stdout}{result.stderr}")

    print("check_nodiscard_rslt.py: consume fixture (result actually used) compiled cleanly")


def main():
    includedir, generated_includedir, cxx, compiler_id, cxx_standard_flag = require_args(sys.argv)

    with tempfile.TemporaryDirectory(prefix="glintfx-nodiscard-") as scratch:
        scratch_path = pathlib.Path(scratch)
        discard_src = scratch_path / "discard.cpp"
        consume_src = scratch_path / "consume.cpp"
        write_discard_fixture(discard_src)
        write_consume_fixture(consume_src)

        assert_discard_fixture_fails_naming_nodiscard(
            discard_src, includedir, generated_includedir, cxx, compiler_id, cxx_standard_flag
        )
        assert_consume_fixture_compiles_cleanly(
            consume_src, includedir, generated_includedir, cxx, compiler_id, cxx_standard_flag
        )

    print(
        "ok: gltfx_rslt<T>'s [[nodiscard]] is a real compiler diagnostic, proven against both "
        f"gltfx_rslt<int> and gltfx_rslt<void>, not decoration, on this platform's compiler ({compiler_id})."
    )


if __name__ == "__main__":
    main()
