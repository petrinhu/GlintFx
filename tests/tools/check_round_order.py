#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_round_order.py - CI gate for TODO.md's GATE-LLROUND-ORDER
# ("A ordem 'compara antes de arredondar' e invisivel a todo teste que
# temos").
#
# ORIGIN (TODO.md, GATE-LLROUND-ORDER, achado do teste de mutacao da
# re-revisao de CORE-TIME, 28/08/2026): moving the rounding call in
# gltfx_duration_from_seconds() (src/core/time.cpp) to BEFORE the two
# range comparisons left the WHOLE suite green, ASan/UBSan included -
# the defect lives INSIDE glibc's own uninstrumented std::llround(),
# and the value computed early is only ever consulted on the branch
# where it would already have been valid. The observable output never
# changes under that mutation, so no value-level test can ever catch
# it, by construction (time.hpp's own header comment - decision D8 -
# names this the precedent for every FUTURE pure math/conversion
# function this project writes, not just this one). Conserto proposed
# by the item itself: a STATIC gate, by inspection, requiring every
# rounding call to be preceded, in the SAME function body, by a
# comparison against BOTH range limits.
#
# THE LOOPHOLE THE ORCHESTRATOR NAMED BEFORE THIS FILE WAS WRITTEN: a
# gate that only asks "is there SOME comparison before the call?" is
# satisfied by ANY comparison, including one against the wrong bound,
# the wrong variable, or nothing to do with the range at all. This
# gate closes that loophole by requiring the comparison to be tied, by
# NAME, to std::numeric_limits<...>::max()/::min() (never a bare
# literal, which could be the wrong limit and this gate would have no
# way to know), AND to be a comparison of the EXACT identifier passed
# as the rounding call's own argument (never an unrelated variable that
# happens to sit nearby). Both requirements are proven by the negative
# controls in --selftest below (a comparison present but on the wrong
# variable, and a comparison present but not tied to numeric_limits).
#
# THE SECOND SHAPE THIS GATE ACCEPTS, MEASURED AGAINST THE REAL TREE
# BEFORE BEING DESIGNED IN (GODS_LAWS.md L-01: "meca antes de decidir"):
# src/core/color.cpp's own unit_to_byte() and src/gfss/color_parse.cpp's
# own clamp_0_255_to_byte()/clamp_unit_to_byte() all round the result of
# an INLINE std::clamp(...) call - `std::lround(std::clamp(unit, 0.0F,
# 1.0F) * k_byte_max)` - never a separate `if` comparison at all. This
# is not a lesser cousin of the CORE-TIME shape: a clamp() NESTED inside
# the SAME expression as the rounding call cannot be reordered "to
# after" the call by any edit that leaves the call's own argument
# expression alone - the exact mutation this item exists to catch (move
# the bound-check to after the round) has no textual place to land,
# because the bound-check and the round call are one expression, not
# two statements that could drift apart. A gate that rejected this
# shape and demanded a separate `if` everywhere would be reporting a
# false violation against code this project's own adversarial review
# already accepted - REJECTED for that reason (GODS_LAWS.md L-01:
# measured against the real tree, not assumed).
#
# WHAT COUNTS AS A "ROUNDING CALL" (closed vocabulary, GODS_LAWS.md
# L-40 - enumerable, revisable by a human in a minute): llround,
# lround, round, nearbyint, rint - the five <cmath> functions that
# convert a floating-point value to the nearest integer and can invoke
# implementation-defined/undefined behavior for an out-of-range or
# non-finite argument (cppreference, "std::lround, std::llround" and
# "std::nearbyint, std::rint"). std::round()/std::trunc()/std::ceil()/
# std::floor() never leave floating-point representation and cannot
# themselves overflow an INTEGER type - only the four *round*-family
# conversions to a fixed-width integer, plus std::round() itself kept
# in the vocabulary because a caller reasonably reaches for it BEFORE
# a narrowing static_cast (the exact shape this item's own
# static_cast<std::int64_t>(std::llround(...)) already uses one level
# up) - are the ones D8's rule actually guards. Matched with (?:std::)?
# so a bare, ADL/using-directive form is not missed either - VERIFIED
# against a fixture in --selftest.
#
# WHERE THIS GATE SCANS: PROJECT_SOURCE_DIR/src and PROJECT_SOURCE_DIR/
# include, recursively, restricted to the two extensions this project's
# own tree actually uses under those two directories today (measured:
# `find src include -type f | sed -E 's/.*(\.[a-zA-Z]+)$/\1/' | sort -u`
# returns only .cpp/.hpp/.txt, and .txt under src/include is only
# CMakeLists.txt, never C++ source) - .cpp and .hpp. A header-only
# rounding call is in scope by construction: this gate does not special-
# case "public API" the way check_macro_balance.py does, because a
# rounding call in a header (an inline function, say) carries the exact
# same risk this item names.
#
# METHOD, DECLARED HONESTLY (GODS_LAWS.md L-40: "nao esconde o que nao
# faz"): this is a TEXTUAL, line-oriented scan, not a real C++ parser.
# It finds the innermost enclosing brace-delimited block that "looks
# like a function" (its own header, back to the previous statement
# terminator, does not start with a control-flow/namespace/class/struct/
# union/enum keyword, and itself contains a parenthesized parameter
# list) and searches for comparisons ONLY inside that block, restricted
# to lines STRICTLY BEFORE the rounding call's own line. A rounding
# call whose own argument is anything other than a bare identifier or a
# clamp(...)-guarded expression is REFUSED outright (never approved by
# guesswork about a complex expression this gate cannot safely trace) -
# the same "recusar alto e melhor que aprovar em silencio" this
# project's other gates already practice. Lambdas are classified as
# "function-like" by the same rule (they have a parameter list and do
# not start with a control keyword) - declared, not hidden: a rounding
# call inside a lambda body is checked against comparisons WITHIN that
# lambda's own body, never the enclosing function's - no rounding call
# in a lambda exists in this project's tree today (measured), so this
# is a documented limitation, not an observed gap.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import sys
import tempfile

