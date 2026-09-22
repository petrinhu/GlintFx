#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_ci_timeouts.py - TODO.md item CI-TIMEOUTS-GATE (GODS_LAWS.md
# L-04/L-17/L-20/L-21/L-23/L-36/L-40).
#
# O QUE FALTAVA ANTES DESTA FATIA: nenhum trabalho de
# .github/workflows/ci.yml declarava `timeout-minutes` (medido:
# grep -c 'timeout-minutes' ci.yml = 0 contra 15 runs-on). Um
# travamento futuro virava seis horas de espera (o teto padrao do
# GitHub Actions) em vez de uma reprovacao rapida - achado por
# sabotagem reproduzida (o revisor prendeu a espera do laco de eventos
# de proposito e viu o trabalho pendurado sem terminar). A fatia
# CI-TIMEOUTS deu teto a cada um dos 15 trabalhos, por CLASSE (curta,
# compilacao, estatica_windows, dependente - ver o bloco de comentario
# logo abaixo de `jobs:` em ci.yml para a medicao e a regra). Este
# portao e' o anti-regressao: sem ele, um trabalho novo nasceria sem
# teto de novo, ou um teto existente seria editado por engano sem
# ninguem perceber a divergencia da classe.
#
# O QUE ESTE PORTAO PROVA: recebe o DIRETORIO
# .github/workflows/ (nunca um arquivo por nome) e, para cada
# *.yml/*.yaml nele, casa os trabalhos do arquivo NOS DOIS SENTIDOS
# contra uma tabela fechada (EXPECTED) job->classe: trabalho no
# arquivo e ausente da tabela = REPROVA ("job novo, classifique-o");
# entrada da tabela ausente do arquivo = REPROVA ("entrada morta,
# o portao esta vigiando um fantasma"); trabalho presente nos dois
# lados mas com `timeout-minutes` ausente ou diferente do teto da
# classe = REPROVA, citando esperado x encontrado. Receber o
# DIRETORIO (nao "ci.yml" por nome) e' o que cobre o arquivo irmao que
# ainda nao nasceu (ex.: release.yml) sem ninguem lembrar de atualizar
# este portao - ele reprova o arquivo novo como "nao classificado" ate
# alguem classifica-lo em EXPECTED, em vez de simplesmente nao olhar
# para ele.
#
# EXCECAO POR CONSTRUCAO (achado de pesquisa do plano da fatia,
# /var/tmp/glintfx-plan/ci-timeouts.md §1): um trabalho que CHAMA
# reusable workflow (`uses:` no nivel do trabalho, nao de um passo)
# NAO ACEITA `timeout-minutes` - o GitHub recusa a chave. Um trabalho
# assim entra na tabela com a classe sentinela REUSABLE_MARK
# ("reutilizavel"), e o portao passa a EXIGIR a AUSENCIA do campo.
# Hoje: zero trabalhos deste tipo (medido: grep -nE '^    uses:'
# ci.yml nao casa nada) - a excecao nasce sem uso real, para nao
# obrigar amanha uma declaracao que o servidor recusaria.
#
# LINHA DE COMENTARIO NUNCA CONTA (GODS_LAWS.md L-17, mesma armadilha
# que check_windows_static_parallel.py ja documenta): o proprio bloco
# de prosa que a fatia CI-TIMEOUTS colocou logo abaixo de `jobs:` em
# ci.yml explica a regra e cita "timeout-minutes" em texto corrido em
# alguma versao futura desse comentario. Toda linha cujo conteudo,
# apos remover espaco em branco a esquerda, comeca com '#' e' zerada
# ANTES de qualquer casamento de padrao - nao so' para o casamento de
# `timeout-minutes:`, tambem para o de `jobs:`, nome de trabalho e
# `uses:`.
#
# TETO DE PASSO NAO E TETO DE TRABALHO: a chave `timeout-minutes` de
# um TRABALHO mora em indentacao de EXATAMENTE 4 espacos (irma de
# `runs-on:`). Um `timeout-minutes` dentro de `steps:` (6+ espacos) e'
# outra coisa - o teto de um PASSO, fora do escopo desta fatia (ver
# secao 8 do plano) - e o portao tem de continuar dizendo "ausente"
# para o trabalho nesse caso, nunca ler o valor de um passo por engano.
# A ancoragem `^    timeout-minutes:` (regex com re.match, nao
# re.search) ja recusa por construcao qualquer indentacao diferente de
# 4 espacos exatos, sem precisar de um caso especial.
#
# PISO DE VARREDURA (GODS_LAWS.md L-40): tres niveis, cada um citado
# por nome, nenhum silencioso: (1) diretorio sem nenhum arquivo
# *.yml/*.yaml; (2) arquivo sem a ancora `jobs:` reconhecivel; (3)
# arquivo com `jobs:` e zero trabalhos dentro. Os tres reprovam, e a
# mensagem sempre diz ONDE a varredura ficou vazia - nunca "nada para
# proteger aqui".
#
# GATE-TREE-PARITY (GODS_LAWS.md L-04): registrado sem guarda de
# sistema, mesma forma que check_windows_static_parallel.py - texto
# puro, nenhum Docker, nenhuma ferramenta de sistema envolvida, roda
# identico nas cinco plataformas.
#
# Usage:
#   check_ci_timeouts.py --check <diretorio .github/workflows>
#   check_ci_timeouts.py --selftest [diretorio .github/workflows]
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

