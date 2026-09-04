#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_no_x11.py - CI gate for GODS_LAWS.md L-05 (Linux e Wayland
# puro, sem X11) e L-06 (parser de keymap proprio; libxkbcommon fora).
#
# PORT of the former tests/tools/check_no_x11.sh (POSIX sh), retired
# in the same fatia that wrote this file (GATE-TREE-PARITY, GODS_LAWS.md
# L-04, decisao do lider: "O comportamento deve ser igual em qualquer
# OS" - the sh version was if(UNIX)-guarded in tests/CMakeLists.txt,
# so nothing checked X11/libxkbcommon-forbidden terms on the Windows
# CI job at all, even though every term this gate looks for is plain
# text with no Unix-specific mechanism behind the check itself).
# Registered here, unguarded, as an ordinary ctest case - the same
# shape check_layers.py and check_vendor_purity.py already proved
# works on all five platforms.
#
# The sh version's own header documented three GODS_LAWS.md L-40
# achados against the block it replaced (inline in the "leis" job of
# .github/workflows/ci.yml at the time): empty scan passed silently,
# tools/ and .github/ were never swept, and the forbidden-term list was
# shorter than the law. This port carries all three fixes forward -
# see require_nonempty_scan(), code_dirs() and FORBIDDEN_NEEDLES below.
#
# GATE-TREE-PARITY-NEWLINE (closed here BY CONSTRUCTION, same class
# check_layers.py/check_vendor_purity.py's own headers document): the
# sh version's `find ... | while IFS= read -r f` is a newline-joined
# shell listing a hostile filename could split. os.walk() below reads
# directory entries directly from the filesystem as discrete Python
# strings, one per entry - there is no text stream to split.
#
# AUTOEXCLUSAO, explicita e estreita (nao um buraco): este PROPRIO
# arquivo precisa conter, em texto puro, cada termo que ele proibe -
# sem isso nao ha o que varrer contra. Ele se auto-acusaria se fosse
# incluido na propria varredura. A exclusao e do CAMINHO EXATO deste
# arquivo (SELF_PATH_SUFFIX abaixo), nunca de um diretorio, nunca de
# outro arquivo - ver selftest_self_exclusion_control.
#
# Usage:
#   check_no_x11.py <repo-root-directory>
#   check_no_x11.py --selftest
#
# --selftest roda quatro controles (positivo, negativo, varredura
# vazia, autoexclusao estreita) contra fixtures descartaveis sob
# tempfile.mkdtemp, nunca contra a arvore rastreada real.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_no_x11.py"

# Caminho deste arquivo relativo a raiz do repo, em forma POSIX ('/'),
# usado tanto no modo real quanto nas fixtures do --selftest.
SELF_PATH_SUFFIX = "tests/tools/check_no_x11.py"

# Termos proibidos pela L-05 (Wayland puro, sem X11) e L-06 (parser de
# keymap proprio, libxkbcommon fora) - ver o cabecalho acima para a
# origem de cada um. Fechado por construcao (GODS_LAWS.md L-40 item 5),
# nao ampliado por busca dirigida.
FORBIDDEN_NEEDLES = (
    r"Xlib\.h",
    r"xcb/",
    r"XOpenDisplay",
    r"XTestFake",
    r"xkbcommon",
    r"XWayland",
    r"Xvfb",
    r"xvfb-run",
    r"xdotool",
    r"X11/",
)

_FORBIDDEN_PATTERN = re.compile("|".join(FORBIDDEN_NEEDLES))

_CODE_DIR_CANDIDATES = ("src", "include", "tests", "examples", "demos", "cmake", "tools", ".github")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- enumeracao ------------------------------------------------------


def code_dirs(root):
    for candidate in _CODE_DIR_CANDIDATES:
        path = os.path.join(root, candidate)
        if os.path.isdir(path):
            yield path


def scanned_files(root):
    """One os.walk() per candidate directory, with the self-excluded
    path removed at the single point that enumerates file by file -
    before any pattern search, mirroring the sh version's own
    scanned_files() ordering.
    """
    files = []
    for source_dir in code_dirs(root):
        for dirpath, _dirnames, filenames in os.walk(source_dir):
            for name in filenames:
                full_path = os.path.join(dirpath, name)
                rel_path = os.path.relpath(full_path, root).replace(os.sep, "/")
                if rel_path == SELF_PATH_SUFFIX:
                    continue
                files.append(full_path)
    return files


def violations_in_file(path):
    violations = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for lineno, line in enumerate(handle, start=1):
                if _FORBIDDEN_PATTERN.search(line):
                    violations.append((path, lineno))
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
    return violations


# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero files found
# under any of the code dirs is not "nothing to report", and reproves -
# the exact achado (a) the sh version's own header names.
def require_nonempty_scan(file_count):
    if file_count == 0:
        print(f"{SCRIPT_NAME}: varredura vazia (0 arquivos)", file=sys.stderr)
        return False
    return True


# --- checagem ----------------------------------------------------------


def check_no_x11(root):
    files = scanned_files(root)
    file_count = len(files)

    if not require_nonempty_scan(file_count):
        return False

    violations = []
    for f in files:
        violations.extend(violations_in_file(f))

    if violations:
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-05 Wayland puro, L-06 keymap proprio):",
            file=sys.stderr,
        )
        for path, lineno in violations:
            print(f"{path}:{lineno}", file=sys.stderr)
        return False

    print(f"{SCRIPT_NAME}: 0 ocorrencia(s) em {file_count} arquivo(s) varrido(s)")
    return True


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_no_x11.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_no_x11(root):
        fail("X11 ou libxkbcommon proibidos encontrados (ver mensagem acima)")


