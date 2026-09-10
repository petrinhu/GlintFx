#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_loop_callback_truths.py - CI gate for the text half of
# LOOP-CALLBACK-THROW (TODO.md; /var/tmp/glintfx-plan/loop-fix.md sec.
# 5.0 item 4, GODS_LAWS.md L-36/L-40).
#
# WHY THIS GATE EXISTS: the leader made "well commented, in the code
# AND in the documentation" a CRITERION OF ACCEPTANCE for this whole
# fix (DECISOES_AUTONOMAS.md D-091010, verbatim: "Tudo deve ficar
# muito bem comentado no codigo e na documentacao"). A criterion with
# no gate behind it is only conferred from memory - GODS_LAWS.md L-36:
# "critério sem portão é critério conferido de memória". This script
# is that gate: a CLOSED, ENUMERATED list of anchor phrases (the truths
# each layer of gltfx_loop_callbacks does NOT give a consumer for
# free, plus the two fixed section titles each layer's own code
# comment must carry), each tied to the file and the SYMBOL it governs
# - never "somewhere in the file", which would let a phrase drift away
# from the declaration it is supposed to be documenting without this
# gate ever noticing.
#
# TWO ANCHOR KINDS:
#
#   "code"  - the phrase must appear in the COMMENT BLOCK IMMEDIATELY
#             ABOVE the line that first matches `symbol_regex` (every
#             contiguous `//` line directly above that declaration, up
#             to the first non-comment line) - never merely somewhere
#             in the file. This is what lets --selftest's swap control
#             below catch a phrase moved to the wrong declaration.
#   "doc"   - the phrase must appear within the markdown SECTION whose
#             heading matches `section_regex` (from that heading up to
#             the next top-level `## ` heading, or end of file).
#
# THIS FATIA (S1a, LOOP-CALLBACK-THROW) wired up the anchors ITS OWN
# scope landed: the two fixed titles for layer 1, and truth (a) (the
# noexcept-is-a-promise warning), in both loop.hpp and docs/api-
# conventions.md's own R10. LOOP-CONTEXT-OWNERSHIP (S1b, THIS COMMIT)
# adds layer 2's own three truths (b0/b1/b2, /var/tmp/glintfx-plan/
# loop-fix.md sec. 5.0 item 2(b)) - the two fixed titles are NOT
# re-added here as separate anchors: "code_whole_file" already checks
# the WHOLE file, and layer 1's own two entries already prove they are
# present somewhere in loop.hpp, which remains true regardless of which
# layer's own comment block carries them. Layers 3/4 (LOOP-CALLBACK-
# BIND, LOOP-CONTEXT-MARK) add THEIR OWN anchors to the SAME list when
# they land - this script's own mechanism does not change, only
# _ANCHORS grows (GODS_LAWS.md L-40: never widen the allowlist/anchor
# list without a citation of why - each entry below names the plan
# section it comes from).
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import sys
import tempfile

SCRIPT_NAME = "check_loop_callback_truths.py"

