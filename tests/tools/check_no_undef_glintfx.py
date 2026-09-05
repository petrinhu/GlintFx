#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_no_undef_glintfx.py - CI gate for LIB-NO-UNDEF (TODO.md,
# GODS_LAWS.md L-36/L-40/L-04/L-45).
#
# THE REAL DEFECT THIS CLOSES (measured 05/09/2026, commit 8eb7d73):
# compound_match.cpp (part of the glintfx_library target) started
# calling attribute_selector_holds() and four structural_* functions
# whose .cpp files (attribute_match.cpp, structural_match.cpp) had
# been added to the TEST targets only, never to src/gfui/CMakeLists.
# txt's target_sources(glintfx_library ...) call. A shared library
# tolerates this in total silence: the link SUCCEEDS (GNU ld does not
# require a .so's own internal references to resolve within the same
# link unless -Wl,--no-undefined is passed - this project's link line
# does not pass it), `nm -D` shows the five symbols as genuinely
# undefined dynamic references, ctest's 88 cases stayed green (the
# suite recompiles the sources a second time INSIDE each test binary
# instead of linking against the shipped library - legitimate
# technique, but it means the suite proves the SOURCE, never the
# ARTIFACT), and only a consumer's own process crashes, the moment it
# calls one of these five functions through the .so - see AUDITORIAS.md
# chapter on the three contracts of version/ABI for why "the tests are
# green" was never a claim about the shipped binary.
#
# DIVISION OF LABOR WITH check_exports.sh (read that file's header
# before touching this one - GODS_LAWS.md L-36 asked for it
# explicitly): check_exports.sh scans DEFINED dynamic symbols and
# reproves anything OUTSIDE the glintfx:: namespace (a leak - the
# library exports MORE than the contract promises, e.g. a stray
# std:: symbol pulled in by an ABI quirk). This gate scans UNDEFINED
# dynamic symbols and reproves anything INSIDE the glintfx:: namespace
# (a hole - the library calls something of ITS OWN it forgot to link
# in). One direction is "too much leaves the library"; the other is
# "not enough entered it". Neither gate subsumes the other: a binary
# can fail either check independently of the other's verdict.
#
# FALSE-POSITIVE GUARD, MEASURED, NOT GUESSED: a healthy glintfx.so
# legitimately carries dynamic undefined symbols that come from the
# C/C++ runtime, libstdc++, libwayland-client and glibc (memcpy, the
# GCC unwinder, wl_display_connect, std::from_chars, ...). Counted
# against build/src/libglintfx.so.0.2.0.0 on 05/09/2026 (HEAD, commit
# f2d0660, `nm -D --undefined-only ... | wc -l` vs `... | grep -c
# glintfx`): 52 legitimate undefined dynamic symbols, 0 of them
# glintfx::-prefixed. Against the SAME command on the defective
# commit 8eb7d73's freshly rebuilt library: 57 undefined dynamic
# symbols, of which exactly 5 are glintfx::-prefixed (the five named
# above) and the other 52 are the identical legitimate set - proving
# the filter below separates the two groups cleanly on a real,
# previously-real defect, not a synthetic one.
#
# STATIC MODE (`--static`): measured 05/09/2026 against build-static/
# src/libglintfx.a - `nm --undefined-only libglintfx.a | grep -c " U
# "` counts 119 undefined references, and grep -c glintfx among those
# counts 30. This is NOT a defect and not comparable to the shared
# case: a .a is an uncombined bag of .o files, and every .o
# legitimately shows "undefined" for a glintfx:: symbol that some
# OTHER .o in the very same archive defines - the archive has no link
# step of its own, so nothing has been resolved yet by construction,
# and nothing here can distinguish "will resolve when the consumer
# links" from "will not". The check that matters for static consumers
# is the consumer's OWN final link (the standard --no-undefined
# behavior of ANY linker building a final executable already reproves
# a truly missing definition there, in a way this project cannot
# rehearse without a consumer binary). --static therefore never fails
# on symbol content; it exists so the case is declared and its count
# printed on every run (GODS_LAWS.md L-40: an absence of a check is
# ANNOUNCED, never a silent skip in tests/CMakeLists.txt), not
# invented as a check with no real object to reprove.
#
# WINDOWS: see tools/ci/check-no-undef-glintfx-win.ps1's own header
# for the by-construction argument (MSVC's link.exe fails a DLL build
# with LNK2019/LNK1120 on ANY unresolved reference among its own
# object files, unlike GNU ld's default for a .so) and the count it
# prints instead.
#
# WHAT THIS GATE DOES NOT CATCH (read before trusting its green,
# GODS_LAWS.md L-41 applied to the next reader instead of only to the
# leader - a portao sold as stronger than it is becomes false comfort,
# and this project already paid once for that shape: 88 green tests
# next to a broken shipped artifact):
#
#   * A glintfx:: symbol that resolves to the WRONG definition (an ODR
#     violation, two different .cpp files defining the same mangled
#     name differently) is INVISIBLE here. This gate only sees total
#     ABSENCE of a definition, never a silently wrong one - `nm -D`
#     cannot tell "resolved correctly" from "resolved to garbage".
#   * `--static` NEVER reproves by symbol content, on any input,
#     ever. The 30-of-119 count measured against today's libglintfx.a
#     is the NORMAL shape of an unlinked archive, not evidence of
#     health - the real guarantee for a static consumer only exists
#     at THAT consumer's own final link, which this repository has no
#     binary to rehearse against. A green --static run proves the gate
#     ran and counted, never that static consumption is safe.
#   * Only DYNAMIC symbols are in scope (what `nm -D` can see on a
#     .so, or what an archive's per-object symbol table shows on a
#     .a). A call the compiler already inlined, or a reference fully
#     resolved inside a single translation unit, never becomes a
#     dynamic relocation and never appears in this scan either way.
#   * This gate does not know what check_exports.sh already covers
#     (a symbol leaving the library that should not) - see the
#     DIVISION OF LABOR paragraph above. A binary can pass this gate
#     and still fail that one, or the reverse.
#
# Usage:
#   check_no_undef_glintfx.py --shared <path-to-.so>
#   check_no_undef_glintfx.py --static <path-to-.a>
#   check_no_undef_glintfx.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import shutil
import subprocess
import sys

