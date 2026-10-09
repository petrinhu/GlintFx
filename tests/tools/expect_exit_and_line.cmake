# SPDX-License-Identifier: AGPL-3.0-or-later
# expect_exit_and_line.cmake - executor of the selftest registrations (D-W8-114).
#
# Form, inside a plain add_test(...) block (blob_selftests.py and check_selftest_orphan.py
# read exactly that form):
#   COMMAND "${CMAKE_COMMAND}" "-DEXPECT_EXIT=<n>" "-DEXPECT_LINE=<whole line>"
#           -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/expect_exit_and_line.cmake" --
#           <the command under test, unchanged>
#
# Why: PASS_REGULAR_EXPRESSION makes ctest ignore the exit code (CMake manual). A selftest that
# prints its OK line and then exits 1 used to pass green. Here the exit code AND one exact,
# literal line are both required. No regex and no list: a ';' in the output cannot split it,
# and an alternation such as (18|19|20) cannot match by construction.
#
# Selftest: cmake -DSELFTEST=ON -P expect_exit_and_line.cmake runs seven cases, each one running
# this same script against expect_exit_and_line_double.cmake. It prints the OK line only when all
# seven verdicts match. The child output is captured and never echoed, so the "FALHOU" word
# appears only on a real divergence.

set(EXPECT_EXIT_AND_LINE_SELF "${CMAKE_CURRENT_LIST_FILE}")
set(EXPECT_EXIT_AND_LINE_DOUBLE "${CMAKE_CURRENT_LIST_DIR}/expect_exit_and_line_double.cmake")

# Puts in `out` the arguments that follow the first "--" in CMAKE_ARGV; empty when there is none.
function(expect_exit_and_line_command out)
  set(command "")
  set(after_separator FALSE)
  math(EXPR last_index "${CMAKE_ARGC} - 1")
  foreach(index RANGE 0 ${last_index})
    if(after_separator)
      list(APPEND command "${CMAKE_ARGV${index}}")
    elseif("${CMAKE_ARGV${index}}" STREQUAL "--")
      set(after_separator TRUE)
    endif()
  endforeach()
  set(${out} "${command}" PARENT_SCOPE)
endfunction()

# Counts the EXACT whole lines equal to `line` in `text` (CR removed). Literal FIND, no regex, no list.
# A line that merely contains `line` does not count. The next search starts one character after the
# match, so two adjacent identical lines are both counted.
function(expect_exit_and_line_count text line out)
  string(REPLACE "\r" "" text "${text}")
  set(rest "\n${text}\n")
  set(needle "\n${line}\n")
  set(count 0)
  while(TRUE)
    string(FIND "${rest}" "${needle}" found)
    if(found EQUAL -1)
      break()
    endif()
    math(EXPR count "${count} + 1")
    math(EXPR cut "${found} + 1")
    string(SUBSTRING "${rest}" ${cut} -1 rest)
  endwhile()
  set(${out} ${count} PARENT_SCOPE)
endfunction()

# The real verdict: run the command under test, require rc == EXPECT_EXIT and exactly one EXPECT_LINE.
function(expect_exit_and_line_run)
  if(NOT DEFINED EXPECT_EXIT OR NOT "${EXPECT_EXIT}" MATCHES "^[0-9]+$")
    message(FATAL_ERROR "expect_exit_and_line: FALHOU - EXPECT_EXIT ausente ou nao inteiro: '${EXPECT_EXIT}'")
  endif()
  if(NOT DEFINED EXPECT_LINE OR "${EXPECT_LINE}" STREQUAL "")
    message(FATAL_ERROR "expect_exit_and_line: FALHOU - EXPECT_LINE ausente ou vazia")
  endif()
  expect_exit_and_line_command(command)
  if("${command}" STREQUAL "")
    message(FATAL_ERROR "expect_exit_and_line: FALHOU - nenhum comando depois de --")
  endif()
  # ERROR_VARIABLE names the same variable as OUTPUT_VARIABLE, so stdout and stderr land together.
  # ECHO_* keeps the output visible in the ctest log of the real run.
  execute_process(
    COMMAND ${command}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
    ECHO_OUTPUT_VARIABLE
    ECHO_ERROR_VARIABLE)
  if(NOT "${rc}" MATCHES "^[0-9]+$")
    message(FATAL_ERROR "expect_exit_and_line: FALHOU - processo nao terminou normalmente: ${rc}")
  endif()
  if(NOT "${rc}" EQUAL "${EXPECT_EXIT}")
    message(FATAL_ERROR "expect_exit_and_line: FALHOU - rc=${rc}, esperado=${EXPECT_EXIT}")
  endif()
  expect_exit_and_line_count("${output}" "${EXPECT_LINE}" count)
  message("expect_exit_and_line: rc=${rc} esperado=${EXPECT_EXIT} linha=${count} esperado=1")
  if(NOT "${count}" EQUAL 1)
    message(FATAL_ERROR "expect_exit_and_line: FALHOU - a linha esperada aparece ${count} vez(es), esperado 1")
  endif()
