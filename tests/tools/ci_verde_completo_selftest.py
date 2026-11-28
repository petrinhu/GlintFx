#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# ci_verde_completo_selftest.py - autoteste de tools/ci/ci_verde_completo.py (EFIC-CI-SELETIVO, E3,
# D-W8-1231; GODS_LAWS.md L-40 e L-36 globais). Controles em memoria: nenhum GitHub, nenhum gh, nenhum
# git. Cada caso fixa o veredito esperado para um conjunto de trabalhos e um modo.
#
# Usage:
#   ci_verde_completo_selftest.py --selftest
#   ci_verde_completo_selftest.py --ci-yml <caminho/ci.yml>
#
# Regra que os controles travam: num run do modo `completo`, todo trabalho sai `success`, exceto o
# trabalho de etiqueta (nome de exibicao exato `NOME_REAL_TRABALHO_SO_EM_TAG`, igual ao `name:` do
# `ci.yml`), que pode sair `skipped`. Um trabalho pulado com qualquer outro nome reprova; zero
# trabalhos reprova (L-40 do projeto: piso de varredura nao-vazia).
#
# O nome literal NAO vem do instrumento: a fixture o escreve aqui, copiado do `name:` do job no
# `ci.yml` e de um run real (S6, rodada 46001049918). Se o instrumento e' trocado por um nome
# errado, o controle vermelho pega. Por isso `--ci-yml` confere, no arquivo real, que o nome do
# instrumento existe como `name:` do job `version-tag`: renomear o job reprova aqui, sem esperar o
# servidor.

import importlib.util
import os
import sys

SCRIPT_NAME = "ci_verde_completo_selftest.py"
ALVO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "ci", "ci_verde_completo.py")
NOME_REAL_TRABALHO_SO_EM_TAG = "Versao == etiqueta (VERSION-TAG-SYNC)"
CHAVE_JOB_SO_EM_TAG = "version-tag"
CHAVE_JOB_DISPARO = "fedora-lock"  # chave do job de disparo manual (CI-PIN-PKG-B) no ci.yml


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
        _job(NOME_REAL_TRABALHO_SO_EM_TAG, "skipped"),
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
    tag_rodou = _verde_completo()
    tag_rodou[-1] = _job(NOME_REAL_TRABALHO_SO_EM_TAG, "success")
    tag_falhou = _verde_completo()
    tag_falhou[-1] = _job(NOME_REAL_TRABALHO_SO_EM_TAG, "failure")
    so_tag = [_job(NOME_REAL_TRABALHO_SO_EM_TAG, "skipped")]
    # nome de exibicao que NAO e' o esperado: nunca e' excecao, mesmo com o marcador no texto
    chave_como_nome = _verde_completo()
    chave_como_nome[-1] = _job(CHAVE_JOB_SO_EM_TAG, "skipped")
    sem_sufixo = _verde_completo()
    sem_sufixo[-1] = _job("Versao == etiqueta", "skipped")
    outra_caixa = _verde_completo()
    outra_caixa[-1] = _job("versao == etiqueta (version-tag-sync)", "skipped")
    espaco_no_fim = _verde_completo()
    espaco_no_fim[-1] = _job(NOME_REAL_TRABALHO_SO_EM_TAG + " ", "skipped")
    outro_com_marcador = _verde_completo()
    outro_com_marcador[1] = _job("Build (VERSION-TAG-SYNC)", "skipped")
    return [
        # positivo: tudo verde, modo completo, trabalho de etiqueta pulado com o nome exato e' o esperado
        ("positivo_completo_verde", verde, "completo", 0, "trabalhos: 7 de 7; pulados: 1; modo: completo"),
        # negativo: um trabalho pulado que nao e' o de etiqueta reprova
        ("negativo_pulado_nao_tag", com_pulado, "completo", 1, "pulados"),
        # vazio: zero trabalhos reprova (L-40 do projeto)
        ("vazio_zero_trabalhos", [], "completo", 1, "zero trabalhos"),
        # so o trabalho de etiqueta (zero trabalhos de CI de fato) tambem reprova: nao e' verde de CI
        ("so_version_tag_reprova", so_tag, "completo", 1, "zero trabalhos"),
        # cancelado reprova
        ("cancelado_reprova", com_cancelado, "completo", 1, "cancelled"),
        # falha reprova
        ("falha_reprova", com_falha, "completo", 1, "failure"),
        # conclusao vazia (run ainda em curso, ou sem dado) reprova
        ("conclusao_vazia_reprova", sem_conclusao, "completo", 1, "sem conclusao"),
        # trabalho de etiqueta que rodou e passou tambem e' aceito
        ("version_tag_success_aceito", tag_rodou, "completo", 0, "modo: completo"),
        # trabalho de etiqueta que falhou reprova
        ("version_tag_failure_reprova", tag_falhou, "completo", 1, "failure"),
        # modo subconjunto nunca e' verde para este instrumento
        ("subconjunto_reprova", verde, "subconjunto", 1, "modo"),
        # modo desconhecido reprova
        ("modo_desconhecido_reprova", verde, "texto-sem-leitor", 1, "modo"),
        # a CHAVE do job (version-tag) nao e' o nome de exibicao: pulada, reprova
        ("chave_como_nome_reprova", chave_como_nome, "completo", 1, "pulado (skipped)"),
        # quase-acertos do nome exato: cada um reprova (sem substring, sem caixa frouxa, sem trim)
        ("quase_sem_sufixo_reprova", sem_sufixo, "completo", 1, "pulado (skipped)"),
        ("quase_outra_caixa_reprova", outra_caixa, "completo", 1, "pulado (skipped)"),
        ("quase_espaco_no_fim_reprova", espaco_no_fim, "completo", 1, "pulado (skipped)"),
        # outro nome com o marcador no texto nao e' excecao
        ("outro_nome_com_marcador_reprova", outro_com_marcador, "completo", 1, "pulado (skipped)"),
    ]


