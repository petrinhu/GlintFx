#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# blob_selftests.py - CI-SPLIT-PER-OS A5 etapa 3 (decisao do CTO, 29/09/2026):
# roda os `--selftest` Python sobre o ARQUIVO COMMITAVEL (o indice do git, extraido
# em outro diretorio) e nao sobre a working tree. Fecha a CLASSE do achado das
# fixtures do P1: o preci local roda sobre a working tree (onde existem arquivos
# ignorados, nao adicionados ou gerados), e o CI roda sobre o blob commitado.
#
# O UNIVERSO e' LIDO de tests/CMakeLists.txt do proprio blob, nunca de lista a mao:
# todo `add_test` com `LABELS selftest` cujo COMMAND e' `"${GLINTFX_PYTHON3_EXECUTABLE}"
# "<script>" --selftest`. Ficam de fora os scripts que chamam cmake, docker ou pwsh
# (texto com `"cmake"`, `"docker"` ou `"pwsh"` entre aspas): sao pesados e/ou dependem
# de ferramenta que o espelho local ja cobre no ctest de verdade.
#
# Imprime SEMPRE "N achados, R rodados, falharam K" (L-40) e o tempo. Reprova se algum
# falhar, se nada for achado, ou se rodados != achados - excluidos.
#
# Usage:
#   blob_selftests.py --root <raiz-extraida-do-indice>
#   blob_selftests.py --selftest

import os
import re
import subprocess
import sys
import tempfile
import time

SCRIPT_NAME = "blob_selftests.py"
PER_SCRIPT_TIMEOUT_S = 180
_HEAVY_RE = re.compile(r"[\"'](?:cmake|docker|pwsh)[\"']")
_COMMAND_RE = re.compile(r'"\$\{GLINTFX_PYTHON3_EXECUTABLE\}"\s+"([^"]+)"[^)]*?--selftest', re.DOTALL)


def _block(text, start):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    return text[start:]


def registered_selftests(cmake_text):
    """[(nome, caminho-relativo-a-raiz)] dos add_test `LABELS selftest` que rodam
    `python <script> --selftest`; e o total de selftests registrados."""
    props = {}
    for m in re.finditer(r"set_tests_properties\s*\(", cmake_text):
        bloco = _block(cmake_text, m.start())
        nome = re.match(r"set_tests_properties\s*\(\s*(\S+)", bloco)
        if nome:
            props[nome.group(1)] = props.get(nome.group(1), "") + bloco
    total, python = 0, []
    for m in re.finditer(r"add_test\s*\(", cmake_text):
        bloco = _block(cmake_text, m.start())
        nome = re.search(r"NAME\s+(\S+)", bloco)
        if not nome or not re.search(r"LABELS\s+selftest\b", props.get(nome.group(1), "")):
            continue
        total += 1
        cmd = _COMMAND_RE.search(bloco)
        if cmd:
            caminho = cmd.group(1).replace("${CMAKE_CURRENT_SOURCE_DIR}", "tests").replace("${PROJECT_SOURCE_DIR}", ".")
            python.append((nome.group(1), os.path.normpath(caminho)))
    return total, python


def preci_blob_errors(preci_text):
    """`run_full_pipeline` (o que --fast e o modo vazio despacham) chama stage_blob: a
    decisao do CTO e' que o --blob entre no --fast por padrao. Achado real: a primeira
    insercao caiu em run_lint_only e ficou 'entregue' sem rodar nunca."""
    m = re.search(r"^run_full_pipeline\(\) \{\n(.*?)^\}\n", preci_text, re.MULTILINE | re.DOTALL)
    if not m:
        return ["run_full_pipeline nao encontrado em tools/preci.sh - varredura vazia (L-40)"]
    if not re.search(r"^\s+stage_blob\s*$", m.group(1), re.MULTILINE):
        return ["run_full_pipeline (o --fast e o modo vazio) nao chama stage_blob - o estagio --blob nao roda no espelho local"]
    return []


