#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# ci_verde_completo_selftest.py - autoteste de tools/ci/ci_verde_completo.py (EFIC-CI-SELETIVO, E3,
# D-W8-1231; GODS_LAWS.md L-40 e L-36 globais). Controles em memoria: nenhum GitHub, nenhum gh, nenhum
# git. Cada caso fixa o veredito esperado para um conjunto de trabalhos e um modo.
#
# Usage:
#   ci_verde_completo_selftest.py --selftest
#
# Regra que os controles travam: num run do modo `completo`, todo trabalho sai `success`, exceto o
# `version-tag`, que pode sair `skipped`. Um trabalho pulado que nao e' o `version-tag` reprova; zero
# trabalhos reprova (L-40 do projeto: piso de varredura nao-vazia).

import importlib.util
import os
import sys

SCRIPT_NAME = "ci_verde_completo_selftest.py"
ALVO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ci", "ci_verde_completo.py")


def _carregar_alvo():
    caminho = os.path.normpath(ALVO)
    spec = importlib.util.spec_from_file_location("ci_verde_completo", caminho)
    if spec is None or spec.loader is None:
        print(f"{SCRIPT_NAME}: nao carregou {caminho}", file=sys.stderr)
        sys.exit(1)
    modulo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modulo)
    return modulo


def _job(nome, conclusao):
    return {"name": nome, "conclusion": conclusao}


def _verde_completo():
    return [
        _job("mudancas", "success"),
        _job("linux", "success"),
        _job("windows", "success"),
        _job("parity", "success"),
        _job("gitleaks", "success"),
        _job("leis", "success"),
        _job("version-tag", "skipped"),
    ]


def _casos():
    """(nome, trabalhos, modo, rc_esperado, trecho_que_deve_aparecer_na_linha)."""
    verde = _verde_completo()
    com_pulado = _verde_completo()
    com_pulado[1] = _job("linux", "skipped")
    com_cancelado = _verde_completo()
    com_cancelado[2] = _job("windows", "cancelled")
    com_falha = _verde_completo()
    com_falha[3] = _job("parity", "failure")
    sem_conclusao = _verde_completo()
    sem_conclusao[4] = _job("gitleaks", "")
    version_tag_rodou = _verde_completo()
    version_tag_rodou[-1] = _job("version-tag", "success")
    version_tag_falhou = _verde_completo()
    version_tag_falhou[-1] = _job("version-tag", "failure")
    so_tag = [_job("version-tag", "skipped")]
    return [
        # positivo: tudo verde, modo completo, version-tag pulado e' o esperado
        ("positivo_completo_verde", verde, "completo", 0, "trabalhos: 7 de 7; pulados: 1; modo: completo"),
        # negativo: um trabalho pulado que nao e' o version-tag reprova
        ("negativo_pulado_nao_tag", com_pulado, "completo", 1, "pulados"),
        # vazio: zero trabalhos reprova (L-40 do projeto)
        ("vazio_zero_trabalhos", [], "completo", 1, "zero trabalhos"),
        # so o version-tag (zero trabalhos de CI de fato) tambem reprova: nao e' verde de CI
        ("so_version_tag_reprova", so_tag, "completo", 1, "zero trabalhos"),
        # cancelado reprova
        ("cancelado_reprova", com_cancelado, "completo", 1, "cancelled"),
        # falha reprova
        ("falha_reprova", com_falha, "completo", 1, "failure"),
        # conclusao vazia (run ainda em curso, ou sem dado) reprova
        ("conclusao_vazia_reprova", sem_conclusao, "completo", 1, "sem conclusao"),
        # version-tag que rodou e passou tambem e' aceito
        ("version_tag_success_aceito", version_tag_rodou, "completo", 0, "modo: completo"),
        # version-tag que falhou reprova
        ("version_tag_failure_reprova", version_tag_falhou, "completo", 1, "failure"),
        # modo subconjunto nunca e' verde para este instrumento
        ("subconjunto_reprova", verde, "subconjunto", 1, "modo"),
        # modo desconhecido reprova
        ("modo_desconhecido_reprova", verde, "texto-sem-leitor", 1, "modo"),
    ]


def selftest_main():
    alvo = _carregar_alvo()
    falhas = []
    casos = _casos()
    for nome, trabalhos, modo, rc_esperado, trecho in casos:
        rc, linha = alvo.veredito(trabalhos, modo)
        if rc != rc_esperado:
            falhas.append(f"{nome}: rc={rc}, esperado {rc_esperado} (linha: {linha!r})")
            continue
        if trecho not in linha:
            falhas.append(f"{nome}: linha {linha!r} nao contem {trecho!r}")
    if falhas:
        for f in falhas:
            print(f"{SCRIPT_NAME}: FALHOU: {f}", file=sys.stderr)
        print(f"{SCRIPT_NAME} --selftest: FALHOU ({len(falhas)} de {len(casos)})", file=sys.stderr)
        return 1
    print(f"{SCRIPT_NAME} --selftest: os {len(casos)} controles OK")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        sys.exit(selftest_main())
    print(f"usage: {SCRIPT_NAME} --selftest", file=sys.stderr)
    sys.exit(2)
