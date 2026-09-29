#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# fase.py - helper de FASE do CI-SPLIT-PER-OS A5 (F1, DESENHO.md 3.3), gemeo de fase.sh e de
# tools/ci/fase.ps1. Imprime `FASE <nome>: X.XX s` e `paralelismo aninhado: <n|ausente>` na saida
# padrao E no arquivo LATERAL <GLINTFX_FASES_DIR>/<GLINTFX_FASES_TESTE>.txt (o ctest corta a saida
# de teste aprovado em 1024 bytes; so' o arquivo lateral e' confiavel). Formato identico nas tres
# linguagens: tests/tools/fixtures/fase/esperado*.txt. Relogio: time.monotonic_ns, em centesimos.
#
# Uso:  import fase; fase.inicio("configure"); ...; fase.fim("configure"); fase.paralelismo()
#       python3 fase.py --selftest

import os
import subprocess
import sys
import tempfile
import time

_T0 = {}


def _agora_cs():
    return time.monotonic_ns() // 10_000_000


def _emitir(linha):
    print(linha, flush=True)
    pasta = os.environ.get("GLINTFX_FASES_DIR")
    teste = os.environ.get("GLINTFX_FASES_TESTE")
    if pasta and teste:
        os.makedirs(pasta, exist_ok=True)
        with open(os.path.join(pasta, teste + ".txt"), "a", encoding="utf-8", newline="\n") as handle:
            handle.write(linha + "\n")


def registrar(nome, centesimos):
    _emitir(f"FASE {nome}: {centesimos // 100}.{centesimos % 100:02d} s")


def inicio(nome):
    _T0[nome] = _agora_cs()


def fim(nome):
    if nome not in _T0:
        raise RuntimeError(f"fase.fim('{nome}') sem fase.inicio")
    registrar(nome, _agora_cs() - _T0.pop(nome))


def paralelismo():
    _emitir("paralelismo aninhado: " + (os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL") or "ausente"))


def _capturar(codigo, ambiente):
    """Roda `codigo` (Python) num processo filho com `ambiente`; devolve a saida padrao."""
    env = {k: v for k, v in os.environ.items() if not k.startswith(("GLINTFX_FASES_", "CMAKE_BUILD_PARALLEL"))}
    env.update(ambiente)
    env["PYTHONPATH"] = os.path.dirname(os.path.abspath(__file__))
    return subprocess.run([sys.executable, "-c", codigo], env=env, capture_output=True, text=True, check=False)


def _ler_fixture(caminho):
    """Bytes da fixture SEM o cabecalho SPDX (linhas iniciadas por `#`, L-08)."""
    with open(caminho, "rb") as handle:
        return b"".join(l for l in handle.read().splitlines(keepends=True) if not l.startswith(b"#"))


def _selftest():
    aqui = os.path.dirname(os.path.abspath(__file__))
    fx = os.path.join(aqui, "fixtures", "fase")
    esperado = _ler_fixture(os.path.join(fx, "esperado.txt"))
    esperado_ausente = _ler_fixture(os.path.join(fx, "esperado_ausente.txt"))
    erros = []
    with tempfile.TemporaryDirectory(prefix="glintfx-fase-selftest-") as tmp:
        lateral = os.path.join(tmp, "fases")
        # 1. formato byte a byte contra a fixture unica, na saida padrao E no arquivo lateral
        r = _capturar("import fase; fase.registrar('configure',1234); fase.registrar('build',3005); "
                      "fase.registrar('resto',110); fase.paralelismo()",
                      {"GLINTFX_FASES_DIR": lateral, "GLINTFX_FASES_TESTE": "demo", "CMAKE_BUILD_PARALLEL_LEVEL": "4"})
        if r.stdout.encode() != esperado:
            erros.append(f"a saida padrao nao bate byte a byte com esperado.txt: {r.stdout!r} {r.stderr!r}")
        try:
            with open(os.path.join(lateral, "demo.txt"), "rb") as h:
                if h.read() != esperado:
                    erros.append("o arquivo lateral nao bate byte a byte com esperado.txt")
        except OSError as exc:
            erros.append(f"arquivo lateral ausente: {exc}")
        # 1b. o filtro de `#` vale SO' para a FIXTURE: fixture com e sem linhas `#` (topo, meio, fim) da o mesmo
        #     conteudo; uma linha `#` no MEIO da saida REAL nao e' descartada (a divergencia aparece)
        linhas = esperado.splitlines(keepends=True)
        com_hash = os.path.join(tmp, "com_hash.txt")
        with open(com_hash, "wb") as h:
            h.write(b"# a\n" + b"".join(linhas[:2]) + b"# b\n" + b"".join(linhas[2:]) + b"# c\n")
        if _ler_fixture(com_hash) != esperado:
            erros.append("a fixture com linhas # deveria dar o mesmo conteudo que sem elas")
        real_intruso = b"".join(linhas[:2]) + b"# intruso\n" + b"".join(linhas[2:])
        if real_intruso == esperado:
            erros.append("uma linha # no MEIO da saida real foi descartada (o filtro so' pode valer para a fixture)")
        # 2. CMAKE_BUILD_PARALLEL_LEVEL ausente vira `ausente`
        _capturar("import fase; fase.paralelismo()", {"GLINTFX_FASES_DIR": lateral, "GLINTFX_FASES_TESTE": "ausente"})
        try:
            with open(os.path.join(lateral, "ausente.txt"), "rb") as h:
                if h.read() != esperado_ausente:
                    erros.append("sem CMAKE_BUILD_PARALLEL_LEVEL o arquivo lateral deveria trazer 'ausente'")
        except OSError as exc:
            erros.append(f"arquivo lateral 'ausente' nao criado: {exc}")
        # 3. sem GLINTFX_FASES_DIR/TESTE: so' a saida padrao, nenhum arquivo novo
        antes = sorted(os.listdir(tmp))
        r = _capturar("import fase; fase.registrar('x',5)", {"CMAKE_BUILD_PARALLEL_LEVEL": "2"})
        if r.stdout != "FASE x: 0.05 s\n":
            erros.append(f"saida padrao sem lateral: {r.stdout!r}")
        if sorted(os.listdir(tmp)) != antes:
            erros.append("sem as variaveis, algo foi gravado")
    # 4. o relogio: ~0,25 s medido fica entre 0,20 s e 2,00 s
    r = _capturar("import fase, time; fase.inicio('r'); time.sleep(0.25); fase.fim('r')", {})
    import re
    m = re.fullmatch(r"FASE r: (\d+)\.(\d{2}) s\n", r.stdout)
    if not m:
        erros.append(f"saida do relogio fora do formato: {r.stdout!r} {r.stderr!r}")
    elif not 20 <= int(m.group(1)) * 100 + int(m.group(2)) <= 200:
        erros.append(f"0,25 s medido como {r.stdout!r} (fora de 0,20 a 2,00 s)")
    # 5. fim sem inicio reprova
    r = _capturar("import fase; fase.fim('inexistente')", {})
    if r.returncode == 0:
        erros.append("fase.fim sem fase.inicio deveria falhar")
    for e in erros:
        print(f"fase.py --selftest: FALHOU - {e}", file=sys.stderr)
    if erros:
        return 1
    print("fase.py --selftest: OK - formato byte a byte (saida e lateral), ausente, sem lateral, relogio, fim sem inicio")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        sys.exit(_selftest())
    print("Uso: import fase | fase.py --selftest", file=sys.stderr)
    sys.exit(2)
