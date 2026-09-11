// SPDX-License-Identifier: AGPL-3.0-or-later
#ifndef _GNU_SOURCE
#define _GNU_SOURCE // program_invocation_short_name (glibc, <errno.h>) -
                    // g++ already predefines this on Linux (libstdc++
                    // needs it too), guarded here so this TU does not
                    // depend on that staying true.
#endif

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#include <link.h>
#include <new>
#include <unistd.h>

#include "alloc_counter_classify.hpp"

// alloc_counter_hook.cpp - CONTAINER-LEAK-COUNTER sub-fatia S2 (/var/
// tmp/glintfx-plan/leak-counter.md sec. 5, GODS_LAWS.md L-04/L-07/
// L-09/L-20/L-27/L-40/L-45): the allocation-counting gancho compiled
// straight into each of the 18 container fixtures (Containerfile,
// F4 - no libglintfx.so in this build, so there is no DSO boundary
// this substitution could fail to cross).
//
// WHAT THIS FILE PROVES, AND HOW (leak-counter.md sec. 3, the four
// proofs; this file is only proof 2 and 4's producer - proofs 1 and 3
// are the job step and the red-start fixture, not this file):
// replaces the global operator new/delete (scalar, both throwing and
// nothrow forms, array forms delegating - same idiom tests/err_no_
// alloc_test.cpp already uses), classifies each live block by the
// stack frame that asked for it (glintfx_leak_counter::classify_
// allocation_frames, S1, tested in isolation in tests/alloc_counter_
// classify_test.cpp), and prints a report at process exit via
// std::atexit registered from a constructor(101) (matches F12: this
// runs AFTER every static destructor, exe's and any .so's, so the
// report reflects the TRUE final state, not a snapshot mid-teardown).
//
// REENTRANCY (F13, measured live before this file existed): glibc's
// backtrace() can itself allocate in rare paths (unwind-table lazy
// load, first-call PLT resolution). g_hook_depth guards against
// recursing into backtrace() from inside backtrace()'s own allocation
// - the inner call is classified conservatively as third_party
// (evidence is unavailable, never guessed as ours) instead of being
// captured a second time.
//
// WHY EVERY FUNCTION IN THIS FILE CARRIES THE SAME [[gnu::section]]
// ATTRIBUTE (leak-counter.md sec. 2, "faixa hook" of alloc_classify_
// ranges): classify_allocation_frames() needs to skip frames that
// belong to THIS file's own call chain (operator new -> the nothrow
// entry point -> the impl function -> the array delegate that called
// it) before it can decide who REALLY asked for the memory - every
// one of those frames sits inside the executable's own [__executable_
// start, etext) range (F4: this file compiles straight into the
// fixture binary, same as src/), so without a distinct "hook" range
// the classifier would misread the gancho's OWN frames as `ours`.
// __start_gfx_alloc_hook/__stop_gfx_alloc_hook are ld's own auto-
// generated boundary symbols for a named section (GNU ld manual,
// "Input Section Example"; the same mechanism the Linux kernel uses
// for initcall tables) - no linker script, no -rdynamic (leak-
// counter.md sec. 2 says explicitly this gancho does not need it).
//
// WHAT THIS FILE DELIBERATELY DOES NOT INTERCEPT (leak-counter.md
// sec. 6, "o que nao prova"): the aligned forms (operator new(size,
// std::align_val_t)) - F5 measured 0/18 fixtures import them today;
// proof 1 (the job's own `nm` step, not in this file) reproves the
// day one does, forcing this gancho to grow rather than silently
// missing an allocation kind.
//
// THE 16-BYTE HEADER AND max_align_t (leak-counter.md sec. 4): glibc's
// malloc already returns blocks aligned to 2*sizeof(void*) == 16 on
// x86-64 for any request, so placing a 16-byte header immediately
// before the block malloc() itself returned keeps the USER pointer
// (header + 1) at the same 16-byte alignment std::malloc always gives
// - no extra arithmetic needed to satisfy max_align_t.

