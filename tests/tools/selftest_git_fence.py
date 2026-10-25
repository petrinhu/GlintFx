# SPDX-License-Identifier: AGPL-3.0-or-later
#
# selftest_git_fence.py - the fenced scratch folder shared by the five gates whose --selftest carries a "not a
# repository" control (check_spdx.py, check_vendor_purity.py, check_dash_pubdoc.py, check_version_matches_tag.py,
# check_format.py; SUITE-TMPDIR-PATHS). Standard library only (L-07). Not a tool: no command line and no --selftest of
# its own, the same shape as selftest_controls.py. What it does is proved by EFFECT, through those five selftests: each
# one fails its own not-a-repository control when TMPDIR sits inside a git work tree or GIT_DIR is exported, unless
# this fence is on.

import os
import tempfile

# Git variables that pin a repository whatever directory git runs in.
# git(1), ENVIRONMENT: GIT_CEILING_DIRECTORIES "will not exclude the
# current working directory or a GIT_DIR set on the command line or in
# the environment", so the fence below drops these as well.
REPOSITORY_PINNING_GIT_VARIABLES = (
    "GIT_DIR",
    "GIT_WORK_TREE",
    "GIT_COMMON_DIR",
    "GIT_INDEX_FILE",
    "GIT_OBJECT_DIRECTORY",
    "GIT_ALTERNATE_OBJECT_DIRECTORIES",
)


def _fence_git_under(parent):
    """Keeps every git call of a --selftest inside the folders created
    under `parent`, the way git's own t/test-lib.sh fences its trash
    directory: `parent` is the ceiling, because git will not "chdir up
    into" a ceiling but does leave a starting directory that is itself
    the ceiling (git(1), ENVIRONMENT). Without it, a TMPDIR that lives
    inside a git work tree (a temporary folder inside a checkout, for
    example) lets the not-a-repository control find that outer
    repository and fail. A ceiling the caller already set is kept,
    after this one. Changes the environment of the calling process
    only."""
    for name in REPOSITORY_PINNING_GIT_VARIABLES:
        os.environ.pop(name, None)
    ceiling = os.path.abspath(parent)
    previous = os.environ.get("GIT_CEILING_DIRECTORIES")
    os.environ["GIT_CEILING_DIRECTORIES"] = ceiling if not previous else ceiling + os.pathsep + previous


def make_fenced_scratch_dir(prefix, parent=None):
    """Creates the scratch folder of a --selftest, under `parent` (the
    temporary folder when None, as tempfile.mkdtemp() does), with git
    already fenced around it. The fence goes on first and the folder is
    created last: nothing that can fail runs after the folder exists,
    so a failure here leaves nothing behind in the caller's temporary
    folder. Removing the folder is the caller's job, in the `finally`
    that has to come right after this call."""
    base = parent if parent else tempfile.gettempdir()
    _fence_git_under(base)
    return tempfile.mkdtemp(prefix=prefix, dir=base)
