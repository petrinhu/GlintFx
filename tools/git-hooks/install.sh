#!/usr/bin/env sh
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/git-hooks/install.sh - copies tools/git-hooks/pre-commit into
# $(git rev-parse --git-common-dir)/hooks/pre-commit, idempotent
# (re-running just overwrites with the current version - no state to
# corrupt). GODS_LAWS.md L-07/D1: without this step the commit half of
# the dependency-zero gate never runs, because .git/hooks/ is NOT
# versioned by git itself.
#
# *** PROHIBITED, by explicit instruction of this fatia's plan: this
# script NEVER sets core.hooksPath. On the maintainer's own machine
# that setting is GLOBAL (~/.claude/githooks) and already chains into
# whatever LOCAL .git/hooks/<name> exists - overwriting it here would
# rip out that entire chain (todo_sync / tab_pendencias) for every
# other repository on the machine, not just this one. A plain
# .git/hooks/pre-commit file is git's own untouched default and works
# unassisted for anyone who clones without such a global chain. ***
#
# Degradation, documented rather than assumed (same precedent as
# tests/CMakeLists.txt's own POSIX-only downgrade comments): this is a
# POSIX sh script. On a machine with git but no POSIX shell able to run
# it (bare Windows without Git for Windows' embedded sh), installing
# the hook here is a no-op the caller must do by other means; the tree
# and CI half of the gate (dep_zero_test, tests/CMakeLists.txt) is
# unaffected either way - it does not depend on this script at all.

set -eu

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
source_hook="$script_dir/pre-commit"

[ -f "$source_hook" ] || {
    echo "install.sh: source hook not found: $source_hook" >&2
    exit 1
}

git_common_dir="$(git rev-parse --git-common-dir 2>/dev/null)" || {
    echo "install.sh: not inside a git repository" >&2
    exit 1
}

hooks_dir="$git_common_dir/hooks"
mkdir -p "$hooks_dir"

target_hook="$hooks_dir/pre-commit"
cp "$source_hook" "$target_hook"
chmod +x "$target_hook"

echo "install.sh: installed $source_hook -> $target_hook"
