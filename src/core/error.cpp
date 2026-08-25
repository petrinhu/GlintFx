// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/error.hpp>

// error.cpp - CE-2 of CORE-ERROR (TODO.md, GODS_LAWS.md L-19): defines
// err_context (empty for now - CE-3 fills in the real fields, purely
// additively) so gltfx_err's copy constructor and destructor, declared
// in error.hpp, have a COMPLETE type to new/delete against.

namespace glintfx {

// Empty in CE-2 on purpose: no attach API exists yet to ever make
// m_context non-null, so this only needs to be a complete, deletable
// type. CE-3 adds the real fields here; nothing in gltfx_err's
// lifecycle code below changes shape when that happens (GODS_LAWS.md
// L-26: additive, bumps B).
struct err_context {};

// Deep copy, conditional: a context-less gltfx_err copies to another
// context-less gltfx_err with NO allocation at all - the property
// tests/error_no_alloc_test.cpp (CE-2) counts, not just reads. CE-3
// adds real fields to err_context; this shape (copy only when there IS
// something to copy) does not change.
gltfx_err::gltfx_err(const gltfx_err &other) : m_code(other.m_code) {
    if (other.m_context != nullptr) {
        m_context = new err_context(*other.m_context);
    }
}

gltfx_err::~gltfx_err() { delete m_context; }

} // namespace glintfx