import contextlib
import io
import os
import re
import sys
import tempfile
from collections import Counter

SCRIPT_NAME = "check_ci_timeouts.py"

# As tres classes medidas (secao 4 do plano) mais a classe que o
# orquestrador criou para nao precisar provar ao vivo se a espera por
# `needs:` conta contra o relogio do teto (mudanca 1 do orquestrador
# sobre o plano original): um teto que sobrevive as duas semanticas
# possiveis, em vez de decidir entre elas.
CLASSES = {
    "curta": 10,
    "compilacao": 40,
    "estatica_windows": 65,
    "dependente": 45,
}

# Classe sentinela: trabalho com `uses:` de nivel de trabalho (reusable
# workflow) nao tem teto numerico - a EXIGENCIA e' a ausencia da chave.
REUSABLE_MARK = "reutilizavel"

# Tabela fechada por arquivo de workflow (GODS_LAWS.md L-40 item 5:
# enumeracao fechada por construcao). Arquivo de workflow que nao
# aparecer aqui e' tratado como "nao classificado" e reprova - nunca
# como "fora de escopo, ignorar".
EXPECTED = {
    "ci.yml": {
        "linux": "compilacao",
        "windows": "compilacao",
        "parity": "dependente",
        "wayland-container": "compilacao",
        "lint": "compilacao",
        "ps-syntax": "curta",
        "windows-lint": "estatica_windows",
        "sanitizer": "compilacao",
        "windows-sanitizer": "compilacao",
        "debug": "compilacao",
        "windows-debug": "compilacao",
        "clang": "compilacao",
        "gitleaks": "curta",
        "leis": "curta",
        "version-tag": "curta",
    },
}


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- preparo: remover linha de comentario antes de qualquer casamento -----


def strip_comment_lines(lines):
    """Devolve a MESMA quantidade de linhas (numero de linha preservado
    para mensagem de erro), com toda linha cujo conteudo comeca com '#'
    (apos remover espaco em branco a esquerda) substituida por string
    vazia. Comentario de FIM de linha (depois de um valor real, ex.:
    'timeout-minutes: 40  # classe...') nao e' tocado aqui - so' a
    linha inteira dedicada a comentario e' zerada."""
    return ["" if line.strip().startswith("#") else line for line in lines]


# --- parsing: trabalhos dentro do bloco 'jobs:' ----------------------------


_JOB_HEADER_RE = re.compile(r"^  ([A-Za-z][A-Za-z0-9_-]*):\s*$")
_TIMEOUT_RE = re.compile(r"^    timeout-minutes:\s*(\d+)")
_USES_JOB_RE = re.compile(r"^    uses:\s*\S")


