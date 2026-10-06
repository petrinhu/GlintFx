// SPDX-License-Identifier: AGPL-3.0-or-later
#include "win32_capture_grab.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <system_error>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "win32_capture_text.hpp"

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace glintfx::capture_tool {

namespace {

constexpr int k_class_name_capacity = 256;

HWND as_window(void *opaque) { return static_cast<HWND>(opaque); }

pixel_rect to_pixel_rect(const RECT &rect) {
    return {.left = rect.left, .top = rect.top, .right = rect.right, .bottom = rect.bottom};
}

grab_result failed(const char *api) {
    return {.ok = false,
            .detail =
                std::string(api) + " failed, GetLastError=" + std::to_string(::GetLastError())};
}

// A 32-bit top-down DIB section selected into a memory DC: the destination GDI draws into.
class memory_dib {
  public:
    memory_dib(int width, int height) : m_width(width), m_height(height) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        m_dc = ::CreateCompatibleDC(nullptr);
        if (m_dc == nullptr) {
            return;
        }
        m_bitmap = ::CreateDIBSection(m_dc, &info, DIB_RGB_COLORS, &m_bits, nullptr, 0);
        if (m_bitmap == nullptr || m_bits == nullptr) {
            return;
        }
        m_previous = ::SelectObject(m_dc, m_bitmap);
    }
    memory_dib(const memory_dib &) = delete;
    memory_dib &operator=(const memory_dib &) = delete;
    ~memory_dib() {
        if (m_previous != nullptr) {
            ::SelectObject(m_dc, m_previous);
        }
        if (m_bitmap != nullptr) {
            ::DeleteObject(m_bitmap);
        }
        if (m_dc != nullptr) {
            ::DeleteDC(m_dc);
        }
    }
    [[nodiscard]] bool ok() const { return m_previous != nullptr; }
    [[nodiscard]] HDC dc() const { return m_dc; }
    // Row `y` (top first) starts here; rows are `width * 4` bytes apart.
    [[nodiscard]] const unsigned char *row(int y) const {
        return static_cast<const unsigned char *>(m_bits) +
               static_cast<std::size_t>(y) * m_width * 4U;
    }
    [[nodiscard]] int height() const { return m_height; }

  private:
    int m_width = 0;
    int m_height = 0;
    HDC m_dc = nullptr;
    HBITMAP m_bitmap = nullptr;
    HGDIOBJ m_previous = nullptr;
    void *m_bits = nullptr;
};

// Copies the `width` x `height` block at (`left`, `top`) of the DIB into a tightly packed image.
void crop_into(const memory_dib &dib, const pixel_rect &block, captured_image &image) {
    image.width = block.right - block.left;
    image.height = block.bottom - block.top;
    image.bgrx.resize(static_cast<std::size_t>(image.width) * image.height * 4U);
    const std::size_t row_bytes = static_cast<std::size_t>(image.width) * 4U;
    for (int y = 0; y < image.height; ++y) {
        const unsigned char *source =
            dib.row(block.top + y) + static_cast<std::size_t>(block.left) * 4U;
        std::copy(source, source + row_bytes,
                  image.bgrx.begin() + static_cast<std::ptrdiff_t>(y * row_bytes));
    }
}

std::string window_class_name(HWND window) {
    if (window == nullptr) {
        return "(nenhuma)";
    }
    wchar_t name[k_class_name_capacity] = {};
    const int length = ::GetClassNameW(window, name, k_class_name_capacity);
    return length > 0 ? wide_to_utf8(std::wstring(name, static_cast<std::size_t>(length)))
                      : "(sem nome)";
}

} // namespace

pixel_rect client_rect_on_screen(void *window) {
    RECT client{};
    POINT origin{0, 0};
    ::GetClientRect(as_window(window), &client);
    ::ClientToScreen(as_window(window), &origin);
    return {.left = origin.x,
            .top = origin.y,
            .right = origin.x + client.right,
            .bottom = origin.y + client.bottom};
}

