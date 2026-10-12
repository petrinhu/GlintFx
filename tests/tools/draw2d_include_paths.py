# SPDX-License-Identifier: AGPL-3.0-or-later
#
# draw2d_include_paths.py - the path side of the draw2d direction gate (CONTRACT.md 5.3). It gives every path the
# compiler wrote one comparable form, and holds the single rule that names the forbidden paths: the public gfss
# folder (installed or generated) and the internal src/gfss/. The other modules of the gate judge through these
# two functions only.
#
# Standard library only (L-07). Identifiers and comments in English (L-21).

import posixpath
import re

DRIVE_PREFIX = re.compile(r"^[A-Za-z]:/")


def normalize_path(path, base):
    """The comparable form of a path: forward slashes, joined to base when relative, normalized, casefolded."""
    unified = path.replace("\\", "/")
    if not unified.startswith("/") and not DRIVE_PREFIX.match(unified):
        unified = posixpath.join(base, unified)
    return posixpath.normpath(unified).casefold()


def display(path, root):
    """The path relative to root, for the report; the path itself when it lies outside root."""
    prefix = normalize_path(root, "/") + "/"
    return path[len(prefix):] if path.startswith(prefix) else path


def is_forbidden(dep, source):
    """True when a dependency reaches gfss: the public folder, installed or generated, or the internal src/gfss/."""
    path = normalize_path(dep, "/")
    internal = normalize_path(source + "/src/gfss", "/") + "/"
    return "/glintfx/gfss/" in path or path.startswith(internal)
