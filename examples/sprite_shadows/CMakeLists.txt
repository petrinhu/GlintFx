# SPDX-License-Identifier: AGPL-3.0-or-later
#
# examples/sprite_shadows/CMakeLists.txt
#
# Single subject: the sprite_shadows example, a plain consumer program. It links glintfx::glintfx and
# nothing else; warnings, sanitizer and the output directory come from the glintfx root when the example
# is built inside the tree (cmake/GlintfxExamples.cmake), and from the caller when it is built against an
# installed glintfx (examples/CMakeLists.txt).
add_executable(sprite_shadows sprite_shadows.cpp)
target_link_libraries(sprite_shadows PRIVATE glintfx::glintfx)
set_target_properties(sprite_shadows PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
# Windows: the glintfx DLL (shared builds) goes next to the example, or the example does not start. The
# guard turns an empty list (static builds) into a no-op, the same guard cmake/GlintfxTest.cmake uses; the
# example cannot call that helper, which depends on BUILD_SHARED_LIBS, absent outside the tree.
if(WIN32)
    add_custom_command(TARGET sprite_shadows POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:sprite_shadows>>,copy_if_different,true>"
            "$<TARGET_RUNTIME_DLLS:sprite_shadows>" "$<TARGET_FILE_DIR:sprite_shadows>"
        COMMAND_EXPAND_LISTS
    )
endif()
