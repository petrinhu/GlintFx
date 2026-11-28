#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# conferir_lista.py - conferencia da lista de testes selecionada antes do ctest (EFIC-CI-SELETIVO, E3,
# Ordem B, decisao D2 do CTO; GODS_LAWS.md L-40 e L-04 do projeto). Uma unica ferramenta, a mesma em todo
# sistema: nao existe gemeo em bash nem pwsh (so' le o veredito e o rc).
#
# Entradas (caminhos absolutos):
#   --lista <arq>        lista de nomes (um por linha; strip; linha vazia ignorada)
#   --inventario <json>  `ctest --show-only=json-v1` sem filtro
#   --universo <json>    o mesmo, com os filtros do proprio passo (-L, -E, -LE); sem filtro, e' o inventario
#   --selecao <json>     o mesmo, com os filtros do passo e --tests-from-file <lista>
#   --saida-veredito <arq>  escreve UMA palavra, `rodar` ou `pular`, so' quando o rc e' 0
#
# Conta, sempre impresso:
#   A = lista - inventario            (nome que nao existe neste sistema: reprova, rc 1)
#   F = (lista & inventario) - universo   (fora do filtro deste passo: so' impresso)
#   E = lista & universo              (o que o ctest deste passo deve rodar)
#   S = nomes de --selecao, que devem ser iguais a E (rc 1 se diferirem, nos dois sentidos)
#
# Codigos de saida: 0 conferido (veredito `rodar`, ou `pular` quando E e' vazio); 1 reprova (A nao vazio,
# S diferente de E, inventario vazio: piso L-40); 2 insumo ilegivel (arquivo ausente, JSON ruim, sem a
# chave `tests`). Nenhum insumo ruim vira pulo ou verde.

import argparse
import json
import sys

SCRIPT_NAME = "conferir_lista.py"


class Insumo(Exception):
    """Insumo ilegivel: sai com 2."""


def ler_lista(caminho):
    try:
        with open(caminho, encoding="utf-8") as handle:
            brutas = handle.read().splitlines()
    except (OSError, UnicodeDecodeError) as exc:
        raise Insumo(f"lista ilegivel {caminho}: {exc}")
    return [linha.strip() for linha in brutas if linha.strip()]


def ler_nomes_json(caminho):
    try:
        with open(caminho, encoding="utf-8") as handle:
            dados = json.load(handle)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise Insumo(f"JSON ilegivel {caminho}: {exc}")
    testes = dados.get("tests") if isinstance(dados, dict) else None
    if not isinstance(testes, list):
        raise Insumo(f"JSON sem a chave 'tests' (lista): {caminho}")
    nomes = set()
    for teste in testes:
        nome = teste.get("name") if isinstance(teste, dict) else None
        if not isinstance(nome, str) or not nome:
            raise Insumo(f"entrada sem 'name' em {caminho}")
        nomes.add(nome)
    return nomes


def conferir(lista, inventario, universo, selecao):
    """Puro: recebe conjuntos e devolve (rc, veredito, linhas). rc 2 nao aparece aqui (e' do CLI).

    veredito e' 'rodar' ou 'pular' quando rc 0; None quando rc 1.
    """
    linhas = []
    k = len(lista)
    i = len(inventario)
    u = len(universo)
    faltantes = sorted(set(lista) - inventario)
    fora_do_filtro = (set(lista) & inventario) - universo
    entrada_do_universo = set(lista) & universo
    a_rodar = len(entrada_do_universo)
    linhas.append(
        f"lista {k}; inventario {i}; universo {u}; a rodar {a_rodar}; "
        f"fora do filtro {len(fora_do_filtro)}; ausentes {len(faltantes)}"
    )
    if i == 0:
        linhas.append("REPROVADO: inventario vazio (L-40 do projeto: piso de varredura)")
        return 1, None, linhas
    if faltantes:
        linhas.append("REPROVADO: nomes da lista ausentes do inventario: " + ", ".join(faltantes))
        return 1, None, linhas
    if fora_do_filtro:
        linhas.append(f"fora do filtro deste passo: {len(fora_do_filtro)}")
    if selecao != entrada_do_universo:
        so_selecao = sorted(selecao - entrada_do_universo)
        so_esperado = sorted(entrada_do_universo - selecao)
        linhas.append(
            "REPROVADO: selecao do ctest difere de E; "
            f"so na selecao: {so_selecao or '-'}; so em E: {so_esperado or '-'}"
        )
        return 1, None, linhas
    if a_rodar == 0:
        linhas.append(
            f"ctest PULADO neste passo: 0 de {k} da lista (A=0, fora do filtro={len(fora_do_filtro)}); "
            "a conferencia entre sistemas e do parity"
        )
        return 0, "pular", linhas
    linhas.append(f"ctest RODA neste passo: {a_rodar} de {k} da lista")
    return 0, "rodar", linhas


def main(argv):
    parser = argparse.ArgumentParser(prog=SCRIPT_NAME, description="conferencia da lista de testes (E3, D2)")
    parser.add_argument("--lista", required=True)
    parser.add_argument("--inventario", required=True)
    parser.add_argument("--universo", required=True)
    parser.add_argument("--selecao", required=True)
    parser.add_argument("--saida-veredito", required=True)
    try:
        args = parser.parse_args(argv)
    except SystemExit:
        return 2
    try:
        lista = set(ler_lista(args.lista))
        inventario = ler_nomes_json(args.inventario)
        universo = ler_nomes_json(args.universo)
        selecao = ler_nomes_json(args.selecao)
    except Insumo as exc:
        print(f"{SCRIPT_NAME}: INSUMO - {exc}", file=sys.stderr)
        return 2
    rc, veredito, linhas = conferir(lista, inventario, universo, selecao)
    for linha in linhas:
        print(f"{SCRIPT_NAME}: {linha}")
    if rc == 0:
        try:
            with open(args.saida_veredito, "w", encoding="utf-8") as handle:
                handle.write(veredito + "\n")
        except OSError as exc:
            print(f"{SCRIPT_NAME}: INSUMO - nao grava o veredito: {exc}", file=sys.stderr)
            return 2
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
