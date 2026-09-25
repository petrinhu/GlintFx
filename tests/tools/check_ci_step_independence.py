#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_ci_step_independence.py - CI-SPLIT-PER-OS A2 (docs/plano-ci-
# split-per-os.md secao 4.2, GODS_LAWS.md L-04 item 3, L-40): "um passo
# vermelho nao esconde os passos seguintes do mesmo sistema" - medido
# ao vivo no run 36036115151 (job `wayland-container`, perna `plain`):
# um vermelho no passo 28 (`egl_protocol_error_smoke`) pulou os passos
# 29 a 51 - 12 fixturas, o piso do inventario, o contador de alocacao,
# a prova `ldd libasan`, a varredura do sanitizer, a coleta MEASURED, a
# publicacao do inventario, e os quatro controles negativos de
# isolamento. Nada daquilo tinha `if:` nenhum - o comportamento default
# do GitHub Actions ("so roda se tudo antes teve sucesso") escondia
# tudo atras da primeira fixture que reprovasse.
#
# O QUE ESTE PORTAO EXIGE, por LEITURA DE TEXTO do ci.yml (nunca
# executa nada - GODS_LAWS.md L-09, script puro de leitura):
#
#   - todo PASSO DE TESTE (identificado por chamar `exec_fixture.sh`
#     dentro do proprio `run:`) tem de ter `if:` citando `!cancelled()`
#     E um PRE-REQUISITO NOMEADO (`steps.<id>.outcome`) - nunca sem
#     `if:` nenhum (o defeito medido: sem isso, um vermelho esconde os
#     passos seguintes).
#   - todo PASSO DE PUBLICACAO de artefato de paridade (`uses: actions/
#     upload-artifact...` citando um `name:` que comeca com
#     `parity-inv-` ou `measured-`) tem de ter `if:` citando
#     `!cancelled()` - senao a publicacao em si vira vitima do mesmo
#     defeito (o inventario nunca chega ao job `parity`).
#
# Usage:
#   check_ci_step_independence.py --check <ci.yml> [--job <nome>]
#   check_ci_step_independence.py --selftest
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

import re
import sys

SCRIPT_NAME = "check_ci_step_independence.py"

DEFAULT_JOB = "wayland-container"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- extracao do bloco do job -------------------------------------------
#
# Mesma logica que check_container_fixture_inventory.py's own
# extract_job_block()/check_container_kill_order.py's own
# extract_job_block() (a mesma funcao, ja duplicada uma vez antes desta
# fatia - GODS_LAWS.md L-17 "regra de 3": esta e' a TERCEIRA copia; a
# extracao para um modulo compartilhado fica registrada como debito
# conhecido, fora do escopo desta fatia (L-32 - nao mexer em dois
# arquivos ja publicados so' por semelhanca de forma).

_JOB_HEADER_RE = re.compile(r"^  ([A-Za-z][A-Za-z0-9_-]*):\s*$")


def extract_job_block(ci_yml_text, job_name):
    target_re = re.compile(r"^  " + re.escape(job_name) + r":\s*$")
    lines = ci_yml_text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if start is None:
            if target_re.match(line):
                start = i
            continue
        if _JOB_HEADER_RE.match(line) and not target_re.match(line):
            return "\n".join(lines[start:i])
    if start is not None:
        return "\n".join(lines[start:])
    return None


# F2 (achado do main, GODS_LAWS.md L-40): lista TODOS os nomes de job
# do arquivo, na ordem em que aparecem - usado pelo modo padrao (sem
# `--job`) pra varrer o arquivo inteiro. `_JOB_HEADER_RE` sozinha NAO
# basta aqui: `on:` tambem tem sub-chaves de 2 espacos (`  push:`,
# `  pull_request:`, ci.yml:68/80) que bateriam na mesma forma se
# aplicadas ao arquivo inteiro sem guarda de secao - por isso so conta
# a partir da linha `jobs:` (nivel 0) e para na proxima chave de nivel
# 0 (ou fim de arquivo).
_JOBS_SECTION_RE = re.compile(r"^jobs:\s*$")
_TOP_LEVEL_KEY_RE = re.compile(r"^[A-Za-z]")