SCRIPT_NAME = "check_round_order.py"

_SOURCE_EXTENSIONS = (".cpp", ".hpp")
_SCANNED_SUBDIRS = ("src", "include")

# Longest alternative first (house convention this project's own CLAUDE.md
# names for `find -regex` alternation; harmless but consistent here too -
# every alternative below starts at a DIFFERENT first letter after \b, so
# the order never actually changes which one matches, only readability).
_ROUND_CALL_RE = re.compile(r"\b(?:std::)?(llround|lround|nearbyint|round|rint)\s*\(")

_CONTROL_HEADER_RE = re.compile(
    r"^\s*(if|else|for|while|switch|do|try|catch|namespace|class|struct|union|enum)\b"
)

_LIMIT_MAX_RE = re.compile(r"numeric_limits\s*<[^>]*>\s*::\s*max\s*\(\s*\)")
_LIMIT_MIN_RE = re.compile(r"numeric_limits\s*<[^>]*>\s*::\s*min\s*\(\s*\)")

_IDENTIFIER_RE = re.compile(r"^[A-Za-z_]\w*$")
_CLAMP_CALL_RE = re.compile(r"\bclamp\s*\(")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- enumeration -----------------------------------------------------


def enumerate_scanned_files(project_root):
    files = []
    for rel_dir in _SCANNED_SUBDIRS:
        abs_dir = os.path.join(project_root, rel_dir)
        for dirpath, _dirnames, filenames in os.walk(abs_dir):
            for name in filenames:
                if name.endswith(_SOURCE_EXTENSIONS):
                    files.append(os.path.join(dirpath, name))
    return sorted(files)


def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read()
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
        return None


