#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# rerun_guard.py - CI-SPLIT-PER-OS A4 (docs/plano-ci-split-per-os.md secao
# 4.5 e D-A4): "no maximo uma reexecucao" e "so' falha de infraestrutura"
# sao CONTAVEIS, entao sao trava no proprio job, nao pratica (memoria
# feedback_aviso_no_briefing_nao_e_portao).
#
# Regra, decidida por github.run_attempt:
#   1        : nao faz nada.
#   >= 3     : reprova ("no maximo uma reexecucao").
#   2        : consulta a API do proprio GitHub (GITHUB_TOKEN, permissao
#              `actions: read`) o job de MESMO NOME na tentativa 1 e so'
#              deixa seguir se o passo que falhou la' era de PREPARO (do
#              inicio do job ate o marco "Preparo concluido", inclusive:
#              inicializacao de container, checkout, toolchain, piso). Se
#              falhou depois do marco (teste), reprova - "a segunda falha e'
#              tratada como real, e falha de teste nao se reexecuta". Job
#              que passou na tentativa 1 segue (reexecucao de tudo).
#   Resposta vazia/404/ilegivel da API, job nao encontrado, job sem o marco
#   ou falha sem passo identificavel: REPROVA, nunca "nao achei, entao pode".
#
# Job com `needs:` (RERUN_DERIVADO=1, o portao exige a env se e so' se o job
# tem `needs:`): so' roda porque as pernas de que depende passaram. Se elas
# falharam em preparo, ele falha em "teste" na tentativa 1 sem culpa propria
# (ex.: parity). Na tentativa 2 ele segue se pelo menos um OUTRO job falhou e
# TODOS os outros falharam em preparo; qualquer outra falha de teste reprova.
#
# O nome do job da tentativa atual vem de GET /actions/jobs/<check_run_id>
# (RERUN_CHECK_RUN_ID = ${{ job.check_run_id }}), e a tentativa 1 de GET
# /actions/runs/<run_id>/attempts/1/jobs (paginado).
#
# Autoteste: respostas REAIS da API gravadas em tests/tools/fixtures/
# rerun_guard/ (run 36523231561, tentativa 1, `gh api`), nunca escritas a
# mao; as variantes que a API real nao ofereceu (falha de preparo, lista
# vazia, marco ausente) sao MUTACOES desses arquivos reais, marcadas como
# tal.
#
# Usage:
#   rerun_guard.py             (no CI; le o ambiente)
#   rerun_guard.py --selftest

import json
import os
import sys
import urllib.error
import urllib.request

SCRIPT_NAME = "rerun_guard.py"
PREP_MARKER = "Preparo concluido"
FIXTURES = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "tests", "tools", "fixtures", "rerun_guard",
)


# --- decisao pura (sem rede) -------------------------------------------


def job_name_from_response(job_json):
    """Nome do job da resposta de GET /actions/jobs/<id>, ou None."""
    if not isinstance(job_json, dict):
        return None
    name = job_json.get("name")
    return name if isinstance(name, str) and name else None


def find_job(attempt1_jobs, name):
    return next((j for j in attempt1_jobs if j.get("name") == name), None)


def classify_failure(job):
    """(fase, passo) da falha do job da tentativa 1: fase e' 'prep', 'teste'
    ou 'desconhecida'."""
    steps = job.get("steps") or []
    marker = next((s for s in steps if s.get("name") == PREP_MARKER), None)
    if marker is None:
        return "desconhecida", f"job sem o passo marco {PREP_MARKER!r} (nao da' para separar preparo de teste)"
    failed = sorted((s for s in steps if s.get("conclusion") == "failure"), key=lambda s: s.get("number", 0))
    if not failed:
        return "desconhecida", f"job com conclusao {job.get('conclusion')!r} e nenhum passo com falha"
    first = failed[0]
    fase = "prep" if first.get("number", 0) <= marker.get("number", 0) else "teste"
    return fase, first.get("name", "?")


