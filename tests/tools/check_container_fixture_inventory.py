#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_container_fixture_inventory.py - closes a blind spot that the
# author of P-0 (docs/plano-w6a-janela.md fatia 1, D-W6a-19,
# GODS_LAWS.md L-04/L-36/L-40) named in his own report, verbatim:
# "o mecanismo NAO e' auto-descoberto [...] se alguem esquecer, o
# piso de varredura nao pega (o arquivo continua nao-vazio por causa
# dos outros fixtures), e o nome esquecido fica invisivel ao portao
# exatamente como antes do P-0 [...] e disciplina manual, nao uma
# trava". This script IS that trava.
#
# THE SHAPE OF THE FAILURE THIS SCRIPT CATCHES: a fixture binary gets
# added to the wayland-container job's image (a new `COPY --from=
# <builder-stage> ... /usr/local/bin/<nome>` line in tests/container/
# Containerfile) and gets exercised there (a new `docker exec ...
# <nome>` step in .github/workflows/ci.yml), but the implementer
# forgets to also append `echo "<nome>" >> parity_inventory.txt` to
# that same step - the one line P-0 needs to feed the name into
# check_test_parity.py's Linux-side inventory. The job still goes
# green (the fixture ran and passed), `Verifica piso de varredura do
# inventario do container` still goes green (OTHER fixtures already
# make the file non-empty), and check_test_parity.py never sees the
# missing name because a name that is never appended is never
# compared - it does not show up as a Windows-side gap, it simply
# never entered the conversation. Passa verde, cobertura incompleta:
# exactly the family of risk docs/plano-w6a-janela.md section 5
# catalogs for this same onda.
#
# TWO SOURCES OF GROUND TRUTH, NEITHER HAND-MAINTAINED A SECOND TIME:
#   - tests/container/Containerfile's own `COPY --from=<stage> ...
#     /usr/local/bin/<nome>` lines. A fixture cannot exist inside the
#     running container without one of these (F8, docs/plano-w6a-
#     janela.md: "cada fixture novo e: uma copia no script de
#     preparo, uma linha no Containerfile, um docker exec no job") -
#     this script reads that line, it does not ask anyone to repeat
#     the name in a THIRD place. Deliberately only the copies FROM a
#     build stage (`--from=...`) count: run_compositor.sh and
#     smoke.sh are copied straight from the build context (no
#     `--from=`) because they are infrastructure, not fixtures meant
#     to enter the paridade inventory.
#   - the `echo "<nome>" >> parity_inventory.txt` lines inside the
#     `wayland-container:` job block of .github/workflows/ci.yml -
#     what P-0 actually wires into the inventory the job `parity`
#     later consumes.
# A name on one side and not the other is reported by name, in
# EITHER direction (a stray append with no matching fixture is a typo
# or a fixture removed without cleaning up after itself - same
# family of bug, cheap to catch here too).
#
# REGISTERED AS `container_fixture_inventory_test` (tests/CMakeLists.
# txt), UNGUARDED BY PLATFORM, same shape as test_parity_selftest
# right above it there: this script only ever reads two plain-text
# files present in any checkout (tests/container/Containerfile and
# .github/workflows/ci.yml itself), nothing OS-specific behind it, so
# it runs identically on Linux and Windows. Because of that, the name
# appears identically in both systems' `ctest -N` inventory - no entry
# is needed in tests/parity_exceptions.txt nor tests/parity_aliases.
# txt for it, for the same reason test_parity_selftest needs neither.
# (Earlier in this same fatia this script ran only as a bare CI step,
# because tests/CMakeLists.txt was under active edit by another agent
# - a tree-ownership boundary for that moment, not a design verdict.
# The tree freed up before this fatia closed, so it moved here.) The
# CI step in .github/workflows/ci.yml that invokes `--compare` before
# the wayland-container job's own image build stays as extra
# fail-fast coverage - it is redundant with the ctest case's local
# `--compare` invocation from the SAME two files, not a second
# mechanism.
#
# Usage:
#   check_container_fixture_inventory.py --compare <Containerfile> <ci.yml>
#   check_container_fixture_inventory.py --selftest
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

import re
import sys

SCRIPT_NAME = "check_container_fixture_inventory.py"

WAYLAND_CONTAINER_JOB = "wayland-container"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- parsing -----------------------------------------------------------


