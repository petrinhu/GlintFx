#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_pkgconfig_validate.py - CI gate for the PKG-VALIDATE install(CODE)
# step (cmake/GlintfxPkgConfigValidate.cmake,
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in): proves it actually
# runs on `cmake --install`, actually reaches real broken layouts with a
# self-sufficient diagnostic, and actually skips via BOTH halves of its
# escape hatch - across REAL, genuine cmake configure/build/install
# cycles, never a text simulation of one.
#
# PORT of the former tests/tools/check_pkgconfig_validate.sh (POSIX sh),
# retired in the same fatia that wrote this file. Ordem do lider,
# 04/09/2026, transmitida por outro agente e verbatim: "O .pc volta a
# ser instalado no Windows, e decidimos juntos o que fazer com o
# validador que reprova instalação boa." (o CTO havia, em modo autonomo,
# restrito o .sh a `if(UNIX)` em 27/08/2026 - essa decisao foi
# REVERTIDA no mesmo dia pelo lider). A resposta do lider ao pendente do
# validador: ENSINAR o verificador a reconhecer o nome do artefato em
# cada sistema, e roda-lo nas cinco plataformas - nao um segundo
# validador so para o Windows, nem uma ausencia declarada la. Este
# arquivo e essa resposta: registrado SEM guarda de sistema (see
# tests/CMakeLists.txt), a mesma forma GATE-TREE-PARITY ja provou
# funcionar para check_layers.py/check_vendor_purity.py/
# check_blank_install_dir_rejected.py.
#
# EIGHTEEN scenarios, not eight: this file's own header, and the
# comment this port replaces in tests/CMakeLists.txt, said "the eight
# scenarios" - stale prose left behind by PKG-WIN-SCOPE's and
# PKG-NATIVE's own later additions (scenarios 9 through 18 were bolted
# on after that comment was written, and it was never updated). The
# closed list actually enumerated below, and exercised by main(), is
# the EIGHTEEN run_*_scenario() functions this file's own main() calls -
# counted, not assumed (GODS_LAWS.md L-40).
#
#   1. run_default_layout_scenario - default layout, real end-to-end
#      install (GREEN).
#   2. run_destdir_scenario - DESTDIR, Fedora format (GREEN); proves the
#      validator's own staged-path resolution, not glintfx.pc's content.
#   3. run_configure_time_hatch_scenario - configure-time escape hatch
#      (-DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON): the install(CODE) step
#      is never even REGISTERED.
#   4. run_install_time_hatch_scenario - install-time escape hatch
#      (GLINTFX_SKIP_PKGCONFIG_VALIDATION=1 in the environment), against
#      a build NOT configured with the hatch above - the second,
#      independent half.
#   5. run_broken_library_scenario - broken install, library artifact
#      removed (RED, real error output): the exact "partially-removed
#      install" PACKAGING.md names.
#   6. run_empty_flags_floor_scenario - empty Cflags/Libs floor
#      (GODS_LAWS.md L-40, RED): correct variable=includedir/libdir,
#      blank Cflags:/Libs: lines.
#   7. run_missing_pc_file_scenario - missing glintfx.pc itself (RED),
#      the OTHER half of "partially-removed install".
#   8. run_pkgconfig_absent_scenario - pkg-config absent from PATH
#      (WARNING, not FATAL - declared downgrade), sandboxed via
#      CMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY against an empty root -
#      confirmed live on this machine (see this file's own selftest-less
#      verification below) to genuinely restrict find_program() in
#      script mode, not merely assumed.
#   9. run_windows_forced_broken_library_scenario - the SAME broken
#      install as scenario 5, re-validated with -DWIN32=1 forcing the
#      Windows branch (RED, PKG-WIN-SCOPE regression proof): must still
#      FATAL, never claim the artifact is "genuinely on disk".
#  10. run_windows_forced_healthy_conversation_warning_scenario -
#      Windows-forced, healthy content, only the pkg-config BINARY
#      conversation itself fails (WARNING, not FATAL - the one
#      legitimate downgrade).
#  11. run_headers_missing_scenario - installed header tree missing, on
#      both a real Unix run AND a Windows-forced run (RED on both).
#  12. run_relative_libdir_cwd_attack_scenario - relative libdir,
#      attacker-controlled CWD, still REFUSES (RED, PKG-WIN-SCOPE round
#      5 regression proof).
#  13. run_real_pkgconfig_syntax_variants_scenario - real pkg-config's
#      own variable syntax (whitespace around "=", trailing comment)
#      accepted (GREEN), cross-checked against a real pkg-config/pkgconf
#      binary when one is on PATH.
#  14. run_relative_prefix_ordinary_dispatch_scenario - relative
#      --prefix, ordinary dispatch (GREEN, PKG-WIN-SCOPE round 6
#      regression proof): no doubled prefix/libdir/pkgconfig segment.
#  15. run_duplicate_variable_rejected_scenario - a glintfx.pc defining a
#      variable TWICE is REJECTED, closed (RED, PKG-NATIVE), by CMake's
#      own native pkg-config parser under STRICTNESS STRICT.
#  16. run_absolute_libdir_scenario - absolute CMAKE_INSTALL_LIBDIR
#      still passes (GREEN, ESCOPO.md paragraph 8's own EXTRA case).
#  17. run_destdir_relative_ordinary_dispatch_scenario - DESTDIR
#      relative, ordinary dispatch (GREEN): the DESTDIR-shaped sibling
#      of scenario 14.
#  18. run_looks_rooted_selftest_scenario - PKG-WIN-SCOPE round 7
#      regression proof (GREEN): glintfx_pkgconfig_looks_rooted()
#      (cmake/GlintfxPkgConfigValidateInstalled.cmake.in) correctly
#      classifies a driveless POSIX-rooted value (e.g. "/usr" - the
#      exact shape that, concatenated with a DESTDIR carrying its OWN
#      drive letter on a real Windows install, used to produce
#      "C:/.../buildrootD:/usr/lib/pkgconfig"), a Windows drive-letter
#      value in either slash direction, and a UNC path as
#      already-rooted (never re-anchored, never drive-letter-borrowed),
#      while a genuinely relative token still requires anchoring - as
#      PURE, platform-INDEPENDENT string logic (never native
#      `if(IS_ABSOLUTE ...)`/`cmake_path(IS_ABSOLUTE ...)`, whose
#      drive-letter recognition is compiled into the cmake BINARY only
#      `#if defined(_WIN32)` and therefore cannot diverge - or be
#      exercised - differently on this Linux machine no matter what a
#      script sets `-DWIN32=1` to), so this exact classification is
#      proven, RED then GREEN, on Linux too (GODS_LAWS.md project L-04).
#
# What this script does NOT test, declared (GODS_LAWS.md L-27):
# component-scoped installs (`cmake --install --component X`) and
# cross-compiled TARGET binaries - see
# GlintfxPkgConfigValidateInstalled.cmake.in's own file header for why
# those are declared, not silently assumed, safe. This port keeps the
# retired .sh's own "-DWIN32=1" forcing for scenarios 9/10/11 rather
# than requiring a real Windows machine - the difference from the
# retired .sh is that THIS file, unguarded, ALSO runs for real on actual
# Windows CI now (tests/CMakeLists.txt no longer wraps it in
# `if(UNIX)`), where every scenario that installs a real library
# artifact produces and deletes a genuine "glintfx.lib", not a
# hand-planted Unix name - see LIBRARY_ARTIFACT_PATTERNS below.
#
# LIBRARY_ARTIFACT_PATTERNS (ENSINANDO O VERIFICADOR, 04/09/2026): the
# retired .sh only ever ran on Unix (if(UNIX) in tests/CMakeLists.txt),
# so its own artifact-removal step only ever searched for
# "libglintfx.so*"/"libglintfx.a" - it had no branch for "glintfx.lib"
# at all, and would have silently found nothing to delete on a real
# Windows install (README.md documents the real names: "glintfx.dll"
# plus its "glintfx.lib" import library for the shared build, and
# "glintfx.lib" for the static build). This port's own closed list of
# three - DUPLICATED, not imported, from
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in's own
# glintfx_pkgconfig_library_glob_patterns() (that function is read, not
# assumed: it globs all three patterns UNCONDITIONALLY, never gated on
# WIN32 - confirmed by reading it, 04/09/2026) - is what actually closes
# that gap: every scenario that deletes a real library artifact
# (5/9/11/12) now searches for, and COUNTS, all three, and fails loudly
# if none is found (a real improvement over the retired .sh, which
# silently proceeded to re-run the validator even if its own `find`
# matched nothing - the RED assertion two lines later happened to catch
# that by accident, never naming the real defect). If that three-entry
# list ever changes, update BOTH this constant and the .cmake.in
# function - each file's header cross-references the other.
#
# LIGHTWEIGHT --selftest: unlike check_layers.py/check_vendor_purity.py/
# check_blank_install_dir_rejected.py, this file has no synthetic-fixture
# mode that replaces the real thing - the real thing here IS a synthetic
# fixture already (hand-assembled .pc files, deliberately broken real
# installs), and real_main()'s own 17 scenarios already are the
# GODS_LAWS.md L-40 red/green proof. --selftest here covers only the
# PURE, cmake-free logic this port adds on top of the retired .sh: the
# universal wrap-normalization (see ASSERT-WRAP below) and the closed
# LIBRARY_ARTIFACT_PATTERNS list, neither of which needs a real compiler
# or a real cmake configure to exercise.
#
# ASSERT-WRAP, APPLIED UNIVERSALLY (fixing a gap the retired .sh left
# open): that file's own header already named this defect class
# (CMake's message() reflow splitting a multi-word phrase a `case`
# pattern searches for) and fixed it for SOME comparisons
# (run_broken_library_scenario, run_missing_pc_file_scenario, and the
# Windows-forced siblings) via its own normalize_wrapped_message() - but
# left others unfixed "by prudence, out of scope", including its own
# run_configure_time_hatch_scenario (the retired .sh's line 482, a bare
# `case "$output" in *"post-install pkg-config validation disabled at
# configure time"*)`, never normalized). Rather than hunt line-by-line
# for which comparisons are exposed and which are not, EVERY comparison
# in this port runs through normalize_wrapped_message() first,
# unconditionally - normalizing a phrase that was never going to wrap
# is a no-op, so this is strictly safer than the selective fix it
# replaces, and closes the exact gap the retired .sh's own line 482 left
# open.
#
# Usage:
#   check_pkgconfig_validate.py <glintfx-source-dir> <cxx-compiler>
#   check_pkgconfig_validate.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import fnmatch
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT_NAME = "check_pkgconfig_validate.py"

