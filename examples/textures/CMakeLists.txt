# SPDX-License-Identifier: AGPL-3.0-or-later
#
# examples/textures/CMakeLists.txt
#
# Single subject: the textures example, a plain consumer program. It links glintfx::glintfx and
# nothing else; warnings, sanitizer and the output directory come from the glintfx root when the example
# is built inside the tree (cmake/GlintfxExamples.cmake), and from the caller when it is built against an
# installed glintfx (examples/CMakeLists.txt).
add_executable(textures textures.cpp)
target_link_libraries(textures PRIVATE glintfx::glintfx)
set_target_properties(textures PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
# Windows: the glintfx DLL (shared builds) goes next to the example, or the example does not start. The
# guard turns an empty list (static builds) into a no-op, the same guard cmake/GlintfxTest.cmake uses; the
# example cannot call that helper, which depends on BUILD_SHARED_LIBS, absent outside the tree.
if(WIN32)
    add_custom_command(TARGET textures POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:textures>>,copy_if_different,true>"
            "$<TARGET_RUNTIME_DLLS:textures>" "$<TARGET_FILE_DIR:textures>"
        COMMAND_EXPAND_LISTS
    )
endif()
