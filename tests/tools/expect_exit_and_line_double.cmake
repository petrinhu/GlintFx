# SPDX-License-Identifier: AGPL-3.0-or-later
# expect_exit_and_line_double.cmake - test double of expect_exit_and_line.cmake (D-W8-114).
#
# Writes EMIT on its own line EMIT_TIMES times, through message(NOTICE) (stderr), then exits
# with EMIT_EXIT. It is the command under test of the executor's selftest only. It is not
# registered with add_test: it contains no add_test(...) on purpose.
#   cmake -DEMIT=<text> -DEMIT_TIMES=<n> -DEMIT_EXIT=<n> -P expect_exit_and_line_double.cmake

foreach(index RANGE 1 ${EMIT_TIMES})
  message(NOTICE "${EMIT}")
endforeach()
# The cases use only 0 (success) and 1 (failure). A literal branch keeps the exit code
# out of cmake_language(EXIT), an indirect call that the L-07 gate refuses to judge.
if(NOT "${EMIT_EXIT}" EQUAL 0)
  message(FATAL_ERROR "expect_exit_and_line_double: saida simulada ${EMIT_EXIT}")
endif()
