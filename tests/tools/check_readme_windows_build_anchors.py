#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_readme_windows_build_anchors.py - README-WIN-DRIFT-GAP
# (GODS_LAWS.md L-36/L-40, TODO.md INBOX, achado correlato do inventario
# de exercicio da onda W3-B, 19/09/2026).
#
# O QUE ESTE GATE NAO E' (decisao de desenho, nao omissao): um
# comparador mecanico de PROSA LIVRE contra config de CI. Essa ideia ja
# foi tentada e recusada nesta arvore para o README - ver o cabecalho de
# check_readme_volatile_numbers.py: "o CTO recusou um parser mais
# esperto da prosa (precisaria de uma ancora por numero, e nove ancoras
# tornam a prosa refem do portao)". README.md's paragrafo "On Windows,
# prepare the MSVC build environment..." e' texto livre em ingles
# contando uma HISTORIA (por que o gerador Ninja nao acha o MSVC
# sozinho, por que rodar vcvarsall antes) - nao ha como comparar essa
# narrativa inteira contra `.github/workflows/ci.yml` sem reescrever um
# parser de linguagem natural, e um portao assim reprovaria por
# reformulacao de frase tanto quanto por informacao errada (ruido maior
# que sinal).
#
# O QUE ESTE GATE E', EM VEZ DISSO: o MESMO principio que ja rege
# check_readme_volatile_numbers.py ("checar um TOKEN fixo, nunca prosa
# livre" - precedente citado la: o version-sync do ecossistema Rust, que
# confere token de forma fixa) aplicado aos TRES ANCORAS CONCRETAS que o
# proprio paragrafo do README ja cita, entre aspas/crases, como prova de
# que a receita espelha o servidor:
#
#   (1) o NOME EXATO do passo `Preparar ambiente do compilador (MSVC
#       x64)` - README cita esse nome textualmente como o passo que faz
#       o mesmo que a receita manual local.
#   (2) o marcador de comentario `CI-WIN-GEN` - README manda o leitor la'
#       "para o porque" do gerador Ninja.
#   (3) a alegacao "usa Ninja exclusivamente" - nenhum `-G` do job
#       `windows` pode citar outro gerador (ex.: "Visual Studio").
#
# Os tres sao IDENTIFICADORES FIXOS (nome de passo, marcador de
# comentario, nome de gerador), no MESMO espirito do L-40: um portao que
# le a prosa e sempre concorda com ela nao prova nada; este verifica que
# os tres fatos que a prosa se apoia continuam existindo na arvore real.
# O que ele NAO cobre, por desenho: se o passo mudar de COMPORTAMENTO
# sem mudar de nome, ou se a narrativa em volta das ancoras ficar
# tecnicamente errada de outro jeito. Isso continua sendo revisao
# humana, como qualquer narrativa livre - a mesma linha que
# check_readme_volatile_numbers.py ja traca para o resto do README.
#
# Usage:
#   check_readme_windows_build_anchors.py <readme-path> <ci-yml-path>
#   check_readme_windows_build_anchors.py --selftest

import re
import sys
from pathlib import Path

SCRIPT_NAME = "check_readme_windows_build_anchors.py"

ANCHOR_STEP_NAME = "Preparar ambiente do compilador (MSVC x64)"
ANCHOR_COMMENT_MARKER = "CI-WIN-GEN"
ANCHOR_GENERATOR = "Ninja"

# Quoted generator name ("Visual Studio 17 2022") OR a bare, single
# whitespace-free token (Ninja) - two alternatives, never one pattern
# trying to do both: a single non-greedy '[^"\n]+?' stopping at the
# first internal whitespace silently truncates a quoted multi-word
# generator name at its first space (measured while writing this gate:
# "Visual Studio 17 2022" came back as just "Visual").
_GENERATOR_FLAG_RE = re.compile(r'-G\s+(?:"([^"]+)"|(\S+))')

