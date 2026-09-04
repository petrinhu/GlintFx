#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_spdx.py - CI gate for repo-wide "SPDX-License-Identifier"
# header coverage (GODS_LAWS.md L-08: publico no GitHub, AGPL-3.0).
#
# PORT of the former tests/tools/check_spdx.sh (POSIX sh + a hand-
# written awk engine), retired in the same fatia that wrote this file
# (ORDEM DE SERVICO, GODS_LAWS.md L-04, decisao do lider de 02/09/2026:
# "O comportamento deve ser igual em qualquer OS" - the sh version was
# registered ONLY in the ubuntu-only "leis" job of .github/workflows/
# ci.yml, never on the four other platforms, and Windows has no POSIX
# sh at all). This file preserves every classification rule the sh
# version enforced and is registered, unguarded, as an ordinary ctest
# case (tests/CMakeLists.txt), so it runs inside the SAME ctest
# invocation the linux and windows CI jobs already execute on all five
# platforms - the same shape check_dep_zero_trace.py (python3, no
# if(UNIX) guard) already proved works.
#
# GATE-SPDX-UNTRACKED, fixed here (was NOT present in the sh version):
# the old engine enumerated only `git ls-files` - a file created and
# never `git add`ed was invisible to it, so a brand-new source file
# with no SPDX header passed this gate cleanly, right up until the
# moment it was staged. Fixed by enumerating BOTH `git ls-files` (
# tracked) and `git ls-files --others --exclude-standard` (untracked,
# not gitignored) and judging the union - see scan_repository() below.
# Proven by --selftest's own selftest_untracked_header_missing_control:
# planting an untracked file without a header and requiring the gate
# to cite it and reprove.
#
# ENUMERATION VIA `git ls-files -z`, not text-quoting/decoding
# ----------------------------------------------------------------
# The sh version had to carry its own git-quote decoder (octal escape
# parsing, core.quotepath handling) because plain `git ls-files`
# quotes any path containing a byte >= 0x80 or a control character
# (tab/newline/quote/backslash) as a C-style double-quoted string -
# see git-config(1), core.quotepath. `git ls-files -z` sidesteps this
# entirely: paths are NUL-terminated and never quoted, which is
# git's own documented mechanism for scripts (git-ls-files(1), "-z").
# This is not a behavior change from the sh version's INTENT (both
# aim to recognize every tracked/untracked path exactly, accented or
# hostile-named alike) - it is a simpler, more direct way to reach the
# same result, and --selftest still proves the accented- and hostile-
# filename cases the sh version's own history had to fix twice.
#
# EXEMPTIONS - identical to the sh version, six branches plus the
# named third_party/khronos/ enumeration (GODS_LAWS.md L-40 item 5:
# a small, enumerable exception space is enumerated whole, never
# matched by a directory-wide pattern):
#
#   *.md                    - documentation, not code or build.
#   LICENSE                  - the license text itself; an SPDX header
#                              inside it makes no sense.
#   .gitignore                - declarative configuration, not
#                              code/build.
#   .bigtech-porte              - one-line marker, no logic, no
#                              copyrightable authorial content.
#   .claude/**                  - Claude Code's own configuration
#                              (hooks, settings), not glintfx code/build.
#   *.json                      - RFC 8259 JSON accepts no comment
#                              syntax; there is no header form that
#                              would not break the file. The one
#                              real-world case today, .claude/
#                              settings.json, is already covered by
#                              the .claude/ branch above - kept as its
#                              own rule because a *.json outside
#                              .claude/ would hit this same technical
#                              reason, not because it is Claude Code's.
#   third_party/khronos/<named file> - third-party vendored file under
#                              GODS_LAWS.md L-07 EXCECAO No 1. See
#                              KNOWN_KHRONOS_VENDOR_FILES below for why
#                              this list is DUPLICATED, not imported,
#                              used to live in tests/tools/khronos_vendor_files.sh, deleted on 04/09/2026 when check_vendor_purity was ported to Python; this constant is the single source now.
#
# Each exemption is matched on the EXACT path relative to the repo
# root, never by substring - "README.md" does not hide "README.md.bak",
# and "third_party/khronos-fake/x" is not "third_party/khronos/x" just
# because it starts similarly (proven in --selftest, same discipline
# the sh version's own selftest already carried).
#
# Usage:
#   check_spdx.py <repo-root-directory>
#   check_spdx.py --selftest
#
# --selftest builds disposable trees under a real, throwaway git
# repository (mktemp-style tempfile.mkdtemp(), `git init` + `git add`),
# the same shape the sh version's own --selftest used - the enumeration
# under test really is `git ls-files`/`git ls-files --others`, so the
# fixture has to be a real repo. Runs the three controls GODS_LAWS.md
# L-40 requires of every gate (positive, negative, empty-scan floor)
# plus this gate's own additional controls: not-a-repo, the
# third_party/khronos/ isolation, accented and hostile filenames, and
# the untracked-file fix (GATE-SPDX-UNTRACKED).
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_spdx.py"
REQUIRED_HEADER = b"SPDX-License-Identifier: AGPL-3.0-or-later"

