#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_rslt_precondition.py - proves BOTH halves of the debug-only
# precondition guard on gltfx_rslt<T>::value()/error() (correction of
# 25/08/2026, GODS_LAWS.md CORE-ERROR): calling value() on a result
# that holds an error, or error() on a result that holds a value, is a
# documented precondition violation. In a Debug build (NDEBUG
# undefined) it stops DETERMINISTICALLY with a message naming the
# violation; in a Release build (NDEBUG defined, this project's
# default) the guard costs nothing and the behavior is UNCHANGED from
# before this fix.
#
# RSLT-PARITY-WIN (TODO.md, GODS_LAWS.md L-04, ordem do lider
# 03/09/2026): this file REPLACES check_rslt_precondition.sh, which
# was POSIX sh and registered only under if(UNIX) - see check_
# nodiscard_rslt.py's own header for the same "Windows has no sh by
# default" precedent this fatia answers for both gates at once.
# docs/api-conventions.md R1 documents this precondition's Debug/
# Release shape as something EVERY consumer of a fallible public
# function relies on; a gate that never runs on the compiler this
# project's Windows consumers actually use is not proof for that
# platform, it is an assumption R1 used to carry without saying so.
#
# WHY THIS COMPILES THE FIXTURE TWICE INSTEAD OF RECONFIGURING A
# SEPARATE CMAKE BUILD TREE: value()/error() and their assert() calls
# are HEADER-ONLY inline functions (include/glintfx/core/err.hpp,
# gltfx_rslt<T> is a template - it cannot be hidden behind the
# library's ABI boundary, see that header's own comment). NDEBUG is
# therefore a property of how the CONSUMER'S OWN translation unit is
# compiled, not of how libglintfx itself was built - the already-
# built library (whatever mode it was configured in) is linked
# against UNCHANGED in both cases below; only the FIXTURE's own two
# compiles differ, by exactly the one flag (-DNDEBUG / /DNDEBUG) that
# is the actual subject under test. This mirrors CMake's own
# convention (a Release build type defines NDEBUG; Debug does not)
# without spinning up a second full glintfx build merely to flip it.
# Same fixture source on both compilers: tests/precondition_fixtures/
# precondition_fixture.cpp is OUT OF SCOPE for this fatia (see the
# ORDEM DE SERVICO: only the two test scripts, tests/CMakeLists.txt,
# and docs/api-conventions.md if a measurement changes it) - it is
# read here, never edited.
#
# ASSERT()'S OWN MESSAGE FORMAT IS COMPILER-SPECIFIC TEXT AROUND THE
# SAME EXPRESSION, so the substring match below (ASSERT_MESSAGE_
# PRIMARY / ASSERT_MESSAGE_VOID) works unchanged on both: glibc's
# assert() prints the failed expression verbatim; MSVC's assert()
# (learn.microsoft.com/cpp/c-runtime-library/reference/assert-macro-
# assert-wassert) prints "Assertion failed: <expression>, file <file>,
# line <line>", and <expression> is the SAME source text - the two
# ASSERT_MESSAGE_* constants are substrings of the string LITERAL
# ANDed into each assert() call in err.hpp, so they appear verbatim in
# either compiler's rendering of the expression.
#
# THE WINDOWS INTERACTIVE-DIALOG RISK, NOT MEASURED HERE (GODS_LAWS.md
# L-04: no Windows toolchain on this machine) - read before touching
# this file's Windows branch: Microsoft's own documentation for
# assert() states plainly that "a dialog box is always displayed
# following an assert call in debug mode" (same URL as above) - the
# message itself goes to stderr for a console app (this fixture is
# one), but the abort() that follows shows its OWN Abort/Retry/Ignore
# dialog whenever assert's message went to stderr. A modal dialog on
# an unattended CI runner does not fail fast - it HANGS the job until
# a timeout kills it. Two independent mitigations are applied, from
# OUTSIDE the fixture's own source (it cannot be edited here):
#   1. apply_windows_crash_dialog_suppression() calls the Win32
#      SetErrorMode API in THIS process before spawning the fixture.
#      SetErrorMode's flags are process-wide and Windows documents
#      them as inherited by child processes (learn.microsoft.com/
#      windows/win32/api/errhandlingapi/nf-errhandlingapi-seterrormode)
#      - this is the standard external mechanism unattended test
#      harnesses use to suppress the Windows Error Reporting "stopped
#      working" dialog and the related abort() dialog without
#      recompiling the crashing binary. NOT proven here to fully
#      suppress the SPECIFIC Abort/Retry/Ignore dialog the assert()
#      documentation describes - only that this is the documented,
#      external, standard mechanism for this class of problem.
#   2. FIXTURE_TIMEOUT_SECONDS bounds every subprocess call. If
#      mitigation 1 is incomplete and a dialog appears anyway, this
#      script FAILS LOUD after a timeout instead of hanging the
#      runner - the failure IS the vermelho de estreia this fatia's
#      ORDEM DE SERVICO explicitly allows for a case that cannot be
#      measured without a Windows toolchain: whatever the first real
#      run reports (clean pass, or a timeout naming which case hung)
#      becomes the next fact this file's own comments are corrected
#      against, the same way docs/api-conventions.md's R1 table
#      already documents two prior corrections made after live
#      evidence contradicted an assumption.
#
# THE RELEASE/VOID STRUCTURAL FAULT SHAPE DIFFERS BY PLATFORM, BY
# CONSTRUCTION, NOT BY GUESS: gltfx_rslt<void>::error() dereferences a
# genuine null pointer (std::get_if) the same way the primary template
# already does (docs/api-conventions.md R1) - on the four POSIX
# targets this is SIGSEGV, exit status 139 (128 + signal 11), already
# proven live by the .sh predecessor of this file. On Windows, a null-
# pointer read that faults is reported as an unhandled structured
# exception, STATUS_ACCESS_VIOLATION (0xC0000005) - Windows surfaces
# an unhandled exception's NT status AS the process's own exit code
# when nothing intercepts it, which Python's subprocess.returncode
# reads back as the SAME bit pattern interpreted as a signed 32-bit
# integer: -1073741819. This is the documented STRUCTURAL shape
# (learn.microsoft.com/windows/win32/debug/getexceptioncode -
# EXCEPTION_ACCESS_VIOLATION's underlying NT status code), the exact
# same epistemic category docs/api-conventions.md R1 already uses for
# the POSIX SIGSEGV claim ("structural... not a guarantee from the
# standard") - NOT measured live on this machine, and the first
# Windows CI run is what actually proves or refutes the exact value.
#
# Usage:
#   check_rslt_precondition.py <include-dir> <generated-include-dir> <runtime-dir> <linker-dir> <cxx-compiler> <compiler-id>
#
# runtime-dir and linker-dir are the SAME directory on the four POSIX
# targets (a .so is both the runtime artifact and what the linker
# consumes) - tests/CMakeLists.txt still passes $<TARGET_FILE_DIR:...>
# for both there. On Windows they can DIFFER (glintfx.dll is the
# runtime artifact; glintfx.lib, the import library, is what the
# linker consumes, and CMake's own $<TARGET_LINKER_FILE_DIR:...>
# generator expression is the portable way to ask for its directory
# rather than assuming it sits next to the DLL) - runtime-dir feeds
# PATH (Windows) / -Wl,-rpath (POSIX) for the RUN step, linker-dir
# feeds /LIBPATH: (Windows) / -L (POSIX) for the COMPILE step.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import ctypes
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT_DIR = pathlib.Path(__file__).resolve().parent.parent.parent
FIXTURE_SRC = ROOT_DIR / "tests" / "precondition_fixtures" / "precondition_fixture.cpp"
ASSERT_MESSAGE_PRIMARY = "value() called on a result that holds an error"
ASSERT_MESSAGE_VOID = "error() called on a result that holds success"

