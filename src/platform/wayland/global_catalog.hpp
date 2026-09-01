// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// global_catalog.hpp - WL-DISPLAY fatia A (TODO.md, GODS_LAWS.md
// L-19/L-20): the pure, Wayland-free half of the registry - a plain
// record of every (name, interface, version) triple a wl_registry
// listener announces, plus the version-clamp rule wayland-book's
// registry/binding chapter documents as the universal binding
// technique. No <wayland-client.h> anywhere in this file or its .cpp:
// this atom is testable without a compositor, without even libwayland
// linked in - fatia B (the registry listener itself) is the only
// thing that ever calls insert()/remove() from real Wayland callbacks.
//
// L-19 opacity does not reach this file (it is not a handle over a
// system resource, just a value type), but it stays INTERNAL all the
// same: lives under src/platform/wayland/, never under include/
// glintfx/ - no consumer of the public library API ever sees a
// wayland_global.

namespace glintfx::platform {

// One entry of the registry's own announcement: the numeric name a
// bind() call needs, the interface string it identifies, and the
// version the SERVER (not us) is offering for it.
struct wayland_global {
    std::uint32_t name = 0;
    std::string interface;
    std::uint32_t version = 0;
};

// A flat, unordered catalog of the globals currently announced. No
// index by name beyond linear search: the whole registry for a
// realistic compositor is a few dozen entries, and this fatia's own
// scope (3.1.A of the plan) is cataloging, not a performance-critical
// lookup path - see global_catalog.cpp's own header comment for why
// std::vector<wayland_global> was chosen over anything fancier.
class global_catalog {
  public:
    // Records a newly-announced global. GODS_LAWS.md L-17 "avisar o
    // gemeo": a name already present (the server re-announcing the
    // same numeric name, which real compositors do not do, but this
    // function does not assume that) is REPLACED in place, never
    // duplicated - find_by_name()'s own single-match guarantee below
    // depends on this.
    //
    // NOEXCEPT BY CONTRACT, not by luck: the underlying std::string/
    // std::vector operations CAN throw std::bad_alloc, and this
    // function catches that internally rather than letting it escape -
    // display_adapter.cpp's registry_global() is a callback invoked by
    // libwayland-client's own C event-dispatch machinery, which gives
    // it no exception-handling contract at all (GODS_LAWS.md L-22).
    // Returns false when the allocation failed and this global was NOT
    // recorded (the catalog is left exactly as it was before the
    // call - a failed replace does not corrupt the existing entry, a
    // failed new insert leaves size() unchanged); true otherwise. This
    // makes a low-memory catalog OBSERVABLE to whoever calls insert()
    // and checks the return, instead of the previous design (a
    // try/catch at the CALL site that swallowed the failure with no
    // signal at all).
    bool insert(std::uint32_t name, std::string interface, std::uint32_t version) noexcept;

    // Erases the global with this NAME - the only key wl_registry's
    // own global_remove event carries (wayland-book, registry chapter:
    // "a client only receives the numeric name back, never the
    // interface string it once had"). A name this catalog never held,
    // or held and already erased, is a silent no-op: the plan's own
    // "remover-inexistente" red case exists exactly to prove this
    // never throws, crashes or corrupts the rest of the catalog.
    void remove(std::uint32_t name) noexcept;

    // First entry whose interface STRING matches, or nullptr if none
    // does. A compositor exposes at most one instance of most
    // interfaces (wl_compositor, xdg_wm_base) but can expose several
    // of others (wl_output, one per monitor) - "first match" is
    // exactly what a caller only interested in "is this interface
    // present at all" (this fatia's own WL-DISPLAY red case B) needs;
    // enumerating every instance of a multi-instance interface is a
    // later fatia's problem (WL-WINDOW/WL-SEAT, not this one).
    [[nodiscard]] const wayland_global *
    find_by_interface(std::string_view interface) const noexcept;

    // Entry with this exact numeric name, or nullptr. Read path for a
    // caller (fatia B's registry listener) that already knows the
    // name from a global_remove event and wants to log/report what it
    // is about to erase before calling remove() above.
    [[nodiscard]] const wayland_global *find_by_name(std::uint32_t name) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return m_globals.size(); }

    // The universal Wayland client-binding rule (wayland-book,
    // registry/binding chapter; SDL3's own SDL_min() at the bind call
    // site, read to learn the technique, GODS_LAWS.md L-29 - not
    // copied): binding to a version ABOVE what the server just
    // announced is a protocol error the server is entitled to kill
    // the connection over, so every bind() call in this project clamps
    // to whichever of "what THIS build of glintfx understands" and
    // "what the server just offered" is smaller. A free function
    // rather than a member: it takes no catalog state at all, only
    // the two numbers a caller already has in hand.
    [[nodiscard]] static std::uint32_t clamp_version(std::uint32_t supported,
                                                     std::uint32_t announced) noexcept;

  private:
    std::vector<wayland_global> m_globals;
};

} // namespace glintfx::platform
