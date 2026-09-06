#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_pkg_dep_coverage.py - CI gate for a regression medida ao vivo
# em 06/09/2026 (GHA run 34056358894, commit 40c2622): a fatia D-W6b-3
# (cmake/GlintfxEgl.cmake) passou a exigir `pkg_check_modules(...
# REQUIRED egl wayland-egl)`, e NENHUM dos cinco alvos do CI instalava
# o pacote de sistema que fornece `egl.pc` - catorze jobs Linux caíram
# no mesmo `CMake Error ... FindPkgConfig.cmake ... Configuring
# incomplete`. Passava local porque a máquina do autor tem o pacote
# (GODS_LAWS.md L-44, o mesmo defeito que já derrubou o job `sanitizer`
# em 25/08/2026 por falta de libasan/libubsan - "portão que nunca
# rodou no ambiente real não é portão").
#
# O defeito de fundo, não só o sintoma: a lista de pacotes que cada job
# instala é escrita à mão em .github/workflows/ci.yml, e nada confere
# essa lista contra o que cmake/*.cmake REALMENTE exige via
# pkg_check_modules(). Este portão fecha essa lacuna DERIVANDO os dois
# lados, nunca hardcoded contra o estado de hoje: (1) os módulos
# pkg-config REQUIRED lidos ao vivo de cmake/*.cmake; (2) os jobs que
# de fato configuram a árvore principal (contêm "cmake -S "), lidos ao
# vivo de .github/workflows/ci.yml, cada um com sua família de distro
# (Fedora/dnf, Ubuntu/apt, Arch/CachyOS/pacman - GODS_LAWS.md L-04:
# CachyOS NÃO é coberto pelo job de Arch, família própria abaixo) e o
# texto de instalação que ele roda.
#
# A ÚNICA parte deste portão que não se deriva sozinha - por natureza,
# não por preguiça - é a TABELA módulo -> pacote por família
# (MODULE_PACKAGES abaixo): descobrir que nome de pacote fornece
# `egl.pc` num sistema de pacotes não é algo que se lê do próprio
# cmake/*.cmake. Cada entrada da tabela foi MEDIDA ao vivo (não
# suposta) contra a imagem EXATA que .github/workflows/ci.yml usa,
# 06/09/2026, GODS_LAWS.md L-44:
#   - fedora:latest  -> `rpm -qf $(rpm -ql libglvnd-devel|grep egl.pc)`
#                        = libglvnd-devel; `rpm -qf $(rpm -ql
#                        wayland-devel|grep wayland-egl.pc)` =
#                        wayland-devel (já instalado antes desta fatia).
#   - ubuntu:latest  -> `dpkg -S egl.pc` = libegl-dev; `dpkg -S
#                        wayland-egl.pc` = libwayland-dev (já instalado).
#   - archlinux:latest -> `pacman -Qo /usr/lib/pkgconfig/egl.pc` =
#                        libglvnd; `pacman -Qo .../wayland-egl.pc` =
#                        wayland (já instalado).
#   - cachyos/cachyos:latest -> mesmo pacman -Qo, mesmo resultado
#                        (repositório "extra" espelhado, medido com o
#                        pacman -Si daquele container: mesma versão
#                        1.7.0-3 do Arch, GODS_LAWS.md L-04: medido,
#                        não presumido igual só por CachyOS ser
#                        Arch-based).
# Se um módulo REQUIRED novo aparecer sem entrada nesta tabela para
# alguma família, o portão REPROVA nomeando o módulo e a família em
# vez de assumir cobertura (GODS_LAWS.md L-40: varredura que não sabe
# a resposta não inventa "ok").
#
# GATE-TREE-PARITY (mesma forma de check_dup_laws.py/check_no_x11.py):
# scanner de texto puro, sem executar nada e sem depender de PyYAML
# (ausente por padrão nos containers minimalistas deste projeto,
# GODS_LAWS.md L-51 - instalar um pacote python só para este portão
# seria o mesmo tipo de "conserto" frágil que ele existe para
# detectar) - por isso roda igual nos cinco alvos e não é
# if(UNIX)-guardado em tests/CMakeLists.txt.
#
# Usage:
#   check_pkg_dep_coverage.py <repo-root-directory>
#   check_pkg_dep_coverage.py --selftest
#
# --selftest roda quatro controles (positivo, negativo - pacote
# faltando num job real, varredura vazia, e módulo sem entrada na
# tabela) contra fixtures descartáveis sob tempfile.mkdtemp, nunca
# contra a árvore rastreada real.
#
# Each function below does one thing (GODS_LAWS.md L-17).

