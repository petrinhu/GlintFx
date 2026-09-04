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
# STD-FLAG-WIN (estreia real no Windows, run 33832169390, GODS_LAWS.md
# L-44/L-49): this file used to hardcode the literal "/std:c++23" on
# the MSVC branch of compile_fixture(). No MSVC version has ever
# accepted that literal - the estreia's own log named the exact
# mechanism: `cl : Command line warning D9002 : ignoring unknown
# option '/std:c++23'`, cl.exe fell back to its pre-C++17 default, and
# BOTH compiles below (debug and release) failed with the same
# cascading C2039 ("'string_view'/'variant' is not a member of 'std'")
# before ever reaching the assert()/access-violation behavior this
# file exists to measure - "debug fixture failed to compile" in the
# log, not a runtime result. See check_nodiscard_rslt.py's own STD-
# FLAG-WIN comment for why GLINTFX_API/the generated export header
# were never the problem (the SAME job's real "Build" step, same
# cl.exe, already compiles this exact header successfully). The new
# cxx-standard-flag argument is CMAKE_CXX23_STANDARD_COMPILE_OPTION,
# CMake's own resolution of C++23 for whatever compiler/version is
# actually configured - "-std:c++latest" on this MSVC toolset, never a
# literal this script or CMake would otherwise have to guess again.
#
# CRT-LINK-WIN (Windows static-mode estreia, coordinator report
# 04/09/2026, GODS_LAWS.md L-04): the shared-mode Windows job passed but
# the STATIC one failed at LINK, not at compile - LNK2038 "mismatch
# detected for 'RuntimeLibrary': value 'MD_DynamicRelease' doesn't match
# value 'MT_StaticRelease'". glintfx_library was built with the C runtime
# CMake picks by default under policy CMP0091 (NEW since this project's
# cmake_minimum_required is 4.1, well past 3.15 where CMP0091 was
# introduced) when CMAKE_MSVC_RUNTIME_LIBRARY is left unset: the target
# property MSVC_RUNTIME_LIBRARY defaults to "MultiThreaded$<$<CONFIG:
# Debug>:Debug>DLL", i.e. the DYNAMIC CRT (/MD in this project's Release
# config - see the "Comandos" section of CLAUDE.md, always -DCMAKE_
# BUILD_TYPE=Release). compile_fixture()'s MSVC branch below never
# passed /MD or /MT at all, so cl.exe fell back to ITS OWN default,
# which is the STATIC CRT (/MT) - the two never agreed, on BOTH the
# debug and release fixture compiles, only the static BUILD_SHARED_LIBS
# mode links glintfx.lib (a static archive) directly into the fixture's
# own binary, which is exactly where MSVC's linker enforces this
# consistency; the shared mode links against glintfx.dll's import
# library through the same mechanism but happened to still pass because
# nothing else in that .lib pulls the mismatched runtime the same way
# (not proven identical by design, just not the failure this fatia
# chased). The FIX IS NOT "use the debug CRT for the debug fixture" -
# the debug/release axis here is this script's OWN NDEBUG toggle on the
# fixture (see "WHY THIS COMPILES THE FIXTURE TWICE" above), a
# completely separate axis from which CRT glintfx_library itself was
# linked against; MSVC keeps assert()'s behavior and CRT linkage
# independent (Debug/RelWithDebInfo asserts still fire when linked
# against the dynamic-Release CRT). The correct fix is to compile BOTH
# fixture binaries against the SAME CRT choice glintfx_library actually
# used, always - never a literal "/MD" this file would have to guess
# again the way STD-FLAG-WIN's predecessor guessed a standard flag.
# msvc_runtime_library (new argument below) is tests/CMakeLists.txt's
# own $<TARGET_PROPERTY:glintfx_library,MSVC_RUNTIME_LIBRARY> generator
# expression, already resolved to one of CMake's four canonical strings
# (MultiThreaded[Debug][DLL]) for whichever config this build actually
# used - resolve_msvc_runtime_flag() below is a straight lookup table
# from those four CMake-defined strings to the matching cl.exe flag,
# not a re-derivation of the policy default.
#
# Usage:
#   check_rslt_precondition.py <include-dir> <generated-include-dir> <runtime-dir> <linker-dir> <cxx-compiler> <compiler-id> <cxx-standard-flag> <msvc-runtime-library>
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

