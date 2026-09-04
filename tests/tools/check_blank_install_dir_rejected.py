#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_blank_install_dir_rejected.py - CI gate proving
# CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR, when given empty
# ("") or whitespace-only (" "), FAIL configure with glintfx's OWN
# FATAL_ERROR (glintfx_require_nonblank_install_subdir,
# cmake/GlintfxInstall.cmake) - instead of silently slipping through
# GNUInstallDirs' own defaulting (confirmed live before this guard
# existed: `-DCMAKE_INSTALL_LIBDIR=` used to configure successfully and
# only fail LATER, deep inside cmake_install.cmake's own generated
# script, with "file cannot create directory: /cmake/glintfx" - because
# the empty variable collapsed an install() DESTINATION to the
# filesystem ROOT).
#
# Blank is never a legitimate layout - unlike a malformed-but-nonblank
# value, an empty or whitespace-only value is always a mistake in how
# it was passed, so this refuses it outright, not computed around.
#
# PORT of tests/tools/check_blank_install_dir_rejected.sh (POSIX sh),
# done for GODS_LAWS.md L-04 ("O comportamento deve ser igual em
# qualquer OS", GATE-TREE-PARITY, INSTALL-PARITY-WIN task) - the sh
# version was registered `if(UNIX)` in tests/CMakeLists.txt, never
# exercised on the windows CI job. Same house shape as check_dep_zero.py/
# check_public_name_collision.py/check_layers.py: an ordinary ctest
# case, unguarded, resolved via find_program(...NAMES python3 python
# REQUIRED) (GLINTFX_PYTHON3_EXECUTABLE).
#
# ASSERT-WRAP (inherited defect class, GODS_LAWS.md L-17 "isolado ou
# padrao - procurar o gemeo"): CMake's own FATAL_ERROR message() text
# reflows at a fixed column width, and a naive single-string search for
# the two-word phrase "empty or blank" (or the variable name
# immediately before it) can land the wrap point BETWEEN them, missing
# a real rejection that fired correctly. Measured LIVE on this machine
# (GODS_LAWS.md L-44 - "nao declarar sem medir"): for THIS gate's own
# message (cmake/GlintfxInstall.cmake:37), the offending value glintfx_
# require_nonblank_install_subdir prints is the BLANK VALUE itself
# ('' or ' '), never this script's own scratch build-dir path - so,
# unlike the sibling gate below, padding that path does not move where
# this file's message wraps. The reflow column itself is what varies in
# practice (terminal width, CMake version, how much text precedes the
# message on a given run) - selftest_normalize_wrap_immunity_control
# below tests immunity against THAT, directly and more generally.
# normalize_wrapped_message() collapses every run of whitespace
# (embedded newlines included) back to one space before searching -
# CMake's own reflow never breaks inside a run of non-whitespace, only
# between words, so this reconstruction is safe. Already fixed once for
# the sibling gate (check_pkgconfig_validate.sh, PKG-VALIDATE-WRAP,
# commit d2110d9): that commit's own message named THIS file's
# identical "*${var_name}*empty or blank*" shape as the same defect
# class, left unfixed "por prudencia" (out of that commit's scope) -
# this file is that fix, applied here, PLUS a control
# (selftest_normalize_wrap_immunity_control below) proving immunity
# across a RANGE of wrap widths, not just the one width a real run
# happens to hit (the sh sibling's own measurement: "falhava em 21 de
# 30 comprimentos de caminho" - a single-value control would have
# missed 20 of those 21).
#
# Unlike the compiler-invoking gates above, --selftest here needs NO
# real cmake/compiler at all: the tested logic (normalize_wrapped_
# message, and the classification of a configure's returncode+output)
# is pure text processing, exercised with synthetic fixtures. Only
# real_main() below spawns a real, nested `cmake -S -B` per scenario -
# the same shape the sh version already had.
#
# Usage:
#   check_blank_install_dir_rejected.py <glintfx-source-dir> <cxx-compiler>
#   check_blank_install_dir_rejected.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import subprocess
import sys
import tempfile
import textwrap

SCRIPT_NAME = "check_blank_install_dir_rejected.py"

