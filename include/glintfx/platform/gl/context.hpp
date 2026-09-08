// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/export.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>

// platform/gl/context.hpp - GL-CONTEXT (docs/plano-w6b-placa-e-laco.md
// fatia 2b, D-W6b-1/2/4/5/6/7/17/25, GODS_LAWS.md L-19/L-22/L-26): the
// public handle a consumer opens ON TOP of an already-open gltfx_
// window (glintfx/platform/window/window.hpp) to get an OpenGL 3.3
// core context, make it current, present a frame, and resolve GL
// function addresses by name - the SAME call on Linux and Windows, no
// #if in consumer code (D-W6b-1: "um handle, um assunto - contexto GL
// sobre uma janela").
//
// ============================================================
// PORTA DE MAO UNICA - WHAT THIS FATIA FREEZES, FOREVER
// ============================================================
//
//   1. gltfx_gl_context_desc IS EXACTLY TWO FIELDS (D-W6b-17): a list
//      of gltfx_gfx_option_entry (gfx_option.hpp) and its count. EVERY
//      opening knob this library ever grows - MSAA, sRGB, GPU
//      preference, and whatever comes after - is a ROW of the append-
//      only options table (gfx_option_registry.hpp), never a new
//      field here. Growing this struct is ABI breakage the moment a
//      consumer has compiled against it; growing the OPTIONS TABLE is
//      not.
//
//   2. gltfx_present_outcome IS THE ANSWER TO "DID THIS FRAME REACH THE
//      SCREEN" (D-W6b-6): `presented` or `skipped_hidden` - NEVER a
//      third value, and swap_buffers() below NEVER blocks the process
//      indefinitely to produce one. A window occluded, minimized, or
//      not currently being repainted degrades to `skipped_hidden`,
//      mechanism differs by platform (Wayland: the frame callback
//      budget this fatia's own adapters, fatia 3, manage; Windows:
//      `IsIconic()`, fatia 4) - the SHAPE of the answer is what this
//      header freezes, not the mechanism.
//
//   3. THE NINE METHODS BELOW ARE THE FROZEN SURFACE (D-W6b-2, emended
//      by sec. 11: `set_swap_interval()` an earlier draft of this plan
//      proposed is GONE - v-sync is the `vsync` row of the options
//      table instead, D-W6b-18 - and `set_option`/`option`/`option_
//      support`/`gpu` take its place): open (static factory),
//      is_open, make_current, swap_buffers, proc_address, set_option,
//      option, option_support, gpu. `proc_address` returns a raw
//      address-or-null the same way eglGetProcAddress/wglGetProcAddress
//      already do - a name this driver does not resolve is an ordinary
//      lookup miss, not a gltfx_err (docs/api-conventions.md's own R1
//      governs functions this library decides CAN fail with a
//      diagnostic; a GL entry point either exists at this address or
//      it does not, the same category std::map::find() already is).
//
//   4. A SECOND CONTEXT ON THE SAME WINDOW MUST ASK FOR THE SAME
//      open_only OPTIONS THE FIRST ONE FIXED (D-W6b-25): the FIRST
//      gltfx_gl_context that successfully opens over a gltfx_window
//      freezes, on that window, every `open_only` option's resolved
//      value (requested, or the table's own default when omitted) -
//      platform::gfx_open_only_fixation.hpp is the shared atom that
//      decides this, identically on every system, BEFORE either
//      adapter is ever asked whether it supports anything. A LATER
//      open() on the SAME window that asks for a DIFFERENT resolved
//      value is refused with `gltfx_err_code::invalid_argument`,
//      `rejected_value()` naming the option - the fixation stays alive
//      until the gltfx_window itself is destroyed, never tied to any
//      one gltfx_gl_context's own lifetime. This is DIFFERENT from
//      "this system does not have this option at all"
//      (`gltfx_err_code::unsupported`, same `rejected_value()` shape,
//      raised by option_support() below returning
//      `unsupported_here`) - TWO CODES for two different causes, so a
//      consumer's own catch site tells "open another window" from
//      "this driver never had this" without parsing a sentence
//      (docs/api-conventions.md R7: a token, never prose).
//
// WHAT THIS FATIA DOES NOT FREEZE (out of scope by construction, see
// docs/plano-w6b-placa-e-laco.md sec. 8/10.4/11.3): choosing which GPU
// renders (gpu_preference stays `no_preference`-only until GL-GPU-
// PREFERENCE), several-GPU enumeration on demand (gltfx_gpu_
// enumeration, fatia 5b), a graphics-preset table and automatic choice
// (fatia 5c), the main loop and fixed-step accumulator (fatia 6),
// contexts shared across threads, MSAA/sRGB actually honored by a
// driver (fatias 3/4 answer via option_support(), never this header).
//
// OPAQUE, PIMPL (CONTRACT.md's own opacity clause, GODS_LAWS.md L-19,
// the SAME shape gltfx_display/gltfx_window already use): the real
// layout - today, either a Wayland EGL adapter or a Win32 WGL adapter,
// picked by src/platform/gl/gl_context_facade.cpp's own #if - lives
// entirely inside that translation unit, defined in gl_context_impl.hpp
// (never installed, never public).
//
// noexcept THROUGHOUT (docs/api-conventions.md R3): every fallible
// signature returns gltfx_rslt<T> and is noexcept - internal failure
// degrades or is translated before crossing this boundary, never
// thrown, never aborts the consumer's process.

