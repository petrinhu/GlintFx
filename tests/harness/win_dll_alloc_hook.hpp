// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// win_dll_alloc_hook.hpp - CORE-ERROR (TODO.md, GODS_LAWS.md L-07/
// L-19/L-20/L-27/L-40): a WINDOWS-ONLY, TEST-ONLY mechanism that lets
// a test executable force a specific ALREADY-LOADED DLL's own calls
// into the CRT allocation primitives to fail, from OUTSIDE that DLL,
// without changing a single line of the library's own source.
//
// THE GAP THIS CLOSES, established by err_context_test.cpp's own
// header comment: replacing THIS translation unit's global operator
// new/delete only affects code THIS BINARY was linked with. On
// Windows, in SHARED mode, err.cpp's `new (std::nothrow)` and
// std::string's own growth are compiled INSIDE glintfx.dll and
// resolve, at THAT DLL's OWN link time, against whatever CRT it was
// linked with - PE/COFF has no ELF-style default global symbol
// interposition, so a test executable's operator-new override never
// reaches code running inside a DLL it merely links against. CI found
// this live (TODO.md CORE-ERROR row, 25/08/2026): both OOM-degradation
// cases in err_context_test.cpp passed on every other leg of the
// five-platform matrix and failed on Windows/SHARED specifically,
// which is exactly the shape this comment predicted before the CI run
// existed.
//
// THE MECHANISM, and what is FACT versus INFERENCE in it (GODS_LAWS.md
// L-27 - this project has no Windows machine, so nothing below is
// "confirmed by running it here"):
//
//   FACT, from Microsoft's own current documentation (learn.microsoft.
//   com/en-us/cpp/c-runtime-library/new-operator-crt, fetched
//   25/08/2026): "Beginning in Visual Studio 2013, [...] operator new
//   and operator delete [...] are now part of the C++ Standard
//   Library" - i.e. compiled STATICALLY into every binary since
//   VS2013/2015, even under /MD. There is therefore no IAT entry for
//   "operator new" itself to patch in glintfx.dll - the call
//   instructions for `new` live in glintfx.dll's own .text section.
//
//   FACT, same family of documentation (learn.microsoft.com/en-us/cpp/
//   c-runtime-library/crt-library-features): under /MD, the malloc/
//   free FAMILY of functions - unlike new/delete - remain genuine
//   Universal CRT (UCRT) functions, dynamically imported from
//   ucrtbase.dll. That import DOES live in glintfx.dll's own Import
//   Address Table (IAT): a writable, per-module data structure,
//   distinct from ucrtbase.dll's own code.
//
//   INFERENCE, sourced from a discussion in the official
//   microsoft/STL GitHub repository (issue #1066, contributors
//   familiar with the CRT's own source layout), NOT independently
//   verified against this exact toolset: MSVC's operator new
//   implementation (vcruntime's new_scalar.cpp) does not call the
//   public malloc() directly - it calls an internal primitive,
//   `_malloc_base`, which malloc() itself also calls in the Release
//   CRT (malloc_base.cpp). If true for the toolset windows-latest
//   runs, patching `_malloc_base`'s IAT entry inside glintfx.dll
//   intercepts operator new's calls there too, without needing to
//   touch operator new itself.
//
// WHY A CANDIDATE LIST, NOT ONE GUESSED NAME (GODS_LAWS.md L-27/L-40):
// the inference above is exactly the kind of premise L-27 says must
// not travel as fact, and this project cannot run `dumpbin /imports
// glintfx.dll` against the real toolset to settle it before writing
// code. Guessing a single name wrong would silently patch nothing -
// precisely the "portao que nao olha e ainda assim imprime verde"
// defect L-40 exists to forbid. Instead of betting on one name, this
// hooks EVERY name in a small, closed, documented candidate list that
// IS actually present in the target module's own import table
// (enumeration of a closed space, not a directed guess - L-40 item 5).
// patched_count() reports exactly how many were found: zero is a FACT
// this mechanism surfaces to the test ("this toolset does not route
// through anything on the list"), never a silent no-op mistaken for
// success.
//
// CONFINED TO TEST, NEVER TOUCHES PRODUCT CODE: this header is
// #include-d ONLY from a test .cpp (tests/err_context_test.cpp).
// Nothing in include/ or src/ references it, changes because of it,
// or needs to know it exists - not the tail wagging the dog. It also
// does nothing outside WIN32+SHARED: on Linux the whole file is not
// even parsed, and on a Windows STATIC build
// GLINTFX_STATIC_DEFINE (the PUBLIC define GlintfxLibrary.cmake
// attaches to every consumer of a static glintfx, so every test
// executable already sees it) guards it off too - there is no
// separate glintfx.dll module to find in that configuration, and this
// TU's own operator-new override already covers that leg (see the CI
// table in this project's CLAUDE.md: Windows static already passes).
//
// SCOPE, DELIBERATELY NARROW: this only intercepts ALLOCATION
// (malloc-family), never free/realloc - the two call sites this hook
// exists for (ensure_context()'s `new (std::nothrow)`, and
// std::string::assign()'s internal growth) only need an allocation to
// fail; nothing here needs to observe or corrupt a matching
// deallocation. Extending the candidate list to cover free() would be
// a different, unproven claim this file does not make.
//
// Function/object pointer punning below (a function address stored
// through a `void*` IAT slot) is not portable ISO C++, but is
// well-defined in practice on the only toolchain this file is ever
// compiled by (MSVC, guarded by _WIN32) - the same accommodation
// PE-import-table tooling in general relies on.

