#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# seletor_selftest.py - autoteste do seletor unico do espelho e do CI (EFIC-SELETOR, lote de eficacia,
# D-W8-1231). Monta repositorios git temporarios sob TMPDIR e roda tools/ci/seletor.py por subprocesso, com
# um JSON de ctest e um codemodel de fixture pequenos. So biblioteca padrao (L-07 do projeto).
#
# Casos (do plano de E1, secao 4):
#   1 positivo: so tests/x_test.cpp mudou -> subconjunto, so o teste x
#   2 negativo: CMakeLists.txt mudou -> completo
#   3 vazio: zero caminhos -> completo com motivo; subconjunto sem teste nem lint -> resolver sai com 1
#   4 texto lido: docs/a.md citado por um script de tests/tools -> subconjunto, nunca texto-sem-leitor
#   5 texto sem leitor: docs/b.md que ninguem cita nem varre -> texto-sem-leitor
#   6 varredura nao declarada: script novo com os.walk fora da lista -> sai com 2; com a linha, passa
#   7 arquivo nao rastreado: com --arvore, conta
#   7b dado de teste: tests/fixtures/c.txt que ninguem cita -> completo (C4b), nunca texto-sem-leitor
#   7c renomeacao com nome acentuado: os dois caminhos contam (leitura com -z)
#   7d base inexistente e base nao ancestral: completo com motivo, sem erro
#   7e linha orfa na lista (script que nao existe) -> sai com 2
#   8 teste sem 'command' no json-v1 (glintfx_add_test): o alvo se acha pelo nome do teste
#   9 leitor C3 seleciona o alvo que o tem; leitor C1 deixa o caminho C5 em completo
#   A1 docs/z.md que casa glob da lista e ninguem cita: subconjunto
#   A6 script que varre por argv sem linha na lista sai com 2
#
# O codigo deste arquivo escreve os scripts que pegam no caso 6 em tempo de execucao, dentro do repositorio
# temporario: como arquivo rastreado da arvore real, ele reprovaria a propria arvore.

import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from selftest_git_fence import make_fenced_scratch_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SELETOR = os.path.join(ROOT, "tools", "ci", "seletor.py")

GIT_ID = [
    "-c", "user.name=seletor-selftest",
    "-c", "user.email=seletor-selftest@invalid",
    "-c", "commit.gpgsign=false",
    "-c", "core.hooksPath=/dev/null",
]

LISTA_FIXTURE = (
    "# SPDX-License-Identifier: AGPL-3.0-or-later\n"
    "tests/tools/lendo.py tests/*\n"
)

BASE_FILES = {
    "CMakeLists.txt": "cmake_minimum_required(VERSION 3.20)\n",
    # registro CMake: lendo_test cita tests/tools/lendo.py (o consumidor que o piso exige)
    "tests/CMakeLists.txt": "add_test(NAME lendo_test COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/lendo.py)\n",
    "tests/x_test.cpp": "int main() { return 0; }\n",
    "tests/y_test.cpp": "int main() { return 0; }\n",
    "tests/w_test.cpp": "int main() { return 0; }\n",
    "tests/tools/lendo.py": "# le docs/a.md\nprint('ok')\n",
    "docs/a.md": "# a\n",
    "docs/b.md": "# b\n",
    "tests/fixtures/c.txt": "dado\n",
    "tools/ci/varreduras-de-arvore.txt": LISTA_FIXTURE,
}

CTEST_FIXTURE = {
    "tests": [
        {"name": "x_test", "command": ["/build/tests/x_test"]},
        {"name": "y_test", "command": ["/build/tests/y_test"]},
        {"name": "lendo_test", "command": ["python3", "tests/tools/lendo.py"]},
        # como glintfx_add_test no ctest real: o json-v1 nao traz 'command'; o nome e o do alvo
        {"name": "w_test"},
    ]
}

# target -> lista de fontes, como o codemodel v2 do CMake as descreve.
TARGETS_FIXTURE = {
    "x_test": ["tests/x_test.cpp"],
    "y_test": ["tests/y_test.cpp"],
    "w_test": ["tests/w_test.cpp"],
}

RESULTS = []


