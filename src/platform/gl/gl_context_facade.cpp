// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cassert>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/gl/gfx_open_only_fixation.hpp"
#include "platform/gl/gfx_option_registry.hpp"
#include "platform/gl/gfx_option_validation.hpp"
#include "platform/gl/gl_context_desc_validation.hpp"
#include "platform/gl/gl_context_impl.hpp"
#include "platform/port/gl_context_adapter_port.hpp"
#include "platform/window/window_impl.hpp"

// gl_context_facade.cpp - GL-CONTEXT (docs/plano-w6b-placa-e-laco.md
// fatia 2b, D-W6b-1/2/16/17/25, GODS_LAWS.md L-17/L-19/L-22): the ONE
// translation unit that knows what glintfx::gl_context_impl actually
// IS - the exact same "allocation and deallocation on the SAME side of
// the boundary" shape display_facade.cpp/window_facade.cpp already
// give their own opaque handles.
//
// NOT YET WIRED INTO glintfx_library's OWN BUILD (src/platform/gl/
// CMakeLists.txt's own header comment explains why, in full): this
// file PIMPLs over platform::selected_gl_context_adapter, a type only
// fatia 3 (Wayland/EGL) or fatia 4 (Win32/WGL) provides. Written and
// reviewed here, as part of fatia 2b's own frozen contract; the LAST
// step of whichever of those two fatias lands second is adding this
// file's own name to that target_sources() call, in the SAME commit
// that finally makes the library build with a real GL context inside.
//
// THE OPEN() SEQUENCE, IN THE EXACT ORDER sec. 14.2 OF THE PLAN NAMES
// (never reordered - the mutation the plan's own §14.2/§4-fatia-5 rows
// name, "fixation depois do suporte", is exactly what reordering this
// would reintroduce):
//
//   1. gl_context_desc_validation.hpp's own validate_gl_context_desc()
//      - the WHOLE descriptor's shape (unknown id, out-of-range value,
//      read_only entry, a repeated id, gpu_preference reserved) -
//      before either fixation or any adapter is ever consulted.
//   2. gfx_open_only_fixation.hpp's own resolve_gfx_open_only_
//      fixation() - identical on every system, BY CONSTRUCTION, since
//      it runs before step 3 ever asks whether THIS driver happens to
//      support anything (D-W6b-25: this is what makes "recusa por
//      fixacao" and "recusa por falta de suporte do sistema" two
//      genuinely different, and differently-coded, outcomes rather
//      than the SAME refusal on one system and a silent accept on the
//      other).
//   3. The concrete platform::selected_gl_context_adapter's own
//      open() - the only step that can fail for a reason neither of
//      the two pure atoms above could ever catch (the driver genuinely
//      does not have a config, the GL version negotiated is not 3.3
//      core, and so on).
//
// THE FIXATION IS COMMITTED TO THE WINDOW ONLY ON SUCCESS, AND ONLY
// WHEN THIS WAS THE FIRST CONTEXT TO FIX IT (D-W6b-25's own rule 3,
// "a fixacao morre com a janela, nunca com o contexto"): a `fix_now`
// outcome whose adapter open() call then fails must NOT leave the
// window believing something is fixed that no context ever actually
// opened with - see the `fix_now` branch below for exactly where this
// is enforced.

