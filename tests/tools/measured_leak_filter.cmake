# SPDX-License-Identifier: AGPL-3.0-or-later
# measured_leak_filter.cmake - the ONE value of the FAIL_REGULAR_EXPRESSION that keeps MEASURED and SCANCOUNT lines
# out of a selftest's output, and the guard of that value and of its two owners (D-W8-176, D-W8-180, D-W8-181,
# D-W8-200, D-W8-201).
#
# include()d by tests/CMakeLists.txt, it only sets GLINTFX_MEASURED_LEAK_FILTER, which the two owners use.
# Run as a script: cmake -DREGISTRY=<absolute path to tests/CMakeLists.txt> -P measured_leak_filter.cmake
#                  cmake -DSELFTEST=ON -P measured_leak_filter.cmake
#
# The rule (D-W8-200): the filter is a SUPERSET of what tests/tools/collect_measured.py:86 collects. That reader is
# `^(?:MEASURED|SCANCOUNT)\s+` over each line of splitlines(), stripped, and Python's \s is Unicode (NBSP, U+2003,
# U+001F ...). The CMake regex reads BYTES, so the class names what CANNOT follow the word: anything that is not a
# letter, a digit or '_' fails the selftest. That also fails `MEASURED:` and `MEASURED=`, which the reader skips: a
# false red is visible, a false green is a silent leak into the parity table. Form feed and vertical tab never
# reach the reader's regex (splitlines() breaks the line there); the filter catches them anyway.
set(GLINTFX_MEASURED_LEAK_FILTER "(MEASURED|SCANCOUNT)[^A-Za-z0-9_]")
if(NOT CMAKE_SCRIPT_MODE_FILE)
  return()
endif()

# The two owners of the filter (D-W8-176). Each sets it through the variable, on the line right under its own
# `set_tests_properties(<owner> PROPERTIES LABELS selftest`, and AFTER the include() that defines it (D-W8-201).
set(MEASURED_LEAK_FILTER_OWNERS run_example_capture_win32_selftest run_capture_known_color_win32_selftest)

# Puts in `out` the names of the decided leak samples (D-W8-200) that `filter` does NOT catch, joined by ','.
function(measured_leak_filter_misses filter out)
  string(ASCII 9 tab)
  string(ASCII 194 160 nbsp)
  string(ASCII 226 128 131 em_space)
  string(ASCII 31 unit_sep)
  set(key "first_window.capture_alpha_compared=0")
  set(sample_tab "MEASURED${tab}${key}")
  set(sample_two_spaces "MEASURED  ${key}")
  set(sample_nbsp "MEASURED${nbsp}${key}")
  set(sample_em_space "MEASURED${em_space}${key}")
  set(sample_unit_sep "MEASURED${unit_sep}${key}")
  set(sample_scancount_space "SCANCOUNT ${key}")
  set(sample_scancount_nbsp "SCANCOUNT${nbsp}${key}")
  set(missed "")
  foreach(name IN ITEMS tab two_spaces nbsp em_space unit_sep scancount_space scancount_nbsp)
    string(REGEX MATCH "${filter}" hit "${sample_${name}}")
    if(hit STREQUAL "")
      list(APPEND missed ${name})
    endif()
  endforeach()
  list(JOIN missed "," joined)
  set(${out} "${joined}" PARENT_SCOPE)
endfunction()

# Puts in `out` the names of the clean samples (lines a passing selftest prints) that `filter` catches, joined by
# ','. Empty is right: a filter that catches them fails every green selftest.
function(measured_leak_filter_clean_hits filter out)
  set(clean_count_line "selftest: 34 controles OK")
  set(clean_identifier "MEASURED_ALPHA_LINE = 1")
  set(hits "")
  foreach(name IN ITEMS count_line identifier)
    string(REGEX MATCH "${filter}" hit "${clean_${name}}")
    if(NOT hit STREQUAL "")
      list(APPEND hits ${name})
    endif()
  endforeach()
  list(JOIN hits "," joined)
  set(${out} "${joined}" PARENT_SCOPE)
endfunction()

