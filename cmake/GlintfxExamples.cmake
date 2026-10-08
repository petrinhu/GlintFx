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
# The pass takes its two roots as ARGUMENTS (D-W8-94), never from
# PROJECT_SOURCE_DIR or PROJECT_BINARY_DIR: source_root is the directory
# that CONTAINS examples/, binary_root is the build directory that matches
# it. The root calls it with PROJECT_*; the examples-pass test project
# (tests/examples_pass/) calls it with a planta tree, which has no PROJECT.
#
# The pass treats EVERY executable of the examples/ subtree (D-W8-92), not
# only the ones that sit directly in a subdirectory, and checks three rules,
# in this order, each with its own FATAL_ERROR and text:
#   1. a directory examples/<x>/ with CMakeLists.txt on disk must be in
#      SUBDIRECTORIES of examples/ (otherwise its executables never enter
#      the build, and the pass would say nothing);
#   2. every reached example directory has at least one executable in its
#      own subtree;
#   3. examples/CMakeLists.txt declares no executable itself: each example
#      lives in its own directory (the copy, the consume gate and the
#      capture count on that).
# Two executables in one directory are legal, and both are treated.
#
# The pass prints one line, always (D-W8-92, criterion 4 of the v2 plan):
#   glintfx: exemplos diretorios=<n> alcancados=<r> executaveis_tratados=<t>
# A zero directory count is a legal state (no example yet): the floor "at
# least one" belongs to the CI criterion that reads this line, not to the
# pass.

# The example directories, one entry per <source_root>/examples/<name>/CMakeLists.txt.
function(glintfx_examples_directories source_root out_var)
    file(GLOB _manifests LIST_DIRECTORIES false
        "${source_root}/examples/*/CMakeLists.txt")
    set(_directories "")
    foreach(_manifest IN LISTS _manifests)
        cmake_path(GET _manifest PARENT_PATH _directory)
        list(APPEND _directories "${_directory}")
    endforeach()
    set(${out_var} "${_directories}" PARENT_SCOPE)
endfunction()

# The directories examples/CMakeLists.txt actually added, normalized.
function(glintfx_examples_reached_directories source_root out_var)
    get_property(_subdirectories DIRECTORY "${source_root}/examples"
        PROPERTY SUBDIRECTORIES)
    set(_normal "")
    foreach(_subdirectory IN LISTS _subdirectories)
        cmake_path(NORMAL_PATH _subdirectory OUTPUT_VARIABLE _normal_one)
        list(APPEND _normal "${_normal_one}")
    endforeach()
    set(${out_var} "${_normal}" PARENT_SCOPE)
endfunction()

# Rule 1. Runs BEFORE any property of the directory is read, so the message
# names the cause and not a cascade of errors from a directory that was
# never added.
function(glintfx_examples_check_reached source_root directory)
    glintfx_examples_reached_directories("${source_root}" _reached)
    cmake_path(NORMAL_PATH directory OUTPUT_VARIABLE _normal_directory)
    list(FIND _reached "${_normal_directory}" _index)
    if(_index EQUAL -1)
        cmake_path(GET directory FILENAME _name)
        message(FATAL_ERROR
            "glintfx: examples/${_name}/ tem CMakeLists.txt mas nao foi adicionado: "
            "acrescente add_subdirectory(${_name}) em examples/CMakeLists.txt")
    endif()
endfunction()

# The glintfx options of ONE example executable: the common compile
# options (standard, warnings, sanitizer) and its own output directory,
# which lives under the build root that the pass was given.
function(glintfx_examples_treat_target target binary_root)
    glintfx_apply_compile_options(${target})
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${binary_root}/examples/bin")
endfunction()

