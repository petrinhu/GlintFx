#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_libdir_includedir_mix_rejected.py - CI gate proving
# CMAKE_INSTALL_LIBDIR and CMAKE_INSTALL_INCLUDEDIR, when one is an
# ABSOLUTE path and the other is RELATIVE, FAIL configure with
# glintfx's own FATAL_ERROR
# (glintfx_require_consistent_libdir_includedir_kind,
# cmake/GlintfxInstall.cmake) - and that every OTHER combination (both
# absolute, both relative) configures normally, exactly like it did
# before this gate existed.
#
# PKG-LIBDIR-MIX (achado colateral de 27/08/2026, TODO.md, pre-existente):
# glintfx_compute_pkgconfig_relocatable_prefix() decides whether
# glintfx.pc's own `prefix=` line is baked-in-absolute or
# `${pcfiledir}`-relocatable using CMAKE_INSTALL_LIBDIR's absoluteness
# ALONE. When CMAKE_INSTALL_INCLUDEDIR disagrees in kind, an ordinary
# install-time `--prefix` override moves whichever half is relative but
# never the baked-in `prefix=` - the generated glintfx.pc ends up
# naming a real, existing, entirely WRONG directory for that half. See
# the comment above glintfx_require_consistent_libdir_includedir_kind()
# in cmake/GlintfxInstall.cmake for the live reproduction that measured
# this before the guard was written.
#
# Same house shape as check_blank_install_dir_rejected.py (its own
# header explains the ASSERT-WRAP class this file inherits the fix
# for): an ordinary ctest case, unguarded by platform (GODS_LAWS.md
# L-04, GATE-TREE-PARITY), resolved via
# find_program(...NAMES python3 python REQUIRED)
# (GLINTFX_PYTHON3_EXECUTABLE).
#
# Usage:
#   check_libdir_includedir_mix_rejected.py <glintfx-source-dir> <cxx-compiler>
#   check_libdir_includedir_mix_rejected.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_libdir_includedir_mix_rejected.py"

