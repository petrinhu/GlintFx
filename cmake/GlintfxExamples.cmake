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
# THE GRAPH IS THE SOURCE (D-W8-98). The pass walks the SUBDIRECTORIES that
# examples/CMakeLists.txt really added, the same graph CMake builds from. The
# disk is only CHECKED against it (rule 1): a directory on disk with a
# CMakeLists.txt that the graph does not reach is refused, at any depth. So a
# grouping folder, or any executable that CMake would build under examples/,
# gets the options and the output directory, or the pass refuses it.
#
# The pass checks four rules, in this order, each with its own FATAL_ERROR and
# text (D-W8-92, D-W8-98, D-W8-99):
#   1. a directory examples/<rel>/ with CMakeLists.txt on disk must be reached
#      by the graph; the message names the nearest ancestor that is reached,
#      and the add_subdirectory that is missing there (D-W8-99);
#   4. every entry of SUBDIRECTORIES of examples/ must be a DIRECT child of
#      examples/ (a grouping needs its own CMakeLists.txt, so the example
#      stays examples/<name>/, D-W8-98);
#   2. every example (a direct child) has at least one executable in its
#      subtree;
#   3. examples/CMakeLists.txt declares no executable itself.
# Two executables in one directory are legal, and both are treated.
#
# The pass prints one line, always (D-W8-100):
#   glintfx: exemplos diretorios=<d> alcancados=<a> executaveis_tratados=<t>
# d is the number of examples (the entries of SUBDIRECTORIES of examples/); a
# is the number of directories the graph reaches BELOW examples/ (examples/
# itself not counted), so a >= d, and a - d counts the subdirectories inside
# the examples; t is the number of executables treated in the whole subtree.
# A zero count is a legal state (no example yet): the floor "at least one"
# belongs to the CI criterion that reads this line, not to the pass.
#
# The order of the steps is part of the contract, and the blocks are marked
# (# STEP ...) so that the V-19 mutants can move them and be seen to die.

# Directories of ONE subtree, normalized, the directory itself first, then
# everything below it through SUBDIRECTORIES.
function(glintfx_examples_collect_tree directory out_list)
    cmake_path(NORMAL_PATH directory OUTPUT_VARIABLE _normal)
    set(_collected "${_normal}")
    get_property(_subdirectories DIRECTORY "${_normal}" PROPERTY SUBDIRECTORIES)
    foreach(_subdirectory IN LISTS _subdirectories)
        glintfx_examples_collect_tree("${_subdirectory}" _below)
        list(APPEND _collected ${_below})
    endforeach()
    set(${out_list} "${_collected}" PARENT_SCOPE)
endfunction()

# The graph: the direct entries of examples/ (normalized) and every directory
# reached BELOW examples/. Only COLLECTS: it reads what CMake built, and never
# touches a directory the graph does not contain (that is what keeps rule 1
# from cascading into get_property errors).
function(glintfx_examples_walk examples out_reached out_entries)
    get_property(_entries DIRECTORY "${examples}" PROPERTY SUBDIRECTORIES)
    set(_normal_entries "")
    set(_reached "")
    foreach(_entry IN LISTS _entries)
        cmake_path(NORMAL_PATH _entry OUTPUT_VARIABLE _normal)
        list(APPEND _normal_entries "${_normal}")
        glintfx_examples_collect_tree("${_normal}" _below)
        list(APPEND _reached ${_below})
    endforeach()
    set(${out_reached} "${_reached}" PARENT_SCOPE)
    set(${out_entries} "${_normal_entries}" PARENT_SCOPE)
endfunction()

# The directories on disk that hold a CMakeLists.txt below examples/, at any
# depth, normalized, sorted. Sorting the DIRECTORIES (not the manifests) puts
# a parent before its children, so the first message names the parent.
function(glintfx_examples_manifest_directories examples out_directories)
    file(GLOB_RECURSE _manifests LIST_DIRECTORIES false "${examples}/*/CMakeLists.txt")
    set(_directories "")
    foreach(_manifest IN LISTS _manifests)
        cmake_path(GET _manifest PARENT_PATH _directory)
        cmake_path(NORMAL_PATH _directory OUTPUT_VARIABLE _normal)
        if(_normal STREQUAL "${examples}")
            continue()
        endif()
        if(_normal MATCHES "/CMakeFiles(/|$)")
            continue()
        endif()
        list(APPEND _directories "${_normal}")
    endforeach()
    list(REMOVE_DUPLICATES _directories)
    list(SORT _directories)
    set(${out_directories} "${_directories}" PARENT_SCOPE)