# See this file's own header, "LIBRARY_ARTIFACT_PATTERNS" - DUPLICATED,
# not imported, from cmake/GlintfxPkgConfigValidateInstalled.cmake.in's
# own glintfx_pkgconfig_library_glob_patterns(). fnmatch-style globs.
LIBRARY_ARTIFACT_PATTERNS = (
    "libglintfx.so*",
    "libglintfx.a",
    "glintfx.lib",
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- ASSERT-WRAP: normalize before every comparison ------------------


def normalize_wrapped_message(text):
    """Collapses every run of whitespace - embedded newlines included -
    back to one space, so a substring search never misses just because
    CMake's own message() reflow happened to land between two words of
    the phrase being searched for. CMake's reflow never breaks INSIDE an
    unbroken run of non-whitespace (a path or identifier with no spaces
    stays intact on one physical line, however long) - only BETWEEN
    words - so this is a safe reconstruction, never a lossy one. See
    this file's own header, "ASSERT-WRAP, APPLIED UNIVERSALLY".
    """
    return re.sub(r"\s+", " ", text)


# --- ASSERT-SEP: normalize path separators before every path comparison --


def to_posix_path(path):
    """PKG-WIN-SCOPE round 8 (VERMELHO, 05/09/2026, run 33946160472,
    jobs 'Windows (primario)' Debug/estatico/compartilhado):
    run_destdir_scenario's own "DESTDIR install succeeded
    (outcome=degraded) did not name the expected resolved path" -
    followed, right below it in the SAME failure output, by the
    validator's own message naming the identical file, character for
    character except for the separator ('C:/Users/.../buildroot/usr/
    lib/pkgconfig/glintfx.pc'). CMake's own internal path
    representation always uses forward slashes, on every platform it
    runs on - this file's own scenario 18 comment (PKG-WIN-SCOPE round
    7) already cites this same convention - while a path THIS script
    builds with os.path.join()/os.walk() is backslash-separated on
    Windows (`os.sep == "\\\\"` there). must_contain_in_order() is a
    plain substring search, never separator-aware, so it never matched
    two spellings of the same file.

    Deliberately UNCONDITIONAL (`path.replace("\\\\", "/")`, never
    gated on `os.sep` the way check_env_sweep.py's own _as_posix() or
    this file's own find_libdir_relative_to_prefix() are) - the same
    portability lesson scenario 18's own glintfx_pkgconfig_looks_rooted()
    already drew for a different comparison: a check gated on the
    CALLING platform's own os.sep can only ever be exercised on that
    platform, so a Linux-only CI run could never prove this fix RED
    then GREEN (GODS_LAWS.md project L-04, "provado em cada sistema").
    An unconditional replace is safe everywhere this script runs it:
    every path it builds itself uses '/' already on POSIX (os.path.join
    never introduces '\\\\' there), so the replace is a genuine no-op on
    Linux/macOS and the real fix on Windows - proven by
    selftest_windows_path_separator_control below with a hand-built
    Windows-style string, never a real os.path.join() call, so it bites
    on THIS (Linux) machine too, not only on a real Windows one.

    Used ONLY at the comparison boundary (assert_real_install_
    validated()'s own `pc_file` argument) - every real filesystem call
    elsewhere in this file (os.path.isfile, os.remove, open,
    rewrite_libdir_line) keeps using the path's native separator, which
    is what the OS actually requires.
    """
    return path.replace("\\", "/")


def must_contain(output, phrase, on_fail):
    if phrase not in normalize_wrapped_message(output):
        fail(f"{on_fail} Got:\n{output}")


def must_contain_in_order(output, *phrases, on_fail):
    """Mirrors the retired .sh's own `case "$text" in *"A"*"B"*"C"*)`
    shape: each phrase must appear, in order, none overlapping the
    previous one's match - not merely "all present somewhere".
    """
    normalized = normalize_wrapped_message(output)
    pos = 0
    for phrase in phrases:
        idx = normalized.find(phrase, pos)
        if idx == -1:
            fail(f"{on_fail} Got:\n{output}")
        pos = idx + len(phrase)


def must_not_contain(output, phrase, on_fail):
    if phrase in normalize_wrapped_message(output):
        fail(f"{on_fail} Got:\n{output}")


# --- process / cmake plumbing -----------------------------------------


def run(cmd, cwd=None, env=None):
    """Runs cmd, returns (returncode, combined stdout+stderr) - stderr
    interleaved into stdout in real time (subprocess.STDOUT), the same
    ordering `2>&1` gives the retired .sh.
    """
    proc = subprocess.run(
        cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    return proc.returncode, proc.stdout


def run_expect_success(cmd, on_fail, cwd=None, env=None):
    rc, output = run(cmd, cwd=cwd, env=env)
    if rc != 0:
        fail(f"{on_fail}\n{output}")
    return output


def run_expect_failure(cmd, on_fail, cwd=None, env=None):
    rc, output = run(cmd, cwd=cwd, env=env)
    if rc == 0:
        fail(f"{on_fail}\n{output}")
    return output


def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") without a hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    return tempfile.mkdtemp(prefix="glintfx-pkgvalidate-", dir=os.environ.get("TMPDIR"))


# C_COMPILER_FLAG (PKG-NATIVE, see the retired .sh's own header): only
# appends -DCMAKE_C_COMPILER when CC is actually set in the environment,
# so a machine where implicit C detection already works is untouched.
def extra_c_compiler_flags():
    cc = os.environ.get("CC")
    return [f"-DCMAKE_C_COMPILER={cc}"] if cc else []


def reconfigure_glintfx(glintfx_src, build_dir, cxx, prefix_value, skip_validation):
    cmd = [
        "cmake", "-S", glintfx_src, "-B", build_dir, "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_CXX_COMPILER={cxx}",
    ]
    cmd += extra_c_compiler_flags()
    cmd += [
        "-DGLINTFX_BUILD_TESTS=OFF",
        f"-DCMAKE_INSTALL_PREFIX={prefix_value}",
        f"-DGLINTFX_SKIP_PKGCONFIG_VALIDATION={skip_validation}",
    ]
    return run_expect_success(
        cmd, f"reconfigure of glintfx (prefix={prefix_value}, skip_validation={skip_validation}) FAILED:"
    )


def build_glintfx(build_dir):
    return run_expect_success(["cmake", "--build", build_dir], "build of glintfx FAILED:")


# --- install-tree inspection (mirrors "validate the output, do not
# predict the input" - the retired .sh's own discipline) -------------


def find_pc_file_under(root):
    for dirpath, _dirnames, filenames in os.walk(root):
        if "glintfx.pc" in filenames:
            return os.path.join(dirpath, "glintfx.pc")
    fail(f"no glintfx.pc found anywhere under {root} after install")


def find_libdir_relative_to_prefix(prefix):
    prefix_abs = os.path.abspath(prefix)
    for dirpath, _dirnames, _filenames in os.walk(prefix_abs):
        if os.path.basename(dirpath) == "pkgconfig":
            libdir_abs = os.path.dirname(dirpath)
            if libdir_abs == prefix_abs or libdir_abs.startswith(prefix_abs + os.sep):
                rel = os.path.relpath(libdir_abs, prefix_abs)
                return rel.replace(os.sep, "/")
            fail(f"pkgconfig/ directory {dirpath} is not under prefix {prefix} - cannot derive a relative libdir")
    fail(f"no pkgconfig/ directory found anywhere under {prefix} - cannot derive the real libdir subdirectory")


def find_generated_validator_script(build_dir):
    script = os.path.join(build_dir, "GlintfxPkgConfigValidateInstalled.cmake")
    if not os.path.isfile(script):
        fail(
            f"GlintfxPkgConfigValidateInstalled.cmake not found at {script} - "
            "glintfx_register_pkgconfig_validation() did not generate it (was "
            "GLINTFX_SKIP_PKGCONFIG_VALIDATION=ON at configure time for this build?)"
        )
    return script


def find_headers_dir(prefix, maxdepth=3):
    prefix_path = Path(prefix).resolve()
    for path in sorted(prefix_path.rglob("glintfx")):
        if not path.is_dir():
            continue
        rel_parts = path.relative_to(prefix_path).parts
        if len(rel_parts) > maxdepth:
            continue
        if "include" in path.parts:
            return path
    return None


def find_library_artifacts(prefix, maxdepth=3):
    """Every file under <prefix>, up to maxdepth levels deep (prefix
    itself is depth 0, mirroring the retired .sh's own `find -maxdepth
    3`), whose name matches any of LIBRARY_ARTIFACT_PATTERNS.
    """
    prefix_abs = os.path.abspath(prefix)
    matches = []
    for dirpath, dirnames, filenames in os.walk(prefix_abs):
        rel = os.path.relpath(dirpath, prefix_abs)
        depth_of_dir = 0 if rel == "." else rel.count(os.sep) + 1
        if depth_of_dir >= maxdepth:
            dirnames[:] = []
        if depth_of_dir + 1 > maxdepth:
            continue
        for name in filenames:
            if any(fnmatch.fnmatch(name, pattern) for pattern in LIBRARY_ARTIFACT_PATTERNS):
                matches.append(os.path.join(dirpath, name))
    return matches


def remove_library_artifacts(prefix):
    """Deletes every real library artifact found (see
    find_library_artifacts) and returns the list removed - CALLERS must
    check this is non-empty before trusting the precondition it sets up
    (GODS_LAWS.md L-40: a scan that finds nothing and proceeds anyway is
    the exact defect class this house exists to rule out; the retired
    .sh did not check this itself and relied on the scenario's own RED
    assertion to catch it by accident).
    """
    artifacts = find_library_artifacts(prefix)
    for path in artifacts:
        os.remove(path)
    return artifacts


def placeholder_library_artifact_name():
    """The platform-appropriate name, from the SAME closed
    LIBRARY_ARTIFACT_PATTERNS list, for a hand-assembled pkgconfig
    fixture's fake library file. The validator's own glob check never
    filters by platform (see this file's own header), so any of the
    three names would satisfy it - this picks the one a REAL build on
    THIS platform would actually produce, so a fixture inspected on
    Windows looks like a real Windows artifact, not a borrowed Unix
    name.
    """
    return "glintfx.lib" if sys.platform.startswith("win") else "libglintfx.so"


def write_placeholder_library_artifact(libdir_path):
    os.makedirs(libdir_path, exist_ok=True)
    path = os.path.join(libdir_path, placeholder_library_artifact_name())
    open(path, "wb").close()
    return path


def rewrite_libdir_line(pc_file, new_value):
    """sed -i equivalent (`s#^libdir=.*#libdir=<new_value>#`) done in
    pure Python - portable by construction, unlike the retired .sh's own
    `sed -i.bak`, which that file's own comment already flagged as
    depending on a GNU-vs-BSD sed difference "this project has not
    fully enumerated".
    """
    with open(pc_file, "r", encoding="utf-8") as handle:
        lines = handle.readlines()
    for index, line in enumerate(lines):
        if line.startswith("libdir="):
            lines[index] = f"libdir={new_value}\n"
            break
    else:
        fail(f"{pc_file}: no 'libdir=' line found to rewrite")
    with open(pc_file, "w", encoding="utf-8") as handle:
        handle.writelines(lines)


# --- scenario 1 --------------------------------------------------------


def pkgconfig_binary_on_path():
    """Mirrors cmake/GlintfxPkgConfigValidateInstalled.cmake.in's own
    `find_program(GLINTFX_PKGCONFIG_VALIDATE_EXE NAMES pkg-config
    pkgconf NO_CACHE)` - same two names, same order - so this scenario
    expects the SAME branch a real `cmake --install` on THIS machine
    actually takes, instead of demanding the SUCCESS message
    unconditionally. VERMELHO 2 (04/09/2026, jobs 'Windows - Debug/
    estatico/compartilhado', run 33910349281): the Windows runners
    have neither tool on PATH, `find_program()` legitimately takes the
    scenario-8 WARNING branch (already proven correct there), and
    scenario 1 was failing only because it never considered that
    branch - the install itself, and glintfx.pc's content, were both
    correct.
    """
    for candidate in ("pkg-config", "pkgconf"):
        if shutil.which(candidate):
            return candidate
    return None


# --- shared: the pkg-config CONVERSATION has THREE honest outcomes,
# not two (scenarios 1, 2, 13, 14, 16, 17) --------------------------------
#
# VERMELHO 3 (05/09/2026, run 33913549588, jobs "Windows (primario)"
# Debug/estatico/compartilhado): run_default_layout_scenario's own
# pkgconfig_binary_on_path() probe (VERMELHO 2, above) correctly widened
# the branch from ONE outcome to TWO - tool absent, or tool present and
# 'pkg-config --exists glintfx' succeeds - but
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in itself already names a
# THIRD, WIN32-only outcome (its own header, "Talking to the pkg-config
# BINARY itself"; proven correct, forced, by
# run_windows_forced_healthy_conversation_warning_scenario, scenario 10):
# the tool IS on PATH, but the '--exists' conversation itself fails
# (measured cause, same file's header: Strawberry Perl's Pure-Perl
# 'pkg-config.bat', the only implementation on GitHub Actions'
# windows-latest runner, mis-splits a bare PKG_CONFIG_PATH value on ':'
# and shreds the drive letter) - a GODS_LAWS.md L-27 DECLARED DOWNGRADE
# (WARNING, exit 0), never a FATAL_ERROR, because this file's own content
# check (CMake's native pkg-config parser) already confirmed glintfx.pc,
# the headers and the library artifact are genuinely on disk BEFORE this
# conversation is even attempted. run_default_layout_scenario knew only
# the first two outcomes, and reproved a genuinely correct Windows
# install the moment the third one fired for real on the server.
#
# GODS_LAWS.md L-17 ("isolado ou padrao?"): run_destdir_scenario,
# run_real_pkgconfig_syntax_variants_scenario,
# run_relative_prefix_ordinary_dispatch_scenario,
# run_absolute_libdir_scenario and
# run_destdir_relative_ordinary_dispatch_scenario all demanded the SAME
# unconditional "post-install pkg-config validation passed" - the exact
# same two-outcome assumption, never caught only because
# run_default_layout_scenario's own sys.exit(1) always aborted this
# whole file (real_main() runs every scenario in one sequential process,
# fail() is sys.exit(1)) before any of the other five ever got the
# chance to run for real against a machine where the third outcome
# fires. All six now share this one function instead of each repeating
# (and each separately forgetting) the same three-way check
# (GODS_LAWS.md L-17, DRY "regra de tres": six call sites, not one).
def assert_windows_degraded_conversation_is_honest(output, on_fail_prefix):
    """The third outcome is a DECLARED DOWNGRADE (GODS_LAWS.md L-27),
    never a silent "ok": it is honest only when it ALSO names its own
    content check as the thing that actually confirmed the install -
    exactly what run_windows_forced_healthy_conversation_warning_scenario
    (scenario 10) already proves correct with the branch forced.
    Accepting the warning phrase ALONE - which is what
    run_default_layout_scenario alone used to do, before this fix -
    would let this gate pass against an install whose content was never
    actually confirmed, for ANY reason the '--exists' conversation
    happens to fail (GODS_LAWS.md L-40: a check that passes on the mere
    absence of the wrong words is not a check).
    """
    must_contain_in_order(
        output,
        "post-install pkg-config content check",
        "CONVERSATION could not be verified on this Windows machine",
        "native content check already ran before this point",
        "already confirmed glintfx.pc, the headers and the library artifact are "
        "genuinely on disk with real content",
        on_fail=(
            f"{on_fail_prefix} took the Windows binary-conversation-failed WARNING "
            "branch, but did not honestly present, IN ORDER, its own content-check STATUS "
            "line, the conversation-unverifiable warning, and the warning's own honest "
            "attribution of the confirmation to that content check having already run - "
            "accepting the warning phrase alone would let this pass against an install "
            "whose content was never actually confirmed."
        ),
    )


def assert_real_install_validated(output, on_fail_prefix, pc_file=None):
    """The THREE outcomes a REAL, unforced install's own pkg-config
    validation can honestly report, counted, not assumed (GODS_LAWS.md
    L-40 - see this section's own header for the real CI run that found
    two of the six call sites below only handling the first two):

      1. "absent"   - pkg-config/pkgconf missing from PATH.
      2. "passed"   - present, and 'pkg-config --exists glintfx' succeeds.
      3. "degraded" - present, but WIN32 only (GODS_LAWS.md L-27 declared
         downgrade) the conversation itself fails while content was
         already confirmed.

    Every outcome's own message names the resolved pc file path in its
    OWN wording (checked separately below, anchored to that wording, NOT
    to "the path appears anywhere in the output" - a `-- Installing:
    ...` line from CMake's own ordinary install progress, printed before
    this validator ever runs, would otherwise let a wrong path inside
    the validator's OWN message go uncaught).

    Returns which outcome fired, so the CALLER prints its own
    branch-specific "ok:" line - never accept all three in silence, which
    would turn this into a check that cannot fail.
    """
    tool = pkgconfig_binary_on_path()
    normalized = normalize_wrapped_message(output)

    if tool is None:
        must_contain(
            output,
            "pkg-config (and pkgconf) not found on PATH",
            on_fail=(
                f"{on_fail_prefix}, but with neither pkg-config nor pkgconf on PATH it "
                "never printed the validator's own tool-absent WARNING - the same "
                "degrade-to-warning branch run_pkgconfig_absent_scenario (scenario 8) "
                "proves is correct."
            ),
        )
        outcome, anchor = "absent", "was written to"
    elif "CONVERSATION could not be verified on this Windows machine" in normalized:
        assert_windows_degraded_conversation_is_honest(output, on_fail_prefix)
        outcome, anchor = "degraded", "failed against the file just installed at"
    else:
        must_contain(
            output,
            "post-install pkg-config validation passed",
            on_fail=(
                f"{on_fail_prefix}, but never printed the validator's own success "
                f"message, even though '{tool}' is on PATH (find_program() should have "
                "taken the SUCCESS branch, or - on Windows only - the declared-downgrade "
                "conversation-unverifiable WARNING branch)."
            ),
        )
        outcome, anchor = "passed", "glintfx.pc at"

    if pc_file is not None:
        # ASSERT-SEP (see to_posix_path()'s own header): pc_file is built
        # by THIS script, with os.path.join()/os.walk() - backslash-
        # separated on Windows - while every message the validator itself
        # prints names the same location with CMake's own forward-slash
        # convention. Comparing the raw, unconverted value is exactly
        # PKG-WIN-SCOPE round 8's own false reprovation of a genuinely
        # correct Windows install.
        pc_file_posix = to_posix_path(pc_file)
        must_contain_in_order(
            output,
            anchor,
            pc_file_posix,
            on_fail=(
                f"{on_fail_prefix} (outcome={outcome}) did not name the expected "
                f"resolved path ({pc_file_posix}) right after its own '{anchor}' phrase - "
                "a path appearing elsewhere in the install log (for instance CMake's own "
                "'-- Installing: ...' line) would not prove the VALIDATOR's own message "
                "computed it correctly."
            ),
        )

    return outcome


def run_default_layout_scenario(build_dir, prefix):
    output = run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", prefix],
        "default-layout install unexpectedly FAILED:",
    )
    # Counts which of the THREE outcomes was taken (GODS_LAWS.md L-40:
    # never accept more than one message in silence, as that would turn
    # this into a check that cannot fail) - see
    # assert_real_install_validated()'s own header for the real CI run
    # (VERMELHO 3) that found this scenario alone knew only TWO of the
    # three outcomes a real, unforced install can honestly report.
    outcome = assert_real_install_validated(
        output, on_fail_prefix="default-layout install succeeded"
    )
    find_pc_file_under(prefix)
    descriptions = {
        "passed": (
            "pkg-config/pkgconf is on PATH, the install(CODE) validator ran and reported "
            "success against a genuinely correct install."
        ),
        "absent": (
            "neither pkg-config nor pkgconf is on PATH, so the install(CODE) validator "
            "correctly degraded to its tool-absent WARNING branch (still exit 0) instead "
            "of claiming a binary confirmation it could not perform."
        ),
        "degraded": (
            "pkg-config is on PATH, but the '--exists' CONVERSATION itself could not be "
            "verified on this (Windows) machine - the install(CODE) validator correctly "
            "degraded to its conversation-unverifiable WARNING branch (still exit 0) "
            "instead of claiming a binary confirmation it could not perform, and its own "
            "content check had already confirmed the install for real."
        ),
    }
    print(f"ok: default-layout scenario - outcome={outcome}: {descriptions[outcome]}")


# --- scenario 2 --------------------------------------------------------


def run_destdir_scenario(glintfx_src, cxx, build_dir, scratch):
    reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr", "OFF")
    build_glintfx(build_dir)

    destdir = os.path.join(scratch, "buildroot")
    env = dict(os.environ, DESTDIR=destdir)
    output = run_expect_success(
        ["cmake", "--install", build_dir], "DESTDIR install unexpectedly FAILED:", env=env
    )

    pc_file = find_pc_file_under(destdir)
    outcome = assert_real_install_validated(
        output, on_fail_prefix="DESTDIR install succeeded", pc_file=pc_file
    )
    print(
        f"ok: DESTDIR scenario (outcome={outcome}) - the validator resolved the STAGED, "
        f"DESTDIR-aware physical location ({pc_file}) and reported it, honestly, in its "
        "own message."
    )


# --- scenario 3 --------------------------------------------------------


def run_configure_time_hatch_scenario(glintfx_src, cxx, build_dir, prefix):
    configure_output = reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr/local", "ON")
    must_contain(
        configure_output,
        "post-install pkg-config validation disabled at configure time",
        on_fail=(
            "configuring with -DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON did not print the "
            "expected configure-time disable message."
        ),
    )

    build_glintfx(build_dir)
    install_output = run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", prefix],
        "install with the configure-time hatch set unexpectedly FAILED:",
    )

    must_not_contain(
        install_output,
        "pkg-config validation",
        on_fail=(
            "install with -DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON at configure time still "
            "printed a pkg-config-validation message during INSTALL - the step should "
            "never have been registered at all."
        ),
    )
    find_pc_file_under(prefix)
    print(
        "ok: configure-time escape hatch - glintfx.pc still installs correctly, and the "
        "validation install(CODE) step is never registered."
    )


