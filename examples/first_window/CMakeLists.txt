# SPDX-License-Identifier: AGPL-3.0-or-later
#
# examples/first_window/CMakeLists.txt
#
# Single subject: the first_window example, a plain consumer program. It links glintfx::glintfx and
# nothing else; warnings, sanitizer and the output directory come from the glintfx root when the example
# is built inside the tree (cmake/GlintfxExamples.cmake), and from the caller when it is built against an
# installed glintfx (examples/CMakeLists.txt).
add_executable(first_window first_window.cpp)
target_link_libraries(first_window PRIVATE glintfx::glintfx)
set_target_properties(first_window PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
# Windows: the glintfx DLL (shared builds) goes next to the example, or the example does not start. The
# guard turns an empty list (static builds) into a no-op, the same guard cmake/GlintfxTest.cmake uses; the
# example cannot call that helper, which depends on BUILD_SHARED_LIBS, absent outside the tree.
if(WIN32)
    add_custom_command(TARGET first_window POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E
            "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:first_window>>,copy_if_different,true>"
            "$<TARGET_RUNTIME_DLLS:first_window>" "$<TARGET_FILE_DIR:first_window>"
        COMMAND_EXPAND_LISTS
    )
endif()
