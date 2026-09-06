#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_container_fixture_includes.py - closes the blind spot the
# 06/09/2026 fix to prepare_arch_ports_fixture.sh's own copy_source_tree()
# opens by construction: staging the WHOLE src/, include/ and
# tests/parity/ trees (instead of a hand-named list of directories)
# means a future header can move, get renamed, or move to a directory
# this script never learns about - and a bare `docker build` only finds
# out three fixtures and eleven minutes in, when one particular `g++`
# invocation dies with "No such file or directory" (the exact failure
# GODS_LAWS.md L-36's own header comment on this fatia's sibling gate,
# check_container_fixture_inventory.py, already names as the shape to
# never let happen silently).
#
# WHAT THIS SCRIPT DOES: reads tests/container/Containerfile's own
# `g++`/`gcc` invocations, treats every `/build/**.cpp` token they name
# as a translation unit (TU), and walks the REAL `#include` closure of
# each one against what prepare_arch_ports_fixture.sh's own
# copy_source_tree() actually staged - never against what that script
# only INTENDS to stage (GODS_LAWS.md L-36: a producer never grades its
# own homework, so this file reads none of that script's own internals,
# only the two ground truths any checkout already has: the Containerfile
# text, and the staged directory tree on disk).
#
# THE THREE OUTCOMES FOR A NAME THAT DOES NOT RESOLVE IN THE FIXTURE:
#   - it is a header `wayland-scanner client-header` generates INSIDE
#     the image at build time (xdg-shell-client-protocol.h) - never a
#     real file in this checkout at all, so it is counted, not chased;
#   - it is a genuine PROJECT header that exists in the real tree but
#     under a root this run of the fixture script did not stage - the
#     one class of defect this gate exists to catch, reported by name,
#     includer, and the real path where it actually lives;
#   - it is a system/third-party header (`<wayland-client.h>`, `<EGL/
#     egl.h>`, `<vector>`) that this checkout never owns at all.
#
# Usage:
#   check_container_fixture_includes.py --compare <Containerfile> <context-dir> <staged-dir> <repo-root>
#   check_container_fixture_includes.py --selftest
#
# Wired into tests/CMakeLists.txt as container_fixture_includes_selftest
# ONLY (real mode has no ctest case: it depends on the staged directory
# tree, which only tests/container/prepare_arch_ports_fixture.sh's own
# `sh` script produces - the same reason container_fixture_inventory_test
# right above it in that file DOES register in real mode and this one
# does not is that THAT gate only ever reads two plain-text files
# already present in any checkout, and this one needs a staged tree that
# is not). The real mode runs at the END of prepare_arch_ports_fixture.sh
# itself, which .github/workflows/ci.yml's own `wayland-container` job
# already invokes before `docker build` - GODS_LAWS.md L-40's own
# fail-fast principle, applied here so a staging gap reproves before the
# far more expensive docker build ever starts.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import sys
import tempfile