def split_jobs(lines):
    """Devolve lista de (nome_job, linha_inicio_0idx, linha_fim_0idx_
    exclusiva) para cada chave de topo do bloco 'jobs:', ou None se a
    ancora 'jobs:' nao existir no arquivo (piso de varredura, caso
    'sem a ancora'). O ancora e' a linha literal 'jobs:' sem
    indentacao - nunca as chaves de 'on:' (push:/pull_request:/
    workflow_dispatch: tem a MESMA indentacao de 2 espacos e aparecem
    ANTES de 'jobs:' no arquivo real; mesma armadilha que
    check_windows_static_parallel.py ja documenta)."""
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


def job_timeout_declared(lines, start, end):
    """Teto declarado no NIVEL DO TRABALHO (indentacao de exatamente 4
    espacos, irma de 'runs-on:'). `timeout-minutes` dentro de 'steps:'
    (6+ espacos) e' teto de PASSO, fora de escopo, e esta funcao
    devolve None para ele - o casamento ancorado em '^    ' ja recusa
    por construcao qualquer indentacao diferente."""
    for i in range(start, end):
        m = _TIMEOUT_RE.match(lines[i])
        if m:
            return int(m.group(1))
    return None


def job_is_reusable(lines, start, end):
    """True se o trabalho chama reusable workflow via `uses:` no nivel
    do trabalho (4 espacos) - esse trabalho NAO aceita
    `timeout-minutes` (achado de pesquisa do plano, §1)."""
    for i in range(start, end):
        if _USES_JOB_RE.match(lines[i]):
            return True
    return False


# --- veredicto por arquivo ---------------------------------------------


def classify_file(filename, text, table):
    """table: dict job->classe (ou REUSABLE_MARK) para ESTE arquivo,
    ja selecionado de EXPECTED. Devolve (nomes_dos_jobs, violacoes,
    contagem_por_classe):
      - nomes_dos_jobs e' None se o arquivo nao tiver a ancora 'jobs:'
        (piso: 'sem ancora').
      - nomes_dos_jobs e' [] (lista vazia, distinta de None) se
        'jobs:' existir mas nao tiver nenhum trabalho (piso: 'bloco
        vazio').
      - violacoes e' sempre uma lista (pode ser vazia).
      - contagem_por_classe so' soma trabalhos com teto CORRETO."""
    lines = strip_comment_lines(text.splitlines())
    jobs = split_jobs(lines)

    if jobs is None:
        return None, [
            f"{filename}: sem a ancora 'jobs:' reconhecivel - varredura vazia (GODS_LAWS.md L-40)"
        ], Counter()

    if not jobs:
        return [], [
            f"{filename}: bloco 'jobs:' existe mas nao tem nenhum trabalho dentro - "
            "varredura vazia (GODS_LAWS.md L-40)"
        ], Counter()

    job_names = [name for name, _start, _end in jobs]
    violacoes = []
    contagem = Counter()

    for name, start, end in jobs:
        classe = table.get(name)
        if classe is None:
            violacoes.append(
                f"{filename}: job {name!r} novo - sem classe na tabela EXPECTED, classifique-o"
            )
            continue

        timeout = job_timeout_declared(lines, start, end)

        if classe == REUSABLE_MARK:
            if timeout is not None:
                violacoes.append(
                    f"{filename}: job {name!r} chama reusable workflow (classe "
                    f"{REUSABLE_MARK!r}) mas declara timeout-minutes={timeout} - "
                    "o GitHub recusa timeout-minutes neste tipo de trabalho, remova"
                )
            continue

        esperado = CLASSES[classe]
        if timeout is None:
            violacoes.append(
                f"{filename}: job {name!r} (classe {classe!r}) sem timeout-minutes "
                f"declarado - esperado {esperado}"
            )
            continue
        if timeout != esperado:
            violacoes.append(
                f"{filename}: job {name!r} (classe {classe!r}) timeout-minutes={timeout}, "
                f"esperado {esperado}"
            )
            continue
        contagem[classe] += 1

    for nome_esperado in table:
        if nome_esperado not in job_names:
            violacoes.append(
                f"{filename}: entrada morta - {nome_esperado!r} esta na tabela EXPECTED "
                "mas nao existe mais no arquivo"
            )

    return job_names, violacoes, contagem


