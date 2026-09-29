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

import importlib.util
import os
import re
import sys

SCRIPT_NAME = "check_ci_step_independence.py"

DEFAULT_JOB = "wayland-container"

# Scripts que o ci.yml chama e que o portao le junto (D-A14): a prova do
# clone git (G2), o piso Linux e o preparo Windows (G5). Caminho relativo
# a raiz do repo - o portao deriva a raiz do proprio caminho do ci.yml
# (`<raiz>/.github/workflows/ci.yml`).
PROOF_SCRIPT = "tools/ci/prova_checkout.sh"
FLOOR_SCRIPT = "tools/ci/floor.sh"
PREP_PS1 = "tools/ci/windows/prep.ps1"
RERUN_GUARD = "tools/ci/rerun_guard.py"
CMAKELISTS = "CMakeLists.txt"
_TRACKED_SCRIPTS = (PROOF_SCRIPT, FLOOR_SCRIPT, PREP_PS1, CMAKELISTS)


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


# `ctest` que EXECUTA (linha de comando que comeca por `ctest`, sem `-N`,
# que so' lista). CTO 29/09, achado 3: sem isto o job linux imprimia "0
# de teste" e o passo "Testes" ficava fora de todas as regras.
_CTEST_RUN_RE = re.compile(r"^\s*(?:run:\s*)?ctest\b(?![^\n]*\s-N\b)", re.MULTILINE)


def is_test_step(step_text):
    if "tests/container/exec_fixture.sh" in step_text:
        return True
    return bool(_CTEST_RUN_RE.search(_code_text(step_text)))


def is_publish_step(step_text):
    """Passo de publicacao de artefato de paridade - `uses: actions/
    upload-artifact` com um `name:` (dentro do `with:`) comecando por
    `parity-inv-` ou `measured-`. Nunca por SUBSTRINCA solta (GODS_LAWS.
    md L-17 "criterio largo edita o alvo errado") - confere as DUAS
    condicoes, nao so' uma."""
    if "uses: actions/upload-artifact" not in step_text:
        return False
    return bool(re.search(r"^\s*name: (parity-inv-|measured-|ctest-junit-)", step_text, re.MULTILINE))


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
# CTO 29/09 achado 2 (X5): alem do inicio de linha e dos separadores, o
# `git` tambem inicia comando depois de uma palavra-chave de controle
# (`if git`, `then git`, `while git`...), de `!`, de `env [VAR=x] git`,
# de `VAR=x git` e de um prefixador (`command`/`exec`/`sudo`/`xargs`).
_GIT_COMMAND_START_RE = re.compile(
    r"(?:^|[;&|(]|\$\(|!\s|"
    r"\b(?:if|then|elif|while|until|do|else|time|exec|command|sudo|xargs)\s+|"
    r"\benv\s+(?:\w+=\S*\s+)*|\b\w+=\S*\s+)\s*git\s+\S"
)
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


_PATH_VALUE_RE = re.compile(r"^\s*path:\s*(\S+)", re.MULTILINE)


def _bootstrap_toolchain_errors(job_name, steps, checkout_indices, final_idx):
    """Com checkout de bootstrap: entre ele e o checkout final tem de
    haver um passo cujo CODIGO chama `<path do bootstrap>/tools/ci/env/`
    (o preparo que instala o git). Sem ele o checkout final roda sem git
    e cai no fallback REST de novo (CTO 29/09, X1/X1b)."""
    errors = []
    for i in checkout_indices:
        if i >= final_idx:
            continue
        m = _PATH_VALUE_RE.search(steps[i][1])
        if not m:
            continue
        alvo = f"{m.group(1)}/tools/ci/env/"
        roda = any(alvo in _code_text(steps[j][1]) for j in range(i + 1, final_idx))
        if not roda:
            errors.append(
                f"job {job_name!r}: nenhum passo entre o bootstrap ({steps[i][0]!r}) e o "
                f"checkout final chama {alvo} - o container nao tem git quando o checkout "
                f"final roda - G1"
            )
    return errors


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

    errors.extend(_bootstrap_toolchain_errors(job_name, steps, checkout_indices, final_idx))
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


def _code_text(text):
    """O texto SEM as linhas de comentario (`# ...`). Um comentario que
    cita `tools/ci/floor.sh`, `git rev-parse HEAD` ou o nome de um
    script NAO e' uma chamada nem uma prova - e' o falso-verde medido
    ao sabotar o proprio G5 (L-27): o comentario de um passo removido
    grudava no passo anterior (o parser parte passos por `- `, e o
    comentario antecede o passo a que pertence) e satisfazia a busca."""
    return "\n".join(
        _strip_trailing_comment(l) for l in text.splitlines() if not l.lstrip().startswith("#")
    )


def _strip_trailing_comment(line):
    """Remove o comentario de FIM DE LINHA (` # ...`) fora de aspas. Um
    `run: echo pulei # tools/ci/prova_checkout.sh` nao chama o script."""
    aspa = None
    for i, ch in enumerate(line):
        if aspa:
            if ch == aspa:
                aspa = None
        elif ch in "'\"":
            aspa = ch
        elif ch == "#" and i > 0 and line[i - 1] in " \t":
            return line[:i].rstrip()
    return line


# D-A14 item 1 (L-17 gemeo): a prova do clone git vale para TODO job com
# `container:`, nao so' o de checkout duplo (bootstrap + final). Nos 5
# jobs fixos (lint/sanitizer/gl-codegen-host-cross/debug/clang) o git
# vem de uma lista de pacotes escrita no proprio ci.yml - se essa lista
# perder o `git`, o checkout cai no fallback REST API (tarball sem
# `.git`) e nada mais reclamava. A prova tem TRES partes, todas
# obrigatorias: `git rev-parse --is-inside-work-tree` (== true), `git
# rev-parse HEAD` e `$GITHUB_SHA` (a comparacao entre os dois). Pode
# estar escrita no proprio passo ou vir do script `PROOF_SCRIPT` que o
# passo chama (fonte unica das 6 pernas Linux) - nesse caso o portao
# le o CONTEUDO do script, nunca confia so' na chamada.
_PROOF_PARTS = (
    ("git rev-parse --is-inside-work-tree", "--is-inside-work-tree"),
    ("git rev-parse HEAD", "HEAD"),
    ("GITHUB_SHA", "GITHUB_SHA"),
)


def _effective_proof_text(step_text, scripts):
    codigo = _code_text(step_text)
    if PROOF_SCRIPT in codigo:
        return codigo + "\n" + _code_text((scripts or {}).get(PROOF_SCRIPT, ""))
    return codigo


# Comparacao DENTRO de um `[ ... ]` (o teste do shell), com operador `=`
# ou `!=`: citar o token em `echo`/atribuicao nao e' prova.
_BRACKET_RE = re.compile(r"\[\s[^\]]*\]")
_COMPARE_OP_RE = re.compile(r"!=|==|(?<![<>=!])=(?!=)")
_OR_TRUE_RE = re.compile(r"\|\|\s*true\b")


def _has_bracket_compare(codigo, token):
    for bloco in _BRACKET_RE.findall(codigo):
        if token in bloco and _COMPARE_OP_RE.search(bloco):
            return True
    return False


def _missing_proof_parts(step_text, scripts):
    texto = _effective_proof_text(step_text, scripts)
    faltam = [label for needle, label in _PROOF_PARTS if needle not in texto]
    if not _has_bracket_compare(texto, "GITHUB_SHA"):
        faltam.append("comparacao com GITHUB_SHA (dentro de [ ... ])")
    if not _has_bracket_compare(texto, "true"):
        faltam.append("comparacao com true (dentro de [ ... ])")
    if _OR_TRUE_RE.search(texto):
        faltam.append("'|| true' (proibido: engole a falha da prova)")
    return faltam


def g2_errors(job_name, job_block_text, steps, scripts=None):
    if not job_has_container(job_block_text):
        return []
    checkout_indices = [i for i, (_n, t) in enumerate(steps) if is_checkout_step(t)]
    checkouts_sem_path = [i for i in checkout_indices if not checkout_has_path(steps[i][1])]
    if not checkouts_sem_path:
        return []  # ja reportado por G1
    final_idx = checkouts_sem_path[-1]

    if final_idx + 1 >= len(steps):
        return [
            f"job {job_name!r}: checkout final e' o ULTIMO passo do job - G2 exige a prova do "
            f"git logo depois"
        ]
    nome_prova, texto_prova = steps[final_idx + 1]
    faltam = _missing_proof_parts(texto_prova, scripts)
    if faltam:
        return [
            f"job {job_name!r}, passo {nome_prova!r}: nao prova git logo apos o checkout final "
            f"(faltam: {', '.join(faltam)}) - G2 (D-A14: vale para TODO job com container:)"
        ]
    return []


# --- A3c (D-A13): G3 - exatamente um id: prep por job, id: build -------
# (quando existir) vem depois dele ---------------------------------------

_STEP_ID_RE = re.compile(r"^\s*id:\s*(\S+)\s*$", re.MULTILINE)


def step_id(step_text):
    m = _STEP_ID_RE.search(step_text)
    return m.group(1) if m else None


_STEPS_REF_RE = re.compile(r"\bsteps\.([A-Za-z_][A-Za-z0-9_-]*)\.")


def g3b_errors(job_name, steps):
    """Todo `steps.<id>.` citado num passo resolve para um `id: <id>` de
    um passo ANTERIOR do mesmo job (CTO 29/09, achado 3, X6/X7/X8)."""
    errors = []
    definidos = set()
    for nome, texto in steps:
        for ref in sorted(set(_STEPS_REF_RE.findall(_code_text(texto)))):
            if ref not in definidos:
                errors.append(
                    f"job {job_name!r}, passo {nome!r}: cita steps.{ref} mas nenhum passo "
                    f"anterior do job tem 'id: {ref}' - o if: valeria sempre falso e o passo "
                    f"pularia calado - G3b"
                )
        sid = step_id(texto)
        if sid:
            definidos.add(sid)
    return errors


# `preci.sh --modo` (invocacao); ler o arquivo (grep no `Popular o cache`) nao conta.
_GATE_CALL_RE = re.compile(r"tools/preci\.sh\s+--|tests/tools/check_\w+\.py")


def g3c_errors(job_name, steps):
    """Nenhum passo ANTES do marco `id: prep` roda portao ou teste
    (CTO 29/09, achado 5)."""
    prep = [i for i, (_n, t) in enumerate(steps) if step_id(t) == "prep"]
    if len(prep) != 1:
        return []  # ja reportado por G3
    errors = []
    for i in range(prep[0]):
        nome, texto = steps[i]
        codigo = _code_text(texto)
        if is_test_step(texto) or _GATE_CALL_RE.search(codigo):
            errors.append(
                f"job {job_name!r}, passo {nome!r}: roda portao/teste ANTES do marco id: prep - "
                f"uma falha dele contaria como falha de preparo (e a re-execucao da A4 a repetiria) - G3c"
            )
    return errors