endfunction()

# The message of rule 1: the directory on disk, and the nearest ancestor that
# the graph reaches (examples/ itself at the limit), with the add_subdirectory
# missing THERE. First level: the text of the original rule 1, unchanged.
function(glintfx_examples_unreached_message examples reached directory out_message)
    cmake_path(GET examples PARENT_PATH _examples_parent)
    file(RELATIVE_PATH _rel "${examples}" "${directory}")
    set(_ancestor "${directory}")
    while(TRUE)
        cmake_path(GET _ancestor PARENT_PATH _previous)
        set(_ancestor "${_previous}")
        if(_ancestor STREQUAL "${examples}")
            break()
        endif()
        if(_ancestor STREQUAL _examples_parent)
            break()
        endif()
        list(FIND reached "${_ancestor}" _found)
        if(NOT _found EQUAL -1)
            break()
        endif()
    endwhile()
    if(_ancestor STREQUAL "${examples}")
        set(_where "examples")
        set(_child "${_rel}")
    else()
        file(RELATIVE_PATH _where_rel "${examples}" "${_ancestor}")
        set(_where "examples/${_where_rel}")
        file(RELATIVE_PATH _child "${_ancestor}" "${directory}")
    endif()
    set(${out_message}
        "glintfx: examples/${_rel}/ tem CMakeLists.txt mas nao foi adicionado: acrescente add_subdirectory(${_child}) em ${_where}/CMakeLists.txt"
        PARENT_SCOPE)
endfunction()

# Rule 1 for ONE directory on disk: it must be in the graph.
function(glintfx_examples_check_reached examples reached directory)
    list(FIND reached "${directory}" _index)
    if(_index EQUAL -1)
        glintfx_examples_unreached_message("${examples}" "${reached}" "${directory}" _message)
        message(FATAL_ERROR "${_message}")
    endif()
endfunction()

# Rule 4 for ONE entry of examples/SUBDIRECTORIES: it must be a direct child.
# The printed path is relative to examples/ when the entry is inside it, and
# absolute when it is not (a `../` entry).
function(glintfx_examples_check_entry_is_direct examples entry)
    cmake_path(GET entry PARENT_PATH _parent)
    if(NOT _parent STREQUAL "${examples}")
        file(RELATIVE_PATH _shown "${examples}" "${entry}")
        if(_shown MATCHES "^[.][.]/")
            set(_shown "${entry}")
        endif()
        message(FATAL_ERROR
            "glintfx: examples/CMakeLists.txt adicionou ${_shown}, que nao e um diretorio filho de examples/; "
            "cada exemplo mora em examples/<nome>/, e um agrupamento precisa do proprio CMakeLists.txt")
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

# The one line the pass prints, always (D-W8-100).
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
    cmake_path(NORMAL_PATH source_root OUTPUT_VARIABLE _source_root)
    set(_examples "${_source_root}/examples")

    # STEP walk
    glintfx_examples_walk("${_examples}" _reached _entries)
    list(LENGTH _entries _directory_count)
    list(LENGTH _reached _reached_count)

    # STEP rule1
    glintfx_examples_manifest_directories("${_examples}" _manifest_directories)
    foreach(_directory IN LISTS _manifest_directories)
        glintfx_examples_check_reached("${_examples}" "${_reached}" "${_directory}")
    endforeach()

    # STEP rule4
    foreach(_entry IN LISTS _entries)
        glintfx_examples_check_entry_is_direct("${_examples}" "${_entry}")
    endforeach()

    # STEP treat
    set(_treated_count 0)
    foreach(_entry IN LISTS _entries)
        glintfx_examples_treat_tree("${_entry}" "${binary_root}" _treated_here)
        glintfx_examples_check_has_executable("${_entry}" ${_treated_here})
        math(EXPR _treated_count "${_treated_count} + ${_treated_here}")
    endforeach()

    # STEP rule3
    glintfx_examples_check_root_targets("${_source_root}")

    # STEP report
    glintfx_examples_report(${_directory_count} ${_reached_count} ${_treated_count})
endfunction()
