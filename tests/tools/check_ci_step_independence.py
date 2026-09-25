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


# --- A3c (D-A13): G1/G2 - checkout de bootstrap sem git, prova do -----
# git logo depois do checkout final -------------------------------------
#
# A REGRESSAO real que estas duas regras existem para pegar (run
# 36110716884, ci.yml de 4a3f1fd - MO2, o vermelho de estreia): um job
# com `container:` fazia um UNICO checkout, ANTES de "Instalar
# toolchain" - o container ainda nao tinha git nenhum, o checkout caia
# no fallback REST API do actions/checkout (tarball, sem `.git`), e TODO
# passo seguinte que usasse git morria com "git: command not found"/
# "not a git repository". G1 exige que nenhum passo ANTES do checkout
# FINAL (o ultimo checkout SEM `path:`) use git, e que todo checkout
# ANTERIOR a ele (o de bootstrap) tenha `path:` - nunca dois checkouts
# escrevendo no MESMO diretorio. G2 exige que o passo LOGO DEPOIS do
# checkout final prove que ele e' um clone git de verdade (`git
# rev-parse`), nunca assumido.

_CONTAINER_KEY_RE = re.compile(r"^    container:\s*\S", re.MULTILINE)
_CHECKOUT_USES_RE = re.compile(r"uses:\s*actions/checkout@")
_PATH_KEY_RE = re.compile(r"^\s*path:\s*\S", re.MULTILINE)
# "git" como INICIO de um comando shell (inicio de linha, ou logo apos
# um separador de comando - ";"/"&&"/"||"/"|"/"$(") - nunca "git" como
# PACOTE numa lista de instalacao (`dnf -y install ... git ...`, onde
# ele aparece no MEIO/FIM de uma lista de argumentos de `install`,
# nunca como primeiro token de um comando). Achado real: a forma
# antiga ("git" solto em qualquer lugar da linha) dava falso positivo
# nos 5 jobs de container fixo (lint/sanitizer/gl-codegen-host-cross/
# debug/clang), cujo "Instalar toolchain" so' MENCIONA o pacote "git"
# dentro de `dnf -y install ...` - main confirmou que esses 5 jobs ja
# satisfazem G1 como estao (a ordem deles, instalar->checkout, nunca
# muda: o checkout roda com git JA instalado, nao cai no fallback).
_GIT_COMMAND_START_RE = re.compile(r"(?:^|[;&|]|\$\()\s*git\s+\S")
_GIT_REV_PARSE_RE = re.compile(r"git rev-parse")
_USES_LINE_RE = re.compile(r"^\s*-?\s*uses:", re.MULTILINE)


def job_has_container(job_block_text):
    return bool(_CONTAINER_KEY_RE.search(job_block_text))


def is_checkout_step(step_text):
    return bool(_CHECKOUT_USES_RE.search(step_text))


def checkout_has_path(step_text):
    return bool(_PATH_KEY_RE.search(step_text))


_RUN_PREFIX_RE = re.compile(r"^(?:-\s*)?run:\s*(.*)$")


def step_uses_git(step_text):
    """Passo que roda algum comando git DENTRO do proprio run: - nunca
    conta a linha `uses: actions/checkout@...` (e' a Action, nao um
    comando git direto) nem comentario/linha em branco.

    Achado real (revisao do main, MO1 verbatim da D-A13: mover
    "Configurar diretorio seguro do git" para antes do checkout final
    escapava): a forma de UMA linha `run: git config ...` tem o comando
    de verdade DEPOIS do prefixo "run: " - _GIT_COMMAND_START_RE exige
    "git" logo no INICIO da linha (ou apos ';'/'&&'/etc.), e a linha
    inteira comeca com "run:", nunca com "git". O prefixo "run:" (ou
    "- run:") e' removido ANTES de testar cada linha - cobre tanto a
    forma de uma linha quanto `run: |` (onde a marca "|" sozinha vira
    linha vazia, pulada, e as linhas seguintes do bloco ja vem SEM
    prefixo, como antes)."""
    for raw_line in step_text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if _USES_LINE_RE.match(line):
            continue
        m = _RUN_PREFIX_RE.match(line)
        if m:
            line = m.group(1).strip()
            if not line or line in ("|", ">-", ">", "|-"):
                continue
        if _GIT_COMMAND_START_RE.search(line):
            return True
    return False


