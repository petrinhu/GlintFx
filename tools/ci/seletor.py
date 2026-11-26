#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# seletor.py - seletor unico do espelho local (tools/preci.sh) e do CI (EFIC-SELETOR, lote de eficacia,
# D-W8-1231). Decide, a partir do que a mudanca tocou, quais testes e quais unidades de lint rodam. So
# biblioteca padrao (L-07 do projeto).
#
# Dois subcomandos:
#   classificar --raiz <dir> (--base <rev> [--head <rev>] | --arvore) [--saida <arq>]
#       Classifica cada caminho tocado numa tabela fechada (C1 a C6) e decide o modo:
#       completo, subconjunto ou texto-sem-leitor. Imprime e grava um JSON.
#   resolver --mudancas <json> --build <dir> --saida-testes <arq> --saida-lint <arq>
#            [--ctest-json <arq>]
#       Lê o inventario de testes (ctest --show-only=json-v1) e o codemodel da CMake File API do diretorio
#       de construcao, e escreve a lista de testes e a de unidades de lint.
#
# Codigos de saida: 0 ok; 1 subconjunto sem teste nem lint (resolver, L-40 do projeto); 2 falha de guarda
# (conferencia de varredura, linha orfa, lista malformada, git ou insumo ausente). Nenhuma falha vira
# "pulado" em silencio.
#
# Regras que o CI e o espelho dependem, todas nesta tabela e nao espalhadas:
#   - toda leitura do git sai com -z, porque nome com acento ou espaco vem entre aspas sem -z;
#   - as tabelas (lista de varreduras, busca de leitor) leem a ARVORE DE TRABALHO de --raiz, nao o <head>;
#   - base inexistente ou nao ancestral do head da completo com motivo, nunca erro.

import argparse
import fnmatch
import json
import os
import re
import subprocess
import sys

LISTA_REL = "tools/ci/varreduras-de-arvore.txt"

# Primitivas de varredura de arvore. Um script rastreado sob tests/ ou tools/ que as contenha precisa de ao
# menos uma linha em LISTA_REL. Casa prosa tambem ("find " em comentario): e de proposito, o custo e uma linha.
# Forma em texto (git ls-files, os.walk, ...) e forma em lista de argumentos (["git", ..., "ls-files"],
# ["find", ...], os.scandir, os.listdir, .iterdir(). A lista de argumentos e' a que um script em Python usa para
# chamar o git por subprocess, e o texto "git ls-files" nao aparece nela: por isso as duas formas.
PRIMITIVAS = re.compile(
    r"git ls-files|git grep|os\.walk|rglob|glob\.glob|\.glob\(|Get-ChildItem|find |os\.scandir|os\.listdir|\.iterdir\("
    r"|[\"']git[\"'][^\n\]]*[\"'](?:ls-files|ls-tree|grep|status)[\"']"
    r"|[\"']find[\"']"
)

# Arquivos que a conferencia embutida le: scripts e CMake, que sao o que escreve varredura.
CONFERIDOS_SUFIXOS = (".py", ".sh", ".ps1", ".cmake")

# Arquivos que contam como leitores de texto (citacao exata do caminho ou do nome do arquivo).
LEITORES_PREFIXOS = ("tests/", "tools/", "cmake/", ".github/")

CLASSES = (
    # (classe, nome, completo?)
    ("C1", "estrutura", True),
    ("C2", "biblioteca", False),
    ("C3", "teste compilado", False),
    ("C4", "script", False),
    ("C4b", "dado de teste", True),
    ("C5", "texto e dado", False),
    ("C6", "desconhecido", True),
)
COMPLETA = {classe for classe, _nome, completo in CLASSES if completo}


class Guarda(Exception):
    """Falha de guarda: sai com 2, com a mensagem, sem classificar."""


class SemAlvo(Exception):
    """Subconjunto sem teste nem unidade de lint: sai com 1 (L-40 do projeto, nunca verde em silencio)."""


def _git(raiz, *args):
    proc = subprocess.run(["git", "-C", raiz, *args], capture_output=True)
    if proc.returncode != 0:
        raise Guarda(f"git {' '.join(args)} saiu {proc.returncode}: {proc.stderr.decode('utf-8', 'replace').strip()}")
    return proc.stdout


def _decode(raw):
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise Guarda(f"caminho que nao e UTF-8 valido: {raw!r} ({exc})")


def _rev_existe(raiz, rev):
    proc = subprocess.run(["git", "-C", raiz, "rev-parse", "--verify", "-q", rev + "^{commit}"], capture_output=True)
    return proc.returncode == 0


def _e_ancestral(raiz, base, head):
    proc = subprocess.run(["git", "-C", raiz, "merge-base", "--is-ancestor", base, head], capture_output=True)
    if proc.returncode == 0:
        return True
    if proc.returncode == 1:
        return False
    raise Guarda(f"git merge-base saiu {proc.returncode}: {proc.stderr.decode('utf-8', 'replace').strip()}")


def tracked(raiz):
    """Indexado e nao rastreado que nao e ignorado: o que a conferencia e a leitura de texto precisam ver."""
    bruto = _git(raiz, "ls-files", "-z", "--cached", "--others", "--exclude-standard")
    return sorted({_decode(p) for p in bruto.split(b"\0") if p})


