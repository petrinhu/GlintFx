#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# test_name_inventory.py - CLAIM-CITATIONS sub-fatia E1
# (docs/plano-w7c.md sec. 3.E, decisoes D14/D15; docs/plano-w7c-adendo-
# revalidacao.md sec. 3.E). Fundacao de DOCS-COUNT-VOCAB (D2, "gramatica
# de citacao") e do proprio portao de citacao (E3, fora desta ordem):
# devolve o inventario FECHADO de nomes de teste que uma citacao "Proved
# by: <nome>" pode legitimamente citar neste projeto.
#
# TRES FONTES, NENHUMA REIMPLEMENTADA (GODS_LAWS.md L-33/L-17 - "um
# portao que mantem a propria lista curada so' muda o defeito de
# lugar" - a mesma razao de check_win32_test_link.py's own header):
#
#   1. add_test(NAME ...) e glintfx_add_test(nome) de tests/CMakeLists.
#      txt - reusa extract_add_test_blocks() de check_selftest_orphan.
#      py (E1b: agora ciente de comentario via cmake_lexer.strip_
#      comments - um nome que so' existe dentro de um comentario de
#      bloco #[[ ]] NAO entra no inventario).
#   2. Fixturas do container (tests/container/Containerfile) - reusa
#      parse_containerfile_fixtures() de check_container_fixture_
#      inventory.py.
#   3. Casos GLINTFX_TEST(nome) do harness proprio (GODS_LAWS.md L-07:
#      sem Catch2/GoogleTest) - varredura textual de todo *.cpp
#      rastreado, linha que comeca (depois de strip) por
#      "GLINTFX_TEST(nome)".
#
# GODS_LAWS.md L-40 (piso de varredura nao-vazia): inventario vazio
# reprova - e' sinal de leitor quebrado, nunca "nenhum teste existe".
#
# Usage:
#   test_name_inventory.py --check <repo-root>
#   test_name_inventory.py --selftest

import os
import re
import sys

import check_container_fixture_inventory
import check_selftest_orphan

SCRIPT_NAME = "test_name_inventory.py"

