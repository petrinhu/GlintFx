#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_selftest_orphan.py - TODO.md item GATE-SELFTEST-ORFAO
# (GODS_LAWS.md L-36/L-40, L-17).
#
# THE DEFECT THIS CLOSES (achado de 10/09/2026, terceira reprovacao de
# WIN-CROSS-STAGE, por um terceiro revisor independente): uma
# ferramenta com autoteste que NADA MECANICO CHAMA so protege se um
# humano se lembrar de digitar o comando. A classe ja reincidiu QUATRO
# vezes neste repositorio: check_win32_test_link.py (achado na segunda
# reprovacao da mesma fatia), collect_measured.py e
# check_measured_parity.py (achados junto, sem ninguem pedir), e
# tools/win-vm-lab/provar-isolamento.sh (achado na terceira reprovacao
# - o mais grave dos quatro, porque e o autoteste que prova o
# ISOLAMENTO da maquina virtual Windows contra a sessao de trabalho do
# lider, GODS_LAWS.md L-09/L-50).
#
# A CAUSA RAIZ, apontada pela pesquisa que a lei obriga na terceira
# volta (GODS_LAWS.md L-22/L-42): o defeito nao e esforco de ninguem,
# e o METODO. O orquestrador declarou, no commit d9bdc2a, "40
# ferramentas tem autoteste, ZERO orfas restantes" - e verdadeiro so
# para tests/tools/, o unico diretorio que aquela varredura manual
# olhou. provar-isolamento.sh mora em tools/win-vm-lab/, fora dali, e
# passou batido - nao por descuido de uma pessoa, mas porque cada
# rodada manual repete o mesmo ponto cego geometrico (um diretorio por
# vez). A cura fecha a CLASSE, varrendo o REPOSITORIO inteiro, nunca
# mais uma instancia.
#
# REFERENCIA LIDA NA PESQUISA (GODS_LAWS.md L-37: para aprender a
# TECNICA, nunca para copiar a implementacao): a convencao de
# autotestes do nucleo do Linux (Documentation/dev-tools/kselftest.rst)
# resolve o mesmo problema de raiz com uma LISTA CENTRAL que vira fonte
# de verdade (o arquivo TEST_LIST/Makefile de cada diretorio) contra a
# qual o runner confere o que existe em disco - o principio geral
# reaproveitado aqui e "nunca confiar em alguem lembrar de registrar
# manualmente", nao o arquivo TEST_LIST em si. Este gate inverte a
# direcao (varre o CODIGO para descobrir o que deveria estar na lista,
# em vez de manter uma lista central escrita a mao) porque o
# TODO.md deste projeto ja pede exatamente isso: "o padrao de codigo
# que declara um autoteste... cruza contra o registro em
# tests/CMakeLists.txt".
#
# O QUE ESTE GATE FAZ: varre TODO o repositorio (git ls-files, tracked
# e untracked-nao-ignorado - mesma convencao de check_spdx.py) por
# arquivo .py/.sh/.ps1, procurando o padrao de CODIGO (nunca o nome do
# arquivo) que o proprio analisador de opcoes do script usa para
# reconhecer --selftest/-SelfTest como argumento de linha de comando -
# ver classify_dispatch_line() abaixo para os tres criterios, um por
# linguagem. Cruza cada ferramenta encontrada contra
# tests/CMakeLists.txt (o unico registro de ctest deste projeto),
# procurando um bloco add_test(...) que cite AO MESMO TEMPO o caminho
# da ferramenta e a flag de autoteste. Imprime SEMPRE as duas
# contagens (declaradas, registradas), mesmo quando batem, e reprova
# nomeando cada orfa (GODS_LAWS.md L-40: zero e sinal de varredura
# quebrada, nunca "nada para proteger").
#
# CRITERIO POR CODIGO, NAO POR NOME DE ARQUIVO (exigencia do
# team-lead, respondendo exatamente ao jeito como as tres primeiras
# instancias passaram batido - varredura manual por NOME/diretorio).
# Uma linha e classificada como "este script despacha --selftest" so
# quando NAO comeca com o marcador de comentario da linguagem (`#`
# tanto em Python quanto em sh/PowerShell) E contem o literal
# ENTRE ASPAS "--selftest"/'--selftest' (Python/sh) ou o parametro
# `[switch]$SelfTest` (PowerShell) numa forma que so aparece em codigo
# de despacho real, nunca em prosa. Ver classify_dispatch_line() para
# o texto exato de cada um dos tres criterios.
#
# FALSOS POSITIVOS MEDIDOS NA ARVORE REAL EM 10/09/2026 (nao supostos -
# ver selftest_measured_false_positive_count_on_real_tree() abaixo,
# que roda o MESMO discover_declared_tools() deste gate contra o
# working tree de verdade dentro do proprio --selftest): ZERO. A
# arvore tem documentacao (.md), o proprio tests/CMakeLists.txt,
# tests/parity_aliases.txt, fixtures C++ (tests/preci_fixtures/) e
# .github/workflows/ci.yml mencionando "--selftest" em prosa ou como
# CHAMADOR (nunca como declarante) - nenhum e .py/.sh/.ps1 exceto os
# arquivos que genuinamente despacham a flag, entao o filtro de
# extensao already exclui a maior parte, e nenhuma linha de comentario
# dentro dos arquivos .py/.sh/.ps1 remanescentes casa com os criterios
# de codigo acima (medido por grep dedicado antes de escrever este
# gate).
#
# O QUE ESTE GATE NAO PEGA (declarado, GODS_LAWS.md L-40 aplicado ao
# proximo leitor, nao so ao lider):
#
#   * Um script cujo autoteste sempre roda incondicionalmente (sem
#     flag nenhuma para esquecer - ex.: tools/ci/check-pkgconfig-
#     installed.ps1, que chama Invoke-SelfTest() sem nenhum parametro
#     [switch]$SelfTest correspondente). Fora de escopo por desenho:
#     esse script nao tem "modo esquecivel", so tem um modo.
#   * Uma forma de despacho nova que nenhum dos padroes hoje
#     conhecidos cobre (ex.: argparse.add_argument("--selftest", ...)
#     em vez de comparacao manual de sys.argv - nenhum script deste
#     projeto usa essa forma hoje, medido). O dia que um script usar
#     essa forma, este gate para de ve-lo ate alguem ensinar o
#     detector - exatamente o tipo de ponto cego que este gate existe
#     para reduzir, nao eliminar por completo.
#   * Registro em qualquer lugar que NAO seja tests/CMakeLists.txt
#     (ex.: um passo cru de shell em .github/workflows/ci.yml que
#     chama --selftest fora do ctest). O unico registro que CONTA para
#     este projeto e uma entrada de `ctest -N` - ver TODO.md's own
#     PARITY-GATE decision sobre os dez gates Windows que so existiam
#     como passo cru de CI e por isso eram invisiveis.
#
# EXCECOES DECLARADAS (tests/tools/selftest_orphan_exceptions.txt,
# mesma forma/regra-da-morte de tests/lib_source_exceptions.txt e
# tests/parity_absences.txt): uma orfa que nao pode entrar no ctest
# HOJE por decisao de escopo de outro item (ex.: check_plan_scope_
# diff.py, ja reservado ao item PLAN-SCOPE-COLUMNS) nunca vira silencio
# - vira linha, com motivo e item TODO aberto, impressa em toda rodada.
# Uma excecao cujo alvo deixa de estar orfao (foi registrado de
# verdade, ou sumiu) e' OBSOLETA e reprova ate a linha ser apagada -
# nunca fica esquecida depois de resolvida.
#
# Usage:
#   check_selftest_orphan.py --check <repo-root-directory>
#   check_selftest_orphan.py --selftest

