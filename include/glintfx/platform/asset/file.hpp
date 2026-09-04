// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string_view>
#include <system_error>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// platform/asset/file.hpp - ASSET-LOAD (TODO.md W3, GODS_LAWS.md
// L-19/L-20/L-22/L-40, ESCOPO.md SS2 decisions 4 and 8): the "path to
// bytes" atom. gltfx_load_file_bytes() does exactly the three things
// ESCOPO.md's own decision 4 names, and NOTHING else: reads the file,
// resolves/classifies the path, and reports failure through
// gltfx_rslt<T> - "a parte publica de carregamento faz tres coisas...
// nao guarda o que ja leu". No cache, on purpose (ESCOPO.md, verbatim:
// "escolher COMO guardar seria impor a nossa politica a todo
// consumidor").
//
// UNDER include/glintfx/platform/, BY THE LIDER'S OWN RULING (28/08/2026,
// adversarial review of ASSET-LOAD): the first version of this header
// lived at include/glintfx/asset/, arguing that std::filesystem/
// std::ifstream are portable stdlib (GODS_LAWS.md L-07) and need no
// port+concept+adapter, so the reasoning that puts Wayland/GL behind
// that machinery ("the API genuinely differs per operating system, and
// a test needs to swap in a fake") does not hold here. That reasoning
// stands ON ITS OWN TERMS - it is why this file carries no adapter, no
// concept, no fake to swap - but it answered a QUESTION L-19 never
// asked. L-19's own text names "arquivo" alongside Wayland/GL/audio/
// gamepad as living in the one layer that touches the OS, full stop;
// it does not condition that on the adapter machinery being needed.
// The question went to the lider, who chose to obey the letter of
// L-19: THIS FILE MOVES, unconditionally. This is not reopened here,
// nor argued against - a future editor who wants a different answer
// asks the lider again, does not read this comment as license to move
// it back (GODS_LAWS.md L-27).
//
// STILL HEADER-ONLY, STILL NO PORT/ADAPTER (that half of the original
// reasoning is untouched by the move above): a unit test still
// exercises the real filesystem directly (a scratch temp directory,
// not a fake adapter) without touching keyboard/mouse/screen - the
// surface GODS_LAWS.md L-09's container isolation actually guards -
// and src/platform/CMakeLists.txt's UNIX/WIN32/unsupported branch
// (ARCH-PORTS) never has to gate this file, because it needs no
// per-platform backend directory to select.
//
// HEADER-ONLY, DELIBERATELY (docs/api-conventions.md R5(b), the SAME
// reason glintfx::core::gltfx_err_fields() and
// glintfx::style::gltfx_gfss_tokenize() stay inline instead of
// GLINTFX_API): the return type is std::vector<std::byte> BY VALUE. An
// EXPORTED function returning a container by value across the .so/.dll
// boundary would allocate the vector's buffer inside the library and
// free it in the CONSUMER's own compiled code when their local
// variable goes out of scope - on Windows, mixing CRTs across that
// boundary corrupts the heap (err.hpp's own "LIFECYCLE" paragraph
// exists to forbid exactly this for gltfx_err). Staying entirely
// inline means every allocation this file ever performs happens on ONE
// side of the boundary, the consumer's, both ends, no crossing at all.
// ASSET-LOAD's own TODO.md row is explicitly NOT a one-way door
// ("Deixa de ser porta de mao unica"), so this shape can still be
// revisited without an API-freeze review if a future slice needs to.
//
// ERROR CODES, REUSED ON PURPOSE, NOT INVENTED: err_code.hpp reserves
// 1000..1999 for "resource/asset domain (TODO.md ASSET-LOAD)", but this
// v1 slice maps every failure onto the three GENERIC codes that already
// carry the exact right meaning - not_found (the path names nothing),
// invalid_argument (the path names something that is not a regular
// file), io_failure (anything else: permission denied, a raw
// std::filesystem::status() failure, or a read that failed mid-stream)
// - with os_error_code() carrying the raw errno/status code where one
// exists. Adding a domain-specific code later is compatible (bumps B,
// GODS_LAWS.md L-26/CE-1's own append-only contract); reusing the
// generic ones today is not a placeholder for that, it is the honest
// v1 answer - none of the six scenarios this file's own test proves
// needs a MORE specific code to be actionable.
//
// noexcept AT THE PUBLIC BOUNDARY (docs/api-conventions.md R3): every
// std::filesystem call below uses the (path, error_code&) overload, on
// purpose, so it reports failure through a value instead of throwing
// std::filesystem_error - the only exception genuinely reachable from
// this file's own code is std::bad_alloc, from std::filesystem::path's
// own internal storage or std::vector<std::byte>'s growth while
// reading. gltfx_load_file_bytes() wraps its whole body in one
// top-level try/catch that translates ANY escaping exception into
// gltfx_err_code::out_of_memory (ESCOPO.md SS2, CORE-ERROR decision 1:
// "a lib NUNCA aborta o processo do consumidor") - the shape
// docs/api-conventions.md R3 itself documents as the general pattern,
// applied here for the first time by a call site outside CORE-ERROR.