def _all_job_names(ci_yml_text):
    names = []
    in_jobs_section = False
    for line in ci_yml_text.splitlines():
        if _JOBS_SECTION_RE.match(line):
            in_jobs_section = True
            continue
        if not in_jobs_section:
            continue
        if _TOP_LEVEL_KEY_RE.match(line):
            break
        m = _JOB_HEADER_RE.match(line)
        if m:
            names.append(m.group(1))
    return names


# --- particao do bloco do job em passos ---------------------------------
#
# Um "passo" comeca em uma linha "      - ..." (6 espacos, o mesmo
# recuo que toda entrada de `steps:` usa neste arquivo) e vai ate a
# proxima linha nessa mesma forma, ou o fim do bloco.
#
# F2b (achado proprio, medido contra .github/workflows/ci.yml real -
# o job `leis` tem um UNICO passo, `- uses: actions/checkout@v7`, SEM
# `name:`; GitHub Actions aceita isso, a UI usa o `uses:` como rotulo):
# a forma antiga so reconhecia inicio de passo pela chave `name:`
# LITERAL - um passo sem ela desaparecia do parser por inteiro (o job
# "aparentava" vazio). Reconhece QUALQUER inicio de item da lista
# (`- `), e usa `name:` como rotulo quando presente (forma normal, todo
# passo de teste/publicacao deste projeto ate hoje), ou o resto da
# PROPRIA linha (ex.: "uses: actions/checkout@v7") quando nao ha' - um
# passo nunca fica sem identificacao numa mensagem de erro.
_STEP_START_RE = re.compile(r"^      - (?P<rest>.+)$")
_STEP_NAME_KEY_RE = re.compile(r"^name: (?P<name>.+)$")


def _step_display_name(rest_of_first_line):
    m = _STEP_NAME_KEY_RE.match(rest_of_first_line)
    return m.group("name") if m else rest_of_first_line


def split_steps(job_block_text):
    """Retorna [(nome, texto_do_passo), ...] - texto_do_passo inclui a
    propria linha inicial ('- name: ...' ou '- uses: ...' etc.) ate' a
    linha anterior ao proximo passo."""
    lines = job_block_text.splitlines()
    starts = [
        (i, _step_display_name(m.group("rest")))
        for i, line in enumerate(lines)
        if (m := _STEP_START_RE.match(line))
    ]
    steps = []
    for idx, (line_no, name) in enumerate(starts):
        end = starts[idx + 1][0] if idx + 1 < len(starts) else len(lines)
        steps.append((name, "\n".join(lines[line_no:end])))
    return steps


# --- classificacao de passo ----------------------------------------------


def is_test_step(step_text):
    return "tests/container/exec_fixture.sh" in step_text


def is_publish_step(step_text):
    """Passo de publicacao de artefato de paridade - `uses: actions/
    upload-artifact` com um `name:` (dentro do `with:`) comecando por
    `parity-inv-` ou `measured-`. Nunca por SUBSTRINCA solta (GODS_LAWS.
    md L-17 "criterio largo edita o alvo errado") - confere as DUAS
    condicoes, nao so' uma."""
    if "uses: actions/upload-artifact" not in step_text:
        return False
    return bool(re.search(r"^\s*name: (parity-inv-|measured-)", step_text, re.MULTILINE))


# --- as duas regras --------------------------------------------------


def _has_if_line(step_text):
    m = re.search(r"^\s*if: (.+)$", step_text, re.MULTILINE)
    return m.group(1) if m else None


