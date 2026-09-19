#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_windows_static_parallel.py - GODS_LAWS.md L-23 item 3: "A
# analise estatica no Windows roda SEMPRE em paralelo" (ordem do
# lider, 19/09/2026, verbatim: "Nos proximos runs com Windows,
# paralelize a checagem estatica"). Ja cumprida para o unico passo que
# existe hoje (`clang-tidy - src/` do job `windows-lint`, paralelizado
# em 19/09/2026 pelo commit 1e950b6 - serial media 43m44s/44m25s/
# 45m22s/34m47s medidos no servidor em quatro execucoes, paralelo
# mediu 14m10s). Este portao existe para que NENHUM passo novo, em
# NENHUM job Windows futuro, volte a chamar um verificador estatico
# por arquivo dentro de um laco serial - a ordem vale "para toda
# checagem estatica nova que nascer num job Windows, nao so para a
# que existe hoje" (GODS_LAWS.md L-23).
#
# O QUE ESTE PORTAO PROVA: dentro de todo job de .github/workflows/
# ci.yml cujo `runs-on` case com windows-* (hoje: `windows`,
# `windows-lint`, `windows-sanitizer`, `windows-debug`), toda
# INVOCACAO de clang-tidy/cppcheck/clang-format/clang-query e
# localizada, e o laco PowerShell que a envolve e classificado por
# ESTRUTURA (pilha de chaves, nao substring solta - GODS_LAWS.md L-17:
# criterio largo ja editou o item errado neste projeto, e um portao
# que so procura a string "-Parallel" em algum lugar do passo se
# acusaria "aprovado" mesmo com a invocacao real dentro de um
# `foreach (` serial num trecho DIFERENTE do mesmo passo). O laco mais
# proximo que de fato envolve a invocacao decide o veredicto:
#   - `foreach (...)  { ... }`               -> SERIAL, reprova
#   - `for (...) { ... }`                    -> SERIAL, reprova
#   - `... | ForEach-Object { ... }`         -> SERIAL, reprova
#   - `... | ForEach-Object -Parallel { }`   -> PARALELO, aprova
#   - nenhum laco envolvendo a invocacao     -> nao ha' o que
#     paralelizar (chamada unica), no' vacuamente aprovado
#
# COMENTARIOS NAO CONTAM. Este proprio arquivo de CI comenta, em
# prosa, tanto "ForEach-Object -Parallel" quanto "clang-tidy" dezenas
# de vezes (ver o cabecalho do passo "clang-tidy - src/" em ci.yml).
# Toda linha cuja forma OMITIDA de espaco em branco comeca com '#' e'
# excluida ANTES de contar chave ou casar invocacao - sem isso, a
# prosa sozinha inflaria a contagem de lacos e de invocacoes.
#
# PISO DE VARREDURA (GODS_LAWS.md L-40): zero job Windows encontrado,
# ou zero invocacao de verificador estatico encontrada em jobs
# Windows, e' sinal de portao QUEBRADO (job renomeado, ci.yml movido,
# convencao de nome de passo mudou) - nunca "nada para proteger aqui".
# As duas condicoes reprovam com exit 1, cada uma citada por nome.
#
# GATE-TREE-PARITY (GODS_LAWS.md L-04): registrado sem guarda de
# sistema - texto puro, nenhum Docker, nenhuma ferramenta de sistema
# envolvida - roda identico nas cinco plataformas, mesma classe que
# check_container_kill_order.py e check_no_x11.py (ver os cabecalhos
# deles).
#
# Usage:
#   check_windows_static_parallel.py --check <ci.yml>
#   check_windows_static_parallel.py --selftest
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

import re
import sys

SCRIPT_NAME = "check_windows_static_parallel.py"

# Verificadores estaticos que a L-23 governa. Fechado por construcao
# (GODS_LAWS.md L-40 item 5) - a propria lei nomeia clang-tidy e
# cppcheck; clang-format e clang-query entram porque sao a mesma
# familia de ferramenta "roda por arquivo" que a ordem do lider mira
# ("toda checagem estatica nova que nascer num job Windows").
STATIC_CHECKERS = ("clang-tidy", "cppcheck", "clang-format", "clang-query")

