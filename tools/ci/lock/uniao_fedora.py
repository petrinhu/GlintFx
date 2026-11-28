#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tools/ci/lock/uniao_fedora.py - CI-PIN-PKG-B (ordem do CTO,
# .trabalho/cto-w8/decisao-ci-pin-pacotes.md, item 3): a UNIAO dos pacotes
# Fedora que o CI instala, lida dos proprios pontos de instalacao e nunca
# copiada a mao. Pontos lidos:
#   - tools/ci/env/fedora.sh (a matriz `linux`);
#   - .github/workflows/ci.yml (os jobs lint, sanitizer, gl-codegen-host-cross,
#     debug e clang, com `dnf -y install` em passo `run:`);
#   - tests/container/Containerfile (o estagio builder e o estagio final: as
#     linhas `RUN ... dnf -y install ...`, com continuacao por barra invertida).
#
# Saida: uma linha por pacote, em ordem, na stdout. Sem nenhum pacote ou sem
# nenhum ponto num arquivo REPROVA (GODS_LAWS.md L-40: varredura vazia nunca
# vira sucesso). Diagnostico (contagem por arquivo) vai para a stderr.
#
# Modos:
#   (sem argumento)  imprime a uniao do repositorio
#   --selftest       controles em texto: um ponto simples, continuacao por
#                    barra, bloco YAML `>-`, comentario ignorado, terminal com
#                    `&&` e varredura vazia reprovada
#
# Este arquivo so le texto. Nao instala nada e nao toca rede.

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
FONTES = (
    ("fedora.sh", REPO / "tools" / "ci" / "env" / "fedora.sh", "script"),
    ("ci.yml", REPO / ".github" / "workflows" / "ci.yml", "yaml"),
    ("Containerfile", REPO / "tests" / "container" / "Containerfile", "script"),
)
MARCA = "dnf -y install"
SEPARADORES = ("&&", ";", "||")


def _tokens(trecho):
    """Pacotes de um trecho de linha de instalacao. Para no separador de
    comando (&&, ;, ||): o que vem depois dele nao e' lista de instalacao."""
    saida = []
    for token in trecho.split():
        if token in SEPARADORES:
            break
        if token == "\\":
            continue
        saida.append(token.rstrip("\\"))
    return [t for t in saida if t]


def _extrair_script(texto):
    """Pontos de um script ou Containerfile: a marca na linha (nao comentario),
    seguida das linhas que terminam em barra invertida e da linha final."""
    pontos = []
    linhas = texto.splitlines()
    i = 0
    while i < len(linhas):
        bruta = linhas[i]
        if bruta.lstrip().startswith("#") or MARCA not in bruta:
            i += 1
            continue
        resto = bruta.split(MARCA, 1)[1]
        pacotes = _tokens(resto)
        fim = bruta.rstrip().endswith("\\")
        i += 1
        while fim and i < len(linhas):
            seguinte = linhas[i]
            pacotes += _tokens(seguinte)
            fim = seguinte.rstrip().endswith("\\")
            i += 1
        pontos.append(pacotes)
    return pontos


def _extrair_yaml(texto):
    """Pontos de um workflow: a marca em linha `run:` (ou bloco `>-`). As
    linhas seguintes entram enquanto tiverem a MESMA indentacao da linha da
    marca, nao forem vazias e nao comecarem por `#`. Ao primeiro desvio, o
    bloco acabou."""
    pontos = []
    linhas = texto.splitlines()
    for i, bruta in enumerate(linhas):
        if bruta.lstrip().startswith("#") or MARCA not in bruta:
            continue
        indent = len(bruta) - len(bruta.lstrip())
        pacotes = _tokens(bruta.split(MARCA, 1)[1])
        j = i + 1
        while j < len(linhas):
            seguinte = linhas[j]
            if not seguinte.strip() or seguinte.lstrip().startswith("#"):
                break
            if len(seguinte) - len(seguinte.lstrip()) != indent:
                break
            pacotes += _tokens(seguinte)
            j += 1
        pontos.append(pacotes)
    return pontos


