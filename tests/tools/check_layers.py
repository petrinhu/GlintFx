#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_layers.py - CI gate for GODS_LAWS.md L-19 ("a CI gate reproves
# the violation" instead of trusting the discipline of whoever writes
# the code).
#
# PORT of the former tests/tools/check_layers.sh (POSIX sh), retired
# in the same fatia that wrote this file (GATE-TREE-PARITY, GODS_LAWS.md
# L-04, decisao do lider: "O comportamento deve ser igual em qualquer
# OS") - the sh version was if(UNIX)-guarded in tests/CMakeLists.txt,
# so nothing checked layer discipline on the Windows CI job at all).
# Registered here, unguarded, as an ordinary ctest case - the same
# shape check_spdx.py and check_hygiene_coverage.py already proved
# works on all five platforms.
#
# Verifies that the pure layers (src/core/, include/glintfx/core/,
# src/gfss/, include/glintfx/gfss/, src/gfui/, include/glintfx/gfui/)
# do not include (a) a header from a layer above (glintfx/platform/),
# nor (b) an operating system header. None of these layers knows
# anything about the OS - only platform/ does (GODS_LAWS.md L-19), and
# render/ is a documented, deliberate exception of its own (GODS_LAWS.md
# L-31, L-07 EXCECAO No 1: the GL loader), so this gate does not scan it.
#
# LAYERS-GATE-GFSS-GFUI (TODO.md, GODS_LAWS.md L-19/L-40/L-67): gfss/
# and gfui/ used to be a KNOWN, DOCUMENTED gap - src/gfss/CMakeLists.txt
# and src/gfui/CMakeLists.txt's own header comments said so in prose,
# while a system header planted in either directory went unseen by
# this gate. This fatia closes the gap: both layers are enumerated
# below, with their OWN per-directory non-empty floor (see
# GFSS_GFUI_DIR_SPECS's own comment for why aggregate is not enough).
# The two CMakeLists.txt comments that documented the gap are deleted
# in the same commit (GODS_LAWS.md L-67: what stops being true is
# removed, not archived).
#
# ANCHOR-ON-DIRECTIVE (achado real, 22/09/2026, ordem do lider por
# AskUserQuestion: "Olhar so diretivas de inclusao"): alargar o
# escopo acima descobriu que a agulha era procurada em QUALQUER linha,
# comentario incluido - src/gfss/anb_parse.cpp:159's own prose ("a
# GL/WGL function name is") casava com a agulha "GL/" sem nunca ter
# sido um #include. A mesma fragilidade sempre existiu em src/core/,
# so nunca se manifestou porque nenhum comentario de core/ citava uma
# dessas substrings fora de contexto de include. O lider decidiu
# corrigir a semantica do portao para TODAS as camadas que ele varre,
# core/ inclusive: a agulha so e procurada dentro de uma linha que SEJA
# uma diretiva de inclusao - #include (com ou sem espaco depois do
# '#', com ou sem espaco antes do '<'/'"') ou, por este projeto ser
# C++23, a forma de unidade de cabecalho `import <...>;`/`import
# "...";` (com ou sem `export` na frente). Ancorar so em #include
# teria trocado o falso positivo por um falso NEGATIVO pior - um
# `import <fstream>;` real passaria em silencio -, entao as duas
# formas sao reconhecidas. Ver _directive_argument() e
# _ANCHOR_DIRECTIVE_CASES abaixo.
#
# PHASE-2-3 (re-verificacao do lider, 22/09/2026, apos o ANCHOR-ON-
# DIRECTIVE acima): a ancora por LINHA FISICA sozinha trocou o falso
# positivo original por DOIS falsos negativos, medidos contra
# src/core/ - `#include \` + quebra + `<fstream>` e `#include /* nada
# */ <fstream>` (a diretiva partida entre duas linhas fisicas, ou com
# um comentario no meio) nao eram mais vistos. A causa e que o
# pre-processador real nao le linha fisica: a fase 2 da traducao
# emenda barra-invertida-e-nova-linha ANTES de qualquer diretiva ser
# reconhecida, e a fase 3 troca cada comentario por um espaco,
# respeitando literais de cadeia e de caractere. _splice_lines() e
# _strip_comments_and_literals() modelam as duas fases; _directive_
# lines() as encadeia e so entao aplica _directive_argument() por
# linha LOGICA resultante. Ver o comentario PHASE-2-3 mais abaixo, no
# topo dessas tres funcoes, para o detalhe e os cinco casos
# confirmados contra `g++ -std=c++23 -fsyntax-only`.
#
# Usage:
#   check_layers.py <source-root-directory>
#   check_layers.py --selftest
#
# --selftest runs the four original GODS_LAWS.md L-40 controls (positive,
# negative, a SECOND negative specific to the file-I/O headers added by
# the ASSET-LOAD conserto of 28/08/2026, empty-scan) PLUS the four
# controls LAYERS-GATE-GFSS-GFUI adds (a violation control and a
# per-directory floor control, each looped over the four gfss/gfui
# directories; the ANCHOR-ON-DIRECTIVE control table; and the
# PHASE-2-3 control table) against disposable fixtures under a
# scratch directory,
# never against the real tracked tree.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_layers.py"

