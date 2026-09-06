#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_self_registration_pinned.py - FACADE-PIN, G2 (docs/plano-
# conserto-fachadas-uaf.md sec. 9, GODS_LAWS.md L-19/L-36/L-40): the
# portao that covers what G1 (the pinned_adapter<A> concept, platform/
# port/adapter_pin.hpp) structurally CANNOT reach - a class that
# registers its own address with the operating system but is never
# selected through display_connection_port/window_adapter_port/
# gl_context_adapter_port in the first place. wayland_shell_adapter and
# wayland_seat_adapter (this plan's own varredura #2/#3) are exactly
# that: xdg_wm_base_add_listener()/wl_seat_add_listener() take `this`,
# but neither class is ever named in a `requires display_connection_
# port<A>` clause, so G1's compile-time check never even looks at them.
#
# THE RULE: any class whose OWN .cpp file contains one of the four
# self-registration needles below (_add_listener(, CreateWindowExW with
# `this` as the lpParam, GWLP_USERDATA, lpParam) must have, in its own
# .hpp file, BOTH deleted move special members - `ClassName(ClassName
# &&) = delete;` and `ClassName &operator=(ClassName &&) = delete;`,
# literally, the exact shape every adapter in this fatia was edited to
# have. A class that registers `this` and still compiles a move
# constructor is the THIRD ENCARNACAO this plan's own sec. 6 table
# warns about - a future adapter that forgets D-UAF-1 reproves here,
# lexically, even if it never touches a port concept at all.
#
# LEXICAL, NOT A C++ PARSER (same discipline check_layers.py/check_
# no_x11.py already use for this project's own gates): a needle found
# in a .cpp maps to the class declared in the SIBLING .hpp of the same
# base name - true for every file in src/platform/wayland/ and
# src/platform/win32/ today (one class per file, this project's own
# L-17 atomization). A future file that breaks this one-class-per-file
# convention is exactly the kind of drift the adversarial reviewer
# checks for, not something this script tries to parse around.
#
# Usage:
#   check_self_registration_pinned.py <source-root-directory>
#   check_self_registration_pinned.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_self_registration_pinned.py"

# Three of the four self-registration needles this plan's own sec. 9
# names, verbatim - unambiguous by themselves in this project's own
# vocabulary. `CreateWindowExW(..., this)` is checked SEPARATELY below
# (_create_window_ex_w_registers_this): a bare "CreateWindowExW(" also
# matches a throwaway helper window with lpParam=nullptr (wgl_extension_
# loader.cpp's own dummy context window, which owns no state to
# register at all) - the needle this plan names is the CALL WITH
# `this`, not the mere presence of the function name.
_REGISTRATION_NEEDLES = (
    "_add_listener(",
    "GWLP_USERDATA",
    "lpParam",
)

# Matches CreateWindowExW(...) allowing ONE level of nested parens
# inside an argument (this project's own calls nest exactly one, e.g.
# ::GetModuleHandleW(nullptr)) - deliberately not a full C++ parser,
# same lexical-scan discipline this file's own header comment already
# names for check_layers.py/check_no_x11.py.
_CREATE_WINDOW_EX_W_CALL_PATTERN = re.compile(
    r"CreateWindowExW\(((?:[^()]|\([^()]*\))*)\)", re.DOTALL
)
_THIS_WORD_PATTERN = re.compile(r"\bthis\b")


def _create_window_ex_w_registers_this(code_text):
    for match in _CREATE_WINDOW_EX_W_CALL_PATTERN.finditer(code_text):
        if _THIS_WORD_PATTERN.search(match.group(1)):
            return True
    return False

_SCAN_SUBDIRS = (os.path.join("src", "platform", "wayland"), os.path.join("src", "platform", "win32"))

# Real declaration only, not a mention inside a comment ("the same
# class ... already gives" reads as "class now" to a naive \bclass\s+
# (\w+)\b - measured against this project's own header comments, which
# talk ABOUT a class constantly): requires `{` or `:` right after the
# name (a base-clause or the opening brace), which prose never has.
_CLASS_DECL_PATTERN = re.compile(r"\bclass\s+(\w+)\s*[:{]")