_PACKAGE_MANAGER_INSTALL_RE = re.compile(r"dnf -y install|apt-get install|pacman -Syu?[^\n]*--noconfirm")
_GIT_PACKAGE_TOKEN_RE = re.compile(r"(?<![\w-])git(?![\w-])")


def step_installs_git_package(step_text):
    """Passo cujo `run:` instala o PACOTE 'git' via um gerenciador de
    pacote conhecido (dnf/apt-get/pacman) - distinto de step_uses_git()
    (que procura "git" como COMANDO): aqui "git" e' argumento de uma
    lista de instalacao, nunca o primeiro token de um comando. Testa o
    texto INTEIRO do passo achatado (nao linha a linha) - `run: >-` do
    YAML dobra o comando de instalacao em VARIAS linhas fisicas (nome
    do pacote/gerenciador podem cair em linhas de texto diferentes)."""
    achatado = " ".join(step_text.split())
    return bool(_PACKAGE_MANAGER_INSTALL_RE.search(achatado) and _GIT_PACKAGE_TOKEN_RE.search(achatado))


def g1_errors(job_name, job_block_text, steps):
    if not job_has_container(job_block_text):
        return []
    errors = []
    checkout_indices = [i for i, (_n, t) in enumerate(steps) if is_checkout_step(t)]
    checkouts_sem_path = [i for i in checkout_indices if not checkout_has_path(steps[i][1])]
    if not checkouts_sem_path:
        errors.append(
            f"job {job_name!r}: nenhum checkout SEM 'path:' encontrado - G1 exige um checkout "
            f"final (definitivo)"
        )
        return errors
    final_idx = checkouts_sem_path[-1]

    # MO2 (o vermelho de estreia real, ci.yml de 4a3f1fd, run
    # 36110716884): UM UNICO checkout, sem separacao de bootstrap, ANTES
    # de qualquer instalacao - o container ainda nao tinha git nenhum, e
    # o checkout caia no fallback REST API. Achado real (main): os 5
    # jobs de container fixo (lint/sanitizer/gl-codegen-host-cross/
    # debug/clang) TAMBEM tem um checkout unico, sem bootstrap - mas sao
    # CORRETOS, porque "Instalar toolchain" (que ja instala o pacote
    # git) roda ANTES desse checkout unico. A distincao real nao e' "tem
    # checkout de bootstrap ou nao" - e' "quando o checkout final e' o
    # UNICO do job, existe ALGUM passo antes dele que instala o pacote
    # git?". Com bootstrap (job `linux`) ou com instalacao antes do
    # checkout unico (os 5 jobs fixos) - as DUAS formas satisfazem G1.
    checkouts_antes = [i for i in checkout_indices if i < final_idx]
    if not checkouts_antes:
        instala_git_antes = any(step_installs_git_package(steps[i][1]) for i in range(final_idx))
        if not instala_git_antes:
            errors.append(
                f"job {job_name!r}: o checkout final e' o UNICO checkout do job, e nenhum passo "
                f"antes dele instala o pacote git - G1 (MO2: a forma que causou a regressao "
                f"real, run 36110716884 - checkout caindo no fallback REST API antes do git "
                f"existir)"
            )

    for i in range(final_idx):
        nome, texto = steps[i]
        if step_uses_git(texto):
            errors.append(
                f"job {job_name!r}, passo {nome!r}: usa git ANTES do checkout final (indice "
                f"{i} < {final_idx}) - G1, GLINTFX-BOOTSTRAP-NOGIT"
            )
    for i in checkout_indices:
        if i >= final_idx:
            continue
        nome, texto = steps[i]
        if not checkout_has_path(texto):
            errors.append(
                f"job {job_name!r}, passo {nome!r}: checkout antes do preparo sem 'path:' - G1"
            )
    return errors


