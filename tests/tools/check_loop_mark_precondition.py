#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_loop_mark_precondition.py - proves BOTH halves of camada 4's
# own debug-only liveness guard (LOOP-CONTEXT-MARK, S1d, /var/tmp/
# glintfx-plan/loop-fix.md sec. 3.4 item 4, GODS_LAWS.md L-36/L-40):
# calling a bound method (through loop_bind.hpp's own thunks) on an
# object that inherits from gltfx_loop_context_mark, after that
# object's own destructor already ran, is a documented precondition
# violation. In a build where NDEBUG is undefined (Debug) it stops
# DETERMINISTICALLY with a message naming the violation; in a build
# where NDEBUG is defined (Release, this project's default) the guard
# costs nothing and this fixture never even exercises the dead-object
# call (that would be real, unguarded undefined behavior).
#
# THIS FILE IS THE MOLDE OF tests/tools/check_rslt_precondition.py,
# NOT A COPY: it reuses every mechanism that file's own header comment
# already justifies at length (WHY THIS COMPILES THE FIXTURE TWICE,
# the Windows crash-dialog risk and its two mitigations, the POSIX
# signal-exit-status convention) without re-deriving them here - read
# that file's own header for the full reasoning behind each piece
# below. The differences, named so nobody re-derives them by
# comparison: (1) there is only ONE precondition to violate here (no
# "primary"/"void" argv split - the fixture takes no arguments at
# all); (2) Release never calls the guarded path at all (calling it
# would be real UB, not a controlled demonstration of a guard that no
# longer exists) - it only prints two structural facts and exits 0,
# so there is nothing to "expect absent" the way check_rslt_
# precondition.py's own release cases check for an absent message;
# (3) the expected Debug fault is SIGABRT (128+6=134 on the four POSIX
# targets - a real assert()-triggered abort(), not a null-pointer
# SIGSEGV), reusing normalize_posix_signal_exit_status()'s own
# reasoning; the MSVC abort() shape is measured, not assumed, exactly
# like check_rslt_precondition.py's own Windows leg was until its
# first real run corrected it (R1's own history is the precedent for
# treating this as declared-until-measured, not guessed).
#
# WHY THIS COMPILES THE FIXTURE TWICE INSTEAD OF RECONFIGURING A
# SEPARATE CMAKE BUILD TREE: the guard lives ENTIRELY in loop_bind.hpp
# and loop_context_mark.hpp, both header-only (loop_context_mark.hpp's
# own top comment, "WHERE THE CHECK LIVES") - NDEBUG is a property of
# how the FIXTURE's own translation unit is compiled, never of how
# libglintfx itself was built. The already-built library (whatever
# mode it was configured in) is linked against UNCHANGED in both
# compiles below; only the fixture's own two compiles differ, by
# exactly the one flag under test.
#
# Usage:
#   check_loop_mark_precondition.py <include-dir> <generated-include-dir> <runtime-dir> <linker-dir> <cxx-compiler> <compiler-id> <cxx-standard-flag> <msvc-runtime-library>
#
# Each function below does one thing (GODS_LAWS.md L-17).

import ctypes
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT_DIR = pathlib.Path(__file__).resolve().parent.parent.parent
FIXTURE_SRC = ROOT_DIR / "tests" / "tools" / "fixtures" / "loop_mark_fixture.cpp"
ASSERT_MESSAGE = "no longer carries its mark"

# check_rslt_precondition.py's own CRT_DIALOG_SUPPRESSION_HEADER/MARKER
# - same file, same reasoning (win_crt_dialog_suppress.hpp's own top
# comment): force-included on the MSVC compile only, so the debug
# CRT's own assert() report routes to stderr and abort()'s dialog/WER
# flags are cleared from INSIDE the fixture's own process, a mechanism
# SetErrorMode (applied below too) never reaches on its own.
CRT_DIALOG_SUPPRESSION_HEADER = (
    ROOT_DIR / "tests" / "harness" / "win_crt_dialog_suppress.hpp"
)
CRT_DIALOG_SUPPRESSION_MARKER = "glintfx_test: CRT dialog suppression applied"

# 128 + SIGABRT(6) - a real assert()-triggered abort(), the same shell-
# style convention this project's other POSIX gates already read exit
# status by (check_rslt_precondition.py's own POSIX_SIGSEGV_EXIT_
# STATUS=139 comment explains the same convention for SIGSEGV).
POSIX_SIGABRT_EXIT_STATUS = 134

# Generous enough for a debug-flags compile-and-run of one small
# translation unit; tight enough that a hung interactive dialog (the
# Windows dialog-risk this file's header names) fails this script in
# minutes, not in however long the CI job's own overall timeout is.
FIXTURE_TIMEOUT_SECONDS = 60


def fail(message):
    print(f"check_loop_mark_precondition.py: {message}", file=sys.stderr)
    sys.exit(1)


def is_msvc(compiler_id):
    return compiler_id == "MSVC"


