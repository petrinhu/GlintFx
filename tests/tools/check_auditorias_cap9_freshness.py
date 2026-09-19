#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_auditorias_cap9_freshness.py - AUDCAP9-FRESHNESS-GAP
# (GODS_LAWS.md L-36/L-40, TODO.md INBOX, achado do inventario de
# exercicio da onda W3, 19/09/2026).
#
# O QUE DECAIU ANTES, E JA DE NOVO: `DOC-AUDCAP9` corrigiu texto
# obsoleto no capitulo 9 de AUDITORIAS.md ("Portoes automaticos de
# qualidade: estado hoje") uma unica vez, a mao, em 28/08/2026 - e
# nenhum mecanismo ficou vigiando aquele capitulo depois disso. Os dois
# portoes de frescor que ja existiam neste projeto (check_readme_
# volatile_numbers.py, check_claude_md_state_numbers.py) vigiam README.md
# e CLAUDE.md; nenhum dos dois olha para AUDITORIAS.md.
#
# ESCOLHA DE DESENHO (por que NAO e' o mesmo "zero digito fora de
# isencao" dos dois gates irmaos): o capitulo 9 de AUDITORIAS.md nao se
# declara, no proprio texto, "sem contador" (ao contrario da secao
# "Estado atual do repositorio" do CLAUDE.md) - ele CITA numeros o tempo
# todo, legitimamente ("capitulo 2 e 3", "nove capitulos", "quatro
# portoes"). Escanear digito cru ali produziria dezenas de falso
# positivo. O que de fato apodrece no capitulo 9 - medido ao escrever
# este gate, nao adivinhado - sao TRES fatos concretos e mecanicos:
#
#   (A) toda lista de nomes de job entre crases que aparece junto da
#       palavra "job"/"jobs" (ex.: "jobs `linux`, `windows` e `clang`",
#       "Job `sanitizer`", "citados acima (`linux`, ..., `gitleaks`)")
#       precisa continuar sendo um job de verdade em
#       .github/workflows/ci.yml - job renomeado/removido faz a citacao
#       mentir sem que nenhum digito mude.
#   (B) a contagem por extenso que precede "jobs ... citados acima
#       (...)" ("os seis jobs ... citados acima (`linux`, ...,
#       `gitleaks`)") precisa bater com o tamanho da propria lista que a
#       segue - o mesmo defeito que ja aconteceu duas vezes neste
#       projeto com contagem escrita (README, e o "oito nomes" que este
#       mesmo capitulo ja citou para tests/tools/ antes de virar "meca,
#       nao leia a lista").
#   (C) cada frase "confira com `<comando>`" do capitulo e' uma PROMESSA
#       de verificacao que o proprio capitulo se compromete a honrar -
#       este gate EXECUTA essa promessa (instrumentar, nao adivinhar:
#       ver a memoria feedback_instrumentar_em_vez_de_adivinhar.md desta
#       maquina) contra a arvore real, em vez de confiar que ela ainda
#       bate.
#
# ARMADILHA MEDIDA AO ESCREVER ESTE GATE (GODS_LAWS.md L-41): as tres
# ocorrencias reais de "confira com `...`" no capitulo QUEBRAM a linha
# do markdown NO MEIO do comando entre crases (ex.: "confira com `grep
# -n 'check_spdx\|check_vendor_purity'\ntests/CMakeLists.txt`, que nao
# retorna nada" - o comando continua na linha seguinte). Um regex sem
# normalizar espaco em branco perde esses tres casos silenciosamente.
# Por isso toda extracao abaixo opera sobre o texto do capitulo com
# `re.sub(r'\s+', ' ', ...)` primeiro.
#
# (C) NAO chama o binario `grep`: este gate roda via `ctest`, sem guarda
# de sistema (ver o registro em tests/CMakeLists.txt), inclusive no job
# `windows` - depender de `grep` no PATH do Windows seria uma segunda
# aposta de portabilidade por cima da primeira. Em vez disso, o proprio
# script LE o arquivo alvo e aplica a MESMA semantica de
# `grep -n [--] 'A\|B\|C' arquivo` (substring literal, alternancia por
# `\|`) em Python puro - mesmo resultado, sem depender de binario
# externo.
#
# Usage:
#   check_auditorias_cap9_freshness.py <auditorias-md-path>
#   check_auditorias_cap9_freshness.py --selftest