def check(name, ok, detail=""):
    """Imprime o resultado e, numa condicao falsa, sai com 1 na hora (o gate de efeito do check exige isso)."""
    RESULTS.append((name, bool(ok)))
    status = "OK" if ok else "FALHOU"
    suffix = f" - {detail}" if detail and not ok else ""
    print(f"selftest: {name}: {status}{suffix}")
    if not ok:
        sys.exit(1)


def git(repo, *args):
    proc = subprocess.run(
        ["git", *GIT_ID, "-C", repo, *args],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
    )
    if proc.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)} saiu {proc.returncode}: {proc.stderr.strip()}")
    return proc.stdout


def head_of(repo, rev="HEAD"):
    return git(repo, "rev-parse", rev).strip()


def write_files(repo, files):
    for rel, text in files.items():
        path = os.path.join(repo, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)


def make_repo(scratch, name, files):
    repo = os.path.join(scratch, name)
    os.makedirs(repo)
    git(repo, "init", "-q", "-b", "main")
    write_files(repo, files)
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", "base")
    return repo


def commit_all(repo, message="mudanca"):
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", message)
    return head_of(repo)


def make_build(scratch, targets):
    """Cria um diretorio de construcao de fixture com a resposta da File API (codemodel-v2)."""
    build = os.path.join(scratch, "build")
    reply = os.path.join(build, ".cmake", "api", "v1", "reply")
    os.makedirs(reply)
    index = {"reply": {"codemodel-v2": {"jsonFile": "codemodel-v2-1.json", "kind": "codemodel", "version": {"major": 2, "minor": 6}}}}
    entries = []
    for name, sources in targets.items():
        target_file = f"target-{name}-Debug.json"
        entries.append({"name": name, "jsonFile": target_file})
        body = {"name": name, "type": "EXECUTABLE", "sources": [{"path": p, "compileGroupIndex": 0} for p in sources]}
        with open(os.path.join(reply, target_file), "w", encoding="utf-8") as handle:
            json.dump(body, handle)
    codemodel = {"configurations": [{"name": "Debug", "targets": entries}]}
    with open(os.path.join(reply, "index-1.json"), "w", encoding="utf-8") as handle:
        json.dump(index, handle)
    with open(os.path.join(reply, "codemodel-v2-1.json"), "w", encoding="utf-8") as handle:
        json.dump(codemodel, handle)
    return build


def write_ctest(scratch, data=CTEST_FIXTURE):
    path = os.path.join(scratch, "ctest.json")
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle)
    return path


def run_seletor(*args):
    proc = subprocess.run(
        [sys.executable, SELETOR, *args],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
    )
    return proc.returncode, proc.stdout, proc.stderr


def classificar(repo, scratch, label, *extra):
    saida = os.path.join(scratch, f"{label}.json")
    rc, out, err = run_seletor("classificar", "--raiz", repo, "--saida", saida, *extra)
    data = None
    if os.path.exists(saida):
        with open(saida, encoding="utf-8") as handle:
            data = json.load(handle)
    return rc, data, out + err


def resolver(scratch, label, mudancas_path, build, ctest_path):
    testes = os.path.join(scratch, f"{label}-testes.txt")
    lint = os.path.join(scratch, f"{label}-lint.txt")
    rc, out, err = run_seletor(
        "resolver", "--mudancas", mudancas_path, "--build", build,
        "--ctest-json", ctest_path, "--saida-testes", testes, "--saida-lint", lint,
    )
    tests = []
    if os.path.exists(testes):
        with open(testes, encoding="utf-8") as handle:
            tests = [line.strip() for line in handle if line.strip()]
    return rc, tests, out + err


