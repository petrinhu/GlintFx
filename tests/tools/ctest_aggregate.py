#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# ctest_aggregate.py - CI-SPLIT-PER-OS A2 (achados 4 e A1 da revisao do
# CTO, 29/09/2026): o resultado agregado de um job de ctest, MEDIDO da
# mesma forma nos dois sistemas (L-04), por STATUS.
#
# HISTORICO DOS DOIS DEFEITOS (nao repetir):
#   1. `executados=$declarados` no ci.yml: a checagem `executados !=
#      declarados` era uma tautologia e um teste pulado passava calado.
#   2. A primeira medicao real contava as linhas "K/N Testing:" do
#      LastTest.log - mas o ctest REAL grava "Testing:" e "Test Passed."
#      tambem para teste Skipped (SKIP_RETURN_CODE) e Disabled, entao
#      pulado e desligado eram contados como "passou" (8 a 9 Skipped por
#      perna Linux e 2 por perna Windows no run 36202987723). O fixture
#      que o autoteste usava foi ESCRITO A MAO no formato que a
#      implementacao supunha - por isso o defeito nunca apareceu no
#      autoteste. Este autoteste usa arquivos GERADOS por ctest real (ver
#      tests/tools/fixtures/ctest_probe/ e a receita mais abaixo).
#
# FONTE DA MEDICAO: `ctest --output-junit <builddir>/ctest-results-
# junit.xml` (passo "Testes" do ci.yml, igual nos dois sistemas). Cada
# <testcase> vira exatamente um status:
#   falhou    - tem <failure> ou <error>;
#   pulou     - tem <skipped> (SKIP_RETURN_CODE);
#   desligado - status="disabled" (DISABLED TRUE);
#   passou    - qualquer outro.
# "declarados" e' o rodape "Total Tests: N" do `ctest -N` (L-45).
#
# "executados" = passou + falhou de verdade; "nao_rodou" = notrun contado como falha.
#
# Reprova se: declarados == 0 (varredura vazia, L-40); JUnit ausente ou
# ilegivel; casos no JUnit != declarados (caso nao reportado = pulado
# calado); qualquer teste DESLIGADO; qualquer falha. Teste PULADO nao
# reprova (e' legitimo, ex.: sem GPU), mas e' impresso a parte e NUNCA
# entra em "passaram". Sempre imprime as seis contagens.
#
# Receita dos fixtures (cmake 4.3.0, 29/09/2026): projeto `project(probe
# NONE) enable_testing()` com add_test(NAME passa COMMAND sh -c "exit 0"),
# add_test(NAME pula77 COMMAND sh -c "exit 77") + SKIP_RETURN_CODE 77,
# add_test(NAME desligado ...) + DISABLED TRUE e add_test(NAME falha
# COMMAND sh -c "exit 1"); `ctest --output-junit x.xml` e `ctest -N`. Os
# quatro fixtures usam subconjuntos desses casos (mixed = os quatro).
#
# Usage:
#   ctest_aggregate.py --builddir <dir> --inventory <parity_inventory.txt>
#   ctest_aggregate.py --selftest

import os
import json
import re
import sys
import xml.etree.ElementTree as ElementTree

SCRIPT_NAME = "ctest_aggregate.py"
JUNIT_NAME = "ctest-results-junit.xml"
TOP_DURATIONS = 25
HEAVY_LIMIT_S = 40.0  # regua ABSOLUTA de pesados (regra 1): duracao do JUnit, sem olhar o TIMEOUT
DEFAULT_TIMEOUT_S = 120.0  # DART_TESTING_TIMEOUT: vale quando o teste nao tem TIMEOUT proprio
# Interruptor da REGRA 2 (folga do P2: duracao > TIMEOUT/3). Etapa 2 da A5: so' IMPRIME; a etapa 3
# passa a True (citado no plano 4.8). Constante no codigo, nao flag de CLI (decisao do CTO, 29/09).
FOLGA_REPROVA = False
NESTED_LOCK = "glintfx_nested_build"
FIXTURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures", "ctest_probe")

_TOTAL_RE = re.compile(r"Total Tests: (\d+)")


def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read()
    except OSError:
        return None


def parse_declared(inventory_path):
    inventory = read_text(inventory_path)
    if inventory is None or not _TOTAL_RE.search(inventory):
        return None, (
            f"rodape 'Total Tests: N' nao encontrado em {inventory_path} - ctest -N nao terminou "
            f"de escrever, ou o formato mudou (GODS_LAWS.md L-40)"
        )
    return int(_TOTAL_RE.search(inventory).group(1)), None


# Unico <skipped> que e' PULO de verdade (SKIP_RETURN_CODE=<n> ou
# SKIP_REGULAR_EXPRESSION). Qualquer outro <skipped> (status="notrun") -
# "Unable to find executable", "Required Files Missing", "Fixture
# dependency failed" - o ctest conta como FAILED (A4, CTO 29/09).
_TRUE_SKIP_RE = re.compile(r"^(?:SKIP_RETURN_CODE=\d+|SKIP_REGULAR_EXPRESSION_MATCHED)$")


def classify_case(case):
    """(status, motivo) - motivo so' quando um notrun conta como falha."""
    if case.find("failure") is not None or case.find("error") is not None:
        return "falhou", None
    skipped = case.find("skipped")
    if skipped is not None:
        mensagem = skipped.get("message", "")
        if _TRUE_SKIP_RE.match(mensagem):
            return "pulou", None
        return "falhou", f"{case.get('name')}: notrun '{mensagem or '<sem mensagem>'}' (o ctest conta como FAILED)"
    if case.get("status") == "disabled":
        return "desligado", None
    return "passou", None


def parse_junit(junit_path):
    """({status: n}, motivos, erro, duracoes) - erro e' None quando o JUnit foi lido;
    motivos = por que cada notrun contou como falha; duracoes = [(nome, segundos)]
    (A5: as medidas reais que calibram TIMEOUT e PROCESSORS sob --parallel)."""
    text = read_text(junit_path)
    if text is None:
        return None, [], f"{junit_path} ausente - o passo 'Testes' nao rodou ate o fim (ou nao usou --output-junit)", []
    try:
        root = ElementTree.fromstring(text)
    except ElementTree.ParseError as exc:
        return None, [], f"{junit_path} ilegivel ({exc})", []
    counts = {"passou": 0, "falhou": 0, "pulou": 0, "desligado": 0}
    motivos = []
    duracoes = []
    for case in root.iter("testcase"):
        status, motivo = classify_case(case)
        counts[status] += 1
        if motivo:
            motivos.append(motivo)
        try:
            duracoes.append((case.get("name", "?"), float(case.get("time", "0"))))
        except ValueError:
            pass
    return counts, motivos, None, duracoes


def verdict(declared, counts, errors, motivos=()):
    """Lista de causas de reprovacao (vazia = aprovado)."""
    problems = list(errors)
    if declared == 0:
        problems.append("piso de varredura vazia (GODS_LAWS.md L-40): 'ctest -N' nao registrou teste nenhum")
    if declared is not None and counts is not None:
        total = sum(counts.values())
        if total != declared:
            problems.append(
                f"casos no JUnit ({total}) != declarados ({declared}) - teste declarado que nao "
                f"foi reportado (pulado calado)"
            )
    if counts is not None and counts["desligado"] > 0:
        problems.append(f"{counts['desligado']} teste(s) DESLIGADO(S) (DISABLED) - teste desligado nao conta como passou")
    if counts is not None and counts["falhou"] > 0:
        problems.append(f"{counts['falhou']} teste(s) falharam")
    problems.extend(f"notrun contado como falha - {m}" for m in motivos)
    return problems


def _props_of(testes, nome):
    return {p["name"]: p["value"] for p in testes.get(nome, {}).get("properties", [])}


def _load_tests_json(tests_json_path):
    """(testes_por_nome, erro)."""
    texto = read_text(tests_json_path)
    if texto is None:
        return None, f"{tests_json_path} ausente - sem `ctest --show-only=json-v1` nao se cruzam as duracoes com os trincos"
    try:
        return {t["name"]: t for t in json.loads(texto)["tests"]}, None
    except (ValueError, KeyError, TypeError) as exc:
        return None, f"{tests_json_path} ilegivel como json-v1 do ctest ({exc})"


def heavy_errors(testes, duracoes, limite):
    """REGRA 1 (trinco, CTO 29/09): todo teste acima da regua ABSOLUTA de `limite`
    segundos (duracao medida no JUnit, SEM olhar o TIMEOUT) tem RESOURCE_LOCK
    glintfx_nested_build ou PROCESSORS > 1 no json-v1. A lista de pesados e' lida
    do proprio ctest, nunca escrita a mao."""
    errors = []
    for nome, segundos in duracoes:
        if segundos <= limite:
            continue
        props = _props_of(testes, nome)
        travado = NESTED_LOCK in (props.get("RESOURCE_LOCK") or [])
        try:
            paralelo = int(props.get("PROCESSORS", 1)) > 1
        except (TypeError, ValueError):
            paralelo = False
        if not (travado or paralelo):
            errors.append(
                f"teste {nome} levou {segundos:.1f} s (> {limite:g} s, regua de pesados) sem "
                f"RESOURCE_LOCK {NESTED_LOCK} nem PROCESSORS - disputa os nucleos"
            )
    return errors


def slack_offenders(testes, duracoes):
    """REGRA 2 (folga do P2): [(nome, duracao, timeout)] com duracao > TIMEOUT/3, o TIMEOUT
    lido do proprio teste no json-v1 (120 s quando nao tem)."""
    fora = []
    for nome, segundos in duracoes:
        props = _props_of(testes, nome)
        try:
            timeout = float(props["TIMEOUT"]) if "TIMEOUT" in props else DEFAULT_TIMEOUT_S
        except (TypeError, ValueError):
            timeout = DEFAULT_TIMEOUT_S
        if segundos > timeout / 3:
            fora.append((nome, segundos, timeout))
    return fora