# 128 + SIGSEGV(11), this project's Linux gates already read exit status this way.
POSIX_SIGSEGV_EXIT_STATUS = 139

# 0xC0000005 (STATUS_ACCESS_VIOLATION) read as a signed 32-bit integer -
# see "THE RELEASE/VOID STRUCTURAL FAULT SHAPE DIFFERS BY PLATFORM" above.
# NOT MEASURED on this machine.
WINDOWS_ACCESS_VIOLATION_EXIT_STATUS = -1073741819

# Generous enough for a debug-flags compile-and-run of one small
# translation unit, tight enough that a hung interactive dialog (see
# the Windows dialog-risk note above) fails this script in minutes,
# not in however long the CI job's own overall timeout is.
FIXTURE_TIMEOUT_SECONDS = 60


def fail(message):
    print(f"check_rslt_precondition.py: {message}", file=sys.stderr)
    sys.exit(1)


def is_msvc(compiler_id):
    return compiler_id == "MSVC"


def require_args(argv):
    if len(argv) != 7:
        fail(
            "usage: check_rslt_precondition.py <include-dir> <generated-include-dir> "
            "<runtime-dir> <linker-dir> <cxx-compiler> <compiler-id>"
        )
    include_dir, generated_include_dir, runtime_dir, linker_dir, cxx, compiler_id = argv[1:]
    for label, path in (
        ("include dir", include_dir),
        ("generated include dir", generated_include_dir),
        ("runtime dir", runtime_dir),
        ("linker dir", linker_dir),
    ):
        if not pathlib.Path(path).is_dir():
            fail(f"{label} not found: {path}")
    if not FIXTURE_SRC.is_file():
        fail(f"fixture source not found: {FIXTURE_SRC}")
    return include_dir, generated_include_dir, runtime_dir, linker_dir, cxx, compiler_id


