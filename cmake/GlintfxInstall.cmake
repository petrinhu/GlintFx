# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxInstall.cmake
#
# Single subject: install() of the target, of the public/generated
# headers, and of the CMake package (glintfxConfig.cmake) that an
# external find_package(glintfx) consumes.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Refuses an install-dir CMake variable that is empty or
# whitespace-only (adversarial review round 3, PKG-DIST). GNUInstallDirs'
# own defaulting only fires when the cache variable is UNDEFINED, not
# when it is explicitly set to "" or " " - confirmed live on this
# machine: `-DCMAKE_INSTALL_LIBDIR=` slips through the ENTIRE configure
# step unnoticed (CMakeCache.txt shows CMAKE_INSTALL_LIBDIR:UNINITIALIZED=,
# genuinely empty, not defaulted) and only fails LATER, deep inside
# cmake_install.cmake, with "file cannot create directory: /cmake/glintfx" -
# because "${CMAKE_INSTALL_LIBDIR}/cmake/glintfx" collapses to the
# ABSOLUTE path "/cmake/glintfx", the filesystem ROOT, the instant the
# variable is empty (the SAME concatenation-without-validation category
# as achados 1 and 3, one input shape earlier: those needed careful
# computation because a MALFORMED-but-nonblank value still names a real
# layout somewhere; blank never does - it is always a mistake in how
# the value was passed, never anyone's actual layout, so this refuses
# it outright instead of computing anything at all). Also observed to
# behave INCONSISTENTLY across destinations before failing (some
# artifacts DO land under a plausible-looking "lib" subdirectory,
# masking the danger, right up until the moment the concatenated,
# root-collapsing destination fails outright) - refusing early removes
# that inconsistency instead of hoping every call site happens to fail
# safely.
function(glintfx_require_nonblank_install_subdir value var_name)
    string(STRIP "${value}" stripped)
    if(stripped STREQUAL "")
        message(FATAL_ERROR
            "glintfx: ${var_name} is empty or blank ('${value}'). This is "
            "never a legitimate install layout - CMake concatenates it "
            "directly into install destinations, and an empty value "
            "collapses some of them to the filesystem ROOT (for example, "
            "\"\${${var_name}}/cmake/glintfx\" becomes the absolute path "
            "\"/cmake/glintfx\"). Either leave ${var_name} unset "
            "(GNUInstallDirs picks a sane per-platform default) or set it "
            "to a real relative or absolute directory."
        )
    endif()
endfunction()

glintfx_require_nonblank_install_subdir("${CMAKE_INSTALL_LIBDIR}" "CMAKE_INSTALL_LIBDIR")
glintfx_require_nonblank_install_subdir("${CMAKE_INSTALL_INCLUDEDIR}" "CMAKE_INSTALL_INCLUDEDIR")

# Refuses a MIXED absolute/relative combination of CMAKE_INSTALL_LIBDIR
# and CMAKE_INSTALL_INCLUDEDIR (PKG-LIBDIR-MIX achado, colateral de
# 27/08/2026, pre-existente). glintfx_compute_pkgconfig_relocatable_prefix()
# further down this file decides whether glintfx.pc's own `prefix=` line
# is a baked-in absolute path or a `${pcfiledir}`-relocatable one using
# CMAKE_INSTALL_LIBDIR's absoluteness ALONE - it has no visibility into
# CMAKE_INSTALL_INCLUDEDIR at all. When the two disagree in kind (one
# absolute, the other relative), an ORDINARY install-time `--prefix`
# override (or a reconfigure under a different CMAKE_INSTALL_PREFIX,
# the same effect) moves whichever half is RELATIVE, but never the
# baked-in `prefix=` - the generated glintfx.pc keeps naming a real,
# existing, entirely WRONG directory for that half.
#
# REPRODUCED LIVE (GODS_LAWS.md L-44, not declared without measuring):
# CMAKE_INSTALL_LIBDIR set absolute, CMAKE_INSTALL_INCLUDEDIR left
# relative, configured with CMAKE_INSTALL_PREFIX=/usr/local, installed
# with `cmake --install <build> --prefix /different-prefix`: the real
# headers land under /different-prefix/include, but the installed
# glintfx.pc's `includedir=` still reads `${prefix}/include` with
# `prefix=/usr/local` baked in at configure time - a descriptor
# pointing a consumer at headers that are not there.
# GlintfxPkgConfigValidateInstalled.cmake's own install-time content
# check DOES catch that specific reproduction (it failed loudly,
# naming the wrong resolved path), but only because that wrong path
# happened to have no glintfx/ header tree on THIS machine - it checks
# "does something plausible exist at the resolved path", never "is
# this genuinely THIS install's own location". A stale glintfx/ tree
# left over at the wrong path by an earlier, unrelated install would
# make that same check pass. Refusing the mixed combination outright,
# here, at configure time, closes that gap unconditionally, independent
# of whatever else happens to already be on disk.
#
# Deliberately narrow, mirroring glintfx_compute_pkgconfig_path_expression()'s
# own per-variable absolute/relative branch below: two absolute values,
# or two relative values (the common case, GNUInstallDirs' own
# per-platform default), are both left alone untouched - see "Supported
# CMAKE_INSTALL_LIBDIR / CMAKE_INSTALL_INCLUDEDIR layouts" in
# PACKAGING.md. Normalized before the IS_ABSOLUTE check (same reason as
# every other absoluteness check in this file: a trailing slash or a
# leading "./" must not change the verdict).
function(glintfx_require_consistent_libdir_includedir_kind libdir_value includedir_value)
    set(normalized_libdir "${libdir_value}")
    cmake_path(NORMAL_PATH normalized_libdir)
    set(normalized_includedir "${includedir_value}")
    cmake_path(NORMAL_PATH normalized_includedir)

    if(IS_ABSOLUTE "${normalized_libdir}" AND NOT IS_ABSOLUTE "${normalized_includedir}")
        message(FATAL_ERROR
            "glintfx: CMAKE_INSTALL_LIBDIR ('${libdir_value}') is an ABSOLUTE "
            "path, but CMAKE_INSTALL_INCLUDEDIR ('${includedir_value}') is "
            "RELATIVE. Mixing an absolute and a relative install directory "
            "produces a glintfx.pc whose 'includedir=' can end up naming the "
            "wrong directory after an install-time --prefix override or a "
            "reconfigure under a different CMAKE_INSTALL_PREFIX, because only "
            "the relative half moves with the prefix. Either set both "
            "CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR to absolute "
            "paths, or leave both relative to the same install prefix."
        )
    endif()

    if(IS_ABSOLUTE "${normalized_includedir}" AND NOT IS_ABSOLUTE "${normalized_libdir}")
        message(FATAL_ERROR
            "glintfx: CMAKE_INSTALL_INCLUDEDIR ('${includedir_value}') is an "
            "ABSOLUTE path, but CMAKE_INSTALL_LIBDIR ('${libdir_value}') is "
            "RELATIVE. Mixing an absolute and a relative install directory "
            "produces a glintfx.pc whose 'libdir=' can end up naming the "
            "wrong directory after an install-time --prefix override or a "
            "reconfigure under a different CMAKE_INSTALL_PREFIX, because only "
            "the relative half moves with the prefix. Either set both "
            "CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR to absolute "
            "paths, or leave both relative to the same install prefix."
        )
    endif()
endfunction()

glintfx_require_consistent_libdir_includedir_kind("${CMAKE_INSTALL_LIBDIR}" "${CMAKE_INSTALL_INCLUDEDIR}")

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