# --- scenario 4 --------------------------------------------------------


def run_install_time_hatch_scenario(glintfx_src, cxx, build_dir, prefix):
    reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr/local", "OFF")
    build_glintfx(build_dir)

    env = dict(os.environ, GLINTFX_SKIP_PKGCONFIG_VALIDATION="1")
    output = run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", prefix],
        "install with GLINTFX_SKIP_PKGCONFIG_VALIDATION=1 in the environment unexpectedly FAILED:",
        env=env,
    )

    must_contain(
        output,
        "post-install pkg-config validation skipped",
        on_fail=(
            "install with GLINTFX_SKIP_PKGCONFIG_VALIDATION=1 did not print the expected "
            "install-time skip message."
        ),
    )
    must_not_contain(
        output,
        "post-install pkg-config validation passed",
        on_fail=(
            "install with the install-time hatch set still printed the validator's SUCCESS "
            "message - it ran the real checks instead of skipping them."
        ),
    )
    find_pc_file_under(prefix)
    print(
        "ok: install-time escape hatch - a build configured WITHOUT the configure-time "
        "hatch still skips validation when GLINTFX_SKIP_PKGCONFIG_VALIDATION=1 is set in "
        "the install environment."
    )


# --- scenario 5 --------------------------------------------------------


def run_broken_library_scenario(glintfx_src, cxx, build_dir, prefix):
    reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr/local", "OFF")
    build_glintfx(build_dir)
    run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", prefix],
        "broken-library scenario's own precondition install unexpectedly FAILED:",
    )

    validator_script = find_generated_validator_script(build_dir)
    removed = remove_library_artifacts(prefix)
    if not removed:
        fail(
            f"broken-library scenario: no library artifact found under {prefix} matching "
            f"any of {LIBRARY_ARTIFACT_PATTERNS} - cannot set up the precondition"
        )

    output = run_expect_failure(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={prefix}", "-P", validator_script],
        "re-running the validator against a real install with its library artifact "
        "REMOVED unexpectedly SUCCEEDED - it did not catch the broken install.",
    )

    must_contain_in_order(
        output,
        "libglintfx.so",
        "libglintfx.a",
        "is not there",
        on_fail=(
            "the broken-library RED did not name the missing artifact by its own promise "
            "('libglintfx.so*/libglintfx.a ... is not there')."
        ),
    )
    must_contain(
        output,
        "GLINTFX_SKIP_PKGCONFIG_VALIDATION=1",
        on_fail=(
            "the broken-library RED message did not mention the escape hatch "
            "(GLINTFX_SKIP_PKGCONFIG_VALIDATION=1) - a real packager hitting this needs to "
            "know how to skip it without reading this repository."
        ),
    )
    print(
        "ok: broken-library RED - the validator, re-run directly against a real install "
        f"with its library artifact deleted ({removed}), fails with a self-sufficient "
        "diagnostic naming the missing artifact and the escape hatch."
    )


