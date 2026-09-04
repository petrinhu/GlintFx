// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <print>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#else
// WIN32_LEAN_AND_MEAN/NOMINMAX before <windows.h>: same guard shape as
// tests/harness/win_dll_alloc_hook.hpp's own include block, the only
// other place in this test tree that already pulls in the Win32 header.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/asset/file.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// asset_load_test.cpp - ASSET-LOAD (TODO.md, GODS_LAWS.md L-20/L-22/
// L-40, ESCOPO.md SS2 decisions 4 and 8): TDD witness for
// glintfx::asset::gltfx_load_file_bytes(), the "path to bytes" atom -
// reads the whole file, classifies the path before opening it, and
// reports failure through gltfx_rslt<T> (L-22), never a cache (ESCOPO
// decision 4: "carrega e nao guarda nada para reaproveitar").
//
// CLOSED MATRIX, SIX SCENARIOS, GODS_LAWS.md L-40 ("enumeracao fechada
// por construcao, a contagem aparece na saida mesmo quando passa"):
// nonexistent path, no read permission, path names a directory (not a
// regular file), an empty file (success, zero bytes - NOT an error),
// a RELATIVE path (ESCOPO decision 8: the recommended, primary case),
// and an ABSOLUTE path (the secondary, still-tested case). Each has
// its own GLINTFX_TEST below; g_scenarios_exercised is incremented by
// every one of them and the LAST case in this file (declaration order
// is registration order within one TU, harness/test_registry.cpp) checks
// the running total against k_total_scenarios and PRINTS it, so a
// scenario silently skipped (never incrementing the counter) reproves
// instead of passing quietly.
//
// BYTE-EXACT ROUND TRIP: bytes_round_trip_including_embedded_null_and_
// non_ascii_bytes below is the case that separates "read the file" from
// "read the file correctly" - a reader that treats the content as a
// C string dies at the embedded 0x00; a reader that assumes ASCII
// mishandles the high-bit bytes. glintfx::asset::gltfx_load_file_bytes
// returns std::vector<std::byte>, never a null-terminated buffer, so
// this is a real property of the type, not just of one lucky test
// input - the test still exercises a HOSTILE input to prove it, not
// just trust the type signature.
//
// MID-STREAM READ FAILURE, NOT PART OF THE CLOSED SIX (adversarial
// review, 28/08/2026): the six scenarios above all fail, or succeed,
// BEFORE or AT open() - require_regular_file() classifies the path,
// then the file either opens or it does not. None of them ever forces
// read_stream_bytes() itself (glintfx/platform/asset/file.hpp) to see
// a REAL I/O failure mid-loop - proved by mutation: recoding that
// branch's error, or deleting the branch outright (silently returning
// the partial bytes read so far as SUCCESS - truncated data the
// caller has no way to tell from a genuinely complete file), left this
// whole suite green in BOTH cases before this test existed.
// mid_stream_read_failure_is_io_failure_not_silent_partial_success
// below closes that gap with a GENUINE OS-level read failure on EVERY
// supported platform, not a simulated stream state - two DIFFERENT
// mechanisms for the SAME observable outcome (GODS_LAWS.md L-04,
// 02/09/2026: "mecanismo pode diferir por sistema; comportamento
// observavel e cobertura, nao"):
//   - POSIX (#else below): reading /proc/self/mem from its own start
//     (virtual address 0, the kernel's own null-page guard, always
//     unmapped) measured live on this toolchain (GCC 16/libstdc++,
//     GODS_LAWS.md L-42/L-43) as std::filesystem::status() reporting
//     file_type::regular (so require_regular_file() lets it through)
//     and the FIRST std::ifstream::read() setting badbit with zero
//     bytes gotten.
//   - _WIN32: measured against microsoft/STL's own public source
//     (github.com/microsoft/STL, fetched 04/09/2026) that badbit is
//     UNREACHABLE this way on MSVC - basic_istream::read() only ever
//     sets eofbit|failbit on a short read, and basic_filebuf::xsgetn()/
//     _Fgetc() never call ferror() on the fread()/fgetc() they wrap, so
//     a genuine OS failure and ordinary EOF are indistinguishable at
//     the stream level there. glintfx/platform/asset/file.hpp's own
//     read_stream_bytes() was fixed the same fatia (ASSET-PARITY-WIN)
//     to catch this on _WIN32 via errno instead of stream.bad() - see
//     that function's own header comment for the full citation. This
//     scenario forces a GENUINE Windows read failure via a byte-range
//     lock (LockFileEx) held on a SECOND handle to the same file, which
//     the OS enforces even against a later handle from this SAME
//     process (learn.microsoft.com/windows/win32/api/fileapi/
//     nf-fileapi-lockfileex: "If the locking process opens the file a
//     second time, it cannot access the specified region through this
//     second handle until it unlocks the region") - std::ifstream's own
//     open (inside gltfx_load_file_bytes()) is that second handle.
// Deliberately does NOT increment g_scenarios_exercised on either
// platform: it is not a member of the closed six-scenario PRE-OPEN
// matrix above (same reason bytes_round_trip_including_embedded_null_
// and_non_ascii_bytes below does not either) - it proves a property of
// read_stream_bytes(), not of require_regular_file().
//
// SCRATCH DIRECTORY, ISOLATED PER PROCESS: each GLINTFX_TEST below that
// touches the filesystem creates its OWN scratch directory under
// std::filesystem::temp_directory_path() (this project's TMPDIR=/var/tmp
// convention, CLAUDE.md - tmpfs on this machine is too small for a
// build, irrelevant here, but respecting the same env var keeps every
// scratch write on the same disk this project already uses) and
// std::filesystem::current_path()'s the PROCESS into it before writing
// the relative-path fixture - safe because ctest runs each test
// executable as its OWN process (glintfx_add_test, cmake/GlintfxTest.cmake),
// so this process's cwd never collides with a sibling test's.

