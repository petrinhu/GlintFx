#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_env_sweep.py - CI gate for TODO.md's GATE-ENV-SWEEP ("Enumeracao
# FECHADA dos portoes que comparam contra um fato capturado numa maquina
# especifica").
#
# ORIGIN (TODO.md, GATE-ENV-SWEEP): numa noite (26->27/08/2026) a mesma
# doenca apareceu em quatro encarnacoes independentes - contagem de
# testes dependente de ferramenta opcional, contagem por nucleo, ordem
# de lista, forma da lista vazia - mais o fim de linha do checkout do
# Windows, e todas foram achadas pelo CI, uma por rodada de meia hora,
# nunca por leitura de codigo. Duas mais apareceram em 04/09/2026: o
# mesmo fim de linha, agora em check_dep_zero.py (commit 605fbd6), e um
# falso positivo de "o compilador esta disponivel?" que nao distinguia
# ferramenta ausente de conceito que nao existe naquele formato de
# binario (commit d316ed5). O revisor de 27/08 procurou uma quinta
# encarnacao e declarou honestamente que a busca dele foi DIRIGIDA, nao
# enumeracao fechada - recusou afirmar que a superficie estava fechada.
#
# O QUE ESTE PORTAO NAO E (limitacao declarada, GODS_LAWS.md L-40 "nao
# esconde o que nao faz"): isto NAO e um analisador semantico que prova
# ausencia de qualquer comparacao-contra-ambiente futura possivel - o
# proprio orquestrador desta fatia avisou que uma heuristica "esperta"
# produziria ruido que ninguem le. Este portao e, em vez disso, uma
# VARREDURA MECANICA por um vocabulario FECHADO de PRIMITIVAS que leem
# um fato da maquina em execucao (nunca do repositorio) -
# _ENV_FACT_SIGNALS abaixo, cada uma com o exato incidente que a
# motivou -, exigindo que toda ocorrencia carregue uma DECLARACAO por
# perto (comentario citando por que aquele uso e seguro, ou o que foi
# feito para neutralizar o risco). A varredura e FECHADA no sentido em
# que a LISTA DE PRIMITIVAS e enumeravel e revisavel por um humano em
# um minuto (nao um heuristica opaca) - achar uma OITAVA primitiva no
# futuro e trabalho de acrescentar UMA linha a essa lista, nomeada e
# datada, nao de reescrever o mecanismo.
#
# ONDE ISTO VARRE, DECLARADO (a ordem exata do FATO no TODO.md):
# tests/tools/, tests/container/, cmake/, tools/ci/ e .github/workflows/
# ci.yml, INTEIROS - ver SWEPT_DIRS/SWEPT_SINGLE_FILES abaixo. Extensoes
# reconhecidas: .py, .sh, .cmake, .in, .ps1, .yml (medido ao vivo contra
# a arvore real: `find tools/ci -type f`, `find cmake -type f`,
# `find tests/tools tests/container -type f`, e o unico arquivo
# .github/workflows/ci.yml - nenhuma outra extensao existe hoje sob
# essas quatro pastas).
#
# CLASSIFICACAO: "propriedade do repositorio" (um valor gravado NO
# proprio commit - uma lista curada, uma constante, um arquivo versionado
# como hostconfig_baseline.txt) nunca precisa de declaracao aqui, porque
# nao e fato do ambiente; so entra no radar deste portao quando o codigo
# em volta usa uma das _ENV_FACT_SIGNALS abaixo - "isto e um fato da
# MAQUINA que executa" (numero de nucleos, se uma ferramenta opcional
# esta instalada, ordem que docker/JSON promete, forma de
# ausencia/nulo de uma API externa, terminador de linha do checkout,
# disponibilidade real de um compilador). A DECLARACAO exigida e uma
# busca textual por um marcador que ja e convencao viva deste
# repositorio (ver _DECLARATION_MARKERS abaixo, todos observados ao vivo
# nos proprios consertos que geraram este item) numa janela de linhas
# ao redor do sinal - nunca a INEXISTENCIA do sinal (GODS_LAWS.md L-40:
# recusar alto e melhor que aprovar em silencio).
#
# NOTA HONESTA (a MESMA do proprio item do TODO.md, preservada aqui): o
# baseline do container (tests/container/hostconfig_baseline.txt) e ele
# mesmo um FATO CONGELADO capturado numa maquina - mas ali e' o
# REMEDIO, nao a DOENCA: falha fechado por desenho (uma mudanca de
# versao do Docker reprova ate ser re-medido, nunca aprova em silencio)
# e e' re-medido a cada mudanca de versao, exatamente o oposto do
# defeito que este portao existe para pegar (uma comparacao que
# SILENCIOSAMENTE muda de veredito conforme a maquina, sem ninguem
# perceber). Arquivos de baseline GRAVADOS E VERSIONADOS
# (hostconfig_baseline.txt) nao entram no vocabulario de sinais - sao a
# resposta certa, nao o problema.
#
# CALIBRACAO (exigencia do orquestrador): --selftest roda tambem
# calibrate_against_known_incidents(), que reexecuta a varredura real
# contra a ARVORE REAL do projeto (nao fixture sintetica) e confere que
# cada uma das SETE encarnacoes conhecidas (ver _KNOWN_INCIDENTS
# abaixo, uma linha por incidente, arquivo+marcador+commit) e
# encontrada pelo vocabulario de sinais E ja carrega declaracao - a
# MESMA prova que o revisor de 27/08 pediu e nao pode dar sozinho.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import os
import re
import sys
import tempfile