def mudancas_arvore(raiz):
    """Indexado, nao indexado e nao rastreado (porcelain v1 com -z). Renomeacao ocupa dois campos NUL."""
    fields = _git(raiz, "status", "--porcelain=v1", "-z", "--untracked-files=all").split(b"\0")
    out = []
    i = 0
    while i < len(fields):
        field = fields[i]
        i += 1
        if not field:
            continue
        xy = field[:2]
        out.append(_decode(field[3:]))
        if xy[:1] in (b"R", b"C") or xy[1:2] in (b"R", b"C"):
            out.append(_decode(fields[i]))
            i += 1
    return out


def mudancas_base(raiz, base, head):
    """Caminhos de `git diff --name-status -M -z base head`. Renomeacao conta os dois caminhos."""
    fields = _git(raiz, "diff", "--name-status", "-M", "-z", base, head).split(b"\0")
    out = []
    i = 0
    while i < len(fields):
        status = fields[i]
        i += 1
        if not status:
            continue
        if status[:1] in (b"R", b"C"):
            out.append(_decode(fields[i]))
            out.append(_decode(fields[i + 1]))
            i += 2
        else:
            out.append(_decode(fields[i]))
            i += 1
    return out


def classe_de(caminho):
    """Primeira regra que casa vence. Tabela fechada: nova classe ou regra nova muda este arquivo, e so ele."""
    nome = caminho.rsplit("/", 1)[-1]
    if (nome == "CMakeLists.txt" or caminho.endswith(".cmake")
            or caminho.startswith(("cmake/", ".github/", "tools/ci/", "tests/harness/", "tests/container/",
                                   "third_party/"))
            or caminho in ("tools/preci.sh", ".clang-format", ".clang-tidy")):
        return "C1", "C1: estrutura (CMake, .github, tools/ci, harness, container, third_party)"
    if caminho.startswith(("include/", "src/")):
        return "C2", "C2: biblioteca (include/, src/)"
    if caminho.startswith("tests/") and caminho.endswith((".cpp", ".hpp", ".h")):
        return "C3", "C3: teste compilado (.cpp/.hpp/.h sob tests/)"
    if caminho.startswith(("tests/", "tools/")) and caminho.endswith((".py", ".sh", ".ps1")):
        return "C4", "C4: script (.py/.sh/.ps1 sob tests/ ou tools/)"
    if caminho.startswith("tests/"):
        return "C4b", "C4b: dado de teste sob tests/ (completo: pode ser carregado pela pasta)"
    if caminho.endswith((".py", ".sh", ".ps1", ".cpp", ".hpp", ".h", ".cmake")):
        return "C6", "C6: codigo fora das pastas conhecidas (desconhecido)"
    return "C5", "C5: texto e dado fora de tests/"


def ler_varreduras(raiz, rastreados):
    """Le `<script> <glob>` por linha. Linha orfa (script que nao existe) e malformada saem com 2."""
    caminho = os.path.join(raiz, LISTA_REL)
    if not os.path.isfile(caminho):
        raise Guarda(f"lista de varreduras ausente: {LISTA_REL}")
    linhas = []
    with open(caminho, encoding="utf-8") as handle:
        for numero, bruto in enumerate(handle, 1):
            texto = bruto.strip()
            if not texto or texto.startswith("#"):
                continue
            partes = texto.split()
            if len(partes) != 2:
                raise Guarda(f"{LISTA_REL}:{numero}: linha malformada (esperado '<script> <glob>'): {texto!r}")
            script, glob = partes
            if script not in rastreados:
                raise Guarda(f"{LISTA_REL}:{numero}: linha orfa, o script {script!r} nao esta rastreado")
            linhas.append((script, glob))
    return linhas


def conferir_varreduras(raiz, rastreados, linhas):
    """Todo script sob tests/ ou tools/ com primitiva de varredura precisa de uma linha na lista."""
    declarados = {script for script, _glob in linhas}
    faltantes = []
    for caminho in rastreados:
        if not caminho.startswith(("tests/", "tools/")):
            continue
        if not (caminho.endswith(CONFERIDOS_SUFIXOS) or caminho.rsplit("/", 1)[-1] == "CMakeLists.txt"):
            continue
        if caminho in declarados:
            continue
        try:
            with open(os.path.join(raiz, caminho), encoding="utf-8", errors="replace") as handle:
                texto = handle.read()
        except OSError:
            continue
        if PRIMITIVAS.search(texto):
            faltantes.append(caminho)
    if faltantes:
        raise Guarda(
            "varredura de arvore sem linha em " + LISTA_REL + ": " + ", ".join(faltantes)
            + " (declare <script> <glob> para cada um; o seletor nao classifica com varredura nao declarada)"
        )


def ler_leitores_texto(raiz, rastreados):
    """Texto dos arquivos que podem citar outro caminho: sob tests/, tools/, cmake/, .github/ e CMakeLists.txt."""
    textos = {}
    for caminho in rastreados:
        if not (caminho.startswith(LEITORES_PREFIXOS) or caminho.rsplit("/", 1)[-1] == "CMakeLists.txt"):
            continue
        try:
            with open(os.path.join(raiz, caminho), "rb") as handle:
                textos[caminho] = handle.read()
        except OSError:
            continue
    return textos