endfunction()

# One case verdict, in the caller's scope (macro, so the counters are shared with the selftest).
macro(expect_exit_and_line_verdict case_name want)
  if("${case_rc}" STREQUAL "0")
    set(case_got APROVA)
  else()
    set(case_got REPROVA)
  endif()
  math(EXPR selftest_total "${selftest_total} + 1")
  if(NOT "${case_got}" STREQUAL "${want}")
    math(EXPR selftest_failures "${selftest_failures} + 1")
    message("FALHOU caso ${case_name}: obtido=${case_got} esperado=${want} rc=${case_rc}")
  endif()
endmacro()

# A case that runs this script with a command (the double), capturing its output silently.
macro(expect_exit_and_line_case_with_command case_name expect_exit expect_line emit times emit_exit want)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DEXPECT_EXIT=${expect_exit}" "-DEXPECT_LINE=${expect_line}"
            -P "${EXPECT_EXIT_AND_LINE_SELF}" --
            "${CMAKE_COMMAND}" "-DEMIT=${emit}" "-DEMIT_TIMES=${times}" "-DEMIT_EXIT=${emit_exit}"
            -P "${EXPECT_EXIT_AND_LINE_DOUBLE}"
    RESULT_VARIABLE case_rc
    OUTPUT_VARIABLE case_output
    ERROR_VARIABLE case_output
    TIMEOUT 120)
  expect_exit_and_line_verdict("${case_name}" "${want}")
endmacro()

# A case that runs this script with NO command after "--".
macro(expect_exit_and_line_case_without_command case_name expect_exit expect_line want)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DEXPECT_EXIT=${expect_exit}" "-DEXPECT_LINE=${expect_line}"
            -P "${EXPECT_EXIT_AND_LINE_SELF}"
    RESULT_VARIABLE case_rc
    OUTPUT_VARIABLE case_output
    ERROR_VARIABLE case_output
    TIMEOUT 120)
  expect_exit_and_line_verdict("${case_name}" "${want}")
endmacro()

# The seven controls. Each one fixes the verdict BEFORE the run: APROVA or REPROVA.
function(expect_exit_and_line_run_selftest)
  set(selftest_total 0)
  set(selftest_failures 0)
  set(line "marca exata 42")
  expect_exit_and_line_case_with_command("1 linha e rc 0 aprova" 0 "${line}" "${line}" 1 0 APROVA)
  expect_exit_and_line_case_with_command("2 linha e rc 1 reprova" 0 "${line}" "${line}" 1 1 REPROVA)
  expect_exit_and_line_case_with_command("3 sem linha e rc 0 reprova" 0 "${line}" "outra saida" 1 0 REPROVA)
  expect_exit_and_line_case_with_command("4 linha duas vezes reprova" 0 "${line}" "${line}" 2 0 REPROVA)
  expect_exit_and_line_case_with_command("5 x antes da linha reprova" 0 "${line}" "x${line}" 1 0 REPROVA)
  expect_exit_and_line_case_with_command("6 rc esperado 1 com linha e rc 1 aprova" 1 "${line}" "${line}" 1 1 APROVA)
  expect_exit_and_line_case_without_command("7 sem comando reprova" 0 "${line}" REPROVA)
  if(NOT "${selftest_total}" EQUAL 7 OR selftest_failures GREATER 0)
    message(FATAL_ERROR "expect_exit_and_line --selftest: FALHOU ${selftest_failures} de ${selftest_total} casos")
  endif()
  message("expect_exit_and_line --selftest: os ${selftest_total} controles OK")
endfunction()

if(SELFTEST)
  expect_exit_and_line_run_selftest()
else()
  expect_exit_and_line_run()
endif()
