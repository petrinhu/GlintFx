#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_ctest_parallel_policy.py - CI-SPLIT-PER-OS A5 (docs/plano-ci-split-
# per-os.md secao 4.8 e linha A5): a politica que impede o `ctest` paralelo
# de falsear a suite. Leitura de texto pura (L-09), tres regras:
#
#   1. NENHUMA repeticao automatica: `ctest ... --repeat` (until-pass,
#      until-fail, after-timeout) e' proibido em ci.yml e tools/preci.sh -
#      esconderia exatamente "o numero que muda" (memoria feedback_carga_
#      concorrente_falseia_suite). Cada teste roda UMA vez.
#   2. Grau impresso: se ci.yml ou tools/preci.sh chamam `ctest` com
#      `--parallel`/`-j`, o mesmo arquivo imprime "paralelismo:" (a linha de
#      escopo da secao 4.8). Sem chamada paralela, a regra e' vazia e o
#      portao imprime isso.
#   3. Estado compartilhado declarado: todo `add_test` de tests/CMakeLists.txt
#      que recebe o DIRETORIO DE BUILD (${PROJECT_BINARY_DIR} ou
#      ${CMAKE_BINARY_DIR}, inteiro ou como prefixo de caminho) ou tem
#      `RESOURCE_LOCK`/`RUN_SERIAL` no `set_tests_properties`, ou traz, nas
#      linhas logo acima, o comentario `# glintfx-build-dir: reads-only - <motivo>`
#      (so' le; medido por snapshot do diretorio de build antes/depois, ou,
#      onde nao da' para medir aqui, declarado como inferencia no motivo).
#
# Piso de varredura (L-40): imprime SEMPRE quantos add_test recebem o
# diretorio de build e quantos estao cobertos por cada via; zero registros
# reprova (varredura quebrada).
#
# Usage:
#   check_ctest_parallel_policy.py --check <tests/CMakeLists.txt> <ci.yml> <preci.sh>
#   check_ctest_parallel_policy.py --selftest

import glob
import os
import re
import sys

