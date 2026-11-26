// SPDX-License-Identifier: AGPL-3.0-or-later
#include <string>
#include <string_view>
#include <vector>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "tools/window_capture_options.hpp"

// window_capture_options_test.cpp - QA-SCREEN-CAPTURE D5a (D-W8-113, DEMO-1), D5a-fix3 and
// D5a-fix4 (D-W8-120 to D-W8-122, D-W8-143): the thirteen decided cases of the Windows
// window-capture tool's command line, in nineteen test functions (case 7 takes four, case 11
// three, case 13 two), proven on every system. Every expected value is a LITERAL written here by
// hand from the decision's own table, never read back from the unit (a test that compares the code
// with its own constant proves nothing).

namespace {

using args_list = std::vector<std::string>;
using glintfx::capture_tool::parse_tool_options;
using glintfx::capture_tool::tool_options;

// Appends the separator and a fixture command, so each case states only the tool's own options.
args_list with_fixture(args_list before) {
    before.emplace_back("--");
    before.emplace_back("fixture.exe");
    return before;
}

// True when a valid command line carrying `flag value` is accepted.
bool budget_accepted(std::string_view flag, std::string_view value) {
    tool_options options;
    args_list before{"--title", "T", "--out", "O"};
    before.emplace_back(flag);
    before.emplace_back(value);
    return parse_tool_options(with_fixture(before), options);
}

// True when the command line is refused. Each call gets its own tool_options, so no case can see
// the state a previous case left behind.
bool refused(const args_list &args) {
    tool_options options;
    return !parse_tool_options(args, options);
}

// True when every field of the two options is equal: the test of "a refusal changed nothing". It is
// tool_options's own defaulted operator==, so a field added to the struct is compared with no edit
// here (D-W8-167).
bool same_fields(const tool_options &left, const tool_options &right) { return left == right; }

// The options before a refused command line: every field holds a known value, so a refusal that
// writes any one field is caught by same_fields. sabotage_occlude starts TRUE here, the very value
// the parser would write, so case 11c starts from the defaults to see a flag written as it is read.
tool_options known_options() {
    tool_options options;
    options.title = "kept_title";
    options.out_directory = "kept_out";
    options.present_budget_ms = 1234;
    options.exit_budget_ms = 4321;
    options.sabotage_occlude = true;
    options.ready_line = "kept_line";
    options.fixture_command = {"kept_fixture"};
    return options;
}

// True when the command line is refused AND the known options come back unchanged.
bool refusal_keeps_known_options(const args_list &args) {
    tool_options options = known_options();
    const tool_options before = options;
    return !parse_tool_options(args, options) && same_fields(options, before);
}

} // namespace

// Case 1: an option the tool does not know, before the separator, is refused.
GLINTFX_TEST(unknown_option_before_separator_is_refused) {
    tool_options options;
    const args_list args = with_fixture({"--title", "T", "--out", "O", "--frames", "3"});
    GLINTFX_CHECK(!parse_tool_options(args, options));
}

// Case 2: without --ready-line the ready line is the literal the fixture has always printed.
GLINTFX_TEST(ready_line_defaults_to_presented_at_attempt) {
    tool_options options;
    const args_list args = with_fixture({"--title", "T", "--out", "O"});
    GLINTFX_CHECK(parse_tool_options(args, options));
    GLINTFX_CHECK_EQ(options.ready_line, std::string("presented at attempt"));
}

// Case 3: --ready-line takes the whole text, spaces and colon included.
GLINTFX_TEST(ready_line_takes_the_whole_text_with_spaces_and_colon) {
    tool_options options;
    const args_list args = with_fixture(
        {"--title", "T", "--out", "O", "--ready-line", "first_window: first frame presented"});
    GLINTFX_CHECK(parse_tool_options(args, options));
    GLINTFX_CHECK_EQ(options.ready_line, std::string("first_window: first frame presented"));
}