def is_heavy(script_text):
    return bool(_HEAVY_RE.search(script_text))


def ensure_git(root):
    """O blob e' uma COPIA do indice sem .git, e muitos selftests chamam `git ls-files`
    (como no CI, que roda num checkout). Vira um repositorio git descartavel com tudo
    adicionado - so' os arquivos do blob, nenhum ignorado ou nao rastreado do original."""
    if os.path.isdir(os.path.join(root, ".git")):
        return
    for args in (["init", "-q"], ["add", "-A", "-f"]):
        subprocess.run(["git", "-C", root, *args], check=True, capture_output=True)


def run_all(root):
    """(rc, linhas)."""
    cmake_path = os.path.join(root, "tests", "CMakeLists.txt")
    if not os.path.isfile(cmake_path):
        return 1, [f"{cmake_path} ausente - varredura vazia (L-40)"]
    with open(cmake_path, "r", encoding="utf-8") as handle:
        total, python = registered_selftests(handle.read())
    if not python:
        return 1, ["varredura vazia: nenhum `python --selftest` registrado (L-40)"]
    ensure_git(root)
    linhas, falharam, rodados, excluidos = [], [], 0, 0
    inicio = time.monotonic()
    for nome, rel in python:
        caminho = os.path.join(root, rel)
        if not os.path.isfile(caminho):
            falharam.append(nome)
            linhas.append(f"FALHOU {nome}: {rel} ausente no blob")
            rodados += 1
            continue
        with open(caminho, "r", encoding="utf-8", errors="replace") as handle:
            if is_heavy(handle.read()):
                excluidos += 1
                continue
        rodados += 1
        t0 = time.monotonic()
        try:
            r = subprocess.run(
                [sys.executable, caminho, "--selftest"], cwd=root, capture_output=True, text=True,
                timeout=PER_SCRIPT_TIMEOUT_S, env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
            )
            ok = r.returncode == 0
            saida = (r.stdout + r.stderr)[-400:]
        except subprocess.TimeoutExpired:
            ok, saida = False, f"timeout de {PER_SCRIPT_TIMEOUT_S} s"
        if not ok:
            falharam.append(nome)
            linhas.append(f"FALHOU {nome} ({rel}, {time.monotonic() - t0:.1f} s): {saida.strip()[-300:]}")
    total_s = time.monotonic() - inicio
    achados = len(python)
    linhas.append(
        f"blob: {total} selftest(s) registrado(s), {achados} achado(s) como `python --selftest`, "
        f"{excluidos} excluido(s) (cmake/docker/pwsh), {rodados} rodado(s), falharam {len(falharam)}, {total_s:.0f} s"
    )
    problema = bool(falharam) or rodados != achados - excluidos
    return (1 if problema else 0), linhas


# --- selftest -----------------------------------------------------------


def _fake_root(scripts, cmake_extra=""):
    raiz = tempfile.mkdtemp(prefix="glintfx-blob-")
    os.makedirs(os.path.join(raiz, "tests", "tools"))
    cmake = []
    for nome, (conteudo, registrado) in scripts.items():
        if conteudo is not None:
            with open(os.path.join(raiz, "tests", "tools", nome), "w", encoding="utf-8") as h:
                h.write(conteudo)
        if registrado:
            stem = nome[:-3]
            cmake.append(
                f'add_test(\n    NAME {stem}_selftest\n    COMMAND "${{GLINTFX_PYTHON3_EXECUTABLE}}"\n'
                f'        "${{CMAKE_CURRENT_SOURCE_DIR}}/tools/{nome}" --selftest\n)\n'
                f"set_tests_properties({stem}_selftest PROPERTIES LABELS selftest)\n"
            )
    with open(os.path.join(raiz, "tests", "CMakeLists.txt"), "w", encoding="utf-8") as h:
        h.write("".join(cmake) + cmake_extra)
    return raiz


