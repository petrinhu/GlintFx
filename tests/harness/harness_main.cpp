// SPDX-License-Identifier: AGPL-3.0-or-later
#include <exception>
#include <print>
#include <string>
#include <string_view>

#include "check.hpp"
#include "test_registry.hpp"

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
