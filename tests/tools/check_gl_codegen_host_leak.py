#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_gl_codegen_host_leak.py - CI gate for GL-CODEGEN-HOST-TOOL's H-5
# (TODO.md, D-11 of docs/plano-fecho-w7b.md, GODS_LAWS.md L-19/L-40): a
# real, end-to-end proof that none of the GL-CODEGEN-HOST-TOOL cache
# variables (cmake/GlintfxGlCodegenHostTool.cmake,
# cmake/GlintfxOptions.cmake) leak into the INSTALLED CMake package or
# pkg-config file a consumer reads.
#
# THE DOR THIS GUARDS AGAINST (D-11, the Qt 6 precedent this project's
# own research cites): QT_HOST_PATH, a build-time-only detail of Qt's
# OWN cross-compiling codegen tools, ended up REQUIRED by even a NATIVE
# consumer of an installed Qt 6 package -
# https://github.com/conda-forge/qt-main-feedstock/issues/273,
# https://forum.qt.io/topic/162798/qt_host_path-is-required-to-use-qt-6.9.1-on-rpi.
# GLINTFX_GL_CODEGEN_EXECUTABLE/GLINTFX_HOST_CXX_COMPILER exist for the
# exact same KIND of problem (a build-time-only codegen HOST tool
# path); this gate is what keeps them from following Qt's variable
# down into glintfx-config.cmake or glintfx.pc.
#
# real_main() does a REAL, fresh, NATIVE configure/build/install of
# THIS project (never cross-compiling - see this file's own header
# comment on why: the forbidden tokens live in
# cmake/glintfx-config.cmake.in and cmake/glintfx.pc.in, which are
# rendered IDENTICALLY regardless of GLINTFX_GL_CODEGEN_HOST_MODE - a
# native build proves the templates are clean BY CONSTRUCTION, and
# stays runnable on every one of the five CI platforms, none of which
# is guaranteed to have a cross toolchain installed), the SAME layout-
# proving shape tests/tools/check_install_includedir.sh already uses.
#
# --selftest (GODS_LAWS.md L-36: a gate only counts once PROVEN red)
# plants each forbidden token into a FIXTURE copy of the real
# cmake/glintfx-config.cmake.in / cmake/glintfx.pc.in TEMPLATES - the
# literal "plantar a variavel no gabarito do config" the plan's own
# H-5 estreia vermelha describes - and proves check_gl_codegen_host_leak()
# reproves each one, citing the fixture path.
#
# Usage:
#   check_gl_codegen_host_leak.py <glintfx-source-dir> <cxx-compiler> <generator>
#   check_gl_codegen_host_leak.py --selftest
#
# <generator> is the SAME CMAKE_GENERATOR this ctest run's own outer
# build already resolved (tests/CMakeLists.txt passes it verbatim) -
# never hardcoded to "Ninja" here, the same reasoning
# embed_dll_colocation_test's own comment gives for why a nested
# configure must receive it explicitly instead of falling back to
# whatever generator happens to be on PATH.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_gl_codegen_host_leak.py"

# Every GL-CODEGEN-HOST-TOOL identifier that is a BUILD-time-only
# detail (cmake/GlintfxGlCodegenHostTool.cmake, cmake/GlintfxOptions.cmake)
# and therefore must NEVER be referenced by an INSTALLED file. Closed
# by construction (GODS_LAWS.md L-40 item 5), not a directed search:
# every cache variable and every INTERNAL variable that module sets.
FORBIDDEN_TOKENS = (
    "GLINTFX_GL_CODEGEN_EXECUTABLE",
    "GLINTFX_HOST_CXX_COMPILER",
    "GLINTFX_GL_CODEGEN_HOST_MODE",
    "GLINTFX_GL_CODEGEN_ABI_EXPECTED",
    "GLINTFX_GL_CODEGEN_RESOLVED_HOST_CXX_COMPILER",
    "GLINTFX_GL_CODEGEN_DETECTED_HOST_CXX",
)

