#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_ci_systems_source.py - CI-SPLIT-PER-OS A3a (D-A12, docs/plano-
# ci-split-per-os.md): a matriz do ci.yml (job `linux`, `slug: fedora`/
# `ubuntu`/`arch`/`cachyos`; job `windows`, `slug: windows`) tem de
# bater EXATAMENTE com tools/ci/systems.txt (fonte unica, carregada por
# ci_systems.py) - slug por slug, familia por familia. E nenhum passo
# `run:` do job `parity` pode citar um slug escrito a mao (a lista de
# argumentos por sistema tem que vir de LER systems.txt em tempo de
# execucao, nao de um texto fixo no ci.yml).
#
# LEITURA DE TEXTO puro do ci.yml (GODS_LAWS.md L-09, nunca executa
# nada) - mesmo padrao de check_ci_step_independence.py/check_
# container_fixture_inventory.py (extract_job_block ja e' a TERCEIRA
# copia da mesma funcao nesses dois; aqui e' a QUARTA - extracao para
# modulo compartilhado registrada como debito conhecido, GODS_LAWS.md
# L-17 regra de 3, fora do escopo desta fatia, L-32).
#
# Usage:
#   check_ci_systems_source.py --check <ci.yml> <systems.txt>
#   check_ci_systems_source.py --selftest

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ci_systems  # noqa: E402

SCRIPT_NAME = "check_ci_systems_source.py"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- extracao do bloco do job -------------------------------------------

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


# --- slugs da matriz ---------------------------------------------------

# M1 do plano: um portao que so comparasse a CONTAGEM de slugs nao
# pegaria "arch" trocado por "manjaro" (mesma contagem, nome errado) -
# devolve o CONJUNTO de nomes, nunca um numero.
_SLUG_FIELD_RE = re.compile(r"^\s*(?:-\s*)?slug:\s*(\S+)\s*$")


def extract_matrix_slugs(job_block_text):
    slugs = set()
    for line in job_block_text.splitlines():
        m = _SLUG_FIELD_RE.match(line)
        if m:
            slugs.add(m.group(1))
    return slugs


def _matrix_slug_errors(ci_yml_text, systems):
    errors = []
    linux_block = extract_job_block(ci_yml_text, "linux")
    windows_block = extract_job_block(ci_yml_text, "windows")
    if linux_block is None:
        fail("job 'linux' nao encontrado no ci.yml")
    if windows_block is None:
        fail("job 'windows' nao encontrado no ci.yml")

    linux_matrix_slugs = extract_matrix_slugs(linux_block)
    windows_matrix_slugs = extract_matrix_slugs(windows_block)
    matrix_slugs = linux_matrix_slugs | windows_matrix_slugs

    systems_slugs = set(systems)

    so_na_matriz = sorted(matrix_slugs - systems_slugs)
    for slug in so_na_matriz:
        errors.append(
            f"slug {slug!r} aparece na matriz do ci.yml mas nao existe em tools/ci/systems.txt"
        )

    so_no_systems = sorted(systems_slugs - matrix_slugs)
    for slug in so_no_systems:
        errors.append(
            f"slug {slug!r} existe em tools/ci/systems.txt mas nao aparece na matriz do ci.yml"
        )

    # Familia certa: um slug presente nos dois lados, mas so' na matriz
    # do sistema ERRADO (ex.: citado em 'windows:' quando systems.txt
    # diz 'linux') - captura o caso "nome bate, familia nao".
    for slug in sorted(matrix_slugs & systems_slugs):
        familia_esperada = ci_systems.slug_family(systems, slug)
        na_matriz_linux = slug in linux_matrix_slugs
        na_matriz_windows = slug in windows_matrix_slugs
        familia_na_matriz = "linux" if na_matriz_linux else "windows" if na_matriz_windows else None
        if familia_esperada != familia_na_matriz:
            errors.append(
                f"slug {slug!r}: tools/ci/systems.txt diz familia {familia_esperada!r}, mas a "
                f"matriz do ci.yml o cita no job {familia_na_matriz!r}"
            )
    return errors


# --- nenhum slug escrito a mao no job `parity` --------------------------

