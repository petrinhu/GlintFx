#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_ci_system_uniformity.py - CI-SPLIT-PER-OS A3b (docs/plano-ci-
# split-per-os.md secao 4.4, 4 itens): a matriz enxuta so' varia nas
# chaves permitidas, nenhum passo fora do preparo bifurca comportamento
# por sistema, cada script de preparo tem entrada de matriz (e vice-
# versa), e todo alvo de tools/ci/systems.txt tem entrada com "piso de
# ferramentas". GODS_LAWS.md L-04: paridade entre sistemas e' promessa
# publica, provada aqui, nao assumida.
#
# LEITURA DE TEXTO puro do ci.yml (GODS_LAWS.md L-09, nunca executa
# nada) - mesmo padrao dos portoes irmaos (check_ci_systems_source.py,
# check_ci_step_independence.py, check_container_fixture_inventory.py).
# extract_job_block importado de ci_systems.py (A3a-fix: morava em
# check_ci_systems_source.py, mas esse modulo passou a importar
# extract_matrix_entries() DAQUI - import circular real, medido
# (ImportError). Movida para ci_systems.py, a biblioteca comum que os
# dois portoes ja importavam sem ciclo; a QUARTA copia da mesma funcao
# virava a QUINTA, reaproveitar reduz a divida em vez de aumenta-la,
# GODS_LAWS.md L-17 regra de 3).
#
# Usage:
#   check_ci_system_uniformity.py --check <ci.yml> <systems.txt> <env_dir>
#   check_ci_system_uniformity.py --selftest

import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ci_systems  # noqa: E402
from ci_systems import extract_job_block  # noqa: E402

SCRIPT_NAME = "check_ci_system_uniformity.py"

# Jobs "por sistema" no modelo astrometrica (fail-fast: false, um job
# por alvo). O job `windows` nao tem script de preparo proprio (FATO,
# D-A12 do CTO: "o preparo dele e' pwsh dentro do ci.yml") - os itens
# (3) e (4) desta fatia ficam restritos a familia LINUX por isso; os
# itens (1) e (2) continuam valendo para os dois jobs.
_JOBS_POR_SISTEMA = ("linux", "windows")

# Nome do passo que legitimamente despacha por sistema (unico onde
# `matrix.slug` e' esperado, dentro do proprio `run:` - o item (2) so'
# proibe CONDICAO `if:` que cite sistema, nao o despacho em si).
_PASSO_DE_PREPARO = "Instalar toolchain"

_PISO_STEP_NAME = "Piso de ferramentas"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- (1) entradas de matriz diferem so' nas chaves permitidas ----------

# Extracao por INDENTACAO RELATIVA a propria linha "include:" - nunca
# um numero fixo de espacos (YAML aninha por indentacao, e um numero
# fixo quebra se o job mudar de profundidade). Cada entrada comeca com
# uma linha "- chave: valor" na indentacao "include_indent + 2"; campos
# seguintes da MESMA entrada ficam na indentacao "include_indent + 4".
_INCLUDE_LINE_RE = re.compile(r"^(\s*)include:\s*$")
_ENTRY_START_RE = re.compile(r"^- (\w+):\s*(.*)$")
_ENTRY_FIELD_RE = re.compile(r"^(\w+):\s*(.*)$")


def extract_matrix_entries(job_block_text):
    lines = job_block_text.splitlines()
    include_indent = None
    entries = []
    current = None
    for line in lines:
        if include_indent is None:
            m = _INCLUDE_LINE_RE.match(line)
            if m:
                include_indent = len(m.group(1))
            continue
        if not line.strip():
            continue
        indent = len(line) - len(line.lstrip(" "))
        if indent <= include_indent:
            break
        content = line.lstrip(" ")
        m_entry = _ENTRY_START_RE.match(content)
        if m_entry:
            if current is not None:
                entries.append(current)
            current = {m_entry.group(1): m_entry.group(2)}
            continue
        m_field = _ENTRY_FIELD_RE.match(content)
        if m_field and current is not None:
            current[m_field.group(1)] = m_field.group(2)
    if current is not None:
        entries.append(current)
    return entries