namespace glintfx {

namespace {

static_assert(platform::gl_context_adapter_port<platform::selected_gl_context_adapter>,
              "selected_gl_context_adapter must satisfy gl_context_adapter_port - GODS_LAWS.md "
              "L-19/L-40: gltfx_gl_context's own facade wraps it directly, and a selection "
              "missing any one member has to fail to COMPILE here, never link a context handle "
              "whose set_option()/gpu() silently does nothing on one platform");

// Every row of the registry, at its resolved value: `open_only` rows
// take `fixed_open_only` (the COMPLETE set gfx_open_only_fixation.hpp
// already resolved, requested-or-default); `live`/`read_only` rows
// take whatever `requested` asks for, or the row's own default
// otherwise. This is what seeds gl_context_impl::current_values AND
// what the concrete adapter's own open() receives - the adapter never
// has to separately reason about "what wins between the opening list
// and the registry's own default", this atom already resolved it once.
[[nodiscard]] std::vector<gltfx_gfx_option_entry>
resolve_full_option_table(std::span<const gltfx_gfx_option_entry> fixed_open_only,
                          std::span<const gltfx_gfx_option_entry> requested) noexcept {
    std::vector<gltfx_gfx_option_entry> resolved;
    resolved.reserve(platform::k_gfx_option_table.size());

    for (const platform::gfx_option_row &row : platform::k_gfx_option_table) {
        if (row.when == gltfx_gfx_option_when::open_only) {
            for (const gltfx_gfx_option_entry &fixed_entry : fixed_open_only) {
                if (fixed_entry.id == row.id) {
                    resolved.push_back(fixed_entry);
                    break;
                }
            }
            continue;
        }

        std::int64_t value = row.default_value;
        for (const gltfx_gfx_option_entry &requested_entry : requested) {
            if (requested_entry.id == row.id) {
                value = requested_entry.value;
                break;
            }
        }
        resolved.push_back(gltfx_gfx_option_entry{.id = row.id, .value = value});
    }

    return resolved;
}

[[nodiscard]] std::string_view option_name_or_placeholder(gltfx_gfx_option id) noexcept {
    const platform::gfx_option_row *row = platform::find_gfx_option_row(id);
    return row != nullptr ? row->name : std::string_view("gfx_option");
}

} // namespace

gltfx_rslt<gltfx_gl_context> gltfx_gl_context::open(gltfx_window &window,
                                                    const gltfx_gl_context_desc &desc) noexcept {
    if (!window.is_open()) {
        return gltfx_rslt<gltfx_gl_context>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }

    // Step 1 (this file's own top comment): the whole descriptor's
    // shape, before either fixation or any adapter ever sees it.
    if (const gltfx_rslt<void> shape_ok = platform::validate_gl_context_desc(desc);
        shape_ok.has_error()) {
        return gltfx_rslt<gltfx_gl_context>::err(shape_ok.error());
    }

    window_impl *window_impl_ptr = window_internal_access::get(window);
    // window.is_open() already proved m_impl != nullptr above - this
    // assert documents that invariant at THIS call site too, the same
    // "precondition, not a recoverable failure" shape window_facade.
    // cpp's own gltfx_window::open() already carries for the identical
    // class of bug.
    assert(window_impl_ptr != nullptr &&
           "gltfx_gl_context::open() called with a gltfx_window that reported is_open() but has "
           "a null internal impl - precondition violated, not a recoverable error");

    const std::span<const gltfx_gfx_option_entry> requested(desc.options, desc.option_count);

    // Step 2: D-W6b-25's own fixation, identical by construction on
    // every system.
    std::optional<std::span<const gltfx_gfx_option_entry>> already_fixed;
    if (window_impl_ptr->fixed_open_only_gfx_options.has_value()) {
        already_fixed =
            std::span<const gltfx_gfx_option_entry>(*window_impl_ptr->fixed_open_only_gfx_options);
    }

    const platform::gfx_open_only_fixation_result fixation =
        platform::resolve_gfx_open_only_fixation(already_fixed, requested);

    if (fixation.outcome == platform::gfx_open_only_fixation_outcome::refuse) {
        return gltfx_rslt<gltfx_gl_context>::err(
            gltfx_err(gltfx_err_code::invalid_argument)
                .with_rejected_value(option_name_or_placeholder(fixation.refused_id)));
    }

    // docs/api-conventions.md R3: resolve_gfx_open_only_fixation()'s own
    // header comment - `alloc_failed` means `fixed`/`refused_id` are
    // both meaningless, and this open() must fail cleanly instead of
    // reading either.
    if (fixation.outcome == platform::gfx_open_only_fixation_outcome::alloc_failed) {
        return gltfx_rslt<gltfx_gl_context>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }

    // X-WGL (docs/plano-w6b-placa-e-laco.md fatia 4, 06/09/2026): this
    // file's own CMakeLists.txt comment already predicted it - wiring
    // gl_context_facade.cpp into glintfx_library's own build for the
    // FIRST time (this commit, once both fatia 3 and fatia 4 existed)
    // is the first time clang-tidy ever analyzed it, and bugprone-
    // unchecked-optional-access caught exactly this line: `already_
    // fixed` and `fixation.outcome` are two SEPARATE variables, and
    // gfx_open_only_fixation.cpp's own resolve_gfx_open_only_fixation()
    // sets `outcome` to `accept` ONLY when `already_fixed.has_value()`
    // is true (its own body, verbatim: "result.outcome = already_fixed.
    // has_value() ? accept : fix_now") - a real invariant, but not one
    // the analyzer can see across two variables with no visible
    // correlation between them. Checking has_value() directly, right
    // here, makes the guard local and provable - and degrades to an
    // empty span (never actually reached, by the invariant above)
    // instead of undefined behavior if that invariant is ever broken
    // by a future edit to either file, the same "never trust the
    // caller's own promise, verify locally" discipline this project's
    // own API boundary already applies at the PUBLIC edge (docs/api-
    // conventions.md), now proven cheap enough to also apply here.
    const std::span<const gltfx_gfx_option_entry> fixed_open_only =
        fixation.outcome == platform::gfx_open_only_fixation_outcome::fix_now
            ? std::span<const gltfx_gfx_option_entry>(fixation.fixed)
        : already_fixed.has_value() ? *already_fixed
                                    : std::span<const gltfx_gfx_option_entry>{};

    std::vector<gltfx_gfx_option_entry> resolved =
        resolve_full_option_table(fixed_open_only, requested);

    // FACADE-PIN (docs/plano-conserto-fachadas-uaf.md sec. 3/4/7.3/7.4,
    // varredura #7): impl is allocated FIRST, at the address it will
    // live at for the rest of its life - THEN the adapter it owns is
    // opened in place. Before this fatia, `adapter` was a local
    // variable, opened on the STACK, and only afterward moved into this
    // exact allocation - safe today only because wayland_egl_context_
    // adapter's own attach_frame_listener() (registers `this` with the
    // OS) never runs from open(), only from swap_buffers(), always
    // AFTER this move already happened (this plan's own sec. 3 #7: "e'
    // bug na primeira refatoracao que mude a ordem"). gl_context_
    // adapter_port now requires pinned_adapter<A>, which forbids the
    // type this old sequence needed to move - the ordering accident is
    // replaced by a construction that is safe BY DESIGN.
    auto *impl = new (std::nothrow) gl_context_impl{};
    if (impl == nullptr) {
        return gltfx_rslt<gltfx_gl_context>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }

    // Step 3: the concrete adapter's own open() - the same call on
    // both platforms, since platform::selected_window_adapter already
    // resolved to the right concrete type by the time this TU was
    // compiled (window_impl.hpp's own #if), and both concrete gl
    // context adapters (fatias 3/4) accept the identical (window
    // adapter, resolved option list) shape.
    const gltfx_rslt<void> opened = impl->adapter.open(
        window_impl_ptr->adapter, std::span<const gltfx_gfx_option_entry>(resolved));

    if (opened.has_error()) {
        delete impl;
        return gltfx_rslt<gltfx_gl_context>::err(opened.error());
    }

    // Only NOW, after a genuinely successful open(), does a `fix_now`
    // outcome get written back to the window - see this file's own
    // top comment for why a failed open() must never leave a window
    // believing something was fixed.
    if (fixation.outcome == platform::gfx_open_only_fixation_outcome::fix_now) {
        window_impl_ptr->fixed_open_only_gfx_options = fixation.fixed;
    }

    impl->current_values = std::move(resolved);

    return gltfx_rslt<gltfx_gl_context>::ok(gltfx_gl_context(impl));
}

gltfx_gl_context::gltfx_gl_context(gltfx_gl_context &&other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

gltfx_gl_context &gltfx_gl_context::operator=(gltfx_gl_context &&other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

gltfx_gl_context::~gltfx_gl_context() { delete m_impl; }

bool gltfx_gl_context::is_open() const noexcept {
    return m_impl != nullptr && m_impl->adapter.is_open();
}

gltfx_rslt<void> gltfx_gl_context::make_current() noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::make_current() called on a moved-from context - the object no "
           "longer owns an adapter");
    return m_impl->adapter.make_current();
}

gltfx_rslt<gltfx_present_outcome> gltfx_gl_context::swap_buffers() noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::swap_buffers() called on a moved-from context - the object no "
           "longer owns an adapter");
    return m_impl->adapter.swap_buffers();
}

