#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_facade_export_boundary.py - GODS_LAWS.md L-36/L-40 gate.
#
# CI run 34067447918 (commit ddd40b3): win32_facade_pin_test recompiled
# src/platform/window/display_facade.cpp/window_facade.cpp/src/core/
# err.cpp/err_code.cpp DIRECTLY, without linking glintfx::glintfx and
# without defining GLINTFX_LIBRARY_STATIC_DEFINE - the real cl.exe (not
# a Linux-configured cmake's __attribute__((visibility)) stand-in,
# tools/msvc-container/README.md) refuses to let the SAME translation
# unit both IMPORT (include/glintfx/platform/window/display.hpp's own
# GLINTFX_API declarations, __declspec(dllimport) with no DLL ever
# built here) and DEFINE a symbol: C4273 "inconsistent dll linkage",
# escalated to a hard error by /WX. Four Windows jobs fell.
#
# WHY NO LOCAL GATE CAUGHT THIS: win32_facade_pin_test is `if(WIN32)`-
# guarded (tests/CMakeLists.txt), so it never even EXISTS as a target
# on the machine that wrote it (Fedora, no MSVC). And on GCC/Clang the
# analogous mistake is invisible by construction - the Linux-flavored
# export.hpp's GLINTFX_API expands to `__attribute__((visibility
# ("default")))` on BOTH sides of the `#ifdef ..._EXPORTS` branch (see
# that generated header's own two identical branches), so there is no
# import/export ASYMMETRY for GCC to ever warn about. Only a REAL
# Windows configure (server) or a REAL cl.exe (tools/msvc-container/,
# which did not exist when win32_facade_pin_test was written) can ever
# observe this class of defect - this gate closes that gap WITHOUT
# needing either: it is a structural read of tests/CMakeLists.txt, not
# a compile.
#
# THE INVARIANT, computed mechanically (never a curated list of file
# names that rots, GODS_LAWS.md L-36's own "closed by FORM, not by
# list" precedent - see check_dep_zero.py's own header): a ".cpp under
# src/" is a BOUNDARY file iff it textually defines an out-of-line
# member (`Class::method(`) or free function named after some
# GLINTFX_API-decorated declaration under include/glintfx/. Any test
# target (tests/CMakeLists.txt) that lists a boundary file as a source,
# and neither links `glintfx::glintfx` nor defines
# `GLINTFX_LIBRARY_STATIC_DEFINE` anywhere in that SAME target's own
# block, reproves - the exact shape win32_facade_pin_test had.
#
# Usage:
#   check_facade_export_boundary.py <repo-root>
#   check_facade_export_boundary.py --selftest

import re
import sys
import tempfile
from pathlib import Path

SCRIPT_NAME = "check_facade_export_boundary.py"

