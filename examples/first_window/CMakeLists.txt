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