# SINGLE SOURCE since 04/09/2026. It used to be duplicated from khronos_vendor_files.sh's own
# known_khronos_vendor_files(), on purpose, declared here rather than
# silently forked (GODS_LAWS.md L-27: fact separated from inference).
# That shell file is still the single source of truth for its OTHER
# consumer, check_vendor_purity.py (ported to Python on 04/09/2026, which
# fatia, still if(UNIX)-guarded in tests/CMakeLists.txt) - but THIS
# gate now has to run on Windows too (GODS_LAWS.md L-04), and shelling
# out to `sh` to source a POSIX library is exactly the kind of
# platform-conditional mechanism this whole port exists to remove.
# A three-entry, closed, rarely-changing enumeration (GODS_LAWS.md
# L-07 EXCECAO No 1, changeable only by the leader's own decision) is
# cheap to keep in sync by hand, and cheaper than reintroducing a
# shell dependency for one list. If this list ever changes, update
# this constant, now that khronos_vendor_files.sh's
# known_khronos_vendor_files() - each file's header now cross-
# references the other for exactly this reason.
KNOWN_KHRONOS_VENDOR_FILES = frozenset({
    "third_party/khronos/gl.xml",
    "third_party/khronos/LICENSE-APACHE-2.0.txt",
    "third_party/khronos/README.md",
})


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- classification, closed by construction (GODS_LAWS.md L-40 item 5) ---


def is_exempt(path):
    if path.endswith(".md"):
        return True
    if path == "LICENSE":
        return True
    if path == ".gitignore":
        return True
    if path == ".bigtech-porte":
        return True
    if path.startswith(".claude/"):
        return True
    if path.endswith(".json"):
        return True
    if path.startswith("third_party/khronos/"):
        return path in KNOWN_KHRONOS_VENDOR_FILES
    return False


# --- enumeration -----------------------------------------------------------


def git_ls_files_z(root, extra_args):
    """Runs `git -C root ls-files -z <extra_args>`, decoded via the
    filesystem encoding with surrogateescape (the same tolerant,
    lossless round-trip os.fsdecode() uses for raw OS path bytes) -
    never crashes on a byte sequence that is not valid UTF-8.

    Returns (paths, ok). ok is False when git itself failed to run
    (not a git repository, or git missing) - the caller turns that
    into an explicit, named failure, never a silent empty result.
    """
    try:
        result = subprocess.run(
            ["git", "-C", root, "ls-files", "-z", *extra_args],
            capture_output=True,
        )
    except FileNotFoundError:
        return [], False
    if result.returncode != 0:
        return [], False
    raw = result.stdout
    if not raw:
        return [], True
    if raw.endswith(b"\0"):
        raw = raw[:-1]
    encoding = sys.getfilesystemencoding()
    paths = [chunk.decode(encoding, errors="surrogateescape") for chunk in raw.split(b"\0")]
    return paths, True


def scanned_files(root):
    """The closed-by-construction universe: every path `git ls-files`
    (tracked) and `git ls-files --others --exclude-standard`
    (untracked, not gitignored) report - GATE-SPDX-UNTRACKED's fix.
    Returns (tracked, untracked, ok).
    """
    tracked, ok = git_ls_files_z(root, [])
    if not ok:
        return [], [], False
    untracked, ok = git_ls_files_z(root, ["--others", "--exclude-standard"])
    if not ok:
        return [], [], False
    return tracked, untracked, True