namespace {

#if defined(__GNUC__)
#define GFX_HOOK_FN [[gnu::section("gfx_alloc_hook")]]
#else
#define GFX_HOOK_FN
#endif

constexpr std::size_t k_ours_table_capacity = 4096;
constexpr std::size_t k_max_leak_lines = 32;
constexpr std::size_t k_max_backtrace_frames = 16;
constexpr std::uint32_t k_alloc_magic = 0x474C4B31u; // "GLK1", ASCII

enum class alloc_owner_klass : std::uint8_t {
    ours = 0,
    third_party = 1,
};

// Exactly 16 bytes, alignof 8 (a const void* dominates): sits directly
// in front of the pointer this file hands back to the caller.
struct alloc_header {
    std::uint32_t magic;
    alloc_owner_klass klass;
    const void *decisive_frame;
};
static_assert(sizeof(alloc_header) == 16, "leak-counter.md sec. 4: 16-byte header, by design");

// --- counters, all relaxed: nothing here orders memory the report
// depends on beyond "eventually visible", and every counter this file
// touches is read only once, at exit, after every allocating thread
// this process ever had has already been joined or has exited
// (fixtures are single-process test binaries, never daemonized). ----
std::atomic<std::uint64_t> g_new_calls{0};
std::atomic<std::uint64_t> g_delete_calls{0};
std::atomic<std::uint64_t> g_live_ours{0};
std::atomic<std::uint64_t> g_live_third_party{0};
std::atomic<std::uint64_t> g_ours_overflow{0};
std::atomic<std::uint64_t> g_foreign_delete{0};

// Live-block table for the `ours` class only (leak-counter.md sec. 4:
// "third_party` guarda so contadores" - LLVM/gallium alone can hold
// hundreds of thousands of blocks, F9/F10, far past anything worth
// tracking by address). A spinlock, not a mutex: contention here is
// only ever the allocations THIS fixture's own single-digit-to-dozens
// of `ours` blocks make (leak-counter.md sec. 3 proof 2's own piso
// reasoning), never a hot path.
std::atomic_flag g_ours_table_lock = ATOMIC_FLAG_INIT;
const void *g_ours_table[k_ours_table_capacity];
std::size_t g_ours_table_count = 0; // guarded by g_ours_table_lock

// The four ranges classify_allocation_frames() compares against - all
// zero-initialized (BSS: {nullptr, nullptr}) until compute_ranges()
// (constructor(101)) fills them in. Before that point every contains()
// is false by construction, so any allocation that happens to occur
// before our own constructor runs degrades to third_party rather than
// a false-positive `ours` - declared, not silently assumed (the same
// "evidence must be positive" rule S1's own header documents for an
// empty backtrace).
glintfx_leak_counter::alloc_classify_ranges g_ranges;

thread_local int g_hook_depth = 0;

extern "C" {
// __executable_start is GNU ld's own default linker-script symbol name (ld
// manual, "Options that control the default linker script"), not a name
// this file invented - the only way to reference it is to spell it exactly.
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp) reason: see above
extern char __executable_start[];
extern char etext[];
// __start_<section>/__stop_<section> are ld's own auto-generated boundary
// symbols for a named section (ld manual, "Input Section Example") - the
// name is dictated by the section name below (gfx_alloc_hook), not chosen
// here.
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp) reason: see above
extern char __start_gfx_alloc_hook[];
// NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp) reason: see above
extern char __stop_gfx_alloc_hook[];
}

GFX_HOOK_FN void table_lock() noexcept {
    while (g_ours_table_lock.test_and_set(std::memory_order_acquire)) {
        // spin - see the table's own comment above for why this never
        // holds long enough to justify a real mutex/futex.
    }
}

GFX_HOOK_FN void table_unlock() noexcept { g_ours_table_lock.clear(std::memory_order_release); }

GFX_HOOK_FN void table_insert(const void *user_ptr) noexcept {
    table_lock();
    if (g_ours_table_count < k_ours_table_capacity) {
        g_ours_table[g_ours_table_count] = user_ptr;
        ++g_ours_table_count;
    } else {
        // Estouro reprova, nunca cala (leak-counter.md sec. 4): the
        // COUNTER below still tracks the true live count via g_live_
        // ours regardless of table capacity - only the ADDRESS list
        // this table backs (for the ALLOC_LEAK report lines) is capped.
        g_ours_overflow.fetch_add(1, std::memory_order_relaxed);
    }
    table_unlock();
}

