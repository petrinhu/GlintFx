#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# worktree_build_links.sh - GODS_LAWS.md L-55, condition 1: a working copy
# under worktrees/ holds only the source; every build directory lives in
# /var/tmp and is reached through a symlink. Idempotent.
#
# Usage (from inside the working copy): tools/worktree_build_links.sh
# Env: GLINTFX_WT_BUILD_ROOT overrides /var/tmp/<worktree-name>-build.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -f "$ROOT_DIR/.git" ]]; then
    echo "worktree_build_links.sh: $ROOT_DIR is not a linked working copy (no .git file); refusing" >&2
    exit 1
fi

name="$(basename "$ROOT_DIR")"
build_root="${GLINTFX_WT_BUILD_ROOT:-/var/tmp/${name}-build}"
mkdir -p "$build_root"

linked=0
for dir_name in build-preci build-preci-sanitize build-preci-debug build-preci-clang; do
    target="$build_root/$dir_name"
    link="$ROOT_DIR/$dir_name"
    mkdir -p "$target"
    if [[ -L "$link" ]]; then
        [[ "$(readlink "$link")" == "$target" ]] || { echo "worktree_build_links.sh: $link points elsewhere; refusing" >&2; exit 1; }
    elif [[ -e "$link" ]]; then
        echo "worktree_build_links.sh: $link exists and is not a link; refusing" >&2
        exit 1
    else
        ln -s "$target" "$link"
    fi
    linked=$((linked + 1))
done
echo "worktree_build_links.sh: $linked build directories linked under $build_root"
[[ "$linked" -gt 0 ]] || exit 1