def file_has_header(root, path):
    """Reads up to the first 3 lines of root/path (binary mode - a
    file's byte content is judged, never assumed to be valid text in
    any particular encoding) looking for REQUIRED_HEADER.

    Returns True (found), False (opened, header missing in the first
    3 lines), or None (open/read failed - fail-closed: a file the
    engine could not open is never silently "missing header", it is
    its own, separately reported reason to reprove, exactly the
    GODS_LAWS.md L-40 shape the sh version's engine already used).
    """
    file_path = os.path.join(root, *path.split("/"))
    try:
        with open(file_path, "rb") as handle:
            for _ in range(3):
                line = handle.readline()
                if not line:
                    break
                if REQUIRED_HEADER in line:
                    return True
    except OSError:
        return None
    return False


# --- checking ----------------------------------------------------------


def check_spdx(root):
    tracked, untracked, ok = scanned_files(root)
    if not ok:
        print(
            f"{SCRIPT_NAME}: 'git ls-files' falhou em '{root}' (nao e "
            "repositorio git, ou git indisponivel) - varredura recusada, "
            "nunca presumida vazia",
            file=sys.stderr,
        )
        return False

    total_count = len(tracked) + len(untracked)
    if total_count == 0:
        print(f"{SCRIPT_NAME}: varredura vazia (0 arquivos rastreados ou nao rastreados)", file=sys.stderr)
        return False

    entries = [(p, False) for p in tracked] + [(p, True) for p in untracked]

    required_count = 0
    exempt_count = 0
    analyzed_count = 0
    failed = []
    missing = []

    for path, is_untracked in entries:
        if is_exempt(path):
            exempt_count += 1
            continue
        required_count += 1
        result = file_has_header(root, path)
        if result is None:
            failed.append(path)
            continue
        analyzed_count += 1
        if not result:
            suffix = " (nao rastreado)" if is_untracked else ""
            missing.append(f"{root}/{path}{suffix}")

    if failed:
        print(
            f"{SCRIPT_NAME}: varredura incompleta - {len(failed)} "
            f"arquivo(s) recusaram abrir, {analyzed_count}/{required_count} "
            "analisados (GODS_LAWS.md L-40 fail-closed):",
            file=sys.stderr,
        )
        for path in failed:
            print(f"{SCRIPT_NAME}: {root}/{path}: open refused", file=sys.stderr)
        if missing:
            print(
                f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-08, publico sob "
                f"AGPL-3.0): ainda faltando '{REQUIRED_HEADER.decode()}' "
                "nas 3 primeiras linhas em:",
                file=sys.stderr,
            )
            for line in missing:
                print(line, file=sys.stderr)
        return False

    if missing:
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-08, publico sob "
            f"AGPL-3.0): {len(missing)} arquivo(s) sem "
            f"'{REQUIRED_HEADER.decode()}' nas 3 primeiras linhas:",
            file=sys.stderr,
        )
        for line in missing:
            print(line, file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: 0 arquivo(s) sem cabecalho ({required_count} "
        f"exige(m), {exempt_count} isento(s), {len(tracked)} rastreado(s), "
        f"{len(untracked)} nao rastreado(s), {total_count} no total)"
    )
    return True


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_spdx.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_spdx(root):
        fail("cabecalho SPDX ausente em arquivo nao isento (ver mensagem acima)")


# --- selftest fixtures and controls -----------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-spdx-selftest-")


def init_fixture_repo(root):
    os.makedirs(root, exist_ok=True)
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.email", "selftest@check-spdx.invalid"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.name", "check_spdx selftest"], check=True)


def track_all(root):
    subprocess.run(["git", "-C", root, "add", "-A"], check=True)


def write_file(root, relative_path, content):
    full_path = os.path.join(root, *relative_path.split("/"))
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, "w", encoding="utf-8") as handle:
        handle.write(content)