# Same lookup check_rslt_precondition.py's own CRT-LINK-WIN header
# comment explains in full: the fixture must be compiled against the
# SAME CRT choice glintfx_library itself was actually linked against,
# never a hardcoded guess.
MSVC_RUNTIME_LIBRARY_TO_FLAG = {
    "MultiThreaded": "/MT",
    "MultiThreadedDebug": "/MTd",
    "MultiThreadedDLL": "/MD",
    "MultiThreadedDebugDLL": "/MDd",
}


def resolve_msvc_runtime_flag(msvc_runtime_library):
    flag = MSVC_RUNTIME_LIBRARY_TO_FLAG.get(msvc_runtime_library)
    if flag is None:
        fail(
            f"unrecognized MSVC_RUNTIME_LIBRARY value: '{msvc_runtime_library}' - expected one of "
            f"{sorted(MSVC_RUNTIME_LIBRARY_TO_FLAG)}"
        )
    return flag


def require_args(argv):
    if len(argv) != 9:
        fail(
            "usage: check_loop_mark_precondition.py <include-dir> <generated-include-dir> "
            "<runtime-dir> <linker-dir> <cxx-compiler> <compiler-id> <cxx-standard-flag> "
            "<msvc-runtime-library>"
        )
    (
        include_dir,
        generated_include_dir,
        runtime_dir,
        linker_dir,
        cxx,
        compiler_id,
        cxx_standard_flag,
        msvc_runtime_library,
    ) = argv[1:]
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
    if not cxx_standard_flag:
        fail("cxx-standard-flag is empty - CMAKE_CXX23_STANDARD_COMPILE_OPTION did not resolve")
    if is_msvc(compiler_id) and not msvc_runtime_library:
        fail(
            "msvc-runtime-library is empty on MSVC - "
            "$<TARGET_PROPERTY:glintfx_library,MSVC_RUNTIME_LIBRARY> did not resolve"
        )
    return (
        include_dir,
        generated_include_dir,
        runtime_dir,
        linker_dir,
        cxx,
        compiler_id,
        cxx_standard_flag,
        msvc_runtime_library,
    )


def apply_windows_crash_dialog_suppression():
    """Best-effort, external mitigation for the interactive crash dialogs
    an assert()-triggered abort() can raise on Windows - see this
    file's own header, and win_crt_dialog_suppress.hpp's own header,
    for the full reasoning. No-op on the four POSIX targets."""
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


