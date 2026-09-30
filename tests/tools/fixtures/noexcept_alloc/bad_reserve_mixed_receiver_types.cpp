// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_reserve_mixed_receiver_types.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (D-B4-3, errata sec. 20: o portao resolve o receptor de um
// metodo que aloca PELO TIPO DECLARADO). VEREDITO ESPERADO: ACUSADA (familia B). O nome `pieces` e
// declarado com um tipo do projeto (noexcept) E com um std::vector: continua acusado.
#include <cstddef>
#include <vector>

namespace {

struct room_list {
    bool reserve(std::size_t count) noexcept;
};

bool from_project(room_list &pieces, std::size_t count) noexcept { return pieces.reserve(count); }

std::size_t stl_size(const std::vector<int> &pieces) noexcept { return pieces.size(); }

} // namespace