# --- modo real: varre o DIRETORIO -------------------------------------


def list_workflow_files(dir_path):
    """Devolve dict ORDENADO {nome_arquivo: texto} de todo *.yml/*.yaml
    dentro de dir_path (nao recursivo - e' o mesmo nivel que
    .github/workflows/ tem hoje). None se dir_path nao existir; dict
    VAZIO (distinto de None) se existir e nao tiver nenhum workflow -
    e' esse o caso que aciona o piso de varredura vazia do nivel
    'diretorio'."""
    if not os.path.isdir(dir_path):
        return None
    nomes = sorted(
        n for n in os.listdir(dir_path) if n.endswith(".yml") or n.endswith(".yaml")
    )
    arquivos = {}
    for nome in nomes:
        with open(os.path.join(dir_path, nome), "r", encoding="utf-8") as handle:
            arquivos[nome] = handle.read()
    return arquivos


def real_main(args):
    if len(args) != 1:
        fail("usage: check_ci_timeouts.py --check <diretorio .github/workflows>")
    (dir_path,) = args

    arquivos = list_workflow_files(dir_path)
    if arquivos is None:
        fail(f"diretorio nao encontrado: {dir_path}")
    if not arquivos:
        fail(
            f"varredura vazia: nenhum arquivo *.yml/*.yaml em {dir_path} - "
            "GODS_LAWS.md L-40, isto e' sinal de portao quebrado (diretorio "
            "renomeado/movido), nunca de ausencia real de workflow"
        )

    total_jobs = 0
    todas_violacoes = []
    contagem_total = Counter()
    arquivos_nao_classificados = []

    for filename in sorted(arquivos):
        table = EXPECTED.get(filename)
        if table is None:
            arquivos_nao_classificados.append(filename)
            continue
        job_names, violacoes, contagem = classify_file(filename, arquivos[filename], table)
        todas_violacoes.extend(violacoes)
        if job_names is not None:
            total_jobs += len(job_names)
        contagem_total.update(contagem)

    for filename in arquivos_nao_classificados:
        todas_violacoes.append(
            f"{filename}: workflow novo - classifique os jobs dele em EXPECTED "
            "(check_ci_timeouts.py)"
        )

    resumo_classes = " ".join(f"{c}={contagem_total.get(c, 0)}" for c in CLASSES)
    print(
        f"{SCRIPT_NAME}: {len(arquivos)} arquivo(s) de workflow, {total_jobs} job(s), "
        f"{sum(contagem_total.values())} com teto declarado ({resumo_classes})"
    )

    if todas_violacoes:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(todas_violacoes)} violacao(oes)):", file=sys.stderr)
        for v in todas_violacoes:
            print(f"  - {v}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: OK")


# --- fixtures e controles para --selftest ----------------------------------


_FIXTURE_TABLE_DOIS_JOBS = {"a": "curta", "b": "compilacao"}

_FAKE_DOIS_JOBS_OK = """\
jobs:
  a:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - run: echo a
  b:
    runs-on: ubuntu-latest
    timeout-minutes: 40
    steps:
      - run: echo b
"""

_FAKE_UM_SEM_TIMEOUT = """\
jobs:
  a:
    runs-on: ubuntu-latest
    steps:
      - run: echo a
  b:
    runs-on: ubuntu-latest
    timeout-minutes: 40
    steps:
      - run: echo b
"""

_FAKE_TETO_FORA_DA_CLASSE = """\
jobs:
  a:
    runs-on: ubuntu-latest
    timeout-minutes: 25
    steps:
      - run: echo a
  b:
    runs-on: ubuntu-latest
    timeout-minutes: 40
    steps:
      - run: echo b
"""

_FAKE_JOB_NOVO = """\
jobs:
  a:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - run: echo a
  b:
    runs-on: ubuntu-latest
    timeout-minutes: 40
    steps:
      - run: echo b
  c:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - run: echo c
"""

_FAKE_ENTRADA_MORTA = """\
jobs:
  a:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - run: echo a
"""

