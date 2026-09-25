#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# ci_systems.py - CI-SPLIT-PER-OS A3a (D-A12, docs/plano-ci-split-per-
# os.md): carregador da FONTE UNICA de sistemas (tools/ci/systems.txt).
# Nao e' um portao de linha de comando como os outros scripts deste
# diretorio - e' uma BIBLIOTECA, importada por check_test_parity.py,
# check_measured_parity.py e check_ci_systems_source.py (GODS_LAWS.md
# L-17: uma responsabilidade - carregar e validar o arquivo, nunca
# comparar contra o ci.yml, isso e' check_ci_systems_source.py, nem
# julgar paridade, isso e' os dois scripts de paridade).
#
# Antes desta fatia, a lista de sistemas existia REDIGITADA A MAO em
# onze lugares (varredura registrada em DECISOES_AUTONOMAS.md, D-A12):
# SISTEMAS_VALIDOS/SISTEMAS_PER_SYSTEM_ESPERADOS/FAMILIA_POR_SLUG/
# _slug_family (check_test_parity.py), PER_SYSTEM_SLUGS_ESPERADOS/
# LINUX_FAMILY_SLUGS (check_measured_parity.py), duas tuplas
# ("linux","windows") no --compare, e duas listas de `slug=` escritas
# a mao no passo do job `parity` (ci.yml). Nenhuma delas lia a outra -
# um sistema novo exigia editar os onze lugares, e um esquecido
# passava calado. Este modulo e' a UNICA fonte a partir de agora.
#
# `_slug_family()` (a funcao, nao o dict) do codigo antigo devolvia
# "linux" para QUALQUER slug desconhecido, por construcao (`"windows"
# if slug == "windows" else "linux"`) - um slug digitado errado (ex.:
# "rocky-9" antes de existir) nunca reprovava, so' virava "linux" em
# silencio. `slug_family()` abaixo reprova nomeando o slug (D-A12,
# vermelho de comportamento do plano).

import re
import sys
from pathlib import Path

_LINE_RE = re.compile(r"^([a-z0-9][a-z0-9-]*)\|(linux|windows)$")
_FAMILIAS_VALIDAS = ("linux", "windows")


class CiSystemsError(ValueError):
    """Erro de validacao de tools/ci/systems.txt - o chamador decide
    como reportar (fail() de portao, ou selftest que espera a
    excecao)."""


# GODS_LAWS.md L-40 (piso de varredura nao-vazia) + D-A12 (M5/M6):
# arquivo vazio reprova; ausencia de QUALQUER sistema de uma das duas
# familias tambem reprova - um systems.txt sem windows nenhum faria
# todo consumidor Windows desaparecer calado.
def parse_systems_text(text, source_label="tools/ci/systems.txt"):
    """Retorna {slug: familia} - dict, nao lista: a ordem do arquivo
    nao importa pra quem consome (paridade compara CONJUNTOS)."""
    lines = [ln.strip() for ln in text.splitlines()]
    lines = [ln for ln in lines if ln and not ln.startswith("#")]
    if not lines:
        raise CiSystemsError(
            f"{source_label}: arquivo vazio - GODS_LAWS.md L-40, piso de varredura nao-vazia"
        )
    systems = {}
    for line in lines:
        m = _LINE_RE.match(line)
        if not m:
            raise CiSystemsError(
                f"{source_label}: linha malformada (esperado 'slug|familia', slug em "
                f"[a-z0-9-]+, familia 'linux' ou 'windows'): {line!r}"
            )
        slug, familia = m.group(1), m.group(2)
        # D-A12 parte 5 (emenda, contradicao apontada pelo implementador,
        # M7): "linux" nunca foi slug individual - so' o agregado que
        # --compare ja usava. Proibido incondicionalmente, sem excecao
        # (ao contrario de "windows" abaixo).
        if slug == "linux":
            raise CiSystemsError(
                f"{source_label}: slug 'linux' proibido - e' nome de familia, nunca de sistema"
            )
        if slug in systems:
            raise CiSystemsError(f"{source_label}: slug {slug!r} repetido")
        systems[slug] = familia
    familias_presentes = set(systems.values())
    for familia in _FAMILIAS_VALIDAS:
        if familia not in familias_presentes:
            raise CiSystemsError(
                f"{source_label}: nenhum sistema da familia {familia!r} - pelo menos um "
                "sistema por familia e' exigido"
            )
    _check_family_named_slugs(systems, source_label)
    return systems