# --- scenario 6 --------------------------------------------------------


def run_empty_flags_floor_scenario(build_dir, scratch, real_libdir):
    validator_script = find_generated_validator_script(build_dir)

    fixture_prefix = os.path.join(scratch, "prefix-floor-fixture")
    libdir_path = os.path.join(fixture_prefix, *real_libdir.split("/"))
    pkgconfig_dir = os.path.join(libdir_path, "pkgconfig")
    os.makedirs(pkgconfig_dir, exist_ok=True)
    os.makedirs(os.path.join(fixture_prefix, "include", "glintfx"), exist_ok=True)
    write_placeholder_library_artifact(libdir_path)

    pc_text = (
        "prefix=" + fixture_prefix + "\n"
        "libdir=${prefix}/" + real_libdir + "\n"
        "includedir=${prefix}/include\n"
        "\n"
        "Name: glintfx\n"
        "Description: check_pkgconfig_validate.py L-40 floor fixture - correct variables, blank flag lines\n"
        "Version: 0.1.0.0\n"
        "Libs:\n"
        "Cflags:\n"
    )
    with open(os.path.join(pkgconfig_dir, "glintfx.pc"), "w", encoding="utf-8") as handle:
        handle.write(pc_text)

    output = run_expect_failure(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={fixture_prefix}", "-P", validator_script],
        "re-running the validator against a fixture with CORRECT variables but BLANK "
        "Cflags:/Libs: lines unexpectedly SUCCEEDED - the L-40 floor did not fire.",
    )

    must_contain_in_order(
        output,
        "ZERO -I flags",
        "GODS_LAWS.md L-40",
        on_fail=(
            "the empty-flags RED did not name the L-40 floor by its own promise "
            "('ZERO -I flags ... GODS_LAWS.md L-40')."
        ),
    )
    print(
        "ok: empty-Cflags/Libs floor RED - a fixture with correct includedir/libdir but "
        "blank flag lines is REPROVED, not silently accepted (GODS_LAWS.md L-40)."
    )


# --- scenario 7 --------------------------------------------------------


def run_missing_pc_file_scenario(glintfx_src, cxx, build_dir, prefix):
    reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr/local", "OFF")
    build_glintfx(build_dir)
    run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", prefix],
        "missing-pc-file scenario's own precondition install unexpectedly FAILED:",
    )

    validator_script = find_generated_validator_script(build_dir)
    pc_file = find_pc_file_under(prefix)
    os.remove(pc_file)

    output = run_expect_failure(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={prefix}", "-P", validator_script],
        "re-running the validator against a real install with glintfx.pc itself REMOVED "
        "unexpectedly SUCCEEDED.",
    )

    must_contain(
        output,
        "right after installing it, and it is not there",
        on_fail="the missing-pc-file RED did not use its own promised wording.",
    )
    must_contain(
        output,
        "GLINTFX_SKIP_PKGCONFIG_VALIDATION",
        on_fail="the missing-pc-file RED message did not mention the escape hatch.",
    )
    print(
        "ok: missing-glintfx.pc RED - the validator, re-run against a real install with "
        "glintfx.pc itself deleted, fails with a self-sufficient diagnostic."
    )


# --- scenario 8 --------------------------------------------------------


def run_pkgconfig_absent_scenario(build_dir, scratch, intact_prefix):
    validator_script = find_generated_validator_script(build_dir)
    empty_root = os.path.join(scratch, "empty-find-root")
    os.makedirs(empty_root, exist_ok=True)

    output = run_expect_success(
        [
            "cmake",
            f"-DCMAKE_INSTALL_PREFIX={intact_prefix}",
            f"-DCMAKE_FIND_ROOT_PATH={empty_root}",
            "-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY",
            "-P", validator_script,
        ],
        "re-running the validator with find_program() sandboxed to an empty root "
        "(simulating a machine with no pkg-config on PATH) unexpectedly FAILED instead "
        "of warning-and-skipping.",
    )

    must_contain(
        output,
        "pkg-config (and pkgconf) not found on PATH",
        on_fail="the pkg-config-absent scenario did not print the expected warning.",
    )
    print(
        "ok: pkg-config-absent scenario - a machine with no pkg-config/pkgconf on PATH "
        "gets a WARNING and a SUCCESSFUL (exit 0) validation step, never a FATAL_ERROR "
        "for a missing tool that is not glintfx's own defect."
    )