GFX_HOOK_FN void table_remove(const void *user_ptr) noexcept {
    table_lock();
    for (std::size_t i = 0; i < g_ours_table_count; ++i) {
        if (g_ours_table[i] == user_ptr) {
            g_ours_table[i] = g_ours_table[g_ours_table_count - 1];
            --g_ours_table_count;
            break;
        }
        // Not found is not an error here: the table can legitimately
        // omit a live pointer that arrived while it was already full
        // (the overflow case above) - its counter-side bookkeeping
        // (g_live_ours) still stays correct either way.
    }
    table_unlock();
}

// dl_iterate_phdr callback (leak-counter.md sec. 2): walks every
// currently-loaded object ONCE, at constructor(101) time, looking for
// libstdc++ and libc by a substring of their SONAME - the main
// executable itself reports dlpi_name == "" and never matches either,
// so it is left alone for the separate __executable_start/etext pair
// above to cover.
struct dl_scan_state {
    glintfx_leak_counter::address_range *libstdcxx;
    glintfx_leak_counter::address_range *libc;
};

GFX_HOOK_FN int phdr_scan_callback(dl_phdr_info *info, std::size_t /*size*/, void *data) noexcept {
    auto *state = static_cast<dl_scan_state *>(data);
    const char *name = info->dlpi_name;
    if (name == nullptr || name[0] == '\0') {
        return 0;
    }
    const bool is_libstdcxx = std::strstr(name, "libstdc++.so") != nullptr;
    const bool is_libc = std::strstr(name, "libc.so") != nullptr;
    if (!is_libstdcxx && !is_libc) {
        return 0;
    }

    std::uintptr_t lo = 0;
    std::uintptr_t hi = 0;
    bool any_load = false;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr) &phdr = info->dlpi_phdr[i];
        if (phdr.p_type != PT_LOAD) {
            continue;
        }
        const std::uintptr_t seg_lo = static_cast<std::uintptr_t>(info->dlpi_addr) + phdr.p_vaddr;
        const std::uintptr_t seg_hi = seg_lo + phdr.p_memsz;
        if (!any_load || seg_lo < lo) {
            lo = seg_lo;
        }
        if (!any_load || seg_hi > hi) {
            hi = seg_hi;
        }
        any_load = true;
    }
    if (!any_load) {
        return 0;
    }

    // lo/hi are dl_iterate_phdr's own dlpi_addr+p_vaddr arithmetic (uintptr_t by
    // construction) - the address converted back to a pointer here is never
    // dereferenced, only compared (glintfx_leak_counter::address_range::
    // contains(), same idiom tests/alloc_counter_classify_test.cpp's own
    // fake_addr() already documents). One variable per cast, each with its own
    // NOLINTNEXTLINE directly above it, so clang-format re-wrapping the
    // expression can never separate the marker from the line it covers again
    // (measured: it did, the first time this was one brace-init statement).
    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
    const void *range_begin = reinterpret_cast<const void *>(lo);
    // NOLINTNEXTLINE(performance-no-int-to-ptr) reason: see comment above
    const void *range_end = reinterpret_cast<const void *>(hi);
    const glintfx_leak_counter::address_range range{range_begin, range_end};
    if (is_libstdcxx) {
        *state->libstdcxx = range;
    }
    if (is_libc) {
        *state->libc = range;
    }
    return 0;
}

GFX_HOOK_FN void write_all(int fd, const char *buf, std::size_t len) noexcept {
    std::size_t off = 0;
    while (off < len) {
        const ssize_t n = ::write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return; // best-effort report: nothing else to do with a
                    // broken output fd at process-exit time.
        }
        if (n == 0) {
            return;
        }
        off += static_cast<std::size_t>(n);
    }
}

