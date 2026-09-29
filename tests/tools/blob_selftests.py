#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# blob_selftests.py - CI-SPLIT-PER-OS A5 etapa 3 (decisao do CTO, 29/09/2026):
# roda os `--selftest` Python sobre o ARQUIVO COMMITAVEL (o indice do git, extraido
# em outro diretorio) e nao sobre a working tree. Fecha a CLASSE do achado das
# fixtures do P1: o preci local roda sobre a working tree (onde existem arquivos
# ignorados, nao adicionados ou gerados), e o CI roda sobre o blob commitado.
#
# O UNIVERSO e' LIDO de tests/CMakeLists.txt do proprio blob, nunca de lista a mao: todo
# `add_test` com `LABELS selftest`. Roda `python <script> --selftest` e `<script>.sh
# --selftest`. Fora SO' o que traz, no bloco de comentarios logo acima do add_test,
# `# glintfx-blob: fora - <motivo>` (declaracao FAIL-CLOSED: sem motivo reprova; forma nao
# rodavel sem declaracao reprova; a saida NOMEIA cada um com o motivo). Nada de heuristica
# por substring no texto do script (B-2, CTO 29/09: excluia scripts leves por citarem cmake
# num comentario).
#
# Imprime SEMPRE "N registrados, R rodados, F fora (declarado), falharam K" (L-40) e o tempo.
# Reprova se algum falhar, se 0 rodarem, ou se a conta rodados + fora == registrados nao
# fechar (registrados = contagem INDEPENDENTE por `LABELS selftest`, nao o resultado da
# propria varredura - B-3, a trava que nao pode falhar).
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


_SH_RE = re.compile(r'"\$\{(?:CMAKE_CURRENT_SOURCE_DIR|PROJECT_SOURCE_DIR)\}/([^"]+\.sh)"\s+--selftest')
_DECL_RE = re.compile(r"#\s*glintfx-blob:\s*fora\b[ \t]*(?:-[ \t]*(\S[^\n]*))?")


def _comment_block_above(cmake_text, offset):
    linhas = cmake_text[:offset].splitlines()
    acima = []
    i = len(linhas) - 1
    while i >= 0 and (linhas[i].lstrip().startswith("#") or not linhas[i].strip()):
        acima.append(linhas[i])
        i -= 1
    return "\n".join(acima)


def _resolve(caminho):
    return os.path.normpath(caminho.replace("${CMAKE_CURRENT_SOURCE_DIR}", "tests").replace("${PROJECT_SOURCE_DIR}", "."))


def registered_selftests(cmake_text):
    """([entrada], nomes_por_label): toda `add_test` com `LABELS selftest`. Entrada =
    {nome, forma ('py' | 'sh' | None), script, fora (None | motivo | '' se declarada sem motivo)}.
    `nomes_por_label` e' a contagem INDEPENDENTE (set_tests_properties ... LABELS selftest)."""
    props = {}
    for m in re.finditer(r"set_tests_properties\s*\(", cmake_text):
        bloco = _block(cmake_text, m.start())
        nome = re.match(r"set_tests_properties\s*\(\s*(\S+)", bloco)
        if nome:
            props[nome.group(1)] = props.get(nome.group(1), "") + bloco
    por_label = {n for n, b in props.items() if re.search(r"LABELS\s+selftest\b", b)}
    entradas = []
    for m in re.finditer(r"add_test\s*\(", cmake_text):
        bloco = _block(cmake_text, m.start())
        nome = re.search(r"NAME\s+(\S+)", bloco)
        if not nome or nome.group(1) not in por_label:
            continue
        decl = _DECL_RE.search(_comment_block_above(cmake_text, m.start()))
        fora = None if not decl else (decl.group(1) or "").strip()
        py = _COMMAND_RE.search(bloco)
        sh = _SH_RE.search(bloco)
        if py:
            forma, script = "py", _resolve(py.group(1))
        elif sh:
            forma, script = "sh", _resolve("${PROJECT_SOURCE_DIR}/" + sh.group(1)) if "PROJECT_SOURCE_DIR" in sh.group(0) else _resolve("${CMAKE_CURRENT_SOURCE_DIR}/" + sh.group(1))
        else:
            forma, script = None, None
        entradas.append({"nome": nome.group(1), "forma": forma, "script": script, "fora": fora})
    return entradas, por_label


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