import os
import re
import subprocess
import sys

SCRIPT_NAME = "check_selftest_orphan.py"

_CANDIDATE_SUFFIXES = (".py", ".sh", ".ps1")

_PS1_SWITCH_RE = re.compile(r"\[switch\]\s*\$self[Tt]est\b", re.IGNORECASE)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- language classification ------------------------------------------


def language_for_path(relpath):
    if relpath.endswith(".py"):
        return "py"
    if relpath.endswith(".sh"):
        return "sh"
    if relpath.endswith(".ps1"):
        return "ps1"
    return None


def flag_for_language(language):
    return "-SelfTest" if language == "ps1" else "--selftest"


# --- per-language dispatch detection (GODS_LAWS.md L-17: uma funcao, uma
# linguagem, nunca uma regex universal tentando cobrir as tres) -------


def _python_line_dispatches(stripped):
    if '"--selftest"' not in stripped and "'--selftest'" not in stripped:
        return False
    # Toda forma observada neste projeto compara o literal contra
    # sys.argv/args (==), ou o testa por pertencimento (in) ou prefixo
    # (.startswith(); nenhum script hoje usa esta ultima, mas a
    # inclusao e barata e documentada acima como ponto cego reduzido).
    # Uma linha so com o literal entre aspas, sem nenhum destes tres,
    # e prosa (ex.: um docstring citando o comando de uso) - nunca
    # despacho real.
    return ("==" in stripped) or (".startswith(" in stripped) or (" in " in stripped)