# Layer above the core (FUND-2's own note: not created yet at the time
# the sh version was written, but the pattern stays ready for when it
# is born).
UPPER_LAYER_NEEDLE = "glintfx/platform/"

# OS headers covered by this gate: Wayland, Win32, GL/EGL, the most
# common low-level POSIX calls, and (ASSET-LOAD conserto, 28/08/2026,
# GODS_LAWS.md L-19/L-40) file I/O - <filesystem> and <fstream>. Ported
# verbatim from OS_HEADER_PATTERN in the sh version - see that file's
# own history (its own --selftest's selftest_negative_control_file_
# header) for why <filesystem>/<fstream> are here: a scratch core file
# that #includes <fstream> passed the PREVIOUS pattern clean before
# that fix.
OS_HEADER_NEEDLES = (
    "wayland",
    "windows.h",
    "winuser",
    "GL/",
    "EGL/",
    "<dlfcn",
    "<unistd",
    "<sys/",
    "<fcntl",
    "<filesystem",
    "<fstream",
)

_HEADER_EXTENSIONS = (".hpp", ".cpp", ".h", ".hh", ".hxx", ".cc", ".cxx")

_FORBIDDEN_PATTERN = re.compile(
    "|".join(re.escape(needle) for needle in (UPPER_LAYER_NEEDLE,) + OS_HEADER_NEEDLES)
)

# ANCHOR-ON-DIRECTIVE (see this file's own top-of-file comment): the
# needle is only ever searched inside the ARGUMENT of one of these two
# directive forms, never in a bare line. `^\s*` only tolerates leading
# WHITESPACE before the keyword - a "//" comment prefix, or any other
# non-whitespace text, rules the line out before the keyword is even
# reached, so a commented-out include ("// #include <windows.h>") does
# not match either pattern.
_INCLUDE_DIRECTIVE_PATTERN = re.compile(r'^\s*#\s*include\s*(<[^>\n]*>|"[^"\n]*")')
_IMPORT_DIRECTIVE_PATTERN = re.compile(r'^\s*(?:export\s+)?import\s*(<[^>\n]*>|"[^"\n]*")\s*;')


def _directive_argument(line):
    """Returns the bracketed/quoted header-name argument of a LOGICAL
    line (post phase-2/phase-3, see _directive_lines() below) that is
    a #include or a C++23 header-unit import directive, or None when
    the line is neither (prose, a comment, plain code, a module import
    with no header-name form). Only the captured argument - never the
    rest of the line - is searched for the forbidden needles, so a
    real include followed by an unrelated trailing comment on the same
    logical line ("#include <good.hpp> // GL/ stuff") does not false-
    positive on the comment half either.
    """
    match = _INCLUDE_DIRECTIVE_PATTERN.match(line) or _IMPORT_DIRECTIVE_PATTERN.match(line)
    return match.group(1) if match else None