def _others_failed_only_in_prep(attempt1_jobs, this_job):
    """(ok, motivo): existe pelo menos um OUTRO job que nao passou na
    tentativa 1 e TODOS eles falharam em preparo (mesma classify_failure)."""
    outros = [
        j for j in attempt1_jobs
        if j is not this_job and j.get("conclusion") in ("failure", "cancelled", "timed_out")
    ]
    if not outros:
        return False, "nenhum outro job falhou na tentativa 1 - a falha de teste deste job e' dele"
    for outro in outros:
        fase, passo = classify_failure(outro)
        if fase != "prep":
            return False, f"o job {outro.get('name')!r} tambem falhou na tentativa 1 fora do preparo ({fase}: {passo})"
    return True, f"{len(outros)} outro(s) job(s) falharam so' em preparo"


def decide(attempt, attempt1_jobs, job_name, derivado=False):
    """(permite, mensagem). attempt1_jobs: lista de jobs da tentativa 1 ou
    None quando a API nao respondeu com jobs. `derivado`: o job tem `needs:`
    (RERUN_DERIVADO=1) - roda so' porque outras pernas passaram, entao a
    falha dele em TESTE segue se as outras falharam so' em preparo (I1)."""
    if not isinstance(attempt, int) or attempt < 1:
        return False, f"github.run_attempt invalido: {attempt!r}"
    if attempt == 1:
        return True, "tentativa 1: nada a checar"
    if attempt >= 3:
        return False, f"tentativa {attempt}: no maximo uma reexecucao (D-A4)"
    if not attempt1_jobs:
        return False, "resposta vazia da API para a tentativa 1 - nao se reexecuta sem saber por que falhou"
    job = find_job(attempt1_jobs, job_name)
    if job is None:
        return False, f"job {job_name!r} nao encontrado na tentativa 1 - nao se reexecuta sem saber por que falhou"
    if job.get("conclusion") in ("success", "skipped"):
        return True, f"job {job_name!r} nao falhou na tentativa 1 ({job.get('conclusion')})"
    fase, passo = classify_failure(job)
    if fase == "prep":
        return True, f"falha de INFRAESTRUTURA na tentativa 1 (passo de preparo {passo!r}): reexecucao permitida"
    if fase == "teste" and derivado:
        ok, motivo = _others_failed_only_in_prep(attempt1_jobs, job)
        if ok:
            return True, (
                f"job derivado (needs:) falhou em TESTE na tentativa 1 (passo {passo!r}), mas {motivo}: "
                f"a falha veio das pernas de preparo, reexecucao permitida"
            )
        return False, (
            f"falha de TESTE na tentativa 1 (passo {passo!r}) num job derivado, e {motivo} - "
            f"a segunda falha e' tratada como real (D-A4)"
        )
    if fase == "teste":
        return False, (
            f"falha de TESTE na tentativa 1 (passo {passo!r}): a segunda falha e' tratada como real, "
            f"e falha de teste nao se reexecuta (D-A4)"
        )
    return False, f"falha nao classificavel na tentativa 1: {passo}"


# --- rede (injetavel) -------------------------------------------------


def http_get_json(url, token):
    request = urllib.request.Request(url, headers={
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
    })
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.loads(response.read().decode("utf-8"))
    except (urllib.error.URLError, ValueError, OSError):
        return None


def fetch_attempt1_jobs(api, repo, run_id, fetch):
    """Todas as paginas de jobs da tentativa 1, ou None se a API falhou."""
    jobs = []
    for page in range(1, 11):
        data = fetch(f"{api}/repos/{repo}/actions/runs/{run_id}/attempts/1/jobs?per_page=100&page={page}")
        if not isinstance(data, dict) or not isinstance(data.get("jobs"), list):
            return None
        jobs.extend(data["jobs"])
        if len(jobs) >= data.get("total_count", 0) or not data["jobs"]:
            break
    return jobs


