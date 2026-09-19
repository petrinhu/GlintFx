// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

// checked_stdio.hpp - LINT-CONTAINER-SMOKES (TODO.md, GODS_LAWS.md
// L-17/L-40): .clang-tidy enables cert-* project-wide, and cert-err33-c
// requires the return of std::fprintf()/std::setvbuf() (both on CERT's
// own "checked function" list - unchecked I/O can silently drop a
// diagnostic or leave a stream unbuffered) to be checked. Every
// tests/container/*.cpp fixture called both UNCHECKED, every single
// time - measured live once these fixtures first entered this build's
// compile_commands.json (tests/container/CMakeLists.txt) and
// run-clang-tidy could see them for the first time: 251 std::fprintf()
// calls and 18 std::setvbuf() calls across 18 files, 269 identical
// findings, not 269 distinct bugs.
//
// GODS_LAWS.md L-17's own DRY "regra de 3": the SAME unchecked-return
// shape repeating 269 times across 18 files is the textbook case for
// extracting the check ONCE, never scattering 269 near-identical
// `if (... < 0) { ... }` blocks (or 269 NOLINTNEXTLINE lines, each
// demanding its own "reason:" under this project's own NOLINT policy) across the
// fixtures themselves. This header is that one place - a plain,
// non-project-namespaced diagnostic write has exactly one sane failure
// response for a throwaway smoke-test fixture: if the fixture cannot
// even report its own status, it has nothing else to report; std::exit()
// makes that observable to the SAME channel that already decides
// pass/fail for these fixtures (GODS_LAWS.md L-09: the wayland-
// container CI job reads the container-run process's own exit code,
// tests/container/exec_fixture.sh), instead of silently limping on
// with a corrupted diagnostic stream.
//
// Same shape src/platform/win32/display_adapter.cpp's own swprintf()
// call already uses for cert-err33-c elsewhere in this tree (its own
// header comment: "Return value checked (cert-err33-c)") - checking
// for real, not casting the return away.
namespace glintfx::container_fixture {

// A genuine C-style variadic function (never a variadic TEMPLATE
// forwarding to std::fprintf() - tried first, measured live: GCC
// rejects [[gnu::format]] on a template with "argument 3 value '3'
// does not refer to a variable argument list", because a parameter
// pack is not a "..." vararg from the attribute's point of view), so
// [[gnu::format(printf, 2, 3)]] keeps GCC/Clang's own -Wformat
// checking of the call site's format string against its arguments
// working THROUGH this wrapper - without it, every fixture would lose
// that check the moment std::fprintf() itself stopped being called
// directly.
[[gnu::format(printf, 2, 3)]] inline void checked_fprintf(std::FILE *stream, const char *format,
                                                          ...) {
    std::va_list args;
    va_start(args, format);
    const int result = std::vfprintf(stream, format, args);
    va_end(args);
    if (result < 0) {
        std::exit(EXIT_FAILURE);
    }
}

inline void checked_setvbuf(std::FILE *stream, char *buffer, int mode, std::size_t size) {
    if (std::setvbuf(stream, buffer, mode, size) != 0) {
        std::exit(EXIT_FAILURE);
    }
}

} // namespace glintfx::container_fixture
