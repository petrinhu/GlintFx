#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_win32_last_error_cleared.py - CI gate for GODS_LAWS.md L-22 (o
# SO relata o erro, a lib nunca inventa) and L-17 (lei do gemeo:
# achado de robustez nunca e' isolado, varre-se a superficie inteira
# atras do irmao). Every Win32/WGL call this project attributes to a
# `platform_failure` through `::GetLastError()` must have that thread-
# local value CLEARED (`::SetLastError(0);`) IMMEDIATELY before the
# call that might fail - GetLastError()'s own documentation says why
# (learn.microsoft.com/windows/win32/api/errhandlingapi/nf-
# errhandlingapi-getlasterror#remarks): "some functions set the last-
# error code to 0 on success and others do not" - without the clear, a
# call that fails WITHOUT itself calling SetLastError() hands back the
# RESIDUAL value of some earlier, unrelated, already-succeeded call.
#
# INCIDENTE-FONTE (medido no integrador, run 34171429194, "Windows -
# estatico"): swap_buffers() in src/platform/win32/wgl_context_
# adapter.cpp reported os_error_code=203 for a SwapBuffers() refusal -
# ERROR_ENVVAR_NOT_FOUND, "The system could not find the environment
# option that was entered" (learn.microsoft.com/windows/win32/debug/
# system-error-codes--0-499-), a code with NOTHING to do with
# presenting a frame. wgl_context_adapter.cpp was the ONE file under
# src/platform/win32/ that read ::GetLastError() at nine call sites
# without ever clearing it first - display_adapter.cpp, seat_
# adapter.cpp and window_adapter.cpp already cleared before every one
# of their own eleven sites, since this backend's own foundation.
#
# ENUMERACAO FECHADA (GODS_LAWS.md L-43, "quando o espaco de
# verificacao e pequeno e enumeravel, enumere-o inteiro"): the
# FALLIBLE_CALLS table below is every Win32/WGL call this project
# attributes through ::GetLastError() in these four files, as of this
# fatia - eleven already-correct sites (RegisterClassExW/CreateWindowExW
# x3/SetWindowLongPtrW/RegisterRawInputDevices/SetWindowTextW) plus the
# nine this fatia fixes (wglMakeCurrent counts twice, once per calling
# function). src/platform/win32/wgl_extension_loader.cpp calls GetDC()
# too, but never reads ::GetLastError() for ANY of its own failures
# (it only carries `.with_rejected_value(...)`, GODS_LAWS.md L-32: a
# pre-existing, separate gap this fatia's own scope does not touch) -
# deliberately NOT in the four-file list below, or this gate would
# accuse a call whose result this project never reads via GetLastError
# in the first place.
#
# WHAT "CLEARED IMMEDIATELY BEFORE" MEANS HERE: walking backward from
# the line where a FALLIBLE_CALLS pattern matches, over blank lines and
# whole-line `//` comments, the check accepts a `::SetLastError(0);`
# found within MAX_LOOKBACK lines - fifteen, comfortably above the
# widest real gap measured in this file set today (nine lines, seat_
# adapter.cpp's own SetWindowLongPtrW site, separated from its own
# clear by a multi-line comment block) - PROVIDED no OTHER entry of
# FALLIBLE_CALLS is crossed first (that would mean the clear found
# belongs to a DIFFERENT call, not this one, and is not proof of
# anything for the site under test).
#
# Usage:
#   check_win32_last_error_cleared.py <repo-root-directory>
#   check_win32_last_error_cleared.py --selftest

import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_win32_last_error_cleared.py"

WIN32_DIR = os.path.join("src", "platform", "win32")

# The four files this project actually reads ::GetLastError() in - see
# the header comment above for why wgl_extension_loader.cpp is not one
# of them.
TARGET_FILES = (
    "display_adapter.cpp",
    "seat_adapter.cpp",
    "wgl_context_adapter.cpp",
    "window_adapter.cpp",
)