def extrair(texto, formato):
    if formato == "yaml":
        return _extrair_yaml(texto)
    return _extrair_script(texto)


def uniao(fontes):
    """fontes: lista de (nome, texto, formato). Devolve (pacotes, contagem).
    Reprova (ValueError) fonte sem ponto de instalacao e uniao vazia."""
    conjunto = set()
    contagem = {}
    for nome, texto, formato in fontes:
        pontos = extrair(texto, formato)
        if not pontos:
            raise ValueError(f"{nome}: nenhum ponto de instalacao 'dnf -y install' (varredura vazia, L-40)")
        contagem[nome] = len(pontos)
        for pacotes in pontos:
            if not pacotes:
                raise ValueError(f"{nome}: ponto de instalacao sem pacote (L-40)")
            conjunto.update(pacotes)
    if not conjunto:
        raise ValueError("uniao vazia (L-40)")
    return sorted(conjunto), contagem


def _ler_fontes():
    return [(nome, caminho.read_text(encoding="utf-8"), formato) for nome, caminho, formato in FONTES]


def _autoteste():
    casos_ok = [
        ("ponto simples", "    dnf -y install gcc-c++ cmake\n", "script", ["cmake", "gcc-c++"]),
        ("continuacao por barra",
         "RUN dnf -y install gcc-c++ \\\n        libasan libubsan \\\n    && dnf clean all\n",
         "script", ["gcc-c++", "libasan", "libubsan"]),
        ("comentario ignorado",
         "# dnf -y install nao-conta\nRUN dnf -y install cmake\n", "script", ["cmake"]),
        ("prefixo com &&",
         "RUN printf x > f \\\n    && dnf -y install \\\n        kwin \\\n        binutils \\\n    && dnf clean all\n",
         "script", ["binutils", "kwin"]),
        ("bloco yaml >-",
         "    steps:\n      - name: x\n        run: >-\n          dnf -y install gcc-c++ cmake\n"
         "          wayland-devel python3\n\n      - uses: a/b\n",
         "yaml", ["cmake", "gcc-c++", "python3", "wayland-devel"]),
    ]
    falhas = 0
    for nome, texto, formato, esperado in casos_ok:
        obtido = sorted(set(t for p in extrair(texto, formato) for t in p))
        ok = obtido == sorted(esperado)
        print(f"selftest {nome}: {'ok' if ok else 'FALHOU'} {obtido}")
        falhas += 0 if ok else 1
    for nome, fontes in [
        ("fonte sem ponto", [("vazia", "nada aqui\n", "script")]),
        ("ponto sem pacote", [("sem", "RUN dnf -y install \\\n    && dnf clean all\n", "script")]),
    ]:
        try:
            uniao(fontes)
            print(f"selftest {nome}: FALHOU (aceitou varredura vazia)")
            falhas += 1
        except ValueError:
            print(f"selftest {nome}: ok (reprovou)")
    if falhas:
        print(f"uniao_fedora: {falhas} controle(s) falharam")
        return 1
    print("uniao_fedora: autoteste verde")
    return 0


def main(argv):
    if argv == ["--selftest"]:
        return _autoteste()
    if argv:
        print("uso: uniao_fedora.py [--selftest]", file=sys.stderr)
        return 2
    pacotes, contagem = uniao(_ler_fontes())
    for nome, quantos in contagem.items():
        print(f"uniao_fedora: {nome}: {quantos} ponto(s) de instalacao", file=sys.stderr)
    print(f"uniao_fedora: {len(pacotes)} pacote(s) na uniao", file=sys.stderr)
    for pacote in pacotes:
        print(pacote)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except ValueError as erro:
        print(f"uniao_fedora: {erro}", file=sys.stderr)
        sys.exit(1)
