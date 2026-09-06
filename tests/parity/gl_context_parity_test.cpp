// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

// gl_context_parity_test.cpp - P-GL (docs/plano-w6b-fatias-5.md sec.
// 3, D-W6b-29, GODS_LAWS.md L-04): ONE file, ONLY the public API
// (glintfx::gltfx_display/gltfx_window/gltfx_gl_context), NO #if of
// any kind - the SAME "no harness, no ctest-framework dependency"
// shape window_parity_test.cpp (one directory over) already uses, and
// the SAME P-0 mechanism (tests/tools/check_test_parity.py): the
// Windows side links this file into a normal add_executable()/add_
// test() target (tests/CMakeLists.txt, if(WIN32)), the Linux side
// stages and runs it as a container fixture of this EXACT name
// (tests/container/prepare_arch_ports_fixture.sh, Containerfile,
// .github/workflows/ci.yml's own wayland-container job) - ctest itself
// is the only thing that differs, never the source.
//
// THE CRITERION IS FIXED HERE, BEFORE THE DATA EXISTS (D-W6b-29,
// GODS_LAWS.md L-43) - three kinds of line, never confused:
//   - ASSERTED EQUAL on both systems (a genuine `gl_context_parity_
//     test: FAIL` and EXIT_FAILURE the moment it disagrees): proc_
//     address resolution, GL >= 3.3 core, the pixel readback, the
//     first-presented budget, the vsync=off 700ms budget, option_
//     support's own non-empty sweep and its three specific rows, and
//     the two reopen cases (fixation).
//   - MEASURED, PRINTED, NEVER ASSERTED EQUAL (tests/tools/collect_
//     measured.py's own token, read back by the `parity` job's own
//     check_measured_parity.py - a key with NO conceivable cross-
//     platform floor lives in tests/measured_exceptions.txt instead,
//     this fatia's own additions there): gl_major/gl_minor/renderer_
//     hash, vsync_on_60_swaps_ms, vsync_adaptive_support, msaa_
//     support/srgb_support.
//   - PLAIN DIAGNOSTIC TEXT: everything else this file prints as it
//     goes, read by a human in the CI log, never by any script.
//
// WHAT THIS FILE DOES NOT DO: call into the concrete Wayland/EGL or
// Win32/WGL adapter directly (that is egl_protocol_error_smoke.cpp's
// own job, one directory over, sec. 3.1 of the same plan) - every
// single call below goes through gltfx_display/gltfx_window/gltfx_gl_
// context, exactly the surface a real consumer links against.
//
// GL FUNCTIONS DECLARED BY HAND, <GL/gl.h> NOT LINKED (GODS_LAWS.md
// L-07, the SAME technique egl_context_adapter.cpp/egl_probe_smoke.cpp/
// win32_runner_probe_test.cpp already use, sourced against the SAME
// vendored registry src/render/gl_abi.hpp is measured against, third_
// party/khronos/gl.xml lines 127/176/970/1047/1073/1980-1981/6124):
// resolved through gltfx_gl_context::proc_address() itself - never a
// second, hidden linkage path against -lGL/opengl32.lib.

namespace {

using gl_enum = unsigned int;
using gl_int = int;
using gl_uint = unsigned int;
using gl_sizei = int;
using gl_float = float;

constexpr gl_enum k_gl_color_buffer_bit = 0x00004000;
constexpr gl_enum k_gl_context_core_profile_bit = 0x00000001;
constexpr gl_enum k_gl_rgba = 0x1908;
constexpr gl_enum k_gl_unsigned_byte = 0x1401;
constexpr gl_enum k_gl_major_version = 0x821B;
constexpr gl_enum k_gl_minor_version = 0x821C;
constexpr gl_enum k_gl_context_profile_mask = 0x9126;

using gl_get_integerv_fn = void (*)(gl_enum, gl_int *);
using gl_clear_color_fn = void (*)(gl_float, gl_float, gl_float, gl_float);
using gl_clear_fn = void (*)(gl_enum);
using gl_read_pixels_fn = void (*)(gl_int, gl_int, gl_sizei, gl_sizei, gl_enum, gl_enum, void *);
using gl_gen_vertex_arrays_fn = void (*)(gl_sizei, gl_uint *);

// FNV-1a 64 (Fowler/Noll/Vo) - a stable, dependency-free way to fold
// gpu().name (GL_RENDERER, never fixed-format, D-W6b-13's own "nunca
// interpretado por texto") into ONE MEASURED number a human can diff
// across two CI runs without reading a raw driver string full of
// slashes and spaces. Not a security hash, not vendored from anywhere
// (GODS_LAWS.md L-07) - the algorithm's own public-domain constants
// are the entire "dependency".
[[nodiscard]] std::uint64_t fnv1a64(std::string_view text) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325ULL; // offset basis
    for (unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 0x100000001b3ULL; // FNV prime
    }
    return hash;
}