def _shell_line_dispatches(stripped):
    # Rotulo de `case`: "--selftest)" - a forma que provar-isolamento.sh
    # e check_win32_test_link.py (portado) usam.
    if re.match(r"^--selftest\)", stripped):
        return True
    # Teste de igualdade: `[ "$1" = "--selftest" ]` / `[ "${1:-}" =
    # "--selftest" ]` - a forma que check_exports.sh/check_port_
    # privacy.sh usam. Exige o literal ENTRE ASPAS seguido de `=` na
    # mesma linha (nunca so o literal solto, que apareceria tambem
    # numa linha de uso/echo como "Uso: ... | --selftest").
    if '"--selftest"' in stripped and re.search(r"=\s*\"--selftest\"", stripped):
        return True
    return False


def _ps1_line_dispatches(stripped):
    return bool(_PS1_SWITCH_RE.search(stripped))


_DISPATCH_FUNCS = {
    "py": _python_line_dispatches,
    "sh": _shell_line_dispatches,
    "ps1": _ps1_line_dispatches,
}


def file_declares_own_selftest(language, text):
    dispatch_func = _DISPATCH_FUNCS[language]
    for raw_line in text.splitlines():
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if dispatch_func(stripped):
            return True
    return False


# --- pure discovery over an in-memory file map (used by both real_main
# and --selftest, so the exact same code path is what is proven) ------


def discover_declared_tools(file_texts):
    """file_texts: dict[relpath] -> text (candidatos ja filtrados por
    extensao, ou nao - filtra aqui tambem, por seguranca). Returns a
    sorted list of (relpath, language, flag)."""
    declared = []
    for relpath, text in file_texts.items():
        language = language_for_path(relpath)
        if language is None:
            continue
        if text is None:
            continue
        if file_declares_own_selftest(language, text):
            declared.append((relpath, language, flag_for_language(language)))
    declared.sort(key=lambda item: item[0])
    return declared


# --- add_test(...) block extraction (paren-balanced, quote-aware: um
# caminho jamais tem parenteses, mas a contagem respeita aspas mesmo
# assim para nunca confundir um generator-expression $<...> com um
# parenteses de verdade por acidente futuro) --------------------------


def extract_add_test_blocks(cmake_text):
    blocks = []
    marker = "add_test("
    idx = 0
    while True:
        pos = cmake_text.find(marker, idx)
        if pos == -1:
            break
        start = pos + len(marker)
        depth = 1
        i = start
        in_string = False
        while i < len(cmake_text) and depth > 0:
            ch = cmake_text[i]
            if ch == '"' and (i == 0 or cmake_text[i - 1] != "\\"):
                in_string = not in_string
            elif not in_string:
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
            i += 1
        blocks.append(cmake_text[start : i - 1])
        idx = i
    return blocks


def match_key_for(relpath):
    """Ultimos dois componentes do caminho (dir-pai/nome-de-arquivo),
    para distinguir dois scripts de mesmo basename em diretorios
    diferentes - nunca so o basename, que colidiria."""
    parts = relpath.split("/")
    if len(parts) >= 2:
        return parts[-2] + "/" + parts[-1]
    return parts[-1]


def block_registers_tool(block_text, relpath, flag):
    key = match_key_for(relpath)
    if key not in block_text:
        return False
    return flag.lower() in block_text.lower()


# --- pure evaluation (o que --selftest exercita em vermelho e verde) --


def find_orphans(declared_tools, cmake_text):
    blocks = extract_add_test_blocks(cmake_text)
    orphans = []
    for relpath, _language, flag in declared_tools:
        if not any(block_registers_tool(block, relpath, flag) for block in blocks):
            orphans.append((relpath, flag))
    return orphans


# --- real mode -----------------------------------------------------------


