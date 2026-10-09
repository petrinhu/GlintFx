# SPDX-License-Identifier: AGPL-3.0-or-later
# check_no_pass_regex.cmake - guard against PASS_REGULAR_EXPRESSION coming back (D-W8-114, D-W8-147 to D-W8-149,
# D-W8-163 to D-W8-165, L-36).
#
# Usage: cmake -DREGISTRY=<absolute path to tests/CMakeLists.txt> -P check_no_pass_regex.cmake
#        cmake -DSELFTEST=ON -P check_no_pass_regex.cmake
#
# Fails when a non-comment line uses PASS_REGULAR_EXPRESSION (ctest then ignores the exit code), and when the
# number of sites is not EXACTLY the number below. A SITE is a non-comment line with
# `expect_exit_and_line.cmake" --`, the executor followed by the separator of the command under test; the
# executor's own selftest registration has no separator and is no site. The count is exact (D-W8-163): a slice
# that adds or removes a site changes the number in the same commit, so the diff shows every change of it. The
# guard cannot remember a maximum; the history of this number in git does.
# The file is read with file(READ), never file(STRINGS) (a ';' would split a line). Before the split every
# '[' and ']' is replaced: CMake does not split a list inside unbalanced square brackets, so a comment with an
# open '[' would swallow the lines after it (D-W8-148).

set(CHECK_NO_PASS_REGEX_SITES 8)

# Puts in `out_pass` and `out_sites` the non-comment uses of PASS_REGULAR_EXPRESSION and the sites of `text`.
function(check_no_pass_regex_count text out_pass out_sites)
  string(REPLACE "[" "<" text "${text}")
  string(REPLACE "]" ">" text "${text}")
  string(REPLACE ";" "\\;" text "${text}")
  string(REPLACE "\n" ";" lines "${text}")
  set(pass_uses 0)
  set(sites 0)
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "^[ \t]+" "" trimmed "${line}")
    if(trimmed MATCHES "^#")
      continue()
    endif()
    if(line MATCHES "PASS_REGULAR_EXPRESSION")
      math(EXPR pass_uses "${pass_uses} + 1")
    endif()
    if(line MATCHES "expect_exit_and_line\\.cmake\" --")
      math(EXPR sites "${sites} + 1")
    endif()
  endforeach()
  set(${out_pass} ${pass_uses} PARENT_SCOPE)
  set(${out_sites} ${sites} PARENT_SCOPE)
endfunction()

# Puts in `out_ok` TRUE only when there is no PASS use and EXACTLY the expected number of sites.
function(check_no_pass_regex_passes pass_uses sites out_ok)
  set(${out_ok} FALSE PARENT_SCOPE)
  if("${pass_uses}" EQUAL 0 AND "${sites}" EQUAL ${CHECK_NO_PASS_REGEX_SITES})
    set(${out_ok} TRUE PARENT_SCOPE)
  endif()
endfunction()

# The real check over the registry named by -DREGISTRY.
function(check_no_pass_regex_run)
  if(NOT DEFINED REGISTRY OR "${REGISTRY}" STREQUAL "")
    message(FATAL_ERROR "check_no_pass_regex: FALHOU - -DREGISTRY vazio ou ausente")
  endif()
  if(NOT EXISTS "${REGISTRY}")
    message(FATAL_ERROR "check_no_pass_regex: FALHOU - registro inexistente: ${REGISTRY}")
  endif()
  file(READ "${REGISTRY}" registry_text)
  check_no_pass_regex_count("${registry_text}" pass_uses sites)
  message("check_no_pass_regex: PASS_REGULAR_EXPRESSION=${pass_uses} sitios=${sites} esperado=${CHECK_NO_PASS_REGEX_SITES}")
  check_no_pass_regex_passes(${pass_uses} ${sites} ok)
  if(NOT ok)
    message(FATAL_ERROR "check_no_pass_regex: FALHOU - PASS fora de comentario=${pass_uses} (tem de ser 0), sitios=${sites} (tem de ser ${CHECK_NO_PASS_REGEX_SITES})")
  endif()
endfunction()