pixel_rect virtual_screen_rect() {
    const int left = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    return {.left = left,
            .top = top,
            .right = left + ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
            .bottom = top + ::GetSystemMetrics(SM_CYVIRTUALSCREEN)};
}

std::vector<occlusion_probe> probe_occlusion(void *window, const pixel_rect &client) {
    std::vector<occlusion_probe> probes;
    for (const screen_point &point : occlusion_points(client)) {
        const HWND hit = ::WindowFromPoint(POINT{point.x, point.y});
        const HWND root = hit == nullptr ? nullptr : ::GetAncestor(hit, GA_ROOT);
        probes.push_back({.point = point,
                          .owned_by_target = root != nullptr && root == as_window(window),
                          .owner_class = window_class_name(root)});
    }
    return probes;
}

grab_result grab_with_printwindow(void *window, captured_image &image) {
    RECT outer{};
    if (::GetWindowRect(as_window(window), &outer) == 0) {
        return failed("GetWindowRect");
    }
    const pixel_rect client = client_rect_on_screen(window);
    const int outer_width = outer.right - outer.left;
    const int outer_height = outer.bottom - outer.top;
    const memory_dib dib(outer_width, outer_height);
    if (outer_width <= 0 || outer_height <= 0 || !dib.ok()) {
        return failed("CreateDIBSection");
    }
    if (::PrintWindow(as_window(window), dib.dc(), PW_RENDERFULLCONTENT) == 0) {
        return failed("PrintWindow");
    }
    ::GdiFlush();
    const pixel_rect block{.left = client.left - outer.left,
                           .top = client.top - outer.top,
                           .right = client.right - outer.left,
                           .bottom = client.bottom - outer.top};
    if (block.left < 0 || block.top < 0 || block.right > outer_width ||
        block.bottom > outer_height) {
        return {.ok = false, .detail = "the client area does not fit inside the window rectangle"};
    }
    crop_into(dib, block, image);
    return {.ok = true, .detail = {}};
}

grab_result grab_with_bitblt(const pixel_rect &client, captured_image &image) {
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const memory_dib dib(width, height);
    if (width <= 0 || height <= 0 || !dib.ok()) {
        return failed("CreateDIBSection");
    }
    const HDC screen = ::GetDC(nullptr);
    if (screen == nullptr) {
        return failed("GetDC(nullptr)");
    }
    const BOOL blitted = ::BitBlt(dib.dc(), 0, 0, width, height, screen, client.left, client.top,
                                  SRCCOPY | CAPTUREBLT);
    const grab_result outcome =
        blitted != 0 ? grab_result{.ok = true, .detail = {}} : failed("BitBlt");
    ::ReleaseDC(nullptr, screen);
    if (!outcome.ok) {
        return outcome;
    }
    ::GdiFlush();
    crop_into(dib, {.left = 0, .top = 0, .right = width, .bottom = height}, image);
    return outcome;
}

grab_result write_capture_pair(const std::string &directory, const captured_image &image) {
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(utf8_to_wide(directory)), error);
    if (error) {
        return {.ok = false, .detail = "cannot create " + directory + ": " + error.message()};
    }
    const std::filesystem::path stem =
        std::filesystem::path(utf8_to_wide(directory)) / "conn1_surface1";
    std::ofstream raw(stem.wstring() + L".raw", std::ios::binary);
    raw.write(reinterpret_cast<const char *>(image.bgrx.data()),
              static_cast<std::streamsize>(image.bgrx.size()));
    std::ofstream meta(stem.wstring() + L".meta", std::ios::binary);
    meta << capture_meta_text(image.width, image.height);
    raw.flush();
    meta.flush();
    if (!raw || !meta) {
        return {.ok = false, .detail = "cannot write the capture files in " + directory};
    }
    return {.ok = true, .detail = {}};
}

} // namespace glintfx::capture_tool
