# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxLibrary.cmake
#
# Single subject: target properties, export header and include
# directories of the glintfx library. Install is GlintfxInstall.cmake;
# language standard and warnings are GlintfxCompileOptions.cmake.

include(GenerateExportHeader)

function(glintfx_set_target_properties target)
    set_target_properties(${target} PROPERTIES
        VERSION ${PROJECT_VERSION}
        # SOVERSION follows PROJECT_VERSION_MAJOR, not a fixed literal
        # (GODS_LAWS.md L-26: "SOVERSION acompanha o A. É o único número
        # que o consumidor precisa ler para saber que o binário dele
        # quebrou"). Today PROJECT_VERSION_MAJOR is 0, so this still
        # renders libglintfx.so.0 - identical on-disk name to before
        # this change - but the value now tracks the version instead of
        # silently staying 0 forever after the first ABI-breaking bump.
        #
        # UNTESTABLE TODAY, BY CONSTRUCTION (adversarial review of
        # VER-4C, 24/08/2026, mutant 4): reverting this line to the old
        # hardcoded literal `SOVERSION 0` survives every test in this
        # repo right now, because PROJECT_VERSION_MAJOR IS 0 today - the
        # two forms produce the byte-identical on-disk SONAME
        # (libglintfx.so.0), so no observation from outside CMake can
        # tell them apart. This is not a gap left by oversight: nothing
        # can distinguish "follows the variable" from "hardcoded to the
        # variable's current value" while the variable never changes.
        # The line only becomes observable, and therefore only becomes
        # testable, the first time PROJECT_VERSION_MAJOR leaves 0 - at
        # that point a stale `SOVERSION 0` would keep emitting
        # libglintfx.so.0 forever while PROJECT_VERSION climbs, and a
        # test asserting the SONAME tracks the major version would
        # catch exactly that regression. Until then, this comment is
        # the only guard there is.
        SOVERSION ${PROJECT_VERSION_MAJOR}
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        # FIX-CONSUMO-2, achado QA-3: the CMake target itself is named
        # glintfx_library (see src/CMakeLists.txt), not the bare
        # glintfx, to avoid colliding with a consumer's own target of
        # that name when embedded. Two properties keep every OTHER name
        # a consumer or a packager can observe exactly as before:
        #   - OUTPUT_NAME: the compiled artifact stays libglintfx.so.0 /
        #     libglintfx.a, not libglintfx_library.*. Without it, a
        #     consumer linking with a bare `-lglintfx` (pkg-config,
        #     manual Makefile, distro packaging) would silently break -
        #     a far bigger regression than the CMake target namespace
        #     issue this rename set out to fix.
        #   - EXPORT_NAME: the INSTALLED package's imported target stays
        #     glintfx::glintfx, not glintfx::glintfx_library. Without
        #     it, install(EXPORT ... NAMESPACE glintfx::) would export
        #     this target under the wrong name, breaking the public
        #     contract (GODS_LAWS.md L-19/L-26).
        OUTPUT_NAME glintfx
        EXPORT_NAME glintfx
        # CRT-LINK-WIN (tests/tools/check_rslt_precondition.py's own
        # header comment, coordinator report 04/09/2026): this project
        # never sets CMAKE_MSVC_RUNTIME_LIBRARY, so CMake's own default
        # applies to glintfx_library - but an UNSET property reads back
        # empty through $<TARGET_PROPERTY:...>, which is exactly what
        # made rslt_precondition_test's own precondition fail on the
        # Windows static-mode estreia ("msvc-runtime-library is empty
        # on MSVC"). The value below is CMake's own documented default
        # for this property when CMAKE_MSVC_RUNTIME_LIBRARY is unset
        # (cmake-properties(7) MSVC_RUNTIME_LIBRARY) - writing it out
        # explicitly changes NOTHING about which /MD or /MT flag MSVC
        # actually receives (it is the literal CMake was already
        # resolving internally), it only makes that already-effective
        # choice readable by the generator expression the test script
        # depends on. A no-op everywhere else: non-MSVC compilers ignore
        # this property entirely.
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endfunction()

# ABI-STDLIB-LEAK: closes what CXX_VISIBILITY_PRESET hidden (above)
# cannot, on its own, close. That property sets the COMPILER's default
# per translation unit; it has no power over a symbol a SYSTEM HEADER
# marks with an explicit __attribute__((visibility("default"))) - and
# libstdc++ does exactly that on class templates such as
# std::__cxx11::basic_string (bits/c++config.h's _GLIBCXX_VISIBILITY
# macro). When one of glintfx's own translation units instantiates
# such a template and the optimizer leaves an out-of-line definition
# behind (weaker inlining, e.g. -O0/-Og), GCC emits that leftover as a
# DEFAULT-visibility symbol in glintfx's .so regardless of the flag -
# measured live, 28/08/2026 (GATE-DEBUG's first real Debug build):
# std::basic_string<char>::_M_replace_cold and
# std::__detail::__from_chars_alnum_to_val_table<false>::value both
# leaked; the SAME source built -DCMAKE_BUILD_TYPE=Release happened to
# inline every call site and leaked neither - an accident of
# optimization level, not a guarantee that survives a compiler bump.
#
# A linker version script (cmake/glintfx.version, read its own header
# comment for the anonymous-node and unquoted-glob findings) acts at
# the FINAL link, after every translation unit's per-TU visibility
# decision is already made, and can force ANY symbol outside the
# glintfx:: allowlist to `local` (out of the .so's dynamic symbol
# table) - including one a header forced to default visibility. This
# is the same technique Abseil, Protobuf and Boost use for exactly
# this problem class.
#
# UNIX/BUILD_SHARED_LIBS guard, matching tests/CMakeLists.txt's own
# visibility_test guard exactly: a version script is a GNU-linker-
# family construct (bfd/gold/lld/mold, none of them Apple's ld - this
# project ships no macOS target, GODS_LAWS.md, five platforms). It has
# nothing to link against for a static archive, and Windows already
# controls its export set through generate_export_header's
# __declspec(dllexport/dllimport) instead.
#
# LINK_DEPENDS registers the map file as a link-step input: CMake/
# Ninja re-links (not merely re-configures) glintfx_library the moment
# cmake/glintfx.version changes, the same way a header change forces a
# recompile - a stale link this file was edited but the .so was not
# re-linked would silently keep serving the OLD export set.
function(glintfx_apply_export_map target)
    if(NOT (BUILD_SHARED_LIBS AND UNIX))
        return()
    endif()
    set(map_file "${PROJECT_SOURCE_DIR}/cmake/glintfx.version")
    target_link_options(${target} PRIVATE "LINKER:--version-script=${map_file}")
    set_target_properties(${target} PROPERTIES LINK_DEPENDS "${map_file}")
endfunction()

function(glintfx_generate_export_header target)
    generate_export_header(${target}
        EXPORT_MACRO_NAME GLINTFX_API
        EXPORT_FILE_NAME "${GLINTFX_GENERATED_INCLUDE_DIR}/glintfx/export.hpp"
    )
endfunction()

function(glintfx_configure_version_header)
    # PROJECT_SOURCE_DIR, not CMAKE_SOURCE_DIR (FIX-CONSUMO achado A7):
    # see the comment at the same substitution in the root CMakeLists.txt.
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/version_macros.hpp.in"
        "${GLINTFX_GENERATED_INCLUDE_DIR}/glintfx/version_macros.hpp"
        @ONLY
    )