def g2_errors(job_name, job_block_text, steps):
    if not job_has_container(job_block_text):
        return []
    checkout_indices = [i for i, (_n, t) in enumerate(steps) if is_checkout_step(t)]
    checkouts_sem_path = [i for i in checkout_indices if not checkout_has_path(steps[i][1])]
    if not checkouts_sem_path:
        return []  # ja reportado por G1
    final_idx = checkouts_sem_path[-1]

    # Restrito ao padrao de checkout DUPLO (bootstrap + final) - decisao
    # explicada no relatorio da fatia, a confirmar por main: nos 5 jobs
    # de container fixo (checkout UNICO, git instalado ANTES dele por
    # "Instalar toolchain" - G1 ja confirma isso), o checkout ja roda
    # com git garantido, entao a prova extra de G2 e' menos critica ali
    # do que no padrao bootstrap (onde o checkout REAL precisa provar
    # que nao caiu no mesmo fallback que o de bootstrap usa por
    # desenho).
    checkouts_antes = [i for i in checkout_indices if i < final_idx]
    if not checkouts_antes:
        return []

    if final_idx + 1 >= len(steps):
        return [
            f"job {job_name!r}: checkout final e' o ULTIMO passo do job - G2 exige a prova do "
            f"git logo depois"
        ]
    nome_prova, texto_prova = steps[final_idx + 1]
    if not _GIT_REV_PARSE_RE.search(texto_prova):
        return [
            f"job {job_name!r}, passo {nome_prova!r}: nao prova git ('git rev-parse') logo "
            f"apos o checkout final - G2"
        ]
    return []


# --- A3c (D-A13): G3 - exatamente um id: prep por job, id: build -------
# (quando existir) vem depois dele ---------------------------------------

_STEP_ID_RE = re.compile(r"^\s*id:\s*(\S+)\s*$", re.MULTILINE)


def step_id(step_text):
    m = _STEP_ID_RE.search(step_text)
    return m.group(1) if m else None


def g3_errors(job_name, steps):
    prep_indices = [i for i, (_n, t) in enumerate(steps) if step_id(t) == "prep"]
    build_indices = [i for i, (_n, t) in enumerate(steps) if step_id(t) == "build"]
    errors = []
    if len(prep_indices) != 1:
        errors.append(
            f"job {job_name!r}: {len(prep_indices)} passo(s) com id: prep, esperado "
            f"exatamente 1 - G3"
        )
        return errors
    prep_idx = prep_indices[0]
    for bi in build_indices:
        if bi <= prep_idx:
            errors.append(
                f"job {job_name!r}: id: build (passo {steps[bi][0]!r}) nao vem DEPOIS de "
                f"id: prep - G3"
            )
    return errors


# --- veredito completo ----------------------------------------------