_CHECKER_ALTERNATION = "|".join(re.escape(tool) for tool in STATIC_CHECKERS)
# Invocacao real: precedida por '&' (operador de chamada do
# PowerShell - a unica forma que este repositorio usa para rodar um
# executavel externo, ver o passo "clang-tidy - src/" de ci.yml) OU no
# inicio da linha (bare command). NUNCA casa uma mencao dentro de uma
# string/Write-Host, porque essas nao tem '&' nem inicio-de-linha
# imediatamente antes do nome da ferramenta.
_INVOKE_RE = re.compile(
    r"(?:&\s*|^)(" + _CHECKER_ALTERNATION + r")(?:\.exe)?\b"
)

_LOOP_KEYWORDS_FOREACH_OBJECT_RE = re.compile(r"\bForEach-Object\b")
_PARALLEL_FLAG_RE = re.compile(r"-Parallel\b")
_FOREACH_RE = re.compile(r"\bforeach\s*\(")
_FOR_RE = re.compile(r"\bfor\s*\(")

SERIAL_KINDS = frozenset({"SERIAL_FOREACH", "SERIAL_FOR", "SERIAL_FOREACH_OBJECT"})
LOOP_KINDS = SERIAL_KINDS | {"PARALLEL_FOREACH_OBJECT"}


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- parsing: jobs dentro do bloco 'jobs:' --------------------------------


_JOB_HEADER_RE = re.compile(r"^  ([A-Za-z][A-Za-z0-9_-]*):\s*$")
_RUNS_ON_RE = re.compile(r"^    runs-on:\s*(.+?)\s*$")
_STEP_START_RE = re.compile(r"^      - \S")
_STEP_NAME_RE = re.compile(r"^\s*-?\s*name:\s*(.+?)\s*$")


def split_jobs(lines):
    """Devolve lista de (nome_job, linha_inicio_0idx, linha_fim_0idx_
    exclusiva) para cada chave de topo do bloco 'jobs:' - nunca das
    chaves de 'on:' (mesma armadilha que extract_ci_job_names() de
    check_auditorias_cap9_freshness.py ja documenta: 'push:'/
    'pull_request:'/'workflow_dispatch:' tem a MESMA indentacao de
    2 espacos que um nome de job, e aparecem ANTES de 'jobs:' no
    arquivo real). O ancora e' a linha literal 'jobs:' sem indentacao."""
    jobs_idx = None
    for i, line in enumerate(lines):
        if line == "jobs:":
            jobs_idx = i
            break
    if jobs_idx is None:
        return None

    headers = []
    for i in range(jobs_idx + 1, len(lines)):
        m = _JOB_HEADER_RE.match(lines[i])
        if m:
            headers.append((m.group(1), i))

    jobs = []
    for idx, (name, start) in enumerate(headers):
        end = headers[idx + 1][1] if idx + 1 < len(headers) else len(lines)
        jobs.append((name, start, end))
    return jobs


def job_runs_on(lines, start, end):
    for i in range(start, end):
        m = _RUNS_ON_RE.match(lines[i])
        if m:
            return m.group(1)
    return None


def is_windows_runner(runs_on_value):
    return runs_on_value is not None and "windows" in runs_on_value.lower()


def split_steps(lines, start, end):
    """Devolve lista de (nome_passo, linha_inicio_0idx, linha_fim_0idx_
    exclusiva) dentro de [start, end) de um job. Passo sem 'name:'
    (ex.: '- uses: actions/checkout@v7') recebe um rotulo derivado do
    numero da linha, nunca None - toda mensagem de violacao precisa
    apontar para algum lugar legivel."""
    starts = [i for i in range(start, end) if _STEP_START_RE.match(lines[i])]
    steps = []
    for idx, step_start in enumerate(starts):
        step_end = starts[idx + 1] if idx + 1 < len(starts) else end
        name = None
        for i in range(step_start, min(step_start + 4, step_end)):
            m = _STEP_NAME_RE.match(lines[i])
            if m:
                name = m.group(1)
                break
        if name is None:
            name = f"(passo sem 'name:' na linha {step_start + 1})"
        steps.append((name, step_start, step_end))
    return steps


