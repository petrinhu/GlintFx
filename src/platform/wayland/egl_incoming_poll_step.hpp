// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include "platform/wayland/incoming_poll_syscall.hpp"

struct wl_display;

// platform/wayland/egl_incoming_poll_step.hpp - CONT-WARMUP C-6, EMENDA
// (ordem do team-lead sobre o residual declarado da primeira rodada de
// C-6: testar a FIAÇÃO real dos dois adaptadores, não só o átomo de
// decisão que ela consome). `poll_and_dispatch_with_budget()` vivia
// dentro de um `namespace { ... }` TU-local em egl_context_adapter.cpp
// - sem linkage externo, chamável só de dentro da mesma translation
// unit. Este cabeçalho é a ÚNICA razão pela qual ela agora tem linkage
// comum: dar a tests/incoming_poll_wiring_test.cpp (OUTRA translation
// unit) um jeito de chamar a função REAL, não uma cópia.
//
// INTERNO POR DESENHO (src/, nunca include/glintfx/ - GODS_LAWS.md
// L-19): nenhum consumidor da biblioteca inclui isto. A implementação
// continua em egl_context_adapter.cpp; este cabeçalho só existe para
// permitir a UMA segunda translation unit (o teste) enxergar o símbolo.
//
// `poll_impl` (src/platform/wayland/incoming_poll_syscall.hpp), com
// `&::poll` como padrão: a costura que o teste substitui por uma
// função script, sem custo nenhum para o único chamador de produção
// (swap_buffers(), mesma translation unit, sempre usa o padrão).
namespace glintfx::platform {

[[nodiscard]] bool
poll_and_dispatch_with_budget(wl_display *display, std::uint32_t budget_ms,
                              incoming_poll_syscall_fn poll_impl = &::poll) noexcept;

} // namespace glintfx::platform