def run_check(job_name, job_block_text, steps):
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
    errors.extend(g1_errors(job_name, job_block_text, steps))
    errors.extend(g2_errors(job_name, job_block_text, steps))
    errors.extend(g3_errors(job_name, steps))
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
    counts, errors = run_check(job_name, job_block, steps)
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
    container: fedora:latest
    steps:
      - name: Checkout de bootstrap (so' o preparo)
        uses: actions/checkout@v7
        with:
          path: _bootstrap

      - name: Instalar toolchain
        run: bash _bootstrap/tools/ci/env/fedora.sh

      - name: Remove o checkout de bootstrap
        run: rm -rf _bootstrap

      - uses: actions/checkout@v7

      - name: Checkout e' repositorio git
        run: |
          git config --global --add safe.directory "$GITHUB_WORKSPACE"
          git rev-parse --is-inside-work-tree

      - name: Preparo concluido
        id: prep
        run: echo "preparo concluido"

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


# A3c (D-A13), MO2: o VERMELHO DE ESTREIA real - o ci.yml de 4a3f1fd
# (run 36110716884) tinha UM UNICO checkout, sem separacao de
# bootstrap, ANTES de "Instalar toolchain". Reproduzido aqui removendo
# o checkout de bootstrap inteiro e o passo "Instalar toolchain"
# continuando a depender do repo - a mesma forma real.
def selftest_mo2_single_checkout_no_bootstrap_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "      - name: Checkout de bootstrap (so' o preparo)\n"
        "        uses: actions/checkout@v7\n"
        "        with:\n"
        "          path: _bootstrap\n\n"
        "      - name: Instalar toolchain\n"
        "        run: bash _bootstrap/tools/ci/env/fedora.sh\n\n"
        "      - name: Remove o checkout de bootstrap\n"
        "        run: rm -rf _bootstrap\n\n"
        "      - uses: actions/checkout@v7\n",
        "      - uses: actions/checkout@v7\n\n"
        "      - name: Instalar toolchain\n"
        "        run: bash tools/ci/env/fedora.sh\n",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: MO2-CHECKOUT-UNICO-SEM-BOOTSTRAP FALHOU (deveria ter reprovado): {output}", file=sys.stderr)
        return False
    if "nenhum passo antes dele instala o pacote git" not in output:
        print(f"selftest: MO2-CHECKOUT-UNICO-SEM-BOOTSTRAP FALHOU (reprovou, mas nao pela causa certa): {output!r}", file=sys.stderr)
        return False
    print("selftest: MO2-CHECKOUT-UNICO-SEM-BOOTSTRAP OK (vermelho de estreia real, run 36110716884, reproduzido e pego)")
    return True


# MO1: safe.directory (comando git) movido para DENTRO do passo
# "Instalar toolchain" - que roda ANTES do checkout final. G1 tem de
# pegar isso mesmo com o checkout de bootstrap presente.
def selftest_mo1_git_before_final_checkout_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "      - name: Instalar toolchain\n"
        "        run: bash _bootstrap/tools/ci/env/fedora.sh\n",
        "      - name: Instalar toolchain\n"
        "        run: |\n"
        "          git config --global --add safe.directory \"$GITHUB_WORKSPACE\"\n"
        "          bash _bootstrap/tools/ci/env/fedora.sh\n",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: MO1-GIT-ANTES-DO-FINAL FALHOU (deveria ter reprovado): {output}", file=sys.stderr)
        return False
    if "'Instalar toolchain'" not in output or "usa git ANTES" not in output:
        print(f"selftest: MO1-GIT-ANTES-DO-FINAL FALHOU (reprovou, mas nao pela causa certa): {output!r}", file=sys.stderr)
        return False
    print("selftest: MO1-GIT-ANTES-DO-FINAL OK (passo com git antes do checkout final pego)")
    return True


# MO1b (achado REAL do main, revisao contra a arvore, forma exata do
# CTO): o passo DEDICADO "Configurar diretorio seguro do git" (`run:
# git config ...`, forma de UMA LINHA, nunca `run: |`) movido para
# ANTES do checkout final - ESCAPAVA da forma antiga de step_uses_git()
# porque a linha inteira comeca com "run:", nunca com "git" (o comando
# de verdade vem DEPOIS do prefixo "run: ", que _GIT_COMMAND_START_RE
# nao reconhecia como separador). Vermelho de estreia reproduzido com
# a MESMA fixture que o main usou (mo1.yml).
def selftest_mo1b_dedicated_safedirectory_step_before_final_checkout_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "      - name: Remove o checkout de bootstrap\n"
        "        run: rm -rf _bootstrap\n\n"
        "      - uses: actions/checkout@v7\n\n"
        "      - name: Checkout e' repositorio git\n"
        "        run: |\n"
        "          git config --global --add safe.directory \"$GITHUB_WORKSPACE\"\n"
        "          git rev-parse --is-inside-work-tree\n\n",
        "      - name: Remove o checkout de bootstrap\n"
        "        run: rm -rf _bootstrap\n\n"
        "      - name: Configurar diretorio seguro do git\n"
        "        run: git config --global --add safe.directory \"$GITHUB_WORKSPACE\"\n\n"
        "      - uses: actions/checkout@v7\n\n"
        "      - name: Checkout e' repositorio git\n"
        "        run: |\n"
        "          git rev-parse --is-inside-work-tree\n\n",
        1,
    )
    steps_extractor_ok = "Configurar diretorio seguro do git" in quebrado
    assert steps_extractor_ok, "fixture MO1b mal formada"
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: MO1B-SAFEDIRECTORY-DEDICADO-ANTES-DO-FINAL FALHOU (deveria ter reprovado): {output}", file=sys.stderr)
        return False
    if "'Configurar diretorio seguro do git'" not in output or "usa git ANTES" not in output:
        print(f"selftest: MO1B-SAFEDIRECTORY-DEDICADO-ANTES-DO-FINAL FALHOU (reprovou, mas nao pela causa certa): {output!r}", file=sys.stderr)
        return False
    print("selftest: MO1B-SAFEDIRECTORY-DEDICADO-ANTES-DO-FINAL OK (achado real do main, mo1.yml, reproduzido e pego)")
    return True