# --- classificacao estrutural do laco -------------------------------------


def classify_opener(prefix_text):
    if _LOOP_KEYWORDS_FOREACH_OBJECT_RE.search(prefix_text):
        if _PARALLEL_FLAG_RE.search(prefix_text):
            return "PARALLEL_FOREACH_OBJECT"
        return "SERIAL_FOREACH_OBJECT"
    if _FOREACH_RE.search(prefix_text):
        return "SERIAL_FOREACH"
    if _FOR_RE.search(prefix_text):
        return "SERIAL_FOR"
    return "NEUTRAL"


def nearest_loop_kind(stack):
    for frame_kind in reversed(stack):
        if frame_kind in LOOP_KINDS:
            return frame_kind
    return "NO_LOOP"


def find_invocations_in_step(lines, step_start, step_end):
    """Varre [step_start, step_end) mantendo uma pilha de chaves '{'/
    '}' classificadas por PREFIXO DE LINHA (GODS_LAWS.md L-17: a
    classificacao olha a estrutura que abriu a chave, nunca so' se a
    palavra '-Parallel' aparece em algum lugar do passo). Linha de
    comentario ('#' apos strip) nunca conta - nem para contar chave,
    nem para casar invocacao - porque este proprio arquivo de CI
    comenta 'ForEach-Object -Parallel' e os quatro verificadores em
    prosa, ao redor do codigo real."""
    stack = []
    found = []
    for i in range(step_start, step_end):
        raw_line = lines[i]
        stripped = raw_line.strip()
        if stripped.startswith("#"):
            continue

        m = _INVOKE_RE.search(stripped)
        if m and not re.search(r"run-" + re.escape(m.group(1)), stripped):
            found.append(
                {
                    "tool": m.group(1),
                    "line_no": i + 1,
                    "loop_kind": nearest_loop_kind(stack),
                }
            )

        pos = 0
        while pos < len(raw_line):
            ch = raw_line[pos]
            if ch == "{":
                stack.append(classify_opener(raw_line[:pos]))
            elif ch == "}":
                if stack:
                    stack.pop()
            pos += 1
    return found


# --- veredicto ---------------------------------------------------------


def run_check(ci_yml_text):
    """Devolve (jobs_windows_encontrados, invocacoes_encontradas,
    lista_de_violacoes). Cada violacao e' um dict com job/step/tool/
    line_no/loop_kind, ja' formatavel em mensagem."""
    lines = ci_yml_text.splitlines()
    jobs = split_jobs(lines)
    if jobs is None:
        return 0, 0, [], "arquivo sem bloco 'jobs:' reconhecivel"

    windows_jobs = [
        (name, start, end)
        for name, start, end in jobs
        if is_windows_runner(job_runs_on(lines, start, end))
    ]

    invocations_total = 0
    violations = []
    analisadas = 0
    for job_name, job_start, job_end in windows_jobs:
        for step_name, step_start, step_end in split_steps(lines, job_start, job_end):
            for inv in find_invocations_in_step(lines, step_start, step_end):
                invocations_total += 1
                analisadas += 1
                if inv["loop_kind"] in SERIAL_KINDS:
                    violations.append(
                        {
                            "job": job_name,
                            "step": step_name,
                            "tool": inv["tool"],
                            "line_no": inv["line_no"],
                            "loop_kind": inv["loop_kind"],
                        }
                    )

    if analisadas != invocations_total:
        # Defensivo (GODS_LAWS.md L-36, "armadilha do lote"): os dois
        # contadores vem do MESMO laco acima, entao so' divergem se um
        # refactor futuro separar contagem de processamento sem
        # manter os dois em sincronia - reprova alto e cedo em vez de
        # publicar um numero que nao bate.
        fail(
            f"cobertura perdida: {invocations_total} invocacao(oes) encontrada(s), "
            f"{analisadas} analisada(s) - o lote morreu no meio (GODS_LAWS.md L-36)"
        )

    return len(windows_jobs), invocations_total, violations, None