def leitores_de(caminho, classe, textos, varreduras):
    """C4: os scripts que citam o nome do arquivo (um nivel). C5: quem cita o caminho ou o nome, e os scripts
    de varredura cujo glob casa o caminho. Sem nenhum, o caminho e texto sem leitor."""
    nome = caminho.rsplit("/", 1)[-1].encode("utf-8")
    alvo = caminho.encode("utf-8")
    achados = []
    for leitor, bruto in sorted(textos.items()):
        if leitor == caminho:
            continue
        if alvo in bruto or nome in bruto:
            achados.append(leitor)
    if classe == "C5":
        for script, glob in varreduras:
            if fnmatch.fnmatchcase(caminho, glob) and script != caminho and script not in achados:
                achados.append(script)
    return sorted(set(achados))


def leitores_por_citacao(caminho, textos):
    """Leitores que citam o caminho ou o nome dele (sem contar a lista de varreduras)."""
    nome = caminho.rsplit("/", 1)[-1].encode("utf-8")
    alvo = caminho.encode("utf-8")
    return sorted(leitor for leitor, bruto in textos.items()
                  if leitor != caminho and (alvo in bruto or nome in bruto))


# --- Rodada 3: papel do leitor e efeito (decisao do CTO, E1 rodada 3) ------------------------------------------
#
# Um leitor de p (um arquivo que cita p, ou que o varre por glob) tem um PAPEL, e o papel dá o EFEITO da citacao.
# A tabela e' esta, e tudo se decide no TEXTO, dentro do classificar, igual em todo sistema. O resolver so cruza
# nomes com o inventario.
#   CMake: lexer tira comentarios e respeita aspas e colchetes. Efeito pela invocacao que contem p.
#   Orquestrador (.github/, tools/preci.sh): linha com '#' nao conta; citacao e' leitor e nao seleciona teste.
#   Script (.py/.sh/.ps1 de C1 ou C4): seleciona os testes que citam o script (regra do CMake). Piso de dois niveis.
#   Compilado (.cpp/.hpp/.h de C1, C3, C6): comentarios C/C++ saem; citacao que sobra num literal ou no codigo: completo.
#   Dado (C4b e o resto de C1): consumidores do dado pelas regras do CMake e do Script, um nivel; sem consumidor: completo.
#   Codigo de fora (C6 nao compilado): completo.
# Texto fora de tests/ (C5 lido por outro texto): nao executa nada, nao seleciona e nao torna completo.

PAPEL_CMAKE = "CMake"
PAPEL_ORQ = "Orquestrador"
PAPEL_SCRIPT = "Script"
PAPEL_COMPILADO = "Compilado"
PAPEL_DADO = "Dado"
PAPEL_FORA = "Codigo de fora"
PAPEL_TEXTO = "Texto"


def papel_de(leitor):
    """Papel do leitor, pela tabela do CTO. Ordem: a primeira que casa vence."""
    cls = classe_de(leitor)[0]
    nome = leitor.rsplit("/", 1)[-1]
    if nome == "CMakeLists.txt" or leitor.endswith(".cmake") or leitor.startswith("cmake/"):
        return PAPEL_CMAKE
    if leitor.startswith(".github/") or leitor == "tools/preci.sh":
        return PAPEL_ORQ
    if cls in ("C1", "C4") and leitor.endswith((".py", ".sh", ".ps1")):
        return PAPEL_SCRIPT
    if leitor.endswith((".cpp", ".hpp", ".h")) and cls in ("C1", "C3", "C6"):
        return PAPEL_COMPILADO
    if cls == "C6":
        return PAPEL_FORA
    if cls in ("C4b", "C1"):
        return PAPEL_DADO
    return PAPEL_TEXTO


