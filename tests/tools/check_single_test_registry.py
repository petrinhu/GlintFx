#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_single_test_registry.py - DEMO-1 D1-fix-a (D-W8-91, achado I-3 da revisao
# da D1): tests/CMakeLists.txt e' o UNICO registro de teste deste projeto. Dezenas
# de ferramentas leem aquele arquivo por TEXTO e tratam-no como o registro inteiro
# (check_selftest_orphan.py, check_ctest_parallel_policy.py, blob_selftests.py,
# entre outras). Um teste registrado em OUTRO arquivo (um `*.cmake` incluido por
# `include()`, um `CMakeLists.txt` de subdiretorio) existe para o ctest mas e'
# invisivel para elas: escapa do portao de corrida entre testes e vira falso orfao.
# Comentario ("NOTE for the next editor") nao e' portao (L-40); este e'.
#
# O QUE FAZ: varre todo arquivo `CMakeLists.txt` e `*.cmake` do repositorio (git
# ls-files, rastreados e nao rastreados nao ignorados) e reprova se algum, fora das
# exclusoes abaixo, tiver uma chamada de CODIGO (comentario nao conta, pelo mesmo
# lexico dos outros portoes: cmake_lexer.strip_comments) a `add_test(` ou a
# `glintfx_add_test(`. A mensagem nomeia arquivo, linha e nome do teste.
#
# EXCLUSOES, cada uma impressa com o motivo (fail-closed: o que nao esta aqui nao
# e' excluido):
#   1. tests/CMakeLists.txt - e' o registro.
#   2. cmake/GlintfxTest.cmake - SO' o `add_test` que esta dentro da DEFINICAO de
#      `function(glintfx_add_test ...)`; qualquer outro naquele arquivo reprova.
#   3. projeto CMake INDEPENDENTE: arquivo cujo diretorio (ou ancestral) tem um
#      CMakeLists.txt com chamada `project(` em codigo. O teste de um projeto
#      assim roda no cmake aninhado dele, nunca no ctest principal. O
#      CMakeLists.txt da RAIZ tem `project(` por ser o projeto principal e NAO e'
#      excluido por isso.
#
# Piso de varredura (L-40): imprime SEMPRE `arquivos_varridos=<n> excluidos=<e>
# registros_fora=<k>`; n == 0 reprova; a ausencia de tests/CMakeLists.txt no
# universo tambem (registro sumido nao e' "nenhum registro fora").
#
# Usage:
#   check_single_test_registry.py <raiz-do-repo>
#   check_single_test_registry.py --selftest

import contextlib
import io
import os
import re
import subprocess
import sys
import tempfile

import cmake_lexer

SCRIPT_NAME = "check_single_test_registry.py"
REGISTRY = "tests/CMakeLists.txt"
DEFINITION_FILE = "cmake/GlintfxTest.cmake"