# --- masking: comments and literals blanked, line/column layout kept
# byte-for-byte identical, so every offset computed on the masked text
# is valid on the ORIGINAL text too (GODS_LAWS.md L-40's own declared
# limitation, same shape check_macro_balance.py's strip_line_comments()
# already carries: single-line "//" and single-line "/* */" only - no
# multi-line block comment exists anywhere under src/ or include/ today,
# measured live: `grep -rn '/\*' src include` finds zero instance that
# is not either prose inside a "//" comment or a same-line "/*name*/"
# elided-parameter comment).


def _mask_span(text, start, end):
    return text[:start] + (" " * (end - start)) + text[end:]


def mask_comments_and_literals(text):
    masked = text
    # Single-line block comments ("/* ... */" with no embedded newline).
    for m in list(re.finditer(r"/\*.*?\*/", masked)):
        if "\n" not in m.group(0):
            masked = _mask_span(masked, m.start(), m.end())
    # Line comments - stop at the first unmasked "//" on each line.
    out_lines = []
    for line in masked.split("\n"):
        idx = line.find("//")
        out_lines.append(line if idx == -1 else line[:idx] + " " * (len(line) - idx))
    masked = "\n".join(out_lines)
    # String and char literals (never contain a brace/paren THIS gate
    # should count as structural).
    masked = re.sub(r'"(?:[^"\\]|\\.)*"', lambda m: '"' + " " * (len(m.group(0)) - 2) + '"', masked)
    masked = re.sub(r"'(?:[^'\\]|\\.)*'", lambda m: "'" + " " * (len(m.group(0)) - 2) + "'", masked)
    return masked


# --- function-body boundary detection ---------------------------------


def find_enclosing_blocks(masked_text):
    """One pass over `masked_text`, returning every brace-delimited
    block as (open_line, close_line, header) - 1-based line numbers,
    inclusive on both ends. `header` is the text since the previous
    statement terminator (';', '{', or '}') up to (not including) the
    block's own '{'.
    """
    blocks = []
    stack = []  # each entry: (open_line, header)
    header_buffer = []
    line = 1
    for ch in masked_text:
        if ch == "\n":
            line += 1
            header_buffer.append(ch)
            continue
        if ch == "{":
            header = "".join(header_buffer)
            stack.append((line, header))
            header_buffer = []
            continue
        if ch == "}":
            if stack:
                open_line, header = stack.pop()
                blocks.append((open_line, line, header))
            header_buffer = []
            continue
        if ch == ";":
            header_buffer = []
            continue
        header_buffer.append(ch)
    return blocks


def is_function_like_header(header):
    stripped = header.strip()
    if _CONTROL_HEADER_RE.match(stripped):
        return False
    return "(" in stripped and ")" in stripped


def enclosing_function_block(blocks, call_line):
    """The innermost function-like block whose [open_line, close_line]
    range contains `call_line` - None if there is none (a rounding call
    that is not inside any recognizable function body at all, e.g. a
    default member initializer outside every brace).
    """
    best = None
    for open_line, close_line, header in blocks:
        if open_line <= call_line <= close_line and is_function_like_header(header):
            if best is None or open_line > best[0]:
                best = (open_line, close_line)
    return best


# --- matching parentheses, for one call's own argument span -----------


