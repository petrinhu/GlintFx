// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_project_reserve_not_noexcept.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (D-B4-3, errata sec. 20: o portao resolve o receptor de um
// metodo que aloca PELO TIPO DECLARADO). VEREDITO ESPERADO: ACUSADA (familia B). Um tipo do projeto
// cujo `reserve` NAO e noexcept pode lancar: continua acusado.
#include <cstddef>

namespace {

struct room_list {
    bool reserve(std::size_t count);
};

bool make_room(room_list &list, std::size_t count) noexcept { return list.reserve(count); }

} // namespace
