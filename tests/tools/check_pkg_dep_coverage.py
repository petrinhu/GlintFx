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
# instala é escrita à mão em .github/workflows/ci.yml (ou no script de
# preparo), e nada confere essa lista contra o que cmake/*.cmake
# REALMENTE exige via pkg_check_modules(). Este portão fecha essa
# lacuna derivando os módulos REQUIRED ao vivo de cmake/*.cmake.
#
# D-A12 PARTE 4 (revisão do CTO, docs/plano-ci-split-per-os.md secao
# 4.4, aplicada em c4dece0): a tradução módulo pkg-config -> pacote de
# sistema NÃO mora mais numa tabela Python (MODULE_PACKAGES, removida
# nesta fatia) - mora no PRÓPRIO script que instala, cada
# tools/ci/env/<slug>.sh, em linhas legíveis por MÁQUINA (lidas como
# texto, nunca executadas):
#   # glintfx-pkgconfig: <modulo>=<pacote>
# Cada script declara só o que ELE realmente instala. O portão confere
# duas coisas, nunca uma tabela solta desalinhada do que se instala de
# verdade: (A) todo módulo REQUIRED tem declaração no script de CADA
# sistema da família linux (fonte: tools/ci/systems.txt); (B) o pacote
# declarado aparece de fato na linha de instalação do alvo (o próprio
# script, para as entradas do job `linux`; a linha inline do job, para
# lint/sanitizer/debug/clang, que continuam instalando via YAML).
#
# distro_family(image) TAMBÉM saiu (era "if 'fedora' in image", a
# mesma classe de hardcode que a D-A12 pediu para eliminar). O slug de
# um job SEM matrix próprio (lint/sanitizer/debug/clang, container:
# fedora:latest) é resolvido por correspondência EXATA entre esse
# `container:` e o `imagem:` de alguma entrada da matriz do job
# `linux` - fonte única, nunca comparação de substring por nome de
# distro.
#
# GATE-TREE-PARITY (mesma forma de check_dup_laws.py/check_no_x11.py):
# scanner de texto puro, sem executar nada e sem depender de PyYAML
# (ausente por padrão nos containers minimalistas deste projeto,
# GODS_LAWS.md L-51 - instalar um pacote python só para este portão
# seria o mesmo tipo de "conserto" frágil que ele existe para
# detectar) - por isso roda igual nos alvos e não é if(UNIX)-guardado
# em tests/CMakeLists.txt. ci_systems.py (biblioteca já usada pelos
# portões irmãos da A3a) é importado para ler tools/ci/systems.txt -
# não é PyYAML, é um parser de texto próprio do projeto.
#
# Usage:
#   check_pkg_dep_coverage.py <repo-root-directory>
#   check_pkg_dep_coverage.py --selftest
#
# Each function below does one thing (GODS_LAWS.md L-17).

import glob
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ci_systems  # noqa: E402

SCRIPT_NAME = "check_pkg_dep_coverage.py"

_PKG_CHECK_MODULES_RE = re.compile(
    r"pkg_check_modules\(\s*\S+\s+REQUIRED\s+([^)]*)\)", re.DOTALL
)
_JOB_KEY_RE = re.compile(r"^  ([A-Za-z_][\w-]*):\s*$", re.MULTILINE)
_MATRIX_ENTRY_RE = re.compile(r"^ {10}- nome:.*$", re.MULTILINE)
_IMAGEM_RE = re.compile(r"imagem:\s*(\S+)")
_SLUG_RE = re.compile(r"^\s*slug:\s*(\S+)\s*$", re.MULTILINE)
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
# DOIS FORMATOS DISTINTOS, DOIS REGEX: o texto do proprio ci.yml
# continua com `run: >-` (YAML folded block scalar - linhas continuam
# por INDENTACAO, sem "\"). Os scripts tools/ci/env/*.sh sao BASH puro,
# onde continuacao e' "\" explicito no fim da linha - a mesma forma
# generica aplicada a um script com logica DEPOIS do comando (tools/ci/
# env/cachyos.sh) capturava CODIGO INTEIRO alem dele (medido: 79
# "pacotes", incluindo o corpo do laco de retry - so' nao quebrava por
# sorte, os tokens reais continuavam la dentro do lixo).
_PKG_MANAGER_INSTALL_RE = re.compile(
    r"(?:dnf -y install|apt-get install -y|pacman -Syu? --noconfirm)(.*?)(?=\n\n|- name:|\Z)",
    re.DOTALL,
)
_PKG_MANAGER_INSTALL_BASH_RE = re.compile(
    r"(?:dnf -y install|apt-get install -y|pacman -Syu? --noconfirm)"
    r"((?:[ \t]*[^\n]*\\\n)*[ \t]*[^\n>]*)"
)

