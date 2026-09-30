#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_python_unbuffered.py - TODO.md item LAYERS-ORACLE-REPORT, sub-
# fatia J2 (docs/plano-w7c-adendo-revalidacao.md secao 3.J, achado N9,
# decisao D-A11, 24/09/2026).
#
# O FATO MEDIDO: nenhum portao Python registrado no ctest deste projeto
# roda sem buffer (`grep -n PYTHONUNBUFFERED` em tests/CMakeLists.txt,
# ci.yml e tools/preci.sh dava zero antes desta fatia). Sob `ctest`, a
# saida padrao (stdout) de um processo Python vai para um cano, nao
# para um terminal interativo - e a biblioteca padrao documenta que,
# nesse caso, stdout e' "block-buffered" enquanto stderr continua
# "line-buffered" (docs.python.org/3/library/sys.html, secao
# sys.stdout). Consequencia MEDIDA no run 35946636755: as linhas de
# erro de um portao Python aparecem no log ANTES das linhas de
# progresso normal do mesmo portao - nao e' defeito do portao, e' o
# comportamento documentado do interpretador, e vale para TODO portao
# Python deste projeto, nao so' o oraculo de camadas que a INBOX citou
# primeiro.
#
# O CONSERTO, em tests/CMakeLists.txt: `PYTHONUNBUFFERED=1` entra na
# propriedade ENVIRONMENT de TODO teste registrado no diretorio (lidos
# da propriedade de diretorio TESTS, no fim do arquivo - depois do
# ULTIMO add_test) - equivalente a chamar todo `python3`/`python` deste
# projeto com a flag `-u`. Uma variavel de ambiente extra em teste que
# nao e' Python (a maioria dos ctest daqui e' binario C++) e' inofensiva:
# nenhum executor de teste le PYTHONUNBUFFERED.
#
# ESTE SCRIPT E' O PORTAO QUE PROVA QUE O CONSERTO ACONTECEU E FICA: le
# `ctest --show-only=json-v1` (GODS_LAWS.md L-45 - o que o CTEST
# REALMENTE REGISTROU, nunca uma releitura estatica do CMakeLists.txt)
# do diretorio de build dado, e reprova se ALGUM teste registrado nao
# carrega PYTHONUNBUFFERED=1 no ENVIRONMENT dele. GODS_LAWS.md L-40:
# zero teste lido e' varredura vazia, reprova tambem.
#
# O QUE ELE NAO VE: a variavel chegar ATE o processo Python de fato (um
# runner exotico que apague o ambiente antes de invocar o interpretador
# nao seria pego por este portao, que so' confere o que o ctest DIZ que
# vai passar, nunca o processo em execucao).

import json
import sys
import tempfile
from pathlib import Path

SCRIPT_NAME = "check_python_unbuffered.py"
REQUIRED_ENV_VALUE = "PYTHONUNBUFFERED=1"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def find_environment_values(test):
    """Devolve a lista ENVIRONMENT de um teste (um item de data['tests']
    do JSON de `ctest --show-only=json-v1`), ou [] se a propriedade nao
    existir - forma pura, sem tocar disco, pra ser testada sem
    subprocess algum."""
    for prop in test.get("properties", []):
        if prop.get("name") == "ENVIRONMENT":
            return prop.get("value") or []
    return []


def missing_pythonunbuffered(data):
    """Devolve (total_de_testes, [nomes sem PYTHONUNBUFFERED=1]), lendo
    o dict ja parseado do JSON de `ctest --show-only=json-v1`."""
    tests = data.get("tests", [])
    missing = [
        test.get("name", "<sem nome>")
        for test in tests
        if REQUIRED_ENV_VALUE not in find_environment_values(test)
    ]
    return len(tests), missing