_FIXTURE_TABLE_REUSABLE = {"a": "curta", "chamador": REUSABLE_MARK}

_FAKE_REUSABLE_COM_TETO = """\
jobs:
  a:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - run: echo a
  chamador:
    uses: ./.github/workflows/outro.yml
    timeout-minutes: 15
"""

_FAKE_REUSABLE_SEM_TETO_OK = """\
jobs:
  a:
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - run: echo a
  chamador:
    uses: ./.github/workflows/outro.yml
"""

_FAKE_JOBS_VAZIO = """\
jobs:
"""

_FAKE_SEM_ANCORA = """\
on:
  push:
    branches: [main]
# nao existe 'jobs:' neste arquivo de propósito
"""

_FAKE_TIMEOUT_SO_EM_COMENTARIO = """\
jobs:
  a:
    runs-on: ubuntu-latest
    # timeout-minutes: 10
    steps:
      - run: echo a
  b:
    runs-on: ubuntu-latest
    timeout-minutes: 40
    steps:
      - run: echo b
"""

_FAKE_TIMEOUT_INDENTACAO_DE_PASSO = """\
jobs:
  a:
    runs-on: ubuntu-latest
    steps:
      - name: passo com teto proprio, nao do trabalho
        timeout-minutes: 5
        run: echo a
  b:
    runs-on: ubuntu-latest
    timeout-minutes: 40
    steps:
      - run: echo b
"""


def _run_real_main_capturing(args):
    """args no formato de linha de comando completo, ex.:
    ['--check', '<dir>'] - esta funcao despacha exatamente como main()
    despacharia (tira o '--check' antes de chamar real_main), capturando
    saida e SystemExit. Devolve (codigo_de_saida,
    saida_combinada_stdout_stderr). Existe para testar o MECANISMO real
    (enumeracao de diretorio, sys.exit), nao so' a logica pura de
    classify_file."""
    assert args and args[0] == "--check", f"_run_real_main_capturing espera ['--check', ...], recebeu {args!r}"
    buf_out = io.StringIO()
    buf_err = io.StringIO()
    rc = 0
    try:
        with contextlib.redirect_stdout(buf_out), contextlib.redirect_stderr(buf_err):
            real_main(args[1:])
    except SystemExit as exc:
        rc = exc.code if isinstance(exc.code, int) else 1
    return rc, buf_out.getvalue() + buf_err.getvalue()


def selftest_positivo_dois_jobs_tetos_certos():
    job_names, violacoes, contagem = classify_file("f.yml", _FAKE_DOIS_JOBS_OK, _FIXTURE_TABLE_DOIS_JOBS)
    if job_names != ["a", "b"] or violacoes or contagem != Counter({"curta": 1, "compilacao": 1}):
        print(
            f"selftest: POSITIVO (dois jobs, tetos certos) FALHOU: jobs={job_names} "
            f"violacoes={violacoes} contagem={contagem}",
            file=sys.stderr,
        )
        return False
    print("selftest: POSITIVO (dois jobs, tetos certos) OK")
    return True


def selftest_negativo_sem_timeout_minutes():
    job_names, violacoes, _contagem = classify_file("f.yml", _FAKE_UM_SEM_TIMEOUT, _FIXTURE_TABLE_DOIS_JOBS)
    if not violacoes or not any("job 'a'" in v and "sem timeout-minutes" in v for v in violacoes):
        print(f"selftest: NEGATIVO (sem timeout-minutes) FALHOU: jobs={job_names} violacoes={violacoes}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (sem timeout-minutes) OK, citado: {violacoes}")
    return True


def selftest_negativo_teto_fora_da_classe():
    _job_names, violacoes, _contagem = classify_file("f.yml", _FAKE_TETO_FORA_DA_CLASSE, _FIXTURE_TABLE_DOIS_JOBS)
    esperado_msg = "job 'a' (classe 'curta') timeout-minutes=25, esperado 10"
    if not any(esperado_msg in v for v in violacoes):
        print(f"selftest: NEGATIVO (teto fora da classe) FALHOU: violacoes={violacoes}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (teto fora da classe) OK, citado esperado x encontrado: {violacoes}")
    return True