def git_ls_files_z(root, extra_args):
    proc = subprocess.run(
        ["git", "-C", root, "ls-files", "-z", *extra_args],
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        return [], False
    raw = proc.stdout
    if not raw:
        return [], True
    if raw.endswith(b"\0"):
        raw = raw[:-1]
    encoding = sys.getfilesystemencoding()
    paths = [chunk.decode(encoding, errors="surrogateescape") for chunk in raw.split(b"\0")]
    return paths, True


def scanned_paths(root):
    """Mesma convencao de check_spdx.py: tracked + untracked-nao-
    ignorado, nunca so tracked (um arquivo novo com --selftest, ainda
    nao commitado, e exatamente o caso que este gate precisa pegar
    ANTES do commit, nao depois)."""
    tracked, ok = git_ls_files_z(root, [])
    if not ok:
        return [], False
    untracked, ok = git_ls_files_z(root, ["--others", "--exclude-standard"])
    if not ok:
        return [], False
    return tracked + untracked, True


def read_text_lenient(path):
    try:
        with open(path, "r", encoding="utf-8", errors="surrogateescape") as handle:
            return handle.read()
    except OSError:
        return None


# --- declared exceptions (tests/tools/selftest_orphan_exceptions.txt,
# mesma forma e mesma regra de morte de tests/lib_source_exceptions.txt
# e tests/parity_absences.txt - GODS_LAWS.md L-40: uma orfa que nao
# pode entrar na suite hoje por decisao de escopo de outro item nunca
# vira silencio, vira LINHA, com motivo e dono, e essa linha morre
# sozinha (reprova) no dia em que deixar de ser verdadeira) ----------


def parse_exceptions_text(text):
    """Retorna (dict[relpath] -> (motivo, item), lista de erros de
    forma). Nunca lanca - forma invalida vira erro reportado, nunca
    excecao Python nem pulo calado."""
    exceptions = {}
    errors = []
    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        parts = stripped.split("|")
        if len(parts) != 3:
            errors.append(f"linha {line_no}: esperava 3 campos separados por '|', veio {stripped!r}")
            continue
        relpath, reason, item = (p.strip() for p in parts)
        if not relpath or not reason or not item:
            errors.append(f"linha {line_no}: campo vazio em {stripped!r}")
            continue
        if relpath in exceptions:
            errors.append(f"linha {line_no}: '{relpath}' ja aparece antes neste arquivo (duplicata)")
            continue
        exceptions[relpath] = (reason, item)
    return exceptions, errors


# Separa os orfaos em (exceptions_aplicadas, orfaos_reais,
# excecoes_obsoletas). Uma excecao so e' "aplicada" quando o caminho
# dela esta de fato entre os orfaos desta rodada; qualquer entrada do
# arquivo de excecoes que NAO esteja mais entre os orfaos (a
# ferramenta sumiu, parou de declarar autoteste, OU foi registrada de
# verdade) e' reportada como obsoleta e reprova - a mesma "regra da
# morte" que tests/parity_absences.txt ja aplica a suas proprias
# linhas.
def apply_exceptions(orphans, exceptions):
    orphan_paths = {relpath for relpath, _flag in orphans}
    applied = []
    remaining = []
    for relpath, flag in orphans:
        if relpath in exceptions:
            applied.append((relpath, flag, exceptions[relpath]))
        else:
            remaining.append((relpath, flag))
    stale = [relpath for relpath in exceptions if relpath not in orphan_paths]
    return applied, remaining, stale


def real_main(args):
    if len(args) != 1:
        fail("usage: check_selftest_orphan.py --check <repo-root-directory>")
    root = args[0]

    all_paths, ok = scanned_paths(root)
    if not ok:
        fail(f"'git ls-files' falhou em '{root}' (nao e repositorio git, ou git indisponivel)")

    if not all_paths:
        fail("varredura vazia: 0 arquivos rastreados ou nao-rastreados - GODS_LAWS.md L-40")

    candidate_paths = [p for p in all_paths if p.endswith(_CANDIDATE_SUFFIXES)]

    file_texts = {}
    for relpath in candidate_paths:
        file_texts[relpath] = read_text_lenient(os.path.join(root, relpath))

    declared_tools = discover_declared_tools(file_texts)

    print(
        f"{SCRIPT_NAME}: {len(all_paths)} arquivo(s) varrido(s) no total, "
        f"{len(candidate_paths)} candidato(s) .py/.sh/.ps1, "
        f"{len(declared_tools)} ferramenta(s) com autoteste proprio declarado"
    )

    if not declared_tools:
        fail(
            "varredura vazia: nenhuma ferramenta com --selftest/-SelfTest proprio encontrada - "
            "GODS_LAWS.md L-40, isto e sinal de detector quebrado, nunca 'nada para proteger' "
            "(este repositorio tem ferramentas com autoteste conhecidas)"
        )

    cmake_path = os.path.join(root, "tests", "CMakeLists.txt")
    cmake_text = read_text_lenient(cmake_path)
    if cmake_text is None:
        fail(f"nao foi possivel ler '{cmake_path}' - o unico registro de ctest deste projeto")

    orphans = find_orphans(declared_tools, cmake_text)
    registered_count = len(declared_tools) - len(orphans)

    print(
        f"{SCRIPT_NAME}: {registered_count} registrada(s) em tests/CMakeLists.txt "
        f"(add_test citando o caminho E a flag de autoteste), {len(orphans)} orfa(s) antes de excecoes"
    )

    exceptions_path = os.path.join(root, "tests", "tools", "selftest_orphan_exceptions.txt")
    exceptions_text = read_text_lenient(exceptions_path)
    if exceptions_text is None:
        fail(f"nao foi possivel ler '{exceptions_path}' - deve existir, mesmo vazio de linhas de dados")

    exceptions, parse_errors = parse_exceptions_text(exceptions_text)
    if parse_errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(parse_errors)} erro(s) de forma em selftest_orphan_exceptions.txt):", file=sys.stderr)
        for error in parse_errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    applied, remaining, stale = apply_exceptions(orphans, exceptions)

    print(f"{SCRIPT_NAME}: {len(exceptions)} excecao(oes) declarada(s) em selftest_orphan_exceptions.txt, {len(applied)} aplicada(s), {len(stale)} obsoleta(s)")
    for relpath, _flag, (reason, item) in applied:
        print(f"  - EXCECAO aplicada: {relpath} (item {item}): {reason}")

    if stale:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(stale)} excecao(oes) obsoleta(s) - a ferramenta ja nao esta orfa ou ja nao existe/declara autoteste, apague a linha):", file=sys.stderr)
        for relpath in stale:
            print(f"  - {relpath}", file=sys.stderr)
        sys.exit(1)

    if remaining:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(remaining)} orfa(s) sem excecao declarada):", file=sys.stderr)
        for relpath, flag in remaining:
            print(f"  - {relpath} declara {flag} no proprio analisador, mas nenhum add_test() "
                  f"em tests/CMakeLists.txt cita ambos, e nenhuma linha em selftest_orphan_exceptions.txt "
                  f"a cobre", file=sys.stderr)
        sys.exit(1)

    print(
        f"{SCRIPT_NAME}: {len(declared_tools)} ferramenta(s) com autoteste - "
        f"{registered_count} registrada(s), {len(applied)} sob excecao declarada, 0 orfa(s) sem dono"
    )


