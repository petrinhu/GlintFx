#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# tests/tools/check_cachyos_onda.py - CI-PIN-PKG-C (D-WP1-0014, desenho 1a).
#
# Portao de correspondencia do artifact cachyos-repo-onda. Confere o que o
# gerador (tools/ci/env/lock/gerar_cachyos_repo.sh) vai publicar, e reconfere o
# artifact baixado. Regras:
#   - origem: todo binario do tar e' um pacote do lock com repo == cachyos,
#     com o mesmo nome, a mesma versao, o mesmo sha256 e o mesmo
#     pkgbuild_sha256sum do .BUILDINFO. Pacote de core/extra/multilib no tar
#     reprova (o Arch nunca sai da perna pelo artifact);
#   - fonte exato: cada pacote tem a receita em fonte/<pkgbase>/ com o
#     PKGBUILD de hash igual ao do binario, e cada fonte do .SRCINFO existe
#     com o sha256 da linha sha256sums;
#   - licenca: cada pkgbase tem ao menos um arquivo de licenca;
#   - manifesto e SOURCES.md: uma linha por binario, origem cachyos, e a frase
#     de ressalva do CLO (parecer, secao 7.4, item 3);
#   - piso (GODS_LAWS.md L-40): zero binario, zero pacote cachyos no lock ou
#     lock sem linha do Arch reprovam. A contagem aparece na saida.
#
# Modos:
#   --publicar DIR  confere a pasta de publicacao do gerador (cachyos-onda.tar,
#                   cachyos.lock, manifesto.txt, SOURCES.md, cachyos-versoes.txt).
#   --selftest      controles com fixtures de texto: positivo, negativos
#                   (sabotagem nos dois sentidos) e varredura vazia.
#
# A decisao (decidir) e' pura; a leitura do tar e dos .pkg.tar.zst (zstd e tar)
# so' existe no modo --publicar, que roda no contenedor.

import hashlib
import re
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

FRASE_CLO = ("pode conter material GPL-2.0 (uapi) e um módulo ligado a "
             "GPL-3.0 (`gdbm`); o fonte correspondente está neste artifact")
REPOS_ARCH = {"core", "extra", "multilib"}
ARQUIVOS_PUBLICACAO = ("cachyos-onda.tar", "cachyos.lock", "manifesto.txt",
                       "SOURCES.md", "cachyos-versoes.txt")


FAMILIAS_DE_SOMA = ("sha256sums", "sha512sums", "b2sums")


def pares_do_srcinfo(texto):
    """Espelha o awk do gerador: source e a familia de soma da secao pkgbase
    (antes do primeiro pkgname). A familia e' a primeira declarada, na ordem
    sha256sums, sha512sums, b2sums. Devolve (por_arquitetura, familia, fontes,
    somas); familia vazia quando nao ha nenhuma soma."""
    fim = False
    fontes = []
    somas = {f: [] for f in FAMILIAS_DE_SOMA}
    por_arq = False
    for linha in texto.splitlines():
        s = linha.strip()
        if s.startswith("pkgname = "):
            fim = True
        if not fim:
            if s.startswith("source = "):
                fontes.append(s[len("source = "):])
            else:
                for f in FAMILIAS_DE_SOMA:
                    if s.startswith(f + " = "):
                        somas[f].append(s[len(f) + 3:])
        if re.match(r"(source|sha256sums|sha512sums|b2sums)_[A-Za-z0-9_]+ = ", s):
            por_arq = True
    familia = next((f for f in FAMILIAS_DE_SOMA if somas[f]), "")
    return por_arq, familia, fontes, somas.get(familia, [])


def hash_de(caminho, familia):
    """Hash do arquivo pela familia declarada pela receita."""
    if familia == "sha512sums":
        h = hashlib.sha512()
    elif familia == "b2sums":
        h = hashlib.blake2b(digest_size=64)
    else:
        h = hashlib.sha256()
    h.update(Path(caminho).read_bytes())
    return h.hexdigest()


def nome_do_arquivo_da_fonte(entrada):
    """`nome::url` usa o nome antes de `::`; `url` usa o ultimo segmento."""
    if "::" in entrada:
        return entrada.split("::", 1)[0]
    if "://" in entrada:
        return entrada.rsplit("/", 1)[-1]
    return entrada


