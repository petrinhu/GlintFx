// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_region.hpp>

// draw2d/image_resolver.hpp - R2D-IMAGE-RESOLVER (docs/gfss-property-registry-v1.md,
// decision D3; decisions D-W8-15, D-W8-24 and D-W8-253 of the W8 plan).
//
// The library never opens a file and never decodes an image. A stylesheet
// writes url("...") and the library keeps that text. When a picture is
// needed, the library ASKS the consumer which texture answers that text.
// If nobody answers, the picture does not paint. In this version no draw
// call asks the resolver: the first call that asks is the one of the first
// paint bridge (GFSS-OBJECT-FIT or R2D-NINEPATCH), and the refusal reason,
// the frame report counter and the log event image_without_owner are born
// there (event below). Each paint layer is its own call, so a picture
// without an owner refuses only the picture, never the box behind it.
//
// From the first paint bridge on, the question is asked at every paint, on
// the renderer's own thread (the only thread renderer_2d.hpp allows to use
// that renderer), and the answer is used inside that paint call only: the
// library copies what it needs (the GL name and the region) before the call
// returns, and it never keeps the texture pointer. The texture belongs to
// the consumer, and so does its lifetime (GODS_LAWS.md L-19).
//
// The reference is the text the stylesheet wrote between the quotes, kept
// VERBATIM (D3): the reader in gfss decodes nothing, and the CSS escapes
// inside the text reach the consumer as they were written. A decoding helper,
// pure and called on demand, comes later (INBOX `GFSS-URL-UNESCAPE`).
// This header never sees the quotes. It never resolves an address either: a
// relative address is relative to the stylesheet that wrote it (CSS Values 4,
// section 4.5.1), and the consumer joins the two. The library has no URL
// parser (GODS_LAWS.md L-07).
//
// Event (R9), born with the first paint bridge: name "image_without_owner",
// category "draw2d.image", severity warn. Fields: reference (text, at most 256
// bytes, cut at a whole UTF-8 character, with a truncation mark; bytes that
// are not printable are written as escapes such as \n and \x0a, without
// allocation and inside noexcept) and truncated (bool). It is emitted only
// when the first unowned reference of a frame differs from the one the
// previous frame reported. A frame with no unowned reference clears that
// memory, so "A, nothing, A" emits twice.
//
// Proved by: image_resolution_test (what this version does, asked
// directly: install and swap, the null function, the context, the text and
// the answer), renderer_2d_impl_test (each renderer keeps its own resolver)
// and draw2d_parity_test (the same through gltfx_renderer_2d, two renderers
// open at once on real contexts). The paint, the refusal, the counter and
// the event above are not in this version, and no test stands for them yet.

namespace glintfx {

// What the library hands the resolver. The library builds it on its own
// stack, once per question. The consumer reads it and never builds it. The
// text is valid only during the resolver call: copy what you keep before you
// return. Never copy this struct, never take its size, and never keep a
// pointer to it after the call.
struct gltfx_image_reference {
    std::string_view text{};
};

// What the consumer answers. texture == nullptr means "no owner": the
// picture does not paint. The texture is borrowed for that one paint call and
// must stay alive through it, as in the texture drawing calls.
struct gltfx_image_resolution {
    const gltfx_texture_2d *texture = nullptr;
    gltfx_texture_region region{};
};

// A plain function pointer and an opaque context, never std::function (R8).
// noexcept is part of the type: a resolver that is not noexcept does not
// convert to this type (R8, reason 4). That noexcept is a PROMISE, not a
// proof (R10, layer 1): a resolver declared noexcept that throws anyway ends
// the consumer's process (std::terminate) the instant the exception tries to
// leave it, and nothing in this library, or on either side of this boundary,
// can stop that.
using gltfx_image_resolver_fn =
    gltfx_image_resolution (*)(void *context, const gltfx_image_reference *reference) noexcept;

// The registration. The context is BORROWED (R10, layer 2: there is no
// destroy function): the library never destroys it, and the consumer keeps
// it alive while the resolver is installed. function == nullptr means "no
// resolver": every reference is then unowned.
struct gltfx_image_resolver {
    gltfx_image_resolver_fn function = nullptr;
    void *context = nullptr;
};

} // namespace glintfx