# --- fixtures and controls for --selftest -----------------------------


_PY_DISPATCH_BODY = (
    'import sys\n'
    'def main():\n'
    '    args = sys.argv[1:]\n'
    '    if args and args[0] == "--selftest":\n'
    '        print("selftest ok")\n'
    'if __name__ == "__main__":\n'
    '    main()\n'
)

_SH_DISPATCH_BODY = (
    '#!/usr/bin/env bash\n'
    'if [ "${1:-}" = "--selftest" ]; then\n'
    '  echo selftest ok\n'
    'fi\n'
)

_PS1_DISPATCH_BODY = (
    'param(\n'
    '    [Parameter(ParameterSetName = "SelfTest")][switch]$SelfTest\n'
    ')\n'
    'if ($SelfTest) { "selftest ok" }\n'
)


def selftest_positive_control_registered_tools_no_orphans():
    declared = discover_declared_tools(
        {
            "tests/tools/fake_check.py": _PY_DISPATCH_BODY,
            "tools/fake_check.sh": _SH_DISPATCH_BODY,
            "tools/ci/fake-check-win.ps1": _PS1_DISPATCH_BODY,
        }
    )
    if len(declared) != 3:
        print(f"selftest: controle POSITIVO FALHOU (esperava 3 ferramentas declaradas, veio {declared})", file=sys.stderr)
        return False
    cmake_text = (
        'add_test(\n'
        '    NAME fake_check_selftest\n'
        '    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/fake_check.py" --selftest\n'
        ')\n'
        'add_test(\n'
        '    NAME fake_check_sh_selftest\n'
        '    COMMAND "${PROJECT_SOURCE_DIR}/tools/fake_check.sh" --selftest\n'
        ')\n'
        'add_test(\n'
        '    NAME fake_check_win_selftest\n'
        '    COMMAND "${GLINTFX_PWSH_EXECUTABLE}" -NoProfile -File\n'
        '        "${PROJECT_SOURCE_DIR}/tools/ci/fake-check-win.ps1" -SelfTest\n'
        ')\n'
    )
    orphans = find_orphans(declared, cmake_text)
    if orphans:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero orfas, veio {orphans})", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (tres linguagens, tres registros, zero orfas)")
    return True


# VERMELHO - o defeito real das quatro instancias: a ferramenta
# declara --selftest no proprio codigo, mas tests/CMakeLists.txt nunca
# a cita. Precisa reprovar E nomear o caminho exato.
def selftest_orphan_reproves_by_name():
    declared = discover_declared_tools({"tools/orphan/never_called.sh": _SH_DISPATCH_BODY})
    cmake_text = "add_test(\n    NAME something_else\n    COMMAND some_other_tool\n)\n"
    orphans = find_orphans(declared, cmake_text)
    if not orphans:
        print("selftest: VERMELHO FALHOU (orfa deveria ter sido reprovada)", file=sys.stderr)
        return False
    if orphans[0][0] != "tools/orphan/never_called.sh":
        print(f"selftest: VERMELHO FALHOU (nao nomeou o caminho certo): {orphans}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO OK (orfa pega e nomeada): {orphans}")
    return True


