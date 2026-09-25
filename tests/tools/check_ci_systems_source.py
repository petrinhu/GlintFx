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
# A3A-FIX (revisao do CTO, mutation testing real contra a arvore, apos
# a A3a aceita em 3d3969d): tres achados IMPORTANTES.
#
# R1: o portao antigo comparava o CONJUNTO de slugs entre a matriz e
# systems.txt - uniao, nunca particao por job E por modo. Duas
# sabotagens escaparam: S1, apagar a entrada "CachyOS - estatico" (o
# CONJUNTO de slugs continuava {fedora,ubuntu,arch,cachyos,windows},
# so' um MODO sumiu); S2, trocar o slug da perna estatica do Windows
# por "fedora" (o conjunto por job ainda "batia" porque a uniao nao
# distingue QUAL job tem QUAL par). Conserto: `_matrix_slug_modo_
# errors()` compara PARES (slug, modo) - todo slug de familia linux
# tem EXATAMENTE {"compartilhado","estatico"} no job `linux` e em
# NENHUM outro job; o slug windows idem no job `windows`.
#
# R2: `_hardcoded_slug_assignment_errors()` antigo so' procurava a
# forma literal "<slug>=". S3 escapou: `for slug in windows fedora
# ubuntu arch cachyos; do` cita os cinco slugs sem nenhum "=" na mesma
# linha. Conserto: qualquer TOKEN de slug solto (limite de palavra,
# nunca substring - "ubuntu-latest" nao e' "ubuntu") dentro de um
# bloco `run:` do job `parity`, fora de comentario e fora de uma linha
# que invoca `ci_systems.py --tool` (o carregador, uso legitimo),
# reprova. Restrito a blocos `run:` (extract_run_blocks) - varrer o
# job inteiro pegaria falso positivo em campos legitimos do GHA (ex.:
# `needs: [linux, windows, ...]`).
#
# LEITURA DE TEXTO puro do ci.yml (GODS_LAWS.md L-09, nunca executa
# nada) - mesmo padrao de check_ci_step_independence.py/check_
# container_fixture_inventory.py (extract_job_block ja e' a TERCEIRA
# copia da mesma funcao nesses dois; aqui e' a QUARTA - extracao para
# modulo compartilhado registrada como debito conhecido, GODS_LAWS.md
# L-17 regra de 3, fora do escopo desta fatia, L-32). extract_matrix_
# entries importado de check_ci_system_uniformity.py (mesma logica de
# indentacao relativa, nao reimplementada uma terceira vez).
#
# Usage:
#   check_ci_systems_source.py --check <ci.yml> <systems.txt>
#   check_ci_systems_source.py --selftest

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ci_systems  # noqa: E402
from ci_systems import extract_job_block  # noqa: E402
from check_ci_system_uniformity import extract_matrix_entries  # noqa: E402

SCRIPT_NAME = "check_ci_systems_source.py"
_MODOS_ESPERADOS = frozenset({"compartilhado", "estatico"})


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- slugs da matriz, agora por PAR (slug, modo) por job ---------------

# M1 do plano: um portao que so comparasse a CONTAGEM de slugs nao
# pegaria "arch" trocado por "manjaro" (mesma contagem, nome errado) -
# devolve o CONJUNTO de nomes, nunca um numero. R1 (A3A-FIX): contagem
# de SLUGS tambem nao pega S1/S2 (ver cabecalho) - precisa do PAR
# (slug, modo).


def extract_matrix_slug_modo_pairs(job_block_text):
    """Devolve a lista de (slug, modo) de cada entrada de matrix.include
    - `slug`/`modo` podem ser None se a entrada nao tiver a chave (o
    chamador decide como reportar)."""
    return [(e.get("slug"), e.get("modo")) for e in extract_matrix_entries(job_block_text)]