# (label, compiled regex) - each regex matches the LINE where the real
# Win32/WGL call begins, never the report site further below/inside an
# `if`. Local names (fn/create/swap_interval, tied to their own m_dc/
# interval arguments) are the function-pointer indirections wgl_
# context_adapter.cpp resolves ARB/EXT entry points through - there is
# no fixed symbol name to match for those three.
#
# SetWindowLongPtrW and RegisterRawInputDevices each occur TWICE in
# these four files, and only ONE of the two occurrences is ever read
# back through ::GetLastError() - the other is a fire-and-forget call
# whose result nothing checks (display_adapter.cpp's own window_proc()
# sets GWLP_USERDATA during WM_NCCREATE and never inspects the return;
# seat_adapter.cpp's own close() unregisters raw input with RIDEV_
# REMOVE and never inspects it either). Their patterns below are
# narrowed to the EXACT shape only the checked occurrence has -
# GWLP_WNDPROC (never GWLP_USERDATA) for the first, an assignment
# (`= ::RegisterRawInputDevices(`, never a bare statement) for the
# second - so this gate never accuses a call this project does not
# even attempt to read the error of.
FALLIBLE_CALLS = tuple(
    (label, re.compile(pattern))
    for label, pattern in (
        ("RegisterClassExW", r"::RegisterClassExW\("),
        ("CreateWindowExW", r"::CreateWindowExW\("),
        ("SetWindowLongPtrW", r"::SetWindowLongPtrW\(window, GWLP_WNDPROC,"),
        ("RegisterRawInputDevices", r"= ::RegisterRawInputDevices\("),
        ("SetWindowTextW", r"::SetWindowTextW\("),
        ("wglChoosePixelFormatARB (fn)", r"\bfn\(m_dc,"),
        ("DescribePixelFormat", r"::DescribePixelFormat\("),
        ("SetPixelFormat", r"::SetPixelFormat\("),
        ("wglCreateContextAttribsARB (create)", r"\bcreate\(m_dc,"),
        ("wglMakeCurrent", r"::wglMakeCurrent\(m_dc, m_context\)"),
        ("GetDC", r"::GetDC\(hwnd\)"),
        ("SwapBuffers", r"::SwapBuffers\(m_dc\)"),
        ("wglSwapIntervalEXT (swap_interval)", r"\bswap_interval\(interval\)"),
    )
)

_CLEAR_LINE = re.compile(r"^::SetLastError\(0\);$")

MAX_LOOKBACK = 15


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- codigo antes do comentario, e deteccao de linha em branco --------


def code_part(line):
    """Everything before the first `//` on the line - none of the
    thirteen FALLIBLE_CALLS patterns or the clear line ever appear
    after `//` in these four files (verified against the real tree
    before this gate was written), so this naive split is enough,
    without a full C++ tokenizer.
    """
    idx = line.find("//")
    return line if idx < 0 else line[:idx]


def is_blank(stripped_code):
    return stripped_code == ""


# A line that ends in `;`, `{` or `}` is a COMPLETE statement or block
# boundary on its own - if it is not the clear itself and not another
# fallible call, it blocks the walk (real code sits between the clear
# and our call). A line ending in anything else (an open `(`, a
# trailing `,`, a bare identifier) is a CONTINUATION of a multi-line
# statement whose closing sits at/after `call_index` - one directory
# over, seat_adapter.cpp's own SetWindowLongPtrW site is exactly this
# shape (`const auto previous = reinterpret_cast<WNDPROC>(` on its own
# line, the real call one line below) - skipped over, never mistaken
# for "real code in the way".
def is_statement_boundary(stripped_code):
    return stripped_code.endswith((";", "{", "}"))


# --- checagem por sitio -------------------------------------------------


def check_site(lines, call_index, label):
    """lines is 0-indexed; call_index is the 0-indexed line where the
    fallible call itself matched. Returns None when a valid, exclusive
    clear was found, or a violation reason string otherwise.
    """
    lookback_end = max(0, call_index - MAX_LOOKBACK)
    for j in range(call_index - 1, lookback_end - 1, -1):
        stripped = code_part(lines[j]).strip()
        if is_blank(stripped):
            continue
        if _CLEAR_LINE.match(stripped):
            return None
        # Any OTHER fallible call crossed first means whatever clear
        # exists further up (if any) belongs to THAT call, not this
        # one - stop here and reprova, rather than keep walking past
        # it into a different call's own clear.
        for other_label, other_pattern in FALLIBLE_CALLS:
            if other_pattern.search(stripped):
                return (
                    f"sem ::SetLastError(0); imediatamente antes - encontrado outro "
                    f"chamado Win32/WGL ({other_label}) primeiro, na linha {j + 1}"
                )
        if is_statement_boundary(stripped):
            return (
                f"sem ::SetLastError(0); imediatamente antes - a linha anterior real "
                f"({j + 1}) e' codigo, nao a limpeza"
            )
        # Continuation of the SAME multi-line statement as the call
        # (or as whatever sits between it and the call) - keep walking.
    return (
        f"sem ::SetLastError(0); nas {MAX_LOOKBACK} linhas anteriores "
        f"({label} na linha {call_index + 1})"
    )