def _conferir_nome_no_yml(texto, nome, chave=CHAVE_JOB_SO_EM_TAG):
    """Devolve a lista de problemas (vazia = ok). Funcao pura sobre o texto do ci.yml.

    Regras: a chave `  <chave>:` existe no bloco de jobs; dentro do bloco dela, a linha
    `    name:` tem o valor `nome` exato; e a linha `name: <nome>` aparece exatamente uma vez no
    arquivo inteiro (comentario com o nome nao conta; duplicata reprova).
    """
    problemas = []
    linhas = texto.splitlines()
    inicio = None
    for i, linha in enumerate(linhas):
        if linha.rstrip() == f"  {chave}:":
            inicio = i
            break
    if inicio is None:
        return [f"chave do job {chave!r} ausente (linha '  {chave}:')"]
    nomes_no_bloco = []
    for linha in linhas[inicio + 1:]:
        if linha.strip() == "" or linha.lstrip().startswith("#"):
            continue
        indentacao = len(linha) - len(linha.lstrip(" "))
        if indentacao <= 2:
            break  # proxima chave irma do job (ou fim do bloco de jobs)
        if indentacao == 4 and linha.startswith("    name:"):
            nomes_no_bloco.append(linha[len("    name:"):].strip())
    if len(nomes_no_bloco) != 1:
        problemas.append(
            f"bloco do job {chave!r} tem {len(nomes_no_bloco)} linhas 'name:' (esperado 1)"
        )
    elif nomes_no_bloco[0] != nome:
        problemas.append(
            f"name do job {chave!r} e' {nomes_no_bloco[0]!r}, instrumento espera {nome!r}"
        )
    exatos = sum(1 for linha in linhas if linha.strip() == f"name: {nome}")
    if exatos != 1:
        problemas.append(f"a linha 'name: {nome}' aparece {exatos} vezes no arquivo (esperado 1)")
    return problemas