def _matrix_key_uniformity_errors(job_name, ci_yml_text):
    errors = []
    job_block = extract_job_block(ci_yml_text, job_name)
    if job_block is None:
        return errors  # job ausente e' erro de OUTRO check (2/3/4), nao deste
    entries = extract_matrix_entries(job_block)
    if not entries:
        errors.append(f"job {job_name!r}: matrix.include vazio ou nao encontrado")
        return errors
    chaves_base = frozenset(entries[0].keys())
    for i, entry in enumerate(entries):
        chaves = frozenset(entry.keys())
        if chaves != chaves_base:
            extras = sorted(chaves - chaves_base)
            faltando = sorted(chaves_base - chaves)
            rotulo = entry.get("nome", f"entrada #{i}")
            detalhe = []
            if extras:
                detalhe.append(f"chave(s) extra {extras}")
            if faltando:
                detalhe.append(f"chave(s) faltando {faltando}")
            errors.append(
                f"job {job_name!r}, entrada {rotulo!r}: difere da primeira entrada em "
                + " e ".join(detalhe)
            )
    return errors


# --- (2) nenhum passo fora do preparo bifurca por sistema, nem em if: --
# nem dentro do proprio `run:` -----------------------------------------
#
# S3 (revisao do main, achado real contra a arvore da A3b, nao suposto):
# o texto do plano (4.4, item 2) so' fala em `if:`, mas o PROPOSITO da
# secao - "o conserto por sistema mora so' no preparo" - e' violado
# igual por um condicional escondido dentro do shell: um passo SEM
# `if:` cujo `run:` faz `if [ "${{ matrix.slug }}" = arch ]; then ...;
# fi` bifurca por sistema do mesmo jeito, e o check antigo (so' linha
# `if:`) nao via. Cobrir os dois: a condicao YAML (`if:`, sem `${{ }}`,
# forma `matrix.slug == 'arch'`) E qualquer comparacao dentro do texto
# `run:` (bash, com `${{ }}` explicito: `= `, `==`, `!=`, `case ... in`,
# `[[ ... ]]`). Composicao de NOME/CAMINHO com `${{ matrix.slug }}`
# (ex.: `name: parity-inv-${{ matrix.slug }}-...`) continua permitida -
# so' reprova quando o valor e' COMPARADO, nao quando so' e' citado.

_STEP_NAME_RE = re.compile(r"^\s*-\s*name:\s*(.+?)\s*$")
_IF_LINE_RE = re.compile(r"^\s*if:\s*(.+?)\s*$")
_SISTEMA_REF_RE = re.compile(r"matrix\.(slug|imagem|nome)\b")
_RUN_SYSTEM_COMPARISON_RE = re.compile(
    r"\[\[?\s*\"?\$\{\{\s*matrix\.(?:slug|imagem)\s*\}\}\"?\s*(?:=|==|!=)"
    r"|(?:=|==|!=)\s*\"?\$\{\{\s*matrix\.(?:slug|imagem)\s*\}\}\"?"
    r"|case\s+\"?\$\{\{\s*matrix\.(?:slug|imagem)\s*\}\}\"?\s+in"
)


def _step_bifurca_por_sistema_errors(job_name, ci_yml_text):
    errors = []
    job_block = extract_job_block(ci_yml_text, job_name)
    if job_block is None:
        return errors
    nome_atual = "<passo sem nome ainda>"
    for line in job_block.splitlines():
        m_name = _STEP_NAME_RE.match(line)
        if m_name:
            nome_atual = m_name.group(1)
            continue
        if nome_atual == _PASSO_DE_PREPARO:
            continue
        m_if = _IF_LINE_RE.match(line)
        if m_if:
            condicao = m_if.group(1)
            if _SISTEMA_REF_RE.search(condicao):
                errors.append(
                    f"job {job_name!r}, passo {nome_atual!r}: if: cita sistema fora do preparo "
                    f"({condicao!r}) - GODS_LAWS.md L-04, paridade exige comportamento igual em "
                    f"todo sistema"
                )
            continue
        if _RUN_SYSTEM_COMPARISON_RE.search(line):
            errors.append(
                f"job {job_name!r}, passo {nome_atual!r}: run: compara sistema fora do preparo "
                f"({line.strip()!r}) - GODS_LAWS.md L-04, o conserto por sistema mora so' no "
                f"preparo; citar ${{{{ matrix.slug }}}} para compor nome/caminho continua "
                f"permitido, so' a COMPARACAO reprova"
            )
    return errors