_NAME_KEYWORD_RE = re.compile(r"\bNAME\s+(\S+)")
_BARE_IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
_GLINTFX_TEST_LINE_RE = re.compile(r"^GLINTFX_TEST\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def read_text_lenient(path):
    try:
        with open(path, "r", encoding="utf-8", errors="surrogateescape") as handle:
            return handle.read()
    except OSError:
        return None


# --- fonte 1: tests/CMakeLists.txt (add_test(NAME ...) / glintfx_add_test(nome)) --


def names_from_cmake_add_test(cmake_text):
    """Reusa extract_add_test_blocks() (check_selftest_orphan.py) -
    ela ja' extrai o conteudo balanceado de cada chamada "add_test(",
    e "glintfx_add_test(" casa a MESMA busca porque "add_test(" e'
    substring literal de "glintfx_add_test(" (o bloco extraido comeca
    logo apos o "add_test(" interno, e' so' o nome nu). Duas formas de
    bloco:
      - "NAME <nome> COMMAND ..." (add_test explicito deste projeto -
        confirmado por leitura: todo add_test() daqui usa a forma com
        palavra-chave, nenhum usa a forma posicional antiga do CMake);
      - um identificador nu sozinho (glintfx_add_test(nome))."""
    names = set()
    for block_text in check_selftest_orphan.extract_add_test_blocks(cmake_text):
        stripped = block_text.strip()
        if not stripped:
            continue
        match = _NAME_KEYWORD_RE.search(stripped)
        if match:
            names.add(match.group(1).strip('"'))
        elif _BARE_IDENTIFIER_RE.match(stripped):
            names.add(stripped)
    return names


# --- fonte 3: GLINTFX_TEST(nome) em *.cpp -------------------------------


def names_from_glintfx_test_macro(cpp_text):
    """O regex e' ANCORADO (^GLINTFX_TEST\\() de proposito: uma linha
    de comentario sempre comeca por "//" ou "*" (dentro de um bloco
    /* */), entao ela nunca casa a ancora sozinha - uma mencao em
    prosa tipo "// GLINTFX_TEST(x) foi removido" fica de fora por
    construcao, sem precisar de um filtro de comentario separado (que
    seria codigo morto, nunca exercitado por mutante nenhum). O QUE
    ISTO NAO VE (GODS_LAWS.md L-40): uma linha DENTRO de um bloco
    /* ... */ que, apos strip(), comece literalmente por
    "GLINTFX_TEST(" sem nenhum "*"/"//" na frente - nenhum arquivo
    real deste repositorio faz isso hoje (GLINTFX_TEST so' aparece
    como macro de verdade, no estilo de invocacao do proprio
    test_registry.hpp)."""
    names = set()
    for raw_line in cpp_text.splitlines():
        match = _GLINTFX_TEST_LINE_RE.match(raw_line.strip())
        if match:
            names.add(match.group(1))
    return names


# --- montagem do inventario (funcao pura sobre texto ja' lido - o mesmo
# caminho de codigo roda em real_main e em --selftest) -----------------


def build_inventory_from_texts(cmake_text, containerfile_text, cpp_texts):
    """cpp_texts: dict[relpath] -> texto (ou None, ignorado)."""
    names = set()
    names |= names_from_cmake_add_test(cmake_text)
    if containerfile_text is not None:
        names |= check_container_fixture_inventory.parse_containerfile_fixtures(containerfile_text)
    for text in cpp_texts.values():
        if text is not None:
            names |= names_from_glintfx_test_macro(text)
    return names


# --- modo real -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: test_name_inventory.py --check <repo-root-directory>")
    root = args[0]

    cmake_path = os.path.join(root, "tests", "CMakeLists.txt")
    cmake_text = read_text_lenient(cmake_path)
    if cmake_text is None:
        fail(f"nao foi possivel ler '{cmake_path}' - o registro de ctest deste projeto")

    containerfile_path = os.path.join(root, "tests", "container", "Containerfile")
    containerfile_text = read_text_lenient(containerfile_path)
    if containerfile_text is None:
        fail(f"nao foi possivel ler '{containerfile_path}'")

    cpp_paths, ok = check_selftest_orphan.scanned_paths(root)
    if not ok:
        fail(f"'git ls-files' falhou em '{root}' (nao e repositorio git, ou git indisponivel)")
    cpp_paths = [p for p in cpp_paths if p.endswith(".cpp")]
    if not cpp_paths:
        fail("varredura vazia: 0 arquivo(s) .cpp rastreado(s) - GODS_LAWS.md L-40")
    cpp_texts = {p: read_text_lenient(os.path.join(root, p)) for p in cpp_paths}

    names = build_inventory_from_texts(cmake_text, containerfile_text, cpp_texts)

    print(
        f"{SCRIPT_NAME}: {len(cpp_paths)} arquivo(s) .cpp varrido(s), "
        f"{len(names)} nome(s) de teste no inventario (tests/CMakeLists.txt + "
        f"tests/container/Containerfile + GLINTFX_TEST)"
    )

    if not names:
        fail(
            "varredura vazia: inventario de nomes de teste ficou vazio - GODS_LAWS.md "
            "L-40, isto e sinal de leitor quebrado, nunca 'nenhum teste existe'"
        )

    return names


# --- controles do --selftest -------------------------------------------


_FIXTURE_CMAKE_TEXT = (
    "add_test(\n"
    "    NAME real_registrado_test\n"
    "    COMMAND real_tool --check\n"
    ")\n"
    "glintfx_add_test(bare_name_test)\n"
    "\n"
    "if(WIN32)\n"
    "add_test(\n"
    "    NAME win32_only_test\n"
    "    COMMAND win32_tool\n"
    ")\n"
    "endif()\n"
    "\n"
    "#[[\n"
    "add_test(\n"
    "    NAME fantasma_test\n"
    "    COMMAND nunca_existiu\n"
    ")\n"
    "]]\n"
)


def _selftest_inventoried_names_from_cmake():
    names = names_from_cmake_add_test(_FIXTURE_CMAKE_TEXT)
    if "nome_inventado_test" in names:
        print(f"selftest: CMAKE-NAMES FALHOU (nome que nunca existiu apareceu): {names}", file=sys.stderr)
        return False
    if "real_registrado_test" not in names or "bare_name_test" not in names:
        print(f"selftest: CMAKE-NAMES FALHOU (nomes reais nao apareceram): {names}", file=sys.stderr)
        return False
    if "win32_only_test" not in names:
        print(f"selftest: CMAKE-NAMES FALHOU (nome so' dentro de if(WIN32) deveria aparecer): {names}", file=sys.stderr)
        return False
    print(f"selftest: CMAKE-NAMES OK (nomes reais e de if(WIN32) presentes, nome inventado ausente): {sorted(names)}")
    return True


# VERMELHO da tabela E1: "registro dentro de #[[ ]] NAO entra no
# inventario" - exercita a consequencia direta da E1b (extract_add_
# test_blocks agora ciente de comentario via cmake_lexer).
# Guarda _BARE_IDENTIFIER_RE: um bloco sem a palavra-chave NAME so'
# vira nome quando e' um identificador NU sozinho (a forma real de
# glintfx_add_test(nome)) - nunca a forma posicional antiga do CMake
# (add_test(<nome> <comando> [<arg>...]), que nenhum add_test() deste
# projeto usa hoje, confirmado por leitura). Sem esta guarda, um bloco
# malformado ou uma forma futura sem NAME viraria um "nome de teste"
# com espaco dentro - lixo, nunca um nome real.
def _selftest_malformed_block_without_name_keyword_ignored():
    cmake_text = (
        "add_test(alvo_posicional comando_qualquer arg_extra)\n"
        "add_test(\n"
        "    NAME real_selftest\n"
        "    COMMAND real_tool\n"
        ")\n"
    )
    names = names_from_cmake_add_test(cmake_text)
    if "real_selftest" not in names:
        print(f"selftest: BLOCO-MALFORMADO FALHOU (nome real com NAME sumiu): {names}", file=sys.stderr)
        return False
    for name in names:
        if " " in name:
            print(f"selftest: BLOCO-MALFORMADO FALHOU (bloco sem NAME virou 'nome' com espaco): {names}", file=sys.stderr)
            return False
    print(f"selftest: BLOCO-MALFORMADO OK (forma posicional sem NAME nunca vira nome de teste): {sorted(names)}")
    return True


def _selftest_bracket_commented_registration_excluded():
    names = names_from_cmake_add_test(_FIXTURE_CMAKE_TEXT)
    if "fantasma_test" in names:
        print(f"selftest: COMENTARIO-DE-BLOCO FALHOU (registro dentro de #[[ ]] entrou no inventario): {names}", file=sys.stderr)
        return False
    print("selftest: COMENTARIO-DE-BLOCO OK (add_test dentro de #[[ ]] nunca entra no inventario)")
    return True


def _selftest_container_fixture_names_included():
    containerfile_text = (
        "FROM builder AS build\n"
        "COPY --from=build /out/some_fixture /usr/local/bin/some_fixture\n"
        "COPY run_compositor.sh /usr/local/bin/run_compositor.sh\n"
    )
    names = build_inventory_from_texts(_FIXTURE_CMAKE_TEXT, containerfile_text, {})
    if "some_fixture" not in names:
        print(f"selftest: CONTAINER-FIXTURES FALHOU (fixture do Containerfile nao apareceu): {names}", file=sys.stderr)
        return False
    if "run_compositor.sh" in names:
        print(f"selftest: CONTAINER-FIXTURES FALHOU (copia sem --from= nao e fixture, nao deveria aparecer): {names}", file=sys.stderr)
        return False
    print("selftest: CONTAINER-FIXTURES OK (fixture do Containerfile no inventario, infraestrutura sem --from= de fora)")
    return True


def _selftest_glintfx_test_macro_names_included():
    cpp_text = (
        "// comentario que menciona GLINTFX_TEST(nao_e_caso) em prosa\n"
        "GLINTFX_TEST(caso_real_um) {\n"
        "}\n"
        "GLINTFX_TEST(caso_real_dois) {\n"
        "}\n"
    )
    names = names_from_glintfx_test_macro(cpp_text)
    if "nao_e_caso" in names:
        print(f"selftest: GLINTFX-TEST-MACRO FALHOU (mencao em comentario virou caso): {names}", file=sys.stderr)
        return False
    if names != {"caso_real_um", "caso_real_dois"}:
        print(f"selftest: GLINTFX-TEST-MACRO FALHOU (casos reais nao batem): {names}", file=sys.stderr)
        return False
    print(f"selftest: GLINTFX-TEST-MACRO OK: {sorted(names)}")
    return True


# VERMELHO (piso de varredura vazia, GODS_LAWS.md L-40): as tres fontes
# vazias tem que devolver inventario vazio - o piso em si (real_main
# reprovando) se prova por leitura do codigo, aqui prova-se que a
# funcao pura nao inventa nome do nada.
def _selftest_all_sources_empty_yields_empty_inventory():
    empty_cmake_text = "#[[\nnada aqui e' codigo\n]]\n"
    names = build_inventory_from_texts(empty_cmake_text, None, {})
    if names:
        print(f"selftest: TUDO-VAZIO FALHOU (deveria devolver conjunto vazio): {names}", file=sys.stderr)
        return False
    print("selftest: TUDO-VAZIO OK (sem add_test real, sem Containerfile, sem .cpp - inventario vazio)")
    return True


# MEDIDO, NUNCA SUPOSTO: roda o inventario de verdade contra o
# working tree real deste repositorio (mesmo caminho de codigo de
# real_main), provando que a fundacao nao esta vazia na arvore real -
# mesma convencao de check_selftest_orphan.py's own selftest_measured_
# false_positive_count_on_real_tree(). Pula, declarado e contado, se
# git nao funcionar neste ambiente.
def _selftest_measured_inventory_on_real_tree():
    here = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(here, "..", ".."))
    cmake_path = os.path.join(repo_root, "tests", "CMakeLists.txt")
    cmake_text = read_text_lenient(cmake_path)
    if cmake_text is None:
        print("selftest: MEDIDO-ARVORE-REAL PULADO (tests/CMakeLists.txt indisponivel aqui - declarado, nao calado)")
        return True
    containerfile_text = read_text_lenient(os.path.join(repo_root, "tests", "container", "Containerfile"))
    cpp_paths, ok = check_selftest_orphan.scanned_paths(repo_root)
    if not ok:
        print("selftest: MEDIDO-ARVORE-REAL PULADO (git ls-files indisponivel aqui - declarado, nao calado)")
        return True
    cpp_paths = [p for p in cpp_paths if p.endswith(".cpp")]
    cpp_texts = {p: read_text_lenient(os.path.join(repo_root, p)) for p in cpp_paths}
    names = build_inventory_from_texts(cmake_text, containerfile_text, cpp_texts)
    print(
        f"selftest: MEDIDO-ARVORE-REAL: {len(cpp_paths)} arquivo(s) .cpp, "
        f"{len(names)} nome(s) de teste no inventario real"
    )
    if not names:
        print("selftest: MEDIDO-ARVORE-REAL FALHOU (inventario vazio contra a arvore real)", file=sys.stderr)
        return False
    return True


def selftest_main():
    controls = [
        _selftest_inventoried_names_from_cmake(),
        _selftest_malformed_block_without_name_keyword_ignored(),
        _selftest_bracket_commented_registration_excluded(),
        _selftest_container_fixture_names_included(),
        _selftest_glintfx_test_macro_names_included(),
        _selftest_all_sources_empty_yields_empty_inventory(),
        _selftest_measured_inventory_on_real_tree(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--check":
        real_main(args[1:])
    else:
        fail("usage: test_name_inventory.py --check <repo-root-directory>  |  --selftest")


if __name__ == "__main__":
    main()
