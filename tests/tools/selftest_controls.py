# SPDX-License-Identifier: AGPL-3.0-or-later
#
# selftest_controls.py - the control helper shared by the selftests of six tools (SELFTEST-CHECK-CANARY,
# D-W8-341, D-W8-361). Standard library only (L-07). Not a tool. That check() stops the selftest on a false
# condition is proved by EFFECT, for every module that exposes a check, by tests/tools/check_selftest_check_effect.py.

import sys

CHECKS = []


def check(name, condition):
    CHECKS.append(name)
    if not condition:
        print(f"selftest: {name} FALHOU", file=sys.stderr)
        sys.exit(1)