def check_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            lines = handle.read().splitlines()
    except OSError as exc:
        return [(path, 0, f"open refused ({exc})")]

    violations = []
    for i, raw_line in enumerate(lines):
        code = code_part(raw_line)
        for label, pattern in FALLIBLE_CALLS:
            if pattern.search(code):
                reason = check_site(lines, i, label)
                if reason is not None:
                    violations.append((path, i + 1, f"{label}: {reason}"))
    return violations


# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero files found is
# a broken scan, never "nothing to report".
def require_nonempty_scan(file_count):
    if file_count == 0:
        print(f"{SCRIPT_NAME}: varredura vazia (0 arquivos)", file=sys.stderr)
        return False
    return True


def target_paths(root):
    return [os.path.join(root, WIN32_DIR, name) for name in TARGET_FILES]


def check_win32_last_error_cleared(root):
    paths = [p for p in target_paths(root) if os.path.isfile(p)]
    if not require_nonempty_scan(len(paths)):
        return False

    violations = []
    for path in paths:
        violations.extend(check_file(path))

    if violations:
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-17/L-22, o gemeo desta fatia):",
            file=sys.stderr,
        )
        for path, lineno, reason in violations:
            print(f"{path}:{lineno}: {reason}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: 0 ocorrencia(s) em {len(paths)} arquivo(s) varrido(s), "
        f"{len(FALLIBLE_CALLS)} padrao(oes) de chamada verificados"
    )
    return True


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_win32_last_error_cleared.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_win32_last_error_cleared(root):
        fail("chamada Win32/WGL sem ::SetLastError(0); imediatamente antes (ver mensagem acima)")


# --- fixtures and controls for --selftest -----------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-win32-last-error-selftest-", dir=os.environ.get("TMPDIR"))


def _make_capture():
    import contextlib
    import io

    class _Captured:
        __slots__ = ("result", "text")

        def __init__(self, result, text):
            self.result = result
            self.text = text

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


def write_target(root, name, content):
    target_dir = os.path.join(root, WIN32_DIR)
    os.makedirs(target_dir, exist_ok=True)
    with open(os.path.join(target_dir, name), "w", encoding="utf-8") as handle:
        handle.write(content)