# tests/harness/win_crt_dialog_suppress.hpp's own header comment: force-
# included on the MSVC compile only (see compile_fixture() below), fixes
# VERMELHO 3 (04/09/2026, job "Windows - Debug") by routing the debug
# CRT's own assert() report to stderr and clearing abort()'s dialog/WER
# flags, from INSIDE the fixture's own process - a mechanism SetErrorMode
# (already applied by apply_windows_crash_dialog_suppression() below)
# never reaches. Marker text duplicated from that header - see this
# constant's own comment there for why it is not imported structurally.
CRT_DIALOG_SUPPRESSION_HEADER = (
    ROOT_DIR / "tests" / "harness" / "win_crt_dialog_suppress.hpp"
)
CRT_DIALOG_SUPPRESSION_MARKER = "glintfx_test: CRT dialog suppression applied"

# 128 + SIGSEGV(11), this project's Linux gates already read exit status this way.
POSIX_SIGSEGV_EXIT_STATUS = 139

# 0xC0000005 (STATUS_ACCESS_VIOLATION) read as a signed 32-bit integer -
# see "THE RELEASE/VOID STRUCTURAL FAULT SHAPE DIFFERS BY PLATFORM" above.
# MEASURED live on real Windows (CI run 33833944182, GODS_LAWS.md L-44/L-49):
# subprocess.returncode surfaced this exact bit pattern read UNSIGNED
# (3221225477), not signed. Both numbers name the same 0xC0000005 - see
# normalize_windows_unsigned_exit_status() below, which folds the raw
# returncode down to this constant's signed reading before comparison, the
# same way normalize_posix_signal_exit_status() folds the POSIX native
# convention down to this file's own shell-style expectation.
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