def ensure_git(root):
    """O blob e' uma COPIA do indice sem .git, e muitos selftests chamam `git ls-files`
    (como no CI, que roda num checkout). Vira um repositorio git descartavel com tudo
    adicionado - so' os arquivos do blob, nenhum ignorado ou nao rastreado do original."""
    if os.path.isdir(os.path.join(root, ".git")):
        return
    for args in (["init", "-q"], ["add", "-A", "-f"]):
        subprocess.run(["git", "-C", root, *args], check=True, capture_output=True)


def _run_one(root, entrada):
    caminho = os.path.join(root, entrada["script"])
    cmd = [sys.executable, caminho, "--selftest"] if entrada["forma"] == "py" else ["bash", caminho, "--selftest"]
    t0 = time.monotonic()
    try:
        r = subprocess.run(
            cmd, cwd=root, capture_output=True, text=True, timeout=PER_SCRIPT_TIMEOUT_S,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
        )
        ok, saida = r.returncode == 0, (r.stdout + r.stderr)
    except subprocess.TimeoutExpired:
        ok, saida = False, f"timeout de {PER_SCRIPT_TIMEOUT_S} s"
    return ok, time.monotonic() - t0, saida.strip()[-300:]


def run_all(root):
    """(rc, linhas). Todo selftest registrado RODA, a menos que o add_test traga logo acima
    `# glintfx-blob: fora - <motivo>` (declaracao fail-closed: sem motivo reprova; forma nao
    rodavel sem declaracao reprova). Conta: rodados + fora == registrados (registrados =
    contagem independente por LABELS selftest); 0 rodados reprova."""
    cmake_path = os.path.join(root, "tests", "CMakeLists.txt")
    if not os.path.isfile(cmake_path):
        return 1, [f"{cmake_path} ausente - varredura vazia (L-40)"]
    with open(cmake_path, "r", encoding="utf-8") as handle:
        entradas, por_label = registered_selftests(handle.read())
    if not entradas:
        return 1, ["varredura vazia: nenhum selftest registrado com LABELS selftest (L-40)"]
    ensure_git(root)
    linhas, falharam, erros = [], [], []
    rodados = fora = 0
    inicio = time.monotonic()
    nomes_add = {e["nome"] for e in entradas}
    for fantasma in sorted(por_label - nomes_add):
        erros.append(f"{fantasma}: LABELS selftest sem add_test correspondente (contagem independente diverge)")
    for e in entradas:
        nome = e["nome"]
        if e["fora"] is not None:
            if not e["fora"]:
                erros.append(f"{nome}: `# glintfx-blob: fora` sem motivo (use `- <motivo>`) - declaracao fail-closed")
                continue
            fora += 1
            linhas.append(f"fora: {nome} - {e['fora']}")
            continue
        if e["forma"] is None:
            erros.append(f"{nome}: forma nao rodavel (nem `python --selftest` nem `.sh --selftest`) e sem declaracao `# glintfx-blob: fora - <motivo>`")
            continue
        if not os.path.isfile(os.path.join(root, e["script"])):
            falharam.append(nome)
            rodados += 1
            linhas.append(f"FALHOU {nome}: {e['script']} ausente no blob")
            continue
        rodados += 1
        ok, seg, saida = _run_one(root, e)
        if not ok:
            falharam.append(nome)
            linhas.append(f"FALHOU {nome} ({e['script']}, {seg:.1f} s): {saida}")
    total_s = time.monotonic() - inicio
    registrados = len(por_label)
    if rodados == 0:
        erros.append("0 rodado(s): nenhum selftest rodou (tudo declarado fora ou coleta quebrada) - nao e' verde (L-40)")
    if rodados + fora + len(erros) != registrados and not erros:
        erros.append(f"conta quebrada: {rodados} rodado(s) + {fora} fora != {registrados} registrado(s)")
    linhas.extend(f"ERRO {e}" for e in erros)
    linhas.append(
        f"blob: {registrados} registrado(s), {rodados} rodado(s), {fora} fora (declarado), falharam {len(falharam)}, "
        f"{len(erros)} erro(s) de declaracao, {total_s:.0f} s"
    )
    return (1 if (falharam or erros) else 0), linhas