_API_DECL_RE = re.compile(
    r"GLINTFX_API\s*(?:static\s+)?(?:\[\[nodiscard\]\]\s*)?[\w:<>&*,\s]*?\b(\w+)\s*\("
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


_CLASS_OPEN_RE = re.compile(r"\b(?:class|struct)\s+(\w+)\b[^;{]*\{")


def collect_api_symbol_names(include_dir):
    """Every GLINTFX_API-decorated declaration, as a (class_name, method_
    name) pair for a class member (class_name tracked via a simple brace-
    depth state machine - one class/struct open per header is the norm
    here, and this project's own headers never nest one GLINTFX_API type
    inside another), or (None, function_name) for a free function. A bare
    method NAME ALONE is not enough to grep a *.cpp for a plausible out-
    of-line definition: adapter classes with unrelated internal linkage
    share generic names (open/close/is_open/pump_events) with the public
    facades they back (measured false positives: display_adapter.cpp/
    window_state.cpp/seat_adapter.cpp all define an unrelated `open`/
    `close` of their own) - qualifying by class is what tells them apart."""
    pairs = set()
    for header in sorted(Path(include_dir).rglob("*.hpp")):
        text = header.read_text(encoding="utf-8", errors="replace")
        depth = 0
        current_class = None
        class_depth = None
        for line in text.splitlines():
            class_open = _CLASS_OPEN_RE.search(line)
            if class_open and current_class is None:
                current_class = class_open.group(1)
                class_depth = depth
            if "GLINTFX_API" in line:
                match = _API_DECL_RE.search(line)
                if match:
                    pairs.add((current_class, match.group(1)))
            depth += line.count("{") - line.count("}")
            if current_class is not None and depth <= class_depth:
                current_class = None
                class_depth = None
    return pairs


_DEFINITION_LOOKAHEAD = 500

# Characters that, found immediately before the matched name (skipping
# whitespace), mean this occurrence continues an EXPRESSION - a CALL,
# never a DEFINITION. A definition's own name is the first token of a
# statement: preceded by a return-type word, or by `;`/`}`/`{`/start-of-
# file - never by one of these.
_EXPRESSION_CONTINUATION_CHARS = set("=(,.&|+-*/!<>:")


def _is_definition_not_call(text, match_start, match_end):
    """Two independent signals, BOTH required (either alone produced a
    measured false positive - src/gfss/color_parse.cpp's own aggregate-
    initializer CALL to gltfx_rgba_from_srgb8(), `{.value = glintfx::
    gltfx_rgba_from_srgb8(encoded), .diagnostic = {}}` - the lookahead
    alone sees the LATER `{}` of `.diagnostic = {}` before the closing
    `;` and mistakes it for a definition body):
    1. LOOKAHEAD - a definition's parameter list is followed, once
       balanced, by `{` (possibly after `noexcept`/a multi-line list)
       with no `;` first; a call's closing `)` is followed by `;` or by
       an enclosing expression's own continuation well before any `{`.
    2. LOOKBEHIND - the character immediately before the matched name
        (skipping whitespace) must NOT be an expression-continuation
        character (see _EXPRESSION_CONTINUATION_CHARS) - a call is
        always the right-hand side of `=`, an argument after `(`/`,`, or
        similar; a definition's name is the first token of a statement."""
    window = text[match_end : match_end + _DEFINITION_LOOKAHEAD]
    brace_pos = window.find("{")
    semi_pos = window.find(";")
    if brace_pos == -1:
        return False
    if semi_pos != -1 and brace_pos >= semi_pos:
        return False

    before = text[:match_start].rstrip()
    if before and before[-1] in _EXPRESSION_CONTINUATION_CHARS:
        return False
    if before.endswith("return"):
        return False
    return True


def find_boundary_cpp_files(src_dir, api_pairs):
    """Every *.cpp under src_dir that contains an out-of-line DEFINITION
    (never a mere CALL - see _is_definition_not_call()) matching one of
    api_pairs: `ClassName::method(` for a class member, or a bare
    `function_name(` (GLINTFX_API free functions in this project are all
    distinctively `gltfx_*`-prefixed - see this file's own header
    comment) for a free function. Measured false positive this
    distinction fixes: src/gfss/color_parse.cpp merely CALLS
    gltfx_rgba_from_srgb8() - a bare-name match without it flagged that
    file as if it DEFINED the function."""
    boundary = set()
    for cpp in sorted(Path(src_dir).rglob("*.cpp")):
        text = cpp.read_text(encoding="utf-8", errors="replace")
        for class_name, method_name in api_pairs:
            if class_name is None:
                pattern = rf"(?<!::){re.escape(method_name)}\s*\("
            else:
                pattern = rf"\b{re.escape(class_name)}\s*::\s*{re.escape(method_name)}\s*\("
            found = False
            for match in re.finditer(pattern, text):
                if _is_definition_not_call(text, match.start(), match.end()):
                    found = True
                    break
            if found:
                boundary.add(cpp.resolve())
                break
    return boundary


def split_into_target_blocks(text):
    """Splits tests/CMakeLists.txt's text into (target_name, block_text)
    pairs, one per add_executable()/glintfx_add_test() call - a block
    runs from that call to the line right before the NEXT such call (or
    EOF). glintfx_add_test(name) targets always link glintfx::glintfx
    (cmake/GlintfxTest.cmake's own function body) and never appear as
    add_executable() blocks with source lists of their own, so they are
    never candidates for THIS gate - only recorded here so a later
    add_executable() block does not accidentally swallow their text."""
    starts = []
    for match in re.finditer(r"^\s*(?:add_executable|glintfx_add_test)\(\s*(\S+)", text, re.MULTILINE):
        starts.append((match.start(), match.group(1)))
    blocks = []
    for i, (pos, name) in enumerate(starts):
        end = starts[i + 1][0] if i + 1 < len(starts) else len(text)
        blocks.append((name, text[pos:end]))
    return blocks


def block_lists_source(block_text, cpp_path, src_dir):
    rel = str(cpp_path.relative_to(Path(src_dir).parent))
    return rel in block_text or cpp_path.name in block_text


def block_is_exempt(block_text):
    return "glintfx::glintfx" in block_text or "GLINTFX_LIBRARY_STATIC_DEFINE" in block_text


def check_facade_export_boundary(repo_root):
    include_dir = Path(repo_root) / "include" / "glintfx"
    src_dir = Path(repo_root) / "src"
    cmake_file = Path(repo_root) / "tests" / "CMakeLists.txt"

    if not include_dir.is_dir():
        print(f"{SCRIPT_NAME}: diretorio nao encontrado: {include_dir}", file=sys.stderr)
        return False
    if not src_dir.is_dir():
        print(f"{SCRIPT_NAME}: diretorio nao encontrado: {src_dir}", file=sys.stderr)
        return False
    try:
        cmake_text = cmake_file.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"{SCRIPT_NAME}: nao foi possivel ler {cmake_file} ({exc})", file=sys.stderr)
        return False

    api_names = collect_api_symbol_names(include_dir)
    boundary_files = find_boundary_cpp_files(src_dir, api_names)
    blocks = split_into_target_blocks(cmake_text)

    violations = []
    for target_name, block_text in blocks:
        if block_is_exempt(block_text):
            continue
        for cpp_path in boundary_files:
            if block_lists_source(block_text, cpp_path, src_dir):
                violations.append((target_name, cpp_path.name))

    if violations:
        print(
            f"{SCRIPT_NAME}: PROIBIDO ({len(violations)} alvo(s) recompila(m) fonte de fronteira "
            f"GLINTFX_API sem 'glintfx::glintfx' nem 'GLINTFX_LIBRARY_STATIC_DEFINE'):",
            file=sys.stderr,
        )
        for target_name, cpp_name in sorted(violations):
            print(f"  {target_name}: {cpp_name}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: {len(boundary_files)} arquivo(s) de fronteira, {len(blocks)} alvo(s) "
        f"varrido(s), 0 violacao(oes)"
    )
    return True