SCRIPT_NAME = "check_no_undef_glintfx.py"

# Same contract as check_exports.sh's EXPECTED_MANGLED_NAMESPACE - kept
# as a literal string here rather than imported, because these are two
# independent POSIX-sh/Python gates by design (GODS_LAWS.md L-33: no
# cross-language coupling between tests/tools/ scripts) and the two
# prefixes are cheap enough to keep in sync by inspection.
_MANGLED_NAMESPACE = "7glintfx"
_FREE_OR_NONCONST_PREFIX = "_ZN" + _MANGLED_NAMESPACE
_CONST_MEMBER_PREFIX = "_ZNK" + _MANGLED_NAMESPACE


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def symbol_is_glintfx_namespaced(symbol):
    return symbol.startswith(_FREE_OR_NONCONST_PREFIX) or symbol.startswith(
        _CONST_MEMBER_PREFIX
    )


def require_nm_present():
    if shutil.which("nm") is None:
        fail("'nm' not found in PATH (no silent skip)")


# Parses `nm -D --undefined-only <path>` / `nm --undefined-only <path>`
# text into a flat list of raw (still-mangled) symbol names. nm's
# undefined-only listing has no address column, so each line is just
# "<type-char> <name>" (type 'U', or 'w' for a weak undefined such as
# __gmon_start__) - splitting on whitespace and keeping the LAST token
# survives both the no-version and the "name@GLIBC_x.y.z" versioned
# forms glibc symbols carry.
def parse_undefined_symbol_names(nm_output_text):
    names = []
    for line in nm_output_text.splitlines():
        line = line.strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        names.append(parts[-1])
    return names


def run_nm_undefined_only(nm_args, path):
    proc = subprocess.run(
        ["nm", *nm_args, "--undefined-only", path],
        capture_output=True,
        text=True,
        check=False,
    )
    # nm's own exit code, read from the completed-process object, never
    # from a piped `| tail`/`| wc` stage (GODS_LAWS.md L-45).
    if proc.returncode != 0:
        fail(
            f"'nm {' '.join(nm_args)} --undefined-only {path}' saiu com "
            f"codigo {proc.returncode}: {proc.stderr.strip()}"
        )
    return parse_undefined_symbol_names(proc.stdout)


