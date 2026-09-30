// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_reserve_overload_not_noexcept.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py (D-B4-3, achado FN-2 da revisao do CTO).
// VEREDITO ESPERADO: ACUSADA (familia B). Uma sobrecarga de `reserve` e
// noexcept e a outra nao: basta UMA sem noexcept para o nome nao ser absolvido,
// porque a chamada pode resolver para ela.
#include <cstddef>

namespace {

struct room_list {
    bool reserve(std::size_t count) noexcept;
    void reserve(std::size_t count, int extra);
};

bool make_room(room_list &list) noexcept {
    list.reserve(1, 2);
    return true;
}

} // namespace
