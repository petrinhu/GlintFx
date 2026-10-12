// SPDX-License-Identifier: AGPL-3.0-or-later
#include "draw2d/texture_release.hpp"

#include <array>
#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>

#include "core/log/emit.hpp"
#include "gl_functions.hpp"

// draw2d/texture_release.cpp - R2D-TEXTURE, fatia T2a-fix1 and T2a-fix2: the release of one
// texture's GL object and its record. The release makes the texture's context current before it
// deletes the GL object. If that context cannot be made current, nothing is deleted through it (the
// object goes with the context), and the loss is reported, never silent (CONTRACT.md §6.4). The
// record is freed in every case.
namespace glintfx::draw2d {

namespace {

using glintfx::gltfx_log_field;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_value;
using glintfx::log_emit_fields;

// One event for one lost object: the context's error, as a typed code and as its system code.
void report_release_failure(const glintfx::gltfx_err &error) noexcept {
    const std::array<gltfx_log_field, 2> fields = {
        gltfx_log_field{k_release_error_code_field, gltfx_log_value::make_unsigned_integer(
                                                        static_cast<std::uint64_t>(error.code()))},
        gltfx_log_field{k_release_os_error_code_field,
                        gltfx_log_value::make_signed_integer(error.os_error_code())},
    };
    log_emit_fields(gltfx_log_severity::err, "draw2d", k_texture_release_failed_event, fields);
}

} // namespace

void release_texture_object(glintfx::texture_2d_impl *texture) noexcept {
    if (texture == nullptr) {
        return;
    }
    const glintfx::gltfx_rslt<void> current = texture->access.make_current(texture->access.context);
    if (current.has_value()) {
        const render::GLuint name = texture->gl_name;
        texture->access.gl->glDeleteTextures(1, &name);
    } else {
        report_release_failure(current.err());
    }
    delete texture;
}

} // namespace glintfx::draw2d