import re
import shlex
import sys
from pathlib import Path

SCRIPT_NAME = "check_auditorias_cap9_freshness.py"

CHAPTER_HEADING_RE = re.compile(r"^## 9\.", re.MULTILINE)
NEXT_HEADING_RE = re.compile(r"\n## 10\.")

CI_YML_RELATIVE_PATH = ".github/workflows/ci.yml"

# Vocabulario fechado de numeral por extenso em portugues, so' o que
# basta para contagens pequenas de job (nunca vai passar de uma duzia).
NUM_WORDS = {
    "um": 1, "uma": 1, "dois": 2, "duas": 2, "tres": 3, "três": 3,
    "quatro": 4, "cinco": 5, "seis": 6, "sete": 7, "oito": 8,
    "nove": 9, "dez": 10,
}

_JOB_LIST_RE = re.compile(
    r"\bjobs?\b((?:\s*(?:,|e)?\s*`[A-Za-z][A-Za-z0-9_-]*`)+)",
    re.IGNORECASE,
)
_CITADOS_ACIMA_RE = re.compile(
    r"\bos (\w+) jobs? de `[^`]+` citados acima \(((?:`[A-Za-z][A-Za-z0-9_-]*`,?\s*)+)\)",
    re.IGNORECASE,
)
_BACKTICK_TOKEN_RE = re.compile(r"`([A-Za-z][A-Za-z0-9_-]*)`")
_CONFIRA_COM_RE = re.compile(r"confira com `([^`]+)`([^.]{0,60})")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def extract_chapter_9(text):
    """Returns chapter 9's body (exclusive of its own heading line), or
    None if the heading is gone - absence is a FAILURE to report
    (GODS_LAWS.md L-40), never a silent pass."""
    m = CHAPTER_HEADING_RE.search(text)
    if m is None:
        return None
    body_start = text.find("\n", m.start()) + 1
    rest = text[body_start:]
    end_m = NEXT_HEADING_RE.search(rest)
    end = end_m.start() if end_m else len(rest)
    return rest[:end]


def flatten(section_text):
    """Collapses all whitespace runs (including newlines) to a single
    space - required before any regex below, since the real chapter
    breaks 'confira com `...`' commands mid-backtick-span across a
    markdown line wrap (see the module header's own L-41 note)."""
    return re.sub(r"\s+", " ", section_text)


# --- (A) job-name freshness --------------------------------------------


def extract_claimed_job_names(flat_text):
    """Every backtick-wrapped bare identifier that appears immediately
    next to the word job/jobs, in either of the two shapes this chapter
    actually uses today: 'jobs `a`, `b` e `c`' / 'Job `x`', or the
    parenthesised 'citados acima (`a`, `b`, ...)' recap list."""
    claimed = set()
    for m in _JOB_LIST_RE.finditer(flat_text):
        claimed.update(_BACKTICK_TOKEN_RE.findall(m.group(1)))
    for m in _CITADOS_ACIMA_RE.finditer(flat_text):
        claimed.update(_BACKTICK_TOKEN_RE.findall(m.group(2)))
    return claimed


def extract_ci_job_names(ci_yml_text):
    """Top-level job keys of the 'jobs:' block in ci.yml (2-space
    indent, everything after the literal '\\njobs:\\n' line) - returns
    None if that block is missing entirely (GODS_LAWS.md L-40: absence
    is reported, never presumed empty-and-fine)."""
    idx = ci_yml_text.find("\njobs:\n")
    if idx == -1:
        return None
    jobs_block = ci_yml_text[idx:]
    return set(re.findall(r"^  ([A-Za-z][A-Za-z0-9_-]*):", jobs_block, re.MULTILINE))