# Treats the executable targets declared IN ONE directory (not below it);
# sets out_count to how many it treated.
function(glintfx_examples_treat_directory directory binary_root out_count)
    get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    set(_treated 0)
    foreach(_target IN LISTS _targets)
        get_target_property(_kind ${_target} TYPE)
        if(_kind STREQUAL "EXECUTABLE")
            glintfx_examples_treat_target(${_target} "${binary_root}")
            math(EXPR _treated "${_treated} + 1")
        endif()
    endforeach()
    set(${out_count} ${_treated} PARENT_SCOPE)
endfunction()

# Treats every executable of ONE example directory AND of everything below
# it, by the SUBDIRECTORIES walk (D-W8-92, rule b). Sets out_count to the
# total treated in the subtree.
function(glintfx_examples_treat_tree directory binary_root out_count)
    glintfx_examples_treat_directory("${directory}" "${binary_root}" _own)
    get_property(_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    set(_total ${_own})
    foreach(_subdirectory IN LISTS _subdirectories)
        glintfx_examples_treat_tree("${_subdirectory}" "${binary_root}" _below)
        math(EXPR _total "${_total} + ${_below}")
    endforeach()
    set(${out_count} ${_total} PARENT_SCOPE)
endfunction()

# Rule 2. An example directory whose subtree has no executable is refused.
function(glintfx_examples_check_has_executable directory executable_count)
    if(executable_count EQUAL 0)
        cmake_path(GET directory FILENAME _name)
        message(FATAL_ERROR
            "glintfx: examples/${_name}/ nao declara nenhum executavel")
    endif()
endfunction()

# Rule 3. An executable declared directly in examples/CMakeLists.txt is
# refused: it would sit outside every example directory.
function(glintfx_examples_check_root_targets source_root)
    get_property(_root_targets DIRECTORY "${source_root}/examples"
        PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_target IN LISTS _root_targets)
        get_target_property(_root_kind ${_target} TYPE)
        if(_root_kind STREQUAL "EXECUTABLE")
            message(FATAL_ERROR
                "glintfx: o executavel ${_target} foi declarado em examples/CMakeLists.txt; "
                "cada exemplo mora no proprio diretorio")
        endif()
    endforeach()
endfunction()

# The one line the pass prints, always (D-W8-92).
function(glintfx_examples_report directory_count reached_count treated_count)
    message(STATUS
        "glintfx: exemplos diretorios=${directory_count} "
        "alcancados=${reached_count} "
        "executaveis_tratados=${treated_count}")
endfunction()

# Entry point. The root calls it with PROJECT_SOURCE_DIR and
# PROJECT_BINARY_DIR when GLINTFX_BUILD_EXAMPLES is ON.
function(glintfx_add_examples source_root binary_root)
    if(NOT EXISTS "${source_root}/examples/CMakeLists.txt")
        message(FATAL_ERROR
            "GLINTFX_BUILD_EXAMPLES is ON but examples/CMakeLists.txt does "
            "not exist under ${source_root}. A partial copy of the "
            "glintfx tree must configure with -DGLINTFX_BUILD_EXAMPLES=OFF.")
    endif()
    add_subdirectory("${source_root}/examples"
        "${binary_root}/examples")

    glintfx_examples_directories("${source_root}" _directories)
    list(LENGTH _directories _directory_count)

    # Rule 1 over EVERY directory first, before any directory is treated.
    set(_reached_count 0)
    foreach(_directory IN LISTS _directories)
        glintfx_examples_check_reached("${source_root}" "${_directory}")
        math(EXPR _reached_count "${_reached_count} + 1")
    endforeach()

    # Rule 2, and the treatment of each subtree.
    set(_treated_count 0)
    foreach(_directory IN LISTS _directories)
        glintfx_examples_treat_tree("${_directory}" "${binary_root}" _treated_here)
        glintfx_examples_check_has_executable("${_directory}" ${_treated_here})
        math(EXPR _treated_count "${_treated_count} + ${_treated_here}")
    endforeach()

    # Rule 3.
    glintfx_examples_check_root_targets("${source_root}")

    glintfx_examples_report(${_directory_count} ${_reached_count} ${_treated_count})
endfunction()