def demangle_best_effort(mangled_names):
    if shutil.which("c++filt") is None or not mangled_names:
        return dict.fromkeys(mangled_names, "")
    proc = subprocess.run(
        ["c++filt"], input="\n".join(mangled_names), capture_output=True, text=True, check=False
    )
    if proc.returncode != 0:
        return dict.fromkeys(mangled_names, "")
    demangled = proc.stdout.splitlines()
    if len(demangled) != len(mangled_names):
        return dict.fromkeys(mangled_names, "")
    return dict(zip(mangled_names, demangled))


# GODS_LAWS.md L-40 (piso de varredura nao-vazia): an empty nm listing
# means nm read the wrong file, or the artifact is stripped/empty -
# never "nothing to report". A HEALTHY glintfx.so/.a always has
# dozens of legitimate runtime undefined symbols (measured above), so
# zero here is never a clean pass.
def require_nonempty_scan(total_count, artifact_kind):
    if total_count == 0:
        fail(
            f"varredura vazia (0 simbolos indefinidos em {artifact_kind}) - "
            "nm leu o arquivo errado, ou o artefato esta vazio/stripped "
            "(GODS_LAWS.md L-40)"
        )


def check_shared_library(path):
    require_nm_present()
    symbols = run_nm_undefined_only(["-D"], path)
    total_count = len(symbols)
    require_nonempty_scan(total_count, ".so (simbolos dinamicos indefinidos)")

    intruders = [s for s in symbols if symbol_is_glintfx_namespaced(s)]
    legit_count = total_count - len(intruders)

    print(
        f"{SCRIPT_NAME}: {path}: {total_count} simbolo(s) dinamico(s) "
        f"indefinido(s) escaneado(s)."
    )

    if intruders:
        demangled = demangle_best_effort(intruders)
        print(
            f"{SCRIPT_NAME}: {len(intruders)} simbolo(s) glintfx:: ficaram "
            "sem definicao na biblioteca entregue - um modulo dentro da "
            "biblioteca chama algo do nosso proprio namespace que nao foi "
            "linkado, e so quebra na mao do consumidor (GODS_LAWS.md L-36):",
            file=sys.stderr,
        )
        for symbol in intruders:
            readable = demangled.get(symbol, "")
            if readable:
                print(f"  {symbol}  ({readable})", file=sys.stderr)
            else:
                print(f"  {symbol}", file=sys.stderr)
        return False

    print(
        f"ok: nenhum simbolo glintfx:: indefinido ({legit_count} simbolo(s) "
        "indefinido(s) legitimo(s) - runtime/libstdc++/libwayland/glibc - "
        "escaneado(s), 0 do nosso proprio namespace)."
    )
    return True


# --static NUNCA reprova por conteudo de simbolo (ver cabecalho: um .a
# nao tem etapa de link propria, entao toda referencia "indefinida"
# apontando para outro objeto do MESMO arquivo e esperada). Ainda
# assim roda de verdade e imprime a contagem real - GODS_LAWS.md L-40
# proibe ausencia de verificacao silenciosa; a ausencia aqui e
# DECLARADA e CONTADA a cada execucao, nunca um caso de ctest que
# simplesmente nao existe.
def check_static_library(path):
    require_nm_present()
    symbols = run_nm_undefined_only([], path)
    total_count = len(symbols)
    require_nonempty_scan(total_count, ".a (simbolos indefinidos entre objetos do arquivo)")

    glintfx_count = sum(1 for s in symbols if symbol_is_glintfx_namespaced(s))

    print(
        f"{SCRIPT_NAME}: {path}: modo estatico - NAO APLICAVEL por "
        "natureza (declarado, GODS_LAWS.md L-04/L-40): um .a nao tem "
        "etapa de link propria, entao cada objeto legitimamente mostra "
        "'indefinido' para um simbolo definido em OUTRO objeto do mesmo "
        f"arquivo. Escaneados {total_count} simbolo(s) indefinido(s) no "
        f"total, {glintfx_count} deles glintfx:: - numero esperado ser "
        "maior que zero, e nao um defeito. A resolucao real acontece so "
        "no link final do CONSUMIDOR, que este repositorio nao pode "
        "ensaiar sem um binario consumidor."
    )
    return True


def real_main(argv):
    if len(argv) != 2 or argv[0] not in ("--shared", "--static"):
        fail("usage: check_no_undef_glintfx.py --shared <path-to-.so> | --static <path-to-.a>")

    mode, path = argv[0], argv[1]

    ok = check_shared_library(path) if mode == "--shared" else check_static_library(path)
    if not ok:
        sys.exit(1)