def check_job_names_fresh(flat_text, ci_yml_text):
    claimed = extract_claimed_job_names(flat_text)
    real_jobs = extract_ci_job_names(ci_yml_text)
    if real_jobs is None:
        return False, [f"'{CI_YML_RELATIVE_PATH}' nao tem bloco 'jobs:' reconhecivel"], 0
    stale = sorted(name for name in claimed if name not in real_jobs)
    return (len(stale) == 0), stale, len(claimed)


# --- (B) count-matches-list ---------------------------------------------


def check_job_count_matches_list(flat_text):
    """'os seis jobs ... citados acima (`a`, `b`, ...)' - the number
    word must equal the length of the list that follows it. Returns
    (ok, detail, found) - found=False means the sentence itself is
    absent (a distinct, reportable state from 'found but mismatched'),
    not silently folded into 'ok'."""
    m = _CITADOS_ACIMA_RE.search(flat_text)
    if m is None:
        return True, None, False
    word = m.group(1).lower()
    names = _BACKTICK_TOKEN_RE.findall(m.group(2))
    if word not in NUM_WORDS:
        return False, f"numeral por extenso '{word}' fora do vocabulario conhecido {sorted(NUM_WORDS)}", True
    expected = NUM_WORDS[word]
    if expected != len(names):
        return False, f"texto diz '{word}' ({expected}) mas a lista que segue tem {len(names)} nome(s): {names}", True
    return True, None, True


# --- (C) confira-com commands, executed for real -------------------------


def _grep_equivalent(pattern, file_path):
    """Same match semantics as `grep -n 'A\\|B\\|C' file_path`: a line
    matches if ANY '\\|'-separated alternative of PATTERN is a literal
    substring of it. Raises FileNotFoundError if file_path is missing -
    the caller turns that into a reported failure, never a silent
    'no matches'."""
    alternatives = pattern.split("\\|")
    text = file_path.read_text(encoding="utf-8")
    return [line for line in text.splitlines() if any(alt in line for alt in alternatives)]


def parse_confira_com_command(cmd_str):
    """Parses the narrow 'grep -n [--] PATTERN FILE' shape every
    'confira com' command in this chapter uses today. Returns
    (pattern, file_relpath) or None if the command doesn't match this
    shape - an unparseable command is a REPORTED failure upstream
    (piso de varredura nao-vazia), never a silent skip."""
    try:
        argv = shlex.split(cmd_str)
    except ValueError:
        return None
    if len(argv) < 3 or argv[0] != "grep" or "-n" not in argv:
        return None
    tail = [a for a in argv[1:] if a != "-n"]
    if tail and tail[0] == "--":
        tail = tail[1:]
    if len(tail) != 2:
        return None
    pattern, file_relpath = tail
    return pattern, file_relpath


def check_confira_com_commands(flat_text, repo_root):
    """Runs every 'confira com `<comando>`' promise in the chapter for
    real against the tree at repo_root, and compares the outcome
    against what the surrounding prose asserts ('que nao retorna nada'
    => expect empty; anything else => expect non-empty). Returns
    (ok, failures, found)."""
    failures = []
    found = 0
    for m in _CONFIRA_COM_RE.finditer(flat_text):
        found += 1
        cmd_str = m.group(1)
        tail = m.group(2)
        expect_empty = "não retorna nada" in tail or "nao retorna nada" in tail
        parsed = parse_confira_com_command(cmd_str)
        if parsed is None:
            failures.append(f"comando 'confira com `{cmd_str}`' nao tem a forma reconhecida 'grep -n [--] PATTERN FILE'")
            continue
        pattern, file_relpath = parsed
        file_path = repo_root / file_relpath
        try:
            matches = _grep_equivalent(pattern, file_path)
        except FileNotFoundError:
            failures.append(f"comando 'confira com `{cmd_str}`' cita arquivo inexistente: {file_relpath}")
            continue
        is_empty = len(matches) == 0
        if expect_empty and not is_empty:
            failures.append(
                f"'confira com `{cmd_str}`' promete 'nao retorna nada' mas encontrou "
                f"{len(matches)} linha(s) em {file_relpath}: {matches[:3]}"
            )
        elif not expect_empty and is_empty:
            failures.append(
                f"'confira com `{cmd_str}`' promete resultado, mas {file_relpath} nao tem nenhuma linha casando"
            )
    return (len(failures) == 0), failures, found


