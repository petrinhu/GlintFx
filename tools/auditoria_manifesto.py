#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# auditoria_manifesto.py - W8 closure, F1 (D-W8-1990, ordem-f1-w8.md): the appendix "frozen public
# text" of each dedicated API review of the W8 (docs/auditoria-api-{shapes,texture,image-resolver,
# sprite-shade}.md). Test tooling, not product code. Standard library only (L-07).
#
# WHY NOT tools/auditoria_texto_congelado.py: that tool rebuilds the final text as "review fences
# plus a hand-written list of amendments". The W8 reviews use 3-backtick fences (that tool reads
# only 4-backtick ones, so it finds none) and their headers changed in 33 to 45 commits after the
# review; an amendment list that long would be a second copy of git history. This tool records,
# instead, WHICH public text was frozen: every public header the review's section 2 names, by its
# git blob id (computed here, LF-normalized, without calling git), and its line count.
#
# THREE MODES, each one thing:
#   generate      writes the appendix at the end of the review (between two markers), from the
#                 headers in <root> as they are now. Run once, at the closing commit of the wave.
#   verify        the appendix is intact: the review body still has the md5 the appendix recorded,
#                 the header list is the one section 2 names, and the table has its own md5. It does
#                 NOT read the headers: later waves change them on purpose, and the appendix is the
#                 record of the version that was frozen (registered in CTest, label consume).
#   check-headers the headers in <root> are exactly the ones the appendix froze. Run only at the
#                 closing commit, before the tag; never registered in CTest (it would freeze them).
#
# FAIL-CLOSED (L-36/L-40 of the project): a section 2 that names no header, a missing appendix, a
# missing header, a changed body or a hand-edited table all fail with code 1, by name.
#
# USAGE: auditoria_manifesto.py generate|verify|check-headers <root> <review.md>...
#        auditoria_manifesto.py --selftest

import hashlib
import os
import re
import sys
import tempfile

SCRIPT_NAME = "auditoria_manifesto.py"
MARK_BEGIN = "<!-- MANIFESTO-INICIO: saida de tools/auditoria_manifesto.py, nunca editar a mao -->"
MARK_END = "<!-- MANIFESTO-FIM -->"
HEADER = re.compile(r"include/glintfx/[a-z0-9_/]+\.hpp")
ROW = re.compile(r"^\| `(include/glintfx/[a-z0-9_/]+\.hpp)` \| `([0-9a-f]{40})` \| ([0-9]+) \|$")


class ManifestError(Exception):
    pass


def read_text(path):
    # Universal newlines: a CRLF checkout (Windows, core.autocrlf=true) reads as LF, so the blob id
    # is the one git stores for an LF file on every system.
    with open(path, "r", encoding="utf-8", newline=None) as handle:
        return handle.read()


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)


def git_blob_id(text):
    data = text.encode("utf-8")
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def md5_of(text):
    return hashlib.md5(text.encode("utf-8")).hexdigest()


def split_appendix(text):
    start = text.find(MARK_BEGIN)
    if start == -1:
        return text, ""
    return text[:start], text[start:]


def section2_headers(body):
    """The sorted, distinct public headers named between '## 2.' and '## 3.'."""
    inside, names = False, set()
    for line in body.splitlines():
        if line.startswith("## 2."):
            inside = True
            continue
        if line.startswith("## 3."):
            break
        if inside:
            names.update(HEADER.findall(line))
    if not names:
        raise ManifestError("a secao 2 nao nomeia nenhum cabecalho publico (varredura vazia)")
    return sorted(names)


def table_rows(root, headers):
    rows = []
    for name in headers:
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            raise ManifestError(f"cabecalho ausente: {name}")
        text = read_text(path)
        rows.append(f"| `{name}` | `{git_blob_id(text)}` | {len(text.splitlines())} |")
    return rows


def render(body, rows):
    table = "\n".join(rows) + "\n"
    return (
        MARK_BEGIN + "\n\n## Apêndice: manifesto do texto público congelado\n\n"
        "Gerado por `tools/auditoria_manifesto.py generate` no commit de fechamento da onda. Cada linha é um "
        "cabeçalho público que a seção 2 nomeia, com o blob do git (conteúdo com fim de linha LF) e o número "
        "de linhas: `git cat-file -p <blob>` mostra o texto exato que foi congelado.\n\n"
        f"corpo-md5: {md5_of(body)}\n\n| Cabeçalho | Blob git | Linhas |\n|---|---|---|\n"
        + table + f"\nmanifesto-md5: {md5_of(table)}\n" + MARK_END + "\n"
    )


def build(text, root):
    body, _old = split_appendix(text)
    return body + render(body, table_rows(root, section2_headers(body)))


