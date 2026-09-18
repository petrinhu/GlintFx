// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_string_view_substr.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ABSOLVIDA.
// Classe de falso positivo ja medida e consertada na calibracao
// (RELATORIO.md secao 5): `substr()` sobre `std::string_view` NAO
// aloca (devolve outra view sobre o mesmo buffer) - so `substr()`
// sobre `std::string` aloca. Sem esta fixture, uma regressao que
// volte a tratar os dois iguais nao seria notada.
#include <string_view>

namespace {

std::string_view trim_first(std::string_view text) noexcept { return text.substr(1); }

} // namespace
