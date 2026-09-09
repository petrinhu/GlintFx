// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// compiler_noinline.hpp - CORE-LOG-CI (run 34350043551, jobs "Windows
// - compartilhado", "Windows - estatico", "Windows - Debug"): the one
// place the GCC-vs-MSVC "do not inline this function" branch is
// written, so no test file re-derives it per compiler (GODS_LAWS.md
// L-04: same behavior on every platform, each one proven).
//
// [[gnu::noinline]] is a GCC/Clang extension MSVC does not recognize.
// Under /WX that is not silently ignored: MSVC emits warning C5030
// ("attribute '[[gnu::noinline]]' is not recognized"), and /WX turns
// it into error C2220 - exactly what broke tests/log_field_test.cpp:
// 128 on all three Windows jobs (one of them, "Windows - estatico",
// had been green in the run before). The technique itself - an
// opaque, noinline function boundary the optimizer cannot see through
// - is correct and still needed on every platform; only the spelling
// of "noinline" differs.
//
// MSVC's own equivalent is the __declspec(noinline) storage-class
// modifier (learn.microsoft.com/cpp/cpp/noinline, fetched 09/09/2026),
// written BEFORE the return type - the same position [[gnu::noinline]]
// takes as an attribute, so GLINTFX_TEST_NOINLINE drops in at either
// compiler's call site unchanged.
#if defined(_MSC_VER) && !defined(__clang__)
#define GLINTFX_TEST_NOINLINE __declspec(noinline)
#else
#define GLINTFX_TEST_NOINLINE [[gnu::noinline]]
#endif