def parse_appendix(appendix):
    if not appendix:
        raise ManifestError("sem apendice (rode generate no fechamento)")
    body_md5 = re.search(r"^corpo-md5: ([0-9a-f]{32})$", appendix, re.M)
    table_md5 = re.search(r"^manifesto-md5: ([0-9a-f]{32})$", appendix, re.M)
    rows = [line for line in appendix.splitlines() if line.startswith("| `")]
    if not body_md5 or not table_md5 or not rows:
        raise ManifestError("apendice sem corpo-md5, sem manifesto-md5 ou sem linhas")
    parsed = []
    for line in rows:
        match = ROW.match(line)
        if not match:
            raise ManifestError(f"linha do manifesto ilegivel: {line!r}")
        parsed.append((match.group(1), match.group(2), int(match.group(3))))
    return body_md5.group(1), table_md5.group(1), rows, parsed


def verify(text):
    body, appendix = split_appendix(text)
    body_md5, table_md5, rows, parsed = parse_appendix(appendix)
    if md5_of(body) != body_md5:
        raise ManifestError("o corpo do parecer mudou depois do apendice (regenere no fechamento)")
    if md5_of("\n".join(rows) + "\n") != table_md5:
        raise ManifestError("a tabela do manifesto foi editada a mao")
    if [name for name, _blob, _lines in parsed] != section2_headers(body):
        raise ManifestError("a lista do manifesto difere dos cabecalhos que a secao 2 nomeia")
    return parsed


def check_headers(text, root):
    for name, blob, lines in verify(text):
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            raise ManifestError(f"cabecalho ausente: {name}")
        current = read_text(path)
        if git_blob_id(current) != blob or len(current.splitlines()) != lines:
            raise ManifestError(f"{name} mudou depois do congelamento")


def run(mode, root, paths):
    bad = 0
    for path in paths:
        try:
            text = read_text(path)
            if mode == "generate":
                write_text(path, build(text, root))
            elif mode == "verify":
                verify(text)
            else:
                check_headers(text, root)
        except (ManifestError, OSError) as exc:
            print(f"{SCRIPT_NAME}: {path}: {exc}", file=sys.stderr)
            bad += 1
    print(f"{SCRIPT_NAME}: {mode}: analisados {len(paths)}, reprovados {bad}")
    return 1 if bad or not paths else 0


# --- selftest: the controls, written before the data ----------------------------------------------

SYNTH = "# Parecer\n\n## 1. Veredito\n\nok\n\n## 2. Texto\n\n`include/glintfx/core/a.hpp` e `include/glintfx/core/b.hpp`\n\n## 3. Regras\n\nfim\n"


def make_tree(base, a_text="int a;\n"):
    os.makedirs(os.path.join(base, "include/glintfx/core"), exist_ok=True)
    write_text(os.path.join(base, "include/glintfx/core/a.hpp"), a_text)
    write_text(os.path.join(base, "include/glintfx/core/b.hpp"), "int b;\n")


def fails(call):
    try:
        call()
    except ManifestError:
        return True
    return False


def selftest():
    checks = []
    with tempfile.TemporaryDirectory() as root:
        make_tree(root)
        done = build(SYNTH, root)
        checks.append(("blob igual ao do git para 'hello\\n'",
                       git_blob_id("hello\n") == "ce013625030ba8dba906f756967f9e9ca394464a"))
        checks.append(("gerado passa no verify", not fails(lambda: verify(done))))
        checks.append(("gerado passa no check-headers", not fails(lambda: check_headers(done, root))))
        checks.append(("gerar duas vezes da o mesmo texto", build(done, root) == done))
        checks.append(("sem apendice reprova", fails(lambda: verify(SYNTH))))
        checks.append(("corpo mudado reprova", fails(lambda: verify(done.replace("ok\n", "ok mudado\n", 1)))))
        checks.append(("tabela editada a mao reprova", fails(lambda: verify(done.replace("| 1 |", "| 2 |", 1)))))
        checks.append(("secao 2 sem cabecalho reprova", fails(lambda: build(SYNTH.replace("include/glintfx", "x"), root))))
        checks.append(("cabecalho ausente reprova no generate",
                       fails(lambda: build(SYNTH.replace("b.hpp", "c.hpp"), root))))
        make_tree(root, "int a; // mudou\n")
        checks.append(("cabecalho mudado reprova no check-headers", fails(lambda: check_headers(done, root))))
        checks.append(("cabecalho mudado NAO reprova no verify", not fails(lambda: verify(done))))
        make_tree(root, "int a;\r\n")
        checks.append(("CRLF le como LF: check-headers passa", not fails(lambda: check_headers(done, root))))
    failed = [label for label, ok in checks if not ok]
    for label in failed:
        print(f"selftest: {label} FALHOU", file=sys.stderr)
    if failed:
        return 1
    print(f"selftest: {len(checks)} controles OK")
    return 0


def main(argv):
    if argv[1:] == ["--selftest"]:
        return selftest()
    if len(argv) >= 4 and argv[1] in ("generate", "verify", "check-headers"):
        return run(argv[1], argv[2], argv[3:])
    print("uso: generate|verify|check-headers <raiz> <parecer.md>... | --selftest", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