_AGG_TAUTOLOGY_RE = re.compile(r"\$?executados\s*=\s*\$declarados\b")
AGGREGATE_SCRIPT = "tests/tools/ctest_aggregate.py"


def aggregate_errors(job_name, steps):
    """Passo "Resultado agregado*": chama ctest_aggregate.py e nao atribui
    executados a partir de declarados (CTO 29/09, achado 4)."""
    errors = []
    tem_junit = any(
        _CTEST_RUN_RE.search(_code_text(t)) and "--output-junit" in _code_text(t) for _n, t in steps
    )
    tem_publicacao = any(
        "uses: actions/upload-artifact" in _code_text(t)
        and re.search(r"^\s*name: ctest-junit-\S+", _code_text(t), re.MULTILINE)
        for _n, t in steps
    )
    for nome, texto in steps:
        if not nome.startswith("Resultado agregado"):
            continue
        codigo = _code_text(texto)
        if nome.startswith("Resultado agregado (") and not tem_publicacao:
            errors.append(
                f"job {job_name!r}, passo {nome!r}: nenhum passo publica o JUnit como artefato "
                f"`ctest-junit-<slug>-<modo>` - o P2 do plano exige a duracao de CADA teste (I3)"
            )
        if nome.startswith("Resultado agregado (") and not tem_junit:
            errors.append(
                f"job {job_name!r}, passo {nome!r}: nenhum passo de ctest usa --output-junit - o "
                f"agregado le o JUnit (LastTest.log conta pulado e desligado como passou)"
            )
        # "Resultado agregado do container (P-0)" mede por results.tsv (ja
        # e' medido, nao atribuido) - so' o agregado de ctest exige o script.
        if nome.startswith("Resultado agregado (") and AGGREGATE_SCRIPT not in codigo:
            errors.append(f"job {job_name!r}, passo {nome!r}: nao chama {AGGREGATE_SCRIPT} - o agregado tem de MEDIR os executados")
        if _AGG_TAUTOLOGY_RE.search(codigo):
            errors.append(f"job {job_name!r}, passo {nome!r}: 'executados = declarados' e' tautologia - a checagem executados != declarados nunca dispararia")
    return errors


_MANDATORY_SCRIPTS = (PROOF_SCRIPT, FLOOR_SCRIPT, PREP_PS1, AGGREGATE_SCRIPT, RERUN_GUARD)
_IF_LINE_RE = re.compile(r"^\s{8}if:\s*(.*)$", re.MULTILINE)
_ASSIGN_LINE_RE = re.compile(r"^(?:set [-+]\w+|\$\w+\s*=.*)$")


def _run_lines(codigo):
    """Linhas de comando do `run:` do passo (inline ou bloco)."""
    linhas = codigo.splitlines()
    for i, linha in enumerate(linhas):
        m = re.match(r"^(\s*)run:\s*(.*)$", linha)
        if not m:
            continue
        inline = m.group(2).strip()
        if inline and inline not in ("|", "|-", ">", ">-"):
            return [inline]
        base = len(m.group(1))
        cmds = []
        for resto in linhas[i + 1:]:
            if resto.strip() and len(resto) - len(resto.lstrip()) <= base:
                break
            if resto.strip():
                cmds.append(resto.strip())
        return cmds
    return []


def _literal_call_re(script):
    return re.compile(r"^(?:&\s*\$py\s+|python3?\s+)?" + re.escape(script) + r"(?:\s|$)")


def _mandatory_step_errors(job_name, nome, script, codigo):
    errors = []
    prefixo = f"job {job_name!r}, passo {nome!r}: chama {script}"
    if re.search(r"^\s*continue-on-error:", codigo, re.MULTILINE):
        errors.append(f"{prefixo} mas tem continue-on-error - o passo obrigatorio nao pode ser neutralizado - A2")
    for m in _IF_LINE_RE.finditer(codigo):
        valor = m.group(1).strip()
        if not (script == AGGREGATE_SCRIPT and valor == "${{ !cancelled() }}"):
            errors.append(f"{prefixo} mas tem 'if: {valor}' - o passo obrigatorio roda sempre (o agregado admite so' !cancelled()) - A2")
    comandos = _run_lines(codigo)
    chama = _literal_call_re(script)
    idx = next((i for i, c in enumerate(comandos) if script in c), None)
    if idx is None or not chama.match(comandos[idx]):
        errors.append(f"{prefixo} mas nao como chamada literal do script (o run: tem de comecar por ele) - A2")
    elif not all(_ASSIGN_LINE_RE.match(c) for c in comandos[:idx]):
        errors.append(f"{prefixo} mas depois de outros comandos alem de set/atribuicoes - A2")
    elif re.search(r"\|", comandos[idx]):
        errors.append(f"{prefixo} com '||' ou pipe na linha da chamada - engole a falha - A2")
    return errors


def mandatory_errors(job_name, steps):
    """Regra UNICA do "passo obrigatorio efetivo" (A2, CTO 29/09) para a
    prova, o piso e o agregado - sem gemeo por script (L-17)."""
    errors = []
    for nome, texto in steps:
        codigo = _code_text(texto)
        for script in _MANDATORY_SCRIPTS:
            if script in codigo:
                errors.extend(_mandatory_step_errors(job_name, nome, script, codigo))
    return errors


_JOB_NEEDS_RE = re.compile(r"^    needs:", re.MULTILINE)
_RERUN_DERIVADO_RE = re.compile(r"RERUN_DERIVADO:\s*[\"']1[\"']")


def rerun_guard_errors(job_name, steps, job_block_text=""):
    """A4 (D-A4): o passo `rerun_guard.py` esta IMEDIATAMENTE antes do marco
    `id: prep`, com GH_TOKEN e RERUN_CHECK_RUN_ID no env. So' julga job com
    exatamente um marco (G3 reporta o resto)."""
    prep = [i for i, (_n, t) in enumerate(steps) if step_id(t) == "prep"]
    if len(prep) != 1:
        return []
    if prep[0] == 0 or RERUN_GUARD not in _code_text(steps[prep[0] - 1][1]):
        return [
            f"job {job_name!r}: o passo logo antes do marco prep tem de chamar {RERUN_GUARD} "
            f"(D-A4: rerun_guard; sem ele 'no maximo uma reexecucao' e 'so' infraestrutura' sao pratica, nao trava) - G6"
        ]
    nome, texto = steps[prep[0] - 1]
    codigo = _code_text(texto)
    errors = [
        f"job {job_name!r}, passo {nome!r}: chama {RERUN_GUARD} sem {var} no env - G6"
        for var in ("GH_TOKEN", "RERUN_CHECK_RUN_ID")
        if var not in codigo
    ]
    tem_needs = bool(_JOB_NEEDS_RE.search(job_block_text))
    tem_env = bool(_RERUN_DERIVADO_RE.search(codigo))
    if tem_needs != tem_env:
        errors.append(
            f"job {job_name!r}, passo {nome!r}: RERUN_DERIVADO: \"1\" no env se e so' se o job tem "
            f"`needs:` (tem needs={tem_needs}, tem env={tem_env}) - I1, G6"
        )
    return errors


_TOP_ON_RE = re.compile(r"^on:\s*$", re.MULTILINE)
_TOP_PERMISSIONS_RE = re.compile(r"^permissions:\s*\n((?:[ ]+\S.*\n)+)", re.MULTILINE)


def permissions_errors(ci_yml_text):
    """O workflow declara `permissions: actions: read` (D-A4). So' julga um
    workflow de verdade (com `on:`)."""
    if not _TOP_ON_RE.search(ci_yml_text):
        return []
    m = _TOP_PERMISSIONS_RE.search(ci_yml_text)
    bloco = m.group(1) if m else ""
    if not re.search(r"^\s+actions:\s*read\s*$", bloco, re.MULTILINE):
        return ["ci.yml: falta `permissions:` com `actions: read` no topo - o rerun_guard consulta a API (D-A4)"]
    return []


def _load_rerun_guard():
    """tools/ci/rerun_guard.py como modulo: fonte UNICA de PREP_MARKER (I3,
    CTO 29/09) - o portao nunca escreve o nome do marco uma segunda vez."""
    caminho = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "tools", "ci", "rerun_guard.py"
    )
    spec = importlib.util.spec_from_file_location("glintfx_rerun_guard", caminho)
    modulo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modulo)
    return modulo


PREP_MARKER = _load_rerun_guard().PREP_MARKER
_JOB_PERMISSIONS_RE = re.compile(r"^    permissions:", re.MULTILINE)


def prep_marker_name_errors(job_name, steps):
    return [
        f"job {job_name!r}: o passo com id: prep se chama {nome!r}, tem de ser EXATAMENTE "
        f"{PREP_MARKER!r} (PREP_MARKER de tools/ci/rerun_guard.py, fonte unica) - o guarda "
        f"classifica preparo x teste por esse nome - G6"
        for nome, texto in steps
        if step_id(texto) == "prep" and nome != PREP_MARKER
    ]


def job_permissions_errors(job_name, job_block_text):
    if _JOB_PERMISSIONS_RE.search(job_block_text):
        return [f"job {job_name!r}: declara `permissions:` proprio - so' o topo do workflow pode (o job ampliaria o token do rerun_guard) - G6"]
    return []


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


# --- D-A14 item 2 (G5): piso de ferramentas em todo job que compila -----
#
# "Todo job com `id: build` tem um passo que chama `tools/ci/floor.sh`
# (familia Linux, job com `container:`) ou `tools/ci/windows/prep.ps1`
# (familia Windows, `runs-on: windows-*`) DEPOIS do checkout e ANTES do
# marco `id: prep`". A familia sai do que o bloco do job DECLARA, nunca
# de uma lista de nomes: job com `id: build` e sem `container:` nem
# `runs-on: windows-*` (ex.: o `wayland-container`, que constroi uma
# imagem Docker no proprio runner) fica FORA do universo, e a fronteira
# e' impressa (L-40). Nenhum job Windows pode ter a instalacao do CMake
# escrita no proprio ci.yml (era copiada 4 vezes, regra de 3 estourada;
# a sonda e' o download da Kitware, nunca o nome do passo). Os scripts
# sao lidos pelo CONTEUDO: um piso que nao checa o que promete nao conta.

_RUNS_ON_WINDOWS_RE = re.compile(r"^    runs-on:\s*windows", re.MULTILINE)
_KITWARE_DOWNLOAD_RE = re.compile(r"Kitware/CMake/releases")