# --- scenario 9 --------------------------------------------------------


def run_windows_forced_broken_library_scenario(glintfx_src, cxx, build_dir, prefix):
    reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr/local", "OFF")
    build_glintfx(build_dir)
    run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", prefix],
        "Windows-forced broken-library scenario's own precondition install unexpectedly FAILED:",
    )

    validator_script = find_generated_validator_script(build_dir)
    removed = remove_library_artifacts(prefix)
    if not removed:
        fail(
            f"Windows-forced broken-library scenario: no library artifact found under "
            f"{prefix} matching any of {LIBRARY_ARTIFACT_PATTERNS} - cannot set up the "
            "precondition"
        )

    output = run_expect_failure(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={prefix}", "-DWIN32=1", "-P", validator_script],
        "re-running the validator against a real install with its library artifact "
        "REMOVED, with the Windows branch FORCED, unexpectedly SUCCEEDED - the "
        "PKG-WIN-SCOPE regression (a false 'already confirmed genuinely on disk' warning "
        "at exit 0) is back.",
    )

    must_contain_in_order(
        output,
        "libglintfx.so",
        "libglintfx.a",
        "is not there",
        on_fail=(
            "the Windows-forced broken-library RED did not name the missing artifact by "
            "its own promise ('libglintfx.so*/libglintfx.a ... is not there')."
        ),
    )
    must_not_contain(
        output,
        "genuinely on disk",
        on_fail=(
            "the Windows-forced broken-library scenario still printed a claim that the "
            "library artifact is genuinely on disk, after deleting it - this is the exact "
            "false-verification defect PKG-WIN-SCOPE's adversarial review found."
        ),
    )
    print(
        "ok: Windows-forced broken-library RED - forcing the Windows branch no longer "
        "lets a real broken install (library artifact deleted) through with a false "
        "'already confirmed genuinely on disk' warning; it FAILS, closed, naming the "
        "missing artifact, exactly as it already does unforced."
    )


# --- scenario 10 -------------------------------------------------------


def run_windows_forced_healthy_conversation_warning_scenario(build_dir, intact_prefix):
    validator_script = find_generated_validator_script(build_dir)

    output = run_expect_success(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={intact_prefix}", "-DWIN32=1", "-P", validator_script],
        "re-running the validator against a genuinely intact install, with only the "
        "Windows branch forced, unexpectedly FAILED instead of warning-and-succeeding.",
    )

    must_contain(
        output,
        "CONVERSATION could not be verified on this Windows machine",
        on_fail="the Windows-forced healthy-content scenario did not print the expected binary-conversation warning.",
    )
    must_contain_in_order(
        output,
        "native content check already ran before this point",
        "already confirmed glintfx.pc, the headers and the library artifact are "
        "genuinely on disk with real content",
        on_fail=(
            "the Windows-forced healthy-content warning did not honestly attribute the "
            "confirmation to its own content check having already run."
        ),
    )
    must_contain(
        output,
        "post-install pkg-config content check",
        on_fail=(
            "the Windows-forced healthy-content scenario never printed its own "
            "content-check STATUS message before the binary-conversation warning - the "
            "content check may not have actually run first."
        ),
    )
    print(
        "ok: Windows-forced healthy-content WARNING - with genuinely correct content on "
        "disk, only the pkg-config BINARY conversation itself failing still degrades to a "
        "WARNING at exit 0, and the warning truthfully names its own, already-run content "
        "check as the thing that verified the filesystem."
    )


# --- scenario 11 -------------------------------------------------------


def run_headers_missing_scenario(glintfx_src, cxx, build_dir, prefix):
    reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr/local", "OFF")
    build_glintfx(build_dir)
    run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", prefix],
        "headers-missing scenario's own precondition install unexpectedly FAILED:",
    )

    validator_script = find_generated_validator_script(build_dir)
    headers_dir = find_headers_dir(prefix)
    if headers_dir is None:
        fail(
            f"no installed 'include/.../glintfx' header directory found anywhere under "
            f"{prefix} - cannot set up the headers-missing scenario"
        )
    shutil.rmtree(headers_dir)

    for forced_win32 in ((), ("-DWIN32=1",)):
        cmd = ["cmake", f"-DCMAKE_INSTALL_PREFIX={prefix}", *forced_win32, "-P", validator_script]
        output = run_expect_failure(
            cmd,
            "re-running the validator against a real install with its ENTIRE header "
            f"directory REMOVED (forced_win32={forced_win32!r}) unexpectedly SUCCEEDED.",
        )
        must_contain_in_order(
            output,
            "has no 'glintfx/' subdirectory",
            "installed public headers are not there",
            on_fail=(
                f"the headers-missing RED (forced_win32={forced_win32!r}) did not name the "
                "missing headers by its own promise."
            ),
        )
    print(
        "ok: headers-missing RED, on both a real Unix run and a Windows-forced run - the "
        "includedir branch of the native content check fires for a genuinely empty header "
        "directory, unconditionally on every platform."
    )


# --- scenario 12 -------------------------------------------------------


def run_relative_libdir_cwd_attack_scenario(glintfx_src, cxx, build_dir, prefix, scratch):
    reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr/local", "OFF")
    build_glintfx(build_dir)
    run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", prefix],
        "relative-libdir CWD-attack scenario's own precondition install unexpectedly FAILED:",
    )

    validator_script = find_generated_validator_script(build_dir)

    removed = remove_library_artifacts(prefix)
    if not removed:
        fail(
            f"relative-libdir CWD-attack scenario: no library artifact found under "
            f"{prefix} matching any of {LIBRARY_ARTIFACT_PATTERNS} - cannot set up the "
            "precondition"
        )

    pc_file = find_pc_file_under(prefix)
    decoy_subdir_name = "decoy-relative-libdir"
    rewrite_libdir_line(pc_file, decoy_subdir_name)

    attacker_dir = os.path.join(scratch, "attacker-cwd")
    decoy_dir = os.path.join(attacker_dir, decoy_subdir_name)
    write_placeholder_library_artifact(decoy_dir)

    output = run_expect_failure(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={prefix}", "-P", validator_script],
        "re-running the validator, from inside an attacker-controlled CWD containing a "
        "decoy library, against a real install with its OWN library artifact REMOVED and "
        "glintfx.pc's libdir rewritten to a bare relative value, unexpectedly SUCCEEDED - "
        "the PKG-WIN-SCOPE round 5 regression (a relative path resolving against the "
        "CALLER's CWD instead of glintfx.pc's own directory) is back.",
        cwd=attacker_dir,
    )

    must_contain_in_order(
        output,
        f"pkgconfig/{decoy_subdir_name}",
        "does not exist on disk",
        on_fail=(
            "the relative-libdir CWD-attack RED did not name a path anchored under "
            f"glintfx.pc's own directory ('.../pkgconfig/{decoy_subdir_name}')."
        ),
    )
    must_not_contain(
        output,
        attacker_dir,
        on_fail=(
            f"the relative-libdir CWD-attack RED message named the ATTACKER directory "
            f"({attacker_dir}) - the relative value is still resolving against the "
            "caller's CWD instead of glintfx.pc's own directory, the exact regression "
            "this scenario exists to catch."
        ),
    )
    print(
        "ok: relative-libdir CWD-attack RED - a real install with its library artifact "
        "removed and glintfx.pc's libdir rewritten to a bare relative value REFUSES even "
        "when invoked from an attacker-controlled working directory holding a decoy "
        "library under the same relative name; the resolved path is anchored under "
        "glintfx.pc's own directory, never the caller's CWD."
    )


# --- scenario 13 -------------------------------------------------------


def run_real_pkgconfig_syntax_variants_scenario(build_dir, scratch, real_libdir):
    validator_script = find_generated_validator_script(build_dir)

    fixture_prefix = os.path.join(scratch, "prefix-syntax-variants-fixture")
    libdir_path = os.path.join(fixture_prefix, *real_libdir.split("/"))
    pkgconfig_dir = os.path.join(libdir_path, "pkgconfig")
    os.makedirs(pkgconfig_dir, exist_ok=True)
    os.makedirs(os.path.join(fixture_prefix, "include", "glintfx"), exist_ok=True)
    write_placeholder_library_artifact(libdir_path)

    pc_text = (
        "prefix=${pcfiledir}/../..\n"
        "exec_prefix = ${prefix}   # whitespace around \"=\" AND a trailing comment, both on this one line\n"
        "includedir=${prefix}/include\n"
        "libdir=${exec_prefix}/" + real_libdir + "\n"
        "\n"
        "Name: glintfx\n"
        "Description: check_pkgconfig_validate.py syntax-variants fixture - whitespace around \"=\" and a trailing comment\n"
        "Version: 0.1.0.0\n"
        "Cflags: -I${includedir}\n"
        "Libs: -L${libdir} -lglintfx\n"
    )
    with open(os.path.join(pkgconfig_dir, "glintfx.pc"), "w", encoding="utf-8") as handle:
        handle.write(pc_text)

    output = run_expect_success(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={fixture_prefix}", "-P", validator_script],
        "re-running the validator against a fixture using real pkg-config's own "
        "whitespace/comment syntax unexpectedly FAILED - this is the exact 'rejects a "
        "genuinely good install' defect class PKG-WIN-SCOPE's round 5 review found.",
    )
    outcome = assert_real_install_validated(
        output,
        on_fail_prefix="the syntax-variants fixture installed cleanly",
    )

    real_tool = None
    for candidate in ("pkgconf", "pkg-config"):
        if shutil.which(candidate):
            real_tool = candidate
            break

    if real_tool is not None:
        env = dict(os.environ, PKG_CONFIG_PATH=pkgconfig_dir)
        real_output = run_expect_success(
            [real_tool, "--print-errors", "--cflags", "--libs", "glintfx"],
            f"a REAL {real_tool} on this machine's PATH rejected the same syntax-variants "
            "fixture this validator just accepted - the fixture is not actually "
            "'genuinely good' by an independent implementation.",
            env=env,
        )
        must_contain(
            real_output,
            "-lglintfx",
            on_fail=f"a real {real_tool} run against the syntax-variants fixture did not emit the expected -lglintfx.",
        )
        print(
            f"ok: real pkg-config syntax variants (outcome={outcome}; whitespace around "
            f"'=', trailing comment) - accepted by BOTH this validator and a real "
            f"{real_tool} on this machine's PATH, cross-checked against the same fixture."
        )
    else:
        print(
            f"ok: real pkg-config syntax variants (outcome={outcome}; whitespace around "
            "'=', trailing comment) - accepted by this validator; no real "
            "pkg-config/pkgconf on PATH to additionally cross-check against (declared, "
            "GODS_LAWS.md L-27)."
        )