# D-A12 parte 5 (emenda): um slug IGUAL AO NOME DE UMA FAMILIA (hoje,
# so' pode ser "windows" - "linux" ja foi barrado acima incondicional-
# mente) so' e' aceito se (M9) pertencer a ESSA MESMA familia - nunca
# "windows|linux" - e (M8) for o UNICO sistema dela - o unico sistema
# Windows suportado hoje sempre teve slug literal "windows", coincidindo
# com o nome da familia por ser o UNICO membro; no dia em que a familia
# ganhar um segundo sistema, o carregador reprova ate o slug "windows"
# ser renomeado - a decisao e' tomada ali, nunca antecipada aqui.
def _check_family_named_slugs(systems, source_label):
    for slug, familia in systems.items():
        if slug not in _FAMILIAS_VALIDAS:
            continue
        if slug != familia:
            raise CiSystemsError(
                f"{source_label}: slug {slug!r} e' nome de familia, mas foi declarado na "
                f"familia {familia!r} - so' e' aceito na PROPRIA familia {slug!r}"
            )
        membros = slugs_of_family(systems, familia)
        if len(membros) != 1:
            raise CiSystemsError(
                f"{source_label}: slug {slug!r} e' nome de familia - so' e' aceito enquanto "
                f"for o UNICO sistema da familia {familia!r} (hoje tem {sorted(membros)}) - "
                f"renomeie o slug {slug!r} antes de acrescentar outro sistema a essa familia"
            )


def load_systems(path="tools/ci/systems.txt"):
    with open(path, "r", encoding="utf-8") as handle:
        text = handle.read()
    return parse_systems_text(text, source_label=path)


# D-A12, vermelho de comportamento: NUNCA "linux" por omissao (a
# regressao exata do `_slug_family()` antigo) - slug desconhecido e'
# erro, sempre, nomeando o slug que faltou.
def slug_family(systems, slug):
    if slug not in systems:
        raise CiSystemsError(f"slug {slug!r} nao existe em tools/ci/systems.txt")
    return systems[slug]


def slugs_of_family(systems, familia):
    return frozenset(slug for slug, f in systems.items() if f == familia)


def all_slugs(systems):
    """Tuple ordenado (nunca dict, pra ordem de mensagem de erro ser
    estavel) - equivalente ao antigo SISTEMAS_PER_SYSTEM_ESPERADOS."""
    return tuple(sorted(systems))


def missing_on_vocabulary(systems):
    """Vocabulario completo de `missing_on` aceito por --compare: as
    duas familias AGREGADAS ("linux"/"windows") mais cada slug
    individual - equivalente ao antigo SISTEMAS_VALIDOS. `dict.fromkeys`
    preserva ordem de 1a aparicao SEM duplicar - achado proprio:
    "windows" e' slug E nome de familia ao mesmo tempo (o unico
    sistema da familia windows), entao apareceria duas vezes sem isso."""
    return tuple(dict.fromkeys(list(_FAMILIAS_VALIDAS) + list(all_slugs(systems))))


def familia_por_slug(systems):
    """{familia: frozenset(slugs)} - equivalente ao antigo
    FAMILIA_POR_SLUG, agora DERIVADO de `systems` em vez de
    redigitado."""
    return {familia: slugs_of_family(systems, familia) for familia in _FAMILIAS_VALIDAS}


# --- selftest -----------------------------------------------------


def _expect_error(func, *args, **kwargs):
    try:
        func(*args, **kwargs)
    except CiSystemsError:
        return True
    return False


def selftest_parse_positive_control():
    systems = parse_systems_text("fedora|linux\nwindows|windows\n")
    if systems != {"fedora": "linux", "windows": "windows"}:
        print(f"selftest: PARSE-POSITIVO FALHOU: {systems}")
        return False
    print("selftest: PARSE-POSITIVO OK (duas linhas bem formadas, dict correto)")
    return True


def selftest_parse_ignores_comments_and_blank_lines():
    systems = parse_systems_text("# comentario\n\nfedora|linux\n\nwindows|windows\n# fim\n")
    if systems != {"fedora": "linux", "windows": "windows"}:
        print(f"selftest: PARSE-COMENTARIO FALHOU: {systems}")
        return False
    print("selftest: PARSE-COMENTARIO OK (comentario e linha em branco ignorados)")
    return True


# M5 do plano: systems.txt vazio (ou so' comentarios) reprova.
def selftest_empty_file_reproves():
    for texto in ("", "# so' comentario\n", "\n\n"):
        if not _expect_error(parse_systems_text, texto):
            print(f"selftest: VAZIO-REPROVA FALHOU (texto {texto!r} deveria ter reprovado)")
            return False
    print("selftest: VAZIO-REPROVA OK (arquivo vazio ou so' comentarios reprova, GODS_LAWS.md L-40)")
    return True


# M6 do plano: nenhum sistema windows reprova.
def selftest_missing_windows_family_reproves():
    if not _expect_error(parse_systems_text, "fedora|linux\nubuntu|linux\n"):
        print("selftest: SEM-WINDOWS FALHOU (deveria ter reprovado)")
        return False
    print("selftest: SEM-WINDOWS OK (nenhum sistema da familia windows reprova)")
    return True


