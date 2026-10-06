// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_frame_writer.hpp"

#include <cstddef>
#include <string>

// wire_capture_verdict.hpp - the VERDICT atom of the wire relay
// (QA-SCREEN-CAPTURE P1, D-W8-32): says whether a capture_report is
// acceptable and how the relay words it. Separate from the writer
// (which only does disk I/O) so each changes for one reason.
//
// Scope of the verdict: ONE connection. The plan puts the "zero images
// FAILS" judgement over the WHOLE run in the reader of these files
// (plano-w8-v3.md SS2 D-W8-32: "O relatorio conta isso, e zero imagens
// reprova"; SS5 item 5(i): "pelo menos uma imagem por alvo"), i.e. in
// QA-SCREEN-CAPTURE P2/P3 - a connection with no frame is normal for
// e.g. a probe connection.
namespace glintfx::test::wire_relay {

// GODS_LAWS.md L-40: a count of zero frames FAILS, and so does any
// file that could not be written - never a green "nothing to see".
[[nodiscard]] bool capture_passes(const capture_report &report);

// The one-line verdict the relay prints for a closed connection; the
// count shows even when it is zero.
[[nodiscard]] std::string describe_capture(const capture_report &report,
                                           std::size_t connection_serial);

} // namespace glintfx::test::wire_relay
