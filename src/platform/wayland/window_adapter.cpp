// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/window_adapter.hpp"

#include <new>
#include <optional>
#include <span>
#include <string>

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "platform/wayland/display_adapter.hpp"
#include "platform/wayland/shell_adapter.hpp"
#include "platform/window/window_desc_validation.hpp"

// window_adapter.cpp - see window_adapter.hpp's own header comment for
// scope. Every *_configure/*_close/*_enter/*_leave/*_preferred_buffer_
// scale callback below is called back by libwayland-client's own C
// event-dispatch machinery - GODS_LAWS.md L-22: no exception may unwind
// across that C stack frame, which is why none of them ever calls
// anything that can throw (window_state's own setters are all noexcept,
// window_configure_sequence::apply_configure() is noexcept).

namespace glintfx::platform {

namespace {

constexpr wl_surface_listener kSurfaceListener = {
    .enter = &wayland_window_adapter::wl_surface_enter,
    .leave = &wayland_window_adapter::wl_surface_leave,
    .preferred_buffer_scale = &wayland_window_adapter::wl_surface_preferred_buffer_scale,
    // preferred_buffer_transform (v6, wl_surface's own sibling event to
    // preferred_buffer_scale) is a NAMED NO-OP, never nullptr - see
    // wayland_window_adapter::wl_surface_preferred_buffer_transform()'s
    // own header comment (window_adapter.hpp) for why. CORRECTION of
    // this struct's earlier assumption: libwayland-client's dispatcher
    // does NOT skip a null callback for an event the bound version
    // makes legal - it logs "listener function for opcode N of
    // INTERFACE is NULL" and calls abort() (SIGABRT), measured here
    // against tests/container/'s own kwin_wayland --virtual the moment
    // window_smoke.cpp's surface received this exact event (opcode 3
    // of wl_surface) - same class of defect the sibling xdg_toplevel_
    // listener below already documents for wm_capabilities.
    .preferred_buffer_transform = &wayland_window_adapter::wl_surface_preferred_buffer_transform,
};

constexpr xdg_surface_listener kXdgSurfaceListener = {
    .configure = &wayland_window_adapter::xdg_surface_configure,
};

constexpr xdg_toplevel_listener kToplevelListener = {
    .configure = &wayland_window_adapter::xdg_toplevel_configure,
    .close = &wayland_window_adapter::xdg_toplevel_close,
    // configure_bounds (v4) and wm_capabilities (v5) are NAMED NO-OPS,
    // never nullptr - see wayland_window_adapter::xdg_toplevel_
    // configure_bounds()'s own header comment (window_adapter.hpp) for
    // why a null entry here is a guaranteed SIGABRT, not a harmless
    // omission, once xdg_wm_base is bound at version 5 or above
    // (shell_adapter.cpp binds it at 6).
    .configure_bounds = &wayland_window_adapter::xdg_toplevel_configure_bounds,
    .wm_capabilities = &wayland_window_adapter::xdg_toplevel_wm_capabilities,
};

} // namespace

wayland_window_adapter::~wayland_window_adapter() { close(); }

void wayland_window_adapter::wl_surface_enter(void * /*data*/, wl_surface * /*surface*/,
                                              wl_output * /*output*/) noexcept {
    // No-op, named deliberately - see this method's own declaration
    // comment in window_adapter.hpp.
}

void wayland_window_adapter::wl_surface_leave(void * /*data*/, wl_surface * /*surface*/,
                                              wl_output * /*output*/) noexcept {
    // No-op, same reasoning as wl_surface_enter() above.
}

void wayland_window_adapter::wl_surface_preferred_buffer_scale(void *data, wl_surface * /*surface*/,
                                                               std::int32_t factor) noexcept {
    auto *self = static_cast<wayland_window_adapter *>(data);
    self->m_state.apply_buffer_scale(static_cast<std::uint32_t>(factor));
}

void wayland_window_adapter::wl_surface_preferred_buffer_transform(
    void * /*data*/, wl_surface * /*surface*/, std::uint32_t /*transform*/) noexcept {
    // No-op, named deliberately - see this method's own declaration
    // comment in window_adapter.hpp.
}

void wayland_window_adapter::xdg_toplevel_configure(void *data, xdg_toplevel * /*toplevel*/,
                                                    std::int32_t width, std::int32_t height,
                                                    wl_array *states) noexcept {
    auto *self = static_cast<wayland_window_adapter *>(data);
    self->m_pending_width = static_cast<std::uint32_t>(width);
    self->m_pending_height = static_cast<std::uint32_t>(height);

    self->m_pending_states_count = 0;
    if (states != nullptr) {
        const auto *values = static_cast<const std::int32_t *>(states->data);
        const std::size_t value_count = states->size / sizeof(std::int32_t);
        for (std::size_t i = 0;
             i < value_count && self->m_pending_states_count < self->m_pending_states.size(); ++i) {
            self->m_pending_states[self->m_pending_states_count] = values[i];
            ++self->m_pending_states_count;
        }
    }
}

void wayland_window_adapter::xdg_toplevel_close(void *data, xdg_toplevel * /*toplevel*/) noexcept {
    auto *self = static_cast<wayland_window_adapter *>(data);
    self->m_state.request_close();
}

void wayland_window_adapter::xdg_toplevel_configure_bounds(void * /*data*/,
                                                           xdg_toplevel * /*toplevel*/,
                                                           std::int32_t /*width*/,
                                                           std::int32_t /*height*/) noexcept {
    // No-op, named deliberately - see this method's own declaration
    // comment in window_adapter.hpp.
}

void wayland_window_adapter::xdg_toplevel_wm_capabilities(void * /*data*/,
                                                          xdg_toplevel * /*toplevel*/,
                                                          wl_array * /*capabilities*/) noexcept {
    // No-op, named deliberately - see this method's own declaration
    // comment in window_adapter.hpp.
}

void wayland_window_adapter::xdg_surface_configure(void *data, xdg_surface * /*surface*/,
                                                   std::uint32_t serial) noexcept {
    auto *self = static_cast<wayland_window_adapter *>(data);
    const window_configure_result result = self->m_configure_sequence.apply_configure(
        self->m_pending_width, self->m_pending_height,
        std::span<const std::int32_t>(self->m_pending_states.data(), self->m_pending_states_count),
        serial);

    // THE 0x0 RULE, APPLIED (window_configure_sequence.hpp's own header
    // comment): has_size == false means "keep whatever size you already
    // have" - apply_desc() (below) already set m_state's logical_size
    // to the REQUESTED size before the first configure ever arrives, so
    // "keep it" here means simply not touching it, never re-deriving a
    // fallback of our own.
    if (result.has_size) {
        self->m_state.apply_logical_size(result.width, result.height);
    }
    self->m_state.set_state(window_state_bit::maximized, result.maximized);
    self->m_state.set_state(window_state_bit::fullscreen, result.fullscreen);
    self->m_state.set_state(window_state_bit::active, result.activated);
    // D-W6b-51: only a compositor bound at xdg_wm_base version >= 6
    // ever sends this code (window_configure_sequence.hpp's own header
    // comment) - result.suspended is simply always false below that,
    // so this line needs no version check of its own.
    self->m_state.set_state(window_state_bit::suspended, result.suspended);
    self->m_configured = true;
}

gltfx_rslt<void> wayland_window_adapter::create_surface(wayland_shell_adapter &shell) noexcept {
    // Refusal, not a null pointer three calls later (docs/plano-w6a-
    // janela.md "onde esta o perigo" item 1, same shape display_
    // adapter.hpp's own bind() comment already documents): a shell
    // never opened means compositor()/shell() both read null, and
    // wl_compositor_create_surface() below would dereference one.
    if (!shell.is_open()) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::not_found).with_rejected_value("wl_compositor"));
    }

    wl_surface *surface = wl_compositor_create_surface(shell.compositor());
    if (surface == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("wl_surface"));
    }

    wl_surface_add_listener(surface, &kSurfaceListener, this);
    m_surface = surface;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_window_adapter::create_xdg_surface(wayland_shell_adapter &shell) noexcept {
    // shell.shell() is guaranteed non-null here: create_surface() above
    // already refused when shell.is_open() was false, and open() (this
    // file's own entry point) never calls this method otherwise.
    xdg_surface *xdg_surf = xdg_wm_base_get_xdg_surface(shell.shell(), m_surface);
    if (xdg_surf == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("xdg_surface"));
    }

    xdg_surface_add_listener(xdg_surf, &kXdgSurfaceListener, this);
    m_xdg_surface = xdg_surf;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_window_adapter::create_toplevel() noexcept {
    xdg_toplevel *toplevel = xdg_surface_get_toplevel(m_xdg_surface);
    if (toplevel == nullptr) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("xdg_toplevel"));
    }

    xdg_toplevel_add_listener(toplevel, &kToplevelListener, this);
    m_xdg_toplevel = toplevel;
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_window_adapter::apply_desc(const wayland_window_desc &desc) noexcept {
    // D-W6a-23: validated BEFORE either backend ever sees the bytes -
    // see window_desc_validation.hpp's own header comment for why.
    if (gltfx_rslt<void> title_ok = validate_window_text_field("title", desc.title);
        title_ok.has_error()) {
        return title_ok;
    }
    if (gltfx_rslt<void> app_id_ok =
            validate_window_text_field("application_id", desc.application_id);
        app_id_ok.has_error()) {
        return app_id_ok;
    }

    // Sets the REQUESTED size proactively, before the first configure
    // ever arrives - the size xdg_surface_configure()'s own 0x0 rule
    // keeps when the compositor defers the decision back to the client
    // (xdg-shell.xml, xdg_toplevel.configure: "if the width or height
    // arguments are zero, it means the client should decide its own
    // window dimension").
    m_state.apply_logical_size(desc.logical_width, desc.logical_height);

    // Empty is always accepted (window_desc_validation.hpp: "v1 never
    // requires a title or an application_id") and simply skips the
    // request - xdg-shell.xml requires neither.
    //
    // CONSERTO (varredura de 07/09/2026, mesma familia de bugs de gfx_
    // open_only_fixation.cpp): std::string(...) below CAN throw std::
    // bad_alloc despite this function's own noexcept - GODS_LAWS.md
    // L-22, letting it escape would call std::terminate() and kill the
    // consumer's process on the very path that opens a window. Unlike
    // global_catalog.hpp's own insert() (a C-callback with no error
    // channel at all), apply_desc() ALREADY returns gltfx_rslt<void> -
    // so the honest desfecho here is the error, not a silent skip that
    // would leave the window open with the wrong title.
    if (!desc.title.empty()) {
        try {
            xdg_toplevel_set_title(m_xdg_toplevel, std::string(desc.title).c_str());
        } catch (const std::bad_alloc &) {
            return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::out_of_memory));
        }
    }
    if (!desc.application_id.empty()) {
        try {
            xdg_toplevel_set_app_id(m_xdg_toplevel, std::string(desc.application_id).c_str());
        } catch (const std::bad_alloc &) {
            return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::out_of_memory));
        }
    }

    return gltfx_rslt<void>::ok();
}