// Case 4: --ready-line as the last argument, with no value, is refused. No separator can follow
// it, so the line is refused for the missing separator as well. Through this API a missing value
// and a missing separator both refuse the line, so this case proves the refusal, not its reason.
// The mutant that turns a missing value into "x" is EQUIVALENT: a value is missing only at the last
// token, and the separator check after the option loop of parse_tool_options refuses that line
// whatever the value is.
GLINTFX_TEST(ready_line_without_value_at_the_end_is_refused) {
    tool_options options;
    const args_list args{"--title", "T", "--out", "O", "--ready-line"};
    GLINTFX_CHECK(!parse_tool_options(args, options));
}

// Case 5: an empty ready line is refused, because an empty needle matches every line of the
// fixture.
GLINTFX_TEST(ready_line_empty_text_is_refused) {
    tool_options options;
    const args_list args = with_fixture({"--title", "T", "--out", "O", "--ready-line", ""});
    GLINTFX_CHECK(!parse_tool_options(args, options));
}

// Case 6: each required piece missing alone is refused: title, output directory, separator,
// command.
GLINTFX_TEST(missing_title_out_separator_or_command_is_refused) {
    const args_list without_title = with_fixture({"--out", "O"});
    const args_list without_out = with_fixture({"--title", "T"});
    const args_list without_separator{"--title", "T", "--out", "O"};
    const args_list without_command{"--title", "T", "--out", "O", "--"};
    GLINTFX_CHECK(refused(without_title));
    GLINTFX_CHECK(refused(without_out));
    GLINTFX_CHECK(refused(without_separator));
    GLINTFX_CHECK(refused(without_command));
}

// Case 7a: the budgets refuse 0, a negative number, 600001 and text, for both flags.
GLINTFX_TEST(budget_zero_negative_above_ceiling_or_text_is_refused) {
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", "0"));
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", "-5"));
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", "600001"));
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", "abc"));
    GLINTFX_CHECK(!budget_accepted("--exit-budget-ms", "0"));
    GLINTFX_CHECK(!budget_accepted("--exit-budget-ms", "-5"));
    GLINTFX_CHECK(!budget_accepted("--exit-budget-ms", "600001"));
    GLINTFX_CHECK(!budget_accepted("--exit-budget-ms", "abc"));
}

// Case 7b: 600000 passes, and without the options the budgets are 30000 and 10000 (literals).
GLINTFX_TEST(budget_ceiling_passes_and_defaults_are_30000_and_10000) {
    tool_options defaults;
    const args_list without_budgets = with_fixture({"--title", "T", "--out", "O"});
    GLINTFX_CHECK(parse_tool_options(without_budgets, defaults));
    GLINTFX_CHECK_EQ(defaults.present_budget_ms, 30000);
    GLINTFX_CHECK_EQ(defaults.exit_budget_ms, 10000);

    tool_options at_ceiling;
    const args_list ceiling =
        with_fixture({"--title", "T", "--out", "O", "--present-budget-ms", "600000"});
    GLINTFX_CHECK(parse_tool_options(ceiling, at_ceiling));
    GLINTFX_CHECK_EQ(at_ceiling.present_budget_ms, 600000);
    GLINTFX_CHECK(budget_accepted("--exit-budget-ms", "600000"));
}

// Case 7c: a budget is decimal digits only: a leading space, a tab, a plus sign or a trailing space
// is refused, for both flags (D-W8-122: std::from_chars, the reading the library itself uses).
GLINTFX_TEST(budget_with_space_tab_or_plus_is_refused) {
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", " 5"));
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", "\t5"));
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", "+5"));
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", "5 "));
    GLINTFX_CHECK(!budget_accepted("--exit-budget-ms", " 5"));
    GLINTFX_CHECK(!budget_accepted("--exit-budget-ms", "+5"));
}

