// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <new>
#include <print>
#include <string>
#include <string_view>
#include <unistd.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/drm_device_facts.hpp"

// drm_device_facts_oom_test.cpp - NOEXCEPT-ALLOC-B8 fatia F2
// (/var/tmp/glintfx-plan/plano-conserto-noexcept.md sec. "F2",
// GODS_LAWS.md L-04/L-09/L-20/L-22): proves the REAL SITE (drm_device_
// facts.cpp:180, `const std::string node(node_path)` inside a
// `noexcept` function reachable, on Linux, from the PUBLIC
// gltfx_gpu_enumeration::query and gltfx_gl_context::open) - not just
// an atom, because F1's own atom (platform/nul_terminated_name.hpp)
// already has exhaustive boundary coverage in tests/proc_name_buffer_
// test.cpp, and this fatia REUSES that atom rather than inventing a
// second copy of the same boundary logic (F1's own commit message:
// parity becomes the compiler's job, not a reader's).
//
// NO REAL DRM NODE OR GPU NEEDED, MEASURED NOT ASSUMED (plan's own F2
// section): the allocation this fatia removes happens BEFORE open(),
// and open() on a nonexistent path fails right after anyway -
// read_drm_device_facts("/does-not-exist") exercises the exact site
// under test without any hardware dependency.
//
// SECOND COMPILE, SAME TECHNIQUE AS gl_proc_address_oom_test.cpp (F1):
// drm_device_facts.cpp is compiled a SECOND time, directly into this
// test's own executable, so this file's own global operator new/
// delete override reaches every allocation read_drm_device_facts()
// performs, with no DSO boundary to cross - the identical reasoning
// tests/gfui_complex_match_resource_exhausted_test.cpp's own "NO
// DLL-CROSSING GAP TO CLOSE HERE" paragraph already gives.
//
// THE BOUNDARY CASE, AND WHY IT NEEDS A REAL FILE ON DISK (plan's own
// F2 mutation m2, "make the overflow truncate instead of refuse: the
// second case has to fail, otherwise the test cannot tell a refusal
// apart from a silent truncation"): a path that is simply "too long and
// nonexistent" cannot distinguish "refused before open()" from "silently
// truncated to PATH_MAX-1 bytes, then opened, and STILL failed" - both
// produce facts.opened == false either way. The only black-box way to
// tell the two apart is to make the TRUNCATED PREFIX itself a real,
// existing, openable file: build_path_of_exact_length() below creates
// one whose absolute path is exactly PATH_MAX - 1 bytes (the largest
// legal length the atom accepts - copy_nul_terminated()'s own contract,
// platform/nul_terminated_name.hpp). A path one byte longer than that,
// whose first PATH_MAX - 1 bytes are IDENTICAL to that real file's own
// path, is refused correctly (facts.opened == false, zero allocations)
// - but under the m2 mutation (truncate-then-open instead of refuse),
// the truncated string is EXACTLY the real file's own path, so
// facts.opened would come back true. That divergence is what
// drm_device_facts_refuses_a_path_one_byte_past_path_max_boundary below
// actually asserts.

