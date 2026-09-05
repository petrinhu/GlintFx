# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/platform/window/CMakeLists.txt
#
# Single subject: the OS-AGNOSTIC common window layer (W-C', docs/
# plano-w6a-janela.md fatia 4) - window_state (the ONE value type both
# backends write into, no #if anywhere in it) and the two D-W6a-23
# validation atoms (window_desc_validation, utf8_validation). No
# operating-system header reachable from any of these three .cpp files -
# same reasoning src/platform/port/'s own CMakeLists.txt already gives
# for being added UNCONDITIONALLY, before the UNIX/WIN32 branch in
# src/platform/CMakeLists.txt: there is nothing platform-specific here
# to gate.
#
# Added to glintfx_library by WL-WINDOW fatia W-E (docs/plano-w6a-
# janela.md fatia 8): window_adapter.cpp (src/platform/wayland/) is the
# FIRST production caller of any of these three - until this fatia, all
# three only ever got compiled a second time, directly, by their own
# unit tests (window_state_test, window_desc_validation_test,
# utf8_validation_test in tests/CMakeLists.txt) - so linking the real
# library needs the symbols to exist inside it too, exactly as
# global_catalog.cpp already does one directory over in wayland/
# CMakeLists.txt.
#
# W-D' (docs/plano-w6a-janela.md fatia 6) adds display_facade.cpp: the
# ONE translation unit that defines glintfx::display_impl and every
# out-of-line member of the public glintfx::gltfx_display (include/
# glintfx/platform/window/display.hpp) - see that .cpp file's own
# header comment for why it lives here (OS-agnostic directory, per-
# platform selection done at the preprocessor level inside the file
# itself) rather than under wayland/ or win32/.

target_sources(glintfx_library PRIVATE
    window_state.cpp
    window_desc_validation.cpp
    utf8_validation.cpp
    display_facade.cpp
)