# --- real mode -------------------------------------------------------------


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def real_main(args):
    if len(args) != 1:
        fail("usage: check_windows_static_parallel.py --check <ci.yml>")
    (ci_yml_path,) = args

    ci_yml_text = _read_file(ci_yml_path)
    jobs_found, invocations_found, violations, parse_error = run_check(ci_yml_text)

    if parse_error is not None:
        fail(f"{parse_error} em {ci_yml_path} - GODS_LAWS.md L-40, varredura RECUSADA")

    print(
        f"{SCRIPT_NAME}: jobs_windows_encontrados={jobs_found} "
        f"invocacoes_analisadas={invocations_found} reprovadas={len(violations)}"
    )

    if jobs_found == 0:
        fail(
            "varredura vazia: nenhum job com runs-on Windows encontrado em "
            f"{ci_yml_path} - GODS_LAWS.md L-40, isto e' sinal de portao quebrado "
            "(nome de job/runner mudou), nunca de ausencia real de job Windows"
        )
    if invocations_found == 0:
        fail(
            "varredura vazia: nenhuma invocacao de clang-tidy/cppcheck/clang-format/"
            f"clang-query encontrada em job Windows de {ci_yml_path} - GODS_LAWS.md "
            "L-40, isto e' sinal de portao quebrado (convencao de chamada mudou), "
            "nunca de ausencia real de checagem estatica"
        )

    if violations:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(violations)} invocacao(oes) serial(is)):", file=sys.stderr)
        for v in violations:
            print(
                f"  - job {v['job']!r}, passo {v['step']!r}, linha {v['line_no']}: "
                f"'{v['tool']}' invocado dentro de laco SERIAL ({v['loop_kind']}) - "
                "GODS_LAWS.md L-23 exige ForEach-Object -Parallel em job Windows",
                file=sys.stderr,
            )
        sys.exit(1)

    print(f"{SCRIPT_NAME}: OK - nenhuma checagem estatica serial em job Windows")


# --- fixtures and controls for --selftest -----------------------------


_FAKE_CI_PARALLEL_OK = """\
jobs:
  linux:
    runs-on: ubuntu-latest
    steps:
      - name: clang-tidy serial no Linux (fora de escopo)
        shell: bash
        run: |
          for f in $files; do
            clang-tidy -p build "$f"
          done

  windows-lint:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v7
      - name: clang-tidy - src/
        shell: pwsh
        run: |
          $arquivos = @(git ls-files -- 'src/*.cpp')
          $dop = 4
          $resultados = @($arquivos | ForEach-Object -Parallel {
            $saida = & clang-tidy -p $using:buildDir --quiet $_ 2>&1
            [PSCustomObject]@{ Arquivo = $_; Rc = $LASTEXITCODE }
          } -ThrottleLimit $dop)
          Write-Host "ok"
"""

_FAKE_CI_FOREACH_SERIAL = """\
jobs:
  windows-lint:
    runs-on: windows-latest
    steps:
      - name: clang-tidy - src/
        shell: pwsh
        run: |
          $arquivos = @(git ls-files -- 'src/*.cpp')
          foreach ($arquivo in $arquivos) {
            $saida = & clang-tidy -p $buildDir --quiet $arquivo 2>&1
            Write-Host $saida
          }
"""

_FAKE_CI_FOREACH_OBJECT_SERIAL = """\
jobs:
  windows-lint:
    runs-on: windows-latest
    steps:
      - name: cppcheck - src/
        shell: pwsh
        run: |
          $arquivos = @(git ls-files -- 'src/*.cpp')
          $resultados = @($arquivos | ForEach-Object {
            $saida = & cppcheck $_ 2>&1
            Write-Host $saida
          })
"""

_FAKE_CI_FOR_SERIAL = """\
jobs:
  windows-lint:
    runs-on: windows-latest
    steps:
      - name: clang-format - src/
        shell: pwsh
        run: |
          for ($i = 0; $i -lt $arquivos.Count; $i++) {
            & clang-format --dry-run --Werror $arquivos[$i]
          }
"""

