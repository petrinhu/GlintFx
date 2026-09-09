// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

// egl_probe_smoke.cpp - G-1 (docs/plano-w6b-placa-e-laco.md, fatia 1;
// GODS_LAWS.md L-09/L-40/L-20): a DIAGNOSTIC probe, the Linux twin of
// tests/win32_runner_probe_test.cpp's own "windows_runner_reports_gl_
// context_creation_capability" case (X-GL-0) - is there a real,
// modern (3.3 core) OpenGL context behind the `kwin_wayland --virtual`
// this container's own compositor is, on software rendering, with no
// /dev/dri? Nobody in this project had measured that before this
// fatia: F4 of the plan (docs/plano-w6b-placa-e-laco.md sec. 0) notes
// the image already carries mesa-dri-drivers/mesa-libEGL/wayland-egl,
// but never that eglGetPlatformDisplay() against a REAL wl_display
// from THIS compositor returns something usable, or that the llvmpipe
// software rasterizer behind it actually grants a 3.3 CORE context
// (llvmpipe advertises up to 4.5 core on this project's own host
// machine, but "advertises elsewhere" is not "measured in the exact
// container image this project ships CI on" - GODS_LAWS.md L-44).
//
// WHAT THIS FILE ACTUALLY ASSERTS, and nothing past it (this project's
// own X-GL-0 precedent, and the plan's own sec. 5 "arvore de decisao
// fixada ANTES do dado"): that the ordinary Wayland/glintfx window
// this fatia's sibling fixtures (window_smoke.cpp, window_parity_
// test.cpp) already prove opens cleanly, and that libwayland-egl's
// own wl_egl_window_create() hands back a non-null handle for its
// wl_surface - both baseline mechanics UNRELATED to the actual EGL/GL
// capability question this probe exists to answer (wl_egl_window is a
// thin bookkeeping struct over a pointer and two integers; it fails
// only if the surface pointer itself is null, which the two GLINTFX_
// CHECK-free early returns above it already ruled out). Every EGL/GL
// call from eglGetPlatformDisplay() onward is PRINTED via `MEASURED
// egl_probe_smoke.<key>=<value>` (tests/tools/collect_measured.py's
// own token shape) and NEVER asserted - a container whose EGL only
// offers OpenGL ES, or whose 3.3-core context creation fails, is a
// fact this probe exists to produce for the plan's own decision tree
// (sec. 5) to act on, not a test failure to chase. Each step below
// only ATTEMPTS the next one when the previous one succeeded (a null
// EGLDisplay makes eglInitialize() meaningless to call at all) - the
// probe still runs to completion and exits EXIT_SUCCESS either way,
// because "did the process run to completion and print what it saw"
// is the whole of what a sonda promises (this file's own MEASURED
// lines are the record of exactly how far it got).
//
// docs/plano-w6b-placa-e-laco.md sec. 6, verbatim: "a unica prova de
// pixel e a leitura de volta (glReadPixels) do proprio back buffer" -
// this probe does not even get that far (no glClear, no glReadPixels,
// no eglSwapBuffers): P-GL (fatia 5 of that same plan) is where the
// REAL contract (gltfx_gl_context, GODS_LAWS.md L-22 no-exception
// boundary) gets built and pixel-tested. This file only answers "can
// the container even hand out the context P-GL's own adapter will
// need", one layer below any public API.
//
// eglGetPlatformDisplay()/EGL_PLATFORM_WAYLAND_KHR/wl_egl_window_
// create() (this file's own L-43 search, docs/plano-w6b-placa-e-
// laco.md sec. 0): eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR,
// wl_display*, ...) is EGL 1.5 core (registry.khronos.org/EGL/
// extensions/KHR/EGL_KHR_platform_wayland.txt: promoted to core in
// 1.5, this machine's own egl.h/eglext.h agree - EGL_VERSION prints
// "1.5" below); eglBindAPI(EGL_OPENGL_API) is per-THREAD state read
// by the NEXT eglCreateContext() call, which is why it happens before
// eglCreateContext() and not after; wl_egl_window_create()'s given
// size is in PIXELS (the window's own pixel_size(), never logical_
// size() - the same distinction D-W6a-17 already fixed for
// wl_surface_set_buffer_scale()).
//
// #define WL_EGL_PLATFORM BEFORE <EGL/egl.h>: without it, <EGL/
// eglplatform.h>'s own #elif chain falls through past the WL_EGL_
// PLATFORM branch (the ONE that types EGLNativeWindowType as `struct
// wl_egl_window *`) straight to its generic `#elif defined(__unix__)`
// branch, which types it as a bare integer (khronos_uintptr_t) - this
// file would then need an integer-cast a real Wayland/EGL program
// never needs. Defining the macro is the documented, standard way
// every Wayland+EGL client (Mesa's own demos among them) gets the
// correctly-typed pointer instead.
//
// gl_enum/gl_ubyte/gl_int and the six GL_* token values below are
// declared BY HAND, the exact same self-contained-translation-unit
// convention tests/win32_runner_probe_test.cpp already uses for the
// identical reason (that file's own header comment, quoting src/
// render/gl_abi.hpp): GODS_LAWS.md L-07 keeps <GL/gl.h> out of this
// project even for TEST code, because the GL ABI is a driver-provided
// contract, not a header this project depends on. Values sourced from
// the SAME vendored registry gl_abi.hpp is measured against (third_
// party/khronos/gl.xml lines 176/1072-1074/1980-1981/6124: GL_VENDOR
// 0x1F00, GL_RENDERER 0x1F01, GL_VERSION 0x1F02, GL_MAJOR_VERSION
// 0x821B, GL_MINOR_VERSION 0x821C, GL_CONTEXT_PROFILE_MASK 0x9126,
// GL_CONTEXT_CORE_PROFILE_BIT 0x00000001) - a fact with a path, not a
// number remembered from a header read once. glGetString/glGetIntegerv
// are resolved through eglGetProcAddress() rather than linked against
// -lGL: this keeps the Containerfile's own new g++ invocation adding
// only `-lEGL -lwayland-egl` (docs/plano-w6b-placa-e-laco.md fatia 1
// table, D-W6b-3's own dependency-zero accounting), and is exactly
// the resolve-by-name mechanism D-W6b-2's own public proc_address()
// will expose later - this probe exercises the same technique one
// layer below any public handle.
#define WL_EGL_PLATFORM
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <xdg-shell-client-protocol.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/shell_adapter.hpp"
#include "platform/wayland/window_adapter.hpp"

