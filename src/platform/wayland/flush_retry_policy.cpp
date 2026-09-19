// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/wayland/flush_retry_policy.hpp"

namespace glintfx::platform {

bool flush_write_wait_is_fatal(bounded_wait_outcome outcome) noexcept {
    // WL-WRITE-TIMEOUT-NAO-FATAL: timed_out nunca e' fatal (ver o
    // header comment deste arquivo - nenhum cliente de referencia mata
    // a conexao por tempo de escrita esgotado); poll_failed continua
    // fatal sempre, e' a UNICA coisa que ainda prova conexao quebrada.
    return outcome == bounded_wait_outcome::poll_failed;
}

} // namespace glintfx::platform