#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <string_view>

namespace glintfx_test {

namespace detail {

// The candidate set (GODS_LAWS.md L-40 item 5: enumerate the closed
// space instead of guessing one name). See this file's own header
// comment for what is FACT and what is INFERENCE behind each name.
inline constexpr std::array<std::string_view, 2> k_candidate_names = {
    "_malloc_base",
    "malloc",
};

inline bool g_force_failure = false;
inline std::size_t g_hooked_call_count = 0;

using malloc_fn = void *(__cdecl *)(std::size_t);
inline malloc_fn g_original_malloc = nullptr;

// The stub every patched IAT entry is redirected to. Same
// pass-through-or-fail shape as err_context_test.cpp's own operator
// new override: a call that is not being forced still allocates for
// real, through the ORIGINAL function this hook saved before
// patching - never a reimplementation of the allocator.
inline void *__cdecl hooked_malloc(std::size_t size) noexcept {
    ++g_hooked_call_count;
    if (g_force_failure) {
        return nullptr;
    }
    return g_original_malloc != nullptr ? g_original_malloc(size) : std::malloc(size);
}

// One patched IAT slot: where it lives, and what was there before -
// enough to restore it exactly on destruction.
struct patched_slot {
    void **iat_entry = nullptr;
    void *original_value = nullptr;
};

} // namespace detail

// Patches every candidate CRT allocation primitive that `module_name`
// actually imports, for as long as this object lives.
class dll_alloc_hook {
  public:
    explicit dll_alloc_hook(const wchar_t *module_name) noexcept { patch(module_name); }

    ~dll_alloc_hook() { restore(); }

    dll_alloc_hook(const dll_alloc_hook &) = delete;
    dll_alloc_hook &operator=(const dll_alloc_hook &) = delete;

    // GODS_LAWS.md L-40: how many of the closed candidate names were
    // actually found and patched in `module_name`'s own import table.
    // Zero is a FACT to report (this toolset's operator new does not
    // route through anything on the candidate list above), never
    // silently treated as success by a caller that forgets to check
    // it.
    [[nodiscard]] std::size_t patched_count() const noexcept { return m_patched_count; }

