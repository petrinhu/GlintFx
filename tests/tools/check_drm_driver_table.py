#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_drm_driver_table.py - GL-GPU-KIND (docs/plano-w6b-fatias-5b-
# revisao.md sec. 3/4.3, D-W6b-30/38): proves the driver-name table
# include/glintfx/platform/gl/gpu.hpp's own header comment documents
# (a marked line, "GLINTFX-DRM-DRIVER-TABLE: name1 name2 ...") is the
# SAME set of names src/platform/wayland/drm_gpu_kind.cpp actually
# branches on - a name added to one and forgotten in the other is
# exactly the class of bug drm_gpu_kind_test's own fixture cells cannot
# catch (they only prove the CLASSIFIER's rules given a driver name;
# they never look at what the header PROMISES a reader that name list
# is).
#
# WRITTEN IN PYTHON FROM THE START, NOT SH (GATE-TREE-PARITY, GODS_
# LAWS.md L-04, decisao do lider: "O comportamento deve ser igual em
# qualquer OS"): this repository's OWN history already paid for the sh-
# then-port lesson once (check_no_x11.py's own header names it) - both
# input files this gate reads are plain, cross-platform text, so there
# is no reason to write a POSIX-sh-only gate here and repeat that
# fixed mistake a second time.
#
# GODS_LAWS.md L-40 (piso de varredura nao-vazia): a comparison that
# finds 0 names on both sides "matches" by accident, never by proof -
# this gate refuses that silently-vacuous pass explicitly.
#
# Usage:
#   check_drm_driver_table.py <glintfx-source-dir>
#   check_drm_driver_table.py --selftest
#
# --selftest runs five controls (match, count-mismatch, name-mismatch,
# empty-header, empty-classifier) against throwaway fixtures under
# tempfile.mkdtemp - never against this repository's own current
# gpu.hpp/drm_gpu_kind.cpp (GODS_LAWS.md L-36: a portao proves itself
# red on a KNOWN-BAD input, not by hoping the real tree happens to be
# wrong today).
#
# Each function below does one thing (GODS_LAWS.md L-17).

import re
import sys
from pathlib import Path

SCRIPT_NAME = "check_drm_driver_table.py"

HEADER_TABLE_RE = re.compile(r"GLINTFX-DRM-DRIVER-TABLE:\s*(.*)")
CLASSIFIER_NAME_RE = re.compile(r'facts\.driver == "([a-z0-9_-]+)"')


def fail(message: str) -> None:
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def header_driver_names(header_text: str) -> set[str]:
    """Names on the ONE marked line - never the whole comment block, so
    prose elsewhere in gpu.hpp can name `nvidia-drm`/`nouveau` freely
    without this gate mistaking prose for the table itself."""
    names: set[str] = set()
    for line in header_text.splitlines():
        match = HEADER_TABLE_RE.search(line)
        if match:
            names.update(match.group(1).split())
    return names


def classifier_driver_names(classifier_text: str) -> set[str]:
    """Every driver name drm_gpu_kind.cpp's own if-chain compares
    `facts.driver` against - a plain regex over the literal comparison
    form, never a C++ parser (the same "shallow but honest" shape
    check_dep_zero.py already uses for CMake calls it cannot fully
    parse)."""
    return set(CLASSIFIER_NAME_RE.findall(classifier_text))


def compare_driver_names(label: str, header_text: str, classifier_text: str) -> None:
    header_names = header_driver_names(header_text)
    classifier_names = classifier_driver_names(classifier_text)

    print(
        f"{SCRIPT_NAME}: {label}: header={len(header_names)} "
        f"classificador={len(classifier_names)}"
    )

    if len(header_names) == 0:
        fail(f"{label}: GLINTFX-DRM-DRIVER-TABLE line missing or empty - non-empty scan required (L-40)")
    if len(classifier_names) == 0:
        fail(f'{label}: no facts.driver == "..." comparisons found - non-empty scan required (L-40)')

    only_in_header = sorted(header_names - classifier_names)
    only_in_classifier = sorted(classifier_names - header_names)

    if only_in_header or only_in_classifier:
        parts = []
        if only_in_header:
            parts.append(f"in the table but NOT branched on: {' '.join(only_in_header)}")
        if only_in_classifier:
            parts.append(f"branched on but NOT in the table: {' '.join(only_in_classifier)}")
        fail(f"{label}: driver name(s) mismatch - " + "; ".join(parts))

    print(f"ok ({label}): the same {len(header_names)} driver(s) named on both sides.")


def run_real(source_dir: Path) -> None:
    header_path = source_dir / "include" / "glintfx" / "platform" / "gl" / "gpu.hpp"
    classifier_path = source_dir / "src" / "platform" / "wayland" / "drm_gpu_kind.cpp"

    if not header_path.is_file():
        fail(f"gpu.hpp not found: {header_path}")
    if not classifier_path.is_file():
        fail(f"drm_gpu_kind.cpp not found: {classifier_path}")

    compare_driver_names(
        "real tree", header_path.read_text(encoding="utf-8"), classifier_path.read_text(encoding="utf-8")
    )


def classifier_source(names: list[str]) -> str:
    return "\n".join(f'if (facts.driver == "{name}") {{}}' for name in names)


def header_source(names: list[str]) -> str:
    return f"// GLINTFX-DRM-DRIVER-TABLE: {' '.join(names)}\n"


def expect_pass(label: str, header_names: list[str], classifier_names: list[str]) -> None:
    try:
        compare_driver_names(label, header_source(header_names), classifier_source(classifier_names))
    except SystemExit:
        print(f"{SCRIPT_NAME} --selftest: FALHOU: {label} should have passed", file=sys.stderr)
        raise
    print(f"selftest: {label} OK")


def expect_fail(label: str, header_names: list[str], classifier_names: list[str]) -> None:
    try:
        compare_driver_names(label, header_source(header_names), classifier_source(classifier_names))
    except SystemExit:
        print(f"selftest: {label} OK (reproved as expected)")
        return
    print(f"{SCRIPT_NAME} --selftest: FALHOU: {label} should have failed", file=sys.stderr)
    sys.exit(1)


def selftest() -> None:
    controls = 0

    expect_pass("MATCH control", ["amdgpu", "i915"], ["amdgpu", "i915"])
    controls += 1

    expect_fail("COUNT-MISMATCH control", ["amdgpu"], ["amdgpu", "i915"])
    controls += 1

    expect_fail("NAME-MISMATCH control", ["amdgpu", "xe"], ["amdgpu", "i915"])
    controls += 1

    expect_fail("EMPTY-HEADER control", [], ["amdgpu"])
    controls += 1

    expect_fail("EMPTY-CLASSIFIER control", ["amdgpu"], [])
    controls += 1

    print(f"{SCRIPT_NAME} --selftest: all {controls} controls OK")


def main() -> None:
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        selftest()
        return

    if len(sys.argv) != 2:
        fail("usage: check_drm_driver_table.py <glintfx-source-dir> | --selftest")

    run_real(Path(sys.argv[1]))


if __name__ == "__main__":
    main()