namespace {

constexpr std::int32_t kWidth = 320;
constexpr std::int32_t kHeight = 240;

using gl_enum = unsigned int;
using gl_ubyte = unsigned char;
using gl_int = int;

constexpr gl_enum k_gl_vendor = 0x1F00;
constexpr gl_enum k_gl_renderer = 0x1F01;
constexpr gl_enum k_gl_version = 0x1F02;
constexpr gl_enum k_gl_major_version = 0x821B;
constexpr gl_enum k_gl_minor_version = 0x821C;
constexpr gl_enum k_gl_context_profile_mask = 0x9126;
constexpr gl_int k_gl_context_core_profile_bit = 0x00000001;

using gl_get_string_fn = const gl_ubyte *(*)(gl_enum);
using gl_get_integerv_fn = void (*)(gl_enum, gl_int *);

[[nodiscard]] const char *text_or_null(const char *value) {
    return value != nullptr ? value : "(null)";
}

[[nodiscard]] const char *text_or_null(const gl_ubyte *value) {
    return value != nullptr ? reinterpret_cast<const char *>(value) : "(null)";
}

// wl_egl_window_guard/egl_display_guard/egl_surface_guard/egl_
// context_guard - same non-copyable, destructor-releases shape
// tests/win32_runner_probe_test.cpp's own window_class_guard/window_
// guard/device_context_guard/gl_context_guard already use for the
// identical reason (this file has no exception-unwind safety net -
// plain main(), not a case-fatal GLINTFX_TEST harness - so cleanup on
// every early return has to be automatic, not repeated by hand at
// each one). Declared in ACQUISITION order below (egl_window, egl_
// display, egl_surface, egl_context) so their automatic destruction
// runs in the REVERSE order EGL's own teardown expects: context
// unbound and destroyed first, then the surface, then eglTerminate()
// on the display, then the wl_egl_window struct - the exact mirror of
// window_smoke.cpp's own "torn down in the REVERSE order open() built
// things in" convention one directory over.
class wl_egl_window_guard {
  public:
    explicit wl_egl_window_guard(wl_egl_window *window) : m_window(window) {}

