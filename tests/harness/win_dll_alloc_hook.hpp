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
//
// TWO INBOX FINDINGS FIXED 26/08/2026 (GODS_LAWS.md L-40), both about
// this file's own silence, not about the PE-walking logic itself:
//
//   achado 1 - "o portao passa, mas nao diz o que verificou": before
//   this date, a run that patched something never named WHICH
//   candidate matched, and a run that patched NOTHING looked, from the
//   outside, exactly like one that never ran the check at all -
//   patched_count() == 0 either way. glintfx_test::format_patch_
//   diagnostic() below turns both cases into one explicit line every
//   dll_alloc_hook construction prints to stderr: on success it names
//   every candidate that matched; on zero matches it says so in words
//   ("matched NONE...") and enumerates the WHOLE closed candidate list
//   that was searched, so a stale list is legible straight from a CI
//   log, without attaching a debugger to a Windows runner.
//
//   achado 2 - "restauracao de protecao de memoria sem conferir
//   retorno": patch_slot() and restore() each call VirtualProtect
//   TWICE - once to make an IAT slot writable (already checked before
//   this date), once to put the ORIGINAL protection flags back
//   afterward (NOT checked before this date). A failed restore left
//   the page's protection silently wrong while the caller believed
//   nothing had gone wrong. THE POLICY CHOSEN, and why (this file had
//   no existing precedent for a "put it back" call specifically - the
//   two calls that already existed are both GATES an action, which
//   this pair is not): report visibly via stderr through
//   glintfx_test::detail::format_protect_restore_failure(), never
//   abort. Two reasons, not one: (a) in BOTH call sites the important
//   state change already happened BEFORE this second VirtualProtect
//   runs - patch_slot() has already written the hook's function
//   pointer into the slot, restore() has already written the ORIGINAL
//   pointer back - so a failure here is the page's protection flags
//   staying non-original, not the pointer swap failing; (b) restore()
//   runs from ~dll_alloc_hook(), i.e. from a test's cleanup path that
//   may already have PASSED - killing the process there would destroy
//   a real, correct result to report a cosmetic cleanup failure
//   instead. This mirrors docs/api-conventions.md R3's own best-effort
//   degradation policy (an operation that fails degrades to a poorer,
//   visible form instead of aborting the caller), applied here to a
//   cleanup step instead of to an allocation.
//
// A THIRD INBOX FINDING FIXED THE SAME DAY, 26/08/2026 (GODS_LAWS.md
// L-40), on a different call site than achado 2 above:
//
//   achado 3 - "a PRIMEIRA protecao de memoria absorve a falha em
//   silencio": patch_slot() calls VirtualProtect TWICE. Achado 2 above
//   fixed the SECOND call (put the original protection back
//   afterward). This finding is about the FIRST one - the call that
//   asks for write access to an IAT slot BEFORE the hook's function
//   pointer is written there - which still returned silently
//   (`return;`, nothing reported) when it failed. Unlike achado 2's
//   call, this one is an ENTRY GATE, not a cleanup step: nothing has
//   happened yet when it runs, so a failure here means the matched
//   candidate simply never becomes a patched slot - INDISTINGUISHABLE,
//   without this fix, from "this candidate's name was never present in
//   the import table" (format_patch_diagnostic()'s own "matched
//   NONE... searched: ..." case). THE POLICY CHOSEN: still report via
//   stderr and keep scanning, never abort - but for reasons specific to
//   an entry gate, not achado 2's cleanup-path reasons (see the
//   comment on this exact VirtualProtect call, and on
//   detail::format_write_access_failure(), below, for the full
//   argument). detail::format_write_access_failure() renders a message
//   that names the SPECIFIC candidate found, so it can never read the
//   same as format_patch_diagnostic()'s "absent" message - closing the
//   ambiguity achado 3 is about.
//
// DECLARED SCOPE OF WHAT WAS ACTUALLY PROVEN FOR ALL THREE FIXES
// (GODS_LAWS.md L-09/L-20): this project has no Windows machine. The
// three pure functions behind the three findings -
// format_patch_diagnostic(), detail::format_protect_restore_failure()
// and detail::format_write_access_failure() - do no Windows API call
// and no I/O, so they are deliberately declared OUTSIDE the
// `#if defined(_WIN32)` guard below and are red/green unit-tested on
// every platform this project builds on, including this (Linux)
// machine - see tests/win_dll_alloc_hook_format_test.cpp and
// tests/win_dll_alloc_hook_protect_restore_format_test.cpp. Everything
// inside the guard below - the IAT walk itself, the three
// VirtualProtect call sites that now check their return value, the
// stderr writes that consume the three functions above - is
// Windows-only code this session could only review, never compile or
// run; that half remains the same declared downgrade the rest of this
// file already carried before today.

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace glintfx_test {

namespace detail {

// The candidate set (GODS_LAWS.md L-40 item 5: enumerate the closed
// space instead of guessing one name). See this file's own header
// comment for what is FACT and what is INFERENCE behind each name.
// Declared OUTSIDE the Windows-only guard below: the list itself, and
// the diagnostic built from it, are pure data with no Windows API
// involved - only the ACT of walking a real PE import table for it is
// Windows-only.
inline constexpr std::array<std::string_view, 2> k_candidate_names = {
    "_malloc_base",
    "malloc",
};

} // namespace detail

// GODS_LAWS.md L-40, achado 1 of 26/08/2026 ("o portao passa, mas nao
// diz o que verificou"): renders the one human-readable line a
// dll_alloc_hook construction prints, on EVERY run, patched or not.
// `matched_names` is the subset of detail::k_candidate_names this run
// actually found in the target module's own import table, in the
// order they were found; empty means none of them were - the message
// then names every candidate that WAS searched for, so a stale list
// (or a differently-named symbol that slipped past it) is legible from
// a CI log alone, never indistinguishable from a genuine, verified
// pass.
[[nodiscard]] inline std::string
format_patch_diagnostic(std::span<const std::string_view> matched_names) {
    if (matched_names.empty()) {
        std::string message = "glintfx_test::dll_alloc_hook: matched NONE of the candidate "
                              "allocation primitives; searched: ";
        for (std::size_t i = 0; i < detail::k_candidate_names.size(); ++i) {
            if (i > 0) {
                message += ", ";
            }
            message += detail::k_candidate_names[i];
        }
        return message;
    }
    std::string message = "glintfx_test::dll_alloc_hook: patched ";
    for (std::size_t i = 0; i < matched_names.size(); ++i) {
        if (i > 0) {
            message += ", ";
        }
        message += matched_names[i];
    }
    return message;
}

namespace detail {

// GODS_LAWS.md L-40, achado 2 of 26/08/2026 ("restauracao de protecao
// de memoria sem conferir retorno"): pure formatting for the one
// diagnostic line a failed VirtualProtect-restore now prints. `phase`
// names WHICH of the two call sites failed ("patch" or "restore" - see
// dll_alloc_hook::patch_slot()/restore() below), `last_error` is the
// raw value ::GetLastError() returned at the point of failure. Kept
// pure and platform-independent (unsigned long round-trips through
// GetLastError() but is not itself a Windows type) so it is red/green
// testable on every platform, same as format_patch_diagnostic() above.
[[nodiscard]] inline std::string format_protect_restore_failure(std::string_view phase,
                                                                unsigned long last_error) {
    std::string message =
        "glintfx_test::dll_alloc_hook: VirtualProtect failed to restore original page "
        "protection during ";
    message += phase;
    message += " (GetLastError=";
    message += std::to_string(last_error);
    message += "); import table entry may be left writable";
    return message;
}

// GODS_LAWS.md L-40, INBOX achado of 26/08/2026 (third finding on this
// file the same day, distinct from the two format_protect_restore_
// failure() above already closes): patch_slot()'s FIRST VirtualProtect
// call - the one asking for write access to an IAT slot BEFORE the
// hook's function pointer is written there - used to fail silently
// (`return;`, nothing reported). That is a DIFFERENT failure shape
// than the one format_protect_restore_failure() covers: this call is
// an ENTRY GATE (nothing has happened yet when it runs), not a cleanup
// step (both call sites format_protect_restore_failure() serves run
// AFTER the state change that matters already succeeded). On failure
// here, the candidate simply never becomes a matched slot - which,
// without this function, was INDISTINGUISHABLE from "this candidate's
// name was never present in the import table" (format_patch_
// diagnostic()'s "matched NONE... searched: ..." case above). This
// message is deliberately shaped to never be confusable with that one:
// it names the SPECIFIC candidate that WAS found by name and says so
// explicitly, instead of only listing what was searched for.
[[nodiscard]] inline std::string format_write_access_failure(std::string_view candidate_name,
                                                             unsigned long last_error) {
    std::string message = "glintfx_test::dll_alloc_hook: found '";
    message += candidate_name;
    message += "' in the import table but VirtualProtect failed to grant write access "
               "(GetLastError=";
    message += std::to_string(last_error);
    message += "); leaving this candidate unpatched";
    return message;
}

} // namespace detail

} // namespace glintfx_test

