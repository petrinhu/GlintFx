#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# conferir_lista_selftest.py - autoteste de tools/ci/conferir_lista.py (EFIC-CI-SELETIVO, E3, Ordem B,
# decisao D2; GODS_LAWS.md L-20 e L-40 do projeto). Os JSON de inventario, universo e selecao saem do
# `ctest --show-only=json-v1` REAL de um projeto CMake descartavel, criado em TMPDIR (nunca fixture
# colada a mao). Sem cmake ou ctest no PATH o autoteste reprova: nao ha pulo.
#
# Usage:
#   conferir_lista_selftest.py --selftest

import contextlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile

SCRIPT_NAME = "conferir_lista_selftest.py"
ALVO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ci", "conferir_lista.py"))

PROJETO_COM_TESTES = """cmake_minimum_required(VERSION 3.29)
project(probe NONE)
enable_testing()
add_test(NAME passa COMMAND sh -c "exit 0")
add_test(NAME outro COMMAND sh -c "exit 0")
add_test(NAME rotulado COMMAND sh -c "exit 0")
set_tests_properties(rotulado PROPERTIES LABELS unit)
"""

PROJETO_VAZIO = """cmake_minimum_required(VERSION 3.29)
project(probe_vazio NONE)
enable_testing()
"""


def _carregar_alvo():
    spec = importlib.util.spec_from_file_location("conferir_lista", ALVO)
    if spec is None or spec.loader is None:
        print(f"{SCRIPT_NAME}: nao carregou {ALVO}", file=sys.stderr)
        sys.exit(1)
    modulo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modulo)
    return modulo


def _ctest_json(build, arquivo_saida, *args):
    proc = subprocess.run(
        ["ctest", "--test-dir", build, "--show-only=json-v1", *args], capture_output=True, text=True
    )
    if proc.returncode != 0:
        raise RuntimeError(f"ctest {' '.join(args)} saiu {proc.returncode}: {proc.stderr.strip()}")
    with open(arquivo_saida, "w", encoding="utf-8") as handle:
        handle.write(proc.stdout)
    return arquivo_saida


def _configurar(raiz, nome, conteudo):
    fonte = os.path.join(raiz, nome, "src")
    os.makedirs(fonte)
    with open(os.path.join(fonte, "CMakeLists.txt"), "w", encoding="utf-8") as handle:
        handle.write(conteudo)
    build = os.path.join(raiz, nome, "b")
    proc = subprocess.run(["cmake", "-S", fonte, "-B", build], capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"cmake {nome} saiu {proc.returncode}: {proc.stderr.strip()}")
    return build


def _escrever_lista(caminho, nomes):
    with open(caminho, "w", encoding="utf-8") as handle:
        handle.write("".join(f"{n}\n" for n in nomes))
    return caminho


def _fixtures(raiz):
    """Gera os JSON reais e devolve um dicionario de caminhos."""
    com = _configurar(raiz, "com", PROJETO_COM_TESTES)
    vazio = _configurar(raiz, "vazio", PROJETO_VAZIO)
    f = {}
    f["inv"] = _ctest_json(com, os.path.join(raiz, "inv.json"))
    f["uni_unit"] = _ctest_json(com, os.path.join(raiz, "uni_unit.json"), "-L", "^unit$")
    f["uni_nada"] = _ctest_json(com, os.path.join(raiz, "uni_nada.json"), "-L", "nada_nao_existe")
    f["vazio"] = _ctest_json(vazio, os.path.join(raiz, "vazio.json"))
    return f, com


def _selecao(com, raiz, nome, lista_caminho, *filtros):
    return _ctest_json(com, os.path.join(raiz, nome), "--tests-from-file", lista_caminho, *filtros)


def _casos(raiz, com, f):
    """(nome, lista_nomes, inventario, universo, selecao_json, rc_esperado, veredito, trecho)."""
    l_com_outro = _escrever_lista(os.path.join(raiz, "l_com_outro.txt"), ["passa", "outro"])
    l_misto = _escrever_lista(os.path.join(raiz, "l_misto.txt"), ["passa", "nao_existe"])
    l_passa = _escrever_lista(os.path.join(raiz, "l_passa.txt"), ["passa"])
    l_passa_rot = _escrever_lista(os.path.join(raiz, "l_passa_rot.txt"), ["passa", "rotulado"])
    l_rot = _escrever_lista(os.path.join(raiz, "l_rot.txt"), ["rotulado"])
    # Fato do ambiente: o fim de linha de uma lista vem de quem a escreveu (CRLF no Windows, LF no Unix). O ctest
    # aceita os dois, e este caso prova que a conferencia tambem aceita.
    crlf = os.path.join(raiz, "l_crlf.txt")
    with open(crlf, "w", encoding="utf-8", newline="") as handle:
        handle.write("passa\r\noutro\r\n")
    sel_com_outro = _selecao(com, raiz, "sel_com_outro.json", l_com_outro)
    sel_misto = _selecao(com, raiz, "sel_misto.json", l_misto)
    sel_passa = _selecao(com, raiz, "sel_passa.json", l_passa)
    sel_passa_nada = _selecao(com, raiz, "sel_passa_nada.json", l_passa, "-L", "nada_nao_existe")
    sel_passa_rot_unit = _selecao(com, raiz, "sel_passa_rot_unit.json", l_passa_rot, "-L", "^unit$")
    sel_rot_unit = _selecao(com, raiz, "sel_rot_unit.json", l_rot, "-L", "^unit$")
    sel_crlf = _selecao(com, raiz, "sel_crlf.json", crlf)
    return [
        ("positivo_rodar", ["passa", "outro"], f["inv"], f["inv"], sel_com_outro, 0, "rodar", "ctest RODA neste passo: 2 de 2"),
        ("misto_nome_ausente_reprova", ["passa", "nao_existe"], f["inv"], f["inv"], sel_misto, 1, None, "ausentes do inventario: nao_existe"),
        ("selecao_diferente_de_E_reprova", ["passa", "outro"], f["inv"], f["inv"], sel_passa, 1, None, "difere de E"),
        ("inventario_vazio_reprova", ["passa"], f["vazio"], f["inv"], sel_passa, 1, None, "inventario vazio"),
        ("E_vazio_pular", ["passa"], f["inv"], f["uni_nada"], sel_passa_nada, 0, "pular", "ctest PULADO neste passo: 0 de 1"),
        ("E_nao_vazio_com_filtro_rodar", ["passa", "rotulado"], f["inv"], f["uni_unit"], sel_passa_rot_unit, 0, "rodar", "fora do filtro deste passo: 1"),
        ("lista_crlf_positivo", ["passa", "outro"], f["inv"], f["inv"], sel_crlf, 0, "rodar", "ctest RODA neste passo: 2 de 2"),
        ("E_unico_com_filtro_rodar", ["rotulado"], f["inv"], f["uni_unit"], sel_rot_unit, 0, "rodar", "ctest RODA neste passo: 1 de 1"),
    ]