def run_guard(env, fetch):
    """Devolve (rc, mensagem). `fetch(url)` -> JSON ou None."""
    try:
        attempt = int(env.get("GITHUB_RUN_ATTEMPT", ""))
    except ValueError:
        return 1, f"{SCRIPT_NAME}: GITHUB_RUN_ATTEMPT ausente ou nao numerico"
    if attempt == 1 or attempt >= 3:
        permite, msg = decide(attempt, None, None)
        return (0 if permite else 1), f"{SCRIPT_NAME}: {msg}"
    api = env.get("GITHUB_API_URL", "https://api.github.com")
    repo, run_id, check_run_id = env.get("GITHUB_REPOSITORY"), env.get("GITHUB_RUN_ID"), env.get("RERUN_CHECK_RUN_ID")
    if not (repo and run_id and check_run_id):
        return 1, f"{SCRIPT_NAME}: GITHUB_REPOSITORY/GITHUB_RUN_ID/RERUN_CHECK_RUN_ID ausente - nao se reexecuta sem saber qual job e' este"
    name = job_name_from_response(fetch(f"{api}/repos/{repo}/actions/jobs/{check_run_id}"))
    if name is None:
        return 1, f"{SCRIPT_NAME}: nao foi possivel ler o nome deste job pela API - nao se reexecuta sem saber qual job e' este"
    derivado = env.get("RERUN_DERIVADO") == "1"
    permite, msg = decide(attempt, fetch_attempt1_jobs(api, repo, run_id, fetch), name, derivado)
    return (0 if permite else 1), f"{SCRIPT_NAME}: {msg}"


# --- autoteste com respostas REAIS gravadas ----------------------------


def _load(name):
    with open(os.path.join(FIXTURES, name), "r", encoding="utf-8") as handle:
        return json.load(handle)


def _check(nome, condicao, detalhe=""):
    print(f"selftest: {nome} {'OK' if condicao else 'FALHOU'}" + ("" if condicao else f" - {detalhe}"))
    return condicao


def _jobs_named(data, prefix):
    return next(j for j in data["jobs"] if j["name"].startswith(prefix))


def _mut_prep_failure(data, prefix, so_a_primeira=False):
    """MUTACAO do arquivo real: o job falha no checkout (passo 2, preparo). No
    real os passos seguintes com `if: !cancelled()` RODAM e FALHAM (I2, CTO
    29/09) - entao todo passo seguinte que nao seja pos-processamento vira
    'failure' tambem (a menos que so_a_primeira)."""
    novo = json.loads(json.dumps(data))
    job = _jobs_named(novo, prefix)
    job["conclusion"] = "failure"
    for passo in job["steps"]:
        if passo["number"] == 2:
            passo["conclusion"] = "failure"
        elif passo["number"] > 2 and passo["name"] != "Complete job" and not passo["name"].startswith("Post "):
            passo["conclusion"] = "skipped" if so_a_primeira else "failure"
    return novo


def _mut_test_failure(data, prefix, passo_nome):
    """MUTACAO do arquivo real: o job falha no passo `passo_nome` (depois do marco)."""
    novo = json.loads(json.dumps(data))
    job = _jobs_named(novo, prefix)
    job["conclusion"] = "failure"
    for passo in job["steps"]:
        if passo["name"] == passo_nome:
            passo["conclusion"] = "failure"
    return novo


