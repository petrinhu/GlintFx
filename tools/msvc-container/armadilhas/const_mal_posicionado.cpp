// Reproducao minima do achado real do commit babbd77 (GlintFx, X-WGL,
// 06/09/2026): 'const HINSTANCE module' - HINSTANCE ja e um ponteiro
// (typedef de 'struct HINSTANCE__ *'), entao 'const HINSTANCE' aplica o
// qualificador ao PONTEIRO, nunca ao apontado (vira 'HINSTANCE__ *const',
// nao 'const HINSTANCE__ *') - clang-tidy misc-misplaced-const, achado no
// runner real do Windows.
#if !defined(_WIN32)
#error "arquivo especifico da armadilha Win32 - so compila visando Windows"
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

HINSTANCE get_module() {
    const HINSTANCE module = ::GetModuleHandleW(nullptr);
    return module;
}