SCRIPT_NAME = "check_ctest_parallel_policy.py"
READS_ONLY_RE = re.compile(r"#\s*glintfx-build-dir:\s*reads-only\s*-\s*\S+")
PRIVATE_SUBDIR_RE = re.compile(r"#\s*glintfx-build-dir:\s*private-subdir\s*-\s*\S+")
LOCK_NAME = "glintfx_build_dir"
# Qualquer uso do diretorio de build num add_test, com ou sem aspas, como
# argumento ou WORKING_DIRECTORY, inclusive o do subdiretorio do proprio
# tests/ (CMAKE_CURRENT_BINARY_DIR) - CTO 29/09 (PM9, PM10, PM11).
_BUILD_DIR_ARG_RE = re.compile(r"\$\{(?:PROJECT|CMAKE|CMAKE_CURRENT)_BINARY_DIR\}")
_CTEST_CALL_RE = re.compile(r"(?:CTEST_PARALLEL_LEVEL[^\n]*|\bctest\b[^\n]*)")
_REPEAT_RE = re.compile(r"--repeat\b")
_PARALLEL_RE = re.compile(r"(?:--parallel\b|(?<![\w-])-j\s*\S|CTEST_PARALLEL_LEVEL)")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def read_text(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def _code_lines(text):
    return [l for l in text.splitlines() if not l.lstrip().startswith("#")]


# --- regras 1 e 2: ci.yml e preci.sh ------------------------------------


def repeat_errors(name, text):
    errors = []
    for linha in _code_lines(text):
        m = _CTEST_CALL_RE.search(linha)
        if m and _REPEAT_RE.search(m.group(0)):
            errors.append(f"{name}: `ctest --repeat` proibido (esconde o numero que muda; cada teste roda uma vez): {linha.strip()!r}")
    return errors


def _yaml_steps(text):
    """Blocos de passo (`      - ...`) de um YAML de workflow; texto sem passo
    (ex.: um .sh) devolve [text]."""
    partes = re.split(r"(?m)^(?=      - )", text)
    return [p for p in partes if p.strip()] if len(partes) > 1 else [text]


def parallel_degree_errors(name, text):
    """(erros, chamadas_paralelas). O grau tem de ser impresso NO MESMO passo
    (ci.yml) ou no mesmo arquivo (preci.sh, sem passos)."""
    errors = []
    total = 0
    for bloco in _yaml_steps(text):
        chamadas = [l for l in _code_lines(bloco) if (m := _CTEST_CALL_RE.search(l)) and _PARALLEL_RE.search(m.group(0))]
        total += len(chamadas)
        for linha in chamadas:
            v = re.search(r"--parallel\s+(\$\w+)", linha)
            if v and f"if (-not {v.group(1)})" not in bloco:
                errors.append(
                    f"{name}: o grau {v.group(1)} do ctest paralelo em PowerShell nao e' validado "
                    f"(`if (-not {v.group(1)}) {{ ... exit 1 }}`): variavel vazia chamaria --parallel sem numero - tem de validar"
                )
        if chamadas and name == "ci.yml":
            codigo = "\n".join(_code_lines(bloco))
            for exigido, porque in (
                ("GLINTFX_PARALELISMO", "o agregado precisa do grau (linha de escopo do plano 4.8)"),
                ("CTEST_PARALELO", "o grau vem do input ctest_paralelo do dispatch (calibracao da A5)"),
            ):
                if exigido not in codigo:
                    primeira = bloco.strip().splitlines()[0].strip()
                    errors.append(f"{name}: passo {primeira!r} chama ctest em paralelo sem {exigido}: {porque}")
        if chamadas and "paralelismo:" not in "\n".join(_code_lines(bloco)):
            primeira = bloco.strip().splitlines()[0].strip()
            errors.append(
                f"{name}: passo {primeira!r} chama ctest em paralelo e nao imprime 'paralelismo:' "
                f"(o grau e' a linha de escopo da secao 4.8)"
            )
    return errors, total


# --- regra 3: registros que recebem o diretorio de build ---------------


def _balanced_block(text, start):
    """Texto de `add_test(` ate o parentese que fecha."""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    return text[start:]


def registrations_receiving_build_dir(cmake_text):
    """[(nome, linha, bloco)] dos add_test que recebem o diretorio de build."""
    found = []
    for m in re.finditer(r"add_test\s*\(", cmake_text):
        bloco = _balanced_block(cmake_text, m.start())
        nome = re.search(r"NAME\s+(\S+)", bloco)
        if nome and _BUILD_DIR_ARG_RE.search(bloco):
            linha = cmake_text.count("\n", 0, m.start()) + 1
            found.append((nome.group(1), linha, bloco))
    return found


def _properties_of(cmake_text, name):
    props = []
    for m in re.finditer(r"set_tests_properties\s*\(", cmake_text):
        bloco = _balanced_block(cmake_text, m.start())
        if re.match(r"set_tests_properties\s*\(\s*" + re.escape(name) + r"\s", bloco):
            props.append(bloco)
    return "\n".join(props)


def _lock_names(props):
    """Nomes em RESOURCE_LOCK, com lista CMake partida por `;` e aspas retiradas
    (C1, CTO 29/09): `RESOURCE_LOCK "a;b"` e `RESOURCE_LOCK a;b` sao dois trincos."""
    nomes = []
    for m in re.finditer(r"RESOURCE_LOCK\s+", props):
        for token in props[m.end():].split():
            if re.fullmatch(r"[A-Z][A-Z_]{2,}", token):
                break
            nomes.extend(n for n in re.split(r";", token.strip('")')) if n)
    return nomes


def _has_build_dir_lock(props):
    """RESOURCE_LOCK com o nome EXATO glintfx_build_dir (PM8)."""
    return LOCK_NAME in _lock_names(props)


def classify(cmake_text, nome, linha):
    """'lock' | 'serial' | 'reads-only' | 'private-subdir' | None."""
    props = _properties_of(cmake_text, nome)
    if _has_build_dir_lock(props):
        return "lock"
    if re.search(r"RUN_SERIAL\s+(?:TRUE|1|ON)", props):
        return "serial"
    # so' o bloco de comentarios CONTIGUO logo acima do add_test (para no
    # primeiro codigo): a declaracao de um registro nao vale para o vizinho.
    linhas = cmake_text.splitlines()
    acima = []
    i = linha - 2
    while i >= 0 and (linhas[i].lstrip().startswith("#") or not linhas[i].strip()):
        acima.append(linhas[i])
        i -= 1
    if READS_ONLY_RE.search("\n".join(acima)):
        return "reads-only"
    if PRIVATE_SUBDIR_RE.search("\n".join(acima)):
        return "private-subdir"
    return None


NESTED_LOCK = "glintfx_nested_build"
NESTED_HEAVY_RE = re.compile(r"#\s*glintfx-nested-build:\s*heavy\s*-\s*\S+")


def _has_lock(props, nome_lock):
    return nome_lock in _lock_names(props)


def _comment_block_above(cmake_text, linha):
    linhas = cmake_text.splitlines()
    acima = []
    i = linha - 2
    while i >= 0 and (linhas[i].lstrip().startswith("#") or not linhas[i].strip()):
        acima.append(linhas[i])
        i -= 1
    return "\n".join(acima)


def nested_build_errors(cmake_text):
    """I1 (CTO 29/09): os portoes de build aninhado (montam um CMake + Ninja
    inteiro) declaram `# glintfx-nested-build: heavy - <motivo>` e tem
    RESOURCE_LOCK glintfx_nested_build (nome exato), e vice-versa. Trinco
    PROVISORIO: na etapa 3 vira `PROCESSORS n` medido. (erros, quantos)."""
    errors = []
    heavy = 0
    for m in re.finditer(r"add_test\s*\(", cmake_text):
        bloco = _balanced_block(cmake_text, m.start())
        nome = re.search(r"NAME\s+(\S+)", bloco)
        if not nome:
            continue
        linha = cmake_text.count("\n", 0, m.start()) + 1
        declarado = bool(NESTED_HEAVY_RE.search(_comment_block_above(cmake_text, linha)))
        travado = _has_lock(_properties_of(cmake_text, nome.group(1)), NESTED_LOCK)
        heavy += 1 if declarado and travado else 0
        if declarado and not travado:
            errors.append(f"tests/CMakeLists.txt:{linha}: {nome.group(1)} declara `glintfx-nested-build: heavy` e nao tem RESOURCE_LOCK {NESTED_LOCK} (nome exato)")
        if travado and not declarado:
            errors.append(f"tests/CMakeLists.txt:{linha}: {nome.group(1)} tem RESOURCE_LOCK {NESTED_LOCK} sem a declaracao `# glintfx-nested-build: heavy - <motivo>` logo acima")
    return errors, heavy


# Testes que o CI MEDIU acima da regua de 40 s do ctest_aggregate.py (varredura de 13265 linhas de teste
# em 48 jobs de tres runs: 37438340098, 37440865588, 37515318202). Todo teste daqui, SE registrado em
# tests/CMakeLists.txt, tem de ter RESOURCE_LOCK glintfx_nested_build: o aggregate so' ve a lentidao
# DEPOIS que ela acontece no CI, este portao a ve antes. Lista crescente: o teste que passar de 40 s
# num job entra aqui no mesmo commit do trinco (nunca se alarga a regua).
MEASURED_HEAVY = {
    "public_name_collision_test": "400,8 s Windows estatico, 37515318202",
    "pkgconfig_validate_test": "113,4 s Windows compartilhado, 37438340098",
    "pkgconfig_test": "97,3 s CachyOS estatico, 37438340098",
    "embed_win_test": "93,0 s Windows compartilhado, 37438340098",
    "win32_test_link_selftest": "92,2 s Windows estatico, 37515318202",
    "embed_test": "77,9 s CachyOS estatico, 37438340098",
    "version_matches_tag_selftest": "64,2 s Windows estatico, 37515318202",
    "install_packager_layout_test": "55,4 s Clang Fedora, 37438340098",
    "dep_zero_trace_selftest": "51,0 s Windows estatico, 37515318202",
    "gl_codegen_host_leak_test": "49,1 s Windows compartilhado, 37438340098",
    "no_target_collision_win_test": "48,0 s Windows compartilhado, 37438340098",
    "layers_selftest": "45,1 s Windows estatico, 37515318202",
    "install_includedir_win_test": "44,6 s Windows compartilhado, 37438340098",
    "dep_zero_selftest": "41,0 s Windows estatico, 37515318202",
}


def measured_heavy_errors(cmake_text, known):
    """(erros, quantos registrados e travados). Nome que nao esta registrado neste texto (par do
    outro sistema) nao conta."""
    errors = []
    travados = 0
    for nome, motivo in sorted(known.items()):
        if not re.search(r"add_test\s*\(\s*NAME\s+%s\b" % re.escape(nome), cmake_text):
            continue
        if _has_lock(_properties_of(cmake_text, nome), NESTED_LOCK):
            travados += 1
        else:
            errors.append(f"tests/CMakeLists.txt: {nome} esta em MEASURED_HEAVY ({motivo}) e nao tem RESOURCE_LOCK {NESTED_LOCK} - passa da regua de 40 s do ctest_aggregate.py")
    return errors, travados


MEASURED_HEAVY_MIN = 14  # piso LITERAL: a lista so' cresce; encolher exige editar ESTA linha, visivel no diff


def measured_heavy_floor_errors(cmake_text, known, minimo):
    """Piso da lista (L-40): nem menos nomes que o minimo, nem nome que nao esta registrado em
    tests/CMakeLists.txt (typo ou teste removido)."""
    errors = []
    if len(known) < minimo:
        errors.append(f"MEASURED_HEAVY tem {len(known)} nome(s), o piso e' {minimo} - a lista so' cresce")
    for nome in sorted(known):
        if not re.search(r"add_test\s*\(\s*NAME\s+%s\b" % re.escape(nome), cmake_text):
            errors.append(f"MEASURED_HEAVY: {nome} nao esta registrado em tests/CMakeLists.txt (grafia errada ou teste removido)")
    return errors


def private_subdir_path_errors(cmake_text):
    """Cada `private-subdir - <caminho>` e' de UM registro (PN1, CTO 29/09): dois
    registros no mesmo subdiretorio disputariam o que a declaracao diz que e' so' seu."""
    donos = {}
    for m in re.finditer(r"add_test\s*\(", cmake_text):
        bloco = _balanced_block(cmake_text, m.start())
        nome = re.search(r"NAME\s+(\S+)", bloco)
        if not nome:
            continue
        linha = cmake_text.count("\n", 0, m.start()) + 1
        d = re.search(r"#\s*glintfx-build-dir:\s*private-subdir\s*-\s*(\S+)", _comment_block_above(cmake_text, linha))
        if d:
            donos.setdefault(d.group(1), []).append(nome.group(1))
    return [
        f"private-subdir {caminho!r} declarado por mais de um registro ({', '.join(nomes)}) - cada subdiretorio privado e' de um so'"
        for caminho, nomes in donos.items() if len(nomes) > 1
    ]


def build_dir_errors(cmake_text):
    """(erros, contagens)."""
    regs = registrations_receiving_build_dir(cmake_text)
    contagens = {"total": len(regs), "lock": 0, "serial": 0, "reads-only": 0, "private-subdir": 0}
    errors = []
    if not regs:
        return ["varredura vazia: nenhum add_test recebe o diretorio de build em tests/CMakeLists.txt - L-40, coleta quebrada"], contagens
    for nome, linha, _bloco in regs:
        via = classify(cmake_text, nome, linha)
        if via is None:
            errors.append(
                f"tests/CMakeLists.txt:{linha}: {nome} recebe o diretorio de build e nao declara nada - "
                f"ponha RESOURCE_LOCK glintfx_build_dir (escreve), `# glintfx-build-dir: reads-only - <motivo>` "
                f"(so' le) ou `# glintfx-build-dir: private-subdir - <caminho>` (escreve so' num subdiretorio proprio) logo acima"
            )
        else:
            contagens[via] += 1
    return errors, contagens


# --- veredito ---------------------------------------------------------


def run_check(cmake_text, ci_text, preci_text, ps1_texts=None, known_heavy=None):
    errors, contagens = build_dir_errors(cmake_text)
    nested_erros, contagens["nested-heavy"] = nested_build_errors(cmake_text)
    errors.extend(nested_erros)
    heavy_erros, contagens["measured-heavy"] = measured_heavy_errors(cmake_text, MEASURED_HEAVY if known_heavy is None else known_heavy)
    errors.extend(heavy_erros)
    errors.extend(private_subdir_path_errors(cmake_text))
    paralelas = 0
    universo = [("ci.yml", ci_text), ("tools/preci.sh", preci_text)] + sorted((ps1_texts or {}).items())
    for name, text in universo:
        errors.extend(repeat_errors(name, text))
        erros, n = parallel_degree_errors(name, text)
        errors.extend(erros)
        paralelas += n
    return errors, contagens, paralelas


def _read_ps1(preci_path):
    """tools/ci/**/*.ps1 ao lado de tools/preci.sh (gemeo do universo, L-17)."""
    raiz = os.path.join(os.path.dirname(os.path.abspath(preci_path)), "ci")
    return {
        os.path.relpath(f, os.path.dirname(os.path.dirname(raiz))): read_text(f)
        for f in glob.glob(os.path.join(raiz, "**", "*.ps1"), recursive=True)
    }


def dispatch_input_errors(ci_text):
    """O workflow_dispatch declara o input `ctest_paralelo` (calibracao da A5, decisao do
    CTO 29/09): so' julga um workflow de verdade (com `on:`)."""
    if not re.search(r"(?m)^on:\s*$", ci_text) or not re.search(r"(?m)^  workflow_dispatch:", ci_text):
        return []
    if not re.search(r"(?m)^    inputs:\s*\n(?:      .*\n)*?      ctest_paralelo:", ci_text):
        return ["ci.yml: workflow_dispatch sem o input `ctest_paralelo` (vazio = paralelo de producao, 1 = serial)"]
    return []


def real_main(args):
    if len(args) != 3:
        fail("usage: check_ctest_parallel_policy.py --check <tests/CMakeLists.txt> <ci.yml> <preci.sh>")
    ps1 = _read_ps1(args[2])
    ci_text = read_text(args[1])
    errors, c, paralelas = run_check(read_text(args[0]), ci_text, read_text(args[2]), ps1)
    errors.extend(dispatch_input_errors(ci_text))
    errors.extend(measured_heavy_floor_errors(read_text(args[0]), MEASURED_HEAVY, MEASURED_HEAVY_MIN))
    print(
        f"{SCRIPT_NAME}: add_test que recebem o diretorio de build: {c['total']} "
        f"(RESOURCE_LOCK: {c['lock']}, RUN_SERIAL: {c['serial']}, reads-only: {c['reads-only']}, "
        f"private-subdir: {c['private-subdir']}); portoes de build aninhado com {NESTED_LOCK}: {c['nested-heavy']}; pesados medidos registrados e travados: {c['measured-heavy']}/{len(MEASURED_HEAVY)}; universo das regras 1 e 2: ci.yml, preci.sh e "
        f"{len(ps1)} .ps1 de tools/ci; chamadas ctest paralelas: {paralelas}"
    )
    if not ps1:
        errors.append("varredura vazia: nenhum tools/ci/**/*.ps1 lido - L-40, coleta quebrada")
    if errors:
        fail(f"{len(errors)} problema(s):\n  " + "\n  ".join(errors))
    print(f"{SCRIPT_NAME}: OK")


# --- selftest -------------------------------------------------------------

_CMAKE_OK = """\
# glintfx-build-dir: reads-only - so' le compile_commands.json (medido por snapshot)
add_test(
    NAME leitor_test
    COMMAND python3 ler.py "${CMAKE_BINARY_DIR}/compile_commands.json"
)
add_test(
    NAME escritor_test
    COMMAND check_consume.sh "${PROJECT_BINARY_DIR}" pkg
)
set_tests_properties(escritor_test PROPERTIES LABELS consume RESOURCE_LOCK glintfx_build_dir)
add_test(
    NAME serial_test
    COMMAND python3 trace.py "${PROJECT_BINARY_DIR}"
)
set_tests_properties(serial_test PROPERTIES LABELS consume RUN_SERIAL TRUE)
add_test(NAME sem_build_dir COMMAND echo ok)
"""

_CI_OK = "      - run: ctest --test-dir b --output-on-failure\n"


def _expect(nome, condicao, detalhe=""):
    print(f"selftest: {nome} {'OK' if condicao else 'FALHOU'}" + ("" if condicao else f" - {detalhe}"))
    return condicao


def selftest_main():
    controls = []
    erros, c, p = run_check(_CMAKE_OK, _CI_OK, "ctest x\n")
    controls.append(_expect("POSITIVO (lock, serial e reads-only declarados)", not erros and c == {"total": 3, "lock": 1, "serial": 1, "reads-only": 1, "private-subdir": 0, "nested-heavy": 0, "measured-heavy": 0}, str((erros, c))))
    sem_decl = _CMAKE_OK.replace("# glintfx-build-dir: reads-only - so' le compile_commands.json (medido por snapshot)\n", "")
    erros, _c, _p = run_check(sem_decl, _CI_OK, "")
    controls.append(_expect("REGISTRO-SEM-DECLARACAO reprova, nomeando o teste", any("leitor_test" in e for e in erros), str(erros)))
    sem_lock = _CMAKE_OK.replace(" RESOURCE_LOCK glintfx_build_dir", "")
    erros, _c, _p = run_check(sem_lock, _CI_OK, "")
    controls.append(_expect("ESCRITOR-SEM-TRINCO reprova", any("escritor_test" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK, "      - run: ctest --repeat until-pass:3\n", "")
    controls.append(_expect("REPEAT-UNTIL-PASS no ci.yml reprova", any("--repeat" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK, _CI_OK, "ctest --repeat after-timeout:2 -N\n")
    controls.append(_expect("REPEAT no preci.sh reprova", any("preci.sh" in e and "--repeat" in e for e in erros), str(erros)))
    erros, _c, p = run_check(_CMAKE_OK, "      - run: ctest --parallel 4\n", "")
    controls.append(_expect("GRAU-NAO-IMPRESSO (--parallel sem 'paralelismo:') reprova", any("paralelismo:" in e for e in erros) and p == 1, str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK, '      - run: |\n          echo "paralelismo: ${CTEST_PARALELO:-4}"\n          echo "GLINTFX_PARALELISMO=4" >> "$GITHUB_ENV"\n          ctest -j 4\n', "")
    controls.append(_expect("GRAU-IMPRESSO passa", not erros, str(erros)))
    dois = ('      - run: |\n          echo "paralelismo: 4"\n          ctest -j 4\n'
            '      - run: ctest --parallel 8\n')
    erros, _c, p = run_check(_CMAKE_OK, dois, "")
    controls.append(_expect("GRAU-IMPRESSO-SO-EM-UM-DOS-PASSOS reprova (por passo, nao por arquivo)", any("passo" in e and "paralelismo:" in e for e in erros) and p == 2, str(erros)))
    # CTO 29/09 (PM3, PM4, PM8..PM11 + private-subdir + ps1)
    erros, _c, _p = run_check(_CMAKE_OK.replace("RESOURCE_LOCK glintfx_build_dir", "RESOURCE_LOCK outro_recurso"), _CI_OK, "")
    controls.append(_expect("PM8-LOCK-COM-NOME-ERRADO reprova (so' glintfx_build_dir vale)", any("escritor_test" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK + "add_test(NAME sem_aspas COMMAND python3 x.py ${PROJECT_BINARY_DIR})\n", _CI_OK, "")
    controls.append(_expect("PM9-BUILD-DIR-SEM-ASPAS reprova", any("sem_aspas" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK + 'add_test(NAME cur COMMAND python3 x.py "${CMAKE_CURRENT_BINARY_DIR}/x")\n', _CI_OK, "")
    controls.append(_expect("PM10-CMAKE_CURRENT_BINARY_DIR reprova", any("cur" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK + "add_test(NAME wd COMMAND python3 x.py WORKING_DIRECTORY ${CMAKE_BINARY_DIR})\n", _CI_OK, "")
    controls.append(_expect("PM11-WORKING_DIRECTORY reprova", any("wd" in e for e in erros), str(erros)))
    priv = "# glintfx-build-dir: private-subdir - tests/x_out/\nadd_test(NAME priv_test COMMAND python3 x.py \"${CMAKE_CURRENT_BINARY_DIR}/x_out\")\n"
    erros, c, _p = run_check(_CMAKE_OK + priv, _CI_OK, "")
    controls.append(_expect("PRIVATE-SUBDIR declarado passa e e' contado", not erros and c.get("private-subdir") == 1, str((erros, c))))
    erros, _c, p = run_check(_CMAKE_OK, "      - run: CTEST_PARALLEL_LEVEL=4 ctest\n", "")
    controls.append(_expect("PM3-CTEST_PARALLEL_LEVEL sem grau impresso reprova", any("paralelismo:" in e for e in erros) and p == 1, str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK, "      - run: |\n          # paralelismo: 4\n          ctest -j 4\n", "")
    controls.append(_expect("PM4-PARALELISMO-SO-EM-COMENTARIO reprova", any("paralelismo:" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK, _CI_OK, "", ps1_texts={"tools/ci/x.ps1": "ctest --repeat until-pass:2\n"})
    controls.append(_expect("PS1-COM-REPEAT reprova (universo inclui tools/ci/**/*.ps1)", any("x.ps1" in e and "--repeat" in e for e in erros), str(erros)))
    # I1 (CTO 29/09): portoes de build aninhado ("heavy") tem o trinco provisorio
    # glintfx_nested_build (nome exato), declarado por comentario.
    heavy = ("# glintfx-nested-build: heavy - monta um CMake + Ninja inteiro (35 a 115 s serial)\n"
             "add_test(NAME pesado_test COMMAND check_x.sh)\n"
             "set_tests_properties(pesado_test PROPERTIES LABELS consume RESOURCE_LOCK glintfx_nested_build)\n")
    erros, c, _p = run_check(_CMAKE_OK + heavy, _CI_OK, "")
    controls.append(_expect("NESTED-BUILD declarado com o trinco exato passa e e' contado", not erros and c.get("nested-heavy") == 1, str((erros, c))))
    erros, _c, _p = run_check(_CMAKE_OK + heavy.replace("glintfx_nested_build)", "outro_nome)"), _CI_OK, "")
    controls.append(_expect("NESTED-BUILD com trinco de nome errado reprova", any("pesado_test" in e and "glintfx_nested_build" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK + heavy.replace(" RESOURCE_LOCK glintfx_nested_build", ""), _CI_OK, "")
    controls.append(_expect("NESTED-BUILD declarado heavy SEM o trinco reprova", any("pesado_test" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK + heavy.replace("# glintfx-nested-build: heavy - monta um CMake + Ninja inteiro (35 a 115 s serial)\n", ""), _CI_OK, "")
    controls.append(_expect("TRINCO glintfx_nested_build SEM a declaracao heavy reprova", any("pesado_test" in e and "heavy" in e for e in erros), str(erros)))
    # W8-CI-PESADOS (run 37515318202): a regua de 40 s do ctest_aggregate.py e' MEDIDA no CI, e um
    # teste que ja' passou dela uma vez e' registrado em MEASURED_HEAVY: tem de ter o trinco.
    conhecidos = {"lento_test": "ate 64 s medido"}
    lento = "add_test(NAME lento_test COMMAND python3 x.py)\n"
    erros, c, _p = run_check(_CMAKE_OK + lento, _CI_OK, "", known_heavy=conhecidos)
    controls.append(_expect("MEDIDO-PESADO sem trinco reprova, nomeando o teste", any("lento_test" in e and "MEASURED_HEAVY" in e for e in erros), str(erros)))
    com = ("# glintfx-nested-build: heavy - 64 s medido\n" + lento
           + "set_tests_properties(lento_test PROPERTIES LABELS selftest RESOURCE_LOCK glintfx_nested_build)\n")
    erros, c, _p = run_check(_CMAKE_OK + com, _CI_OK, "", known_heavy=conhecidos)
    controls.append(_expect("MEDIDO-PESADO com trinco passa e e' contado", not erros and c.get("measured-heavy") == 1, str((erros, c))))
    erros, c, _p = run_check(_CMAKE_OK, _CI_OK, "", known_heavy=conhecidos)
    controls.append(_expect("MEDIDO-PESADO nao registrado (so' do outro sistema) nao reprova e conta zero", not erros and c.get("measured-heavy") == 0, str((erros, c))))
    # Piso da lista (revisao de c9b70ba, furo 3): encolher a lista ou errar a grafia de um nome reprova.
    erros = measured_heavy_floor_errors(_CMAKE_OK + com, conhecidos, 1)
    controls.append(_expect("PISO-DA-LISTA com o nome registrado e a contagem minima passa", not erros, str(erros)))
    erros = measured_heavy_floor_errors(_CMAKE_OK + com, {}, 1)
    controls.append(_expect("PISO-DA-LISTA encolhida (um nome removido) reprova", any("MEASURED_HEAVY tem 0" in e for e in erros), str(erros)))
    erros = measured_heavy_floor_errors(_CMAKE_OK + com, {"lento_testt": "typo"}, 1)
    controls.append(_expect("PISO-DA-LISTA com nome de grafia errada (nao registrado) reprova, nomeando-o", any("lento_testt" in e for e in erros), str(erros)))
    # PN1 (CTO 29/09): caminhos private-subdir unicos; PN: variavel de grau do PowerShell validada
    dup = ("# glintfx-build-dir: private-subdir - tests/x_out/\nadd_test(NAME p1_test COMMAND python3 x.py \"${CMAKE_CURRENT_BINARY_DIR}/x_out\")\n"
           "# glintfx-build-dir: private-subdir - tests/x_out/\nadd_test(NAME p2_test COMMAND python3 y.py \"${CMAKE_CURRENT_BINARY_DIR}/x_out\")\n")
    erros, _c, _p = run_check(_CMAKE_OK + dup, _CI_OK, "")
    controls.append(_expect("PN1-PRIVATE-SUBDIR-DUPLICADO reprova, nomeando os dois", any("p1_test" in e and "p2_test" in e for e in erros), str(erros)))
    ps_sem = '      - shell: pwsh\n        run: |\n          $n = $env:CTEST_PARALELO\n          Write-Host "paralelismo: $n"\n          Add-Content $env:GITHUB_ENV "GLINTFX_PARALELISMO=$n"\n          ctest --parallel $n\n'
    erros, _c, _p = run_check(_CMAKE_OK, ps_sem, "")
    controls.append(_expect("PN-GRAU-POWERSHELL-SEM-VALIDACAO reprova", any("$n" in e and "validar" in e for e in erros), str(erros)))
    ps_com = ps_sem.replace("          ctest --parallel $n", "          if (-not $n) { Write-Error 'sem grau'; exit 1 }\n          ctest --parallel $n")
    erros, _c, _p = run_check(_CMAKE_OK, ps_com, "")
    controls.append(_expect("PN-GRAU-POWERSHELL-VALIDADO passa", not erros, str(erros)))
    # C1 (CTO 29/09): a lista de RESOURCE_LOCK e' partida por `;` (CMake), com ou sem aspas
    comp = _CMAKE_OK.replace("RESOURCE_LOCK glintfx_build_dir", 'RESOURCE_LOCK "glintfx_build_dir;outro_recurso"')
    erros, c, _p = run_check(comp, _CI_OK, "")
    controls.append(_expect("C1-LOCK-COMPOSTO com aspas e ';' conta como trinco", not erros and c["lock"] == 1, str((erros, c))))
    comp2 = _CMAKE_OK.replace("RESOURCE_LOCK glintfx_build_dir", "RESOURCE_LOCK outro_recurso;glintfx_build_dir")
    erros, c, _p = run_check(comp2, _CI_OK, "")
    controls.append(_expect("C1-LOCK-COMPOSTO sem aspas conta como trinco", not erros and c["lock"] == 1, str((erros, c))))
    comp3 = _CMAKE_OK.replace("RESOURCE_LOCK glintfx_build_dir", 'RESOURCE_LOCK "glintfx_build_dir_x;outro"')
    erros, _c, _p = run_check(comp3, _CI_OK, "")
    controls.append(_expect("C1-LOCK-COMPOSTO sem o nome exato continua reprovando", any("escritor_test" in e for e in erros), str(erros)))
    # Etapa 3: o grau vira env do job (GLINTFX_PARALELISMO em $GITHUB_ENV) e o dispatch tem o input.
    passo_ok = ('      - run: |\n          paralelismo="${CTEST_PARALELO:-4}"\n          echo "paralelismo: $paralelismo"\n'
                '          echo "GLINTFX_PARALELISMO=$paralelismo" >> "$GITHUB_ENV"\n          ctest --parallel "$paralelismo"\n')
    erros, _c, _p = run_check(_CMAKE_OK, passo_ok, "")
    controls.append(_expect("GRAU-EM-GITHUB_ENV passa", not erros, str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK, passo_ok.replace('          echo "GLINTFX_PARALELISMO=$paralelismo" >> "$GITHUB_ENV"\n', ""), "")
    controls.append(_expect("GRAU-SEM-GITHUB_ENV reprova (o agregado precisa do grau)", any("GLINTFX_PARALELISMO" in e for e in erros), str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK, passo_ok.replace("${CTEST_PARALELO:-4}", "4"), "")
    controls.append(_expect("GRAU-SEM-INPUT-CTEST_PARALELO reprova", any("CTEST_PARALELO" in e for e in erros), str(erros)))
    dispatch = "on:\n  push:\n    branches: [main]\n  workflow_dispatch:\n"
    erros = dispatch_input_errors(dispatch)
    controls.append(_expect("DISPATCH-SEM-INPUT ctest_paralelo reprova", any("ctest_paralelo" in e for e in erros), str(erros)))
    erros = dispatch_input_errors(dispatch + "    inputs:\n      ctest_paralelo:\n        default: ''\n")
    controls.append(_expect("DISPATCH-COM-INPUT passa", not erros, str(erros)))
    erros, _c, _p = run_check(_CMAKE_OK, "      # ctest --repeat until-pass\n" + _CI_OK, "")
    controls.append(_expect("COMENTARIO com --repeat nao reprova", not erros, str(erros)))
    erros, c, _p = run_check("add_test(NAME x COMMAND echo)\n", _CI_OK, "")
    controls.append(_expect("VARREDURA-VAZIA (0 registros com o build dir) reprova", any("varredura vazia" in e for e in erros) and c["total"] == 0, str(erros)))
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
        fail("usage: check_ctest_parallel_policy.py --check <tests/CMakeLists.txt> <ci.yml> <preci.sh>  |  --selftest")


if __name__ == "__main__":
    main()