# PHASE-2-3 (achado real, 22/09/2026, re-verificacao do lider apos o
# ANCHOR-ON-DIRECTIVE acima): _directive_argument() sozinho ancora
# certo NA LINHA FISICA, mas o pre-processador real NAO le linha
# fisica. Medido em copia fora da arvore, nos DOIS sentidos, contra
# src/core/ (a camada que ja era protegida antes desta fatia):
#   - `#include \` + quebra + `<fstream>` - o portao ancorado por linha
#     fisica NAO via (falso negativo: cada metade da diretiva cai em
#     uma linha fisica diferente, nenhuma delas casa sozinha).
#   - `#include /* nada */ <fstream>` - idem (o comentario de bloco no
#     meio da diretiva impede o regex de casar `#\s*include\s*<...>`
#     como uma unica sequencia).
# A causa: a norma C++ especifica duas fases de traducao ANTES de
# qualquer diretiva ser reconhecida - fase 2 emenda toda linha
# terminada em barra invertida com a seguinte (ANTES de qualquer
# comentario ou literal ser identificado, entao a emenda acontece
# mesmo "dentro" do que vira comentario/string), e fase 3 troca cada
# comentario por um unico espaco, respeitando literais de cadeia e de
# caractere (um '/*' ou uma '"' dentro de aspas nao abre comentario
# nem fecha a string). _splice_lines() modela a fase 2; _strip_
# comments_and_literals() modela a fase 3; _directive_lines() as
# encadeia e SO ENTAO aplica _directive_argument() por linha LOGICA
# resultante, preservando o numero da linha FISICA original (a do
# primeiro caractere da linha logica) para a mensagem de reprovacao
# continuar citando arquivo:linha certo. Cada caso deste comentario
# foi confirmado contra o compilador real (`g++ -std=c++23
# -fsyntax-only`) antes de virar controle de --selftest - GODS_LAWS.md
# L-22: "a pesquisa vem antes do planejamento", aqui aplicada como "o
# compilador vem antes da minha tabela".
#
# LIMITACAO CONHECIDA, verificada ausente em src/core, src/gfss,
# src/gfui, include/glintfx/{core,gfss,gfui} em 22/09/2026 (grep por
# `R"..."(`  devolveu zero ocorrencias): string literal bruta
# (`R"delim(...)delim"`) nao e reconhecida como estado proprio - seu
# conteudo e varrido como codigo comum. Nao e lacuna silenciosa: este
# paragrafo a nomeia, para quando este projeto passar a usar `R"(...)"`
# nessas camadas.
def _splice_lines(text):
    """Phase 2 (emenda de linha): uma barra invertida imediatamente
    seguida de nova linha (\\n ou \\r\\n) e removida, juntando a linha
    fisica com a seguinte - incondicionalmente, antes de qualquer
    comentario ou literal ser reconhecido. Retorna (spliced_text,
    origin_lines): origin_lines[i] e o numero da linha fisica (1-based)
    de onde veio o caractere spliced_text[i] - mesmo tamanho dos dois,
    sempre.
    """
    origin_lines = []
    out = []
    line_no = 1
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        if ch == "\\" and i + 1 < n and text[i + 1] in ("\n", "\r"):
            if text[i + 1] == "\r" and i + 2 < n and text[i + 2] == "\n":
                i += 3
            else:
                i += 2
            line_no += 1
            continue
        out.append(ch)
        origin_lines.append(line_no)
        if ch == "\n":
            line_no += 1
        i += 1
    return "".join(out), origin_lines


