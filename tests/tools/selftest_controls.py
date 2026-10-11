# SPDX-License-Identifier: AGPL-3.0-or-later
#
# selftest_controls.py - the control helper shared by the selftests of six tools (SELFTEST-CHECK-CANARY,
# D-W8-341, D-W8-352). Before it, each kept its own identical copy (GODS_LAWS.md global L-33: extract on the third).
# Standard library only (L-07). Not a tool: no --selftest of its own.
#
# THE CANARY lives inside check() itself: the first call of any selftest runs it, so no selftest can forget it.
# It calls check() with a false condition and requires the selftest to stop there with a non-zero code; if check()
# ever stops doing so, every control goes mute and the selftest would still print its count, and the canary fails
# loudly instead, without check().

import contextlib
import io
import sys

CHECKS = []
_canary_armed = False


def check(name, condition):
    global _canary_armed
    if not _canary_armed:
        _canary_armed = True
        _canary()
    CHECKS.append(name)
    if not condition:
        print(f"selftest: {name} FALHOU", file=sys.stderr)
        sys.exit(1)


def _canary():
    """check() with a false condition must stop with a non-zero code; its own entry leaves the count."""
    with contextlib.redirect_stderr(io.StringIO()):
        try:
            check("canario: este controle tem de reprovar", False)
        except SystemExit as stopped:
            if stopped.code not in (0, None):
                CHECKS.pop()
                return
    raise SystemExit("selftest: o check deixou de reprovar (canario, D-W8-352)")