[[nodiscard]] bool within_tolerance(unsigned char actual, unsigned char expected,
                                    int tolerance) noexcept {
    const int diff = static_cast<int>(actual) - static_cast<int>(expected);
    return diff >= -tolerance && diff <= tolerance;
}

} // namespace

int main() {
    // Unbuffer stdout explicitly - same fix, same reason, applied to
    // every fixture in this family (window_parity_test.cpp's own
    // header comment on this exact line: `_IOLBF` with size 0 crashed
    // the Windows CI job with 0xC0000409, `_IONBF` ignores size/buffer
    // entirely and is the one mode MSVC's own docs confirm actually
    // disables buffering on Win32).
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    glintfx::gltfx_rslt<glintfx::gltfx_display> display_opened = glintfx::gltfx_display::open();
    if (display_opened.has_error()) {
        std::fprintf(
            stderr, "gl_context_parity_test: gltfx_display::open() failed: %s\n",
            std::string(glintfx::gltfx_err_code_name(display_opened.error().code())).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_display display = std::move(display_opened.value());

    const glintfx::gltfx_window_desc desc{
        .title = "janela de paridade do contexto grafico",
        .application_id = "org.glintfx.gl_context_parity_test",
        .logical_size = {.width = 640, .height = 480},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, desc);
    if (window_opened.has_error()) {
        std::fprintf(
            stderr, "gl_context_parity_test: gltfx_window::open() failed: %s (rejected_value=%s)\n",
            std::string(glintfx::gltfx_err_code_name(window_opened.error().code())).c_str(),
            std::string(window_opened.error().rejected_value()).c_str());
        return EXIT_FAILURE;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());

    // --- First open(): every asserted/measured behaviour lives inside
    // this scope, so the context closes cleanly (RAII) before the two
    // reopen cases below ever run (D-W6b-25's own "a fixacao mora na
    // janela, nunca no contexto" - closing THIS context must not
    // disturb what it fixed).
    {
        const glintfx::gltfx_gl_context_desc empty_desc{};
        glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context_opened =
            glintfx::gltfx_gl_context::open(window, empty_desc);
        if (context_opened.has_error()) {
            std::fprintf(
                stderr, "gl_context_parity_test: gltfx_gl_context::open() failed: %s (rejected_value=%s)\n",
                std::string(glintfx::gltfx_err_code_name(context_opened.error().code())).c_str(),
                std::string(context_opened.error().rejected_value()).c_str());
            return EXIT_FAILURE;
        }
        glintfx::gltfx_gl_context context = std::move(context_opened.value());

        if (glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
            std::fprintf(stderr, "gl_context_parity_test: make_current() failed: %s\n",
                         std::string(glintfx::gltfx_err_code_name(current.error().code())).c_str());
            return EXIT_FAILURE;
        }

        // The five proc_address() names D-W6b-29 fixes: four are GL 1.1
        // (glGetIntegerv, glClearColor, glClear, glReadPixels - the
        // WGL armadilha, resolvable without any extension loader on
        // Windows) plus glGenVertexArrays (GL 3.0 core), the proof the
        // resolver actually reaches core profile entry points, never
        // just the compatibility ones every driver hands out for free.
        void *get_integerv_addr = context.proc_address("glGetIntegerv");
        void *clear_color_addr = context.proc_address("glClearColor");
        void *clear_addr = context.proc_address("glClear");
        void *read_pixels_addr = context.proc_address("glReadPixels");
        void *gen_vertex_arrays_addr = context.proc_address("glGenVertexArrays");
        if (get_integerv_addr == nullptr || clear_color_addr == nullptr || clear_addr == nullptr ||
            read_pixels_addr == nullptr || gen_vertex_arrays_addr == nullptr) {
            std::fprintf(stderr,
                         "gl_context_parity_test: proc_address() resolution failed - "
                         "glGetIntegerv=%d glClearColor=%d glClear=%d glReadPixels=%d "
                         "glGenVertexArrays=%d\n",
                         get_integerv_addr != nullptr, clear_color_addr != nullptr,
                         clear_addr != nullptr, read_pixels_addr != nullptr,
                         gen_vertex_arrays_addr != nullptr);
            return EXIT_FAILURE;
        }
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast) reason: the universal
        // dlsym-style function-to-object-pointer cast every GL loader already relies on
        // (egl_context_adapter.cpp's own comment on this exact cast, one layer below).
        const auto get_integerv = reinterpret_cast<gl_get_integerv_fn>(get_integerv_addr);
        const auto clear_color = reinterpret_cast<gl_clear_color_fn>(clear_color_addr);
        const auto clear = reinterpret_cast<gl_clear_fn>(clear_addr);
        const auto read_pixels = reinterpret_cast<gl_read_pixels_fn>(read_pixels_addr);
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
        (void)gen_vertex_arrays_addr; // resolved and checked non-null above, never called

        gl_int gl_major = 0;
        gl_int gl_minor = 0;
        gl_int gl_profile_mask = 0;
        get_integerv(k_gl_major_version, &gl_major);
        get_integerv(k_gl_minor_version, &gl_minor);
        get_integerv(k_gl_context_profile_mask, &gl_profile_mask);
        const bool core_profile = (gl_profile_mask & k_gl_context_core_profile_bit) != 0;
        if (gl_major < 3 || (gl_major == 3 && gl_minor < 3) || !core_profile) {
            std::fprintf(stderr,
                         "gl_context_parity_test: context is %d.%d (core=%d), expected >= 3.3 "
                         "core\n",
                         gl_major, gl_minor, core_profile ? 1 : 0);
            return EXIT_FAILURE;
        }
        const glintfx::gltfx_gpu_info gpu = context.gpu();
        const std::uint64_t renderer_hash = fnv1a64(gpu.name);
        std::fprintf(stdout, "MEASURED gl_context_parity_test.gl_major=%d\n", gl_major);
        std::fprintf(stdout, "MEASURED gl_context_parity_test.gl_minor=%d\n", gl_minor);
        std::fprintf(stdout, "MEASURED gl_context_parity_test.renderer_hash=%016llx\n",
                     static_cast<unsigned long long>(renderer_hash));

        // Pixel readback BEFORE any swap (D-W6b-29, sec. 6 of the onda
        // plan, verbatim: "a unica prova de pixel e a leitura de volta
        // do proprio back buffer") - (64,128,191,255), tolerance 1 per
        // channel.
        clear_color(0.25F, 0.5F, 0.75F, 1.0F);
        clear(k_gl_color_buffer_bit);
        unsigned char pixel[4] = {0, 0, 0, 0};
        read_pixels(0, 0, 1, 1, k_gl_rgba, k_gl_unsigned_byte, pixel);
        constexpr unsigned char kExpected[4] = {64, 128, 191, 255};
        if (!within_tolerance(pixel[0], kExpected[0], 1) ||
            !within_tolerance(pixel[1], kExpected[1], 1) ||
            !within_tolerance(pixel[2], kExpected[2], 1) ||
            !within_tolerance(pixel[3], kExpected[3], 1)) {
            std::fprintf(stderr,
                         "gl_context_parity_test: readback pixel (%u,%u,%u,%u), expected "
                         "(64,128,191,255) +-1\n",
                         pixel[0], pixel[1], pixel[2], pixel[3]);
            return EXIT_FAILURE;
        }

        // First `presented` within 5 attempts (vsync=on, the registry's
        // own default).
        int first_presented_attempt = 0;
        for (int attempt = 1; attempt <= 5; ++attempt) {
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
            if (swapped.has_error()) {
                std::fprintf(
                    stderr, "gl_context_parity_test: swap_buffers() attempt %d failed: %s (rejected_value=%s)\n",
                    attempt,
                    std::string(glintfx::gltfx_err_code_name(swapped.error().code())).c_str(),
                    std::string(swapped.error().rejected_value()).c_str());
                return EXIT_FAILURE;
            }
            if (swapped.value() == glintfx::gltfx_present_outcome::presented) {
                first_presented_attempt = attempt;
                break;
            }
        }
        std::fprintf(stdout, "MEASURED gl_context_parity_test.first_presented_attempt=%d\n",
                     first_presented_attempt);
        if (first_presented_attempt < 1) {
            std::fprintf(stderr,
                         "gl_context_parity_test: never reached `presented` within 5 attempts\n");
            return EXIT_FAILURE;
        }

        // T (D-W6b-27's own proof): vsync=off, 60 swaps, budget 700ms -
        // a blocked-under-the-Mesa-swap-interval-1 regression costs
        // ~1000ms for the same 60 swaps (D-W6b-29's own mutation row).
        if (glintfx::gltfx_rslt<void> set_off =
                context.set_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 0});
            set_off.has_error()) {
            std::fprintf(stderr, "gl_context_parity_test: set_option(vsync=off) failed: %s\n",
                         std::string(glintfx::gltfx_err_code_name(set_off.error().code())).c_str());
            return EXIT_FAILURE;
        }
        const auto vsync_off_start = std::chrono::steady_clock::now();
        for (int i = 0; i < 60; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
            if (swapped.has_error()) {
                std::fprintf(stderr, "gl_context_parity_test: swap_buffers() (vsync=off) failed: %s\n",
                             std::string(glintfx::gltfx_err_code_name(swapped.error().code())).c_str());
                return EXIT_FAILURE;
            }
        }
        const auto vsync_off_end = std::chrono::steady_clock::now();
        const auto vsync_off_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(vsync_off_end - vsync_off_start)
                .count();
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_off_60_swaps_ms=%lld\n",
                     static_cast<long long>(vsync_off_ms));
        if (vsync_off_ms > 700) {
            std::fprintf(stderr,
                         "gl_context_parity_test: vsync=off 60 swaps: %lld ms (orcamento 700)\n",
                         static_cast<long long>(vsync_off_ms));
            return EXIT_FAILURE;
        }

        // vsync=on, 60 swaps, printed only (no cross-platform floor,
        // tests/measured_exceptions.txt).
        if (glintfx::gltfx_rslt<void> set_on =
                context.set_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 1});
            set_on.has_error()) {
            std::fprintf(stderr, "gl_context_parity_test: set_option(vsync=on) failed: %s\n",
                         std::string(glintfx::gltfx_err_code_name(set_on.error().code())).c_str());
            return EXIT_FAILURE;
        }
        const auto vsync_on_start = std::chrono::steady_clock::now();
        for (int i = 0; i < 60; ++i) {
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped = context.swap_buffers();
            if (swapped.has_error()) {
                std::fprintf(stderr, "gl_context_parity_test: swap_buffers() (vsync=on) failed: %s\n",
                             std::string(glintfx::gltfx_err_code_name(swapped.error().code())).c_str());
                return EXIT_FAILURE;
            }
        }
        const auto vsync_on_end = std::chrono::steady_clock::now();
        const auto vsync_on_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(vsync_on_end - vsync_on_start)
                .count();
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_on_60_swaps_ms=%lld\n",
                     static_cast<long long>(vsync_on_ms));

        // vsync=adaptive: D-W6b-18 recuses it BY NAME on Wayland (no
        // EGL equivalent); WGL may accept it when the driver exposes
        // WGL_EXT_swap_control_tear - the ANSWER diverges by design and
        // is only MEASURED (tests/measured_exceptions.txt).
        glintfx::gltfx_rslt<void> set_adaptive =
            context.set_option({.id = glintfx::gltfx_gfx_option::vsync, .value = 2});
        const bool adaptive_supported = !set_adaptive.has_error();
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_adaptive_support=%d\n",
                     adaptive_supported ? 1 : 0);
        if (!adaptive_supported) {
            if (set_adaptive.error().code() != glintfx::gltfx_err_code::unsupported ||
                set_adaptive.error().rejected_value() != std::string_view{"vsync"}) {
                std::fprintf(stderr,
                             "gl_context_parity_test: set_option(vsync=adaptive) refused as %s/%s, "
                             "expected unsupported/vsync\n",
                             std::string(glintfx::gltfx_err_code_name(set_adaptive.error().code()))
                                 .c_str(),
                             std::string(set_adaptive.error().rejected_value()).c_str());
                return EXIT_FAILURE;
            }
        }

        // The Godot dor (docs/plano-w6b-fatias-5.md sec. 1): a rapid
        // off/on/off/on toggle, one real swap per step, all `ok` -
        // proves toggling vsync mid-session never wedges the adapter,
        // independent of whether `adaptive` itself was ever accepted.
        constexpr std::int64_t kToggleSequence[] = {0, 1, 0, 1};
        for (std::int64_t value : kToggleSequence) {
            if (glintfx::gltfx_rslt<void> set_toggle = context.set_option(
                    {.id = glintfx::gltfx_gfx_option::vsync, .value = value});
                set_toggle.has_error()) {
                std::fprintf(stderr,
                             "gl_context_parity_test: set_option(vsync=%lld) during the toggle "
                             "sequence failed: %s\n",
                             static_cast<long long>(value),
                             std::string(glintfx::gltfx_err_code_name(set_toggle.error().code()))
                                 .c_str());
                return EXIT_FAILURE;
            }
            if (glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped =
                    context.swap_buffers();
                swapped.has_error()) {
                std::fprintf(
                    stderr,
                    "gl_context_parity_test: swap_buffers() during the toggle sequence "
                    "(vsync=%lld) failed: %s\n",
                    static_cast<long long>(value),
                    std::string(glintfx::gltfx_err_code_name(swapped.error().code())).c_str());
                return EXIT_FAILURE;
            }
        }
        std::fprintf(stdout, "gl_context_parity_test: sequencia off/on/off/on de vsync ok\n");

        // option_support() sweep over EVERY registry row (piso de
        // varredura nao-vazia, GODS_LAWS.md L-40) - three rows carry a
        // cross-platform-asserted answer, two carry a MEASURED one.
        const std::size_t option_count = glintfx::gltfx_gfx_option_count();
        std::fprintf(stdout, "gl_context_parity_test: option_support varreu %zu linha(s)\n",
                     option_count);
        if (option_count == 0) {
            std::fprintf(stderr,
                         "gl_context_parity_test: option_support varredura vazia (GODS_LAWS.md "
                         "L-40)\n");
            return EXIT_FAILURE;
        }
        bool saw_msaa = false;
        bool saw_srgb = false;
        for (std::size_t i = 0; i < option_count; ++i) {
            const glintfx::gltfx_gfx_option_info info = glintfx::gltfx_gfx_option_at(i);
            const glintfx::gltfx_gfx_option_support support = context.option_support(info.id);
            if (info.id == glintfx::gltfx_gfx_option::auto_choice_reason ||
                info.id == glintfx::gltfx_gfx_option::power_source) {
                if (support != glintfx::gltfx_gfx_option_support::read_only_here) {
                    std::fprintf(stderr,
                                 "gl_context_parity_test: option_support(%s) is not read_only_here\n",
                                 std::string(info.name).c_str());
                    return EXIT_FAILURE;
                }
            }
            if (info.id == glintfx::gltfx_gfx_option::vsync &&
                support != glintfx::gltfx_gfx_option_support::supported) {
                std::fprintf(stderr, "gl_context_parity_test: option_support(vsync) is not supported\n");
                return EXIT_FAILURE;
            }
            if (info.id == glintfx::gltfx_gfx_option::msaa_samples) {
                saw_msaa = true;
                std::fprintf(stdout, "MEASURED gl_context_parity_test.msaa_support=%d\n",
                             support == glintfx::gltfx_gfx_option_support::supported ? 1 : 0);
            }
            if (info.id == glintfx::gltfx_gfx_option::srgb_framebuffer) {
                saw_srgb = true;
                std::fprintf(stdout, "MEASURED gl_context_parity_test.srgb_support=%d\n",
                             support == glintfx::gltfx_gfx_option_support::supported ? 1 : 0);
            }
        }
        if (!saw_msaa || !saw_srgb) {
            std::fprintf(stderr,
                         "gl_context_parity_test: registry sweep never saw msaa_samples/"
                         "srgb_framebuffer (saw_msaa=%d saw_srgb=%d)\n",
                         saw_msaa, saw_srgb);
            return EXIT_FAILURE;
        }

        std::fprintf(stdout, "gl_context_parity_test: primeiro open() - todas as asserts ok\n");
    } // context closed here

    // Reopen with the SAME (empty) list: D-W6b-25's own fixation
    // resolves an empty list to the registry's own defaults, which is
    // exactly what the FIRST open() above already fixed - accepted.
    {
        const glintfx::gltfx_gl_context_desc empty_desc{};
        glintfx::gltfx_rslt<glintfx::gltfx_gl_context> reopened =
            glintfx::gltfx_gl_context::open(window, empty_desc);
        if (reopened.has_error()) {
            std::fprintf(stderr,
                         "gl_context_parity_test: reabertura com a mesma lista falhou: %s "
                         "(rejected_value=%s), esperado sucesso\n",
                         std::string(glintfx::gltfx_err_code_name(reopened.error().code())).c_str(),
                         std::string(reopened.error().rejected_value()).c_str());
            return EXIT_FAILURE;
        }
        std::fprintf(stdout, "gl_context_parity_test: reabertura com a mesma lista aceita, como "
                             "esperado\n");
    } // closed here

    // Reopen with a DIFFERENT msaa_samples: refused with invalid_
    // argument/"msaa_samples" - fixed BEFORE support is ever asked
    // (D-W6b-29), independent of whether this executor's driver
    // actually has an MSAA config.
    {
        const glintfx::gltfx_gfx_option_entry entries[] = {
            {.id = glintfx::gltfx_gfx_option::msaa_samples, .value = 4},
        };
        const glintfx::gltfx_gl_context_desc desc_with_msaa{
            .options = entries,
            .option_count = 1,
        };
        glintfx::gltfx_rslt<glintfx::gltfx_gl_context> reopened =
            glintfx::gltfx_gl_context::open(window, desc_with_msaa);
        if (!reopened.has_error()) {
            std::fprintf(stderr,
                         "gl_context_parity_test: reabertura com msaa_samples=4 teve sucesso, "
                         "esperado invalid_argument/msaa_samples\n");
            return EXIT_FAILURE;
        }
        if (reopened.error().code() != glintfx::gltfx_err_code::invalid_argument ||
            reopened.error().rejected_value() != std::string_view{"msaa_samples"}) {
            std::fprintf(stderr,
                         "gl_context_parity_test: reabertura com msaa_samples=4 falhou como %s/%s, "
                         "esperado invalid_argument/msaa_samples\n",
                         std::string(glintfx::gltfx_err_code_name(reopened.error().code())).c_str(),
                         std::string(reopened.error().rejected_value()).c_str());
            return EXIT_FAILURE;
        }
        std::fprintf(stdout,
                     "gl_context_parity_test: reabertura com msaa_samples=4 recusada "
                     "(invalid_argument/msaa_samples), como esperado\n");
    }

    // No explicit close() call on `window`/`display` - GODS_LAWS.md
    // L-22's own RAII shape, the SAME reverse-of-creation teardown
    // order window_parity_test.cpp's own final comment already proves.
    return EXIT_SUCCESS;
}
