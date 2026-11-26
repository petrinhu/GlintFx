# SPDX-License-Identifier: AGPL-3.0-or-later
#
# inject_alloc_counter.cmake - DEMO-1 D6-red (D-W8-74): included by CMAKE_PROJECT_INCLUDE when the
# container image builds examples/ against the installed library, so every example links the TEST
# allocation counter (alloc_counter_classify.cpp, alloc_counter_hook.cpp) like every fixture of the
# Containerfile, without a line of the example knowing it. Test tooling: it never reaches a consumer.
# The two sources sit BESIDE this file (the Containerfile copies all three into /build), so the path is
# relative to it: check_container_fixture_link.py --exec runs the same configure outside the image, in a
# scratch directory that stands for /build, and an absolute /build path would not exist there.
add_library(glintfx_alloc_counter OBJECT
    "${CMAKE_CURRENT_LIST_DIR}/alloc_counter_classify.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/alloc_counter_hook.cpp"
)
set_target_properties(glintfx_alloc_counter PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_include_directories(glintfx_alloc_counter PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
link_libraries(glintfx_alloc_counter)