def cmake_chamadas(texto):
    """Invocacoes de nivel de comando de um arquivo CMake. Comentarios '#' e '#[[ ]]' saem; aspas e colchetes sao
    respeitados (um argumento entre aspas ou colchetes e' texto, nao codigo). Cada chamada: nome, linha, argumentos
    e o tipo de cada argumento ('q' aspas, 'b' colchetes, 'w' palavra nua, 'p' parentese aninhado)."""
    tokens = []
    i, n, linha = 0, len(texto), 1
    while i < n:
        c = texto[i]
        if c == "\n":
            linha += 1
            i += 1
            continue
        if c in " \t\r":
            i += 1
            continue
        if c == "#":
            m = re.match(r"#\[(=*)\[", texto[i:i + 80])
            if m:
                fim = "]" + m.group(1) + "]"
                j = texto.find(fim, i + m.end())
                j = n if j < 0 else j + len(fim)
                linha += texto.count("\n", i, j)
                i = j
                continue
            j = texto.find("\n", i)
            i = n if j < 0 else j
            continue
        if c in "()":
            tokens.append(("(" if c == "(" else ")", c, linha))
            i += 1
            continue
        if c == '"':
            j = i + 1
            buf = []
            while j < n and texto[j] != '"':
                if texto[j] == "\\" and j + 1 < n:
                    buf.append(texto[j:j + 2])
                    j += 2
                    continue
                buf.append(texto[j])
                j += 1
            tokens.append(("q", "".join(buf), linha))
            linha += texto.count("\n", i, j)
            i = j + 1
            continue
        m = re.match(r"\[(=*)\[", texto[i:i + 80])
        if m:
            fim = "]" + m.group(1) + "]"
            j = texto.find(fim, i + m.end())
            j = n if j < 0 else j
            tokens.append(("b", texto[i + m.end():j], linha))
            linha += texto.count("\n", i, j)
            i = n if j >= n else j + len(fim)
            continue
        m = re.match(r'[^\s()#"]+', texto[i:])
        palavra = m.group(0)
        tokens.append(("w", palavra, linha))
        i += len(palavra)
    chamadas = []
    k = 0
    while k < len(tokens):
        tipo, valor, ln = tokens[k]
        if tipo == "w" and k + 1 < len(tokens) and tokens[k + 1][0] == "(":
            args, kinds, prof = [], [], 0
            k += 2
            while k < len(tokens):
                tt, vv, _ = tokens[k]
                if tt == "(":
                    prof += 1
                    args.append("(")
                    kinds.append("p")
                elif tt == ")":
                    if prof == 0:
                        break
                    prof -= 1
                    args.append(")")
                    kinds.append("p")
                else:
                    args.append(vv)
                    kinds.append(tt)
                k += 1
            chamadas.append({"name": valor, "line": ln, "args": args, "kinds": kinds})
        k += 1
    return chamadas


def _sem_comentarios_cpp(texto):
    """Texto C/C++ com os comentarios trocados por espacos. Literais, raw strings e o separador 1'000 ficam."""
    out = []
    i, n = 0, len(texto)
    while i < n:
        c = texto[i]
        if texto.startswith("//", i):
            j = texto.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
            continue
        if texto.startswith("/*", i):
            j = texto.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", texto[i:j]))
            i = j
            continue
        if c == "R" and texto.startswith('R"', i) and (i == 0 or not (texto[i - 1].isalnum() or texto[i - 1] == "_")):
            m = re.match(r'R"([^()\\ \t\n]{0,16})\(', texto[i:i + 40])
            if m:
                fim = ")" + m.group(1) + '"'
                j = texto.find(fim, i + m.end())
                j = n if j < 0 else j + len(fim)
                out.append(texto[i:j])
                i = j
                continue
        if c == '"':
            j = i + 1
            while j < n and texto[j] != '"':
                j += 2 if texto[j] == "\\" else 1
            j = min(n, j + 1)
            out.append(texto[i:j])
            i = j
            continue
        if c == "'":
            prev = texto[i - 1] if i > 0 else ""
            nxt = texto[i + 1] if i + 1 < n else ""
            if prev.isalnum() and nxt.isalnum():  # separador de digito: 1'000
                out.append(c)
                i += 1
                continue
            j = i + 1
            while j < n and texto[j] != "'":
                j += 2 if texto[j] == "\\" else 1
            j = min(n, j + 1)
            out.append(texto[i:j])
            i = j
            continue
        out.append(c)
        i += 1
    return "".join(out)


def _vazio():
    return {"testes": set(), "alvos": set(), "completo": False, "motivos": []}


def _completo(motivo):
    e = _vazio()
    e["completo"] = True
    e["motivos"].append(motivo)
    return e


def _soma(a, b):
    a["testes"] |= b["testes"]
    a["alvos"] |= b["alvos"]
    a["completo"] = a["completo"] or b["completo"]
    a["motivos"] += b["motivos"]
    return a


def _cita_chamada(chamada, p, nome):
    return any((p in a) or (nome and nome in a) for a in chamada["args"])


