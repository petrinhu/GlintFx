# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/container/CMakeLists.txt
#
# Single subject: puts every tests/container/*.cpp Wayland/EGL fixture
# INTO this build's compile_commands.json, and nothing else.
#
# LINT-CONTAINER-SMOKES (TODO.md, GODS_LAWS.md L-09/L-17/L-40): these
# fixtures are the ones tests/container/Containerfile compiles with raw
# g++ invocations and the `wayland-container` CI job then RUNS inside a
# container against a real, isolated Wayland compositor (GODS_LAWS.md
# L-09 - never on the host, never in this developer's live session).
# Because they never entered THIS project's own CMake build, they never
# entered compile_commands.json, and run-clang-tidy (tools/preci.sh's
# own stage_tidy) only ever analyzes what is in that bank - a fixture
# outside it is silently never analyzed, success and all. Measured
# 19/09/2026: 20 .cpp tracked, 1 in the bank.
#
# GODS_LAWS.md L-09, the constraint that shapes every line below: an
# OBJECT library is COMPILED ONLY - CMake never links it into anything,
# never produces an executable from it, and `cmake --build` never
# invokes it as a subprocess (configuring/compiling is explicitly
# outside the "toca a superficie/executa" rule; RUNNING one of these
# fixtures is not, and nothing here ever does that). This file's only
# job is to make these translation units COMPILE on the host, so static
# analysis sees them; whether they LINK or RUN is answered exclusively
# by tests/container/Containerfile, inside the container, which this
# file does not touch.
if(UNIX)
    # alloc_counter_classify.cpp is DELIBERATELY EXCLUDED from the glob
    # below - it already compiles, links AND RUNS on the host today, as
    # part of alloc_counter_classify_test (see tests/CMakeLists.txt's
    # own comment above that glintfx_add_test() call): it is a pure
    # classifier that touches no OS surface, so GODS_LAWS.md L-09 never
    # applied to it and it was wired into the real suite instead of
    # left as a container-only fixture. It already sits in
    # compile_commands.json through that target. Adding it a second
    # time here would compile the same TU twice for no measurement this
    # gate needs - alloc_counter_classify_test alone is the reason
    # check_container_smokes_in_compile_db.py already finds it covered.
    file(GLOB GLINTFX_CONTAINER_SMOKE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )
    list(REMOVE_ITEM GLINTFX_CONTAINER_SMOKE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/alloc_counter_classify.cpp"
    )

    # WL-ACK-SMOKE-BLUNT A0/A3 (docs/plano-w7c-adendo-revalidacao.md
    # SS3.A, decisao D-A2): wire_relay_main.cpp is the ONE file under
    # tests/container/wire_relay/ this project's real build (tests/
    # CMakeLists.txt) never compiles - it binds/listens/accepts/
    # connects real Unix sockets (GODS_LAWS.md L-09: only ever run
    # INSIDE the isolated container run_compositor.sh builds, never on
    # this host), so unlike the five atoms (wire_decoder.cpp etc.,
    # already covered by tests/CMakeLists.txt's real, RUNNING
    # wire_relay_selftest - A0 confirmed that coverage sufficient,
    # "encontrados=28 no_banco=28 faltando=0", nothing added there for
    # them) it has no other path into compile_commands.json. Listed
    # EXPLICITLY, not glob-widened into wire_relay/ (the ${CMAKE_
    # CURRENT_SOURCE_DIR}/*.cpp glob above is already non-recursive and
    # would not reach it anyway, but GLOB_RECURSE was rejected on
    # purpose here too: tests/container/_arch_ports_src/ - staged by
    # prepare_arch_ports_fixture.sh, gitignored, existing on disk
    # during a real build - would enter this OBJECT library as a
    # duplicate compile of files already compiled via THAT staging
    # path, exactly the trap check_container_smokes_in_compile_db_test
    # exists to catch by finding two compile_commands.json entries for
    # the same logical source).
    list(APPEND GLINTFX_CONTAINER_SMOKE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/wire_relay/wire_relay_main.cpp"
    )

    add_library(glintfx_container_smokes OBJECT ${GLINTFX_CONTAINER_SMOKE_SOURCES})

    # include/ for the public <glintfx/...> surface these fixtures
    # exercise, src/ for the private "platform/..." adapters they reach
    # into directly (the exact same two-directory shape several in-tree
    # tests already use - e.g. tests/CMakeLists.txt's own store_loop_
    # callbacks_test/loop_open_alloc_failure_test).
    #
    # GLINTFX_GENERATED_INCLUDE_DIR (root CMakeLists.txt) is the third
    # directory every fixture that reaches <glintfx/core/err_code.hpp>
    # needs: glintfx/export.hpp and glintfx/version_macros.hpp are
    # generate_export_header()/configure_file() output, never committed
    # under include/ (cmake/GlintfxLibrary.cmake's own glintfx_generate_
    # export_header()). Both are written at CONFIGURE time (generate_
    # export_header() and configure_file() are configure-time calls, not
    # build-time custom commands - confirmed live: they exist under
    # PROJECT_BINARY_DIR/generated/include/glintfx/ right after `cmake
    # -S -B`, before any `cmake --build`), so no add_dependencies() on
    # glintfx_library is needed here - that would pull this OBJECT
    # library's whole purpose (compile-only, GODS_LAWS.md L-09) into
    # forcing the real library to build too, for no reason this file has.
    target_include_directories(glintfx_container_smokes PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
        "${GLINTFX_GENERATED_INCLUDE_DIR}"
    )

    # Same two attachment functions src/platform/wayland/CMakeLists.txt
    # itself uses on glintfx_library (cmake/GlintfxWaylandProtocols.cmake,
    # cmake/GlintfxEgl.cmake): generates/exposes the xdg-shell binding
    # (wait_events_smoke.cpp, window_smoke.cpp, loop_hidden_test.cpp,
    # egl_probe_smoke.cpp, egl_protocol_error_smoke.cpp all #include
    # <xdg-shell-client-protocol.h>) and the system EGL/wayland-egl
    # headers (egl_device_reader_probe.cpp, egl_probe_smoke.cpp,
    # egl_protocol_error_smoke.cpp). Applying both to the WHOLE OBJECT
    # library, not per-source, is harmless for the fixtures that need
    # neither - an unused PRIVATE include dir/compile option compiles
    # away with no effect - and keeps this file from having to track
    # which of the 19 needs which, a distinction that changes every time
    # a fixture is added.
    glintfx_add_wayland_xdg_shell_binding(glintfx_container_smokes)
    glintfx_add_egl(glintfx_container_smokes)

    # Same standard/warning/sanitizer flags every other glintfx target
    # gets (cmake/GlintfxCompileOptions.cmake) - these fixtures are
    # meant to compile as cleanly on the host as they already do under
    # tests/container/Containerfile's own `g++ -std=c++23 ... -Wall
    # -Wextra -Werror` invocations.
    glintfx_apply_compile_options(glintfx_container_smokes)
endif()