def apply_windows_crash_dialog_suppression():
    """Best-effort, external mitigation for the interactive crash dialogs
    documented for both a Debug assert() and an unhandled Release access
    violation - see this file's own header for the full reasoning and its
    declared limits. No-op on the four POSIX targets."""
    if sys.platform != "win32":
        return
    SEM_FAILCRITICALERRORS = 0x0001
    SEM_NOGPFAULTERRORBOX = 0x0002
    SEM_NOOPENFILEERRORBOX = 0x8000
    ctypes.windll.kernel32.SetErrorMode(
        SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX
    )


def binary_path(scratch_path, name, compiler_id):
    return scratch_path / (f"{name}.exe" if is_msvc(compiler_id) else name)


def compile_fixture(includedir, generated_includedir, linkerdir, cxx, compiler_id, ndebug, output_bin):
    # ndebug is the ONE variable under test - everything else is held
    # fixed between the two compiles.
    if is_msvc(compiler_id):
        command = [
            cxx,
            "/nologo",
            "/std:c++23",
            "/Od",
            "/W4",
            "/WX",
            "/EHsc",
        ]
        if ndebug:
            command.append("/DNDEBUG")
        command += [
            f"/I{includedir}",
            f"/I{generated_includedir}",
            str(FIXTURE_SRC),
            f"/Fe{output_bin}",
            "/link",
            f"/LIBPATH:{linkerdir}",
            "glintfx.lib",
        ]
    else:
        command = [cxx, "-std=c++23", "-O0", "-g", "-Wall", "-Wextra", "-Werror"]
        if ndebug:
            command.append("-DNDEBUG")
        command += [
            "-I",
            includedir,
            "-I",
            generated_includedir,
            str(FIXTURE_SRC),
            "-L",
            linkerdir,
            "-Wl,-rpath," + linkerdir,
            "-lglintfx",
            "-o",
            str(output_bin),
        ]
    return subprocess.run(command, capture_output=True, text=True)


def normalize_posix_signal_exit_status(returncode):
    """Python's own subprocess.returncode uses POSIX's NATIVE convention
    (negative = killed by signal -returncode), not the SHELL convention
    (128 + signal) the predecessor .sh script's `$?` used and that this
    file's own POSIX_SIGSEGV_EXIT_STATUS=139 is written against - caught
    live (this fatia's own first ctest run: release/void reported -11,
    not 139, for the exact same SIGSEGV). Windows never returns a
    negative returncode for this reason (no POSIX signal delivery), so
    this is a no-op there."""
    if returncode is not None and returncode < 0:
        return 128 - returncode
    return returncode