# --- orchestration --------------------------------------------------------


def check_auditorias_cap9_freshness(auditorias_md_path):
    auditorias_md_path = Path(auditorias_md_path)
    repo_root = auditorias_md_path.resolve().parent
    try:
        text = auditorias_md_path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"{SCRIPT_NAME}: nao foi possivel ler {auditorias_md_path} ({exc})", file=sys.stderr)
        return False

    chapter = extract_chapter_9(text)
    if chapter is None:
        print(
            f"{SCRIPT_NAME}: capitulo '## 9.' nao encontrado em {auditorias_md_path} "
            "(GODS_LAWS.md L-40: ausencia e' falha, nunca sucesso silencioso)",
            file=sys.stderr,
        )
        return False
    flat_text = flatten(chapter)

    ci_yml_path = repo_root / CI_YML_RELATIVE_PATH
    try:
        ci_yml_text = ci_yml_path.read_text(encoding="utf-8")
    except OSError as exc:
        print(f"{SCRIPT_NAME}: nao foi possivel ler {ci_yml_path} ({exc})", file=sys.stderr)
        return False

    ok_a, stale_jobs, n_claimed = check_job_names_fresh(flat_text, ci_yml_text)
    ok_b, detail_b, found_b = check_job_count_matches_list(flat_text)
    ok_c, failures_c, found_c = check_confira_com_commands(flat_text, repo_root)

    print(
        f"{SCRIPT_NAME}: (A) nomes de job citados={n_claimed} obsoleto(s)={len(stale_jobs)} "
        f"| (B) sentenca-de-contagem encontrada={found_b} | "
        f"(C) comandos 'confira com' encontrados={found_c} falharam={len(failures_c)}"
    )

    ok = True
    if not ok_a:
        ok = False
        print(f"{SCRIPT_NAME}: (A) nome(s) de job obsoleto(s) no capitulo 9, nao existem em {CI_YML_RELATIVE_PATH}:", file=sys.stderr)
        for name in stale_jobs:
            print(f"  job `{name}`", file=sys.stderr)
    if not ok_b:
        ok = False
        print(f"{SCRIPT_NAME}: (B) contagem por extenso nao bate com a lista: {detail_b}", file=sys.stderr)
    if not ok_c:
        ok = False
        print(f"{SCRIPT_NAME}: (C) promessa(s) de verificacao do capitulo 9 nao se confirmam contra a arvore real:", file=sys.stderr)
        for f in failures_c:
            print(f"  {f}", file=sys.stderr)

    if n_claimed == 0 and found_c == 0:
        # Piso de varredura nao-vazia (GODS_LAWS.md L-40): um capitulo 9
        # que nao cita NENHUM nome de job e NENHUM 'confira com' e' sinal
        # de que a extracao quebrou (o capitulo real sempre cita os
        # dois), nao de capitulo limpo - reprova por principio, nunca
        # declara verde por "nada para checar".
        ok = False
        print(
            f"{SCRIPT_NAME}: varredura vazia (0 nome de job e 0 comando 'confira com' "
            "no capitulo 9) - extracao provavelmente quebrada, GODS_LAWS.md L-40",
            file=sys.stderr,
        )

    return ok


# --- selftest ---------------------------------------------------------


def _write_fixture_tree(tmp_path, chapter_body, ci_yml_jobs_block, extra_files=None):
    auditorias = tmp_path / "AUDITORIAS.md"
    auditorias.write_text(
        f"# Doc\n\n## 8. Anterior\n\ncorpo\n\n## 9. Portões automáticos de qualidade: estado hoje (L-23)\n\n"
        f"{chapter_body}\n\n## 10. Proximo\n\ncorpo\n",
        encoding="utf-8",
    )
    ci_dir = tmp_path / ".github" / "workflows"
    ci_dir.mkdir(parents=True, exist_ok=True)
    (ci_dir / "ci.yml").write_text(f"name: CI\n\njobs:\n{ci_yml_jobs_block}", encoding="utf-8")
    for relpath, content in (extra_files or {}).items():
        p = tmp_path / relpath
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")
    return auditorias


