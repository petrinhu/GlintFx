// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include "draw2d/texture_2d_impl.hpp"

// draw2d/texture_release.hpp - R2D-TEXTURE, fatia T2a-fix1: the release of the GL texture object of
// one texture, apart from its making (texture_upload.hpp), because the two change for different
// reasons: the release is where the deferral of the T3 lands. Over an already loaded
// gl_function_table, so it runs against a fake context and a fake table
// (tests/texture_release_test.cpp).
namespace glintfx::draw2d {

// The log event of a release that could not make the context current
// (docs/auditoria-api-texture.md, R9 and D-W8-26): an identifier plus typed fields, never free
// text, because it leaves from a destructor. Category "draw2d", severity err. The name is the final
// one; the T3 emits the same event from the deferred release.
inline constexpr std::string_view k_texture_release_failed_event = "texture_release_failed";
// gltfx_err_code of the context's error, as an unsigned integer.
inline constexpr std::string_view k_release_error_code_field = "error_code";
// os_error_code() of the context's error, as a signed integer (0 when the system gave none).
inline constexpr std::string_view k_release_os_error_code_field = "os_error_code";

// Makes the texture's context current and deletes its GL object, then deletes the record. Never
// refused: if the context cannot be made current, the GL object goes with the context, nothing is
// deleted through it, and the event above says so (CONTRACT.md §6.4: never ignored in silence).
// Null is harmless. The record is freed in every case; that promise is proved by the LeakSanitizer
// run (tools/preci.sh --sanitizer-only), not by the plain ctest (INBOX, review N-B).
void release_texture_object(glintfx::texture_2d_impl *texture) noexcept;

} // namespace glintfx::draw2d
