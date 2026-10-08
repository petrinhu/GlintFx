# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/examples_tests.cmake
#
# Single subject: the tests that belong to the consumer examples
# (examples/), kept apart from tests/CMakeLists.txt so that file does
# not grow with every slice of that work. Included by ONE line of
# tests/CMakeLists.txt, which makes this file share its directory scope:
# every test registered here is a test of tests/ for ctest, labels and
# properties included.
#
# NOTE for the next editor: gates that scan tests/CMakeLists.txt by its
# TEXT (tests/tools/check_ctest_parallel_policy.py, blob_selftests.py,
# check_selftest_orphan.py) do not read this file. The
# `# glintfx-build-dir:` annotation below follows the convention anyway.

# EXAMPLES-DEFAULT: the default of GLINTFX_BUILD_EXAMPLES, both branches
# (see tests/examples_option_default/CMakeLists.txt for what it proves
# and for what it leaves to embed_test).
# glintfx-build-dir: private-subdir - tests/examples_option_default_check/ (configura um build aninhado so' ali, LANGUAGES NONE, sem compilador)
add_test(
    NAME examples_option_default_test
    COMMAND "${CMAKE_COMMAND}"
        --fresh
        -S "${CMAKE_CURRENT_SOURCE_DIR}/examples_option_default"
        -B "${CMAKE_CURRENT_BINARY_DIR}/examples_option_default_check"
        -G "${CMAKE_GENERATOR}"
        "-DGLINTFX_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
)
set_tests_properties(examples_option_default_test PROPERTIES LABELS consume)
