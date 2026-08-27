// SPDX-License-Identifier: AGPL-3.0-or-later
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// harness_check_invariant_test.cpp - proves
// glintfx::test::ensure_check_failure_was_recorded() (check.hpp), the
// function harness_main.cpp's catch (const case_check_failed &) clause
// now calls instead of doing nothing.
//
// WHY THIS EXISTS: clang-tidy's bugprone-empty-catch flagged that
// catch clause (tools/preci.sh's lint stage, 27/08/2026) because it
// really was empty - the comment above it explained the design intent
// (GLINTFX_CHECK already recorded the failure before throwing, so
// there is "nothing left to do"), but nothing in the code PROVED that
// intent held. ensure_check_failure_was_recorded() turns the assumed
// invariant into a checked one: if a case_check_failed ever reaches
// the catch clause WITHOUT record_check_failure() having run first
// (a case body throwing case_check_failed directly, bypassing
// GLINTFX_CHECK, or a future refactor of the macro that reorders
// record/throw), it now itself calls record_check_failure() - turning
// what would have been a silent, wrong PASS (run_single_case() reads
// failure_count() == 0 and reports the case as passing despite having
// thrown) into a loud, reported FAIL instead.
//
// SCOPE, NAMED SO IT IS NOT MISTAKEN FOR MORE THAN IT IS: these two
// cases unit-test ensure_check_failure_was_recorded() directly, the
// exact function the catch clause now delegates to - they do not spawn
// a harness process and inspect its PASS/FAIL/exit code end to end.
// That is enough here: the catch clause's own body is a one-line call
// to this function (see harness_main.cpp), so proving the function's
// two branches (missing record -> flagged; already recorded -> not
// double-counted) proves the catch clause's whole behavior by
// construction, without needing a subprocess harness of its own.
//
// EACH case below manipulates the SAME process-global failure counter
// run_single_case() also reads to decide THIS case's own verdict
// (check.cpp's g_failure_count has no other storage). Both cases
// therefore capture the counter's value into a local, reset the
// counter back to zero, and ONLY THEN assert on the captured value -
// so the probe's own side effect never leaks into (and never forges)
// this case's own PASS/FAIL.

GLINTFX_TEST(ensure_check_failure_was_recorded_flags_a_missing_record) {
    glintfx::test::reset_failure_count();

    // Simulate a case_check_failed reaching the catch clause WITHOUT
    // GLINTFX_CHECK's record_check_failure() call having run first -
    // the exact violation this function exists to turn into a loud
    // FAIL instead of a silent PASS.
    glintfx::test::ensure_check_failure_was_recorded();

    const int after_missing_record = glintfx::test::failure_count();
    glintfx::test::reset_failure_count();

    GLINTFX_CHECK_EQ(after_missing_record, 1);
}

GLINTFX_TEST(ensure_check_failure_was_recorded_does_not_double_count) {
    glintfx::test::reset_failure_count();

    // Simulate the NORMAL path: GLINTFX_CHECK already called
    // record_check_failure() before throwing, so the invariant holds
    // by the time the catch clause (and this function) runs.
    glintfx::test::record_check_failure(__FILE__, __LINE__, "synthetic, for this test only");
    glintfx::test::ensure_check_failure_was_recorded();

    const int after_already_recorded = glintfx::test::failure_count();
    glintfx::test::reset_failure_count();

    // Must stay 1, not 2: when the invariant already holds, this
    // function has nothing to add - a second recorded failure here
    // would mean it always fires, silencer-style, instead of actually
    // checking the invariant.
    GLINTFX_CHECK_EQ(after_already_recorded, 1);
}