void *gltfx_gl_context::proc_address(std::string_view name) const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::proc_address() called on a moved-from context - the object no "
           "longer owns an adapter");
    return m_impl->adapter.proc_address(name);
}

gltfx_rslt<void> gltfx_gl_context::set_option(gltfx_gfx_option_entry entry) noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::set_option() called on a moved-from context - the object no "
           "longer owns an adapter");

    if (const gltfx_rslt<void> shape_ok =
            platform::validate_gfx_option_entry(entry, /*already_open=*/true);
        shape_ok.has_error()) {
        return gltfx_rslt<void>::err(shape_ok.error());
    }

    // D-W6b-16's own "nunca degrada em silencio": a system that does
    // not have this option refuses BY NAME, never silently ignoring
    // the request.
    if (m_impl->adapter.option_support(entry.id) == gltfx_gfx_option_support::unsupported_here) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported)
                .with_rejected_value(option_name_or_placeholder(entry.id)));
    }

    if (const gltfx_rslt<void> applied = m_impl->adapter.apply_option(entry); applied.has_error()) {
        return gltfx_rslt<void>::err(applied.error());
    }

    for (gltfx_gfx_option_entry &current : m_impl->current_values) {
        if (current.id == entry.id) {
            current.value = entry.value;
            break;
        }
    }

    return gltfx_rslt<void>::ok();
}

gltfx_rslt<std::int64_t> gltfx_gl_context::option(gltfx_gfx_option id) const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::option() called on a moved-from context - the object no longer "
           "owns an adapter");

    for (const gltfx_gfx_option_entry &current : m_impl->current_values) {
        if (current.id == id) {
            return gltfx_rslt<std::int64_t>::ok(current.value);
        }
    }

    return gltfx_rslt<std::int64_t>::err(
        gltfx_err(gltfx_err_code::not_found).with_rejected_value(option_name_or_placeholder(id)));
}

gltfx_gfx_option_support gltfx_gl_context::option_support(gltfx_gfx_option id) const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::option_support() called on a moved-from context - the object no "
           "longer owns an adapter");
    return m_impl->adapter.option_support(id);
}

gltfx_gpu_info gltfx_gl_context::gpu() const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::gpu() called on a moved-from context - the object no longer owns "
           "an adapter");
    return m_impl->adapter.gpu();
}

} // namespace glintfx
