// SPDX-License-Identifier: AGPL-3.0-or-later
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
//     support/srgb_support, open_us, and - GL-GPU-KIND, docs/plano-
//     w6b-fatias-5b-revisao.md sec. 4.6 - gpu_kind_raw/gpu_name_hash/
//     gpu_enumeration_index_raw/gpu_enumeration_count/gpu_entry_{0,1,
//     2}_kind/gpu_entry_{0,1,2}_name_hash. `gpu_kind_raw` (and the
//     enumeration_index/entry keys with it) is DELIBERATELY NOT
//     ASSERTED EQUAL YET (sec. 4.6's own correction): the assertion
//     `gpu().kind == software` on both sides enters in the commit
//     AFTER the first real run has printed these values, never before
//     - only `gpu_enumeration_count >= 1` is asserted today (GODS_
//     LAWS.md L-40's own non-empty floor, safe regardless of what
//     `kind` resolves to).
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

// PRESENT-EMPTY-FRAME (D-W8-55/56, context.hpp item 2): a swap_buffers() that reaches the driver
// with nothing drawn since the previous presentation may be refused, so EVERY swap below follows
// a draw. A clear is enough.
[[nodiscard]] glintfx::gltfx_rslt<glintfx::gltfx_present_outcome>
draw_then_swap(glintfx::gltfx_gl_context &context, gl_clear_fn clear) {
    clear(k_gl_color_buffer_bit);
    return context.swap_buffers();
}

} // namespace

namespace {

// D-SRGB2-12: the live cell of the `open_only` contract. A request for an open_only option on a NEW
// window (the fixation of D-W6b-25 forbids reusing one) ends in exactly one of two ways: it OPENS,
// and then option_support(id) is `supported` and option(id) equals the request; or it is REFUSED
// with code `unsupported` and rejected_value equal to the NAME of the option. Anything else
// reproves. Prints MEASURED gl_context_parity_test.open_only_<name>_opened=<0|1>.
[[nodiscard]] bool open_only_cell(glintfx::gltfx_display &display, glintfx::gltfx_gfx_option id,
                                  std::int64_t value) {
    const std::string name(glintfx::gltfx_gfx_option_describe(id).name);
    const glintfx::gltfx_window_desc window_desc{
        .title = "janela da celula open_only",
        .application_id = "org.glintfx.gl_context_parity_test",
        .logical_size = {.width = 320, .height = 240},
    };
    glintfx::gltfx_rslt<glintfx::gltfx_window> window_opened =
        glintfx::gltfx_window::open(display, window_desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "gl_context_parity_test: open_only(%s): gltfx_window::open failed\n",
                     name.c_str());
        return false;
    }
    glintfx::gltfx_window window = std::move(window_opened.value());
    const glintfx::gltfx_gfx_option_entry entries[] = {{.id = id, .value = value}};
    const glintfx::gltfx_gl_context_desc context_desc{.options = entries, .option_count = 1};
    glintfx::gltfx_rslt<glintfx::gltfx_gl_context> opened =
        glintfx::gltfx_gl_context::open(window, context_desc);
    if (opened.has_error()) {
        const bool named = opened.err().code() == glintfx::gltfx_err_code::unsupported &&
                           opened.err().rejected_value() == std::string_view{name};
        std::fprintf(stdout, "MEASURED gl_context_parity_test.open_only_%s_opened=0\n",
                     name.c_str());
        if (!named) {
            std::fprintf(stderr,
                         "gl_context_parity_test: open_only(%s) recusou como %s/%s, esperado "
                         "unsupported/%s\n",
                         name.c_str(),
                         std::string(glintfx::gltfx_err_code_name(opened.err().code())).c_str(),
                         std::string(opened.err().rejected_value()).c_str(), name.c_str());
        }
        return named;
    }
    std::fprintf(stdout, "MEASURED gl_context_parity_test.open_only_%s_opened=1\n", name.c_str());
    const glintfx::gltfx_gl_context &context = opened.value();
    const glintfx::gltfx_rslt<std::int64_t> confirmed = context.option(id);
    const bool supported =
        context.option_support(id) == glintfx::gltfx_gfx_option_support::supported;
    const bool equal = confirmed.has_value() && confirmed.value() == value;
    if (!supported || !equal) {
        std::fprintf(stderr,
                     "gl_context_parity_test: open_only(%s) abriu mas supported=%d "
                     "option_igual_ao_pedido=%d (pedido %lld)\n",
                     name.c_str(), supported ? 1 : 0, equal ? 1 : 0, static_cast<long long>(value));
        return false;
    }
    return true;
}

} // namespace

