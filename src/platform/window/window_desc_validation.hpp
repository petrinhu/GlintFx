// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/core/err.hpp>

// platform/window/window_desc_validation.hpp - D-W6a-23 (docs/plano-
// w6a-janela.md): validates a window description's text fields
// (title, application_id) BEFORE either backend ever sees the bytes.
// Risk 7 of the plan's own sec. 5 table names the alternative
// ("obrigacao do chamador, nao verificada") as exactly the shape that
// lets the SAME malformed input be silently accepted on Wayland
// (xdg_toplevel_set_title just copies bytes into a wl_array) and
// refused on Win32 (MultiByteToWideChar(MB_ERR_INVALID_CHARS)) - a
// behavior divergence GODS_LAWS.md L-04 forbids by construction, not
// by discipline.
//
// Two independent reasons reject a field, both reported as
// gltfx_err_code::invalid_argument with rejected_value() carrying the
// FIELD'S NAME - never the malformed bytes themselves, which may not
// even be valid UTF-8 to begin with, so echoing them back is not
// always safely representable as text a caller could print: an
// embedded NUL byte (silently truncates one platform's C-string API
// and not the other's length-carrying one) and invalid UTF-8 (utf8_
// validation.hpp, its own atom - GODS_LAWS.md L-17). An EMPTY field is
// always accepted: v1 never requires a title or an application_id.

namespace glintfx::platform {

[[nodiscard]] gltfx_rslt<void> validate_window_text_field(std::string_view field_name,
                                                          std::string_view value) noexcept;

} // namespace glintfx::platform