namespace {

bool g_force_alloc_failure = false;
std::size_t g_calls_to_allow_before_failure = 0;
std::size_t g_override_new_call_count = 0;

[[nodiscard]] bool should_fail_this_allocation() noexcept {
    if (!g_force_alloc_failure) {
        return false;
    }
    if (g_calls_to_allow_before_failure > 0) {
        --g_calls_to_allow_before_failure;
        return false;
    }
    return true;
}

// Same MSVC/GCC-ASan-only declare-and-skip valve gl_proc_address_
// oom_test.cpp's own oom_forcing_declared_not_applicable() uses (this
// file's own header comment: this leg is Linux-only, but the Linux
// `sanitizer` CI job DOES build with ASan - GODS_LAWS.md L-44, honest
// declaration over silent assumption if the override ever loses the
// race).
[[nodiscard]] bool oom_forcing_declared_not_applicable() {
#if defined(__SANITIZE_ADDRESS__)
    return std::getenv("GLINTFX_OOM_TEST_FORCE_NOT_APPLICABLE") != nullptr ||
           true; // ASan's own operator new/delete wins by default over any
                 // user override linked into the same binary (learn.
                 // microsoft.com/cpp/sanitizers/asan-known-issues, the
                 // same citation this suite's other OOM-forcing tests
                 // already give in full) - declared unconditionally
                 // true under any ASan build rather than assumed safe
                 // without measuring it here.
#else
    return std::getenv("GLINTFX_OOM_TEST_FORCE_NOT_APPLICABLE") != nullptr;
#endif
}

void declare_oom_forcing_not_applicable(std::string_view case_name) {
    std::println(stderr,
                 "drm_device_facts_oom_test: {} declared NOT APPLICABLE under AddressSanitizer "
                 "(this TU's own operator new/delete override never gets a chance to run under "
                 "ASan, so the assertion this case exists to prove would measure nothing)",
                 case_name);
}

// RAII scratch tree under TMPDIR (this project's own /var/tmp
// convention, CLAUDE.md) - built ONLY by
// build_path_of_exact_length()'s own callers, torn down on scope exit
// regardless of how the case ends, the same idiom tests/display_
// connect_failure_test.cpp's own private_empty_runtime_dir already
// uses.
class scoped_scratch_tree {
  public:
    scoped_scratch_tree() {
        const std::filesystem::path base = std::filesystem::temp_directory_path();
        m_root = base / "glintfx-f2-oom-XXXXXX";
        std::string template_str = m_root.string();
        const char *created = mkdtemp(template_str.data());
        if (created == nullptr) {
            std::println(stderr, "drm_device_facts_oom_test: mkdtemp failed for template {}",
                         template_str);
            std::abort();
        }
        m_root = created;
    }

    scoped_scratch_tree(const scoped_scratch_tree &) = delete;
    scoped_scratch_tree &operator=(const scoped_scratch_tree &) = delete;

    ~scoped_scratch_tree() {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
    }

    [[nodiscard]] const std::filesystem::path &root() const noexcept { return m_root; }

  private:
    std::filesystem::path m_root;
};

// Builds a REAL, existing, empty regular file whose absolute path is
// exactly `target_len` bytes long, by nesting directories (each
// component well under the ext4/btrfs NAME_MAX of 255 bytes) under
// `tree.root()`. Aborts (via GLINTFX_CHECK inside the caller) rather
// than silently returning a wrong length - a boundary test with the
// wrong boundary would prove nothing.
[[nodiscard]] std::string build_path_of_exact_length(const scoped_scratch_tree &tree,
                                                     std::size_t target_len) {
    constexpr std::size_t k_component_len = 200; // safely under NAME_MAX (255)
    std::string path = tree.root().string();
    while (target_len > path.size() && target_len - path.size() > k_component_len + 2) {
        path += '/';
        path += std::string(k_component_len, 'd');
        std::filesystem::create_directory(path);
    }
    // Room for exactly one more '/' plus a final filename component -
    // sized so the TOTAL length lands on target_len precisely, never
    // approximately.
    const std::size_t remaining = target_len - path.size() - 1;
    path += '/';
    path += std::string(remaining, 'f');

    const int fd = open(path.c_str(), O_CREAT | O_WRONLY, 0600);
    if (fd < 0) {
        std::println(stderr,
                     "drm_device_facts_oom_test: could not create fixture file of length {} "
                     "(path length {}, errno-reported open() failure)",
                     target_len, path.size());
        std::abort();
    }
    close(fd);
    return path;
}

} // namespace

