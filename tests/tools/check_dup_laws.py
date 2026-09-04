#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_dup_laws.py - CI gate for the mitigation proposed in
# /var/tmp/glintfx-plan/separacao-leis-escopo.md section 5, applied in
# 26/08/2026 (fase 2 da separacao LEI/ESCOPO, ordem do lider: "As god
# laws sao leis minhas para voce executar ao fazer o projeto, nao
# decisoes sobre o projeto").
#
# PORT of the former tests/tools/check_dup_laws.sh (POSIX sh), retired
# in the same fatia that wrote this file (GATE-TREE-PARITY, GODS_LAWS.md
# L-04, decisao do lider: "O comportamento deve ser igual em qualquer
# OS" - the sh version was if(UNIX)-guarded in tests/CMakeLists.txt, so
# GODS_LAWS.md/ESCOPO.md drift was never checked on the Windows CI job,
# even though the check itself is plain text comparison with no
# Unix-specific mechanism behind it). Registered here, unguarded, as an
# ordinary ctest case - the same shape check_layers.py and
# check_vendor_purity.py already proved works on all five platforms.
#
# GODS_LAWS.md e ESCOPO.md tem blocos DUPLICADOS por inteiro (nao
# ponteiro): as leis L-05 e L-07 por decisao explicita do lider (as
# duas opcoes apresentadas a ele diziam que duas copias divergem se so
# uma for editada, e ele escolheu duplicar mesmo assim), e L-06, L-19,
# L-22, L-26 e a LEI ZERO por duvida GENUINA de classificacao do
# orquestrador (regra aplicada: "onde houver duvida, DUPLICA", para o
# lider poder desfazer depois sem perder nada).
#
# Cada bloco duplicado e envolvido, nos DOIS arquivos, por um par de
# ancoras identicas:
#
#   <!-- DUP-BLOCK:ID:START -->
#   ...texto...
#   <!-- DUP-BLOCK:ID:END -->
#
# Este portao prova que as duas copias de cada ID continuam byte a
# byte iguais. Se um dia divergirem (alguem editou um lado e esqueceu
# do outro), ele reprova.
#
# Aplica GODS_LAWS.md L-40 (piso de varredura nao-vazia) em DUAS
# camadas: (a) zero blocos encontrados em QUALQUER um dos dois
# arquivos e' reprovacao, nunca sucesso silencioso; (b) o conjunto de
# IDs tem de ser o MESMO nos dois arquivos - um ID que existe so de um
# lado (ancora apagada, renomeada, ou copia esquecida ao criar bloco
# novo) tambem reprova, mesmo que os blocos que EXISTEM nos dois
# batam.
#
# Usage:
#   check_dup_laws.py <repo-root-directory>
#   check_dup_laws.py --selftest
#
# --selftest roda quatro controles (positivo, negativo, varredura
# vazia, e conjunto de IDs divergente) contra fixtures descartaveis
# sob tempfile.mkdtemp, nunca contra a arvore rastreada real.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import difflib
import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_dup_laws.py"

_ANCHOR_START_RE = re.compile(r"<!-- DUP-BLOCK:([A-Za-z0-9_-]+):START -->")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- extracao ----------------------------------------------------------


def block_ids(text):
    """Lists the block IDs present in a file's text, one per START
    anchor, in the order they appear. Matches only START - END pairing
    is verified in block_text() below.
    """
    return _ANCHOR_START_RE.findall(text)


def block_text(lines, block_id):
    """Extracts the text of ONE block (between its two anchors),
    excluding the anchor lines themselves - mirrors the sh version's
    own `sed -n '/START/,/END/p' | sed '1d;$d'` LINE-based semantics
    exactly (not a substring slice): find the START anchor's line
    index and the END anchor's line index, then join every line
    strictly between them. Returns None if the ID's START/END pair is
    not found. Operating on lines (not raw text) means trailing/
    leading blank lines INSIDE a block are preserved, never collapsed
    by a blanket strip - a divergence made only of blank lines would
    otherwise be masked, which the sh version's own line-based sed
    never allowed either.
    """
    start_marker = f"<!-- DUP-BLOCK:{block_id}:START -->"
    end_marker = f"<!-- DUP-BLOCK:{block_id}:END -->"
    start_i = None
    end_i = None
    for i, line in enumerate(lines):
        if start_i is None and start_marker in line:
            start_i = i
        elif start_i is not None and end_i is None and end_marker in line:
            end_i = i
            break
    if start_i is None or end_i is None:
        return None
    return "\n".join(lines[start_i + 1:end_i])