endfunction()

function(glintfx_set_target_include_dirs target)
    # FIX-CONSUMO achado A6: was `$<INSTALL_INTERFACE:include>`, a literal
    # that silently drifted from CMAKE_INSTALL_INCLUDEDIR. This is now the
    # ONLY place that adds an install-time include dir to the exported
    # target (FIX-CONSUMO-2, achado QA-1: the redundant
    # `INCLUDES DESTINATION` clause that used to duplicate this value in
    # GlintfxInstall.cmake was removed, because it masked a regression of
    # this very line - proven by mutation in check_install_includedir.sh).
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${GLINTFX_GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endfunction()

# GLINTFX_STATIC_DEFINE must be PUBLIC (not PRIVATE): when glintfx is
# installed static, the export set carries this definition to the
# consumer via find_package; otherwise GLINTFX_API expands to
# __declspec(dllimport) in a binary that was never linked as a DLL.
function(glintfx_apply_static_define target)
    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PUBLIC GLINTFX_STATIC_DEFINE)
    endif()
endfunction()

# EMBED-DLL: makes glintfx's own DLL discoverable, by default, to an
# embedding consumer's Windows executable - see the option's own
# comment in GlintfxOptions.cmake for the failure this exists to close.
#
# RUNTIME_OUTPUT_DIRECTORY, not LIBRARY_OUTPUT_DIRECTORY (confirmed
# against CMake's own docs, cmake-buildsystem(7) "Output Artifacts",
# not assumed): on DLL platforms the .dll itself is a RUNTIME artifact
# (its import library, .lib, is the ARCHIVE artifact); on non-DLL
# platforms the shared object is a LIBRARY artifact instead. Setting
# RUNTIME_OUTPUT_DIRECTORY is therefore the property that actually
# moves a Windows DLL, and a silent no-op everywhere this function's
# own WIN32 guard does not apply.
function(glintfx_colocate_embedded_runtime_dll target)
    if(NOT GLINTFX_EMBEDDED_RUNTIME_COLOCATE)
        return()
    endif()
    if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        # The consumer already established a single runtime output
        # location for every target of theirs, via the CMake-blessed
        # variable for exactly this problem - glintfx's own default
        # RUNTIME_OUTPUT_DIRECTORY already inherits it, same as any
        # other target that does not set its own property. Overriding
        # it here with our own guess (CMAKE_BINARY_DIR) would FIGHT
        # their existing convention instead of joining it, and could
        # land glintfx.dll in a DIFFERENT directory than the one they
        # already, correctly, send everything else to.
        return()
    endif()
    # CMAKE_BINARY_DIR (not PROJECT_BINARY_DIR, the opposite choice
    # from GLINTFX_GENERATED_INCLUDE_DIR at the top of the root
    # CMakeLists.txt, and deliberately so): the OUTERMOST project's own
    # binary directory is exactly what any of the CONSUMER's own
    # targets, declared directly in their top-level CMakeLists.txt,
    # already default to when THEY do not set RUNTIME_OUTPUT_DIRECTORY
    # either - glintfx joining that pre-existing CMake default, not
    # inventing a new one. No per-config generator expression is
    # written here: CMake's own multi-config generators (Visual Studio)
    # already append the per-configuration subdirectory to a plain
    # RUNTIME_OUTPUT_DIRECTORY value automatically, the exact same way
    # they would for any other un-customized target - measured live
    # (CI-CONSUME): the real Windows CI log placed an unrelated,
    # un-customized top-level target (embed_consumer.exe) at
    # <top-binary-dir>/Release/, with no generator expression written
    # by tests/embed/CMakeLists.txt at all.
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endfunction()