SCRIPT_NAME = "check_env_sweep.py"

# --- the closed signal vocabulary (GODS_LAWS.md L-40: enumeravel, nao
# heuristica) -----------------------------------------------------------
#
# Each entry: category name -> (regex, one-line origin note). The
# origin note is not read by the code - it exists so a human auditing
# this list sees WHY each primitive is here without leaving this file.
#
# TOOL_AVAILABILITY e CORE_COUNT sao NOMEADOS pela funcao-remedio real,
# nao pela primitiva generica (`shutil.which(`/`find_program(`/`nproc`
# sozinhos) - MEDIDO ao vivo contra a arvore inteira antes de decidir
# assim: a primitiva generica dispara em ONZE locais legitimos que
# apenas EXIGEM uma ferramenta e falham duro se ausente (nenhuma
# COMPARACAO contra contagem, nenhum veredito que muda goo com a
# maquina) - check_public_name_collision.py's own `shutil.which(cxx)`
# antes de compilar, check_pkgconfig_validate.py's own probes,
# GlintfxWaylandProtocols.cmake's own `find_program(WAYLAND_SCANNER)`,
# entre outros - e barrar todos eles seria exatamente o ruido que o
# orquestrador desta fatia avisou contra ("uma lista cheia de ruido,
# ninguem vai ler, vira decoracao"). A pergunta que TOOL_AVAILABILITY/
# CORE_COUNT existem para vigiar nao e "este arquivo usa uma
# ferramenta/contagem opcional", e' "o RESULTADO dessa leitura
# alimenta uma COMPARACAO contra um valor gravado em outro lugar" - e
# essa segunda pergunta so tem resposta positiva, hoje, nas DUAS
# funcoes-remedio abaixo (uma por incidente). Uma OITAVA ocorrencia
# futura, com outro nome, nao e' pega por este regex - limitacao
# declarada, nao escondida (mesma classe da limitacao de #ifdef que
# check_macro_balance.py's own header comment tambem declara).
_ENV_FACT_SIGNALS = {
    "TOOL_AVAILABILITY": (
        re.compile(r"\bcount_optional_tooling_tests\b"),
        "contagem de teste dependente de ferramenta opcional (TODO.md GATE-ENV-SWEEP, incidente 1; "
        "check_readme_test_count.py's own ENV-DRIFT/count_optional_tooling_tests())",
    ),
    "CORE_COUNT": (
        re.compile(r"\bmasked_paths_cpu_thermal_count\b|\bhost_thermal_throttle_dir_count\b"),
        "contagem por nucleo (TODO.md GATE-ENV-SWEEP, incidente 2; check_isolation.sh's own "
        "cpuN/thermal_throttle vs nproc, GODS_LAWS.md L-40/ISO-BASELINE)",
    ),
    "LIST_ORDER": (
        re.compile(r"\bsort_json_arrays\b"),
        "ordem de lista (TODO.md GATE-ENV-SWEEP, incidente 3; check_isolation.sh's own "
        "ORDER-DRIFT/sort_json_arrays(), commit 118838a)",
    ),
    "EMPTY_REPRESENTATION": (
        re.compile(r"\bcollapse_null_outside_strings\b"),
        "forma da lista vazia, null vs [] (TODO.md GATE-ENV-SWEEP, incidente 4; check_isolation.sh's "
        "own collapse_null_outside_strings(), commit d4c97c9)",
    ),
    "LINE_ENDING": (
        re.compile(r"\\r\\n|\bCRLF\b|\bautocrlf\b|_read_lines_latin1\b"),
        "fim de linha do checkout do Windows (TODO.md GATE-ENV-SWEEP, incidentes 5 e 6; "
        "check_dep_zero.py's own _read_lines_latin1(), commit 605fbd6)",
    ),
    "COMPILER_AVAILABILITY": (
        re.compile(r"\brunning_as_windows_linker\b"),
        "falso positivo de 'compilador disponivel' (TODO.md GATE-ENV-SWEEP, incidente 7; "
        "check_dep_zero.py's own running_as_windows_linker(), commit d316ed5)",
    ),
}