def _matrix_slug_modo_errors(ci_yml_text, systems):
    errors = []
    linux_block = extract_job_block(ci_yml_text, "linux")
    windows_block = extract_job_block(ci_yml_text, "windows")
    if linux_block is None:
        fail("job 'linux' nao encontrado no ci.yml")
    if windows_block is None:
        fail("job 'windows' nao encontrado no ci.yml")

    linux_pares = extract_matrix_slug_modo_pairs(linux_block)
    windows_pares = extract_matrix_slug_modo_pairs(windows_block)

    linux_slugs_matriz = {slug for slug, _ in linux_pares if slug}
    windows_slugs_matriz = {slug for slug, _ in windows_pares if slug}
    matrix_slugs = linux_slugs_matriz | windows_slugs_matriz
    systems_slugs = set(systems)

    so_na_matriz = sorted(matrix_slugs - systems_slugs)
    for slug in so_na_matriz:
        errors.append(
            f"slug {slug!r} aparece na matriz do ci.yml mas nao existe em tools/ci/systems.txt"
        )

    so_no_systems = sorted(systems_slugs - matrix_slugs)
    for slug in so_no_systems:
        errors.append(
            f"slug {slug!r} existe em tools/ci/systems.txt mas nao aparece em nenhuma matriz do ci.yml"
        )

    linux_slugs_esperados = ci_systems.slugs_of_family(systems, "linux")
    windows_slugs_esperados = ci_systems.slugs_of_family(systems, "windows")

    # R1 (S1): todo slug de familia linux tem EXATAMENTE os dois modos
    # no job 'linux' - nem a menos (S1, um modo sumiu), nem a mais.
    for slug in sorted(linux_slugs_esperados):
        modos = {modo for s, modo in linux_pares if s == slug}
        if modos != _MODOS_ESPERADOS:
            errors.append(
                f"slug {slug!r} (familia linux): job 'linux' tem os modos {sorted(m for m in modos if m)}, "
                f"esperado exatamente {sorted(_MODOS_ESPERADOS)}"
            )
        if slug in windows_slugs_matriz:
            errors.append(f"slug {slug!r} (familia linux) aparece na matriz do job 'windows'")

    # R1 (S2): mesma regra para o(s) slug(s) de familia windows - pega
    # a perna estatica do Windows com slug trocado por 'fedora' (o par
    # (windows, estatico) some do job windows, e 'fedora' - familia
    # linux - aparece la, pego pela regra acima tambem).
    for slug in sorted(windows_slugs_esperados):
        modos = {modo for s, modo in windows_pares if s == slug}
        if modos != _MODOS_ESPERADOS:
            errors.append(
                f"slug {slug!r} (familia windows): job 'windows' tem os modos {sorted(m for m in modos if m)}, "
                f"esperado exatamente {sorted(_MODOS_ESPERADOS)}"
            )
        if slug in linux_slugs_matriz:
            errors.append(f"slug {slug!r} (familia windows) aparece na matriz do job 'linux'")

    return errors


# --- nenhum slug escrito a mao no job `parity` --------------------------

# R2 (A3A-FIX, S3): a forma antiga so' procurava "<slug>=" - `for slug
# in windows fedora ubuntu arch cachyos; do` cita os cinco nomes sem
# "=" nenhum e escapava. Restrito a blocos `run:` (nunca o job inteiro
# - campos legitimos do GHA como `needs: [linux, windows, ...]`
# citariam o nome do slug sem ser hardcode de argumento).
_RUN_LINE_RE = re.compile(r"^(\s*)run:\s*(.*)$")


def extract_run_blocks(job_block_text):
    """Devolve uma lista de textos, um por bloco `run:` de cada step -
    forma de uma linha (`run: comando`) e forma multilinha (`run: |` /
    `run: >-`, linhas seguintes com indentacao MAIOR que a do `run:`)."""
    lines = job_block_text.splitlines()
    blocks = []
    i = 0
    while i < len(lines):
        m = _RUN_LINE_RE.match(lines[i])
        if not m:
            i += 1
            continue
        indent = len(m.group(1))
        resto = m.group(2).strip()
        if resto and resto not in ("|", ">-", ">", "|-"):
            blocks.append(resto)
            i += 1
            continue
        bloco_linhas = []
        i += 1
        while i < len(lines):
            prox = lines[i]
            if not prox.strip():
                bloco_linhas.append(prox)
                i += 1
                continue
            prox_indent = len(prox) - len(prox.lstrip(" "))
            if prox_indent <= indent:
                break
            bloco_linhas.append(prox)
            i += 1
        blocks.append("\n".join(bloco_linhas))
    return blocks