#if defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>

namespace glintfx_test {

namespace detail {

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

// One patched IAT slot: where it lives, what was there before - enough
// to restore it exactly on destruction - and, since 26/08/2026 (achado
// 1), the NAME of the candidate that matched here, so a successful run
// can say what it found instead of only how many.
struct patched_slot {
    void **iat_entry = nullptr;
    void *original_value = nullptr;
    std::string_view name;
};

} // namespace detail

// Patches every candidate CRT allocation primitive that `module_name`
// actually imports, for as long as this object lives.
class dll_alloc_hook {
  public:
    explicit dll_alloc_hook(const wchar_t *module_name) noexcept {
        patch(module_name);
        report_patch_result();
    }

    ~dll_alloc_hook() { restore(); }

    dll_alloc_hook(const dll_alloc_hook &) = delete;
    dll_alloc_hook &operator=(const dll_alloc_hook &) = delete;

    // GODS_LAWS.md L-40: how many of the closed candidate names were
    // actually found and patched in `module_name`'s own import table.
    // Zero is a FACT to report (this toolset's operator new does not
    // route through anything on the candidate list above), never
    // silently treated as success by a caller that forgets to check
    // it. report_patch_result() (called from the constructor above)
    // already prints the same fact, plus the NAMES, unconditionally -
    // this accessor lets a caller additionally gate its own assertions
    // on it (see err_context_test.cpp's allocator_reach_probe).
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
                    patch_slot(iat_thunk, candidate);
                    break;
                }
            }
        }
    }

    void patch_slot(IMAGE_THUNK_DATA *iat_thunk, std::string_view matched_name) noexcept {
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
            // GODS_LAWS.md L-40, INBOX achado of 26/08/2026 (entry-gate
            // finding, distinct from the two "restore" call sites
            // fixed earlier the same day): POLICY CHOSEN - report via
            // stderr and continue scanning, never abort the process.
            // The two reasons behind the earlier "report, never abort"
            // choice for the RESTORE calls do NOT hold here (this is
            // an entry gate, not a cleanup path: (a) there, the pointer
            // swap had already happened before the failing call; here,
            // nothing has happened yet - matched_name simply never
            // becomes a patched slot, which is the correct, harmless
            // outcome; (b) there, restore() runs from a destructor that
            // may follow a PASSED test; here, patch() runs from the
            // CONSTRUCTOR, before any test body has executed, so there
            // is no passed result an abort would destroy). Kept
            // non-fatal anyway, for two DIFFERENT reasons specific to
            // this entry gate: first, dll_alloc_hook's constructor is
            // deliberately noexcept and this loop still has OTHER
            // import descriptors and OTHER candidate names left to
            // examine - aborting here would also forfeit a candidate
            // that might still succeed (e.g. "malloc" failing to
            // protect must not prevent trying "_malloc_base"); second,
            // this hook is constructed from inside a single test
            // executable's process that GLINTFX_TEST's registry may run
            // several cases in - killing the process here would forfeit
            // every OTHER test case's result to report one candidate's
            // cosmetic inability to gain write access. What changes
            // from the restore case is not the action (report, do not
            // abort) but the CONTENT of the report:
            // format_write_access_failure() names the specific
            // candidate that WAS found, so this failure reads as
            // distinct from format_patch_diagnostic()'s "matched
            // NONE... searched: ..." message for a name that was never
            // present at all - see that function's own comment above
            // for the ambiguity this closes.
            std::fprintf(
                stderr, "%s\n",
                detail::format_write_access_failure(matched_name, ::GetLastError()).c_str());
            return;
        }
        // First candidate found wins the "original" slot -
        // detail::g_original_malloc only needs ONE real implementation
        // to pass calls through to.
        if (detail::g_original_malloc == nullptr) {
            detail::g_original_malloc = reinterpret_cast<detail::malloc_fn>(*entry);
        }
        m_patched[m_patched_count] = detail::patched_slot{entry, *entry, matched_name};
        ++m_patched_count;
        *entry = reinterpret_cast<void *>(&detail::hooked_malloc);
        DWORD restored_protect = 0;
        if (::VirtualProtect(entry, sizeof(void *), old_protect, &restored_protect) == 0) {
            // GODS_LAWS.md L-40, achado 2 of 26/08/2026: the hook is
            // already installed (the write above already succeeded) -
            // only the page's protection flags failed to go back to
            // what they were. Reported, never fatal: see this file's
            // own top comment for why aborting here would be the wrong
            // policy, not just an omission.
            std::fprintf(stderr, "%s\n",
                         detail::format_protect_restore_failure("patch", ::GetLastError()).c_str());
        }
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
            DWORD restored_protect = 0;
            if (::VirtualProtect(slot.iat_entry, sizeof(void *), old_protect, &restored_protect) ==
                0) {
                // GODS_LAWS.md L-40, achado 2 of 26/08/2026: the
                // pointer this hook owns (slot.original_value) is
                // ALREADY restored by the assignment above - the only
                // thing that failed is putting the page's protection
                // flags back. Reported, never fatal: this runs from
                // the destructor, at the tail of a test that may
                // already have PASSED, and killing the process here
                // would destroy that real result to report a cosmetic
                // cleanup failure instead - see this file's own top
                // comment for the full reasoning.
                std::fprintf(
                    stderr, "%s\n",
                    detail::format_protect_restore_failure("restore", ::GetLastError()).c_str());
            }
        }
        if (m_patched_count == 0) {
            detail::g_original_malloc = nullptr;
        }
    }

    // GODS_LAWS.md L-40, achado 1 of 26/08/2026: prints, unconditionally
    // and on every construction, the one line format_patch_diagnostic()
    // renders - which candidates matched on success, or the declared
    // failure naming the whole searched list on zero matches. Never
    // silent either way.
    void report_patch_result() const noexcept {
        std::array<std::string_view, detail::k_candidate_names.size()> matched{};
        for (std::size_t i = 0; i < m_patched_count; ++i) {
            matched[i] = m_patched[i].name;
        }
        const std::string message = format_patch_diagnostic(
            std::span<const std::string_view>(matched.data(), m_patched_count));
        std::fprintf(stderr, "%s\n", message.c_str());
    }

    // Bounded by k_candidate_names' own size - see patch_slot().
    std::array<detail::patched_slot, detail::k_candidate_names.size()> m_patched{};
    std::size_t m_patched_count = 0;
};

inline void arm_forced_failure() noexcept { detail::g_force_failure = true; }
inline void disarm_forced_failure() noexcept { detail::g_force_failure = false; }
[[nodiscard]] inline std::size_t hooked_call_count() noexcept {
    return detail::g_hooked_call_count;
}

} // namespace glintfx_test

#endif // defined(_WIN32) && !defined(GLINTFX_STATIC_DEFINE)
