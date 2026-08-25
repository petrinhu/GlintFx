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
