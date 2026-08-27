// SPDX-License-Identifier: AGPL-3.0-or-later
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include "gl_registry_codegen/sha256.hpp"

// gl_sha256_test.cpp - GL-LOADER (TODO.md, GODS_LAWS.md L-20). Proves
// the hand-written SHA-256 (tools/gl_registry_codegen/sha256.hpp/.cpp)
// against the standard published test vectors (NIST FIPS 180-4 /
// widely reproduced) - never against a value this project invented
// itself, so an implementation bug cannot pass by construction.

using glintfx::gl_codegen::sha256_hex;

GLINTFX_TEST(empty_string_matches_the_published_test_vector) {
    GLINTFX_CHECK_EQ(sha256_hex(""),
                     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

GLINTFX_TEST(abc_matches_the_published_test_vector) {
    GLINTFX_CHECK_EQ(sha256_hex("abc"),
                     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

GLINTFX_TEST(the_two_block_message_matches_the_published_test_vector) {
    // NIST FIPS 180-4's own second standard vector: 448 bits (56
    // bytes), chosen because it forces the padding into a SECOND
    // 512-bit block - the single-block "abc" case above cannot catch
    // a bug in that boundary.
    GLINTFX_CHECK_EQ(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

GLINTFX_TEST(digest_is_64_lowercase_hex_characters) {
    const std::string digest = sha256_hex("glintfx");
    GLINTFX_CHECK_EQ(digest.size(), 64u);
    for (const char c : digest) {
        GLINTFX_CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
    // Cross-checked live against the coreutils sha256sum(1) binary on
    // this machine (printf 'glintfx' | sha256sum), not invented.
    GLINTFX_CHECK_EQ(digest, "5ee0097038774545adc19168c03e620d3f3a7a93577ac68050b676f151e5081b");
}
