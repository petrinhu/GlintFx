// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"

// win32_display_native_handle_test.cpp - PARITY-GATE (05/09/2026 do
// servidor real, run 34043723220): tests/display_native_display_test.cpp
// (o irmao Wayland) prova que wayland_display_adapter::native_display()
// - o wl_display* que sai da fronteira desta classe para um chamador
// como o probe EGL de GL-CONTEXT fatia 1 - reporta o ponteiro nulo
// antes de open(). Aquele proprio arquivo ja nomeia o par certo do
// lado Windows no seu comentario de cabecalho: "the same pointer
// window_adapter.hpp's own surface() and win32's own native_handle()
// already expose for the analogous... need" - ou seja,
// win32_display_adapter::native_handle() (display_adapter.hpp,
// win32/) e' o acessor ANALOGO neste lado, ja existente no codigo
// (documentado la como "Test seam only") mas NUNCA verificado, em
// nenhum teste, no estado ANTES de open() - so depois (tests/
// win32_display_refusal_test.cpp e tests/two_displays_test.cpp so
// leem native_handle() DEPOIS de open() ter sido chamado, para
// comparar duas janelas ja abertas). Este arquivo fecha exatamente
// essa lacuna: a MESMA pergunta (handle nulo antes de open()) que o
// lado Wayland ja responde.
//
// NAO E O MESMO ESCOPO que WIN-GL/GL-CONTEXT (TODO.md): aquele item
// cobre o CONTEXTO OpenGL via WGL sobre a janela Windows, ainda
// pendente e sem bloqueio nenhum sobre este teste - native_handle()
// ja existe, publico, hoje, independente de WGL existir. Este teste
// nao prova nada sobre WGL nem sobre a garantia grafica que WIN-GL vai
// fechar; prova so o mesmo fato de fronteira que o lado Wayland ja
// prova para native_display(): o acessor comeca nulo.
//
// win32_display_adapter e' compilado uma SEGUNDA vez direto no
// conjunto de objetos deste executavel (tests/CMakeLists.txt), mesma
// tecnica que win32_display_connect_test.cpp/win32_display_refusal_
// test.cpp ja documentam (visibilidade oculta do glintfx.dll, GODS_
// LAWS.md L-19 - "nada e exportado").

GLINTFX_TEST(win32_display_adapter_native_handle_is_null_before_open) {
    const glintfx::platform::win32_display_adapter adapter;
    GLINTFX_CHECK(!adapter.is_open());
    GLINTFX_CHECK(adapter.native_handle() == nullptr);
}

#endif // defined(_WIN32)