# So conta copias vindas de um estagio de build (`--from=...`) - as
# copias diretas do contexto (run_compositor.sh, smoke.sh) sao
# infraestrutura, nao fixture de teste, e nao devem entrar aqui.
_CONTAINERFILE_COPY_RE = re.compile(r"^COPY\s+--from=\S+\s+\S+\s+/usr/local/bin/(\S+)\s*$")


def parse_containerfile_fixtures(text):
    names = set()
    for raw_line in text.splitlines():
        line = raw_line.strip()
        m = _CONTAINERFILE_COPY_RE.match(line)
        if m:
            names.add(m.group(1))
    return names


# Job top-level dentro de `jobs:` neste arquivo e' sempre indentado
# por exatamente 2 espacos, com o nome do job e ":" sozinhos na linha
# (confirmado contra .github/workflows/ci.yml: "  parity:", "  lint:",
# "  wayland-container:" - todo o corpo de cada job vive a 4+ espacos
# de indentacao). Isola so o bloco do job pedido, para nunca misturar
# um "echo ... >> parity_inventory.txt" de outro job (os jobs `linux`/
# `windows` escrevem nesse MESMO nome de arquivo, por outro mecanismo
# inteiro - `ctest -N > parity_inventory.txt`, sem nenhum `echo`, mas
# a fronteira entre blocos e' o que garante isso por construcao, nao
# por sorte de padrao textual).
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


_APPEND_RE = re.compile(r'echo\s+"([A-Za-z0-9_]+)"\s*>>\s*parity_inventory\.txt')


def parse_ci_appends(job_block_text):
    names = set()
    for line in job_block_text.splitlines():
        m = _APPEND_RE.search(line)
        if m:
            names.add(m.group(1))
    return names


# --- comparison logic ----------------------------------------------------


# Veredicto inteiro como lista de erros - vazia significa que o
# portao passa. Piso de varredura (GODS_LAWS.md L-40) primeiro: zero
# de qualquer lado e' coleta quebrada, nunca "nao ha fixture nenhum".
def run_comparison(containerfile_fixtures, ci_appends):
    errors = []

    if not containerfile_fixtures:
        errors.append(
            "varredura vazia: nenhum fixture 'COPY --from=... /usr/local/bin/<nome>' "
            "encontrado em tests/container/Containerfile - GODS_LAWS.md L-40, isto e "
            "sinal de coleta quebrada, nunca de ausencia real de fixtures"
        )
    if not ci_appends:
        errors.append(
            f"varredura vazia: nenhuma linha 'echo \"<nome>\" >> parity_inventory.txt' "
            f"encontrada no job {WAYLAND_CONTAINER_JOB!r} de .github/workflows/ci.yml - "
            "GODS_LAWS.md L-40, isto e sinal de coleta quebrada, nunca de ausencia real "
            "de fixtures"
        )
    if not containerfile_fixtures or not ci_appends:
        # Sem os dois lados a comparacao de nomes nao tem sentido -
        # devolve so o(s) erro(s) de varredura vazia.
        return errors

    missing_append = sorted(containerfile_fixtures - ci_appends)
    orphan_append = sorted(ci_appends - containerfile_fixtures)

    for name in missing_append:
        errors.append(
            f"{name}: existe como fixture no Containerfile (COPY --from=...) mas nao "
            f"tem 'echo \"{name}\" >> parity_inventory.txt' no job "
            f"{WAYLAND_CONTAINER_JOB!r} - o portao de paridade (check_test_parity.py) "
            "nunca vai ver este nome"
        )
    for name in orphan_append:
        errors.append(
            f"{name}: tem 'echo \"{name}\" >> parity_inventory.txt' no job "
            f"{WAYLAND_CONTAINER_JOB!r} mas nao existe fixture COPY --from=... "
            "correspondente no Containerfile - append orfao (nome errado, ou fixture "
            "removida sem limpar o append)"
        )

    return errors


# --- real mode -------------------------------------------------------


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def real_main(args):
    if len(args) != 2:
        fail(
            "usage: check_container_fixture_inventory.py --compare "
            "<Containerfile> <ci.yml>"
        )
    containerfile_path, ci_yml_path = args

    containerfile_text = _read_file(containerfile_path)
    ci_yml_text = _read_file(ci_yml_path)

    containerfile_fixtures = parse_containerfile_fixtures(containerfile_text)

    job_block = extract_job_block(ci_yml_text, WAYLAND_CONTAINER_JOB)
    if job_block is None:
        fail(f"job {WAYLAND_CONTAINER_JOB!r} nao encontrado em {ci_yml_path}")
    ci_appends = parse_ci_appends(job_block)

    print(
        f"{SCRIPT_NAME}: Containerfile={len(containerfile_fixtures)} fixture(s), "
        f"ci.yml({WAYLAND_CONTAINER_JOB})={len(ci_appends)} append(s)"
    )

    errors = run_comparison(containerfile_fixtures, ci_appends)
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: OK - todo fixture do Containerfile tem append correspondente")


