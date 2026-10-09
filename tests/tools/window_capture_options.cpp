// SPDX-License-Identifier: AGPL-3.0-or-later
#include "window_capture_options.hpp"

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

// window_capture_options.cpp - QA-SCREEN-CAPTURE D5a (D-W8-113, DEMO-1): the argument parsing of
// the Windows window-capture tool, as pure functions. See window_capture_options.hpp for the
// contract. Everything here is the rule of the command line; nothing here opens a window or a file.

namespace glintfx::capture_tool {
namespace {

constexpr int k_budget_ceiling_ms = 600000;

// Reads a whole number of milliseconds in (0, ceiling]; false for text, zero, a sign or overflow.
bool read_positive_int(const char *text, int &value) {
    char *end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed <= 0 || parsed > k_budget_ceiling_ms) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

// Takes the value that follows option `name` at args[index]; false when args[index] is not `name`.
bool take_option(const std::vector<std::string> &args, std::size_t &index, std::string_view name,
                 std::string &value) {
    if (args[index] != name) {
        return false;
    }
    value = index + 1 < args.size() ? args[index + 1] : std::string();
    index += 2;
    return true;
}

// Takes --ready-line and its text into `parsed`. An empty text is refused: an empty needle matches
// every line the fixture prints, and the tool would declare the first frame ready before it is.
bool take_ready_line(const std::vector<std::string> &args, std::size_t &index,
                     tool_options &parsed) {
    std::string value;
    if (!take_option(args, index, "--ready-line", value)) {
        return false;
    }
    if (value.empty()) {
        return false;
    }
    parsed.ready_line = value;
    return true;
}

} // namespace

bool parse_tool_options(const std::vector<std::string> &args, tool_options &options) {
    tool_options parsed;
    std::size_t index = 0;
    std::string present_text = std::to_string(k_default_present_budget_ms);
    std::string exit_text = std::to_string(k_default_exit_budget_ms);
    while (index < args.size() && args[index] != "--") {
        if (args[index] == "--sabotage-occlude") {
            parsed.sabotage_occlude = true;
            ++index;
            continue;
        }
        const bool known = take_option(args, index, "--title", parsed.title) ||
                           take_option(args, index, "--out", parsed.out_directory) ||
                           take_option(args, index, "--present-budget-ms", present_text) ||
                           take_option(args, index, "--exit-budget-ms", exit_text) ||
                           take_ready_line(args, index, parsed);
        if (!known) {
            return false;
        }
    }
    if (index >= args.size() || parsed.title.empty() || parsed.out_directory.empty()) {
        return false;
    }
    using difference_type = std::vector<std::string>::difference_type;
    parsed.fixture_command.assign(args.begin() + static_cast<difference_type>(index) + 1,
                                  args.end());
    if (parsed.fixture_command.empty() ||
        !read_positive_int(present_text.c_str(), parsed.present_budget_ms) ||
        !read_positive_int(exit_text.c_str(), parsed.exit_budget_ms)) {
        return false;
    }
    options = parsed; // only a whole, accepted command line reaches the caller's options
    return true;
}

} // namespace glintfx::capture_tool