def _check(nome, condicao, detalhe=""):
    print(f"selftest: {nome} {'OK' if condicao else 'FALHOU'}" + ("" if condicao else f" - {detalhe}"))
    return condicao


def selftest_main():
    ok_py = "import sys\nsys.exit(0)\n"
    ruim_py = "import sys\nsys.exit(1)\n"
    pesado_py = 'import subprocess\nsubprocess.run(["cmake", "--version"])\n'
    controles = []
    rc, linhas = run_all(_fake_root({"a.py": (ok_py, True), "b.py": (ok_py, True)}))
    controles.append(_check("POSITIVO (2 achados, 2 rodados, 0 falharam)", rc == 0 and "2 rodado(s), falharam 0" in linhas[-1], str(linhas)))
    rc, linhas = run_all(_fake_root({"a.py": (ok_py, True), "b.py": (ruim_py, True)}))
    controles.append(_check("NEGATIVO (um selftest falha) reprova, nomeando-o", rc == 1 and any("FALHOU b_selftest" in l for l in linhas), str(linhas)))
    rc, linhas = run_all(_fake_root({"a.py": (ok_py, True), "p.py": (pesado_py, True)}))
    controles.append(_check("script que chama cmake fica de fora (excluido, nao rodado)", rc == 0 and "1 excluido(s)" in linhas[-1] and "1 rodado(s)" in linhas[-1], str(linhas)))
    rc, linhas = run_all(_fake_root({"a.py": (ok_py, True), "sumiu.py": (None, True)}))
    controles.append(_check("registrado mas AUSENTE no blob reprova (o defeito das fixtures)", rc == 1 and any("ausente no blob" in l for l in linhas), str(linhas)))
    rc, linhas = run_all(_fake_root({"a.py": (ok_py, False)}))
    controles.append(_check("VARREDURA VAZIA (nenhum selftest registrado) reprova", rc == 1 and "varredura vazia" in linhas[0], str(linhas)))
    ok_preci = "run_full_pipeline() {\n    stage_format\n    stage_blob\n    stage_configure\n}\n\nrun_lint_only() {\n    stage_format\n}\n"
    controles.append(_check("preci: run_full_pipeline chama stage_blob passa", not preci_blob_errors(ok_preci), str(preci_blob_errors(ok_preci))))
    so_lint = "run_full_pipeline() {\n    stage_format\n    stage_configure\n}\n\nrun_lint_only() {\n    stage_format\n    stage_blob\n}\n"
    controles.append(_check("preci: stage_blob so' no run_lint_only (o erro real) reprova",
                            any("run_full_pipeline" in e for e in preci_blob_errors(so_lint)), str(preci_blob_errors(so_lint))))
    controles.append(_check("preci: sem run_full_pipeline reprova (varredura vazia)", bool(preci_blob_errors("nada\n"))))
    controles.append(_check("a linha de escopo e' sempre impressa", "achado(s)" in run_all(_fake_root({"a.py": (ok_py, True)}))[1][-1]))
    if not all(controles):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controles)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
        return
    if len(args) == 2 and args[0] == "--check-preci":
        with open(args[1], "r", encoding="utf-8") as handle:
            erros = preci_blob_errors(handle.read())
        for e in erros:
            print(f"{SCRIPT_NAME}: {e}", file=sys.stderr)
        print(f"{SCRIPT_NAME}: run_full_pipeline chama stage_blob: {'nao' if erros else 'sim'}")
        sys.exit(1 if erros else 0)
    if len(args) == 2 and args[0] == "--root":
        rc, linhas = run_all(args[1])
        print("\n".join(linhas))
        sys.exit(rc)
    print("usage: blob_selftests.py --root <raiz>  |  --check-preci <preci.sh>  |  --selftest", file=sys.stderr)
    sys.exit(2)


if __name__ == "__main__":
    main()