def _efeito_chamada(c, p, nome, arquivo, chamadas, outros, profundo=False):
    """Efeito de UMA invocacao de comando que cita p. Tabela do CTO: add_test, set_tests_properties, set_property(TEST),
    target_compile_definitions, message, option (texto de ajuda), set (com e sem CACHE). Qualquer outra: completo."""
    cmd = c["name"].lower()
    args, kinds = c["args"], c["kinds"]
    idx = [i for i, a in enumerate(args) if (p in a) or (nome and nome in a)]
    local = f"{arquivo}:{c['line']} {c['name']}"
    if cmd == "message":
        return _vazio()
    if cmd == "add_test":
        if "NAME" not in args or args.index("NAME") + 1 >= len(args):
            return _completo(f"{local}: add_test sem NAME literal")
        nome_teste = args[args.index("NAME") + 1]
        if "${" in nome_teste:
            return _completo(f"{local}: nome de teste com ${{}}")
        e = _vazio()
        e["testes"].add(nome_teste)
        return e
    if cmd == "set_tests_properties":
        if "PROPERTIES" not in args:
            return _completo(f"{local}: set_tests_properties sem PROPERTIES")
        fim = args.index("PROPERTIES")
        nomes = args[:fim]
        if not nomes or any("${" in x for x in nomes) or any(i < fim for i in idx):
            return _completo(f"{local}: nome com ${{}} ou citacao antes de PROPERTIES")
        e = _vazio()
        e["testes"] |= set(nomes)
        return e
    if cmd == "set_property":
        if args[:1] == ["TEST"] and "PROPERTY" in args:
            fim = args.index("PROPERTY")
            nomes = args[1:fim]
            if not nomes or any("${" in x for x in nomes):
                return _completo(f"{local}: nome de teste com ${{}}")
            e = _vazio()
            e["testes"] |= set(nomes)
            return e
        return _completo(f"{local}: set_property fora de TEST")
    if cmd == "target_compile_definitions":
        if not args or "${" in args[0] or any(i == 0 for i in idx):
            return _completo(f"{local}: alvo com ${{}} ou citacao no nome do alvo")
        e = _vazio()
        e["alvos"].add(args[0])
        return e
    if cmd == "option":
        if idx and all(i == 1 for i in idx):
            return _vazio()  # texto de ajuda de option()
        return _completo(f"{local}: citacao fora do texto de ajuda de option()")
    if cmd == "set":
        if not args:
            return _completo(f"{local}: set vazio")
        if "CACHE" in args:
            ci = args.index("CACHE")
            if idx and all(i == ci + 2 for i in idx):
                return _vazio()  # texto de ajuda de set(... CACHE)
            return _completo(f"{local}: citacao em set CACHE fora do texto de ajuda")
        if profundo:
            return _completo(f"{local}: segundo nivel de set (so um nivel e' seguido)")
        return _segue_variavel(args[0], c, arquivo, chamadas, outros)
    return _completo(f"{local}: outra invocacao ({c['name']}) com a citacao de {p}")


def _segue_variavel(var, origem, arquivo, chamadas, outros):
    """set(var ...) com a citacao no valor: segue cada ${var} do arquivo, um nivel. Completo se var aparece pelo nome
    sem ${} em outra invocacao, se aparece em outro arquivo CMake, ou se nao tem uso."""
    marca = "${" + var + "}"
    local = f"{arquivo}:{origem['line']} set({var})"
    for c2 in chamadas:
        if c2 is origem or (c2["name"].lower() == "set" and c2["args"][:1] == [var]):
            continue
        if any(k == "w" and a == var for a, k in zip(c2["args"], c2["kinds"])):
            return _completo(f"{local}: {var} sem ${{}} em {c2['name']} (linha {c2['line']})")
    for outro, texto in outros.items():
        if outro != arquivo and marca in texto:
            return _completo(f"{local}: {marca} tambem em {outro}")
    usos = [c2 for c2 in chamadas if c2 is not origem and any(marca in a for a in c2["args"])]
    if not usos:
        return _completo(f"{local}: {marca} sem uso")
    e = _vazio()
    for u in usos:
        _soma(e, _efeito_chamada(u, marca, marca, arquivo, chamadas, outros, profundo=True))
    return e


def _efeito_arquivo_cmake(p, nome, arquivo, chamadas, outros):
    """Efeito das invocacoes de um arquivo CMake que citam p (ou o nome). Comentario nao aparece aqui: nada."""
    e = _vazio()
    for c in chamadas:
        if _cita_chamada(c, p, nome):
            _soma(e, _efeito_chamada(c, p, nome, arquivo, chamadas, outros))
    return e


def _cita_em_cmake(p, nome, ctx):
    return any(_cita_chamada(c, p, nome) for info in ctx["cmake"].values() for c in info["chamadas"])


def _efeito_cmake_todos(p, nome, ctx):
    e = _vazio()
    outros = {a: info["texto"] for a, info in ctx["cmake"].items()}
    for arquivo, info in ctx["cmake"].items():
        _soma(e, _efeito_arquivo_cmake(p, nome, arquivo, info["chamadas"], outros))
    return e


def _testes_do_script(s, ctx):
    """Testes que citam o script s pela regra do CMake (um nivel, sem piso)."""
    return _efeito_cmake_todos(s, s.rsplit("/", 1)[-1], ctx)


def _efeito_script(s, ctx):
    """Efeito de um script que le ou varre: os testes que o citam. Piso de dois niveis: sem teste e fora do
    orquestrador, sobe um nivel (os scripts que citam o nome dele); se ainda nada, completo."""
    nome = s.rsplit("/", 1)[-1]
    e = _testes_do_script(s, ctx)
    if e["testes"] or e["alvos"] or e["completo"]:
        return e
    if nome.encode("utf-8") in ctx["orquestrador"]:
        return e  # o CI roda o script: nada a selecionar, sem piso
    for leitor, bruto in ctx["scripts"].items():
        if leitor != s and nome.encode("utf-8") in bruto:
            _soma(e, _testes_do_script(leitor, ctx))
    if e["testes"] or e["alvos"] or e["completo"]:
        return e
    return _completo(f"piso: {s} sem teste que o cite, fora do orquestrador e sem script que o cite")


def _efeito_dado(d, ctx):
    """Dado: consumidores pelas regras do CMake e do Script, um nivel. Sem consumidor: completo."""
    nome = d.rsplit("/", 1)[-1]
    e = _vazio()
    achou = False
    for arquivo, info in ctx["cmake"].items():
        if _cita_em_cmake(d, nome, {"cmake": {arquivo: info}}):
            achou = True
    if achou:
        _soma(e, _efeito_cmake_todos(d, nome, ctx))
    for leitor, bruto in ctx["scripts"].items():
        if leitor != d and nome.encode("utf-8") in bruto:
            achou = True
            _soma(e, _testes_do_script(leitor, ctx))
    if not achou:
        return _completo(f"dado {d} sem consumidor")
    return e


def _efeito_compilado(bruto, p, nome, leitor):
    """Compilado: citacao que sobra fora de comentario (literal ou codigo): completo. Comentario nao executa nada."""
    limpo = _sem_comentarios_cpp(bruto.decode("utf-8", "replace"))
    if (p in limpo) or (nome and nome in limpo):
        return _completo(f"{leitor}: citacao de {p} em literal ou codigo do compilado")
    return _vazio()


def efeito_do_leitor(p, nome, leitor, ctx):
    """Efeito de UM leitor sobre o caminho p, pelo papel dele (tabela do CTO)."""
    papel = papel_de(leitor)
    if papel == PAPEL_CMAKE:
        info = ctx["cmake"][leitor]
        return _efeito_arquivo_cmake(p, nome, leitor, info["chamadas"], {a: i["texto"] for a, i in ctx["cmake"].items()})
    if papel == PAPEL_SCRIPT:
        return _efeito_script(leitor, ctx)
    if papel == PAPEL_COMPILADO:
        return _efeito_compilado(ctx["bruto"][leitor], p, nome, leitor)
    if papel == PAPEL_DADO:
        return _efeito_dado(leitor, ctx)
    if papel == PAPEL_FORA:
        return _completo(f"{leitor}: codigo de fora, nao compilado")
    return _vazio()  # Orquestrador e Texto: nao selecionam teste


def contexto_de_leitura(textos):
    """Uma vez por classificacao: os arquivos CMake ja lexados, e os textos de orquestrador e de script."""
    ctx = {"cmake": {}, "orquestrador": b"", "scripts": {}, "bruto": textos}
    orq = []
    for arq, bruto in textos.items():
        papel = papel_de(arq)
        if papel == PAPEL_CMAKE:
            texto = bruto.decode("utf-8", "replace")
            ctx["cmake"][arq] = {"texto": texto, "chamadas": cmake_chamadas(texto)}
        elif papel == PAPEL_ORQ:
            orq.append(bruto)
        elif papel == PAPEL_SCRIPT:
            ctx["scripts"][arq] = bruto
    ctx["orquestrador"] = b"\n".join(orq)
    return ctx


def _resumo(e):
    return {"testes": sorted(e["testes"]), "alvos": sorted(e["alvos"]),
            "completo": e["completo"], "motivos": e["motivos"]}


def _entrada_c5_ou_c4(caminho, classe, ctx, textos, varreduras):
    """Efeito de um caminho C4 (o proprio script) ou C5 (lido por leitores), pela tabela do CTO."""
    nome = caminho.rsplit("/", 1)[-1]
    if classe == "C4":
        ef = _efeito_script(caminho, ctx)
        return ef, [{"leitor": caminho, "papel": PAPEL_SCRIPT, "efeito": _resumo(ef)}]
    ef = _vazio()
    registros = []
    for leitor in leitores_de(caminho, classe, textos, varreduras):
        e1 = efeito_do_leitor(caminho, nome, leitor, ctx)
        _soma(ef, e1)
        registros.append({"leitor": leitor, "papel": papel_de(leitor), "efeito": _resumo(e1)})
    return ef, registros



def classificar(raiz, base, head, arvore):
    rastreados = tracked(raiz)
    varreduras = ler_varreduras(raiz, set(rastreados))
    conferir_varreduras(raiz, rastreados, varreduras)
    motivo_base = None
    if arvore:
        caminhos = mudancas_arvore(raiz)
        origem = "arvore"
        head_usado = "arvore"
    else:
        origem = base
        head_usado = head
        if not _rev_existe(raiz, base):
            motivo_base = f"base ausente: {base} nao existe neste repositorio"
            caminhos = []
        elif not _rev_existe(raiz, head):
            raise Guarda(f"head inexistente: {head}")
        elif not _e_ancestral(raiz, base, head):
            motivo_base = f"base {base} nao e ancestral de {head}"
            caminhos = []
        else:
            caminhos = mudancas_base(raiz, base, head)
    caminhos = sorted(set(caminhos))
    textos = ler_leitores_texto(raiz, rastreados)
    por_caminho = []
    contagem = {classe: 0 for classe, _n, _c in CLASSES}
    ctx = contexto_de_leitura(textos)
    for caminho in caminhos:
        classe, regra = classe_de(caminho)
        if classe in ("C4", "C5"):
            ef, registros = _entrada_c5_ou_c4(caminho, classe, ctx, textos, varreduras)
        else:
            ef, registros = _vazio(), []
        leitores = [r["leitor"] for r in registros] if classe == "C5" else []
        contagem[classe] += 1
        por_caminho.append({"caminho": caminho, "classe": classe, "regra": regra, "leitores": leitores,
                            "efeitos": registros, "completo": ef["completo"], "motivos": ef["motivos"],
                            "testes": sorted(ef["testes"]), "alvos": sorted(ef["alvos"])})
    contagem["total"] = len(caminhos)

    if motivo_base:
        modo, motivo = "completo", motivo_base
    elif not caminhos:
        modo, motivo = "completo", "zero caminhos tocados"
    else:
        completos = [e for e in por_caminho if e["classe"] in COMPLETA or e["completo"]]
        if completos:
            modo = "completo"
            partes = []
            for e in completos:
                if e["classe"] in COMPLETA:
                    partes.append(f"{e['caminho']} ({e['classe']})")
                else:
                    partes.append(f"{e['caminho']} (C5): " + " | ".join(e["motivos"]))
            motivo = "caminho completo: " + "; ".join(partes)
        elif all(e["classe"] == "C5" and not e["leitores"] for e in por_caminho):
            modo = "texto-sem-leitor"
            motivo = f"todo caminho e texto sem leitor ({len(por_caminho)} caminhos)"
        else:
            modo = "subconjunto"
            sem_leitor = sum(1 for e in por_caminho if e["classe"] == "C5" and not e["leitores"])
            motivo = f"{len(por_caminho)} caminhos; {sem_leitor} de texto sem leitor (nao selecionam teste)"

    return {
        "modo": modo,
        "motivo": motivo,
        "base": origem,
        "head": head_usado,
        "raiz": os.path.abspath(raiz),
        "caminhos": caminhos,
        "por_caminho": por_caminho,
        "contagem": contagem,
    }


