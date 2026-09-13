#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_container_kill_order.py - a regra "fatal_error_smoke roda por
# ULTIMO entre as fixtures do container" vivia SO' como comentario em
# .github/workflows/ci.yml (o proprio cabecalho do passo "WL-DISPLAY
# fatia C" ja dizia isso, verbatim, ha' semanas). Comentario nao e'
# portao: em 13/09/2026 CONTAINER-LEAK-COUNTER S4 (alloc_cycle_growth_
# smoke, commit e69fe40) foi acrescentada DEPOIS do passo que mata o
# compositor, precisando dele vivo pra reabrir display/shell/janela/
# contexto GL - o job real reprovou nas duas pernas (run 34771360899,
# jobs 103761435188/103761435190/103762867408), custando um ciclo
# inteiro de diagnostico antes do conserto (commit 8e88ac5, que trocou
# a ordem dos dois passos e virou a fonte deste item). Este script E'
# o mecanismo que falta pra ninguem repetir o erro calado.
#
# O QUE ESTE PORTAO PROVA: dentro do job `wayland-container` de ci.yml,
# nenhuma fixture roda DEPOIS de fatal_error_smoke.cpp (WL-DISPLAY
# fatia C) - a unica fixture que mata de proposito o kwin_wayland do
# proprio container (`std::system("pkill -x kwin_wayland")`, tests/
# container/fatal_error_smoke.cpp), sem reinicio (tests/container/
# run_compositor.sh's own header comment: o container sobrevive, o
# compositor nao). Qualquer fixture que precise do compositor vivo -
# e nenhuma fixture deste diretorio faz outra coisa - tem que rodar
# ANTES dela.
#
# COMO A ORDEM E' MEDIDA, ESTRUTURAL, NUNCA POR SUBSTRING SOLTA: a
# mesma fonte de verdade que tests/tools/check_container_fixture_
# inventory.py ja usa para "este passo executou UMA fixture real do
# container" - a linha `echo "<nome>" >> parity_inventory.txt` que
# cada passo real de fixture tem, sempre logo apos a chamada de
# tests/container/exec_fixture.sh (P-0). Casar por essa linha, e nao
# pela chamada de exec_fixture.sh em si, evita dois riscos: (1) a
# chamada de exec_fixture.sh carrega expressao condicional do GitHub
# Actions (${{ matrix.perna == 'asan' && ... }}) que dificultaria um
# regex confiavel; (2) o UNICO controle negativo deste job que tambem
# chama exec_fixture.sh (cn5, GODS_LAWS.md L-45/CONTAINER-LOG-SIGNAL-
# FIRST) nunca termina com um nome de fixture solto - termina com
# redirecionamento de shell - e por isso nunca gera uma linha de
# append, o que already o exclui deste inventario por CONSTRUCAO, sem
# precisar de lista de excecao nenhuma. A ORDEM das linhas de append
# no arquivo E' a ordem em que o job de fato executa as fixtures -
# GitHub Actions roda os passos de um `steps:` na ordem em que
# aparecem no YAML, sem excecao.
#
# PISO DE VARREDURA (GODS_LAWS.md L-40): zero linhas de append
# encontradas no job e' coleta quebrada - o proprio verificador parou
# de enxergar o arquivo (job renomeado, indentacao mudou, o mecanismo
# de append foi substituido por outro) - nunca "nenhuma fixture no
# job", que nao e' um estado real deste projeto.
#
# ONDE ISTO RODA: e' verificacao de TEXTO de .github/workflows/ci.yml,
# nenhum Docker envolvido - mesma fronteira que container_fixture_
# inventory_test ja usa, e pelo mesmo motivo roda identico nas CINCO
# plataformas, sem entrada em tests/parity_exceptions.txt nem tests/
# parity_aliases.txt (nenhuma delas e' Windows-specific nem Unix-
# specific).
#
# Usage:
#   check_container_kill_order.py --compare <ci.yml>
#   check_container_kill_order.py --selftest
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

import re
import sys

SCRIPT_NAME = "check_container_kill_order.py"

WAYLAND_CONTAINER_JOB = "wayland-container"

# A UNICA fixture deste diretorio que mata o compositor do proprio
# container de proposito (tests/container/fatal_error_smoke.cpp,
# std::system("pkill -x kwin_wayland")) - constante nomeada, igual a
# WAYLAND_CONTAINER_JOB acima, em vez de tentar "descobrir" a intencao
# por leitura de comentario (fragil, nao estrutural).
KILLER_FIXTURE = "fatal_error_smoke"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- parsing -------------------------------------------------------------


# Job top-level dentro de `jobs:` neste arquivo e' sempre indentado por
# exatamente 2 espacos, com o nome do job e ":" sozinhos na linha
# (mesma verificacao que tests/tools/check_container_fixture_inventory.
# py's own extract_job_block ja prova contra o arquivo real - copiada
# aqui, nao importada, porque estes scripts sao atomos independentes
# por desenho desta casa, ver o restante de tests/tools/).
_JOB_HEADER_RE = re.compile(r"^  [A-Za-z][A-Za-z0-9_-]*:\s*$")


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