// Case 7d: a leading zero is still decimal digits only (D-W8-143): "05" reads 5, "007" reads 7,
// and "010" reads 10 (decimal, never octal). "00" is zero, refused like "0".
GLINTFX_TEST(budget_with_a_leading_zero_reads_the_number) {
    tool_options options;
    const args_list args = with_fixture(
        {"--title", "T", "--out", "O", "--present-budget-ms", "05", "--exit-budget-ms", "007"});
    GLINTFX_CHECK(parse_tool_options(args, options));
    GLINTFX_CHECK_EQ(options.present_budget_ms, 5);
    GLINTFX_CHECK_EQ(options.exit_budget_ms, 7);

    tool_options ten;
    const args_list ten_args =
        with_fixture({"--title", "T", "--out", "O", "--present-budget-ms", "010"});
    GLINTFX_CHECK(parse_tool_options(ten_args, ten));
    GLINTFX_CHECK_EQ(ten.present_budget_ms, 10);
    GLINTFX_CHECK(!budget_accepted("--present-budget-ms", "00"));
}

// Case 8: --sabotage-occlude sets the field; without it the field stays false.
GLINTFX_TEST(sabotage_occlude_sets_the_field_only_when_present) {
    tool_options with_flag;
    const args_list present = with_fixture({"--sabotage-occlude", "--title", "T", "--out", "O"});
    GLINTFX_CHECK(parse_tool_options(present, with_flag));
    GLINTFX_CHECK(with_flag.sabotage_occlude);

    tool_options without_flag;
    const args_list absent = with_fixture({"--title", "T", "--out", "O"});
    GLINTFX_CHECK(parse_tool_options(absent, without_flag));
    GLINTFX_CHECK(!without_flag.sabotage_occlude);
}

// Case 9: after the separator, options belong to the fixture and pass through whole, unconsumed.
GLINTFX_TEST(options_after_separator_pass_through_to_the_fixture) {
    tool_options options;
    const args_list args{"--title", "T", "--out",        "O", "--", "f",
                         "--title", "X", "--ready-line", "Y"};
    const args_list expected_fixture{"f", "--title", "X", "--ready-line", "Y"};
    GLINTFX_CHECK(parse_tool_options(args, options));
    GLINTFX_CHECK_EQ(options.title, std::string("T"));
    GLINTFX_CHECK_EQ(options.ready_line, std::string("presented at attempt"));
    GLINTFX_CHECK(options.fixture_command == expected_fixture);
}

// Case 10: the tool's options are accepted in any order.
GLINTFX_TEST(options_are_accepted_in_any_order) {
    tool_options options;
    const args_list args{"--ready-line", "R", "--out", "O", "--title", "T", "--", "f"};
    GLINTFX_CHECK(parse_tool_options(args, options));
    GLINTFX_CHECK_EQ(options.ready_line, std::string("R"));
    GLINTFX_CHECK_EQ(options.out_directory, std::string("O"));
    GLINTFX_CHECK_EQ(options.title, std::string("T"));
}

// Case 11: a refusal in the middle of the line (--out is read, then --frames is refused) changes
// nothing in the known options, field by field.
GLINTFX_TEST(refused_command_line_leaves_options_untouched) {
    const args_list args{"--out", "O", "--frames", "3", "--", "f"};
    GLINTFX_CHECK(refusal_keeps_known_options(args));
}

// Case 11b: a refusal found AFTER the loop changes nothing either. Each line below gets through the
// loop and is refused by the checks that follow it (no title, no command, a budget out of range),
// so a parser that copies `parsed` into `options` before those checks is caught here.
GLINTFX_TEST(refusal_after_the_loop_leaves_options_untouched) {
    const args_list without_title{"--out", "O", "--", "f"};
    const args_list without_command{"--title", "T", "--out", "O", "--"};
    const args_list bad_budget{"--title", "T", "--out", "O", "--present-budget-ms", "0", "--", "f"};
    GLINTFX_CHECK(refusal_keeps_known_options(without_title));
    GLINTFX_CHECK(refusal_keeps_known_options(without_command));
    GLINTFX_CHECK(refusal_keeps_known_options(bad_budget));
}

