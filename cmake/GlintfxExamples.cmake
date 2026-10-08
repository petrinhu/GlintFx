# SPDX-License-Identifier: AGPL-3.0-or-later
#
# GlintfxExamples.cmake
#
# Single subject: wiring the consumer examples (examples/) into the
# glintfx build. The root applies compile options and the runtime output
# directory to every example executable AFTER add_subdirectory(examples),
# per target PROPERTY, never through a directory variable: a variable set
# before add_subdirectory(examples) would be inherited by tests/ too.
#
# The pass counts what it did and prints it (D-W8-84):
#   glintfx: exemplos diretorios=<n> alvos_tratados=<m>
# n is the number of examples/*/CMakeLists.txt, m the number of
# executable targets that received the options. n != m is a
# FATAL_ERROR: an example the pass did not reach would be built without
# the warnings-as-errors and the sanitizer of the rest of the tree, and
# nothing would say so. Zero directories is a legal state (no example
# yet): the floor "at least one" belongs to the CI criterion that reads
# this line, not to the pass.

# The example directories, one entry per examples/<name>/CMakeLists.txt.
function(glintfx_examples_directories out_var)
    file(GLOB _manifests LIST_DIRECTORIES false
        "${PROJECT_SOURCE_DIR}/examples/*/CMakeLists.txt")
    set(_directories "")
    foreach(_manifest IN LISTS _manifests)
        cmake_path(GET _manifest PARENT_PATH _directory)
        list(APPEND _directories "${_directory}")
    endforeach()
    set(${out_var} "${_directories}" PARENT_SCOPE)
endfunction()

# The glintfx options of ONE example executable: the common compile
# options (standard, warnings, sanitizer) and its own output directory.
function(glintfx_examples_treat_target target)
    glintfx_apply_compile_options(${target})
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/examples/bin")
endfunction()

# Treats every executable target of ONE example directory; sets
# out_count to how many it treated.
function(glintfx_examples_treat_directory directory out_count)
    get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    set(_treated 0)
    foreach(_target IN LISTS _targets)
        get_target_property(_type ${_target} TYPE)
        if(_type STREQUAL "EXECUTABLE")
            glintfx_examples_treat_target(${_target})
            math(EXPR _treated "${_treated} + 1")
        endif()
    endforeach()
    set(${out_count} ${_treated} PARENT_SCOPE)
endfunction()

# Entry point called by the root CMakeLists.txt when
# GLINTFX_BUILD_EXAMPLES is ON.
function(glintfx_add_examples)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/examples/CMakeLists.txt")
        message(FATAL_ERROR
            "GLINTFX_BUILD_EXAMPLES is ON but examples/CMakeLists.txt does "
            "not exist under ${PROJECT_SOURCE_DIR}. A partial copy of the "
            "glintfx tree must configure with -DGLINTFX_BUILD_EXAMPLES=OFF.")
    endif()
    add_subdirectory("${PROJECT_SOURCE_DIR}/examples"
        "${PROJECT_BINARY_DIR}/examples")

    glintfx_examples_directories(_directories)
    list(LENGTH _directories _directory_count)
    set(_treated_count 0)
    foreach(_directory IN LISTS _directories)
        glintfx_examples_treat_directory("${_directory}" _treated_here)
        math(EXPR _treated_count "${_treated_count} + ${_treated_here}")
    endforeach()

    message(STATUS
        "glintfx: exemplos diretorios=${_directory_count} "
        "alvos_tratados=${_treated_count}")
    if(NOT _directory_count EQUAL _treated_count)
        message(FATAL_ERROR
            "glintfx: exemplos diretorios=${_directory_count} but "
            "alvos_tratados=${_treated_count}: an example directory was not "
            "reached by the pass that applies warnings, sanitizer and the "
            "output directory.")
    endif()
endfunction()