# --- (3) todo tools/ci/env/*.sh tem entrada de matriz, e vice-versa ----
# Restrito a familia LINUX (FATO: windows nao tem script de preparo
# proprio - D-A12 do CTO, "o preparo dele e' pwsh dentro do ci.yml").


def _env_script_correspondence_errors(env_dir, systems):
    errors = []
    linux_slugs = set(ci_systems.slugs_of_family(systems, "linux"))
    script_slugs = set()
    for path in sorted(Path(env_dir).glob("*.sh")):
        script_slugs.add(path.stem)

    so_no_systems = sorted(linux_slugs - script_slugs)
    for slug in so_no_systems:
        errors.append(
            f"slug {slug!r} (familia linux, tools/ci/systems.txt) nao tem script "
            f"tools/ci/env/{slug}.sh"
        )
    so_no_script = sorted(script_slugs - linux_slugs)
    for slug in so_no_script:
        errors.append(
            f"tools/ci/env/{slug}.sh existe mas {slug!r} nao e' um slug de familia linux em "
            f"tools/ci/systems.txt"
        )
    return errors


# --- (4) todo alvo de tools/ci/systems.txt tem entrada, com piso -------
# Restrito a familia LINUX pelo mesmo motivo do item (3) - o piso de
# ferramentas (passo dedicado) so' existe no job `linux` nesta fatia.


def _target_has_entry_and_floor_errors(ci_yml_text, systems):
    errors = []
    linux_slugs = set(ci_systems.slugs_of_family(systems, "linux"))
    linux_block = extract_job_block(ci_yml_text, "linux")
    if linux_block is None:
        fail("job 'linux' nao encontrado no ci.yml")

    matrix_slugs = set()
    for entry in extract_matrix_entries(linux_block):
        if "slug" in entry:
            matrix_slugs.add(entry["slug"])

    sem_entrada = sorted(linux_slugs - matrix_slugs)
    for slug in sem_entrada:
        errors.append(
            f"slug {slug!r} (tools/ci/systems.txt, familia linux) nao tem entrada na matriz "
            f"do job 'linux'"
        )

    tem_piso = any(
        _STEP_NAME_RE.match(line) and _STEP_NAME_RE.match(line).group(1) == _PISO_STEP_NAME
        for line in linux_block.splitlines()
    )
    if not tem_piso and linux_slugs:
        errors.append(
            f"job 'linux' nao tem passo {_PISO_STEP_NAME!r} - alvo(s) {sorted(linux_slugs)} "
            f"sem piso de ferramentas provado"
        )
    return errors


# --- veredito completo --------------------------------------------------


def run_check(ci_yml_text, systems, env_dir):
    errors = []
    for job_name in _JOBS_POR_SISTEMA:
        errors.extend(_matrix_key_uniformity_errors(job_name, ci_yml_text))
        errors.extend(_step_bifurca_por_sistema_errors(job_name, ci_yml_text))
    errors.extend(_env_script_correspondence_errors(env_dir, systems))
    errors.extend(_target_has_entry_and_floor_errors(ci_yml_text, systems))
    return errors


# --- modo real -----------------------------------------------------------


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def real_main(args):
    if len(args) != 3:
        fail("usage: check_ci_system_uniformity.py --check <ci.yml> <systems.txt> <env_dir>")
    ci_yml_path, systems_path, env_dir = args

    ci_yml_text = _read_file(ci_yml_path)
    try:
        systems = ci_systems.parse_systems_text(_read_file(systems_path), source_label=systems_path)
    except ci_systems.CiSystemsError as exc:
        fail(str(exc))

    if not Path(env_dir).is_dir():
        fail(f"diretorio nao encontrado: {env_dir}")

    errors = run_check(ci_yml_text, systems, env_dir)
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(
        f"{SCRIPT_NAME}: OK - matriz enxuta uniforme, nenhum passo bifurca por sistema fora do "
        f"preparo, scripts de env/ correspondem a systems.txt, todo alvo linux tem entrada + piso"
    )