# Controle: um add_test() cita o caminho certo mas SEM a flag de
# autoteste (ex.: so a invocacao REAL, sem --selftest) - continua orfa,
# nao pode ser confundido com registro.
def selftest_path_without_flag_still_orphan():
    declared = discover_declared_tools({"tests/tools/only_real_mode.py": _PY_DISPATCH_BODY})
    cmake_text = (
        'add_test(\n'
        '    NAME only_real_mode_test\n'
        '    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/only_real_mode.py" "${PROJECT_SOURCE_DIR}"\n'
        ')\n'
    )
    orphans = find_orphans(declared, cmake_text)
    if not orphans:
        print("selftest: controle SEM-FLAG FALHOU (registro sem --selftest nao deveria contar como registrado)", file=sys.stderr)
        return False
    print(f"selftest: controle SEM-FLAG OK (caminho citado sem a flag continua orfao): {orphans}")
    return True


# Controle: dois scripts de MESMO basename em diretorios diferentes -
# match_key_for() usa os dois ultimos componentes exatamente para nao
# confundir um com o outro.
def selftest_same_basename_different_dir_not_confused():
    declared = discover_declared_tools(
        {
            "tools/a/check_thing.sh": _SH_DISPATCH_BODY,
            "tools/b/check_thing.sh": _SH_DISPATCH_BODY,
        }
    )
    cmake_text = (
        'add_test(\n'
        '    NAME a_selftest\n'
        '    COMMAND "${PROJECT_SOURCE_DIR}/tools/a/check_thing.sh" --selftest\n'
        ')\n'
    )
    orphans = find_orphans(declared, cmake_text)
    orphan_paths = [p for p, _flag in orphans]
    if orphan_paths != ["tools/b/check_thing.sh"]:
        print(f"selftest: controle MESMO-BASENAME FALHOU (esperava so tools/b orfao): {orphans}", file=sys.stderr)
        return False
    print("selftest: controle MESMO-BASENAME OK (a/check_thing.sh registrado reconhecido, b/check_thing.sh orfao pego, sem confundir os dois)")
    return True


# Controle NUNCA-POR-COMENTARIO (o requisito central do team-lead:
# criterio e por CODIGO, nunca documentacao/nome): um comentario que
# MENCIONA --selftest em prosa, sem nenhum despacho real, nao pode
# fazer um arquivo ser classificado como ferramenta.
def selftest_comment_only_mention_never_flagged():
    py_comment_only = (
        '#!/usr/bin/env python3\n'
        '# Uso:\n'
        '#   this_tool.py --check <root>\n'
        '#   this_tool.py --selftest\n'
        '#\n'
        '# --selftest ainda nao foi implementado.\n'
        'import sys\n'
        'print("apenas um script comum, sem despacho de --selftest")\n'
    )
    sh_usage_banner_only = (
        '#!/usr/bin/env bash\n'
        'echo "Uso: $0 --dominio <nome> | --selftest" >&2\n'
        'exit 2\n'
    )
    declared = discover_declared_tools(
        {
            "tools/doc_only.py": py_comment_only,
            "tools/usage_banner_only.sh": sh_usage_banner_only,
        }
    )
    if declared:
        print(f"selftest: controle NUNCA-POR-COMENTARIO FALHOU (nao deveria classificar nada): {declared}", file=sys.stderr)
        return False
    print("selftest: controle NUNCA-POR-COMENTARIO OK (mencao em comentario/banner de uso nunca vira ferramenta)")
    return True


# VERMELHO (piso de varredura vazia, GODS_LAWS.md L-40): zero
# candidatos .py/.sh/.ps1 no mapa de arquivos - discover_declared_
# tools() tem que devolver lista vazia (o piso em si vive em real_main,
# testado separadamente pela leitura do codigo - aqui prova-se que a
# funcao pura nao inventa ferramenta do nada).
def selftest_empty_file_map_yields_zero_declared():
    declared = discover_declared_tools({})
    if declared:
        print(f"selftest: VERMELHO#2 FALHOU (mapa vazio deveria devolver zero ferramentas): {declared}", file=sys.stderr)
        return False
    print("selftest: VERMELHO#2 OK (mapa de arquivos vazio nao inventa ferramenta)")
    return True


# Controle: arquivo que NAO e .py/.sh/.ps1 (ex.: .md ou .cpp) contendo
# o literal --selftest em qualquer forma nunca e candidato - o filtro
# de extensao e a primeira linha de defesa contra documentacao/CI/
# fixtures.
def selftest_non_candidate_extension_ignored():
    declared = discover_declared_tools(
        {
            "docs/algum-plano.md": 'Rode `ferramenta.py --selftest` antes do push.\nif args[0] == "--selftest":\n',
            "tests/some_test.cpp": 'const char* flag = "--selftest";\n',
        }
    )
    if declared:
        print(f"selftest: controle EXTENSAO FALHOU (arquivo .md/.cpp nao e candidato): {declared}", file=sys.stderr)
        return False
    print("selftest: controle EXTENSAO OK (.md e .cpp nunca viram candidato, so .py/.sh/.ps1)")
    return True


