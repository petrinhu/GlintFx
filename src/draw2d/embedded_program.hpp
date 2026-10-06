// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>

#include "gl_abi.hpp"
#include "gl_functions.hpp"

// draw2d/embedded_program.hpp - R2D-BATCH, fatia B3b (docs/plano-w7d.md sec. 4.3, D-W7D-15,
// docs/auditoria-api-draw2d.md B0 R7/R9, PLANO-errata.md D-API-10): the ONE drawing program the
// renderer embeds, written in GLSL 330 core (the only GL this library targets, GODS_LAWS.md L-31),
// and the function that compiles and links it through an ALREADY LOADED gl_function_table.
//
// It takes the table, never a context: this file includes nothing of the operating system or of
// src/platform/ (GODS_LAWS.md L-19), so it compiles and runs on all five systems and can be
// exercised against a table of fakes. The caller (the renderer, B4) makes the context current and
// loads the table through the context's own resolver first.
//
// THE PROGRAM draws the batch of triangle_batch.hpp: position float2 in PIXELS (attribute 0),
// texture coordinate float2 (attribute 1, inert until R2D-TEXTURE), color float4 linear and
// PREMULTIPLIED (attribute 2). The vertex shader turns pixels into clip space with the one uniform
// `u_viewport_pixels` (top-left origin, y down, as the corner roles of core/quad.hpp); the fragment
// shader writes the color as it is when the surface encodes for us (the premultiplication already
// happened, once, on the CPU), and encodes it to sRGB itself when `u_encode_srgb` is 1 (the context
// option `srgb_framebuffer` off).
//
// FAILURE (docs/api-conventions.md R1, R3, R7): a gltfx_err `platform_failure` whose
// rejected_value() is one of the three TOKENS below, never a sentence. The driver's own message
// (a sentence, in the driver's language) goes to the log sink (core/log) as the field `driver_log`
// of the event `draw2d_program_rejected` (category "draw2d", severity err: the library's own
// program being refused is why the renderer cannot open). It is read into a buffer of
// k_driver_log_capacity bytes on the STACK and TRUNCATED to it - no allocation, no std::string
// (GODS_LAWS.md L-22, R3 degrau 1); a longer message is cut, never refused, and the cut is SAID:
// the event carries a second field, the boolean `driver_log_truncated`, true when the message did
// not fit (the driver's own GL_INFO_LOG_LENGTH, which counts the terminator, is larger than the
// buffer); a third field, `reason`, is a TOKEN that says why (shader_compile_failed,
// program_link_failed, create_shader_returned_zero, create_program_returned_zero), with driver_log
// EMPTY when the refusal was not the driver's own message. Every GL object created on the way is
// deleted before the function returns, on success and on failure.
namespace glintfx::draw2d {

inline constexpr std::string_view k_reject_vertex_shader = "vertex_shader";
inline constexpr std::string_view k_reject_fragment_shader = "fragment_shader";
inline constexpr std::string_view k_reject_program_link = "program_link";

inline constexpr std::string_view k_program_rejected_event = "draw2d_program_rejected";
inline constexpr std::string_view k_driver_log_field = "driver_log";
inline constexpr std::string_view k_driver_log_truncated_field = "driver_log_truncated";
// The third field of the event: WHY the program was refused, as a token (never a sentence of ours,
// R7). driver_log stays EMPTY when the refusal was not the driver's own message.
inline constexpr std::string_view k_reason_field = "reason";
inline constexpr std::string_view k_reason_shader_compile_failed = "shader_compile_failed";
inline constexpr std::string_view k_reason_program_link_failed = "program_link_failed";
inline constexpr std::string_view k_reason_create_shader_returned_zero =
    "create_shader_returned_zero";
inline constexpr std::string_view k_reason_create_program_returned_zero =
    "create_program_returned_zero";
inline constexpr std::size_t k_driver_log_capacity = 1024;

// The attribute locations the shaders declare (the vertex array of B3c/B4 binds to these).
inline constexpr std::uint32_t k_attribute_position = 0;
inline constexpr std::uint32_t k_attribute_texcoord = 1;
inline constexpr std::uint32_t k_attribute_color = 2;

// The two sources, exposed so a test can check them and a sabotaged copy can be built outside the
// tree; the compile function reads exactly these.
[[nodiscard]] std::string_view embedded_vertex_shader_source() noexcept;
[[nodiscard]] std::string_view embedded_fragment_shader_source() noexcept;

struct embedded_program {
    std::uint32_t program = 0;
    std::int32_t viewport_location = -1;    // uniform vec2 u_viewport_pixels
    std::int32_t encode_srgb_location = -1; // uniform int u_encode_srgb
};

// Compiles both shaders and links the program. On success the shaders are already deleted and
// `program` is a linked GL program; on failure nothing is left alive.
[[nodiscard]] glintfx::gltfx_rslt<embedded_program>
create_embedded_program(const render::gl_function_table &gl) noexcept;

// Deletes the program (harmless on an empty one) and empties `program`.
void destroy_embedded_program(const render::gl_function_table &gl,
                              embedded_program &program) noexcept;

} // namespace glintfx::draw2d