namespace {

// GFX-PRESET, P4 (docs/auditoria-api-gfx-preset.md, D-A68, /var/tmp/cto-w7d/plano-fechamento-w7d.md
// sec. 2.3): the live cells of the suggested-preset contract, through the PUBLIC API only, on a
// real context. Each helper is one cell and returns false the moment it disagrees (EXIT_FAILURE in
// main).

void print_option_read_failure(std::string_view what, glintfx::gltfx_gfx_option id,
                               const glintfx::gltfx_err &err) {
    std::fprintf(stderr, "gl_context_parity_test: %s: option(%s) failed: %s\n",
                 std::string(what).c_str(),
                 std::string(glintfx::gltfx_gfx_option_describe(id).name).c_str(),
                 std::string(glintfx::gltfx_err_code_name(err.code())).c_str());
}

[[nodiscard]] bool read_option(const glintfx::gltfx_gl_context &context,
                               glintfx::gltfx_gfx_option id, std::string_view what,
                               std::int64_t &value) {
    const glintfx::gltfx_rslt<std::int64_t> read = context.option(id);
    if (read.has_error()) {
        print_option_read_failure(what, id, read.err());
        return false;
    }
    value = read.value();
    return true;
}

// Every row of the registry, read through option(), in registry order. The photo the
// "asking does not write" cell compares before and after.
[[nodiscard]] bool photograph_every_row(const glintfx::gltfx_gl_context &context,
                                        std::vector<std::int64_t> &photo) {
    photo.clear();
    const std::size_t row_count = glintfx::gltfx_gfx_option_count();
    for (std::size_t i = 0; i < row_count; ++i) {
        std::int64_t value = 0;
        if (!read_option(context, glintfx::gltfx_gfx_option_at(i).id, "foto das linhas", value)) {
            return false;
        }
        photo.push_back(value);
    }
    return row_count != 0;
}

// The rows of the preset `preset`, read as the public pair, must all read back as applied.
[[nodiscard]] bool preset_rows_read_back(const glintfx::gltfx_gl_context &context,
                                         std::int64_t preset, std::string_view what) {
    const std::size_t row_count = glintfx::gltfx_gfx_preset_row_count(preset);
    if (row_count == 0) {
        std::fprintf(stderr, "gl_context_parity_test: %s: o preset %lld nao tem linha nenhuma\n",
                     std::string(what).c_str(), static_cast<long long>(preset));
        return false;
    }
    for (std::size_t i = 0; i < row_count; ++i) {
        const glintfx::gltfx_gfx_option_entry row = glintfx::gltfx_gfx_preset_row_at(preset, i);
        std::int64_t read = 0;
        if (!read_option(context, row.id, what, read)) {
            return false;
        }
        if (read != row.value) {
            std::fprintf(stderr,
                         "gl_context_parity_test: %s: linha %s do preset %lld: esperado %lld, "
                         "lido %lld\n",
                         std::string(what).c_str(),
                         std::string(glintfx::gltfx_gfx_option_describe(row.id).name).c_str(),
                         static_cast<long long>(preset), static_cast<long long>(row.value),
                         static_cast<long long>(read));
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool set_option_ok(glintfx::gltfx_gl_context &context,
                                 glintfx::gltfx_gfx_option_entry entry, std::string_view what) {
    const glintfx::gltfx_rslt<void> set = context.set_option(entry);
    if (set.has_error()) {
        std::fprintf(stderr, "gl_context_parity_test: %s: set_option(%s=%lld) failed: %s\n",
                     std::string(what).c_str(),
                     std::string(glintfx::gltfx_gfx_option_describe(entry.id).name).c_str(),
                     static_cast<long long>(entry.value),
                     std::string(glintfx::gltfx_err_code_name(set.err().code())).c_str());
        return false;
    }
    return true;
}

// The three computed rows, MEASURED raw (one integer per key, never asserted equal between the two
// systems: the numbers are the system's own report) and held to the closed vocabulary the header
// promises - the suggestion is never manual/automatic, the reason is never `none`.
[[nodiscard]] bool measure_suggestion_keys(const glintfx::gltfx_gl_context &context) {
    std::int64_t power = 0;
    std::int64_t reason = 0;
    std::int64_t suggested = 0;
    if (!read_option(context, glintfx::gltfx_gfx_option::power_source, "power_source", power) ||
        !read_option(context, glintfx::gltfx_gfx_option::auto_choice_reason, "auto_choice_reason",
                     reason) ||
        !read_option(context, glintfx::gltfx_gfx_option::suggested_preset, "suggested_preset",
                     suggested)) {
        return false;
    }
    std::fprintf(stdout, "MEASURED gl_context_parity_test.power_source=%lld\n",
                 static_cast<long long>(power));
    std::fprintf(stdout, "MEASURED gl_context_parity_test.auto_choice_reason=%lld\n",
                 static_cast<long long>(reason));
    std::fprintf(stdout, "MEASURED gl_context_parity_test.suggested_preset=%lld\n",
                 static_cast<long long>(suggested));
    const bool power_in_vocabulary = power >= glintfx::k_gltfx_power_source_unknown &&
                                     power <= glintfx::k_gltfx_power_source_battery;
    const bool reason_in_vocabulary = reason >= glintfx::k_gltfx_auto_choice_reason_on_battery &&
                                      reason <= glintfx::k_gltfx_auto_choice_reason_unknown_gpu;
    const bool suggestion_concrete = suggested >= glintfx::k_gltfx_preset_power_saving &&
                                     suggested <= glintfx::k_gltfx_preset_performance;
    if (!power_in_vocabulary || !reason_in_vocabulary || !suggestion_concrete) {
        std::fprintf(stderr,
                     "gl_context_parity_test: sugestao fora do vocabulario fechado: "
                     "power_source=%lld auto_choice_reason=%lld suggested_preset=%lld\n",
                     static_cast<long long>(power), static_cast<long long>(reason),
                     static_cast<long long>(suggested));
        return false;
    }
    return true;
}

// CELL "perguntar nao grava" (rule 1 of the SUGGESTED PRESET block): the label is `manual` and the
// suggestion is always 1..3, so a library that wrote the suggestion anywhere would change a row.
[[nodiscard]] bool asking_writes_nothing_cell(glintfx::gltfx_gl_context &context) {
    if (!set_option_ok(
            context,
            {.id = glintfx::gltfx_gfx_option::preset, .value = glintfx::k_gltfx_preset_manual},
            "perguntar nao grava")) {
        return false;
    }
    std::vector<std::int64_t> before;
    std::vector<std::int64_t> after;
    if (!photograph_every_row(context, before)) {
        return false;
    }
    std::int64_t ignored = 0;
    for (int ask = 0; ask < 2; ++ask) {
        if (!read_option(context, glintfx::gltfx_gfx_option::suggested_preset, "perguntar",
                         ignored) ||
            !read_option(context, glintfx::gltfx_gfx_option::auto_choice_reason, "perguntar",
                         ignored)) {
            return false;
        }
    }
    if (!photograph_every_row(context, after) || before.size() != after.size()) {
        return false;
    }
    for (std::size_t i = 0; i < before.size(); ++i) {
        if (before[i] != after[i]) {
            std::fprintf(stderr,
                         "gl_context_parity_test: perguntar mudou preset: antes %lld, depois %lld "
                         "(linha %s)\n",
                         static_cast<long long>(before[i]), static_cast<long long>(after[i]),
                         std::string(glintfx::gltfx_gfx_option_at(i).name).c_str());
            return false;
        }
    }
    std::fprintf(stdout, "gl_context_parity_test: celula perguntar nao grava (%zu linhas) ok\n",
                 before.size());
    return true;
}

// CELL "the label is yours, and nothing is reapplied" (rules 3 and 4): after a concrete preset, a
// hand change of one row keeps the label, and asking for the suggestion neither rewrites the label
// nor undoes the hand change.
[[nodiscard]] bool label_is_the_consumers_cell(glintfx::gltfx_gl_context &context) {
    if (!set_option_ok(
            context,
            {.id = glintfx::gltfx_gfx_option::preset, .value = glintfx::k_gltfx_preset_performance},
            "rotulo do consumidor") ||
        !preset_rows_read_back(context, glintfx::k_gltfx_preset_performance,
                               "rotulo do consumidor") ||
        !set_option_ok(
            context, {.id = glintfx::gltfx_gfx_option::vsync, .value = glintfx::k_gltfx_vsync_off},
            "rotulo do consumidor")) {
        return false;
    }
    std::int64_t ignored = 0;
    std::int64_t label = -1;
    std::int64_t vsync = -1;
    if (!read_option(context, glintfx::gltfx_gfx_option::suggested_preset, "rotulo do consumidor",
                     ignored) ||
        !read_option(context, glintfx::gltfx_gfx_option::preset, "rotulo do consumidor", label) ||
        !read_option(context, glintfx::gltfx_gfx_option::vsync, "rotulo do consumidor", vsync)) {
        return false;
    }
    if (label != glintfx::k_gltfx_preset_performance || vsync != glintfx::k_gltfx_vsync_off) {
        std::fprintf(stderr,
                     "gl_context_parity_test: o rotulo ou a linha do consumidor foi reescrito: "
                     "preset=%lld (esperado %lld), vsync=%lld (esperado %lld)\n",
                     static_cast<long long>(label),
                     static_cast<long long>(glintfx::k_gltfx_preset_performance),
                     static_cast<long long>(vsync),
                     static_cast<long long>(glintfx::k_gltfx_vsync_off));
        return false;
    }
    std::fprintf(stdout, "gl_context_parity_test: celula rotulo do consumidor ok\n");
    return true;
}

// CELL "automatic applies the suggestion of right now, once, and stores the concrete value" (rule
// 2).
[[nodiscard]] bool automatic_applies_the_suggestion_cell(glintfx::gltfx_gl_context &context) {
    std::int64_t suggested = 0;
    if (!read_option(context, glintfx::gltfx_gfx_option::suggested_preset, "automatic",
                     suggested) ||
        !set_option_ok(
            context,
            {.id = glintfx::gltfx_gfx_option::preset, .value = glintfx::k_gltfx_preset_automatic},
            "automatic")) {
        return false;
    }
    std::int64_t label = -1;
    if (!read_option(context, glintfx::gltfx_gfx_option::preset, "automatic", label)) {
        return false;
    }
    if (label == glintfx::k_gltfx_preset_automatic || label != suggested) {
        std::fprintf(stderr,
                     "gl_context_parity_test: preset=automatic gravou o rotulo %lld, esperado a "
                     "sugestao concreta %lld (nunca automatic)\n",
                     static_cast<long long>(label), static_cast<long long>(suggested));
        return false;
    }
    if (!preset_rows_read_back(context, label, "automatic")) {
        return false;
    }
    std::fprintf(stdout, "gl_context_parity_test: celula automatic aplica a sugestao ok\n");
    return true;
}

// CELL E1 (the degraded pair): the entry the library hands back for a row that does not exist is
// {suggested_preset, manual}, and applying it is refused by name and changes nothing, label
// included.
[[nodiscard]] bool degraded_pair_is_refused_cell(glintfx::gltfx_gl_context &context) {
    const glintfx::gltfx_gfx_option_entry degraded = glintfx::gltfx_gfx_preset_row_at(99, 0);
    if (degraded.id != glintfx::gltfx_gfx_option::suggested_preset ||
        degraded.value != glintfx::k_gltfx_preset_manual) {
        std::fprintf(stderr,
                     "gl_context_parity_test: gltfx_gfx_preset_row_at(99, 0) devolveu id=%u "
                     "value=%lld, esperado {suggested_preset, manual}\n",
                     static_cast<unsigned>(degraded.id), static_cast<long long>(degraded.value));
        return false;
    }
    std::vector<std::int64_t> before;
    std::vector<std::int64_t> after;
    if (!photograph_every_row(context, before)) {
        return false;
    }
    const glintfx::gltfx_rslt<void> refused = context.set_option(degraded);
    if (!refused.has_error() || refused.err().code() != glintfx::gltfx_err_code::invalid_argument ||
        refused.err().rejected_value() != std::string_view{"suggested_preset"}) {
        std::fprintf(stderr, "gl_context_parity_test: aplicar o par degradado nao foi recusado "
                             "como invalid_argument/suggested_preset\n");
        return false;
    }
    if (!photograph_every_row(context, after) || before != after) {
        std::fprintf(stderr, "gl_context_parity_test: aplicar o par degradado mudou uma linha "
                             "ou o rotulo\n");
        return false;
    }
    std::fprintf(stdout, "gl_context_parity_test: celula par degradado recusado ok\n");
    return true;
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
            std::string(glintfx::gltfx_err_code_name(display_opened.err().code())).c_str());
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
            std::string(glintfx::gltfx_err_code_name(window_opened.err().code())).c_str(),
            std::string(window_opened.err().rejected_value()).c_str());
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
        const auto open_start = std::chrono::steady_clock::now();
        glintfx::gltfx_rslt<glintfx::gltfx_gl_context> context_opened =
            glintfx::gltfx_gl_context::open(window, empty_desc);
        const auto open_end = std::chrono::steady_clock::now();
        const auto open_us =
            std::chrono::duration_cast<std::chrono::microseconds>(open_end - open_start).count();
        std::fprintf(stdout, "MEASURED gl_context_parity_test.open_us=%lld\n",
                     static_cast<long long>(open_us));
        if (context_opened.has_error()) {
            std::fprintf(
                stderr,
                "gl_context_parity_test: gltfx_gl_context::open() failed: %s (rejected_value=%s)\n",
                std::string(glintfx::gltfx_err_code_name(context_opened.err().code())).c_str(),
                std::string(context_opened.err().rejected_value()).c_str());
            return EXIT_FAILURE;
        }
        glintfx::gltfx_gl_context context = std::move(context_opened.value());

        if (glintfx::gltfx_rslt<void> current = context.make_current(); current.has_error()) {
            std::fprintf(stderr, "gl_context_parity_test: make_current() failed: %s\n",
                         std::string(glintfx::gltfx_err_code_name(current.err().code())).c_str());
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
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast) reason: closes the block above
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

        // GL-GPU-KIND (docs/plano-w6b-fatias-5.md sec. 4.3; docs/plano-
        // w6b-fatias-5b-revisao.md sec. 4.6, D-W6b-33/37/38): the API
        // public surface does NOT expose IsHardware/IsIntegrated raw,
        // nor `dxcore_loaded`/`egl_device_queried` - those live in the
        // two reader probes (tests/dxcore_reader_probe_test.cpp,
        // tests/container/egl_device_reader_probe.cpp), not here. This
        // file only prints what gltfx_gl_context::gpu()/gltfx_gpu_
        // enumeration themselves hand back - `gpu_kind_raw`/`gpu_name_
        // hash`/`gpu_enumeration_index_raw` measured, NEVER asserted
        // equal yet (sec. 4.6's own correction, L-43: the assertion
        // `gpu().kind == software` on both sides enters only in the
        // commit AFTER the first real run has printed these values -
        // no such run has happened yet for this fatia).
        std::fprintf(stdout, "MEASURED gl_context_parity_test.gpu_kind_raw=%d\n",
                     static_cast<int>(gpu.kind));
        std::fprintf(stdout, "MEASURED gl_context_parity_test.gpu_name_hash=%016llx\n",
                     static_cast<unsigned long long>(renderer_hash));
        std::fprintf(stdout, "MEASURED gl_context_parity_test.gpu_enumeration_index_raw=%u\n",
                     gpu.enumeration_index);

        glintfx::gltfx_rslt<glintfx::gltfx_gpu_enumeration> enumeration_opened =
            glintfx::gltfx_gpu_enumeration::query();
        if (enumeration_opened.has_error()) {
            std::fprintf(
                stderr, "gl_context_parity_test: gltfx_gpu_enumeration::query() failed: %s\n",
                std::string(glintfx::gltfx_err_code_name(enumeration_opened.err().code())).c_str());
            return EXIT_FAILURE;
        }
        glintfx::gltfx_gpu_enumeration enumeration = std::move(enumeration_opened.value());
        const std::size_t enumeration_count = enumeration.count();
        std::fprintf(stdout, "MEASURED gl_context_parity_test.gpu_enumeration_count=%zu\n",
                     enumeration_count);
        if (enumeration_count == 0) {
            std::fprintf(stderr,
                         "gl_context_parity_test: gpu_enumeration_count=0 (GODS_LAWS.md L-40, "
                         "piso de varredura nao-vazia)\n");
            return EXIT_FAILURE;
        }
        // Three fixed keys, never a wildcard (tests/tools/collect_
        // measured.py's own MEASURED_LINE regex has no prefix concept,
        // sec. 4.3 of the plan's own fallback: "o teste imprime so as
        // tres primeiras entradas com chave fixa").
        for (std::size_t i = 0; i < enumeration_count && i < 3; ++i) {
            const glintfx::gltfx_gpu_info entry = enumeration.at(i);
            std::fprintf(stdout, "MEASURED gl_context_parity_test.gpu_entry_%zu_kind=%d\n", i,
                         static_cast<int>(entry.kind));
            std::fprintf(stdout,
                         "MEASURED gl_context_parity_test.gpu_entry_%zu_name_hash=%016llx\n", i,
                         static_cast<unsigned long long>(fnv1a64(entry.name)));
        }

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
            glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped =
                draw_then_swap(context, clear);
            if (swapped.has_error()) {
                std::fprintf(
                    stderr,
                    "gl_context_parity_test: swap_buffers() attempt %d failed: %s "
                    "(rejected_value=%s)\n",
                    attempt,
                    std::string(glintfx::gltfx_err_code_name(swapped.err().code())).c_str(),
                    std::string(swapped.err().rejected_value()).c_str());
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
                         std::string(glintfx::gltfx_err_code_name(set_off.err().code())).c_str());
            return EXIT_FAILURE;
        }
        int vsync_off_presented = 0;
        int vsync_off_skipped = 0;
        const auto vsync_off_start = std::chrono::steady_clock::now();
        for (int i = 0; i < 60; ++i) {
            const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped =
                draw_then_swap(context, clear);
            if (swapped.has_error()) {
                std::fprintf(
                    stderr, "gl_context_parity_test: swap_buffers() (vsync=off) failed: %s\n",
                    std::string(glintfx::gltfx_err_code_name(swapped.err().code())).c_str());
                return EXIT_FAILURE;
            }
            if (swapped.value() == glintfx::gltfx_present_outcome::presented) {
                ++vsync_off_presented;
            } else {
                ++vsync_off_skipped;
            }
        }
        const auto vsync_off_end = std::chrono::steady_clock::now();
        const auto vsync_off_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(vsync_off_end - vsync_off_start)
                .count();
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_off_60_swaps_ms=%lld\n",
                     static_cast<long long>(vsync_off_ms));
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_off_60_swaps_presented=%d\n",
                     vsync_off_presented);
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_off_60_swaps_skipped=%d\n",
                     vsync_off_skipped);
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
                         std::string(glintfx::gltfx_err_code_name(set_on.err().code())).c_str());
            return EXIT_FAILURE;
        }
        int vsync_on_presented = 0;
        int vsync_on_skipped = 0;
        const auto vsync_on_start = std::chrono::steady_clock::now();
        for (int i = 0; i < 60; ++i) {
            const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped =
                draw_then_swap(context, clear);
            if (swapped.has_error()) {
                std::fprintf(
                    stderr, "gl_context_parity_test: swap_buffers() (vsync=on) failed: %s\n",
                    std::string(glintfx::gltfx_err_code_name(swapped.err().code())).c_str());
                return EXIT_FAILURE;
            }
            if (swapped.value() == glintfx::gltfx_present_outcome::presented) {
                ++vsync_on_presented;
            } else {
                ++vsync_on_skipped;
            }
        }
        const auto vsync_on_end = std::chrono::steady_clock::now();
        const auto vsync_on_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(vsync_on_end - vsync_on_start)
                .count();
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_on_60_swaps_ms=%lld\n",
                     static_cast<long long>(vsync_on_ms));
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_on_60_swaps_presented=%d\n",
                     vsync_on_presented);
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_on_60_swaps_skipped=%d\n",
                     vsync_on_skipped);

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
            if (set_adaptive.err().code() != glintfx::gltfx_err_code::unsupported ||
                set_adaptive.err().rejected_value() != std::string_view{"vsync"}) {
                std::fprintf(
                    stderr,
                    "gl_context_parity_test: set_option(vsync=adaptive) refused as %s/%s, "
                    "expected unsupported/vsync\n",
                    std::string(glintfx::gltfx_err_code_name(set_adaptive.err().code())).c_str(),
                    std::string(set_adaptive.err().rejected_value()).c_str());
                return EXIT_FAILURE;
            }
        }

        // The Godot dor (docs/plano-w6b-fatias-5.md sec. 1): a rapid
        // off/on/off/on toggle, one real swap per step, all `ok` -
        // proves toggling vsync mid-session never wedges the adapter,
        // independent of whether `adaptive` itself was ever accepted.
        constexpr std::int64_t kToggleSequence[] = {0, 1, 0, 1};
        int toggle_presented = 0;
        for (std::int64_t value : kToggleSequence) {
            if (glintfx::gltfx_rslt<void> set_toggle =
                    context.set_option({.id = glintfx::gltfx_gfx_option::vsync, .value = value});
                set_toggle.has_error()) {
                std::fprintf(
                    stderr,
                    "gl_context_parity_test: set_option(vsync=%lld) during the toggle "
                    "sequence failed: %s\n",
                    static_cast<long long>(value),
                    std::string(glintfx::gltfx_err_code_name(set_toggle.err().code())).c_str());
                return EXIT_FAILURE;
            }
            const glintfx::gltfx_rslt<glintfx::gltfx_present_outcome> swapped =
                draw_then_swap(context, clear);
            if (swapped.has_error()) {
                std::fprintf(
                    stderr,
                    "gl_context_parity_test: swap_buffers() during the toggle sequence "
                    "(vsync=%lld) failed: %s\n",
                    static_cast<long long>(value),
                    std::string(glintfx::gltfx_err_code_name(swapped.err().code())).c_str());
                return EXIT_FAILURE;
            }
            if (swapped.value() == glintfx::gltfx_present_outcome::presented) {
                ++toggle_presented;
            }
        }
        std::fprintf(stdout, "MEASURED gl_context_parity_test.vsync_toggle_presented=%d\n",
                     toggle_presented);
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
                info.id == glintfx::gltfx_gfx_option::power_source ||
                info.id == glintfx::gltfx_gfx_option::suggested_preset) {
                if (support != glintfx::gltfx_gfx_option_support::read_only_here) {
                    std::fprintf(
                        stderr,
                        "gl_context_parity_test: option_support(%s) is not read_only_here\n",
                        std::string(info.name).c_str());
                    return EXIT_FAILURE;
                }
            }
            if (info.id == glintfx::gltfx_gfx_option::vsync &&
                support != glintfx::gltfx_gfx_option_support::supported) {
                std::fprintf(stderr,
                             "gl_context_parity_test: option_support(vsync) is not supported\n");
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

        // GFX-PRESET, P4: the suggested-preset cells, last in this scope on purpose - `automatic`
        // writes vsync/frame_rate_cap, and nothing below may depend on the earlier state. The
        // cadence cell (the 30 fps cap OBEYED by the loop) lives in loop_parity_test.cpp.
        if (!measure_suggestion_keys(context) || !asking_writes_nothing_cell(context) ||
            !label_is_the_consumers_cell(context) ||
            !automatic_applies_the_suggestion_cell(context) ||
            !degraded_pair_is_refused_cell(context)) {
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
                         std::string(glintfx::gltfx_err_code_name(reopened.err().code())).c_str(),
                         std::string(reopened.err().rejected_value()).c_str());
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
        if (reopened.err().code() != glintfx::gltfx_err_code::invalid_argument ||
            reopened.err().rejected_value() != std::string_view{"msaa_samples"}) {
            std::fprintf(stderr,
                         "gl_context_parity_test: reabertura com msaa_samples=4 falhou como %s/%s, "
                         "esperado invalid_argument/msaa_samples\n",
                         std::string(glintfx::gltfx_err_code_name(reopened.err().code())).c_str(),
                         std::string(reopened.err().rejected_value()).c_str());
            return EXIT_FAILURE;
        }
        std::fprintf(stdout, "gl_context_parity_test: reabertura com msaa_samples=4 recusada "
                             "(invalid_argument/msaa_samples), como esperado\n");
    }

    // D-SRGB2-12: the open_only contract, alive, each request on its own NEW window.
    if (!open_only_cell(display, glintfx::gltfx_gfx_option::msaa_samples, 4) ||
        !open_only_cell(display, glintfx::gltfx_gfx_option::srgb_framebuffer, 1)) {
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "gl_context_parity_test: celulas open_only (msaa_samples, "
                         "srgb_framebuffer) ok\n");

    // No explicit close() call on `window`/`display` - GODS_LAWS.md
    // L-22's own RAII shape, the SAME reverse-of-creation teardown
    // order window_parity_test.cpp's own final comment already proves.
    return EXIT_SUCCESS;
}