# F1 (achado do main, medido contra /var/tmp/glintfx-a2-verif/ci.yml e
# vermelho reproduzido acima nos dois selftests F1-*): a forma antiga
# so' exigia QUALQUER comparacao contra `steps.X.outcome`, entao
# `steps.build.outcome != 'success'` (o oposto exato da regra) e
# `steps.build.outcome == 'failure'` passavam calados. O plano (docs/
# plano-ci-split-per-os.md:140) e as ocorrencias reais do ci.yml usam
# exatamente `steps.<id>.outcome == 'success'` - a regex exige essa
# forma literal, nunca "qualquer comparacao contra outcome".
_PREREQ_RE = re.compile(r"steps\.[A-Za-z_][A-Za-z0-9_]*\.outcome\s*==\s*'success'")


def test_step_errors(name, step_text):
    """Um passo de teste tem de ter `if:` citando `!cancelled()` E um
    pre-requisito NOMEADO (`steps.<id>.outcome`) - as DUAS coisas, na
    MESMA linha `if:` (um `if:` que so' cita uma das duas ainda deixa
    o defeito medido em algum sentido: so' `!cancelled()` roda mesmo
    sem pre-requisito nenhum satisfeito; so' `steps.X.outcome` sem
    `!cancelled()` volta a esconder o passo atras de cancelamento)."""
    if_value = _has_if_line(step_text)
    if if_value is None:
        return [f"{name!r}: passo de teste sem 'if:' nenhum - reproduz o defeito medido (run 36036115151)"]
    errors = []
    if "!cancelled()" not in if_value:
        errors.append(f"{name!r}: 'if:' nao cita '!cancelled()' - {if_value!r}")
    if not _PREREQ_RE.search(if_value):
        errors.append(f"{name!r}: 'if:' nao cita pre-requisito nomeado (steps.<id>.outcome) - {if_value!r}")
    return errors


def publish_step_errors(name, step_text):
    if_value = _has_if_line(step_text)
    if if_value is None or "!cancelled()" not in if_value:
        return [f"{name!r}: passo de publicacao de artefato sem '!cancelled()' - if={if_value!r}"]
    return []


# --- veredito completo ----------------------------------------------


def run_check(steps):
    """Retorna (contagens, erros) - contagens e' {'testes': N,
    'publicacoes': N} (GODS_LAWS.md L-40: piso de varredura impresso
    SEMPRE, mesmo passando)."""
    test_steps = [(n, t) for n, t in steps if is_test_step(t)]
    publish_steps = [(n, t) for n, t in steps if is_publish_step(t)]
    errors = []
    for name, text in test_steps:
        errors.extend(test_step_errors(name, text))
    for name, text in publish_steps:
        errors.extend(publish_step_errors(name, text))
    counts = {"testes": len(test_steps), "publicacoes": len(publish_steps)}
    return counts, errors


# --- modo real -------------------------------------------------------


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


# F2 (achado do main, GODS_LAWS.md L-40/L-04): `job_name` agora e'
# `None` por padrao - "todos os jobs" (docs/plano-ci-split-per-os.md:
# 297, coluna gemeo: "o portao le o arquivo inteiro"), nunca so'
# DEFAULT_JOB calado. `--job <nome>` continua existindo pra focar UM
# job so' (usado pelos selftests que testam o escopo antigo).
def _parse_real_main_args(args):
    job_name = None
    positional = []
    i = 0
    while i < len(args):
        if args[i] == "--job":
            if i + 1 >= len(args):
                fail("--check: --job exige um nome de job")
            job_name = args[i + 1]
            i += 2
            continue
        positional.append(args[i])
        i += 1
    if len(positional) != 1:
        fail("usage: check_ci_step_independence.py --check <ci.yml> [--job <nome>]")
    return positional[0], job_name


def _resolve_job_names(ci_yml_text, ci_yml_path, job_name):
    """`job_name=None`: TODOS os jobs do arquivo, na ordem em que
    aparecem - varredura vazia (nenhum job) reprova (GODS_LAWS.md
    L-40). `job_name` explicito: so' ele (comportamento de --job
    preservado)."""
    if job_name is not None:
        return [job_name]
    job_names = _all_job_names(ci_yml_text)
    if not job_names:
        fail(f"varredura vazia: nenhum job encontrado em {ci_yml_path} - GODS_LAWS.md L-40")
    return job_names