def selftest_main():
    real = _load("run36523231561_attempt1_jobs.json")
    lint_win = _jobs_named(real, "Windows - Lint")["name"]
    lint_lnx = _jobs_named(real, "Lint (Fedora")["name"]
    jobs = real["jobs"]
    controles = [
        _check("tentativa 1 permite", decide(1, None, None)[0]),
        _check("tentativa 3 reprova", not decide(3, jobs, lint_lnx)[0]),
        _check("tentativa 4 reprova", not decide(4, jobs, lint_lnx)[0]),
        _check("tentativa 0/invalida reprova", not decide(0, jobs, lint_lnx)[0] and not decide(None, jobs, lint_lnx)[0]),
    ]
    permite, msg = decide(2, jobs, lint_win)
    controles.append(_check(
        "tentativa 2, falha de TESTE real (windows-lint, passo 8) reprova, citando o passo",
        (not permite) and "Prova - compile_commands.json" in msg, msg))
    controles.append(_check("tentativa 2, job que passou na tentativa 1 (real) permite", decide(2, jobs, lint_lnx)[0]))
    mut = _mut_prep_failure(real, "Windows - Lint")
    permite, msg = decide(2, mut["jobs"], lint_win)
    controles.append(_check("tentativa 2, falha de PREPARO (checkout; mutacao do arquivo real) permite", permite and "INFRAESTRUTURA" in msg, msg))
    controles.append(_check("tentativa 2, resposta 404 REAL da API reprova",
                            not decide(2, fetch_attempt1_jobs("A", "r", "1", lambda u: _load("run36523231561_attempt9_jobs_vazio.json")), lint_lnx)[0]))
    controles.append(_check("tentativa 2, lista vazia (mutacao) reprova", not decide(2, [], lint_lnx)[0]))
    controles.append(_check("tentativa 2, job inexistente na tentativa 1 reprova", not decide(2, jobs, "Job que nao existe")[0]))
    sem_marco = json.loads(json.dumps(mut))
    for passo in _jobs_named(sem_marco, "Windows - Lint")["steps"]:
        if passo["name"] == PREP_MARKER:
            passo["name"] = "Outro nome"
    controles.append(_check("tentativa 2, job sem o marco (mutacao) reprova", not decide(2, sem_marco["jobs"], lint_win)[0]))
    controles.append(_check("nome do job pela API (resposta real de /actions/jobs/<id>)",
                            job_name_from_response(_load("job109260499103.json")) == lint_win))
    controles.append(_check("nome do job: 404 REAL nao da' nome", job_name_from_response(_load("run36523231561_attempt9_jobs_vazio.json")) is None))
    controles.append(_check(
        "I2: a PRIMEIRA falha decide (checkout falha, os passos seguintes tambem falham) permite",
        decide(2, _mut_prep_failure(real, "Windows - Lint")["jobs"], lint_win)[0]))
    controles.append(_check(
        "I2: so' o checkout falhou (o resto skipped) tambem permite",
        decide(2, _mut_prep_failure(real, "Windows - Lint", so_a_primeira=True)["jobs"], lint_win)[0]))
    controles.extend(_selftest_derivado(real))
    controles.extend(_selftest_run_guard(real, lint_win))
    if not all(controles):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controles)} controles OK")


def _selftest_derivado(real):
    """I1 (CTO 29/09): job com `needs:` (RERUN_DERIVADO=1) que falha em TESTE so'
    porque uma perna falhou no preparo segue na tentativa 2; qualquer outra
    falha de teste, ou nenhuma outra falha, reprova. Mutacoes dos arquivos reais."""
    parity = _jobs_named(real, "Paridade")["name"]
    fedora = _jobs_named(real, "Fedora (primario) - compartilhado")
    # perna Fedora falha em "Instalar toolchain" (preparo, passo 4) e o parity
    # falha em "Separar e unir inventarios por sistema" (passo 5, depois do marco).
    # O run real tem 3 pernas Windows vermelhas (teste): a mutacao as CURA para
    # isolar o cenario (so' a perna Fedora falha, em preparo).
    curado = _heal(real)
    base = _mut_test_failure(curado, "Paridade", "Separar e unir inventarios por sistema")
    for passo in _jobs_named(base, "Fedora (primario) - compartilhado")["steps"]:
        if passo["name"] == "Instalar toolchain":
            passo["conclusion"] = "failure"
    _jobs_named(base, "Fedora (primario) - compartilhado")["conclusion"] = "failure"
    outra_teste = json.loads(json.dumps(base))  # + a perna windows-lint com a falha de TESTE REAL de volta
    real_lint = _jobs_named(real, "Windows - Lint")
    outra_teste["jobs"] = [real_lint if j["name"] == real_lint["name"] else j for j in outra_teste["jobs"]]
    so_parity = _mut_test_failure(curado, "Paridade", "Separar e unir inventarios por sistema")
    _selftest_derivado.cenario = (base, parity)
    return [
        _check("I1: derivado + so' pernas com falha de PREPARO segue",
               decide(2, base["jobs"], parity, derivado=True)[0]),
        _check("I1: derivado + outra perna com falha de TESTE (windows-lint real) reprova",
               not decide(2, outra_teste["jobs"], parity, derivado=True)[0]),
        _check("I1: derivado, sozinho a falhar em teste (nenhuma outra falha) reprova",
               not decide(2, so_parity["jobs"], parity, derivado=True)[0]),
        _check("I1: NAO derivado nao ganha a excecao (mesma resposta, derivado=False) reprova",
               not decide(2, base["jobs"], parity, derivado=False)[0]),
        _check("I1: outra perna cancelada nao conta como preparo (reprova)",
               not decide(2, _cancel(base, "Arch - compartilhado")["jobs"], parity, derivado=True)[0]),
    ]