def caso_positivo(scratch):
    repo = make_repo(scratch, "positivo", BASE_FILES)
    base = head_of(repo)
    with open(os.path.join(repo, "tests", "x_test.cpp"), "a", encoding="utf-8") as handle:
        handle.write("// comentario\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "positivo", "--base", base, "--head", head)
    check("1 positivo: classifica subconjunto", rc == 0 and data and data["modo"] == "subconjunto",
          f"rc={rc} {text.strip()}")
    check("1 positivo: so tests/x_test.cpp, classe C3",
          data and data["caminhos"] == ["tests/x_test.cpp"]
          and data["por_caminho"][0]["classe"] == "C3", str(data))
    build = make_build(scratch, TARGETS_FIXTURE)
    ctest = write_ctest(scratch)
    rc_r, tests, text_r = resolver(scratch, "positivo", os.path.join(scratch, "positivo.json"), build, ctest)
    check("1 positivo: resolver seleciona so x_test", rc_r == 0 and tests == ["x_test"],
          f"rc={rc_r} testes={tests} {text_r.strip()}")


def caso_teste_sem_command(scratch):
    repo = make_repo(scratch, "sem_command", BASE_FILES)
    base = head_of(repo)
    with open(os.path.join(repo, "tests", "w_test.cpp"), "a", encoding="utf-8") as handle:
        handle.write("// w\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "sem_command", "--base", base, "--head", head)
    check("8 teste sem command: C3 mapeia pelo nome do alvo", rc == 0 and data and data["modo"] == "subconjunto",
          f"rc={rc} {text.strip()}")
    build = make_build(scratch, TARGETS_FIXTURE)
    ctest = write_ctest(scratch)
    rc_r, tests, text_r = resolver(scratch, "sem_command", os.path.join(scratch, "sem_command.json"), build, ctest)
    check("8 teste sem command: resolver seleciona w_test (e so ele)", rc_r == 0 and tests == ["w_test"],
          f"rc={rc_r} testes={tests} {text_r.strip()}")




def caso_glob_sem_citacao(scratch):
    # A1: docs/z.md que ninguem cita, mas que casa um glob da lista, nunca e texto-sem-leitor
    files = dict(BASE_FILES)
    files["docs/z.md"] = "# z\n"
    files["tools/ci/varreduras-de-arvore.txt"] = LISTA_FIXTURE + "tests/tools/lendo.py docs/*\n"
    repo = make_repo(scratch, "glob_sem_citacao", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "z.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "glob_sem_citacao", "--base", base, "--head", head)
    leitores = data["por_caminho"][0].get("leitores", []) if data else []
    check("A1 glob sem citacao: docs/z.md -> subconjunto, nunca texto-sem-leitor",
          rc == 0 and data and data["modo"] == "subconjunto" and "tests/tools/lendo.py" in leitores,
          f"rc={rc} modo={data and data['modo']} leitores={leitores} {text.strip()}")


def caso_argv_sem_linha(scratch):
    # A6: script que varre por argv (["git", ..., "ls-files"]) sem linha na lista sai com 2
    files = dict(BASE_FILES)
    files["tests/tools/argv_varre.py"] = (
        "# varre a arvore por argv\nimport subprocess\nsubprocess.run([\"git\", \"-C\", \".\", \"ls-files\"])\n")
    repo = make_repo(scratch, "argv_sem_linha", files)
    rc, _data, text = classificar(repo, scratch, "argv_sem_linha", "--arvore")
    check("A6 argv sem linha: script ['git', ..., 'ls-files'] sai com 2",
          rc == 2 and "argv_varre.py" in text, f"rc={rc} {text.strip()}")
    with open(os.path.join(repo, "tools", "ci", "varreduras-de-arvore.txt"), "a", encoding="utf-8") as handle:
        handle.write("tests/tools/argv_varre.py tests/*\n")
    rc2, _d2, text2 = classificar(repo, scratch, "argv_com_linha", "--arvore")
    check("A6 argv com linha: passa", rc2 == 0, f"rc={rc2} {text2.strip()}")



def caso_leitor_c3(scratch):
    # Decisao do CTO (rodada 3): .cpp que cita o doc so em COMENTARIO nao executa nada. Seleciona so o consumidor CMake.
    files = dict(BASE_FILES)
    files["tests/leitor_test.cpp"] = "// le docs/a.md em tempo de execucao\nint main() { return 0; }\n"
    repo = make_repo(scratch, "leitor_c3", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "leitor_c3", "--base", base, "--head", head)
    entrada = data["por_caminho"][0] if data else {}
    check("9 leitor C3 em comentario nao seleciona (regra do CTO)",
          rc == 0 and data and data["modo"] == "subconjunto" and entrada.get("testes") == ["lendo_test"],
          f"rc={rc} testes={entrada.get('testes')} {text.strip()}")


def caso_leitor_c3_literal(scratch):
    # Decisao do CTO: citacao que sobra num literal de .cpp (fora de comentario) dá completo
    files = dict(BASE_FILES)
    files["tests/leitor_test.cpp"] = 'const char* doc = "docs/a.md";\nint main() { return 0; }\n'
    repo = make_repo(scratch, "leitor_c3_literal", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "leitor_c3_literal", "--base", base, "--head", head)
    check("9c leitor C3 com citacao em literal: completo, com motivo",
          rc == 0 and data and data["modo"] == "completo" and "leitor_test.cpp" in data["motivo"],
          f"rc={rc} {text.strip()}")


def caso_leitor_c1(scratch):
    # B1-C1 por citacao em CODIGO (configure_file): continua dando completo, com a linha no motivo
    files = dict(BASE_FILES)
    files["tests/CMakeLists.txt"] = "configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/a.md out.txt)\n"
    repo = make_repo(scratch, "leitor_c1", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "leitor_c1", "--base", base, "--head", head)
    check("9 leitor C1 por configure_file: docs/a.md -> completo, com a linha no motivo",
          rc == 0 and data and data["modo"] == "completo" and "tests/CMakeLists.txt:1" in data["motivo"]
          and "configure_file" in data["motivo"], f"rc={rc} {text.strip()}")


def caso_glob_c1(scratch):
    # Decisao do CTO: documento que so casa o glob de um varredor C1 segue a regra do Script. Aqui o varredor
    # tem um add_test que o cita: seleciona esse teste -> subconjunto. (Antes da rodada 3 dava completo.)
    files = dict(BASE_FILES)
    files["tools/ci/varredor.ps1"] = "# varre a arvore em silencio\n"
    files["docs/z.md"] = "# z\n"
    files["tests/CMakeLists.txt"] = "add_test(NAME varredor_test COMMAND pwsh ${CMAKE_CURRENT_SOURCE_DIR}/../tools/ci/varredor.ps1)\n"
    files["tools/ci/varreduras-de-arvore.txt"] = LISTA_FIXTURE + "tools/ci/varredor.ps1 docs/*\n"
    repo = make_repo(scratch, "glob_c1", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "z.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "glob_c1", "--base", base, "--head", head)
    entrada = data["por_caminho"][0] if data else {}
    check("9b leitor C1 so por glob: docs/z.md -> subconjunto com varredor_test, nao completo",
          rc == 0 and data and data["modo"] == "subconjunto" and entrada.get("testes") == ["varredor_test"],
          f"rc={rc} modo={data and data['modo']} testes={entrada.get('testes')} {text.strip()}")


def caso_cmake_positivo(scratch):
    # Decisao do CTO, positivo: docs/a.md como argumento de add_test dá so o teste nomeado
    files = dict(BASE_FILES)
    files["tests/tools/lendo.py"] = "print('ok')\n"  # sem outro leitor de docs/a.md: so o add_test decide
    files["tests/CMakeLists.txt"] = ("add_test(NAME doc_test COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/docs/a.md)\n"
                                     "add_test(NAME outro_test COMMAND true)\n")
    repo = make_repo(scratch, "cmake_positivo", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "cmake_positivo", "--base", base, "--head", head)
    entrada = data["por_caminho"][0] if data else {}
    check("D positivo: add_test com docs/a.md dá so doc_test",
          rc == 0 and data and data["modo"] == "subconjunto" and entrada.get("testes") == ["doc_test"],
          f"rc={rc} modo={data and data['modo']} testes={entrada.get('testes')} {text.strip()}")


def caso_cmake_negativo(scratch):
    # Decisao do CTO, negativo: um configure_file a mais, com o mesmo arquivo, dá completo com a linha no motivo
    files = dict(BASE_FILES)
    files["tests/tools/lendo.py"] = "print('ok')\n"
    files["tests/CMakeLists.txt"] = ("add_test(NAME doc_test COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/docs/a.md)\n"
                                     "configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/a.md out.txt)\n")
    repo = make_repo(scratch, "cmake_negativo", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "cmake_negativo", "--base", base, "--head", head)
    check("D negativo: configure_file com docs/a.md dá completo, com a linha no motivo",
          rc == 0 and data and data["modo"] == "completo" and "tests/CMakeLists.txt:2" in data["motivo"]
          and "configure_file" in data["motivo"], f"rc={rc} motivo={data and data['motivo'][:200]} {text.strip()}")


def caso_cmake_piso(scratch):
    # Decisao do CTO, piso: docs/a.md citado por add_test e tambem por um script fora de comentario que nenhum
    # teste nem o CI alcança: dá completo, nunca so doc_test
    files = dict(BASE_FILES)
    files["tests/CMakeLists.txt"] = "add_test(NAME doc_test COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/docs/a.md)\n"
    files["tools/orfao.py"] = "# ninguem chama\nopen('docs/a.md')\n"
    repo = make_repo(scratch, "cmake_piso", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "cmake_piso", "--base", base, "--head", head)
    check("D piso: script orfao sem teste nem CI dá completo, nunca so doc_test",
          rc == 0 and data and data["modo"] == "completo" and "piso" in data["motivo"], f"rc={rc} {text.strip()}")


def caso_cmake_comentario_bloco(scratch):
    # Lexer: citacao so dentro de comentario de bloco CMake (#[[ ]]) ou de linha nao executa nada
    files = dict(BASE_FILES)
    files["tests/tools/lendo.py"] = "print('ok')\n"
    files["tests/CMakeLists.txt"] = ("#[[ add_test(NAME falso COMMAND docs/a.md) ]]\n"
                                     "# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/a.md out.txt)\n")
    repo = make_repo(scratch, "cmake_comentario", files)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "cmake_comentario", "--base", base, "--head", head)
    entrada = data["por_caminho"][0] if data else {}
    check("lexer CMake: citacao so em comentario nao executa nada (zero testes, sem completo)",
          rc == 0 and data and data["modo"] == "subconjunto" and entrada.get("testes") == []
          and not entrada.get("completo"), f"rc={rc} modo={data and data['modo']} {text.strip()}")



def caso_negativo(scratch):
    repo = make_repo(scratch, "negativo", BASE_FILES)
    base = head_of(repo)
    with open(os.path.join(repo, "CMakeLists.txt"), "a", encoding="utf-8") as handle:
        handle.write("# mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "negativo", "--base", base, "--head", head)
    check("2 negativo: CMakeLists.txt mudado -> completo",
          rc == 0 and data and data["modo"] == "completo", f"rc={rc} {text.strip()}")


def caso_vazio(scratch):
    repo = make_repo(scratch, "vazio", BASE_FILES)
    head = head_of(repo)
    rc, data, text = classificar(repo, scratch, "vazio", "--base", head, "--head", head)
    check("3 vazio: zero caminhos -> completo com motivo",
          rc == 0 and data and data["modo"] == "completo" and "zero caminhos" in data["motivo"],
          f"rc={rc} {text.strip()}")
    build = make_build(scratch, TARGETS_FIXTURE)
    ctest = write_ctest(scratch)
    rc_r, tests, _ = resolver(scratch, "vazio", os.path.join(scratch, "vazio.json"), build, ctest)
    check("3 vazio: completo leva todos os testes", rc_r == 0 and sorted(tests) == ["lendo_test", "w_test", "x_test", "y_test"],
          f"rc={rc_r} testes={tests}")
    # subconjunto sem teste nem lint: z_test.cpp nao esta em nenhum alvo do inventario
    files = dict(BASE_FILES)
    files["tests/z_test.cpp"] = "int main() { return 0; }\n"
    repo_z = make_repo(scratch, "vazio_z", files)
    base_z = head_of(repo_z)
    with open(os.path.join(repo_z, "tests", "z_test.cpp"), "a", encoding="utf-8") as handle:
        handle.write("// z\n")
    head_z = commit_all(repo_z)
    rc_z, data_z, _ = classificar(repo_z, scratch, "vazio_z", "--base", base_z, "--head", head_z)
    rc_rz, tests_z, text_rz = resolver(scratch, "vazio_z", os.path.join(scratch, "vazio_z.json"), build, ctest)
    check("3 vazio: subconjunto sem teste nem lint sai com 1",
          rc_z == 0 and data_z and data_z["modo"] == "subconjunto" and rc_rz == 1 and tests_z == [],
          f"classif_rc={rc_z} resolver_rc={rc_rz} testes={tests_z} {text_rz.strip()}")


def caso_texto_lido(scratch):
    repo = make_repo(scratch, "texto_lido", BASE_FILES)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "texto_lido", "--base", base, "--head", head)
    ok = rc == 0 and data and data["modo"] == "subconjunto" and data["modo"] != "texto-sem-leitor"
    leitores = data["por_caminho"][0].get("leitores", []) if data else []
    check("4 texto lido: docs/a.md -> subconjunto, leitor tests/tools/lendo.py",
          ok and "tests/tools/lendo.py" in leitores, f"rc={rc} leitores={leitores} {text.strip()}")
    build = make_build(scratch, TARGETS_FIXTURE)
    ctest = write_ctest(scratch)
    rc_r, tests, _ = resolver(scratch, "texto_lido", os.path.join(scratch, "texto_lido.json"), build, ctest)
    check("4 texto lido: o teste que cita o leitor entra", rc_r == 0 and tests == ["lendo_test"],
          f"rc={rc_r} testes={tests}")


def caso_texto_sem_leitor(scratch):
    repo = make_repo(scratch, "sem_leitor", BASE_FILES)
    base = head_of(repo)
    with open(os.path.join(repo, "docs", "b.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "sem_leitor", "--base", base, "--head", head)
    check("5 texto sem leitor: docs/b.md -> texto-sem-leitor",
          rc == 0 and data and data["modo"] == "texto-sem-leitor", f"rc={rc} {text.strip()}")
    build = make_build(scratch, TARGETS_FIXTURE)
    ctest = write_ctest(scratch)
    rc_r, tests, _ = resolver(scratch, "sem_leitor", os.path.join(scratch, "sem_leitor.json"), build, ctest)
    check("5 texto sem leitor: resolver sai 0 sem testes", rc_r == 0 and tests == [], f"rc={rc_r} testes={tests}")


def caso_varredura(scratch):
    repo = make_repo(scratch, "varredura", BASE_FILES)
    base = head_of(repo)
    with open(os.path.join(repo, "tests", "tools", "novo_varre.py"), "w", encoding="utf-8") as handle:
        handle.write("# varre a arvore\nimport os\nfor _ in os.walk('.'):\n    pass\n")
    head = commit_all(repo)
    rc, _data, text = classificar(repo, scratch, "varredura", "--base", base, "--head", head)
    check("6 varredura nao declarada: sai com 2", rc == 2 and "novo_varre.py" in text,
          f"rc={rc} {text.strip()}")
    with open(os.path.join(repo, "tools", "ci", "varreduras-de-arvore.txt"), "a", encoding="utf-8") as handle:
        handle.write("tests/tools/novo_varre.py tests/*\n")
    rc2, _data2, text2 = classificar(repo, scratch, "varredura_ok", "--base", base, "--head", head)
    check("6 varredura declarada: com a linha, passa", rc2 == 0, f"rc={rc2} {text2.strip()}")


def caso_orfa(scratch):
    repo = make_repo(scratch, "orfa", BASE_FILES)
    base = head_of(repo)
    with open(os.path.join(repo, "tools", "ci", "varreduras-de-arvore.txt"), "a", encoding="utf-8") as handle:
        handle.write("tests/tools/inexistente.py tests/*\n")
    with open(os.path.join(repo, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("mudou\n")
    head = commit_all(repo)
    rc, _data, text = classificar(repo, scratch, "orfa", "--base", base, "--head", head)
    check("7e linha orfa na lista: sai com 2", rc == 2 and "inexistente.py" in text, f"rc={rc} {text.strip()}")


def caso_arvore(scratch):
    repo = make_repo(scratch, "arvore", BASE_FILES)
    with open(os.path.join(repo, "tests", "x_novo_test.cpp"), "w", encoding="utf-8") as handle:
        handle.write("int main() { return 0; }\n")
    rc, data, text = classificar(repo, scratch, "arvore", "--arvore")
    check("7 arquivo nao rastreado: com --arvore, conta",
          rc == 0 and data and "tests/x_novo_test.cpp" in data["caminhos"], f"rc={rc} {text.strip()}")


def caso_dado_de_teste(scratch):
    repo = make_repo(scratch, "dado", BASE_FILES)
    base = head_of(repo)
    with open(os.path.join(repo, "tests", "fixtures", "c.txt"), "a", encoding="utf-8") as handle:
        handle.write("mais\n")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "dado", "--base", base, "--head", head)
    check("7b dado de teste: tests/fixtures/c.txt -> completo (C4b)",
          rc == 0 and data and data["modo"] == "completo" and data["por_caminho"][0]["classe"] == "C4b",
          f"rc={rc} {text.strip()}")


def caso_renomeacao(scratch):
    repo = make_repo(scratch, "renomeia", BASE_FILES)
    base = head_of(repo)
    git(repo, "mv", "docs/a.md", "docs/ação.md")
    head = commit_all(repo)
    rc, data, text = classificar(repo, scratch, "renomeia", "--base", base, "--head", head)
    ok = rc == 0 and data and sorted(data["caminhos"]) == sorted(["docs/a.md", "docs/ação.md"])
    check("7c renomeacao acentuada: os dois caminhos contam", ok,
          f"rc={rc} caminhos={data and data['caminhos']} {text.strip()}")


def caso_base_invalida(scratch):
    repo = make_repo(scratch, "base_invalida", BASE_FILES)
    rc, data, text = classificar(repo, scratch, "base_invalida", "--base", "0" * 40, "--head", "HEAD")
    check("7d base inexistente: completo com motivo, sem erro",
          rc == 0 and data and data["modo"] == "completo" and "base" in data["motivo"], f"rc={rc} {text.strip()}")
    # base de outro ramo, que nao e ancestral do HEAD
    repo2 = make_repo(scratch, "nao_ancestral", BASE_FILES)
    base_b = head_of(repo2)
    git(repo2, "checkout", "-q", "-b", "lado", base_b)
    with open(os.path.join(repo2, "docs", "a.md"), "a", encoding="utf-8") as handle:
        handle.write("lado\n")
    side = commit_all(repo2, "lado")
    git(repo2, "checkout", "-q", "main")
    with open(os.path.join(repo2, "docs", "b.md"), "a", encoding="utf-8") as handle:
        handle.write("main\n")
    head2 = commit_all(repo2, "main")
    rc2, data2, text2 = classificar(repo2, scratch, "nao_ancestral", "--base", side, "--head", head2)
    check("7d base nao ancestral: completo com motivo, sem erro",
          rc2 == 0 and data2 and data2["modo"] == "completo" and "ancestral" in data2["motivo"],
          f"rc={rc2} {text2.strip()}")


def main():
    scratch = make_fenced_scratch_dir("seletor-selftest-", parent=os.environ.get("TMPDIR") or None)
    try:
        for caso in (caso_positivo, caso_teste_sem_command, caso_leitor_c3, caso_leitor_c1,
                     caso_glob_sem_citacao, caso_argv_sem_linha, caso_glob_c1,
                     caso_leitor_c3_literal, caso_cmake_positivo, caso_cmake_negativo, caso_cmake_piso,
                     caso_cmake_comentario_bloco, caso_negativo, caso_vazio, caso_texto_lido, caso_texto_sem_leitor,
                     caso_varredura, caso_orfa, caso_arvore, caso_dado_de_teste, caso_renomeacao,
                     caso_base_invalida):
            sub = tempfile.mkdtemp(prefix=caso.__name__ + "-", dir=scratch)
            try:
                caso(sub)
            except Exception as exc:  # um caso que quebra conta como reprovado, nunca como pulado
                check(f"{caso.__name__}: excecao", False, repr(exc))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    falhas = [name for name, ok in RESULTS if not ok]
    print(f"selftest: {len(RESULTS)} verificacoes, {len(RESULTS) - len(falhas)} OK, {len(falhas)} falharam")
    if not RESULTS:
        print("selftest: FALHOU - zero verificacoes (L-40 do projeto)", file=sys.stderr)
        return 1
    return 1 if falhas else 0


if __name__ == "__main__":
    # Forma `--selftest` exigida pelo gate de blob (tests/tools/blob_selftests.py): roda sobre o indice.
    if sys.argv[1:] != ["--selftest"]:
        print("uso: seletor_selftest.py --selftest", file=sys.stderr)
        sys.exit(2)
    sys.exit(main())