# Strips `//` line comments and `/* */` block comments before any
# regex runs against a file's own text - both class_name_in_header()
# and header_declares_pinned_moves() below need this, or a comment
# that happens to mention "class X" or paste a `= delete;` shape in
# prose (this file's own header comment does exactly that, describing
# what the fix looks like) would be read as real code.
_COMMENT_PATTERN = re.compile(r"//[^\n]*|/\*.*?\*/", re.DOTALL)


def _strip_comments(text):
    return _COMMENT_PATTERN.sub(" ", text)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def _read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read()
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
        return None


def cpp_files_with_registration_site(root):
    """Every .cpp under the two scanned subdirectories that contains at
    least one of the four needles OUTSIDE a comment - the SITES this
    gate exists to enumerate. Returns a list of (path, code_text) pairs,
    sorted, so the scan is deterministic across platforms (os.walk's
    own directory order is not guaranteed). `code_text` has comments
    already stripped, ready for every downstream check to reuse.
    """
    sites = []
    for rel_dir in _SCAN_SUBDIRS:
        abs_dir = os.path.join(root, rel_dir)
        if not os.path.isdir(abs_dir):
            continue
        for name in sorted(os.listdir(abs_dir)):
            if not name.endswith(".cpp"):
                continue
            path = os.path.join(abs_dir, name)
            text = _read_text(path)
            if text is None:
                continue
            code_text = _strip_comments(text)
            has_site = any(needle in code_text for needle in _REGISTRATION_NEEDLES)
            has_site = has_site or _create_window_ex_w_registers_this(code_text)
            if has_site:
                sites.append((path, code_text))
    return sites


def sibling_header_path(cpp_path):
    base, _ext = os.path.splitext(cpp_path)
    return base + ".hpp"


def class_name_in_header(header_code_text):
    match = _CLASS_DECL_PATTERN.search(header_code_text)
    return match.group(1) if match else None


def header_declares_pinned_moves(header_code_text, class_name):
    """Both deleted move special members, literally, the exact shape
    every adapter in this fatia was edited to have - see this file's
    own header comment for why a lexical match is enough here.
    """
    move_ctor_pattern = re.compile(
        rf"\b{re.escape(class_name)}\s*\(\s*{re.escape(class_name)}\s*&&\s*\)\s*=\s*delete\s*;"
    )
    move_assign_pattern = re.compile(
        rf"\boperator\s*=\s*\(\s*{re.escape(class_name)}\s*&&\s*\)\s*=\s*delete\s*;"
    )
    return bool(move_ctor_pattern.search(header_code_text)) and bool(
        move_assign_pattern.search(header_code_text)
    )


# The actual gate logic, factored out of real_main() so --selftest
# exercises the EXACT same function - not a reimplementation that could
# drift from production.
def check_self_registration_pinned(root):
    sites = cpp_files_with_registration_site(root)
    site_count = len(sites)

    # GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero sites found
    # is not "nothing to report" - it means the scan itself is broken
    # (wrong root, renamed subdirectory, needle list gone stale), and
    # this project has at least wayland_display_adapter.cpp/window_
    # adapter.cpp/shell_adapter.cpp registering `this` today.
    if site_count == 0:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 sitio(s) de registro encontrado(s) sob "
            f"{'/'.join(_SCAN_SUBDIRS)}) - GODS_LAWS.md L-40",
            file=sys.stderr,
        )
        return False

    classes_checked = 0
    approved = 0
    failures = []

    for cpp_path, _cpp_code_text in sites:
        header_path = sibling_header_path(cpp_path)
        header_text = _read_text(header_path)
        if header_text is None:
            failures.append(f"{cpp_path}: sibling header not found or unreadable ({header_path})")
            continue
        header_code_text = _strip_comments(header_text)

        class_name = class_name_in_header(header_code_text)
        if class_name is None:
            failures.append(f"{header_path}: no \"class NAME\" declaration found")
            continue

        classes_checked += 1
        if header_declares_pinned_moves(header_code_text, class_name):
            approved += 1
        else:
            failures.append(
                f"{header_path}: class {class_name} registers `this` in {cpp_path} but does not "
                f"delete BOTH move special members ({class_name}(&&)=delete and "
                "operator=(&&)=delete) - GODS_LAWS.md L-19/L-36 (FACADE-PIN, D-UAF-1)"
            )

    print(
        f"{SCRIPT_NAME}: sitios={site_count} classes={classes_checked} aprovadas={approved} "
        f"reprovadas={len(failures)}"
    )

    if failures:
        print(f"{SCRIPT_NAME}: reprovacoes:", file=sys.stderr)
        for line in failures:
            print(line, file=sys.stderr)
        return False

    return True


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_self_registration_pinned.py <source-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_self_registration_pinned(root):
        fail("self-registration pin violation found (GODS_LAWS.md L-19/L-36; see message above)")


