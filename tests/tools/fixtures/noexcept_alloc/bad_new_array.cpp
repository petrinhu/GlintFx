// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_new_array.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ACUSADA
// (familia B). `new T[n]` CRU (sem `std::nothrow`) dentro de funcao
// noexcept - lanca std::bad_alloc sob falta de memoria, sem rede.
namespace {

int *make_buffer(int n) noexcept { return new int[n]; }

} // namespace
