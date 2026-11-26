// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_shm_writer_selftest.cpp - autoteste de QA-SCREEN-CAPTURE
// P1 (D-W8-32), so o WRITER e o VEREDITO: o que vai para o disco, o
// marcador "nenhum quadro" e a contagem zero que reprova (GODS_LAWS.md
// L-40). Pixels escritos a mao num memfd, como nos outros dois
// arquivos de P1.
#include "wire_capture_verdict.hpp"
#include "wire_frame_writer.hpp"
#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

using namespace glintfx::test::wire_relay;

GLINTFX_TEST(wire_shm_snapshot_client_that_never_committed_saves_no_frame_and_fails_the_count) {
    const fd_holder file(make_memfd({1u, 2u, 3u, 4u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get());
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(5, 7)); // never committed

    const scratch_dir dir;
    const capture_report report = save_session_frames(snapshot, capture_target{dir.path(), 3});

    GLINTFX_CHECK_EQ(report.frames_saved, std::size_t{0});
    GLINTFX_CHECK(!capture_passes(report));
    GLINTFX_CHECK_EQ(describe_capture(report, 3),
                     std::string("wire_relay: capture conn3 - nenhum quadro"));
    GLINTFX_CHECK_EQ(read_text(dir.path() + "/conn3_no_frame.txt"), std::string("nenhum quadro\n"));
}

GLINTFX_TEST(wire_shm_snapshot_saved_frame_has_raw_bytes_and_a_layout_sidecar) {
    const fd_holder file(make_memfd({0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get());
    present(snapshot, 5, 7);

    const scratch_dir dir;
    const capture_report report = save_session_frames(snapshot, capture_target{dir.path(), 2});

    GLINTFX_CHECK_EQ(report.frames_saved, std::size_t{1});
    GLINTFX_CHECK_EQ(report.files_failed, std::size_t{0});
    GLINTFX_CHECK(capture_passes(report));
    GLINTFX_CHECK_EQ(describe_capture(report, 2),
                     std::string("wire_relay: capture conn2 - 1 frame(s) saved, 0 file(s) failed"));
    GLINTFX_CHECK(read_words(dir.path() + "/conn2_surface5.raw") ==
                  (words{0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    GLINTFX_CHECK_EQ(read_text(dir.path() + "/conn2_surface5.meta"),
                     std::string("width=2\nheight=2\nstride=8\nformat=0\n"));
    GLINTFX_CHECK_EQ(read_text(dir.path() + "/conn2_no_frame.txt"), std::string());
}

GLINTFX_TEST(wire_shm_snapshot_unwritable_directory_fails_the_count_instead_of_hiding_it) {
    const fd_holder file(make_memfd({1u, 2u, 3u, 4u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get());
    present(snapshot, 5, 7);

    const scratch_dir dir;
    const capture_report report =
        save_session_frames(snapshot, capture_target{dir.path() + "/does-not-exist", 1});

    GLINTFX_CHECK_EQ(report.frames_saved, std::size_t{0});
    GLINTFX_CHECK_EQ(report.files_failed, std::size_t{1});
    GLINTFX_CHECK(!capture_passes(report));
}