# Same six is_exempt() branches as the sh version's own fixture, one
# file per branch plus one WITH a header - proves none of the six is
# falsely charged.
def make_positive_fixture(root):
    write_file(root, "src/foo.cpp", "// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n")
    write_file(root, "README.md", "# doc sem cabecalho\n")
    write_file(root, "LICENSE", "texto legal, sem cabecalho\n")
    write_file(root, ".gitignore", "build/\n")
    write_file(root, ".bigtech-porte", "porte=bigtech\n")
    write_file(root, ".claude/settings.json", '{\n  "chave": "valor"\n}\n')
    write_file(root, ".claude/hooks/foo.sh", "#!/bin/sh\necho hook, sem cabecalho\n")
    write_file(root, "config.json", '{\n  "outra": true\n}\n')
    track_all(root)


def selftest_positive_control(scratch):
    root = os.path.join(scratch, "positive")
    init_fixture_repo(root)
    make_positive_fixture(root)
    if check_spdx(root):
        print("selftest: controle POSITIVO OK (arquivo com cabecalho passa, seis ramos de excecao sem cabecalho nao sao cobrados)")
        return True
    print("selftest: controle POSITIVO FALHOU", file=sys.stderr)
    return False


def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    init_fixture_repo(root)
    make_positive_fixture(root)
    write_file(root, "src/bar.cpp", "int g();\n")
    track_all(root)

    output = capture(lambda: check_spdx(root))
    if output.result:
        print("selftest: controle NEGATIVO FALHOU (src/bar.cpp sem cabecalho deveria ter sido reprovado)", file=sys.stderr)
        return False

    ok = True
    if "src/bar.cpp" not in output.text:
        print("selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou src/bar.cpp)", file=sys.stderr)
        ok = False
    for isento in ("README.md", "LICENSE", ".gitignore", ".bigtech-porte", ".claude/settings.json", ".claude/hooks/foo.sh", "config.json"):
        if isento in output.text:
            print(f"selftest: controle NEGATIVO FALHOU (cobrou arquivo isento '{isento}' - excecao vazou)", file=sys.stderr)
            ok = False
    if ok:
        print("selftest: controle NEGATIVO OK (src/bar.cpp citado, nenhum arquivo isento cobrado)")
    return ok


def make_third_party_khronos_fixture(root):
    write_file(root, "src/foo.cpp", "// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n")
    write_file(root, "third_party/khronos/gl.xml", "<comment>vendored verbatim, no header on purpose</comment>\n")
    write_file(root, "third_party/khronos/LICENSE-APACHE-2.0.txt", "Apache License 2.0 full text, no SPDX header on purpose\n")
    write_file(root, "third_party/khronos/mystery.cpp", "int surprise();\n")
    write_file(root, "third_party/other_vendor/vendor.dat", "not the exempt directory\n")
    write_file(root, "third_party/khronos-fake/vendor.dat", "looks like the exempt dir, is not\n")
    track_all(root)


def selftest_third_party_khronos_control(scratch, capture):
    root = os.path.join(scratch, "third_party_khronos")
    init_fixture_repo(root)
    make_third_party_khronos_fixture(root)

    output = capture(lambda: check_spdx(root))
    if output.result:
        print("selftest: controle third_party/khronos FALHOU (deveria ter reprovado)", file=sys.stderr)
        return False

    ok = True
    for exigido in ("third_party/khronos/mystery.cpp", "third_party/other_vendor/vendor.dat", "third_party/khronos-fake/vendor.dat"):
        if exigido not in output.text:
            print(f"selftest: controle third_party/khronos FALHOU (nao citou '{exigido}')", file=sys.stderr)
            ok = False
    for isento in ("third_party/khronos/gl.xml", "third_party/khronos/LICENSE-APACHE-2.0.txt"):
        if isento in output.text:
            print(f"selftest: controle third_party/khronos FALHOU (cobrou '{isento}', que a excecao isenta)", file=sys.stderr)
            ok = False
    if ok:
        print("selftest: controle third_party/khronos OK (os dois arquivos vendorizados nomeados passam sem cabecalho; arquivo desconhecido na mesma pasta, pasta irma e pasta com nome parecido continuam exigindo)")
    return ok