def _strip_comments_and_literals(text, origin_lines):
    """Phase 3 (parcial): troca cada comentario de bloco (/* ... */,
    podendo atravessar linhas) e de linha (// ... ate a proxima nova
    linha real) por um UNICO espaco, sem entrar em literais de cadeia
    ou de caractere (uma barra invertida escapa o proximo caractere
    dentro de um literal, entao uma aspas escapada nao o fecha cedo).
    Retorna (clean_text, clean_lines) no mesmo contrato de alinhamento
    de _splice_lines() acima - sempre do mesmo tamanho um do outro.
    """
    state_code, state_string, state_char, state_block, state_line = range(5)
    state = state_code
    out = []
    out_lines = []
    comment_start_line = None
    n = len(text)
    i = 0
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == state_code:
            if ch == "/" and nxt == "*":
                state = state_block
                comment_start_line = origin_lines[i]
                i += 2
                continue
            if ch == "/" and nxt == "/":
                state = state_line
                comment_start_line = origin_lines[i]
                i += 2
                continue
            if ch == '"':
                state = state_string
            elif ch == "'":
                state = state_char
            out.append(ch)
            out_lines.append(origin_lines[i])
            i += 1
            continue
        if state in (state_string, state_char):
            out.append(ch)
            out_lines.append(origin_lines[i])
            if ch == "\\" and i + 1 < n:
                out.append(text[i + 1])
                out_lines.append(origin_lines[i + 1])
                i += 2
                continue
            if (state == state_string and ch == '"') or (state == state_char and ch == "'"):
                state = state_code
            i += 1
            continue
        if state == state_block:
            if ch == "*" and nxt == "/":
                out.append(" ")
                out_lines.append(comment_start_line)
                state = state_code
                i += 2
                continue
            i += 1
            continue
        if state == state_line:
            if ch == "\n":
                out.append(" ")
                out_lines.append(comment_start_line)
                out.append(ch)
                out_lines.append(origin_lines[i])
                state = state_code
                i += 1
                continue
            i += 1
            continue
    if state in (state_line, state_block):
        # Comentario nao fechado ate o fim do arquivo (arquivo mal
        # formado) - emite o espaco mesmo assim, em vez de descartar
        # o restante em silencio; um '*/' faltando ja seria erro de
        # compilacao real antes disso.
        out.append(" ")
        out_lines.append(comment_start_line)
    return "".join(out), out_lines


def _directive_lines(text):
    """Gera (linha_fisica, argumento) para cada diretiva #include/
    import encontrada em `text`, apos modelar as fases 2 e 3 da
    traducao (ver o comentario PHASE-2-3 acima). `linha_fisica` e a
    linha do PRIMEIRO caractere da linha logica correspondente - onde
    a diretiva de fato comeca no arquivo real, mesmo quando ela foi
    emendada ou teve um comentario no meio.
    """
    spliced_text, spliced_lines = _splice_lines(text)
    clean_text, clean_lines = _strip_comments_and_literals(spliced_text, spliced_lines)

    start = 0
    n = len(clean_text)
    for idx, ch in enumerate(clean_text):
        if ch != "\n":
            continue
        segment = clean_text[start:idx]
        if segment.strip():
            argument = _directive_argument(segment)
            if argument is not None:
                yield clean_lines[start], argument
        start = idx + 1
    if start < n:
        segment = clean_text[start:n]
        if segment.strip():
            argument = _directive_argument(segment)
            if argument is not None:
                yield clean_lines[start], argument


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def core_source_dirs(root):
    for candidate in (
        os.path.join(root, "src", "core"),
        os.path.join(root, "include", "glintfx", "core"),
    ):
        if os.path.isdir(candidate):
            yield candidate


def _walk_header_files(directory):
    files = []
    for dirpath, _dirnames, filenames in os.walk(directory):
        for name in filenames:
            if name.endswith(_HEADER_EXTENSIONS):
                files.append(os.path.join(dirpath, name))
    return files


def core_source_files(root):
    """One os.walk() per candidate directory - directory entries come
    back as discrete strings from the filesystem, never a newline-
    joined text stream a hostile filename could split (the same
    GATE-TREE-PARITY-NEWLINE reasoning check_vendor_purity.py's own
    header documents).
    """
    files = []
    for source_dir in core_source_dirs(root):
        files.extend(_walk_header_files(source_dir))
    return files