def _check_one_job(ci_yml_path, ci_yml_text, job_name):
    """(contagens, erros-com-prefixo-do-job) de UM job - extraida de
    real_main() pra caber em 40 linhas (GODS_LAWS.md L-17)."""
    job_block = extract_job_block(ci_yml_text, job_name)
    if job_block is None:
        fail(f"job {job_name!r} nao encontrado em {ci_yml_path}")
    steps = split_steps(job_block)
    if not steps:
        fail(
            f"varredura vazia: nenhum passo ('- name: ...') encontrado no job {job_name!r} de "
            f"{ci_yml_path} - GODS_LAWS.md L-40, isto e sinal de coleta quebrada"
        )
    counts, errors = run_check(steps)
    print(
        f"{SCRIPT_NAME}: job {job_name!r} - {len(steps)} passo(s) total, "
        f"{counts['testes']} de teste, {counts['publicacoes']} de publicacao"
    )
    return counts, [f"{job_name}: {e}" for e in errors]


def real_main(args):
    ci_yml_path, job_name = _parse_real_main_args(args)
    ci_yml_text = _read_file(ci_yml_path)
    job_names = _resolve_job_names(ci_yml_text, ci_yml_path, job_name)

    total_testes = 0
    all_errors = []
    for jn in job_names:
        counts, errors = _check_one_job(ci_yml_path, ci_yml_text, jn)
        total_testes += counts["testes"]
        all_errors.extend(errors)

    if total_testes == 0:
        fail(
            f"varredura vazia: 0 passo(s) de teste (exec_fixture.sh) em {len(job_names)} job(s) - "
            "GODS_LAWS.md L-40, isto e sinal de coleta quebrada, nunca de arquivo sem fixture nenhuma"
        )
    if all_errors:
        fail(f"{len(all_errors)} problema(s) de independencia de passo:\n  " + "\n  ".join(all_errors))
    print(f"{SCRIPT_NAME}: OK - todo passo de teste e de publicacao sobrevive a um vermelho de sibling")


# --- selftest -----------------------------------------------------

_FIXTURE_CI_YML = """\
  wayland-container:
    steps:
      - name: Sobe o compositor limpo
        id: build
        run: docker run -d --name c glintfx-wltest:ci

      - name: fixture a
        if: ${{ !cancelled() && steps.build.outcome == 'success' }}
        run: |
          echo "a" >> parity_inventory.txt
          codigo=0
          tests/container/exec_fixture.sh c a || codigo=$?
          printf 'a\\t%s\\n' "$codigo" >> results.tsv
          exit "$codigo"

      - name: fixture b
        if: ${{ !cancelled() && steps.build.outcome == 'success' }}
        run: |
          echo "b" >> parity_inventory.txt
          codigo=0
          tests/container/exec_fixture.sh c b || codigo=$?
          printf 'b\\t%s\\n' "$codigo" >> results.tsv
          exit "$codigo"

      - name: Publica o inventario do container (P-0)
        if: ${{ !cancelled() && matrix.perna == 'plain' }}
        uses: actions/upload-artifact@v7
        with:
          name: parity-inv-fedora-container
          path: parity_inventory.txt

  outro-job:
    steps:
      - name: nao pertence a este job
        run: echo alheio
"""


def _run_real_main_capturing(ci_yml_text, job_name=DEFAULT_JOB):
    """`job_name=None` reproduz o CLI real sem `--job` nenhum (F2: o
    modo padrao varre TODOS os jobs do arquivo) - os selftests
    antigos, focados num job so', continuam passando `job_name`
    explicito (comportamento antigo preservado por `--job`)."""
    import contextlib
    import io
    import tempfile
    from pathlib import Path

    buffer = io.StringIO()
    exit_code = None
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "ci.yml"
        path.write_text(ci_yml_text, encoding="utf-8")
        args = [str(path)] if job_name is None else [str(path), "--job", job_name]
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            try:
                real_main(args)
            except SystemExit as exc:
                exit_code = exc.code
    return exit_code, buffer.getvalue()


