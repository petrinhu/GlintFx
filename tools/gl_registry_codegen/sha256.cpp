// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gl_registry_codegen/sha256.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <format>
#include <vector>

// GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1). Straight
// implementation of FIPS 180-4 SHA-256 - the constants (k, initial
// hash value) are the standard's own published values, not derived;
// tests/gl_sha256_test.cpp proves the RESULT against the standard's
// own published test vectors, not against this file's own constants.

namespace glintfx::gl_codegen {

namespace {

// FIPS 180-4 4.2.2: the first 32 bits of the fractional parts of the
// cube roots of the first 64 prime numbers.
constexpr std::array<std::uint32_t, 64> k = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
                                             0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, //
                                             0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
                                             0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, //
                                             0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                                             0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, //
                                             0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                                             0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, //
                                             0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
                                             0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, //
                                             0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
                                             0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, //
                                             0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
                                             0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, //
                                             0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                                             0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

// FIPS 180-4 5.3.3: the first 32 bits of the fractional parts of the
// square roots of the first 8 prime numbers.
constexpr std::array<std::uint32_t, 8> initial_hash = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

std::uint32_t rotr(std::uint32_t x, int n) { return std::rotr(x, n); }

// FIPS 180-4 6.2.2: processes exactly one 64-byte block, updating `h`
// in place - the only place the compression function itself lives, so
// pad_message() below never has to know how a block is compressed,
// only how many of them to hand over.
void process_block(std::array<std::uint32_t, 8> &h, const std::uint8_t *block) {
    std::array<std::uint32_t, 64> w{};
    for (int i = 0; i < 16; ++i) {
        w[static_cast<std::size_t>(i)] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                                         (static_cast<std::uint32_t>(block[(i * 4) + 1]) << 16) |
                                         (static_cast<std::uint32_t>(block[(i * 4) + 2]) << 8) |
                                         static_cast<std::uint32_t>(block[(i * 4) + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[static_cast<std::size_t>(i - 15)], 7) ^
                                 rotr(w[static_cast<std::size_t>(i - 15)], 18) ^
                                 (w[static_cast<std::size_t>(i - 15)] >> 3);
        const std::uint32_t s1 = rotr(w[static_cast<std::size_t>(i - 2)], 17) ^
                                 rotr(w[static_cast<std::size_t>(i - 2)], 19) ^
                                 (w[static_cast<std::size_t>(i - 2)] >> 10);
        w[static_cast<std::size_t>(i)] =
            w[static_cast<std::size_t>(i - 16)] + s0 + w[static_cast<std::size_t>(i - 7)] + s1;
    }

    std::uint32_t a = h[0];
    std::uint32_t b = h[1];
    std::uint32_t c = h[2];
    std::uint32_t d = h[3];
    std::uint32_t e = h[4];
    std::uint32_t f = h[5];
    std::uint32_t g = h[6];
    std::uint32_t hh = h[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ ((~e) & g);
        const std::uint32_t temp1 =
            hh + s1 + ch + k[static_cast<std::size_t>(i)] + w[static_cast<std::size_t>(i)];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + maj;

        hh = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
}

// FIPS 180-4 5.1.1: appends a single 1-bit, then zero bits, then the
// original bit length as a 64-bit big-endian integer, so the total
// length is a multiple of 64 bytes.
std::vector<std::uint8_t> pad_message(std::string_view data) {
    std::vector<std::uint8_t> padded(data.begin(), data.end());
    const std::uint64_t bit_length = static_cast<std::uint64_t>(data.size()) * 8;

    padded.push_back(0x80);
    while (padded.size() % 64 != 56) {
        padded.push_back(0x00);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded.push_back(static_cast<std::uint8_t>(bit_length >> shift));
    }
    return padded;
}

} // namespace

std::string sha256_hex(std::string_view data) {
    std::array<std::uint32_t, 8> h = initial_hash;
    const std::vector<std::uint8_t> padded = pad_message(data);

    for (std::size_t offset = 0; offset < padded.size(); offset += 64) {
        process_block(h, padded.data() + offset);
    }

    std::string hex;
    hex.reserve(64);
    for (std::uint32_t word : h) {
        hex += std::format("{:08x}", word);
    }
    return hex;
}

} // namespace glintfx::gl_codegen