# Positive control: every fixture call, one per file, correctly cleared
# right before it (including a long-gap case, mirroring seat_adapter.
# cpp's own SetWindowLongPtrW site, to prove the lookback window and
# comment-skipping both work). Expected: passes.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    write_target(
        root,
        "display_adapter.cpp",
        "void open() {\n"
        "    ::SetLastError(0);\n"
        "    const ATOM atom = ::RegisterClassExW(&wc);\n"
        "    ::SetLastError(0);\n"
        "    HWND window = ::CreateWindowExW(0, name, L\"\", 0);\n"
        "}\n",
    )
    write_target(
        root,
        "seat_adapter.cpp",
        "void open() {\n"
        "    ::SetLastError(0);\n"
        "    // um comentario\n"
        "    // outro comentario, de proposito bem longo pra exercitar o lookback\n"
        "    // e mais um\n"
        "    // e mais um\n"
        "    // e mais um\n"
        "    // e mais um\n"
        "    const auto previous = reinterpret_cast<WNDPROC>(\n"
        "        ::SetWindowLongPtrW(window, GWLP_WNDPROC, proc));\n"
        "    ::SetLastError(0);\n"
        "    const BOOL registered = ::RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));\n"
        "}\n",
    )
    write_target(
        root,
        "window_adapter.cpp",
        "void set_title() {\n"
        "    ::SetLastError(0);\n"
        "    if (::SetWindowTextW(m_window, wide.c_str()) == 0) {}\n"
        "}\n",
    )
    write_target(
        root,
        "wgl_context_adapter.cpp",
        "void set_pixel_format_once() {\n"
        "    ::SetLastError(0);\n"
        "    const bool ok = fn(m_dc, attribs, nullptr, 1, &format, &num_formats) != 0;\n"
        "    ::SetLastError(0);\n"
        "    if (::DescribePixelFormat(m_dc, chosen_format, size, &pfd) == 0) {}\n"
        "    ::SetLastError(0);\n"
        "    if (::SetPixelFormat(m_dc, chosen_format, &pfd) == 0) {}\n"
        "}\n"
        "void create_context() {\n"
        "    ::SetLastError(0);\n"
        "    HGLRC context = create(m_dc, nullptr, context_attribs);\n"
        "    ::SetLastError(0);\n"
        "    if (::wglMakeCurrent(m_dc, m_context) == 0) {}\n"
        "}\n"
        "void open() {\n"
        "    ::SetLastError(0);\n"
        "    m_dc = ::GetDC(hwnd);\n"
        "}\n"
        "void make_current() {\n"
        "    ::SetLastError(0);\n"
        "    if (::wglMakeCurrent(m_dc, m_context) == 0) {}\n"
        "}\n"
        "void swap_buffers() {\n"
        "    ::SetLastError(0);\n"
        "    if (::SwapBuffers(m_dc) == 0) {}\n"
        "}\n"
        "void call_swap_interval() {\n"
        "    ::SetLastError(0);\n"
        "    if (swap_interval(interval) == 0) {}\n"
        "}\n",
    )

    outcome = capture(lambda: check_win32_last_error_cleared(root))
    if outcome.result:
        print("selftest: controle POSITIVO OK (treze chamadas, todas limpas antes, aprovado)")
        return True
    print("selftest: controle POSITIVO FALHOU (fixture correta deveria ter sido aprovada)", file=sys.stderr)
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: reproduces the EXACT incidente-fonte shape - a
# SwapBuffers() call with NO preceding clear at all. Expected: reprova,
# citing the file:line and naming SwapBuffers.
def selftest_negative_control_no_clear(scratch, capture):
    root = os.path.join(scratch, "negative_no_clear")
    write_target(
        root,
        "wgl_context_adapter.cpp",
        "void swap_buffers() {\n"
        "    if (::IsIconic(m_window) != 0) { return; }\n"
        "    if (::SwapBuffers(m_dc) == 0) {}\n"
        "}\n",
    )
    outcome = capture(lambda: check_win32_last_error_cleared(root))
    if outcome.result:
        print("selftest: controle NEGATIVO (sem limpeza) FALHOU (deveria ter reprovado)", file=sys.stderr)
        return False
    if "SwapBuffers" not in outcome.text or ":3:" not in outcome.text:
        print(
            "selftest: controle NEGATIVO (sem limpeza) FALHOU (reprovou, mas nao citou "
            "SwapBuffers na linha certa)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO (sem limpeza) OK (reproduz o incidente-fonte, citado)")
    return True


# Negative control: the clear EXISTS, but a DIFFERENT fallible call sits
# between it and the call under test - the clear does not belong to
# THIS site. Expected: reprova, naming the call that was crossed.
def selftest_negative_control_wrong_owner(scratch, capture):
    root = os.path.join(scratch, "negative_wrong_owner")
    write_target(
        root,
        "wgl_context_adapter.cpp",
        "void open() {\n"
        "    ::SetLastError(0);\n"
        "    m_dc = ::GetDC(hwnd);\n"
        "    if (::SwapBuffers(m_dc) == 0) {}\n"
        "}\n",
    )
    outcome = capture(lambda: check_win32_last_error_cleared(root))
    if outcome.result:
        print("selftest: controle NEGATIVO (dono errado) FALHOU (deveria ter reprovado)", file=sys.stderr)
        return False
    if "GetDC" not in outcome.text:
        print(
            "selftest: controle NEGATIVO (dono errado) FALHOU (reprovou, mas nao citou "
            "GetDC como o chamado cruzado)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO (dono errado) OK (chamado cruzado citado)")
    return True


# Empty-scan floor: none of the four target files exist. Expected:
# reprova with "varredura vazia".
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)
    outcome = capture(lambda: check_win32_last_error_cleared(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar raiz sem os "
            "quatro arquivos-alvo, mas passou)",
            file=sys.stderr,
        )
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control_no_clear(scratch, capture),
            selftest_negative_control_wrong_owner(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
        ]
        if not all(controls):
            print("check_win32_last_error_cleared.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_win32_last_error_cleared.py --selftest: os {len(controls)} controles OK")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
