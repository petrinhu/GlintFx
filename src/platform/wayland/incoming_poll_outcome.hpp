// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// platform/wayland/incoming_poll_outcome.hpp - CONT-WARMUP C-2 (docs/
// plano-fecho-w7b.md D-10, GODS_LAWS.md L-20): the ONE pure atom
// display_adapter.cpp::wait_for_incoming_data() calls to decide what a
// completed poll() on the READ side means - the same "policy pulled
// out of the syscall-touching function into its own testable atom"
// shape this project already uses for the write-side twin
// (flush_retry_policy.hpp, this same directory) and for frame_
// callback_sequence.hpp/gl_version_policy.hpp. No `wl_display`, no
// `int fd`, no real `poll()` call reachable from this header or its
// .cpp - only the two plain values a caller already has in hand right
// after its own `::poll()` returns: the raw return code and the
// `revents` bitmask. GODS_LAWS.md L-20's own declared TDD exception
// ("adaptador que so encaminha chamada ao sistema operacional... o
// teste honesto e' de integracao e nao unitario") does NOT apply to
// THIS atom - it touches no OS surface at all, so it gets an ordinary
// unit test (tests/incoming_poll_outcome_test.cpp) with synthetic
// revents, no socket, no container.
//
// PROVA POR SEAM, NAO POR CENARIO REAL (achado do orquestrador, esta
// fatia - relatorio em /var/tmp/glintfx-plan/impl-cont-warmup.md): o
// caminho real (kwin_wayland --virtual, matado por `pkill`) NUNCA
// entrega POLLHUP sem POLLIN ao vivo - medido, quatro vezes, sempre
// POLLIN|POLLHUP juntos (este kernel reporta POLLIN sempre que read()
// nao vai bloquear, o que inclui devolver 0/EOF). O ramo que ESTE
// atomo corrige (POLLHUP/POLLERR sem POLLIN; POLLNVAL) fica sem
// testemunha de container por desenho, nao por descuido - a unica
// forma honesta de provar que ele existe e se comporta certo e'
// exercita-lo diretamente, com valores sinteticos, o que este atomo
// agora permite.
namespace glintfx::platform {

enum class incoming_poll_outcome : std::uint8_t {
    nothing_yet,      // budget exhausted, or a spurious wake with none of
                      // POLLIN/POLLHUP/POLLERR/POLLNVAL set - ordinary
                      // "nothing arrived yet", never fatal by itself.
    ready_to_read,    // proceed to read_and_dispatch_incoming() - covers
                      // plain POLLIN AND POLLHUP/POLLERR without POLLIN
                      // (poll(2)'s own contract: a hangup can still have
                      // real buffered data ahead of the EOF; the read
                      // itself is what discovers true end-of-file).
    fatal,            // POLLNVAL: an invalid fd is never something a read
                      // could make sense of - latched before any read is
                      // attempted, never absorbed as "nothing to read".
    poll_call_failed, // poll_result < 0: ::poll() itself failed. CONT-
                      // WARMUP C-5 (revisao adversarial C-4, GODS_LAWS.md
                      // L-17 "gemeo"): a THIRD read-side call site (src/
                      // platform/wayland/egl_context_adapter.cpp) used to
                      // fold this straight into "budget exhausted" via
                      // `poll_result <= 0`, and even THIS atom's own
                      // pre-C-5 body agreed by accident - `revents` is
                      // left untouched by the kernel on a real poll(2)
                      // failure (POSIX; a caller's zero-initialized pollfd
                      // still reads 0), so `poll_result < 0` used to fall
                      // through to the SAME "spurious wake, nothing set"
                      // branch a genuine `poll_result == 0` timeout takes.
                      // WHICH errno it was (EINTR, retriable, vs anything
                      // else, a real connection failure) is real OS state
                      // this pure atom never reads (this header's own
                      // comment) - that distinction is the CALLER's job,
                      // read right after this call returns, before errno
                      // can be clobbered by anything else.
};

// `poll_result`/`revents` are exactly what a caller's own `::poll(&pfd,
// 1, timeout_ms)` just produced for `pfd.revents` - this atom performs
// no syscall of its own and reads no OS state.
[[nodiscard]] incoming_poll_outcome classify_incoming_poll(int poll_result, short revents) noexcept;

} // namespace glintfx::platform
