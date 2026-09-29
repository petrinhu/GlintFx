#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_ignored_fixtures.py - CI-SPLIT-PER-OS A5 etapa 3 (achado do CTO, 29/09/2026,
# L-17 gemeo, L-36): o preci local roda sobre a WORKING TREE e o CI sobre o BLOB
# commitado. Uma fixture que o `.gitignore` engole (foi o caso: `build-shared/`
# casa `build-*/` sob tests/tools/fixtures/p1_rounds/) existe em disco, o preci
# passa, e o CI reprova em toda perna por "arquivo ausente". Este portao le o
# que o git IGNORA sob as pastas de fixture e reprova se houver alguma:
#
#     git ls-files -o -i --exclude-standard -- tests/
#
# Pasta de fixture = qualquer componente de caminho `fixtures`, `fixture`,
# `preci_fixtures` ou que comece por `fixtures_`. Fora delas o git pode ignorar
# a vontade (`__pycache__`, `tests/container/_arch_ports_src/`, regenerados).
# So' e' util onde o arquivo ignorado EXISTE (o preci local); no CI a arvore
# clonada nao tem ignorado nenhum e o portao passa de forma vazia (o proprio
# teste que le a fixture falha, por outra razao).
#
# Piso de varredura (L-40): imprime SEMPRE quantos arquivos ignorados o git ve sob
# tests/, quantos caem em pasta de fixture (tem de ser 0) e quantos arquivos de
# fixture estao RASTREADOS (piso > 0).
#
# Usage:
#   check_ignored_fixtures.py <raiz-do-repo>
#   check_ignored_fixtures.py --selftest

import os
import re
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_ignored_fixtures.py"
_FIXTURE_COMPONENT_RE = re.compile(r"^(?:fixtures?|preci_fixtures|fixtures_.*)$")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def git_lines(root, *args):
    try:
        saida = subprocess.run(["git", "-C", root, *args], capture_output=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError) as exc:
        fail(f"git {' '.join(args)} falhou em {root} ({exc}) - varredura recusada, nunca presumida vazia")
    return [p.decode("utf-8", "surrogateescape") for p in saida.split(b"\0") if p]


def in_fixture_dir(path):
    return any(_FIXTURE_COMPONENT_RE.match(c) for c in path.split("/")[:-1])


def scan(root):
    """(ignorados_sob_tests, ignorados_em_fixture, fixtures_rastreadas)."""
    ignorados = git_lines(root, "ls-files", "-z", "-o", "-i", "--exclude-standard", "--", "tests/")
    rastreados = git_lines(root, "ls-files", "-z", "--", "tests/")
    return ignorados, [p for p in ignorados if in_fixture_dir(p)], [p for p in rastreados if in_fixture_dir(p)]


def run(root):
    ignorados, ruins, rastreadas = scan(root)
    print(
        f"{SCRIPT_NAME}: {len(ignorados)} arquivo(s) ignorado(s) sob tests/, {len(ruins)} em pasta de "
        f"fixture (tem de ser 0), {len(rastreadas)} arquivo(s) de fixture rastreado(s)"
    )
    problemas = [
        f"{p}: fixture IGNORADA pelo .gitignore - existe em disco (o preci passa) mas nao entra no commit (o CI reprova)"
        for p in ruins
    ]
    if not rastreadas:
        problemas.append("varredura vazia: nenhum arquivo de fixture rastreado sob tests/ - L-40, coleta quebrada")
    for problema in problemas:
        print(f"{SCRIPT_NAME}: {problema}", file=sys.stderr)
    return 1 if problemas else 0


# --- selftest: repositorios git descartaveis ---------------------------


def _sh(cwd, *args):
    subprocess.run(args, cwd=cwd, check=True, capture_output=True)


def _repo(files, gitignore="build-*/\n"):
    tmp = tempfile.mkdtemp(prefix="glintfx-ignfix-")
    _sh(tmp, "git", "init", "-q")
    with open(os.path.join(tmp, ".gitignore"), "w", encoding="utf-8") as handle:
        handle.write(gitignore)
    for rel, tracked in files:
        caminho = os.path.join(tmp, rel)
        os.makedirs(os.path.dirname(caminho), exist_ok=True)
        with open(caminho, "w", encoding="utf-8") as handle:
            handle.write("x\n")
        if tracked:
            _sh(tmp, "git", "add", "-f", "--", rel)
    return tmp


def _rc(root):
    import contextlib
    import io
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        rc = run(root)
    return rc, buffer.getvalue()


def _check(nome, condicao, detalhe=""):
    print(f"selftest: {nome} {'OK' if condicao else 'FALHOU'}" + ("" if condicao else f" - {detalhe}"))
    return condicao


def selftest_main():
    controles = []
    limpo = _repo([("tests/tools/fixtures/a/dados.json", True), ("tests/tools/__pycache__/x.pyc", False)])
    rc, saida = _rc(limpo)
    controles.append(_check("POSITIVO (fixture rastreada; __pycache__ ignorado fora de fixture nao conta)", rc == 0, saida))
    # O defeito real: tests/tools/fixtures/p1_rounds/.../build-shared/ engolido por `build-*/`.
    ruim = _repo([("tests/tools/fixtures/p1/r0/show.json", True), ("tests/tools/fixtures/p1/r0/build-shared/junit.xml", False)])
    rc, saida = _rc(ruim)
    controles.append(_check("NEGATIVO (build-shared/ sob fixtures ignorado) reprova, citando o arquivo",
                            rc == 1 and "build-shared/junit.xml" in saida, saida))
    ruim2 = _repo([("tests/tools/fixtures/ok.json", True), ("tests/preci_fixtures/dirty/CMakeCache.txt", False)], gitignore="CMakeCache.txt\n")
    rc, saida = _rc(ruim2)
    controles.append(_check("NEGATIVO 2 (preci_fixtures/, ignorado por outro padrao) reprova", rc == 1 and "CMakeCache.txt" in saida, saida))
    vazio = _repo([("tests/tools/outra/coisa.py", True)])
    rc, saida = _rc(vazio)
    controles.append(_check("PISO: nenhuma fixture rastreada reprova (varredura vazia)", rc == 1 and "varredura vazia" in saida, saida))
    controles.append(_check("componente `fixtures_x` conta como pasta de fixture", in_fixture_dir("tests/fixtures_x/a.txt")))
    controles.append(_check("componente parecido (`myfixtures`) NAO conta", not in_fixture_dir("tests/myfixtures/a.txt")))
    if not all(controles):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controles)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif len(args) == 1:
        sys.exit(run(args[0]))
    else:
        fail("usage: check_ignored_fixtures.py <raiz-do-repo>  |  --selftest")


if __name__ == "__main__":
    main()
