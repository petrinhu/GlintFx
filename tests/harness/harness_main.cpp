// SPDX-License-Identifier: AGPL-3.0-or-later
#include <exception>
#include <print>
#include <string>
#include <string_view>

#include "check.hpp"
#include "test_registry.hpp"
#include "win_crt_dialog_suppress.hpp"

// WIN-HANG-2 (07/09/2026, run 34172428046, job "Windows - Debug", GODS_
// LAWS.md L-17/L-40/L-49): gfx_open_only_fixation_test hung the full 120s
// DART_TESTING_TIMEOUT with ZERO output - not even its own FIRST case's
// "[PASS]"/"[FAIL]" line ever printed (proved from the runner's own log,
// L-49: the fonte de dentro do processo, not a dashboard summary). That
// silence is the same shape win_crt_dialog_suppress.hpp's own header
// comment already names for VERMELHO 3 (04/09/2026): a debug-CRT assert()/
// abort() dialog blocks the ENTIRE process before any further stdout/
// stderr line is ever written, on the one Windows job (Debug) where
// assert() is not compiled out by NDEBUG.
//
// THE GEMEO THIS CLOSES (L-17): win_crt_dialog_suppress.hpp was written
// and proven for exactly ONE binary (tests/precondition_fixtures/
// precondition_fixture.cpp, force-included via /FI from check_rslt_
// precondition.py's own compile_fixture() - that binary is compiled
// standalone, outside this harness, so /FI was the only way to reach it).
// It was never wired into THIS file - the shared entry point every OTHER
// glintfx_add_test()-registered case links against (55 targets in tests/
// CMakeLists.txt, all "unit"-labeled, gfx_open_only_fixation_test among
// them) - so the fix existed for one test and not for the other 55 that
// share the exact same risk (assert() live, same job, same silent-hang
// mechanism). A plain #include here (not /FI) is enough: this TU's own
// dynamic initialization - win_crt_dialog_suppress.hpp's own g_auto_
// suppress object - still completes before ITS OWN main() below runs,
// the identical guarantee the header's comment already documents for the
// force-included case.
//
// DECLARED SCOPE (L-49): this closes the gap for every case that shares
// THIS entry point. It does not reach the 19 test executables in this
// tree that carry their OWN main() outside this file (tests/container/*,
// tests/parity/*, tests/embed*/main.cpp, tests/package/main.cpp, tests/
// raw_link/main.cpp) - extending this same mitigation to those is a
// separate, larger fatia (each has its own build/launch path) and is
// left untouched here, named rather than silently out of scope.
//
// NOT PROVEN HERE, NAMED SO NOBODY ASSUMES IT SILENTLY: this project has
// no Windows machine (GODS_LAWS.md L-04/L-44) - whether an assert() (and
// which one) actually fires inside gfx_open_only_fixation_test's own
// process was NOT identified by reading its call path (resolve_gfx_
// open_only_fixation() and every function it calls were read end to end;
// none reaches an assert() or a gltfx_rslt<T>::value()/error() misuse).
// This change does not claim to have found or fixed that root cause - it
// closes the ENVIRONMENT half GODS_LAWS.md L-17 demands regardless (a
// portao/mecanismo that dies instead of reporting is worse than one that
// is merely less convenient, L-36): the next real Windows Debug run
// either passes (the blocking call was the whole story) or fails LOUD
// with a stderr message naming the real fault, in place of today's silent
// timeout.

// harness_main.cpp - entry point of the in-house harness (FUND-1).
//
// With no argument: runs every registered case. With "--list": only
// prints the names. With an exact name: runs only that case (and
// fails if the name does not exist, instead of silently ignoring it).

namespace {

using glintfx::test::Case;

void print_case_list() {
    for (const Case &c : glintfx::test::all_cases()) {
        std::println("{}", c.name);
    }
}

// Runs c.fn(), catching whatever it throws so that ONE case never
// takes the whole harness process down with it (QA-HARNESS-ABORT,
// 27/08/2026 - see check.hpp's own header comment for the full
// rationale). The EXPECTED path is case_check_failed, thrown by
// GLINTFX_CHECK itself; the other two catches exist so that a genuinely
// UNEXPECTED throw (a real bug, not a check failure) also ends in a
// reported FAIL instead of an unhandled-exception abort - the crash
// this whole change exists to stop is not specific to case_check_
// failed, it is "anything a case body lets escape reaches main()
// unguarded".
void invoke_case_body(const Case &c) {
    try {
        c.fn();
    } catch (const glintfx::test::case_check_failed &) {
        // Expected unwind: GLINTFX_CHECK already ran record_check_
        // failure() (message printed, count incremented) before
        // throwing. ensure_check_failure_was_recorded() (check.hpp)
        // checks exactly that invariant instead of assuming it
        // silently - a no-op here in the expected case, and a
        // recorded failure if a case_check_failed ever reaches this
        // clause some OTHER way (see that function's own comment).
        // run_single_case() below reads failure_count() to decide
        // PASS/FAIL.
        glintfx::test::ensure_check_failure_was_recorded();
    } catch (const std::exception &e) {
        const std::string message =
            "case body let an unexpected exception escape: " + std::string(e.what());
        glintfx::test::record_check_failure(__FILE__, __LINE__, message);
    } catch (...) {
        glintfx::test::record_check_failure(
            __FILE__, __LINE__, "case body let an unexpected non-exception throw escape");
    }
}

bool run_single_case(const Case &c) {
    glintfx::test::reset_failure_count();
    invoke_case_body(c);
    const bool passed = glintfx::test::failure_count() == 0;
    std::println("[{}] {}", passed ? "PASS" : "FAIL", c.name);
    return passed;
}

const Case *find_case_by_name(std::string_view name) {
    for (const Case &c : glintfx::test::all_cases()) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

int run_all_cases() {
    int failures = 0;
    for (const Case &c : glintfx::test::all_cases()) {
        if (!run_single_case(c)) {
            ++failures;
        }
    }
    return failures;
}

int run_named_case(std::string_view name) {
    const Case *c = find_case_by_name(name);
    if (c == nullptr) {
        std::println(stderr, "harness: no test case named \"{}\"", name);
        return 1;
    }
    return run_single_case(*c) ? 0 : 1;
}

void print_summary(std::size_t total, int failures) {
    std::println("--- {} case(s), {} failure(s) ---", total, failures);
}

} // namespace

// The only calls below that clang-tidy's analysis considers throwing are
// std::println's internal std::format_error path (print_case_list and
// print_summary above). A std::format_error can only be thrown by a
// runtime-parsed format string, and every format string this harness ever
// calls println with is a string literal, validated at COMPILE time by
// consteval (P2216). clang-tidy's static analysis does not model that
// guarantee and flags the throw path unconditionally; there is no runtime
// input here that could make the literal "{}" pattern invalid.
// NOLINTNEXTLINE(bugprone-exception-escape) reason: literal format string, compile-time checked
int main(int argc, char **argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--list") {
        print_case_list();
        return 0;
    }

    if (argc == 2) {
        return run_named_case(argv[1]);
    }

    const int failures = run_all_cases();
    print_summary(glintfx::test::all_cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