# --- --selftest (GODS_LAWS.md L-36: estreia vermelha obrigatoria; L-40:
# os tres controles da casa). Feeds synthetic nm-shaped text straight
# into parse_undefined_symbol_names()/symbol_is_glintfx_namespaced() -
# no real binary, no real `nm` invocation - because what needs proving
# here is the PARSER/FILTER logic; the real binary is what the two
# live runs documented in this file's own header already proved
# against 8eb7d73 (red) and HEAD (green). ---

_SELFTEST_HEALTHY_NM_TEXT = """\
                 U memcpy@GLIBC_2.14
                 U wl_display_connect
                 w __gmon_start__
                 U _ZSt19__throw_length_errorPKc
"""

_SELFTEST_DEFECTIVE_NM_TEXT = (
    _SELFTEST_HEALTHY_NM_TEXT
    + "                 U _ZN7glintfx4gfui6detail24attribute_selector_holdsERKNS_5style6detail"
    "20gfss_simple_selectorERKNS0_15gltfx_node_viewE\n"
)


def selftest_positive_control():
    symbols = parse_undefined_symbol_names(_SELFTEST_HEALTHY_NM_TEXT)
    intruders = [s for s in symbols if symbol_is_glintfx_namespaced(s)]
    if intruders:
        print(
            f"selftest: controle POSITIVO FALHOU (simbolos legitimos "
            f"acusados como glintfx::): {intruders}",
            file=sys.stderr,
        )
        return False
    print(
        f"selftest: controle POSITIVO OK ({len(symbols)} simbolos "
        "legitimos, 0 acusados)"
    )
    return True


def selftest_negative_control_real_defect_shape():
    symbols = parse_undefined_symbol_names(_SELFTEST_DEFECTIVE_NM_TEXT)
    intruders = [s for s in symbols if symbol_is_glintfx_namespaced(s)]
    if len(intruders) != 1 or not intruders[0].startswith(_FREE_OR_NONCONST_PREFIX):
        print(
            "selftest: controle NEGATIVO FALHOU (deveria achar exatamente "
            f"1 simbolo glintfx:: indefinido, achou {intruders})",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle NEGATIVO OK (o mesmo formato do defeito real "
        "de 8eb7d73 - attribute_selector_holds indefinido - foi pego)"
    )
    return True


def selftest_negative_control_const_member_shape():
    # A CONST member function mangles with the "K" cv-qualifier infix
    # (_ZNK7glintfx...) - check_exports.sh's own CE-3 finding, ported
    # here so a future const glintfx:: accessor left unresolved is
    # caught the same way a free function is.
    symbol = "_ZNK7glintfx9gltfx_err4pathEv"
    if not symbol_is_glintfx_namespaced(symbol):
        print(
            f"selftest: controle NEGATIVO (membro const) FALHOU ({symbol} "
            "deveria ter sido reconhecido como glintfx::)",
            file=sys.stderr,
        )
        return False
    print("selftest: controle NEGATIVO (membro const, infixo K) OK")
    return True


def selftest_empty_scan_control():
    symbols = parse_undefined_symbol_names("")
    if len(symbols) != 0:
        print(
            f"selftest: controle de VARREDURA VAZIA FALHOU (esperava 0 "
            f"simbolos de texto vazio, veio {len(symbols)})",
            file=sys.stderr,
        )
        return False
    try:
        require_nonempty_scan(len(symbols), "teste")
    except SystemExit:
        print(
            "selftest: controle de VARREDURA VAZIA OK (require_nonempty_"
            "scan(0) reprova, GODS_LAWS.md L-40)"
        )
        return True
    print(
        "selftest: controle de VARREDURA VAZIA FALHOU (require_nonempty_"
        "scan(0) deveria ter chamado sys.exit e nao chamou)",
        file=sys.stderr,
    )
    return False


def selftest_main():
    controls = (
        selftest_positive_control,
        selftest_negative_control_real_defect_shape,
        selftest_negative_control_const_member_shape,
        selftest_empty_scan_control,
    )
    overall_ok = True
    for control in controls:
        if not control():
            overall_ok = False

    if not overall_ok:
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os quatro controles OK")


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        selftest_main()
    else:
        real_main(sys.argv[1:])


if __name__ == "__main__":
    main()