# --- fixtures and controls for --selftest -----------------------------


_FAKE_CI_YML = """\
jobs:
  other-job:
    steps:
      - run: echo "not_this_one" >> parity_inventory.txt

  wayland-container:
    steps:
      - run: |
          docker exec glintfx-wltest-clean x
          echo "x" >> parity_inventory.txt

  lint:
    steps:
      - run: echo "also_not_this" >> parity_inventory.txt
"""

_FAKE_CONTAINERFILE = """\
FROM fedora:44 AS arch-ports-builder
RUN dnf -y install gcc-c++

FROM fedora:44
COPY run_compositor.sh /usr/local/bin/run_compositor.sh
COPY --from=arch-ports-builder /build/x /usr/local/bin/x
"""


def selftest_positive_control():
    errors = run_comparison({"x"}, {"x"})
    if errors:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (mesmo nome dos dois lados, zero erro)")
    return True


def selftest_missing_append_reproves():
    errors = run_comparison({"a", "b"}, {"a"})
    if not errors:
        print("selftest: VERMELHO#1 FALHOU (fixture sem append deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("b" in e and "nao tem" in e for e in errors):
        print(f"selftest: VERMELHO#1 FALHOU (reprovou, mas nao citou 'b' faltando): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#1 OK (fixture sem append pego, nomeado): {errors}")
    return True


def selftest_orphan_append_reproves():
    errors = run_comparison({"a"}, {"a", "stray"})
    if not errors:
        print("selftest: VERMELHO#2 FALHOU (append orfao deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("stray" in e and "orfao" in e for e in errors):
        print(f"selftest: VERMELHO#2 FALHOU (reprovou, mas nao citou 'stray' como orfao): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#2 OK (append orfao pego, nomeado): {errors}")
    return True


def selftest_empty_containerfile_reproves():
    errors = run_comparison(set(), {"a"})
    if not errors:
        print("selftest: VERMELHO#3 FALHOU (Containerfile vazio deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("varredura vazia" in e and "Containerfile" in e for e in errors):
        print(f"selftest: VERMELHO#3 FALHOU (sem 'varredura vazia'/Containerfile): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#3 OK (Containerfile vazio pego): {errors}")
    return True


def selftest_empty_ci_appends_reproves():
    errors = run_comparison({"a"}, set())
    if not errors:
        print("selftest: VERMELHO#4 FALHOU (ausencia de append no ci.yml deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("varredura vazia" in e and WAYLAND_CONTAINER_JOB in e for e in errors):
        print(f"selftest: VERMELHO#4 FALHOU (sem 'varredura vazia'/{WAYLAND_CONTAINER_JOB}): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#4 OK (nenhum append no ci.yml pego): {errors}")
    return True


def selftest_parsing_round_trip():
    fixtures = parse_containerfile_fixtures(_FAKE_CONTAINERFILE)
    if fixtures != {"x"}:
        print(f"selftest: PARSING FALHOU (containerfile fixtures): {fixtures}", file=sys.stderr)
        return False

    block = extract_job_block(_FAKE_CI_YML, WAYLAND_CONTAINER_JOB)
    if block is None or "not_this_one" in block or "also_not_this" in block:
        print(f"selftest: PARSING FALHOU (extract_job_block vazou de outro job): {block!r}", file=sys.stderr)
        return False

    appends = parse_ci_appends(block)
    if appends != {"x"}:
        print(f"selftest: PARSING FALHOU (ci appends, escopo do job): {appends}", file=sys.stderr)
        return False

    print("selftest: PARSING OK (fixtures do Containerfile e appends escopados ao job)")
    return True


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_missing_append_reproves(),
        selftest_orphan_append_reproves(),
        selftest_empty_containerfile_reproves(),
        selftest_empty_ci_appends_reproves(),
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
        fail(
            "usage: check_container_fixture_inventory.py --compare "
            "<Containerfile> <ci.yml>  |  --selftest"
        )


if __name__ == "__main__":
    main()