# M3 do plano: uma lista `slug=` escrita a mao volta ao passo do
# `parity` -> reprova. Procura, em CADA passo `run:` do job `parity`,
# a forma literal "<slug>=" para cada slug conhecido de tools/ci/
# systems.txt - se aparecer, o passo nao esta lendo o arquivo em tempo
# de execucao, esta citando o nome fixo no proprio ci.yml.
def _hardcoded_slug_assignment_errors(parity_job_block, systems):
    errors = []
    for slug in sorted(systems):
        pattern = re.compile(r"(?<![\w-])" + re.escape(slug) + r"=")
        for raw_line in parity_job_block.splitlines():
            line = raw_line.strip()
            if pattern.search(line):
                errors.append(
                    f"job 'parity': linha cita {slug!r}= escrito a mao - os argumentos por "
                    f"sistema tem que vir de ler tools/ci/systems.txt em tempo de execucao: {line!r}"
                )
    return errors


# --- veredito completo ----------------------------------------------


def run_check(ci_yml_text, systems):
    errors = _matrix_slug_errors(ci_yml_text, systems)
    parity_block = extract_job_block(ci_yml_text, "parity")
    if parity_block is None:
        fail("job 'parity' nao encontrado no ci.yml")
    errors.extend(_hardcoded_slug_assignment_errors(parity_block, systems))
    return errors


# --- modo real -------------------------------------------------------


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def real_main(args):
    if len(args) != 2:
        fail("usage: check_ci_systems_source.py --check <ci.yml> <systems.txt>")
    ci_yml_path, systems_path = args

    ci_yml_text = _read_file(ci_yml_path)
    try:
        systems = ci_systems.parse_systems_text(_read_file(systems_path), source_label=systems_path)
    except ci_systems.CiSystemsError as exc:
        fail(str(exc))

    print(f"{SCRIPT_NAME}: {len(systems)} sistema(s) em {systems_path}")

    errors = run_check(ci_yml_text, systems)
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: OK - matriz do ci.yml bate com tools/ci/systems.txt, sem slug escrito a mao no job parity")


# --- selftest -----------------------------------------------------

_FAKE_SYSTEMS = {"fedora": "linux", "ubuntu": "linux", "arch": "linux", "windows": "windows"}

_FAKE_CI_YML_GOOD = """\
jobs:
  linux:
    strategy:
      matrix:
        include:
          - slug: fedora
          - slug: ubuntu
          - slug: arch
  windows:
    strategy:
      matrix:
        include:
          - slug: windows
  parity:
    steps:
      - name: monta argumentos
        run: |
          while IFS='|' read -r slug familia; do
            args="$args ${slug}=/tmp/persystem/${slug}.txt"
          done < tools/ci/systems.txt
"""


def selftest_positive_control():
    errors = run_check(_FAKE_CI_YML_GOOD, _FAKE_SYSTEMS)
    if errors:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (matriz bate com systems.txt, nenhum slug fixo no parity)")
    return True


# M1: 'arch' trocado por 'manjaro' - MESMA contagem, nome errado. Um
# portao que so' contasse passaria calado.
def selftest_wrong_name_same_count_reproves():
    ci_yml_wrong_name = _FAKE_CI_YML_GOOD.replace("- slug: arch", "- slug: manjaro")
    errors = run_check(ci_yml_wrong_name, _FAKE_SYSTEMS)
    if not errors:
        print("selftest: M1-NOME-ERRADO FALHOU (deveria ter reprovado - nome trocado, mesma contagem)", file=sys.stderr)
        return False
    if not any("manjaro" in e for e in errors) or not any("arch" in e for e in errors):
        print(f"selftest: M1-NOME-ERRADO FALHOU (reprovou, mas nao citou os dois nomes): {errors}", file=sys.stderr)
        return False
    print(f"selftest: M1-NOME-ERRADO OK (nome trocado, mesma contagem, pego): {errors}")
    return True


def selftest_extra_matrix_slug_reproves():
    ci_yml_extra = _FAKE_CI_YML_GOOD.replace("- slug: arch\n", "- slug: arch\n          - slug: cachyos\n")
    errors = run_check(ci_yml_extra, _FAKE_SYSTEMS)
    if not any("cachyos" in e for e in errors):
        print(f"selftest: SLUG-EXTRA-NA-MATRIZ FALHOU (deveria citar 'cachyos'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: SLUG-EXTRA-NA-MATRIZ OK (slug na matriz sem systems.txt correspondente pego): {errors}")
    return True