# MO4: a prova do git (passo "Checkout e' repositorio git") removida -
# G2 tem de pegar que o passo LOGO DEPOIS do checkout final nao prova
# git nenhum.
def selftest_mo4_git_proof_removed_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "      - name: Checkout e' repositorio git\n"
        "        run: |\n"
        "          git config --global --add safe.directory \"$GITHUB_WORKSPACE\"\n"
        "          git rev-parse --is-inside-work-tree\n\n",
        "",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: MO4-PROVA-GIT-REMOVIDA FALHOU (deveria ter reprovado): {output}", file=sys.stderr)
        return False
    if "nao prova git" not in output:
        print(f"selftest: MO4-PROVA-GIT-REMOVIDA FALHOU (reprovou, mas nao pela causa certa): {output!r}", file=sys.stderr)
        return False
    print("selftest: MO4-PROVA-GIT-REMOVIDA OK (ausencia da prova do git logo apos o checkout final pega)")
    return True


# MO5: job sem `id: prep` nenhum - G3 tem de reprovar citando a
# contagem (0, esperado exatamente 1).
def selftest_mo5_job_without_prep_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "      - name: Preparo concluido\n"
        "        id: prep\n"
        "        run: echo \"preparo concluido\"\n\n",
        "",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: MO5-SEM-PREP FALHOU (deveria ter reprovado): {output}", file=sys.stderr)
        return False
    if "id: prep, esperado exatamente 1" not in output:
        print(f"selftest: MO5-SEM-PREP FALHOU (reprovou, mas nao pela causa certa): {output!r}", file=sys.stderr)
        return False
    print("selftest: MO5-SEM-PREP OK (job sem id: prep pego, G3)")
    return True


# G3, controle negativo/gemeo: `id: build` ANTES de `id: prep` (ordem
# invertida) tem de reprovar - nao basta os dois existirem, a ORDEM
# importa.
def selftest_g3_build_before_prep_reproves():
    quebrado = _FIXTURE_CI_YML.replace(
        "      - name: Preparo concluido\n"
        "        id: prep\n"
        "        run: echo \"preparo concluido\"\n\n"
        "      - name: Sobe o compositor limpo\n"
        "        id: build\n"
        "        run: docker run -d --name c glintfx-wltest:ci\n",
        "      - name: Sobe o compositor limpo\n"
        "        id: build\n"
        "        run: docker run -d --name c glintfx-wltest:ci\n\n"
        "      - name: Preparo concluido\n"
        "        id: prep\n"
        "        run: echo \"preparo concluido\"\n",
        1,
    )
    exit_code, output = _run_real_main_capturing(quebrado)
    if exit_code != 1:
        print(f"selftest: G3-BUILD-ANTES-DE-PREP FALHOU (deveria ter reprovado): {output}", file=sys.stderr)
        return False
    if "nao vem DEPOIS de id: prep" not in output:
        print(f"selftest: G3-BUILD-ANTES-DE-PREP FALHOU (reprovou, mas nao pela causa certa): {output!r}", file=sys.stderr)
        return False
    print("selftest: G3-BUILD-ANTES-DE-PREP OK (id: build antes de id: prep pego)")
    return True


