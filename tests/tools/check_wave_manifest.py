#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/tools/check_wave_manifest.py - CI-PIN-PKG-A (ordem do CTO,
# .trabalho/cto-w8/decisao-ci-pin-pacotes.md, item 2): portao de manifesto
# das pernas de CI que congelam pacote por data (Ubuntu por snapshot, Arch
# por ALA). O manifesto (tools/ci/wave-manifest/<slug>.txt) lista, uma linha
# por pacote, "<pacote> <versao>" no formato que dpkg-query e pacman -Q
# imprimem. O portao compara com o que a perna instalou de fato e REPROVA na
# divergencia, imprimindo as duas listas.
#
# GODS_LAWS.md L-40: piso de varredura nao-vazia. Manifesto com zero entradas
# REPROVA; varredura de instalados com zero itens REPROVA. Nunca sucesso
# silencioso. L-36/L-40: o autoteste (--selftest) prova o vermelho, o verde e
# a varredura vazia, e roda em todo sistema (so exercita a comparacao pura;
# a coleta com dpkg-query/pacman so existe no servidor).
#
# Modos:
#   --slug <ubuntu|arch|cachyos> [--manifesto F] [--instalado-fixture F]
#       conferencia (o modo do CI). Sem --instalado-fixture, coleta com o
#       gerenciador da distro. Sai 1 na divergencia.
#   --gerar --slug <slug> [--instalado-fixture F]
#       grava o manifesto a partir do instalado. Uso local/servidor para
#       a proxima troca de onda; o CI nunca grava.
#   --piso
#       confere que cada manifesto da matriz tem pelo menos uma entrada e
#       que o carimbo da onda (tools/ci/wave-stamp.env) e valido.
#   --selftest
#       controles em memoria: verde, vermelho por versao, por ausencia,
#       manifesto vazio, instalado vazio, linha malformada e duplicata.

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
MANIFEST_DIR = REPO / "tools" / "ci" / "wave-manifest"
STAMP_FILE = REPO / "tools" / "ci" / "wave-stamp.env"
SLUGS = ("ubuntu", "arch", "fedora", "fedora-container", "cachyos")  # fedora*: CI-PIN-PKG-B; cachyos: CI-PIN-PKG-C (pacman, repositorio da onda)
STAMP_RE = re.compile(r"^[0-9]{8}T[0-9]{6}Z$")


def ler_manifesto(texto):
    """Devolve {pacote: versao}. Comentario (#) e linha em branco nao
    contam. Linha malformada ou pacote repetido levanta ValueError."""
    esperado = {}
    for numero, bruta in enumerate(texto.splitlines(), start=1):
        linha = bruta.strip()
        if not linha or linha.startswith("#"):
            continue
        partes = linha.split()
        if len(partes) != 2:
            raise ValueError(f"linha {numero} malformada: {bruta!r}")
        pacote, versao = partes
        if pacote in esperado:
            raise ValueError(f"linha {numero}: pacote repetido {pacote!r}")
        esperado[pacote] = versao
    return esperado


def comparar(esperado, instalado):
    """Devolve a lista de divergencias. Lista vazia = verde."""
    divergencias = []
    if not esperado:
        divergencias.append("manifesto com zero entradas (L-40)")
    if not instalado:
        divergencias.append("varredura de instalados com zero itens (L-40)")
    for pacote in sorted(esperado):
        if pacote not in instalado:
            divergencias.append(f"{pacote}: esperado {esperado[pacote]}, ausente na instalacao")
        elif instalado[pacote] != esperado[pacote]:
            divergencias.append(
                f"{pacote}: esperado {esperado[pacote]}, instalado {instalado[pacote]}"
            )
    return divergencias


def _versao_do_pacman(saida):
    """`pacman -Q <pacote>` imprime "<pacote> <versao>": devolve so a versao
    (vazio se a saida nao tiver a segunda coluna)."""
    return saida.split(maxsplit=1)[1] if " " in saida else ""


def coletar(slug, pacotes, fixture=None):
    """Devolve {pacote: versao} do que esta instalado de fato. Pacote
    ausente fica fora do dicionario (a comparacao o reprova)."""
    if fixture is not None:
        return ler_manifesto(Path(fixture).read_text(encoding="utf-8"))
    encontrado = {}
    for pacote in pacotes:
        if slug == "ubuntu":
            cmd = ["dpkg-query", "-W", "-f=${Version}", pacote]
        elif slug.startswith("fedora"):
            cmd = ["rpm", "-q", "--qf", "%{VERSION}-%{RELEASE}\n", pacote]
        else:
            cmd = ["pacman", "-Q", pacote]
        proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
        if proc.returncode != 0:
            continue
        if slug.startswith("fedora"):
            # Varias arquiteturas com a mesma versao colapsam num valor; versoes diferentes ficam juntas e reprovam.
            saida = " ".join(sorted(set(proc.stdout.split())))
        else:
            saida = proc.stdout.strip()
        if slug in ("arch", "cachyos"):
            saida = _versao_do_pacman(saida)
        if saida:
            encontrado[pacote] = saida
    return encontrado


def _listas(titulo, dicionario):
    print(f"{titulo} ({len(dicionario)}):")
    for pacote in sorted(dicionario):
        print(f"  {pacote} {dicionario[pacote]}")