void wayland_window_adapter::commit_initial() noexcept { wl_surface_commit(m_surface); }

gltfx_rslt<void>
wayland_window_adapter::wait_first_configure(wayland_display_adapter &connection) noexcept {
    // Budget generous on purpose (same reasoning pump_smoke.cpp's own
    // header comment already documents for a CI container slower than
    // the leader's own machine): wl_display_roundtrip() blocks until
    // the compositor answers, so ONE round-trip is normally enough to
    // both receive and dispatch the xdg_toplevel.configure +
    // xdg_surface.configure pair this adapter's own commit_initial()
    // triggers - this loop only guards against a compositor that
    // defers the sequence across more than one round-trip.
    constexpr int kMaxRoundtrips = 50;
    for (int i = 0; i < kMaxRoundtrips && !m_configured; ++i) {
        if (gltfx_rslt<void> roundtripped = connection.roundtrip(); roundtripped.has_error()) {
            return roundtripped;
        }
    }
    if (!m_configured) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::platform_failure).with_rejected_value("xdg_surface"));
    }

    // D-W5-7 (ack once per serial, impossible to double): take_serial_
    // to_ack() reads AND clears the pending serial in the same call.
    // THIS is the line the adversarial reviewer removes to prove
    // window_smoke.cpp's own mutation case - see this method's own
    // declaration comment in window_adapter.hpp for the full chain from
    // "no ack" to "xdg_surface.error.unconfigured_buffer" once the
    // fixture goes on to attach a real buffer (D-W5-9).
    if (std::optional<std::uint32_t> serial = m_configure_sequence.take_serial_to_ack();
        serial.has_value()) {
        xdg_surface_ack_configure(m_xdg_surface, *serial);
    }

    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_window_adapter::open(wayland_display_adapter &connection,
                                              wayland_shell_adapter &shell,
                                              const wayland_window_desc &desc) noexcept {
    if (gltfx_rslt<void> surfaced = create_surface(shell); surfaced.has_error()) {
        return surfaced;
    }
    if (gltfx_rslt<void> xdg_surfaced = create_xdg_surface(shell); xdg_surfaced.has_error()) {
        close();
        return xdg_surfaced;
    }
    if (gltfx_rslt<void> topleveled = create_toplevel(); topleveled.has_error()) {
        close();
        return topleveled;
    }
    if (gltfx_rslt<void> desc_applied = apply_desc(desc); desc_applied.has_error()) {
        close();
        return desc_applied;
    }

    commit_initial();

    if (gltfx_rslt<void> configured = wait_first_configure(connection); configured.has_error()) {
        close();
        return configured;
    }

    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> wayland_window_adapter::set_title(std::string_view title) noexcept {
    if (!is_open()) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("title"));
    }
    if (gltfx_rslt<void> title_ok = validate_window_text_field("title", title);
        title_ok.has_error()) {
        return title_ok;
    }

    // Same "empty is a real, always-accepted request" shape apply_desc()
    // above documents for open()-time - but unlike open() (which SKIPS
    // the protocol call entirely for an empty title, xdg-shell.xml
    // requiring neither), a caller who explicitly asks to CLEAR the
    // title mid-session gets that request honored for real, the same
    // way win32_window_adapter::set_title()'s own SetWindowTextW call
    // never special-cases an empty string either.
    //
    // GODS_LAWS.md L-22: same guard apply_desc() above already applies
    // to the identical std::string(...) construction - this function
    // also has a gltfx_rslt<void> to carry the honest failure through.
    try {
        xdg_toplevel_set_title(m_xdg_toplevel, std::string(title).c_str());
    } catch (const std::bad_alloc &) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
    return gltfx_rslt<void>::ok();
}

void wayland_window_adapter::close() noexcept {
    // Reverse order of creation (same convention wayland_display_
    // adapter::close()/wayland_shell_adapter::close() already document).
    if (m_xdg_toplevel != nullptr) {
        xdg_toplevel_destroy(m_xdg_toplevel);
        m_xdg_toplevel = nullptr;
    }
    if (m_xdg_surface != nullptr) {
        xdg_surface_destroy(m_xdg_surface);
        m_xdg_surface = nullptr;
    }
    if (m_surface != nullptr) {
        wl_surface_destroy(m_surface);
        m_surface = nullptr;
    }
    m_configured = false;
    m_pending_width = 0;
    m_pending_height = 0;
    m_pending_states_count = 0;
}

} // namespace glintfx::platform