_FLOOR_SCRIPT_NEEDLES = ("__GNUC__", "cmake --version", "python3", "xdg-shell.xml")
_PREP_PS1_NEEDLES = ("cmake --version", "/std:c++latest", "python3", "RUNNER_TEMP")

# A garantia do commit 46b21c1: a sonda do cl.exe leva /Fo E /Fe
# explicitos. Procurados DENTRO da funcao que monta os argumentos do
# cl.exe (`Get-ClArguments`), nunca no arquivo inteiro - o -Autoteste do
# proprio script cita as duas flags, e uma busca no arquivo todo seria
# satisfeita por ele mesmo com a garantia sabotada (medido em P4).
_CL_ARGUMENTS_FN_RE = re.compile(r"function Get-ClArguments\b.*?^\}", re.DOTALL | re.MULTILINE)


def _cl_arguments_missing(ps1_code):
    m = _CL_ARGUMENTS_FN_RE.search(ps1_code)
    if not m:
        return ["function Get-ClArguments (monta os argumentos do cl.exe)"]
    return [f"{flag} em Get-ClArguments" for flag in ("/Fo:", "/Fe:") if flag not in m.group(0)]


def job_family(job_block_text):
    """'linux' (container:), 'windows' (runs-on: windows-*) ou None
    (fora do universo do piso)."""
    if job_has_container(job_block_text):
        return "linux"
    if _RUNS_ON_WINDOWS_RE.search(job_block_text):
        return "windows"
    return None


_FAMILY_FLOOR = {"linux": FLOOR_SCRIPT, "windows": PREP_PS1}
_FAMILY_NEEDLES = {FLOOR_SCRIPT: _FLOOR_SCRIPT_NEEDLES, PREP_PS1: _PREP_PS1_NEEDLES}


def _floor_script_errors(job_name, script, scripts):
    texto = (scripts or {}).get(script)
    if texto is None:
        return [f"job {job_name!r}: {script} ausente - G5 le o CONTEUDO do piso, nunca so' a chamada"]
    codigo = _code_text(texto)
    faltam = [n for n in _FAMILY_NEEDLES[script] if n not in codigo]
    if script == PREP_PS1:
        faltam.extend(_cl_arguments_missing(codigo))
        if re.search(r"cmake\s+--version[^\n]*\|\s*Select-Object", codigo):
            faltam.append("nada de `| Select-Object` sobre o cmake nativo (deixa $LASTEXITCODE nulo)")
        if not re.search(r"^exit 0\s*$", codigo, re.MULTILINE):
            faltam.append("`exit 0` explicito no fim do script")
        if not re.search(r"Assert-MsvcAcceptsCxx23\s+\$env:RUNNER_TEMP\b", codigo):
            faltam.append("Assert-MsvcAcceptsCxx23 $env:RUNNER_TEMP (a sonda do cl.exe nunca no workspace)")
    if faltam:
        return [f"job {job_name!r}: {script} nao checa {', '.join(faltam)} - o piso nao cobre o que promete (G5)"]
    return []


def _floor_position_errors(job_name, steps, script):
    indices = [
        i for i, (_n, t) in enumerate(steps) if script in _code_text(t) and "-VerifyCmake" not in _code_text(t)
    ]
    if not indices:
        return [
            f"job {job_name!r}: nenhum passo chama {script} - G5: todo job com id: build "
            f"tem o piso de ferramentas (D-A14)"
        ]
    errors = []
    prep = [i for i, (_n, t) in enumerate(steps) if step_id(t) == "prep"]
    if prep and indices[0] > prep[0]:
        errors.append(f"job {job_name!r}: o piso ({script}) vem DEPOIS do marco id: prep, tem de vir ANTES do marco - G5")
    finais = [i for i, (_n, t) in enumerate(steps) if is_checkout_step(t) and not checkout_has_path(t)]
    if finais and indices[0] < finais[-1]:
        errors.append(f"job {job_name!r}: o piso ({script}) roda ANTES do checkout - o script mora no repo, tem de vir DEPOIS do checkout - G5")
    return errors


def _handwritten_cmake_install_errors(job_name, steps):
    return [
        f"job {job_name!r}, passo {nome!r}: instala o CMake a mao (download da Kitware) - "
        f"G5: a instalacao mora so' em {PREP_PS1} (D-A14, regra de 3)"
        for nome, texto in steps
        if _KITWARE_DOWNLOAD_RE.search(_code_text(texto)) and PREP_PS1 not in _code_text(texto)
    ]


def _piso_universe(job_block_text, steps):
    """Onde este job cai no universo do G5: None (sem id: build, nada a
    exigir), 'linux'/'windows' (piso exigido) ou 'fora' (compila sem
    container nem windows - fronteira impressa, nunca calada)."""
    if not any(step_id(t) == "build" for _n, t in steps):
        return None
    return job_family(job_block_text) or "fora"


_VERIFY_CMD = PREP_PS1 + " -VerifyCmake"
_PS1_CALL_RE = re.compile(r"^(?:&\s*)?tools/ci/\S+\.ps1\b")


def _verify_cmake_errors(job_name, steps):
    """O passo IMEDIATAMENTE depois do marco `id: prep` e' um passo PROPRIO
    cujo unico comando e' `prep.ps1 -VerifyCmake` (item (i) do CTO e run
    36523231561: como primeira linha de um passo que faz outra coisa, ele
    encerrava o passo calado - ver ps1_single_command_errors)."""
    prep = [i for i, (_n, t) in enumerate(steps) if step_id(t) == "prep"]
    if len(prep) != 1:
        return []
    if prep[0] + 1 >= len(steps):
        return [f"job {job_name!r}: o marco prep e' o ultimo passo - G5 exige '{_VERIFY_CMD}' logo depois"]
    nome, texto = steps[prep[0] + 1]
    if _run_lines(_code_text(texto)) != [_VERIFY_CMD]:
        return [
            f"job {job_name!r}, passo {nome!r}: o passo logo depois do marco prep tem de ser "
            f"um passo proprio de uma linha so' ('{_VERIFY_CMD}') - nada prova que os passos "
            f"seguintes enxergam o CMake pinado (e nao o do Visual Studio) - G5"
        ]
    return []


def ps1_single_command_errors(job_name, steps):
    """Nenhum `run:` pwsh tem linhas depois de uma chamada a tools/ci/**/*.ps1
    (run 36523231561): um .ps1 chamado sem `exit` explicito deixa
    $LASTEXITCODE nulo, e o `if ($LASTEXITCODE -ne 0) { exit ... }` da linha
    seguinte sai com 0 - o resto do passo nunca roda e o passo fica verde."""
    errors = []
    for nome, texto in steps:
        codigo = _code_text(texto)
        if not re.search(r"^\s*shell:\s*pwsh\b", codigo, re.MULTILINE):
            continue
        comandos = _run_lines(codigo)
        chamadas = [i for i, c in enumerate(comandos) if _PS1_CALL_RE.match(c)]
        if chamadas and len(comandos) > 1:
            errors.append(
                f"job {job_name!r}, passo {nome!r}: chama {comandos[chamadas[0]].split()[0]} e tem mais "
                f"{len(comandos) - 1} linha(s) no mesmo run: - o passo pwsh encerra calado quando "
                f"$LASTEXITCODE fica nulo; o script tem de ser o UNICO comando do passo - G5"
            )
    return errors


def g5_errors(job_name, job_block_text, steps, scripts=None):
    if not any(step_id(t) == "build" for _n, t in steps):
        return []
    familia = job_family(job_block_text)
    if familia is None:
        return []
    script = _FAMILY_FLOOR[familia]
    errors = _floor_position_errors(job_name, steps, script)
    errors.extend(_floor_script_errors(job_name, script, scripts))
    if familia == "windows":
        errors.extend(_handwritten_cmake_install_errors(job_name, steps))
        errors.extend(_verify_cmake_errors(job_name, steps))
    return errors


# --- veredito completo ----------------------------------------------


def run_check(job_name, job_block_text, steps, scripts=None):
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
    errors.extend(g2_errors(job_name, job_block_text, steps, scripts))
    errors.extend(g3_errors(job_name, steps))
    errors.extend(g3b_errors(job_name, steps))
    errors.extend(g3c_errors(job_name, steps))
    errors.extend(aggregate_errors(job_name, steps))
    errors.extend(mandatory_errors(job_name, steps))
    errors.extend(ps1_single_command_errors(job_name, steps))
    errors.extend(rerun_guard_errors(job_name, steps, job_block_text))
    errors.extend(prep_marker_name_errors(job_name, steps))
    errors.extend(job_permissions_errors(job_name, job_block_text))
    errors.extend(g5_errors(job_name, job_block_text, steps, scripts))
    counts = {
        "testes": len(test_steps),
        "publicacoes": len(publish_steps),
        "piso": _piso_universe(job_block_text, steps),
    }
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


def _check_one_job(ci_yml_path, ci_yml_text, job_name, scripts):
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
    counts, errors = run_check(job_name, job_block, steps, scripts)
    print(
        f"{SCRIPT_NAME}: job {job_name!r} - {len(steps)} passo(s) total, "
        f"{counts['testes']} de teste, {counts['publicacoes']} de publicacao"
    )
    return counts, [f"{job_name}: {e}" for e in errors]


def _load_scripts(ci_yml_path):
    """Le os scripts que o ci.yml chama (`_TRACKED_SCRIPTS`), a partir
    da raiz derivada do caminho do ci.yml (`<raiz>/.github/workflows/
    ci.yml`). Script ausente simplesmente nao entra no dict - quem o
    referencia reprova por nao provar o que promete (nunca presume)."""
    from pathlib import Path

    root = Path(ci_yml_path).resolve().parent.parent.parent
    scripts = {}
    for rel in _TRACKED_SCRIPTS:
        candidate = root / rel
        if candidate.is_file():
            scripts[rel] = candidate.read_text(encoding="utf-8")
    return scripts


def _print_piso_universe(universo):
    """Piso de varredura do G5 (L-40): encontrados/exigidos por familia
    e a fronteira (jobs com id: build fora do universo, NOMEADOS)."""
    por_familia = {f: sorted(j for j, x in universo.items() if x == f) for f in ("linux", "windows", "fora")}
    print(
        f"{SCRIPT_NAME}: G5 piso de ferramentas - {len(universo)} job(s) com id: build; "
        f"exigido em {len(por_familia['linux']) + len(por_familia['windows'])} "
        f"(linux={len(por_familia['linux'])}, windows={len(por_familia['windows'])}); "
        f"fora do universo (sem container: nem runs-on: windows): {por_familia['fora'] or 'nenhum'}"
    )


_MIN_REQUIRED_RE = re.compile(r"cmake_minimum_required\s*\(\s*VERSION\s+(\d+)\.(\d+)", re.IGNORECASE)