def selftest_missing_matrix_slug_reproves():
    systems_com_cachyos = dict(_FAKE_SYSTEMS)
    systems_com_cachyos["cachyos"] = "linux"
    errors = run_check(_FAKE_CI_YML_GOOD, systems_com_cachyos)
    if not any("cachyos" in e for e in errors):
        print(f"selftest: SLUG-FALTANDO-NA-MATRIZ FALHOU (deveria citar 'cachyos'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: SLUG-FALTANDO-NA-MATRIZ OK (systems.txt com slug ausente da matriz pego): {errors}")
    return True


def selftest_wrong_family_in_matrix_reproves():
    """'windows' citado no job errado (linux) - nome bate, familia nao."""
    ci_yml_wrong_family = _FAKE_CI_YML_GOOD.replace(
        "  linux:\n    strategy:\n      matrix:\n        include:\n          - slug: fedora",
        "  linux:\n    strategy:\n      matrix:\n        include:\n          - slug: windows\n          - slug: fedora",
    ).replace(
        "  windows:\n    strategy:\n      matrix:\n        include:\n          - slug: windows\n",
        "  windows:\n    strategy:\n      matrix:\n        include: []\n",
    )
    systems_so_windows_extra = dict(_FAKE_SYSTEMS)
    errors = run_check(ci_yml_wrong_family, systems_so_windows_extra)
    if not any("windows" in e and "familia" in e for e in errors):
        print(f"selftest: FAMILIA-ERRADA-NA-MATRIZ FALHOU: {errors}", file=sys.stderr)
        return False
    print(f"selftest: FAMILIA-ERRADA-NA-MATRIZ OK (slug no job errado pego): {errors}")
    return True


# M3: lista 'slug=' escrita a mao volta ao passo do parity.
def selftest_hardcoded_slug_in_parity_reproves():
    ci_yml_hardcoded = _FAKE_CI_YML_GOOD.replace(
        "      - name: monta argumentos\n        run: |\n          while IFS='|' read -r slug familia; do\n            args=\"$args ${slug}=/tmp/persystem/${slug}.txt\"\n          done < tools/ci/systems.txt\n",
        "      - name: chama o portao\n        run: |\n          python3 tests/tools/check_test_parity.py --per-system e.txt t.md fedora=/tmp/persystem/fedora.txt ubuntu=/tmp/persystem/ubuntu.txt arch=/tmp/persystem/arch.txt windows=/tmp/persystem/windows.txt\n",
    )
    errors = run_check(ci_yml_hardcoded, _FAKE_SYSTEMS)
    if not errors:
        print("selftest: M3-SLUG-FIXO-NO-PARITY FALHOU (lista escrita a mao deveria ter reprovado)", file=sys.stderr)
        return False
    citados = {"fedora", "ubuntu", "arch", "windows"}
    if not all(any(slug in e for e in errors) for slug in citados):
        print(f"selftest: M3-SLUG-FIXO-NO-PARITY FALHOU (nem todos os 4 slugs foram citados): {errors}", file=sys.stderr)
        return False
    print(f"selftest: M3-SLUG-FIXO-NO-PARITY OK ({len(errors)} slug(s) escrito(s) a mao pego(s))")
    return True


def selftest_job_not_found_reproves():
    """`run_check()` chama `fail()` (sys.exit) quando o job 'linux' ou
    'windows' nao existe no ci.yml - GODS_LAWS.md L-40, coleta quebrada
    nunca deve virar 'zero slug encontrado' calado."""
    ci_sem_linux = "jobs:\n  windows:\n    strategy:\n      matrix:\n        include:\n          - slug: windows\n  parity:\n    steps: []\n"
    try:
        run_check(ci_sem_linux, _FAKE_SYSTEMS)
    except SystemExit:
        print("selftest: JOB-INEXISTENTE OK (job 'linux' ausente reprova via fail(), nunca zero slug calado)")
        return True
    print("selftest: JOB-INEXISTENTE FALHOU (deveria ter reprovado - job 'linux' nao existe)", file=sys.stderr)
    return False


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_wrong_name_same_count_reproves(),
        selftest_extra_matrix_slug_reproves(),
        selftest_missing_matrix_slug_reproves(),
        selftest_wrong_family_in_matrix_reproves(),
        selftest_hardcoded_slug_in_parity_reproves(),
        selftest_job_not_found_reproves(),
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
        fail("usage: check_ci_systems_source.py --check <ci.yml> <systems.txt>  |  --selftest")


if __name__ == "__main__":
    main()