# The two installed-file SHAPES a consumer's build could read (a
# find_package(glintfx) CMake package tree, or a pkg-config .pc file) -
# GODS_LAWS.md L-40's own piso de varredura: real_main() reproves if
# NEITHER shape is found under the fresh install prefix (a broken
# install that installed nothing is not "nothing to leak", it is a
# varredura vazia).
_INSTALLED_SUFFIXES = (".cmake", ".pc")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- the check itself (shared by real_main and --selftest) -----------


def scanned_installed_files(root):
    files = []
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            if name.endswith(_INSTALLED_SUFFIXES):
                files.append(os.path.join(dirpath, name))
    return files


def violations_in_file(path, tokens):
    violations = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for lineno, line in enumerate(handle, start=1):
                for token in tokens:
                    if token in line:
                        violations.append((path, lineno, token))
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
    return violations


# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero .cmake/.pc
# files under the install prefix means the install itself is broken,
# not that there is nothing to leak - reproves, never a silent pass.
def require_nonempty_scan(file_count):
    if file_count == 0:
        print(f"{SCRIPT_NAME}: varredura vazia (0 arquivo .cmake/.pc instalado)", file=sys.stderr)
        return False
    return True


def check_gl_codegen_host_leak(root, tokens=FORBIDDEN_TOKENS):
    files = scanned_installed_files(root)
    file_count = len(files)

    if not require_nonempty_scan(file_count):
        return False

    violations = []
    for f in files:
        violations.extend(violations_in_file(f, tokens))

    if violations:
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GL-CODEGEN-HOST-TOOL H-5, D-11 de "
            "docs/plano-fecho-w7b.md - variavel de ferramenta HOST vazou para o instalado):",
            file=sys.stderr,
        )
        for path, lineno, token in violations:
            print(f"{path}:{lineno}: {token}", file=sys.stderr)
        return False

    print(f"{SCRIPT_NAME}: 0 ocorrencia(s) em {file_count} arquivo(s) .cmake/.pc instalado(s) varrido(s)")
    return True


# --- real mode: a genuine, fresh, native install ----------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-codegen-host-leak-", dir=os.environ.get("TMPDIR"))


def configure_build_install(glintfx_src, cxx, generator, build_dir, prefix):
    configure = subprocess.run(
        [
            "cmake",
            "-S", glintfx_src,
            "-B", build_dir,
            "-G", generator,
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            "-DGLINTFX_BUILD_TESTS=OFF",
        ],
        capture_output=True, text=True,
    )
    if configure.returncode != 0:
        fail(f"configure falhou:\n{configure.stdout}\n{configure.stderr}")

    build = subprocess.run(["cmake", "--build", build_dir], capture_output=True, text=True)
    if build.returncode != 0:
        fail(f"build falhou:\n{build.stdout}\n{build.stderr}")

    install = subprocess.run(
        ["cmake", "--install", build_dir, "--prefix", prefix], capture_output=True, text=True,
    )
    if install.returncode != 0:
        fail(f"install falhou:\n{install.stdout}\n{install.stderr}")


def real_main(args):
    if len(args) != 3:
        fail("usage: check_gl_codegen_host_leak.py <glintfx-source-dir> <cxx-compiler> <generator>")
    glintfx_src, cxx, generator = args
    if not os.path.isdir(glintfx_src):
        fail(f"glintfx source dir not found: {glintfx_src}")

    scratch = make_scratch_workdir()
    try:
        build_dir = os.path.join(scratch, "build")
        prefix = os.path.join(scratch, "prefix")
        configure_build_install(glintfx_src, cxx, generator, build_dir, prefix)

        if not check_gl_codegen_host_leak(prefix):
            fail("variavel de ferramenta HOST do GL-CODEGEN-HOST-TOOL vazou para o instalado (ver acima)")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


# --- fixtures and controls for --selftest -----------------------------

_REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
_REAL_CONFIG_TEMPLATE = os.path.join(_REPO_ROOT, "cmake", "glintfx-config.cmake.in")
_REAL_PC_TEMPLATE = os.path.join(_REPO_ROOT, "cmake", "glintfx.pc.in")


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


def _copy_real_templates_as_fixture_install(root):
    """Mirrors the SHAPE a real install leaves behind (a *.cmake and a
    *.pc file under an arbitrary prefix tree), using the REAL, live
    template files' own content - not a hand-written stand-in that
    could silently drift from what cmake/GlintfxInstall.cmake actually
    ships."""
    lib_cmake_dir = os.path.join(root, "lib", "cmake", "glintfx")
    pkgconfig_dir = os.path.join(root, "lib", "pkgconfig")
    os.makedirs(lib_cmake_dir, exist_ok=True)
    os.makedirs(pkgconfig_dir, exist_ok=True)
    shutil.copy(_REAL_CONFIG_TEMPLATE, os.path.join(lib_cmake_dir, "glintfx-config.cmake"))
    shutil.copy(_REAL_PC_TEMPLATE, os.path.join(pkgconfig_dir, "glintfx.pc"))


# Positive control: the REAL templates, untouched. Expected: passes -
# proves the templates are clean TODAY, not just that the check logic
# can pass something.
def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    _copy_real_templates_as_fixture_install(root)

    outcome = capture(lambda: check_gl_codegen_host_leak(root))
    if outcome.result:
        print("selftest: controle POSITIVO OK (templates reais, sem token proibido)")
        return True
    print("selftest: controle POSITIVO FALHOU (templates reais deveriam ter sido aprovados)", file=sys.stderr)
    print(outcome.text, file=sys.stderr)
    return False


# Negative control (the estreia vermelha docs/plano-fecho-w7b.md's own
# H-5 row names verbatim: "plantar a variavel no gabarito do config
# numa copia reprova"): for EACH forbidden token, plant it into a
# FRESH copy of the real glintfx-config.cmake.in TEMPLATE and confirm
# the gate reproves, citing the mutated fixture path and the exact
# token.
def selftest_negative_control(scratch, capture):
    ok = True
    for token in FORBIDDEN_TOKENS:
        root = os.path.join(scratch, f"negative_{token}")
        _copy_real_templates_as_fixture_install(root)
        planted_path = os.path.join(root, "lib", "cmake", "glintfx", "glintfx-config.cmake")
        with open(planted_path, "a", encoding="utf-8") as handle:
            handle.write(f"\n# vazamento plantado: {token}=/algum/caminho\n")

        outcome = capture(lambda root=root: check_gl_codegen_host_leak(root))
        if outcome.result:
            print(f"selftest: controle NEGATIVO FALHOU (token '{token}' plantado nao foi pego)", file=sys.stderr)
            ok = False
        elif token not in outcome.text:
            print(f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou o token '{token}')", file=sys.stderr)
            print(outcome.text, file=sys.stderr)
            ok = False

    if ok:
        print(f"selftest: controle NEGATIVO OK ({len(FORBIDDEN_TOKENS)} token(s) plantado(s), todos pegos)")
    return ok


# Empty-scan control (GODS_LAWS.md L-40, the SAME belt check_no_x11.py's
# own selftest_empty_scan_control wears): an install prefix with no
# .cmake/.pc file at all must REPROVE, never pass silently.
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)
    with open(os.path.join(root, "readme.txt"), "w", encoding="utf-8") as handle:
        handle.write("nada aqui e .cmake nem .pc\n")

    outcome = capture(lambda: check_gl_codegen_host_leak(root))
    if outcome.result:
        print("selftest: controle DE VARREDURA VAZIA FALHOU (deveria ter reprovado)", file=sys.stderr)
        return False
    print("selftest: controle DE VARREDURA VAZIA OK (0 arquivos .cmake/.pc reprova)")
    return True


def run_selftest():
    scratch = tempfile.mkdtemp(prefix="glintfx-codegen-host-leak-selftest-", dir=os.environ.get("TMPDIR"))
    capture = _make_capture()
    try:
        controls = (
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
        )
        if all(controls):
            print(f"{SCRIPT_NAME}: selftest OK (3 controles)")
            return True
        print(f"{SCRIPT_NAME}: selftest FALHOU", file=sys.stderr)
        return False
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        if not run_selftest():
            fail("selftest FALHOU (ver mensagens acima)")
        return
    real_main(sys.argv[1:])


if __name__ == "__main__":
    main()