# One selftest case: the counts and the verdict of `text` must be the expected values.
function(check_no_pass_regex_case case_name text want_pass want_sites want_verdict)
  check_no_pass_regex_count("${text}" case_pass case_sites)
  check_no_pass_regex_passes(${case_pass} ${case_sites} case_ok)
  set(case_verdict REPROVA)
  if(case_ok)
    set(case_verdict APROVA)
  endif()
  math(EXPR total "${selftest_total} + 1")
  set(selftest_total ${total} PARENT_SCOPE)
  set(got "${case_pass}/${case_sites}/${case_verdict}")
  set(want "${want_pass}/${want_sites}/${want_verdict}")
  if(NOT "${got}" STREQUAL "${want}")
    math(EXPR failures "${selftest_failures} + 1")
    set(selftest_failures ${failures} PARENT_SCOPE)
    message("FALHOU caso ${case_name}: obtido=${got} esperado=${want}")
  endif()
endfunction()

# The nine controls, over texts built here (no file). The plants are built from the expected number (exact, one
# less, one more), so the logic is proven for any number; the registry check (no_pass_regex_test) pins the number
# itself. Each case fixes counts and verdict BEFORE the run.
function(check_no_pass_regex_run_selftest)
  set(selftest_total 0)
  set(selftest_failures 0)
  set(site "        -P \"tools/expect_exit_and_line.cmake\" --\n")
  set(selftest_form "        -P \"tools/expect_exit_and_line.cmake\"\n")
  set(pass_line "set_tests_properties(t PROPERTIES PASS_REGULAR_EXPRESSION \"x\")\n")
  set(pass_trailing "set_tests_properties(t PROPERTIES PASS_REGULAR_EXPRESSION \"x\") # nota\n")
  set(comment_pass "# PASS_REGULAR_EXPRESSION only in a comment\n")
  set(open_bracket "# nota [\n")
  set(close_bracket "# fecha ]\n")
  set(exact ${CHECK_NO_PASS_REGEX_SITES})
  math(EXPR below "${exact} - 1")
  math(EXPR above "${exact} + 1")
  string(REPEAT "${site}" ${exact} exact_sites)
  string(REPEAT "${site}" ${below} below_sites)
  string(REPEAT "${site}" ${above} above_sites)
  check_no_pass_regex_case("1 a contagem exata e PASS so em comentario aprova" "${comment_pass}${exact_sites}"
                           0 ${exact} APROVA)
  check_no_pass_regex_case("2 PASS em linha de codigo reprova" "${exact_sites}${pass_line}" 1 ${exact} REPROVA)
  check_no_pass_regex_case("3 um sitio a menos reprova" "${below_sites}" 0 ${below} REPROVA)
  check_no_pass_regex_case("4 colchete aberto num comentario nao engole o PASS seguinte"
                           "${exact_sites}${open_bracket}${pass_line}" 1 ${exact} REPROVA)
  check_no_pass_regex_case("5 PASS com comentario no fim da linha reprova" "${exact_sites}${pass_trailing}"
                           1 ${exact} REPROVA)
  check_no_pass_regex_case("6 o registro do autoteste do executor nao e sitio" "${below_sites}${selftest_form}"
                           0 ${below} REPROVA)
  check_no_pass_regex_case("7 registro vazio reprova" "" 0 0 REPROVA)
  check_no_pass_regex_case("8 colchete de fechar solto num comentario nao engole o PASS seguinte"
                           "${exact_sites}${close_bracket}${pass_line}" 1 ${exact} REPROVA)
  check_no_pass_regex_case("9 um sitio a mais reprova: a contagem e exata" "${above_sites}" 0 ${above} REPROVA)
  if(NOT "${selftest_total}" EQUAL 9 OR selftest_failures GREATER 0)
    message(FATAL_ERROR "check_no_pass_regex --selftest: FALHOU ${selftest_failures} de ${selftest_total} casos")
  endif()
  message("check_no_pass_regex --selftest: os ${selftest_total} controles OK")
endfunction()

if(SELFTEST)
  check_no_pass_regex_run_selftest()
else()
  check_no_pass_regex_run()
endif()