def selftest_missing_linux_family_reproves():
    if not _expect_error(parse_systems_text, "windows|windows\n"):
        print("selftest: SEM-LINUX FALHOU (deveria ter reprovado)")
        return False
    print("selftest: SEM-LINUX OK (nenhum sistema da familia linux reprova)")
    return True


# M4 do plano: slug repetido reprova.
def selftest_duplicate_slug_reproves():
    if not _expect_error(parse_systems_text, "fedora|linux\nfedora|linux\nwindows|windows\n"):
        print("selftest: SLUG-REPETIDO FALHOU (deveria ter reprovado)")
        return False
    print("selftest: SLUG-REPETIDO OK (slug repetido reprova)")
    return True


# D-A12 parte 5 (emenda, M7): "linux" nunca e' slug, incondicionalmente
# - ao contrario de "windows" (ver os controles seguintes).
def selftest_linux_slug_always_reproves():
    for texto in ("linux|linux\nwindows|windows\n", "linux|windows\nfedora|linux\nwindows|windows\n"):
        if not _expect_error(parse_systems_text, texto):
            print(f"selftest: LINUX-SLUG-SEMPRE-REPROVA FALHOU ({texto!r} deveria ter reprovado)")
            return False
    print("selftest: LINUX-SLUG-SEMPRE-REPROVA OK (M7 - 'linux' nunca e' slug, mesmo dentro da familia windows)")
    return True


# D-A12 parte 5 (emenda): "windows|windows" sozinho, sendo o UNICO
# sistema da familia windows, e' ACEITO - contradicao real que a
# versao anterior desta fatia proibia por engano (o unico sistema
# Windows suportado hoje sempre teve slug literal "windows").
def selftest_windows_slug_accepted_when_only_member_control():
    systems = parse_systems_text("fedora|linux\nwindows|windows\n")
    if systems != {"fedora": "linux", "windows": "windows"}:
        print(f"selftest: WINDOWS-SLUG-ACEITO FALHOU: {systems}")
        return False
    print("selftest: WINDOWS-SLUG-ACEITO OK ('windows|windows' aceito quando e' o unico sistema da familia)")
    return True


# D-A12 parte 5 (emenda, M9): "windows" como slug, mas declarado na
# familia ERRADA (linux) - nunca aceito, mesmo que fosse o unico
# membro daquela familia.
def selftest_windows_slug_wrong_family_reproves():
    if not _expect_error(parse_systems_text, "windows|linux\nfedora|linux\nwindows-real|windows\n"):
        print("selftest: WINDOWS-SLUG-FAMILIA-ERRADA FALHOU (deveria ter reprovado)")
        return False
    print("selftest: WINDOWS-SLUG-FAMILIA-ERRADA OK (M9 - 'windows|linux' reprova, slug so' e' aceito na PROPRIA familia)")
    return True


# D-A12 parte 5 (emenda, M8): "windows|windows" deixa de ser o UNICO
# sistema da familia windows (um segundo sistema windows-arm64|windows
# chega) - o carregador reprova citando o slug "windows", ate ele ser
# renomeado.
def selftest_windows_slug_second_family_member_reproves():
    if not _expect_error(parse_systems_text, "fedora|linux\nwindows|windows\nwindows-arm64|windows\n"):
        print("selftest: WINDOWS-SLUG-SEGUNDO-MEMBRO FALHOU (deveria ter reprovado)")
        return False
    print(
        "selftest: WINDOWS-SLUG-SEGUNDO-MEMBRO OK (M8 - 'windows' deixa de ser o unico "
        "sistema da familia windows, reprova citando o slug)"
    )
    return True


def selftest_malformed_line_reproves():
    for texto in ("fedora\n", "fedora|\n", "fedora|LINUX\n", "Fedora|linux\n", "fedora|linux|extra\n"):
        if not _expect_error(parse_systems_text, texto):
            print(f"selftest: LINHA-MALFORMADA FALHOU ({texto!r} deveria ter reprovado)")
            return False
    print("selftest: LINHA-MALFORMADA OK (forma diferente de 'slug|familia' reprova)")
    return True


# D-A12, vermelho de comportamento citado no plano: slug desconhecido
# NUNCA vira "linux" por omissao - o defeito exato do `_slug_family()`
# antigo.
def selftest_slug_family_unknown_slug_reproves():
    systems = {"fedora": "linux", "windows": "windows"}
    if not _expect_error(slug_family, systems, "rocky-9"):
        print("selftest: SLUG-FAMILY-DESCONHECIDO FALHOU (deveria ter reprovado, nunca 'linux' por omissao)")
        return False
    print("selftest: SLUG-FAMILY-DESCONHECIDO OK (slug desconhecido reprova, nunca linux por omissao)")
    return True


