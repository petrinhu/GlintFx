// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_reserve_auto_homonym.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (D-B4-3, achado FN-1 da revisao do CTO).
// VEREDITO ESPERADO: ACUSADA (familia B). O nome `buf` e declarado com um tipo
// do projeto (noexcept) em outro lugar, mas AQUI e declarado com `auto` sobre
// um std::vector obtido por referencia: o tipo e desconhecido, entao o
// `reserve` continua acusado.
#include <cstddef>
#include <vector>

namespace {

struct room_list {
    bool reserve(std::size_t count) noexcept;
};

struct holder {
    room_list buf;
};

std::vector<int> &shared_vector() noexcept;

bool fill(std::size_t count) noexcept {
    auto &buf = shared_vector();
    buf.reserve(count);
    return true;
}

} // namespace