namespace {

int g_scenarios_exercised = 0;
constexpr int k_total_scenarios = 6;

// make_scratch_dir - one fresh, empty, uniquely-named directory per
// call, never reused across scenarios (GODS_LAWS.md L-17: an atom that
// needs "and" in its name was not an atom - this one only creates).
[[nodiscard]] std::filesystem::path make_scratch_dir(std::string_view tag) {
    static int counter = 0;
    ++counter;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("glintfx_asset_load_test_" + std::string(tag) + "_" + std::to_string(counter));
    std::filesystem::create_directories(dir);
    return dir;
}

// write_file - the ONLY place this TU calls std::ofstream, so every
// scenario below builds its fixture the same, already-proven way.
void write_file(const std::filesystem::path &path, const std::vector<std::byte> &content) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(content.data()),
              static_cast<std::streamsize>(content.size()));
}

// running_as_root - the declared downgrade this project's own memory
// already names (feedback in MEMORY.md, "container roda como root e
// root ignora bits de permissao"): the four Linux CI matrix jobs run
// inside a container as uid 0, where chmod 0000 does not block a read.
// Named and printed, never silently skipped (GODS_LAWS.md L-40).
[[nodiscard]] bool running_as_root() {
#if defined(_WIN32)
    return false;
#else
    return geteuid() == 0;
#endif
}

#if defined(_WIN32)
// exclusive_handle_guard - RAII so a GLINTFX_CHECK failure inside
// no_read_permission_is_io_failure (which unwinds the case via
// case_check_failed, harness/check.hpp's own CASE-FATAL design) can
// never leak the exclusive HANDLE that scenario opens. Same
// non-copyable, destructor-closes shape as this file's sibling
// display_connect_failure_test.cpp's own private_empty_runtime_dir.
class exclusive_handle_guard {
  public:
    explicit exclusive_handle_guard(HANDLE handle) : m_handle(handle) {}

    exclusive_handle_guard(const exclusive_handle_guard &) = delete;
    exclusive_handle_guard &operator=(const exclusive_handle_guard &) = delete;