# M3 do plano (R2, alargado): qualquer TOKEN de slug solto (limite de
# palavra) dentro de um bloco `run:` do job `parity` reprova, fora de
# comentario e fora de uma linha que invoca `ci_systems.py --tool` (o
# carregador - uso legitimo, a propria linha nunca cita o NOME do
# slug). Cobre tanto "<slug>=" (forma antiga) quanto `for slug in
# windows fedora ...` (S3) e qualquer outra forma futura de citar o
# nome a mao.
# A3a-fix, F2 (revisao do CTO, S3d): isentar a LINHA INTEIRA quando ela
# continha "ci_systems.py" deixava passar uma linha que MISTURA a
# chamada legitima do carregador com uma comparacao hardcoded no MESMO
# comando composto - `slug=$(... ci_systems.py --tool slug-of-artifact
# "$resto"); [ "$slug" = fedora ] && continue` tem os dois na mesma
# linha, e o "fedora" hardcoded escapava. Conserto: dividir a linha por
# delimitador de comando shell (`;`, `&&`, `||`, `|`) e aplicar a busca
# de slug em CADA SEGMENTO - so' o segmento que de fato invoca
# `ci_systems.py` fica isento, os outros continuam sob a regra.
_COMMAND_SEPARATOR_RE = re.compile(r";|&&|\|\|(?!\|)|\|(?!\|)")


def _split_shell_segments(line):
    return [seg.strip() for seg in _COMMAND_SEPARATOR_RE.split(line) if seg.strip()]


def _hardcoded_slug_assignment_errors(parity_job_block, systems):
    errors = []
    run_blocks = extract_run_blocks(parity_job_block)
    for slug in sorted(systems):
        pattern = re.compile(r"(?<![\w-])" + re.escape(slug) + r"(?![\w-])")
        for block in run_blocks:
            for raw_line in block.splitlines():
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue
                for segment in _split_shell_segments(line):
                    if "ci_systems.py" in segment:
                        continue
                    if pattern.search(segment):
                        errors.append(
                            f"job 'parity': linha cita o slug {slug!r} fora de ci_systems.py "
                            f"--tool - os argumentos por sistema tem que vir do carregador em "
                            f"tempo de execucao: {line!r}"
                        )
    return errors


# --- veredito completo ----------------------------------------------


def run_check(ci_yml_text, systems):
    errors = _matrix_slug_modo_errors(ci_yml_text, systems)
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