def _autoteste():
    casos = [
        ("verde", {"cmake": "4.2.3", "ninja": "1.13"}, {"cmake": "4.2.3", "ninja": "1.13"}, 0),
        ("versao sabotada", {"cmake": "4.2.3"}, {"cmake": "4.2.4"}, 1),
        ("pacote ausente", {"cmake": "4.2.3", "ninja": "1.13"}, {"cmake": "4.2.3"}, 1),
        ("manifesto vazio", {}, {"cmake": "4.2.3"}, 1),
        ("instalado vazio", {"cmake": "4.2.3"}, {}, 1),
    ]
    falhas = 0
    for nome, esperado, instalado, quantas in casos:
        obtidas = comparar(esperado, instalado)
        ok = (len(obtidas) > 0) == (quantas > 0)
        print(f"selftest {nome}: {'ok' if ok else 'FALHOU'} ({len(obtidas)} divergencia(s))")
        if not ok:
            falhas += 1
    for nome, texto, esperado_erro in [
        ("linha malformada", "cmake\n", True),
        ("pacote repetido", "cmake 1\ncmake 2\n", True),
        ("so comentario", "# nada\n\n", False),
    ]:
        try:
            ler_manifesto(texto)
            erro = False
        except ValueError:
            erro = True
        ok = erro == esperado_erro
        print(f"selftest parser {nome}: {'ok' if ok else 'FALHOU'}")
        if not ok:
            falhas += 1
    # CI-PIN-PKG-C: a divisao "pacote versao" do pacman (Arch e CachyOS) e' pura, e o autoteste a prova.
    for nome, saida, esperado in [
        ("pacman arch/cachyos", "cmake 4.5.1-1", "4.5.1-1"),
        ("pacman sem versao", "", ""),
    ]:
        ok = _versao_do_pacman(saida) == esperado
        print(f"selftest {nome}: {'ok' if ok else 'FALHOU'}")
        if not ok:
            falhas += 1
    if "cachyos" not in SLUGS:
        print("selftest slug cachyos: FALHOU (fora de SLUGS)")
        falhas += 1
    if falhas:
        print(f"check_wave_manifest: {falhas} controle(s) falharam")
        return 1
    print("check_wave_manifest: autoteste verde")
    return 0


def _conferir(slug, manifesto, fixture):
    if slug not in SLUGS:
        print(f"check_wave_manifest: slug {slug!r} fora de {SLUGS}", file=sys.stderr)
        return 1
    caminho = Path(manifesto) if manifesto else MANIFEST_DIR / f"{slug}.txt"
    try:
        esperado = ler_manifesto(caminho.read_text(encoding="utf-8"))
    except (OSError, ValueError) as erro:
        print(f"check_wave_manifest: manifesto {slug}: {erro}", file=sys.stderr)
        return 1
    if not esperado:
        print(f"check_wave_manifest: manifesto {caminho} tem zero entradas (L-40)", file=sys.stderr)
        return 1
    instalado = coletar(slug, sorted(esperado), fixture)
    print(f"check_wave_manifest: varreu {len(esperado)} pacote(s) do manifesto de {slug}")
    _listas("manifesto", esperado)
    _listas("instalado", instalado)
    divergencias = comparar(esperado, instalado)
    for linha in divergencias:
        print(f"DIVERGENCIA: {linha}", file=sys.stderr)
    if divergencias:
        print(f"check_wave_manifest: {slug} REPROVADO ({len(divergencias)} divergencia(s))", file=sys.stderr)
        return 1
    print(f"check_wave_manifest: {slug} verde")
    return 0


def _gerar(slug, fixture):
    instalado = coletar(slug, [], fixture) if fixture else {}
    if not instalado:
        print("check_wave_manifest: --gerar sem instalado: nada a gravar (L-40)", file=sys.stderr)
        return 1
    caminho = MANIFEST_DIR / f"{slug}.txt"
    corpo = "# manifesto gerado por --gerar; conferir antes de commitar\n"
    corpo += "".join(f"{p} {v}\n" for p, v in sorted(instalado.items()))
    caminho.write_text(corpo, encoding="utf-8")
    print(f"check_wave_manifest: gravado {caminho} ({len(instalado)} entradas)")
    return 0


def _piso():
    falhas = 0
    for slug in SLUGS:
        caminho = MANIFEST_DIR / f"{slug}.txt"
        try:
            entradas = ler_manifesto(caminho.read_text(encoding="utf-8"))
        except (OSError, ValueError) as erro:
            print(f"piso {slug}: {erro}", file=sys.stderr)
            falhas += 1
            continue
        print(f"piso {slug}: {len(entradas)} entrada(s)")
        if not entradas:
            print(f"piso {slug}: zero entradas REPROVA (L-40)", file=sys.stderr)
            falhas += 1
    stamp = {}
    for linha in STAMP_FILE.read_text(encoding="utf-8").splitlines():
        if "=" in linha and not linha.lstrip().startswith("#"):
            chave, valor = linha.split("=", 1)
            stamp[chave.strip()] = valor.strip()
    onda = stamp.get("ONDA_STAMP", "")
    print(f"piso carimbo: ONDA_STAMP={onda or '<vazio>'}")
    if not STAMP_RE.match(onda):
        print("piso carimbo: ONDA_STAMP fora do formato AAAAMMDDTHHMMSSZ", file=sys.stderr)
        falhas += 1
    return 1 if falhas else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Portao de manifesto de pacotes do CI (CI-PIN-PKG-A).")
    parser.add_argument("--slug", choices=SLUGS)
    parser.add_argument("--manifesto")
    parser.add_argument("--instalado-fixture")
    parser.add_argument("--gerar", action="store_true")
    parser.add_argument("--piso", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)
    if args.selftest:
        return _autoteste()
    if args.piso:
        return _piso()
    if args.gerar:
        return _gerar(args.slug, args.instalado_fixture)
    if not args.slug:
        parser.error("--slug e obrigatorio fora de --selftest e --piso")
    return _conferir(args.slug, args.manifesto, args.instalado_fixture)


if __name__ == "__main__":
    sys.exit(main())