def cmake_floor_source_errors(scripts):
    """C1 (CTO 29/09): o pino do CMake e o piso escrito em floor.sh e
    prep.ps1 sao a MESMA major.minor de `cmake_minimum_required(VERSION
    X.Y)` do CMakeLists.txt (a fonte do piso) - nenhuma copia solta."""
    fonte = _code_text((scripts or {}).get(CMAKELISTS, ""))
    m = _MIN_REQUIRED_RE.search(fonte)
    if not m:
        return [f"{CMAKELISTS}: cmake_minimum_required(VERSION X.Y) nao encontrado - a fonte do piso do CMake (C1)"]
    major, minor = m.group(1), m.group(2)
    errors = []
    prep = _code_text((scripts or {}).get(PREP_PS1, ""))
    floor = _code_text((scripts or {}).get(FLOOR_SCRIPT, ""))
    pino = re.search(r"\$CmakeVersion\s*=\s*'(\d+)\.(\d+)\.\d+'", prep)
    if not pino or (pino.group(1), pino.group(2)) != (major, minor):
        visto = f"{pino.group(1)}.{pino.group(2)}" if pino else "ausente"
        errors.append(f"{PREP_PS1}: pino do CMake {visto} nao e' a major.minor {major}.{minor} de {CMAKELISTS} cmake_minimum_required (C1)")
    prep_piso = f"$major -lt {major} -or ($major -eq {major} -and $minor -lt {minor})"
    if prep_piso not in prep:
        errors.append(f"{PREP_PS1}: piso do CMake diferente de {major}.{minor} (esperado `{prep_piso}`) - C1")
    floor_piso = f'"$cmake_major" -lt {major} ] || {{ [ "$cmake_major" -eq {major} ] && [ "$cmake_minor" -lt {minor} ]; }}'
    if floor_piso not in floor:
        errors.append(f"{FLOOR_SCRIPT}: piso do CMake diferente de {major}.{minor} (esperado `{floor_piso}`) - C1")
    return errors


def real_main(args):
    ci_yml_path, job_name = _parse_real_main_args(args)
    ci_yml_text = _read_file(ci_yml_path)
    job_names = _resolve_job_names(ci_yml_text, ci_yml_path, job_name)
    scripts = _load_scripts(ci_yml_path)

    total_testes = 0
    all_errors = []
    universo = {}
    for jn in job_names:
        counts, errors = _check_one_job(ci_yml_path, ci_yml_text, jn, scripts)
        total_testes += counts["testes"]
        all_errors.extend(errors)
        if counts["piso"] is not None:
            universo[jn] = counts["piso"]
    _print_piso_universe(universo)
    all_errors.extend(cmake_floor_source_errors(scripts))
    all_errors.extend(permissions_errors(ci_yml_text))
    if job_name is None and not universo:
        fail(
            f"varredura vazia: 0 job(s) com id: build em {len(job_names)} job(s) - G5, "
            "GODS_LAWS.md L-40, isto e sinal de coleta quebrada"
        )

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
          [ "$(git rev-parse --is-inside-work-tree)" = true ]
          [ "$(git rev-parse HEAD)" = "$GITHUB_SHA" ]

      - name: Piso de ferramentas
        run: tools/ci/floor.sh

      - name: Guarda de reexecucao (rerun_guard)
        env:
          GH_TOKEN: ${{ github.token }}
          RERUN_CHECK_RUN_ID: ${{ job.check_run_id }}
        run: python3 tools/ci/rerun_guard.py

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


def _run_real_main_capturing(ci_yml_text, job_name=DEFAULT_JOB, extra_files=None, drop_defaults=False):
    """`job_name=None` reproduz o CLI real sem `--job` nenhum (F2: o
    modo padrao varre TODOS os jobs do arquivo) - os selftests
    antigos, focados num job so', continuam passando `job_name`
    explicito (comportamento antigo preservado por `--job`).
    `extra_files` ({caminho-relativo-a-raiz: texto}) monta uma arvore
    de raiz falsa (`.github/workflows/ci.yml` + os scripts que o
    ci.yml chama), do mesmo formato que real_main() le do repo real.
    Sem `extra_files`, monta os tres scripts corretos (`_DEFAULT_SCRIPTS`);
    `extra_files` sobrepoe por caminho, e `drop_defaults=True` monta SO'
    o que `extra_files` traz (para provar ausencia de script)."""
    import contextlib
    import io
    import tempfile
    from pathlib import Path

    buffer = io.StringIO()
    exit_code = None
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / ".github" / "workflows" / "ci.yml"
        path.parent.mkdir(parents=True)
        path.write_text(ci_yml_text, encoding="utf-8")
        files = dict(extra_files or {}) if drop_defaults else {**_DEFAULT_SCRIPTS, **(extra_files or {})}
        for rel, text in files.items():
            target = Path(tmp) / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(text, encoding="utf-8")
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
        "          [ \"$(git rev-parse --is-inside-work-tree)\" = true ]\n"
        "          [ \"$(git rev-parse HEAD)\" = \"$GITHUB_SHA\" ]\n\n",
        "      - name: Remove o checkout de bootstrap\n"
        "        run: rm -rf _bootstrap\n\n"
        "      - name: Configurar diretorio seguro do git\n"
        "        run: git config --global --add safe.directory \"$GITHUB_WORKSPACE\"\n\n"
        "      - uses: actions/checkout@v7\n\n"
        "      - name: Checkout e' repositorio git\n"
        "        run: |\n"
        "          [ \"$(git rev-parse --is-inside-work-tree)\" = true ]\n"
        "          [ \"$(git rev-parse HEAD)\" = \"$GITHUB_SHA\" ]\n\n",
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
        "          [ \"$(git rev-parse --is-inside-work-tree)\" = true ]\n"
        "          [ \"$(git rev-parse HEAD)\" = \"$GITHUB_SHA\" ]\n\n",
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
          [ "$(git rev-parse --is-inside-work-tree)" = true ]
          [ "$(git rev-parse HEAD)" = "$GITHUB_SHA" ]

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


# D-A14 item 1 (G2 estendida): job com `container:` e checkout UNICO
# (o padrao dos 5 jobs fixos) TAMBEM tem de provar o clone git logo
# depois do checkout - sem a prova, um checkout que cai no fallback
# REST API (sem .git) passaria calado.
_FIXTURE_FIXED_JOB = """jobs:
  lint:
    container: fedora:latest
    steps:
      - name: Instalar toolchain
        run: >-
          dnf -y install gcc-c++ cmake ninja-build pkgconf-pkg-config git

      - uses: actions/checkout@v7

      - name: Checkout e' repositorio git
        run: tools/ci/prova_checkout.sh

      - name: Piso de ferramentas
        run: tools/ci/floor.sh

      - name: Guarda de reexecucao (rerun_guard)
        env:
          GH_TOKEN: ${{ github.token }}
          RERUN_CHECK_RUN_ID: ${{ job.check_run_id }}
        run: python3 tools/ci/rerun_guard.py

      - name: Preparo concluido
        id: prep
        run: echo ok

      - name: preci.sh --lint-only
        id: build
        run: tools/preci.sh --lint-only

      - name: fixture a
        if: ${{ !cancelled() && steps.build.outcome == 'success' }}
        run: tests/container/exec_fixture.sh c a
"""

_PROOF_SCRIPT_OK = """#!/usr/bin/env bash
set -eu
git config --global --add safe.directory "$GITHUB_WORKSPACE"
inside="$(git rev-parse --is-inside-work-tree)"
[ "$inside" = true ] || exit 1
head="$(git rev-parse HEAD)"
[ "$head" = "$GITHUB_SHA" ] || exit 1
echo "inside-work-tree=$inside"
"""

_FLOOR_SCRIPT_OK = """#!/usr/bin/env bash
gnuc="$("$CXX" -dM -E -x c++ /dev/null | awk '$2 == "__GNUC__" {print $3}')"
cmake --version
if [ "$cmake_major" -lt 4 ] || { [ "$cmake_major" -eq 4 ] && [ "$cmake_minor" -lt 1 ]; }; then :; fi
command -v python3
xml="$(pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml"
"""

_PREP_PS1_OK = """param([switch]$Autoteste)
cmake --version
function Get-ClArguments($Paths) {
    return @('/std:c++latest', "/Fe:$($Paths.Exe)", "/Fo:$($Paths.Obj)", $Paths.Src)
}
$probe = Join-Path $env:RUNNER_TEMP 'probe'
$CmakeVersion = '4.1.6'
if ($major -lt 4 -or ($major -eq 4 -and $minor -lt 1)) { }
function Invoke-Prep {
    Assert-MsvcAcceptsCxx23 $env:RUNNER_TEMP
}
Get-Command python3
exit 0
"""

_CMAKELISTS_OK = "cmake_minimum_required(VERSION 4.1)\n"

_DEFAULT_SCRIPTS = {
    PROOF_SCRIPT: _PROOF_SCRIPT_OK,
    FLOOR_SCRIPT: _FLOOR_SCRIPT_OK,
    PREP_PS1: _PREP_PS1_OK,
    CMAKELISTS: _CMAKELISTS_OK,
}


def _fixed_job_files(proof_script=_PROOF_SCRIPT_OK):
    return {**_DEFAULT_SCRIPTS, PROOF_SCRIPT: proof_script}


def selftest_g2x_fixed_job_with_proof_script_passes():
    exit_code, output = _run_real_main_capturing(
        _FIXTURE_FIXED_JOB, job_name="lint", extra_files=_fixed_job_files()
    )
    if exit_code not in (None, 0):
        print(f"selftest: G2X-POSITIVO FALHOU (job fixo correto reprovou): {output}", file=sys.stderr)
        return False
    print("selftest: G2X-POSITIVO OK (checkout unico + prova por script + piso, tudo presente, passa)")
    return True


def selftest_g2x_fixed_job_without_proof_reproves():
    quebrado = _FIXTURE_FIXED_JOB.replace(
        "      - name: Checkout e' repositorio git\n        run: tools/ci/prova_checkout.sh\n\n", "", 1
    )
    if quebrado == _FIXTURE_FIXED_JOB:
        print("selftest: G2X-SEM-PROVA FALHOU (ancora do replace nao casou)", file=sys.stderr)
        return False
    exit_code, output = _run_real_main_capturing(quebrado, job_name="lint", extra_files=_fixed_job_files())
    if exit_code != 1 or "nao prova git" not in output:
        print(f"selftest: G2X-SEM-PROVA FALHOU (codigo {exit_code!r}): {output!r}", file=sys.stderr)
        return False
    print("selftest: G2X-SEM-PROVA OK (job fixo, checkout unico, sem a prova do git reprova)")
    return True