def selftest_negativo_job_novo():
    _job_names, violacoes, _contagem = classify_file("f.yml", _FAKE_JOB_NOVO, _FIXTURE_TABLE_DOIS_JOBS)
    if not any("'c' novo" in v for v in violacoes):
        print(f"selftest: NEGATIVO (job novo) FALHOU: violacoes={violacoes}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (job novo) OK, citado: {violacoes}")
    return True


def selftest_negativo_entrada_morta():
    _job_names, violacoes, _contagem = classify_file("f.yml", _FAKE_ENTRADA_MORTA, _FIXTURE_TABLE_DOIS_JOBS)
    if not any("entrada morta - 'b'" in v for v in violacoes):
        print(f"selftest: NEGATIVO (entrada morta) FALHOU: violacoes={violacoes}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (entrada morta) OK, citado: {violacoes}")
    return True


def selftest_negativo_reusable_com_teto():
    _job_names, violacoes, _contagem = classify_file("f.yml", _FAKE_REUSABLE_COM_TETO, _FIXTURE_TABLE_REUSABLE)
    if not any("chamador" in v and "reusable workflow" in v for v in violacoes):
        print(f"selftest: NEGATIVO (reusable com teto) FALHOU: violacoes={violacoes}", file=sys.stderr)
        return False
    # controle-espelho, no mesmo fixture-family: reusable SEM teto tem
    # de passar limpo, senao a excecao nao serve para nada.
    _job_names_ok, violacoes_ok, _c = classify_file("f.yml", _FAKE_REUSABLE_SEM_TETO_OK, _FIXTURE_TABLE_REUSABLE)
    if violacoes_ok:
        print(f"selftest: NEGATIVO (reusable com teto) FALHOU no espelho (sem teto deveria passar): {violacoes_ok}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (reusable com teto) OK, citado: {violacoes}; espelho sem teto OK")
    return True


def selftest_varredura_vazia_diretorio_sem_yml():
    with tempfile.TemporaryDirectory() as tmp:
        rc, saida = _run_real_main_capturing(["--check", tmp])
    if rc != 1 or "varredura vazia" not in saida:
        print(f"selftest: VARREDURA VAZIA (diretorio sem .yml) FALHOU: rc={rc} saida={saida!r}", file=sys.stderr)
        return False
    print("selftest: VARREDURA VAZIA (diretorio sem .yml) OK, reprovou com rc=1")
    return True


def selftest_varredura_vazia_jobs_sem_trabalho():
    with tempfile.TemporaryDirectory() as tmp:
        with open(os.path.join(tmp, "ci.yml"), "w", encoding="utf-8") as f:
            f.write(_FAKE_JOBS_VAZIO)
        rc, saida = _run_real_main_capturing(["--check", tmp])
    if rc != 1 or "bloco 'jobs:' existe mas nao tem nenhum trabalho" not in saida:
        print(f"selftest: VARREDURA VAZIA (jobs: sem trabalho) FALHOU: rc={rc} saida={saida!r}", file=sys.stderr)
        return False
    print("selftest: VARREDURA VAZIA (jobs: sem trabalho) OK, reprovou com rc=1")
    return True


def selftest_varredura_vazia_sem_ancora():
    with tempfile.TemporaryDirectory() as tmp:
        with open(os.path.join(tmp, "ci.yml"), "w", encoding="utf-8") as f:
            f.write(_FAKE_SEM_ANCORA)
        rc, saida = _run_real_main_capturing(["--check", tmp])
    if rc != 1 or "sem a ancora 'jobs:'" not in saida:
        print(f"selftest: VARREDURA VAZIA (sem ancora jobs:) FALHOU: rc={rc} saida={saida!r}", file=sys.stderr)
        return False
    print("selftest: VARREDURA VAZIA (sem ancora jobs:) OK, reprovou com rc=1")
    return True


