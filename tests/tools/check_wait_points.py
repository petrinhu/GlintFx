#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_wait_points.py - CI gate for GODS_LAWS.md L-17/L-36/L-40
# (INBOX, drenagem 06/09/2026): "a promessa publica de que o desenho e
# o bombeamento de eventos nunca prendem o aplicativo" foi furada duas
# vezes por um poll() sem teto (72754af consertou o primeiro sitio;
# display_adapter.cpp::flush_with_retry() era o gemeo, vivo ate este
# commit) - as duas vezes porque a auditoria anterior olhou so' o ponto
# ja conhecido em vez de enumerar o espaco inteiro. Este script e' o
# portao que faz essa enumeracao NUNCA MAIS ser feita de cabeca:
#
#   1. varre os arquivos-alvo (TARGET_FILES) atras das AGULHAS
#      (NEEDLES) - toda chamada que este projeto ja aprendeu (na
#      pratica ou por busca, docs/plano-w6b-fatias-6-8.md sec. 1) que
#      PODE bloquear a thread que a chama;
#   2. confere cada sitio encontrado contra tests/wait_points.txt (o
#      manifesto, formato descrito no cabecalho dele) - um sitio sem
#      linha no manifesto reprova, e uma linha do manifesto que nao
#      aparece mais na arvore tambem reprova (manifesto podre);
#   3. reprova quando existe sitio classificado como espera sem teto
#      que NAO seja `sem-teto-fora-do-caminho` nem `sem-teto-
#      declarado` - as duas unicas formas que este projeto aceita para
#      uma espera sem orcamento (fora do caminho de quadro, ou dentro
#      dele mas medida e nomeada);
#   4. imprime a contagem por classe SEMPRE, mesmo zero (GODS_LAWS.md
#      L-40 - "zero" e' sinal de varredura quebrada, nunca de "nada a
#      relatar" quando a varredura em si e' a promessa deste portao).
#
# Comentarios ("//") sao descartados de cada linha ANTES da comparacao
# com as agulhas - as proprias agulhas aparecem dentro de comentarios
# deste arquivo-fonte e dos quatro adaptadores que ele varre (citando o
# defeito, explicando o conserto), e um portao que casasse com
# comentario reprovaria a si mesmo.
#
# Usage:
#   check_wait_points.py <repo-root-directory>
#   check_wait_points.py --selftest

import os
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_wait_points.py"

MANIFEST_RELATIVE_PATH = os.path.join("tests", "wait_points.txt")

# Os quatro adaptadores que hoje tocam o sistema operacional para
# abrir/bombear a conexao e apresentar um quadro, mais o atomo
# compartilhado que a fatia desta drenagem extraiu (bounded_output_
# wait.cpp - ver o cabecalho dele: a duplicacao da MESMA espera em dois
# arquivos e' o que deixou o gemeo do defeito original vivo). src/
# platform/loop/ (LOOP-RUN, docs/plano-w6b-fatias-6-8.md) ainda nao
# existe nesta arvore - quando nascer, entra aqui como arquivo novo,
# nunca antecipado (tests/wait_points.txt's own header comment repete
# a mesma regra do lado do manifesto).
# Forma canonica com "/" - a MESMA forma que tests/wait_points.txt usa
# no campo `arquivo` (o manifesto e' um arquivo de texto versionado,
# nunca escreve "\"). os.path.join() aqui produziria "\" no Windows e
# quebraria toda comparacao contra o manifesto (CI run 34124767196,
# job "Windows" - "sitios encontrados sem linha no manifesto" para um
# sitio que TINHA linha, so' que escrita com a barra errada): relpath
# fica posix-puro do inicio ao fim deste script (chave de dict,
# mensagem de erro, comparacao com o manifesto) e so' vira caminho
# nativo no unico lugar que toca o sistema de arquivos (scan_file(),
# abaixo).
TARGET_FILES = (
    "src/platform/wayland/display_adapter.cpp",
    "src/platform/wayland/egl_context_adapter.cpp",
    "src/platform/wayland/bounded_output_wait.cpp",
    "src/platform/win32/display_adapter.cpp",
    "src/platform/win32/wgl_context_adapter.cpp",
)

# docs/plano-w6b-fatias-6-8.md, D-W6b-58 - a lista fixada ANTES do dado
# (GODS_LAWS.md L-43): toda chamada que este projeto ja sabe que PODE
# bloquear. wl_display_dispatch_pending NAO conta (ARMADILHA 1's own
# manpage-blessed drain, nunca bloqueante por construcao) - tratado a
# parte, abaixo, porque e' um PREFIXO de wl_display_dispatch.
_DISPATCH_NEEDLE = "wl_display_dispatch"
_DISPATCH_PENDING_SUFFIX = "_pending"