def _rodar_caso(modulo, raiz, nome, lista_nomes, inv, uni, sel, rc_esperado, veredito_esperado, trecho):
    lista = _escrever_lista(os.path.join(raiz, f"lista_{nome}.txt"), lista_nomes)
    saida = os.path.join(raiz, f"veredito_{nome}.txt")
    if os.path.exists(saida):
        os.remove(saida)
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = modulo.main(["--lista", lista, "--inventario", inv, "--universo", uni, "--selecao", sel, "--saida-veredito", saida])
    problemas = []
    if trecho not in buf.getvalue():
        problemas.append(f"linha impressa sem {trecho!r}")
    if rc != rc_esperado:
        problemas.append(f"rc={rc}, esperado {rc_esperado}")
    if veredito_esperado is not None:
        if not os.path.isfile(saida):
            problemas.append("veredito nao gravado")
        else:
            with open(saida, encoding="utf-8") as handle:
                lido = handle.read().strip()
            if lido != veredito_esperado:
                problemas.append(f"veredito {lido!r}, esperado {veredito_esperado!r}")
    if rc_esperado != 0 and os.path.exists(saida):
        problemas.append("veredito gravado com rc != 0")
    return problemas


def _insumos_ruins(modulo, raiz, inv):
    """rc 2 para insumo ilegivel, sem veredito."""
    problemas = []
    sem_lista = os.path.join(raiz, "nao_existe_lista.txt")
    saida = os.path.join(raiz, "veredito_ruim.txt")
    rc = modulo.main(["--lista", sem_lista, "--inventario", inv, "--universo", inv, "--selecao", inv, "--saida-veredito", saida])
    if rc != 2:
        problemas.append(f"lista ausente: rc={rc}, esperado 2")
    sem_chave = os.path.join(raiz, "sem_tests.json")
    with open(sem_chave, "w", encoding="utf-8") as handle:
        json.dump({"kind": "ctestInfo"}, handle)
    lista = _escrever_lista(os.path.join(raiz, "lista_ruim.txt"), ["passa"])
    rc = modulo.main(["--lista", lista, "--inventario", sem_chave, "--universo", inv, "--selecao", inv, "--saida-veredito", saida])
    if rc != 2:
        problemas.append(f"JSON sem 'tests': rc={rc}, esperado 2")
    if os.path.exists(saida):
        problemas.append("veredito gravado com insumo ruim")
    return problemas


def selftest_main():
    if shutil.which("cmake") is None or shutil.which("ctest") is None:
        print(f"{SCRIPT_NAME}: cmake e ctest sao obrigatorios no PATH (sem pulo)", file=sys.stderr)
        return 1
    modulo = _carregar_alvo()
    raiz = tempfile.mkdtemp(prefix="conferir-lista-")
    falhas = []
    try:
        f, com = _fixtures(raiz)
        casos = _casos(raiz, com, f)
        for nome, lista_nomes, inv, uni, sel, rc_esperado, vered, trecho in casos:
            problemas = _rodar_caso(modulo, raiz, nome, lista_nomes, inv, uni, sel, rc_esperado, vered, trecho)
            if problemas:
                falhas.append(f"{nome}: " + "; ".join(problemas))
        falhas.extend(_insumos_ruins(modulo, raiz, f["inv"]))
    except RuntimeError as exc:
        print(f"{SCRIPT_NAME}: {exc}", file=sys.stderr)
        return 1
    finally:
        shutil.rmtree(raiz, ignore_errors=True)
    total = len(casos) + 1
    if falhas:
        for falha in falhas:
            print(f"{SCRIPT_NAME}: FALHOU: {falha}", file=sys.stderr)
        print(f"{SCRIPT_NAME} --selftest: FALHOU ({len(falhas)} de {total})", file=sys.stderr)
        return 1
    print(f"{SCRIPT_NAME} --selftest: os {total} grupos de controle OK ({len(casos)} casos e insumo ruim)")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        sys.exit(selftest_main())
    print(f"usage: {SCRIPT_NAME} --selftest", file=sys.stderr)
    sys.exit(2)