    wl_egl_window_guard(const wl_egl_window_guard &) = delete;
    wl_egl_window_guard &operator=(const wl_egl_window_guard &) = delete;

    ~wl_egl_window_guard() {
        if (m_window != nullptr) {
            wl_egl_window_destroy(m_window);
        }
    }

    [[nodiscard]] wl_egl_window *get() const noexcept { return m_window; }
    [[nodiscard]] bool is_valid() const noexcept { return m_window != nullptr; }

  private:
    wl_egl_window *m_window = nullptr;
};

class egl_display_guard {
  public:
    explicit egl_display_guard(EGLDisplay display) : m_display(display) {}

    egl_display_guard(const egl_display_guard &) = delete;
    egl_display_guard &operator=(const egl_display_guard &) = delete;

    ~egl_display_guard() {
        if (m_display != EGL_NO_DISPLAY) {
            eglTerminate(m_display);
        }
    }

    [[nodiscard]] EGLDisplay get() const noexcept { return m_display; }
    [[nodiscard]] bool is_valid() const noexcept { return m_display != EGL_NO_DISPLAY; }

  private:
    EGLDisplay m_display = EGL_NO_DISPLAY;
};

class egl_surface_guard {
  public:
    egl_surface_guard(EGLDisplay display, EGLSurface surface)
        : m_display(display), m_surface(surface) {}

    egl_surface_guard(const egl_surface_guard &) = delete;
    egl_surface_guard &operator=(const egl_surface_guard &) = delete;

    ~egl_surface_guard() {
        if (m_surface != EGL_NO_SURFACE) {
            eglDestroySurface(m_display, m_surface);
        }
    }

    [[nodiscard]] EGLSurface get() const noexcept { return m_surface; }
    [[nodiscard]] bool is_valid() const noexcept { return m_surface != EGL_NO_SURFACE; }

  private:
    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLSurface m_surface = EGL_NO_SURFACE;
};

class egl_context_guard {
  public:
    egl_context_guard(EGLDisplay display, EGLContext context)
        : m_display(display), m_context(context) {}

    egl_context_guard(const egl_context_guard &) = delete;
    egl_context_guard &operator=(const egl_context_guard &) = delete;

    ~egl_context_guard() {
        if (m_context != EGL_NO_CONTEXT) {
            eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(m_display, m_context);
        }
    }

    [[nodiscard]] EGLContext get() const noexcept { return m_context; }
    [[nodiscard]] bool is_valid() const noexcept { return m_context != EGL_NO_CONTEXT; }

