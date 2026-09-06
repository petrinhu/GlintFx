// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
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

// WINDOW-SIZE-REFUSE (docs/plano-w6a-janela.md, WL-WINDOW-HANDLE):
// refuses a logical size where EITHER dimension is zero, with
// rejected_value() == "logical_size" - called once, here, by the
// public facade (gltfx_window::open(), window_facade.cpp) BEFORE
// either backend ever sees the request, the SAME "validated once,
// common to both backends" discipline validate_window_text_field()
// above already gives title/application_id. This is what makes the
// refusal identical on every platform BY CONSTRUCTION rather than by
// each backend separately choosing to reject it (GODS_LAWS.md L-04):
// neither wayland_window_adapter nor win32_window_adapter is ever
// reached with a zero dimension through this call path again. See
// include/glintfx/platform/window/window.hpp's own "WHAT THIS FATIA
// FREEZES" item 5 for why zero means refusal, never "the system
// chooses" - measured, this same fatia, against a real kwin_wayland
// --virtual compositor: it echoes a requested zero straight back
// rather than substituting anything (window_parity_test.cpp's own
// header comment carries the measurement).
[[nodiscard]] gltfx_rslt<void> validate_window_logical_size(std::uint32_t width,
                                                            std::uint32_t height) noexcept;

} // namespace glintfx::platform