_CLEAN_JOBS_BLOCK = (
    "  linux:\n    runs-on: ubuntu-latest\n"
    "  windows:\n    runs-on: windows-latest\n"
    "  clang:\n    runs-on: ubuntu-latest\n"
    "  sanitizer:\n    runs-on: ubuntu-latest\n"
    "  lint:\n    runs-on: ubuntu-latest\n"
    "  gitleaks:\n    runs-on: ubuntu-latest\n"
)

_CLEAN_CHAPTER = (
    "Estado confirmado por leitura de `.github/workflows/ci.yml`. "
    "`-Werror` nos jobs `linux`, `windows` e `clang`, confira com `grep -n WERROR fixture_werror.txt`. "
    "Job `sanitizer` em `.github/workflows/ci.yml`, roda alguma coisa. "
    "os seis jobs de `.github/workflows/ci.yml` citados acima (`linux`, `windows`, `clang`, `sanitizer`, `lint`, `gitleaks`) foram lidos por inteiro. "
    "check_x.sh e check_y.sh nao tem add_test (confira com `grep -n 'check_x\\|check_y' fixture_cmake.txt`, que nao retorna nada)."
)


def selftest_clean_chapter_passes():
    import tempfile

    with tempfile.TemporaryDirectory(prefix="glintfx-audcap9-clean-") as tmp_dir:
        tmp_path = Path(tmp_dir)
        auditorias = _write_fixture_tree(
            tmp_path,
            _CLEAN_CHAPTER,
            _CLEAN_JOBS_BLOCK,
            extra_files={
                "fixture_werror.txt": "linha com WERROR de verdade\n",
                "fixture_cmake.txt": "nada relacionado aqui\n",
            },
        )
        ok = check_auditorias_cap9_freshness(auditorias)
    if not ok:
        print("selftest: controle CLEAN-CHAPTER FALHOU (capitulo limpo nao deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle CLEAN-CHAPTER OK (capitulo limpo passa)")
    return True


def selftest_stale_job_name_fails():
    import tempfile

    dirty_chapter = _CLEAN_CHAPTER.replace("Job `sanitizer`", "Job `sanitizador-renomeado`")
    with tempfile.TemporaryDirectory(prefix="glintfx-audcap9-stalejob-") as tmp_dir:
        tmp_path = Path(tmp_dir)
        auditorias = _write_fixture_tree(
            tmp_path,
            dirty_chapter,
            _CLEAN_JOBS_BLOCK,
            extra_files={
                "fixture_werror.txt": "linha com WERROR de verdade\n",
                "fixture_cmake.txt": "nada relacionado aqui\n",
            },
        )
        ok = check_auditorias_cap9_freshness(auditorias)
    if ok:
        print("selftest: controle STALE-JOB-NAME FALHOU (job renomeado deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle STALE-JOB-NAME OK (job inexistente em ci.yml reprova)")
    return True


def selftest_stale_count_fails():
    import tempfile

    dirty_chapter = _CLEAN_CHAPTER.replace("os seis jobs", "os sete jobs")
    with tempfile.TemporaryDirectory(prefix="glintfx-audcap9-stalecount-") as tmp_dir:
        tmp_path = Path(tmp_dir)
        auditorias = _write_fixture_tree(
            tmp_path,
            dirty_chapter,
            _CLEAN_JOBS_BLOCK,
            extra_files={
                "fixture_werror.txt": "linha com WERROR de verdade\n",
                "fixture_cmake.txt": "nada relacionado aqui\n",
            },
        )
        ok = check_auditorias_cap9_freshness(auditorias)
    if ok:
        print("selftest: controle STALE-COUNT FALHOU ('sete' contra lista de 6 deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle STALE-COUNT OK (numeral por extenso divergente da lista reprova)")
    return True