# --- selftest --------------------------------------------------------

_FAKE_SYSTEMS = {"fedora": "linux", "ubuntu": "linux", "windows": "windows"}

_FAKE_CI_YML_GOOD = """\
jobs:
  linux:
    strategy:
      matrix:
        include:
          - nome: Fedora - compartilhado
            slug: fedora
            imagem: fedora:latest
            modo: compartilhado
          - nome: Fedora - estatico
            slug: fedora
            imagem: fedora:latest
            modo: estatico
          - nome: Ubuntu - compartilhado
            slug: ubuntu
            imagem: ubuntu:latest
            modo: compartilhado
    steps:
      - name: Instalar toolchain
        run: bash tools/ci/env/${{ matrix.slug }}.sh
      - name: Piso de ferramentas
        run: echo ok
      - name: Versoes da toolchain
        run: echo ok
  windows:
    strategy:
      matrix:
        include:
          - nome: Windows - compartilhado
            slug: windows
            modo: compartilhado
    steps:
      - name: Instalar toolchain
        run: echo pwsh aqui
"""


# tempfile.mkdtemp() (stdlib, portatil - GODS_LAWS.md L-44, nunca
# caminho absoluto de sessao/maquina em arquivo versionado): cada
# selftest ganha um diretorio TEMPORARIO proprio, nunca reaproveitado
# entre selftests nem escrito num caminho fixo.
def _fake_env_dir(rotulo, slugs):
    d = Path(tempfile.mkdtemp(prefix=f"glintfx-uniformity-selftest-{rotulo}-"))
    for slug in slugs:
        (d / f"{slug}.sh").write_text("#!/usr/bin/env bash\n")
    return str(d)


def selftest_positive_control():
    env_dir = _fake_env_dir("good", ["fedora", "ubuntu"])
    errors = run_check(_FAKE_CI_YML_GOOD, _FAKE_SYSTEMS, env_dir)
    if errors:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (matriz uniforme, sem bifurcacao, scripts batem, piso presente)")
    return True


# item (1): uma entrada com campo EXTRA que as outras nao tem.
def selftest_extra_field_in_one_entry_reproves():
    ci_extra = _FAKE_CI_YML_GOOD.replace(
        "          - nome: Ubuntu - compartilhado\n            slug: ubuntu\n            imagem: ubuntu:latest\n            modo: compartilhado\n",
        "          - nome: Ubuntu - compartilhado\n            slug: ubuntu\n            imagem: ubuntu:latest\n            modo: compartilhado\n            instalar: apt-get install -y foo\n",
    )
    env_dir = _fake_env_dir("extra", ["fedora", "ubuntu"])
    errors = run_check(ci_extra, _FAKE_SYSTEMS, env_dir)
    if not any("extra" in e and "instalar" in e for e in errors):
        print(f"selftest: CAMPO-EXTRA FALHOU (deveria citar 'instalar' como chave extra): {errors}", file=sys.stderr)
        return False
    print(f"selftest: CAMPO-EXTRA OK: {errors}")
    return True


# item (2): O RED DE ESTREIA que o proprio plano descreve (docs/plano-
# ci-split-per-os.md secao 4.4): "um passo `if: matrix.nome == 'Arch -
# compartilhado'` fora do preparo: nenhum portao de hoje o pega
# (vermelho); depois reprova." Cenario reproduzido literalmente.
def selftest_if_citing_system_outside_preparo_reproves():
    ci_bifurcado = _FAKE_CI_YML_GOOD.replace(
        "      - name: Versoes da toolchain\n        run: echo ok\n",
        "      - name: Versoes da toolchain\n        if: matrix.nome == 'Arch - compartilhado'\n        run: echo ok\n",
    )
    env_dir = _fake_env_dir("bifurca", ["fedora", "ubuntu"])
    errors = run_check(ci_bifurcado, _FAKE_SYSTEMS, env_dir)
    if not any("Versoes da toolchain" in e and "matrix.nome" in e for e in errors):
        print(f"selftest: IF-BIFURCA-SISTEMA FALHOU (deveria pegar o if: fora do preparo): {errors}", file=sys.stderr)
        return False
    print(f"selftest: IF-BIFURCA-SISTEMA OK (red de estreia do plano reproduzido e pego): {errors}")
    return True


