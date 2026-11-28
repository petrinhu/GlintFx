#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/tools/check_cachyos_nao_republica.py - CI-PIN-PKG-C (D-WP1-0014).
#
# Trava de nao-republicacao: nenhum passo do CI publica os pacotes do Arch
# (core, extra, multilib) que a perna CachyOS baixa do Arch Linux Archive. Por
# parecer do CLO (secao 7.2), se um passo republicar um desses arquivos, ele
# volta a ser distribuicao nossa e precisa de fonte. Regras, varridas no
# .github/workflows/ci.yml inteiro:
#   R1  nenhum actions/cache aponta para o cache do pacman, para a pasta de
#       pacotes da perna ou para a onda CachyOS;
#   R2  nenhum upload-artifact aponta para o cache ou para a pasta de trabalho
#       do gerador (trabalho/, ala/, pkgs/, cache/);
#   R3  o job cachyos-repo-gerar tem exatamente um upload-artifact, e ele sobe
#       so' a pasta publicar/;
#   R4  nenhum passo publica imagem ou release (docker push, docker commit,
#       docker save, ghcr.io, gh release, action-gh-release);
#   R5  piso (L-40): ha job gerador, ha upload-artifact varrido, e cada passo
#       varrido e' contado na saida. Zero varredura reprova.
#
# Modos:
#   (sem argumento) confere tests/ ... .github/workflows/ci.yml da arvore.
#   --selftest      fixtures: positivo e sabotagem em cada regra, varredura vazia.

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CI_YML = REPO / ".github" / "workflows" / "ci.yml"
JOB_GERADOR = "cachyos-repo-gerar"
PROIBIDOS_IMAGEM = ("docker push", "docker commit", "docker save", "ghcr.io",
                    "gh release", "action-gh-release")
PROIBIDOS_CACHE = ("pacman", "/var/cache", "glintfx-onda", "cachyos")
PROIBIDOS_UPLOAD = ("pacman", "/var/cache", "trabalho/", "/ala", "/pkgs", "/cache/", "glintfx-onda")


def blocos_de_passo(linhas):
    """Devolve (indice, texto) de cada passo: `- name` ou `- uses` a 6 espacos."""
    inicios = [i for i, l in enumerate(linhas) if re.match(r"^ {6}- ", l)]
    blocos = []
    for k, i in enumerate(inicios):
        fim = inicios[k + 1] if k + 1 < len(inicios) else len(linhas)
        blocos.append((i, "\n".join(linhas[i:fim])))
    return blocos


def job_gerador(linhas):
    """Texto do job cachyos-repo-gerar (do cabecalho ate o proximo job de 2 espacos)."""
    inicio = None
    for i, l in enumerate(linhas):
        if re.match(rf"^  {re.escape(JOB_GERADOR)}:\s*$", l):
            inicio = i
            break
    if inicio is None:
        return None
    fim = len(linhas)
    for j in range(inicio + 1, len(linhas)):
        if re.match(r"^  [A-Za-z0-9_-]+:\s*$", linhas[j]):
            fim = j
            break
    return linhas[inicio:fim]


def varrer(texto):
    """Devolve (erros, contagens). Funcao pura, testada pelo autoteste."""
    erros = []
    linhas = texto.splitlines()
    n_cache = n_upload = 0

    for i, bloco in blocos_de_passo(linhas):
        # So' valores de path:/key: contam; comentario de YAML (inclusive o que
        # fica entre dois passos) nao e' caminho de upload nem de cache.
        chaves = [l.split(":", 1)[1].strip().lower() for l in bloco.splitlines()
                  if re.match(r"^\s*(path|key|restore-keys):", l)]
        valores = " \n ".join(chaves)
        if "actions/cache" in bloco.lower():
            n_cache += 1
            for p in PROIBIDOS_CACHE:
                if p in valores:
                    erros.append(f"R1 linha {i + 1}: actions/cache mexe em '{p}' (pacote do Arch, GODS_LAWS.md L-40)")
                    break
        if "actions/upload-artifact" in bloco.lower():
            n_upload += 1
            for p in PROIBIDOS_UPLOAD:
                if p in valores:
                    erros.append(f"R2 linha {i + 1}: upload-artifact aponta para '{p}' (pasta de trabalho ou cache do Arch)")
                    break

    gerador = job_gerador(linhas)
    if gerador is None:
        erros.append(f"R3: job {JOB_GERADOR} ausente (piso, L-40)")
        n_gerador_uploads = 0
    else:
        n_gerador_uploads = 0
        for _, bloco in blocos_de_passo(gerador):
            if "actions/upload-artifact" in bloco.lower():
                n_gerador_uploads += 1
                if not re.search(r"path:\s*\$\{\{ runner\.temp \}\}/saida/publicar/", bloco):
                    erros.append(f"R3: upload do {JOB_GERADOR} nao sobe so a pasta publicar/")
        if n_gerador_uploads != 1:
            erros.append(f"R3: {JOB_GERADOR} tem {n_gerador_uploads} upload-artifact, esperado 1")

    for p in PROIBIDOS_IMAGEM:
        for i, l in enumerate(linhas):
            if p in l.lower() and not l.lstrip().startswith("#"):
                erros.append(f"R4 linha {i + 1}: '{p}' publica imagem ou release (parecer CLO 7.2)")

    if n_upload == 0:
        erros.append("R5: nenhum upload-artifact varrido (piso, L-40)")
    contagens = {"passos_upload": n_upload, "passos_cache": n_cache,
                 "uploads_no_gerador": n_gerador_uploads if gerador is not None else 0}
    return erros, contagens


