# SPDX-License-Identifier: AGPL-3.0-or-later
# check_no_pass_regex.cmake - guard against PASS_REGULAR_EXPRESSION coming back (D-W8-114, L-36).
#
# Usage: cmake -DREGISTRY=<absolute path to tests/CMakeLists.txt> -P check_no_pass_regex.cmake
#
# Fails when a non-comment line uses PASS_REGULAR_EXPRESSION (ctest then ignores the exit code),
# and when fewer than 7 non-comment lines pass through expect_exit_and_line.cmake (the floor of
# the seven selftest sites; zero or a short count fails, L-40). The file is read with file(READ),
# never file(STRINGS): a line with ';' would be split into two list items.

if(NOT DEFINED REGISTRY OR "${REGISTRY}" STREQUAL "")
  message(FATAL_ERROR "check_no_pass_regex: FALHOU - -DREGISTRY vazio ou ausente")
endif()
if(NOT EXISTS "${REGISTRY}")
  message(FATAL_ERROR "check_no_pass_regex: FALHOU - registro inexistente: ${REGISTRY}")
endif()

file(READ "${REGISTRY}" registry_text)
string(REPLACE ";" "\\;" registry_text "${registry_text}")
string(REPLACE "\n" ";" registry_lines "${registry_text}")

set(pass_uses 0)
set(executor_uses 0)
foreach(line IN LISTS registry_lines)
  string(REGEX REPLACE "^[ \t]+" "" trimmed "${line}")
  if(trimmed MATCHES "^#")
    continue()
  endif()
  if(line MATCHES "PASS_REGULAR_EXPRESSION")
    math(EXPR pass_uses "${pass_uses} + 1")
  endif()
  if(line MATCHES "expect_exit_and_line\\.cmake")
    math(EXPR executor_uses "${executor_uses} + 1")
  endif()
endforeach()

message("check_no_pass_regex: PASS_REGULAR_EXPRESSION=${pass_uses} executor=${executor_uses}")
if(NOT "${pass_uses}" EQUAL 0)
  message(FATAL_ERROR "check_no_pass_regex: FALHOU - ${pass_uses} uso(s) de PASS_REGULAR_EXPRESSION fora de comentario")
endif()
if("${executor_uses}" LESS 7)
  message(FATAL_ERROR "check_no_pass_regex: FALHOU - so ${executor_uses} registro(s) pelo executor, piso 7")
endif()