namespace glintfx {

// Forward-declared only (the same "a reference needs no more" shape
// window.hpp's own gltfx_display forward-declaration already uses):
// open() below takes a `gltfx_window &` by reference. A consumer
// opening a context already includes <glintfx/platform/window/
// window.hpp> for gltfx_window::open() itself, so this never leaves a
// caller with an incomplete type at the one call site that matters.
class gltfx_window;

// D-W6b-6: the ONE thing swap_buffers() below promises about whether a
// frame reached the screen (item 2 of this header's own "WHAT THIS
// FATIA FREEZES" list, above). Also the value gltfx_frame_tick::last_
// present (a LATER fatia, platform/loop/loop.hpp) copies straight from
// here - never re-derived from anything else (D-W6b-26). std::uint8_t:
// a closed, structural vocabulary, the same category gltfx_gfx_
// option_kind already is.
enum class gltfx_present_outcome : std::uint8_t {
    presented,
    skipped_hidden,
};

// The open-time request (item 1 of this header's own "WHAT THIS FATIA
// FREEZES" list, above): plain data, never validated by this struct
// itself (docs/api-conventions.md's own convention for a struct like
// this - the validation lives in src/platform/gl/gl_context_desc_
// validation.hpp, an internal atom open() below calls before any
// adapter ever sees a request). `options` may be `nullptr` only when
// `option_count == 0` - a non-null count with a null pointer is
// refused the same way an unknown option id is, never dereferenced.
struct gltfx_gl_context_desc {
    const gltfx_gfx_option_entry *options = nullptr;
    std::size_t option_count = 0;
};

// Opaque (GODS_LAWS.md L-19): the full layout is a private
// implementation detail, defined ONLY in gl_context_facade.cpp. A
// consumer never sees this type - gltfx_gl_context below carries only
// a pointer to one, the exact same shape gltfx_display/gltfx_window
// already have.
struct gl_context_impl;

// gl_context_internal_access - GL-CONTEXT (docs/plano-w6b-fatias-6-8.md
// D-W6b-53): the SAME passkey idiom display_internal_access (platform/
// window/display.hpp) and window_internal_access (platform/window/
// window.hpp) already establish, applied here so a later translation
// unit OUTSIDE this class's own port contract - loop_facade.cpp,
// LOOP-RUN's own sonda de oculto, D-W6b-46 - can reach the gl_context_
// impl an already-open gltfx_gl_context wraps, the same way window_
// facade.cpp already reaches display_impl through display_internal_
// access one directory over.
//
// A FRIEND STRUCT, NOT A METHOD ON gltfx_gl_context, FOR THE SAME
// REASON display_internal_access already gives (that struct's own
// header comment, read in full before this one was written): a public
// method here would grow gltfx_gl_context's own frozen surface (this
// header's own "PORTA DE MAO UNICA" list, item 3) - a promise the 1.0
// review would have to honor or explicitly remove. This struct sits
// OUTSIDE gltfx_gl_context entirely.
//
// get() DECLARED HERE, DEFINED ONLY IN gl_context_facade.cpp - the
// same "no GLINTFX_API, never in the dynamic symbol table" convention
// display_internal_access::get() already documents in full: a
// consumer's own translation unit sees only this declaration, never
// the out-of-line body, so it cannot synthesize the access itself - it
// can only ask the LINKER for a symbol this project deliberately never
// exports from the public glintfx::glintfx target.
struct gl_context_internal_access {
    [[nodiscard]] static gl_context_impl *get(class gltfx_gl_context &context) noexcept;
};

// gltfx_gl_context - see this header's own top comment for the full
// "WHAT THIS FATIA FREEZES" list. Named-constructor idiom, the same
// shape gltfx_display::open()/gltfx_window::open() already use: open()
// is a FALLIBLE static factory returning gltfx_rslt<gltfx_gl_context>,
// never a bare constructor that could fail.
class gltfx_gl_context {
  public:
    // Opens a context over an already-open `window` - GODS_LAWS.md
    // L-22: any refusal (an unknown/out-of-range/read-only option in
    // `desc`, an open_only option that disagrees with what this SAME
    // window already fixed - item 4 above -, an unsupported option
    // this driver does not have, or a platform-specific failure)
    // comes back as an ordinary gltfx_err through this gltfx_rslt<
    // gltfx_gl_context>, never an exception, never a half-open object.
    //
    // `window` MUST OUTLIVE the returned context - the same
    // precondition gltfx_window::open() already documents for its own
    // `display` parameter, one layer up. `desc` is read, never stored:
    // its `options` pointer needs to stay valid only for the duration
    // of this call.
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_gl_context>
    open(gltfx_window &window, const gltfx_gl_context_desc &desc) noexcept;