# --- selftest -----------------------------------------------------------


def _fake_root(scripts, cmake_extra=""):
    """scripts: {arquivo: (conteudo|None, registrado, comentario_acima|None, forma)}; forma
    'py' (`python x.py --selftest`), 'sh' (`x.sh --selftest`) ou 'pwsh'."""
    raiz = tempfile.mkdtemp(prefix="glintfx-blob-")
    os.makedirs(os.path.join(raiz, "tests", "tools"))
    cmake = []
    for nome, (conteudo, registrado, comentario, forma) in scripts.items():
        if conteudo is not None:
            caminho = os.path.join(raiz, "tests", "tools", nome)
            with open(caminho, "w", encoding="utf-8") as h:
                h.write(conteudo)
            os.chmod(caminho, 0o755)
        if registrado:
            stem = nome.rsplit(".", 1)[0]
            if forma == "py":
                cmd = f'"${{GLINTFX_PYTHON3_EXECUTABLE}}"\n        "${{CMAKE_CURRENT_SOURCE_DIR}}/tools/{nome}" --selftest'
            elif forma == "sh":
                cmd = f'"${{CMAKE_CURRENT_SOURCE_DIR}}/tools/{nome}" --selftest'
            else:
                cmd = f'"${{GLINTFX_PWSH_EXECUTABLE}}" -NoProfile -File "${{PROJECT_SOURCE_DIR}}/tools/{nome}" -SelfTest'
            acima = (comentario + "\n") if comentario else ""
            cmake.append(
                f"{acima}add_test(\n    NAME {stem}_selftest\n    COMMAND {cmd}\n)\n"
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
    ok_sh = "#!/bin/sh\nexit 0\n"
    ruim_sh = "#!/bin/sh\nexit 1\n"
    cita_cmake = "# este comentario cita cmake, docker e pwsh\nimport sys\nsys.exit(0)\n"
    controles = []

    def roda(scripts, extra=""):
        return run_all(_fake_root(scripts, extra))

    rc, linhas = roda({"a.py": (ok_py, True, None, "py"), "b.py": (ok_py, True, None, "py")})
    controles.append(_check("POSITIVO (2 registrados, 2 rodados, 0 falharam)", rc == 0 and "2 rodado(s)" in linhas[-1] and "falharam 0" in linhas[-1], str(linhas)))
    rc, linhas = roda({"a.py": (ok_py, True, None, "py"), "b.py": (ruim_py, True, None, "py")})
    controles.append(_check("um selftest falha: reprova, nomeando-o", rc == 1 and any("FALHOU b_selftest" in l for l in linhas), str(linhas)))
    rc, linhas = roda({"a.sh": (ok_sh, True, None, "sh"), "b.sh": (ruim_sh, True, None, "sh")})
    controles.append(_check("B-1: o `.sh --selftest` tambem RODA (e reprova quando falha)", rc == 1 and any("FALHOU b_selftest" in l for l in linhas) and "2 rodado(s)" in linhas[-1], str(linhas)))
    rc, linhas = roda({"a.py": (ok_py, True, None, "py"), "x.ps1": (None, True, "# glintfx-blob: fora - precisa de pwsh", "pwsh")})
    controles.append(_check("B-1: o que nao roda fica fora SO' por declaracao, e a saida NOMEIA o motivo",
                            rc == 0 and any("fora: x_selftest - precisa de pwsh" in l for l in linhas), str(linhas)))
    rc, linhas = roda({"a.py": (ok_py, True, None, "py"), "x.ps1": (None, True, None, "pwsh")})
    controles.append(_check("B-1: forma nao rodavel SEM declaracao reprova (fail-closed)", rc == 1 and any("x_selftest" in l and "declaracao" in l for l in linhas), str(linhas)))
    rc, linhas = roda({"a.py": (ok_py, True, None, "py"), "p.py": (ok_py, True, "# glintfx-blob: fora", "py")})
    controles.append(_check("B-2: declaracao SEM motivo reprova", rc == 1 and any("p_selftest" in l and "motivo" in l for l in linhas), str(linhas)))
    rc, linhas = roda({"a.py": (cita_cmake, True, None, "py")})
    controles.append(_check("B-2: script que so' CITA cmake em comentario continua RODANDO (mutante da heuristica por substring)",
                            rc == 0 and "1 rodado(s)" in linhas[-1] and "0 fora" in linhas[-1], str(linhas)))
    rc, linhas = roda({"a.py": (ok_py, True, "# glintfx-blob: fora - pesado (cmake)", "py")})
    controles.append(_check("B-3: 0 rodados REPROVA (tudo declarado fora nao e' verde)", rc == 1 and any("0 rodado" in l or "nenhum" in l for l in linhas), str(linhas)))
    rc, linhas = roda({"sumiu.py": (None, True, None, "py"), "a.py": (ok_py, True, None, "py")})
    controles.append(_check("registrado mas AUSENTE no blob reprova (o defeito das fixtures)", rc == 1 and any("ausente no blob" in l for l in linhas), str(linhas)))
    rc, linhas = roda({"a.py": (ok_py, False, None, "py")})
    controles.append(_check("VARREDURA VAZIA (nenhum selftest registrado) reprova", rc == 1 and "varredura vazia" in linhas[0], str(linhas)))
    # B-3: a conta rodados + fora == registrados usa uma contagem INDEPENDENTE (LABELS selftest)
    rc, linhas = roda({"a.py": (ok_py, True, None, "py")}, extra="set_tests_properties(fantasma_selftest PROPERTIES LABELS selftest)\n")
    controles.append(_check("B-3: LABELS selftest de um teste que nao tem add_test reprova (contagem independente)", rc == 1 and any("fantasma_selftest" in l for l in linhas), str(linhas)))
    rc, linhas = roda({"a.py": (ok_py, True, "# glintfx-blob: fora - motivo qualquer", "py"), "b.py": (ok_py, True, None, "py")})
    controles.append(_check("a linha de escopo soma: rodados + fora == registrados", "1 rodado(s), 1 fora" in linhas[-1] and "2 registrado(s)" in linhas[-1], str(linhas)))
    ok_preci = "run_full_pipeline() {\n    stage_format\n    stage_blob\n    stage_configure\n}\n\nrun_lint_only() {\n    stage_format\n}\n"
    controles.append(_check("preci: run_full_pipeline chama stage_blob passa", not preci_blob_errors(ok_preci), str(preci_blob_errors(ok_preci))))
    so_lint = "run_full_pipeline() {\n    stage_format\n    stage_configure\n}\n\nrun_lint_only() {\n    stage_format\n    stage_blob\n}\n"
    controles.append(_check("preci: stage_blob so' no run_lint_only (o erro real) reprova",
                            any("run_full_pipeline" in e for e in preci_blob_errors(so_lint)), str(preci_blob_errors(so_lint))))
    controles.append(_check("preci: sem run_full_pipeline reprova (varredura vazia)", bool(preci_blob_errors("nada\n"))))
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