def decidir(lock, binarios, fontes, licencas, manifesto, sources_md, versoes):
    """Funcao pura. lock: lista de dict(repo,nome,versao,arq,sha,pbsha).
    binarios: dict arq -> dict(nome,versao,pkgbase,sha,pbsha,sig) do tar.
    fontes: dict pkgbase -> dict(pbsha, arquivos: dict nome->sha real,
            esperados: lista de (nome, sha do .SRCINFO), commit).
    licencas: dict pkgbase -> quantidade de arquivos de licenca.
    manifesto: lista de dict(nome,pkgbase,versao,origem,sha).
    Devolve (erros, contagens)."""
    erros = []
    cy_lock = {l["arq"]: l for l in lock if l["repo"] == "cachyos"}
    arch_lock = [l for l in lock if l["repo"] != "cachyos"]

    # Piso (L-40): a varredura nao pode ser vazia.
    if not cy_lock:
        erros.append("piso: lock sem nenhum pacote cachyos (L-40)")
    if not arch_lock:
        erros.append("piso: lock sem nenhum pacote do Arch (L-40)")
    if not binarios:
        erros.append("piso: artifact sem nenhum binario (L-40)")

    for l in lock:
        if l["repo"] == "cachyos" and l["pbsha"] in ("", "-"):
            erros.append(f"lock: {l['arq']} cachyos sem pkgbuild_sha256 (L-40)")
        if l["repo"] != "cachyos" and l["pbsha"] != "-":
            erros.append(f"lock: {l['arq']} do Arch com pkgbuild_sha256 (formato) (L-40)")
        if l["repo"] not in REPOS_ARCH | {"cachyos"}:
            erros.append(f"lock: repo inesperado {l['repo']} em {l['arq']} (L-40)")

    # Origem: so' pacote cachyos pode estar no artifact.
    for arq, b in sorted(binarios.items()):
        l = cy_lock.get(arq)
        if l is None:
            erros.append(f"origem: {arq} nao e' pacote cachyos no lock (pode ser do Arch) (L-40)")
            continue
        if b["nome"] != l["nome"] or b["versao"] != l["versao"]:
            erros.append(f"origem: {arq} tem nome/versao {b['nome']} {b['versao']}, lock diz {l['nome']} {l['versao']}")
        if b["sha"] != l["sha"]:
            erros.append(f"sha256: {arq} diverge do lock")
        if b["pbsha"] != l["pbsha"]:
            erros.append(f"fonte: {arq} pkgbuild_sha256sum do .BUILDINFO diverge do lock")
        if not b.get("sig"):
            erros.append(f"assinatura: {arq} sem .sig no artifact (L-40)")

    for arq in sorted(cy_lock):
        if arq not in binarios:
            erros.append(f"artifact: {arq} esta no lock como cachyos mas nao foi publicado (L-40)")

    # Fonte exato por pkgbase.
    pkgbases = sorted({b["pkgbase"] for b in binarios.values()})
    n_fontes_ok = 0
    for pb in pkgbases:
        f = fontes.get(pb)
        if f is None:
            erros.append(f"fonte: {pb} sem a pasta fonte/{pb}/ no artifact (L-40)")
            continue
        if not f.get("commit"):
            erros.append(f"fonte: {pb} sem COMMIT (L-40)")
        bin_pbsha = {b["pbsha"] for b in binarios.values() if b["pkgbase"] == pb}
        if f.get("pbsha") not in bin_pbsha:
            erros.append(f"fonte: {pb} PKGBUILD com sha {f.get('pbsha')} diferente do .BUILDINFO")
        if not f.get("esperados"):
            erros.append(f"fonte: {pb} sem nenhuma fonte no .SRCINFO (L-40)")
        for nome, esperado in f.get("esperados", []):
            real = f.get("arquivos", {}).get(nome)
            if real is None:
                erros.append(f"fonte: {pb} sem o arquivo {nome} (L-40)")
            elif real != esperado:
                erros.append(f"fonte: {pb} {nome} sha {real} diverge do .SRCINFO {esperado} (L-40)")
        if f.get("esperados") and all(f.get("arquivos", {}).get(n) == s for n, s in f["esperados"]):
            n_fontes_ok += 1
        if licencas.get(pb, 0) < 1:
            erros.append(f"licenca: {pb} sem nenhum texto de licenca (L-40)")

    # Manifesto: uma linha por binario, origem cachyos.
    if len(manifesto) != len(binarios):
        erros.append(f"manifesto: {len(manifesto)} linha(s) para {len(binarios)} binario(s) (L-40)")
    for m in manifesto:
        if m["origem"] != "cachyos":
            erros.append(f"manifesto: {m['nome']} com origem {m['origem']} (L-40)")
    manifestados = {m["sha"] for m in manifesto}
    for arq, b in binarios.items():
        if b["sha"] not in manifestados:
            erros.append(f"manifesto: {arq} sem linha com o seu sha256")

    if FRASE_CLO not in sources_md:
        erros.append("SOURCES.md: falta a ressalva do CLO (parecer 7.4, item 3)")
    if not versoes:
        erros.append("cachyos-versoes.txt vazio (L-40)")

    contagens = {
        "binarios": len(binarios),
        "cachyos_no_lock": len(cy_lock),
        "arch_no_lock": len(arch_lock),
        "fontes_conferidas": n_fontes_ok,
        "pkgbases": len(pkgbases),
        "manifesto": len(manifesto),
    }
    return erros, contagens


