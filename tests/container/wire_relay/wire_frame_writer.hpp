// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_shm_snapshot.hpp"

#include <cstddef>
#include <string>

// wire_frame_writer.hpp - the FRAME WRITER atom of the wire relay
// (QA-SCREEN-CAPTURE P1, D-W8-32): persists what wire_shm_snapshot
// kept, once, when a client leaves, and judges the count.
//
// On-disk contract (the reader is QA-SCREEN-CAPTURE P2's raw_to_png):
//   <dir>/conn<N>_surface<S>.raw   the frame's bytes exactly as the
//                                  client committed them (stride *
//                                  height, no header)
//   <dir>/conn<N>_surface<S>.meta  text, one "key=value" per line, in
//                                  this order: width, height, stride,
//                                  format (raw wl_shm.format value, 0 =
//                                  ARGB8888, 1 = XRGB8888)
//   <dir>/conn<N>_no_frame.txt     written INSTEAD, containing "nenhum
//                                  quadro", when the connection never
//                                  committed a buffer
// N is the relay's own connection serial, S the wl_surface object id.
namespace glintfx::test::wire_relay {

struct capture_target {
    std::string directory;
    std::size_t connection_serial = 0;
};

struct capture_report {
    std::size_t frames_saved = 0;
    std::size_t files_failed = 0;
};

// Writes every frame of `snapshot` (or the "nenhum quadro" marker when
// there is none) under target.directory.
[[nodiscard]] capture_report save_session_frames(const shm_snapshot &snapshot,
                                                 const capture_target &target);

} // namespace glintfx::test::wire_relay