# A match is DECLARED when one of these markers (case-insensitive)
# appears within _DECLARATION_WINDOW lines of it - every marker below
# is vocabulary this repository's own commits ALREADY use for exactly
# this reasoning (never a new tag invented by this gate).
_DECLARATION_MARKERS = (
    "gods_laws.md l-04",
    "gods_laws.md l-40",
    "env-drift",
    "order-drift",
    "measured (",
    "fato do ambiente",
    "declared absence",
    "ausencia declarada",
    "ausência declarada",
    "propriedade do repositorio",
    "propriedade do repositório",
    "environment fact",
)

_DECLARATION_WINDOW = 50

# --- what this gate sweeps, declared (TODO.md GATE-ENV-SWEEP's own text)

SWEPT_DIRS = ("tests/tools", "tests/container", "cmake", "tools/ci")
SWEPT_SINGLE_FILES = (".github/workflows/ci.yml",)
_SWEPT_EXTENSIONS = (".py", ".sh", ".cmake", ".in", ".ps1", ".yml")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- enumeration ---------------------------------------------------------


def enumerate_swept_files(project_root):
    """Every file under SWEPT_DIRS (recursively) plus SWEPT_SINGLE_FILES,
    restricted to _SWEPT_EXTENSIONS - the extensions actually present
    under those directories today (measured live against the real
    tree, see this file's own header comment). A directory that does
    not exist is silently skipped here (os.walk over a missing path
    yields nothing) - require_nonempty_scan() below is what turns an
    overall empty result into a hard failure, never a per-directory
    silent skip.
    """
    files = []
    for rel_dir in SWEPT_DIRS:
        abs_dir = os.path.join(project_root, rel_dir)
        for dirpath, _dirnames, filenames in os.walk(abs_dir):
            for name in filenames:
                if name.endswith(_SWEPT_EXTENSIONS):
                    files.append(os.path.join(dirpath, name))
    for rel_file in SWEPT_SINGLE_FILES:
        abs_file = os.path.join(project_root, rel_file)
        if os.path.isfile(abs_file):
            files.append(abs_file)
    return sorted(files)