# Controle add_test(...) com parenteses aninhados na mesma chamada
# (generator expression $<TARGET_FILE:x> nao tem parenteses de
# verdade, mas outra chamada pode legitimamente ter um argumento entre
# parenteses de CMake - prova que o extrator conta profundidade e para
# no fechamento certo, nao no primeiro ')' que aparecer).
def selftest_nested_parens_in_add_test_handled():
    cmake_text = (
        'add_test(NAME x COMMAND foo bar(baz) qux)\n'
        'add_test(\n'
        '    NAME real_selftest\n'
        '    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/nested.py" --selftest\n'
        ')\n'
    )
    declared = discover_declared_tools({"tools/nested.py": _PY_DISPATCH_BODY})
    orphans = find_orphans(declared, cmake_text)
    if orphans:
        print(f"selftest: controle PARENTESES-ANINHADOS FALHOU: {orphans}", file=sys.stderr)
        return False
    blocks = extract_add_test_blocks(cmake_text)
    if len(blocks) != 2:
        print(f"selftest: controle PARENTESES-ANINHADOS FALHOU (esperava 2 blocos, veio {len(blocks)}): {blocks}", file=sys.stderr)
        return False
    print("selftest: controle PARENTESES-ANINHADOS OK (extrator para no fechamento certo, 2 blocos distintos)")
    return True


# MEDIDO, NUNCA SUPOSTO: roda o discover_declared_tools() de verdade
# (mesmo caminho de codigo do real_main) contra o working tree real
# deste repositorio, e verifica que zero arquivo fora de tests/tools/,
# tools/, tools/ci/ e tools/win-vm-lab/ e classificado - ou seja, que
# nenhum .md/.txt/.yml/.cpp (ja filtrados por extensao) e, mais
# importante, nenhum .py/.sh/.ps1 SEM despacho real (fixtures,
# scripts de instalacao, diagnostico) e classificado por engano. Pula,
# declarado e contado (nunca calado), quando `git` nao funciona neste
# ambiente (ex.: arvore extraida sem historico) - a MESMA convencao de
# skip declarado que check_spdx.py/check_dup_laws.py ja usam para
# controles que precisam de um repositorio git de verdade.
def selftest_measured_false_positive_count_on_real_tree():
    here = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(here, "..", ".."))
    all_paths, ok = scanned_paths(repo_root)
    if not ok or not all_paths:
        print("selftest: controle MEDIDO-ARVORE-REAL PULADO (sem repositorio git utilizavel aqui - declarado, nao calado)")
        return True
    candidate_paths = [p for p in all_paths if p.endswith(_CANDIDATE_SUFFIXES)]
    file_texts = {p: read_text_lenient(os.path.join(repo_root, p)) for p in candidate_paths}
    declared = discover_declared_tools(file_texts)
    # Diretorios onde uma ferramenta com autoteste de verdade mora
    # legitimamente neste projeto - lista MEDIDA em 10/09/2026 (tests/
    # container/, tests/tools/, tools/, tools/ci/, tools/win-vm-lab/),
    # nao uma verdade fixa "por definicao": e um retrato da arvore
    # naquela data, e a arvore cresce. tests/container/ entrou nela em
    # 11/09/2026, menos de 24h depois da medicao anterior, quando a
    # fatia 1 criou tests/container/exec_fixture.sh - uma ferramenta
    # legitima fora dos quatro diretorios de entao. Um achado FORA
    # desta lista e UMA DE DUAS COISAS, nunca presumida sem checar
    # qual: (a) falso positivo do detector (comentario/banner que
    # parece despacho de flag, ver os selftests acima), ou (b) um
    # diretorio novo e legitimo que a lista ainda nao capturou. O
    # conserto do caso (b) e ACRESCENTAR o diretorio aqui, com a data,
    # nunca silenciar ou alargar o controle para aceitar qualquer
    # caminho - isso mataria o proprio proposito dele, que e pegar o
    # caso (a).
    allowed_prefixes = ("tests/container/", "tests/tools/", "tools/")
    false_positives = [relpath for relpath, _lang, _flag in declared if not relpath.startswith(allowed_prefixes)]
    print(
        f"selftest: controle MEDIDO-ARVORE-REAL: {len(candidate_paths)} candidato(s) .py/.sh/.ps1 "
        f"na arvore real, {len(declared)} classificado(s) como ferramenta com autoteste proprio, "
        f"{len(false_positives)} falso(s) positivo(s) fora de {', '.join(allowed_prefixes)}"
    )
    if false_positives:
        print(
            "selftest: controle MEDIDO-ARVORE-REAL FALHOU "
            f"(achado fora da lista permitida {allowed_prefixes}): {false_positives}. "
            "Isto significa UMA DE DUAS COISAS - (a) falso positivo do detector "
            "(ferramenta classificada por engano, corrija discover_declared_tools), ou "
            "(b) diretorio novo e legitimo (acrescente-o a allowed_prefixes, com a data, "
            "neste arquivo). NUNCA alargue allowed_prefixes para um prefixo generico so "
            "para silenciar isto - o controle existe para pegar o caso (a).",
            file=sys.stderr,
        )
        return False
    return True