# The four crossings this gate proves, CLOSED by construction
# (GODS_LAWS.md L-17's "espaco de combinacoes pequeno e fechado ->
# enumerar inteiro"): CMAKE_INSTALL_LIBDIR crossed with
# CMAKE_INSTALL_INCLUDEDIR, each independently absolute or relative.
# "reject": True means glintfx_require_consistent_libdir_includedir_kind
# must FAIL this configure; False means it must leave it alone (the
# guard is deliberately narrow - only a MIXED kind is refused).
# Absolute values are rooted under a scratch, throwaway directory
# (never a real system path like /opt) so a real configure can freely
# name them without needing write access outside the scratch tree.
SCENARIOS = (
    ("libdir-abs_includedir-rel", True, "abs", "rel"),
    ("libdir-rel_includedir-abs", True, "rel", "abs"),
    ("both-absolute", False, "abs", "abs"),
    ("both-relative", False, "rel", "rel"),
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def normalize_wrapped_message(text):
    """ASSERT-WRAP (GODS_LAWS.md L-17 "isolado ou padrao - procurar o
    gemeo" - the same defect class check_blank_install_dir_rejected.py's
    own header names): CMake's own message() text reflows at a fixed
    column width that varies by run (terminal width, CMake version, how
    much text precedes the message), and a naive single-string search
    for a MULTI-WORD phrase can land the wrap point between two words
    that belong together, missing a real rejection that fired
    correctly. Collapses every run of whitespace (embedded newlines
    included) back to one space before searching - CMake's own reflow
    never breaks INSIDE a run of non-whitespace, only BETWEEN words, so
    this reconstruction is safe.
    #
    # Kept here as defense-in-depth even though classify_configure_result()
    # below deliberately never searches for a multi-word phrase (unlike
    # the blank-dir gate's "empty or blank"): every anchor it checks -
    # the two raw CMAKE_INSTALL_LIBDIR/CMAKE_INSTALL_INCLUDEDIR values
    # (single filesystem paths, no embedded whitespace) and the single
    # words "ABSOLUTE"/"RELATIVE" - is one unbroken token, and CMake's
    # reflow never breaks a run of non-whitespace apart. That is a
    # STRUCTURAL choice, not a tested one: this gate has no
    # selftest_normalize_wrap_immunity_control the way
    # check_blank_install_dir_rejected.py does, because there is no
    # multi-word phrase here for a wrap to land inside - fabricating one
    # just to exercise this function would test a defect this file does
    # not have.
    """
    return re.sub(r"\s+", " ", text)


def make_scratch_workdir():
    # tempfile.mkdtemp with no hardcoded fallback: dir=os.environ.get(
    # "TMPDIR") falls through to gettempdir() (TMPDIR/TEMP/TMP, then the
    # platform default) - a hand-written "/tmp" does not exist on
    # Windows.
    return tempfile.mkdtemp(prefix="glintfx-libdirmix-", dir=os.environ.get("TMPDIR"))


def install_dir_value(kind, scratch, var_name):
    """Builds the CMAKE_INSTALL_LIBDIR/CMAKE_INSTALL_INCLUDEDIR value
    for one scenario. "abs" roots an absolute path under this
    scenario's own scratch directory (never a real system path); "rel"
    is a plain relative subdirectory name, the GNUInstallDirs-typical
    shape.
    """
    if kind == "abs":
        return os.path.join(scratch, "abs-root", var_name.lower())
    if kind == "rel":
        return var_name.lower()
    fail(f"internal: unknown kind '{kind}' for {var_name}")
    return None  # unreachable, fail() exits; keeps linters happy


def classify_configure_result(returncode, output, expect_reject, libdir_value, includedir_value):
    """Given a nested `cmake -S -B` configure's returncode and combined
    stdout+stderr, decides whether it matched this scenario's
    expectation - and WHY, when it did not. Split out from
    assert_configure_for_scenario below so --selftest can drive it
    directly with synthetic fixtures, never a real nested cmake
    configure.
    """
    if expect_reject:
        if returncode == 0:
            return False, (
                f"configure with CMAKE_INSTALL_LIBDIR='{libdir_value}' / "
                f"CMAKE_INSTALL_INCLUDEDIR='{includedir_value}' (mixed kind) "
                "unexpectedly SUCCEEDED - the mixed-kind guard did not fire"
            )
        normalized = normalize_wrapped_message(output)
        if libdir_value not in normalized or includedir_value not in normalized:
            return False, (
                "configure failed, but the message did not name BOTH "
                f"conflicting values ('{libdir_value}', '{includedir_value}'). Got:\n{output}"
            )
        if "ABSOLUTE" not in normalized or "RELATIVE" not in normalized:
            return False, (
                "configure failed, but not with the expected ABSOLUTE/RELATIVE "
                f"wording. Got:\n{output}"
            )
        return True, "mixed kind correctly rejected at configure time, naming both values"

    if returncode != 0:
        return False, (
            f"configure with CMAKE_INSTALL_LIBDIR='{libdir_value}' / "
            f"CMAKE_INSTALL_INCLUDEDIR='{includedir_value}' (SAME kind, not "
            f"mixed) unexpectedly FAILED - the guard must leave this "
            f"combination alone. Got:\n{output}"
        )
    return True, "same-kind combination correctly left alone (configure succeeded)"


def assert_configure_for_scenario(glintfx_src, build_dir, cxx_compiler, scenario_name, expect_reject, libdir_value, includedir_value):
    try:
        proc = subprocess.run(
            [
                "cmake", "-S", glintfx_src, "-B", build_dir, "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
                f"-DCMAKE_INSTALL_LIBDIR={libdir_value}",
                f"-DCMAKE_INSTALL_INCLUDEDIR={includedir_value}",
                "-DGLINTFX_BUILD_TESTS=OFF",
            ],
            capture_output=True,
            text=True,
        )
    except OSError as exc:
        fail(f"failed to run 'cmake' ({exc}) - is it in PATH?")
        return  # unreachable, fail() exits; keeps linters happy

    output = proc.stdout + proc.stderr
    ok, message = classify_configure_result(proc.returncode, output, expect_reject, libdir_value, includedir_value)
    if not ok:
        fail(f"[{scenario_name}] {message}")
    print(f"{SCRIPT_NAME}: [{scenario_name}] {message}")


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 2:
        fail("usage: check_libdir_includedir_mix_rejected.py <glintfx-source-dir> <cxx-compiler>")
    glintfx_src, cxx_compiler = args
    if not os.path.isdir(glintfx_src):
        fail(f"glintfx source dir not found: {glintfx_src}")
    if not cxx_compiler:
        fail("cxx-compiler argument is empty")

    scratch = make_scratch_workdir()
    analyzed = 0
    try:
        for index, (scenario_name, expect_reject, libdir_kind, includedir_kind) in enumerate(SCENARIOS):
            scenario_scratch = os.path.join(scratch, f"scenario-{index}")
            os.makedirs(scenario_scratch, exist_ok=True)
            build_dir = os.path.join(scenario_scratch, "build")
            libdir_value = install_dir_value(libdir_kind, scenario_scratch, "CMAKE_INSTALL_LIBDIR")
            includedir_value = install_dir_value(includedir_kind, scenario_scratch, "CMAKE_INSTALL_INCLUDEDIR")
            assert_configure_for_scenario(
                glintfx_src, build_dir, cxx_compiler, scenario_name, expect_reject, libdir_value, includedir_value
            )
            analyzed += 1
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    # Piso de varredura nao-vazia (GODS_LAWS.md L-40 item 5): conta o
    # que foi encontrado (SCENARIOS) contra o que foi de fato analisado
    # - as duas devem bater, e nenhuma pode ser zero.
    found = len(SCENARIOS)
    if analyzed != found or found == 0:
        fail(
            f"varredura quebrada: {found} cenario(s) encontrado(s), {analyzed} "
            "analisado(s) - as duas contagens deveriam bater e nenhuma pode ser zero"
        )

    print(
        f"{SCRIPT_NAME}: ok - encontrados {found}, analisados {analyzed}, falharam 0 "
        "- os dois cruzamentos MISTOS (libdir absoluto/includedir relativo e o inverso) "
        "sao recusados na hora de configurar, nomeando os dois valores em conflito; os "
        "dois cruzamentos do MESMO tipo (ambos absolutos, ambos relativos) continuam "
        "configurando normalmente."
    )


# --- selftest: pure text-processing controls, no cmake/compiler needed ---


def selftest_classify_reject_positive_control():
    """A configure that failed (returncode != 0) with an UNWRAPPED
    message naming both conflicting values and the ABSOLUTE/RELATIVE
    wording: classified as ok."""
    output = (
        "CMake Error at cmake/GlintfxInstall.cmake:100 (message):\n"
        "  glintfx: CMAKE_INSTALL_LIBDIR ('/tmp/x/lib64') is an ABSOLUTE path, but\n"
        "  CMAKE_INSTALL_INCLUDEDIR ('include') is RELATIVE.\n"
    )
    ok, message = classify_configure_result(1, output, True, "/tmp/x/lib64", "include")
    if not ok:
        print(f"selftest: controle POSITIVO (rejeicao) FALHOU: {message}", file=sys.stderr)
        return False
    if "correctly rejected" not in message:
        print(f"selftest: controle POSITIVO (rejeicao) FALHOU (mensagem inesperada): {message}", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO (rejeicao) OK")
    return True


def selftest_classify_reject_unexpected_success_control():
    """A mixed-kind scenario whose configure SUCCEEDED (returncode 0):
    classified as NOT ok, regardless of output - the guard did not
    fire."""
    ok, message = classify_configure_result(0, "-- Configuring done\n", True, "/tmp/x/lib64", "include")
    if ok:
        print("selftest: controle de SUCESSO INESPERADO (rejeicao) FALHOU", file=sys.stderr)
        return False
    if "unexpectedly SUCCEEDED" not in message:
        print(f"selftest: controle de SUCESSO INESPERADO (rejeicao) FALHOU (motivo errado): {message}", file=sys.stderr)
        return False
    print("selftest: controle de SUCESSO INESPERADO (rejeicao) OK")
    return True


def selftest_classify_reject_missing_value_control():
    """A mixed-kind scenario that failed, but the message does not name
    BOTH conflicting values: classified as NOT ok - a failure that does
    not point at the actual conflict is not proof the right guard
    fired."""
    output = "CMake Error: something else entirely went wrong\n"
    ok, message = classify_configure_result(1, output, True, "/tmp/x/lib64", "include")
    if ok:
        print("selftest: controle de VALOR AUSENTE (rejeicao) FALHOU", file=sys.stderr)
        return False
    if "did not name BOTH" not in message:
        print(f"selftest: controle de VALOR AUSENTE (rejeicao) FALHOU (motivo errado): {message}", file=sys.stderr)
        return False
    print("selftest: controle de VALOR AUSENTE (rejeicao) OK")
    return True


def selftest_classify_same_kind_positive_control():
    """A same-kind (non-mixed) scenario whose configure SUCCEEDED:
    classified as ok - the guard must leave this combination alone."""
    ok, message = classify_configure_result(0, "-- Configuring done\n", False, "/tmp/x/lib64", "/tmp/x/include")
    if not ok:
        print(f"selftest: controle POSITIVO (mesmo tipo) FALHOU: {message}", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO (mesmo tipo) OK")
    return True


def selftest_classify_same_kind_unexpected_failure_control():
    """A same-kind (non-mixed) scenario whose configure FAILED:
    classified as NOT ok - a same-kind combination is never supposed to
    be refused, so any failure here is itself a defect."""
    output = "CMake Error at cmake/GlintfxInstall.cmake:100 (message):\n  glintfx: ...\n"
    ok, message = classify_configure_result(1, output, False, "lib64", "include")
    if ok:
        print("selftest: controle de FALHA INESPERADA (mesmo tipo) FALHOU", file=sys.stderr)
        return False
    if "unexpectedly FAILED" not in message:
        print(f"selftest: controle de FALHA INESPERADA (mesmo tipo) FALHOU (motivo errado): {message}", file=sys.stderr)
        return False
    print("selftest: controle de FALHA INESPERADA (mesmo tipo) OK")
    return True


# No selftest_normalize_wrap_immunity_control here, unlike
# check_blank_install_dir_rejected.py: see normalize_wrapped_message()'s
# own docstring above for why - every anchor classify_configure_result()
# below searches for is a single unbroken token (a raw path value, or
# one of the words ABSOLUTE/RELATIVE), never a multi-word phrase a wrap
# could land inside. Manufacturing a fake multi-word phrase just to
# exercise this control would test a defect this file's own search
# logic does not have; textwrap.fill() with break_long_words=False,
# tried against this file's real message shape while writing it,
# confirmed exactly that (every anchor survived every width from 20 to
# 50 unnormalized - the vacuous-control guard this project's own house
# style requires caught it, and this comment is the correction: drop
# the control instead of keeping a vacuous one).
def selftest_scenario_list_control():
    """L-40's closed-enumeration floor for THIS gate's own fixed
    scenario list (GODS_LAWS.md L-17: espaco pequeno e fechado ->
    enumerar inteiro): exactly os quatro cruzamentos de
    CMAKE_INSTALL_LIBDIR x CMAKE_INSTALL_INCLUDEDIR (abs/rel cada),
    metade recusada (mista), metade aceita (mesmo tipo) - nunca
    encolhido nem crescido em silencio.
    """
    expected_kinds = {("abs", "rel"), ("rel", "abs"), ("abs", "abs"), ("rel", "rel")}

    if len(SCENARIOS) != 4:
        print(f"selftest: controle da LISTA DE CENARIOS FALHOU (esperava 4 cenarios, achou {len(SCENARIOS)})", file=sys.stderr)
        return False
    names = [name for name, _, _, _ in SCENARIOS]
    if len(set(names)) != len(names):
        print("selftest: controle da LISTA DE CENARIOS FALHOU (ha nome de cenario duplicado)", file=sys.stderr)
        return False

    seen_kinds = {(libdir_kind, includedir_kind) for _, _, libdir_kind, includedir_kind in SCENARIOS}
    if seen_kinds != expected_kinds:
        print(f"selftest: controle da LISTA DE CENARIOS FALHOU (cruzamentos {seen_kinds} != esperado {expected_kinds})", file=sys.stderr)
        return False

    mixed = [name for name, expect_reject, libdir_kind, includedir_kind in SCENARIOS if libdir_kind != includedir_kind]
    same_kind = [name for name, expect_reject, libdir_kind, includedir_kind in SCENARIOS if libdir_kind == includedir_kind]
    if len(mixed) != 2 or len(same_kind) != 2:
        print(
            f"selftest: controle da LISTA DE CENARIOS FALHOU (esperava 2 mistos e 2 do mesmo tipo, "
            f"achou {len(mixed)} mistos e {len(same_kind)} do mesmo tipo)",
            file=sys.stderr,
        )
        return False

    for name, expect_reject, libdir_kind, includedir_kind in SCENARIOS:
        should_reject = libdir_kind != includedir_kind
        if expect_reject != should_reject:
            print(
                f"selftest: controle da LISTA DE CENARIOS FALHOU (cenario '{name}': expect_reject={expect_reject}, "
                f"mas libdir_kind={libdir_kind!r} vs includedir_kind={includedir_kind!r} implica {should_reject})",
                file=sys.stderr,
            )
            return False

    print("selftest: controle da LISTA DE CENARIOS OK (4 de 4: 2 mistos recusados, 2 do mesmo tipo aceitos, sem duplicata)")
    return True


def selftest_main():
    controls = [
        selftest_classify_reject_positive_control(),
        selftest_classify_reject_unexpected_success_control(),
        selftest_classify_reject_missing_value_control(),
        selftest_classify_same_kind_positive_control(),
        selftest_classify_same_kind_unexpected_failure_control(),
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
            fail("usage: check_libdir_includedir_mix_rejected.py --selftest")
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