  private:
    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLContext m_context = EGL_NO_CONTEXT;
};

// probe_egl_and_gl() - EVERY EGL/wl_egl_window resource this probe
// creates lives and dies INSIDE this one function call, entirely
// within the lifetime of the wl_surface/wl_display `window`/`adapter`
// still own when it is called. This is not a style choice: it is the
// fix for a real SIGSEGV a reviewer found running this fixture for
// real (rc=139, container isolation proved). The crash's root cause,
// read from the EGL 1.5 spec (registry.khronos.org/EGL/specs/
// eglspec.1.5.pdf, sec. 3.2 "Initialization" and sec. 3.9.3
// "Destroying a Rendering Context" - eglTerminate() "releases
// resources associated with an EGLDisplay object", and per-platform
// EGL implementations over Wayland (Mesa's wayland platform among
// them) keep protocol state bound to the SAME wl_display connection
// for that release): with `egl_window`/`egl_display` declared as
// PLAIN local variables in main() alongside `adapter`/`shell`/
// `window`, C++'s own reverse-destruction-order rule ONLY protects
// against automatic destructors racing each other - it does nothing
// against an EXPLICIT window.close()/adapter.close() call that main()
// made further up in its OWN body, before ever reaching the closing
// brace where those automatic destructors would run. That earlier
// version called adapter.close() (which disconnects the wl_display,
// wl_display_disconnect()) BEFORE its own egl_display_guard was ever
// destroyed - so eglTerminate() ran against a wl_display already
// freed by libwayland-client, and Mesa's own Wayland-platform cleanup
// dereferenced it. A function boundary makes the correct order true
// BY CONSTRUCTION: every guard this function owns is destroyed at
// its OWN closing brace, in reverse declaration order (egl_display
// before egl_window, exactly EGL's own documented "terminate the
// display, then release native resources" order), and NONE of that
// can happen after the CALLER'S later window.close()/adapter.close()
// - those literally have not executed yet when this function's own
// locals go out of scope.
//
// Returns false only for the ONE assertion this probe makes (this
// file's own header comment): wl_egl_window_create() returning null.
// Every EGL/GL call past that point is PRINTED via `MEASURED egl_
// probe_smoke.<key>=<value>` and never turns this into a failure -
// docs/plano-w6b-placa-e-laco.md sec. 5's own decision tree is what
// turns these lines into a decision, not this function.
[[nodiscard]] bool probe_egl_and_gl(glintfx::platform::wayland_display_adapter &adapter,
                                    glintfx::platform::wayland_window_adapter &window,
                                    const glintfx::platform::window_size &pixel_size) {
    // THE ONE ASSERTION THIS PROBE MAKES (this file's own header
    // comment): wl_egl_window_create() over a wl_surface this
    // process's own glintfx window already opened is a baseline
    // libwayland-egl mechanic, unrelated to the EGL/GL capability
    // question everything past this point exists to answer.
    wl_egl_window_guard egl_window(wl_egl_window_create(
        window.surface(), static_cast<int>(pixel_size.width), static_cast<int>(pixel_size.height)));
    std::fprintf(stdout, "MEASURED egl_probe_smoke.wl_egl_window_create_ok=%d\n",
                 egl_window.is_valid() ? 1 : 0);
    if (!egl_window.is_valid()) {
        std::fprintf(stderr, "egl_probe_smoke: wl_egl_window_create() returned null\n");
        return false;
    }

    // From here on: PRINTED, never asserted (this file's own header
    // comment) - docs/plano-w6b-placa-e-laco.md sec. 5's own decision
    // tree is what turns these MEASURED lines into a decision, not
    // this probe.
    egl_display_guard egl_display(
        eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, adapter.native_display(), nullptr));
    std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_get_platform_display_ok=%d\n",
                 egl_display.is_valid() ? 1 : 0);

    EGLint egl_major = 0;
    EGLint egl_minor = 0;
    bool egl_initialized = false;
    if (egl_display.is_valid()) {
        egl_initialized = eglInitialize(egl_display.get(), &egl_major, &egl_minor) == EGL_TRUE;
    }
    std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_initialize_ok=%d\n",
                 egl_initialized ? 1 : 0);
    std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_major=%d\n", egl_major);
    std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_minor=%d\n", egl_minor);

    bool gl_api_bound = false;
    EGLConfig config = nullptr;
    bool config_chosen = false;

    if (egl_initialized) {
        const char *egl_version_text = eglQueryString(egl_display.get(), EGL_VERSION);
        const char *egl_vendor_text = eglQueryString(egl_display.get(), EGL_VENDOR);
        const char *egl_client_apis_text = eglQueryString(egl_display.get(), EGL_CLIENT_APIS);
        std::fprintf(stdout, "egl_probe_smoke: EGL_VERSION=%s EGL_VENDOR=%s EGL_CLIENT_APIS=%s\n",
                     text_or_null(egl_version_text), text_or_null(egl_vendor_text),
                     text_or_null(egl_client_apis_text));
        std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_version=%s\n",
                     text_or_null(egl_version_text));
        std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_vendor=%s\n",
                     text_or_null(egl_vendor_text));
        std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_client_apis=%s\n",
                     text_or_null(egl_client_apis_text));

        gl_api_bound = eglBindAPI(EGL_OPENGL_API) == EGL_TRUE;
        std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_bind_api_ok=%d\n", gl_api_bound ? 1 : 0);
    }

    if (gl_api_bound) {
        // RGBA8 + stencil8 (D-W6b-4's own choice for the real GL-
        // CONTEXT adapter, docs/plano-w6b-placa-e-laco.md sec. 3): a
        // config this SAME container will need to hand out for real
        // once fatia 3 (W-EGL) lands is exactly the one worth probing
        // for here.
        const EGLint config_attribs[] = {
            EGL_SURFACE_TYPE,
            EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE,
            EGL_OPENGL_BIT,
            EGL_RED_SIZE,
            8,
            EGL_GREEN_SIZE,
            8,
            EGL_BLUE_SIZE,
            8,
            EGL_ALPHA_SIZE,
            8,
            EGL_STENCIL_SIZE,
            8,
            EGL_NONE,
        };
        EGLint num_configs = 0;
        config_chosen = eglChooseConfig(egl_display.get(), config_attribs, &config, 1,
                                        &num_configs) == EGL_TRUE &&
                        num_configs > 0;
        std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_choose_config_ok=%d\n",
                     config_chosen ? 1 : 0);
    }

    // Nested rather than a flat sequence of reassigned guards on
    // purpose: egl_surface_guard/egl_context_guard each own a
    // non-copyable, non-movable EGL handle (this file's own header
    // comment on the guard classes - a user-declared destructor
    // suppresses the implicit move operations too), so each one is
    // constructed exactly once, in the innermost scope that actually
    // has a value worth giving it - never default-constructed empty
    // and reassigned later.
    if (config_chosen) {
        egl_surface_guard surface(
            egl_display.get(),
            eglCreateWindowSurface(egl_display.get(), config, egl_window.get(), nullptr));
        std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_create_window_surface_ok=%d\n",
                     surface.is_valid() ? 1 : 0);

        if (surface.is_valid()) {
            const EGLint context_attribs[] = {
                EGL_CONTEXT_MAJOR_VERSION,
                3,
                EGL_CONTEXT_MINOR_VERSION,
                3,
                EGL_CONTEXT_OPENGL_PROFILE_MASK,
                k_gl_context_core_profile_bit,
                EGL_NONE,
            };
            egl_context_guard context(
                egl_display.get(),
                eglCreateContext(egl_display.get(), config, EGL_NO_CONTEXT, context_attribs));
            std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_create_context_33_core_ok=%d\n",
                         context.is_valid() ? 1 : 0);

            if (context.is_valid()) {
                const bool context_current =
                    eglMakeCurrent(egl_display.get(), surface.get(), surface.get(),
                                   context.get()) == EGL_TRUE;
                std::fprintf(stdout, "MEASURED egl_probe_smoke.egl_make_current_ok=%d\n",
                             context_current ? 1 : 0);

                if (context_current) {
                    // wglGetProcAddress-style resolve-by-name (this
                    // file's own header comment): glGetString/
                    // glGetIntegerv are core GL 1.0/1.1 entry points
                    // every real implementation exports, but this
                    // project links neither -lGL nor a system <GL/
                    // gl.h> - eglGetProcAddress() is the SAME
                    // mechanism D-W6b-2's own public proc_address()
                    // will expose, exercised here one layer below any
                    // handle this project ships.
                    const auto get_string = reinterpret_cast<gl_get_string_fn>(
                        reinterpret_cast<void *>(eglGetProcAddress("glGetString")));
                    const auto get_integerv = reinterpret_cast<gl_get_integerv_fn>(
                        reinterpret_cast<void *>(eglGetProcAddress("glGetIntegerv")));
                    std::fprintf(stdout, "MEASURED egl_probe_smoke.gl_get_string_resolved=%d\n",
                                 get_string != nullptr ? 1 : 0);
                    std::fprintf(stdout, "MEASURED egl_probe_smoke.gl_get_integerv_resolved=%d\n",
                                 get_integerv != nullptr ? 1 : 0);

                    if (get_string != nullptr) {
                        const gl_ubyte *vendor = get_string(k_gl_vendor);
                        const gl_ubyte *renderer = get_string(k_gl_renderer);
                        const gl_ubyte *version = get_string(k_gl_version);
                        std::fprintf(
                            stdout, "egl_probe_smoke: GL_VENDOR=%s GL_RENDERER=%s GL_VERSION=%s\n",
                            text_or_null(vendor), text_or_null(renderer), text_or_null(version));
                        std::fprintf(stdout, "MEASURED egl_probe_smoke.gl_vendor=%s\n",
                                     text_or_null(vendor));
                        std::fprintf(stdout, "MEASURED egl_probe_smoke.gl_renderer=%s\n",
                                     text_or_null(renderer));
                        std::fprintf(stdout, "MEASURED egl_probe_smoke.gl_version=%s\n",
                                     text_or_null(version));
                    }

                    if (get_integerv != nullptr) {
                        gl_int gl_major = -1;
                        gl_int gl_minor = -1;
                        gl_int gl_profile_mask = 0;
                        get_integerv(k_gl_major_version, &gl_major);
                        get_integerv(k_gl_minor_version, &gl_minor);
                        get_integerv(k_gl_context_profile_mask, &gl_profile_mask);
                        const bool core_profile =
                            (gl_profile_mask & k_gl_context_core_profile_bit) != 0;
                        std::fprintf(stdout,
                                     "egl_probe_smoke: GL_MAJOR_VERSION=%d GL_MINOR_VERSION=%d "
                                     "GL_CONTEXT_PROFILE_MASK=%#x (core=%d)\n",
                                     gl_major, gl_minor, gl_profile_mask, core_profile ? 1 : 0);
                        std::fprintf(stdout, "MEASURED egl_probe_smoke.gl_major_version=%d\n",
                                     gl_major);
                        std::fprintf(stdout, "MEASURED egl_probe_smoke.gl_minor_version=%d\n",
                                     gl_minor);
                        std::fprintf(stdout,
                                     "MEASURED egl_probe_smoke.gl_context_profile_mask=%#x\n",
                                     gl_profile_mask);
                        std::fprintf(stdout,
                                     "MEASURED egl_probe_smoke.gl_context_is_core_profile=%d\n",
                                     core_profile ? 1 : 0);
                    }
                }
            }
        }
    }

    return true;
}

} // namespace

