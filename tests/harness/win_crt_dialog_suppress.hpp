// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// win_crt_dialog_suppress.hpp - VERMELHO 3 (04/09/2026, run 33910349281,
// job "Windows - Debug", GODS_LAWS.md L-04/L-22/L-40): rslt_precondition_
// test hung 60s on debug/primary and was killed by check_rslt_
// precondition.py's own FIXTURE_TIMEOUT_SECONDS - exactly the risk that
// file's own header comment ("THE WINDOWS INTERACTIVE-DIALOG RISK, NOT
// MEASURED HERE") already declared unmeasured without a Windows
// toolchain. This is that measurement, and the fix.
//
// TWO SEPARATE MECHANISMS, per Microsoft's own current documentation
// (fetched 04/09/2026) - suppressing one does NOT suppress the other:
//
//   1. Windows Error Reporting, for a genuinely UNHANDLED exception (the
//      release/void access-violation case check_rslt_precondition.py's
//      own normalize_windows_unsigned_exit_status() already measures).
//      SetErrorMode(SEM_NOGPFAULTERRORBOX | ...) - learn.microsoft.com/
//      windows/win32/api/errhandlingapi/nf-errhandlingapi-seterrormode -
//      suppresses this one, and was already applied, process-wide,
//      before this header existed (check_rslt_precondition.py's own
//      apply_windows_crash_dialog_suppression(), called in the LAUNCHER
//      process before spawning the fixture - SetErrorMode's flags are
//      documented as inherited by child processes).
//
//   2. The DEBUG CRT's own assert()/_CrtDbgReport() reporting - a
//      COMPLETELY SEPARATE code path that never goes through Windows
//      Error Reporting at all. learn.microsoft.com/cpp/c-runtime-
//      library/reference/crtsetreportmode: "If you don't call
//      _CrtSetReportMode ... Assertion failures and errors are directed
//      to a debug message window" - _CRTDBG_MODE_WNDW is the documented
//      DEFAULT, an Abort/Retry/Ignore message box, entirely outside
//      SetErrorMode's reach. This is exactly the assert() call this
//      project's precondition guard fires (include/glintfx/core/
//      err.hpp), and exactly why the DEBUG job specifically (not
//      Release, which never compiles that assert() in) is the one that
//      hung. learn.microsoft.com/cpp/c-runtime-library/reference/abort
//      ("abort", Microsoft Specific section) documents a SECOND,
//      independent risk right after: abort() (which assert() calls
//      after writing its report) prints its own "R6010 - abort() has
//      been called" text and, unless _set_abort_behavior() clears
//      _CALL_REPORTFAULT, invokes Windows Error Reporting a SECOND time
//      on top of #1 above.
//
// WHY A FORCE-INCLUDED HEADER, NOT AN EDIT TO precondition_fixture.cpp:
// check_rslt_precondition.py's own header comment declares that fixture
// source OUT OF SCOPE for this file's ORDEM DE SERVICO ("read here,
// never edited"). _CrtSetReportMode()/_set_abort_behavior() are per-
// PROCESS CRT state - they must run INSIDE the fixture's own process,
// before its first assert() fires, so (unlike SetErrorMode) they cannot
// be applied externally from the Python launcher. MSVC's own /FI
// compiler option (learn.microsoft.com/cpp/build/reference/fi-name-
// forced-include-file) is documented to have "the same effect as
// specifying the file with double quotation marks in an #include
// directive on the first line of every source file" -
// check_rslt_precondition.py's own compile_fixture() passes this header
// via /FI on the MSVC branch only, so the fixture's OWN translation
// unit gets it without a single line of precondition_fixture.cpp
// changing. The suppression call runs from this header's own namespace-
// scope object's constructor: C++ dynamic initialization of every non-
// local static-storage object in a translation unit completes before
// that translation unit's own main() begins - the same "runs before
// main" guarantee the self-registering-test idiom already relies on
// elsewhere in this project's own harness (tests/harness/test_
// registry.hpp).
//
// PROVES IT RAN, NEVER ASSUMED (GODS_LAWS.md L-40, same discipline
// win_dll_alloc_hook.hpp's own report_patch_result() already applies):
// suppress_crt_dialogs() prints ONE unconditional marker line to stderr
// every time it runs - check_rslt_precondition.py's own assertions
// require that marker in EVERY MSVC run's captured output, as a SECOND,
// independent fact from "the process stopped within the timeout with
// the expected message". A run where the header failed to force-
// include, or where dynamic initialization did not happen before main
// (see the guarantee above), fails LOUD on the missing marker instead
// of silently depending on a suppression that may or may not have
// executed.
//
// MSVC-ONLY, NO-OP EVERYWHERE ELSE: guarded by _MSC_VER, so this header
// is inert if ever included on a non-MSVC compiler by mistake - the
// four POSIX targets never reach it at all (compile_fixture()'s /FI is
// on the MSVC branch only).
//
// DECLARED SCOPE OF WHAT WAS ACTUALLY PROVEN (GODS_LAWS.md L-04/L-44):
// this project has no Windows machine. Both mechanisms above are FACT,
// sourced from Microsoft's own current documentation (URLs in this
// comment) - NOT independently verified against this exact toolset by
// running it here. check_rslt_precondition.py's own FIXTURE_TIMEOUT_
// SECONDS is unchanged: if this mitigation is still incomplete, the
// next real Windows run fails loud with the same 60s timeout
// diagnostic, never hangs silently - the same declared downgrade win_
// dll_alloc_hook.hpp's own header already carries for its own Windows-
// only half.

#if defined(_MSC_VER)

#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>

namespace glintfx_test {

// Read by check_rslt_precondition.py's own assertions (the marker text
// itself is duplicated there, not imported - the two files have no
// shared build step to import through, the same reason win_dll_alloc_
// hook.hpp's own messages are matched by substring from Python, never
// parsed structurally).
inline constexpr const char *k_crt_dialog_suppression_marker =
    "glintfx_test: CRT dialog suppression applied";

namespace detail {

inline void suppress_crt_dialogs() noexcept {
    // Mechanism 2 above: _CRT_ASSERT/_CRT_ERROR reports go to stderr
    // instead of a message box (default mode, absent this call, is
    // _CRTDBG_MODE_WNDW).
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);

    // abort()'s OWN two flags: clearing _WRITE_ABORT_MSG silences its
    // "R6010 - abort() has been called" text, clearing _CALL_REPORTFAULT
    // stops it invoking Windows Error Reporting a second time on top of
    // SetErrorMode(SEM_NOGPFAULTERRORBOX) above.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    // GODS_LAWS.md L-40: prints, unconditionally, on every run that
    // links this header in - the fact this mechanism ran at all, never
    // assumed silently by the Python side that checks for it.
    std::fprintf(stderr, "%s\n", k_crt_dialog_suppression_marker);
}

// Runs before this translation unit's main() - see this file's own
// header comment for the guarantee this relies on.
struct auto_suppress {
    auto_suppress() noexcept { suppress_crt_dialogs(); }
};
inline auto_suppress g_auto_suppress{};

} // namespace detail

} // namespace glintfx_test

#endif // defined(_MSC_VER)
