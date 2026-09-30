// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_std_vector_reserve.cpp - fixture de sabotagem de tests/tools/check_noexcept_alloc.py
// (D-B4-3, errata sec. 20: o portao resolve o receptor de um metodo que aloca PELO TIPO DECLARADO).
// VEREDITO ESPERADO: ACUSADA (familia B). `std::vector::reserve` dentro de noexcept lanca sob falta
// de memoria.
#include <cstddef>
#include <vector>

namespace {

void make_room(std::vector<int> &values, std::size_t count) noexcept { values.reserve(count); }

} // namespace