def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    init_fixture_repo(root)

    output = capture(lambda: check_spdx(root))
    if output.result:
        print("selftest: controle de VARREDURA VAZIA FALHOU (repo git sem arquivo rastreado deveria ter sido recusado)", file=sys.stderr)
        return False
    if "varredura vazia" not in output.text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (repo git sem arquivo rastreado nem nao-rastreado recusado)")
    return True


def selftest_not_a_repo_control(scratch, capture):
    root = os.path.join(scratch, "not_a_repo")
    write_file(root, "src/baz.cpp", "int h();\n")

    output = capture(lambda: check_spdx(root))
    if output.result:
        print("selftest: controle de NAO-E-REPO FALHOU (diretorio sem .git deveria ter sido recusado)", file=sys.stderr)
        return False
    if "nao e" not in output.text or "repositorio git" not in output.text:
        print("selftest: controle de NAO-E-REPO FALHOU (recusou, mas nao disse o motivo)", file=sys.stderr)
        return False
    print("selftest: controle de NAO-E-REPO OK (diretorio sem .git recusado, nao presumido vazio)")
    return True


def selftest_accented_filename_control(scratch, capture):
    root = os.path.join(scratch, "accented")
    init_fixture_repo(root)
    write_file(root, "src/Waylând.cpp", "// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n")
    track_all(root)

    output = capture(lambda: check_spdx(root))
    if not output.result:
        print("selftest: controle ACCENTED-FILENAME FALHOU (src/Waylând.cpp tem o cabecalho e deveria ter passado)", file=sys.stderr)
        return False
    print("selftest: controle ACCENTED-FILENAME OK (arquivo com nome acentuado e cabecalho presente reconhecido via git ls-files -z)")
    return True


def hostile_filename_case(name):
    return {
        "lf": "src/Way\nland_hostile.cpp",
        "dquote": 'src/Way"land_hostile.cpp',
        "backslash": "src/Way\\land_hostile.cpp",
    }[name]


HOSTILE_FILENAME_CASE_NAMES = ("lf", "dquote", "backslash")

# Human-readable reason each case is inviable on Windows (NTFS/Win32),
# printed verbatim in the "declarado NAO APLICAVEL" line so the reader
# never has to reconstruct WHY from the case name alone.
HOSTILE_FILENAME_CASE_DESCRIPTIONS = {
    "lf": "quebra de linha (caractere de controle, codepoint < 0x20) no nome do arquivo",
    "dquote": 'aspas duplas (") no nome do arquivo - caractere reservado do Win32',
    "backslash": "barra invertida (\\) no nome do arquivo - e o proprio separador de caminho no Windows, nunca um byte literal de nome",
}


# GATE-SPDX-WINFS, added by ORDEM DE SERVICO 02/09/2026 (Windows CI run
# 33670074103, job 100381198522): write_file() planting "Way<LF>land_
# hostile.cpp" crashed with OSError: [Errno 22] Invalid argument on
# Windows - NTFS forbids a control character in a filename outright.
# spdx_test itself (the real gate) had already passed on Windows; only
# this selftest's own FIXTURE PLANTING failed, before the gate under
# test was ever exercised. GODS_LAWS.md L-04 (paridade) and L-40 (piso
# de varredura nao-vazia) forbid silently dropping the case: instead it
# is DECLARED not-applicable, at the moment it would have been planted,
# with the reason named - and the expected count is DERIVED from what
# was actually exercised, never hand-written back to a smaller number.
def windows_forbidden_char(ch):
    """True if `ch` cannot appear in a Windows (NTFS/Win32) filename
    component at all: either it is one of the nine reserved characters
    (< > : " / \\ | ? *) or a control character (codepoint < 0x20,
    which includes LF). Backslash and forward slash are path
    separators, not literal name bytes, on every OS this project
    targets; the other seven are reserved by the Win32 CreateFile API
    regardless of separator conventions.
    """
    if ord(ch) < 0x20:
        return True
    return ch in '<>:"/\\|?*'


def name_creatable_on_windows(path):
    """Applies windows_forbidden_char() to every character of every "/"-
    separated component of `path` (the "/" itself is check_spdx.py's
    own path-joining convention, not part of any component's literal
    name, so it is never checked)."""
    return all(not windows_forbidden_char(ch) for component in path.split("/") for ch in component)