# Mesma regex que check_container_fixture_inventory.py's own _APPEND_RE
# - casa a linha inteira "echo "<nome>" >> parity_inventory.txt" por
# estrutura (a linha tem essa forma exata), nunca por "a palavra
# aparece em algum lugar da linha" (GODS_LAWS.md L-17: criterio largo
# ja editou o item errado neste projeto).
_APPEND_RE = re.compile(r'echo\s+"([A-Za-z0-9_]+)"\s*>>\s*parity_inventory\.txt')


def parse_fixture_order(job_block_text):
    """Devolve a lista de nomes de fixture, NA ORDEM em que os passos
    do job aparecem no arquivo - que e' a ordem em que o GitHub Actions
    de fato os executa."""
    order = []
    for line in job_block_text.splitlines():
        m = _APPEND_RE.search(line)
        if m:
            order.append(m.group(1))
    return order


# --- comparison logic ------------------------------------------------------


# Veredicto inteiro como lista de erros - vazia significa que o portao
# passa. Piso de varredura (GODS_LAWS.md L-40) primeiro: zero fixtures
# e' coleta quebrada, nunca "job sem fixture nenhuma".
def run_check(fixture_order):
    errors = []

    if not fixture_order:
        errors.append(
            f"varredura vazia: nenhuma linha 'echo \"<nome>\" >> parity_inventory.txt' "
            f"encontrada no job {WAYLAND_CONTAINER_JOB!r} de .github/workflows/ci.yml - "
            "GODS_LAWS.md L-40, isto e sinal de coleta quebrada, nunca de ausencia real "
            "de fixtures"
        )
        return errors

    if KILLER_FIXTURE not in fixture_order:
        # Fora de escopo deste portao (que verifica ORDEM, nao
        # presenca): se a fixture assassina sumir do job, nao ha
        # posicao nenhuma pra comparar, e a regra de ordem fica
        # vacuamente satisfeita. Outro portao cobriria a presenca.
        return errors

    killer_index = fixture_order.index(KILLER_FIXTURE)
    after_killer = fixture_order[killer_index + 1 :]
    for name in after_killer:
        errors.append(
            f"{name}: roda DEPOIS de {KILLER_FIXTURE!r} no job {WAYLAND_CONTAINER_JOB!r} - "
            f"{KILLER_FIXTURE!r} mata o compositor deste container de proposito (tests/"
            "container/fatal_error_smoke.cpp) e run_compositor.sh nunca o reinicia; "
            f"qualquer fixture que precise do compositor vivo tem que rodar ANTES de "
            f"{KILLER_FIXTURE!r}, nunca depois (medido quebrando de verdade: run "
            "34771360899, jobs 103761435188/103761435190)"
        )

    return errors


# --- real mode -------------------------------------------------------------


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def real_main(args):
    if len(args) != 1:
        fail("usage: check_container_kill_order.py --compare <ci.yml>")
    (ci_yml_path,) = args

    ci_yml_text = _read_file(ci_yml_path)

    job_block = extract_job_block(ci_yml_text, WAYLAND_CONTAINER_JOB)
    if job_block is None:
        fail(f"job {WAYLAND_CONTAINER_JOB!r} nao encontrado em {ci_yml_path}")

    fixture_order = parse_fixture_order(job_block)

    print(f"{SCRIPT_NAME}: {len(fixture_order)} fixture(s) no job {WAYLAND_CONTAINER_JOB!r}, na ordem: " + ", ".join(fixture_order))

    errors = run_check(fixture_order)
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    if KILLER_FIXTURE in fixture_order:
        print(f"{SCRIPT_NAME}: OK - {KILLER_FIXTURE!r} e' a ultima fixture do job, nenhuma depois dela")
    else:
        print(f"{SCRIPT_NAME}: OK - {KILLER_FIXTURE!r} nao esta no job, regra de ordem vacuamente satisfeita")


# --- fixtures and controls for --selftest -----------------------------


_FAKE_CI_YML_GOOD_ORDER = """\
jobs:
  other-job:
    steps:
      - run: echo "not_this_one" >> parity_inventory.txt

  wayland-container:
    steps:
      - run: |
          docker exec glintfx-wltest-clean a
          echo "a" >> parity_inventory.txt
      - run: |
          docker exec glintfx-wltest-clean b
          echo "b" >> parity_inventory.txt
      - run: |
          docker exec glintfx-wltest-clean fatal_error_smoke
          echo "fatal_error_smoke" >> parity_inventory.txt

  lint:
    steps:
      - run: echo "also_not_this" >> parity_inventory.txt
"""