def violations_in_file(path):
    violations = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
        return violations
    for lineno, argument in _directive_lines(text):
        if _FORBIDDEN_PATTERN.search(argument):
            violations.append((path, lineno))
    return violations


# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero files found
# under src/core/ or include/glintfx/core/ is not "nothing to report",
# and reproves.
def require_nonempty_scan(file_count):
    if file_count == 0:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 arquivos em src/core ou "
            "include/glintfx/core) - GODS_LAWS.md L-40",
            file=sys.stderr,
        )
        return False
    return True


# --- gfss/gfui additions (LAYERS-GATE-GFSS-GFUI) ----------------------
#
# Unlike core_source_dirs()/require_nonempty_scan() above, whose floor
# is on the AGGREGATE file count across src/core/ and
# include/glintfx/core/ combined, each of these four directories gets
# its OWN floor, checked separately: a layer split across
# src/<name>/ and include/glintfx/<name>/ can have its files
# concentrated almost entirely in one of the two (measured against the
# real tree on 22/09/2026: include/glintfx/gfui/ has exactly ONE
# public header, node_view.hpp, against dozens under src/gfui/) - an
# AGGREGATE check would let a directory that silently lost every file
# hide behind the other directory's count. GODS_LAWS.md L-40: absence
# is declared and counted, never a quiet skip - so a missing directory
# and an existing-but-empty one are both failures, reported by name.
GFSS_GFUI_DIR_SPECS = (
    ("src/gfss", ("src", "gfss")),
    ("src/gfui", ("src", "gfui")),
    ("include/glintfx/gfss", ("include", "glintfx", "gfss")),
    ("include/glintfx/gfui", ("include", "glintfx", "gfui")),
)


def gfss_gfui_dir_files(root):
    """Returns {label: (files, exists)} for each of the four directories
    in GFSS_GFUI_DIR_SPECS. `exists` is False when the directory itself
    is missing (distinct from existing-but-empty; both fail the floor
    below, but the message says which case it is)."""
    result = {}
    for label, parts in GFSS_GFUI_DIR_SPECS:
        path = os.path.join(root, *parts)
        if not os.path.isdir(path):
            result[label] = ([], False)
            continue
        result[label] = (_walk_header_files(path), True)
    return result


def require_nonempty_gfss_gfui_dirs(dir_files):
    ok = True
    for label, (files, exists) in dir_files.items():
        if not exists:
            print(
                f"{SCRIPT_NAME}: varredura vazia ({label} nao existe) - "
                "GODS_LAWS.md L-40",
                file=sys.stderr,
            )
            ok = False
        elif len(files) == 0:
            print(
                f"{SCRIPT_NAME}: varredura vazia (0 arquivos em {label}) - "
                "GODS_LAWS.md L-40",
                file=sys.stderr,
            )
            ok = False
    return ok


# The actual gate logic, factored out of real_main() so --selftest
# exercises the EXACT same function - not a reimplementation that
# could drift from production.
def check_layers(root):
    core_files = core_source_files(root)
    if not require_nonempty_scan(len(core_files)):
        return False

    gfss_gfui_files = gfss_gfui_dir_files(root)
    if not require_nonempty_gfss_gfui_dirs(gfss_gfui_files):
        return False

    files = list(core_files)
    for dir_file_list, _exists in gfss_gfui_files.values():
        files.extend(dir_file_list)
    file_count = len(files)

    violations = []
    for f in files:
        violations.extend(violations_in_file(f))

    if violations:
        print(f"{SCRIPT_NAME}: layer violations (GODS_LAWS.md L-19):", file=sys.stderr)
        for path, lineno in violations:
            print(f"{path}:{lineno}", file=sys.stderr)
        return False

    print(f"{SCRIPT_NAME}: violations: 0 in {file_count} files scanned")
    return True


# --- real mode -------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_layers.py <source-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_layers(root):
        fail("layer violation found (GODS_LAWS.md L-19; see message above)")


# --- fixtures and controls for --selftest -----------------------------