# The closed, cited anchor list. `symbol_regex` (kind "code") or
# `section_regex` (kind "doc") is matched as a Python regex against
# the RAW file text; both are anchored to the start of a line
# (`re.MULTILINE`) so a phrase appearing incidentally inside a comment
# elsewhere in the file cannot be mistaken for the symbol/section
# itself.
_ANCHORS = (
    {
        "id": "layer1-title-solves",
        "kind": "code_whole_file",
        "file": "include/glintfx/platform/loop/loop.hpp",
        "phrase": "WHAT THIS LAYER SOLVES",
        "why": "S1a sec. 5.0 item 1 - the fixed title every layer's own code comment must carry",
    },
    {
        "id": "layer1-title-does-not-solve",
        "kind": "code_whole_file",
        "file": "include/glintfx/platform/loop/loop.hpp",
        "phrase": "WHAT THIS LAYER DOES NOT SOLVE",
        "why": "S1a sec. 5.0 item 1 - the fixed title every layer's own code comment must carry",
    },
    {
        "id": "truth-a-code",
        "kind": "code",
        "file": "include/glintfx/platform/loop/loop.hpp",
        "symbol_regex": r"^struct gltfx_loop_callbacks\b",
        "phrase": "noexcept on your callback is a PROMISE, not a proof",
        "why": "S1a sec. 5.0 item 2(a) - truth (a), above the struct it governs",
    },
    {
        "id": "truth-a-doc",
        "kind": "doc",
        "file": "docs/api-conventions.md",
        "section_regex": r"^## R10\b",
        "phrase": "noexcept on your callback is a PROMISE, not a proof",
        "why": "S1a sec. 5.0 item 3 - the same truth (a), inside R10's own section",
    },
    {
        "id": "truth-b0-code",
        "kind": "code",
        "file": "include/glintfx/platform/loop/loop.hpp",
        # Not the bare "destroy_context" - that substring ALSO appears
        # inside this file's own prose comments (this exact anchor's
        # own truth (b1)/(b2) cross-references, for one) - anchoring on
        # the full declaration text is what makes this match the FIELD
        # itself, never an earlier mention of its name.
        "symbol_regex": r"^\s*gltfx_loop_context_destroy_fn destroy_context\b",
        "phrase": "Once handed over, never touch the object again, whatever the call returned.",
        "why": "S1b sec. 5.0 item 2(b0) - above the destroy_context field it governs",
    },
    {
        "id": "truth-b1-code",
        "kind": "code",
        "file": "include/glintfx/platform/loop/loop.hpp",
        # Requires the DECLARATION shape ([[nodiscard]] ... noexcept;),
        # never a comment line that merely MENTIONS "run(gltfx_loop_
        # callbacks)" in passing (destroy_context's own comment block,
        # above, does exactly that, and sits EARLIER in the file - a
        # looser regex would match that prose line first and never
        # reach the real declaration below it).
        "symbol_regex": (
            r"^\s*\[\[nodiscard\]\] GLINTFX_API gltfx_rslt<void> run\(gltfx_loop_callbacks\b"
        ),
        "phrase": "Callbacks handed to run(callbacks) live for that call only: destroyed on "
        "every return path, refusal included.",
        "why": "S1b sec. 5.0 item 2(b1) - above run(gltfx_loop_callbacks)",
    },
    {
        "id": "truth-b2-code",
        "kind": "code",
        "file": "include/glintfx/platform/loop/loop.hpp",
        # set_callbacks(...) itself is clang-format-wrapped onto its OWN
        # line (100-column limit), so the comment block sits above the
        # RETURN-TYPE line, not above the line containing the name -
        # this bare "gltfx_rslt<void>" (nothing else on the line) is
        # unique to that one wrap in this file (run(callbacks)/run()
        # both fit on one line each, so neither produces this shape).
        "symbol_regex": r"^\s*\[\[nodiscard\]\] GLINTFX_API gltfx_rslt<void>\s*$",
        "phrase": "Callbacks handed to set_callbacks() live with the loop: destroyed when "
        "replaced, or when the loop is destroyed, never earlier.",
        "why": "S1b sec. 5.0 item 2(b2) - above set_callbacks(gltfx_loop_callbacks)",
    },
    {
        "id": "truth-b0-doc",
        "kind": "doc",
        "file": "docs/api-conventions.md",
        "section_regex": r"^## R10\b",
        "phrase": "Once handed over, never touch the object again, whatever the call returned.",
        "why": "S1b sec. 5.0 item 3 - truth (b0), inside R10's own section",
    },
    {
        "id": "truth-b1-doc",
        "kind": "doc",
        "file": "docs/api-conventions.md",
        "section_regex": r"^## R10\b",
        "phrase": "Callbacks handed to run(callbacks) live for that call only: destroyed on "
        "every return path, refusal included.",
        "why": "S1b sec. 5.0 item 3 - truth (b1), inside R10's own section",
    },
    {
        "id": "truth-b2-doc",
        "kind": "doc",
        "file": "docs/api-conventions.md",
        "section_regex": r"^## R10\b",
        "phrase": "Callbacks handed to set_callbacks() live with the loop: destroyed when "
        "replaced, or when the loop is destroyed, never earlier.",
        "why": "S1b sec. 5.0 item 3 - truth (b2), inside R10's own section",
    },
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def _normalize(text):
    """Collapses all whitespace runs (including newlines) to a single
    space, so a phrase written on one physical line in the source
    still matches even if a future edit re-wraps the comment/prose
    across several lines - the CONTENT is what this gate protects,
    never one specific line-wrap.
    """
    return re.sub(r"\s+", " ", text).strip()


def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read(), None
    except OSError as exc:
        return None, f"open refused ({exc})"


def extract_comment_block_above(text, symbol_regex):
    """Finds the FIRST line matching symbol_regex, then walks UPWARD
    collecting every contiguous line that is itself a `//` comment
    (after stripping leading whitespace) - stopping at the first line
    that is not, which is what makes the block "the comment
    IMMEDIATELY above this declaration", never "anywhere earlier in
    the file". Returns None if the symbol itself is never found, and
    an EMPTY STRING (not None) if the symbol is found but nothing
    directly above it is a comment line - the caller distinguishes the
    two failure shapes in its own message.
    """
    lines = text.splitlines()
    pattern = re.compile(symbol_regex)
    symbol_line = None
    for index, line in enumerate(lines):
        if pattern.match(line):
            symbol_line = index
            break
    if symbol_line is None:
        return None

    collected = []
    cursor = symbol_line - 1
    while cursor >= 0:
        stripped = lines[cursor].strip()
        if stripped.startswith("//"):
            collected.append(stripped[2:].strip())
            cursor -= 1
        else:
            break
    collected.reverse()
    return " ".join(collected)


def extract_doc_section(text, section_regex):
    """Returns the text of the FIRST markdown section whose heading
    line matches section_regex, from that heading line (inclusive) up
    to the next top-level `## ` heading or end of file - None if the
    heading itself is never found.
    """
    lines = text.splitlines()
    pattern = re.compile(section_regex)
    start = None
    for index, line in enumerate(lines):
        if pattern.match(line):
            start = index
            break
    if start is None:
        return None

    end = len(lines)
    for index in range(start + 1, len(lines)):
        if lines[index].startswith("## "):
            end = index
            break
    return "\n".join(lines[start:end])


def check_anchor(anchor, project_root):
    path = os.path.join(project_root, anchor["file"])
    text, err = read_text(path)
    if err is not None:
        return False, f"{anchor['file']} not readable ({err})"

    phrase = _normalize(anchor["phrase"])

    if anchor["kind"] == "code_whole_file":
        if phrase in _normalize(text):
            return True, ""
        return False, f"{anchor['file']}: phrase not found anywhere in the file"

    if anchor["kind"] == "code":
        block = extract_comment_block_above(text, anchor["symbol_regex"])
        if block is None:
            return False, f"{anchor['file']}: symbol {anchor['symbol_regex']!r} not found"
        if phrase in _normalize(block):
            return True, ""
        return False, (
            f"{anchor['file']}: comment block immediately above "
            f"{anchor['symbol_regex']!r} does not contain the phrase"
        )

    if anchor["kind"] == "doc":
        section = extract_doc_section(text, anchor["section_regex"])
        if section is None:
            return False, f"{anchor['file']}: section {anchor['section_regex']!r} not found"
        if phrase in _normalize(section):
            return True, ""
        return False, (
            f"{anchor['file']}: section {anchor['section_regex']!r} does not contain the phrase"
        )

    fail(f"unknown anchor kind: {anchor['kind']!r} (anchor id {anchor['id']!r})")
    return False, ""  # unreachable, fail() exits


def check_all_anchors(anchors, project_root):
    if not anchors:
        print(f"{SCRIPT_NAME}: varredura vazia (anchor list is empty)", file=sys.stderr)
        return False

    found = 0
    missing = []
    for anchor in anchors:
        ok, detail = check_anchor(anchor, project_root)
        if ok:
            found += 1
        else:
            missing.append(f"{anchor['id']}: {detail}")

    if missing:
        print(f"{SCRIPT_NAME}: ANCHOR(S) MISSING (LOOP-CALLBACK-THROW D-091010):", file=sys.stderr)
        for line in missing:
            print(f"  {line}", file=sys.stderr)
        return False

    print(f"{SCRIPT_NAME}: {found} of {len(anchors)} anchor(s) found")
    return True


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_loop_callback_truths.py <project_root>")
    (project_root,) = args
    if not os.path.isdir(project_root):
        fail(f"project root not found: {project_root}")
    if not check_all_anchors(_ANCHORS, project_root):
        fail("one or more required anchor phrases are missing (see message above)")


# --- selftest fixtures and controls -----------------------------------


def make_scratch_root():
    return tempfile.mkdtemp(prefix="glintfx-loop-truths-selftest-", dir=os.environ.get("TMPDIR"))


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


_FIXTURE_ANCHORS = (
    {
        "id": "fixture-truth-alpha",
        "kind": "code",
        "file": "header.hpp",
        "symbol_regex": r"^struct alpha_t\b",
        "phrase": "alpha does not survive a reboot",
    },
    {
        "id": "fixture-truth-beta",
        "kind": "code",
        "file": "header.hpp",
        "symbol_regex": r"^struct beta_t\b",
        "phrase": "beta is not thread safe",
    },
    {
        "id": "fixture-doc-alpha",
        "kind": "doc",
        "file": "doc.md",
        "section_regex": r"^## R99\b",
        "phrase": "alpha does not survive a reboot",
    },
)


# Positive control: every phrase directly above/inside the symbol it
# governs. Expected: all three anchors found, function returns True.
def selftest_positive_control(scratch):
    root = os.path.join(scratch, "positive")
    _write(
        os.path.join(root, "header.hpp"),
        "#pragma once\n\n"
        "// alpha does not survive a reboot\n"
        "struct alpha_t {\n    int value = 0;\n};\n\n"
        "// beta is not thread safe\n"
        "struct beta_t {\n    int value = 0;\n};\n",
    )
    _write(
        os.path.join(root, "doc.md"),
        "# Doc\n\n## R99: Alpha\n\nalpha does not survive a reboot, ever.\n\n## R100: Other\n\nnothing here.\n",
    )
    ok = check_all_anchors(_FIXTURE_ANCHORS, root)
    if not ok:
        print("selftest: controle POSITIVO FALHOU (esperava todas as ancoras encontradas)", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (as tres ancoras encontradas nos simbolos certos)")
    return True


# Negative control: the two code phrases SWAPPED between the two
# structs - alpha's phrase sits above beta_t, and vice versa. Both
# code anchors must fail (each looks for its OWN phrase above its OWN
# symbol, and finds the other one instead).
def selftest_swap_control(scratch):
    root = os.path.join(scratch, "swap")
    _write(
        os.path.join(root, "header.hpp"),
        "#pragma once\n\n"
        "// beta is not thread safe\n"
        "struct alpha_t {\n    int value = 0;\n};\n\n"
        "// alpha does not survive a reboot\n"
        "struct beta_t {\n    int value = 0;\n};\n",
    )
    _write(
        os.path.join(root, "doc.md"),
        "# Doc\n\n## R99: Alpha\n\nalpha does not survive a reboot, ever.\n\n## R100: Other\n\nnothing here.\n",
    )
    ok = check_all_anchors(_FIXTURE_ANCHORS, root)
    if ok:
        print("selftest: controle de TROCA FALHOU (deveria reprovar as duas ancoras de codigo trocadas)", file=sys.stderr)
        return False
    alpha_ok, _ = check_anchor(_FIXTURE_ANCHORS[0], root)
    beta_ok, _ = check_anchor(_FIXTURE_ANCHORS[1], root)
    if alpha_ok or beta_ok:
        print(f"selftest: controle de TROCA FALHOU (esperava as duas false, achou alpha={alpha_ok} beta={beta_ok})", file=sys.stderr)
        return False
    print("selftest: controle de TROCA OK (frase trocada de simbolo e reprovada, nao aprovada por engano)")
    return True


# Negative control: the phrase simply absent (comment block exists,
# says something else entirely).
def selftest_missing_phrase_control(scratch):
    root = os.path.join(scratch, "missing_phrase")
    _write(
        os.path.join(root, "header.hpp"),
        "#pragma once\n\n"
        "// this comment is about something unrelated\n"
        "struct alpha_t {\n    int value = 0;\n};\n\n"
        "// beta is not thread safe\n"
        "struct beta_t {\n    int value = 0;\n};\n",
    )
    _write(
        os.path.join(root, "doc.md"),
        "# Doc\n\n## R99: Alpha\n\nalpha does not survive a reboot, ever.\n\n## R100: Other\n\nnothing here.\n",
    )
    ok, detail = check_anchor(_FIXTURE_ANCHORS[0], root)
    if ok:
        print("selftest: controle de FRASE AUSENTE FALHOU (deveria reprovar alpha, aprovou)", file=sys.stderr)
        return False
    print(f"selftest: controle de FRASE AUSENTE OK ({detail})")
    return True


# Negative control: the symbol itself does not exist in the file at
# all (not merely undocumented) - must fail with a message naming the
# missing symbol, never crash.
def selftest_missing_symbol_control(scratch):
    root = os.path.join(scratch, "missing_symbol")
    _write(
        os.path.join(root, "header.hpp"),
        "#pragma once\n\nstruct beta_t {\n    int value = 0;\n};\n",
    )
    ok, detail = check_anchor(_FIXTURE_ANCHORS[0], root)
    if ok:
        print("selftest: controle de SIMBOLO AUSENTE FALHOU (alpha_t nao existe, mas aprovou)", file=sys.stderr)
        return False
    if "not found" not in detail:
        print(f"selftest: controle de SIMBOLO AUSENTE FALHOU (mensagem nao cita ausencia: {detail!r})", file=sys.stderr)
        return False
    print(f"selftest: controle de SIMBOLO AUSENTE OK ({detail})")
    return True


# Empty-list floor control (GODS_LAWS.md L-40): an empty anchor list
# must be REFUSED, never presumed "nothing to check, so it passes".
def selftest_empty_anchor_list_control(scratch):
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        result = check_all_anchors((), scratch)
    text = buffer.getvalue()
    if result:
        print("selftest: controle de LISTA VAZIA FALHOU (deveria recusar lista de ancoras vazia)", file=sys.stderr)
        return False
    if "varredura vazia" not in text:
        print("selftest: controle de LISTA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        return False
    print("selftest: controle de LISTA VAZIA OK (lista de ancoras vazia recusada)")
    return True


def _run_all_controls(scratch):
    return [
        selftest_positive_control(scratch),
        selftest_swap_control(scratch),
        selftest_missing_phrase_control(scratch),
        selftest_missing_symbol_control(scratch),
        selftest_empty_anchor_list_control(scratch),
    ]


def selftest_main():
    scratch = make_scratch_root()
    try:
        results = _run_all_controls(scratch)
    finally:
        pass  # scratch left for post-mortem inspection on failure, same as sibling gates
    if not all(results):
        fail(f"one or more selftest controls failed (scratch preserved at {scratch})")
    print(f"{SCRIPT_NAME}: selftest OK, {len(results)} control(s) passed (scratch: {scratch})")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
        return
    real_main(args)


if __name__ == "__main__":
    main()