_FAKE_CI_YML_BAD_ORDER = """\
jobs:
  wayland-container:
    steps:
      - run: |
          docker exec glintfx-wltest-clean a
          echo "a" >> parity_inventory.txt
      - run: |
          docker exec glintfx-wltest-clean fatal_error_smoke
          echo "fatal_error_smoke" >> parity_inventory.txt
      - run: |
          docker exec glintfx-wltest-clean alloc_cycle_growth_smoke
          echo "alloc_cycle_growth_smoke" >> parity_inventory.txt
"""

_FAKE_CI_YML_NO_FIXTURES = """\
jobs:
  wayland-container:
    steps:
      - run: echo hello world
      - run: docker build -t x .
"""


def selftest_bad_order_reproves():
    """VERMELHO#1 - uma fixture depois da que mata o compositor tem
    que reprovar. Este e' exatamente o cenario que quebrou o job real
    em 13/09/2026 antes do commit 8e88ac5."""
    order = parse_fixture_order(extract_job_block(_FAKE_CI_YML_BAD_ORDER, WAYLAND_CONTAINER_JOB))
    errors = run_check(order)
    if not errors:
        print("selftest: VERMELHO#1 FALHOU (fixture depois da que mata o compositor deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("alloc_cycle_growth_smoke" in e and KILLER_FIXTURE in e for e in errors):
        print(f"selftest: VERMELHO#1 FALHOU (reprovou, mas nao citou 'alloc_cycle_growth_smoke' depois de {KILLER_FIXTURE!r}): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#1 OK (fixture fora de ordem pega, nomeada): {errors}")
    return True


def selftest_good_order_passes():
    """A ordem certa (fatal_error_smoke por ultimo) tem que passar,
    zero erros."""
    order = parse_fixture_order(extract_job_block(_FAKE_CI_YML_GOOD_ORDER, WAYLAND_CONTAINER_JOB))
    errors = run_check(order)
    if errors:
        print(f"selftest: ORDEM CERTA FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    print("selftest: ORDEM CERTA OK (fatal_error_smoke por ultimo, zero erro)")
    return True


def selftest_no_fixtures_reproves():
    """VERMELHO#2 - piso de varredura (GODS_LAWS.md L-40): zero passos
    de fixture no job tem que reprovar, nunca passar calado."""
    order = parse_fixture_order(extract_job_block(_FAKE_CI_YML_NO_FIXTURES, WAYLAND_CONTAINER_JOB))
    errors = run_check(order)
    if not errors:
        print("selftest: VERMELHO#2 FALHOU (job sem fixture nenhuma deveria ter reprovado pelo piso)", file=sys.stderr)
        return False
    if not any("varredura vazia" in e and WAYLAND_CONTAINER_JOB in e for e in errors):
        print(f"selftest: VERMELHO#2 FALHOU (sem 'varredura vazia'/{WAYLAND_CONTAINER_JOB}): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#2 OK (piso de varredura pego): {errors}")
    return True


def selftest_killer_absent_is_vacuous():
    """Se a fixture assassina nao estiver no job (fora do escopo deste
    portao), a regra de ordem e' vacuamente satisfeita - nao e' erro
    de piso (ha' fixtures), so nao ha' o que comparar."""
    order = ["a", "b", "c"]
    errors = run_check(order)
    if errors:
        print(f"selftest: AUSENCIA DA ASSASSINA FALHOU (deveria ser vacuamente satisfeito, veio {errors})", file=sys.stderr)
        return False
    print("selftest: AUSENCIA DA ASSASSINA OK (sem a fixture assassina no job, ordem vacuamente satisfeita)")
    return True


def selftest_parsing_round_trip():
    """extract_job_block tem que isolar SO' o bloco do job pedido -
    nomes de outros jobs (not_this_one/also_not_this) nunca podem
    vazar pro parsing de fixture_order."""
    block = extract_job_block(_FAKE_CI_YML_GOOD_ORDER, WAYLAND_CONTAINER_JOB)
    if block is None or "not_this_one" in block or "also_not_this" in block:
        print(f"selftest: PARSING FALHOU (extract_job_block vazou de outro job): {block!r}", file=sys.stderr)
        return False
    order = parse_fixture_order(block)
    if order != ["a", "b", "fatal_error_smoke"]:
        print(f"selftest: PARSING FALHOU (ordem lida nao bate com a esperada): {order}", file=sys.stderr)
        return False
    print(f"selftest: PARSING OK (bloco escopado ao job, ordem correta lida): {order}")
    return True


def selftest_main():
    controls = [
        selftest_bad_order_reproves(),
        selftest_good_order_passes(),
        selftest_no_fixtures_reproves(),
        selftest_killer_absent_is_vacuous(),
        selftest_parsing_round_trip(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--compare":
        real_main(args[1:])
    else:
        fail("usage: check_container_kill_order.py --compare <ci.yml>  |  --selftest")


if __name__ == "__main__":
    main()
