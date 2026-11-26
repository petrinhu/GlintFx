// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_frame_writer.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>

namespace glintfx::test::wire_relay {

namespace {

bool write_file(const std::string &path, const std::uint8_t *data, std::size_t size) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        return false;
    }
    std::size_t done = 0;
    while (done < size) {
        const ssize_t put = ::write(fd, data + done, size - done);
        if (put <= 0) {
            (void)::close(fd);
            return false;
        }
        done += static_cast<std::size_t>(put);
    }
    return ::close(fd) == 0;
}

bool write_text(const std::string &path, const std::string &text) {
    return write_file(path, reinterpret_cast<const std::uint8_t *>(text.data()), text.size());
}

std::string connection_prefix(const capture_target &target) {
    return target.directory + "/conn" + std::to_string(target.connection_serial);
}

std::string layout_text(const frame_layout &layout) {
    return "width=" + std::to_string(layout.width) + "\nheight=" + std::to_string(layout.height) +
           "\nstride=" + std::to_string(layout.stride) +
           "\nformat=" + std::to_string(layout.format) + "\n";
}

bool save_one_frame(const capture_target &target, std::uint32_t surface_id,
                    const captured_frame &frame) {
    const std::string stem = connection_prefix(target) + "_surface" + std::to_string(surface_id);
    return write_file(stem + ".raw", frame.bytes.data(), frame.bytes.size()) &&
           write_text(stem + ".meta", layout_text(frame.layout));
}

} // namespace

capture_report save_session_frames(const shm_snapshot &snapshot, const capture_target &target) {
    capture_report report;
    if (snapshot.frames().empty()) {
        if (!write_text(connection_prefix(target) + "_no_frame.txt", "nenhum quadro\n")) {
            ++report.files_failed;
        }
        return report;
    }
    for (const auto &[surface_id, frame] : snapshot.frames()) {
        if (save_one_frame(target, surface_id, frame)) {
            ++report.frames_saved;
        } else {
            ++report.files_failed;
        }
    }
    return report;
}

} // namespace glintfx::test::wire_relay