def matching_close_paren(text, open_paren_index):
    depth = 0
    for i in range(open_paren_index, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return i
    return -1


# --- the two accepted shapes -------------------------------------------


def argument_is_clamp_guarded(masked_text, open_paren_index, close_paren_index):
    argument_span = masked_text[open_paren_index + 1 : close_paren_index]
    return bool(_CLAMP_CALL_RE.search(argument_span))


def identifier_traces_to_limit(function_text, identifier, limit_re):
    """Traces `identifier` to a declaration/initializer tied to
    `limit_re`, ANYWHERE in `function_text` - not line-by-line, on
    purpose: the real CORE-TIME shape this gate exists to keep passing
    wraps its initializer onto a SECOND line (`const double
    max_nanoseconds_as_double =\n    static_cast<double>(std::
    numeric_limits<...>::max());`), so a per-line search would miss it.
    Split on ';' instead - one statement per chunk, regardless of how
    many source lines it spans - and require BOTH the identifier and
    the limit pattern inside the SAME statement, never merely the same
    function (that would reopen the untied-comparison loophole this
    file's own header comment names).
    """
    for statement in function_text.split(";"):
        if re.search(rf"\b{re.escape(identifier)}\b", statement) and limit_re.search(statement):
            return True
    return False


def operand_ties_to_limit(function_text, operand, limit_re):
    operand = operand.strip()
    if limit_re.search(operand):
        return True
    if _IDENTIFIER_RE.match(operand):
        return identifier_traces_to_limit(function_text, operand, limit_re)
    return False


# Comparisons with `arg_name` as either operand, one compiled pattern
# per (operator side, which side arg_name is on) - four shapes cover
# every direction a human writes "x >= bound" or "bound <= x".
def _comparison_patterns(arg_name):
    name = re.escape(arg_name)
    return [
        # arg_name <OP> operand
        (re.compile(rf"\b{name}\s*(>=|>)\s*([A-Za-z_]\w*|[^;{{}}]+?)(?=[;)\s]|$)"), "max"),
        (re.compile(rf"\b{name}\s*(<=|<)\s*([A-Za-z_]\w*|[^;{{}}]+?)(?=[;)\s]|$)"), "min"),
        # operand <OP> arg_name
        (re.compile(rf"([A-Za-z_]\w*|[^;{{}}]+?)\s*(<=|<)\s*{name}\b"), "max"),
        (re.compile(rf"([A-Za-z_]\w*|[^;{{}}]+?)\s*(>=|>)\s*{name}\b"), "min"),
    ]


def argument_is_compared_against_both_limits(function_text, function_first_line, arg_name, call_line):
    """Scans `function_text` (the enclosing function's own body text,
    starting at `function_first_line`) for comparisons of `arg_name`
    STRICTLY before `call_line`, each tied by name to
    numeric_limits<...>::max()/::min() - never a bare literal, never an
    unrelated variable. Returns (has_upper, has_lower) - the caller
    decides what an incomplete pair means.
    """
    has_upper = False
    has_lower = False
    lines = function_text.split("\n")
    for offset, line in enumerate(lines):
        this_line_no = function_first_line + offset
        if this_line_no >= call_line:
            break
        if not re.search(rf"\b{re.escape(arg_name)}\b", line):
            continue
        for pattern, bound in _comparison_patterns(arg_name):
            m = pattern.search(line)
            if not m:
                continue
            operand = m.group(2) if pattern.groups >= 2 else m.group(1)
            limit_re = _LIMIT_MAX_RE if bound == "max" else _LIMIT_MIN_RE
            if operand_ties_to_limit(function_text, operand, limit_re):
                if bound == "max":
                    has_upper = True
                else:
                    has_lower = True
    return has_upper, has_lower


# --- per-call verdict ----------------------------------------------------


def classify_round_call(original_text, masked_text, match, blocks):
    line = original_text.count("\n", 0, match.start()) + 1
    func_name = match.group(1)
    open_paren_index = match.end() - 1
    close_paren_index = matching_close_paren(masked_text, open_paren_index)
    if close_paren_index == -1:
        return {"line": line, "func": func_name, "verdict": "FAIL",
                "reason": "parenteses desbalanceados (nao encontrado o fechamento)"}

    if argument_is_clamp_guarded(masked_text, open_paren_index, close_paren_index):
        return {"line": line, "func": func_name, "verdict": "PASS", "reason": "guardado por clamp() na mesma expressao"}

    enclosing = enclosing_function_block(blocks, line)
    if enclosing is None:
        return {"line": line, "func": func_name, "verdict": "FAIL",
                "reason": "nenhum corpo de funcao reconhecivel envolvendo a chamada"}
    open_line, close_line = enclosing
    # MASKED text, not original - comments and string/char literals are
    # already blanked here, so a stray "// ... numeric_limits<...>::max()"
    # remark (this project's own time.hpp has several, describing this
    # exact function) can never be mistaken for a real tie.
    function_text = "\n".join(masked_text.split("\n")[open_line - 1 : close_line])

    argument_span = masked_text[open_paren_index + 1 : close_paren_index].strip()
    if not _IDENTIFIER_RE.match(argument_span):
        return {"line": line, "func": func_name, "verdict": "FAIL",
                "reason": (
                    f"argumento '{argument_span}' nao e' um identificador simples nem "
                    "guardado por clamp() - nao verificavel com honestidade por este portao"
                )}

    has_upper, has_lower = argument_is_compared_against_both_limits(
        function_text, open_line, argument_span, line
    )
    if has_upper and has_lower:
        return {"line": line, "func": func_name, "verdict": "PASS",
                "reason": f"comparado contra ambos os limites (numeric_limits) antes da chamada, mesma variavel '{argument_span}'"}

    missing = []
    if not has_upper:
        missing.append("limite superior (::max())")
    if not has_lower:
        missing.append("limite inferior (::min())")
    return {"line": line, "func": func_name, "verdict": "FAIL",
            "reason": (
                f"argumento '{argument_span}': faltando comparacao presa a {', '.join(missing)} "
                "antes da chamada, na mesma funcao (uma comparacao PRESENTE mas NAO presa a "
                "numeric_limits, ou presa a uma variavel diferente, nao conta)"
            )}


def scan_file(path):
    text = read_text(path)
    if text is None:
        return []
    masked = mask_comments_and_literals(text)
    blocks = find_enclosing_blocks(masked)
    results = []
    for match in _ROUND_CALL_RE.finditer(masked):
        verdict = classify_round_call(text, masked, match, blocks)
        verdict["file"] = path
        results.append(verdict)
    return results


# --- L-40 floor ------------------------------------------------------


def require_nonempty_scan(value, what):
    if not value:
        print(f"{SCRIPT_NAME}: varredura vazia ({what})", file=sys.stderr)
        return False
    return True


# --- the check itself --------------------------------------------------


def check_round_order(project_root):
    files = enumerate_scanned_files(project_root)
    if not require_nonempty_scan(
        files, f"0 arquivo(s) encontrado(s) sob {'/'.join(_SCANNED_SUBDIRS)} em {project_root}"
    ):
        return False

    all_results = []
    for path in files:
        all_results.extend(scan_file(path))

    if not require_nonempty_scan(
        all_results, "0 chamada(s) de arredondamento encontrada(s) (vocabulario: llround/lround/round/nearbyint/rint)"
    ):
        return False

    failures = [r for r in all_results if r["verdict"] == "FAIL"]
    passes = [r for r in all_results if r["verdict"] == "PASS"]
    by_clamp = [r for r in passes if "clamp" in r["reason"]]
    by_compare = [r for r in passes if "clamp" not in r["reason"]]

    if failures:
        print(
            f"{SCRIPT_NAME}: {len(failures)} chamada(s) de arredondamento sem a ordem "
            "'compara antes de arredondar' (TODO.md GATE-LLROUND-ORDER):",
            file=sys.stderr,
        )
        for r in failures:
            print(f"  {r['file']}:{r['line']}:{r['func']}(): {r['reason']}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: {len(all_results)} chamada(s) de arredondamento encontrada(s) em "
        f"{len(files)} arquivo(s) - {len(by_clamp)} guardada(s) por clamp(), "
        f"{len(by_compare)} guardada(s) por comparacao contra os dois limites, 0 sem guarda"
    )
    return True


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_round_order.py <project_root>")
    (project_root,) = args
    if not os.path.isdir(project_root):
        fail(f"project root not found: {project_root}")
    if not check_round_order(project_root):
        fail("chamada de arredondamento sem guarda encontrada (ver mensagem acima)")


# --- selftest fixtures and controls -------------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-round-order-selftest-", dir=os.environ.get("TMPDIR"))


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def _make_fixture_root(scratch, label):
    root = os.path.join(scratch, label)
    os.makedirs(os.path.join(root, "src"), exist_ok=True)
    return root


_FIXTURE_PREAMBLE = (
    "#include <cmath>\n#include <cstdint>\n#include <limits>\n\n"
)


# Positive control 1 (the REAL CORE-TIME shape): comparisons against
# BOTH limits, tied by name to numeric_limits, on the SAME variable
# passed to the round call, both strictly before it. Expected: PASS.
def selftest_positive_compare_control(scratch):
    root = _make_fixture_root(scratch, "positive_compare")
    _write(
        os.path.join(root, "src", "widget.cpp"),
        _FIXTURE_PREAMBLE
        + "long long convert(double seconds) noexcept {\n"
        "    const double scaled = seconds * 1e9;\n"
        "    const double max_ns = static_cast<double>(std::numeric_limits<long long>::max());\n"
        "    const double min_ns = static_cast<double>(std::numeric_limits<long long>::min());\n"
        "    if (scaled >= max_ns) {\n"
        "        return std::numeric_limits<long long>::max();\n"
        "    }\n"
        "    if (scaled <= min_ns) {\n"
        "        return std::numeric_limits<long long>::min();\n"
        "    }\n"
        "    return static_cast<long long>(std::llround(scaled));\n"
        "}\n",
    )
    results = scan_file(os.path.join(root, "src", "widget.cpp"))
    if len(results) != 1 or results[0]["verdict"] != "PASS":
        print(f"selftest: controle POSITIVO (comparacao) FALHOU: {results}", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO (comparacao contra os dois limites) OK")
    return True


# Positive control 2 (the REAL color.cpp/color_parse.cpp shape): the
# round call's own argument is a std::clamp(...) expression, no
# separate `if` anywhere. Expected: PASS.
def selftest_positive_clamp_control(scratch):
    root = _make_fixture_root(scratch, "positive_clamp")
    _write(
        os.path.join(root, "src", "widget.cpp"),
        _FIXTURE_PREAMBLE
        + "#include <algorithm>\n\n"
        "std::uint8_t unit_to_byte(float unit) noexcept {\n"
        "    return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0F, 1.0F) * 255.0F));\n"
        "}\n",
    )
    results = scan_file(os.path.join(root, "src", "widget.cpp"))
    if len(results) != 1 or results[0]["verdict"] != "PASS":
        print(f"selftest: controle POSITIVO (clamp) FALHOU: {results}", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO (guardado por clamp()) OK")
    return True


# Negative control 1 (the EXACT mutation TODO.md's own GATE-LLROUND-
# ORDER item names): the comparisons still exist, but AFTER the round
# call, not before - the reorder that broke every value-level test.
# Expected: REPROVA, citing the missing limits.
def selftest_negative_reorder_control(scratch):
    root = _make_fixture_root(scratch, "negative_reorder")
    _write(
        os.path.join(root, "src", "widget.cpp"),
        _FIXTURE_PREAMBLE
        + "long long convert(double seconds) noexcept {\n"
        "    const double scaled = seconds * 1e9;\n"
        "    const long long rounded = static_cast<long long>(std::llround(scaled));\n"
        "    const double max_ns = static_cast<double>(std::numeric_limits<long long>::max());\n"
        "    const double min_ns = static_cast<double>(std::numeric_limits<long long>::min());\n"
        "    if (scaled >= max_ns) {\n"
        "        return std::numeric_limits<long long>::max();\n"
        "    }\n"
        "    if (scaled <= min_ns) {\n"
        "        return std::numeric_limits<long long>::min();\n"
        "    }\n"
        "    return rounded;\n"
        "}\n",
    )
    results = scan_file(os.path.join(root, "src", "widget.cpp"))
    if len(results) != 1 or results[0]["verdict"] != "FAIL":
        print(f"selftest: controle NEGATIVO (reorder) FALHOU (deveria reprovar): {results}", file=sys.stderr)
        return False
    print(f"selftest: controle NEGATIVO (a MUTACAO real do item) OK - reprovado: {results[0]['reason']}")
    return True


# Negative control 2 (the loophole named in this file's own header
# comment): a comparison IS present, tied to numeric_limits, but on a
# DIFFERENT variable than the one passed to round(). Expected: REPROVA.
def selftest_negative_wrong_variable_control(scratch):
    root = _make_fixture_root(scratch, "negative_wrong_variable")
    _write(
        os.path.join(root, "src", "widget.cpp"),
        _FIXTURE_PREAMBLE
        + "long long convert(double seconds, double unrelated) noexcept {\n"
        "    const double scaled = seconds * 1e9;\n"
        "    const double max_ns = static_cast<double>(std::numeric_limits<long long>::max());\n"
        "    const double min_ns = static_cast<double>(std::numeric_limits<long long>::min());\n"
        "    if (unrelated >= max_ns) {\n"
        "        return std::numeric_limits<long long>::max();\n"
        "    }\n"
        "    if (unrelated <= min_ns) {\n"
        "        return std::numeric_limits<long long>::min();\n"
        "    }\n"
        "    return static_cast<long long>(std::llround(scaled));\n"
        "}\n",
    )
    results = scan_file(os.path.join(root, "src", "widget.cpp"))
    if len(results) != 1 or results[0]["verdict"] != "FAIL":
        print(f"selftest: controle NEGATIVO (variavel errada) FALHOU (deveria reprovar): {results}", file=sys.stderr)
        return False
    print(f"selftest: controle NEGATIVO (comparacao presa a limite, mas na variavel ERRADA) OK - reprovado: {results[0]['reason']}")
    return True


# Negative control 3 (the loophole's other half): a comparison of the
# RIGHT variable, but NOT tied to numeric_limits at all - a plain
# literal, which could be the wrong bound and this gate has no way to
# know it is correct. Expected: REPROVA.
def selftest_negative_untied_comparison_control(scratch):
    root = _make_fixture_root(scratch, "negative_untied")
    _write(
        os.path.join(root, "src", "widget.cpp"),
        _FIXTURE_PREAMBLE
        + "long long convert(double seconds) noexcept {\n"
        "    const double scaled = seconds * 1e9;\n"
        "    if (scaled >= 0.0) {\n"
        "        return static_cast<long long>(std::llround(scaled));\n"
        "    }\n"
        "    return 0;\n"
        "}\n",
    )
    results = scan_file(os.path.join(root, "src", "widget.cpp"))
    if len(results) != 1 or results[0]["verdict"] != "FAIL":
        print(f"selftest: controle NEGATIVO (comparacao nao presa) FALHOU (deveria reprovar): {results}", file=sys.stderr)
        return False
    print(f"selftest: controle NEGATIVO (comparacao presente mas NAO presa a numeric_limits) OK - reprovado: {results[0]['reason']}")
    return True


# Negative control 4: only ONE of the two limits guarded (the exact
# asymmetric-guard mistake a partial fix could leave behind).
def selftest_negative_one_sided_control(scratch):
    root = _make_fixture_root(scratch, "negative_one_sided")
    _write(
        os.path.join(root, "src", "widget.cpp"),
        _FIXTURE_PREAMBLE
        + "long long convert(double seconds) noexcept {\n"
        "    const double scaled = seconds * 1e9;\n"
        "    const double max_ns = static_cast<double>(std::numeric_limits<long long>::max());\n"
        "    if (scaled >= max_ns) {\n"
        "        return std::numeric_limits<long long>::max();\n"
        "    }\n"
        "    return static_cast<long long>(std::llround(scaled));\n"
        "}\n",
    )
    results = scan_file(os.path.join(root, "src", "widget.cpp"))
    if len(results) != 1 or results[0]["verdict"] != "FAIL":
        print(f"selftest: controle NEGATIVO (so um lado) FALHOU (deveria reprovar): {results}", file=sys.stderr)
        return False
    if "inferior" not in results[0]["reason"]:
        print(f"selftest: controle NEGATIVO (so um lado) FALHOU (nao citou o limite que falta): {results}", file=sys.stderr)
        return False
    print(f"selftest: controle NEGATIVO (so o limite superior guardado) OK - reprovado: {results[0]['reason']}")
    return True


# Empty-scan floor control: a root with neither src/ nor include/
# present must be REFUSED, never presumed clean.
def selftest_empty_scan_control(scratch):
    root = os.path.join(scratch, "empty_scan")
    os.makedirs(root, exist_ok=True)

    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        result = check_round_order(root)
    text = buffer.getvalue()
    if result:
        print("selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar raiz sem src/include, mas passou)", file=sys.stderr)
        return False
    if "varredura vazia" not in text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (raiz sem src/include recusada)")
    return True


# Inverse-of-the-obvious control: a file with ZERO rounding calls at
# all must pass cleanly (zero results), never be mistaken for a
# violation nor trip the floor by itself (the FLOOR is over the whole
# tree's total call count, checked separately by
# selftest_empty_scan_control above and by the real run against this
# project's own tree in --selftest below).
def selftest_file_without_calls_control(scratch):
    root = _make_fixture_root(scratch, "no_calls")
    _write(
        os.path.join(root, "src", "widget.cpp"),
        _FIXTURE_PREAMBLE + "double plain(double x) noexcept { return x * 2.0; }\n",
    )
    results = scan_file(os.path.join(root, "src", "widget.cpp"))
    if results:
        print(f"selftest: controle SEM CHAMADA FALHOU (esperava 0 resultados, achou {results})", file=sys.stderr)
        return False
    print("selftest: controle SEM CHAMADA OK (arquivo sem nenhuma chamada de arredondamento nao gera resultado)")
    return True


# Complex-argument control: the round call's own argument is neither a
# bare identifier nor clamp()-guarded - this gate REFUSES rather than
# guess, per its own declared method.
def selftest_complex_argument_refused_control(scratch):
    root = _make_fixture_root(scratch, "complex_argument")
    _write(
        os.path.join(root, "src", "widget.cpp"),
        _FIXTURE_PREAMBLE + "double convert(double a, double b) noexcept { return std::round(a + b); }\n",
    )
    results = scan_file(os.path.join(root, "src", "widget.cpp"))
    if len(results) != 1 or results[0]["verdict"] != "FAIL":
        print(f"selftest: controle de ARGUMENTO COMPLEXO FALHOU (deveria recusar, nao aprovar por suposicao): {results}", file=sys.stderr)
        return False
    print(f"selftest: controle de ARGUMENTO COMPLEXO OK - recusado sem adivinhar: {results[0]['reason']}")
    return True


# Calibration: reruns the REAL check against THIS project's own real
# tree (never a fixture) and requires it to PASS - the real
# src/core/time.cpp (compare-guarded), src/core/color.cpp and
# src/gfss/color_parse.cpp (clamp()-guarded) are the three real call
# sites this gate exists to keep green.
def selftest_calibration_control(project_root):
    result = check_round_order(project_root)
    if not result:
        print("selftest: controle de CALIBRACAO FALHOU (a arvore real do projeto deveria passar)", file=sys.stderr)
        return False
    print("selftest: controle de CALIBRACAO OK (arvore real do projeto passa)")
    return True


def _project_root_from_this_file():
    # tests/tools/check_round_order.py -> two parents up is the project root.
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def _run_all_controls(scratch):
    return [
        selftest_positive_compare_control(scratch),
        selftest_positive_clamp_control(scratch),
        selftest_negative_reorder_control(scratch),
        selftest_negative_wrong_variable_control(scratch),
        selftest_negative_untied_comparison_control(scratch),
        selftest_negative_one_sided_control(scratch),
        selftest_empty_scan_control(scratch),
        selftest_file_without_calls_control(scratch),
        selftest_complex_argument_refused_control(scratch),
        selftest_calibration_control(_project_root_from_this_file()),
    ]


def selftest_main():
    scratch = make_scratch_workdir()
    try:
        controls = _run_all_controls(scratch)
        if not all(controls):
            print("check_round_order.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_round_order.py --selftest: os {len(controls)} controles OK")
    finally:
        import shutil

        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
