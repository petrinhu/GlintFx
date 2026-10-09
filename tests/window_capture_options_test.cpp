// SPDX-License-Identifier: AGPL-3.0-or-later
#include <string>
#include <string_view>
#include <vector>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "tools/window_capture_options.hpp"

// window_capture_options_test.cpp - QA-SCREEN-CAPTURE D5a (D-W8-113, DEMO-1): the ten decided cases
// of the Windows window-capture tool's command line, proven on every system. Every expected value
// is a LITERAL written here by hand from the decision's own table, never read back from the unit
// (a test that compares the code with its own constant proves nothing).

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

// True when every field of the two options is equal: the test of "a refusal changed nothing".
bool same_fields(const tool_options &left, const tool_options &right) {
    return left.title == right.title && left.out_directory == right.out_directory &&
           left.present_budget_ms == right.present_budget_ms &&
           left.exit_budget_ms == right.exit_budget_ms &&
           left.sabotage_occlude == right.sabotage_occlude && left.ready_line == right.ready_line &&
           left.fixture_command == right.fixture_command;
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

// Case 4: --ready-line as the last argument, with no value, is refused.
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

// Case 11: a refused command line changes NOTHING in options, field by field. The options start
// with known values, and the refusal comes in the middle of the line (--out is read, then --frames
// is refused), so a parser that writes as it reads is caught here.
GLINTFX_TEST(refused_command_line_leaves_options_untouched) {
    tool_options options;
    options.title = "kept_title";
    options.out_directory = "kept_out";
    options.present_budget_ms = 1234;
    options.exit_budget_ms = 4321;
    options.sabotage_occlude = true;
    options.ready_line = "kept_line";
    options.fixture_command = {"kept_fixture"};
    const tool_options before = options;
    const args_list args{"--out", "O", "--frames", "3", "--", "f"};
    GLINTFX_CHECK(!parse_tool_options(args, options));
    GLINTFX_CHECK(same_fields(options, before));
}
