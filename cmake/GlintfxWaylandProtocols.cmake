# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxWaylandProtocols.cmake
#
# Single subject: turns the SYSTEM xdg-shell.xml (GODS_LAWS.md L-07:
# never vendored, located on the machine that builds glintfx via
# pkg-config) into a C client binding via wayland-scanner, and wires
# it PRIVATE into a glintfx target. Nothing in this file decides what
# uses the binding at runtime (that is src/platform/wayland/, born in a
# later fatia) - this module only knows how to GENERATE and ATTACH it.
#
# Included only under if(UNIX) (root CMakeLists.txt, GODS_LAWS.md L-05:
# Wayland is a Linux-only surface); the two find calls below would
# simply fail on a platform with no libwayland-client/wayland-scanner,
# which is correct there, not a bug to guard against here.

# No IMPORTED_TARGET (measured live, static-mode consume_test):
# target_link_libraries(glintfx_library PRIVATE PkgConfig::x) leaks the
# imported target's NAME into glintfx's exported INTERFACE_LINK_LIBRARIES
# the moment BUILD_SHARED_LIBS=OFF - CMake propagates a STATIC library's
# PRIVATE link dependencies to consumers by design (a .a has no linker
# step of its own to resolve them at build time, so the consumer's own
# link line has to carry them) - and a consumer that never called
# pkg_check_modules(... IMPORTED_TARGET wayland-client) itself has no
# target named PkgConfig::GlintfxWaylandClient to satisfy that
# reference: `cmake --build` on the installed package failed outright
# with "The link interface of target glintfx::glintfx contains
# PkgConfig::GlintfxWaylandClient but the target was not found." Using
# the plain _LIBRARIES/_INCLUDE_DIRS/_LIBRARY_DIRS variables below
# instead means the propagated entry is the bare linker token
# "wayland-client" (-lwayland-client), which any consumer's linker
# resolves via ordinary system search paths - no second pkg-config call
# required on their side, shared or static.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GlintfxWaylandClient REQUIRED wayland-client)

find_program(GLINTFX_WAYLAND_SCANNER_EXE wayland-scanner REQUIRED)

# Single directory the generated .h/.c pair for every protocol lands
# in. PRIVATE by construction (GODS_LAWS.md L-19: the public surface
# does not move) - nothing under this directory is ever added to
# glintfx's own include dirs or installed; only the specific targets
# that call glintfx_add_wayland_xdg_shell_binding() below, or a test
# that explicitly asks for this exact path, ever see it.
set(GLINTFX_GENERATED_WAYLAND_DIR "${PROJECT_BINARY_DIR}/generated/wayland")

# Resolves the xdg-shell.xml this build's OWN machine ships inside its
# wayland-protocols package. A function, not inline code at include()
# time, so a second protocol added in a later fatia can reuse the same
# resolution pattern instead of duplicating the pkg-config call.
function(glintfx_locate_xdg_shell_protocol_xml out_var)
    execute_process(
        COMMAND "${PKG_CONFIG_EXECUTABLE}" --variable=pkgdatadir wayland-protocols
        OUTPUT_VARIABLE wayland_protocols_pkgdatadir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE pkg_config_result
    )
    if(NOT pkg_config_result EQUAL 0)
        message(FATAL_ERROR
            "GlintfxWaylandProtocols: pkg-config could not resolve wayland-protocols' pkgdatadir")
    endif()

    set(xml_path "${wayland_protocols_pkgdatadir}/stable/xdg-shell/xdg-shell.xml")
    if(NOT EXISTS "${xml_path}")
        message(FATAL_ERROR
            "GlintfxWaylandProtocols: xdg-shell.xml not found at ${xml_path} "
            "(wayland-protocols installed, but this stable protocol is missing)")
    endif()

    set("${out_var}" "${xml_path}" PARENT_SCOPE)
endfunction()

