// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/window/window_desc_validation.hpp"

#include <glintfx/core/err_code.hpp>

#include "platform/window/utf8_validation.hpp"

namespace glintfx::platform {

gltfx_rslt<void> validate_window_text_field(std::string_view field_name,
                                            std::string_view value) noexcept {
    if (value.empty()) {
        return gltfx_rslt<void>::ok();
    }
    if (value.find('\0') != std::string_view::npos) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(field_name));
    }
    if (!is_valid_utf8(value)) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(field_name));
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> validate_window_logical_size(std::uint32_t width, std::uint32_t height) noexcept {
    if (width == 0 || height == 0) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("logical_size"));
    }
    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform
