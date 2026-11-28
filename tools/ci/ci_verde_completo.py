#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# ci_verde_completo.py - instrumento da linha CI-VERDE-<parte> (EFIC-CI-SELETIVO, E3, D-W8-1231;
# GODS_LAWS.md L-18 e L-40 do projeto, L-36 global). Diz se um run do CI foi COMPLETO de verdade: o modo
# decidido pelo trabalho `mudancas` tem de ser `completo`, e todo trabalho tem de ter saido `success`,
# com uma unica excecao: o trabalho de etiqueta pode sair `skipped` (ele so roda em etiqueta). Zero
# trabalhos reprova (piso de varredura nao-vazia, L-40 do projeto). So biblioteca padrao (L-07 do projeto).
#
# O trabalho de etiqueta e' identificado pelo NOME DE EXIBICAO exato (`name:` do job no ci.yml), que e' o
# unico campo que `gh run view --json jobs` devolve: a chave do job (`version-tag`) nao vem no JSON. O
# texto e' fixado aqui e conferido contra o ci.yml real pelo `ci_verde_completo_selftest.py --ci-yml`
# (portao), entao renomear o job reprova no CI local, e nao so no servidor. Nome diferente nunca e'
# excecao (L-40): igualdade exata, sem substring e sem caixa frouxa.
#
# Usage:
#   ci_verde_completo.py <run-id> [--repo <dono/nome>] [--dir-mudancas <dir>]
#
# Codigos de saida: 0 verde completo; 1 reprovado (o veredito diz por que); 2 guarda (insumo ausente:
# gh, JSON ou artefato `mudancas`). Nenhuma falha de insumo vira verde.
#
# Insumos: `gh run view <id> --json jobs,headSha,event` (conclusao por trabalho) e o artefato `mudancas`
# baixado para <dir>, de onde se le `mudancas.json` (campo `modo`).

import json
import os
import subprocess
import sys
import tempfile

SCRIPT_NAME = "ci_verde_completo.py"
TRABALHO_SO_EM_TAG = "Versao == etiqueta (VERSION-TAG-SYNC)"  # `name:` do job `version-tag` no ci.yml
MODOS_ACEITOS = ("completo",)


class Guarda(Exception):
    """Insumo ausente ou ilegivel: sai com 2, sem veredito."""


def veredito(trabalhos, modo):
    """Devolve (rc, linha). rc 0 so' com modo completo, zero reprovas e todo trabalho aceito.

    Funcao pura: nao chama gh nem le arquivo. E' o que o autoteste trava (tests/tools/
    ci_verde_completo_selftest.py) com fixtures em memoria.
    """
    if modo not in MODOS_ACEITOS:
        return 1, f"REPROVADO: modo: {modo} recusado (o instrumento so aceita completo)"

    motivos = []
    verdes = 0
    pulados = 0
    de_ci = 0
    for trabalho in trabalhos:
        nome = trabalho.get("name", "?")
        conclusao = trabalho.get("conclusion", "")
        if conclusao == "skipped":
            pulados += 1
        if nome != TRABALHO_SO_EM_TAG:
            de_ci += 1
        if nome == TRABALHO_SO_EM_TAG:
            if conclusao in ("success", "skipped"):
                verdes += 1
            else:
                motivos.append(f"{nome} saiu {conclusao or 'sem conclusao'}")
            continue
        if conclusao == "success":
            verdes += 1
        elif conclusao == "skipped":
            motivos.append(f"{nome} pulado (skipped) fora do trabalho de etiqueta")
        elif conclusao == "":
            motivos.append(f"{nome} sem conclusao (run em curso ou dado ausente)")
        else:
            motivos.append(f"{nome} saiu {conclusao}")

    total = len(trabalhos)
    resumo = f"trabalhos: {verdes} de {total}; pulados: {pulados}; modo: {modo}"
    if de_ci == 0:
        return 1, f"REPROVADO: zero trabalhos de CI (L-40 do projeto); {resumo}"
    if motivos:
        return 1, "REPROVADO: " + "; ".join(motivos) + f"; {resumo}"
    return 0, resumo


def _gh(*args):
    try:
        proc = subprocess.run(["gh", *args], capture_output=True, text=True)
    except FileNotFoundError as exc:
        raise Guarda(f"gh ausente no PATH ({exc})")
    if proc.returncode != 0:
        raise Guarda(f"gh {' '.join(args)} saiu {proc.returncode}: {proc.stderr.strip()}")
    return proc.stdout


def _ler_run(run_id, repo):
    args = ["run", "view", str(run_id), "--json", "jobs,headSha,event"]
    if repo:
        args += ["-R", repo]
    try:
        return json.loads(_gh(*args))
    except json.JSONDecodeError as exc:
        raise Guarda(f"JSON de gh run view ilegivel: {exc}")


def _baixar_modo(run_id, repo, pasta):
    args = ["run", "download", str(run_id), "-n", "mudancas", "-D", pasta]
    if repo:
        args += ["-R", repo]
    _gh(*args)
    caminho = os.path.join(pasta, "mudancas.json")
    if not os.path.isfile(caminho):
        raise Guarda(f"artefato mudancas sem mudancas.json em {pasta}")
    try:
        with open(caminho, encoding="utf-8") as handle:
            dados = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise Guarda(f"mudancas.json ilegivel: {exc}")
    modo = dados.get("modo")
    if not isinstance(modo, str) or not modo:
        raise Guarda("mudancas.json sem campo modo")
    return modo


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(f"uso: {SCRIPT_NAME} <run-id> [--repo <dono/nome>] [--dir-mudancas <dir>]", file=sys.stderr)
        return 2
    run_id = argv[0]
    repo = None
    pasta = None
    resto = argv[1:]
    while resto:
        flag = resto.pop(0)
        if flag == "--repo" and resto:
            repo = resto.pop(0)
        elif flag == "--dir-mudancas" and resto:
            pasta = resto.pop(0)
        else:
            print(f"{SCRIPT_NAME}: argumento desconhecido {flag!r}", file=sys.stderr)
            return 2
    try:
        if pasta is None:
            pasta = tempfile.mkdtemp(prefix="ci-verde-", dir=os.environ.get("TMPDIR") or None)
        run = _ler_run(run_id, repo)
        modo = _baixar_modo(run_id, repo, pasta)
    except Guarda as exc:
        print(f"{SCRIPT_NAME}: GUARDA - {exc}", file=sys.stderr)
        return 2
    trabalhos = run.get("jobs") or []
    rc, linha = veredito(trabalhos, modo)
    print(f"run {run_id} (headSha {run.get('headSha', '?')}, evento {run.get('event', '?')}): {linha}")
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