def selftest_negativo_timeout_so_em_comentario():
    _job_names, violacoes, _contagem = classify_file(
        "f.yml", _FAKE_TIMEOUT_SO_EM_COMENTARIO, _FIXTURE_TABLE_DOIS_JOBS
    )
    if not any("job 'a'" in v and "sem timeout-minutes" in v for v in violacoes):
        print(f"selftest: NEGATIVO (timeout so' em comentario) FALHOU: violacoes={violacoes}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (timeout so' em comentario) OK - comentario nao conta como declaracao: {violacoes}")
    return True


def selftest_negativo_timeout_indentacao_de_passo():
    _job_names, violacoes, _contagem = classify_file(
        "f.yml", _FAKE_TIMEOUT_INDENTACAO_DE_PASSO, _FIXTURE_TABLE_DOIS_JOBS
    )
    if not any("job 'a'" in v and "sem timeout-minutes" in v for v in violacoes):
        print(f"selftest: NEGATIVO (timeout de passo) FALHOU: violacoes={violacoes}", file=sys.stderr)
        return False
    print(f"selftest: NEGATIVO (timeout de passo) OK - teto de passo (6+ espacos) nao conta como teto de trabalho: {violacoes}")
    return True


def selftest_arvore_real(dir_path):
    """Sanidade final contra a arvore REAL (nao fixture): os 15
    trabalhos de .github/workflows/ci.yml tem de aprovar hoje - se
    este controle falhar, a fatia CI-TIMEOUTS regrediu."""
    if not os.path.isdir(dir_path):
        print(f"selftest: ARVORE REAL - diretorio nao encontrado ({dir_path}), controle pulado")
        return True
    rc, saida = _run_real_main_capturing(["--check", dir_path])
    if rc != 0:
        print(f"selftest: ARVORE REAL FALHOU: rc={rc} saida={saida!r}", file=sys.stderr)
        return False
    if "15 job(s), 15 com teto declarado" not in saida:
        print(f"selftest: ARVORE REAL FALHOU (contagem inesperada): {saida!r}", file=sys.stderr)
        return False
    print(f"selftest: ARVORE REAL OK: {saida.strip()}")
    return True


def selftest_bonus_arquivo_nao_classificado():
    """Extra alem dos 12 controles nomeados no plano: o mecanismo do
    §5.2 passo 2 (arquivo de workflow novo, fora de EXPECTED, reprova
    'workflow novo: classifique-o') precisa de prova tambem, mesmo sem
    ter numero proprio na tabela do plano."""
    with tempfile.TemporaryDirectory() as tmp:
        with open(os.path.join(tmp, "release.yml"), "w", encoding="utf-8") as f:
            f.write(_FAKE_DOIS_JOBS_OK)
        rc, saida = _run_real_main_capturing(["--check", tmp])
    if rc != 1 or "release.yml: workflow novo" not in saida:
        print(f"selftest: BONUS (arquivo nao classificado) FALHOU: rc={rc} saida={saida!r}", file=sys.stderr)
        return False
    print("selftest: BONUS (arquivo nao classificado) OK, reprovou pedindo classificacao")
    return True


def selftest_main(dir_path):
    controls = [
        selftest_positivo_dois_jobs_tetos_certos(),
        selftest_negativo_sem_timeout_minutes(),
        selftest_negativo_teto_fora_da_classe(),
        selftest_negativo_job_novo(),
        selftest_negativo_entrada_morta(),
        selftest_negativo_reusable_com_teto(),
        selftest_varredura_vazia_diretorio_sem_yml(),
        selftest_varredura_vazia_jobs_sem_trabalho(),
        selftest_varredura_vazia_sem_ancora(),
        selftest_negativo_timeout_so_em_comentario(),
        selftest_negativo_timeout_indentacao_de_passo(),
        selftest_arvore_real(dir_path),
        selftest_bonus_arquivo_nao_classificado(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        dir_path = args[1] if len(args) > 1 else "../.github/workflows"
        selftest_main(dir_path)
    elif args and args[0] == "--check":
        real_main(args[1:])
    else:
        fail(
            "usage: check_ci_timeouts.py --check <diretorio .github/workflows>  |  "
            "--selftest [diretorio .github/workflows]"
        )


if __name__ == "__main__":
    main()