# --- fixtures and controls for --selftest -----------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(
        prefix="glintfx-self-registration-pinned-selftest-", dir=os.environ.get("TMPDIR")
    )


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


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


_PINNED_HEADER_TEMPLATE = """// fixture
#pragma once
namespace glintfx::platform {{
class {name} {{
  public:
    {name}() noexcept = default;
    {name}(const {name} &) = delete;
    {name} &operator=(const {name} &) = delete;
    {name}({name} &&) = delete;
    {name} &operator=({name} &&) = delete;
}};
}} // namespace glintfx::platform
"""

_MOVABLE_HEADER_TEMPLATE = """// fixture
#pragma once
namespace glintfx::platform {{
class {name} {{
  public:
    {name}() noexcept = default;
    {name}(const {name} &) = delete;
    {name} &operator=(const {name} &) = delete;
    {name}({name} &&other) noexcept;
    {name} &operator=({name} &&other) noexcept;
}};
}} // namespace glintfx::platform
"""

_REGISTERING_CPP_TEMPLATE = """// fixture
#include "{name}.hpp"
namespace glintfx::platform {{
void register_it() {{
    wl_surface_add_listener(nullptr, nullptr, this);
}}
}} // namespace glintfx::platform
"""


# Positive control: a class that registers `this` AND deletes both move
# special members. Expected: passes, one class approved.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    _write(
        os.path.join(root, "src", "platform", "wayland", "pinned_thing.hpp"),
        _PINNED_HEADER_TEMPLATE.format(name="pinned_thing"),
    )
    _write(
        os.path.join(root, "src", "platform", "wayland", "pinned_thing.cpp"),
        _REGISTERING_CPP_TEMPLATE.format(name="pinned_thing"),
    )

    outcome = capture(lambda: check_self_registration_pinned(root))
    if outcome.result and "reprovadas=0" in outcome.text:
        print("selftest: controle POSITIVO OK (classe pinned aprovada)")
        return True
    print(
        "selftest: controle POSITIVO FALHOU (classe pinned deveria ter sido aprovada)",
        file=sys.stderr,
    )
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: a class that registers `this` but is still movable.
# Expected: reproves and cites the header.
def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    header = os.path.join(root, "src", "platform", "wayland", "movable_thing.hpp")
    _write(header, _MOVABLE_HEADER_TEMPLATE.format(name="movable_thing"))
    _write(
        os.path.join(root, "src", "platform", "wayland", "movable_thing.cpp"),
        _REGISTERING_CPP_TEMPLATE.format(name="movable_thing"),
    )

    outcome = capture(lambda: check_self_registration_pinned(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO FALHOU (classe movel que registra `this` deveria ter "
            "reprovado)",
            file=sys.stderr,
        )
        return False
    if header not in outcome.text:
        print(
            f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou {header})",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (classe movel que registra `this` pega e citada)")
    return True


# Empty-scan floor: neither scanned subdirectory exists at all.
# Expected: reproves with "varredura vazia" in the message.
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)

    outcome = capture(lambda: check_self_registration_pinned(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (raiz sem src/platform/wayland nem "
            "src/platform/win32 deveria ter sido recusada, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura "
            "vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (raiz sem os subdiretorios recusada)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
        ]
        if not all(controls):
            print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")
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