# Controle: uma excecao valida (o caminho esta genuinamente entre os
# orfaos) e' aplicada e some da lista de "orfaos sem dono".
def selftest_exception_applied_removes_from_remaining():
    orphans = [("tests/tools/check_plan_scope_diff.py", "--selftest")]
    exceptions, errors = parse_exceptions_text(
        "tests/tools/check_plan_scope_diff.py|motivo de teste|PLAN-SCOPE-COLUMNS\n"
    )
    if errors:
        print(f"selftest: controle EXCECAO-APLICADA FALHOU (parse nao deveria ter erro): {errors}", file=sys.stderr)
        return False
    applied, remaining, stale = apply_exceptions(orphans, exceptions)
    if len(applied) != 1 or remaining or stale:
        print(f"selftest: controle EXCECAO-APLICADA FALHOU: applied={applied} remaining={remaining} stale={stale}", file=sys.stderr)
        return False
    print("selftest: controle EXCECAO-APLICADA OK (orfao coberto por excecao sai de 'sem dono')")
    return True


# VERMELHO - excecao OBSOLETA: o arquivo de excecoes cita um caminho
# que NAO esta mais entre os orfaos (foi registrado de verdade, ou
# deixou de existir/declarar autoteste) - tem que reprovar, nunca
# ficar esquecida.
def selftest_stale_exception_reproves():
    orphans = []  # nada orfao nesta rodada - a ferramenta foi registrada
    exceptions, _errors = parse_exceptions_text(
        "tests/tools/algo_ja_registrado.py|motivo velho|ALGUM-ITEM\n"
    )
    applied, remaining, stale = apply_exceptions(orphans, exceptions)
    if not stale:
        print("selftest: VERMELHO EXCECAO-OBSOLETA FALHOU (deveria ter marcado a excecao como obsoleta)", file=sys.stderr)
        return False
    if stale != ["tests/tools/algo_ja_registrado.py"]:
        print(f"selftest: VERMELHO EXCECAO-OBSOLETA FALHOU (nome errado): {stale}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO EXCECAO-OBSOLETA OK (excecao morta pega e nomeada): {stale}")
    return True


# Controle: excecao mal formada (numero errado de campos separados por
# '|', ou campo vazio) reprova na LEITURA, nunca vira excecao valida
# por acidente de parsing tolerante.
def selftest_malformed_exception_line_reproves():
    _exceptions, errors = parse_exceptions_text("caminho/sem/motivo/nem/item\n")
    if not errors:
        print("selftest: controle EXCECAO-MALFORMADA FALHOU (linha sem 3 campos deveria reprovar o parse)", file=sys.stderr)
        return False
    _exceptions2, errors2 = parse_exceptions_text("caminho/ok.py||ITEM\n")
    if not errors2:
        print("selftest: controle EXCECAO-MALFORMADA FALHOU (motivo vazio deveria reprovar o parse)", file=sys.stderr)
        return False
    print(f"selftest: controle EXCECAO-MALFORMADA OK (forma invalida sempre reportada, nunca engolida): {errors} / {errors2}")
    return True


def selftest_main():
    controls = [
        selftest_positive_control_registered_tools_no_orphans(),
        selftest_orphan_reproves_by_name(),
        selftest_path_without_flag_still_orphan(),
        selftest_same_basename_different_dir_not_confused(),
        selftest_comment_only_mention_never_flagged(),
        selftest_empty_file_map_yields_zero_declared(),
        selftest_non_candidate_extension_ignored(),
        selftest_nested_parens_in_add_test_handled(),
        selftest_measured_false_positive_count_on_real_tree(),
        selftest_exception_applied_removes_from_remaining(),
        selftest_stale_exception_reproves(),
        selftest_malformed_exception_line_reproves(),
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
        fail("usage: check_selftest_orphan.py --check <repo-root-directory>  |  --selftest")


if __name__ == "__main__":
    main()
