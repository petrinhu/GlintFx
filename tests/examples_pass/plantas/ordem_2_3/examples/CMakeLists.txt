# SPDX-License-Identifier: AGPL-3.0-or-later
#
# planta ordem_2_3: lib_only has no executable (rule 2) and root_tool sits at the root (rule 3).

add_subdirectory(lib_only)
add_executable(root_tool root_tool.cpp)
