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
    for caminho in caminhos:
        classe, regra = classe_de(caminho)
        leitores = leitores_de(caminho, classe, textos, varreduras) if classe in ("C4", "C5") else []
        citacao = leitores_por_citacao(caminho, textos) if classe == "C5" else []
        contagem[classe] += 1
        por_caminho.append({"caminho": caminho, "classe": classe, "regra": regra, "leitores": leitores,
                            "leitores_citacao": citacao})
    contagem["total"] = len(caminhos)

    if motivo_base:
        modo, motivo = "completo", motivo_base
    elif not caminhos:
        modo, motivo = "completo", "zero caminhos tocados"
    else:
        completos = [e for e in por_caminho if e["classe"] in COMPLETA or _lido_por_completo(e)]
        if completos:
            modo = "completo"
            motivo = "caminho completo: " + ", ".join(_motivo_completo(e) for e in completos)
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


LEITOR_COMPLETO = ("C1", "C4b", "C6")


def _lido_por_completo(entrada):
    """Caminho C5 CITADO por leitor C1, C4b ou C6 (estrutura, dado de teste, codigo desconhecido) vira completo:
    o leitor pode mudar o que a mudanca afeta, e nao sabemos quanto. Lado seguro (critério B1). Leitor que chega
    so por glob da lista de varreduras NAO torna o caminho completo: ele segue a regra do C4 (ver resolver)."""
    if entrada["classe"] != "C5":
        return False
    return any(classe_de(leitor)[0] in LEITOR_COMPLETO for leitor in entrada["leitores_citacao"])


def _motivo_completo(entrada):
    if entrada["classe"] in COMPLETA:
        return f"{entrada['caminho']} ({entrada['classe']})"
    lidos = [leitor for leitor in entrada["leitores_citacao"] if classe_de(leitor)[0] in LEITOR_COMPLETO]
    return f"{entrada['caminho']} (C5 lido por {', '.join(lidos)})"


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
                if classe == "C4":
                    selecionados |= {t["name"] for t in testes if _cita(t, caminho)}
                for leitor in sorted(leitores):
                    classe_leitor = classe_de(leitor)[0]
                    if classe_leitor == "C3":
                        # leitor .cpp sob tests/: os testes dos alvos que o tem entre as fontes (regra do C3)
                        selecionados |= _testes_de_fonte(testes, alvos, leitor)
                    elif classe_leitor == "C5":
                        pass  # texto nao executa nada
                    else:
                        # C4, C1, C4b, C6 que chegaram por glob: os testes que citam o script, um nivel, qualquer
                        # que seja a classe dele (regra do C4). Os que citaram o caminho ja deixaram completo.
                        selecionados |= {t["name"] for t in testes if _cita(t, leitor)}
        if not selecionados and not lint:
            raise SemAlvo("subconjunto sem teste nem unidade de lint: a classificacao disse que havia o que conferir "
                          "(L-40 do projeto)")
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
