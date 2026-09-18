// SPDX-License-Identifier: AGPL-3.0-or-later
//
// good_nothrow_new.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ABSOLVIDA.
// `new (std::nothrow) T` NAO lanca (devolve nullptr sob falta de
// memoria) - a mesma forma ja usada em src/core/err.cpp,
// src/platform/gl/display_facade.cpp e src/platform/window/
// window_facade.cpp.
namespace {

struct widget {
    int value = 0;
};

widget *make_widget() noexcept { return new (std::nothrow) widget{}; }

} // namespace
