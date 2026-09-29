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
import re
import sys
import xml.etree.ElementTree as ElementTree

SCRIPT_NAME = "ctest_aggregate.py"
JUNIT_NAME = "ctest-results-junit.xml"
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
    """({status: n}, motivos, erro) - erro e' None quando o JUnit foi lido;
    motivos = por que cada notrun contou como falha."""
    text = read_text(junit_path)
    if text is None:
        return None, [], f"{junit_path} ausente - o passo 'Testes' nao rodou ate o fim (ou nao usou --output-junit)"
    try:
        root = ElementTree.fromstring(text)
    except ElementTree.ParseError as exc:
        return None, [], f"{junit_path} ilegivel ({exc})"
    counts = {"passou": 0, "falhou": 0, "pulou": 0, "desligado": 0}
    motivos = []
    for case in root.iter("testcase"):
        status, motivo = classify_case(case)
        counts[status] += 1
        if motivo:
            motivos.append(motivo)
    return counts, motivos, None


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


def run(builddir, inventory_path, junit_path=None):
    junit_path = junit_path or os.path.join(builddir, JUNIT_NAME)
    declared, declared_error = parse_declared(inventory_path)
    counts, motivos, junit_error = parse_junit(junit_path)
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
    problems = verdict(declared, counts, errors, motivos)
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


def _case(name, expect_rc, junit, inventory, expect_text=(), junit_text=None):
    """Roda `run` contra fixtures reais. `junit_text` substitui o conteudo do
    JUnit (mutante feito sobre o arquivo REAL)."""
    import tempfile

    with tempfile.TemporaryDirectory() as tmp:
        junit_path = os.path.join(tmp, JUNIT_NAME)
        if junit is not None or junit_text is not None:
            with open(junit_path, "w", encoding="utf-8") as handle:
                handle.write(junit_text if junit_text is not None else read_text(_fixture(junit)))
        rc, output = _capture(lambda: run(tmp, inventory if os.path.isabs(inventory) else _fixture(inventory)))
    faltando = [t for t in expect_text if t not in output]
    if rc != expect_rc or faltando:
        print(f"selftest: {name} FALHOU (rc={rc}, esperado {expect_rc}; faltou {faltando}): {output!r}", file=sys.stderr)
        return False
    print(f"selftest: {name} OK")
    return True


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
               "Unable to find executable", "Required Files Missing", "Fixture dependency failed"]),
        _case("JUNIT-AUSENTE", 1, None, "inventory_allpass.txt", ["ausente"]),
        _case("JUNIT-ILEGIVEL", 1, None, "inventory_allpass.txt", ["ilegivel"], junit_text="<testsuite"),
        _case("RODAPE-AUSENTE", 1, "junit_allpass.xml", sem_rodape, ["Total Tests"]),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def _parse_real_args(args):
    if len(args) == 4 and args[0] == "--builddir" and args[2] == "--inventory":
        return args[1], args[3]
    print(f"usage: {SCRIPT_NAME} --builddir <dir> --inventory <arquivo>  |  --selftest", file=sys.stderr)
    sys.exit(2)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
        return
    builddir, inventory = _parse_real_args(args)
    sys.exit(run(builddir, inventory))


if __name__ == "__main__":
    main()