import glob
import os
import re
import shutil
import sys
import tempfile

SCRIPT_NAME = "check_pkg_dep_coverage.py"

# Tabela medida ao vivo - ver o cabecalho acima para a fonte de cada
# linha. Uma familia por gerenciador de pacote real (GODS_LAWS.md
# L-04: CachyOS tem entrada PROPRIA, nunca reaproveita "arch").
MODULE_PACKAGES = {
    "wayland-client": {
        "fedora": ["wayland-devel"],
        "ubuntu": ["libwayland-dev"],
        "arch": ["wayland"],
        "cachyos": ["wayland"],
    },
    "wayland-egl": {
        "fedora": ["wayland-devel"],
        "ubuntu": ["libwayland-dev"],
        "arch": ["wayland"],
        "cachyos": ["wayland"],
    },
    "egl": {
        "fedora": ["libglvnd-devel", "mesa-libegl-devel"],
        "ubuntu": ["libegl-dev"],
        "arch": ["libglvnd"],
        "cachyos": ["libglvnd"],
    },
}

_PKG_CHECK_MODULES_RE = re.compile(
    r"pkg_check_modules\(\s*\S+\s+REQUIRED\s+([^)]*)\)", re.DOTALL
)
_JOB_KEY_RE = re.compile(r"^  ([A-Za-z_][\w-]*):\s*$", re.MULTILINE)
_MATRIX_ENTRY_RE = re.compile(r"^ {10}- nome:.*$", re.MULTILINE)
_IMAGEM_RE = re.compile(r"imagem:\s*(\S+)")
_INSTALAR_BLOCK_RE = re.compile(r"instalar:\s*>-\n((?:^ {14}.*\n)+)", re.MULTILINE)
_CONTAINER_RE = re.compile(r"^\s*container:\s*(\S+)\s*$", re.MULTILINE)