// The report itself (leak-counter.md sec. 5 S2, exact line format the
// job's check_alloc_report.sh - S3 - reads). snprintf into a stack
// buffer, then write(2): NEVER std::print/operator<</std::string -
// this function runs from std::atexit, after every static destructor
// has already run (F12), and must not itself register as a NEW `ours`
// allocation the moment after the numbers it is about to print were
// already read.
GFX_HOOK_FN void print_report() noexcept {
    const char *fixture = program_invocation_short_name;
    if (fixture == nullptr) {
        fixture = "unknown_fixture"; // glibc always sets this from
                                     // argv[0]; the fallback exists so
                                     // a future libc that ever leaves
                                     // it null still emits a well-
                                     // formed (if unattributed) report
                                     // instead of a null dereference.
    }

    char buf[320];
    const std::uint64_t new_calls = g_new_calls.load(std::memory_order_relaxed);
    const std::uint64_t delete_calls = g_delete_calls.load(std::memory_order_relaxed);
    const std::uint64_t live_ours = g_live_ours.load(std::memory_order_relaxed);
    const std::uint64_t live_third_party = g_live_third_party.load(std::memory_order_relaxed);
    const std::uint64_t ours_overflow = g_ours_overflow.load(std::memory_order_relaxed);
    const std::uint64_t foreign_delete = g_foreign_delete.load(std::memory_order_relaxed);

    struct {
        const char *key;
        std::uint64_t value;
    } const lines[] = {
        {"alloc_new_calls", new_calls},         {"alloc_delete_calls", delete_calls},
        {"alloc_live_ours", live_ours},         {"alloc_live_third_party", live_third_party},
        {"alloc_ours_overflow", ours_overflow}, {"alloc_foreign_delete", foreign_delete},
    };
    for (const auto &line : lines) {
        const int n = std::snprintf(buf, sizeof(buf), "MEASURED %s.%s=%llu\n", fixture, line.key,
                                    static_cast<unsigned long long>(line.value));
        if (n > 0) {
            write_all(STDOUT_FILENO, buf, static_cast<std::size_t>(n));
        }
    }

    // ALLOC_LEAK lines: at most k_max_leak_lines, no lock needed here
    // - by the time atexit runs, every static destructor already ran
    // (F12), so no other thread is still calling operator new/delete
    // to race this read. The printed address is the DECISIVE FRAME
    // (the call site that asked for the block, read back from the
    // block's own header - leak-counter.md sec. 3 proof 3: this is
    // what addr2line -e <fixture> resolves to file:line, NOT the
    // block's own heap address, which addr2line cannot resolve to
    // anything).
    const std::size_t leak_count =
        g_ours_table_count < k_max_leak_lines ? g_ours_table_count : k_max_leak_lines;
    for (std::size_t i = 0; i < leak_count; ++i) {
        const auto *header = reinterpret_cast<const alloc_header *>(
            static_cast<const char *>(g_ours_table[i]) - sizeof(alloc_header));
        const int n =
            std::snprintf(buf, sizeof(buf), "ALLOC_LEAK ours=%p\n", header->decisive_frame);
        if (n > 0) {
            write_all(STDOUT_FILENO, buf, static_cast<std::size_t>(n));
        }
    }
}

GFX_HOOK_FN void compute_ranges() noexcept {
    g_ranges.executable = {static_cast<const void *>(&__executable_start[0]),
                           static_cast<const void *>(&etext[0])};
    g_ranges.hook = {static_cast<const void *>(&__start_gfx_alloc_hook[0]),
                     static_cast<const void *>(&__stop_gfx_alloc_hook[0])};
    dl_scan_state state{&g_ranges.libstdcxx, &g_ranges.libc};
    dl_iterate_phdr(&phdr_scan_callback, &state);
}