# Puts in `out_owned` how many owners set the filter through the variable right under their LABELS line, after the
# include(); in `out_consumers` the non-comment lines that use the variable; in `out_literals` the non-comment lines
# that write a MEASURED or SCANCOUNT FAIL_REGULAR_EXPRESSION by hand. The lines are split as in
# check_no_pass_regex.cmake: '[' and ']' replaced first, or an open '[' in a comment swallows the lines after it.
function(measured_leak_filter_count text out_owned out_consumers out_literals)
  set(use "FAIL_REGULAR_EXPRESSION \"\${GLINTFX_MEASURED_LEAK_FILTER}\"")
  string(FIND "${text}" "\ninclude(\"\${CMAKE_CURRENT_SOURCE_DIR}/tools/measured_leak_filter.cmake\")" include_at)
  set(owned 0)
  foreach(owner IN LISTS MEASURED_LEAK_FILTER_OWNERS)
    string(FIND "${text}" "\nset_tests_properties(${owner} PROPERTIES LABELS selftest\n    ${use})" at)
    if(include_at GREATER -1 AND at GREATER include_at)
      math(EXPR owned "${owned} + 1")
    endif()
  endforeach()
  string(REPLACE "[" "<" text "${text}")
  string(REPLACE "]" ">" text "${text}")
  string(REPLACE ";" "\;" text "${text}")
  string(REPLACE "\n" ";" lines "${text}")
  set(consumers 0)
  set(literals 0)
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "^[ \t]+" "" trimmed "${line}")
    if(trimmed MATCHES "^#")
      continue()
    endif()
    string(FIND "${line}" "${use}" at)
    if(at GREATER -1)
      math(EXPR consumers "${consumers} + 1")
    elseif(line MATCHES "FAIL_REGULAR_EXPRESSION.*(MEASURED|SCANCOUNT)")
      math(EXPR literals "${literals} + 1")
    endif()
  endforeach()
  set(${out_owned} ${owned} PARENT_SCOPE)
  set(${out_consumers} ${consumers} PARENT_SCOPE)
  set(${out_literals} ${literals} PARENT_SCOPE)
endfunction()

# Puts in `out_ok` TRUE only when every owner is anchored, the variable is used EXACTLY once per owner, and no line
# writes the filter by hand.
function(measured_leak_filter_registry_passes owned consumers literals out_ok)
  list(LENGTH MEASURED_LEAK_FILTER_OWNERS want)
  set(${out_ok} FALSE PARENT_SCOPE)
  if("${owned}" EQUAL ${want} AND "${consumers}" EQUAL ${want} AND "${literals}" EQUAL 0)
    set(${out_ok} TRUE PARENT_SCOPE)
  endif()
endfunction()

# The real check: the value against the samples, and the registry named by -DREGISTRY against the owners.
function(measured_leak_filter_run)
  if(NOT DEFINED REGISTRY OR "${REGISTRY}" STREQUAL "")
    message(FATAL_ERROR "measured_leak_filter: FALHOU - -DREGISTRY vazio ou ausente")
  endif()
  if(NOT EXISTS "${REGISTRY}")
    message(FATAL_ERROR "measured_leak_filter: FALHOU - registro inexistente: ${REGISTRY}")
  endif()
  measured_leak_filter_misses("${GLINTFX_MEASURED_LEAK_FILTER}" missed)
  measured_leak_filter_clean_hits("${GLINTFX_MEASURED_LEAK_FILTER}" hits)
  file(READ "${REGISTRY}" registry_text)
  measured_leak_filter_count("${registry_text}" owned consumers literals)
  list(LENGTH MEASURED_LEAK_FILTER_OWNERS want)
  message("measured_leak_filter: perdidas=<${missed}> limpas_pegas=<${hits}> donos=${owned} consumidores=${consumers}"
          " esperado=${want} literais=${literals}")
  measured_leak_filter_registry_passes(${owned} ${consumers} ${literals} ok)
  string(LENGTH "${missed}${hits}" wrong_samples)
  if(NOT ok OR wrong_samples GREATER 0)
    message(FATAL_ERROR "measured_leak_filter: FALHOU - amostra perdida, linha limpa pega, dono sem ancora, uso"
            " fora dos donos ou filtro escrito a mao (linha acima)")
  endif()
endfunction()

# One filter case: the missed decided samples and the caught clean samples of `filter` must be the expected texts.
function(measured_leak_filter_filter_case case_name filter want_missed want_hits)
  measured_leak_filter_misses("${filter}" missed)
  measured_leak_filter_clean_hits("${filter}" hits)
  math(EXPR total "${selftest_total} + 1")
  set(selftest_total ${total} PARENT_SCOPE)
  set(got "<${missed}>/<${hits}>")
  set(want "<${want_missed}>/<${want_hits}>")
  if(NOT "${got}" STREQUAL "${want}")
    math(EXPR failures "${selftest_failures} + 1")
    set(selftest_failures ${failures} PARENT_SCOPE)
    message("FALHOU caso ${case_name}: obtido=${got} esperado=${want}")
  endif()
endfunction()

