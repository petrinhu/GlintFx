// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/error.hpp>

#include <cstdint>
#include <new>
#include <string>
#include <string_view>

// error.cpp - CE-2/CE-3 of CORE-ERROR (TODO.md, GODS_LAWS.md L-19):
// defines err_context and every gltfx_err member that needs its
// complete layout.

namespace glintfx {

// Real fields, added in CE-3 (GODS_LAWS.md L-26: additive, does not
// change gltfx_err's frozen footprint or its CE-2 lifecycle code -
// only what m_context points AT grows). Default member initializers,
// not a user-declared constructor: `new err_context()` at every
// ensure_context() call site below stays valid aggregate-style
// zero-init instead of needing a matching constructor signature.
struct err_context {
    std::string path;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::uint64_t byte_offset = 0;
    std::string rejected_value;
    std::int64_t os_error_code = 0;
};

// Deep copy, conditional: a context-less gltfx_err copies to another
// context-less gltfx_err with NO allocation at all - the property
// tests/error_no_alloc_test.cpp (CE-2) counts, not just reads.
gltfx_err::gltfx_err(const gltfx_err &other) : m_code(other.m_code) {
    if (other.m_context != nullptr) {
        m_context = new err_context(*other.m_context);
    }
}

gltfx_err::~gltfx_err() { delete m_context; }

bool gltfx_err::ensure_context() noexcept {
    if (m_context == nullptr) {
        m_context = new (std::nothrow) err_context();
    }
    return m_context != nullptr;
}

// Every accessor reads m_context ONLY when it exists; a null context
// (never attached to, or an attach that never got past
// ensure_context()) reads back empty/zero for every field - the same
// convention err_context's own default member initializers already
// establish for a context that DOES exist but has that one field
// unset (GODS_LAWS.md L-22: never undefined behavior for an expected,
// absent value).

std::string_view gltfx_err::path() const noexcept {
    return m_context != nullptr ? std::string_view{m_context->path} : std::string_view{};
}

std::uint32_t gltfx_err::line() const noexcept {
    return m_context != nullptr ? m_context->line : 0;
}

std::uint32_t gltfx_err::column() const noexcept {
    return m_context != nullptr ? m_context->column : 0;
}

std::uint64_t gltfx_err::byte_offset() const noexcept {
    return m_context != nullptr ? m_context->byte_offset : 0;
}

std::string_view gltfx_err::rejected_value() const noexcept {
    return m_context != nullptr ? std::string_view{m_context->rejected_value} : std::string_view{};
}

std::int64_t gltfx_err::os_error_code() const noexcept {
    return m_context != nullptr ? m_context->os_error_code : 0;
}

// Every with_*() below follows the same shape: ensure_context() first
// (best-effort; degrades to code-only if it fails), then a try/catch
// around the one operation that can still throw despite that -
// std::string::assign() growing past err_context's small-string-
// optimization buffer, which uses the THROWING global operator new,
// not the nothrow one ensure_context() already guarded. Letting that
// exception escape a noexcept function would call std::terminate() -
// exactly the process-abort the leader's OOM decision forbids
// (GODS_LAWS.md, TODO.md CORE-ERROR row: "a lib NUNCA aborta o
// processo do consumidor") - so it is caught here, not propagated.

gltfx_err &gltfx_err::with_path(std::string_view path) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    try {
        m_context->path.assign(path);
    } catch (const std::bad_alloc &) { // NOLINT(bugprone-empty-catch) reason: intentionally empty,
                                       // best-effort attach
        // `path` keeps whatever value it held before this call (unset,
        // or a prior successful attach) - the error itself is
        // unaffected, see error.hpp's "ATTACH IS BEST-EFFORT" comment.
        // Swallowing on purpose: propagating would violate `noexcept`
        // and call std::terminate(), the exact process-abort this
        // design exists to forbid.
    }
    return *this;
}

gltfx_err &gltfx_err::with_position(std::uint32_t line, std::uint32_t column) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    // Plain scalar writes: cannot throw, no try/catch needed.
    m_context->line = line;
    m_context->column = column;
    return *this;
}

gltfx_err &gltfx_err::with_byte_offset(std::uint64_t offset) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    m_context->byte_offset = offset;
    return *this;
}

gltfx_err &gltfx_err::with_rejected_value(std::string_view value) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    try {
        m_context->rejected_value.assign(value);
    } catch (const std::bad_alloc &) { // NOLINT(bugprone-empty-catch) reason: see with_path() above
    }
    return *this;
}

gltfx_err &gltfx_err::with_os_error_code(std::int64_t code) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    m_context->os_error_code = code;
    return *this;
}

} // namespace glintfx
