# SPDX-License-Identifier: AGPL-3.0-or-later
"""How the drivers run the capture tool: a deadline, and the whole process tree killed when it passes."""

import contextlib
import os
import signal
import subprocess
import sys


PROCESS_TIMEOUT_SECONDS = 180
KILL_GRACE_SECONDS = 5


def kill_tree(process):
    """Kills the child AND everything it started. The tool launches the fixture, and the fixture
    inherits nothing it should not (the tool restricts the handles), but a tree left alive after a
    timeout would hold the pipe: on Windows subprocess.run then waits for the pipe's EOF after the
    kill, which is exactly the hang the timeout exists to prevent. POSIX: the whole process group;
    Windows: taskkill /T. NOT MEASURED on the Windows runner (declared): the mitigation is the known
    one, the proof of it there is the first red run."""
    if sys.platform == "win32":
        subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True, check=False)
    else:
        with contextlib.suppress(ProcessLookupError, PermissionError):
            os.killpg(process.pid, signal.SIGKILL)
    with contextlib.suppress(OSError):
        process.kill()


def run_process(args, timeout=PROCESS_TIMEOUT_SECONDS):
    """The real executor: (exit code, stdout+stderr). A hung child is a failure, never a hang."""
    try:
        process = subprocess.Popen([str(part) for part in args], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, errors="replace", close_fds=True,
                                   start_new_session=sys.platform != "win32")
    except OSError as error:
        return 127, f"cannot run {args[0]}: {error}"
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        kill_tree(process)
        with contextlib.suppress(subprocess.TimeoutExpired):
            process.communicate(timeout=KILL_GRACE_SECONDS)
        return 124, f"timeout after {timeout} s: {args[0]}"
    return process.returncode, output