# One registry case: the counts and the verdict of `text` must be the expected values.
function(measured_leak_filter_registry_case case_name text want)
  measured_leak_filter_count("${text}" owned consumers literals)
  measured_leak_filter_registry_passes(${owned} ${consumers} ${literals} ok)
  set(verdict REPROVA)
  if(ok)
    set(verdict APROVA)
  endif()
  math(EXPR total "${selftest_total} + 1")
  set(selftest_total ${total} PARENT_SCOPE)
  set(got "${owned}/${consumers}/${literals}/${verdict}")
  if(NOT "${got}" STREQUAL "${want}")
    math(EXPR failures "${selftest_failures} + 1")
    set(selftest_failures ${failures} PARENT_SCOPE)
    message("FALHOU caso ${case_name}: obtido=${got} esperado=${want}")
  endif()
endfunction()

# The thirteen controls, over values and texts built here (no file), each with its result fixed BEFORE the run.
function(measured_leak_filter_run_selftest)
  set(selftest_total 0)
  set(selftest_failures 0)
  measured_leak_filter_filter_case("1 o filtro decidido pega toda amostra e nenhuma linha limpa"
                                   "${GLINTFX_MEASURED_LEAK_FILTER}" "" "")
  measured_leak_filter_filter_case("2 o filtro antigo de espaco e tab perde os brancos Unicode"
                                   "(MEASURED|SCANCOUNT)[ \t]" "nbsp,em_space,unit_sep,scancount_nbsp" "")
  measured_leak_filter_filter_case("3 o filtro sem SCANCOUNT perde as linhas SCANCOUNT"
                                   "MEASURED[^A-Za-z0-9_]" "scancount_space,scancount_nbsp" "")
  measured_leak_filter_filter_case("4 um filtro que casa tudo pega as linhas limpas" "." "" "count_line,identifier")
  set(top "# registro sintetico\n")
  set(inc "include(\"\${CMAKE_CURRENT_SOURCE_DIR}/tools/measured_leak_filter.cmake\")\n")
  set(head_a "set_tests_properties(run_example_capture_win32_selftest PROPERTIES LABELS selftest\n")
  set(head_b "set_tests_properties(run_capture_known_color_win32_selftest PROPERTIES LABELS selftest\n")
  set(head_other "set_tests_properties(outro_selftest PROPERTIES LABELS selftest\n")
  set(use "    FAIL_REGULAR_EXPRESSION \"\${GLINTFX_MEASURED_LEAK_FILTER}\")\n")
  set(old "    FAIL_REGULAR_EXPRESSION \"(MEASURED|SCANCOUNT)< >\")\n")
  set(open_bracket "# nota [\n")
  set(both "${head_a}${use}${head_b}${use}")
  measured_leak_filter_registry_case("5 os dois donos pela variavel depois do include aprovam" "${top}${inc}${both}"
                                     "2/2/0/APROVA")
  measured_leak_filter_registry_case("6 o irmao de volta a forma escrita a mao reprova (MX-IRMAO-OLD)"
                                     "${top}${inc}${head_a}${use}${head_b}${old}" "1/1/1/REPROVA")
  measured_leak_filter_registry_case("7 o filtro migrado para um terceiro teste reprova, mesmo com 2 usos"
                                     "${top}${inc}${head_a}${use}${head_other}${use}" "1/2/0/REPROVA")
  measured_leak_filter_registry_case("8 o uso so num comentario nao conta"
                                     "${top}${inc}${head_a}${use}${head_b}# ${use}" "1/1/0/REPROVA")
  measured_leak_filter_registry_case("9 colchete aberto num comentario nao engole o dono seguinte"
                                     "${top}${inc}${head_a}${use}${open_bracket}${head_b}${use}" "2/2/0/APROVA")
  measured_leak_filter_registry_case("10 um uso a mais reprova: a contagem e exata"
                                     "${top}${inc}${both}${head_other}${use}" "2/3/0/REPROVA")
  measured_leak_filter_registry_case("11 registro vazio reprova" "" "0/0/0/REPROVA")
  measured_leak_filter_registry_case("12 um filtro escrito a mao num terceiro teste reprova"
                                     "${top}${inc}${both}${head_other}${old}" "2/2/1/REPROVA")
  measured_leak_filter_registry_case("13 os donos antes do include reprovam: a variavel ainda nao existe"
                                     "${top}${both}${inc}" "0/2/0/REPROVA")
  if(NOT "${selftest_total}" EQUAL 13 OR selftest_failures GREATER 0)
    message(FATAL_ERROR "measured_leak_filter --selftest: FALHOU ${selftest_failures} de ${selftest_total} casos")
  endif()
  message("measured_leak_filter --selftest: os ${selftest_total} controles OK")
endfunction()

if(SELFTEST)
  measured_leak_filter_run_selftest()
else()
  measured_leak_filter_run()
endif()
