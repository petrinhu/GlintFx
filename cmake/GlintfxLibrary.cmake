# GlintfxLibrary.cmake
#
# Single subject: target properties, export header and include
# directories of the glintfx library. Install is GlintfxInstall.cmake;
# language standard and warnings are GlintfxCompileOptions.cmake.

include(GenerateExportHeader)

function(glintfx_set_target_properties target)
    set_target_properties(${target} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 0
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endfunction()

function(glintfx_generate_export_header target)
    generate_export_header(${target}
        EXPORT_MACRO_NAME GLINTFX_API
        EXPORT_FILE_NAME "${GLINTFX_GENERATED_INCLUDE_DIR}/glintfx/export.hpp"
    )
endfunction()

function(glintfx_configure_version_header)
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/version_macros.hpp.in"
        "${GLINTFX_GENERATED_INCLUDE_DIR}/glintfx/version_macros.hpp"
        @ONLY
    )
endfunction()

function(glintfx_set_target_include_dirs target)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${GLINTFX_GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
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