SCRIPT_NAME = "check_container_fixture_includes.py"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def read_lines_lenient(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.readlines()


# --- parsing the Containerfile's own RUN invocations --------------------


_TU_RE = re.compile(r"^/build/\S+\.cpp$")
_GENERATED_HEADER_RE = re.compile(r"^/build/\S+\.h$")


# Dockerfile `RUN` statements span multiple physical lines joined by a
# trailing `\` - this reconstructs each RUN statement as ONE logical
# string (leading "RUN" kept off; see parse_containerfile() below),
# never attempting to interpret anything outside RUN blocks (COPY/FROM
# lines carry no g++ invocation and are not this function's concern).
def extract_run_blocks(containerfile_text):
    lines = containerfile_text.splitlines()
    blocks = []
    i = 0
    n = len(lines)
    while i < n:
        if lines[i].lstrip().startswith("RUN "):
            parts = []
            while True:
                stripped = lines[i].rstrip()
                if stripped.endswith("\\"):
                    parts.append(stripped[:-1].strip())
                    i += 1
                else:
                    parts.append(stripped.strip())
                    i += 1
                    break
            joined = " ".join(parts)
            if joined.startswith("RUN "):
                joined = joined[len("RUN "):]
            blocks.append(joined)
            continue
        i += 1
    return blocks


# A RUN block chains sub-commands with literal ` && ` (this Containerfile
# never puts `&&` inside a `$(...)` substitution) - split on it, never on
# `&&` alone, so a stray `&&` glued to another token cannot mis-split.
def split_subcommands(run_block_text):
    return [chunk.strip() for chunk in re.split(r"\s&&\s", run_block_text) if chunk.strip()]


# Returns (tu_order, tu_include_roots, generated_header_tokens):
#   tu_order: `/build/**.cpp` tokens, in first-seen order, never repeated
#   tu_include_roots: {tu_token: [raw "-I" tokens, in order]} for the
#     EXACT g++/gcc invocation that names that TU
#   generated_header_tokens: `/build/**.h` tokens that appear on a
#     `wayland-scanner client-header` sub-command - never a real file in
#     this checkout, always produced fresh inside the image
def parse_containerfile(containerfile_text):
    tu_order = []
    tu_include_roots = {}
    generated_header_tokens = set()

    for run_block in extract_run_blocks(containerfile_text):
        for subcmd in split_subcommands(run_block):
            tokens = subcmd.split()
            if not tokens:
                continue
            if tokens[0] in ("g++", "gcc"):
                roots = [tokens[i + 1] for i, tok in enumerate(tokens) if tok == "-I" and i + 1 < len(tokens)]
                for tok in tokens:
                    if _TU_RE.match(tok) and tok not in tu_include_roots:
                        tu_order.append(tok)
                        tu_include_roots[tok] = roots
            elif tokens[0] == "wayland-scanner" and len(tokens) > 1 and tokens[1] == "client-header":
                for tok in tokens:
                    if _GENERATED_HEADER_RE.match(tok):
                        generated_header_tokens.add(tok)

    return tu_order, tu_include_roots, generated_header_tokens


# --- mapping a Containerfile `/build/...` token to a real path ----------


_STAGED_PREFIX = "/build/_arch_ports_src/"


# `/build/_arch_ports_src/<rel>` was COPYed from the staged tree
# (tests/container/_arch_ports_src/<rel> on the real filesystem);
# every other `/build/<x>` was COPYed straight from the build CONTEXT
# (tests/container/<x>) - see the Containerfile's own COPY lines. A
# token outside both shapes is not a path this gate can map (never
# reached in practice: every token that survives the TU/`-I` regexes
# above already starts with `/build`).
def map_build_path(token, context_dir, staged_dir):
    if token == "/build/_arch_ports_src":
        return staged_dir
    if token.startswith(_STAGED_PREFIX):
        return os.path.join(staged_dir, token[len(_STAGED_PREFIX):])
    if token == "/build":
        return context_dir
    if token.startswith("/build/"):
        return os.path.join(context_dir, token[len("/build/"):])
    return None


# --- resolving one #include name against the fixture ---------------------


_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')


# Quoted names resolve first against the includer's OWN directory, then
# against every `-I` root in order (matching how quoted resolution
# already behaves in this codebase - e.g. window_facade.cpp's own
# `#include "platform/window/window_impl.hpp"` resolves via the `-I src`
# root, not relative to its own directory). Angle-bracket names skip the
# includer's own directory and only ever search the `-I` roots.
def resolve_in_fixture(name, is_quoted, includer_dir, mapped_roots):
    if is_quoted:
        candidate = os.path.join(includer_dir, name)
        if os.path.isfile(candidate):
            return os.path.normpath(candidate)
    for root in mapped_roots:
        candidate = os.path.join(root, name)
        if os.path.isfile(candidate):
            return os.path.normpath(candidate)
    return None


# Maps a file already resolved somewhere in the fixture back to "the
# directory this file really lives in, in the actual repository" - used
# only to look up a NOT-yet-resolved sibling include relative to that
# real location (the fourth fallback in search_repo_fallback() below). A
# file under staged_dir mirrors repo_root/<same relative path>; a file
# under context_dir (tests/container/) already IS a real repository
# path, unchanged.
def real_dir_of_includer(includer_path, staged_dir, repo_root):
    includer_dir = os.path.dirname(includer_path)
    staged_prefix = staged_dir.rstrip(os.sep) + os.sep
    if includer_path.startswith(staged_prefix):
        rel = os.path.relpath(includer_dir, staged_dir)
        return os.path.normpath(os.path.join(repo_root, rel))
    return includer_dir


# The fallback this gate exists to run: a name that resolved NOWHERE in
# the fixture, checked against the three fixed top-level roots this
# project's own real tree is built from, plus the includer's own real
# (repository) directory - never a full recursive search, the same
# "closed, cheap, named" shape GODS_LAWS.md L-40 already asks of every
# other gate's own vocabulary in this project.
def search_repo_fallback(name, includer_path, staged_dir, repo_root):
    candidates = [
        os.path.join(repo_root, "include", name),
        os.path.join(repo_root, "src", name),
        os.path.join(repo_root, "tests", "container", name),
        os.path.join(real_dir_of_includer(includer_path, staged_dir, repo_root), name),
    ]
    for candidate in candidates:
        if os.path.isfile(candidate):
            return os.path.normpath(candidate)
    return None


# "src", "include" or "tests" - whichever top-level root of repo_root a
# resolved real path falls under, for the FALTANDO message's own "raiz
# nao estagiada" field.
def root_label_for(real_path, repo_root):
    rel = os.path.relpath(real_path, repo_root)
    return rel.split(os.sep)[0]


def relpath_for_display(path, repo_root):
    try:
        return os.path.relpath(path, repo_root)
    except ValueError:
        return path


# --- walking one TU's full #include closure ------------------------------


# Mutates the four shared accumulators (never returns a copy) so the
# SAME header, reached from two different TUs, is only ever walked and
# counted once - a header's own #include closure does not depend on
# which TU pulled it in, so re-walking it a second time would only ever
# rediscover the same names (GODS_LAWS.md L-11: no repeated work per
# item scanned, not just no repeated process).
def walk_tu(tu_path, mapped_roots, generated_basenames, staged_dir, repo_root,
            resolved_headers, generated_hits, externals, missing):
    stack = [tu_path]
    walked = set()
    while stack:
        current = stack.pop()
        if current in walked:
            continue
        walked.add(current)
        includer_dir = os.path.dirname(current)
        for lineno, line in enumerate(read_lines_lenient(current), start=1):
            match = _INCLUDE_RE.match(line)
            if not match:
                continue
            quote_char, name = match.group(1), match.group(2)
            is_quoted = quote_char == '"'

            resolved = resolve_in_fixture(name, is_quoted, includer_dir, mapped_roots)
            if resolved is not None:
                if resolved not in resolved_headers:
                    resolved_headers.add(resolved)
                    stack.append(resolved)
                continue

            if os.path.basename(name) in generated_basenames:
                generated_hits.add(name)
                continue

            real_hit = search_repo_fallback(name, current, staged_dir, repo_root)
            if real_hit is not None:
                missing.append({
                    "name": name,
                    "includer": relpath_for_display(current, repo_root),
                    "line": lineno,
                    "real_path": relpath_for_display(real_hit, repo_root),
                    "root_label": root_label_for(real_hit, repo_root),
                })
                continue

            externals.add(name)


# --- the comparison itself, factored out for --selftest ------------------


# Returns (summary_dict, errors) - summary is printed UNCONDITIONALLY,
# errors decide pass/fail (GODS_LAWS.md L-40: the count appears even
# when the gate passes, so "passou" and "nunca varreu" are never the
# same output).
def run_comparison(containerfile_text, context_dir, staged_dir, repo_root):
    tu_order, tu_include_roots, generated_tokens = parse_containerfile(containerfile_text)
    generated_basenames = {os.path.basename(tok) for tok in generated_tokens}

    present_count = 0
    resolved_headers = set()
    generated_hits = set()
    externals = set()
    missing = []

    for tu in tu_order:
        mapped_tu = map_build_path(tu, context_dir, staged_dir)
        if mapped_tu is None or not os.path.isfile(mapped_tu):
            continue
        present_count += 1
        mapped_roots = []
        for raw_root in tu_include_roots.get(tu, []):
            mapped_root = map_build_path(raw_root, context_dir, staged_dir)
            if mapped_root is not None:
                mapped_roots.append(mapped_root)
        walk_tu(mapped_tu, mapped_roots, generated_basenames, staged_dir, repo_root,
                resolved_headers, generated_hits, externals, missing)

    summary = {
        "tu_total": len(tu_order),
        "tu_present": present_count,
        "resolved": len(resolved_headers),
        "external": len(externals),
        "generated": len(generated_hits),
        "missing": missing,
    }

    errors = []
    if summary["tu_total"] == 0:
        errors.append(
            "varredura vazia: nenhuma TU (/build/*.cpp numa invocacao g++/gcc) encontrada em "
            "tests/container/Containerfile - GODS_LAWS.md L-40, isto e sinal de coleta quebrada"
        )
    elif summary["tu_present"] != summary["tu_total"]:
        errors.append(
            f"{summary['tu_total'] - summary['tu_present']} de {summary['tu_total']} TU(s) do Containerfile "
            "nao existem na fixture/contexto (COPY sem arquivo correspondente)"
        )
    if summary["resolved"] == 0 and summary["tu_present"] > 0:
        errors.append(
            "varredura vazia: nenhum header do projeto foi resolvido na fixture a partir de nenhuma TU "
            "presente - GODS_LAWS.md L-40, isto e sinal de coleta quebrada"
        )
    for miss in missing:
        errors.append(
            f"{miss['name']}: incluido por {miss['includer']}:{miss['line']}, existe em "
            f"{miss['real_path']}, raiz nao estagiada: {miss['root_label']}"
        )

    return summary, errors


def print_summary(summary):
    print(
        f"{SCRIPT_NAME}: TU(s) no Containerfile: {summary['tu_total']} | "
        f"presentes na fixture: {summary['tu_present']} | "
        f"headers do projeto resolvidos: {summary['resolved']} | "
        f"externos: {summary['external']} | "
        f"gerados na imagem: {summary['generated']} | "
        f"faltando: {len(summary['missing'])}"
    )


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 4:
        fail(
            "usage: check_container_fixture_includes.py --compare "
            "<Containerfile> <context-dir> <staged-dir> <repo-root>"
        )
    containerfile_path, context_dir, staged_dir, repo_root = args
    containerfile_text = read_file(containerfile_path)

    summary, errors = run_comparison(containerfile_text, context_dir, staged_dir, repo_root)
    print_summary(summary)

    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: OK - toda TU presente, includes do projeto resolvidos na fixture")


# --- fixtures and controls for --selftest -----------------------------


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def _make_scratch():
    return tempfile.mkdtemp(prefix="glintfx-fixture-includes-selftest-", dir=os.environ.get("TMPDIR"))


# Builds one minimal, hermetic repo/context/staged triple: a repo_root
# with real src/ and include/ trees, a context_dir (tests/container/,
# holding one smoke .cpp), and a staged_dir this control may sabotage
# before calling run_comparison() - the same "sabotage a COPY, never the
# tracked original" shape GODS_LAWS.md L-27 requires, applied here to a
# throwaway fixture instead of the real tree.
def _build_base_fixture(scratch, label):
    root = os.path.join(scratch, label)
    context_dir = os.path.join(root, "tests", "container")
    staged_dir = os.path.join(context_dir, "_arch_ports_src")

    _write(os.path.join(root, "src", "core", "widget.hpp"), "#pragma once\nint widget();\n")
    _write(os.path.join(context_dir, "main_smoke.cpp"), '#include "core/widget.hpp"\nint main() { return widget(); }\n')

    containerfile = (
        "FROM fedora:44 AS arch-ports-builder\n"
        "RUN dnf -y install gcc-c++ \\\n"
        "    && dnf clean all\n"
        "COPY _arch_ports_src /build/_arch_ports_src\n"
        "COPY main_smoke.cpp /build/main_smoke.cpp\n"
        "RUN g++ -std=c++23 -O2 -Wall -Wextra -Werror \\\n"
        "        -I /build/_arch_ports_src/include -I /build/_arch_ports_src/src \\\n"
        "        -o /build/main_smoke \\\n"
        "        /build/main_smoke.cpp\n"
    )

    for rel in ("src/core/widget.hpp",):
        _write(os.path.join(staged_dir, rel), read_file_direct(os.path.join(root, rel)))

    return root, context_dir, staged_dir, containerfile


def read_file_direct(path):
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def selftest_positive_control(scratch):
    root, context_dir, staged_dir, containerfile = _build_base_fixture(scratch, "positive")
    summary, errors = run_comparison(containerfile, context_dir, staged_dir, root)
    if errors:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    if summary["tu_total"] != 1 or summary["tu_present"] != 1 or summary["resolved"] != 1:
        print(f"selftest: controle POSITIVO FALHOU (contagens inesperadas: {summary})", file=sys.stderr)
        return False
    print(f"selftest: controle POSITIVO OK (fixture minima resolve por completo: {summary})")
    return True


# VERMELHO#1 (a razao deste portao existir): um header foi removido da
# fixture ESTAGIADA (sabotagem, GODS_LAWS.md L-27 - nunca do arquivo
# rastreado real) mas continua existindo no "repo" sintetico - a mesma
# forma exata do achado real F3/5.3 do plano.
def selftest_missing_from_stage_reproves(scratch):
    root, context_dir, staged_dir, containerfile = _build_base_fixture(scratch, "missing-from-stage")
    os.remove(os.path.join(staged_dir, "src", "core", "widget.hpp"))

    summary, errors = run_comparison(containerfile, context_dir, staged_dir, root)
    if not errors:
        print("selftest: VERMELHO#1 FALHOU (header removido da fixture deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("widget.hpp" in e and "raiz nao estagiada: src" in e and "incluido por" in e for e in errors):
        print(f"selftest: VERMELHO#1 FALHOU (reprovou, mas sem citar includente/raiz): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#1 OK (header presente no repo mas ausente da fixture, pego e citado): {errors}")
    return True


# VERMELHO#2: uma TU citada no Containerfile mas o COPY correspondente
# nunca aconteceu (arquivo ausente do contexto).
def selftest_missing_tu_reproves(scratch):
    root, context_dir, staged_dir, _containerfile = _build_base_fixture(scratch, "missing-tu")
    containerfile = (
        "FROM fedora:44 AS arch-ports-builder\n"
        "RUN dnf -y install gcc-c++\n"
        "COPY _arch_ports_src /build/_arch_ports_src\n"
        "RUN g++ -std=c++23 -O2 -Wall -Wextra -Werror \\\n"
        "        -I /build/_arch_ports_src/include -I /build/_arch_ports_src/src \\\n"
        "        -o /build/never_copied \\\n"
        "        /build/never_copied.cpp\n"
    )
    summary, errors = run_comparison(containerfile, context_dir, staged_dir, root)
    if not errors:
        print("selftest: VERMELHO#2 FALHOU (TU sem COPY correspondente deveria ter reprovado)", file=sys.stderr)
        return False
    if summary["tu_total"] != 1 or summary["tu_present"] != 0:
        print(f"selftest: VERMELHO#2 FALHOU (contagens erradas: {summary})", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#2 OK (TU ausente da fixture pega): {errors}")
    return True


# VERMELHO#3: Containerfile sem nenhuma invocacao g++/gcc (piso de
# varredura vazia, GODS_LAWS.md L-40).
def selftest_empty_containerfile_reproves():
    containerfile = "FROM fedora:44\nRUN dnf -y install gcc-c++\n"
    summary, errors = run_comparison(containerfile, "/nonexistent/context", "/nonexistent/staged", "/nonexistent/repo")
    if not errors:
        print("selftest: VERMELHO#3 FALHOU (Containerfile sem g++/gcc deveria ter reprovado)", file=sys.stderr)
        return False
    if summary["tu_total"] != 0 or not any("varredura vazia" in e for e in errors):
        print(f"selftest: VERMELHO#3 FALHOU (sem 'varredura vazia'/TU=0): {summary} {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#3 OK (Containerfile sem TU nenhuma pego): {errors}")
    return True


# VERMELHO#4: TU presente mas o unico #include dela e' de sistema - zero
# header do projeto resolvido (piso de varredura vazia do LADO dos
# headers, distinto do piso de TUs do VERMELHO#3).
def selftest_only_system_include_reproves(scratch):
    root = os.path.join(scratch, "only-system")
    context_dir = os.path.join(root, "tests", "container")
    staged_dir = os.path.join(context_dir, "_arch_ports_src")
    _write(os.path.join(context_dir, "vector_smoke.cpp"), "#include <vector>\nint main() { return 0; }\n")
    os.makedirs(staged_dir, exist_ok=True)

    containerfile = (
        "FROM fedora:44 AS arch-ports-builder\n"
        "COPY _arch_ports_src /build/_arch_ports_src\n"
        "COPY vector_smoke.cpp /build/vector_smoke.cpp\n"
        "RUN g++ -std=c++23 -O2 -Wall -Wextra -Werror \\\n"
        "        -I /build/_arch_ports_src/include -I /build/_arch_ports_src/src \\\n"
        "        -o /build/vector_smoke \\\n"
        "        /build/vector_smoke.cpp\n"
    )
    summary, errors = run_comparison(containerfile, context_dir, staged_dir, root)
    if not errors:
        print("selftest: VERMELHO#4 FALHOU (TU so' com <vector> deveria ter reprovado por zero headers)", file=sys.stderr)
        return False
    if summary["resolved"] != 0 or not any("nenhum header do projeto" in e for e in errors):
        print(f"selftest: VERMELHO#4 FALHOU (contagens/mensagem erradas): {summary} {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#4 OK (TU sem nenhum header do projeto pega): {errors}")
    return True


# Controle POSITIVO adicional: header GERADO na imagem (wayland-scanner
# client-header) nao e' tratado como faltando nem como externo - conta
# como "gerado" e passa.
def selftest_generated_header_passes(scratch):
    root = os.path.join(scratch, "generated")
    context_dir = os.path.join(root, "tests", "container")
    staged_dir = os.path.join(context_dir, "_arch_ports_src")
    _write(os.path.join(root, "src", "core", "widget.hpp"), "#pragma once\nint widget();\n")
    _write(os.path.join(staged_dir, "src", "core", "widget.hpp"), "#pragma once\nint widget();\n")
    _write(
        os.path.join(context_dir, "proto_smoke.cpp"),
        '#include "core/widget.hpp"\n#include <fake-proto.h>\nint main() { return widget(); }\n',
    )

    containerfile = (
        "FROM fedora:44 AS arch-ports-builder\n"
        "COPY _arch_ports_src /build/_arch_ports_src\n"
        "COPY proto_smoke.cpp /build/proto_smoke.cpp\n"
        "RUN wayland-scanner client-header \"$(pkg-config --variable=pkgdatadir wayland-protocols)/x.xml\" \\\n"
        "        /build/fake-proto.h \\\n"
        "    && g++ -std=c++23 -O2 -Wall -Wextra -Werror \\\n"
        "        -I /build/_arch_ports_src/include -I /build/_arch_ports_src/src -I /build \\\n"
        "        -o /build/proto_smoke \\\n"
        "        /build/_arch_ports_src/src/core/widget.hpp \\\n"
        "        /build/proto_smoke.cpp\n"
    )
    # widget.hpp above is NOT a .cpp so it never counts as a TU (regex is
    # anchored on .cpp) - included here only so this fixture's own
    # Containerfile text is realistic; the real TU is proto_smoke.cpp.
    summary, errors = run_comparison(containerfile, context_dir, staged_dir, root)
    if errors:
        print(f"selftest: controle GERADO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    if summary["generated"] != 1 or summary["resolved"] != 1:
        print(f"selftest: controle GERADO FALHOU (contagens erradas): {summary}", file=sys.stderr)
        return False
    print(f"selftest: controle GERADO OK (header gerado na imagem contado, nao reprovado): {summary}")
    return True


# Controle POSITIVO adicional: header de SISTEMA (nunca gerado por este
# projeto, nunca existente em lugar nenhum do repo sintetico) conta como
# externo e passa.
def selftest_system_header_passes(scratch):
    root, context_dir, staged_dir, _containerfile = _build_base_fixture(scratch, "system-header")
    containerfile = (
        "FROM fedora:44 AS arch-ports-builder\n"
        "COPY _arch_ports_src /build/_arch_ports_src\n"
        "COPY main_smoke.cpp /build/main_smoke.cpp\n"
        "RUN g++ -std=c++23 -O2 -Wall -Wextra -Werror \\\n"
        "        -I /build/_arch_ports_src/include -I /build/_arch_ports_src/src \\\n"
        "        -o /build/main_smoke \\\n"
        "        /build/main_smoke.cpp\n"
    )
    _write(
        os.path.join(context_dir, "main_smoke.cpp"),
        '#include "core/widget.hpp"\n#include <some-system-header.h>\nint main() { return widget(); }\n',
    )
    summary, errors = run_comparison(containerfile, context_dir, staged_dir, root)
    if errors:
        print(f"selftest: controle EXTERNO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    if summary["external"] != 1:
        print(f"selftest: controle EXTERNO FALHOU (esperava 1 externo, veio {summary})", file=sys.stderr)
        return False
    print(f"selftest: controle EXTERNO OK (header de sistema contado como externo, nao reprovado): {summary}")
    return True


def selftest_main():
    scratch = _make_scratch()
    controls = [
        selftest_positive_control(scratch),
        selftest_missing_from_stage_reproves(scratch),
        selftest_missing_tu_reproves(scratch),
        selftest_empty_containerfile_reproves(),
        selftest_only_system_include_reproves(scratch),
        selftest_generated_header_passes(scratch),
        selftest_system_header_passes(scratch),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--compare":
        real_main(args[1:])
    else:
        fail(
            "usage: check_container_fixture_includes.py --compare "
            "<Containerfile> <context-dir> <staged-dir> <repo-root>  |  --selftest"
        )


if __name__ == "__main__":
    main()
