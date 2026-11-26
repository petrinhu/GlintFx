#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# source_snapshot.py - E2c (VENDOR-PURITY-TRABALHO, TMPDIR-NA-ARVORE): o rascunho de um teste de
# CMake que cai DENTRO da arvore-fonte copia essa arvore para fora dele. Sem isso, o CMake recusa
# um include absoluto dentro da fonte que nao esta dentro do prefixo de instalacao (causa no
# cmExportInstallFileGenerator::CheckInterfaceDirs). Os cenarios dos testes nao mudam: so a fonte.
#
# Uso:
#   source_snapshot.py resolve <fonte> <rascunho>   imprime, na ultima linha de stdout, o caminho
#                                                   a configurar; o relato vai a stderr
#   source_snapshot.py --selftest

import os
import shutil
import subprocess
import sys


class SnapshotError(Exception):
    """A copia nao pode ser feita: a varredura veio vazia, ou algo falhou. Nunca presume vazio."""


def _inside(child, parent):
    """Estritamente dentro: o mesmo caminho nao conta. Os dois ja' vem de realpath."""
    return child != parent and child.startswith(parent.rstrip(os.sep) + os.sep)


def _is_scratch_or_inside(path, real_scratch):
    return path == real_scratch or _inside(path, real_scratch)


def _git_listing(real_source):
    """O rastreado e o nao rastreado nao ignorado, do git. None quando o git nao responde."""
    try:
        done = subprocess.run(
            ["git", "-C", real_source, "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
            capture_output=True,
        )
    except OSError:
        return None
    if done.returncode != 0:
        return None
    return [item for item in done.stdout.decode("utf-8", "surrogateescape").split("\0") if item]


def _walk_listing(real_source, real_scratch):
    """Sem git (fonte de tarball): varre a arvore, sem .git e sem entrar no rascunho."""
    listing = []
    for dirpath, dirnames, filenames in os.walk(real_source):
        dirnames[:] = [
            name for name in dirnames
            if name != ".git"
            and not _is_scratch_or_inside(os.path.realpath(os.path.join(dirpath, name)), real_scratch)
        ]
        for name in filenames:
            listing.append(os.path.relpath(os.path.join(dirpath, name), real_source))
    return listing


def resolve_source(source_dir, scratch_dir):
    """(caminho a configurar, arquivos copiados). A fonte fica como esta, a menos que o rascunho
    esteja estritamente dentro dela: so' ai a arvore vai para <rascunho>/source (disco, nao HEAD)."""
    real_source = os.path.realpath(source_dir)
    real_scratch = os.path.realpath(scratch_dir)
    if not _inside(real_scratch, real_source):
        return source_dir, 0
    listing = _git_listing(real_source)
    if listing is None:
        listing = _walk_listing(real_source, real_scratch)
    destination = os.path.join(scratch_dir, "source")
    copied = 0
    for rel in sorted(set(listing)):
        origin = os.path.join(real_source, rel)
        if not os.path.isfile(origin):
            continue  # apagado sem commit, ou um caminho que o disco ja' nao tem
        if _is_scratch_or_inside(os.path.realpath(origin), real_scratch):
            continue
        target = os.path.join(destination, rel)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copy2(origin, target)
        copied += 1
    if copied == 0:
        raise SnapshotError(
            f"varredura vazia: nenhum arquivo para copiar de {real_source} (GODS_LAWS.md L-40, nunca presumida ok)"
        )
    return destination, copied


def _git(root, *args):
    subprocess.run(["git", "-C", root, *args], check=True, capture_output=True)


def _repo(root):
    os.makedirs(root)
    _git(root, "init", "-q")
    _git(root, "config", "user.email", "selftest@source-snapshot.invalid")
    _git(root, "config", "user.name", "source_snapshot selftest")


def _put(root, rel, text, mode=None):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)
    if mode is not None:
        os.chmod(path, mode)


def _control_git_inside(work):
    """Rascunho dentro de um repositorio: copia o rastreado (do disco, modificado), o nao rastreado
    e nao ignorado, com o modo; NAO copia o ignorado, o apagado sem commit, nem o proprio rascunho."""
    src = os.path.join(work, "fonte-git")
    _repo(src)
    _put(src, "tracked.txt", "versao do commit\n")
    _put(src, "exec.sh", "#!/bin/sh\n", mode=0o755)
    _put(src, "apagado.txt", "sera apagado\n")
    _put(src, ".gitignore", "ignorado.txt\n")
    _git(src, "add", "-A")
    _git(src, "commit", "-q", "-m", "base")
    _put(src, "tracked.txt", "versao do disco, modificada\n")
    _put(src, "novo.txt", "nao rastreado\n")
    _put(src, "ignorado.txt", "ignorado\n")
    os.remove(os.path.join(src, "apagado.txt"))
    scratch = os.path.join(src, "rascunho")
    os.makedirs(scratch)
    _put(scratch, "lixo.txt", "do rascunho\n")
    dst, copied = resolve_source(src, scratch)
    checks = [
        (dst == os.path.join(scratch, "source"), "destino fora do rascunho"),
        (copied == 4, f"esperados 4 arquivos (tracked, exec, novo, .gitignore), vieram {copied}"),
        (_read(dst, "tracked.txt") == "versao do disco, modificada\n", "conteudo do disco, nao o do commit"),
        (os.path.isfile(os.path.join(dst, "novo.txt")), "nao rastreado e nao ignorado deveria ser copiado"),
        (not os.path.exists(os.path.join(dst, "ignorado.txt")), "o ignorado nao devia ser copiado"),
        (not os.path.exists(os.path.join(dst, "apagado.txt")), "o apagado sem commit nao devia ser copiado"),
        (not os.path.exists(os.path.join(dst, "rascunho")), "o rascunho nao pode entrar na copia (recursao)"),
        ((os.stat(os.path.join(dst, "exec.sh")).st_mode & 0o777) == 0o755, "o bit executavel se perdeu"),
    ]
    return _verdict("rascunho dentro de repositorio", checks)


