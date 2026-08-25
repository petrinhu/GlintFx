# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxInstall.cmake
#
# Single subject: install() of the target, of the public/generated
# headers, and of the CMake package (glintfxConfig.cmake) that an
# external find_package(glintfx) consumes.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(glintfx_install_library_target target)
    # No `INCLUDES DESTINATION` clause here (FIX-CONSUMO-2, achado QA-1):
    # it used to duplicate, verbatim, what GlintfxLibrary.cmake's
    # target_include_directories($<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    # already adds to the exported target's INTERFACE_INCLUDE_DIRECTORIES.
    # The duplicate was harmless by construction (same value twice) but
    # it also silently compensated for a wrong/hardcoded value there, so
    # a regression of the real source of truth went uncaught (proven by
    # mutation in check_install_includedir.sh). Single source of truth
    # now: the target_include_directories() call in GlintfxLibrary.cmake.
    install(TARGETS ${target}
        EXPORT glintfxTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endfunction()

function(glintfx_install_public_headers)
    # PROJECT_SOURCE_DIR, not CMAKE_SOURCE_DIR (FIX-CONSUMO achado A7):
    # see the comment at the same substitution in the root CMakeLists.txt.
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/glintfx"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(FILES
        "${GLINTFX_GENERATED_INCLUDE_DIR}/glintfx/export.hpp"
        "${GLINTFX_GENERATED_INCLUDE_DIR}/glintfx/version_macros.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/glintfx"
    )
endfunction()

function(glintfx_write_package_version_file)
    # COMPATIBILITY stays SameMinorVersion for now, on purpose (VER-4C
    # does NOT touch this policy - GODS_LAWS.md L-26: "Antes da 1.0:
    # SameMinorVersion é o correto, porque B é onde a quebra mora
    # enquanto A é zero. Ao chegar na 1.0, isso muda para
    # SameMajorVersion e a mudança não pode ser esquecida."). This
    # comment IS that reminder: when PROJECT_VERSION_MAJOR leaves 0,
    # change the line below to SameMajorVersion, not before.
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/glintfxConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
    )
endfunction()

function(glintfx_configure_package_config_file)
    # PROJECT_SOURCE_DIR, not CMAKE_SOURCE_DIR (FIX-CONSUMO achado A7):
    # see the comment at the same substitution in the root CMakeLists.txt.
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/glintfx-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/glintfxConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/glintfx"
    )
endfunction()

function(glintfx_install_cmake_package)
    install(EXPORT glintfxTargets
        NAMESPACE glintfx::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/glintfx"
    )

    glintfx_write_package_version_file()
    glintfx_configure_package_config_file()

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/glintfxConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/glintfxConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/glintfx"
    )
endfunction()

# Computes the pkg-config `${pcfiledir}`-relative prefix expression for
# glintfx.pc (PKG-DIST; GODS_LAWS.md LEI ZERO: relocatable, because
# this library ships to a consumer whose install prefix we never know
# - a distro build root, a relocatable package, an unpacked archive on
# someone else's machine). CMAKE_INSTALL_LIBDIR/pkgconfig is the .pc
# file's own install directory; the number of `/..` segments needed to
# climb back to the prefix depends on how many path segments that
# directory has (lib/pkgconfig: 2; a Debian-multiarch
# lib/x86_64-linux-gnu/pkgconfig: 3). Computed here from
# CMAKE_INSTALL_LIBDIR's actual value, never hand-typed in the
# template, so a non-default layout is never silently wrong.
function(glintfx_compute_pkgconfig_relocatable_prefix out_var)
    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        # GNUInstallDirs allows CMAKE_INSTALL_LIBDIR to be an absolute
        # path (a packager staging into a fixed system location outside
        # CMAKE_INSTALL_PREFIX). ${pcfiledir}-relative relocation has no
        # defined answer there, so this falls back to the absolute,
        # configured prefix instead of guessing - correct for that one
        # build, just not relocatable afterwards.
        set(${out_var} "${CMAKE_INSTALL_PREFIX}" PARENT_SCOPE)
        return()
    endif()

    set(pkgconfig_subdir "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    string(REPLACE "/" ";" path_segments "${pkgconfig_subdir}")
    list(LENGTH path_segments depth)

    # Backslash-escaped: this has to reach glintfx.pc as the LITERAL
    # text "${pcfiledir}/.." repeated, for pkg-config's own parser to
    # expand at resolve time - not something configure_file()'s @ONLY
    # substitution (glintfx_install_pkgconfig() below) should try to
    # interpret as a CMake variable while building this string.
    set(relative_prefix "\${pcfiledir}")
    foreach(_unused RANGE 1 ${depth})
        string(APPEND relative_prefix "/..")
    endforeach()
    set(${out_var} "${relative_prefix}" PARENT_SCOPE)
endfunction()

# Libs.private carries what a STATIC glintfx needs beyond -lglintfx
# itself. On Linux, glintfx_library links wayland-client PRIVATE
# (cmake/GlintfxWaylandProtocols.cmake); CMake's own static-propagation
# of that PRIVATE link dependency only reaches a
# find_package(glintfx)-based consumer's INTERFACE_LINK_LIBRARIES -
# pkg-config has no CMake target graph to read, so a static consumer
# resolved purely via `pkg-config --libs --static glintfx` needs the
# linker token spelled out here, or static linking fails with undefined
# references to wl_* symbols.
#
# Bare -l token, not `Requires.private: wayland-client` (which would
# pull in wayland-client's own .pc transitively): mirrors the exact
# choice GlintfxWaylandProtocols.cmake already made on the CMake side
# (plain _LIBRARIES variable, no IMPORTED_TARGET, same comment there)
# for the same reason - this is the one place that decision gets made,
# and it must not diverge between the CMake and pkg-config packaging
# paths.
#
# Empty (and valid pkg-config syntax) on any platform other than Linux:
# glintfx has no platform/ layer outside UNIX yet
# (src/platform/CMakeLists.txt is only added under if(UNIX) in
# src/CMakeLists.txt), so there is nothing else to add today - a
# Windows backend, when it is born, extends this function's UNIX-only
# branch pattern, it does not get a second Libs.private-computing
# function.
function(glintfx_compute_pkgconfig_libs_private out_var)
    set(libs_private "")
    if(UNIX)
        foreach(lib_name IN LISTS GlintfxWaylandClient_LIBRARIES)
            string(APPEND libs_private "-l${lib_name} ")
        endforeach()
        string(STRIP "${libs_private}" libs_private)
    endif()
    set(${out_var} "${libs_private}" PARENT_SCOPE)
endfunction()

function(glintfx_install_pkgconfig)
    glintfx_compute_pkgconfig_relocatable_prefix(GLINTFX_PC_RELOCATABLE_PREFIX)
    glintfx_compute_pkgconfig_libs_private(GLINTFX_PC_LIBS_PRIVATE)

    # PROJECT_SOURCE_DIR, not CMAKE_SOURCE_DIR (FIX-CONSUMO achado A7):
    # see the comment at the same substitution in the root CMakeLists.txt.
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/glintfx.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/glintfx.pc"
        @ONLY
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/glintfx.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )
endfunction()
