// SPDX-License-Identifier: AGPL-3.0-or-later
// example_harness.hpp - what every GlintFx example shares to run WITHOUT a person (the CI, a
// replay): the command line (--frames N), the ready line a capture tool waits for, the line of a
// failed step, and the tally of what each frame reported. It is not part of the library and it is
// not installed; a program of your own needs none of it. Each example keeps its own lesson (the
// scene and the calls that draw it) in its own file, and reads this one only for the run.
//
// THE COMMON REASON TO CHANGE (CONTRACT.md 6.7, the third example): the contract between an
// example and the capture drivers (tests/container/run_example_capture.sh on Linux,
// tests/tools/run_example_capture_win32.py on Windows). The drivers start "<example> --frames N",
// wait for the line "<example>: first frame presented", read "<example>: <step> failed: ..." when
// a step of the library fails, and read the exit code below.
//
// USAGE of an example: <example> [--frames N]
//   no argument: runs until the system asks to close the window, and exits 0;
//   --frames N (1 to 1000000): stops by itself after N DRAWN frames, through the normal path of the
//   loop.
// EXIT: 0 done; 1 a step of the library failed (the line says which) or a frame lost pieces; 2 bad
// usage.
#pragma once

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string_view>
#include <system_error>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/loop/loop.hpp>

namespace glintfx_examples {

inline constexpr int k_exit_ok = 0;
inline constexpr int k_exit_failed = 1;
inline constexpr int k_exit_usage = 2;
inline constexpr std::uint64_t k_max_frames = 1000000;

// The command line: nothing, or --frames N with N in [1, 1000000], digits only (std::from_chars).
struct command_line {
    bool valid = false;
    std::optional<std::uint64_t> frames_to_draw;
};

inline command_line read_command_line(int argc, char **argv) {
    if (argc == 1) {
        return {.valid = true, .frames_to_draw = std::nullopt};
    }
    if (argc != 3 || std::string_view(argv[1]) != "--frames") {
        return {};
    }
    const std::string_view text(argv[2]);
    std::uint64_t frames = 0;
    const std::from_chars_result result =
        std::from_chars(text.data(), text.data() + text.size(), frames);
    const bool whole_number = result.ec == std::errc{} && result.ptr == text.data() + text.size();
    if (!whole_number || frames < 1 || frames > k_max_frames) {
        return {};
    }
    return {.valid = true, .frames_to_draw = frames};
}

// The usage line of a bad command line; the example then exits 2.
inline int print_usage(std::string_view example) {
    (void)std::fprintf(stderr, "usage: %.*s [--frames N]   (N from 1 to 1000000)\n",
                       static_cast<int>(example.size()), example.data());
    return k_exit_usage;
}

// One failed step of the library, in the form the capture drivers read; the example then exits 1.
inline int fail(std::string_view example, std::string_view step, const glintfx::gltfx_err &error) {
    const std::string_view code = glintfx::gltfx_err_code_name(error.code());
    const std::string_view rejected = error.rejected_value();
    (void)std::fprintf(stderr, "%.*s: %.*s failed: %.*s rejected_value=%.*s\n",
                       static_cast<int>(example.size()), example.data(),
                       static_cast<int>(step.size()), step.data(), static_cast<int>(code.size()),
                       code.data(), static_cast<int>(rejected.size()), rejected.data());
    return k_exit_failed;
}

// What a run has drawn and reported so far. An example keeps one, next to its renderers, and calls
// it from the two callbacks of its loop.
struct frame_tally {
    std::string_view example;
    std::optional<std::uint64_t> frames_to_draw;
    std::uint64_t frames_drawn = 0;
    bool ready_line_printed = false;
    std::uint64_t pieces_lost = 0;
    bool frame_failed = false;

    // A piece refused, dropped or abandoned is a frame that did not show the whole scene: the run
    // fails.
    void count_frame(const glintfx::gltfx_rslt<glintfx::gltfx_frame_2d_report> &finished) noexcept {
        if (finished.has_error()) {
            frame_failed = true;
            return;
        }
        const glintfx::gltfx_frame_2d_report &report = finished.value();
        pieces_lost += report.pieces_refused + report.pieces_dropped_out_of_memory +
                       report.pieces_dropped_graphics_failure +
                       report.pieces_dropped_outside_frame + report.frames_abandoned;
    }

    // The line a capture tool waits for before it reads a pixel, printed once, at the first tick
    // after a frame was really presented. fflush, because a pipe makes stdout block-buffered and
    // the tool would never see it.
    void announce_first_frame(const glintfx::gltfx_frame_tick &tick) noexcept {
        if (ready_line_printed || frames_drawn < 1 ||
            tick.last_present != glintfx::gltfx_present_outcome::presented) {
            return;
        }
        (void)std::printf("%.*s: first frame presented\n", static_cast<int>(example.size()),
                          example.data());
        (void)std::fflush(stdout);
        ready_line_printed = true;
    }

    [[nodiscard]] bool all_frames_drawn() const noexcept {
        return frames_to_draw.has_value() && frames_drawn >= *frames_to_draw;
    }

    // The frame report of the whole run: any piece lost, or a frame that failed to finish, fails
    // the run.
    [[nodiscard]] int report() const {
        std::printf("%.*s: frames_drawn=%llu pieces_lost=%llu frame_failed=%d\n",
                    static_cast<int>(example.size()), example.data(),
                    static_cast<unsigned long long>(frames_drawn),
                    static_cast<unsigned long long>(pieces_lost), frame_failed ? 1 : 0);
        return pieces_lost > 0 || frame_failed ? k_exit_failed : k_exit_ok;
    }
};

} // namespace glintfx_examples