def selftest_stale_confira_com_fails():
    import tempfile

    # a promessa diz "nao retorna nada", mas o arquivo fixture agora TEM
    # a palavra procurada - a mesma forma exata do defeito real
    # encontrado em AUDITORIAS.md (check_spdx/check_vendor_purity com
    # add_test de verdade, texto ainda dizendo o contrario).
    with tempfile.TemporaryDirectory(prefix="glintfx-audcap9-staleconfira-") as tmp_dir:
        tmp_path = Path(tmp_dir)
        auditorias = _write_fixture_tree(
            tmp_path,
            _CLEAN_CHAPTER,
            _CLEAN_JOBS_BLOCK,
            extra_files={
                "fixture_werror.txt": "linha com WERROR de verdade\n",
                "fixture_cmake.txt": "check_x aparece aqui agora\n",
            },
        )
        ok = check_auditorias_cap9_freshness(auditorias)
    if ok:
        print("selftest: controle STALE-CONFIRA-COM FALHOU (promessa quebrada deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle STALE-CONFIRA-COM OK ('nao retorna nada' que agora retorna reprova)")
    return True


def selftest_missing_chapter_fails():
    import tempfile

    with tempfile.TemporaryDirectory(prefix="glintfx-audcap9-missing-") as tmp_dir:
        tmp_path = Path(tmp_dir)
        auditorias = tmp_path / "AUDITORIAS.md"
        auditorias.write_text("# Doc\n\nsem capitulo 9 nenhum aqui\n", encoding="utf-8")
        (tmp_path / ".github" / "workflows").mkdir(parents=True)
        (tmp_path / ".github" / "workflows" / "ci.yml").write_text("jobs:\n  x:\n", encoding="utf-8")
        ok = check_auditorias_cap9_freshness(auditorias)
    if ok:
        print("selftest: controle MISSING-CHAPTER FALHOU (capitulo ausente deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle MISSING-CHAPTER OK (capitulo 9 ausente reprova, nunca passa em silencio)")
    return True


def selftest_empty_scan_fails():
    import tempfile

    with tempfile.TemporaryDirectory(prefix="glintfx-audcap9-emptyscan-") as tmp_dir:
        tmp_path = Path(tmp_dir)
        auditorias = _write_fixture_tree(tmp_path, "corpo sem job nenhum e sem confira com nada.", _CLEAN_JOBS_BLOCK)
        ok = check_auditorias_cap9_freshness(auditorias)
    if ok:
        print("selftest: controle EMPTY-SCAN FALHOU (0 job + 0 confira-com deveria reprovar pelo piso L-40)", file=sys.stderr)
        return False
    print("selftest: controle EMPTY-SCAN OK (varredura vazia reprova, GODS_LAWS.md L-40)")
    return True


def selftest_real_tree_passes(auditorias_md_path):
    ok = check_auditorias_cap9_freshness(auditorias_md_path)
    if not ok:
        print("selftest: controle REAL-TREE FALHOU (o capitulo 9 real deveria passar hoje)", file=sys.stderr)
        return False
    print("selftest: controle REAL-TREE OK (o capitulo 9 real deste repositorio passa)")
    return True


def selftest_main():
    real_auditorias_md = Path(__file__).resolve().parents[2] / "AUDITORIAS.md"
    controls = [
        selftest_clean_chapter_passes(),
        selftest_stale_job_name_fails(),
        selftest_stale_count_fails(),
        selftest_stale_confira_com_fails(),
        selftest_missing_chapter_fails(),
        selftest_empty_scan_fails(),
        selftest_real_tree_passes(real_auditorias_md),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def real_main(args):
    if len(args) != 1:
        fail("usage: check_auditorias_cap9_freshness.py <auditorias-md-path>")
    path = args[0]
    if not Path(path).is_file():
        fail(f"file not found: {path}")
    if not check_auditorias_cap9_freshness(path):
        fail("capitulo 9 de AUDITORIAS.md com fato obsoleto (ver mensagem acima)")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