def make_scratch_workdir():
    # A hand written Unix path does not exist on every platform (Windows
    # has no /tmp). dir=os.environ.get("TMPDIR") without a hardcoded
    # fallback lets tempfile.mkdtemp fall through to gettempdir(), which
    # already checks TMPDIR/TEMP/TMP and then the platform default.
    return tempfile.mkdtemp(prefix="glintfx-layers-selftest-", dir=os.environ.get("TMPDIR"))


def _write_clean_file(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("#include <cstdint>\n// clean layer file, no OS or upper-layer header\n")


def make_gfss_gfui_clean_fixture(root):
    for _label, parts in GFSS_GFUI_DIR_SPECS:
        _write_clean_file(os.path.join(root, *parts, "clean.hpp"))


def make_clean_fixture(root):
    _write_clean_file(os.path.join(root, "src", "core", "clean.cpp"))
    make_gfss_gfui_clean_fixture(root)


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

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print("selftest: controle POSITIVO OK (fixture limpa aprovada)")
        return True
    print(
        "selftest: controle POSITIVO FALHOU (fixture limpa deveria ter sido aprovada)",
        file=sys.stderr,
    )
    print(outcome.text, file=sys.stderr)
    return False


# Negative control: plants a forbidden OS header inside
# include/glintfx/core/. Expected: reproves and cites the planted file.
def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    make_clean_fixture(root)
    target_dir = os.path.join(root, "include", "glintfx", "core")
    os.makedirs(target_dir, exist_ok=True)
    target = os.path.join(target_dir, "dirty.hpp")
    with open(target, "w", encoding="utf-8") as handle:
        handle.write("#include <wayland-client.h>\n")

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO FALHOU (header do SO em "
            "include/glintfx/core/ nao foi pego)",
            file=sys.stderr,
        )
        return False
    if target not in outcome.text:
        print(
            f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou {target})",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (header do SO em include/glintfx/core/ pego e citado)")
    return True