  private:
    void patch(const wchar_t *module_name) noexcept {
        HMODULE module = ::GetModuleHandleW(module_name);
        if (module == nullptr) {
            return;
        }
        auto *base = reinterpret_cast<std::uint8_t *>(module);
        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return;
        }
        const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) {
            return;
        }
        const auto &import_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (import_dir.VirtualAddress == 0) {
            return;
        }
        auto *descriptor =
            reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + import_dir.VirtualAddress);
        for (; descriptor->Name != 0; ++descriptor) {
            patch_descriptor(base, *descriptor);
        }
    }

    void patch_descriptor(std::uint8_t *base, const IMAGE_IMPORT_DESCRIPTOR &descriptor) noexcept {
        // OriginalFirstThunk (the INT) carries the NAMES and is
        // read-only in the image; FirstThunk (the IAT) carries the
        // resolved CALL TARGETS the compiled code actually jumps
        // through, and is what this hook overwrites. Some linkers
        // omit the INT (OriginalFirstThunk == 0); falling back to the
        // IAT for names too is the documented tolerance for that
        // case.
        const std::uintptr_t name_thunk_rva = descriptor.OriginalFirstThunk != 0
                                                   ? descriptor.OriginalFirstThunk
                                                   : descriptor.FirstThunk;
        auto *name_thunk = reinterpret_cast<const IMAGE_THUNK_DATA *>(base + name_thunk_rva);
        auto *iat_thunk = reinterpret_cast<IMAGE_THUNK_DATA *>(base + descriptor.FirstThunk);

        for (; name_thunk->u1.AddressOfData != 0; ++name_thunk, ++iat_thunk) {
            if ((name_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) != 0) {
                continue; // Imported by ordinal, not by name - none of
                          // this hook's candidates are looked up this
                          // way.
            }
            const auto *import_by_name =
                reinterpret_cast<const IMAGE_IMPORT_BY_NAME *>(base + name_thunk->u1.AddressOfData);
            const std::string_view imported_name{
                reinterpret_cast<const char *>(import_by_name->Name)};
            for (std::string_view candidate : detail::k_candidate_names) {
                if (imported_name == candidate) {
                    patch_slot(iat_thunk);
                    break;
                }
            }
        }
    }

    void patch_slot(IMAGE_THUNK_DATA *iat_thunk) noexcept {
        if (m_patched_count >= m_patched.size()) {
            return; // Bounded by k_candidate_names' own size above -
                     // unreachable in practice, kept as a hard bound
                     // instead of an unbounded container (this hook
                     // must not itself allocate while it is busy
                     // redirecting the allocator).
        }
        auto **entry = reinterpret_cast<void **>(&iat_thunk->u1.Function);
        DWORD old_protect = 0;
        if (::VirtualProtect(entry, sizeof(void *), PAGE_READWRITE, &old_protect) == 0) {
            return;
        }
        // First candidate found wins the "original" slot -
        // detail::g_original_malloc only needs ONE real implementation
        // to pass calls through to.
        if (detail::g_original_malloc == nullptr) {
            detail::g_original_malloc = reinterpret_cast<detail::malloc_fn>(*entry);
        }
        m_patched[m_patched_count] = detail::patched_slot{entry, *entry};
        ++m_patched_count;
        *entry = reinterpret_cast<void *>(&detail::hooked_malloc);
        ::VirtualProtect(entry, sizeof(void *), old_protect, &old_protect);
    }

    void restore() noexcept {
        for (std::size_t i = m_patched_count; i > 0; --i) {
            const detail::patched_slot &slot = m_patched[i - 1];
            DWORD old_protect = 0;
            if (::VirtualProtect(slot.iat_entry, sizeof(void *), PAGE_READWRITE, &old_protect) ==
                0) {
                continue;
            }
            *slot.iat_entry = slot.original_value;
            ::VirtualProtect(slot.iat_entry, sizeof(void *), old_protect, &old_protect);
        }
        if (m_patched_count == 0) {
            detail::g_original_malloc = nullptr;
        }
    }

    // Bounded by k_candidate_names' own size - see patch_slot().
    std::array<detail::patched_slot, detail::k_candidate_names.size()> m_patched{};
    std::size_t m_patched_count = 0;
};

inline void arm_forced_failure() noexcept { detail::g_force_failure = true; }
inline void disarm_forced_failure() noexcept { detail::g_force_failure = false; }
[[nodiscard]] inline std::size_t hooked_call_count() noexcept { return detail::g_hooked_call_count; }

} // namespace glintfx_test

#endif // defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