def selftest_slug_family_known_slugs():
    systems = {"fedora": "linux", "arch": "linux", "windows": "windows"}
    if slug_family(systems, "fedora") != "linux" or slug_family(systems, "windows") != "windows":
        print("selftest: SLUG-FAMILY-CONHECIDO FALHOU")
        return False
    print("selftest: SLUG-FAMILY-CONHECIDO OK (slug conhecido devolve a familia certa)")
    return True


def selftest_derived_helpers():
    systems = {"fedora": "linux", "ubuntu": "linux", "windows": "windows"}
    if all_slugs(systems) != ("fedora", "ubuntu", "windows"):
        print(f"selftest: DERIVADOS FALHOU (all_slugs): {all_slugs(systems)}")
        return False
    vocab = missing_on_vocabulary(systems)
    if set(vocab) != {"linux", "windows", "fedora", "ubuntu"}:
        print(f"selftest: DERIVADOS FALHOU (missing_on_vocabulary): {vocab}")
        return False
    # "windows" e' slug E nome de familia ao mesmo tempo - achado
    # proprio, mutation testing: sem cuidado, a tupla teria "windows"
    # DUAS vezes (uma como familia agregada, outra como slug), feio em
    # qualquer mensagem que a imprima.
    if len(vocab) != len(set(vocab)):
        print(f"selftest: DERIVADOS FALHOU (missing_on_vocabulary tem duplicata - 'windows' aparece 2x): {vocab}")
        return False
    familia = familia_por_slug(systems)
    if familia != {"linux": frozenset({"fedora", "ubuntu"}), "windows": frozenset({"windows"})}:
        print(f"selftest: DERIVADOS FALHOU (familia_por_slug): {familia}")
        return False
    print("selftest: DERIVADOS OK (all_slugs/missing_on_vocabulary/familia_por_slug consistentes, sem duplicata)")
    return True


# D-A12 parte (e) (achado do main): --selftest NUNCA toca o arquivo
# real - D1, a A7 acrescentando uma distro nao pode mudar o resultado
# de NENHUM --selftest deste projeto. O controle contra o arquivo REAL
# vira um MODO PROPRIO (--check), nunca uma entrada de --selftest -
# registrado como um add_test SEPARADO em tests/CMakeLists.txt.
def selftest_main():
    controls = [
        selftest_parse_positive_control(),
        selftest_parse_ignores_comments_and_blank_lines(),
        selftest_empty_file_reproves(),
        selftest_missing_windows_family_reproves(),
        selftest_missing_linux_family_reproves(),
        selftest_duplicate_slug_reproves(),
        selftest_linux_slug_always_reproves(),
        selftest_windows_slug_accepted_when_only_member_control(),
        selftest_windows_slug_wrong_family_reproves(),
        selftest_windows_slug_second_family_member_reproves(),
        selftest_malformed_line_reproves(),
        selftest_slug_family_unknown_slug_reproves(),
        selftest_slug_family_known_slugs(),
        selftest_derived_helpers(),
    ]
    if not all(controls):
        print("ci_systems.py --selftest: FALHOU (ver acima)")
        raise SystemExit(1)
    print(f"ci_systems.py --selftest: os {len(controls)} controles OK")


# D2 (achado do main): resolve tools/ci/systems.txt a partir da
# LOCALIZACAO DO PROPRIO SCRIPT (nunca do cwd) - este arquivo mora em
# <raiz-do-repo>/tests/tools/, entao o terceiro pai resolvido e' a
# raiz. Mesmo idioma que check_test_parity.py/check_measured_
# parity.py ja usam para a mesma razao (o ctest roda a partir do
# diretorio de build).
def _real_systems_path():
    return Path(__file__).resolve().parents[2] / "tools" / "ci" / "systems.txt"


# Unico controle contra a ARVORE REAL deste modulo - fora do
# --selftest de proposito (ver o comentario de selftest_main() acima).
# So' a FORMA (as duas familias presentes), nunca o conjunto exato de
# slugs - a A7 acrescentando uma distro tem que deixar isto intacto.
def real_main():
    path = _real_systems_path()
    try:
        systems = load_systems(str(path))
    except (OSError, CiSystemsError) as exc:
        print(f"ci_systems.py: {path} nao carregou: {exc}", file=sys.stderr)
        raise SystemExit(1)
    familias_presentes = set(systems.values())
    faltando = set(_FAMILIAS_VALIDAS) - familias_presentes
    if faltando:
        print(f"ci_systems.py: familia(s) ausente(s) em {path}: {sorted(faltando)}", file=sys.stderr)
        raise SystemExit(1)
    print(
        f"ci_systems.py: OK - {path} carrega, {len(systems)} sistema(s), "
        f"familias {sorted(familias_presentes)}"
    )


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--check":
        real_main()
    else:
        print("usage: ci_systems.py --check  |  --selftest", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