    ~exclusive_handle_guard() {
        if (m_handle != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_handle);
        }
    }

    [[nodiscard]] bool is_valid() const noexcept { return m_handle != INVALID_HANDLE_VALUE; }

    // Added for mid_stream_read_failure_is_io_failure_not_silent_partial_
    // success below (ASSET-PARITY-WIN): that scenario needs the raw
    // HANDLE itself to pass to ::LockFileEx, which is_valid() alone
    // cannot provide - same shape as std::unique_ptr::get(), the guard
    // still owns the handle, this only lets a caller act on it.
    [[nodiscard]] HANDLE handle() const noexcept { return m_handle; }

  private:
    HANDLE m_handle;
};
#endif

} // namespace

GLINTFX_TEST(nonexistent_relative_path_is_not_found) {
    const std::filesystem::path dir = make_scratch_dir("missing");
    std::filesystem::current_path(dir);

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("does_not_exist.bin");

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::not_found);
    ++g_scenarios_exercised;
}

// NO CHMOD 0000 EQUIVALENT ON WINDOWS (measured, not assumed -
// GODS_LAWS.md L-42/L-43): FILE_ATTRIBUTE_READONLY only blocks WRITES;
// there is no attribute that blocks a read the way Unix's permission
// bits do, so a version of this scenario that just skipped the chmod
// call under _WIN32 (what this file did until this fix) left the file
// fully readable, gltfx_load_file_bytes() genuinely succeeded, and the
// scenario silently proved nothing on that platform - GODS_LAWS.md
// L-40's own "varredura que contou zero reprova" caught it in the
// closed-matrix count at the bottom of this file.
//
// THE NATIVE WINDOWS FAILURE THIS SCENARIO USES INSTEAD: a sharing
// violation. This process opens its OWN second handle on the same file
// with dwShareMode = 0 ("deny all" - no other handle, including one
// opened later by this same process, may read OR write while this
// handle stays open), and keeps it open for the whole scenario. The
// library's own std::ifstream open (glintfx::asset::gltfx_load_file_
// bytes(), platform/asset/file.hpp) then collides with it and fails
// with ERROR_SHARING_VIOLATION - a genuine OS-level "this process
// cannot read this file right now" failure, not a simulated stream
// state, and it maps to the SAME gltfx_err_code::io_failure a Unix
// permission-denied open does (the library's open() failure path does
// not distinguish WHY CreateFile-under-the-hood refused).
//
// UNLIKE chmod 0000, THIS HAS NO PRIVILEGE-LEVEL DOWNGRADE (checked,
// not assumed): a Windows sharing violation is enforced by the
// kernel's own share-mode check against every open handle, regardless
// of the caller's privilege level. That is a DIFFERENT mechanism from
// an ACL permission check - the one uid 0 bypasses on Unix, and the
// one an Administrator/SYSTEM account can similarly bypass on Windows
// via SeBackupPrivilege/SeTakeOwnershipPrivilege. Neither of those
// privileges lets a second CreateFile call ignore an existing
// exclusive lock; the lock is not a permission being checked, it is a
// second handle's request genuinely conflicting with a live one. So
// this scenario, unlike its Unix sibling, has exactly one branch on
// Windows: it always exercises the failure, in CI running as
// Administrator or not.
GLINTFX_TEST(no_read_permission_is_io_failure) {
    const std::filesystem::path dir = make_scratch_dir("noperm");
    std::filesystem::current_path(dir);
    write_file(dir / "secret.bin", {std::byte{0x01}, std::byte{0x02}});

#if defined(_WIN32)
    const exclusive_handle_guard exclusive(::CreateFileW((dir / "secret.bin").c_str(), GENERIC_READ,
                                                         /*dwShareMode=*/0, nullptr, OPEN_EXISTING,
                                                         FILE_ATTRIBUTE_NORMAL, nullptr));
    GLINTFX_CHECK(exclusive.is_valid());

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("secret.bin");

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::io_failure);
#else
    GLINTFX_CHECK(::chmod((dir / "secret.bin").c_str(), 0000) == 0);

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("secret.bin");

    if (running_as_root()) {
        // Declared downgrade (GODS_LAWS.md L-40): uid 0 reads through
        // the permission bits this scenario relies on, so the file
        // opens and reads successfully here instead of failing - the
        // scenario still ran (the counter below still increments), it
        // just could not exercise the permission-denied branch on this
        // process's privilege level.
        //
        // ASSERTS has_value(), NOT "has_value() || has_error()"
        // (adversarial review, 28/08/2026 - GODS_LAWS.md L-40's own
        // "isto e testado" corollary): gltfx_rslt<T> is a closed union
        // of exactly those two states, so the disjunction can NEVER be
        // false - it proves only that the call did not crash, nothing
        // this scenario actually claims. The paragraph right above
        // already names the real, checkable claim: under uid 0 the
        // permission bits this scenario relies on do not apply, so the
        // read genuinely succeeds - that is the assertion that wants
        // to be made here.
        std::println("asset_load_test: no_read_permission_is_io_failure running as root - "
                     "permission bits do not apply, downgrading to a no-crash check");
        GLINTFX_CHECK(result.has_value());
    } else {
        GLINTFX_CHECK(result.has_error());
        GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::io_failure);
    }