def run(builddir, inventory_path, junit_path=None, tests_json=None, heavy_limit=None, paralelismo=None):
    junit_path = junit_path or os.path.join(builddir, JUNIT_NAME)
    declared, declared_error = parse_declared(inventory_path)
    counts, motivos, junit_error, duracoes = parse_junit(junit_path)
    errors = [e for e in (declared_error, junit_error) if e]
    shown = "?" if declared is None else declared
    c = counts or {"passou": 0, "falhou": 0, "pulou": 0, "desligado": 0}
    # "executados" = os que o ctest de fato RODOU (passou + falhou de verdade);
    # notrun contado como falha (sem executavel, arquivo faltando, dependencia
    # de fixture) nunca executou e sai a parte em "nao_rodou" (C3, CTO 29/09:
    # rotulo que nao correspondia ao conteudo).
    nao_rodou = len(motivos)
    print(
        f"declarados: {shown}, executados: {c['passou'] + c['falhou'] - nao_rodou}, passaram: {c['passou']}, "
        f"falharam: {c['falhou']}, pulados: {c['pulou']}, desligados: {c['desligado']}, nao_rodou: {nao_rodou}"
    )
    if duracoes:
        maiores = sorted(duracoes, key=lambda d: -d[1])[:TOP_DURATIONS]
        print("duracoes (maiores primeiro, s): " + " ".join(f"{n}={t:.2f}" for n, t in maiores))
        print(f"soma dos tempos: {sum(t for _n, t in duracoes):.1f} s")
    problems = verdict(declared, counts, errors, motivos)
    if tests_json is not None:
        testes, erro_json = _load_tests_json(tests_json)
        if erro_json:
            problems.append(erro_json)
        else:
            grau = os.environ.get("GLINTFX_PARALELISMO", "") if paralelismo is None else paralelismo
            n_proc = sum(1 for t in testes.values() if "PROCESSORS" in _props_of(testes, t["name"]))
            n_lock = sum(1 for t in testes.values() if "RESOURCE_LOCK" in _props_of(testes, t["name"]))
            # Linha de escopo do plano 4.8, SEMPRE impressa; o grau vem do passo "Testes"
            # (GLINTFX_PARALELISMO, gravado em $GITHUB_ENV) e ausente reprova.
            print(f"testes: {len(testes)}, com PROCESSORS: {n_proc}, com RESOURCE_LOCK: {n_lock}, paralelismo: {grau or '?'}")
            if not grau:
                problems.append("GLINTFX_PARALELISMO ausente - o passo 'Testes' nao gravou o grau (linha de escopo do plano 4.8)")
            problems.extend(heavy_errors(testes, duracoes, HEAVY_LIMIT_S if heavy_limit is None else heavy_limit))
            folga = slack_offenders(testes, duracoes)
            # SEMPRE impressa, inclusive com zero (L-40).
            print(
                f"folga P2: {len(folga)} teste(s) acima de TIMEOUT/3"
                + (": " + " ".join(f"{n}={t:.1f}/{to:g}" for n, t, to in sorted(folga, key=lambda f: -f[1])) if folga else "")
            )
            if FOLGA_REPROVA:
                problems.extend(f"teste {n} levou {t:.1f} s (> TIMEOUT/3 = {to / 3:g} s) - folga do P2" for n, t, to in folga)
    for problem in problems:
        print(f"{SCRIPT_NAME}: {problem}", file=sys.stderr)
    return 1 if problems else 0


# --- selftest: arquivos GERADOS por ctest real ------------------------


def _fixture(name):
    return os.path.join(FIXTURES, name)


def _capture(runner):
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
        rc = runner()
    return rc, buffer.getvalue()


def _case(name, expect_rc, junit, inventory, expect_text=(), junit_text=None, tests_json=None, heavy_limit=None, proibido=(), paralelismo="1"):
    """Roda `run` contra fixtures reais. `junit_text` substitui o conteudo do
    JUnit (mutante feito sobre o arquivo REAL)."""
    import tempfile

    with tempfile.TemporaryDirectory() as tmp:
        junit_path = os.path.join(tmp, JUNIT_NAME)
        if junit is not None or junit_text is not None:
            with open(junit_path, "w", encoding="utf-8") as handle:
                handle.write(junit_text if junit_text is not None else read_text(_fixture(junit)))
        json_path = _fixture(tests_json) if tests_json else None
        rc, output = _capture(lambda: run(tmp, inventory if os.path.isabs(inventory) else _fixture(inventory), None, json_path, heavy_limit, paralelismo))
    faltando = [t for t in expect_text if t not in output]
    citados = [t for t in proibido if t in output.split("duracoes")[0] or f"teste {t}" in output]
    if rc != expect_rc or faltando or citados:
        print(f"selftest: {name} FALHOU (rc={rc}, esperado {expect_rc}; faltou {faltando}; citou {citados}): {output!r}", file=sys.stderr)
        return False
    print(f"selftest: {name} OK")
    return True


def _case_folga_reprova():
    """O interruptor FOLGA_REPROVA=True (etapa 3) transforma a folga em reprovacao."""
    global FOLGA_REPROVA
    anterior = FOLGA_REPROVA
    FOLGA_REPROVA = True
    try:
        return _case("REGRA2 com FOLGA_REPROVA=True (etapa 3) reprova", 1, "junit_slow.xml", "inventory_slow.txt",
                     ["folga P2:", "TIMEOUT/3"], tests_json="show_slow.json", heavy_limit=40.0)
    finally:
        FOLGA_REPROVA = anterior