# item (2), controle negativo: if: no PROPRIO preparo NAO reprova (e' o
# unico passo onde citar sistema seria legitimo, ainda que hoje ele
# desapche via run:, nao via if:).
def selftest_if_in_preparo_itself_does_not_reprove():
    ci_if_no_preparo = _FAKE_CI_YML_GOOD.replace(
        "      - name: Instalar toolchain\n        run: bash tools/ci/env/${{ matrix.slug }}.sh\n",
        "      - name: Instalar toolchain\n        if: matrix.slug != ''\n        run: bash tools/ci/env/${{ matrix.slug }}.sh\n",
    )
    env_dir = _fake_env_dir("preparo-if", ["fedora", "ubuntu"])
    errors = run_check(ci_if_no_preparo, _FAKE_SYSTEMS, env_dir)
    if any("Instalar toolchain" in e for e in errors):
        print(f"selftest: IF-NO-PREPARO-CONTROLE FALHOU (nao deveria reprovar o proprio preparo): {errors}", file=sys.stderr)
        return False
    print("selftest: IF-NO-PREPARO-CONTROLE OK (if: no proprio preparo nao reprova)")
    return True


# S3 (achado real do main, revisao contra a arvore da A3b): um passo
# SEM if: cujo run: bifurca por sistema DENTRO do shell escapava do
# portao antigo (que so' olhava a linha if:). Vermelho de estreia
# reproduzido com a MESMA fixture que o main usou para achar o
# defeito, antes do fix.
def selftest_run_comparison_outside_preparo_reproves():
    ci_s3 = _FAKE_CI_YML_GOOD.replace(
        "      - name: Versoes da toolchain\n        run: echo ok\n",
        "      - name: Versoes da toolchain\n        run: |\n"
        "          if [ \"${{ matrix.slug }}\" = arch ]; then\n"
        "            echo especial\n"
        "          fi\n",
    )
    env_dir = _fake_env_dir("s3-run-if", ["fedora", "ubuntu"])
    errors = run_check(ci_s3, _FAKE_SYSTEMS, env_dir)
    if not any("Versoes da toolchain" in e and "run:" in e for e in errors):
        print(f"selftest: S3-RUN-COMPARA-SISTEMA FALHOU (deveria pegar o if bash dentro do run:): {errors}", file=sys.stderr)
        return False
    print(f"selftest: S3-RUN-COMPARA-SISTEMA OK (achado do main reproduzido e pego): {errors}")
    return True


# controle POSITIVO do S3: citar ${{ matrix.slug }} para COMPOR nome de
# artefato/caminho (sem comparar) continua permitido - o proprio passo
# "Instalar toolchain" da fixture ja faz isso
# (bash tools/ci/env/${{ matrix.slug }}.sh) e nunca reprovou; este
# controle prova o mesmo padrao num passo QUALQUER, fora do preparo.
def selftest_run_slug_composition_outside_preparo_does_not_reprove():
    ci_composicao = _FAKE_CI_YML_GOOD.replace(
        "      - name: Versoes da toolchain\n        run: echo ok\n",
        "      - name: Versoes da toolchain\n"
        "        run: echo \"nome do artefato: parity-inv-${{ matrix.slug }}-${{ matrix.modo }}\"\n",
    )
    env_dir = _fake_env_dir("composicao-nome", ["fedora", "ubuntu"])
    errors = run_check(ci_composicao, _FAKE_SYSTEMS, env_dir)
    if any("Versoes da toolchain" in e for e in errors):
        print(f"selftest: COMPOSICAO-NOME-CONTROLE FALHOU (citar slug em nome/caminho nao deveria reprovar): {errors}", file=sys.stderr)
        return False
    print("selftest: COMPOSICAO-NOME-CONTROLE OK (compor nome de artefato com slug continua permitido)")
    return True