int main() {
    // Unbuffer stdout explicitly (defect found by a reviewer
    // running this fixture for real): stdout is FULLY buffered by
    // default whenever it is not a TTY - exactly the case for every
    // real invocation of this probe (`docker exec ... | tee -a
    // container_measured_raw.log`, this project's own ci.yml). A
    // fully-buffered stdout means a crash BEFORE an orderly return
    // (the very crash this file's own probe_egl_and_gl() comment
    // fixes one cause of) discards whatever had not yet reached the
    // libc buffer's flush threshold - all 25+ MEASURED lines this
    // probe can produce, gone, with nothing on the other end of the
    // pipe to show for it. A reader would see "died immediately" for
    // a run that in fact measured everything. Forcing no buffering
    // makes every fprintf(..., "\n") reach the pipe as soon as it is
    // printed, immune to whatever happens to this process afterward -
    // GODS_LAWS.md L-40's own "medida sem leitor" rule extended one
    // step earlier: a measurement that never reaches the pipe is
    // indistinguishable from one that was never taken. CORRECTED same
    // day (06/09/2026): this line originally asked for `_IOLBF` with
    // `size` 0, and that combination is what crashed the Windows CI
    // job with 0xC0000409 - MSVC's setvbuf requires 2 <= size <=
    // INT_MAX for the `_IOFBF`/`_IOLBF` modes and invokes its
    // invalid-parameter handler (which aborts the process) outside
    // that range (learn.microsoft.com/cpp/c-runtime-library/reference/
    // setvbuf); glibc never validated that range, which is why this
    // line built and ran clean on every machine that wrote it. `_IONBF`
    // ignores `size` and `buffer` entirely, so no range applies - and
    // MSVC's own docs say `_IOLBF` behaves exactly like `_IOFBF` (full
    // buffering) on Win32 anyway, so the line was never buying the
    // per-line flush this comment promises on that platform in the
    // first place.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    glintfx::platform::wayland_display_adapter adapter;
    glintfx::gltfx_rslt<void> opened = adapter.open();
    if (opened.has_error()) {
        std::fprintf(stderr, "egl_probe_smoke: display open() failed: %s\n",
                     std::string(glintfx::gltfx_err_code_name(opened.err().code())).c_str());
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_shell_adapter shell;
    glintfx::gltfx_rslt<void> shell_opened = shell.open(adapter);
    if (shell_opened.has_error()) {
        std::fprintf(stderr, "egl_probe_smoke: shell.open() failed: %s (rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(shell_opened.err().code())).c_str(),
                     std::string(shell_opened.err().rejected_value()).c_str());
        adapter.close();
        return EXIT_FAILURE;
    }

    glintfx::platform::wayland_window_adapter window;
    glintfx::platform::wayland_window_desc desc{
        .logical_width = static_cast<std::uint32_t>(kWidth),
        .logical_height = static_cast<std::uint32_t>(kHeight),
        .title = "egl_probe_smoke",
        .application_id = "org.glintfx.egl_probe_smoke",
    };
    glintfx::gltfx_rslt<void> window_opened = window.open(adapter, shell, desc);
    if (window_opened.has_error()) {
        std::fprintf(stderr, "egl_probe_smoke: window.open() failed: %s (rejected_value=%s)\n",
                     std::string(glintfx::gltfx_err_code_name(window_opened.err().code())).c_str(),
                     std::string(window_opened.err().rejected_value()).c_str());
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    const glintfx::platform::window_size pixel_size = window.state().pixel_size();
    std::fprintf(stdout, "egl_probe_smoke: window configured - pixel_size=%ux%u\n",
                 pixel_size.width, pixel_size.height);

    // probe_egl_and_gl() owns EVERY EGL/wl_egl_window resource this
    // probe creates, and destroys all of them before it returns here -
    // see that function's own header comment for the crash this
    // ordering fixes. `probe_completed` is false only for the ONE
    // assertion this probe makes (wl_egl_window_create() returning
    // null); the close()/is_open() sequence below still runs either
    // way, so a failed probe leaves the same clean Wayland teardown a
    // successful one does.
    const bool probe_completed = probe_egl_and_gl(adapter, window, pixel_size);

    window.close();
    if (window.is_open()) {
        std::fprintf(stderr, "egl_probe_smoke: window.close() ran but is_open() is still true\n");
        shell.close();
        adapter.close();
        return EXIT_FAILURE;
    }
    shell.close();
    adapter.close();
    if (adapter.is_open()) {
        std::fprintf(stderr, "egl_probe_smoke: adapter.close() ran but is_open() is still true\n");
        return EXIT_FAILURE;
    }
    if (!probe_completed) {
        return EXIT_FAILURE;
    }
    std::fprintf(stdout, "egl_probe_smoke: closed cleanly\n");

    return EXIT_SUCCESS;
}