# "Configura a arvore principal" tem DUAS formas neste ci.yml, e as
# duas precisam ser reconhecidas ou o portao cega para 3 dos 14 job-
# runs reais (achado ao vivo, primeira rodada deste portao contra o
# commit 40c2622: so pegou 9 dos 14 vermelhos, porque `lint`/
# `sanitizer`/`debug` nunca escrevem "cmake -S ." no proprio ci.yml -
# delegam para tools/preci.sh, que configura por dentro,
# `cmake -S "$ROOT_DIR" -B ...`). `tools/preci.sh --selftest` sozinho
# e' a UNICA excecao: opera so sobre tests/preci_fixtures/, nunca sobre
# a arvore principal (o proprio cabecalho do script documenta isso) -
# por isso o padrao abaixo exige um dos tres flags REAIS depois de
# preci.sh, nunca so a palavra "preci.sh".
_CMAKE_CONFIGURE_RE = re.compile(
    r"cmake -S \.|tools/preci\.sh --(?:lint-only|sanitizer-only|debug-only|fast)\b"
)
_PKG_MANAGER_INSTALL_RE = re.compile(
    r"(?:dnf -y install|apt-get install -y|pacman -Syu? --noconfirm)(.*?)(?=\n\n|- name:|\Z)",
    re.DOTALL,
)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- lado 1: o que cmake/*.cmake EXIGE ------------------------------------


def required_pkgconfig_modules(cmake_dir):
    """Varre todo cmake/*.cmake por pkg_check_modules(... REQUIRED ...)
    e devolve {modulo: [arquivo, ...]} - a proveniencia de cada modulo
    (qual arquivo o exige) entra no relatorio, nunca so o nome.
    """
    modules = {}
    for path in sorted(glob.glob(os.path.join(cmake_dir, "*.cmake"))):
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        for match in _PKG_CHECK_MODULES_RE.finditer(text):
            for name in match.group(1).split():
                modules.setdefault(name, []).append(os.path.basename(path))
    return modules


# --- lado 2: o que .github/workflows/ci.yml INSTALA -----------------------


def distro_family(image):
    if "fedora" in image:
        return "fedora"
    if "cachyos" in image:
        return "cachyos"
    if "archlinux" in image:
        return "arch"
    if "ubuntu" in image:
        return "ubuntu"
    return None


def job_chunks(ci_yml_text):
    """Corta o arquivo em blocos por job de topo (chave com 2 espacos de
    indentacao sob `jobs:`), do inicio de uma chave ate o inicio da
    proxima (ou fim de arquivo) - mesma tecnica de fronteira por
    ancora textual que check_dup_laws.py ja usa para blocos DUP-BLOCK.
    """
    starts = [(m.start(), m.group(1)) for m in _JOB_KEY_RE.finditer(ci_yml_text)]
    chunks = []
    for i, (start, name) in enumerate(starts):
        end = starts[i + 1][0] if i + 1 < len(starts) else len(ci_yml_text)
        chunks.append((name, ci_yml_text[start:end]))
    return chunks


def linux_matrix_entries(linux_job_text):
    """Corta o job `linux` (matrix.include) em uma entrada por `- nome:`,
    cada uma com sua propria imagem e string de instalacao - as oito
    entradas hoje (quatro distros x dois modos) nao sao hardcoded
    aqui, o corte segue as ancoras `- nome:` que ja existem no arquivo.
    """
    starts = [m.start() for m in _MATRIX_ENTRY_RE.finditer(linux_job_text)]
    entries = []
    for i, start in enumerate(starts):
        end = starts[i + 1] if i + 1 < len(starts) else len(linux_job_text)
        entries.append(linux_job_text[start:end])
    return entries


def configure_targets(ci_yml_text):
    """Devolve uma lista de (rotulo, familia, texto_de_instalacao) - um
    item por job/entrada de matriz que de fato configura a arvore
    principal (contem "cmake -S ."). Job sem essa marca (gitleaks,
    leis, wayland-container - que builda via Containerfile separado,
    nunca via este CMakeLists.txt) fica de fora por construcao, nao
    por lista escrita a mao.
    """
    targets = []
    for job_name, chunk in job_chunks(ci_yml_text):
        if job_name == "linux":
            for entry in linux_matrix_entries(chunk):
                if not _CMAKE_CONFIGURE_RE.search(entry) and not _CMAKE_CONFIGURE_RE.search(chunk):
                    continue
                imagem_match = _IMAGEM_RE.search(entry)
                instalar_match = _INSTALAR_BLOCK_RE.search(entry)
                if not imagem_match or not instalar_match:
                    continue
                nome_match = re.search(r"- nome:\s*(.+)$", entry, re.MULTILINE)
                label = f"linux/{nome_match.group(1).strip() if nome_match else '?'}"
                family = distro_family(imagem_match.group(1))
                targets.append((label, family, instalar_match.group(1)))
            continue

        if not _CMAKE_CONFIGURE_RE.search(chunk):
            continue
        container_match = _CONTAINER_RE.search(chunk)
        if not container_match:
            # Job sem `container:` que configura a arvore principal e'
            # o runner nativo (Windows) - GlintfxEgl.cmake so entra sob
            # if(UNIX) (CMakeLists.txt), entao nenhum modulo desta
            # tabela se aplica a ele; nao e' lacuna, e' fora do escopo
            # da lei L-05/L-07 que esta tabela cobre.
            continue
        family = distro_family(container_match.group(1))
        install_match = _PKG_MANAGER_INSTALL_RE.search(chunk)
        install_text = install_match.group(1) if install_match else ""
        targets.append((f"job/{job_name}", family, install_text))
    return targets


def install_tokens(install_text):
    return set(re.split(r"[\s\\]+", install_text.strip()))


# --- cruzamento -------------------------------------------------------


def check_coverage(cmake_dir, ci_yml_text):
    """Devolve (ok: bool, relatorio: str). Nunca declara sucesso sem
    contar (GODS_LAWS.md L-40): imprime encontrados/conferidos/
    faltando sempre, mesmo quando tudo bate.
    """
    modules = required_pkgconfig_modules(cmake_dir)
    if not modules:
        return False, "varredura vazia: nenhum pkg_check_modules(... REQUIRED ...) encontrado em cmake/*.cmake"

    targets = configure_targets(ci_yml_text)
    if not targets:
        return False, "varredura vazia: nenhum job/entrada de matriz configura a arvore principal (\"cmake -S .\") em .github/workflows/ci.yml"

    lines = []
    lines.append(f"modulos REQUIRED encontrados: {len(modules)}")
    for name, sources in sorted(modules.items()):
        lines.append(f"  - {name} (exigido por: {', '.join(sources)})")
    lines.append(f"alvos conferidos: {len(targets)}")

    missing = []
    for label, family, install_text in targets:
        tokens = install_tokens(install_text)
        if family is None:
            missing.append((label, None, "familia de distro desconhecida (imagem nao mapeada em distro_family())"))
            continue
        for module_name in modules:
            table_entry = MODULE_PACKAGES.get(module_name, {}).get(family)
            if not table_entry:
                missing.append((label, module_name, f"MODULE_PACKAGES nao tem entrada para modulo '{module_name}' na familia '{family}'"))
                continue
            if not any(pkg in tokens for pkg in table_entry):
                missing.append((label, module_name, f"nenhum de {table_entry} presente na instalacao de {label} ({family})"))
        lines.append(f"  - {label} ({family}): {len(tokens)} pacote(s) na linha de instalacao")

    lines.append(f"faltando: {len(missing)}")
    for label, module_name, detail in missing:
        lines.append(f"  - {label} / {module_name}: {detail}")

    report = "\n".join(lines)
    return (len(missing) == 0), report


# --- modo real -----------------------------------------------------------


def real_main(args):
    if len(args) != 1:
        fail("usage: check_pkg_dep_coverage.py <repo-root-directory>")
    root = args[0]
    if not os.path.isdir(root):
        fail(f"directory not found: {root}")
    cmake_dir = os.path.join(root, "cmake")
    ci_yml = os.path.join(root, ".github", "workflows", "ci.yml")
    if not os.path.isdir(cmake_dir):
        fail(f"diretorio nao encontrado: {cmake_dir}")
    if not os.path.isfile(ci_yml):
        fail(f"arquivo nao encontrado: {ci_yml}")
    with open(ci_yml, "r", encoding="utf-8", errors="replace") as handle:
        ci_yml_text = handle.read()

    ok, report = check_coverage(cmake_dir, ci_yml_text)
    print(report)
    if not ok:
        fail("cobertura de pacote de sistema incompleta (ver 'faltando' acima)")


# --- fixtures e controles do --selftest -----------------------------------


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-pkg-dep-coverage-selftest-", dir=os.environ.get("TMPDIR"))


def _make_capture():
    import contextlib
    import io

    class _Captured:
        __slots__ = ("result", "text")

        def __init__(self, result, text):
            self.result = result
            self.text = text

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


_FIXTURE_CI_YML = """\
jobs:
  linux:
    strategy:
      matrix:
        include:
          - nome: Fedora (primario) - compartilhado
            imagem: fedora:latest
            instalar: >-
              dnf -y install gcc-c++ cmake wayland-devel {fedora_egl}
            modo: compartilhado
    steps:
      - name: Build
        run: |
          cmake -S . -B build

  gitleaks:
    runs-on: ubuntu-latest
    steps:
      - name: gitleaks detect
        run: gitleaks detect --no-banner --source .
"""


def _write_fixture(scratch, fedora_egl):
    cmake_dir = os.path.join(scratch, "cmake")
    os.makedirs(cmake_dir, exist_ok=True)
    with open(os.path.join(cmake_dir, "GlintfxEgl.cmake"), "w", encoding="utf-8") as handle:
        handle.write("pkg_check_modules(GlintfxEgl REQUIRED egl wayland-egl)\n")
    workflows_dir = os.path.join(scratch, ".github", "workflows")
    os.makedirs(workflows_dir, exist_ok=True)
    ci_yml_path = os.path.join(workflows_dir, "ci.yml")
    with open(ci_yml_path, "w", encoding="utf-8") as handle:
        handle.write(_FIXTURE_CI_YML.format(fedora_egl=fedora_egl))
    return ci_yml_path


# Positive control: instalacao ja cobre egl (via libglvnd-devel) e
# wayland-egl (via wayland-devel, ja presente). Esperado: aprova.
def selftest_positive_control(scratch, capture):
    fixture_dir = os.path.join(scratch, "positive")
    os.makedirs(fixture_dir, exist_ok=True)
    ci_yml_path = _write_fixture(fixture_dir, "libglvnd-devel")
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(os.path.join(fixture_dir, "cmake"), ci_text))
    ok, report = outcome.result
    if ok and "faltando: 0" in report:
        print("selftest: controle POSITIVO OK (cobertura completa aprovada)")
        return True
    print("selftest: controle POSITIVO FALHOU", file=sys.stderr)
    print(report, file=sys.stderr)
    return False


# Negative control: a mesma fixture, mas SEM o pacote que fornece
# egl.pc - reproduz exatamente a regressao real de 06/09/2026.
# Esperado: reprova, citando o modulo 'egl' e o rotulo do job.
def selftest_negative_control(scratch, capture):
    fixture_dir = os.path.join(scratch, "negative")
    os.makedirs(fixture_dir, exist_ok=True)
    ci_yml_path = _write_fixture(fixture_dir, "")
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(os.path.join(fixture_dir, "cmake"), ci_text))
    ok, report = outcome.result
    if ok:
        print("selftest: controle NEGATIVO FALHOU (pacote de egl faltando foi aprovado)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    if "egl" not in report or "linux/Fedora" not in report:
        print("selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou o modulo/job certo)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (pacote de egl faltando detectado e citado)")
    return True


# Varredura vazia: cmake/ sem nenhum pkg_check_modules(... REQUIRED
# ...). Esperado: reprova citando "varredura vazia".
def selftest_empty_scan_control(scratch, capture):
    fixture_dir = os.path.join(scratch, "empty")
    cmake_dir = os.path.join(fixture_dir, "cmake")
    os.makedirs(cmake_dir, exist_ok=True)
    with open(os.path.join(cmake_dir, "GlintfxOptions.cmake"), "w", encoding="utf-8") as handle:
        handle.write("option(GLINTFX_WERROR \"\" OFF)\n")

    outcome = capture(lambda: check_coverage(cmake_dir, _FIXTURE_CI_YML.format(fedora_egl="libglvnd-devel")))
    ok, report = outcome.result
    if ok:
        print("selftest: controle de VARREDURA VAZIA FALHOU (cmake/ sem modulo nenhum deveria reprovar)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    if "varredura vazia" not in report:
        print("selftest: controle de VARREDURA VAZIA FALHOU (reprovou, mas nao disse 'varredura vazia')", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK")
    return True


# Modulo sem entrada na tabela: pkg_check_modules exige um modulo novo
# que MODULE_PACKAGES nao conhece para nenhuma familia. Esperado:
# reprova nomeando o modulo, nunca assume cobertura por omissao (a
# mesma classe de defeito desta lei, um nivel acima).
def selftest_unmapped_module_control(scratch, capture):
    fixture_dir = os.path.join(scratch, "unmapped")
    cmake_dir = os.path.join(fixture_dir, "cmake")
    os.makedirs(cmake_dir, exist_ok=True)
    with open(os.path.join(cmake_dir, "GlintfxFutura.cmake"), "w", encoding="utf-8") as handle:
        handle.write("pkg_check_modules(GlintfxFutura REQUIRED libpulse)\n")

    outcome = capture(lambda: check_coverage(cmake_dir, _FIXTURE_CI_YML.format(fedora_egl="libglvnd-devel")))
    ok, report = outcome.result
    if ok:
        print("selftest: controle de MODULO SEM TABELA FALHOU (modulo desconhecido foi aprovado)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    if "libpulse" not in report:
        print("selftest: controle de MODULO SEM TABELA FALHOU (reprovou, mas nao citou 'libpulse')", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    print("selftest: controle de MODULO SEM TABELA OK (modulo desconhecido reprovado, nunca assumido coberto)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_unmapped_module_control(scratch, capture),
        ]
        if not all(controls):
            print("check_pkg_dep_coverage.py --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"check_pkg_dep_coverage.py --selftest: os {len(controls)} controles OK")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