# D-A12 parte 4: a anotacao legivel por maquina que cada tools/ci/env/
# <slug>.sh carrega, uma linha por (modulo, pacote). Lida como TEXTO -
# o script nunca e' executado por este portao.
_PKGCONFIG_ANNOTATION_RE = re.compile(
    r"^\s*#\s*glintfx-pkgconfig:\s*(\S+)=(\S+)\s*$", re.MULTILINE
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


# --- lado 2: o que cada alvo/script INSTALA e DECLARA ----------------------


def _read_env_script_text(env_dir, slug):
    """Le tools/ci/env/<slug>.sh cru. Devolve None se o script nao
    existir (o chamador decide como reportar a ausencia - nunca
    assumida coberta, GODS_LAWS.md L-40)."""
    if env_dir is None:
        return None
    script_path = os.path.join(env_dir, f"{slug}.sh")
    if not os.path.isfile(script_path):
        return None
    with open(script_path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def install_text_from_script(script_text):
    """Extrai o texto do comando de instalacao (dnf/apt-get/pacman) de
    um script bash real - regex de continuacao por "\\", nunca o
    generico de YAML (ver cabecalho, defeito medido do vazamento)."""
    match = _PKG_MANAGER_INSTALL_BASH_RE.search(script_text)
    return match.group(1) if match else ""


def parse_pkgconfig_annotations(script_text):
    """Devolve {modulo: [pacote, ...]} a partir das linhas
    '# glintfx-pkgconfig: <modulo>=<pacote>' do script - dado da
    PROPRIA distro, nunca hardcoded aqui (D-A12 parte 4)."""
    annotations = {}
    for module_name, package in _PKGCONFIG_ANNOTATION_RE.findall(script_text):
        annotations.setdefault(module_name, []).append(package)
    return annotations


def install_tokens(install_text):
    return set(re.split(r"[\s\\]+", install_text.strip()))


# --- alvos que configuram a arvore principal, e o slug de cada um ----------


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
    cada uma com sua propria imagem/slug - as entradas de hoje nao sao
    hardcoded aqui, o corte segue as ancoras `- nome:` que ja existem
    no arquivo.
    """
    starts = [m.start() for m in _MATRIX_ENTRY_RE.finditer(linux_job_text)]
    entries = []
    for i, start in enumerate(starts):
        end = starts[i + 1] if i + 1 < len(starts) else len(linux_job_text)
        entries.append(linux_job_text[start:end])
    return entries


def build_image_to_slug(ci_yml_text):
    """Devolve {imagem: slug}, construido das entradas da matriz do
    job `linux` - fonte UNICA para resolver a familia de um job SEM
    matrix propria (lint/sanitizer/debug/clang), nunca mais por
    substring de nome de distro (distro_family() removida, D-A12
    parte 4)."""
    mapping = {}
    for job_name, chunk in job_chunks(ci_yml_text):
        if job_name != "linux":
            continue
        for entry in linux_matrix_entries(chunk):
            imagem_match = _IMAGEM_RE.search(entry)
            slug_match = _SLUG_RE.search(entry)
            if imagem_match and slug_match:
                mapping[imagem_match.group(1)] = slug_match.group(1)
    return mapping


def configure_targets(ci_yml_text, env_dir, image_to_slug):
    """Devolve uma lista de (rotulo, slug_or_None, texto_de_instalacao)
    - um item por job/entrada de matriz que de fato configura a arvore
    principal (contem "cmake -S ."). Job sem essa marca (gitleaks,
    leis, wayland-container - que builda via Containerfile separado,
    nunca via este CMakeLists.txt) fica de fora por construcao, nao
    por lista escrita a mao.

    Job `linux`: slug vem de `slug:` na propria entrada, texto de
    instalacao vem do script (env_dir/<slug>.sh) - e' quem de fato
    instala. Demais jobs (lint/sanitizer/debug/clang): slug resolvido
    via `image_to_slug` (build_image_to_slug(), construido da matriz
    linux); texto de instalacao vem da linha INLINE do proprio job -
    esses jobs continuam instalando por YAML, nao por script.
    """
    targets = []
    for job_name, chunk in job_chunks(ci_yml_text):
        if job_name == "linux":
            for entry in linux_matrix_entries(chunk):
                if not _CMAKE_CONFIGURE_RE.search(entry) and not _CMAKE_CONFIGURE_RE.search(chunk):
                    continue
                slug_match = _SLUG_RE.search(entry)
                if not slug_match:
                    continue
                nome_match = re.search(r"- nome:\s*(.+)$", entry, re.MULTILINE)
                label = f"linux/{nome_match.group(1).strip() if nome_match else '?'}"
                slug = slug_match.group(1)
                script_text = _read_env_script_text(env_dir, slug)
                if script_text is None:
                    targets.append((label, slug, None))
                    continue
                targets.append((label, slug, install_text_from_script(script_text)))
            continue

        if not _CMAKE_CONFIGURE_RE.search(chunk):
            continue
        container_match = _CONTAINER_RE.search(chunk)
        if not container_match:
            # Job sem `container:` que configura a arvore principal e'
            # o runner nativo (Windows) - GlintfxEgl.cmake so entra sob
            # if(UNIX) (CMakeLists.txt), entao nenhum modulo desta
            # verificacao se aplica a ele; nao e' lacuna, e' fora do
            # escopo da lei L-05/L-07 que esta verificacao cobre.
            continue
        imagem = container_match.group(1)
        slug = image_to_slug.get(imagem)
        install_match = _PKG_MANAGER_INSTALL_RE.search(chunk)
        install_text = install_match.group(1) if install_match else ""
        targets.append((f"job/{job_name}", slug, install_text))
    return targets


# --- cruzamento -------------------------------------------------------


def check_coverage(cmake_dir, ci_yml_text, env_dir, systems):
    """Devolve (ok: bool, relatorio: str). Nunca declara sucesso sem
    contar (GODS_LAWS.md L-40): imprime encontrados/conferidos/
    faltando sempre, mesmo quando tudo bate.

    Duas verificacoes (D-A12 parte 4):
    (A) todo slug de familia linux (tools/ci/systems.txt) tem script,
        o script declara (glintfx-pkgconfig) todo modulo REQUIRED, e
        cada pacote declarado esta de fato na instalacao DO PROPRIO
        SCRIPT.
    (B) todo alvo que configura a arvore (job `linux` e os demais,
        lint/sanitizer/debug/clang) tem slug resolvido, e cada pacote
        que o script do seu slug declara para um modulo REQUIRED esta
        de fato na instalacao DESTE ALVO especifico (para os jobs fora
        da matriz, a instalacao inline pode divergir da do script).
    """
    modules = required_pkgconfig_modules(cmake_dir)
    if not modules:
        return False, "varredura vazia: nenhum pkg_check_modules(... REQUIRED ...) encontrado em cmake/*.cmake"

    linux_slugs = sorted(ci_systems.slugs_of_family(systems, "linux"))
    if not linux_slugs:
        return False, "varredura vazia: nenhum slug de familia linux em tools/ci/systems.txt"

    lines = []
    lines.append(f"modulos REQUIRED encontrados: {len(modules)}")
    for name, sources in sorted(modules.items()):
        lines.append(f"  - {name} (exigido por: {', '.join(sources)})")

    missing = []
    annotations_by_slug = {}

    # --- verificacao (A): por slug de familia linux ---
    lines.append(f"slugs de familia linux conferidos: {len(linux_slugs)}")
    for slug in linux_slugs:
        script_text = _read_env_script_text(env_dir, slug)
        if script_text is None:
            missing.append((f"script/{slug}", None, f"tools/ci/env/{slug}.sh nao encontrado (slug de familia linux em tools/ci/systems.txt)"))
            continue
        annotations = parse_pkgconfig_annotations(script_text)
        annotations_by_slug[slug] = annotations
        script_tokens = install_tokens(install_text_from_script(script_text))
        for module_name in modules:
            declared = annotations.get(module_name)
            if not declared:
                missing.append((f"script/{slug}", module_name, f"tools/ci/env/{slug}.sh nao declara 'glintfx-pkgconfig: {module_name}=...'"))
                continue
            for package in declared:
                if package not in script_tokens:
                    missing.append((f"script/{slug}", module_name, f"declara pacote '{package}' mas ele nao esta na propria instalacao de tools/ci/env/{slug}.sh"))
        lines.append(f"  - {slug}: {len(annotations)} modulo(s) declarado(s), {len(script_tokens)} pacote(s) instalado(s)")

    # --- verificacao (B): por alvo que configura a arvore ---
    image_to_slug = build_image_to_slug(ci_yml_text)
    targets = configure_targets(ci_yml_text, env_dir, image_to_slug)
    if not targets:
        return False, "varredura vazia: nenhum job/entrada de matriz configura a arvore principal (\"cmake -S .\") em .github/workflows/ci.yml"

    lines.append(f"alvos conferidos: {len(targets)}")
    for label, slug, install_text in targets:
        if slug is None:
            missing.append((label, None, "imagem do container nao corresponde a nenhuma entrada da matriz do job 'linux' (build_image_to_slug())"))
            continue
        if install_text is None:
            missing.append((label, None, f"tools/ci/env/{slug}.sh nao encontrado para este alvo"))
            continue
        tokens = install_tokens(install_text)
        annotations = annotations_by_slug.get(slug)
        if annotations is None:
            script_text = _read_env_script_text(env_dir, slug)
            annotations = parse_pkgconfig_annotations(script_text) if script_text else {}
        for module_name in modules:
            declared = annotations.get(module_name)
            if not declared:
                continue  # ja reportado pela verificacao (A), nao duplica
            if not any(package in tokens for package in declared):
                missing.append((label, module_name, f"nenhum de {declared} (declarado por tools/ci/env/{slug}.sh) presente na instalacao de {label}"))
        lines.append(f"  - {label} ({slug}): {len(tokens)} pacote(s) na linha de instalacao")

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
    env_dir = os.path.join(root, "tools", "ci", "env")
    systems_path = os.path.join(root, "tools", "ci", "systems.txt")
    if not os.path.isdir(cmake_dir):
        fail(f"diretorio nao encontrado: {cmake_dir}")
    if not os.path.isfile(ci_yml):
        fail(f"arquivo nao encontrado: {ci_yml}")
    if not os.path.isdir(env_dir):
        fail(f"diretorio nao encontrado: {env_dir}")
    if not os.path.isfile(systems_path):
        fail(f"arquivo nao encontrado: {systems_path}")
    with open(ci_yml, "r", encoding="utf-8", errors="replace") as handle:
        ci_yml_text = handle.read()
    try:
        systems = ci_systems.parse_systems_text(
            Path(systems_path).read_text(encoding="utf-8"), source_label=systems_path
        )
    except ci_systems.CiSystemsError as exc:
        fail(str(exc))

    ok, report = check_coverage(cmake_dir, ci_yml_text, env_dir, systems)
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


_FAKE_SYSTEMS = {"fedora": "linux", "windows": "windows"}

# A matriz do job `linux` so' tem `slug:`, sem `instalar:` - o texto de
# instalacao vem de env_dir/<slug>.sh (fixture propria, escrita por
# _write_fixture abaixo). O `lint` fixture, fora da matriz, resolve o
# slug por container: fedora:latest == imagem: fedora:latest.
_FIXTURE_CI_YML = """\
jobs:
  linux:
    strategy:
      matrix:
        include:
          - nome: Fedora - compartilhado
            slug: fedora
            imagem: fedora:latest
            modo: compartilhado
    steps:
      - name: Build
        run: |
          cmake -S . -B build

  lint:
    container: fedora:latest
    steps:
      - name: Instalar toolchain e ferramentas de lint
        run: >-
          dnf -y install gcc-c++ cmake wayland-devel {lint_egl}

      - name: Build
        run: |
          cmake -S . -B build

  gitleaks:
    runs-on: ubuntu-latest
    steps:
      - name: gitleaks detect
        run: gitleaks detect --no-banner --source .
"""

_FIXTURE_ENV_SCRIPT = """\
#!/usr/bin/env bash
set -eu
# glintfx-pkgconfig: wayland-client={script_egl_module_owner}
# glintfx-pkgconfig: wayland-egl={script_egl_module_owner}
# glintfx-pkgconfig: egl={script_egl}
dnf -y install gcc-c++ cmake wayland-devel {script_egl}
"""


def _write_fixture(scratch, script_egl, lint_egl=None, script_egl_module_owner="wayland-devel"):
    """script_egl: pacote declarado+instalado no SCRIPT para o modulo
    'egl' (vazio = nao instala nada, deixa a declaracao 'orfa' se ainda
    assim for escrita - os testes controlam isso separado). lint_egl:
    o que o job `lint` (fora da matriz) instala inline; por padrao
    igual a script_egl, salvo quando o teste quer fazer os dois
    divergirem (verificacao B)."""
    if lint_egl is None:
        lint_egl = script_egl
    cmake_dir = os.path.join(scratch, "cmake")
    os.makedirs(cmake_dir, exist_ok=True)
    with open(os.path.join(cmake_dir, "GlintfxEgl.cmake"), "w", encoding="utf-8") as handle:
        handle.write("pkg_check_modules(GlintfxEgl REQUIRED egl wayland-egl)\n")
    workflows_dir = os.path.join(scratch, ".github", "workflows")
    os.makedirs(workflows_dir, exist_ok=True)
    ci_yml_path = os.path.join(workflows_dir, "ci.yml")
    with open(ci_yml_path, "w", encoding="utf-8") as handle:
        handle.write(_FIXTURE_CI_YML.format(lint_egl=lint_egl))
    env_dir = os.path.join(scratch, "tools", "ci", "env")
    os.makedirs(env_dir, exist_ok=True)
    with open(os.path.join(env_dir, "fedora.sh"), "w", encoding="utf-8") as handle:
        handle.write(_FIXTURE_ENV_SCRIPT.format(script_egl=script_egl, script_egl_module_owner=script_egl_module_owner))
    return ci_yml_path, env_dir


# Positive control: script declara e instala egl+wayland-*; job `lint`
# instala o mesmo pacote. Esperado: aprova.
def selftest_positive_control(scratch, capture):
    fixture_dir = os.path.join(scratch, "positive")
    os.makedirs(fixture_dir, exist_ok=True)
    ci_yml_path, env_dir = _write_fixture(fixture_dir, "libglvnd-devel")
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(os.path.join(fixture_dir, "cmake"), ci_text, env_dir, _FAKE_SYSTEMS))
    ok, report = outcome.result
    if ok and "faltando: 0" in report:
        print("selftest: controle POSITIVO OK (cobertura completa aprovada)")
        return True
    print("selftest: controle POSITIVO FALHOU", file=sys.stderr)
    print(report, file=sys.stderr)
    return False


# V1 (main, revisao D-A12 parte 4): remover a declaracao de um modulo
# no script reprova, citando o slug e o modulo. Fixture: reescreve o
# script SEM a linha 'glintfx-pkgconfig: egl=...'.
def selftest_missing_annotation_reproves(scratch, capture):
    fixture_dir = os.path.join(scratch, "v1-sem-anotacao")
    os.makedirs(fixture_dir, exist_ok=True)
    ci_yml_path, env_dir = _write_fixture(fixture_dir, "libglvnd-devel")
    script_path = os.path.join(env_dir, "fedora.sh")
    with open(script_path, "r", encoding="utf-8") as handle:
        script_text = handle.read()
    script_sem_egl = script_text.replace("# glintfx-pkgconfig: egl=libglvnd-devel\n", "")
    with open(script_path, "w", encoding="utf-8") as handle:
        handle.write(script_sem_egl)
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(os.path.join(fixture_dir, "cmake"), ci_text, env_dir, _FAKE_SYSTEMS))
    ok, report = outcome.result
    if ok:
        print("selftest: V1-SEM-ANOTACAO FALHOU (modulo sem declaracao foi aprovado)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    if "fedora" not in report or "egl" not in report or "nao declara" not in report:
        print("selftest: V1-SEM-ANOTACAO FALHOU (reprovou, mas nao citou slug/modulo/motivo certos)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    print("selftest: V1-SEM-ANOTACAO OK (modulo sem declaracao detectado, citando o slug)")
    return True


# V2 (main): declarar um pacote que NAO esta na linha de instalacao
# reprova - tanto quando a divergencia e' no PROPRIO SCRIPT (verificacao
# A) quanto quando e' so' no job fora da matriz (verificacao B).
def selftest_declared_package_not_installed_in_script_reproves(scratch, capture):
    fixture_dir = os.path.join(scratch, "v2-script-diverge")
    os.makedirs(fixture_dir, exist_ok=True)
    # declara 'egl=libglvnd-devel' mas o script so' instala outro
    # pacote para o modulo egl (script_egl != o declarado)
    ci_yml_path, env_dir = _write_fixture(fixture_dir, "mesa-libegl-devel")
    script_path = os.path.join(env_dir, "fedora.sh")
    with open(script_path, "r", encoding="utf-8") as handle:
        script_text = handle.read()
    # forca a declaracao a apontar para um pacote que a instalacao NAO tem
    script_divergente = script_text.replace(
        "# glintfx-pkgconfig: egl=mesa-libegl-devel\n", "# glintfx-pkgconfig: egl=libglvnd-devel\n"
    )
    with open(script_path, "w", encoding="utf-8") as handle:
        handle.write(script_divergente)
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(os.path.join(fixture_dir, "cmake"), ci_text, env_dir, _FAKE_SYSTEMS))
    ok, report = outcome.result
    if ok:
        print("selftest: V2-SCRIPT-DIVERGE FALHOU (pacote declarado ausente da instalacao foi aprovado)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    if "libglvnd-devel" not in report or "nao esta na propria instalacao" not in report:
        print("selftest: V2-SCRIPT-DIVERGE FALHOU (reprovou, mas nao citou o pacote/motivo certos)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    print("selftest: V2-SCRIPT-DIVERGE OK (pacote declarado sem instalacao real detectado)")
    return True


# V2b: o script bate consigo mesmo (declara e instala o mesmo pacote),
# mas o job `lint`, FORA da matriz, instala outro - a verificacao (B)
# tem de pegar isso mesmo com a verificacao (A) inteira verde.
def selftest_declared_package_not_installed_in_other_job_reproves(scratch, capture):
    fixture_dir = os.path.join(scratch, "v2b-lint-diverge")
    os.makedirs(fixture_dir, exist_ok=True)
    ci_yml_path, env_dir = _write_fixture(fixture_dir, "libglvnd-devel", lint_egl="mesa-libegl-devel")
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(os.path.join(fixture_dir, "cmake"), ci_text, env_dir, _FAKE_SYSTEMS))
    ok, report = outcome.result
    if ok:
        print("selftest: V2B-LINT-DIVERGE FALHOU (job lint sem o pacote declarado foi aprovado)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    if "job/lint" not in report:
        print("selftest: V2B-LINT-DIVERGE FALHOU (reprovou, mas nao citou 'job/lint')", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    print("selftest: V2B-LINT-DIVERGE OK (job fora da matriz com instalacao divergente do script detectado)")
    return True


# V3 (main, ja existia antes com outro nome): slug de familia linux SEM
# script - nunca pode virar "cobertura ok" por omissao (GODS_LAWS.md
# L-40). Esperado: reprova citando o slug, e NAO e' ignorado.
def selftest_missing_env_script_control(scratch, capture):
    fixture_dir = os.path.join(scratch, "v3-script-ausente")
    os.makedirs(fixture_dir, exist_ok=True)
    ci_yml_path, env_dir = _write_fixture(fixture_dir, "libglvnd-devel")
    os.remove(os.path.join(env_dir, "fedora.sh"))
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(os.path.join(fixture_dir, "cmake"), ci_text, env_dir, _FAKE_SYSTEMS))
    ok, report = outcome.result
    if ok:
        print("selftest: V3-SCRIPT-AUSENTE FALHOU (script fedora.sh ausente foi aprovado)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    if "fedora" not in report or "nao encontrado" not in report:
        print("selftest: V3-SCRIPT-AUSENTE FALHOU (reprovou, mas nao citou o slug/motivo certo)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    print("selftest: V3-SCRIPT-AUSENTE OK (script ausente detectado, nunca assumido coberto)")
    return True


# Regressao (defeito medido, nao suposto): um script bash com logica
# DEPOIS do comando de instalacao (retry, log, etc. - o proprio tools/
# ci/env/cachyos.sh real) nao pode fazer a extracao "vazar" codigo
# alheio para dentro dos tokens de pacote.
def selftest_bash_script_with_trailing_logic_does_not_leak(scratch, capture):
    script_com_logica_depois = """\
#!/usr/bin/env bash
set -eu
pacman -Syu --noconfirm gcc cmake ninja pkgconf git \\
    wayland wayland-protocols libglvnd >"$log" 2>&1
codigo=$?
cat "$log"
if [ "$codigo" -eq 0 ]; then
    echo "sucesso, isto NAO pode virar token de pacote"
fi
"""
    tokens = install_tokens(install_text_from_script(script_com_logica_depois))
    esperado = {"gcc", "cmake", "ninja", "pkgconf", "git", "wayland", "wayland-protocols", "libglvnd"}
    if tokens != esperado:
        print(
            f"selftest: BASH-NAO-VAZA-LOGICA FALHOU (esperava {sorted(esperado)}, veio {sorted(tokens)})",
            file=sys.stderr,
        )
        return False
    print(f"selftest: BASH-NAO-VAZA-LOGICA OK (extraiu exatamente {sorted(tokens)}, sem vazar o corpo do retry)")
    return True


# Varredura vazia: cmake/ sem nenhum pkg_check_modules(... REQUIRED
# ...). Esperado: reprova citando "varredura vazia".
def selftest_empty_scan_control(scratch, capture):
    fixture_dir = os.path.join(scratch, "empty")
    os.makedirs(fixture_dir, exist_ok=True)
    ci_yml_path, env_dir = _write_fixture(fixture_dir, "libglvnd-devel")
    cmake_dir = os.path.join(fixture_dir, "cmake")
    os.remove(os.path.join(cmake_dir, "GlintfxEgl.cmake"))
    with open(os.path.join(cmake_dir, "GlintfxOptions.cmake"), "w", encoding="utf-8") as handle:
        handle.write("option(GLINTFX_WERROR \"\" OFF)\n")
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(cmake_dir, ci_text, env_dir, _FAKE_SYSTEMS))
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


# Modulo sem declaracao em NENHUM script: pkg_check_modules exige um
# modulo novo que nenhum tools/ci/env/*.sh conhece. Esperado: reprova
# nomeando o modulo, nunca assume cobertura por omissao (mesma classe
# de defeito, um nivel acima do V1).
def selftest_unmapped_module_control(scratch, capture):
    fixture_dir = os.path.join(scratch, "unmapped")
    os.makedirs(fixture_dir, exist_ok=True)
    ci_yml_path, env_dir = _write_fixture(fixture_dir, "libglvnd-devel")
    cmake_dir = os.path.join(fixture_dir, "cmake")
    os.remove(os.path.join(cmake_dir, "GlintfxEgl.cmake"))
    with open(os.path.join(cmake_dir, "GlintfxFutura.cmake"), "w", encoding="utf-8") as handle:
        handle.write("pkg_check_modules(GlintfxFutura REQUIRED libpulse)\n")
    with open(ci_yml_path, "r", encoding="utf-8") as handle:
        ci_text = handle.read()

    outcome = capture(lambda: check_coverage(cmake_dir, ci_text, env_dir, _FAKE_SYSTEMS))
    ok, report = outcome.result
    if ok:
        print("selftest: controle de MODULO SEM DECLARACAO FALHOU (modulo desconhecido foi aprovado)", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    if "libpulse" not in report:
        print("selftest: controle de MODULO SEM DECLARACAO FALHOU (reprovou, mas nao citou 'libpulse')", file=sys.stderr)
        print(report, file=sys.stderr)
        return False
    print("selftest: controle de MODULO SEM DECLARACAO OK (modulo desconhecido reprovado, nunca assumido coberto)")
    return True


# resolucao de slug via imagem, controle positivo: build_image_to_slug()
# resolve 'fedora:latest' (container do job `lint`) para o slug
# 'fedora' por correspondencia EXATA com a matriz do job `linux` - a
# fixture positiva ja prova isso (job/lint aparece no relatorio); este
# controle prova o CONTRARIO: imagem sem entrada na matriz reprova, sem
# adivinhar por substring.
def selftest_unknown_image_reproves():
    ci_yml_imagem_desconhecida = _FIXTURE_CI_YML.format(lint_egl="libglvnd-devel").replace(
        "  lint:\n    container: fedora:latest\n", "  lint:\n    container: rockylinux:9\n"
    )
    image_to_slug = build_image_to_slug(ci_yml_imagem_desconhecida)
    targets = configure_targets(ci_yml_imagem_desconhecida, None, image_to_slug)
    lint_targets = [t for t in targets if t[0] == "job/lint"]
    if not lint_targets or lint_targets[0][1] is not None:
        print(f"selftest: IMAGEM-DESCONHECIDA FALHOU (esperava slug None para 'job/lint'): {lint_targets}", file=sys.stderr)
        return False
    print("selftest: IMAGEM-DESCONHECIDA OK (imagem sem entrada na matriz nao resolve slug, nunca adivinha)")
    return True


def selftest_main():
    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_missing_annotation_reproves(scratch, capture),
            selftest_declared_package_not_installed_in_script_reproves(scratch, capture),
            selftest_declared_package_not_installed_in_other_job_reproves(scratch, capture),
            selftest_missing_env_script_control(scratch, capture),
            selftest_bash_script_with_trailing_logic_does_not_leak(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_unmapped_module_control(scratch, capture),
            selftest_unknown_image_reproves(),
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
