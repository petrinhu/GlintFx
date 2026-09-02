# wayland-display.md

> **Internal engineering document, not a public API reference.** Everything described here lives under `src/platform/wayland/` - no header under `include/glintfx/`, no `GLINTFX_API` symbol, nothing installed. It exists to explain how glintfx talks to a Wayland compositor today, for whoever extends the platform layer next (a window, a seat, keyboard input): none of these shapes, names, or files are a promise to a consumer, and any of them can change without a version bump. Read [`docs/api-conventions.md`](api-conventions.md) for the one boundary that IS a promise: the error type every fallible call in this project returns, public or not.

## What this covers, and what it does not

`glintfx::platform::wayland_display_adapter` (`src/platform/wayland/display_adapter.hpp`) is the one thing on this platform that owns a live connection to a Wayland compositor. As of this writing it can: connect, discover what the compositor offers, resynchronize on demand, and pump incoming events without blocking a caller's own frame loop. It cannot yet create a window, bind a seat, or read a single keystroke - those are later slices built on top of the same class, not a second competing one.

## Opening a connection

`open()` does three things, in order, and only reports success once all three have happened:

1. Connects to the compositor the same way every Wayland client does - reading `WAYLAND_DISPLAY`, falling back to the socket named `wayland-0` inside `XDG_RUNTIME_DIR` when that variable is unset. There is no way to point this at a hand-picked socket name, and none is planned: a consumer that needs that is not this library's target.
2. Asks the compositor for its registry - the object that will announce every capability (interface) the compositor offers.
3. Performs one round trip with the compositor before returning. This is what closes the initial burst of announcements every compositor sends right after the registry is created: by the time `open()` returns successfully, `globals()` (below) already lists everything that was available at connection time.

Any refusal along this path - no compositor listening, the registry request rejected, the round trip itself failing - comes back as an ordinary `glintfx::gltfx_rslt<void>` error (`gltfx_err_code::platform_failure`), never an exception, never a process-ending signal. `open()` also cleans up after itself on failure: nothing is left half-open. `is_open()` tells a caller whether a connection is currently held.

## What the compositor announced: the global catalog

Once open, `globals()` returns a flat catalog of every capability the compositor is currently offering - `wl_compositor`, `xdg_wm_base`, one `wl_output` per monitor, and so on. Each entry carries three things: the numeric name a later bind call would need, the interface name (a string like `"wl_seat"`), and the version the compositor is offering for it - not the version this library understands, the version the *server* announced. The catalog stays current after the initial round trip too: as the compositor adds or removes a capability at runtime (a monitor plugged in, one unplugged), the adapter updates the catalog on its own, with no action required from the caller.

Two ways to read the catalog: look up whether a given interface is present at all (the common case - "does this compositor even offer `xdg_wm_base`"), or look up what was announced under one specific numeric name. A compositor typically offers only one instance of most interfaces but several of others (one `wl_output` per monitor); the catalog does not hide that, it just does not yet offer a convenience for enumerating "every instance of X" - that is future work, once something in this library actually needs it.

**Binding to a global, an obligation on the caller, not on this class:** any future code that binds one of these globals must never ask for a version higher than what the compositor just announced - doing so is a protocol violation the compositor is entitled to kill the connection over. The adapter offers a small helper (`global_catalog::clamp_version`) that keeps a bind request to whichever is smaller, what this build understands or what the server just offered; every future bind call in this project is expected to run through it, not to trust a hand-picked version number.

## Resynchronizing on demand: `roundtrip()`

Beyond the one round trip `open()` performs internally, any later code (a window, a seat) that needs to know "has the compositor definitely processed my last request yet" calls `roundtrip()` directly. It blocks until every event already in flight has been handled - the opposite of the event pump described below, which never blocks.

**A Wayland connection that reports one failed protocol operation is permanently unusable from that point on** - this is not a glintfx rule, it is the documented contract of `wl_display` itself. The adapter honors that literally: the first time any protocol operation on a connection fails, the adapter remembers it (`has_fatal_error()`) and never issues another real protocol call on that connection again. Every subsequent `roundtrip()` (or event-pump call) on a connection in that state returns the same diagnostic immediately, without touching the compositor a second time. This is a separate question from `is_open()`: the connection handle is still owned and still needs to be closed to free it, even once it can no longer be used for anything.

When a failure is fatal, the diagnostic carries what the compositor itself reported: the raw OS-level error code always, and - specifically when the failure was a protocol violation - the name of the interface whose request the compositor rejected, attached as the error's "rejected value" (`docs/api-conventions.md`'s token-vocabulary convention: an interface name like `"wl_compositor"` is itself an identifier, never a sentence).

## Pumping events without blocking: `pump_events()`

`roundtrip()` is for "I need an answer now, and I'm willing to wait." A consumer's own frame loop needs the opposite: "is there anything from the compositor RIGHT NOW, and if not, get back to me immediately." `pump_events()` is that call - safe to invoke every single frame without ever stalling the loop that calls it.

What it does, each time it's called:

1. First works through anything already received but not yet handled.
2. Sends out anything this process still owes the compositor (a request queued but not yet flushed to the socket) - and if the kernel's own send buffer is temporarily full, it waits only for room to write, not for a response; this is the one place `pump_events()` can briefly block, and only until the OS confirms it can accept the next write.
3. Checks, without waiting even a moment, whether the compositor has sent anything new.
4. If yes, reads and processes it; if no, returns immediately with nothing left to do.

This is the exact sequence Wayland's own client library documents as the only safe way to integrate its event delivery into a caller-owned loop - a naive "just poll and dispatch" ordering has more than one way to deadlock the connection, and this method exists specifically so nothing above it has to know that.

## What this is not, yet

There is no window here. `wayland_display_adapter` connects, catalogs, resynchronizes, and pumps - it does not create a surface, does not know about `xdg_wm_base` beyond seeing it listed in the catalog, and does not read keyboard or pointer input. Those are later slices (a window, a seat) built as siblings inside the same `src/platform/wayland/` directory, extending this adapter's own responsibilities rather than introducing a second, competing connection type.

## Where to look for the code itself

`src/platform/wayland/display_adapter.hpp`/`.cpp` (the adapter and its four operations above) and `src/platform/wayland/global_catalog.hpp`/`.cpp` (the catalog, deliberately free of any Wayland header - it is a plain record type, testable without a compositor at all). Both files carry extensive comments on the exact sequencing and the specific pitfalls each step avoids; this document explains the shape and the why, the source is where the line-by-line mechanics live.