// Case 11c: a refusal after a VALID present budget (the exit budget is refused) changes nothing,
// starting from the DEFAULTS: known_options() already holds sabotage_occlude = true, the value the
// parser would write, so only a default start sees the flag written as it is read.
GLINTFX_TEST(refusal_after_a_valid_budget_leaves_defaults_untouched) {
    tool_options options;
    const tool_options before = options;
    const args_list args{
        "--sabotage-occlude", "--title", "T",  "--out", "O", "--present-budget-ms", "5",
        "--exit-budget-ms",   "0",       "--", "f"};
    GLINTFX_CHECK(!parse_tool_options(args, options));
    GLINTFX_CHECK(same_fields(options, before));
}

// Case 12: an option's value is the NEXT argument whatever it is, `--` included, as getopt reads it
// (D-W8-120): only the first `--` that is not a value ends the tool's options.
GLINTFX_TEST(separator_text_is_taken_as_an_option_value) {
    tool_options titled;
    const args_list title_dashes{"--title", "--", "--out", "O", "--", "f"};
    GLINTFX_CHECK(parse_tool_options(title_dashes, titled));
    GLINTFX_CHECK_EQ(titled.title, std::string("--"));
    GLINTFX_CHECK(titled.fixture_command == args_list{"f"});

    tool_options ready;
    const args_list ready_dashes{"--ready-line", "--", "--title", "T", "--out", "O", "--", "f"};
    GLINTFX_CHECK(parse_tool_options(ready_dashes, ready));
    GLINTFX_CHECK_EQ(ready.ready_line, std::string("--"));
}

// Case 13: a repeated option keeps its LAST value (D-W8-121: POSIX guideline 11, argparse, getopt).
GLINTFX_TEST(repeated_option_keeps_the_last_value) {
    tool_options ready;
    const args_list ready_twice{"--ready-line", "A", "--ready-line", "B", "--title", "T",
                                "--out",        "O", "--",           "f"};
    GLINTFX_CHECK(parse_tool_options(ready_twice, ready));
    GLINTFX_CHECK_EQ(ready.ready_line, std::string("B"));

    tool_options titled;
    const args_list title_twice{"--title", "T1", "--title", "T2", "--out", "O", "--", "f"};
    GLINTFX_CHECK(parse_tool_options(title_twice, titled));
    GLINTFX_CHECK_EQ(titled.title, std::string("T2"));
}

// Case 13b: the same rule for the output directory and both budgets: D-W8-121 holds for EVERY
// option with a value, not only the two of case 13. An earlier value is discarded unread, even "0",
// which alone is refused (D-W8-169).
GLINTFX_TEST(repeated_out_and_budgets_keep_the_last_value) {
    tool_options out_twice;
    const args_list out_args{"--title", "T", "--out", "O1", "--out", "O2", "--", "f"};
    GLINTFX_CHECK(parse_tool_options(out_args, out_twice));
    GLINTFX_CHECK_EQ(out_twice.out_directory, std::string("O2"));

    tool_options budgets_twice;
    const args_list budget_args = with_fixture({"--title", "T", "--out", "O", "--present-budget-ms",
                                                "5", "--present-budget-ms", "7", "--exit-budget-ms",
                                                "8", "--exit-budget-ms", "9"});
    GLINTFX_CHECK(parse_tool_options(budget_args, budgets_twice));
    GLINTFX_CHECK_EQ(budgets_twice.present_budget_ms, 7);
    GLINTFX_CHECK_EQ(budgets_twice.exit_budget_ms, 9);

    tool_options zero_then_seven;
    const args_list zero_args = with_fixture(
        {"--title", "T", "--out", "O", "--present-budget-ms", "0", "--present-budget-ms", "7"});
    GLINTFX_CHECK(parse_tool_options(zero_args, zero_then_seven));
    GLINTFX_CHECK_EQ(zero_then_seven.present_budget_ms, 7);
}