SIMPLE_NEEDLES = (
    "poll(",
    "ppoll(",
    "select(",
    "wl_display_roundtrip",
    "wl_display_prepare_read",
    "wl_display_flush",
    "wl_display_read_events",
    "eglSwapBuffers",
    "eglWaitClient",
    "eglWaitNative",
    "glFinish",
    "glClientWaitSync",
    "glWaitSync",
    "MsgWaitForMultipleObjects",
    "WaitForSingleObject",
    "WaitForMultipleObjects",
    "GetMessage",
    "WaitMessage",
    "Sleep(",
    "SleepEx",
    "SendMessage",
    "::SwapBuffers",
    "DwmFlush",
    "std::this_thread::sleep",
    "wait_for(",
    "wait_until(",
    "gltfx_now(",
)

# As duas unicas formas de "sem teto" que este projeto aceita (tests/
# wait_points.txt's own header comment tem a definicao completa de
# cada uma das cinco).
VALID_CLASSES = frozenset(
    {
        "teto-nosso",
        "teto-do-sistema",
        "sem-teto-fora-do-caminho",
        "sem-teto-declarado",
        "nao-espera",
    }
)
ACCEPTED_UNBOUNDED_CLASSES = frozenset({"sem-teto-fora-do-caminho", "sem-teto-declarado"})


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def strip_comment(line):
    # So' "//" - a mesma convencao exclusiva deste projeto inteiro
    # (medido: nenhum dos cinco arquivos-alvo usa /* */ span alem de
    # comentarios de nome de parametro fechados na MESMA linha, que
    # nunca carregam nenhuma agulha desta lista).
    return line.split("//", 1)[0]


def line_wait_sites(code):
    """Every needle that matches this (comment-stripped) line of code."""
    found = []
    for needle in SIMPLE_NEEDLES:
        if needle in code:
            found.append(needle)
    idx = 0
    while True:
        idx = code.find(_DISPATCH_NEEDLE, idx)
        if idx == -1:
            break
        after = code[idx + len(_DISPATCH_NEEDLE) : idx + len(_DISPATCH_NEEDLE) + len(_DISPATCH_PENDING_SUFFIX)]
        if after != _DISPATCH_PENDING_SUFFIX:
            found.append(_DISPATCH_NEEDLE)
        idx += len(_DISPATCH_NEEDLE)
    return found


def scan_file(root, relpath):
    """Returns (sites, code_lines) - sites is [(lineno, code)], one
    entry per LINE that matched at least one needle (not one per
    needle - a line with two needles is one site, same as a human
    reading it would count it); code_lines is every comment-stripped
    line, kept for the manifest staleness check below (a manifest row
    can cite a line that itself carries no needle - GODS_LAWS.md L-43's
    own honesty: the classification text lives on the SAME line as the
    call, and that line already matched by construction, but a future
    row citing a helper's own doc line should still resolve).
    """
    # relpath chega sempre com "/" (TARGET_FILES's own comment) - so'
    # aqui, no unico ponto que abre o arquivo de verdade, ele vira
    # separador nativo.
    path = os.path.join(root, *relpath.split("/"))
    if not os.path.isfile(path):
        fail(f"arquivo-alvo nao encontrado: {relpath} (TARGET_FILES esta desatualizada?)")
    sites = []
    code_lines = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for lineno, raw_line in enumerate(handle, start=1):
            code = strip_comment(raw_line)
            code_lines.append(code)
            if line_wait_sites(code):
                sites.append((lineno, code))
    return sites, code_lines


def parse_manifest(manifest_path):
    rows = []
    with open(manifest_path, "r", encoding="utf-8", errors="replace") as handle:
        for lineno, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            fields = [field.strip() for field in line.split("|")]
            if len(fields) != 4:
                fail(f"{manifest_path}:{lineno}: esperava 4 campos separados por '|', achei {len(fields)}")
            file_field, function_field, snippet_field, class_field = fields
            if class_field not in VALID_CLASSES:
                fail(f"{manifest_path}:{lineno}: classe desconhecida '{class_field}'")
            if not snippet_field:
                fail(f"{manifest_path}:{lineno}: trecho literal vazio")
            rows.append(
                {
                    "file": file_field,
                    "function": function_field,
                    "snippet": snippet_field,
                    "class": class_field,
                    "lineno": lineno,
                }
            )
    return rows


