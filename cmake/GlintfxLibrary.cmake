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
