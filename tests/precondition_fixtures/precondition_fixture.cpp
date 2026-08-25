// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// precondition_fixture.cpp - proves both halves of the debug-only
// precondition guard on gltfx_rslt<T>::value()/error(): in a build
// where NDEBUG is undefined (Debug), the assert() inside these
// accessors fires FIRST, before the underlying undefined behavior, and
// prints a message naming exactly which precondition the caller
// violated. In a build where NDEBUG is defined (Release, this
// project's default), the assert compiles to nothing, and the SAME
// undefined behavior this code already had before the guard still
// happens, unchanged.
//
// Two cases, selected by argv[1], because the two gltfx_rslt<T> forms
// are two SEPARATE implementations with independently-proven UB
// shapes when the guard is compiled out:
//   "primary" - gltfx_rslt<int>::value() on an error-holding result.
//               Storage is std::variant<int, gltfx_err>; std::get_if
//               returns a genuine null pointer when the wrong
//               alternative is active, and dereferencing it is a real
//               null-pointer read - typically SIGSEGV in Release.
//   "void"    - gltfx_rslt<void>::error() on a success-holding
//               (ok()) result. Storage is std::optional<gltfx_err>;
//               std::optional::operator*() on an unengaged optional
//               reads the optional's OWN internal buffer directly (it
//               exists whether engaged or not) - this does NOT
//               reliably fault, it can silently hand back a
//               fabricated gltfx_err. Proving this case separately is
//               the point: "the process dies either way" is NOT
//               automatically true for this form in Release.
int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: precondition_fixture <primary|void>\n");
        return 2;
    }

    if (std::strcmp(argv[1], "primary") == 0) {
        const glintfx::gltfx_rslt<int> err_result =
            glintfx::gltfx_rslt<int>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::not_found));
        // Precondition violation ON PURPOSE: this is the point of this
        // program, not a mistake. err_result.has_value() is false.
        const int &v = err_result.value();
        std::printf("UNEXPECTED: read value=%d without any guard firing (Release, "
                    "primary-template UB did not fault this run)\n",
                    v);
        return 0;
    }

    if (std::strcmp(argv[1], "void") == 0) {
        const glintfx::gltfx_rslt<void> ok_result = glintfx::gltfx_rslt<void>::ok();
        // Precondition violation ON PURPOSE: ok_result.has_error() is
        // false.
        const glintfx::gltfx_err &e = ok_result.error();
        std::printf(
            "UNEXPECTED: read error code=%u without any guard firing (Release, void-specialization "
            "UB fabricated a gltfx_err instead of faulting this run)\n",
            static_cast<unsigned>(e.code()));
        return 0;
    }

    std::fprintf(stderr, "usage: precondition_fixture <primary|void>\n");
    return 2;
}