def _read(root, rel):
    with open(os.path.join(root, rel), encoding="utf-8") as handle:
        return handle.read()


def _control_nogit_inside(work):
    """Rascunho dentro de uma fonte SEM git (tarball): varre a arvore, sem entrar no rascunho."""
    src = os.path.join(work, "fonte-tarball")
    os.makedirs(os.path.join(src, "sub"))
    _put(src, "a.txt", "a\n")
    _put(src, os.path.join("sub", "b.txt"), "b\n")
    scratch = os.path.join(src, "rascunho")
    os.makedirs(scratch)
    _put(scratch, "lixo.txt", "do rascunho\n")
    dst, copied = resolve_source(src, scratch)
    checks = [
        (copied == 2, f"esperados 2 arquivos, vieram {copied}"),
        (os.path.isfile(os.path.join(dst, "sub", "b.txt")), "o subdiretorio devia ser copiado"),
        (not os.path.exists(os.path.join(dst, "rascunho")), "sem git, o rascunho tambem nao pode entrar"),
    ]
    return _verdict("rascunho dentro de fonte sem git", checks)


def _control_outside(work):
    """Rascunho fora da fonte: devolve a original, sem copiar nada."""
    src = os.path.join(work, "fonte-fora")
    _repo(src)
    _put(src, "x.txt", "x\n")
    _git(src, "add", "-A")
    _git(src, "commit", "-q", "-m", "base")
    scratch = os.path.join(work, "rascunho-fora")
    os.makedirs(scratch)
    path, copied = resolve_source(src, scratch)
    checks = [
        (os.path.realpath(path) == os.path.realpath(src), "fora da fonte devia devolver a original"),
        (copied == 0, "fora da fonte nao se copia nada"),
    ]
    return _verdict("rascunho fora da fonte", checks)


def _control_empty(work):
    """Varredura vazia (so o rascunho dentro da fonte): recusa, nunca presume copia vazia-e-ok."""
    src = os.path.join(work, "fonte-vazia")
    _repo(src)
    scratch = os.path.join(src, "rascunho")
    os.makedirs(scratch)
    _put(scratch, "so.txt", "so do rascunho\n")
    try:
        resolve_source(src, scratch)
    except SnapshotError:
        return _verdict("varredura vazia recusada", [(True, "")])
    return _verdict("varredura vazia recusada", [(False, "uma varredura vazia devia ser recusada")])


def _verdict(name, checks):
    falhas = [motivo for ok, motivo in checks if not ok]
    if falhas:
        for motivo in falhas:
            print(f"source_snapshot.py --selftest: controle {name} FALHOU: {motivo}", file=sys.stderr)
        return False
    print(f"source_snapshot.py --selftest: controle {name} OK")
    return True


def selftest():
    import tempfile

    work = tempfile.mkdtemp(prefix="glintfx-source-snapshot-", dir=os.environ.get("TMPDIR") or None)
    try:
        controls = [
            _control_git_inside(work),
            _control_nogit_inside(work),
            _control_outside(work),
            _control_empty(work),
        ]
    finally:
        shutil.rmtree(work, ignore_errors=True)
    if not all(controls):
        print("source_snapshot.py --selftest: FALHOU (ver acima)", file=sys.stderr)
        return 1
    print(f"source_snapshot.py --selftest: os {len(controls)} controles OK")
    return 0


def main(argv):
    if argv[1:] == ["--selftest"]:
        return selftest()
    if len(argv) == 4 and argv[1] == "resolve":
        try:
            path, copied = resolve_source(argv[2], argv[3])
        except SnapshotError as error:
            print(f"source_snapshot.py: {error}", file=sys.stderr)
            return 1
        if copied:
            print(f"fonte: copiada para {path} ({copied} arquivos; o rascunho esta dentro da arvore-fonte)",
                  file=sys.stderr)
        else:
            print(f"fonte: original {path}", file=sys.stderr)
        print(path)
        return 0
    print("uso: source_snapshot.py resolve <fonte> <rascunho> | --selftest", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
