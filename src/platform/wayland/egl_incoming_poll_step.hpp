// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include "platform/wayland/incoming_poll_reaction.hpp"
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
//
// `reaction_impl` (src/platform/wayland/incoming_poll_reaction.hpp,
// CONT-WARMUP C-8): a MESMA ideia, um nível acima - com
// `&is_incoming_poll_connection_fatal` como padrão, é a costura que
// tests/incoming_poll_wiring_test.cpp usa para injetar um átomo
// DIVERGENTE e provar que este call site de fato consulta a decisão em
// vez de a hardcoded. Custo em produção: zero, o padrão é o mesmo átomo
// que já rodava antes desta fatia.
namespace glintfx::platform {

[[nodiscard]] bool poll_and_dispatch_with_budget(
    wl_display *display, std::uint32_t budget_ms, incoming_poll_syscall_fn poll_impl = &::poll,
    incoming_poll_reaction_fn reaction_impl = &is_incoming_poll_connection_fatal) noexcept;

} // namespace glintfx::platform