    // Move-only - a live context is a resource (a real GL context and,
    // on Wayland, an EGL surface), the same reasoning gltfx_window's
    // own class comment already gives for a live window handle.
    gltfx_gl_context(const gltfx_gl_context &) = delete;
    gltfx_gl_context &operator=(const gltfx_gl_context &) = delete;

    GLINTFX_API gltfx_gl_context(gltfx_gl_context &&other) noexcept;
    GLINTFX_API gltfx_gl_context &operator=(gltfx_gl_context &&other) noexcept;

    // Closes on scope exit - RAII, the same contract every other
    // handle in this library already gives.
    GLINTFX_API ~gltfx_gl_context();

    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;

    // Makes this context current on the calling thread - v1 is
    // single-thread only (this header's own top comment, "WHAT THIS
    // FATIA DOES NOT FREEZE"): a consumer that calls this from more
    // than one thread over the same context's lifetime gets whatever
    // the underlying driver does, undocumented until a later fatia
    // names it.
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> make_current() noexcept;

    // Presents the current frame - see this header's own top comment,
    // item 2, for the full gltfx_present_outcome contract. NEVER
    // blocks indefinitely: a window this library cannot currently
    // repaint degrades to `skipped_hidden` within a bounded budget,
    // never a hang.
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_present_outcome> swap_buffers() noexcept;

    // Resolves a GL function's address by name - an ordinary lookup,
    // not a gltfx_err-shaped failure (see this header's own top
    // comment, item 3). `name` not resolvable by this driver returns
    // `nullptr`, the same way eglGetProcAddress/wglGetProcAddress
    // already answer "I do not have this".
    [[nodiscard]] GLINTFX_API void *proc_address(std::string_view name) const noexcept;

    // Changes a `live` option (gltfx_gfx_option_when::live, gfx_
    // option.hpp) after open() - refused the same way gfx_option_
    // validation.hpp already refuses an unknown id, an out-of-range
    // value, or a `read_only`/`open_only` entry (`invalid_argument`),
    // and refused with `unsupported` when this system answers `option_
    // support(entry.id) == unsupported_here` (never silently ignored -
    // D-W6b-16's own "nunca degrada em silencio").
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> set_option(gltfx_gfx_option_entry entry) noexcept;

    // Reads back the CURRENT value of any option this context knows
    // about - what a consumer requested (open_only or live), what the
    // registry's own default is when nothing was requested, or what
    // the library itself last wrote for a `read_only` option (`none`,
    // `unknown`, and so on, until a later fatia starts writing a real
    // one). An `id` this build's own table does not carry (gfx_
    // option.hpp's own gltfx_gfx_option_describe() would degrade the
    // same `id` to an empty gltfx_gfx_option_info) returns
    // gltfx_err_code::not_found here instead - option() DOES have a
    // gltfx_err-shaped failure, unlike proc_address() above, because
    // "this id does not exist" is a genuine mistake on the caller's
    // part, not an ordinary lookup miss.
    [[nodiscard]] GLINTFX_API gltfx_rslt<std::int64_t> option(gltfx_gfx_option id) const noexcept;

    // What THIS system answers when asked whether it honors `id` -
    // distinct from gltfx_gfx_option_describe() (gfx_option.hpp),
    // which answers what the LIBRARY knows about the option in the
    // abstract, with no system in the picture (D-W6b-16's own
    // "distinta de... com o sistema"). An `id` this build's table does
    // not carry answers `unsupported_here` - degrading, never
    // undefined behavior, the same docs/api-conventions.md R4 shape
    // gfx_option.hpp's own accessors already use.
    [[nodiscard]] GLINTFX_API gltfx_gfx_option_support
    option_support(gltfx_gfx_option id) const noexcept;

    // The GPU this context is currently rendering on, as far as this
    // system has told the library (gpu.hpp's own header comment: `unknown`
    // is the honest default, never a guess).
    [[nodiscard]] GLINTFX_API gltfx_gpu_info gpu() const noexcept;

  private:
    explicit gltfx_gl_context(gl_context_impl *impl) noexcept : m_impl(impl) {}

    // gl_context_internal_access::get() is the ONLY thing outside this
    // class ever granted access to m_impl - see that struct's own
    // header comment, right above this class, for why it is a friend
    // struct with an out-of-line static method and not a public method
    // on gltfx_gl_context itself.
    friend struct gl_context_internal_access;

    gl_context_impl *m_impl = nullptr;
};

} // namespace glintfx