# --- fixtures and controls for --selftest -----------------------------


def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") without a hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    return tempfile.mkdtemp(prefix="glintfx-no-x11-selftest-", dir=os.environ.get("TMPDIR"))


def make_clean_fixture(root):
    src_dir = os.path.join(root, "src")
    os.makedirs(src_dir, exist_ok=True)
    with open(os.path.join(src_dir, "janela.cpp"), "w", encoding="utf-8") as handle:
        handle.write("// codigo limpo, sem termo proibido\n")


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


# Positive control: clean fixture. Expected: passes.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    make_clean_fixture(root)

    outcome = capture(lambda: check_no_x11(root))
    if outcome.result:
        print("selftest: controle POSITIVO OK (fixture limpa aprovada)")
        return True
    print(
        "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)",
        file=sys.stderr,
    )
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: plants EVERY forbidden term in tools/ AND in
# .github/ (the two directories the sh version's own achado (b) never
# swept). Expected: each term, in each new directory, reproves and is
# cited in the message.
def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    make_clean_fixture(root)
    tools_dir = os.path.join(root, "tools", "ci")
    github_dir = os.path.join(root, ".github", "workflows")
    os.makedirs(tools_dir, exist_ok=True)
    os.makedirs(github_dir, exist_ok=True)

    ok = True
    termos = ("Xlib.h", "xcb/foo", "XOpenDisplay", "XTestFakeKeyEvent", "xkbcommon",
              "XWayland", "Xvfb", "xvfb-run", "xdotool", "X11/Xutil.h")
    for termo in termos:
        slug = re.sub(r"[^a-zA-Z0-9]", "_", termo)
        alvo = os.path.join(tools_dir, f"plantado_{slug}.sh")
        with open(alvo, "w", encoding="utf-8") as handle:
            handle.write(f"# {termo}\n")

        outcome = capture(lambda alvo=alvo: check_no_x11(root))
        if outcome.result:
            print(f"selftest: controle NEGATIVO FALHOU (termo '{termo}' em tools/ nao foi pego)", file=sys.stderr)
            ok = False
        elif alvo not in outcome.text:
            print(f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou {alvo})", file=sys.stderr)
            print(outcome.text, file=sys.stderr)
            ok = False
        os.remove(alvo)

    alvo = os.path.join(github_dir, "plantado.yml")
    with open(alvo, "w", encoding="utf-8") as handle:
        handle.write("run: xvfb-run ./demo\n")
    outcome = capture(lambda: check_no_x11(root))
    if outcome.result:
        print("selftest: controle NEGATIVO FALHOU (termo em .github/ nao foi pego)", file=sys.stderr)
        ok = False
    elif alvo not in outcome.text:
        print(f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou {alvo})", file=sys.stderr)
        print(outcome.text, file=sys.stderr)
        ok = False
    os.remove(alvo)

    if ok:
        print("selftest: controle NEGATIVO OK (dez termos, tools/ e .github/, todos pegos e citados)")
    return ok


# Empty-scan floor: none of the swept directories exist. Expected:
# reproves with "varredura vazia" in the message.
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)

    outcome = capture(lambda: check_no_x11(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar raiz sem diretorio de codigo, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (raiz sem diretorio de codigo recusada)")
    return True


# Narrow self-exclusion control: plants a forbidden term inside the
# self-excluded file itself (must PASS, else this gate would accuse
# itself every real run) AND, in the SAME directory, inside a SIBLING
# file (must REPROVE - proof the exclusion is of one file, not the
# whole tests/tools/ directory).
def selftest_self_exclusion_control(scratch, capture):
    root = os.path.join(scratch, "self_exclusion")
    make_clean_fixture(root)
    self_dir = os.path.join(root, "tests", "tools")
    os.makedirs(self_dir, exist_ok=True)

    self_file = os.path.join(self_dir, "check_no_x11.py")
    with open(self_file, "w", encoding="utf-8") as handle:
        handle.write("# contem Xvfb de proposito, e o alvo da autoexclusao\n")

    outcome = capture(lambda: check_no_x11(root))
    if not outcome.result:
        print(
            "selftest: controle de AUTOEXCLUSAO FALHOU (o proprio arquivo excluido foi pego - "
            "o gate se acusaria sozinho em producao)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        os.remove(self_file)
        return False

    alvo = os.path.join(self_dir, "outro_arquivo.sh")
    with open(alvo, "w", encoding="utf-8") as handle:
        handle.write("# xdotool plantado no irmao, nao no arquivo excluido\n")

    outcome = capture(lambda: check_no_x11(root))
    if outcome.result:
        print(
            "selftest: controle de AUTOEXCLUSAO FALHOU (irmao no mesmo diretorio do arquivo excluido escapou - "
            "a exclusao vazou para o diretorio inteiro)",
            file=sys.stderr,
        )
        os.remove(self_file)
        os.remove(alvo)
        return False
    if alvo not in outcome.text:
        print(
            "selftest: controle de AUTOEXCLUSAO FALHOU (reprovou o irmao, mas nao o citou na mensagem)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        os.remove(self_file)
        os.remove(alvo)
        return False

    os.remove(self_file)
    os.remove(alvo)
    print("selftest: controle de AUTOEXCLUSAO OK (arquivo excluido passa, irmao no mesmo diretorio reprova)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_self_exclusion_control(scratch, capture),
        ]
        if not all(controls):
            print("check_no_x11.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_no_x11.py --selftest: os {len(controls)} controles OK")
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