def _heal(data):
    """MUTACAO do arquivo real: todo job/passo com falha vira success."""
    novo = json.loads(json.dumps(data))
    for job in novo["jobs"]:
        if job.get("conclusion") == "failure":
            job["conclusion"] = "success"
        for passo in job.get("steps", []):
            if passo.get("conclusion") in ("failure", "skipped"):
                passo["conclusion"] = "success"
    return novo


def _cancel(data, prefix):
    novo = json.loads(json.dumps(data))
    _jobs_named(novo, prefix)["conclusion"] = "cancelled"
    return novo


def _selftest_run_guard(real, lint_win):
    """A costura de ponta a ponta (run_guard), com `fetch` servindo as
    respostas gravadas."""
    base = {"GITHUB_REPOSITORY": "petrinhu/GlintFx", "GITHUB_RUN_ID": "36523231561", "RERUN_CHECK_RUN_ID": "109260499103"}
    job = _load("job109260499103.json")

    def servir(tentativa1):
        def fetch(url):
            return job if "/actions/jobs/" in url else tentativa1
        return fetch
    rc1, _ = run_guard({**base, "GITHUB_RUN_ATTEMPT": "1"}, lambda u: None)
    rc3, _ = run_guard({**base, "GITHUB_RUN_ATTEMPT": "3"}, lambda u: None)
    rc2t, _ = run_guard({**base, "GITHUB_RUN_ATTEMPT": "2"}, servir(real))
    rc2p, _ = run_guard({**base, "GITHUB_RUN_ATTEMPT": "2"}, servir(_mut_prep_failure(real, "Windows - Lint")))
    rc2x, _ = run_guard({**base, "GITHUB_RUN_ATTEMPT": "2"}, lambda u: None)
    rc2s, _ = run_guard({"GITHUB_RUN_ATTEMPT": "2"}, servir(real))
    rcbad, _ = run_guard({**base, "GITHUB_RUN_ATTEMPT": "abc"}, servir(real))
    base_derivada, parity = _selftest_derivado.cenario
    pj = {"name": parity}
    fetch_d = lambda u: pj if "/actions/jobs/" in u else base_derivada
    rcd1, _ = run_guard({**base, "GITHUB_RUN_ATTEMPT": "2", "RERUN_DERIVADO": "1"}, fetch_d)
    rcd0, _ = run_guard({**base, "GITHUB_RUN_ATTEMPT": "2"}, fetch_d)
    return [
        _check("run_guard: job derivado (RERUN_DERIVADO=1) com so' preparo nas outras pernas rc=0", rcd1 == 0),
        _check("run_guard: o mesmo cenario SEM RERUN_DERIVADO rc=1", rcd0 == 1),
        _check("run_guard: tentativa 1 rc=0", rc1 == 0),
        _check("run_guard: tentativa 3 rc=1", rc3 == 1),
        _check("run_guard: tentativa 2 + falha de teste real rc=1", rc2t == 1),
        _check("run_guard: tentativa 2 + falha de preparo (mutacao) rc=0", rc2p == 0),
        _check("run_guard: tentativa 2 + API sem resposta rc=1", rc2x == 1),
        _check("run_guard: tentativa 2 sem repositorio/job no ambiente rc=1", rc2s == 1),
        _check("run_guard: GITHUB_RUN_ATTEMPT nao numerico rc=1", rcbad == 1),
    ]


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        selftest_main()
        return
    env = dict(os.environ)
    token = env.get("GH_TOKEN") or env.get("GITHUB_TOKEN", "")
    rc, msg = run_guard(env, lambda url: http_get_json(url, token))
    print(msg, file=sys.stderr if rc else sys.stdout)
    sys.exit(rc)


if __name__ == "__main__":
    main()