def _run_ctest_show_only(build_dir):
    import subprocess

    try:
        completed = subprocess.run(
            ["ctest", "--show-only=json-v1", "--test-dir", str(build_dir)],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as exc:
        fail(f"nao consegui executar 'ctest --show-only=json-v1 --test-dir {build_dir}': {exc}")
    if completed.returncode != 0:
        fail(
            f"'ctest --show-only=json-v1 --test-dir {build_dir}' saiu com codigo "
            f"{completed.returncode}: {completed.stderr.strip()}"
        )
    try:
        return json.loads(completed.stdout)
    except json.JSONDecodeError as exc:
        fail(f"saida de 'ctest --show-only=json-v1' nao e' JSON valido: {exc}")


def real_main(args):
    if not args or args[0] != "--compare":
        fail(f"usage: {SCRIPT_NAME} --compare <build-dir>  |  --selftest")
    if len(args) != 2:
        fail(f"usage: {SCRIPT_NAME} --compare <build-dir>  |  --selftest")
    build_dir = Path(args[1])
    if not build_dir.is_dir():
        fail(f"diretorio de build nao encontrado: {build_dir}")

    data = _run_ctest_show_only(build_dir)
    total, missing = missing_pythonunbuffered(data)
    print(f"{SCRIPT_NAME}: {total} teste(s) lido(s) de 'ctest --show-only=json-v1 --test-dir {build_dir}'")
    if total == 0:
        fail("0 teste(s) lido(s) - varredura vazia (GODS_LAWS.md L-40)")
    if missing:
        fail(
            f"{len(missing)} de {total} teste(s) sem {REQUIRED_ENV_VALUE!r} no ENVIRONMENT: "
            f"{sorted(missing)!r}"
        )
    print(f"{SCRIPT_NAME}: os {total} teste(s) carregam {REQUIRED_ENV_VALUE!r}")


# -- selftest --------------------------------------------------------

def _fake_show_only_json(tests):
    return {"kind": "ctestInfo", "version": {"major": 1, "minor": 0}, "tests": tests}


def _test_entry(name, environment=None, command=None):
    properties = []
    if environment is not None:
        properties.append({"name": "ENVIRONMENT", "value": environment})
    return {
        "name": name,
        "command": command or [f"/build/tests/{name}"],
        "properties": properties,
    }


def selftest_positive_control():
    """GREEN: todo teste com PYTHONUNBUFFERED=1 no ENVIRONMENT dele -
    zero faltando."""
    data = _fake_show_only_json(
        [
            _test_entry("version_test", environment=["PYTHONUNBUFFERED=1"]),
            _test_entry(
                "layers_oracle_test",
                environment=["PYTHONUNBUFFERED=1", "GLINTFX_LAYERS_ORACLE=ON"],
            ),
        ]
    )
    total, missing = missing_pythonunbuffered(data)
    if total != 2 or missing:
        print(f"selftest: POSITIVO FALHOU (total={total}, missing={missing!r})", file=sys.stderr)
        return False
    print(f"selftest: POSITIVO OK ({total} teste(s), 0 faltando)")
    return True


def selftest_shell_wrapped_python_without_command_string_caught():
    """MUTANTE QUE MATA (tabela J1/J2 do adendo): um teste cujo COMANDO
    e' um roteiro de shell (run_compositor.sh) que so' chama Python por
    dentro - "command" nao contem a palavra "python" em lugar nenhum -
    mas ficou SEM PYTHONUNBUFFERED=1 no ENVIRONMENT. Um portao que
    filtrasse "so' testes cujo comando contem python" nunca acusaria
    este caso; este portao tem de acusar, porque olha o ENVIRONMENT de
    TODO teste, nunca o texto do comando."""
    data = _fake_show_only_json(
        [
            _test_entry("version_test", environment=["PYTHONUNBUFFERED=1"]),
            _test_entry(
                "container_run_compositor_selftest",
                environment=[],
                command=["/bin/sh", "/build/tests/container/run_compositor.sh", "--selftest"],
            ),
        ]
    )
    total, missing = missing_pythonunbuffered(data)
    if total != 2 or missing != ["container_run_compositor_selftest"]:
        print(
            f"selftest: WRAPPER-SHELL FALHOU (total={total}, missing={missing!r}, esperava "
            f"['container_run_compositor_selftest'])",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: WRAPPER-SHELL OK (teste sem 'python' no comando, mas sem a variavel, e' "
        "acusado do mesmo jeito - o mutante 'so filtra por texto do comando' morreria aqui)"
    )
    return True


def selftest_missing_environment_property_counted():
    """Teste sem a propriedade ENVIRONMENT alguma (nunca setada) conta
    como faltando, nao como "nao se aplica"."""
    data = _fake_show_only_json([_test_entry("err_code_test", environment=None)])
    total, missing = missing_pythonunbuffered(data)
    if total != 1 or missing != ["err_code_test"]:
        print(f"selftest: SEM-ENVIRONMENT FALHOU (total={total}, missing={missing!r})", file=sys.stderr)
        return False
    print("selftest: SEM-ENVIRONMENT OK (ausencia total da propriedade conta como faltando)")
    return True


def selftest_empty_tests_list_reproves():
    """Piso de varredura vazia (GODS_LAWS.md L-40): zero teste no JSON
    tem de reprovar - nunca "nada a conferir, passa"."""
    data = _fake_show_only_json([])
    total, _missing = missing_pythonunbuffered(data)
    if total != 0:
        print(f"selftest: LISTA-VAZIA FALHOU (setup errado: total={total}, esperava 0)", file=sys.stderr)
        return False
    print("selftest: LISTA-VAZIA OK (missing_pythonunbuffered devolve total=0 - real_main() reprova aqui)")
    return True


def _make_scratch_ctest_dir(with_test=True):
    """Fabrica um build-dir MINIMO (CTestTestfile.cmake de verdade) -
    com um teste sem PYTHONUNBUFFERED quando with_test=True, ou
    COMPLETAMENTE VAZIO (zero add_test, ctest --show-only devolve
    "tests": [] de verdade - conferido ao vivo) quando with_test=False.
    Quem chama e' responsavel por limpar com shutil.rmtree."""
    scratch = Path(tempfile.mkdtemp(prefix="glintfx-python-unbuffered-selftest-"))
    content = (
        'add_test(fake_test "/bin/true")\n'
        'set_tests_properties(fake_test PROPERTIES LABELS "unit")\n'
    ) if with_test else ""
    (scratch / "CTestTestfile.cmake").write_text(content, encoding="utf-8")
    return scratch


def selftest_real_main_reproves_malformed_ctestfile():
    """Ponta a ponta (litmus 'if False' aplicado a TODO piso de
    real_main(), 24/09/2026): nenhum controle antes deste fazia o
    PROPRIO `ctest --show-only=json-v1` falhar (`completed.returncode
    != 0`) - so' a falta de build-dir era testada, e essa e' pega mais
    cedo pelo `is_dir()`. Aqui o diretorio EXISTE (passa o `is_dir()`),
    mas o CTestTestfile.cmake dentro dele e' sintaxe CMake invalida de
    proposito - ctest de verdade sai com codigo != 0 nisso (conferido
    ao vivo, RC=8). real_main() tem de sair 1."""
    import shutil

    if shutil.which("ctest") is None:
        print("selftest: REAL-MAIN-CTESTFILE-INVALIDO pulado (ctest nao esta no PATH)")
        return True
    scratch = Path(tempfile.mkdtemp(prefix="glintfx-python-unbuffered-selftest-"))
    (scratch / "CTestTestfile.cmake").write_text("isto nao e sintaxe CMake valida (((\n", encoding="utf-8")
    try:
        return _run_real_main_expecting_exit_1(scratch, "REAL-MAIN-CTESTFILE-INVALIDO")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def _probe_ctest_show_only_available(scratch):
    """Confere, com uma chamada REAL, se 'ctest --show-only=json-v1'
    roda neste ambiente contra o build-dir fabricado - devolve
    (disponivel, motivo_se_nao)."""
    import subprocess

    try:
        probe = subprocess.run(
            ["ctest", "--show-only=json-v1", "--test-dir", str(scratch)],
            capture_output=True, text=True, check=False,
        )
    except OSError as exc:
        return False, f"ctest indisponivel neste ambiente: {exc}"
    if probe.returncode != 0:
        return False, f"ctest --show-only nao rodou: {probe.stderr.strip()!r}"
    return True, ""


def selftest_real_main_reproves_missing_build_dir():
    """Ponta a ponta (litmus 'if False' aplicado a TODO piso de
    real_main(), 24/09/2026): nenhum controle antes deste passava um
    build-dir INEXISTENTE - `if not build_dir.is_dir(): fail(...)`
    desligado nunca mudava nenhum resultado, porque nenhum controle o
    exercitava. real_main() com um caminho que nunca existiu tem de
    sair 1."""
    missing_dir = Path(tempfile.gettempdir()) / "glintfx-python-unbuffered-nao-existe"
    try:
        real_main(["--compare", str(missing_dir)])
    except SystemExit as exc:
        if exc.code != 1:
            print(f"selftest: REAL-MAIN-DIR-AUSENTE FALHOU (codigo {exc.code}, esperava 1)",
                  file=sys.stderr)
            return False
        print("selftest: REAL-MAIN-DIR-AUSENTE OK (build-dir inexistente reprova, codigo 1)")
        return True
    print("selftest: REAL-MAIN-DIR-AUSENTE FALHOU (nao reprovou - esperava exit 1)", file=sys.stderr)
    return False


def _run_real_main_expecting_exit_1(scratch, label):
    """Roda real_main() ponta a ponta contra `scratch` e confere que
    sai com codigo 1 - fatorado (teto de linhas L-17) entre o controle
    de teste sem a variavel e o de lista vazia, que fazem a MESMA
    dança de SystemExit."""
    try:
        real_main(["--compare", str(scratch)])
    except SystemExit as exc:
        if exc.code != 1:
            print(f"selftest: {label} FALHOU (codigo {exc.code}, esperava 1)", file=sys.stderr)
            return False
        print(f"selftest: {label} OK (codigo 1)")
        return True
    print(f"selftest: {label} FALHOU (nao reprovou - esperava exit 1)", file=sys.stderr)
    return False


def selftest_real_main_reproves_on_missing():
    """Ponta a ponta: real_main() roda 'ctest --show-only=json-v1' de
    verdade contra um build-dir fabricado (_make_scratch_ctest_dir, sem
    PYTHONUNBUFFERED em teste nenhum) e tem de sair 1 (bate no `if
    missing: fail(...)`)."""
    import shutil

    scratch = _make_scratch_ctest_dir(with_test=True)
    try:
        available, motivo = _probe_ctest_show_only_available(scratch)
        if not available:
            print(f"selftest: REAL-MAIN pulado ({motivo})")
            return True
        return _run_real_main_expecting_exit_1(scratch, "REAL-MAIN")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def selftest_real_main_reproves_on_empty_test_list():
    """Ponta a ponta (achado da revisao main, litmus 'if False' em
    24/09/2026): selftest_empty_tests_list_reproves acima so' confere
    missing_pythonunbuffered() PURO - nunca passa pelo `if total == 0:
    fail(...)` de real_main(). Um mutante que troca esse `if` por `if
    False:` sobrevivia (nenhum controle chamava real_main() sobre um
    build-dir SEM teste nenhum). real_main() sobre
    _make_scratch_ctest_dir(with_test=False) tem de sair 1."""
    import shutil

    scratch = _make_scratch_ctest_dir(with_test=False)
    try:
        available, motivo = _probe_ctest_show_only_available(scratch)
        if not available:
            print(f"selftest: REAL-MAIN-LISTA-VAZIA pulado ({motivo})")
            return True
        return _run_real_main_expecting_exit_1(scratch, "REAL-MAIN-LISTA-VAZIA")
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_shell_wrapped_python_without_command_string_caught(),
        selftest_missing_environment_property_counted(),
        selftest_empty_tests_list_reproves(),
        selftest_real_main_reproves_missing_build_dir(),
        selftest_real_main_reproves_malformed_ctestfile(),
        selftest_real_main_reproves_on_missing(),
        selftest_real_main_reproves_on_empty_test_list(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