def cmd_classificar(args):
    if not args.arvore and not args.base:
        raise Guarda("classificar precisa de --base <rev> ou de --arvore")
    if args.arvore and args.base:
        raise Guarda("--base e --arvore sao excludentes")
    resultado = classificar(args.raiz, args.base, args.head or "HEAD", args.arvore)
    texto = json.dumps(resultado, ensure_ascii=False, indent=2)
    print(texto)
    if args.saida:
        with open(args.saida, "w", encoding="utf-8") as handle:
            handle.write(texto + "\n")
    print(f"seletor: modo {resultado['modo']}: {resultado['motivo']}", file=sys.stderr)
    return 0


# --- resolver ------------------------------------------------------------------------------------------------


def _inventario_testes(build, ctest_json):
    if ctest_json:
        with open(ctest_json, encoding="utf-8") as handle:
            dados = json.load(handle)
    else:
        proc = subprocess.run(["ctest", "--test-dir", build, "--show-only=json-v1"], capture_output=True)
        if proc.returncode != 0:
            raise Guarda(f"ctest --show-only=json-v1 saiu {proc.returncode}: {proc.stderr.decode('utf-8', 'replace')}")
        dados = json.loads(proc.stdout.decode("utf-8"))
    testes = []
    for teste in dados.get("tests", []):
        testes.append({"name": teste["name"], "command": [str(c) for c in teste.get("command", [])]})
    return testes


def _codemodel(build):
    """Alvos e fontes pela CMake File API (codemodel v2). A consulta e criada antes do configure, por quem chama."""
    reply = os.path.join(build, ".cmake", "api", "v1", "reply")
    if not os.path.isdir(reply):
        raise Guarda(f"sem resposta da File API em {reply}: crie .cmake/api/v1/query/codemodel-v2 antes do configure")
    indices = sorted(n for n in os.listdir(reply) if n.startswith("index-") and n.endswith(".json"))
    if not indices:
        raise Guarda(f"sem index-*.json em {reply}")
    with open(os.path.join(reply, indices[-1]), encoding="utf-8") as handle:
        indice = json.load(handle)
    cm_info = indice.get("reply", {}).get("codemodel-v2")
    if not cm_info:
        raise Guarda("a resposta da File API nao tem codemodel-v2")
    with open(os.path.join(reply, cm_info["jsonFile"]), encoding="utf-8") as handle:
        codemodel = json.load(handle)
    alvos = {}
    for configuracao in codemodel.get("configurations", []):
        for alvo in configuracao.get("targets", []):
            with open(os.path.join(reply, alvo["jsonFile"]), encoding="utf-8") as handle:
                corpo = json.load(handle)
            fontes = []
            for fonte in corpo.get("sources", []):
                caminho = fonte.get("path", "")
                if os.path.isabs(caminho):
                    continue  # fonte fora da arvore nao e caminho do repositorio
                fontes.append(caminho.replace(os.sep, "/"))
            alvos.setdefault(corpo["name"], set()).update(fontes)
    return alvos


def _cita(teste, caminho):
    return caminho in " ".join(teste["command"])


def _alvo_do_teste(teste):
    """Alvo do CMake que o teste executa. O ctest --show-only=json-v1 so traz 'command' em parte dos testes (no
    ctest real, 197 de 405 nao trazem): nesses, o teste leva o nome do alvo (glintfx_add_test), e e' esse o nome."""
    if teste["command"]:
        return os.path.basename(teste["command"][0])
    return teste["name"]