JOB_NAME = "windows"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def extract_job_block(ci_yml_text, job_name):
    """Returns the body of job JOB_NAME (from '\\n  <job_name>:\\n' up to
    the next top-level 2-space-indented 'key:' line, or EOF), or None if
    the job key itself is not found - absence is reported, never
    presumed empty (GODS_LAWS.md L-40)."""
    m = re.search(rf"\n  {re.escape(job_name)}:\n", ci_yml_text)
    if m is None:
        return None
    start = m.end()
    rest = ci_yml_text[start:]
    next_m = re.search(r"^  [A-Za-z][A-Za-z0-9_-]*:\s*$", rest, re.MULTILINE)
    end = next_m.start() if next_m else len(rest)
    return rest[:end]


def check_step_name_present(job_block):
    """README cites this step's NAME as literal proof the manual recipe
    matches CI - verified by looking for a YAML 'name:' line whose value
    is exactly ANCHOR_STEP_NAME (not just a substring anywhere, which
    could match a stray comment instead of the real step)."""
    pattern = re.compile(
        r"^\s*- name:\s*" + re.escape(ANCHOR_STEP_NAME) + r"\s*$", re.MULTILINE
    )
    return pattern.search(job_block) is not None


def check_comment_marker_present(job_block):
    return ANCHOR_COMMENT_MARKER in job_block


def _strip_comment_lines(text):
    """Drops every line whose stripped content starts with '#' - this
    job's own prose comments narrate the generator history ("-G Ninja:
    mesmo motivo...", "sem -G explicito...") using the exact '-G' shape
    the real invocations use, and a naive scan over the whole job block
    picks up THOSE as if they were commands (measured while writing this
    gate: it did, on the real tree, before this filter existed)."""
    return "\n".join(line for line in text.splitlines() if not line.strip().startswith("#"))


def check_generator_exclusive(job_block):
    """README claims the `windows` job 'uses Ninja exclusively' - every
    '-G' generator flag in the job's REAL cmake invocations (comment
    lines excluded, see _strip_comment_lines) must name Ninja."""
    code_only = _strip_comment_lines(job_block)
    generator_flags = [quoted or bare for quoted, bare in _GENERATOR_FLAG_RE.findall(code_only)]
    non_ninja = [g for g in generator_flags if g.strip() != ANCHOR_GENERATOR]
    return (len(non_ninja) == 0), generator_flags, non_ninja