_UNIVERSE_RE = re.compile(r"(?:^|/)CMakeLists\.txt$|\.cmake$")
_REGISTER_RE = re.compile(r"^[ \t]*(add_test|glintfx_add_test)[ \t]*\(", re.I | re.M)
_PROJECT_RE = re.compile(r"^[ \t]*project[ \t]*\(", re.I | re.M)
_FUNC_OPEN_RE = re.compile(r"^[ \t]*function[ \t]*\([ \t]*glintfx_add_test\b", re.I | re.M)
_FUNC_CLOSE_RE = re.compile(r"^[ \t]*endfunction[ \t]*\(", re.I | re.M)
_FIRST_ARG_RE = re.compile(r"\(\s*(?:NAME\s+)?([^\s)]+)", re.I)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def git_universe(root):
    """Caminhos CMake do repositorio: rastreados mais nao rastreados nao ignorados
    (mesma convencao de check_spdx.py), para o preci local ver o arquivo novo."""
    try:
        saida = subprocess.run(
            ["git", "-C", root, "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
            capture_output=True, check=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError) as exc:
        fail(f"git ls-files falhou em {root} ({exc}) - varredura recusada, nunca presumida vazia")
    todos = {p.decode("utf-8", "surrogateescape") for p in saida.split(b"\0") if p}
    return sorted(p for p in todos if _UNIVERSE_RE.search(p))


def code_of(text):
    """Texto sem comentario, mesmas linhas. Arquivo vazio nao tem codigo."""
    return cmake_lexer.strip_comments(text) if text.strip() else ""


def line_of(code, offset):
    return code.count("\n", 0, offset) + 1


def definition_ranges(code):
    """(inicio, fim) de cada function(glintfx_add_test ...) ... endfunction()."""
    ranges = []
    for abre in _FUNC_OPEN_RE.finditer(code):
        fecha = _FUNC_CLOSE_RE.search(code, abre.end())
        ranges.append((abre.start(), fecha.end() if fecha else len(code)))
    return ranges


def find_registrations(code):
    """[(offset, linha, comando, nome_do_teste)] de cada chamada de codigo."""
    achados = []
    for achado in _REGISTER_RE.finditer(code):
        primeiro = _FIRST_ARG_RE.match(code, achado.end() - 1)
        nome = primeiro.group(1) if primeiro else "?"
        achados.append((achado.start(), line_of(code, achado.start()), achado.group(1).lower(), nome))
    return achados


def independent_dirs(codes):
    """Diretorios (nao a raiz) cujo CMakeLists.txt tem `project(` em codigo."""
    dirs = set()
    for path, code in codes.items():
        if os.path.basename(path) == "CMakeLists.txt" and os.path.dirname(path) and _PROJECT_RE.search(code):
            dirs.add(os.path.dirname(path))
    return dirs


def owning_project(path, dirs):
    atual = os.path.dirname(path)
    while atual:
        if atual in dirs:
            return atual
        atual = os.path.dirname(atual)
    return None


def evaluate(root, paths):
    """(varridos, excluidos[(caminho, motivo)], isentos[str], violacoes[str], erros[str])."""
    codes, erros = {}, []
    for path in paths:
        try:
            with open(os.path.join(root, path), encoding="utf-8", errors="replace") as handle:
                codes[path] = code_of(handle.read())
        except OSError as exc:
            erros.append(f"{path}: ilegivel ({exc}) - varredura recusada")
    dirs = independent_dirs(codes)
    excluidos, isentos, violacoes = [], [], []
    for path in sorted(codes):
        code = codes[path]
        if path == REGISTRY:
            excluidos.append((path, "e' o registro unico de teste"))
            continue
        dono = owning_project(path, dirs)
        if dono:
            excluidos.append((path, f"projeto CMake independente ({dono}/CMakeLists.txt tem project())"))
            continue
        ranges = definition_ranges(code) if path == DEFINITION_FILE else []
        for offset, linha, comando, nome in find_registrations(code):
            if comando == "add_test" and any(a <= offset < b for a, b in ranges):
                isentos.append(f"{path}:{linha}: add_test({nome}) dentro da definicao de function(glintfx_add_test)")
                continue
            violacoes.append(f"{path}:{linha}: {comando}({nome}) fora de {REGISTRY} - o teste escapa dos portoes que leem o registro por texto")
    return len(codes), excluidos, isentos, violacoes, erros


def report(root, paths):
    varridos, excluidos, isentos, violacoes, erros = evaluate(root, paths)
    print(f"{SCRIPT_NAME}: arquivos_varridos={varridos} excluidos={len(excluidos)} registros_fora={len(violacoes)}")
    for caminho, motivo in excluidos:
        print(f"{SCRIPT_NAME}: excluido {caminho} - {motivo}")
    for item in isentos:
        print(f"{SCRIPT_NAME}: isento {item}")
    problemas = list(erros) + list(violacoes)
    if varridos == 0:
        problemas.append("varredura vazia: nenhum CMakeLists.txt/*.cmake no universo - L-40, coleta quebrada")
    elif not any(c == REGISTRY for c, _ in excluidos):
        problemas.append(f"{REGISTRY} ausente do universo - o registro unico sumiu ou a coleta esta quebrada")
    for problema in problemas:
        print(f"{SCRIPT_NAME}: {problema}", file=sys.stderr)
    return 1 if problemas else 0


def run(root):
    return report(root, git_universe(root))


# --- selftest: arvores de fixture descartaveis, sem git -----------------

_REGISTRY_OK = "add_test(NAME a_test COMMAND a)\n"
_DEF_OK = (
    "function(glintfx_add_test name)\n"
    "    add_executable(${name} ${name}.cpp)\n"
    "    add_test(NAME ${name} COMMAND ${name})\n"
    "endfunction()\n"
)


def _tree(files):
    tmp = tempfile.mkdtemp(prefix="glintfx-singlereg-")
    for rel, conteudo in files.items():
        caminho = os.path.join(tmp, rel)
        os.makedirs(os.path.dirname(caminho), exist_ok=True)
        with open(caminho, "w", encoding="utf-8") as handle:
            handle.write(conteudo)
    return tmp, sorted(files)


def _rc(files):
    tmp, paths = _tree(files)
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        rc = report(tmp, paths)
    return rc, buffer.getvalue()


def _check(nome, condicao, detalhe=""):
    print(f"selftest: {nome} {'OK' if condicao else 'FALHOU'}" + ("" if condicao else f" - {detalhe}"))
    return condicao


def _base(**extra):
    arvore = {"CMakeLists.txt": "project(x)\n", REGISTRY: _REGISTRY_OK, DEFINITION_FILE: _DEF_OK}
    arvore.update(extra)
    return arvore


def selftest_main():
    c = []
    rc, saida = _rc(_base())
    c.append(_check("POSITIVO (registro so' no lugar certo; definicao isenta; raiz com project() varrida) passa, linha de contagem exata",
                    rc == 0 and "arquivos_varridos=3 excluidos=1 registros_fora=0" in saida and "isento cmake/GlintfxTest.cmake:3" in saida, saida))

    rc, saida = _rc(_base(**{"tests/extra.cmake": "\n\nadd_test(\n    NAME fora_test\n    COMMAND x\n)\n"}))
    c.append(_check("NEGATIVO: add_test num *.cmake incluido reprova, nomeando arquivo, linha e teste",
                    rc == 1 and "tests/extra.cmake:3: add_test(fora_test)" in saida and "registros_fora=1" in saida, saida))

    rc, saida = _rc(_base(**{"src/CMakeLists.txt": "glintfx_add_test(solto_test)\n"}))
    c.append(_check("NEGATIVO: glintfx_add_test fora do registro reprova",
                    rc == 1 and "src/CMakeLists.txt:1: glintfx_add_test(solto_test)" in saida, saida))

    rc, saida = _rc(_base(**{"tests/extra.cmake": "ADD_TEST(NAME maiusculo_test COMMAND x)\n"}))
    c.append(_check("NEGATIVO: comando em maiuscula (CMake nao diferencia) reprova", rc == 1 and "maiusculo_test" in saida, saida))

    rc, saida = _rc({})
    c.append(_check("PISO: universo vazio reprova", rc == 1 and "varredura vazia" in saida and "arquivos_varridos=0" in saida, saida))

    rc, saida = _rc({"CMakeLists.txt": "project(x)\n", "src/CMakeLists.txt": "# nada\n"})
    c.append(_check("PISO: universo sem tests/CMakeLists.txt reprova", rc == 1 and "ausente do universo" in saida, saida))

    rc, saida = _rc(_base(**{"tests/c.cmake": "# add_test(NAME comentado_test COMMAND x)\n#[[ add_test(NAME bloco_test COMMAND x) ]]\n"}))
    c.append(_check("EXCLUSAO: add_test em comentario (de linha e de bloco) passa",
                    rc == 0 and "registros_fora=0" in saida, saida))

    rc, saida = _rc(_base(**{"tests/ind/CMakeLists.txt": "project(ind)\nadd_test(NAME ind_test COMMAND x)\n",
                             "tests/ind/helper.cmake": "add_test(NAME ind2_test COMMAND x)\n"}))
    c.append(_check("EXCLUSAO: add_test num projeto independente (project()) e no *.cmake dele passa, com motivo impresso",
                    rc == 0 and "excluido tests/ind/CMakeLists.txt - projeto CMake independente" in saida
                    and "excluido tests/ind/helper.cmake" in saida and "excluidos=3" in saida, saida))

    rc, saida = _rc(_base(**{"tests/ind/CMakeLists.txt": "# project(so_comentario)\nadd_test(NAME x_test COMMAND x)\n"}))
    c.append(_check("EXCLUSAO: project() so' em comentario NAO torna o projeto independente",
                    rc == 1 and "tests/ind/CMakeLists.txt:2" in saida, saida))

    arvore = _base()
    arvore["CMakeLists.txt"] = "project(x)\nadd_test(NAME raiz_test COMMAND x)\n"
    rc, saida = _rc(arvore)
    c.append(_check("EXCLUSAO: o CMakeLists.txt da raiz tem project() e MESMO ASSIM e' varrido", rc == 1 and "CMakeLists.txt:2: add_test(raiz_test)" in saida, saida))

    arvore = _base()
    arvore[DEFINITION_FILE] = _DEF_OK + "add_test(NAME solto_na_definicao_test COMMAND x)\n"
    rc, saida = _rc(arvore)
    c.append(_check("EXCLUSAO: add_test FORA da function em cmake/GlintfxTest.cmake reprova",
                    rc == 1 and "solto_na_definicao_test" in saida and "isento cmake/GlintfxTest.cmake:3" in saida, saida))

    rc, saida = _rc(_base(**{"cmake/Outro.cmake": _DEF_OK}))
    c.append(_check("EXCLUSAO: a isencao da definicao vale so' para cmake/GlintfxTest.cmake", rc == 1 and "cmake/Outro.cmake:3" in saida, saida))

    if not all(c):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(c)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif len(args) == 1:
        sys.exit(run(args[0]))
    else:
        fail("usage: check_single_test_registry.py <raiz-do-repo>  |  --selftest")


if __name__ == "__main__":
    main()