# --- selftest ------------------------------------------------------------


def selftest_positive_boundary_detected(tmp_dir):
    include_dir = Path(tmp_dir) / "include" / "glintfx"
    include_dir.mkdir(parents=True, exist_ok=True)
    (include_dir / "thing.hpp").write_text(
        "class gltfx_thing {\n public:\n  GLINTFX_API static gltfx_thing open() noexcept;\n};\n",
        encoding="utf-8",
    )
    src_dir = Path(tmp_dir) / "src"
    src_dir.mkdir(parents=True, exist_ok=True)
    (src_dir / "thing_facade.cpp").write_text(
        "gltfx_thing gltfx_thing::open() noexcept { return {}; }\n", encoding="utf-8"
    )
    api_pairs = collect_api_symbol_names(include_dir)
    if ("gltfx_thing", "open") not in api_pairs:
        print(
            f"selftest: controle POSITIVE-BOUNDARY FALHOU (par ('gltfx_thing', 'open') nao "
            f"extraido, achou {api_pairs})",
            file=sys.stderr,
        )
        return False
    boundary = find_boundary_cpp_files(src_dir, api_pairs)
    if len(boundary) != 1:
        print(
            f"selftest: controle POSITIVE-BOUNDARY FALHOU (esperava 1 arquivo de fronteira, achou {len(boundary)})",
            file=sys.stderr,
        )
        return False
    print("selftest: controle POSITIVE-BOUNDARY OK (thing_facade.cpp reconhecido como fronteira)")
    return True


def selftest_non_boundary_not_flagged(tmp_dir):
    include_dir = Path(tmp_dir) / "include2" / "glintfx"
    include_dir.mkdir(parents=True, exist_ok=True)
    (include_dir / "thing.hpp").write_text(
        "class gltfx_thing {\n public:\n  GLINTFX_API static gltfx_thing open() noexcept;\n};\n",
        encoding="utf-8",
    )
    src_dir = Path(tmp_dir) / "src2"
    src_dir.mkdir(parents=True, exist_ok=True)
    (src_dir / "pure_atom.cpp").write_text(
        "int internal_only_helper() { return 42; }\n", encoding="utf-8"
    )
    api_names = collect_api_symbol_names(include_dir)
    boundary = find_boundary_cpp_files(src_dir, api_names)
    if boundary:
        print("selftest: controle NON-BOUNDARY FALHOU (arquivo interno marcado como fronteira)", file=sys.stderr)
        return False
    print("selftest: controle NON-BOUNDARY OK (arquivo interno nao marcado como fronteira)")
    return True


def selftest_violation_detected(tmp_dir):
    repo = Path(tmp_dir) / "repo_violation"
    (repo / "include" / "glintfx").mkdir(parents=True, exist_ok=True)
    (repo / "src" / "platform").mkdir(parents=True, exist_ok=True)
    (repo / "tests").mkdir(parents=True, exist_ok=True)
    (repo / "include" / "glintfx" / "thing.hpp").write_text(
        "class gltfx_thing {\n public:\n  GLINTFX_API static gltfx_thing open() noexcept;\n};\n",
        encoding="utf-8",
    )
    (repo / "src" / "platform" / "thing_facade.cpp").write_text(
        "gltfx_thing gltfx_thing::open() noexcept { return {}; }\n", encoding="utf-8"
    )
    (repo / "tests" / "CMakeLists.txt").write_text(
        'add_executable(bad_test "thing_test.cpp"\n'
        '    "${PROJECT_SOURCE_DIR}/src/platform/thing_facade.cpp"\n'
        ")\n"
        "target_link_libraries(bad_test PRIVATE glintfx_test_harness)\n",
        encoding="utf-8",
    )
    ok = check_facade_export_boundary(repo)
    if ok:
        print("selftest: controle VIOLATION-DETECTED FALHOU (deveria reprovar bad_test)", file=sys.stderr)
        return False
    print("selftest: controle VIOLATION-DETECTED OK (bad_test reprovado)")
    return True