# The four scenarios this gate proves, CLOSED by construction (GODS_
# LAWS.md L-40 item 5, and L-17's "espaco de combinacoes pequeno e
# fechado -> enumerar inteiro"): both install-dir variables this
# project validates, crossed with both blank shapes (truly empty,
# whitespace-only) glintfx_require_nonblank_install_subdir treats
# identically (string(STRIP ...) first, cmake/GlintfxInstall.cmake).
# Growing this list is a conscious, reviewable edit of this file,
# proven non-empty and complete by selftest_scenario_list_control
# below.
SCENARIOS = (
    ("CMAKE_INSTALL_LIBDIR", ""),
    ("CMAKE_INSTALL_LIBDIR", " "),
    ("CMAKE_INSTALL_INCLUDEDIR", ""),
    ("CMAKE_INSTALL_INCLUDEDIR", " "),
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def normalize_wrapped_message(text):
    """ASSERT-WRAP (see this file's own header): collapses every run of
    whitespace - embedded newlines included - back to one space, so a
    two-anchor search never misses because CMake's own message() reflow
    happened to land between the anchors. CMake's reflow never breaks
    INSIDE an unbroken run of non-whitespace (a variable name or path
    with no spaces stays intact on one physical line, however long) -
    only BETWEEN words - so this is a safe reconstruction, never a
    lossy one.
    """
    return re.sub(r"\s+", " ", text)


def make_scratch_workdir():
    # tempfile.mkdtemp with no hardcoded fallback: dir=os.environ.get(
    # "TMPDIR") falls through to gettempdir() (TMPDIR/TEMP/TMP, then
    # the platform default) - a hand-written "/tmp" does not exist on
    # Windows.
    return tempfile.mkdtemp(prefix="glintfx-blankdir-", dir=os.environ.get("TMPDIR"))


def classify_configure_result(returncode, output, var_name):
    """Given a nested `cmake -S -B` configure's returncode and combined
    stdout+stderr, decides whether it correctly rejected the blank
    value - and WHY, when it did not. Split out from
    assert_configure_rejects_blank_var below so --selftest can drive it
    directly with synthetic fixtures, never a real nested cmake
    configure.
    """
    if returncode == 0:
        return False, (
            f"configure with {var_name} unexpectedly SUCCEEDED - the blank-value "
            "guard did not fire"
        )
    normalized = normalize_wrapped_message(output)
    if var_name not in normalized or "empty or blank" not in normalized:
        return False, (
            f"configure with {var_name} failed, but not with the expected message "
            f"naming '{var_name}' as empty or blank. Got:\n{output}"
        )
    return True, f"{var_name} correctly rejected at configure time"


def assert_configure_rejects_blank_var(glintfx_src, build_dir, var_name, blank_value, cxx_compiler):
    try:
        proc = subprocess.run(
            [
                "cmake", "-S", glintfx_src, "-B", build_dir, "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
                f"-D{var_name}={blank_value}",
                "-DGLINTFX_BUILD_TESTS=OFF",
            ],
            capture_output=True,
            text=True,
        )
    except OSError as exc:
        fail(f"failed to run 'cmake' ({exc}) - is it in PATH?")
        return  # unreachable, fail() exits; keeps linters happy

    output = proc.stdout + proc.stderr
    ok, message = classify_configure_result(proc.returncode, output, var_name)
    if not ok:
        fail(message)
    print(f"{SCRIPT_NAME}: {message}")


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 2:
        fail("usage: check_blank_install_dir_rejected.py <glintfx-source-dir> <cxx-compiler>")
    glintfx_src, cxx_compiler = args
    if not os.path.isdir(glintfx_src):
        fail(f"glintfx source dir not found: {glintfx_src}")
    if not cxx_compiler:
        fail("cxx-compiler argument is empty")

    scratch = make_scratch_workdir()
    try:
        for index, (var_name, blank_value) in enumerate(SCENARIOS):
            build_dir = os.path.join(scratch, f"build-{index}")
            assert_configure_rejects_blank_var(glintfx_src, build_dir, var_name, blank_value, cxx_compiler)
    finally:
        # No git repository is ever created under this scratch tree
        # (unlike check_dep_zero.py's own fixtures) - a plain
        # shutil.rmtree is enough, no read-only-object retry needed.
        import shutil
        shutil.rmtree(scratch, ignore_errors=True)

    print(
        f"{SCRIPT_NAME}: ok - CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR are both "
        f"rejected at configure time when empty or whitespace-only ({len(SCENARIOS)} "
        "scenario(s)), with a message naming the offending variable."
    )


# --- selftest: pure text-processing controls, no cmake/compiler needed ---


def selftest_classify_positive_control():
    """A configure that failed (returncode != 0) with an UNWRAPPED
    message naming the variable and the phrase: classified as ok."""
    output = "CMake Error at cmake/GlintfxInstall.cmake:37 (message):\n  glintfx: CMAKE_INSTALL_LIBDIR is empty or blank ('').\n"
    ok, message = classify_configure_result(1, output, "CMAKE_INSTALL_LIBDIR")
    if not ok:
        print(f"selftest: controle POSITIVO FALHOU (deveria classificar como ok, nao classificou): {message}", file=sys.stderr)
        return False
    if "correctly rejected" not in message:
        print(f"selftest: controle POSITIVO FALHOU (classificou ok, mas com mensagem inesperada): {message}", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (falha bem formada classificada como ok)")
    return True


def selftest_classify_unexpected_success_control():
    """returncode == 0 (configure succeeded): classified as NOT ok,
    regardless of what the output says - the blank-value guard simply
    did not fire."""
    ok, message = classify_configure_result(0, "-- Configuring done\n-- Generating done\n", "CMAKE_INSTALL_LIBDIR")
    if ok:
        print("selftest: controle de SUCESSO INESPERADO FALHOU (returncode 0 deveria reprovar, nao reprovou)", file=sys.stderr)
        return False
    if "unexpectedly SUCCEEDED" not in message:
        print(f"selftest: controle de SUCESSO INESPERADO FALHOU (reprovou, mas sem o motivo certo): {message}", file=sys.stderr)
        return False
    print("selftest: controle de SUCESSO INESPERADO OK (configure bem-sucedido e' reprovado)")
    return True


def selftest_classify_wrong_reason_control():
    """returncode != 0, but the failure has NOTHING to do with the
    blank-value guard (a compiler-not-found error, say): classified as
    NOT ok - a real failure for the WRONG reason must never be
    mistaken for proof the guard fired."""
    output = "CMake Error: could not find CMAKE_CXX_COMPILER\n"
    ok, message = classify_configure_result(1, output, "CMAKE_INSTALL_LIBDIR")
    if ok:
        print("selftest: controle de MOTIVO ERRADO FALHOU (falha por outro motivo deveria reprovar, nao reprovou)", file=sys.stderr)
        return False
    if "Got:" not in message or "CMAKE_INSTALL_LIBDIR" not in message.split("Got:")[0]:
        print(f"selftest: controle de MOTIVO ERRADO FALHOU (reprovou, mas sem citar a variavel esperada): {message}", file=sys.stderr)
        return False
    print("selftest: controle de MOTIVO ERRADO OK (falha por outro motivo nao e confundida com o guard)")
    return True


# ASSERT-WRAP immunity control: proves normalize_wrapped_message() (and
# therefore classify_configure_result(), which calls it) is immune to
# CMake's own message() reflow across a RANGE of columns where the wrap
# could land - never just the one column a real run happens to hit.
#
# Measured LIVE on this machine first (cmake 4.3.0, GODS_LAWS.md L-44 -
# "nao declarar sem medir"), before writing this control: padding
# check_blank_install_dir_rejected.py's OWN nested build-dir path (0 to
# 30 "x" characters) never once shifted where THIS file's specific
# message wraps - unlike the sibling gate's own measured defect ("is
# not there", check_pkgconfig_validate.sh, PKG-VALIDATE-WRAP, "falhava
# em 21 de 30 comprimentos de caminho"), the blank/offending VALUE
# glintfx_require_nonblank_install_subdir prints is not the scratch
# path at all, so padding that path alone never moves this message's
# own wrap point. The property that DOES determine where any reflow
# lands - the column width itself - is what this control varies
# directly, over 30 distinct widths: a strictly MORE GENERAL immunity
# proof than replaying one incidental cause (path length) that this
# file's own message happens not to be sensitive to. CMake's own reflow
# never breaks INSIDE a word (this file's own header, and confirmed
# live in the same measurement above: the offending variable name never
# split, only the space between "empty" and "or" did) -
# break_long_words=False mirrors that documented invariant; without it
# Python's textwrap would hyphen-break the LONG var name itself, a
# defect CMake does not have and this control must not manufacture.
def selftest_normalize_wrap_immunity_control():
    var_name = "CMAKE_INSTALL_LIBDIR"
    message = (
        f"glintfx: {var_name} is empty or blank (''). This is never a "
        "legitimate install layout - CMake concatenates it directly into "
        "install destinations, and an empty value collapses some of them "
        "to the filesystem ROOT."
    )
    widths = range(20, 50)  # 30 distinct reflow widths

    without_normalization_failures = []
    with_normalization_failures = []

    for width in widths:
        wrapped = textwrap.fill(message, width=width, break_long_words=False, break_on_hyphens=False)

        raw_ok = (var_name in wrapped) and ("empty or blank" in wrapped)
        if not raw_ok:
            without_normalization_failures.append(width)

        normalized = normalize_wrapped_message(wrapped)
        normalized_ok = (var_name in normalized) and ("empty or blank" in normalized)
        if not normalized_ok:
            with_normalization_failures.append(width)

    if with_normalization_failures:
        print(
            "selftest: controle de IMUNIDADE AO WRAP FALHOU - normalize_wrapped_message() ainda "
            f"perdeu a frase em {len(with_normalization_failures)} de {len(widths)} largura(s) de "
            f"reflow: {with_normalization_failures}",
            file=sys.stderr,
        )
        return False

    # Not vacuous: without normalization, SOME of the widths must have
    # actually split the phrase across lines - otherwise this control
    # would not be proving anything a plain substring search did not
    # already guarantee.
    if not without_normalization_failures:
        print(
            "selftest: controle de IMUNIDADE AO WRAP FALHOU (vazio: nenhuma das "
            f"{len(widths)} larguras quebrou a frase SEM normalizar - o controle nao esta testando "
            "nada, ajuste a faixa de larguras)",
            file=sys.stderr,
        )
        return False

    print(
        "selftest: controle de IMUNIDADE AO WRAP OK - sem normalizar, "
        f"{len(without_normalization_failures)} de {len(widths)} largura(s) de reflow teriam perdido "
        f"a frase (prova de que o defeito e real, larguras: {without_normalization_failures}); "
        f"normalizando, {len(widths)} de {len(widths)} continuam encontrando '{var_name}' e "
        "'empty or blank' juntos"
    )
    return True


def selftest_scenario_list_control():
    """L-40's closed-enumeration floor for THIS gate's own fixed
    scenario list (GODS_LAWS.md L-17: espaco pequeno e fechado ->
    enumerar inteiro): exactly the two validated variables, crossed
    with exactly the two blank shapes glintfx_require_nonblank_
    install_subdir treats identically - never silently shrunk to
    fewer, never silently grown without this control catching it.
    """
    expected_vars = {"CMAKE_INSTALL_LIBDIR", "CMAKE_INSTALL_INCLUDEDIR"}
    expected_blanks = {"", " "}

    if len(SCENARIOS) != 4:
        print(f"selftest: controle da LISTA DE CENARIOS FALHOU (esperava 4 cenarios, achou {len(SCENARIOS)})", file=sys.stderr)
        return False
    if len(set(SCENARIOS)) != len(SCENARIOS):
        print("selftest: controle da LISTA DE CENARIOS FALHOU (ha cenario duplicado)", file=sys.stderr)
        return False

    seen_vars = {var for var, _ in SCENARIOS}
    seen_blanks = {blank for _, blank in SCENARIOS}
    if seen_vars != expected_vars:
        print(f"selftest: controle da LISTA DE CENARIOS FALHOU (variaveis {seen_vars} != esperado {expected_vars})", file=sys.stderr)
        return False
    if seen_blanks != expected_blanks:
        print(f"selftest: controle da LISTA DE CENARIOS FALHOU (formas de branco {seen_blanks!r} != esperado {expected_blanks!r})", file=sys.stderr)
        return False
    for var in expected_vars:
        for blank in expected_blanks:
            if (var, blank) not in SCENARIOS:
                print(f"selftest: controle da LISTA DE CENARIOS FALHOU (falta a combinacao {(var, blank)!r})", file=sys.stderr)
                return False

    print("selftest: controle da LISTA DE CENARIOS OK (4 de 4: 2 variaveis x 2 formas de branco, sem duplicata)")
    return True


def selftest_main():
    controls = [
        selftest_classify_positive_control(),
        selftest_classify_unexpected_success_control(),
        selftest_classify_wrong_reason_control(),
        selftest_normalize_wrap_immunity_control(),
        selftest_scenario_list_control(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        if len(args) != 1:
            fail("usage: check_blank_install_dir_rejected.py --selftest")
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