# ---------------------------------------------------------------------------
# Leitura real (so' no modo --publicar)
# ---------------------------------------------------------------------------

def ler_lock(caminho):
    linhas = []
    for crua in Path(caminho).read_text(encoding="utf-8").splitlines():
        if not crua.strip() or crua.startswith("#"):
            continue
        campos = crua.split()
        if len(campos) != 6:
            raise SystemExit(f"lock malformado (6 campos, achei {len(campos)}): {crua!r}")
        repo, nome, versao, arq, sha, pbsha = campos
        linhas.append({"repo": repo, "nome": nome, "versao": versao,
                       "arq": arq, "sha": sha, "pbsha": pbsha})
    return linhas


def ler_membro_do_pacote(pacote, membro):
    z = subprocess.run(["zstd", "-dc", "-q", str(pacote)], capture_output=True, check=True)
    t = subprocess.run(["tar", "-xOf", "-", membro], input=z.stdout, capture_output=True)
    if t.returncode != 0:
        raise SystemExit(f"{pacote.name}: sem {membro}: {t.stderr.decode(errors='replace')}")
    return t.stdout.decode("utf-8", errors="replace")


def sha256_de(caminho):
    return hashlib.sha256(Path(caminho).read_bytes()).hexdigest()


def ler_campo(texto, chave):
    for linha in texto.splitlines():
        if linha.startswith(chave + " = "):
            return linha[len(chave) + 3:].strip()
    return ""