def sorted_unique(ids):
    return sorted(set(ids))


# --- checagem ------------------------------------------------------------


def check_dup_laws(gods_laws_path, escopo_path):
    """Mirrors check_dup_laws.sh's own check_dup_laws() shell function.
    Returns True (pass, prints the ok summary to stdout) or False
    (reprove, prints diagnostics to stderr) - never raises for an
    ordinary reprove.
    """
    with open(gods_laws_path, "r", encoding="utf-8", errors="replace") as handle:
        gods_laws_text = handle.read()
    with open(escopo_path, "r", encoding="utf-8", errors="replace") as handle:
        escopo_text = handle.read()
    gods_laws_lines = gods_laws_text.splitlines()
    escopo_lines = escopo_text.splitlines()

    ids_a = block_ids(gods_laws_text)
    ids_b = block_ids(escopo_text)

    if not ids_a and not ids_b:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 blocos DUP-BLOCK em qualquer um dos dois arquivos)",
            file=sys.stderr,
        )
        return False

    sorted_a = sorted_unique(ids_a)
    sorted_b = sorted_unique(ids_b)

    if sorted_a != sorted_b:
        print(
            f"{SCRIPT_NAME}: CONJUNTO DE IDs DIVERGENTE entre '{gods_laws_path}' e '{escopo_path}':",
            file=sys.stderr,
        )
        print(f"  IDs em {gods_laws_path}:", file=sys.stderr)
        for i in sorted_a:
            print(f"    - {i}", file=sys.stderr)
        print(f"  IDs em {escopo_path}:", file=sys.stderr)
        for i in sorted_b:
            print(f"    - {i}", file=sys.stderr)
        return False

    mismatch = False
    for block_id in sorted_a:
        text_a = block_text(gods_laws_lines, block_id)
        text_b = block_text(escopo_lines, block_id)
        if text_a != text_b:
            print(f"{SCRIPT_NAME}: bloco '{block_id}' DIVERGE entre os dois arquivos:", file=sys.stderr)
            diff = difflib.unified_diff(
                (text_a or "").splitlines(keepends=True),
                (text_b or "").splitlines(keepends=True),
                fromfile=gods_laws_path,
                tofile=escopo_path,
            )
            sys.stderr.writelines(diff)
            mismatch = True

    if mismatch:
        return False

    print(f"{SCRIPT_NAME}: {len(sorted_a)} bloco(s) duplicado(s) comparados, 0 divergencias")
    return True


# --- modo real -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_dup_laws.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    gods_laws = os.path.join(root, "GODS_LAWS.md")
    escopo = os.path.join(root, "ESCOPO.md")
    if not os.path.isfile(gods_laws):
        fail(f"arquivo nao encontrado: {gods_laws}")
    if not os.path.isfile(escopo):
        fail(f"arquivo nao encontrado: {escopo}")
    if not check_dup_laws(gods_laws, escopo):
        fail("blocos duplicados divergiram ou conjunto de IDs nao bate (ver mensagem acima)")


# --- fixtures e controles do --selftest -----------------------------------


def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") without a hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    return tempfile.mkdtemp(prefix="glintfx-dup-laws-selftest-", dir=os.environ.get("TMPDIR"))


def _make_capture():
    import contextlib
    import io

    class _Captured:
        __slots__ = ("result", "text")

        def __init__(self, result, text):
            self.result = result
            self.text = text

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