# item (3): script sem entrada correspondente em systems.txt.
def selftest_orphan_script_reproves():
    env_dir = _fake_env_dir("orfao", ["fedora", "ubuntu", "manjaro"])
    errors = run_check(_FAKE_CI_YML_GOOD, _FAKE_SYSTEMS, env_dir)
    if not any("manjaro" in e for e in errors):
        print(f"selftest: SCRIPT-ORFAO FALHOU (deveria citar 'manjaro'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: SCRIPT-ORFAO OK: {errors}")
    return True


# item (3), espelho: slug de familia linux SEM script correspondente.
def selftest_missing_script_reproves():
    env_dir = _fake_env_dir("sem-script", ["fedora"])
    errors = run_check(_FAKE_CI_YML_GOOD, _FAKE_SYSTEMS, env_dir)
    if not any("ubuntu" in e and "script" in e for e in errors):
        print(f"selftest: SCRIPT-FALTANDO FALHOU (deveria citar 'ubuntu' sem script): {errors}", file=sys.stderr)
        return False
    print(f"selftest: SCRIPT-FALTANDO OK: {errors}")
    return True


# item (4): slug de familia linux sem entrada de matriz nenhuma.
def selftest_target_without_matrix_entry_reproves():
    systems_com_arch = dict(_FAKE_SYSTEMS)
    systems_com_arch["arch"] = "linux"
    env_dir = _fake_env_dir("sem-entrada", ["fedora", "ubuntu", "arch"])
    errors = run_check(_FAKE_CI_YML_GOOD, systems_com_arch, env_dir)
    if not any("arch" in e and "entrada" in e for e in errors):
        print(f"selftest: ALVO-SEM-ENTRADA FALHOU (deveria citar 'arch' sem entrada): {errors}", file=sys.stderr)
        return False
    print(f"selftest: ALVO-SEM-ENTRADA OK: {errors}")
    return True


# item (4): job linux SEM o passo "Piso de ferramentas".
def selftest_missing_piso_step_reproves():
    ci_sem_piso = _FAKE_CI_YML_GOOD.replace(
        "      - name: Piso de ferramentas\n        run: echo ok\n", ""
    )
    env_dir = _fake_env_dir("sem-piso", ["fedora", "ubuntu"])
    errors = run_check(ci_sem_piso, _FAKE_SYSTEMS, env_dir)
    if not any("Piso de ferramentas" in e for e in errors):
        print(f"selftest: PISO-AUSENTE FALHOU (deveria citar o passo ausente): {errors}", file=sys.stderr)
        return False
    print(f"selftest: PISO-AUSENTE OK: {errors}")
    return True


def selftest_job_not_found_reproves():
    """`run_check()` chama `fail()` via `_target_has_entry_and_floor_errors`
    quando o job 'linux' nao existe - GODS_LAWS.md L-40, coleta quebrada
    nunca deve virar 'zero problema' calado."""
    ci_sem_linux = "jobs:\n  windows:\n    strategy:\n      matrix:\n        include:\n          - slug: windows\n    steps: []\n"
    env_dir = _fake_env_dir("sem-job", ["fedora", "ubuntu"])
    try:
        run_check(ci_sem_linux, _FAKE_SYSTEMS, env_dir)
    except SystemExit:
        print("selftest: JOB-INEXISTENTE OK (job 'linux' ausente reprova via fail(), nunca zero problema calado)")
        return True
    print("selftest: JOB-INEXISTENTE FALHOU (deveria ter reprovado - job 'linux' nao existe)", file=sys.stderr)
    return False


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_extra_field_in_one_entry_reproves(),
        selftest_if_citing_system_outside_preparo_reproves(),
        selftest_if_in_preparo_itself_does_not_reprove(),
        selftest_run_comparison_outside_preparo_reproves(),
        selftest_run_slug_composition_outside_preparo_does_not_reprove(),
        selftest_orphan_script_reproves(),
        selftest_missing_script_reproves(),
        selftest_target_without_matrix_entry_reproves(),
        selftest_missing_piso_step_reproves(),
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
        fail("usage: check_ci_system_uniformity.py --check <ci.yml> <systems.txt> <env_dir>  |  --selftest")


if __name__ == "__main__":
    main()