def check_wait_points(root, target_files=None, manifest_path=None):
    target_files = TARGET_FILES if target_files is None else target_files
    manifest_path = (
        os.path.join(root, MANIFEST_RELATIVE_PATH) if manifest_path is None else manifest_path
    )

    if not os.path.isfile(manifest_path):
        fail(f"manifesto nao encontrado: {manifest_path}")
    rows = parse_manifest(manifest_path)

    print(f"{SCRIPT_NAME}: agulhas = {', '.join(SIMPLE_NEEDLES + (_DISPATCH_NEEDLE,))}")

    per_file_sites = {}
    per_file_code = {}
    for relpath in target_files:
        sites, code_lines = scan_file(root, relpath)
        per_file_sites[relpath] = sites
        per_file_code[relpath] = code_lines

    counts = {klass: 0 for klass in VALID_CLASSES}
    undeclared = []
    ambiguous = []

    for relpath, sites in per_file_sites.items():
        for lineno, code in sites:
            candidates = [
                row
                for row in rows
                if row["file"] == relpath and row["snippet"] in code
            ]
            if not candidates:
                undeclared.append((relpath, lineno, code.strip()))
                continue
            classes_here = {row["class"] for row in candidates}
            if len(classes_here) > 1:
                ambiguous.append((relpath, lineno, code.strip(), sorted(classes_here)))
                continue
            counts[next(iter(classes_here))] += 1

    stale_rows = []
    for row in rows:
        code_lines = per_file_code.get(row["file"])
        if code_lines is None:
            stale_rows.append(row)
            continue
        if not any(row["snippet"] in code for code in code_lines):
            stale_rows.append(row)

    total_found = sum(counts.values()) + len(undeclared) + len(ambiguous)

    ok = True

    if total_found == 0:
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 sitios em {len(target_files)} arquivos-alvo) - "
            "GODS_LAWS.md L-40",
            file=sys.stderr,
        )
        ok = False

    if undeclared:
        print(f"{SCRIPT_NAME}: sitios encontrados sem linha no manifesto:", file=sys.stderr)
        for relpath, lineno, code in undeclared:
            print(f"  {relpath}:{lineno}: {code}", file=sys.stderr)
        ok = False

    if ambiguous:
        print(f"{SCRIPT_NAME}: sitios com classes conflitantes no manifesto:", file=sys.stderr)
        for relpath, lineno, code, classes_here in ambiguous:
            print(f"  {relpath}:{lineno}: {code} -> {classes_here}", file=sys.stderr)
        ok = False

    if stale_rows:
        print(f"{SCRIPT_NAME}: manifesto podre (linha nao encontrada na arvore):", file=sys.stderr)
        for row in stale_rows:
            print(
                f"  {manifest_path}:{row['lineno']}: {row['file']} | {row['snippet']}",
                file=sys.stderr,
            )
        ok = False

    # A quinta classe ("sem-teto" puro, sem a qualificacao fora-do-
    # caminho/declarado) e' proibida por construcao: VALID_CLASSES nem
    # admite esse texto, entao o unico jeito de um sitio sem teto
    # escapar das duas formas aceitas e' ele nao ter linha de manifesto
    # nenhuma - ja' coberto por `undeclared` acima. Contado a parte,
    # sempre impresso, para casar com o nome de campo que o plano
    # fixou (docs/plano-w6b-fatias-6-8.md, D-W6b-58).
    sem_teto_nao_declarado = len(undeclared)

    print(
        "wait_points: encontrados={total} nao_espera={nao_espera} teto_nosso={teto_nosso} "
        "teto_do_sistema={teto_do_sistema} sem_teto_fora_do_caminho={fora} "
        "sem_teto_declarado={declarado} sem_teto_nao_declarado={nao_declarado}".format(
            total=total_found,
            nao_espera=counts["nao-espera"],
            teto_nosso=counts["teto-nosso"],
            teto_do_sistema=counts["teto-do-sistema"],
            fora=counts["sem-teto-fora-do-caminho"],
            declarado=counts["sem-teto-declarado"],
            nao_declarado=sem_teto_nao_declarado,
        )
    )

    return ok


# --- real mode ---------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_wait_points.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    if not check_wait_points(root):
        fail("wait-point violation found (ver mensagens acima)")


# --- fixtures and controls for --selftest ------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-wait-points-selftest-", dir=os.environ.get("TMPDIR"))