def coletar(pub):
    """Le o tar e a pasta de publicacao; devolve os argumentos de decidir()."""
    pub = Path(pub)
    for nome in ARQUIVOS_PUBLICACAO:
        if not (pub / nome).is_file():
            raise SystemExit(f"publicacao sem {nome} (L-40)")
    lock = ler_lock(pub / "cachyos.lock")
    tmp = Path(tempfile.mkdtemp(prefix="cachyos-onda-"))
    with tarfile.open(pub / "cachyos-onda.tar") as tar:
        if hasattr(tarfile, "data_filter"):
            tar.extractall(tmp, filter="data")
        else:
            tar.extractall(tmp)

    binarios = {}
    for pkg in sorted((tmp / "binarios").glob("*.pkg.tar.zst")):
        info = ler_membro_do_pacote(pkg, ".PKGINFO")
        bi = ler_membro_do_pacote(pkg, ".BUILDINFO")
        binarios[pkg.name] = {
            "nome": ler_campo(info, "pkgname"),
            "versao": ler_campo(info, "pkgver"),
            "pkgbase": ler_campo(info, "pkgbase"),
            "sha": sha256_de(pkg),
            "pbsha": ler_campo(bi, "pkgbuild_sha256sum"),
            "sig": (tmp / "binarios" / (pkg.name + ".sig")).is_file(),
        }

    fontes, licencas = {}, {}
    for pb in {b["pkgbase"] for b in binarios.values()}:
        d = tmp / "fonte" / pb
        if not d.is_dir():
            continue
        pb_path = d / "PKGBUILD"
        commit = (d / "COMMIT").read_text(encoding="utf-8").strip() if (d / "COMMIT").is_file() else ""
        por_arq, familia, entradas, sums = pares_do_srcinfo(
            (d / ".SRCINFO").read_text(encoding="utf-8")) if (d / ".SRCINFO").is_file() else (False, "", [], [])
        if por_arq or not familia or len(entradas) != len(sums) or "SKIP" in sums:
            raise SystemExit(f"{pb}: .SRCINFO com arquitetura, sem soma, desigual ou SKIP (L-40)")
        esperados, arquivos = [], {}
        for entrada, soma in zip(entradas, sums):
            nome = nome_do_arquivo_da_fonte(entrada)
            esperados.append((nome, soma))
            # O gerador grava toda fonte em tarballs/ (as locais tambem); a copia
            # que fica na raiz da receita nao conta como fonte do artifact.
            cand = d / "tarballs" / nome
            if cand.is_file():
                arquivos[nome] = hash_de(cand, familia)
        fontes[pb] = {
            "commit": commit,
            "pbsha": sha256_de(pb_path) if pb_path.is_file() else "",
            "esperados": esperados,
            "arquivos": arquivos,
        }
        lic = tmp / "licencas" / pb
        licencas[pb] = sum(1 for p in lic.rglob("*") if p.is_file()) if lic.is_dir() else 0

    manifesto = []
    for linha in (pub / "manifesto.txt").read_text(encoding="utf-8").splitlines():
        if not linha.strip():
            continue
        c = linha.split(maxsplit=6)
        if len(c) < 6:
            raise SystemExit(f"manifesto malformado: {linha!r}")
        manifesto.append({"nome": c[0], "pkgbase": c[1], "versao": c[2],
                          "origem": c[3], "sha": c[4], "pbsha": c[5]})
    sources_md = (pub / "SOURCES.md").read_text(encoding="utf-8")
    versoes = [v for v in (pub / "cachyos-versoes.txt").read_text(encoding="utf-8").splitlines() if v.strip()]
    return lock, binarios, fontes, licencas, manifesto, sources_md, versoes


def publicar(pub):
    lock, binarios, fontes, licencas, manifesto, sources_md, versoes = coletar(pub)
    erros, cont = decidir(lock, binarios, fontes, licencas, manifesto, sources_md, versoes)
    resumo = ", ".join(f"{k}={v}" for k, v in cont.items())
    if erros:
        print(f"check_cachyos_onda: REPROVADO ({resumo})", file=sys.stderr)
        for e in erros:
            print(f"  - {e}", file=sys.stderr)
        return 1
    print(f"check_cachyos_onda: OK ({resumo})")
    return 0


# ---------------------------------------------------------------------------
# Autoteste: fixtures de texto, controles positivo, negativos e varredura vazia.
# ---------------------------------------------------------------------------

def _fixture():
    lock = [
        {"repo": "core", "nome": "gcc", "versao": "16.2.1-1", "arq": "gcc-16.2.1-1-x86_64.pkg.tar.zst", "sha": "a1", "pbsha": "-"},
        {"repo": "extra", "nome": "libxau", "versao": "1.0.12-1", "arq": "libxau-1.0.12-1-x86_64.pkg.tar.zst", "sha": "a2", "pbsha": "-"},
        {"repo": "cachyos", "nome": "mesa", "versao": "3:26.3.1-5", "arq": "mesa-3:26.3.1-5-x86_64.pkg.tar.zst", "sha": "c1", "pbsha": "p1"},
        {"repo": "cachyos", "nome": "python", "versao": "3.14.10-3", "arq": "python-3.14.10-3-x86_64.pkg.tar.zst", "sha": "c2", "pbsha": "p2"},
    ]
    binarios = {
        "mesa-3:26.3.1-5-x86_64.pkg.tar.zst": {"nome": "mesa", "versao": "3:26.3.1-5", "pkgbase": "mesa", "sha": "c1", "pbsha": "p1", "sig": True},
        "python-3.14.10-3-x86_64.pkg.tar.zst": {"nome": "python", "versao": "3.14.10-3", "pkgbase": "python", "sha": "c2", "pbsha": "p2", "sig": True},
    }
    fontes = {
        "mesa": {"commit": "0a748aa", "pbsha": "p1", "esperados": [("mesa-26.3.1.tar.xz", "s1"), ("gamescope.patch", "s2")],
                 "arquivos": {"mesa-26.3.1.tar.xz": "s1", "gamescope.patch": "s2"}},
        "python": {"commit": "486", "pbsha": "p2", "esperados": [("Python-3.14.10.tar.xz", "s3")],
                   "arquivos": {"Python-3.14.10.tar.xz": "s3"}},
    }
    licencas = {"mesa": 3, "python": 1}
    manifesto = [
        {"nome": "mesa", "pkgbase": "mesa", "versao": "3:26.3.1-5", "origem": "cachyos", "sha": "c1", "pbsha": "p1"},
        {"nome": "python", "pkgbase": "python", "versao": "3.14.10-3", "origem": "cachyos", "sha": "c2", "pbsha": "p2"},
    ]
    sources_md = "texto\n> " + FRASE_CLO + "\n"
    versoes = ["mesa 3:26.3.1-5", "python 3.14.10-3"]
    return lock, binarios, fontes, licencas, manifesto, sources_md, versoes