# Positive control: two files with two identical blocks each. Expected:
# passes, citing "2 bloco(s)".
def selftest_positive_control(scratch, capture):
    a = os.path.join(scratch, "positive_a.md")
    b = os.path.join(scratch, "positive_b.md")
    fixture = (
        "# doc\n\n"
        "<!-- DUP-BLOCK:ALPHA:START -->\n"
        "texto identico alpha\n"
        "<!-- DUP-BLOCK:ALPHA:END -->\n\n"
        "<!-- DUP-BLOCK:BETA:START -->\n"
        "texto identico beta\n"
        "com duas linhas\n"
        "<!-- DUP-BLOCK:BETA:END -->\n"
    )
    with open(a, "w", encoding="utf-8") as handle:
        handle.write(fixture.replace("# doc", "# doc A"))
    with open(b, "w", encoding="utf-8") as handle:
        handle.write(fixture.replace("# doc", "# doc B"))

    outcome = capture(lambda: check_dup_laws(a, b))
    if outcome.result and "2 bloco(s)" in outcome.text:
        print("selftest: controle POSITIVO OK (dois blocos identicos aprovados)")
        return True
    print("selftest: controle POSITIVO FALHOU", file=sys.stderr)
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: same ID pair, one block diverges by one word.
# Expected: reproves, citing the divergent ID.
def selftest_negative_control(scratch, capture):
    a = os.path.join(scratch, "negative_a.md")
    b = os.path.join(scratch, "negative_b.md")
    with open(a, "w", encoding="utf-8") as handle:
        handle.write("<!-- DUP-BLOCK:GAMMA:START -->\nversao original do texto\n<!-- DUP-BLOCK:GAMMA:END -->\n")
    with open(b, "w", encoding="utf-8") as handle:
        handle.write("<!-- DUP-BLOCK:GAMMA:START -->\nversao EDITADA do texto\n<!-- DUP-BLOCK:GAMMA:END -->\n")

    outcome = capture(lambda: check_dup_laws(a, b))
    if outcome.result:
        print("selftest: controle NEGATIVO FALHOU (bloco divergente foi aprovado)", file=sys.stderr)
        return False
    if "GAMMA" not in outcome.text:
        print("selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou o ID GAMMA)", file=sys.stderr)
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (bloco divergente reprovado e citado)")
    return True


# Empty-scan floor: neither file has any DUP-BLOCK at all. Expected:
# reproves with "varredura vazia" in the message.
def selftest_empty_scan_control(scratch, capture):
    a = os.path.join(scratch, "empty_a.md")
    b = os.path.join(scratch, "empty_b.md")
    with open(a, "w", encoding="utf-8") as handle:
        handle.write("# doc sem blocos\n")
    with open(b, "w", encoding="utf-8") as handle:
        handle.write("# outro doc sem blocos\n")

    outcome = capture(lambda: check_dup_laws(a, b))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (dois arquivos sem bloco algum deveriam reprovar)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK")
    return True


# Divergent ID set: a block exists ONLY in one of the two files (a
# deleted anchor, or a copy forgotten when a new block was created).
# Expected: reproves, even though the block that DOES exist in both is
# identical - the control the "only what exists in both" comparison
# alone would NEVER catch.
def selftest_id_set_mismatch_control(scratch, capture):
    a = os.path.join(scratch, "mismatch_a.md")
    b = os.path.join(scratch, "mismatch_b.md")
    with open(a, "w", encoding="utf-8") as handle:
        handle.write(
            "<!-- DUP-BLOCK:DELTA:START -->\nbloco presente nos dois, identico\n<!-- DUP-BLOCK:DELTA:END -->\n\n"
            "<!-- DUP-BLOCK:SO_EM_A:START -->\neste bloco so existe no arquivo A\n<!-- DUP-BLOCK:SO_EM_A:END -->\n"
        )
    with open(b, "w", encoding="utf-8") as handle:
        handle.write("<!-- DUP-BLOCK:DELTA:START -->\nbloco presente nos dois, identico\n<!-- DUP-BLOCK:DELTA:END -->\n")

    outcome = capture(lambda: check_dup_laws(a, b))
    if outcome.result:
        print(
            "selftest: controle de CONJUNTO DE IDs FALHOU (bloco so-em-A deveria ter reprovado, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "SO_EM_A" not in outcome.text:
        print("selftest: controle de CONJUNTO DE IDs FALHOU (reprovou, mas nao citou SO_EM_A)", file=sys.stderr)
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de CONJUNTO DE IDs OK (bloco orfao detectado mesmo com o par comum identico)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_id_set_mismatch_control(scratch, capture),
        ]
        if not all(controls):
            print("check_dup_laws.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_dup_laws.py --selftest: os {len(controls)} controles OK")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