def check_readme_windows_build_anchors(readme_path, ci_yml_path):
    try:
        ci_yml_text = Path(ci_yml_path).read_text(encoding="utf-8")
    except OSError as exc:
        print(f"{SCRIPT_NAME}: nao foi possivel ler {ci_yml_path} ({exc})", file=sys.stderr)
        return False

    # README's own presence is checked too (GODS_LAWS.md L-40: a
    # README que parou de citar as tres ancoras silenciosamente e' tao
    # suspeito quanto um ci.yml que parou de tras-las - relatado, nunca
    # ignorado).
    try:
        readme_text = Path(readme_path).read_text(encoding="utf-8")
    except OSError as exc:
        print(f"{SCRIPT_NAME}: nao foi possivel ler {readme_path} ({exc})", file=sys.stderr)
        return False

    readme_cites_step = ANCHOR_STEP_NAME in readme_text
    readme_cites_marker = ANCHOR_COMMENT_MARKER in readme_text

    job_block = extract_job_block(ci_yml_text, JOB_NAME)
    if job_block is None:
        print(
            f"{SCRIPT_NAME}: job '{JOB_NAME}:' nao encontrado em {ci_yml_path} "
            "(GODS_LAWS.md L-40: ausencia e' falha, nunca sucesso silencioso)",
            file=sys.stderr,
        )
        return False

    step_ok = check_step_name_present(job_block)
    marker_ok = check_comment_marker_present(job_block)
    gen_ok, generator_flags, non_ninja = check_generator_exclusive(job_block)

    print(
        f"{SCRIPT_NAME}: README cita passo={readme_cites_step} README cita marcador={readme_cites_marker} "
        f"| job '{JOB_NAME}' tem passo={step_ok} tem marcador={marker_ok} "
        f"| geradores encontrados={generator_flags} exclusivamente Ninja={gen_ok}"
    )

    ok = True
    if not readme_cites_step:
        ok = False
        print(f"{SCRIPT_NAME}: README.md nao cita mais o passo '{ANCHOR_STEP_NAME}' - ancora perdida", file=sys.stderr)
    if not readme_cites_marker:
        ok = False
        print(f"{SCRIPT_NAME}: README.md nao cita mais o marcador '{ANCHOR_COMMENT_MARKER}' - ancora perdida", file=sys.stderr)
    if not step_ok:
        ok = False
        print(
            f"{SCRIPT_NAME}: job '{JOB_NAME}' de {ci_yml_path} nao tem mais um passo chamado "
            f"'{ANCHOR_STEP_NAME}' - a receita manual do README nao espelha mais o servidor",
            file=sys.stderr,
        )
    if not marker_ok:
        ok = False
        print(
            f"{SCRIPT_NAME}: job '{JOB_NAME}' de {ci_yml_path} nao tem mais o comentario "
            f"'{ANCHOR_COMMENT_MARKER}' que o README manda o leitor consultar",
            file=sys.stderr,
        )
    if not gen_ok:
        ok = False
        print(
            f"{SCRIPT_NAME}: job '{JOB_NAME}' usa gerador(es) alem de '{ANCHOR_GENERATOR}': {non_ninja} "
            f"- README afirma 'uses Ninja exclusively'",
            file=sys.stderr,
        )

    if not generator_flags:
        # Piso de varredura nao-vazia (GODS_LAWS.md L-40): o job
        # `windows` real SEMPRE tem pelo menos uma chamada com -G - zero
        # e' sinal de extracao quebrada, nunca de job limpo.
        ok = False
        print(
            f"{SCRIPT_NAME}: 0 flag '-G' encontrada no job '{JOB_NAME}' - varredura provavelmente quebrada, GODS_LAWS.md L-40",
            file=sys.stderr,
        )

    return ok


# --- selftest ------------------------------------------------------------


def _ci_yml_with(step_line, marker_present, generator):
    marker_comment = f"      # {ANCHOR_COMMENT_MARKER} explica o porque\n" if marker_present else ""
    return (
        "name: CI\n\njobs:\n"
        "  linux:\n    runs-on: ubuntu-latest\n"
        "  windows:\n"
        "    runs-on: windows-latest\n"
        "    steps:\n"
        f"{marker_comment}"
        f"{step_line}"
        f'      - run: cmake -S . -B build -G "{generator}"\n'
        "  parity:\n    runs-on: ubuntu-latest\n"
    )


_CLEAN_STEP_LINE = f"      - name: {ANCHOR_STEP_NAME}\n        shell: pwsh\n        run: echo prep\n"
_CLEAN_README = (
    f'On Windows, run the "{ANCHOR_STEP_NAME}" step, the same one the `windows` job runs - '
    f"see that file's `{ANCHOR_COMMENT_MARKER}` comment for why it uses Ninja exclusively.\n"
)


def selftest_clean_passes(tmp_path):
    readme = tmp_path / "README.md"
    ci_yml = tmp_path / "ci.yml"
    readme.write_text(_CLEAN_README, encoding="utf-8")
    ci_yml.write_text(_ci_yml_with(_CLEAN_STEP_LINE, True, ANCHOR_GENERATOR), encoding="utf-8")
    ok = check_readme_windows_build_anchors(readme, ci_yml)
    if not ok:
        print("selftest: controle CLEAN FALHOU (ancoras batendo nao deveriam reprovar)", file=sys.stderr)
        return False
    print("selftest: controle CLEAN OK (as tres ancoras batem, passa)")
    return True


