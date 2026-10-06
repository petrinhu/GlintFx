// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_capture_verdict.hpp"

namespace glintfx::test::wire_relay {

bool capture_passes(const capture_report &report) {
    return report.frames_saved > 0 && report.files_failed == 0;
}

std::string describe_capture(const capture_report &report, std::size_t connection_serial) {
    const std::string head = "wire_relay: capture conn" + std::to_string(connection_serial) + " - ";
    if (!capture_passes(report) && report.files_failed == 0) {
        return head + "nenhum quadro";
    }
    return head + std::to_string(report.frames_saved) + " frame(s) saved, " +
           std::to_string(report.files_failed) + " file(s) failed";
}

} // namespace glintfx::test::wire_relay
