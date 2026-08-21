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
    install(TARGETS ${target}
        EXPORT glintfxTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
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
