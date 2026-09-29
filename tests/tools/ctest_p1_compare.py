#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# ctest_p1_compare.py - CI-SPLIT-PER-OS A5 etapa 3 (docs/plano-ci-split-per-os.md
# 4.8, P1 a P3 e regra 4; decisoes do CTO em D-A24, 29/09/2026): compara as 6
# rodadas de calibracao do ctest (r0 = serial, r1..r5 = paralelas), no MESMO
# SHA, lendo os artefatos `ctest-junit-<slug>-<modo>` baixados com
# `gh run download <run_id> -D <dir>` (um diretorio por rodada; cada artefato traz
# o JUnit e o ctest-show.json).
#
#   P1: por perna (sistema x modo), o CONJUNTO DE APROVADOS e' identico nas 6
#       rodadas. Diferenca reprova e o relatorio traz, para cada teste divergente,
#       o status e a DURACAO em cada rodada (o primeiro passo da investigacao e'
#       ler as duracoes do JUnit do teste divergente - nunca reexecutar).
#   P2: para cada teste, a maior duracao nas 5 rodadas paralelas (todas as pernas)
#       contra TIMEOUT/3 (TIMEOUT do json-v1; 120 s se nao tem). Quem nao tem folga
#       ganha, pela regra 4, TIMEOUT novo = 3 x maior duracao arredondado para CIMA em
#       multiplos de 30 s - nunca abaixo do TIMEOUT atual. So' RELATA (rc nao muda).
#   P3 (opcional, --jobs): duracao do passo "Testes (...)" de cada job, serial x pior
#       rodada paralela, lida das respostas de `gh api runs/<id>/jobs`. Ganho abaixo
#       de 50% REPROVA e a regua NAO se move (L-43): "P3 reprovado, ganho medido X%".
#
# Piso de varredura (L-40): 0 pernas ou menos de 6 rodadas reprova.
#
# Usage:
#   ctest_p1_compare.py --rounds <r0> <r1> <r2> <r3> <r4> <r5> [--jobs <j0> ... <j5>]
#   ctest_p1_compare.py --selftest

import glob
import importlib.util
import json
import math
import os
import sys
import xml.etree.ElementTree as ElementTree
from datetime import datetime

SCRIPT_NAME = "ctest_p1_compare.py"
ROUNDS = 6
GAIN_TARGET = 0.5
FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures")


def _load_aggregate():
    caminho = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ctest_aggregate.py")
    spec = importlib.util.spec_from_file_location("glintfx_ctest_aggregate", caminho)
    modulo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modulo)
    return modulo


AGG = _load_aggregate()


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def find_legs(round_dir):
    return sorted(d for d in os.listdir(round_dir) if d.startswith("ctest-junit-") and os.path.isdir(os.path.join(round_dir, d)))


def _one(pattern, root):
    achados = glob.glob(os.path.join(root, "**", pattern), recursive=True)
    return achados[0] if achados else None


def read_leg(round_dir, leg):
    """({nome: (status, duracao)}, {nome: timeout}) de uma perna numa rodada."""
    junit = _one("ctest-results-junit.xml", os.path.join(round_dir, leg))
    if junit is None:
        fail(f"{round_dir}/{leg}: ctest-results-junit.xml ausente")
    root = ElementTree.parse(junit).getroot()
    resultados = {}
    for case in root.iter("testcase"):
        status, _motivo = AGG.classify_case(case)
        try:
            duracao = float(case.get("time", "0"))
        except ValueError:
            duracao = 0.0
        resultados[case.get("name", "?")] = (status, duracao)
    timeouts = {}
    show = _one("ctest-show.json", os.path.join(round_dir, leg))
    if show is not None:
        with open(show, "r", encoding="utf-8") as handle:
            for teste in json.load(handle)["tests"]:
                for prop in teste.get("properties", []):
                    if prop["name"] == "TIMEOUT":
                        timeouts[teste["name"]] = float(prop["value"])
    return resultados, timeouts