// constructor(101): the earliest priority GCC/Clang allow user code to
// claim (0-100 are reserved for the implementation, GCC manual,
// "Common Function Attributes") - runs before any static object in
// this executable or any shared object it links finishes constructing
// (F12), and crucially BEFORE main(). Registers print_report() via
// std::atexit here too (not as a separate call site) for the same
// reason F12 measured: atexit handlers run in REVERSE registration
// order, so registering as early as possible here is what makes this
// handler run LAST, after every static destructor.
//
// MEASURED BUG, FIXED HERE (CONTAINER-LEAK-COUNTER S2, found running
// the estreia against gpu_kind_report_smoke, not a synthetic case): a
// first version of this function ran through a global variable's own
// initializer (a lambda IIFE assigned to a namespace-scope const) -
// that is an ORDINARY global initializer with NO priority, so its
// place in the init order is decided by LINK ORDER between
// translation units, exactly like g_category/g_name below. Every
// smoke test written so far (leak_plant_smoke.cpp, smoke_main.cpp)
// happened to list alloc_counter_hook.cpp BEFORE the file under test
// on the g++ command line, so init_hook() always ran first there by
// accident. The real Containerfile lists every fixture.cpp BEFORE
// alloc_counter_classify.cpp/alloc_counter_hook.cpp (leak-counter.md
// sec. 5 S2's own "os dois caminhos acrescentados... antes do
// $(pkg-config --libs...)") - under THAT order, a fixture's own
// namespace-scope std::string (gpu_kind_report_smoke.cpp's g_name, 17
// bytes, past libstdc++'s ~15-byte SSO) got ITS destructor registered
// BEFORE print_report(), so LIFO ran print_report() first and the
// still-live string read back as a false `ours` leak - reproduced in
// isolation (7 translation units, no Wayland involved) before this
// fix, confirmed clean after it. The real GNU attribute below is what
// this file's own comment always claimed - it forces `init_hook` into
// `.init_array.101`, a section the linker always places BEFORE the
// unprioritized `.init_array` every ordinary global initializer (this
// file's own g_ranges included) lands in, regardless of link order.
// Both attributes in ONE __attribute__((...)) list, GNU syntax only
// (never mixed with the [[gnu::section(...)]] GFX_HOOK_FN macro on
// this one function): a separate [[...]] attribute-specifier is not
// grammatically allowed between a GNU __attribute__((constructor))
// and the declaration it applies to - measured, this is exactly what
// broke first when the two were written as two separate attributes.
__attribute__((constructor(101), section("gfx_alloc_hook"))) void init_hook() noexcept {
    compute_ranges();
    if (std::atexit(&print_report) != 0) {
        // cert-err33-c: the return value is checked, never discarded.
        // There is no gltfx_rslt/exception channel this early (this
        // runs before main(), GFX_HOOK_FN L-22 "no exception crosses
        // the public API" does not even apply yet) - write(2) directly
        // is the only honest way to surface a failure this file's own
        // report would otherwise never get the chance to explain (if
        // atexit() itself failed, print_report() never runs at all).
        constexpr char msg[] = "alloc_counter_hook: std::atexit(print_report) failed\n";
        write_all(STDERR_FILENO, msg, sizeof(msg) - 1);
    }
}

GFX_HOOK_FN glintfx_leak_counter::alloc_classify_result classify_current_stack() noexcept {
    if (g_hook_depth != 0) {
        // Reentered from inside backtrace()'s OWN allocation (F13) -
        // capturing a second backtrace here would recurse without
        // bound. No frame evidence is available for THIS allocation,
        // so it is third_party by the same "never ours without
        // positive evidence" rule S1 already tests (empty-stack case).
        return {.klass = glintfx_leak_counter::alloc_frame_class::third_party,
                .decisive_frame = nullptr};
    }
    ++g_hook_depth;
    void *frames[k_max_backtrace_frames];
    const int frame_count = ::backtrace(frames, static_cast<int>(k_max_backtrace_frames));
    --g_hook_depth;
    const std::size_t count = frame_count > 0 ? static_cast<std::size_t>(frame_count) : 0;
    return glintfx_leak_counter::classify_allocation_frames(
        const_cast<const void *const *>(static_cast<void *const *>(frames)), count, g_ranges);
}

// hook_new_bookkeep - o que os dois lados (throwing e nothrow) fazem
// DEPOIS que o malloc do bloco ja teve sucesso: so classifica a pilha
// (classify_current_stack, noexcept, L.413) e mexe nos contadores/
// tabela (fetch_add e table_insert, ambos noexcept, L.169-181) - nada
// aqui pode lancar, e por isso a funcao e noexcept de verdade, nao so
// de comentario (CONTAINER-LEAK-COUNTER: cppcheck throwInNoexceptFunction
// apontou exatamente a falta dessa garantia no tipo).
GFX_HOOK_FN void *hook_new_bookkeep(void *raw) noexcept {
    const glintfx_leak_counter::alloc_classify_result result = classify_current_stack();

    auto *header = static_cast<alloc_header *>(raw);
    header->magic = k_alloc_magic;
    header->klass = result.klass == glintfx_leak_counter::alloc_frame_class::ours
                        ? alloc_owner_klass::ours
                        : alloc_owner_klass::third_party;
    header->decisive_frame = result.decisive_frame;

    void *user_ptr = header + 1;
    if (header->klass == alloc_owner_klass::ours) {
        g_live_ours.fetch_add(1, std::memory_order_relaxed);
        table_insert(user_ptr);
    } else {
        g_live_third_party.fetch_add(1, std::memory_order_relaxed);
    }
    return user_ptr;
}