_FAKE_CI_NO_WINDOWS_JOB = """\
jobs:
  linux:
    runs-on: ubuntu-latest
    steps:
      - name: clang-tidy - src/
        shell: bash
        run: |
          run-clang-tidy -p build
"""

_FAKE_CI_WINDOWS_NO_INVOCATION = """\
jobs:
  windows:
    runs-on: windows-latest
    steps:
      - name: build
        shell: pwsh
        run: |
          cmake --build build
"""

# Prosa que menciona os quatro verificadores e "ForEach-Object
# -Parallel" fora de qualquer invocacao real - tem que passar limpo,
# senao o portao se acusaria sozinho na propria arvore (este arquivo
# de ci.yml faz exatamente isto ao redor do passo real).
_FAKE_CI_COMMENT_NOISE = """\
jobs:
  windows-lint:
    runs-on: windows-latest
    steps:
      - name: clang-tidy - src/
        shell: pwsh
        run: |
          # Comentario mencionando clang-tidy, cppcheck, clang-format,
          # clang-query e ForEach-Object -Parallel sem invocar nada -
          # foreach ($x in $y) { clang-tidy $x } tambem em comentario.
          $resultados = @($arquivos | ForEach-Object -Parallel {
            $saida = & clang-tidy -p $using:buildDir --quiet $_ 2>&1
          } -ThrottleLimit 4)
"""


def selftest_positive_parallel_passes():
    jobs_found, invocations_found, violations, err = run_check(_FAKE_CI_PARALLEL_OK)
    if err is not None or jobs_found != 1 or invocations_found != 1 or violations:
        print(
            f"selftest: POSITIVO (ForEach-Object -Parallel) FALHOU: jobs={jobs_found} "
            f"inv={invocations_found} violations={violations} err={err}",
            file=sys.stderr,
        )
        return False
    print("selftest: POSITIVO OK (ForEach-Object -Parallel aprovado, laco serial do job Linux fora de escopo)")
    return True


def selftest_foreach_serial_reproves():
    jobs_found, invocations_found, violations, err = run_check(_FAKE_CI_FOREACH_SERIAL)
    if err is not None or not violations:
        print(f"selftest: NEGATIVO (foreach serial) FALHOU: violations={violations} err={err}", file=sys.stderr)
        return False
    v = violations[0]
    if v["job"] != "windows-lint" or v["tool"] != "clang-tidy" or v["loop_kind"] != "SERIAL_FOREACH":
        print(f"selftest: NEGATIVO (foreach serial) FALHOU (detalhe errado): {v}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (foreach serial) OK, citado: {v}")
    return True


def selftest_foreach_object_serial_reproves():
    jobs_found, invocations_found, violations, err = run_check(_FAKE_CI_FOREACH_OBJECT_SERIAL)
    if err is not None or not violations:
        print(f"selftest: NEGATIVO (ForEach-Object sem -Parallel) FALHOU: violations={violations} err={err}", file=sys.stderr)
        return False
    v = violations[0]
    if v["tool"] != "cppcheck" or v["loop_kind"] != "SERIAL_FOREACH_OBJECT":
        print(f"selftest: NEGATIVO (ForEach-Object sem -Parallel) FALHOU (detalhe errado): {v}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (ForEach-Object sem -Parallel) OK, citado: {v}")
    return True


def selftest_for_serial_reproves():
    jobs_found, invocations_found, violations, err = run_check(_FAKE_CI_FOR_SERIAL)
    if err is not None or not violations:
        print(f"selftest: NEGATIVO (for serial) FALHOU: violations={violations} err={err}", file=sys.stderr)
        return False
    v = violations[0]
    if v["tool"] != "clang-format" or v["loop_kind"] != "SERIAL_FOR":
        print(f"selftest: NEGATIVO (for serial) FALHOU (detalhe errado): {v}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (for serial) OK, citado: {v}")
    return True