def _casos_ci_yml():
    """(nome, texto, nome_esperado_pelo_instrumento, tem_que_reprovar)."""
    base = (
        "name: ci\n"
        "jobs:\n"
        "  linux:\n"
        "    name: Linux\n"
        "    runs-on: ubuntu-24.04\n"
        "  version-tag:\n"
        f"    name: {NOME_REAL_TRABALHO_SO_EM_TAG}\n"
        "    runs-on: ubuntu-24.04\n"
    )
    renomeado = base.replace(f"    name: {NOME_REAL_TRABALHO_SO_EM_TAG}", "    name: Versao == tag")
    so_comentario = base.replace(
        f"    name: {NOME_REAL_TRABALHO_SO_EM_TAG}",
        f"    # name: {NOME_REAL_TRABALHO_SO_EM_TAG}\n    name: Outro",
    )
    duplicado = base + (
        "  outro:\n"
        f"    name: {NOME_REAL_TRABALHO_SO_EM_TAG}\n"
        "    runs-on: ubuntu-24.04\n"
    )
    sem_chave = base.replace("  version-tag:", "  outro-job:")
    return [
        ("ci_yml_nome_real_passa", base, NOME_REAL_TRABALHO_SO_EM_TAG, False),
        ("ci_yml_renomeado_reprova", renomeado, NOME_REAL_TRABALHO_SO_EM_TAG, True),
        ("ci_yml_so_em_comentario_reprova", so_comentario, NOME_REAL_TRABALHO_SO_EM_TAG, True),
        ("ci_yml_duplicado_reprova", duplicado, NOME_REAL_TRABALHO_SO_EM_TAG, True),
        ("ci_yml_sem_chave_reprova", sem_chave, NOME_REAL_TRABALHO_SO_EM_TAG, True),
        ("ci_yml_vazio_reprova", "", NOME_REAL_TRABALHO_SO_EM_TAG, True),
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
    for nome, texto, esperado, deve_reprovar in _casos_ci_yml():
        problemas = _conferir_nome_no_yml(texto, esperado)
        if bool(problemas) != deve_reprovar:
            falhas.append(f"{nome}: problemas={problemas!r}, esperado reprovar={deve_reprovar}")
    # CI-PIN-PKG-B: o job de disparo manual pode sair skipped (excecao declarada); falhar reprova.
    base = _verde_completo()[:-1]
    rc_pulado, _ = alvo.veredito(base + [_job(alvo.TRABALHO_SO_EM_DISPARO, "skipped")], "completo")
    if rc_pulado != 0:
        falhas.append(f"disparo_pulado_passa: rc={rc_pulado}, esperado 0")
    rc_falhou, _ = alvo.veredito(base + [_job(alvo.TRABALHO_SO_EM_DISPARO, "failure")], "completo")
    if rc_falhou != 1:
        falhas.append(f"disparo_falhou_reprova: rc={rc_falhou}, esperado 1")
    total = len(casos) + len(_casos_ci_yml()) + 2
    if falhas:
        for f in falhas:
            print(f"{SCRIPT_NAME}: FALHOU: {f}", file=sys.stderr)
        print(f"{SCRIPT_NAME} --selftest: FALHOU ({len(falhas)} de {total})", file=sys.stderr)
        return 1
    print(f"{SCRIPT_NAME} --selftest: os {total} controles OK")
    return 0


def ci_yml_main(caminho):
    alvo = _carregar_alvo()
    try:
        with open(caminho, encoding="utf-8") as handle:
            texto = handle.read()
    except OSError as exc:
        print(f"{SCRIPT_NAME}: GUARDA - nao leu {caminho}: {exc}", file=sys.stderr)
        return 2
    for nome, chave in ((alvo.TRABALHO_SO_EM_TAG, CHAVE_JOB_SO_EM_TAG), (alvo.TRABALHO_SO_EM_DISPARO, CHAVE_JOB_DISPARO)):
        problemas = _conferir_nome_no_yml(texto, nome, chave)
        if problemas:
            for p in problemas:
                print(f"{SCRIPT_NAME} --ci-yml: REPROVADO: {p}", file=sys.stderr)
            return 1
        print(f"{SCRIPT_NAME} --ci-yml: nome {nome!r} existe como name de job no ci.yml")
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        sys.exit(selftest_main())
    if len(sys.argv) == 3 and sys.argv[1] == "--ci-yml":
        sys.exit(ci_yml_main(sys.argv[2]))
    print(f"usage: {SCRIPT_NAME} --selftest | --ci-yml <caminho/ci.yml>", file=sys.stderr)
    sys.exit(2)