// hook_new_impl - forma THROWING (::operator new escalar): malloc
// falhando e bad_alloc de verdade, [new.delete.single] exige. NAO
// noexcept, e por isso nunca pode ser chamada do lado nothrow (era
// exatamente esse o defeito: o lado nothrow chamava esta mesma funcao
// por um bool de runtime, e o tipo dela nunca provava nada).
GFX_HOOK_FN void *hook_new_impl(std::size_t size) {
    g_new_calls.fetch_add(1, std::memory_order_relaxed);
    const std::size_t total = sizeof(alloc_header) + size;
    void *raw = std::malloc(total);
    if (raw == nullptr) {
        throw std::bad_alloc();
    }
    return hook_new_bookkeep(raw);
}

// hook_new_impl_nothrow - forma NOTHROW (::operator new(size,
// std::nothrow) escalar): malloc falhando devolve nullptr, nunca
// lanca. noexcept aqui e verificavel por leitura - malloc nao lanca,
// hook_new_bookkeep acima e noexcept - nao supressao, garantia real.
GFX_HOOK_FN void *hook_new_impl_nothrow(std::size_t size) noexcept {
    g_new_calls.fetch_add(1, std::memory_order_relaxed);
    const std::size_t total = sizeof(alloc_header) + size;
    void *raw = std::malloc(total);
    if (raw == nullptr) {
        return nullptr;
    }
    return hook_new_bookkeep(raw);
}

GFX_HOOK_FN void hook_delete_impl(void *ptr) noexcept {
    if (ptr == nullptr) {
        return; // operator delete(nullptr) is a mandated no-op
                // ([expr.delete]) - not counted as a call, nothing to
                // classify or free.
    }
    g_delete_calls.fetch_add(1, std::memory_order_relaxed);

    auto *header =
        reinterpret_cast<alloc_header *>(static_cast<char *>(ptr) - sizeof(alloc_header));
    if (header->magic != k_alloc_magic) {
        // Did not come from THIS file's operator new (leak-counter.md
        // sec. 4: "e UB do programa; o contador denuncia em vez de
        // corromper") - free the pointer exactly as it arrived rather
        // than trusting a header that was never written by us.
        g_foreign_delete.fetch_add(1, std::memory_order_relaxed);
        std::free(ptr);
        return;
    }

    if (header->klass == alloc_owner_klass::ours) {
        g_live_ours.fetch_sub(1, std::memory_order_relaxed);
        table_remove(ptr);
    } else {
        g_live_third_party.fetch_sub(1, std::memory_order_relaxed);
    }
    std::free(header);
}

} // namespace

// --- the replaceable global allocation functions themselves ([basic.
// stc.dynamic.allocation]) - GFX_HOOK_FN on every one of them, see
// this file's own top comment for why the array forms need it too
// (they are a real call frame between the real caller and hook_new_
// impl's own backtrace() capture, and that frame otherwise reads as
// `ours` by being inside the executable). --------------------------

GFX_HOOK_FN void *operator new(std::size_t size) { return hook_new_impl(size); }

GFX_HOOK_FN void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    return hook_new_impl_nothrow(size);
}

GFX_HOOK_FN void *operator new[](std::size_t size) { return ::operator new(size); }

GFX_HOOK_FN void *operator new[](std::size_t size, const std::nothrow_t &tag) noexcept {
    return ::operator new(size, tag);
}

GFX_HOOK_FN void operator delete(void *ptr) noexcept { hook_delete_impl(ptr); }

GFX_HOOK_FN void operator delete(void *ptr, std::size_t /*size*/) noexcept {
    ::operator delete(ptr);
}

GFX_HOOK_FN void operator delete[](void *ptr) noexcept { ::operator delete(ptr); }

GFX_HOOK_FN void operator delete[](void *ptr, std::size_t /*size*/) noexcept {
    ::operator delete(ptr);
}