def _ci_bom():
    return "\n".join([
        "name: ci",
        "jobs:",
        "  build:",
        "    steps:",
        "      - uses: actions/cache@v4",
        "        with:",
        "          path: ~/.cache/pip",
        "      - uses: actions/upload-artifact@v7",
        "        with:",
        "          path: build/out/",
        "  cachyos-repo-gerar:",
        "    steps:",
        "      - name: gera",
        "        run: echo gera",
        "      - uses: actions/upload-artifact@v7",
        "        with:",
        "          name: cachyos-repo-onda",
        "          path: ${{ runner.temp }}/saida/publicar/",
        "",
    ])


def _autoteste():
    falhas = 0
    base = _ci_bom()

    def controle(nome, texto, esperar_erro):
        nonlocal falhas
        erros, cont = varrer(texto)
        ok = bool(erros) == esperar_erro
        print(f"selftest {nome}: {'ok' if ok else 'FALHOU'} ({len(erros)} erro(s), uploads varridos={cont.get('passos_upload', 0)})")
        if not ok:
            falhas += 1

    controle("positivo: ci sem republicacao passa", base, False)
    controle("negativo R1: cache do pacman", base.replace("path: ~/.cache/pip", "path: /var/cache/pacman/pkg"), True)
    controle("negativo R2: artifact com a pasta de trabalho", base.replace("path: build/out/", "path: ${{ runner.temp }}/saida/trabalho/ala/"), True)
    controle("negativo R3: gerador com dois uploads", base.replace("          path: ${{ runner.temp }}/saida/publicar/",
             "          path: ${{ runner.temp }}/saida/publicar/\n      - uses: actions/upload-artifact@v7\n        with:\n          path: x/"), True)
    controle("negativo R3: gerador sobe a pasta inteira", base.replace("saida/publicar/", "saida/"), True)
    controle("negativo R4: docker push no ci", base + "      - run: docker push ghcr.io/x/y\n", True)
    controle("negativo R4: gh release", base + "      - run: gh release create v1\n", True)
    controle("negativo R5: sem nenhum upload", base.replace("uses: actions/upload-artifact@v7", "uses: actions/checkout@v4"), True)
    controle("negativo R3: sem job gerador", base.replace("  cachyos-repo-gerar:", "  outro:"), True)

    erros_vazio, _ = varrer("")
    ok_vazio = any("piso" in e or "R5" in e for e in erros_vazio) and bool(erros_vazio)
    print(f"selftest varredura vazia reprova: {'ok' if ok_vazio else 'FALHOU'} ({len(erros_vazio)} erro(s))")
    if not ok_vazio:
        falhas += 1

    if falhas:
        print(f"check_cachyos_nao_republica: {falhas} controle(s) falharam")
        return 1
    print("check_cachyos_nao_republica: selftest verde")
    return 0


def main(argv):
    if argv == ["--selftest"]:
        return _autoteste()
    if argv:
        print("uso: check_cachyos_nao_republica.py [--selftest]", file=sys.stderr)
        return 2
    texto = CI_YML.read_text(encoding="utf-8")
    erros, cont = varrer(texto)
    resumo = ", ".join(f"{k}={v}" for k, v in cont.items())
    if cont.get("passos_upload", 0) == 0:
        print(f"check_cachyos_nao_republica: REPROVADO, zero upload varrido ({resumo}) (L-40)", file=sys.stderr)
        return 1
    if erros:
        print(f"check_cachyos_nao_republica: REPROVADO ({resumo})", file=sys.stderr)
        for e in erros:
            print(f"  - {e}", file=sys.stderr)
        return 1
    print(f"check_cachyos_nao_republica: OK ({resumo})")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