def selftest_g2x_proof_script_without_head_reproves():
    sem_head = "\n".join(
        line for line in _PROOF_SCRIPT_OK.splitlines() if "HEAD" not in line and "GITHUB_SHA" not in line
    )
    exit_code, output = _run_real_main_capturing(
        _FIXTURE_FIXED_JOB, job_name="lint", extra_files=_fixed_job_files(sem_head)
    )
    if exit_code != 1 or "HEAD" not in output:
        print(f"selftest: G2X-SCRIPT-SEM-HEAD FALHOU (codigo {exit_code!r}): {output!r}", file=sys.stderr)
        return False
    print("selftest: G2X-SCRIPT-SEM-HEAD OK (script de prova sem HEAD=$GITHUB_SHA reprova)")
    return True


def selftest_g2x_proof_script_missing_reproves():
    files = {k: v for k, v in _DEFAULT_SCRIPTS.items() if k != PROOF_SCRIPT}
    exit_code, output = _run_real_main_capturing(
        _FIXTURE_FIXED_JOB, job_name="lint", extra_files=files, drop_defaults=True
    )
    if exit_code != 1 or "nao prova git" not in output:
        print(f"selftest: G2X-SCRIPT-AUSENTE FALHOU (codigo {exit_code!r}): {output!r}", file=sys.stderr)
        return False
    print("selftest: G2X-SCRIPT-AUSENTE OK (passo chama script de prova que nao existe: reprova)")
    return True


def selftest_g1_job_without_container_not_checked():
    sem_container = """jobs:
  windows:
    steps:
      - uses: actions/checkout@v7
      - name: Instalar CMake
        run: echo instala
      - name: Guarda de reexecucao (rerun_guard)
        shell: pwsh
        env:
          GH_TOKEN: ${{ github.token }}
          RERUN_CHECK_RUN_ID: ${{ job.check_run_id }}
        run: python tools/ci/rerun_guard.py

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


# --- D-A14 item 2 (G5): piso de ferramentas em todo job que compila -----

_FIXTURE_WINDOWS_JOB = """jobs:
  windows-x:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v7

      - name: Preparar ambiente do compilador (MSVC x64)
        shell: pwsh
        run: echo msvc

      - name: Piso de ferramentas (Windows)
        shell: pwsh
        run: tools/ci/windows/prep.ps1

      - name: Guarda de reexecucao (rerun_guard)
        shell: pwsh
        env:
          GH_TOKEN: ${{ github.token }}
          RERUN_CHECK_RUN_ID: ${{ job.check_run_id }}
        run: python tools/ci/rerun_guard.py

      - name: Preparo concluido
        id: prep
        shell: pwsh
        run: Write-Host ok

      - name: CMake pinado (Windows)
        shell: pwsh
        run: tools/ci/windows/prep.ps1 -VerifyCmake

      - name: Compilar
        id: build
        shell: pwsh
        run: cmake --build build

      - name: fixture a
        if: ${{ !cancelled() && steps.build.outcome == 'success' }}
        shell: pwsh
        run: tests/container/exec_fixture.sh c a
"""

_FIXTURE_HOST_DOCKER_JOB = """jobs:
  imagem-docker:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v7

      - name: Guarda de reexecucao (rerun_guard)
        env:
          GH_TOKEN: ${{ github.token }}
          RERUN_CHECK_RUN_ID: ${{ job.check_run_id }}
        run: python3 tools/ci/rerun_guard.py

      - name: Preparo concluido
        id: prep
        run: echo ok

      - name: Constroi a imagem
        id: build
        run: docker build .

      - name: fixture a
        if: ${{ !cancelled() && steps.build.outcome == 'success' }}
        run: tests/container/exec_fixture.sh c a
