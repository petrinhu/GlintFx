# SPDX-License-Identifier: AGPL-3.0-or-later
#
# draw2d_include_selftest.py - the controls of the draw2d direction gate's selftest (CONTRACT.md 5.3). Seventeen
# controls in four groups; each one fixes its verdict before the run, and each refusal control checks the exit code
# AND the phrase, never the exit code alone. The fixtures live in draw2d_include_fixtures.py.
#
# Standard library only (L-07). Identifiers and comments in English (L-21).

from selftest_controls import CHECKS, check
from draw2d_include_build_log import parse_ninja_deps
from draw2d_include_fixtures import (
    PROBE_OUTPUT,
    SOURCE_OUTPUT,
    base_case,
    canned_text,
    deps_block,
    in_fixture,
    probe_entry,
    probe_reaches,
    pub_deps,
    short_count_case,
    source_entry,
    source_reaches,
    src_deps,
    without_output,
)
from draw2d_include_judges import PROBE_LABEL
from draw2d_include_paths import is_forbidden, normalize_path


def selftest_directions():
    """Controls 1 to 6: the clean tree passes, each shape of gfss reaching draw2d is refused, look-alikes pass."""
    rc, out, _ = in_fixture(base_case)
    check("positivo: arvore limpa passa com violacoes=0", rc == 0 and "analisados=3 violacoes=0" in out)
    rc, _, err = in_fixture(source_reaches(lambda root: [f"{root}/include/glintfx/gfss/token.hpp"]))
    check("direto: cabecalho de gfss no fecho de um objeto reprova",
          rc == 1 and "VIOLACAO (CONTRACT.md 5.3): src/draw2d/a.cpp pelo objeto" in err and "gfss/token.hpp" in err)
    rc, _, err = in_fixture(source_reaches(lambda root: [f"{root}/include/glintfx/core/rect.hpp",
                                                        f"{root}/include/glintfx/gfss/token.hpp"]))
    check("indireto: core/rect.hpp e depois gfss/token.hpp no fecho reprova",
          rc == 1 and "VIOLACAO (CONTRACT.md 5.3): src/draw2d/a.cpp" in err)
    rc, _, err = in_fixture(source_reaches(lambda root: [f"{root}/src/gfss/image_value_read.hpp"]))
    check("interno: src/gfss no fecho reprova", rc == 1 and "VIOLACAO (CONTRACT.md 5.3): src/draw2d/a.cpp" in err)
    windows_dep = normalize_path(r"D:\A\Include\GlintFX\GFSS\token.hpp", "/")
    table, problems = parse_ninja_deps(r"src\CMakeFiles\glintfx.dir\draw2d\a.cpp.obj: #deps 0, deps mtime 1 (VALID)"
                                       "\n\n", r"D:\A\build")
    key = normalize_path("src/CMakeFiles/glintfx.dir/draw2d/a.cpp.obj", "D:/A/build")
    same_source = normalize_path("src/draw2d/a.cpp", r"D:\A") == normalize_path(r"D:\A\src\draw2d\a.cpp", "/")
    check("windows: barra invertida no caminho e na base, letra de unidade e caixa alta",
          is_forbidden(windows_dep, r"D:\A") and key in table and not problems and same_source)
    check("vizinho: glintfx/gfss_extra e glintfx/gfssx nao sao gfss",
          not is_forbidden("/x/include/glintfx/gfss_extra/token.hpp", "/x")
          and not is_forbidden("/x/include/glintfx/gfssx/token.hpp", "/x"))


def selftest_objects():
    """Controls 7 to 11: a source without object, an entry without output, and the three bad object states."""
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(entries=[probe_entry(root)]))
    check("fonte de draw2d sem objeto reprova",
          rc == 1 and "fonte de draw2d sem objeto no compile_commands: src/draw2d/a.cpp" in err)
    rc, _, err = in_fixture(without_output)
    check("entrada do compile_commands sem output reprova", rc == 1 and "sem output" in err)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(
        text=deps_block(PROBE_OUTPUT, "VALID", pub_deps(root))))
    check("objeto sem entrada no log de dependencias reprova",
          rc == 1 and f"objeto sem entrada no log de dependencias: {SOURCE_OUTPUT}" in err)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(
        text=deps_block(SOURCE_OUTPUT, "STALE", src_deps(root)) + deps_block(PROBE_OUTPUT, "VALID", pub_deps(root))))
    check("entrada STALE reprova", rc == 1 and f"entrada STALE no log de dependencias: {SOURCE_OUTPUT}" in err)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(
        text=deps_block(SOURCE_OUTPUT, "VALID", []) + deps_block(PROBE_OUTPUT, "VALID", pub_deps(root))))
    check("entrada com zero dependencias reprova", rc == 1 and "entrada com zero dependencias" in err)


def selftest_headers_scan():
    """Controls 12 to 14: a header outside every closure, an empty sweep, and an extension outside the closed list."""
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(
        text=canned_text(src_deps(root), [f"{root}/src/draw2d/a.cpp"])))
    check("cabecalho de draw2d fora do fecho reprova",
          rc == 1 and "include/glintfx/draw2d/pub.hpp (inclua-o no .cpp que o implementa)" in err)
    rc, out, err = in_fixture(lambda root: base_case(root)._replace(tree=False, text="", entries=[]))
    check("varredura vazia reprova", rc == 1 and "analisados=0" in out and "varredura vazia" in err)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(files=("src/draw2d/notas.txt",)))
    check("extensao fora da lista reprova",
          rc == 1 and "extensao fora da lista, arquivo nao julgavel: src/draw2d/notas.txt" in err)


def selftest_format_generator_probe():
    """Controls 15 to 17: the format of the log, the generator name, and the probe as a root."""
    rc_a, _, err_a = in_fixture(lambda root: base_case(root)._replace(
        text="sem cabecalho nem recuo\n" + canned_text(src_deps(root), pub_deps(root))))
    rc_b, _, err_b = in_fixture(short_count_case)
    check("linha fora do formato do ninja reprova",
          rc_a == 1 and "linha fora do formato do ninja: sem cabecalho" in err_a
          and rc_b == 1 and "linha fora do formato do ninja: #deps 2" in err_b)
    rc, _, err = in_fixture(lambda root: base_case(root)._replace(generator="Unix Makefiles"))
    check("gerador que nao e Ninja reprova nomeando o gerador",
          rc == 1 and "o portao exige o gerador Ninja; este build usa Unix Makefiles" in err)
    rc_c, _, err_c = in_fixture(lambda root: base_case(root)._replace(entries=[source_entry(root)]))
    rc_d, _, err_d = in_fixture(probe_reaches(lambda root: [f"{root}/include/glintfx/gfss/token.hpp"]))
    check("sonda sem objeto ou com gfss reprova",
          rc_c == 1 and "sonda ausente do compile_commands" in err_c
          and rc_d == 1 and f"VIOLACAO (CONTRACT.md 5.3): {PROBE_LABEL} pelo objeto" in err_d)


def run_selftest():
    """Run the four groups of controls; the count line is the one the ctest registration checks."""
    selftest_directions()
    selftest_objects()
    selftest_headers_scan()
    selftest_format_generator_probe()
    print(f"selftest: {len(CHECKS)} controles OK")
    return 0
