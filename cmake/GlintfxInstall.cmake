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
        # build, just not relocatable afterwards. Normalized for the
        # same reason as everything else in this file: CMAKE_INSTALL_PREFIX
        # is a user/packager-supplied value too, and nothing here should
        # trust it unnormalized just because it happens to be a
        # different variable than CMAKE_INSTALL_LIBDIR.
        set(normalized_prefix "${CMAKE_INSTALL_PREFIX}")
        cmake_path(NORMAL_PATH normalized_prefix)
        set(${out_var} "${normalized_prefix}" PARENT_SCOPE)
        return()
    endif()

    # Normalize BEFORE counting segments (adversarial review, PKG-DIST
    # achado 1, reproduced live: -DCMAKE_INSTALL_LIBDIR=lib64/ - a
    # trailing slash is plausible packager input). Unnormalized,
    # "lib64/" + "/pkgconfig" is the literal string "lib64//pkgconfig";
    # string(REPLACE "/" ";" ...) on that string produces FOUR
    # semicolon-separated tokens, one of them empty ("lib64", "",
    # "pkgconfig"... the trailing/doubled separator manufactures a
    # phantom directory level that was never on disk), so
    # list(LENGTH) over-counts by one and the emitted .pc walks ONE
    # DIRECTORY TOO FAR UP - pkg-config reports success either way
    # (--exists does not check that the resolved libdir/includedir
    # actually contain anything), so this used to fail with NO error
    # and NO warning, exactly the silent-wrong-path failure mode LEI
    # ZERO exists to rule out. cmake_path(NORMAL_PATH) collapses
    # repeated/trailing separators (and "." segments) before the count
    # ever runs, so the depth always matches the REAL directory nesting
    # CMake's own install() step produces on disk, regardless of how
    # the caller spelled CMAKE_INSTALL_LIBDIR. Proven by
    # malformed_libdir_test in tests/tools/check_pkgconfig.sh.
    set(pkgconfig_subdir "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    cmake_path(NORMAL_PATH pkgconfig_subdir)
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

# Computes the right-hand-side VALUE for one pkg-config path variable
# assignment (`libdir=`/`includedir=`), given the CORRESPONDING CMake
# install-dir variable's raw value (CMAKE_INSTALL_LIBDIR /
# CMAKE_INSTALL_INCLUDEDIR). GNUInstallDirs officially allows either
# variable to be RELATIVE (the common case, joined under the prefix at
# pkg-config resolve time) OR ABSOLUTE (a packager staging into a fixed
# system location, independent of any prefix - a real, documented use,
# not a corner case). Applied identically to libdir and includedir
# (adversarial review, PKG-DIST achado 3: "trate os tres - prefix,
# libdir, includedir - pela mesma regra, nao um a um" -
# glintfx_compute_pkgconfig_relocatable_prefix() above already had this
# exact branch for `prefix=` itself; this function is the same rule,
# generalized, for the two call sites that were missing it).
#
# RELATIVE raw_value: normalized, then expressed as the LITERAL text
# "${<base_pkgconfig_var>}/<normalized>" - backslash-escaped so
# CMake's OWN `${}` variable expansion (which runs the instant this
# string is built by set(), independent of configure_file()'s later
# @ONLY pass) does not try to resolve base_pkgconfig_var as a CMake
# variable and silently collapse it to empty. pkg-config resolves the
# real value itself at query time - this is what stays genuinely
# relocatable.
#
# ABSOLUTE raw_value (reproduced live before this fix, adversarial
# review achado 3: -DCMAKE_INSTALL_LIBDIR=/var/tmp/gvabs/inst/lib64
# produced "libdir=${exec_prefix}//var/tmp/gvabs/inst/lib64" - the
# prefix appears TWICE, `pkg-config --libs` emitted a path that does
# not exist on disk, and `pkg-config --exists` still reported success,
# because it never checks that the resolved directory contains
# anything): normalized and used AS-IS, with NO "${base}/" joined in
# front at all. Concatenating any base in front of an already-absolute
# path duplicates it; the correct pkg-config idiom for an absolute
# install dir is the bare path, with no variable reference.
function(glintfx_compute_pkgconfig_path_expression raw_value base_pkgconfig_var out_var)
    set(normalized "${raw_value}")
    cmake_path(NORMAL_PATH normalized)

    if(IS_ABSOLUTE "${normalized}")
        set(${out_var} "${normalized}" PARENT_SCOPE)
        return()
    endif()

    set(${out_var} "\${${base_pkgconfig_var}}/${normalized}" PARENT_SCOPE)
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
    glintfx_compute_pkgconfig_path_expression("${CMAKE_INSTALL_LIBDIR}" "exec_prefix" GLINTFX_PC_LIBDIR)
    glintfx_compute_pkgconfig_path_expression("${CMAKE_INSTALL_INCLUDEDIR}" "prefix" GLINTFX_PC_INCLUDEDIR)

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