"""


def _g5_run(fixture, job, mutate=None, files=None, drop_defaults=False):
    texto = fixture if mutate is None else mutate(fixture)
    if mutate is not None and texto == fixture:
        return None, "ancora do replace nao casou"
    return _run_real_main_capturing(texto, job_name=job, extra_files=files, drop_defaults=drop_defaults)


def _g5_expect(nome, resultado, *trechos):
    exit_code, output = resultado
    if exit_code != 1 or not all(t in output for t in trechos):
        print(f"selftest: {nome} FALHOU (codigo {exit_code!r}, esperava {trechos}): {output!r}", file=sys.stderr)
        return False
    print(f"selftest: {nome} OK")
    return True


def selftest_g5_positives():
    for fixture, job in ((_FIXTURE_FIXED_JOB, "lint"), (_FIXTURE_WINDOWS_JOB, "windows-x"), (_FIXTURE_HOST_DOCKER_JOB, "imagem-docker")):
        exit_code, output = _run_real_main_capturing(fixture, job_name=job)
        if exit_code not in (None, 0):
            print(f"selftest: G5-POSITIVO ({job}) FALHOU: {output}", file=sys.stderr)
            return False
    print("selftest: G5-POSITIVOS OK (linux com container, windows e job de host sem container fora do universo)")
    return True


def selftest_g5_p1_windows_without_prep_reproves():
    resultado = _g5_run(
        _FIXTURE_WINDOWS_JOB, "windows-x",
        lambda t: t.replace("        run: tools/ci/windows/prep.ps1\n", "        run: echo sem piso\n", 1),
    )
    return _g5_expect("G5-P1-WINDOWS-SEM-PISO", resultado, "windows-x", "tools/ci/windows/prep.ps1")


def selftest_g5_linux_without_floor_reproves():
    resultado = _g5_run(
        _FIXTURE_FIXED_JOB, "lint",
        lambda t: t.replace("      - name: Piso de ferramentas\n        run: tools/ci/floor.sh\n\n", "", 1),
    )
    return _g5_expect("G5-LINUX-SEM-PISO", resultado, "lint", "tools/ci/floor.sh")


def selftest_g5_p2_handwritten_cmake_install_reproves():
    resultado = _g5_run(
        _FIXTURE_WINDOWS_JOB, "windows-x",
        lambda t: t.replace(
            "      - name: Preparar ambiente do compilador (MSVC x64)\n",
            "      - name: Baixar CMake a mao\n        shell: pwsh\n"
            "        run: Invoke-WebRequest -Uri https://github.com/Kitware/CMake/releases/download/v4.1.6/cmake.zip\n\n"
            "      - name: Preparar ambiente do compilador (MSVC x64)\n", 1),
    )
    return _g5_expect("G5-P2-CMAKE-A-MAO", resultado, "windows-x", "Kitware")


def selftest_g5_floor_after_prep_reproves():
    def mover(t):
        piso = "      - name: Piso de ferramentas (Windows)\n        shell: pwsh\n        run: tools/ci/windows/prep.ps1\n\n"
        t = t.replace(piso, "", 1)
        return t.replace("      - name: Compilar\n", piso + "      - name: Compilar\n", 1)
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", mover)
    return _g5_expect("G5-PISO-DEPOIS-DO-PREP", resultado, "windows-x", "ANTES do marco")


def selftest_g5_floor_before_checkout_reproves():
    def mover(t):
        piso = "      - name: Piso de ferramentas (Windows)\n        shell: pwsh\n        run: tools/ci/windows/prep.ps1\n\n"
        t = t.replace(piso, "", 1)
        return t.replace("      - uses: actions/checkout@v7\n\n", piso + "      - uses: actions/checkout@v7\n\n", 1)
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", mover)
    return _g5_expect("G5-PISO-ANTES-DO-CHECKOUT", resultado, "windows-x", "DEPOIS do checkout")


def selftest_g5_floor_script_without_cmake_check_reproves():
    sem_cmake = _FLOOR_SCRIPT_OK.replace("cmake --version\n", "")
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", files={FLOOR_SCRIPT: sem_cmake})
    return _g5_expect("G5-SCRIPT-SEM-CMAKE", resultado, FLOOR_SCRIPT, "cmake --version")


def selftest_g5_prep_ps1_without_fo_reproves():
    sem_fo = _PREP_PS1_OK.replace(' "/Fo:$($Paths.Obj)",', "")
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", files={PREP_PS1: sem_fo})
    return _g5_expect("G5-PS1-SEM-FO", resultado, PREP_PS1, "/Fo: em Get-ClArguments")


def selftest_g5_prep_ps1_missing_reproves():
    files = {k: v for k, v in _DEFAULT_SCRIPTS.items() if k != PREP_PS1}
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", files=files, drop_defaults=True)
    return _g5_expect("G5-PS1-AUSENTE", resultado, PREP_PS1)



# Falso-verde medido ao sabotar o G5 contra o ci.yml real (L-27): tirar so'
# o PASSO do piso deixava o COMENTARIO dele, que cita o script e gruda no
# passo anterior. Comentario nao e' chamada: tem de reprovar igual.
def selftest_g5_comment_mentioning_script_is_not_a_call():
    resultado = _g5_run(
        _FIXTURE_WINDOWS_JOB, "windows-x",
        lambda t: t.replace(
            "      - name: Piso de ferramentas (Windows)\n        shell: pwsh\n        run: tools/ci/windows/prep.ps1\n",
            "      # piso: tools/ci/windows/prep.ps1 (so' comentario, sem passo)\n", 1),
    )
    return _g5_expect("G5-COMENTARIO-NAO-E-CHAMADA", resultado, "windows-x", "nenhum passo chama")


def selftest_g2_comment_mentioning_proof_is_not_a_proof():
    resultado = _g5_run(
        _FIXTURE_FIXED_JOB, "lint",
        lambda t: t.replace(
            "        run: tools/ci/prova_checkout.sh\n",
            "        run: echo sem prova\n        # git rev-parse --is-inside-work-tree, git rev-parse HEAD, GITHUB_SHA\n", 1),
    )
    return _g5_expect("G2-COMENTARIO-NAO-E-PROVA", resultado, "nao prova git")



# CTO 29/09 achado 7 (mutante X4): G2 casa o MECANISMO, nao o rotulo. Uma
# prova que so' CITA `git rev-parse` mas nao compara nada (ou engole o
# resultado com `|| true`) passava. A prova tem de comparar o valor de
# --is-inside-work-tree com `true` e o HEAD com $GITHUB_SHA, dentro de
# um `[ ... ]`, e nao pode conter `|| true`.
def _g2_mecanismo(nome, trocas, *trechos):
    script = _PROOF_SCRIPT_OK
    for velho, novo in trocas:
        if velho not in script:
            print(f"selftest: {nome} FALHOU (ancora do replace nao casou: {velho!r})", file=sys.stderr)
            return False
        script = script.replace(velho, novo, 1)
    resultado = _run_real_main_capturing(
        _FIXTURE_FIXED_JOB, job_name="lint", extra_files=_fixed_job_files(script)
    )
    return _g5_expect(nome, resultado, *trechos)


def selftest_g2_or_true_reproves():
    return _g2_mecanismo(
        "G2-OR-TRUE (X4)",
        [('inside="$(git rev-parse --is-inside-work-tree)"', 'inside="$(git rev-parse --is-inside-work-tree)" || true')],
        "nao prova git", "|| true",
    )


def selftest_g2_sha_echoed_not_compared_reproves():
    return _g2_mecanismo(
        "G2-SHA-SO-CITADO",
        [('[ "$head" = "$GITHUB_SHA" ] || exit 1', 'echo "$head $GITHUB_SHA"')],
        "nao prova git", "comparacao com GITHUB_SHA",
    )


def selftest_g2_inside_not_compared_reproves():
    return _g2_mecanismo(
        "G2-INSIDE-SEM-COMPARAR-COM-TRUE",
        [('[ "$inside" = true ] || exit 1\n', "")],
        "nao prova git", "comparacao com true",
    )



# CTO 29/09 achado 2 (mutantes X1, X1b, X5): (a) com checkout de
# bootstrap, o passo que roda o preparo DO bootstrap (`<path>/tools/ci/
# env/<slug>.sh`) tem de existir ENTRE o bootstrap e o checkout final -
# sem ele (removido, ou movido para depois do final) o container nao
# tem git quando o checkout final roda e ele cai no fallback REST de
# novo; (b) o comando git antes do checkout final tambem e' pego em
# `if git`, `! git` e `env X=1 git`.
_BOOT_TOOLCHAIN = (
    "      - name: Instalar toolchain\n"
    "        run: bash _bootstrap/tools/ci/env/fedora.sh\n\n"
)


def selftest_g1_bootstrap_toolchain_removed_reproves():
    resultado = _g5_run(_FIXTURE_CI_YML, "wayland-container", lambda t: t.replace(_BOOT_TOOLCHAIN, "", 1))
    return _g5_expect("G1-X1B-PREPARO-DO-BOOTSTRAP-AUSENTE", resultado, "wayland-container", "tools/ci/env/")


def selftest_g1_bootstrap_toolchain_after_final_checkout_reproves():
    def mover(t):
        t = t.replace(_BOOT_TOOLCHAIN, "", 1)
        return t.replace("      - name: Piso de ferramentas\n", _BOOT_TOOLCHAIN + "      - name: Piso de ferramentas\n", 1)
    resultado = _g5_run(_FIXTURE_CI_YML, "wayland-container", mover)
    return _g5_expect("G1-X1-PREPARO-DEPOIS-DO-FINAL", resultado, "wayland-container", "tools/ci/env/")


def _g1_git_form(nome, linha):
    resultado = _g5_run(
        _FIXTURE_CI_YML, "wayland-container",
        lambda t: t.replace(
            "        run: rm -rf _bootstrap\n",
            "        run: |\n          " + linha + "\n          rm -rf _bootstrap\n", 1),
    )
    return _g5_expect(nome, resultado, "wayland-container", "usa git ANTES")


def selftest_g1_if_git_reproves():
    return _g1_git_form("G1-X5-IF-GIT", "if git rev-parse HEAD; then echo x; fi")


def selftest_g1_negated_git_reproves():
    return _g1_git_form("G1-GIT-NEGADO", "! git rev-parse HEAD")


def selftest_g1_env_prefixed_git_reproves():
    return _g1_git_form("G1-ENV-GIT", "env GIT_TRACE=1 git rev-parse HEAD")


def selftest_g1_var_prefixed_git_reproves():
    return _g1_git_form("G1-VAR-GIT", "GIT_TRACE=1 git rev-parse HEAD")



# CTO 29/09 achado 3 (mutantes X6, X7, X8): (G3b) todo `steps.<id>` que um
# passo cita tem de resolver para um `id: <id>` de um passo ANTERIOR do
# mesmo job. Um id apagado ou renomeado faz `steps.build.outcome ==
# 'success'` valer sempre falso e o passo pula calado. E (is_test_step)
# o passo que roda `ctest` (fora `ctest -N`, so' lista) e' passo de
# teste: o portao imprimia "0 de teste" no job linux.
def selftest_g3b_removed_id_reproves():
    resultado = _g5_run(_FIXTURE_CI_YML, "wayland-container", lambda t: t.replace("        id: build\n", "", 1))
    return _g5_expect("G3B-X6-ID-APAGADO", resultado, "wayland-container", "steps.build", "nenhum passo anterior")


def selftest_g3b_renamed_id_reproves():
    resultado = _g5_run(_FIXTURE_CI_YML, "wayland-container", lambda t: t.replace("        id: build\n", "        id: buildx\n", 1))
    return _g5_expect("G3B-X7-ID-RENOMEADO", resultado, "wayland-container", "steps.build")


def selftest_g3b_reference_before_id_reproves():
    def mover(t):
        bloco = "      - name: Sobe o compositor limpo\n        id: build\n        run: docker run -d --name c glintfx-wltest:ci\n\n"
        t = t.replace(bloco, "", 1)
        return t.replace("      - name: fixture a\n", "      - name: fixture a\n", 1).replace(
            "      - name: Publica o inventario do container (P-0)\n", bloco + "      - name: Publica o inventario do container (P-0)\n", 1)
    resultado = _g5_run(_FIXTURE_CI_YML, "wayland-container", mover)
    return _g5_expect("G3B-REFERENCIA-ANTES-DO-ID", resultado, "wayland-container", "steps.build")


_CTEST_STEP = (
    "      - name: Testes\n        shell: pwsh\n        run: |\n"
    "          ctest --test-dir build --output-on-failure\n\n"
)


def selftest_ctest_step_without_if_reproves():
    resultado = _g5_run(
        _FIXTURE_WINDOWS_JOB, "windows-x",
        lambda t: t.replace("      - name: fixture a\n", _CTEST_STEP + "      - name: fixture a\n", 1),
    )
    return _g5_expect("CTEST-SEM-IF", resultado, "windows-x", "'Testes'", "sem 'if:'")


def selftest_ctest_list_only_is_not_a_test_step():
    resultado = _g5_run(
        _FIXTURE_WINDOWS_JOB, "windows-x",
        lambda t: t.replace(
            "      - name: fixture a\n",
            "      - name: Lista\n        shell: pwsh\n        run: ctest --test-dir build -N > inv.txt\n\n      - name: fixture a\n", 1),
    )
    exit_code, output = resultado
    if exit_code not in (None, 0):
        print(f"selftest: CTEST-N-NAO-E-TESTE FALHOU (ctest -N so' lista, nao e' passo de teste): {output!r}", file=sys.stderr)
        return False
    print("selftest: CTEST-N-NAO-E-TESTE OK")
    return True



# CTO 29/09 achado 5: o marco `id: prep` fecha a fase de PREPARO. Passo
# que RODA portao/teste (tools/preci.sh, tests/tools/check_*, ctest,
# exec_fixture.sh) antes do marco faria uma falha de teste contar como
# falha de preparo, e a re-execucao automatica (A4) rodaria de novo
# uma reprovacao legitima.
def selftest_g3c_gate_step_before_prep_reproves():
    resultado = _g5_run(
        _FIXTURE_CI_YML, "wayland-container",
        lambda t: t.replace(
            "      - name: Preparo concluido\n        id: prep\n",
            "      - name: Verifica fiacao\n        run: python3 tests/tools/check_x.py --compare a b\n\n"
            "      - name: Preparo concluido\n        id: prep\n", 1),
    )
    return _g5_expect("G3C-PORTAO-ANTES-DO-PREP", resultado, "wayland-container", "Verifica fiacao", "ANTES do marco")


def selftest_g3c_preci_before_prep_reproves():
    resultado = _g5_run(
        _FIXTURE_CI_YML, "wayland-container",
        lambda t: t.replace(
            "      - name: Preparo concluido\n        id: prep\n",
            "      - name: preci.sh --selftest\n        run: tools/preci.sh --selftest\n\n"
            "      - name: Preparo concluido\n        id: prep\n", 1),
    )
    return _g5_expect("G3C-PRECI-ANTES-DO-PREP", resultado, "wayland-container", "preci.sh --selftest", "ANTES do marco")



# CTO 29/09 achado 4: o passo "Resultado agregado" tem de MEDIR os
# executados (tests/tools/ctest_aggregate.py, mesma medicao nos dois
# sistemas), nunca atribui-los aos declarados - `executados=$declarados`
# fazia a checagem `executados != declarados` uma tautologia.
_AGG_STEP = (
    "      - name: Resultado agregado (x)\n"
    "        if: ${{ !cancelled() }}\n"
    "        run: python3 tests/tools/ctest_aggregate.py --builddir build --inventory parity_inventory.txt\n\n"
)


_TESTES_JUNIT = (
    "      - name: Testes (x)\n"
    "        if: ${{ !cancelled() && steps.build.outcome == 'success' }}\n"
    "        run: ctest --test-dir build --output-on-failure --output-junit ctest-results-junit.xml\n\n"
)


_JUNIT_PUBLISH = (
    "      - name: Publicar JUnit do ctest (x)\n        if: ${{ !cancelled() }}\n"
    "        uses: actions/upload-artifact@v7\n        with:\n"
    "          name: ctest-junit-${{ matrix.slug }}-${{ matrix.modo }}\n"
    "          path: build/ctest-results-junit.xml\n          retention-days: 7\n\n"
)


def _agg_fixture(step, testes=_TESTES_JUNIT, publica=_JUNIT_PUBLISH):
    return _FIXTURE_CI_YML.replace("      - name: fixture a\n", testes + step + publica + "      - name: fixture a\n", 1)


def selftest_aggregate_with_script_passes():
    exit_code, output = _run_real_main_capturing(_agg_fixture(_AGG_STEP))
    if exit_code not in (None, 0):
        print(f"selftest: AGREGADO-POSITIVO FALHOU: {output!r}", file=sys.stderr)
        return False
    print("selftest: AGREGADO-POSITIVO OK (passo que chama ctest_aggregate.py passa)")
    return True


def selftest_aggregate_tautology_reproves():
    taut = (
        "      - name: Resultado agregado (x)\n        if: ${{ !cancelled() }}\n"
        "        run: |\n          declarados=3\n          executados=$declarados\n"
        "          [ \"$executados\" -ne \"$declarados\" ] && exit 1\n\n"
    )
    resultado = _run_real_main_capturing(_agg_fixture(taut))
    return _g5_expect("AGREGADO-TAUTOLOGIA", resultado, "Resultado agregado", "ctest_aggregate.py")


def selftest_aggregate_tautology_alongside_script_reproves():
    ambos = _AGG_STEP.replace("run: python3", "run: |\n          executados=$declarados\n          python3").replace(
        "--inventory parity_inventory.txt\n", "--inventory parity_inventory.txt\n")
    resultado = _run_real_main_capturing(_agg_fixture(ambos))
    return _g5_expect("AGREGADO-TAUTOLOGIA-COM-SCRIPT", resultado, "Resultado agregado", "tautologia")



# A1 (CTO 29/09): o agregado le o JUnit; sem `--output-junit` no passo de
# testes o script nao tem o que medir (e o LastTest.log conta pulado e
# desligado como "Test Passed.").
def selftest_aggregate_without_junit_output_reproves():
    resultado = _run_real_main_capturing(_agg_fixture(_AGG_STEP, _TESTES_JUNIT.replace(" --output-junit ctest-results-junit.xml", "")))
    return _g5_expect("AGREGADO-SEM-OUTPUT-JUNIT", resultado, "Resultado agregado", "--output-junit")



# --- A2 (CTO 29/09): "passo obrigatorio efetivo" ------------------------
# Um passo que chama prova/piso/agregado so' vale se NAO puder ser
# neutralizado: sem continue-on-error, sem `if:` (o agregado admite so'
# `!cancelled()`), a chamada literal do script como comando (sem `||`,
# sem pipe) e sem que o nome do script esteja so' num comentario de fim
# de linha ou como argumento de outro comando. Mutantes N4, N5, N10,
# N11, N12, N15, N16 da revisao do CTO.
def _mand(nome, fixture, job, velho, novo, *trechos):
    if velho not in fixture:
        print(f"selftest: {nome} FALHOU (ancora do replace nao casou)", file=sys.stderr)
        return False
    return _g5_expect(nome, _run_real_main_capturing(fixture.replace(velho, novo, 1), job_name=job), *trechos)


def selftest_mandatory_or_true_floor_reproves():  # N5
    return _mand("A2-N5-PISO-OR-TRUE", _FIXTURE_FIXED_JOB, "lint",
                 "        run: tools/ci/floor.sh\n", "        run: tools/ci/floor.sh || true\n", "tools/ci/floor.sh", "||")


def selftest_mandatory_script_as_argument_reproves():  # N10
    return _mand("A2-N10-SCRIPT-COMO-ARGUMENTO", _FIXTURE_FIXED_JOB, "lint",
                 "        run: tools/ci/floor.sh\n", "        run: bash -c 'exit 0' tools/ci/floor.sh\n", "tools/ci/floor.sh", "literal")


def selftest_mandatory_trailing_comment_reproves():  # N4
    return _mand("A2-N4-COMENTARIO-DE-FIM-DE-LINHA", _FIXTURE_FIXED_JOB, "lint",
                 "        run: tools/ci/prova_checkout.sh\n", "        run: echo pulei # tools/ci/prova_checkout.sh\n", "nao prova git")


def selftest_mandatory_if_false_reproves():  # N12
    return _mand("A2-N12-IF-FALSE", _FIXTURE_FIXED_JOB, "lint",
                 "        run: tools/ci/prova_checkout.sh\n", "        run: tools/ci/prova_checkout.sh\n        if: false\n", "tools/ci/prova_checkout.sh", "if:")


def selftest_mandatory_continue_on_error_reproves():  # N15
    return _mand("A2-N15-CONTINUE-ON-ERROR", _FIXTURE_WINDOWS_JOB, "windows-x",
                 "        run: tools/ci/windows/prep.ps1\n", "        run: tools/ci/windows/prep.ps1\n        continue-on-error: true\n",
                 "windows/prep.ps1", "continue-on-error")


def selftest_mandatory_aggregate_or_true_reproves():  # N11
    return _mand("A2-N11-AGREGADO-OR-TRUE", _agg_fixture(_AGG_STEP), "wayland-container",
                 "--inventory parity_inventory.txt\n", "--inventory parity_inventory.txt || true\n", "ctest_aggregate.py", "||")


def selftest_mandatory_aggregate_if_false_reproves():  # N16
    return _mand("A2-N16-AGREGADO-IF-FALSE", _agg_fixture(_AGG_STEP), "wayland-container",
                 "        if: ${{ !cancelled() }}\n        run: python3", "        if: false\n        run: python3", "ctest_aggregate.py", "if:")



# C3 (CTO 29/09, a razao do 46b21c1): a sonda do cl.exe recebe
# $env:RUNNER_TEMP, nunca o diretorio corrente/workspace (mutante PSB).
def selftest_prep_probe_root_from_workspace_reproves():
    sabotado = _PREP_PS1_OK.replace("Assert-MsvcAcceptsCxx23 $env:RUNNER_TEMP", "Assert-MsvcAcceptsCxx23 (Get-Location).Path")
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", files={PREP_PS1: sabotado})
    return _g5_expect("C3-PSB-SONDA-NO-WORKSPACE", resultado, PREP_PS1, "Assert-MsvcAcceptsCxx23 $env:RUNNER_TEMP")



# Item (i) do CTO (29/09) + run 36523231561: nos jobs Windows, o passo
# IMEDIATAMENTE depois do marco `id: prep` e' um passo PROPRIO cujo unico
# comando e' `prep.ps1 -VerifyCmake`. Como primeira linha de um passo que
# faz outra coisa ele encerrava o passo calado: o .ps1 sem `exit` deixa
# $LASTEXITCODE nulo, `$null -ne 0` e' verdadeiro e `exit $null` sai com 0
# (windows-lint/sanitizer/debug ficaram verdes sem compilar).
_VERIFY_STEP = (
    "      - name: CMake pinado (Windows)\n        shell: pwsh\n"
    "        run: tools/ci/windows/prep.ps1 -VerifyCmake\n\n"
)


def selftest_verify_cmake_missing_reproves():
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", lambda t: t.replace(_VERIFY_STEP, "", 1))
    return _g5_expect("VERIFYCMAKE-AUSENTE", resultado, "windows-x", "-VerifyCmake")


def selftest_verify_cmake_mixed_step_reproves():
    def misturar(t):
        t = t.replace(_VERIFY_STEP, "", 1)
        return t.replace(
            "        run: cmake --build build\n",
            "        run: |\n          tools/ci/windows/prep.ps1 -VerifyCmake\n          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }\n          cmake --build build\n", 1)
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", misturar)
    return _g5_expect("VERIFYCMAKE-EM-PASSO-MISTO (o defeito do run 36523231561)", resultado, "windows-x", "-VerifyCmake")


def selftest_verify_cmake_not_right_after_prep_reproves():
    def afastar(t):
        return t.replace(
            _VERIFY_STEP, "      - name: Antes\n        shell: pwsh\n        run: echo y\n\n" + _VERIFY_STEP, 1)
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", afastar)
    return _g5_expect("VERIFYCMAKE-NAO-COLADO-NO-PREP", resultado, "windows-x", "-VerifyCmake")


def selftest_ps1_call_followed_by_lines_reproves():
    resultado = _g5_run(
        _FIXTURE_WINDOWS_JOB, "windows-x",
        lambda t: t.replace(
            "      - name: Compilar\n",
            "      - name: Diagnostico\n        shell: pwsh\n        run: |\n          tools/ci/diagnose-x.ps1 -BuildDir b\n          Write-Host depois\n\n      - name: Compilar\n", 1),
    )
    return _g5_expect("PS1-CHAMADA-SEGUIDA-DE-LINHAS", resultado, "windows-x", "Diagnostico", "diagnose-x.ps1")


# C1 (CTO 29/09): o pino do CMake (prep.ps1) e o piso escrito em floor.sh e
# prep.ps1 estao amarrados a major.minor de `cmake_minimum_required(VERSION
# X.Y)` do CMakeLists.txt - a fonte do piso. Mutante Q4b: pino 4.3.0.
def _c1(nome, files, *trechos):
    resultado = _run_real_main_capturing(_FIXTURE_FIXED_JOB, job_name="lint", extra_files=files)
    return _g5_expect(nome, resultado, *trechos)


def selftest_pin_off_minimum_reproves():  # Q4b
    return _c1("C1-Q4B-PINO-FORA-DO-MINIMO", {PREP_PS1: _PREP_PS1_OK.replace("'4.1.6'", "'4.3.0'")},
               "CMakeLists.txt", "4.1", "pino do CMake 4.3")


def selftest_minimum_drift_reproves():
    return _c1("C1-MINIMO-MUDOU", {CMAKELISTS: "cmake_minimum_required(VERSION 4.2)\n"}, "CMakeLists.txt", "4.2")


def selftest_floor_sh_threshold_drift_reproves():
    return _c1("C1-PISO-FLOOR-SH-DIFERENTE", {FLOOR_SCRIPT: _FLOOR_SCRIPT_OK.replace('"$cmake_minor" -lt 1', '"$cmake_minor" -lt 0')},
               "floor.sh", "piso")


def selftest_prep_threshold_drift_reproves():
    return _c1("C1-PISO-PREP-DIFERENTE", {PREP_PS1: _PREP_PS1_OK.replace("$minor -lt 1", "$minor -lt 0")},
               "prep.ps1", "piso")



# run 36523231561: `cmake --version | Select-Object -First 1` interrompe o
# nativo e deixa $LASTEXITCODE nulo. E o `exit 0` explicito no fim do prep.ps1
# e' o que torna a checagem do chamador confiavel.
def selftest_prep_select_object_on_native_reproves():
    sabotado = _PREP_PS1_OK.replace("cmake --version\n", "$l = (cmake --version 2>$null | Select-Object -First 1)\n", 1)
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", files={PREP_PS1: sabotado})
    return _g5_expect("PS1-SELECT-OBJECT-SOBRE-NATIVO", resultado, PREP_PS1, "Select-Object")


def selftest_prep_without_explicit_exit_reproves():
    sabotado = _PREP_PS1_OK.replace("\nexit 0\n", "\n")
    resultado = _g5_run(_FIXTURE_WINDOWS_JOB, "windows-x", files={PREP_PS1: sabotado})
    return _g5_expect("PS1-SEM-EXIT-0-EXPLICITO", resultado, PREP_PS1, "exit 0")



# --- A4 (D-A4, docs/plano-ci-split-per-os.md 4.5): rerun_guard ----------
# Todo job tem um passo `rerun_guard.py` IMEDIATAMENTE antes do marco `id:
# prep` (o script mora no repo, entao vem depois do checkout; nos jobs com
# container, depois do piso, que ja provou o python), com GH_TOKEN e
# RERUN_CHECK_RUN_ID no env; e o workflow declara `permissions: actions:
# read`. Sem a trava, "no maximo uma reexecucao" e "so' infraestrutura" sao
# pratica, nao trava (memoria feedback_aviso_no_briefing_nao_e_portao).
_GUARD_LNX = (
    "      - name: Guarda de reexecucao (rerun_guard)\n        env:\n"
    "          GH_TOKEN: ${{ github.token }}\n          RERUN_CHECK_RUN_ID: ${{ job.check_run_id }}\n"
    "        run: python3 tools/ci/rerun_guard.py\n\n"
)


def selftest_rerun_guard_missing_reproves():
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", lambda t: t.replace(_GUARD_LNX, "", 1))
    return _g5_expect("A4-SEM-RERUN-GUARD", resultado, "lint", "rerun_guard")


def selftest_rerun_guard_not_before_prep_reproves():
    def afastar(t):
        t = t.replace(_GUARD_LNX, "", 1)
        return t.replace("      - name: Piso de ferramentas\n        run: tools/ci/floor.sh\n\n",
                         _GUARD_LNX + "      - name: Piso de ferramentas\n        run: tools/ci/floor.sh\n\n", 1)
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", afastar)
    return _g5_expect("A4-GUARDA-NAO-COLADA-NO-PREP", resultado, "lint", "rerun_guard")


def selftest_rerun_guard_without_token_reproves():
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", lambda t: t.replace("          GH_TOKEN: ${{ github.token }}\n", "", 1))
    return _g5_expect("A4-GUARDA-SEM-GH-TOKEN", resultado, "lint", "GH_TOKEN")


def selftest_rerun_guard_or_true_reproves():
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", lambda t: t.replace("run: python3 tools/ci/rerun_guard.py", "run: python3 tools/ci/rerun_guard.py || true", 1))
    return _g5_expect("A4-GUARDA-OR-TRUE", resultado, "lint", "rerun_guard.py", "||")


_PERMISSIONS_CI = "name: CI\non:\n  push:\n    branches: [main]\n" + _FIXTURE_FIXED_JOB


def selftest_permissions_actions_read_missing_reproves():
    resultado = _run_real_main_capturing(_PERMISSIONS_CI, job_name="lint")
    return _g5_expect("A4-SEM-PERMISSIONS-ACTIONS-READ", resultado, "permissions", "actions: read")


def selftest_permissions_actions_read_present_passes():
    texto = _PERMISSIONS_CI.replace("jobs:\n", "permissions:\n  contents: read\n  actions: read\n\njobs:\n", 1)
    exit_code, output = _run_real_main_capturing(texto, job_name="lint")
    if exit_code not in (None, 0):
        print(f"selftest: A4-PERMISSIONS-PRESENTE FALHOU: {output!r}", file=sys.stderr)
        return False
    print("selftest: A4-PERMISSIONS-PRESENTE OK")
    return True



# I3 (CTO 29/09, mutante G6e): o nome do marco `id: prep` e' EXATAMENTE
# PREP_MARKER de tools/ci/rerun_guard.py (fonte unica: o guarda classifica
# preparo x teste por esse nome; um marco renomeado numa perna faria a
# tentativa 2 reprovar como "job sem o marco"). C-a (mutante G6l): nenhum
# job declara `permissions:` proprio - so' o topo, senao um job poderia
# ampliar o token que o rerun_guard usa.
def selftest_prep_marker_renamed_reproves():  # G6e
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", lambda t: t.replace("      - name: Preparo concluido\n        id: prep\n", "      - name: Preparo concluido X\n        id: prep\n", 1))
    return _g5_expect("I3-G6E-MARCO-RENOMEADO", resultado, "lint", "tools/ci/rerun_guard.py", "PREP_MARKER")


def selftest_job_level_permissions_reproves():  # G6l
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", lambda t: t.replace("    container: fedora:latest\n", "    container: fedora:latest\n    permissions:\n      contents: write\n", 1))
    return _g5_expect("CA-G6L-PERMISSIONS-NO-JOB", resultado, "lint", "permissions")



# I1 (CTO 29/09): o guarda do job com `needs:` declara RERUN_DERIVADO: "1" (e
# so' ele) - o job derivado que falha em teste porque a perna de que depende
# falhou no preparo segue na tentativa 2. Sem a env, o parity reprovaria toda
# reexecucao legitima; com a env num job sem `needs:`, uma falha de teste
# propria seria perdoada.
def selftest_derived_job_without_env_reproves():
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", lambda t: t.replace("    container: fedora:latest\n", "    container: fedora:latest\n    needs: [outro]\n", 1))
    return _g5_expect("I1-NEEDS-SEM-RERUN-DERIVADO", resultado, "lint", "RERUN_DERIVADO")


def selftest_env_without_needs_reproves():
    resultado = _g5_run(_FIXTURE_FIXED_JOB, "lint", lambda t: t.replace("          GH_TOKEN: ${{ github.token }}\n", "          GH_TOKEN: ${{ github.token }}\n          RERUN_DERIVADO: \"1\"\n", 1))
    return _g5_expect("I1-RERUN-DERIVADO-SEM-NEEDS", resultado, "lint", "RERUN_DERIVADO")


def selftest_derived_job_with_env_passes():
    def derivar(t):
        t = t.replace("    container: fedora:latest\n", "    container: fedora:latest\n    needs: [outro]\n", 1)
        return t.replace("          GH_TOKEN: ${{ github.token }}\n", "          GH_TOKEN: ${{ github.token }}\n          RERUN_DERIVADO: \"1\"\n", 1)
    exit_code, output = _run_real_main_capturing(derivar(_FIXTURE_FIXED_JOB), job_name="lint")
    if exit_code not in (None, 0):
        print(f"selftest: I1-DERIVADO-COM-ENV FALHOU: {output!r}", file=sys.stderr)
        return False
    print("selftest: I1-DERIVADO-COM-ENV OK")
    return True



# I3 (CTO 29/09): o P2 do plano exige a duracao de CADA teste; o log so' traz
# as 25 maiores. O job que mede (agregado) publica o JUnit inteiro como
# artefato `ctest-junit-<slug>-<modo>`, `!cancelled()`, retention 7.
def selftest_junit_artifact_missing_reproves():
    resultado = _run_real_main_capturing(_agg_fixture(_AGG_STEP, publica=""))
    return _g5_expect("I3-JUNIT-SEM-ARTEFATO", resultado, "Resultado agregado", "ctest-junit-")


def selftest_junit_artifact_without_cancelled_reproves():
    quebrado = _JUNIT_PUBLISH.replace("if: ${{ !cancelled() }}", "if: always()")
    resultado = _run_real_main_capturing(_agg_fixture(_AGG_STEP, publica=quebrado))
    return _g5_expect("I3-JUNIT-ARTEFATO-SEM-CANCELLED", resultado, "Publicar JUnit", "!cancelled()")


def selftest_junit_artifact_wrong_name_reproves():
    resultado = _run_real_main_capturing(_agg_fixture(_AGG_STEP, publica=_JUNIT_PUBLISH.replace("ctest-junit-${{ matrix.slug }}-${{ matrix.modo }}", "junit")))
    return _g5_expect("I3-JUNIT-ARTEFATO-NOME-ERRADO", resultado, "Resultado agregado", "ctest-junit-")



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
        selftest_g2x_fixed_job_with_proof_script_passes(),
        selftest_g2x_fixed_job_without_proof_reproves(),
        selftest_g2x_proof_script_without_head_reproves(),
        selftest_g2x_proof_script_missing_reproves(),
        selftest_g5_positives(),
        selftest_g5_p1_windows_without_prep_reproves(),
        selftest_g5_linux_without_floor_reproves(),
        selftest_g5_p2_handwritten_cmake_install_reproves(),
        selftest_g5_floor_after_prep_reproves(),
        selftest_g5_floor_before_checkout_reproves(),
        selftest_g5_floor_script_without_cmake_check_reproves(),
        selftest_g5_prep_ps1_without_fo_reproves(),
        selftest_g5_prep_ps1_missing_reproves(),
        selftest_g5_comment_mentioning_script_is_not_a_call(),
        selftest_g2_comment_mentioning_proof_is_not_a_proof(),
        selftest_g2_or_true_reproves(),
        selftest_g2_sha_echoed_not_compared_reproves(),
        selftest_g2_inside_not_compared_reproves(),
        selftest_g1_bootstrap_toolchain_removed_reproves(),
        selftest_g1_bootstrap_toolchain_after_final_checkout_reproves(),
        selftest_g1_if_git_reproves(),
        selftest_g1_negated_git_reproves(),
        selftest_g1_env_prefixed_git_reproves(),
        selftest_g1_var_prefixed_git_reproves(),
        selftest_g3b_removed_id_reproves(),
        selftest_g3b_renamed_id_reproves(),
        selftest_g3b_reference_before_id_reproves(),
        selftest_ctest_step_without_if_reproves(),
        selftest_ctest_list_only_is_not_a_test_step(),
        selftest_g3c_gate_step_before_prep_reproves(),
        selftest_g3c_preci_before_prep_reproves(),
        selftest_aggregate_with_script_passes(),
        selftest_aggregate_tautology_reproves(),
        selftest_aggregate_tautology_alongside_script_reproves(),
        selftest_aggregate_without_junit_output_reproves(),
        selftest_junit_artifact_missing_reproves(),
        selftest_junit_artifact_without_cancelled_reproves(),
        selftest_junit_artifact_wrong_name_reproves(),
        selftest_mandatory_or_true_floor_reproves(),
        selftest_mandatory_script_as_argument_reproves(),
        selftest_mandatory_trailing_comment_reproves(),
        selftest_mandatory_if_false_reproves(),
        selftest_mandatory_continue_on_error_reproves(),
        selftest_mandatory_aggregate_or_true_reproves(),
        selftest_mandatory_aggregate_if_false_reproves(),
        selftest_prep_probe_root_from_workspace_reproves(),
        selftest_verify_cmake_missing_reproves(),
        selftest_verify_cmake_mixed_step_reproves(),
        selftest_verify_cmake_not_right_after_prep_reproves(),
        selftest_ps1_call_followed_by_lines_reproves(),
        selftest_prep_select_object_on_native_reproves(),
        selftest_prep_without_explicit_exit_reproves(),
        selftest_rerun_guard_missing_reproves(),
        selftest_rerun_guard_not_before_prep_reproves(),
        selftest_rerun_guard_without_token_reproves(),
        selftest_rerun_guard_or_true_reproves(),
        selftest_permissions_actions_read_missing_reproves(),
        selftest_permissions_actions_read_present_passes(),
        selftest_prep_marker_renamed_reproves(),
        selftest_job_level_permissions_reproves(),
        selftest_derived_job_without_env_reproves(),
        selftest_env_without_needs_reproves(),
        selftest_derived_job_with_env_passes(),
        selftest_pin_off_minimum_reproves(),
        selftest_minimum_drift_reproves(),
        selftest_floor_sh_threshold_drift_reproves(),
        selftest_prep_threshold_drift_reproves(),
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
