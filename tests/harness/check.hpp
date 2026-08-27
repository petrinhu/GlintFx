// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

// check.hpp - assertions of the in-house harness (FUND-1).
//
// CASE-FATAL SINCE QA-HARNESS-ABORT (27/08/2026) - this REPLACES an
// earlier "never unwind, only count" design that crashed the whole
// process, by signal, five separate times in one day. GLINTFX_CHECK
// records the failure (record_check_failure() below prints WHERE and
// WHAT), THEN throws case_check_failed to unwind the current case
// immediately.
//
// WHY: a case that writes "assert precondition P holds, THEN act as
// if it does" - e.g. GLINTFX_CHECK(r.has_error());
// GLINTFX_CHECK(r.error().code() == ...); - is not being careless. It
// is following the SAME precondition convention core/err.hpp's own
// gltfx_rslt<T>::value()/error() document for their callers: calling
// error() when has_value() is true is UNDEFINED BEHAVIOR, guarded only
// by an assert() that the RELEASE build (the only build
// tools/preci.sh's gates ever run, ESCOPO.md SS2) compiles OUT. Under
// the OLD "never unwind" design, a failing GLINTFX_CHECK(r.has_error())
// did not stop the case: execution walked straight into the next
// line's now-unguarded error(), which dereferenced a null
// std::get_if<...>(&m_storage) and the process died by signal
// (SIGSEGV) - no [FAIL] line, no failure count, nothing but a crash
// that looked, from the outside, like the harness itself was broken.
//
// WHAT THIS PRESERVES: case_check_failed unwinds ONLY the one case
// that threw it - harness_main.cpp's run_single_case() catches it (and
// any OTHER exception a case body lets escape) right there and moves
// on to the next case, so the suite-wide property this harness exists
// to give - see every case's own PASS/FAIL in one run, never stop the
// whole suite at the first red - is UNCHANGED. A second, unrelated
// GLINTFX_CHECK failure in a DIFFERENT case is still reported on its
// own, independently.
//
// WHAT THIS COSTS, NAMED SO NOBODY REDISCOVERS IT BY SURPRISE: a case
// that piles up SEVERAL independent GLINTFX_CHECK calls in a row (four
// field comparisons, or a loop asserting the same shape over many
// items) used to report every failing one in a single run; now the
// FIRST failure in that case stops the rest of ITS OWN body - a second
// failing check three lines later in the SAME case never gets a
// chance to run. Accepted on purpose: a case that only ever fails
// harmlessly-independent checks pays a strictly cheaper cost (rerun,
// see the next one) than a case that ever crashes the process pays
// (Section "L-40" of GODS_LAWS.md: a portal/gate that dies instead of
// reporting is worse than one that is merely less convenient).

namespace glintfx::test {

// Thrown by GLINTFX_CHECK on failure, to unwind ONLY the current test
// case. Deliberately NOT derived from std::exception: this is a
// control-flow signal internal to this harness, not a runtime error a
// caller might reasonably want to inspect or recover from by catching
// std::exception - keeping it a distinct type means a future
// std::exception-catching helper elsewhere in a test file cannot
// swallow it by accident and mask the very failure it exists to
// report.
struct case_check_failed {};

void record_check_failure(std::string_view file, int line, std::string_view expr);
[[nodiscard]] int failure_count();
void reset_failure_count();

} // namespace glintfx::test

#define GLINTFX_CHECK(cond)                                                                        \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            ::glintfx::test::record_check_failure(__FILE__, __LINE__, #cond);                      \
            throw ::glintfx::test::case_check_failed{};                                            \
        }                                                                                          \
    } while (false)

#define GLINTFX_CHECK_EQ(a, b) GLINTFX_CHECK((a) == (b))