def selftest_exempt_by_link_passes(tmp_dir):
    repo = Path(tmp_dir) / "repo_exempt_link"
    (repo / "include" / "glintfx").mkdir(parents=True, exist_ok=True)
    (repo / "src" / "platform").mkdir(parents=True, exist_ok=True)
    (repo / "tests").mkdir(parents=True, exist_ok=True)
    (repo / "include" / "glintfx" / "thing.hpp").write_text(
        "class gltfx_thing {\n public:\n  GLINTFX_API static gltfx_thing open() noexcept;\n};\n",
        encoding="utf-8",
    )
    (repo / "src" / "platform" / "thing_facade.cpp").write_text(
        "gltfx_thing gltfx_thing::open() noexcept { return {}; }\n", encoding="utf-8"
    )
    (repo / "tests" / "CMakeLists.txt").write_text(
        'add_executable(good_test "thing_test.cpp"\n'
        '    "${PROJECT_SOURCE_DIR}/src/platform/thing_facade.cpp"\n'
        ")\n"
        "target_link_libraries(good_test PRIVATE glintfx::glintfx glintfx_test_harness)\n",
        encoding="utf-8",
    )
    ok = check_facade_export_boundary(repo)
    if not ok:
        print("selftest: controle EXEMPT-BY-LINK FALHOU (good_test nao deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle EXEMPT-BY-LINK OK (link glintfx::glintfx isenta o alvo)")
    return True


def selftest_exempt_by_static_define_passes(tmp_dir):
    repo = Path(tmp_dir) / "repo_exempt_define"
    (repo / "include" / "glintfx").mkdir(parents=True, exist_ok=True)
    (repo / "src" / "platform").mkdir(parents=True, exist_ok=True)
    (repo / "tests").mkdir(parents=True, exist_ok=True)
    (repo / "include" / "glintfx" / "thing.hpp").write_text(
        "class gltfx_thing {\n public:\n  GLINTFX_API static gltfx_thing open() noexcept;\n};\n",
        encoding="utf-8",
    )
    (repo / "src" / "platform" / "thing_facade.cpp").write_text(
        "gltfx_thing gltfx_thing::open() noexcept { return {}; }\n", encoding="utf-8"
    )
    (repo / "tests" / "CMakeLists.txt").write_text(
        'add_executable(win_test "thing_test.cpp"\n'
        '    "${PROJECT_SOURCE_DIR}/src/platform/thing_facade.cpp"\n'
        ")\n"
        "target_compile_definitions(win_test PRIVATE GLINTFX_LIBRARY_STATIC_DEFINE)\n"
        "target_link_libraries(win_test PRIVATE glintfx_test_harness)\n",
        encoding="utf-8",
    )
    ok = check_facade_export_boundary(repo)
    if not ok:
        print(
            "selftest: controle EXEMPT-BY-STATIC-DEFINE FALHOU (win_test nao deveria reprovar)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle EXEMPT-BY-STATIC-DEFINE OK (GLINTFX_LIBRARY_STATIC_DEFINE isenta o alvo)")
    return True


def selftest_real_tree_passes(repo_root):
    ok = check_facade_export_boundary(repo_root)
    if not ok:
        print("selftest: controle REAL-TREE FALHOU (a arvore real deveria passar hoje)", file=sys.stderr)
        return False
    print("selftest: controle REAL-TREE OK (a arvore real deste repositorio passa)")
    return True


def selftest_main():
    repo_root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="glintfx-facade-export-boundary-selftest-") as tmp_dir:
        controls = [
            selftest_positive_boundary_detected(tmp_dir),
            selftest_non_boundary_not_flagged(tmp_dir),
            selftest_violation_detected(tmp_dir),
            selftest_exempt_by_link_passes(tmp_dir),
            selftest_exempt_by_static_define_passes(tmp_dir),
            selftest_real_tree_passes(repo_root),
        ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def real_main(args):
    if len(args) != 1:
        fail("usage: check_facade_export_boundary.py <repo-root>")
    root = args[0]
    if not Path(root).is_dir():
        fail(f"directory not found: {root}")
    if not check_facade_export_boundary(root):
        fail("alvo(s) recompilam fonte de fronteira GLINTFX_API sem resolver o macro de export (ver mensagem acima)")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