def _decidir_fixture(mod):
    lock, binarios, fontes, licencas, manifesto, sources_md, versoes = _fixture()
    args = {"lock": lock, "binarios": binarios, "fontes": fontes, "licencas": licencas,
            "manifesto": manifesto, "sources_md": sources_md, "versoes": versoes}
    mod(args)
    return decidir(**args)


def _autoteste():
    falhas = 0

    def controle(nome, esperar_erro, mod):
        nonlocal falhas
        erros, cont = _decidir_fixture(mod)
        ok = bool(erros) == esperar_erro
        print(f"selftest {nome}: {'ok' if ok else 'FALHOU'} ({len(erros)} erro(s), {cont.get('binarios', 0)} binario(s))")
        if not ok:
            falhas += 1
        return erros

    controle("positivo: onda completa passa", False, lambda a: None)
    controle("negativo: arquivo do Arch no artifact (origem)", True,
             lambda a: a["binarios"].update({"libxau-1.0.12-1-x86_64.pkg.tar.zst": {
                 "nome": "libxau", "versao": "1.0.12-1", "pkgbase": "libxau", "sha": "a2", "pbsha": "-", "sig": True}}))
    controle("negativo: fonte de um pacote dividido ausente", True,
             lambda a: a["fontes"]["mesa"]["arquivos"].pop("gamescope.patch"))
    controle("negativo: sha de fonte diverge do .SRCINFO", True,
             lambda a: a["fontes"]["python"]["arquivos"].update({"Python-3.14.10.tar.xz": "errado"}))
    controle("negativo: pasta fonte de um pacote inteira ausente", True,
             lambda a: a["fontes"].pop("python"))
    controle("negativo: binario sem assinatura", True,
             lambda a: a["binarios"]["python-3.14.10-3-x86_64.pkg.tar.zst"].update({"sig": False}))
    controle("negativo: PKGBUILD diverge do .BUILDINFO", True,
             lambda a: a["fontes"]["mesa"].update({"pbsha": "outro"}))
    controle("negativo: manifesto com uma linha a menos", True,
             lambda a: a["manifesto"].pop())
    controle("negativo: SOURCES.md sem a ressalva do CLO", True,
             lambda a: a.update({"sources_md": "sem ressalva"}))
    controle("negativo: licenca ausente", True,
             lambda a: a["licencas"].update({"python": 0}))
    controle("negativo: lock sem a linha de um binario cachyos", True,
             lambda a: a["lock"].pop(3))
    controle("negativo: origem declarada diferente de cachyos no manifesto", True,
             lambda a: a["manifesto"][0].update({"origem": "extra"}))

    # Varredura vazia (L-40): zero em tudo tem de reprovar e nunca passar.
    erros_vazio, cont_vazio = _decidir_fixture(lambda a: (a.update({"binarios": {}, "manifesto": [], "lock": []})))
    ok_vazio = len(erros_vazio) > 0 and any("piso" in e for e in erros_vazio)
    print(f"selftest varredura vazia reprova: {'ok' if ok_vazio else 'FALHOU'} ({len(erros_vazio)} erro(s))")
    if not ok_vazio:
        falhas += 1

    if falhas:
        print(f"check_cachyos_onda: {falhas} controle(s) falharam")
        return 1
    print("check_cachyos_onda: selftest verde")
    return 0


def main(argv):
    if argv == ["--selftest"]:
        return _autoteste()
    if len(argv) == 2 and argv[0] == "--publicar":
        return publicar(argv[1])
    print("uso: check_cachyos_onda.py --selftest | --publicar DIR", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
