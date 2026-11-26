#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_machine_paths.py - W8 deep verification C7-1 (D-W8-2025; AUDITORIAS.md chapter 7; the leader's
# answer of 20/11/2026: "Corrigir daqui para frente"). No tracked text file names the home directory of
# a person's machine. Test tooling, not product code. Standard library only (L-07 of the project).
#
# TWO RULES, and neither freezes a fact of one machine (memory "portao que congela um fato do ambiente"):
#   A. the home directory of WHOEVER RUNS the gate, read at run time (os.path.expanduser("~")), compared
#      without case. On the leader's machine it is his; on another developer's, theirs. It is never
#      written in this file, so the gate itself names nobody.
#   B. the shape of a POSIX home, /home/<name>/ or /Users/<name>/, for any <name> outside the closed
#      list PLACEHOLDERS: the stand-ins this repository uses on purpose in examples and tests (x, user,
#      usuario) and the GitHub runner's account (runner), which is nobody's machine. A name made only of
#      dots ("...") is an elision, not a name. Windows paths are out on purpose: the ones in this
#      repository are the runner's (RUNNER~1) and the test VM's own account (glintfx), by design.
#
# The universe is `git ls-files` of the root (one process); a file with a NUL byte in its first 8 KiB is
# binary and skipped, and the count says so. FAIL-CLOSED (L-40): zero files analysed fails.
#
# USAGE: check_machine_paths.py <repository_root>
#        check_machine_paths.py --selftest
import os
import re
import subprocess
import sys
import tempfile

from selftest_controls import CHECKS, check

PLACEHOLDERS = frozenset({"x", "user", "usuario", "runner"})
HOME_SHAPE = re.compile(r"(?<![A-Za-z0-9_.-])/(?:home|Users)/([A-Za-z0-9._-]+)/")


def own_home():
    home = os.path.expanduser("~").rstrip("/")
    return home if home.count("/") >= 2 else ""


def findings_in_text(text, home):
    """The (line number, reason) of every machine path in one text."""
    found = []
    home_lower = home.lower()
    for number, line in enumerate(text.splitlines(), 1):
        if home and home_lower in line.lower():
            found.append((number, "a pasta pessoal de quem roda o portao"))
            continue
        for match in HOME_SHAPE.finditer(line):
            name = match.group(1)
            if name in PLACEHOLDERS or set(name) == {"."}:
                continue
            found.append((number, "uma pasta pessoal (/home ou /Users com nome)"))
            break
    return found


def tracked_files(root):
    out = subprocess.run(["git", "-C", root, "ls-files", "-z"], capture_output=True, check=True)
    return [name for name in out.stdout.decode("utf-8").split("\0") if name]


def read_text(path):
    with open(path, "rb") as handle:
        data = handle.read()
    if b"\0" in data[:8192]:
        return None
    return data.decode("utf-8", errors="replace")


def scan(root, names, home):
    analysed, binary, findings = 0, 0, []
    for name in names:
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            continue
        text = read_text(path)
        if text is None:
            binary += 1
            continue
        analysed += 1
        findings.extend((name, number, reason) for number, reason in findings_in_text(text, home))
    return analysed, binary, findings


def run(root):
    names = tracked_files(root)
    analysed, binary, findings = scan(root, names, own_home())
    for name, number, reason in findings:
        print(f"{name}:{number}: {reason}")
    print(f"check_machine_paths: rastreados={len(names)} analisados={analysed} binarios={binary} achados={len(findings)}")
    if analysed == 0:
        print("check_machine_paths: varredura vazia reprova (L-40)", file=sys.stderr)
        return 1
    return 1 if findings else 0


def selftest():
    # Every path below is built at run time, so this file never names a home of its own.
    someone = "/" + "home" + "/" + "fulano" + "/notas.txt"
    mac = "/" + "Users" + "/" + "fulana" + "/x"
    injected = "/" + "home" + "/" + "beltrano"
    check("texto limpo passa", findings_in_text("cmake --build build\n", "") == [])
    check("/home/<nome>/ reprova", len(findings_in_text("abre " + someone + "\n", "")) == 1)
    check("/Users/<nome>/ reprova", len(findings_in_text("abre " + mac + "\n", "")) == 1)
    check("os substitutos x, user, usuario e runner passam",
          findings_in_text("/home/x/a /home/user/b /home/usuario/c /home/runner/d\n", "") == [])
    check("a elisao /Users/.../ passa", findings_in_text("C:/Users/.../buildroot e /Users/.../a\n", "") == [])
    check("o espaco reservado /home/<usuario>/ passa", findings_in_text("/home/<usuario>/a\n", "") == [])
    check("a pasta de quem roda reprova, sem caso",
          len(findings_in_text("ver " + injected.upper() + "/a\n", injected)) == 1)
    check("o numero da linha e o certo", findings_in_text("a\nb\n" + someone + "\n", "")[0][0] == 3)
    check("dois achados na mesma linha contam uma vez", len(findings_in_text(someone + " " + mac + "\n", "")) == 1)
    with tempfile.TemporaryDirectory() as tmp:
        with open(os.path.join(tmp, "texto.md"), "w", encoding="utf-8") as handle:
            handle.write("ver " + someone + "\n")
        with open(os.path.join(tmp, "imagem.bin"), "wb") as handle:
            handle.write(b"\0\1" + someone.encode("utf-8"))
        analysed, binary, findings = scan(tmp, ["texto.md", "imagem.bin", "nao_existe.md"], "")
        check("arquivo binario e pulado e contado", (analysed, binary) == (1, 1))
        check("o achado diz o arquivo", findings == [("texto.md", 1, "uma pasta pessoal (/home ou /Users com nome)")])
        check("varredura vazia nao analisa nada", scan(tmp, [], "")[0] == 0)
    print(f"selftest: {len(CHECKS)} controles OK")
    return 0


def main(argv):
    if argv[1:] == ["--selftest"]:
        return selftest()
    if len(argv) != 2:
        print("uso: check_machine_paths.py <raiz_do_repositorio> | --selftest", file=sys.stderr)
        return 2
    return run(argv[1])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