def compile_fixture(
    includedir,
    generated_includedir,
    linkerdir,
    cxx,
    compiler_id,
    cxx_standard_flag,
    msvc_runtime_flag,
    ndebug,
    output_bin,
):
    # ndebug is the ONE variable under test - everything else, INCLUDING
    # msvc_runtime_flag, is held fixed between the two compiles (see
    # check_rslt_precondition.py's own CRT-LINK-WIN header comment for
    # why the CRT choice must never also flip alongside NDEBUG).
    if is_msvc(compiler_id):
        command = [
            cxx,
            "/nologo",
            cxx_standard_flag,
            msvc_runtime_flag,
            "/Od",
            "/W4",
            "/WX",
            "/EHsc",
            f"/FI{CRT_DIALOG_SUPPRESSION_HEADER}",
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
        command = [cxx, cxx_standard_flag, "-O0", "-g", "-Wall", "-Wextra", "-Werror"]
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
    (128 + signal) this file's own POSIX_SIGABRT_EXIT_STATUS=134 is
    written against - the same fold check_rslt_precondition.py's own
    twin function already applies for the identical reason. No-op on
    Windows (no POSIX signal delivery there)."""
    if returncode is not None and returncode < 0:
        return 128 - returncode
    return returncode


def run_capture(binary, runtimedir, compiler_id):
    env = os.environ.copy()
    if is_msvc(compiler_id):
        env["PATH"] = f"{runtimedir}{os.pathsep}{env.get('PATH', '')}"
    else:
        env["LD_LIBRARY_PATH"] = f"{runtimedir}{os.pathsep}{env.get('LD_LIBRARY_PATH', '')}"

    try:
        result = subprocess.run(
            [str(binary)],
            capture_output=True,
            text=True,
            env=env,
            timeout=FIXTURE_TIMEOUT_SECONDS,
        )
        status = result.returncode if is_msvc(compiler_id) else normalize_posix_signal_exit_status(
            result.returncode
        )
        return status, result.stdout + result.stderr
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout.decode("utf-8", "replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        stderr = exc.stderr.decode("utf-8", "replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
        note = (
            f"\n[check_loop_mark_precondition.py: TIMED OUT after {FIXTURE_TIMEOUT_SECONDS}s - "
            "most likely an interactive crash dialog that apply_windows_crash_dialog_"
            "suppression() did not fully suppress; this file's own header declares this exact "
            "risk as unmeasured without a Windows toolchain]"
        )
        return None, stdout + stderr + note


def assert_crt_dialog_suppression_applied(output, case_label, compiler_id):
    """SECOND, independent fact from 'the process stopped in time with
    the expected message' - see win_crt_dialog_suppress.hpp's own
    header comment. MSVC-only: the header is force-included on the
    MSVC branch of compile_fixture() only."""
    if not is_msvc(compiler_id):
        return
    if CRT_DIALOG_SUPPRESSION_MARKER not in output:
        fail(
            f"{case_label}: win_crt_dialog_suppress.hpp's own marker "
            f"('{CRT_DIALOG_SUPPRESSION_MARKER}') is missing from the captured output - the /FI "
            "force-include did not take effect, or its dynamic initialization did not run "
            "before main(), so the CRT dialog suppression this run needed may not actually "
            "have applied."
        )


def assert_debug_stops_with_message(binary, runtimedir, compiler_id):
    status, output = run_capture(binary, runtimedir, compiler_id)
    print(f"check_loop_mark_precondition.py: debug run exited with status {status}, output:")
    print(output)

    if status is None:
        fail(f"debug run did not stop within {FIXTURE_TIMEOUT_SECONDS}s - see output above")
    if status == 0:
        fail("debug run exited 0 - the mark guard did not stop the process at all")
    if not is_msvc(compiler_id) and status != POSIX_SIGABRT_EXIT_STATUS:
        fail(
            f"debug run exited {status}, expected {POSIX_SIGABRT_EXIT_STATUS} (128+SIGABRT) - "
            "the process stopped, but not by the assert()-triggered abort() this guard is "
            "supposed to raise"
        )
    if ASSERT_MESSAGE not in output:
        fail(
            f"debug run stopped (status {status}) but its output did not name the violation "
            f"(expected to contain: {ASSERT_MESSAGE!r})"
        )
    assert_crt_dialog_suppression_applied(output, "debug", compiler_id)

    print(
        "check_loop_mark_precondition.py: debug run OK (stopped deterministically, "
        "message present)"
    )


def assert_release_shows_no_debug_message_and_exits_clean(binary, runtimedir, compiler_id):
    status, output = run_capture(binary, runtimedir, compiler_id)
    print(f"check_loop_mark_precondition.py: release run exited with status {status}, output:")
    print(output)

    if status is None:
        fail(f"release run did not stop within {FIXTURE_TIMEOUT_SECONDS}s - see output above")
    if status != 0:
        fail(
            f"release run exited {status}, expected 0 - the fixture's own Release branch never "
            "calls through the dead object; a non-zero exit means something else broke"
        )
    if ASSERT_MESSAGE in output:
        fail(
            "release run printed the DEBUG-ONLY assert message even though compiled with "
            "NDEBUG - the guard is not actually compiled out"
        )
    if "UNEXPECTED" in output:
        fail("release run printed an UNEXPECTED marker - see fixture output above")
    assert_crt_dialog_suppression_applied(output, "release", compiler_id)

    print(
        "check_loop_mark_precondition.py: release run OK (no debug-only message, clean exit - "
        "the guard compiled to nothing)"
    )


def main():
    (
        includedir,
        generated_includedir,
        runtimedir,
        linkerdir,
        cxx,
        compiler_id,
        cxx_standard_flag,
        msvc_runtime_library,
    ) = require_args(sys.argv)
    apply_windows_crash_dialog_suppression()

    msvc_runtime_flag = resolve_msvc_runtime_flag(msvc_runtime_library) if is_msvc(compiler_id) else None

    with tempfile.TemporaryDirectory(prefix="glintfx-loop-mark-precond-") as scratch:
        scratch_path = pathlib.Path(scratch)
        debug_bin = binary_path(scratch_path, "loop_mark_fixture_debug", compiler_id)
        release_bin = binary_path(scratch_path, "loop_mark_fixture_release", compiler_id)

        print("check_loop_mark_precondition.py: compiling debug fixture (NDEBUG undefined)")
        result = compile_fixture(
            includedir,
            generated_includedir,
            linkerdir,
            cxx,
            compiler_id,
            cxx_standard_flag,
            msvc_runtime_flag,
            False,
            debug_bin,
        )
        if result.returncode != 0:
            fail(f"debug fixture failed to compile: {result.stdout}{result.stderr}")

        print("check_loop_mark_precondition.py: compiling release fixture (NDEBUG)")
        result = compile_fixture(
            includedir,
            generated_includedir,
            linkerdir,
            cxx,
            compiler_id,
            cxx_standard_flag,
            msvc_runtime_flag,
            True,
            release_bin,
        )
        if result.returncode != 0:
            fail(f"release fixture failed to compile: {result.stdout}{result.stderr}")

        assert_debug_stops_with_message(debug_bin, runtimedir, compiler_id)
        assert_release_shows_no_debug_message_and_exits_clean(release_bin, runtimedir, compiler_id)

    print(
        "ok: gltfx_loop_context_mark's debug-only liveness guard stops deterministically with "
        "a message in Debug, and costs nothing (no message, clean exit) in Release, on this "
        f"platform's compiler ({compiler_id})."
    )


if __name__ == "__main__":
    main()