# --- scenario 14 -------------------------------------------------------


def run_relative_prefix_ordinary_dispatch_scenario(build_dir, dispatch_dir, real_libdir):
    os.makedirs(dispatch_dir, exist_ok=True)
    output = run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", "./stage"],
        "an ORDINARY relative --prefix install ('cmake --install <build> --prefix "
        f"./stage', dispatched from {dispatch_dir}, nothing adversarial) unexpectedly "
        "FAILED - this is the PKG-WIN-SCOPE round 6 regression (CMAKE_INSTALL_PREFIX "
        "read raw, never forced absolute before being appended to, doubling the shared "
        "prefix/libdir/pkgconfig segment into every resolved path).",
        cwd=dispatch_dir,
    )

    expected_pc_file = os.path.join(dispatch_dir, "stage", *real_libdir.split("/"), "pkgconfig", "glintfx.pc")
    outcome = assert_real_install_validated(
        output,
        on_fail_prefix="the relative-prefix ordinary-dispatch install succeeded",
        pc_file=expected_pc_file,
    )
    if not os.path.isfile(expected_pc_file):
        fail(
            f"expected glintfx.pc at {expected_pc_file} after a relative --prefix install, "
            "and it is not there on disk, despite the validator reporting a healthy outcome."
        )
    print(
        f"ok: relative --prefix, ordinary dispatch (outcome={outcome}) - a plain 'cmake "
        "--install <build> --prefix ./stage' install passes, naming the "
        "correctly-resolved, single-occurrence staged glintfx.pc path, with no doubled "
        "prefix/libdir/pkgconfig segment anywhere."
    )


# --- scenario 15 -------------------------------------------------------


def run_duplicate_variable_rejected_scenario(build_dir, scratch, real_libdir):
    validator_script = find_generated_validator_script(build_dir)

    fixture_prefix = os.path.join(scratch, "prefix-duplicate-variable-fixture")
    libdir_path = os.path.join(fixture_prefix, *real_libdir.split("/"))
    pkgconfig_dir = os.path.join(libdir_path, "pkgconfig")
    os.makedirs(pkgconfig_dir, exist_ok=True)
    os.makedirs(os.path.join(fixture_prefix, "include", "glintfx"), exist_ok=True)
    write_placeholder_library_artifact(libdir_path)

    pc_text = (
        "prefix=${pcfiledir}/../..\n"
        "exec_prefix=${prefix}\n"
        "includedir=${prefix}/include\n"
        "libdir=${exec_prefix}/" + real_libdir + "-does-not-exist-yet\n"
        "libdir=${exec_prefix}/" + real_libdir + "\n"
        "\n"
        "Name: glintfx\n"
        "Description: check_pkgconfig_validate.py duplicate-variable-rejected fixture\n"
        "Version: 0.1.0.0\n"
        "Cflags: -I${includedir}\n"
        "Libs: -L${libdir} -lglintfx\n"
    )
    with open(os.path.join(pkgconfig_dir, "glintfx.pc"), "w", encoding="utf-8") as handle:
        handle.write(pc_text)

    output = run_expect_failure(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={fixture_prefix}", "-P", validator_script],
        "re-running the validator against a fixture that defines 'libdir' TWICE "
        "unexpectedly SUCCEEDED - CMake's own native pkg-config parser, under "
        "STRICTNESS STRICT, should refuse an ambiguous file with a duplicate variable "
        "definition rather than silently picking one.",
    )

    must_contain_in_order(
        output,
        "cmake_pkg_config",
        "Resolution failed",
        on_fail="the duplicate-variable RED did not print the expected 'cmake_pkg_config ... Resolution failed' diagnostic.",
    )
    print(
        "ok: duplicate-variable RED - a glintfx.pc defining 'libdir' twice is REFUSED, "
        "closed, by CMake's own native pkg-config parser under STRICTNESS STRICT - a "
        "deliberate narrowing from the deleted hand-written reader's own "
        "last-definition-wins behavior, not a regression."
    )


# --- scenario 16 -------------------------------------------------------


def run_absolute_libdir_scenario(glintfx_src, cxx, build_dir, scratch):
    absolute_libdir = os.path.join(scratch, "absolute-libdir-target", "lib64")
    absolute_includedir = os.path.join(scratch, "absolute-libdir-target", "include")

    cmd = [
        "cmake", "-S", glintfx_src, "-B", build_dir, "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_CXX_COMPILER={cxx}",
    ]
    cmd += extra_c_compiler_flags()
    cmd += [
        "-DGLINTFX_BUILD_TESTS=OFF",
        "-DCMAKE_INSTALL_PREFIX=/usr/local",
        f"-DCMAKE_INSTALL_LIBDIR={absolute_libdir}",
        f"-DCMAKE_INSTALL_INCLUDEDIR={absolute_includedir}",
        "-DGLINTFX_SKIP_PKGCONFIG_VALIDATION=OFF",
    ]
    run_expect_success(cmd, "absolute-libdir configure unexpectedly FAILED:")
    build_glintfx(build_dir)

    output = run_expect_success(
        ["cmake", "--install", build_dir],
        "an absolute CMAKE_INSTALL_LIBDIR/CMAKE_INSTALL_INCLUDEDIR install unexpectedly FAILED.",
    )
    outcome = assert_real_install_validated(
        output, on_fail_prefix="the absolute-libdir install succeeded"
    )

    expected_pc = os.path.join(absolute_libdir, "pkgconfig", "glintfx.pc")
    if not os.path.isfile(expected_pc):
        fail(f"expected glintfx.pc at {expected_pc} after an absolute-libdir install, and it is not there.")
    print(
        f"ok: absolute CMAKE_INSTALL_LIBDIR scenario (outcome={outcome}) - a glintfx.pc "
        "whose 'prefix=' line never references ${pcfiledir} at all still installs and "
        "validates correctly through CMake's native pkg-config parser."
    )


# --- scenario 17 -------------------------------------------------------


def run_destdir_relative_ordinary_dispatch_scenario(build_dir, dispatch_dir, real_libdir):
    os.makedirs(dispatch_dir, exist_ok=True)
    env = dict(os.environ, DESTDIR="relstage")
    output = run_expect_success(
        ["cmake", "--install", build_dir, "--prefix", "/usr/local"],
        f"an ORDINARY relative DESTDIR install ('DESTDIR=relstage cmake --install <build> "
        f"--prefix /usr/local', dispatched from {dispatch_dir}, nothing adversarial) "
        "unexpectedly FAILED - the DESTDIR branch may be reading $ENV{DESTDIR} raw again, "
        "without forcing it absolute first.",
        cwd=dispatch_dir,
        env=env,
    )

    expected_pc_file = os.path.join(
        dispatch_dir, "relstage", "usr", "local", *real_libdir.split("/"), "pkgconfig", "glintfx.pc"
    )
    outcome = assert_real_install_validated(
        output,
        on_fail_prefix="the relative-DESTDIR ordinary-dispatch install succeeded",
        pc_file=expected_pc_file,
    )
    if not os.path.isfile(expected_pc_file):
        fail(
            f"expected glintfx.pc at {expected_pc_file} after a relative-DESTDIR install, "
            "and it is not there on disk, despite the validator reporting a healthy outcome."
        )
    print(
        f"ok: DESTDIR relative, ordinary dispatch (outcome={outcome}) - a plain "
        "'DESTDIR=relstage cmake --install <build> --prefix /usr/local' install passes, "
        "naming the correctly-resolved, single-occurrence staged glintfx.pc path, with no "
        "doubled DESTDIR segment anywhere."
    )


# --- scenario 18 -------------------------------------------------------