# Straight lookup from CMake's own four MSVC_RUNTIME_LIBRARY strings
# (Modules/Platform/Windows-MSVC.cmake) to the cl.exe flag each one
# means - see this file's own CRT-LINK-WIN header comment for why this
# has to be a lookup against the library's ACTUAL build, never a
# hardcoded "/MD" guess.
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
            "usage: check_rslt_precondition.py <include-dir> <generated-include-dir> "
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
    # ndebug is the ONE variable under test - everything else is held
    # fixed between the two compiles, INCLUDING msvc_runtime_flag: the
    # CRT glintfx_library was actually linked against never changes
    # between the debug and release fixture, only whether NDEBUG is
    # defined does - see this file's own CRT-LINK-WIN header comment.
    # cxx_standard_flag (CMAKE_CXX23_STANDARD_COMPILE_OPTION, see this
    # file's own STD-FLAG-WIN header comment) replaces the literal
    # "/std:c++23"/"-std=c++23" no compiler was ever guaranteed to
    # accept verbatim.
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
            # tests/harness/win_crt_dialog_suppress.hpp's own header
            # comment: /FI is documented to have "the same effect as
            # specifying the file ... in an #include directive on the
            # first line" - no edit to precondition_fixture.cpp itself,
            # which this file's own header declares out of scope.
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
    (128 + signal) the predecessor .sh script's `$?` used and that this
    file's own POSIX_SIGSEGV_EXIT_STATUS=139 is written against - caught
    live (this fatia's own first ctest run: release/void reported -11,
    not 139, for the exact same SIGSEGV). Windows never returns a
    negative returncode for this reason (no POSIX signal delivery), so
    this is a no-op there."""
    if returncode is not None and returncode < 0:
        return 128 - returncode
    return returncode


def normalize_windows_unsigned_exit_status(returncode):
    """A Windows exit code is a 32-bit DWORD; the NT status of an unhandled
    structured exception (STATUS_ACCESS_VIOLATION, 0xC0000005) is the SAME
    bit pattern whether Python's subprocess.returncode happens to surface it
    as an unsigned reading (3221225477) or a signed one (-1073741819) - both
    denote 0xC0000005, and which one shows up is a detail of how the raw
    DWORD got converted, not a fact about the exception itself. Caught live
    (RSLT-PARITY-WIN estreia, CI run 33833944182): this file's own
    WINDOWS_ACCESS_VIOLATION_EXIT_STATUS constant was written against the
    signed reading before any Windows toolchain had run this fixture, and
    the real run returned the unsigned one instead. Fold the unsigned form
    down to the signed one so exactly one representation is compared
    anywhere in this file - the same reason normalize_posix_signal_exit_
    status() exists for the POSIX side below. No-op for any value that is
    not in the unsigned upper half (0 included), and a no-op on the four
    POSIX targets since this is only called on the MSVC branch."""
    if returncode is not None and returncode > 0x7FFFFFFF:
        return returncode - 0x100000000
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
        # Each platform gets its OWN normalization, never both stacked on the
        # same value: normalize_posix_signal_exit_status()'s negative-input
        # branch would otherwise mangle a Windows returncode that happened to
        # already be negative (the signed reading of 0xC0000005 IS negative),
        # turning a correct value into a wrong one instead of leaving it
        # alone - the same class of platform-representation bug this fatia
        # exists to fix, just a second occurrence of it in this same file.
        if is_msvc(compiler_id):
            status = normalize_windows_unsigned_exit_status(result.returncode)
        else:
            status = normalize_posix_signal_exit_status(result.returncode)
        return status, result.stdout + result.stderr
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


def assert_crt_dialog_suppression_applied(output, case_label, compiler_id):
    """SECOND, independent fact from 'the process stopped in time with
    the expected message' - see tests/harness/win_crt_dialog_suppress.
    hpp's own header comment (GODS_LAWS.md L-40, the same 'prove it ran,
    never assume it ran' discipline win_dll_alloc_hook.hpp's own
    report_patch_result() already applies). MSVC-only: the header is
    force-included on the MSVC branch of compile_fixture() only, so a
    POSIX run never has this marker to look for.
    """
    if not is_msvc(compiler_id):
        return
    if CRT_DIALOG_SUPPRESSION_MARKER not in output:
        fail(
            f"{case_label}: win_crt_dialog_suppress.hpp's own marker "
            f"('{CRT_DIALOG_SUPPRESSION_MARKER}') is missing from the captured output - the /FI "
            "force-include did not take effect, or its dynamic initialization did not run before "
            "main(), so the CRT dialog suppression this run needed may not actually have applied "
            "(VERMELHO 3, 04/09/2026)."
        )


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
    assert_crt_dialog_suppression_applied(output, f"debug/{case_arg}", compiler_id)

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
    assert_crt_dialog_suppression_applied(output, f"release/{case_arg}", compiler_id)

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
    assert_crt_dialog_suppression_applied(output, "release/void", compiler_id)

    print(
        "check_rslt_precondition.py: release/void OK (structural null-pointer fault on this platform, "
        "not a library-level guarantee)"
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

    # Resolved ONCE, used unchanged on both fixture compiles below - see
    # this file's own CRT-LINK-WIN header comment for why the debug/
    # release NDEBUG toggle must never also flip the CRT choice.
    msvc_runtime_flag = resolve_msvc_runtime_flag(msvc_runtime_library) if is_msvc(compiler_id) else None

    with tempfile.TemporaryDirectory(prefix="glintfx-rslt-precond-") as scratch:
        scratch_path = pathlib.Path(scratch)
        debug_bin = binary_path(scratch_path, "precondition_fixture_debug", compiler_id)
        release_bin = binary_path(scratch_path, "precondition_fixture_release", compiler_id)

        print("check_rslt_precondition.py: compiling debug fixture (NDEBUG undefined)")
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

        print("check_rslt_precondition.py: compiling release fixture (NDEBUG)")
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