namespace glintfx::asset {

namespace detail {

// require_regular_file - the "resolve o caminho" half of ESCOPO.md's
// three-step pipeline: classifies `path` BEFORE any attempt to open it,
// so a caller gets not_found/invalid_argument instead of a generic
// platform failure code for the two cases std::filesystem can already
// tell apart cheaply. Deliberately does NOT check read permission -
// std::filesystem::status() can succeed on a file this process cannot
// actually read (permission denies reading the FILE's own bytes, not
// stat-ing it), so that case is left to the real open() attempt in
// gltfx_load_file_bytes() below, which is the only place that failure
// is genuinely observable.
[[nodiscard]] inline gltfx_rslt<void> require_regular_file(const std::filesystem::path &fs_path,
                                                           std::string_view path_view) {
    std::error_code status_ec;
    const std::filesystem::file_status status = std::filesystem::status(fs_path, status_ec);

    // ORDER CORRECTED BY A RED TEST, NOT BY READING THE STANDARD FIRST
    // (GODS_LAWS.md L-42/L-43 - "o que a documentacao garante, e o que
    // apenas parece garantir"): the type() check below runs BEFORE the
    // status_ec check on purpose, even though most prose describing
    // std::filesystem::status() says a nonexistent path is "not an
    // error" and ec "is cleared". Measured live on this toolchain
    // (libstdc++, GCC 16): for a genuinely nonexistent path, status()
    // correctly sets type() to file_type::not_found AND STILL sets
    // status_ec to ENOENT - checking status_ec first, as an initial cut
    // of this function did, misreported every not_found case as a raw
    // io_failure. Trusting type() == not_found first, regardless of
    // status_ec, matches what this function actually needs to promise
    // (docs/api-conventions.md R6/CE-1 lineage: never trust a claim
    // about behavior this project has not watched fail and pass).
    if (status.type() == std::filesystem::file_type::not_found) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::not_found).with_path(path_view));
    }
    if (status_ec) {
        return gltfx_rslt<void>::err(gltfx_err(gltfx_err_code::io_failure)
                                         .with_path(path_view)
                                         .with_os_error_code(status_ec.value()));
    }
    if (status.type() != std::filesystem::file_type::regular) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_path(path_view));
    }
    return gltfx_rslt<void>::ok();
}