void *operator new(std::size_t size) {
    ++g_override_new_call_count;
    if (should_fail_this_allocation()) {
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new(std::size_t size, const std::nothrow_t & /*tag*/) noexcept {
    ++g_override_new_call_count;
    if (should_fail_this_allocation()) {
        return nullptr;
    }
    return std::malloc(size);
}

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

void operator delete(void *p, const std::nothrow_t & /*tag*/) noexcept { std::free(p); }

// THE CASE (plan's own F2, first "teste que falha ANTES"): against the
// pre-fix site (`const std::string node(node_path);` inside a
// `noexcept` function), an allocation failure forced on the VERY NEXT
// call escapes as std::bad_alloc out of a noexcept function, which
// [except.terminate] mandates calls std::terminate() - this whole test
// BINARY dies before GLINTFX_CHECK ever runs. After the fix, the call
// returns normally (facts.opened == false, since the path does not
// exist) instead.
//
// MUTATION m1 OF THE PLAN'S OWN F2 SECTION LIVES HERE: reverting the
// site to `const std::string node(node_path);` makes this exact case
// std::terminate() again.
GLINTFX_TEST(read_drm_device_facts_survives_an_armed_allocator) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable("read_drm_device_facts_survives_an_armed_allocator");
        return;
    }

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0; // the very next allocation, if any, fails
    const drm_device_facts facts =
        read_drm_device_facts("/does-not-exist/glintfx-f2-oom-marker-renderD999");
    g_force_alloc_failure = false;

    // Reaching this line at all already proves the process did not
    // std::terminate() - before this fatia's own fix, it never did.
    // The path does not exist, so open() fails regardless of the
    // allocation site's own fate; the observable contract this fatia
    // owes is that "does not exist" degrades to opened == false, the
    // SAME desfecho the function already has for "did not open".
    GLINTFX_CHECK(!facts.opened);
}

// THE POSITIVE FORM OF STEP 1 (plan sec. 2, "do not allocate"; the same
// "no_alloc" shape gl_proc_address_oom_test.cpp's own positive case
// uses for F1): with a HEALTHY allocator, resolving a nonexistent,
// ordinary-length path allocates exactly zero times at the site this
// fatia fixes - the alloc happens (or, post-fix, does not happen)
// strictly BEFORE open() is ever attempted.
GLINTFX_TEST(read_drm_device_facts_allocates_nothing_for_an_ordinary_missing_path) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "read_drm_device_facts_allocates_nothing_for_an_ordinary_missing_path");
        return;
    }

    const std::size_t calls_before = g_override_new_call_count;
    const drm_device_facts facts =
        read_drm_device_facts("/does-not-exist/glintfx-f2-oom-marker-renderD998");
    GLINTFX_CHECK_EQ(g_override_new_call_count, calls_before);
    GLINTFX_CHECK(!facts.opened);
}

// THE BOUNDARY, PROVED A STEP BEYOND ITSELF (GODS_LAWS.md L-43; plan's
// own F2 section, "second case: a path longer than PATH_MAX returns
// opened=false without allocating"): first proves the LEGAL boundary
// (PATH_MAX - 1 bytes, the largest a path may legitimately be under
// this atom's own contract) is ACCEPTED and genuinely opens a real
// file - never just asserted in the abstract - then proves one byte
// past it is REFUSED, with zero allocations, and (the case's own
// reason for existing, see this file's own header comment) refused
// WITHOUT ever touching the filesystem: the truncated-to-PATH_MAX-1
// prefix of the overflow path is BYTE-IDENTICAL to the real, openable
// file the first half of this case just proved opens - so a
// truncate-instead-of-refuse mutation (m2 of the plan's own F2
// section) would make this SAME file resolve again and flip
// facts.opened to true.
GLINTFX_TEST(read_drm_device_facts_refuses_a_path_one_byte_past_path_max_boundary) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "read_drm_device_facts_refuses_a_path_one_byte_past_path_max_boundary");
        return;
    }

    const scoped_scratch_tree tree;
    const std::string boundary_path = build_path_of_exact_length(tree, PATH_MAX - 1);
    GLINTFX_CHECK_EQ(boundary_path.size(), static_cast<std::size_t>(PATH_MAX - 1));

    // The legal boundary itself: PATH_MAX - 1 bytes fits (copy_nul_
    // terminated()'s own "<", not "<=" - one byte reserved for the
    // NUL terminator), and this is a REAL file, so it genuinely opens.
    const drm_device_facts boundary_facts = read_drm_device_facts(boundary_path);
    GLINTFX_CHECK(boundary_facts.opened);

    // One byte past it: refused, never truncated-then-opened.
    const std::string overflow_path = boundary_path + "X";
    GLINTFX_CHECK_EQ(overflow_path.size(), static_cast<std::size_t>(PATH_MAX));

    const std::size_t calls_before = g_override_new_call_count;
    const drm_device_facts overflow_facts = read_drm_device_facts(overflow_path);
    GLINTFX_CHECK(!overflow_facts.opened);
    GLINTFX_CHECK_EQ(g_override_new_call_count, calls_before);
}
