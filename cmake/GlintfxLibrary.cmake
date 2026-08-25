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
    )
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