def _testes_de_fonte(testes, alvos, caminho):
    """Nomes dos testes cujo alvo tem `caminho` entre as fontes do codemodel (ou cujo comando o cita)."""
    nomes = set()
    for teste in testes:
        if caminho in alvos.get(_alvo_do_teste(teste), set()) or _cita(teste, caminho):
            nomes.add(teste["name"])
    return nomes


def _includers(raiz, unidades, cabecalho):
    """Unidades .cpp que incluem o cabecalho, por sufixo do caminho do include (sem include/ nem src/)."""
    chave = cabecalho.split("/", 1)[1] if cabecalho.startswith(("include/", "src/")) else cabecalho
    padrao = re.compile(r'^[ \t]*#[ \t]*include[ \t]*[<"]([^>"]+)[>"]', re.MULTILINE)
    achados = []
    for unidade in sorted(unidades):
        try:
            with open(os.path.join(raiz, unidade), encoding="utf-8", errors="replace") as handle:
                texto = handle.read()
        except OSError:
            continue
        for incluido in padrao.findall(texto):
            if incluido == chave or incluido.endswith("/" + chave):
                achados.append(unidade)
                break
    return achados


def resolver(mudancas, build, ctest_json):
    testes = _inventario_testes(build, ctest_json)
    alvos = _codemodel(build)
    todas_fontes = set().union(*alvos.values()) if alvos else set()
    unidades_cpp = sorted(f for f in todas_fontes if f.endswith(".cpp"))
    nomes = {t["name"] for t in testes}
    raiz = mudancas["raiz"]
    modo = mudancas["modo"]

    if modo == "completo":
        selecionados = set(nomes)
        lint = sorted(f for f in unidades_cpp if f.startswith("src/"))
        precisa_build = True
    elif modo == "texto-sem-leitor":
        selecionados, lint, precisa_build = set(), [], False
    else:
        selecionados, lint = set(), set()
        precisa_build = False
        for entrada in mudancas["por_caminho"]:
            caminho, classe = entrada["caminho"], entrada["classe"]
            leitores = set(entrada.get("leitores", []))
            if classe == "C2":
                precisa_build = True
                selecionados |= nomes
                if caminho.endswith(".cpp"):
                    lint.add(caminho)
                elif caminho.endswith((".hpp", ".h")):
                    lint.update(_includers(raiz, unidades_cpp, caminho))
            elif classe == "C3":
                precisa_build = True
                for teste in testes:
                    alvo = _alvo_do_teste(teste)
                    if caminho in alvos.get(alvo, set()) or _cita(teste, caminho):
                        selecionados.add(teste["name"])
            elif classe in ("C4", "C5"):
                # o efeito ja foi decidido no classificar (tabela do CTO): aqui so cruza nomes com o inventario
                selecionados |= {nome for nome in entrada.get("testes", []) if nome in nomes}
                for alvo in entrada.get("alvos", []):
                    selecionados |= {t["name"] for t in testes if _alvo_do_teste(t) == alvo}
        if not selecionados and not lint:
            raise SemAlvo("subconjunto sem teste nem unidade de lint: a classificacao disse que havia o que conferir "
                          "(L-40 do projeto)")
    if any(_alvo_do_teste(t) in alvos for t in testes if t["name"] in selecionados):
        precisa_build = True  # teste compilado selecionado liga o build
    lista_testes = sorted(selecionados)
    lista_lint = sorted(lint)
    return lista_testes, lista_lint, len(nomes), precisa_build


def cmd_resolver(args):
    with open(args.mudancas, encoding="utf-8") as handle:
        mudancas = json.load(handle)
    try:
        testes, lint, total, precisa_build = resolver(mudancas, args.build, args.ctest_json)
    except SemAlvo as exc:
        print(f"seletor: {exc}", file=sys.stderr)
        return 1
    with open(args.saida_testes, "w", encoding="utf-8") as handle:
        handle.write("".join(f"{nome}\n" for nome in testes))
    with open(args.saida_lint, "w", encoding="utf-8") as handle:
        handle.write("".join(f"{caminho}\n" for caminho in lint))
    print(f"seletor: {len(testes)} testes de {total} no inventario; {len(lint)} arquivos para lint; "
          f"precisa de build: {'sim' if precisa_build else 'nao'}")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="seletor unico do espelho e do CI (EFIC-SELETOR)")
    sub = parser.add_subparsers(dest="comando", required=True)

    p_class = sub.add_parser("classificar", help="classifica os caminhos tocados e decide o modo")
    p_class.add_argument("--raiz", required=True)
    p_class.add_argument("--base")
    p_class.add_argument("--head")
    p_class.add_argument("--arvore", action="store_true")
    p_class.add_argument("--saida")
    p_class.set_defaults(func=cmd_classificar)

    p_res = sub.add_parser("resolver", help="escreve a lista de testes e a de lint")
    p_res.add_argument("--mudancas", required=True)
    p_res.add_argument("--build", required=True)
    p_res.add_argument("--saida-testes", required=True)
    p_res.add_argument("--saida-lint", required=True)
    p_res.add_argument("--ctest-json")
    p_res.set_defaults(func=cmd_resolver)

    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except SemAlvo as exc:
        print(f"seletor: {exc}", file=sys.stderr)
        return 1
    except Guarda as exc:
        print(f"seletor: FALHOU - {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