# PKG-WIN-SCOPE round 7 (05/09/2026, CI run 33944572704, job "Windows
# (primario)", 3 identical jobs): CMAKE_INSTALL_LIBDIR/pkgconfig for the
# staged (DESTDIR-aware) directory used to concatenate DESTDIR with
# CMAKE_INSTALL_PREFIX after routing the prefix through
# cmake_path(ABSOLUTE_PATH ... BASE_DIRECTORY ...) unconditionally - a
# command whose Windows-only notion of "absolute" requires a root-name
# (drive letter), stricter than CMake's own classic, every-platform
# notion (a leading path separator alone is enough - see this scenario's
# own header, and glintfx_pkgconfig_force_absolute()'s, for the CMake
# documentation citation). A prefix like "/usr" - has a root-directory,
# no root-name - was silently completed by BORROWING base_directory's
# own drive letter, turning "/usr" into "D:/usr"; concatenated as a
# plain string prefix onto a DESTDIR carrying its OWN, different drive
# letter ("C:/Users/.../buildroot"), the result was the impossible
# "C:/Users/.../buildrootD:/usr/lib/pkgconfig" the validator's own
# FATAL_ERROR reported against a genuinely GOOD install.
#
# glintfx_pkgconfig_looks_rooted() (defined in
# cmake/GlintfxPkgConfigValidateInstalled.cmake.in, right above
# glintfx_pkgconfig_force_absolute()) is the fix: a PORTABLE,
# string-only reimplementation of "does this value already look like a
# full path" (leading separator, or drive-letter-plus-separator, or a
# UNC prefix) that classifies "/usr" as already-rooted on EVERY
# platform, never delegating to a native command whose drive-letter
# recognition only exists in a cmake binary actually COMPILED for
# Windows (`#if defined(_WIN32)`, a compile-time switch - MEASURED live
# on this machine: both `if(IS_ABSOLUTE "D:/foo")` and
# `cmake_path(IS_ABSOLUTE "D:/foo")` answer FALSE here, so neither
# native command could ever diverge, or be exercised diverging, on a
# cmake built for Linux, no matter what a script sets `-DWIN32=1` to).
#
# This scenario invokes glintfx_pkgconfig_selftest_looks_rooted()
# (defined in the SAME .cmake.in file, right after
# glintfx_validate_installed_pkgconfig() - never read by a real
# `cmake --install`) via its own dedicated environment variable, the
# same test-only-forced-branch shape this project already uses
# elsewhere (tests/tools/check_dep_zero.py's
# GLINTFX_DEPZERO_SELFTEST_FORCE_WINDOWS_NEEDED_SKIP,
# tests/tools/check_public_name_collision.py's GCC-frontend-forcing
# equivalent) - proving the exact classification this fix rests on,
# with pure string logic that runs identically regardless of which
# platform the calling cmake binary was built for, so it is proven,
# RED then GREEN, on THIS (Linux) machine, not only on a real Windows
# one (GODS_LAWS.md project L-04: "provado em cada [sistema]"). RED/
# GREEN for this exact fixture was proven by hand, mutating the
# function's own regex to require a drive letter unconditionally
# (reproducing the OLD, stricter classification) and confirming the
# selftest fails closed against "/usr" before restoring it - the
# manual mutation-testing step GODS_LAWS.md L-36 requires before a new
# gate counts, recorded here because the mutation itself cannot live in
# the CI-run copy of this file without permanently breaking the fix.
def run_looks_rooted_selftest_scenario(build_dir, intact_prefix):
    validator_script = find_generated_validator_script(build_dir)

    env = dict(os.environ, GLINTFX_PKGVALIDATE_SELFTEST_LOOKS_ROOTED="1")
    output = run_expect_success(
        ["cmake", f"-DCMAKE_INSTALL_PREFIX={intact_prefix}", "-P", validator_script],
        "the looks_rooted selftest (GLINTFX_PKGVALIDATE_SELFTEST_LOOKS_ROOTED=1) "
        "unexpectedly FAILED - glintfx_pkgconfig_looks_rooted() misclassified at least one "
        "of its own fixed test cases (a driveless POSIX-rooted value, a Windows "
        "drive-letter value in either slash direction, a UNC path, or a genuinely relative "
        "token) - see the CMake FATAL_ERROR above for which one and why.",
        env=env,
    )
    must_contain(
        output,
        "looks_rooted selftest OK",
        on_fail=(
            "the looks_rooted selftest ran without a CMake-level FATAL_ERROR but never "
            "printed its own success message - glintfx_pkgconfig_selftest_looks_rooted() "
            "may have silently skipped cases instead of checking all of them "
            "(GODS_LAWS.md L-40)."
        ),
    )
    must_contain(
        output,
        "9/9 inputs classified correctly",
        on_fail=(
            "the looks_rooted selftest's own success message did not report checking all "
            "9 of its declared cases - GODS_LAWS.md L-40: a selftest that silently checks "
            "fewer cases than it declares is not a selftest."
        ),
    )
    # This scenario must NEVER run the real validation path (it forces
    # the selftest branch instead) - a stray "post-install pkg-config"
    # message here would mean the environment-variable gate at the
    # bottom of the .cmake.in file is not actually exclusive.
    must_not_contain(
        output,
        "post-install pkg-config",
        on_fail=(
            "the looks_rooted selftest ALSO ran the real post-install validation - "
            "GLINTFX_PKGVALIDATE_SELFTEST_LOOKS_ROOTED=1 should exclusively select the "
            "selftest branch at the bottom of GlintfxPkgConfigValidateInstalled.cmake.in, "
            "never both."
        ),
    )
    print(
        "ok: looks_rooted selftest (PKG-WIN-SCOPE round 7) - "
        "glintfx_pkgconfig_looks_rooted() classifies a driveless POSIX-rooted value, a "
        "Windows drive-letter value in either slash direction, a UNC path, and a "
        "genuinely relative token all correctly, as portable string logic proven on this "
        "machine regardless of which platform the calling cmake binary was built for."
    )


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 2:
        fail("usage: check_pkgconfig_validate.py <glintfx-source-dir> <cxx-compiler>")
    glintfx_src, cxx = args
    if not os.path.isdir(glintfx_src):
        fail(f"glintfx source dir not found: {glintfx_src}")
    if not cxx:
        fail("cxx-compiler argument is empty")

    scratch = make_scratch_workdir()
    try:
        # PERF-PKGVALIDATE (see the retired .sh's own header for the
        # measured before/after): ONE configure and ONE build for the
        # whole file, reused (reconfigured in place, cheap) by every
        # scenario below except run_absolute_libdir_scenario, the one
        # deliberate exception (a CACHE variable no other scenario ever
        # sets).
        build_dir = os.path.join(scratch, "build-shared")
        reconfigure_glintfx(glintfx_src, build_dir, cxx, "/usr/local", "OFF")
        build_glintfx(build_dir)

        prefix_default = os.path.join(scratch, "prefix-default")
        run_default_layout_scenario(build_dir, prefix_default)
        real_libdir = find_libdir_relative_to_prefix(prefix_default)

        run_destdir_scenario(glintfx_src, cxx, build_dir, scratch)
        run_configure_time_hatch_scenario(glintfx_src, cxx, build_dir, os.path.join(scratch, "prefix-hatch-configure"))
        run_install_time_hatch_scenario(glintfx_src, cxx, build_dir, os.path.join(scratch, "prefix-hatch-install"))
        run_broken_library_scenario(glintfx_src, cxx, build_dir, os.path.join(scratch, "prefix-broken-library"))
        run_missing_pc_file_scenario(glintfx_src, cxx, build_dir, os.path.join(scratch, "prefix-missing-pc"))
        run_empty_flags_floor_scenario(build_dir, scratch, real_libdir)
        run_pkgconfig_absent_scenario(build_dir, scratch, prefix_default)
        run_windows_forced_broken_library_scenario(
            glintfx_src, cxx, build_dir, os.path.join(scratch, "prefix-windows-forced-broken-library")
        )
        run_windows_forced_healthy_conversation_warning_scenario(build_dir, prefix_default)
        run_headers_missing_scenario(glintfx_src, cxx, build_dir, os.path.join(scratch, "prefix-headers-missing"))
        run_relative_libdir_cwd_attack_scenario(
            glintfx_src, cxx, build_dir, os.path.join(scratch, "prefix-relative-cwd-attack"), scratch
        )
        run_real_pkgconfig_syntax_variants_scenario(build_dir, scratch, real_libdir)
        run_duplicate_variable_rejected_scenario(build_dir, scratch, real_libdir)
        run_relative_prefix_ordinary_dispatch_scenario(
            build_dir, os.path.join(scratch, "dispatch-relative-prefix"), real_libdir
        )
        run_destdir_relative_ordinary_dispatch_scenario(
            build_dir, os.path.join(scratch, "dispatch-relative-destdir"), real_libdir
        )
        run_absolute_libdir_scenario(glintfx_src, cxx, os.path.join(scratch, "build-absolute-libdir"), scratch)
        run_looks_rooted_selftest_scenario(build_dir, prefix_default)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    print(
        "ok: the PKG-VALIDATE install(CODE) step runs on real installs (default relative "
        "layout, DESTDIR - both absolute and relative -, an absolute "
        "CMAKE_INSTALL_LIBDIR, and an ordinary relative --prefix), honors both halves of "
        "its escape hatch, fails closed with a self-sufficient diagnostic on a real "
        "broken library artifact, a real missing glintfx.pc, a real missing header tree, "
        "a hand-assembled empty-Cflags/Libs (L-40) fixture, and a relative libdir "
        "resolved from an attacker-controlled working directory - on the real Unix path "
        "AND with the Windows branch forced alike - while degrading to a WARNING (not a "
        "FATAL_ERROR) only when pkg-config itself is absent, or when a real pkg-config "
        "binary genuinely cannot be talked to despite content already confirmed correct; "
        "accepts real pkg-config's own whitespace/comment syntax, cross-checked against a "
        "real pkg-config binary when one is on PATH, while a variable defined TWICE is "
        "now refused, closed, by CMake's own native pkg-config parser under STRICTNESS "
        "STRICT; no longer doubles a shared prefix/libdir/pkgconfig or DESTDIR path "
        "segment when --prefix or DESTDIR is given as a plain relative path; and no "
        "longer borrows a wrong drive letter (PKG-WIN-SCOPE round 7) when a staged, "
        "DESTDIR-aware directory is concatenated with a driveless-but-rooted prefix - "
        "proven with portable, platform-independent string logic (all eighteen "
        "scenarios, unguarded by platform)."
    )


# --- --selftest: pure logic, no cmake/compiler needed ---------------------