def p1_errors(leg, por_rodada):
    """Diferencas no conjunto de aprovados entre as rodadas; devolve mensagens."""
    aprovados = [{n for n, (s, _d) in r.items() if s == "passou"} for r in por_rodada]
    uniao = set().union(*aprovados)
    divergentes = sorted(n for n in uniao if not all(n in a for a in aprovados))
    linhas = []
    for nome in divergentes:
        detalhe = " ".join(
            f"r{i}={por_rodada[i].get(nome, ('ausente', 0.0))[0]}/{por_rodada[i].get(nome, ('ausente', 0.0))[1]:.2f}s"
            for i in range(len(por_rodada))
        )
        linhas.append(f"P1 {leg}: {nome} nao e' aprovado em todas as rodadas - {detalhe} (investigar pelas duracoes; nunca reexecutar)")
    return linhas, len(aprovados[0])


def round_up_30(segundos):
    return int(math.ceil(segundos / 30.0) * 30)


def p2_report(maximos, timeouts):
    """[(nome, max, timeout_atual, timeout_novo|None)] dos testes sem folga (max >= TIMEOUT/3)."""
    fora = []
    for nome, maximo in sorted(maximos.items(), key=lambda kv: -kv[1]):
        atual = timeouts.get(nome, AGG.DEFAULT_TIMEOUT_S)  # fonte unica: ctest_aggregate.py
        if maximo >= atual / 3:
            novo = round_up_30(3 * maximo)
            fora.append((nome, maximo, atual, novo if novo > atual else None))
    return fora


# Passos que usam o grau de calibracao (I-1, CTO 29/09): so' os dois modos das pernas de
# calibracao. "Testes (ASan, rotulo unit)" e "Testes (Debug, suite inteira)" (Windows
# Sanitizer e Debug) sao passos de teste proprios, sem o grau, e reprovariam o P3 com 0%.
CALIBRATION_STEPS = ("Testes (compartilhado)", "Testes (estatico)")


def _step_seconds(job):
    for passo in job.get("steps", []):
        if passo.get("name", "") in CALIBRATION_STEPS:
            try:
                ini = datetime.fromisoformat(passo["started_at"].replace("Z", "+00:00"))
                fim = datetime.fromisoformat(passo["completed_at"].replace("Z", "+00:00"))
            except (KeyError, ValueError):
                return None
            return (fim - ini).total_seconds()
    return None


def p3_report(jobs_por_rodada):
    """([(job, serial, pior_paralelo, ganho)], erros)."""
    def por_nome(dados):
        return {j["name"]: _step_seconds(j) for j in dados["jobs"]}
    serial = por_nome(jobs_por_rodada[0])
    paralelos = [por_nome(d) for d in jobs_por_rodada[1:]]
    linhas, erros = [], []
    for job, s in sorted(serial.items()):
        if s is None:
            continue
        piores = [p.get(job) for p in paralelos if p.get(job) is not None]
        if not piores:
            erros.append(f"P3 {job}: sem passo de calibracao (Testes compartilhado/estatico) nas rodadas paralelas")
            continue
        pior = max(piores)
        ganho = 1 - pior / s if s > 0 else 0.0
        linhas.append((job, s, pior, ganho))
        if pior > s * GAIN_TARGET:
            erros.append(f"P3 reprovado, ganho medido {ganho * 100:.0f}% em {job} (serial {s:.0f} s, pior paralela {pior:.0f} s; alvo {int((1 - GAIN_TARGET) * 100)}%)")
    return linhas, erros


