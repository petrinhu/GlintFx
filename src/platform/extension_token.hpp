// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string_view>

// platform/extension_token.hpp - D-SRGB-2 (D-SRGB2-6), slice S1: the ONE atom that answers "is this
// extension listed?" for a space-separated extension list, by the WHOLE token.
//
// WHY ONE ATOM: a substring search is wrong in a way no driver bug reveals until the day a longer
// name appears (EGL_MESA_device_software_x is not EGL_MESA_device_software; WGL_EXT_swap_control is
// not WGL_EXT_swap_control_tear). The EGL colorspace test and the EGL device query each had their
// own copy, and the WGL side was about to get two more: the repetition is the reason to extract
// (the project's rule of three is met, CONTRACT.md sec. 6).
//
// HEADER-ONLY, NO OS HEADER: it lives at the root of platform/ (next to nul_terminated_name.hpp) so
// the Wayland and the Win32 adapters can both use it without either including from the other
// (L-19), and it runs under the same unit test on every system.

namespace glintfx::platform {

// Whether `name` is one of the space-separated tokens of `list`: the WHOLE token, never a substring
// of a longer one nor a prefix of one. Runs of spaces are skipped. A null list lists nothing, and
// an empty `name` is never listed. Allocation-free and noexcept: it is called from adapter paths
// that promise no exception crosses the public API (L-22).
[[nodiscard]] inline bool extension_token_listed(const char *list, std::string_view name) noexcept {
    if (list == nullptr || name.empty()) {
        return false;
    }
    const std::string_view all(list);
    std::size_t start = 0;
    while (start < all.size()) {
        while (start < all.size() && all[start] == ' ') {
            ++start;
        }
        std::size_t end = start;
        while (end < all.size() && all[end] != ' ') {
            ++end;
        }
        if (all.substr(start, end - start) == name) {
            return true;
        }
        start = end;
    }
    return false;
}

} // namespace glintfx::platform