def running_as_windows_filesystem(force_windows_hostile_skip=False):
    """True when the current process is on a filesystem that enforces
    Windows' reserved-character rule: real Windows (os.name == "nt"),
    or the GLINTFX_SPDX_SELFTEST_FORCE_WINDOWS_HOSTILE_SKIP=1 escape
    hatch this fatia adds so the EXACT same declare-and-derive code
    path Windows CI exercises can be proven, and shown, green on Linux
    too (GODS_LAWS.md L-40: a mechanism that only ever ran on the one
    platform that never triggers it is not a proven mechanism).
    """
    if force_windows_hostile_skip:
        return True
    return os.name == "nt"


def _run_hostile_filename_cases(scratch, capture, prefix, header, control_name, verdict):
    """Shared loop for the positive and negative hostile-filename
    controls: for each of HOSTILE_FILENAME_CASE_NAMES, either exercise
    it (plant + run check_spdx() + apply `verdict`) or, when this
    platform's filesystem cannot even hold the name, declare it not
    applicable and skip planting it - never attempt the write that
    would crash. Returns (status, exercised_names, skipped_names).
    """
    force_windows_hostile_skip = os.environ.get("GLINTFX_SPDX_SELFTEST_FORCE_WINDOWS_HOSTILE_SKIP") == "1"
    status = True
    exercised = []
    skipped = []
    for case_name in HOSTILE_FILENAME_CASE_NAMES:
        relative_path = hostile_filename_case(case_name)
        if not name_creatable_on_windows(relative_path) and running_as_windows_filesystem(force_windows_hostile_skip):
            skipped.append(case_name)
            print(
                f"selftest: {control_name}({case_name}) declarado NAO APLICAVEL "
                f"(o sistema de arquivos do Windows proibe {HOSTILE_FILENAME_CASE_DESCRIPTIONS[case_name]}, "
                "entao este controle nao pode ser exercido aqui)"
            )
            continue
        exercised.append(case_name)
        root = os.path.join(scratch, f"{prefix}-{case_name}")
        init_fixture_repo(root)
        write_file(root, relative_path, header)
        track_all(root)
        output = capture(lambda: check_spdx(root))
        if not verdict(case_name, output):
            status = False
    return status, exercised, skipped


def selftest_hostile_filename_positive(scratch, capture):
    def verdict(case_name, output):
        if output.result:
            return True
        print(f"selftest: HOSTILE-FILENAME-POSITIVE({case_name}) FALHOU (arquivo com cabecalho deveria ter passado)", file=sys.stderr)
        return False

    status, exercised, skipped = _run_hostile_filename_cases(
        scratch, capture, "hostile-positive",
        "// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n",
        "HOSTILE-FILENAME-POSITIVE", verdict,
    )
    if len(exercised) + len(skipped) != len(HOSTILE_FILENAME_CASE_NAMES):
        print("selftest: HOSTILE-FILENAME-POSITIVE FALHOU (varredura de casos incompleta - nem todo caso foi exercido ou declarado)", file=sys.stderr)
        return False
    if status:
        print(
            f"selftest: HOSTILE-FILENAME-POSITIVE OK ({len(exercised)}/{len(HOSTILE_FILENAME_CASE_NAMES)} "
            f"caso(s) exercido(s) aqui - {', '.join(exercised)}; {len(skipped)} declarado(s) nao aplicavel "
            f"nesta plataforma - {', '.join(skipped) if skipped else 'nenhum'})"
        )
    return status