def compare(round_dirs, jobs_files=None):
    """(rc, linhas de relatorio, erros)."""
    if len(round_dirs) != ROUNDS:
        return 1, [], [f"{ROUNDS} rodadas esperadas (r0 serial + r1..r5 paralelas), recebi {len(round_dirs)}"]
    legs = find_legs(round_dirs[0])
    if not legs:
        return 1, [], [f"varredura vazia: nenhuma perna `ctest-junit-*` em {round_dirs[0]} - L-40"]
    relatorio, erros = [], []
    maximos, timeouts = {}, {}
    for leg in legs:
        por_rodada = []
        for rd in round_dirs:
            if leg not in os.listdir(rd):
                erros.append(f"{leg} ausente na rodada {rd}")
                break
            resultados, tos = read_leg(rd, leg)
            por_rodada.append(resultados)
            timeouts.update(tos)
        else:
            e, n = p1_errors(leg, por_rodada)
            erros.extend(e)
            relatorio.append(f"P1 {leg}: {n} aprovado(s) na rodada serial; conjunto {'IDENTICO' if not e else 'DIFERENTE'} nas {ROUNDS} rodadas")
            for r in por_rodada[1:]:
                for nome, (_s, d) in r.items():
                    maximos[nome] = max(maximos.get(nome, 0.0), d)
    fora = p2_report(maximos, timeouts)
    relatorio.append(f"P2: {len(maximos)} teste(s) medidos, {len(fora)} sem folga (max >= TIMEOUT/3)")
    for nome, maximo, atual, novo in fora:
        relatorio.append(
            f"P2 {nome}: max {maximo:.2f} s, TIMEOUT {atual:g} s -> "
            + (f"TIMEOUT novo {novo} s (regra 4: 3 x max, multiplo de 30 s)" if novo else "TIMEOUT atual ja cobre 3 x max (nao se baixa)")
        )
    if jobs_files:
        dados = []
        for caminho in jobs_files:
            with open(caminho, "r", encoding="utf-8") as handle:
                dados.append(json.load(handle))
        linhas, e3 = p3_report(dados)
        relatorio.append(f"P3: {len(linhas)} job(s) com passo de calibracao (Testes compartilhado/estatico) medido")
        relatorio.extend(f"P3 {j}: serial {s:.0f} s, pior paralela {p:.0f} s, ganho {g * 100:.0f}%" for j, s, p, g in linhas)
        erros.extend(e3)
    return (1 if erros else 0), relatorio, erros


# --- selftest: rodadas REAIS de ctest (tests/tools/fixtures/p1_rounds) ---


def _rounds(variante):
    base = os.path.join(FIXTURES, "p1_rounds", variante)
    return [os.path.join(base, f"r{i}") for i in range(ROUNDS)]


def _check(nome, condicao, detalhe=""):
    print(f"selftest: {nome} {'OK' if condicao else 'FALHOU'}" + ("" if condicao else f" - {detalhe}"))
    return condicao


def selftest_main():
    if not os.path.isdir(os.path.join(FIXTURES, "p1_rounds", "ok")):
        fail(f"fixtures ausentes em {FIXTURES}/p1_rounds - varredura vazia (L-40)")
    controles = []
    rc, rel, erros = compare(_rounds("ok"))
    texto = "\n".join(rel)
    controles.append(_check("P1: 6 rodadas reais identicas passam", rc == 0 and "IDENTICO" in texto, f"{rc} {erros} {rel}"))
    controles.append(_check("P2: o teste de TIMEOUT 3 (2 s) ganha teto novo pela regra 4 (3 x 2 = 6 s -> 30 s)",
                            "P2 lento_timeout_apertado" in texto and "TIMEOUT novo 30 s" in texto, texto))
    controles.append(_check("P2: os testes com folga (rapido) nao sao listados", "P2 rapido" not in texto, texto))
    rc, rel, erros = compare(_rounds("diverge"))
    controles.append(_check("P1: um teste que falha SO' na rodada r3 reprova, com status e duracao por rodada",
                            rc == 1 and any("flaky" in e and "r3=" in e and "r0=passou" in e for e in erros), str(erros)))
    rc, rel, erros = compare(_rounds("diverge_serial"))
    controles.append(_check("P1: a divergencia so' na rodada SERIAL (r0) tambem reprova (r0 entra no conjunto)",
                            rc == 1 and any("flaky" in e and "r0=falhou" in e for e in erros), str(erros)))
    anterior = AGG.DEFAULT_TIMEOUT_S
    AGG.DEFAULT_TIMEOUT_S = 3.0
    try:
        _rc, rel, _e = compare(_rounds("ok"))
    finally:
        AGG.DEFAULT_TIMEOUT_S = anterior
    controles.append(_check("P2: o TIMEOUT padrao decide (DEFAULT_TIMEOUT_S=3.0 lista 'medio', 1 s, sem TIMEOUT proprio)",
                            "P2 medio" in "\n".join(rel), "\n".join(rel)))
    rc, rel, erros = compare(_rounds("serial_diferente"))
    texto = "\n".join(rel)
    controles.append(_check("P2 usa so' as rodadas PARALELAS (serial_lento: 3 s na serial, 1 s nas paralelas, TIMEOUT 5)",
                            rc == 0 and "P2 serial_lento" not in texto, texto + str(erros)))
    controles.append(_check("estrutura: menos de 6 rodadas reprova", compare(_rounds("ok")[:5])[0] == 1))
    controles.append(_check("varredura vazia: rodada sem pernas reprova", compare([FIXTURES] * ROUNDS)[0] == 1))
    controles.extend(_selftest_p3())
    if not all(controles):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controles)} controles OK")