# G1, controle negativo: job SEM `container:` nunca e' varrido por
# G1/G2 (a regra so' se aplica a jobs que rodam DENTRO de um container
# - um job em runner nativo, ex. `windows`, checkout roda no host, que
# ja tem git).
# G1, controle positivo (achado real do main): job com CONTAINER e UM
# UNICO checkout, mas com "Instalar toolchain" (que ja instala o
# pacote git) ANTES desse checkout - o padrao real dos 5 jobs de
# container fixo (lint/sanitizer/gl-codegen-host-cross/debug/clang) -
# NUNCA pode reprovar. Repete o mutante MO2 ao contrario: mesma forma
# de UM checkout so', mas com a instalacao de git ja tendo acontecido
# antes dele - o que faz TODA a diferenca (o checkout roda com git ja
# disponivel, nao cai no fallback REST API).
def selftest_g1_single_checkout_with_prior_git_install_does_not_reprove():
    job_block = """  lint:
    container: fedora:latest
    steps:
      - name: Instalar toolchain e ferramentas de lint
        run: >-
          dnf -y install gcc-c++ cmake ninja-build pkgconf-pkg-config git
          wayland-devel wayland-protocols-devel libglvnd-devel

      - uses: actions/checkout@v7

      - name: preci.sh --selftest
        run: |
          git config --global --add safe.directory "$GITHUB_WORKSPACE"
          git rev-parse --is-inside-work-tree

      - name: Preparo concluido
        id: prep
        run: echo ok

      - name: preci.sh --lint-only
        id: build
        run: tools/preci.sh --lint-only
"""
    steps = split_steps(job_block)
    errors = g1_errors("lint", job_block, steps) + g2_errors("lint", job_block, steps)
    if errors:
        print(
            f"selftest: G1-CHECKOUT-UNICO-COM-INSTALACAO-PREVIA FALHOU (padrao real dos 5 "
            f"jobs de container fixo nao deveria reprovar): {errors}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: G1-CHECKOUT-UNICO-COM-INSTALACAO-PREVIA OK (checkout unico com git ja "
        "instalado antes dele nunca reprova - o padrao real de lint/sanitizer/etc.)"
    )
    return True


def selftest_g1_job_without_container_not_checked():
    sem_container = """jobs:
  windows:
    steps:
      - uses: actions/checkout@v7
      - name: Instalar CMake
        run: echo instala
      - name: Preparo concluido
        id: prep
        run: echo ok
      - name: Build
        id: build
        run: echo build
      - name: Testes
        if: ${{ !cancelled() && steps.build.outcome == 'success' }}
        run: |
          echo "t" >> parity_inventory.txt
          codigo=0
          tests/container/exec_fixture.sh c t || codigo=$?
          printf 't\\t%s\\n' "$codigo" >> results.tsv
          exit "$codigo"
"""
    exit_code, output = _run_real_main_capturing(sem_container, job_name="windows")
    if exit_code not in (None, 0):
        print(f"selftest: G1-SEM-CONTAINER-CONTROLE FALHOU (job sem container: nao deveria acionar G1/G2): {output}", file=sys.stderr)
        return False
    print("selftest: G1-SEM-CONTAINER-CONTROLE OK (G1/G2 restritos a job com container:)")
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
        selftest_mo2_single_checkout_no_bootstrap_reproves(),
        selftest_mo1_git_before_final_checkout_reproves(),
        selftest_mo1b_dedicated_safedirectory_step_before_final_checkout_reproves(),
        selftest_mo4_git_proof_removed_reproves(),
        selftest_mo5_job_without_prep_reproves(),
        selftest_g3_build_before_prep_reproves(),
        selftest_g1_single_checkout_with_prior_git_install_does_not_reprove(),
        selftest_g1_job_without_container_not_checked(),
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