def write_file(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(content)


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


_FIXTURE_TARGET = os.path.join("src", "adapter.cpp")


def _fixture_root(scratch, name):
    return os.path.join(scratch, name)


def _write_fixture_manifest(root, rows_text):
    write_file(os.path.join(root, MANIFEST_RELATIVE_PATH), rows_text)


def _run_fixture(root, capture):
    return capture(lambda: check_wait_points(root, target_files=(_FIXTURE_TARGET,)))


# Positive control: one declared, bounded site. Expected: passes, with
# encontrados=1.
def selftest_positive_control(scratch, capture):
    root = _fixture_root(scratch, "positive")
    write_file(
        os.path.join(root, _FIXTURE_TARGET),
        "void f() {\n    poll(&pfd, 1, 100);\n}\n",
    )
    _write_fixture_manifest(
        root,
        f"{_FIXTURE_TARGET}|f()|poll(&pfd, 1, 100)|teto-nosso\n",
    )
    outcome = _run_fixture(root, capture)
    if not outcome.result or "encontrados=1" not in outcome.text:
        print("selftest: controle POSITIVO FALHOU", file=sys.stderr)
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (sitio declarado, teto-nosso, aprovado)")
    return True


# Negative control 1 (o proprio defeito desta drenagem, reproduzido em
# fixture): poll(..., -1) sem NENHUMA linha no manifesto. Expected:
# reprova, cita o arquivo:linha.
def selftest_negative_control_undeclared(scratch, capture):
    root = _fixture_root(scratch, "negative_undeclared")
    write_file(
        os.path.join(root, _FIXTURE_TARGET),
        "void f() {\n    poll(&pfd, 1, -1);\n}\n",
    )
    _write_fixture_manifest(root, "# manifesto vazio de proposito\n")
    outcome = _run_fixture(root, capture)
    if outcome.result:
        print(
            "selftest: controle NEGATIVO (sem declaracao) FALHOU (sitio sem manifesto passou)",
            file=sys.stderr,
        )
        return False
    if f"{_FIXTURE_TARGET}:2" not in outcome.text:
        print(
            "selftest: controle NEGATIVO (sem declaracao) FALHOU "
            f"(reprovou, mas nao citou {_FIXTURE_TARGET}:2)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO (sem declaracao) OK (sitio sem manifesto pego e citado)")
    return True


# Negative control 2: manifesto cita uma linha que nao existe mais na
# arvore (manifesto podre). Expected: reprova.
def selftest_negative_control_stale_manifest(scratch, capture):
    root = _fixture_root(scratch, "negative_stale")
    write_file(
        os.path.join(root, _FIXTURE_TARGET),
        "void f() {\n    poll(&pfd, 1, 100);\n}\n",
    )
    _write_fixture_manifest(
        root,
        f"{_FIXTURE_TARGET}|f()|poll(&pfd, 1, 100)|teto-nosso\n"
        f"{_FIXTURE_TARGET}|g()|poll(&outro_pfd, 1, -1)|sem-teto-declarado\n",
    )
    outcome = _run_fixture(root, capture)
    if outcome.result:
        print(
            "selftest: controle NEGATIVO (manifesto podre) FALHOU (linha fantasma passou)",
            file=sys.stderr,
        )
        return False
    if "manifesto podre" not in outcome.text:
        print(
            "selftest: controle NEGATIVO (manifesto podre) FALHOU (nao mencionou 'manifesto podre')",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO (manifesto podre) OK (linha fantasma pega)")
    return True


# Empty-scan floor: nenhum arquivo-alvo tem sitio nenhum. Expected:
# reprova com "varredura vazia".
def selftest_empty_scan_control(scratch, capture):
    root = _fixture_root(scratch, "empty")
    write_file(os.path.join(root, _FIXTURE_TARGET), "void f() {}\n")
    _write_fixture_manifest(root, "# manifesto vazio de proposito\n")
    outcome = _run_fixture(root, capture)
    if outcome.result:
        print(
            "selftest: controle de VARREDURA VAZIA FALHOU (deveria ter sido recusada)",
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
    print("selftest: controle de VARREDURA VAZIA OK (arvore sem nenhum sitio recusada)")
    return True


# Comment control: a agulha so' aparece dentro de um comentario "//" -
# nao e' sitio nenhum. Expected: passa como varredura vazia (reprova
# pelo piso do L-40, nao pelo mecanismo de comentario em si - o que
# este controle prova e' que o COMENTARIO nao contou como sitio;
# reprovar por "varredura vazia" e' o resultado honesto de um arquivo
# que so' MENCIONA poll() em prosa).
def selftest_comment_is_not_a_site_control(scratch, capture):
    root = _fixture_root(scratch, "comment_only")
    write_file(
        os.path.join(root, _FIXTURE_TARGET),
        "// isto so fala de poll(&pfd, 1, -1) em prosa, nunca chama\nvoid f() {}\n",
    )
    _write_fixture_manifest(root, "# manifesto vazio de proposito\n")
    outcome = _run_fixture(root, capture)
    if outcome.result or "varredura vazia" not in outcome.text:
        print(
            "selftest: controle de COMENTARIO FALHOU (agulha em comentario deveria ter sido "
            "ignorada, sobrando varredura vazia)",
            file=sys.stderr,
        )
        print(outcome.text, file=sys.stderr)
        return False
    print("selftest: controle de COMENTARIO OK (agulha dentro de '//' nao contou como sitio)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control_undeclared(scratch, capture),
            selftest_negative_control_stale_manifest(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_comment_is_not_a_site_control(scratch, capture),
        ]
        if not all(controls):
            print("check_wait_points.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_wait_points.py --selftest: os {len(controls)} controles OK")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args == ["--selftest"]:
        selftest_main()
        return
    real_main(args)


if __name__ == "__main__":
    main()