def selftest_normalize_wrap_immunity_control():
    """Mirrors check_blank_install_dir_rejected.py's own control of the
    same name: proves normalize_wrapped_message() survives a RANGE of
    reflow widths, not just one incidental value.
    """
    import textwrap

    phrase_a = "post-install pkg-config validation disabled at configure time"
    message = f"glintfx: {phrase_a} (GLINTFX_SKIP_PKGCONFIG_VALIDATION=ON)."
    widths = range(15, 45)

    without_normalization_failures = []
    with_normalization_failures = []
    for width in widths:
        wrapped = textwrap.fill(message, width=width, break_long_words=False, break_on_hyphens=False)
        if phrase_a not in wrapped:
            without_normalization_failures.append(width)
        if phrase_a not in normalize_wrapped_message(wrapped):
            with_normalization_failures.append(width)

    if with_normalization_failures:
        print(
            "selftest: controle de IMUNIDADE AO WRAP FALHOU - normalize_wrapped_message() "
            f"ainda perdeu a frase em {len(with_normalization_failures)} de {len(widths)} "
            f"largura(s): {with_normalization_failures}",
            file=sys.stderr,
        )
        return False
    if not without_normalization_failures:
        print(
            "selftest: controle de IMUNIDADE AO WRAP FALHOU (vazio: nenhuma das "
            f"{len(widths)} larguras quebrou a frase SEM normalizar - o controle nao esta "
            "testando nada, ajuste a faixa de larguras)",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle de IMUNIDADE AO WRAP OK - sem normalizar, "
        f"{len(without_normalization_failures)} de {len(widths)} largura(s) teriam "
        "perdido a frase; normalizando, nenhuma perde"
    )
    return True


def selftest_must_contain_in_order_control():
    ok = True
    try:
        must_contain_in_order("libglintfx.so and libglintfx.a: is not there", "libglintfx.so", "libglintfx.a", "is not there", on_fail="POSITIVO deveria ter passado")
    except SystemExit:
        print("selftest: controle POSITIVO (must_contain_in_order) FALHOU", file=sys.stderr)
        ok = False
    else:
        print("selftest: controle POSITIVO (must_contain_in_order) OK")

    # Out-of-order phrases must NOT satisfy an ordered match, even though
    # both are present somewhere in the text (this is exactly what
    # distinguishes must_contain_in_order from "all phrases present
    # anywhere", and what the retired .sh's own `case *"A"*"B"*` shell
    # glob pattern actually enforces).
    try:
        must_contain_in_order("is not there, libglintfx.a, libglintfx.so", "libglintfx.so", "libglintfx.a", on_fail="NEGATIVO: deveria ter reprovado ordem errada")
    except SystemExit:
        print("selftest: controle NEGATIVO (ordem errada) OK - reprovou frases fora de ordem")
    else:
        print("selftest: controle NEGATIVO (ordem errada) FALHOU - aceitou frases fora de ordem", file=sys.stderr)
        ok = False
    return ok


def selftest_windows_path_separator_control():
    """PKG-WIN-SCOPE round 8 (see to_posix_path()'s own header for the
    real CI run this reproduces): proves, RED then GREEN, on THIS
    (Linux) machine - never a real Windows box, never a real cmake run
    - that a hand-built Windows-style path (literal '\\\\', never an
    os.path.join() call, so this bites identically on every platform
    this script runs on, GODS_LAWS.md project L-04) fails to match
    CMake's own forward-slash message before to_posix_path(), and
    matches after it.
    """
    cmake_style_output = (
        "glintfx: post-install pkg-config validation passed - glintfx.pc at "
        "C:/Users/RUNNER~1/AppData/Local/Temp/glintfx-pkgvalidate-qk0ms6em/"
        "buildroot/usr/lib/pkgconfig/glintfx.pc\n"
    )
    windows_style_pc_file = (
        "C:\\Users\\RUNNER~1\\AppData\\Local\\Temp\\glintfx-pkgvalidate-qk0ms6em\\"
        "buildroot\\usr\\lib\\pkgconfig\\glintfx.pc"
    )

    try:
        must_contain_in_order(
            cmake_style_output,
            "glintfx.pc at",
            windows_style_pc_file,
            on_fail="RED deveria ter reprovado (isto e esperado neste controle)",
        )
    except SystemExit:
        pass
    else:
        print(
            "selftest: controle de SEPARADOR DE CAMINHO FALHOU (RED nao reprovou - o "
            "defeito de separador (PKG-WIN-SCOPE round 8) nao foi reproduzido, o "
            "controle nao esta testando nada)",
            file=sys.stderr,
        )
        return False

    try:
        must_contain_in_order(
            cmake_style_output,
            "glintfx.pc at",
            to_posix_path(windows_style_pc_file),
            on_fail="GREEN deveria ter passado apos to_posix_path()",
        )
    except SystemExit:
        print(
            "selftest: controle de SEPARADOR DE CAMINHO FALHOU (GREEN reprovou apos "
            "to_posix_path() - a normalizacao nao resolveu o defeito)",
            file=sys.stderr,
        )
        return False

    print(
        "selftest: controle de SEPARADOR DE CAMINHO OK (RED: caminho estilo Windows sem "
        "normalizar nao bate contra a mensagem em barra normal do CMake, mesmo nomeando "
        "o mesmo arquivo; GREEN: to_posix_path() resolve, provado nesta maquina Linux)"
    )
    return True


def selftest_library_artifact_patterns_control():
    """L-40's closed-enumeration floor for LIBRARY_ARTIFACT_PATTERNS:
    exactly the three names cmake/GlintfxPkgConfigValidateInstalled.cmake.in's
    own glintfx_pkgconfig_library_glob_patterns() globs, no more, no
    fewer, no duplicate.
    """
    expected = ("libglintfx.so*", "libglintfx.a", "glintfx.lib")
    if len(LIBRARY_ARTIFACT_PATTERNS) != 3:
        print(
            f"selftest: controle da LISTA FECHADA DE ARTEFATOS FALHOU (esperava 3, achou "
            f"{len(LIBRARY_ARTIFACT_PATTERNS)})",
            file=sys.stderr,
        )
        return False
    if len(set(LIBRARY_ARTIFACT_PATTERNS)) != len(LIBRARY_ARTIFACT_PATTERNS):
        print("selftest: controle da LISTA FECHADA DE ARTEFATOS FALHOU (ha padrao duplicado)", file=sys.stderr)
        return False
    if set(LIBRARY_ARTIFACT_PATTERNS) != set(expected):
        print(
            f"selftest: controle da LISTA FECHADA DE ARTEFATOS FALHOU ({LIBRARY_ARTIFACT_PATTERNS!r} != {expected!r})",
            file=sys.stderr,
        )
        return False
    print("selftest: controle da LISTA FECHADA DE ARTEFATOS OK (3 de 3: libglintfx.so*, libglintfx.a, glintfx.lib)")
    return True


def selftest_find_and_remove_library_artifacts_control():
    """Exercises find_library_artifacts()/remove_library_artifacts()
    against a synthetic fixture - pure filesystem, no cmake - proving
    each of the three closed patterns is actually matched, and that an
    empty tree yields an empty (never a crashing) result.
    """
    scratch = tempfile.mkdtemp(prefix="glintfx-pkgvalidate-selftest-", dir=os.environ.get("TMPDIR"))
    ok = True
    try:
        populated = os.path.join(scratch, "populated", "lib64")
        os.makedirs(populated, exist_ok=True)
        expected_names = ("libglintfx.so.0.1.0.0", "libglintfx.a", "glintfx.lib", "unrelated.txt")
        for name in expected_names:
            open(os.path.join(populated, name), "wb").close()

        found = find_library_artifacts(os.path.join(scratch, "populated"))
        found_names = sorted(os.path.basename(p) for p in found)
        if found_names != ["glintfx.lib", "libglintfx.a", "libglintfx.so.0.1.0.0"]:
            print(
                f"selftest: controle de BUSCA DE ARTEFATOS FALHOU (achou {found_names}, "
                "esperava os tres nomes fechados, sem 'unrelated.txt')",
                file=sys.stderr,
            )
            ok = False
        else:
            print("selftest: controle de BUSCA DE ARTEFATOS OK (os tres padroes fechados encontrados, arquivo alheio ignorado)")

        removed = remove_library_artifacts(os.path.join(scratch, "populated"))
        if len(removed) != 3 or find_library_artifacts(os.path.join(scratch, "populated")):
            print("selftest: controle de REMOCAO DE ARTEFATOS FALHOU", file=sys.stderr)
            ok = False
        else:
            print("selftest: controle de REMOCAO DE ARTEFATOS OK (os tres removidos, nada sobrou)")

        empty_root = os.path.join(scratch, "empty")
        os.makedirs(empty_root, exist_ok=True)
        if find_library_artifacts(empty_root) != []:
            print("selftest: controle de VARREDURA VAZIA (artefatos) FALHOU", file=sys.stderr)
            ok = False
        else:
            print("selftest: controle de VARREDURA VAZIA (artefatos) OK (lista vazia, sem excecao)")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    return ok


def selftest_rewrite_libdir_line_control():
    scratch = tempfile.mkdtemp(prefix="glintfx-pkgvalidate-selftest-", dir=os.environ.get("TMPDIR"))
    ok = True
    try:
        pc_file = os.path.join(scratch, "glintfx.pc")
        with open(pc_file, "w", encoding="utf-8") as handle:
            handle.write("prefix=/usr\nlibdir=${prefix}/lib64\nName: glintfx\n")
        rewrite_libdir_line(pc_file, "decoy-relative-libdir")
        with open(pc_file, "r", encoding="utf-8") as handle:
            text = handle.read()
        if "libdir=decoy-relative-libdir\n" not in text or "prefix=/usr\n" not in text:
            print(f"selftest: controle de REESCRITA DE libdir FALHOU. Conteudo:\n{text}", file=sys.stderr)
            ok = False
        else:
            print("selftest: controle de REESCRITA DE libdir OK (so a linha libdir= mudou)")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    return ok


def selftest_main():
    controls = [
        selftest_normalize_wrap_immunity_control(),
        selftest_must_contain_in_order_control(),
        selftest_windows_path_separator_control(),
        selftest_library_artifact_patterns_control(),
        selftest_find_and_remove_library_artifacts_control(),
        selftest_rewrite_libdir_line_control(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        if len(args) != 1:
            fail("usage: check_pkgconfig_validate.py --selftest")
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
