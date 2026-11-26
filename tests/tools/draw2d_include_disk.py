# SPDX-License-Identifier: AGPL-3.0-or-later
#
# draw2d_include_disk.py - the disk side of the draw2d direction gate (CONTRACT.md 5.3). It enumerates every file under
# src/draw2d and include/glintfx/draw2d from the disk, not from git, so an untracked new file is seen, and sorts
# each one into the closed classes: source, header, ignored by name, or unknown (refused by name). Zero files is
# a refusal (L-40).
#
# Standard library only (L-07). Identifiers and comments in English (L-21); messages to the user in
# pt-br, without accents.

import os
import posixpath

INTERNAL_DIR = "src/draw2d"
PUBLIC_DIR = "include/glintfx/draw2d"
SOURCE_EXTENSIONS = (".cpp",)
HEADER_EXTENSIONS = (".hpp", ".h", ".hh", ".hxx", ".inl", ".ipp", ".inc")
IGNORED_NAMES = ("CMakeLists.txt",)


def list_draw2d_files(source):
    """Every file under the two draw2d folders, recursive, read from the disk, as paths relative to source."""
    found = []
    for folder in (INTERNAL_DIR, PUBLIC_DIR):
        for current, _folders, names in os.walk(os.path.join(source, folder)):
            for name in names:
                full = os.path.join(current, name)
                found.append(os.path.relpath(full, source).replace(os.sep, "/"))
    return sorted(found)


def classify_file(rel):
    """The class of one file by the closed table: source, header, ignored by name, or unknown (refused)."""
    name = posixpath.basename(rel)
    extension = posixpath.splitext(name)[1]
    if name in IGNORED_NAMES:
        return "ignored"
    if extension in SOURCE_EXTENSIONS:
        return "source"
    if extension in HEADER_EXTENSIONS:
        return "header"
    return "unknown"


def classify_draw2d_files(source):
    """Sort every draw2d file into (sources, headers, unknown), as relative paths in order."""
    sources, headers, unknown = [], [], []
    for rel in list_draw2d_files(source):
        kind = classify_file(rel)
        if kind == "source":
            sources.append(rel)
        elif kind == "header":
            headers.append(rel)
        elif kind == "unknown":
            unknown.append(rel)
    return sources, headers, unknown


def scan_disk(source):
    """The disk side of the gate: (sources, headers, problems). An unknown extension is a named problem, and a sweep
    that found nothing is a refusal that names the root (L-40)."""
    sources, headers, unknown = classify_draw2d_files(source)
    problems = [f"extensao fora da lista, arquivo nao julgavel: {rel}" for rel in unknown]
    if not sources and not headers:
        problems.append(f"varredura vazia, nenhum arquivo de draw2d em {source}")
    return sources, headers, problems