def read_lines(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.readlines()
    except OSError as exc:
        print(f"{SCRIPT_NAME}: {path}: open refused ({exc})", file=sys.stderr)
        return []


def has_nearby_declaration(lines, match_lineno_zero_based):
    lo = max(0, match_lineno_zero_based - _DECLARATION_WINDOW)
    hi = min(len(lines), match_lineno_zero_based + _DECLARATION_WINDOW + 1)
    window_text = "".join(lines[lo:hi]).lower()
    return any(marker in window_text for marker in _DECLARATION_MARKERS)


# --- the sweep itself ----------------------------------------------------


def sweep_file(path):
    """Returns a list of dicts, one per env-fact signal match in this
    file: {file, line, category, declared}. Every match is reported
    regardless of declared/undeclared - the caller decides what counts
    as a failure (GODS_LAWS.md L-40: the count is never hidden, even
    the ones that pass).
    """
    lines = read_lines(path)
    hits = []
    for category, (pattern, _origin) in _ENV_FACT_SIGNALS.items():
        for idx, line in enumerate(lines):
            if pattern.search(line):
                hits.append({
                    "file": path,
                    "line": idx + 1,
                    "category": category,
                    "declared": has_nearby_declaration(lines, idx),
                })
    return hits


def _propagate_declaration_within_file(hits):
    """A rich declaration comment sits ONCE, at a signal's OWN
    definition (`def _read_lines_latin1(...)`, `sort_json_arrays()
    {`), not repeated at every CALL site that reuses the same name
    elsewhere in the file - measured live against
    tests/tools/check_dep_zero.py, whose _read_lines_latin1() is
    called from two functions ~90 lines apart, only one of which sits
    inside the declaration's own window. Treat declared/undeclared as
    a property of the (file, category) PAIR, not of each individual
    line: if ANY occurrence of a category's signal in a file carries a
    nearby declaration, every occurrence of that SAME category in that
    SAME file is considered declared - the risk was named once, by
    name, and every reuse of that name inherits the reasoning. This
    still requires the declaration to sit near a REAL occurrence of
    the primitive (never "anywhere in the file, unrelated") - it only
    relaxes "near every call site" to "near at least one".
    """
    declared_pairs = {(h["file"], h["category"]) for h in hits if h["declared"]}
    for h in hits:
        if (h["file"], h["category"]) in declared_pairs:
            h["declared"] = True
    return hits


def sweep_all(project_root):
    files = enumerate_swept_files(project_root)
    all_hits = []
    for path in files:
        all_hits.extend(sweep_file(path))
    _propagate_declaration_within_file(all_hits)
    return files, all_hits


# --- L-40 floor ------------------------------------------------------


def require_nonempty_scan(value, what):
    if not value:
        print(f"{SCRIPT_NAME}: varredura vazia ({what})", file=sys.stderr)
        return False
    return True


# --- the check itself --------------------------------------------------


def check_env_sweep(project_root):
    files, hits = sweep_all(project_root)
    if not require_nonempty_scan(
        files, f"0 arquivo(s) varrido(s) sob {', '.join(SWEPT_DIRS)} + {', '.join(SWEPT_SINGLE_FILES)}"
    ):
        return False

    undeclared = [h for h in hits if not h["declared"]]
    declared = [h for h in hits if h["declared"]]

    by_category = {}
    for h in hits:
        by_category.setdefault(h["category"], 0)
        by_category[h["category"]] += 1
    category_report = ", ".join(f"{cat}={n}" for cat, n in sorted(by_category.items()))

    if undeclared:
        print(
            f"{SCRIPT_NAME}: {len(undeclared)} comparacao(oes) contra FATO DO AMBIENTE sem "
            "declaracao por perto (TODO.md GATE-ENV-SWEEP):",
            file=sys.stderr,
        )
        for h in undeclared:
            print(f"  {h['file']}:{h['line']}:{h['category']}", file=sys.stderr)
        return False

    print(
        f"{SCRIPT_NAME}: {len(files)} arquivo(s) varrido(s), {len(hits)} sinal(is) de fato-do-ambiente "
        f"encontrado(s) ({category_report or 'nenhuma categoria'}), {len(declared)} declarado(s), "
        f"0 sem declaracao"
    )
    return True


# --- real mode -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_env_sweep.py <project_root>")
    (project_root,) = args
    if not os.path.isdir(project_root):
        fail(f"project root not found: {project_root}")
    if not check_env_sweep(project_root):
        fail("comparacao contra fato do ambiente sem declaracao encontrada (ver mensagem acima)")


# --- calibration against the seven known incidents ------------------------
#
# One row per incident this item was born to close - see this file's
# own header comment for the full narrative. `marker` is text that
# MUST be found within _DECLARATION_WINDOW lines of the FIRST signal
# match in `file` for `category` - proving both that the signal
# vocabulary above actually fires on the real, historical location,
# and that the declaration search finds the real, historical comment.
_KNOWN_INCIDENTS = (
    {
        "n": 1,
        "what": "contagem de teste dependente de ferramenta opcional",
        "file": "tests/tools/check_readme_test_count.py",
        "category": "TOOL_AVAILABILITY",
        "marker": "env-drift",
    },
    {
        "n": 2,
        "what": "contagem por nucleo",
        "file": "tests/container/check_isolation.sh",
        "category": "CORE_COUNT",
        "marker": "gods_laws.md l-40",
    },
    {
        "n": 3,
        "what": "ordem de lista",
        "file": "tests/container/check_isolation.sh",
        "category": "LIST_ORDER",
        "marker": "order-drift",
    },
    {
        "n": 4,
        "what": "forma da lista vazia (null vs [])",
        "file": "tests/container/check_isolation.sh",
        "category": "EMPTY_REPRESENTATION",
        "marker": "gods_laws.md l-40",
    },
    {
        "n": 5,
        "what": "fim de linha do checkout do Windows (noite de 26-27/08, mesma classe do incidente 6)",
        "file": "tests/tools/check_dep_zero.py",
        "category": "LINE_ENDING",
        "marker": "measured (",
    },
    {
        "n": 6,
        "what": "fim de linha no portao de dependencia zero (04/09, commit 605fbd6)",
        "file": "tests/tools/check_dep_zero.py",
        "category": "LINE_ENDING",
        "marker": "gods_laws.md l-04",
    },
    {
        "n": 7,
        "what": "falso positivo de compilador disponivel (04/09, commit d316ed5)",
        "file": "tests/tools/check_dep_zero.py",
        "category": "COMPILER_AVAILABILITY",
        "marker": "gods_laws.md l-04",
    },
)


def _as_posix(path):
    """`_KNOWN_INCIDENTS[*]['file']` is written once, with '/' (it is
    prose in this file, not a filesystem call) - `enumerate_swept_files()`
    builds `h["file"]` with `os.path.join()`/`os.walk()`, which on
    Windows yields '\\'-separated paths. `"...\\tests\\tools\\x.py".
    endswith("tests/tools/x.py")` is False on EVERY incident (all seven
    known-incident paths cross a directory), which is exactly why
    --selftest found ZERO of seven on the Windows runners (GATE-ENV-
    SWEEP calibration, measured 04/09/2026: forcing '/' -> '\\' on the
    real hits reproduces 0/7 candidates for every incident on Linux
    too). Comparing on '/' only, here, fixes the match without
    touching how `h["file"]` is built or printed anywhere else.
    """
    return path.replace(os.sep, "/") if os.sep != "/" else path


def calibrate_against_known_incidents(project_root):
    """Reruns the REAL sweep against the REAL tree (never a fixture)
    and checks, for each of the seven known incidents, that the
    matching category fires in the expected file AND that the
    declaration window around the FIRST such match contains the
    expected marker text. Returns (found_count, total_count, per-
    incident detail) - never hides which ones were NOT found, exactly
    what the calibration exists to prove (TODO.md: "reencontrar as
    sete encarnacoes conhecidas... e a calibracao").
    """
    _files, hits = sweep_all(project_root)
    results = []
    for incident in _KNOWN_INCIDENTS:
        candidates = [
            h for h in hits
            if h["category"] == incident["category"] and _as_posix(h["file"]).endswith(incident["file"])
        ]
        if not candidates:
            results.append({**incident, "found": False, "reason": "categoria nao disparou naquele arquivo"})
            continue
        abs_file = os.path.join(project_root, incident["file"])
        lines = read_lines(abs_file)
        first = min(candidates, key=lambda h: h["line"])
        lo = max(0, first["line"] - 1 - _DECLARATION_WINDOW)
        hi = min(len(lines), first["line"] - 1 + _DECLARATION_WINDOW + 1)
        window_text = "".join(lines[lo:hi]).lower()
        if incident["marker"] not in window_text:
            results.append({
                **incident, "found": False,
                "reason": f"disparou em {first['file']}:{first['line']}, mas marcador '{incident['marker']}' nao esta na janela",
            })
            continue
        results.append({**incident, "found": True, "line": first["line"]})
    return results


# --- selftest fixtures and controls -------------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-env-sweep-selftest-", dir=os.environ.get("TMPDIR"))


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def _make_fixture_root(scratch, label):
    root = os.path.join(scratch, label)
    os.makedirs(os.path.join(root, "tests", "tools"), exist_ok=True)
    return root


# Positive control: a signal WITH a declaration nearby. Expected:
# passes, 1 signal found, 0 undeclared.
def selftest_positive_control(scratch):
    root = _make_fixture_root(scratch, "positive")
    _write(
        os.path.join(root, "tests", "tools", "check_widget.py"),
        "# ENV-DRIFT (27/08/2026, GODS_LAWS.md L-40): widget count depends on\n"
        "# an optional tool - declared and handled, same shape\n"
        "# check_readme_test_count.py's own count_optional_tooling_tests()\n"
        "# uses.\n"
        "import shutil\n"
        "if shutil.which('clang-format') is not None:\n"
        "    pass\n",
    )
    _files, hits = sweep_all(root)
    if not hits:
        print("selftest: controle POSITIVO FALHOU (esperava 1 sinal, achou 0)", file=sys.stderr)
        return False
    if any(not h["declared"] for h in hits):
        print(f"selftest: controle POSITIVO FALHOU (esperava todos declarados, achou {hits})", file=sys.stderr)
        return False
    print(f"selftest: controle POSITIVO OK ({len(hits)} sinal(is), todos declarados)")
    return True


# Negative control: the EXACT defect class this gate exists to catch -
# a core-count comparison with NO declaration anywhere nearby.
# Expected: check_env_sweep() reproves, citing file/line/category.
def selftest_negative_control(scratch):
    root = _make_fixture_root(scratch, "negative")
    _write(
        os.path.join(root, "tests", "tools", "check_widget.py"),
        "def masked_paths_cpu_thermal_count():\n"
        "    expected_widgets = 4\n"
        "    if _measure() != expected_widgets:\n"
        "        raise SystemExit(1)\n",
    )
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        result = check_env_sweep(root)
    text = buffer.getvalue()
    if result:
        print("selftest: controle NEGATIVO FALHOU (esperava reprovar comparacao de nucleo sem declaracao, passou)", file=sys.stderr)
        return False
    if "CORE_COUNT" not in text:
        print(f"selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou CORE_COUNT): {text}", file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (comparacao por nucleo sem declaracao reprovada, citando categoria)")
    return True


# Empty-scan floor: a root with none of the swept directories/files
# present must be REFUSED, never presumed clean.
def selftest_empty_scan_control(scratch):
    root = os.path.join(scratch, "empty_scan")
    os.makedirs(root, exist_ok=True)

    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        result = check_env_sweep(root)
    text = buffer.getvalue()
    if result:
        print("selftest: controle de VARREDURA VAZIA FALHOU (deveria recusar raiz sem nada varrivel, mas passou)", file=sys.stderr)
        return False
    if "varredura vazia" not in text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (recusou, mas nao disse 'varredura vazia')", file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (raiz sem nada varrivel recusada)")
    return True


# Inverse-of-the-obvious control: a file with ZERO env-fact signals
# (a plain gate that never touches machine facts) must pass cleanly,
# never be mistaken for an empty scan or a violation.
def selftest_file_without_signals_control(scratch):
    root = _make_fixture_root(scratch, "no_signals")
    _write(
        os.path.join(root, "tests", "tools", "check_plain.py"),
        "def check_plain():\n    return True\n",
    )
    _files, hits = sweep_all(root)
    if hits:
        print(f"selftest: controle SEM SINAL FALHOU (esperava 0 sinais, achou {hits})", file=sys.stderr)
        return False
    print("selftest: controle SEM SINAL OK (arquivo sem nenhum fato-do-ambiente nao gera sinal)")
    return True


# Baseline-file exemption, by construction (this file's own header
# comment, "Nota honesta"): a JSON/text baseline recorded and versioned
# (the same shape hostconfig_baseline.txt has) never matches any of the
# _ENV_FACT_SIGNALS regexes on its own - it is DATA, not code that
# reads a machine fact. Proven here so the exemption is not just prose.
def selftest_baseline_file_is_not_a_signal_control(scratch):
    root = _make_fixture_root(scratch, "baseline_exempt")
    os.makedirs(os.path.join(root, "tests", "container"), exist_ok=True)
    _write(
        os.path.join(root, "tests", "container", "hostconfig_baseline.txt"),
        '{"Privileged": false, "MaskedPaths": ["/proc/scsi", "/proc/sched_debug"]}\n',
    )
    _files, hits = sweep_all(root)
    if hits:
        print(f"selftest: controle de BASELINE FALHOU (esperava 0 sinais num arquivo de baseline, achou {hits})", file=sys.stderr)
        return False
    print("selftest: controle de BASELINE OK (arquivo de baseline gravado nao e' confundido com fato do ambiente)")
    return True


def selftest_calibration_control(project_root):
    """Runs calibrate_against_known_incidents() against the REAL
    project tree (the one --selftest was invoked from, resolved the
    same way real_main() would) and requires ALL SEVEN to be found -
    the calibration TODO.md's own GATE-ENV-SWEEP item demands.
    """
    results = calibrate_against_known_incidents(project_root)
    found = [r for r in results if r["found"]]
    missing = [r for r in results if not r["found"]]
    for r in results:
        status = "OK" if r["found"] else f"NAO ACHADO ({r.get('reason', '?')})"
        print(f"selftest: calibracao incidente {r['n']} ({r['what']}): {status}")
    if missing:
        print(
            f"selftest: controle de CALIBRACAO FALHOU ({len(found)}/{len(results)} das sete encarnacoes "
            f"conhecidas reencontradas - faltam: {[m['n'] for m in missing]})",
            file=sys.stderr,
        )
        return False
    print(f"selftest: controle de CALIBRACAO OK ({len(found)}/{len(results)} encarnacoes conhecidas reencontradas)")
    return True


def _project_root_from_this_file():
    # tests/tools/check_env_sweep.py -> two parents up is the project root.
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def _run_all_controls(scratch):
    return [
        selftest_positive_control(scratch),
        selftest_negative_control(scratch),
        selftest_empty_scan_control(scratch),
        selftest_file_without_signals_control(scratch),
        selftest_baseline_file_is_not_a_signal_control(scratch),
        selftest_calibration_control(_project_root_from_this_file()),
    ]


def selftest_main():
    scratch = make_scratch_workdir()
    try:
        controls = _run_all_controls(scratch)
        if not all(controls):
            print("check_env_sweep.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_env_sweep.py --selftest: os {len(controls)} controles OK")
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