#endif
    ++g_scenarios_exercised;
}

GLINTFX_TEST(directory_path_is_invalid_argument) {
    const std::filesystem::path dir = make_scratch_dir("isdir");
    std::filesystem::current_path(dir);
    std::filesystem::create_directories(dir / "a_directory");

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("a_directory");

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    ++g_scenarios_exercised;
}

GLINTFX_TEST(empty_file_reads_as_empty_success_not_an_error) {
    const std::filesystem::path dir = make_scratch_dir("empty");
    std::filesystem::current_path(dir);
    write_file(dir / "empty.bin", {});

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("empty.bin");

    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK(result.value().empty());
    ++g_scenarios_exercised;
}

// ESCOPO.md SS2 decision 8: relative is the PRIMARY tested case, not
// the edge case - this scenario is deliberately not folded into the
// byte-exact round-trip case below, so the "relative path works at
// all" property has its own name and its own PASS/FAIL line.
GLINTFX_TEST(relative_path_is_the_primary_exercised_case) {
    const std::filesystem::path dir = make_scratch_dir("relative");
    std::filesystem::current_path(dir);
    write_file(dir / "payload.bin", {std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}});

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("payload.bin");

    GLINTFX_CHECK(result.has_value());
    const std::vector<std::byte> expected{std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}};
    GLINTFX_CHECK(result.value() == expected);
    ++g_scenarios_exercised;
}

GLINTFX_TEST(absolute_path_also_succeeds) {
    const std::filesystem::path dir = make_scratch_dir("absolute");
    // Deliberately NOT chdir-ing here: the whole point of this scenario
    // is that an absolute path does not depend on the process cwd at
    // all, unlike every relative-path scenario above.
    write_file(dir / "payload.bin", {std::byte{0x10}, std::byte{0x20}});

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes((dir / "payload.bin").string());

    GLINTFX_CHECK(result.has_value());
    const std::vector<std::byte> expected{std::byte{0x10}, std::byte{0x20}};
    GLINTFX_CHECK(result.value() == expected);
    ++g_scenarios_exercised;
}

// The case that separates "read the file" from "read the file
// correctly" - see this file's own header comment. Bytes chosen: a
// literal 0x00 in the MIDDLE of the buffer (not at the end, where a
// C-string-based bug could hide behind an accidental extra terminator),
// plus bytes with the high bit set (0x80 and above), which is what
// "non-ASCII" means at the byte level - this test makes no claim about
// any particular text encoding, only about byte fidelity.
GLINTFX_TEST(bytes_round_trip_including_embedded_null_and_non_ascii_bytes) {
    const std::filesystem::path dir = make_scratch_dir("hostile_bytes");
    std::filesystem::current_path(dir);

    const std::vector<std::byte> hostile_content{
        std::byte{0xDE}, std::byte{0xAD}, std::byte{0x00}, std::byte{0xBE},
        std::byte{0xEF}, std::byte{0xC3}, std::byte{0xA9}, std::byte{0xFF},
        std::byte{0x01}, std::byte{0x00}, std::byte{0x80},
    };
    write_file(dir / "hostile.bin", hostile_content);

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("hostile.bin");

    GLINTFX_CHECK(result.has_value());
    GLINTFX_CHECK(result.value().size() == hostile_content.size());
    GLINTFX_CHECK(result.value() == hostile_content);
}

