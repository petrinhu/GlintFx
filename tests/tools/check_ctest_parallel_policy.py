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

import re
import sys

SCRIPT_NAME = "check_ctest_parallel_policy.py"
READS_ONLY_RE = re.compile(r"#\s*glintfx-build-dir:\s*reads-only\s*-\s*\S+")
_BUILD_DIR_ARG_RE = re.compile(r'"\$\{(?:PROJECT|CMAKE)_BINARY_DIR\}(?:/[^"]*)?"')
_CTEST_CALL_RE = re.compile(r"\bctest\b[^\n]*")
_REPEAT_RE = re.compile(r"--repeat\b")
_PARALLEL_RE = re.compile(r"(?:--parallel\b|(?<![\w-])-j\s*\S)")


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


def parallel_degree_errors(name, text):
    """(erros, chamadas_paralelas)."""
    chamadas = [l for l in _code_lines(text) if (m := _CTEST_CALL_RE.search(l)) and _PARALLEL_RE.search(m.group(0))]
    if chamadas and "paralelismo:" not in text:
        return [f"{name}: chama ctest em paralelo ({len(chamadas)}x) e nao imprime 'paralelismo:' (o grau e' a linha de escopo da secao 4.8)"], len(chamadas)
    return [], len(chamadas)


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


def classify(cmake_text, nome, linha):
    """'lock' | 'serial' | 'reads-only' | None."""
    props = _properties_of(cmake_text, nome)
    if "RESOURCE_LOCK" in props:
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
    return None


def build_dir_errors(cmake_text):
    """(erros, contagens)."""
    regs = registrations_receiving_build_dir(cmake_text)
    contagens = {"total": len(regs), "lock": 0, "serial": 0, "reads-only": 0}
    errors = []
    if not regs:
        return ["varredura vazia: nenhum add_test recebe o diretorio de build em tests/CMakeLists.txt - L-40, coleta quebrada"], contagens
    for nome, linha, _bloco in regs:
        via = classify(cmake_text, nome, linha)
        if via is None:
            errors.append(
                f"tests/CMakeLists.txt:{linha}: {nome} recebe o diretorio de build e nao declara nada - "
                f"ponha RESOURCE_LOCK glintfx_build_dir (escreve) ou `# glintfx-build-dir: reads-only - <motivo>` "
                f"logo acima (so' le)"
            )
        else:
            contagens[via] += 1
    return errors, contagens


# --- veredito ---------------------------------------------------------


def run_check(cmake_text, ci_text, preci_text):
    errors, contagens = build_dir_errors(cmake_text)
    paralelas = 0
    for name, text in (("ci.yml", ci_text), ("tools/preci.sh", preci_text)):
        errors.extend(repeat_errors(name, text))
        erros, n = parallel_degree_errors(name, text)
        errors.extend(erros)
        paralelas += n
    return errors, contagens, paralelas


def real_main(args):
    if len(args) != 3:
        fail("usage: check_ctest_parallel_policy.py --check <tests/CMakeLists.txt> <ci.yml> <preci.sh>")
    errors, c, paralelas = run_check(read_text(args[0]), read_text(args[1]), read_text(args[2]))
    print(
        f"{SCRIPT_NAME}: add_test que recebem o diretorio de build: {c['total']} "
        f"(RESOURCE_LOCK: {c['lock']}, RUN_SERIAL: {c['serial']}, reads-only: {c['reads-only']}); "
        f"chamadas ctest paralelas em ci.yml/preci.sh: {paralelas}"
    )
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
    controls.append(_expect("POSITIVO (lock, serial e reads-only declarados)", not erros and c == {"total": 3, "lock": 1, "serial": 1, "reads-only": 1}, str((erros, c))))
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
    erros, _c, _p = run_check(_CMAKE_OK, '      - run: |\n          echo "paralelismo: 4"\n          ctest -j 4\n', "")
    controls.append(_expect("GRAU-IMPRESSO passa", not erros, str(erros)))
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