def _case_default_timeout():
    """Caminho "sem TIMEOUT proprio" (cosmetico A5 do CTO): DEFAULT_TIMEOUT_S=3.0 dentro do
    selftest faz lento_com_trinco (2 s, sem TIMEOUT no json-v1) entrar na linha de folga; com o
    120 real ele nao entra. Mutante "o padrao de 120 e' ignorado" sobrevivia sem este caso."""
    global DEFAULT_TIMEOUT_S
    anterior = DEFAULT_TIMEOUT_S
    DEFAULT_TIMEOUT_S = 3.0
    try:
        return _case("REGRA2: o TIMEOUT padrao (sem TIMEOUT proprio) e' o que decide - DEFAULT_TIMEOUT_S=3.0 inclui lento_com_trinco",
                     1, "junit_slow.xml", "inventory_slow.txt", ["lento_com_trinco=2.0/3"],
                     tests_json="show_slow.json", heavy_limit=1.0)
    finally:
        DEFAULT_TIMEOUT_S = anterior


def selftest_main():
    real = read_text(_fixture("junit_allpass.xml"))
    if real is None:
        print(f"{SCRIPT_NAME} --selftest: fixtures ausentes em {FIXTURES} - varredura vazia (L-40)", file=sys.stderr)
        sys.exit(1)
    um_a_menos = re.sub(r'\t<testcase name="passa2".*?</testcase>\n', "", real, flags=re.DOTALL)
    sem_rodape = os.path.join(FIXTURES, "junit_allpass.xml")  # arquivo sem "Total Tests:"
    controls = [
        _case("POSITIVO (3 passaram, JUnit real)", 0, "junit_allpass.xml", "inventory_allpass.txt",
              ["declarados: 3, executados: 3, passaram: 3, falharam: 0, pulados: 0, desligados: 0, nao_rodou: 0"]),
        # A1: pulado NAO e' passou (1 passou + 1 pulado, ctest real).
        _case("PULADO-NAO-E-PASSOU (ctest real)", 0, "junit_skipped_only.xml", "inventory_skipped_only.txt",
              ["passaram: 1", "pulados: 1"]),
        _case("DESLIGADO-REPROVA (ctest real)", 1, "junit_skipped_disabled.xml", "inventory_skipped_disabled.txt",
              ["passaram: 1", "pulados: 1", "desligados: 1", "DESLIGADO"]),
        _case("FALHA-E-DESLIGADO (ctest real, os quatro status)", 1, "junit_mixed.xml", "inventory_mixed.txt",
              ["passaram: 1", "falharam: 1", "pulados: 1", "desligados: 1"]),
        # Mutante sobre o arquivo REAL: um caso some do JUnit (teste pulado calado).
        _case("CASO-NAO-REPORTADO (JUnit real sem passa2)", 1, None, "inventory_allpass.txt",
              ["casos no JUnit (2) != declarados (3)"], junit_text=um_a_menos),
        # A4 (CTO 29/09): o ctest real grava <skipped> status="notrun" tambem
        # para "Unable to find executable", "Required Files Missing" e
        # "Fixture dependency failed", que ELE conta como FAILED. Fixture
        # gerado por ctest real (9 casos: 1 passou, 2 pulados de verdade,
        # 1 desligado, 5 falhas - 3 delas notrun).
        _case("NOTRUN-NAO-E-PULADO (ctest real)", 1, "junit_notrun.xml", "inventory_notrun.txt",
              ["executados: 3", "passaram: 1", "falharam: 5", "pulados: 2", "desligados: 1", "nao_rodou: 3",
               "Unable to find executable", "Required Files Missing", "Fixture dependency failed",
               "duracoes (maiores primeiro, s): estoura=1.01", "soma dos tempos:"]),
        # Regra 1 (trinco, CTO 29/09): REGUA ABSOLUTA sobre a duracao do JUnit, sem olhar o
        # TIMEOUT. Fixtures REAIS (limite 1 s: os testes lentos dormem 2 s).
        _case("REGRA1: pesado sem trinco reprova, TIMEOUT alto NAO isenta", 1, "junit_slow.xml", "inventory_slow.txt",
              ["teste lento_sem_trinco levou", "teste lento_lock_de_outro_nome levou", "teste lento_timeout_curto levou",
               "teste lento_timeout_longo levou", "regua de pesados", "disputa os nucleos"],
              tests_json="show_slow.json", heavy_limit=1.0,
              proibido=["lento_com_trinco", "lento_com_processors", "rapido", "lento_lock_composto", "lento_trinco_timeout_curto"]),
        # Regra 2 (folga do P2): IMPRIME sempre; nao reprova enquanto FOLGA_REPROVA e' False.
        _case("REGRA2: a linha de folga lista quem passa de TIMEOUT/3", 1, "junit_slow.xml", "inventory_slow.txt",
              ["folga P2:", "lento_timeout_curto=2.0/3", "lento_trinco_timeout_curto=2.0/3"],
              tests_json="show_slow.json", heavy_limit=1.0),
        _case("REGRA2 sozinha nao reprova (limite alto, so' a folga): rc=0 com FOLGA_REPROVA=False", 0, "junit_slow.xml", "inventory_slow.txt",
              ["folga P2: 2 teste(s) acima de TIMEOUT/3"], tests_json="show_slow.json", heavy_limit=40.0),
        # Linha de escopo do plano 4.8 (sempre impressa; contagens lidas do json-v1 real).
        _case("ESCOPO: 'testes: N, com PROCESSORS: a, com RESOURCE_LOCK: b, paralelismo: j'", 1, "junit_slow.xml", "inventory_slow.txt",
              ["testes: 9, com PROCESSORS: 1, com RESOURCE_LOCK: 4, paralelismo: 4"], tests_json="show_slow.json", heavy_limit=1.0, paralelismo="4"),
        _case("ESCOPO: grau ausente reprova (nunca 'desconhecido' calado)", 1, "junit_allpass.xml", "inventory_allpass.txt",
              ["GLINTFX_PARALELISMO ausente"], tests_json="show_slow.json", heavy_limit=40.0, paralelismo=""),
        _case("REGRA2: a linha de folga sai tambem com ZERO", 0, "junit_allpass.xml", "inventory_allpass.txt",
              ["folga P2: 0 teste(s) acima de TIMEOUT/3"], tests_json="show_slow.json", heavy_limit=40.0),
        _case_folga_reprova(),
        _case_default_timeout(),
        _case("REGRA1: json ausente reprova (nao se cruza sem os dados)", 1, "junit_slow.xml", "inventory_slow.txt",
              ["ausente"], tests_json="nao_existe.json", heavy_limit=1.0),
        _case("JUNIT-AUSENTE", 1, None, "inventory_allpass.txt", ["ausente"]),
        _case("JUNIT-ILEGIVEL", 1, None, "inventory_allpass.txt", ["ilegivel"], junit_text="<testsuite"),
        _case("RODAPE-AUSENTE", 1, "junit_allpass.xml", sem_rodape, ["Total Tests"]),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def _parse_real_args(args):
    """(builddir, inventory, tests_json|None)."""
    if len(args) in (4, 6) and args[0] == "--builddir" and args[2] == "--inventory":
        tests_json = None
        if len(args) == 6:
            if args[4] != "--tests-json":
                print(f"usage: {SCRIPT_NAME} --builddir <dir> --inventory <arquivo> [--tests-json <json-v1>]  |  --selftest", file=sys.stderr)
                sys.exit(2)
            tests_json = args[5]
        return args[1], args[3], tests_json
    print(f"usage: {SCRIPT_NAME} --builddir <dir> --inventory <arquivo> [--tests-json <json-v1>]  |  --selftest", file=sys.stderr)
    sys.exit(2)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
        return
    builddir, inventory, tests_json = _parse_real_args(args)
    sys.exit(run(builddir, inventory, None, tests_json))


if __name__ == "__main__":
    main()