// See this file's own "MID-STREAM READ FAILURE, NOT PART OF THE
// CLOSED SIX" header comment for why each mechanism below is genuinely
// OS-level (never a simulated stream state) and why neither branch
// increments g_scenarios_exercised.
#if defined(_WIN32)
GLINTFX_TEST(mid_stream_read_failure_is_io_failure_not_silent_partial_success) {
    const std::filesystem::path dir = make_scratch_dir("midstream");
    std::filesystem::current_path(dir);
    write_file(dir / "locked.bin",
               {std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}, std::byte{0xDD}});

    // Second handle, held for the whole scenario (exclusive_handle_guard,
    // same RAII shape no_read_permission_is_io_failure above already
    // uses): generous share flags on purpose - this handle only needs to
    // hold the LOCK, not deny the OPEN that gltfx_load_file_bytes()'s own
    // std::ifstream performs right below (that is a DIFFERENT scenario,
    // the sharing-violation one above; this one forces failure at READ,
    // after a successful open, per this file's own "MID-STREAM READ
    // FAILURE" header comment).
    const exclusive_handle_guard lock_handle(::CreateFileW(
        (dir / "locked.bin").c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr));
    GLINTFX_CHECK(lock_handle.is_valid());

    // Locks [0, 0xFFFFFFFF) - "Locking a region that goes beyond the
    // current end-of-file position is not an error" (same MS Learn page
    // as this file's own header comment), so this covers the whole
    // fixture above regardless of its exact size, with no dependency on
    // k_chunk_size (glintfx/platform/asset/file.hpp, private to that
    // header) or on where inside it the failure actually lands.
    OVERLAPPED overlapped{};
    const BOOL locked = ::LockFileEx(lock_handle.handle(), LOCKFILE_EXCLUSIVE_LOCK, 0, 0xFFFFFFFFU,
                                      0xFFFFFFFFU, &overlapped);
    // SECOND ASSERTION, DISTINCT FROM THE OUTCOME BELOW (same "two
    // facts, two assertions" shape as tests/harness/win_dll_alloc_
    // hook.hpp's own gancho-found-vs-gancho-had-no-effect split): proves
    // the FORCING MECHANISM ITSELF engaged. A LockFileEx that silently
    // failed to acquire would leave the file fully readable, and the
    // outcome assertion below would then fail too - but for the WRONG
    // reason (no forcing ever happened), which is exactly the "mechanism
    // that does not bite and the test passes/fails without proving
    // anything" shape this project has hit twice already this session.
    GLINTFX_CHECK(locked != 0);

    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("locked.bin");

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::io_failure);
}
#else
GLINTFX_TEST(mid_stream_read_failure_is_io_failure_not_silent_partial_success) {
    const glintfx::gltfx_rslt<std::vector<std::byte>> result =
        glintfx::asset::gltfx_load_file_bytes("/proc/self/mem");

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::io_failure);
}
#endif

// Closes the matrix (GODS_LAWS.md L-40): relies on declaration-order
// registration within this ONE translation unit (test_registry.cpp's
// own comment: CaseRegistrar's static constructor runs in file order),
// so this is the LAST GLINTFX_TEST in the file, on purpose - every
// scenario above increments g_scenarios_exercised, so a scenario that
// silently stopped running (renamed out of GLINTFX_TEST, or an early
// return before the increment) reproves HERE instead of the suite
// quietly reporting a smaller, still-green total.
GLINTFX_TEST(closed_error_matrix_was_fully_exercised) {
    std::println("asset_load_test: closed matrix exercised {} of {} scenarios",
                 g_scenarios_exercised, k_total_scenarios);
    GLINTFX_CHECK(g_scenarios_exercised == k_total_scenarios);
}
