// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>

// win32_capture_text.hpp - QA-SCREEN-CAPTURE C2b-2: UTF-8 <-> UTF-16 for the Windows capture tool
// (window titles, the child's command line). Test tooling, not product code. Windows only.

namespace glintfx::capture_tool {

// An empty result for an input that is not valid UTF-8 (never a guess).
[[nodiscard]] std::wstring utf8_to_wide(const std::string &text);
[[nodiscard]] std::string wide_to_utf8(const std::wstring &text);

} // namespace glintfx::capture_tool