def _selftest_p3():
    """P3 sobre a resposta REAL de `gh api runs/36523231561/attempts/1/jobs` (fixture do
    rerun_guard); a rodada 'paralela' mais rapida e' uma MUTACAO (tempos do passo Testes
    reduzidos a 40%)."""
    caminho = os.path.join(FIXTURES, "rerun_guard", "run36523231561_attempt1_jobs.json")
    with open(caminho, "r", encoding="utf-8") as handle:
        real = json.load(handle)
    linhas, erros = p3_report([real] * ROUNDS)
    saida = [_check("P3: rodada paralela igual a serial (ganho 0%) reprova citando o ganho medido",
                    any("ganho medido 0%" in e for e in erros), str(erros))]
    rapido = json.loads(json.dumps(real))
    for job in rapido["jobs"]:
        for passo in job["steps"]:
            if passo["name"].startswith("Testes ("):
                ini = datetime.fromisoformat(passo["started_at"].replace("Z", "+00:00"))
                fim = datetime.fromisoformat(passo["completed_at"].replace("Z", "+00:00"))
                passo["completed_at"] = (ini + (fim - ini) * 0.4).isoformat().replace("+00:00", "Z")
    linhas, erros = p3_report([real] + [rapido] * (ROUNDS - 1))
    saida.append(_check("P3: paralela a 40% da serial (ganho 60%) passa", not erros and linhas, str(erros)))
    # I-1 (CTO 29/09): so' "Testes (compartilhado)" e "Testes (estatico)" usam o grau de
    # calibracao; "Testes (ASan, rotulo unit)" e "Testes (Debug, suite inteira)" (Windows
    # Sanitizer e Debug) nao entram - com os dados REAIS da API reprovariam com 0% de ganho.
    linhas0, erros0 = p3_report([real] * ROUNDS)
    citados = " ".join(erros0) + " " + str(linhas0)
    saida.append(_check("P3 I-1: Windows Sanitizer e Debug (passos de teste proprios) NAO entram no universo",
                        "Sanitizer" not in citados and "Debug" not in citados, citados[:300]))
    saida.append(_check("P3 I-1: os jobs de calibracao (Testes compartilhado/estatico) entram",
                        bool(linhas0) and all(("compartilhado" in j or "estatico" in j) for j, *_ in linhas0), str(linhas0)))
    lento_so_nos_outros = json.loads(json.dumps(rapido))
    for job in lento_so_nos_outros["jobs"]:
        for passo in job["steps"]:
            if passo["name"] in ("Testes (ASan, rotulo unit)", "Testes (Debug, suite inteira)"):
                ini = datetime.fromisoformat(passo["started_at"].replace("Z", "+00:00"))
                passo["completed_at"] = (ini + __import__("datetime").timedelta(hours=1)).isoformat().replace("+00:00", "Z")
    _l, erros_x = p3_report([real] + [lento_so_nos_outros] * (ROUNDS - 1))
    saida.append(_check("P3 I-1: passos Sanitizer/Debug LENTOS nas paralelas nao reprovam o P3 (fora do universo)", not erros_x, str(erros_x)))
    return saida


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
        return
    if not args or args[0] != "--rounds":
        fail("usage: ctest_p1_compare.py --rounds <r0> ... <r5> [--jobs <j0> ... <j5>]  |  --selftest")
    resto = args[1:]
    jobs = None
    if "--jobs" in resto:
        i = resto.index("--jobs")
        resto, jobs = resto[:i], resto[i + 1:]
    rc, relatorio, erros = compare(resto, jobs)
    print("\n".join(relatorio))
    for erro in erros:
        print(f"{SCRIPT_NAME}: {erro}", file=sys.stderr)
    sys.exit(rc)


if __name__ == "__main__":
    main()