def selftest_hostile_filename_negative(scratch, capture):
    def verdict(case_name, output):
        if output.result:
            print(f"selftest: HOSTILE-FILENAME-NEGATIVE({case_name}) FALHOU (arquivo sem cabecalho deveria ter reprovado)", file=sys.stderr)
            return False
        if "land_hostile.cpp" not in output.text:
            print(f"selftest: HOSTILE-FILENAME-NEGATIVE({case_name}) FALHOU (reprovou, mas nao citou o arquivo)", file=sys.stderr)
            return False
        return True

    status, exercised, skipped = _run_hostile_filename_cases(
        scratch, capture, "hostile-negative",
        "int f();\n",
        "HOSTILE-FILENAME-NEGATIVE", verdict,
    )
    if len(exercised) + len(skipped) != len(HOSTILE_FILENAME_CASE_NAMES):
        print("selftest: HOSTILE-FILENAME-NEGATIVE FALHOU (varredura de casos incompleta - nem todo caso foi exercido ou declarado)", file=sys.stderr)
        return False
    if status:
        print(
            f"selftest: HOSTILE-FILENAME-NEGATIVE OK ({len(exercised)}/{len(HOSTILE_FILENAME_CASE_NAMES)} "
            f"caso(s) exercido(s) aqui - {', '.join(exercised)}; {len(skipped)} declarado(s) nao aplicavel "
            f"nesta plataforma - {', '.join(skipped) if skipped else 'nenhum'})"
        )
    return status


# GATE-SPDX-UNTRACKED: the fix this fatia exists to add. A file that
# was never `git add`ed and carries no header must still reprove -
# planted and proven here the same way the rest of this file proves
# every other rule, not just claimed in a comment.
def selftest_untracked_header_missing_control(scratch, capture):
    root = os.path.join(scratch, "untracked-missing")
    init_fixture_repo(root)
    write_file(root, "src/tracked.cpp", "// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n")
    track_all(root)
    write_file(root, "src/never_added.cpp", "int g();\n")  # deliberately NOT git add'ed

    output = capture(lambda: check_spdx(root))
    if output.result:
        print("selftest: controle UNTRACKED-HEADER-MISSING FALHOU (GATE-SPDX-UNTRACKED: arquivo nao rastreado sem cabecalho deveria ter reprovado)", file=sys.stderr)
        return False
    if "src/never_added.cpp" not in output.text:
        print("selftest: controle UNTRACKED-HEADER-MISSING FALHOU (reprovou, mas nao citou src/never_added.cpp)", file=sys.stderr)
        return False
    print("selftest: controle UNTRACKED-HEADER-MISSING OK (GATE-SPDX-UNTRACKED: arquivo nunca 'git add'ado, sem cabecalho, citado e reprovado)")
    return True


# The mirror positive control: an untracked file WITH a header must
# still pass - the fix must not turn "not yet staged" into an
# automatic failure on its own.
def selftest_untracked_header_present_control(scratch, capture):
    root = os.path.join(scratch, "untracked-present")
    init_fixture_repo(root)
    write_file(root, "src/tracked.cpp", "// SPDX-License-Identifier: AGPL-3.0-or-later\nint f();\n")
    track_all(root)
    write_file(root, "src/never_added_but_headered.cpp", "// SPDX-License-Identifier: AGPL-3.0-or-later\nint g();\n")

    output = capture(lambda: check_spdx(root))
    if not output.result:
        print("selftest: controle UNTRACKED-HEADER-PRESENT FALHOU (arquivo nao rastreado, mas com cabecalho, deveria ter passado)", file=sys.stderr)
        return False
    print("selftest: controle UNTRACKED-HEADER-PRESENT OK (arquivo nao rastreado com cabecalho reconhecido, nao penalizado so por nao estar staged)")
    return True


class _Captured:
    __slots__ = ("result", "text")

    def __init__(self, result, text):
        self.result = result
        self.text = text


def _make_capture():
    """Returns a `capture(fn)` helper that runs fn() (a check_spdx()
    call) with stderr AND stdout redirected to an in-memory buffer,
    and hands back both the boolean result and everything printed -
    the same "output" the sh version's `output="$(... 2>&1)"` pattern
    captured.
    """
    import contextlib
    import io

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch),
            selftest_negative_control(scratch, capture),
            selftest_third_party_khronos_control(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_not_a_repo_control(scratch, capture),
            selftest_accented_filename_control(scratch, capture),
            selftest_hostile_filename_positive(scratch, capture),
            selftest_hostile_filename_negative(scratch, capture),
            selftest_untracked_header_missing_control(scratch, capture),
            selftest_untracked_header_present_control(scratch, capture),
        ]
        if not all(controls):
            print("check_spdx.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_spdx.py --selftest: os {len(controls)} controles OK")
    finally:
        import shutil

        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
