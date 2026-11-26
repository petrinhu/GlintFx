// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_shm_snapshot_selftest.cpp - autoteste de QA-SCREEN-CAPTURE
// P1 (D-W8-20/D-W8-32, /var/tmp/cto-w8/plano-w8-v3.md SS3), so o
// ATOMO wire_shm_snapshot: pool destruido ainda legivel, tres buffers
// rotativos, resize e limite do pool. Os pixels sao literais escritos
// A MAO num memfd (valores decididos aqui, nunca copiados da saida do
// programa), distintos por buffer. O writer e o relé inteiro tem os
// proprios arquivos (wire_relay_shm_writer_selftest.cpp, wire_relay_
// shm_capture_selftest.cpp).
#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <unistd.h>

using namespace glintfx::test::wire_relay;

GLINTFX_TEST(wire_shm_snapshot_pool_destroyed_after_create_buffer_is_still_readable) {
    fd_holder pool_file(make_memfd({0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, pool_file.get());
    // The client closes ITS descriptor and destroys the pool right
    // away (M1): only the snapshot's own duplicate keeps the pixels.
    pool_file.reset();
    feed(snapshot, known_interface::wl_shm_pool, encode_shm_pool_destroy(6));
    present(snapshot, 5, 7);

    GLINTFX_CHECK_EQ(snapshot.frames().size(), std::size_t{1});
    const captured_frame &frame = snapshot.frames().at(5);
    GLINTFX_CHECK_EQ(frame.layout.width, 2u);
    GLINTFX_CHECK_EQ(frame.layout.height, 2u);
    GLINTFX_CHECK_EQ(frame.layout.stride, 8u);
    GLINTFX_CHECK_EQ(frame.layout.format, 0u);
    GLINTFX_CHECK(words_of(frame.bytes) ==
                  (words{0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{0});
}

GLINTFX_TEST(wire_shm_snapshot_three_rotating_buffers_keep_the_last_commit_not_the_last_attach) {
    const fd_holder file_a(make_memfd({0xA0000001u, 0xA0000002u, 0xA0000003u, 0xA0000004u}));
    const fd_holder file_b(make_memfd({0xB0000001u, 0xB0000002u, 0xB0000003u, 0xB0000004u}));
    const fd_holder file_c(make_memfd({0xC0000001u, 0xC0000002u, 0xC0000003u, 0xC0000004u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 10, 11, file_a.get());
    open_buffer(snapshot, 12, 13, file_b.get());
    open_buffer(snapshot, 14, 15, file_c.get());

    present(snapshot, 5, 11); // A attached and committed
    GLINTFX_CHECK(words_of(snapshot.frames().at(5).bytes) ==
                  (words{0xA0000001u, 0xA0000002u, 0xA0000003u, 0xA0000004u}));
    present(snapshot, 5, 13); // B attached and committed
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(5, 15)); // C: attach only

    GLINTFX_CHECK_EQ(snapshot.frames().size(), std::size_t{1});
    GLINTFX_CHECK(words_of(snapshot.frames().at(5).bytes) ==
                  (words{0xB0000001u, 0xB0000002u, 0xB0000003u, 0xB0000004u}));
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{0});
}

GLINTFX_TEST(wire_shm_snapshot_null_attach_clears_the_pending_buffer) {
    const fd_holder file(make_memfd({1u, 2u, 3u, 4u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get());
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(5, 7));
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(5, 0)); // null attach
    feed(snapshot, known_interface::wl_surface, encode_surface_commit(5));

    GLINTFX_CHECK(snapshot.frames().empty());
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{0});
}

GLINTFX_TEST(wire_shm_snapshot_pool_resize_is_respected) {
    const fd_holder file(make_memfd({0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get()); // pool declared with 16 bytes
    // The client grows the file and writes the new region FIRST, so
    // only the pool's declared size can still refuse a buffer there.
    GLINTFX_CHECK(::ftruncate(file.get(), 32) == 0);
    overwrite_memfd(file.get(), {0x0A0B0C0Du, 0x1A1B1C1Du, 0x2A2B2C2Du, 0x3A3B3C3Du}, 16);
    feed(snapshot, known_interface::wl_shm_pool,
         encode_shm_pool_create_buffer(6, 9, 16, 2, 2, 8, 0));
    present(snapshot, 5, 9);
    GLINTFX_CHECK(snapshot.frames().empty()); // refused: past the declared 16 bytes
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{1});

    feed(snapshot, known_interface::wl_shm_pool, encode_shm_pool_resize(6, 32));
    feed(snapshot, known_interface::wl_shm_pool,
         encode_shm_pool_create_buffer(6, 9, 16, 2, 2, 8, 0));
    present(snapshot, 5, 9);
    GLINTFX_CHECK(words_of(snapshot.frames().at(5).bytes) ==
                  (words{0x0A0B0C0Du, 0x1A1B1C1Du, 0x2A2B2C2Du, 0x3A3B3C3Du}));
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{1});
}

GLINTFX_TEST(wire_shm_snapshot_frame_over_the_size_cap_is_refused_and_exactly_the_cap_is_taken) {
    const fd_holder file(make_memfd({1u}));
    GLINTFX_CHECK(::ftruncate(file.get(), 70000000) == 0); // sparse: zeros, no memory
    shm_snapshot snapshot;
    feed(snapshot, known_interface::wl_shm, encode_shm_create_pool(3, 6, 70000000), {file.get()});
    // 16384 * 4096 = 64 MiB exactly: at the cap, taken.
    feed(snapshot, known_interface::wl_shm_pool,
         encode_shm_pool_create_buffer(6, 7, 0, 4096, 4096, 16384, 0));
    present(snapshot, 5, 7);
    GLINTFX_CHECK_EQ(snapshot.frames().at(5).bytes.size(), shm_frame_size_cap);
    // One row more: over the cap, refused and counted.
    feed(snapshot, known_interface::wl_shm_pool,
         encode_shm_pool_create_buffer(6, 8, 0, 4096, 4097, 16384, 0));
    present(snapshot, 9, 8);
    GLINTFX_CHECK(snapshot.frames().find(9) == snapshot.frames().end());
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{1});
}