def selftest_no_windows_job_is_empty_scan():
    jobs_found, invocations_found, violations, err = run_check(_FAKE_CI_NO_WINDOWS_JOB)
    if err is not None:
        print(f"selftest: PISO (sem job Windows) FALHOU (parse_error inesperado): {err}", file=sys.stderr)
        return False
    if jobs_found != 0:
        print(f"selftest: PISO (sem job Windows) FALHOU (deveria achar 0 jobs Windows, achou {jobs_found})", file=sys.stderr)
        return False
    print("selftest: PISO (sem job Windows) OK (0 jobs Windows encontrados, real_main reprovaria)")
    return True


def selftest_windows_no_invocation_is_empty_scan():
    jobs_found, invocations_found, violations, err = run_check(_FAKE_CI_WINDOWS_NO_INVOCATION)
    if err is not None:
        print(f"selftest: PISO (sem invocacao) FALHOU (parse_error inesperado): {err}", file=sys.stderr)
        return False
    if jobs_found != 1 or invocations_found != 0:
        print(
            f"selftest: PISO (sem invocacao) FALHOU: jobs={jobs_found} inv={invocations_found} "
            "(esperava 1 job Windows, 0 invocacao)",
            file=sys.stderr,
        )
        return False
    print("selftest: PISO (sem invocacao) OK (1 job Windows sem checagem estatica, real_main reprovaria)")
    return True


def selftest_comment_noise_ignored():
    jobs_found, invocations_found, violations, err = run_check(_FAKE_CI_COMMENT_NOISE)
    if err is not None or invocations_found != 1 or violations:
        print(
            f"selftest: RUIDO DE COMENTARIO FALHOU: inv={invocations_found} violations={violations} err={err} "
            "(prosa mencionando os quatro verificadores e ForEach-Object -Parallel nao pode contar)",
            file=sys.stderr,
        )
        return False
    print("selftest: RUIDO DE COMENTARIO OK (prosa ignorada, so' a invocacao real contou)")
    return True


def selftest_real_tree_matches_expectation(ci_yml_path):
    """Sanidade final contra a arvore REAL (nao fixture): o passo
    'clang-tidy - src/' de windows-lint tem que aparecer PARALELO hoje
    - se este controle falhar, o proprio commit 1e950b6 regrediu."""
    try:
        with open(ci_yml_path, "r", encoding="utf-8") as handle:
            real_text = handle.read()
    except OSError:
        print(f"selftest: ARVORE REAL - ci.yml nao encontrado em {ci_yml_path}, controle pulado")
        return True
    jobs_found, invocations_found, violations, err = run_check(real_text)
    if err is not None:
        print(f"selftest: ARVORE REAL FALHOU (parse_error): {err}", file=sys.stderr)
        return False
    if jobs_found == 0 or invocations_found == 0:
        print(
            f"selftest: ARVORE REAL FALHOU: jobs={jobs_found} inv={invocations_found} "
            "(piso de varredura da arvore real nao pode ser zero)",
            file=sys.stderr,
        )
        return False
    if violations:
        print(f"selftest: ARVORE REAL FALHOU (violacao real presente hoje): {violations}", file=sys.stderr)
        return False
    print(f"selftest: ARVORE REAL OK (jobs_windows={jobs_found}, invocacoes={invocations_found}, 0 reprovada)")
    return True


def selftest_main(ci_yml_path):
    controls = [
        selftest_positive_parallel_passes(),
        selftest_foreach_serial_reproves(),
        selftest_foreach_object_serial_reproves(),
        selftest_for_serial_reproves(),
        selftest_no_windows_job_is_empty_scan(),
        selftest_windows_no_invocation_is_empty_scan(),
        selftest_comment_noise_ignored(),
        selftest_real_tree_matches_expectation(ci_yml_path),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        ci_yml_path = args[1] if len(args) > 1 else "../.github/workflows/ci.yml"
        selftest_main(ci_yml_path)
    elif args and args[0] == "--check":
        real_main(args[1:])
    else:
        fail("usage: check_windows_static_parallel.py --check <ci.yml>  |  --selftest [ci.yml]")


if __name__ == "__main__":
    main()