# Runs wayland-scanner twice over the located XML: client-header (the
# public-shaped C declarations, incl. `extern const struct wl_interface
# xdg_wm_base_interface;`) and private-code (the C definitions, meant
# to be compiled once and kept internal - GODS_LAWS.md L-07's own
# scanner strongly recommends private-code over public-code for
# exactly that reason). Idempotent: safe to call more than once, the
# custom commands/target are only declared the first time.
function(glintfx_generate_xdg_shell_binding)
    if(TARGET glintfx_wayland_xdg_shell_binding)
        return()
    endif()

    glintfx_locate_xdg_shell_protocol_xml(xdg_shell_xml)
    file(MAKE_DIRECTORY "${GLINTFX_GENERATED_WAYLAND_DIR}")

    set(generated_header "${GLINTFX_GENERATED_WAYLAND_DIR}/xdg-shell-client-protocol.h")
    set(generated_source "${GLINTFX_GENERATED_WAYLAND_DIR}/xdg-shell-protocol.c")

    add_custom_command(
        OUTPUT "${generated_header}"
        COMMAND "${GLINTFX_WAYLAND_SCANNER_EXE}" client-header "${xdg_shell_xml}" "${generated_header}"
        DEPENDS "${xdg_shell_xml}"
        COMMENT "wayland-scanner: xdg-shell client-header"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${generated_source}"
        COMMAND "${GLINTFX_WAYLAND_SCANNER_EXE}" private-code "${xdg_shell_xml}" "${generated_source}"
        DEPENDS "${xdg_shell_xml}"
        COMMENT "wayland-scanner: xdg-shell private-code"
        VERBATIM
    )

    # LANGUAGE C: the generated .c is fed to the C compiler, never the
    # C++ one (GODS_LAWS.md L-03 is C++23 for glintfx's own code; this
    # file is 100% generated, not glintfx's code - GODS_LAWS.md L-20
    # excludes generated code from TDD for exactly this reason).
    set_source_files_properties("${generated_source}" PROPERTIES
        LANGUAGE C
        GENERATED ON
    )
    set_source_files_properties("${generated_header}" PROPERTIES GENERATED ON)

    # Named custom target (not just the two add_custom_command()
    # OUTPUTs) so any target that needs the binding to exist before it
    # compiles - the library that carries the .c source below, AND a
    # test TU that only #includes the header without owning the source
    # itself - can add_dependencies() on one name, instead of one
    # target depending on files it never lists as its own sources.
    add_custom_target(glintfx_wayland_xdg_shell_binding
        DEPENDS "${generated_header}" "${generated_source}"
    )

    set(GLINTFX_XDG_SHELL_GENERATED_SOURCE "${generated_source}" CACHE INTERNAL
        "Path of the generated xdg-shell private-code .c file")
endfunction()

# Attaches the xdg-shell binding to `target` as PRIVATE: the generated
# .c becomes one of target's own sources (so it links straight into
# the library's object set, no separate static archive to manage), the
# generated header's directory is a PRIVATE include dir (GODS_LAWS.md
# L-19: never PUBLIC, the public surface does not move), and
# wayland-client (wayland-client.h, pulled in by the generated
# client-header, plus the real libwayland-client symbols a later
# fatia's connection code will call) is linked PRIVATE via the plain
# pkg-config variables, not an imported target - see the comment above
# pkg_check_modules() for why.
function(glintfx_add_wayland_xdg_shell_binding target)
    glintfx_generate_xdg_shell_binding()

    add_dependencies(${target} glintfx_wayland_xdg_shell_binding)
    target_sources(${target} PRIVATE "${GLINTFX_XDG_SHELL_GENERATED_SOURCE}")
    target_include_directories(${target} PRIVATE
        "${GLINTFX_GENERATED_WAYLAND_DIR}"
        ${GlintfxWaylandClient_INCLUDE_DIRS}
    )
    target_link_directories(${target} PRIVATE ${GlintfxWaylandClient_LIBRARY_DIRS})
    target_link_libraries(${target} PRIVATE ${GlintfxWaylandClient_LIBRARIES})
    target_compile_options(${target} PRIVATE ${GlintfxWaylandClient_CFLAGS_OTHER})
endfunction()