// read_stream_bytes - the "le o arquivo" half: `stream` is ALREADY open,
// in binary mode. Reads it to the end in fixed-size chunks (never one
// std::filesystem::file_size() guess trusted blindly - a file can grow,
// shrink or be a non-seekable special file between stat and read), and
// treats reaching end-of-file as success, including for a zero-length
// file (the byte-copy step below is simply skipped, since gcount() is
// 0 - `contents` stays empty).
//
// TWO DIFFERENT SIGNALS FOR "this short read was a REAL I/O failure,
// not merely end-of-file", one per platform - GODS_LAWS.md L-04 (TODO.md
// ASSET-PARITY-WIN, 04/09/2026): mecanismo pode diferir por sistema,
// comportamento observavel e cobertura, nao. On POSIX, stream.bad() is
// that signal (libstdc++'s own read() sets badbit for a genuine failure,
// measured live via /proc/self/mem in this file's own test). On
// _WIN32, stream.bad() is NOT that signal, and this is not a hunch -
// it is measured against microsoft/STL's own public source
// (github.com/microsoft/STL, fetched 04/09/2026):
//   - stl/inc/istream, basic_istream::read(): on a short read, only
//     `_State |= ios_base::eofbit | ios_base::failbit;` - NEVER badbit,
//     unless the call underneath THROWS (it never does for a plain I/O
//     failure).
//   - stl/inc/__msvc_filebuf.hpp, basic_filebuf::xsgetn()/_Fgetc(): both
//     call fread()/fgetc() on the CRT FILE* and look ONLY at the item
//     count those return - neither calls ferror(), so a genuine mid-read
//     OS failure (a locked byte range, a device error, anything) and an
//     ordinary end-of-file come back IDENTICAL at the stream level.
// Before this fix, that meant a real Windows read failure here returned
// gltfx_rslt<T>::ok() with fewer bytes than the file actually has - the
// exact shape CORE-ERROR already named the risk of: "pior que travar:
// travar e ruidoso, dado errado em silencio faz o consumidor decidir
// sobre uma mentira". errno IS still the signal that survives on
// _WIN32: it is a CRT global that _read() (which fread()/fgetc() call
// under the hood) sets on a REAL failure and leaves UNTOUCHED on
// ordinary EOF - documented at learn.microsoft.com/cpp/c-runtime-library/
// errno-doserrno-sys-errlist-and-sys-nerr ("errno is set on an error...
// run-time library calls that set errno on error don't clear errno on
// success... always clear errno immediately before a call that may set
// it, and check it immediately after"). Cleared right before the ONE
// stream.read() call below and read right back after it returns -
// nothing else runs in between that could touch it, and a single
// stream.read() of k_chunk_size bytes never spans more than one real
// OS-level failure (xsgetn's own internal fread() loop returns the
// instant one of its own sub-reads comes up short, so at most one
// failure is ever live per outer read() call).
//
// LOOP RESTRUCTURED FROM A `while` INTO THIS `for (;;)` (same fatia):
// the old `while (stream.read(...) || stream.gcount() > 0)` shape ran
// one extra, genuinely no-op iteration after every short read (a
// stream.read() call on an already-failed stream never reaches
// rdbuf()->sgetn() at all - the sentry gate blocks it, per the
// standard - so it neither performs I/O nor touches errno); this shape
// breaks out of the loop the instant that same short read is seen,
// with IDENTICAL bytes collected and an IDENTICAL final stream state -
// proved by re-running this file's own suite unchanged (ASSET-PARITY-WIN
// report) before and after this restructuring.
//
// LOCAL VARIABLE NAMED "contents", NOT "bytes" (docs/api-conventions.md
// R6, tests/tools/check_public_name_collision.sh's own mechanical gate,
// GODS_LAWS.md L-40): the gate's enumerate_names.awk flags a bare
// "TYPE name;" line even inside an INLINE function body, on purpose -
// this whole file is header-only (see this file's own "HEADER-ONLY,
// DELIBERATELY" comment above), so its body text is parsed inside
// WHATEVER macros a consumer's translation unit already has active,
// exactly the same risk a real data member has. The gate caught this
// live: `bytes` collides with a function-like macro Linux's own
// linux/netfilter/xt_sctp.h defines (measured against the real
// compiler search path, not a curated list).
[[nodiscard]] inline gltfx_rslt<std::vector<std::byte>>
read_stream_bytes(std::ifstream &stream, std::string_view path_view) {
    std::vector<std::byte> contents;
    constexpr std::size_t k_chunk_size = 1U << 16U;
    char chunk[k_chunk_size];

    for (;;) {
#if defined(_WIN32)
        // See this function's own "TWO DIFFERENT SIGNALS" comment above.
        errno = 0;
#endif
        const bool read_ok =
            static_cast<bool>(stream.read(chunk, static_cast<std::streamsize>(k_chunk_size)));
        const auto got = static_cast<std::size_t>(stream.gcount());
#if defined(_WIN32)
        const int read_errno = errno;
#endif

        if (got > 0) {
            const auto *begin = reinterpret_cast<const std::byte *>(chunk);
            contents.insert(contents.end(), begin, begin + got);
        }

        if (read_ok) {
            continue;
        }

#if defined(_WIN32)
        if (read_errno != 0) {
            return gltfx_rslt<std::vector<std::byte>>::err(gltfx_err(gltfx_err_code::io_failure)
                                                                 .with_path(path_view)
                                                                 .with_os_error_code(read_errno));
        }
#endif
        break;
    }

    if (stream.bad()) {
        return gltfx_rslt<std::vector<std::byte>>::err(
            gltfx_err(gltfx_err_code::io_failure).with_path(path_view));
    }
    return gltfx_rslt<std::vector<std::byte>>::ok(std::move(contents));
}

} // namespace detail

// gltfx_load_file_bytes - the public entry point. Orchestrates the
// three atoms above (classify, open, read) - allowed to orchestrate
// because it DELEGATES each step rather than doing the work itself
// (GODS_LAWS.md L-17: this is the same shape
// platform::display_connection<A>::connect() already uses to wrap
// adapter.open()).
[[nodiscard]] inline gltfx_rslt<std::vector<std::byte>>
gltfx_load_file_bytes(std::string_view path) noexcept {
    try {
        const std::filesystem::path fs_path(path);

        const gltfx_rslt<void> precheck = detail::require_regular_file(fs_path, path);
        if (precheck.has_error()) {
            return gltfx_rslt<std::vector<std::byte>>::err(precheck.error());
        }

        std::ifstream stream(fs_path, std::ios::binary);
        if (!stream.is_open()) {
            return gltfx_rslt<std::vector<std::byte>>::err(
                gltfx_err(gltfx_err_code::io_failure).with_path(path).with_os_error_code(errno));
        }

        return detail::read_stream_bytes(stream, path);
    } catch (...) {
        // ESCOPO.md SS2, CORE-ERROR decision 1: "a lib NUNCA aborta o
        // processo do consumidor" - the only exception genuinely
        // reachable above is an allocation failure (see this header's
        // own "noexcept AT THE PUBLIC BOUNDARY" comment), so that is
        // the one translation this catch-all performs; it is not a
        // general "swallow and guess" handler.
        return gltfx_rslt<std::vector<std::byte>>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
}

} // namespace glintfx::asset
