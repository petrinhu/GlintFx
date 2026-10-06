// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_project_reserve_noexcept.cpp - fixture de sabotagem de tests/tools/check_noexcept_alloc.py
// (D-B4-3, errata sec. 20: o portao resolve o receptor de um metodo que aloca PELO TIPO DECLARADO).
// VEREDITO ESPERADO: ABSOLVIDA. `reserve` de um tipo do projeto declarado noexcept (cresce por
// realloc, nunca lanca) nao e o `std::vector::reserve`.
#include <cstddef>

namespace {

struct room_list {
    bool reserve(std::size_t count) noexcept;
};

bool make_room(room_list &list, std::size_t count) noexcept { return list.reserve(count); }

} // namespace