def selftest_positive_control():
    exit_code, output = _run_real_main_capturing(_FIXTURE_CI_YML)
    if exit_code not in (None, 0):
        print(f"selftest: fixture correta reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    print("selftest: POSITIVO OK (2 passos de teste + 1 publicacao, todos com if: correto)")
    return True


# M1 nomeado no plano da A2: "tirar !cancelled() de um passo -> reprova"
def selftest_missing_cancelled_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "if: ${{ !cancelled() && steps.build.outcome == 'success' }}\n        run: |\n          echo \"a\"",
        "run: |\n          echo \"a\"",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: passo de teste SEM if: nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "'fixture a'" not in output:
        print(f"selftest: reprovou, mas nao nomeou o passo certo: {output!r}", file=sys.stderr)
        return False
    print("selftest: M1-SEM-IF OK (passo de teste sem if: reprova, nomeado)")
    return True


# M2 nomeado no plano da A2: "passo de teste sem pre-requisito nomeado -> reprova"
def selftest_missing_named_prerequisite_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "if: ${{ !cancelled() && steps.build.outcome == 'success' }}\n        run: |\n          echo \"b\"",
        "if: ${{ !cancelled() }}\n        run: |\n          echo \"b\"",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: passo sem pre-requisito nomeado nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "'fixture b'" not in output or "pre-requisito nomeado" not in output:
        print(f"selftest: reprovou, mas nao pela razao certa: {output!r}", file=sys.stderr)
        return False
    print("selftest: M2-SEM-PREREQ OK (if: so' com !cancelled(), sem steps.X.outcome, reprova)")
    return True


# F1 (achado do main, medido contra /var/tmp/glintfx-a2-verif/ci.yml,
# GODS_LAWS.md L-40): pre-requisito com a direcao INVERTIDA - um passo
# que so' roda quando o pre-requisito FALHOU (`!= 'success'`), o
# oposto exato da regra - tinha de reprovar e passava calado, porque
# _PREREQ_RE so' exigia QUALQUER comparacao contra `steps.X.outcome`.
def selftest_prerequisite_wrong_direction_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "if: ${{ !cancelled() && steps.build.outcome == 'success' }}\n        run: |\n          echo \"a\"",
        "if: ${{ !cancelled() && steps.build.outcome != 'success' }}\n        run: |\n          echo \"a\"",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: pre-requisito com direcao invertida (!=) nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "'fixture a'" not in output or "pre-requisito nomeado" not in output:
        print(f"selftest: reprovou, mas nao pela razao certa: {output!r}", file=sys.stderr)
        return False
    print("selftest: F1-PREREQ-DIRECAO-INVERTIDA OK (steps.X.outcome != 'success' reprova)")
    return True


# Gemeo do achado acima: comparar contra QUALQUER outro valor que nao
# seja 'success' (ex.: 'failure') tem de reprovar do mesmo jeito -
# prova que a regra exige a forma exata, nao so' rejeita '!='.
def selftest_prerequisite_wrong_value_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "if: ${{ !cancelled() && steps.build.outcome == 'success' }}\n        run: |\n          echo \"b\"",
        "if: ${{ !cancelled() && steps.build.outcome == 'failure' }}\n        run: |\n          echo \"b\"",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: pre-requisito comparando contra 'failure' nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "'fixture b'" not in output or "pre-requisito nomeado" not in output:
        print(f"selftest: reprovou, mas nao pela razao certa: {output!r}", file=sys.stderr)
        return False
    print("selftest: F1-PREREQ-VALOR-ERRADO OK (steps.X.outcome == 'failure' reprova)")
    return True


# GEMEO do M1: prova que a checagem de '!cancelled()' morde MESMO
# quando o 'if:' JA' tem 'steps.X.outcome' (a forma que passa pela
# checagem de pre-requisito, mas nao pela de !cancelled() - achado
# proprio, GODS_LAWS.md L-27: mutation testing revelou que a fixture
# de 'sem if: nenhum' do M1 acima nunca chega a exercitar esta linha,
# porque o codigo sai mais cedo, no caso 'if_value is None').
def selftest_prerequisite_without_cancelled_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "if: ${{ !cancelled() && steps.build.outcome == 'success' }}\n        run: |\n          echo \"a\"",
        "if: ${{ steps.build.outcome == 'success' }}\n        run: |\n          echo \"a\"",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: if: com pre-requisito mas SEM !cancelled() nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "'fixture a'" not in output or "!cancelled()" not in output:
        print(f"selftest: reprovou, mas nao pela razao certa: {output!r}", file=sys.stderr)
        return False
    print("selftest: M1-GEMEO-COM-PREREQ-SEM-CANCELLED OK (if: com pre-requisito, sem !cancelled(), reprova)")
    return True


# M3 nomeado no plano da A2: "publicacao do inventario sem !cancelled() -> reprova"
def selftest_publish_without_cancelled_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "if: ${{ !cancelled() && matrix.perna == 'plain' }}\n        uses: actions/upload-artifact@v7",
        "if: matrix.perna == 'plain'\n        uses: actions/upload-artifact@v7",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: publicacao sem !cancelled() nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    if "Publica o inventario do container" not in output:
        print(f"selftest: reprovou, mas nao nomeou o passo de publicacao: {output!r}", file=sys.stderr)
        return False
    print("selftest: M3-PUBLICACAO-SEM-CANCELLED OK (publicacao sem !cancelled() reprova)")
    return True


def selftest_empty_job_reproves():
    exit_code, output = _run_real_main_capturing("  wayland-container:\n    steps: []\n")
    if exit_code != 1:
        print(f"selftest: job sem passo nenhum nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    print("selftest: PISO-JOB-VAZIO OK (job sem passo nenhum reprova, GODS_LAWS.md L-40)")
    return True


def selftest_no_test_steps_reproves():
    sem_fixture = """\
  wayland-container:
    steps:
      - name: so publicacao
        if: ${{ !cancelled() }}
        uses: actions/upload-artifact@v7
        with:
          name: parity-inv-fedora-container
"""
    exit_code, output = _run_real_main_capturing(sem_fixture)
    if exit_code != 1:
        print(f"selftest: job sem passo de teste nenhum nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    print("selftest: PISO-SEM-TESTES OK (0 passo de teste reprova, GODS_LAWS.md L-40)")
    return True


def selftest_job_not_found_reproves():
    exit_code, output = _run_real_main_capturing(_FIXTURE_CI_YML, job_name="job-que-nao-existe")
    if exit_code != 1:
        print(f"selftest: job inexistente nao reprovou (codigo {exit_code!r}): {output}", file=sys.stderr)
        return False
    print("selftest: JOB-INEXISTENTE OK (--job com nome que nao existe reprova, nunca varre outro job)")
    return True


def selftest_scoped_to_named_job():
    """O passo do OUTRO job (outro-job) na mesma fixture nunca conta -
    nem para o piso, nem para as regras."""
    exit_code, output = _run_real_main_capturing(_FIXTURE_CI_YML)
    if "nao pertence a este job" in output:
        print(f"selftest: passo de OUTRO job vazou para o resultado: {output!r}", file=sys.stderr)
        return False
    print("selftest: ESCOPO-POR-JOB OK (passo de outro job nunca entra na contagem)")
    return True


# F2 (achado do main, GODS_LAWS.md L-40/L-04): o modo real so' varria
# DEFAULT_JOB ('wayland-container') - um job DIFERENTE (ex.: `lint`,
# ci.yml:2677/2681, "Inventario de paridade (lint)"/"Publicar
# inventario de paridade (lint)") publicando `parity-inv-*` com
# `if: always()` NUNCA era visto pelo portao, mesmo sem `--job` (o
# default calado era `wayland-container`, nunca "todos"). O plano
# (docs/plano-ci-split-per-os.md:297, coluna gemeo: "as mesmas regras
# nos jobs `windows*` (o portao le o arquivo inteiro)"; :143:
# "Publicacao do inventario: if: !cancelled(), sempre") pede o arquivo
# inteiro. Fixture com DOIS jobs: `wayland-container` correto, e
# `outro-job-furado` publicando `parity-inv-outro` com `always()` -
# sem `--job` nenhum (comportamento padrao real), o portao de HOJE
# nao acha o furo.
_FIXTURE_CI_YML_TWO_JOBS = "jobs:\n" + _FIXTURE_CI_YML + """
  outro-job-furado:
    steps:
      - name: Publicar inventario de outro job (furado)
        if: always()
        uses: actions/upload-artifact@v7
        with:
          name: parity-inv-outro
          path: parity_inventory.txt
"""


def selftest_publish_always_in_other_job_reproves():
    exit_code, output = _run_real_main_capturing(_FIXTURE_CI_YML_TWO_JOBS, job_name=None)
    if exit_code != 1:
        print(
            f"selftest: publicacao com always() em job DIFERENTE de wayland-container nao "
            f"reprovou sem --job (codigo {exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    if "Publicar inventario de outro job" not in output:
        print(f"selftest: reprovou, mas nao nomeou o passo do outro job: {output!r}", file=sys.stderr)
        return False
    print(
        "selftest: F2-PUBLICACAO-ALWAYS-EM-OUTRO-JOB OK (sem --job, o portao varre TODOS "
        "os jobs - um publish com always() em qualquer job reprova)"
    )
    return True


# F2b (achado proprio, medido ao vivo contra .github/workflows/ci.yml
# real ao rodar F2 pela primeira vez sem --job): o job `leis` tem UM
# UNICO passo de verdade, `- uses: actions/checkout@v7`, SEM `name:`
# (GitHub Actions aceita isso - a UI usa o `uses:` como rotulo). O
# parser antigo so reconhecia inicio de passo pela chave `name:`
# literal (`_STEP_NAME_RE` exigia "- name: "), entao esse passo
# desaparecia por inteiro - o job "aparentava" ter 0 passos, e
# reprovava pelo piso de varredura vazia por um motivo ERRADO (nao e'
# coleta quebrada, e' um passo real sem nome que o parser nao sabia
# ler). Mais grave: um passo de TESTE ou PUBLICACAO real sem `name:`
# um dia tambem sumiria em silencio, o oposto do que este portao
# existe pra garantir.
_FIXTURE_CI_YML_STEP_WITHOUT_NAME = """jobs:
  leis:
    steps:
      - uses: actions/checkout@v7
"""


def selftest_step_without_name_is_recognized():
    exit_code, output = _run_real_main_capturing(
        _FIXTURE_CI_YML_STEP_WITHOUT_NAME, job_name="leis"
    )
    if exit_code == 1 and "varredura vazia: nenhum passo" in output:
        print(
            f"selftest: F2b-PASSO-SEM-NOME FALHOU (passo sem 'name:' desapareceu do parser, "
            f"job tratado como vazio por engano): {output!r}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: F2b-PASSO-SEM-NOME OK (passo sem 'name:', ex. '- uses: ...', e' "
        "reconhecido como passo de verdade, nunca some do parser)"
    )
    return True


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_missing_cancelled_reproves(),
        selftest_missing_named_prerequisite_reproves(),
        selftest_prerequisite_wrong_direction_reproves(),
        selftest_prerequisite_wrong_value_reproves(),
        selftest_prerequisite_without_cancelled_reproves(),
        selftest_publish_without_cancelled_reproves(),
        selftest_empty_job_reproves(),
        selftest_no_test_steps_reproves(),
        selftest_job_not_found_reproves(),
        selftest_scoped_to_named_job(),
        selftest_publish_always_in_other_job_reproves(),
        selftest_step_without_name_is_recognized(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--check":
        real_main(args[1:])
    else:
        fail("usage: check_ci_step_independence.py --check <ci.yml> [--job <nome>]  |  --selftest")


if __name__ == "__main__":
    main()