def selftest_renamed_step_fails(tmp_path):
    readme = tmp_path / "README.md"
    ci_yml = tmp_path / "ci.yml"
    readme.write_text(_CLEAN_README, encoding="utf-8")
    renamed = "      - name: Preparar ambiente do compilador (x64)\n        shell: pwsh\n        run: echo prep\n"
    ci_yml.write_text(_ci_yml_with(renamed, True, ANCHOR_GENERATOR), encoding="utf-8")
    ok = check_readme_windows_build_anchors(readme, ci_yml)
    if ok:
        print("selftest: controle RENAMED-STEP FALHOU (passo renomeado deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle RENAMED-STEP OK (nome de passo divergente reprova)")
    return True


def selftest_missing_marker_fails(tmp_path):
    readme = tmp_path / "README.md"
    ci_yml = tmp_path / "ci.yml"
    readme.write_text(_CLEAN_README, encoding="utf-8")
    ci_yml.write_text(_ci_yml_with(_CLEAN_STEP_LINE, False, ANCHOR_GENERATOR), encoding="utf-8")
    ok = check_readme_windows_build_anchors(readme, ci_yml)
    if ok:
        print("selftest: controle MISSING-MARKER FALHOU (marcador sumido deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle MISSING-MARKER OK (comentario CI-WIN-GEN sumido reprova)")
    return True


def selftest_non_ninja_generator_fails(tmp_path):
    readme = tmp_path / "README.md"
    ci_yml = tmp_path / "ci.yml"
    readme.write_text(_CLEAN_README, encoding="utf-8")
    ci_yml.write_text(_ci_yml_with(_CLEAN_STEP_LINE, True, "Visual Studio 17 2022"), encoding="utf-8")
    ok = check_readme_windows_build_anchors(readme, ci_yml)
    if ok:
        print("selftest: controle NON-NINJA FALHOU (gerador trocado deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle NON-NINJA OK (gerador diferente de Ninja reprova)")
    return True


def selftest_missing_job_fails(tmp_path):
    readme = tmp_path / "README.md"
    ci_yml = tmp_path / "ci.yml"
    readme.write_text(_CLEAN_README, encoding="utf-8")
    ci_yml.write_text("name: CI\n\njobs:\n  linux:\n    runs-on: ubuntu-latest\n", encoding="utf-8")
    ok = check_readme_windows_build_anchors(readme, ci_yml)
    if ok:
        print("selftest: controle MISSING-JOB FALHOU (job windows ausente deveria reprovar)", file=sys.stderr)
        return False
    print("selftest: controle MISSING-JOB OK (job 'windows' ausente reprova, nunca passa em silencio)")
    return True


def selftest_real_tree_passes(readme_path, ci_yml_path):
    ok = check_readme_windows_build_anchors(readme_path, ci_yml_path)
    if not ok:
        print("selftest: controle REAL-TREE FALHOU (a arvore real deveria passar hoje)", file=sys.stderr)
        return False
    print("selftest: controle REAL-TREE OK (README e ci.yml reais batem hoje)")
    return True


def selftest_main():
    import tempfile

    repo_root = Path(__file__).resolve().parents[2]
    real_readme = repo_root / "README.md"
    real_ci_yml = repo_root / ".github" / "workflows" / "ci.yml"

    with tempfile.TemporaryDirectory(prefix="glintfx-readme-win-anchors-selftest-") as tmp_dir:
        tmp_path = Path(tmp_dir)
        controls = [
            selftest_clean_passes(tmp_path),
            selftest_renamed_step_fails(tmp_path),
            selftest_missing_marker_fails(tmp_path),
            selftest_non_ninja_generator_fails(tmp_path),
            selftest_missing_job_fails(tmp_path),
            selftest_real_tree_passes(real_readme, real_ci_yml),
        ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def real_main(args):
    if len(args) != 2:
        fail("usage: check_readme_windows_build_anchors.py <readme-path> <ci-yml-path>")
    readme_path, ci_yml_path = args
    if not Path(readme_path).is_file():
        fail(f"file not found: {readme_path}")
    if not Path(ci_yml_path).is_file():
        fail(f"file not found: {ci_yml_path}")
    if not check_readme_windows_build_anchors(readme_path, ci_yml_path):
        fail("ancora(s) da receita de build do Windows do README divergem de ci.yml (ver mensagem acima)")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