def run_capture(binary, case_arg, runtimedir, compiler_id):
    env = os.environ.copy()
    if is_msvc(compiler_id):
        env["PATH"] = f"{runtimedir}{os.pathsep}{env.get('PATH', '')}"
    else:
        env["LD_LIBRARY_PATH"] = f"{runtimedir}{os.pathsep}{env.get('LD_LIBRARY_PATH', '')}"

    try:
        result = subprocess.run(
            [str(binary), case_arg],
            capture_output=True,
            text=True,
            env=env,
            timeout=FIXTURE_TIMEOUT_SECONDS,
        )
        return normalize_posix_signal_exit_status(result.returncode), result.stdout + result.stderr
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout.decode("utf-8", "replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        stderr = exc.stderr.decode("utf-8", "replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
        note = (
            f"\n[check_rslt_precondition.py: TIMED OUT after {FIXTURE_TIMEOUT_SECONDS}s running "
            f"'{case_arg}' - most likely an interactive crash dialog that apply_windows_crash_"
            "dialog_suppression() did not fully suppress; this file's own header declares this "
            "exact risk as unmeasured without a Windows toolchain]"
        )
        return None, stdout + stderr + note


def assert_debug_case_stops_with_message(binary, case_arg, expected_message, runtimedir, compiler_id):
    status, output = run_capture(binary, case_arg, runtimedir, compiler_id)
    print(f"check_rslt_precondition.py: debug/{case_arg} exited with status {status}, output:")
    print(output)

    if status is None:
        fail(f"debug/{case_arg} did not stop within {FIXTURE_TIMEOUT_SECONDS}s - see output above")
    if status == 0:
        fail(f"debug/{case_arg} exited 0 - the precondition violation did not stop the process at all")
    if expected_message not in output:
        fail(
            f"debug/{case_arg} stopped (status {status}) but its output did not name the "
            f"violation (expected to contain: {expected_message})"
        )

    print(f"check_rslt_precondition.py: debug/{case_arg} OK (stopped deterministically, message present)")


def assert_release_case_shows_no_debug_message(binary, case_arg, forbidden_message, runtimedir, compiler_id):
    status, output = run_capture(binary, case_arg, runtimedir, compiler_id)
    print(f"check_rslt_precondition.py: release/{case_arg} exited with status {status}, output:")
    print(output)

    if status is None:
        fail(f"release/{case_arg} did not stop within {FIXTURE_TIMEOUT_SECONDS}s - see output above")
    if forbidden_message in output:
        fail(
            f"release/{case_arg} printed the DEBUG-ONLY assert message even though compiled with "
            "NDEBUG - the guard is not actually compiled out"
        )

    print(
        f"check_rslt_precondition.py: release/{case_arg} OK (no debug-only message - assert "
        "compiled to nothing, whatever happened is the SAME undefined behavior this code already had)"
    )


def assert_release_void_faults_via_null_dereference(binary, runtimedir, compiler_id):
    expected_status = WINDOWS_ACCESS_VIOLATION_EXIT_STATUS if is_msvc(compiler_id) else POSIX_SIGSEGV_EXIT_STATUS
    status, output = run_capture(binary, "void", runtimedir, compiler_id)
    print(f"check_rslt_precondition.py: release/void (null-dereference check) exited with status {status}, output:")
    print(output)

    if status != expected_status:
        fail(
            f"release/void exited {status}, expected {expected_status} - gltfx_rslt<void>::error() "
            "no longer faults via a real null-pointer dereference the way gltfx_rslt<T>'s primary "
            "template already does"
        )

    print(
        "check_rslt_precondition.py: release/void OK (structural null-pointer fault on this platform, "
        "not a library-level guarantee)"
    )


def main():
    includedir, generated_includedir, runtimedir, linkerdir, cxx, compiler_id = require_args(sys.argv)
    apply_windows_crash_dialog_suppression()

    with tempfile.TemporaryDirectory(prefix="glintfx-rslt-precond-") as scratch:
        scratch_path = pathlib.Path(scratch)
        debug_bin = binary_path(scratch_path, "precondition_fixture_debug", compiler_id)
        release_bin = binary_path(scratch_path, "precondition_fixture_release", compiler_id)

        print("check_rslt_precondition.py: compiling debug fixture (NDEBUG undefined)")
        result = compile_fixture(includedir, generated_includedir, linkerdir, cxx, compiler_id, False, debug_bin)
        if result.returncode != 0:
            fail(f"debug fixture failed to compile: {result.stdout}{result.stderr}")

        print("check_rslt_precondition.py: compiling release fixture (NDEBUG)")
        result = compile_fixture(includedir, generated_includedir, linkerdir, cxx, compiler_id, True, release_bin)
        if result.returncode != 0:
            fail(f"release fixture failed to compile: {result.stdout}{result.stderr}")

        assert_debug_case_stops_with_message(debug_bin, "primary", ASSERT_MESSAGE_PRIMARY, runtimedir, compiler_id)
        assert_debug_case_stops_with_message(debug_bin, "void", ASSERT_MESSAGE_VOID, runtimedir, compiler_id)

        assert_release_case_shows_no_debug_message(release_bin, "primary", ASSERT_MESSAGE_PRIMARY, runtimedir, compiler_id)
        assert_release_case_shows_no_debug_message(release_bin, "void", ASSERT_MESSAGE_VOID, runtimedir, compiler_id)

        assert_release_void_faults_via_null_dereference(release_bin, runtimedir, compiler_id)

    print(
        "ok: gltfx_rslt<T>'s debug-only precondition guard stops deterministically with a message "
        "in Debug, and costs nothing (no message, unchanged behavior) in Release, for both the "
        "primary template and the void specialization, on this platform's compiler "
        f"({compiler_id})."
    )


if __name__ == "__main__":
    main()