# Cada slug de familia linux com os DOIS modos (R1) - a fixture antiga
# so' tinha uma entrada por slug, sem `modo:`, o que nunca teria
# reproduzido S1/S2 (o par (slug,modo) e' o que a R1 confere).
_FAKE_CI_YML_GOOD = """\
jobs:
  linux:
    strategy:
      matrix:
        include:
          - slug: fedora
            modo: compartilhado
          - slug: fedora
            modo: estatico
          - slug: ubuntu
            modo: compartilhado
          - slug: ubuntu
            modo: estatico
          - slug: arch
            modo: compartilhado
          - slug: arch
            modo: estatico
  windows:
    strategy:
      matrix:
        include:
          - slug: windows
            modo: compartilhado
          - slug: windows
            modo: estatico
  parity:
    needs: [linux, windows]
    steps:
      - name: monta argumentos
        run: |
          args=$(python3 tests/tools/ci_systems.py --tool args-per-system /tmp/persystem)
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
    ci_yml_wrong_name = _FAKE_CI_YML_GOOD.replace("slug: arch", "slug: manjaro")
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
    ci_yml_extra = _FAKE_CI_YML_GOOD.replace(
        "          - slug: arch\n            modo: estatico\n",
        "          - slug: arch\n            modo: estatico\n          - slug: cachyos\n            modo: compartilhado\n          - slug: cachyos\n            modo: estatico\n",
    )
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
    """'windows' citado no job errado (linux), com os dois modos -
    nome bate, familia nao."""
    ci_yml_wrong_family = _FAKE_CI_YML_GOOD.replace(
        "          - slug: fedora\n            modo: compartilhado\n",
        "          - slug: windows\n            modo: compartilhado\n          - slug: windows\n            modo: estatico\n          - slug: fedora\n            modo: compartilhado\n",
    ).replace(
        "          - slug: windows\n            modo: compartilhado\n          - slug: windows\n            modo: estatico\n  parity:",
        "  parity:",
    )
    errors = run_check(ci_yml_wrong_family, _FAKE_SYSTEMS)
    if not any("windows" in e and "familia" in e for e in errors):
        print(f"selftest: FAMILIA-ERRADA-NA-MATRIZ FALHOU: {errors}", file=sys.stderr)
        return False
    print(f"selftest: FAMILIA-ERRADA-NA-MATRIZ OK (slug no job errado pego): {errors}")
    return True


# --- R1: S1 e S2, as DUAS sabotagens reais do CTO que a contagem por
# CONJUNTO deixava passar (rc=0 contra a arvore real, confirmado antes
# do fix - ver o relatorio da A3a-fix) --------------------------------


# S1: apaga o par (cachyos, estatico) - o CONJUNTO de slugs continua
# {fedora,ubuntu,arch,windows} (cachyos nao esta em _FAKE_SYSTEMS, mas
# o efeito e' o mesmo com arch): um modo sumiu, a contagem de slugs
# nunca via isso.
def selftest_s1_missing_modo_pair_reproves():
    ci_s1 = _FAKE_CI_YML_GOOD.replace("          - slug: arch\n            modo: estatico\n", "")
    errors = run_check(ci_s1, _FAKE_SYSTEMS)
    if not any("arch" in e and "modos" in e for e in errors):
        print(f"selftest: S1-MODO-FALTANDO FALHOU (deveria citar 'arch' com os modos incompletos): {errors}", file=sys.stderr)
        return False
    print(f"selftest: S1-MODO-FALTANDO OK (achado real do CTO reproduzido e pego): {errors}")
    return True


# S2: troca o slug da perna estatica do Windows por 'fedora' - o
# CONJUNTO de slugs por job continuava "batendo" porque a uniao nao
# distingue QUAL job tem QUAL par.
def selftest_s2_wrong_slug_in_windows_leg_reproves():
    ci_s2 = _FAKE_CI_YML_GOOD.replace(
        "          - slug: windows\n            modo: estatico\n",
        "          - slug: fedora\n            modo: estatico\n",
    )
    errors = run_check(ci_s2, _FAKE_SYSTEMS)
    if not any("windows" in e and "modos" in e for e in errors):
        print(f"selftest: S2-SLUG-TROCADO FALHOU (deveria citar 'windows' com os modos incompletos): {errors}", file=sys.stderr)
        return False
    if not any("fedora" in e and "familia linux" in e and "'windows'" in e for e in errors):
        print(f"selftest: S2-SLUG-TROCADO FALHOU (deveria citar 'fedora' aparecendo no job windows): {errors}", file=sys.stderr)
        return False
    print(f"selftest: S2-SLUG-TROCADO OK (achado real do CTO reproduzido e pego, dupla causa): {errors}")
    return True


# M3/R2: lista 'slug=' escrita a mao volta ao passo do parity.
def selftest_hardcoded_slug_in_parity_reproves():
    ci_yml_hardcoded = _FAKE_CI_YML_GOOD.replace(
        "      - name: monta argumentos\n        run: |\n          args=$(python3 tests/tools/ci_systems.py --tool args-per-system /tmp/persystem)\n",
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


# R2, S3: `for slug in windows fedora ubuntu arch cachyos; do` - os
# cinco nomes sem NENHUM "=" na mesma linha, escapava do M3 antigo.
def selftest_s3_for_loop_without_equals_reproves():
    ci_s3 = _FAKE_CI_YML_GOOD.replace(
        "      - name: monta argumentos\n        run: |\n          args=$(python3 tests/tools/ci_systems.py --tool args-per-system /tmp/persystem)\n",
        "      - name: monta argumentos\n        run: |\n          args=\"\"\n          for slug in windows fedora ubuntu arch cachyos; do\n            args=\"$args ${slug}=/tmp/persystem/${slug}.txt\"\n          done\n",
    )
    errors = run_check(ci_s3, _FAKE_SYSTEMS)
    citados = {"windows", "fedora", "ubuntu", "arch"}
    if not all(any(slug in e for e in errors) for slug in citados):
        print(f"selftest: S3-FOR-SEM-IGUAL FALHOU (deveria citar os slugs soltos no 'for'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: S3-FOR-SEM-IGUAL OK (achado real do CTO reproduzido e pego): {errors}")
    return True


# A3a-fix F2 (S3d, achado real do CTO): a MESMA linha mistura a chamada
# legitima do carregador com uma comparacao hardcoded no comando
# composto seguinte (`;`) - a forma antiga isentava a LINHA INTEIRA por
# conter "ci_systems.py", deixando o "fedora" apos o ";" escapar.
def selftest_s3d_mixed_line_reproves():
    ci_s3d = _FAKE_CI_YML_GOOD.replace(
        "      - name: monta argumentos\n        run: |\n          args=$(python3 tests/tools/ci_systems.py --tool args-per-system /tmp/persystem)\n",
        "      - name: chama o portao\n        run: |\n          slug=$(python3 tests/tools/ci_systems.py --tool slug-of-artifact \"$resto\"); [ \"$slug\" = fedora ] && continue\n",
    )
    errors = run_check(ci_s3d, _FAKE_SYSTEMS)
    if not any("fedora" in e for e in errors):
        print(f"selftest: S3D-LINHA-MISTA FALHOU (deveria citar 'fedora' apos o ';'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: S3D-LINHA-MISTA OK (achado real do CTO reproduzido e pego): {errors}")
    return True


# R2, controle negativo: chamar ci_systems.py --tool NAO reprova - e' o
# uso LEGITIMO do carregador (a fixture positiva ja prova isso; este
# controle isola o motivo, provando que 'args-per-system' sozinho na
# linha nao dispara nenhum slug).
def selftest_ci_systems_tool_call_does_not_reprove():
    errors = _hardcoded_slug_assignment_errors(
        "steps:\n  - name: x\n    run: |\n      args=$(python3 tests/tools/ci_systems.py --tool args-per-system /tmp/persystem)\n",
        _FAKE_SYSTEMS,
    )
    if errors:
        print(f"selftest: CI-SYSTEMS-TOOL-CONTROLE FALHOU (chamada ao carregador nao deveria reprovar): {errors}", file=sys.stderr)
        return False
    print("selftest: CI-SYSTEMS-TOOL-CONTROLE OK (uso legitimo de ci_systems.py --tool nunca reprova)")
    return True


# R2, controle negativo: campo `needs: [linux, windows, ...]` (fora de
# qualquer `run:`) NAO reprova - restrito a blocos `run:`, nunca o job
# inteiro (a fixture positiva ja tem `needs: [linux, windows]` e passa).
def selftest_needs_field_outside_run_does_not_reprove():
    errors = _hardcoded_slug_assignment_errors(
        "needs: [linux, windows]\nsteps:\n  - name: x\n    run: echo ok\n",
        _FAKE_SYSTEMS,
    )
    if errors:
        print(f"selftest: NEEDS-FORA-DE-RUN-CONTROLE FALHOU (campo needs: nao deveria reprovar): {errors}", file=sys.stderr)
        return False
    print("selftest: NEEDS-FORA-DE-RUN-CONTROLE OK (campo needs:, fora de run:, nunca reprova)")
    return True


def selftest_job_not_found_reproves():
    """`run_check()` chama `fail()` (sys.exit) quando o job 'linux' ou
    'windows' nao existe no ci.yml - GODS_LAWS.md L-40, coleta quebrada
    nunca deve virar 'zero slug encontrado' calado."""
    ci_sem_linux = "jobs:\n  windows:\n    strategy:\n      matrix:\n        include:\n          - slug: windows\n            modo: compartilhado\n  parity:\n    steps: []\n"
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
        selftest_s1_missing_modo_pair_reproves(),
        selftest_s2_wrong_slug_in_windows_leg_reproves(),
        selftest_hardcoded_slug_in_parity_reproves(),
        selftest_s3_for_loop_without_equals_reproves(),
        selftest_s3d_mixed_line_reproves(),
        selftest_ci_systems_tool_call_does_not_reprove(),
        selftest_needs_field_outside_run_does_not_reprove(),
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