# Second negative control (ASSET-LOAD conserto, 28/08/2026): plants
# <fstream> - a file-I/O header, not a Wayland/GL/POSIX one - inside
# src/core/. Expected: reproves and cites the planted file. Separate
# function, separate fixture, on purpose (GODS_LAWS.md L-40 "enumeracao
# fechada por construcao"): the ORIGINAL negative control above only
# ever exercises the wayland-client.h branch of OS_HEADER_NEEDLES, so a
# regression that broke JUST the <filesystem>/<fstream> entries would
# pass every other control silently - this is the control that closes
# that gap specifically.
def selftest_negative_control_file_header(scratch, capture):
    root = os.path.join(scratch, "negative_file_header")
    make_clean_fixture(root)
    target = os.path.join(root, "src", "core", "dirty_file_io.cpp")
    with open(target, "w", encoding="utf-8") as handle:
        handle.write("#include <fstream>\n")

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle NEGATIVO (header de arquivo) FALHOU "
            "(<fstream> em src/core/ nao foi pego)",
            file=sys.stderr,
        )
        return False
    if target not in outcome.text:
        print(
            "selftest: controle NEGATIVO (header de arquivo) FALHOU "
            f"(reprovou, mas nao citou {target})",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print(
        "selftest: controle NEGATIVO (header de arquivo) OK "
        "(<fstream> em src/core/ pego e citado)"
    )
    return True


# Empty-scan floor: neither src/core/ nor include/glintfx/core/ exists.
# Expected: reproves with "varredura vazia" in the message.
def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    os.makedirs(root, exist_ok=True)

    outcome = capture(lambda: check_layers(root))
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (raiz sem "
            "src/core nem include/glintfx/core deveria ter sido "
            "recusada, mas passou)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    if "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas "
            "nao disse 'varredura vazia')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (raiz sem diretorio de core recusada)")
    return True


# LAYERS-GATE-GFSS-GFUI control 1: a forbidden OS header planted in
# EACH of the four gfss/gfui directories in turn (own fixture per
# directory, so one broken branch cannot hide behind another passing).
# Expected: reproves and cites the planted file, for all four.
def selftest_negative_control_gfss_gfui(scratch, capture):
    ok = True
    for label, parts in GFSS_GFUI_DIR_SPECS:
        safe_label = label.replace("/", "_")
        root = os.path.join(scratch, f"negative_gfss_gfui_{safe_label}")
        make_clean_fixture(root)
        target = os.path.join(root, *parts, "dirty.hpp")
        with open(target, "w", encoding="utf-8") as handle:
            handle.write("#include <wayland-client.h>\n")

        outcome = capture(lambda: check_layers(root))
        if outcome.result:
            print(
                f"selftest: controle NEGATIVO ({label}) FALHOU (header do SO "
                "nao foi pego)",
                file=sys.stderr,
            )
            ok = False
            continue
        if target not in outcome.text:
            print(
                f"selftest: controle NEGATIVO ({label}) FALHOU (reprovou, mas "
                f"nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        print(f"selftest: controle NEGATIVO ({label}) OK (header do SO pego e citado)")
    return ok


# LAYERS-GATE-GFSS-GFUI control 2: the PER-DIRECTORY floor. For each of
# the four directories in turn, build a fixture where the OTHER three
# are populated and clean, and THIS one is present but empty (zero
# matching files). Expected: reproves, naming exactly this directory -
# proving the floor is per-directory, not an aggregate that the other
# three's files could mask.
def selftest_gfss_gfui_per_directory_floor(scratch, capture):
    ok = True
    for label, parts in GFSS_GFUI_DIR_SPECS:
        safe_label = label.replace("/", "_")
        root = os.path.join(scratch, f"floor_gfss_gfui_{safe_label}")
        make_clean_fixture(root)
        empty_dir = os.path.join(root, *parts)
        # empty it back out: make_clean_fixture already wrote clean.hpp
        # into it above, remove just that one file so the directory
        # exists but is empty, while the other three keep theirs.
        os.remove(os.path.join(empty_dir, "clean.hpp"))

        outcome = capture(lambda: check_layers(root))
        if outcome.result:
            print(
                f"selftest: controle de PISO POR DIRETORIO ({label}) FALHOU "
                "(diretorio vazio deveria ter sido recusado, mas passou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if f"0 arquivos em {label}" not in outcome.text:
            print(
                f"selftest: controle de PISO POR DIRETORIO ({label}) FALHOU "
                f"(recusou, mas nao citou '0 arquivos em {label}')",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        print(
            f"selftest: controle de PISO POR DIRETORIO ({label}) OK "
            "(diretorio vazio recusado nominalmente, sem mascaramento pelos outros tres)"
        )
    return ok


# ANCHOR-ON-DIRECTIVE control (ordem do lider, 22/09/2026: "Olhar so
# diretivas de inclusao"): the needle search only fires inside a line
# that IS an #include or C++23 header-unit import directive - never
# inside a comment that merely mentions one of the needle substrings
# in prose (the false positive that started this: anb_parse.cpp:159's
# own "a GL/WGL function name is"), and never missed just because the
# real include uses C++23 `import <header>;` instead of `#include
# <header>` (a naive #include-only anchor would have opened exactly
# that hole - a real forbidden import passing silently, worse than the
# false positive it fixes). Each case below is planted, alone, into an
# otherwise-clean fixture; `True` means the gate must reprove and cite
# the planted file, `False` means it must pass clean.
_ANCHOR_DIRECTIVE_CASES = (
    ("#include <fstream>\n", True),
    ("#  include <GL/gl.h>\n", True),
    ("#include<windows.h>\n", True),
    ("import <fstream>;\n", True),
    ("export import <fstream>;\n", True),
    ("// a GL/WGL function name is a mouthful\n", False),
    ("// #include <windows.h>\n", False),
)


def selftest_anchor_directive_control(scratch, capture):
    ok = True
    for index, (planted_line, should_reprove) in enumerate(_ANCHOR_DIRECTIVE_CASES):
        root = os.path.join(scratch, f"anchor_{index}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"anchor_case_{index}.cpp")
        with open(target, "w", encoding="utf-8") as handle:
            handle.write(planted_line)

        outcome = capture(lambda: check_layers(root))
        reproved = not outcome.result
        label = repr(planted_line.rstrip("\n"))

        if should_reprove and not reproved:
            print(
                f"selftest: controle de ANCORA FALHOU ({label} deveria ter "
                "reprovado, mas passou)",
                file=sys.stderr,
            )
            ok = False
            continue
        if should_reprove and target not in outcome.text:
            print(
                f"selftest: controle de ANCORA FALHOU ({label} reprovou, mas "
                f"nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if not should_reprove and reproved:
            print(
                f"selftest: controle de ANCORA FALHOU ({label} deveria ter "
                "passado (nao e diretiva de inclusao), mas reprovou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue

        verdict = "reprovado" if should_reprove else "passou"
        print(f"selftest: controle de ANCORA OK ({label} {verdict} como esperado)")
    return ok


# PHASE-2-3 control (re-verificacao do lider, 22/09/2026): os cinco
# casos abaixo, cada um confirmado contra `g++ -std=c++23
# -fsyntax-only` antes de virar controle (ver o comentario PHASE-2-3
# no topo do arquivo). Cada entrada e o CONTEUDO MULTI-LINHA completo
# plantado num arquivo novo de src/core/ - nao uma unica linha, porque
# o proprio fenomeno sob teste (emenda de linha, comentario
# multi-linha) so existe atravessando mais de uma linha fisica.
_PHASE23_DIRECTIVE_CASES = (
    ("case_a_splice_before_bracket", "#include \\\n<fstream>\n", True),
    ("case_b_block_comment_mid_directive", "#include /* nada */ <fstream>\n", True),
    (
        "case_c_directive_inside_block_comment",
        "/*\n#include <windows.h>\n*/\n",
        False,
    ),
    (
        "case_d_quote_does_not_open_comment",
        'const char* s = "/*";\n#include <fstream>\n// */\n',
        True,
    ),
    (
        "case_e_line_comment_swallows_spliced_continuation",
        "// comentario \\\n#include <fstream>\n",
        False,
    ),
)


def selftest_phase23_directive_control(scratch, capture):
    ok = True
    for name, content, should_reprove in _PHASE23_DIRECTIVE_CASES:
        root = os.path.join(scratch, f"phase23_{name}")
        make_clean_fixture(root)
        target = os.path.join(root, "src", "core", f"{name}.cpp")
        with open(target, "w", encoding="utf-8") as handle:
            handle.write(content)

        outcome = capture(lambda: check_layers(root))
        reproved = not outcome.result

        if should_reprove and not reproved:
            print(
                f"selftest: controle FASE-2-3 FALHOU ({name} deveria ter "
                "reprovado, mas passou)",
                file=sys.stderr,
            )
            ok = False
            continue
        if should_reprove and target not in outcome.text:
            print(
                f"selftest: controle FASE-2-3 FALHOU ({name} reprovou, mas "
                f"nao citou {target})",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue
        if not should_reprove and reproved:
            print(
                f"selftest: controle FASE-2-3 FALHOU ({name} deveria ter "
                "passado, mas reprovou)",
                file=sys.stderr,
            )
            print(outcome.text, file=sys.stderr)
            ok = False
            continue

        verdict = "reprovado" if should_reprove else "passou"
        print(f"selftest: controle FASE-2-3 OK ({name} {verdict} como esperado)")
    return ok


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_negative_control_file_header(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_negative_control_gfss_gfui(scratch, capture),
            selftest_gfss_gfui_per_directory_floor(scratch, capture),
            selftest_anchor_directive_control(scratch, capture),
            selftest_phase23_directive_control(scratch, capture),
        ]
        if not all(controls):
            print("check_layers.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_layers.py --selftest: os {len(controls)} controles OK")
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
