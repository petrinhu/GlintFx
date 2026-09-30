#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# extract_oracle_report.py - TODO.md item LAYERS-ORACLE-REPORT, sub-
# fatia J1 (docs/plano-w7c-adendo-revalidacao.md secao 3.J, achado N8,
# decisao D-A11, 24/09/2026).
#
# O FURO QUE ESTE SCRIPT FECHA: os tres passos "Relatorio do oraculo de
# camadas" de .github/workflows/ci.yml (Linux :644, Windows PowerShell
# :3247, Clang :3401) extraiam o relatorio do oraculo de camadas
# (check_layers_oracle.py) filtrando o LastTest.log INTEIRO por
# `grep '^check_layers_oracle.py:'`, e reprovavam so' quando essa busca
# devolvia zero linha (GODS_LAWS.md L-40). O problema: `layers_oracle_
# selftest` - o AUTOTESTE do proprio oraculo, que roda em todo sistema,
# opcao GLINTFX_LAYERS_ORACLE ligada ou nao - TAMBEM imprime linhas com
# o MESMO prefixo `check_layers_oracle.py:` (varias funcoes internas do
# oraculo, como a que imprime o censo de calibracao, usam o mesmo
# `f"{SCRIPT_NAME}: ..."` tanto no caminho real quanto durante o
# --selftest). Se o `layers_oracle_test` - o caso REAL, que roda o
# compilador de verdade - parar de rodar por qualquer motivo
# (SKIP_RETURN_CODE, opcao desligada por engano, nome trocado), o
# `layers_oracle_selftest` sozinho ja basta para o `grep` encontrar
# linhas: o passo que EXISTE para acusar essa ausencia passa VERDE
# FALSO com o texto do autoteste.
#
# O CONSERTO: extrair nao por PREFIXO DE LINHA (que qualquer teste pode
# imitar por acaso), mas por BLOCO DE TESTE do proprio ctest. O
# `LastTest.log` marca cada teste com uma linha "N/M Testing: <nome>"
# antes do corpo dele (formato do proprio ctest, nao deste projeto -
# ver qualquer Testing/Temporary/LastTest.log real) - este script
# separa o log nesses blocos e devolve so' as linhas do bloco cujo nome
# e' EXATAMENTE "layers_oracle_test" (nunca "layers_oracle_selftest",
# mesmo que ele exista no mesmo arquivo, com o mesmo prefixo de linha).
#
# O QUE ELE NAO VE: se o formato do LastTest.log do ctest mudar (a
# linha "N/M Testing: <nome>" e' versionada pelo proprio ctest, fora do
# controle deste projeto), a extracao por bloco para de achar QUALQUER
# bloco e reprova por ausencia - nunca volta, silenciosamente, a
# misturar autoteste com caso real. Este script tambem nao confere se o
# CONTEUDO das linhas extraidas faz sentido (quem prova isso e'
# check_layers_oracle.py, via o --selftest dele); ele so' prova que as
# linhas publicadas vieram do bloco certo do ctest.

import re
import sys
import tempfile
from pathlib import Path

SCRIPT_NAME = "extract_oracle_report.py"
REPORT_LINE_PREFIX = "check_layers_oracle.py:"
DEFAULT_TEST_NAME = "layers_oracle_test"

# ctest grava cada bloco de teste comecando por uma linha "N/M Testing:
# <nome>" - MULTILINE para casar no inicio de QUALQUER linha do texto,
# nao so' da primeira.
_BLOCK_HEADER_RE = re.compile(r"^\d+/\d+ Testing: (\S+)$", re.MULTILINE)


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def split_into_test_blocks(text):
    """Devolve {nome_do_teste: conteudo_do_bloco}, do cabecalho 'N/M
    Testing: <nome>' ate o proximo cabecalho ou o fim do arquivo. Se o
    mesmo nome aparecer mais de uma vez no log (nao deveria acontecer
    numa rodada normal, mas o formato e' do ctest, nao deste script), o
    ULTIMO bloco vence."""
    headers = list(_BLOCK_HEADER_RE.finditer(text))
    blocks = {}
    for index, header in enumerate(headers):
        name = header.group(1)
        start = header.start()
        end = headers[index + 1].start() if index + 1 < len(headers) else len(text)
        blocks[name] = text[start:end]
    return blocks


def extract_report_lines(block_text):
    """As linhas `check_layers_oracle.py:` de DENTRO de um bloco so' -
    nunca do arquivo inteiro (isso e' exatamente o furo descrito no
    cabecalho deste arquivo)."""
    return [line for line in block_text.splitlines() if line.startswith(REPORT_LINE_PREFIX)]


def naive_whole_file_grep(text):
    """Reproducao FIEL do `grep '^check_layers_oracle.py:' "$log"` que
    .github/workflows/ci.yml usava antes desta fatia. Existe SO' para o
    controle de estreia medir e registrar o furo (ver
    selftest_furo_reproduzido_e_documentado abaixo) - o caminho real
    (real_main) nunca chama esta funcao."""
    return [line for line in text.splitlines() if line.startswith(REPORT_LINE_PREFIX)]


def real_main(args):
    usage = f"usage: {SCRIPT_NAME} --out <arquivo-saida> <LastTest.log> [--test-name <nome>]  |  --selftest"
    if not args or args[0] != "--out":
        fail(usage)
    if len(args) < 3:
        fail(usage)
    out_path = Path(args[1])
    log_path = Path(args[2])
    rest = args[3:]
    test_name = DEFAULT_TEST_NAME
    if rest:
        if len(rest) != 2 or rest[0] != "--test-name":
            fail(usage)
        test_name = rest[1]

    if not log_path.is_file():
        fail(f"log nao encontrado: {log_path}")
    text = log_path.read_text(encoding="utf-8", errors="replace")

    blocks = split_into_test_blocks(text)
    if test_name not in blocks:
        fail(
            f"bloco do teste {test_name!r} nao encontrado em {log_path} (testes vistos no log: "
            f"{sorted(blocks)!r}) - o oraculo nao rodou, a opcao esta desligada, ou o nome do "
            f"teste mudou (GODS_LAWS.md L-40)"
        )

    lines = extract_report_lines(blocks[test_name])
    if not lines:
        fail(
            f"bloco de {test_name!r} encontrado, mas nenhuma linha {REPORT_LINE_PREFIX!r} dentro "
            f"dele - varredura vazia (GODS_LAWS.md L-40)"
        )

    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{SCRIPT_NAME}: {len(lines)} linha(s) extraida(s) do bloco de {test_name!r}, gravadas em {out_path}")


# -- selftest --------------------------------------------------------

def _write_temp(content):
    with tempfile.NamedTemporaryFile(mode="w", suffix=".log", delete=False, encoding="utf-8") as handle:
        handle.write(content)
    return Path(handle.name)


# Corpo de bloco layers_oracle_selftest, com as linhas que o autoteste
# de verdade imprime (mesmo prefixo `check_layers_oracle.py:` do
# oraculo REAL - ver _print_calibration_report e selftest_main em
# check_layers_oracle.py) - NENHUM bloco layers_oracle_test presente,
# como aconteceria se o caso real nunca rodasse.
_FAKE_SELFTEST_ONLY_LOG = (
    "1/1 Testing: layers_oracle_selftest\n"
    "1/1 Test: layers_oracle_selftest\n"
    "Command: \"/build/tests/tools_selftest_runner\"\n"
    "Output:\n"
    "----------------------------------------------------------\n"
    "check_layers_oracle.py: calibracao: censo=42 (piso 30)\n"
    "check_layers_oracle.py: sentinela shadow-sombra: OK\n"
    "check_layers_oracle.py --selftest: 29 controles OK (140 casos no total)\n"
    "<end of output>\n"
    "Test time =   1.20 sec\n"
    "----------------------------------------------------------\n"
    "Test Passed.\n"
    "End testing: Sep 24 00:00 -03\n"
)


def selftest_furo_reproduzido_e_documentado():
    """RED do adendo (secao 3.J, tabela J1): um log que so' tem o bloco
    de layers_oracle_selftest (o layers_oracle_test REAL nunca rodou)
    ainda assim tem linhas 'check_layers_oracle.py:' dentro dele - o
    grep antigo (naive_whole_file_grep) devolve essas linhas, o verde
    falso medido e registrado aqui. O extrator por bloco tem de
    reprovar o mesmo arquivo, por ausencia do bloco 'layers_oracle_
    test'."""
    old_grep_lines = naive_whole_file_grep(_FAKE_SELFTEST_ONLY_LOG)
    if not old_grep_lines:
        print(
            "selftest: FURO-REPRODUZIDO FALHOU (a fixture parou de reproduzir o furo - o grep "
            "antigo devolveu 0 linhas, esperava mais de zero para provar o verde falso historico)",
            file=sys.stderr,
        )
        return False
    print(
        f"selftest: FURO-REPRODUZIDO medido - grep antigo (naive_whole_file_grep) sobre log "
        f"so'-com-selftest devolveu {len(old_grep_lines)} linha(s): {old_grep_lines!r} "
        f"(verde falso historico documentado)"
    )

    blocks = split_into_test_blocks(_FAKE_SELFTEST_ONLY_LOG)
    if DEFAULT_TEST_NAME in blocks:
        print(
            f"selftest: FURO-REPRODUZIDO FALHOU (a fixture ganhou um bloco {DEFAULT_TEST_NAME!r} "
            f"sem querer - o teste deixaria de provar o que promete)",
            file=sys.stderr,
        )
        return False
    print(
        f"selftest: FURO-REPRODUZIDO OK (extrator por bloco nao acha {DEFAULT_TEST_NAME!r} no "
        f"mesmo log onde o grep antigo passava calado)"
    )
    return True


# Log com os DOIS blocos - layers_oracle_test (o caso real) e
# layers_oracle_selftest logo depois, com prefixo identico - prova que
# so' o bloco certo sai.
_FAKE_REAL_ORACLE_LOG = (
    "1/2 Testing: layers_oracle_test\n"
    "1/2 Test: layers_oracle_test\n"
    "Command: \"/build/tests/tools/check_layers_oracle.py\"\n"
    "Output:\n"
    "----------------------------------------------------------\n"
    "check_layers_oracle.py: comando: compilador='g++' dialeto=23 flags=[]\n"
    "check_layers_oracle.py: linhas fora da arvore (total do trabalho): 0\n"
    "check_layers_oracle.py: oraculo de camadas OK\n"
    "<end of output>\n"
    "Test time =  12.34 sec\n"
    "----------------------------------------------------------\n"
    "Test Passed.\n"
    "\n"
    "2/2 Testing: layers_oracle_selftest\n"
    "2/2 Test: layers_oracle_selftest\n"
    "Command: \"/build/tests/tools_selftest_runner\"\n"
    "Output:\n"
    "----------------------------------------------------------\n"
    "check_layers_oracle.py: calibracao: censo=42 (piso 30)\n"
    "check_layers_oracle.py --selftest: 29 controles OK (140 casos no total)\n"
    "<end of output>\n"
    "Test time =   1.20 sec\n"
    "----------------------------------------------------------\n"
    "Test Passed.\n"
    "End testing: Sep 24 00:00 -03\n"
)


def selftest_positive_control_ignores_selftest_block():
    """GREEN: com os dois blocos presentes, so' as tres linhas do bloco
    'layers_oracle_test' saem - a linha de 'layers_oracle_selftest'
    (mesmo prefixo) fica de fora."""
    blocks = split_into_test_blocks(_FAKE_REAL_ORACLE_LOG)
    if DEFAULT_TEST_NAME not in blocks:
        print("selftest: POSITIVO FALHOU (bloco layers_oracle_test nao achado)", file=sys.stderr)
        return False
    lines = extract_report_lines(blocks[DEFAULT_TEST_NAME])
    expected = [
        "check_layers_oracle.py: comando: compilador='g++' dialeto=23 flags=[]",
        "check_layers_oracle.py: linhas fora da arvore (total do trabalho): 0",
        "check_layers_oracle.py: oraculo de camadas OK",
    ]
    if lines != expected:
        print(f"selftest: POSITIVO FALHOU (linhas erradas: {lines!r}, esperava {expected!r})", file=sys.stderr)
        return False
    print(f"selftest: POSITIVO OK ({len(lines)} linha(s) do bloco real, bloco do autoteste ignorado)")
    return True


_FAKE_EMPTY_ORACLE_LOG = (
    "1/1 Testing: layers_oracle_test\n"
    "1/1 Test: layers_oracle_test\n"
    "Output:\n"
    "----------------------------------------------------------\n"
    "<sem saida - a opcao GLINTFX_LAYERS_ORACLE estava desligada>\n"
    "<end of output>\n"
    "Test Passed.\n"
    "End testing: Sep 24 00:00 -03\n"
)


def selftest_empty_block_reproves():
    """Bloco 'layers_oracle_test' presente, mas SEM nenhuma linha
    check_layers_oracle.py: dentro dele - piso de varredura vazia
    (GODS_LAWS.md L-40), tem de reprovar mesmo tendo achado o bloco
    certo."""
    blocks = split_into_test_blocks(_FAKE_EMPTY_ORACLE_LOG)
    if DEFAULT_TEST_NAME not in blocks:
        print("selftest: BLOCO-VAZIO FALHOU (setup errado: bloco nao achado)", file=sys.stderr)
        return False
    lines = extract_report_lines(blocks[DEFAULT_TEST_NAME])
    if lines:
        print(f"selftest: BLOCO-VAZIO FALHOU (achou linha(s) onde nao deveria: {lines!r})", file=sys.stderr)
        return False
    print("selftest: BLOCO-VAZIO OK (bloco existe, zero linhas de relatorio dentro dele)")
    return True


def selftest_real_main_reproves_missing_log_file():
    """Ponta a ponta (litmus 'if False' aplicado a TODO piso de
    real_main(), 24/09/2026): nenhum controle antes deste passava um
    caminho de log INEXISTENTE - `if not log_path.is_file(): fail(...)`
    desligado nunca mudava nenhum resultado, porque nenhum controle o
    exercitava (piso nao-testado, nao so' contornavel). real_main() com
    um caminho de log que nunca existiu tem de sair 1."""
    out = _write_temp("")
    missing_log = Path(tempfile.gettempdir()) / "glintfx-extract-oracle-report-nao-existe.log"
    try:
        try:
            real_main(["--out", str(out), str(missing_log)])
        except SystemExit as exc:
            if exc.code != 1:
                print(f"selftest: REAL-MAIN-LOG-AUSENTE FALHOU (codigo {exc.code}, esperava 1)",
                      file=sys.stderr)
                return False
            print("selftest: REAL-MAIN-LOG-AUSENTE OK (log inexistente reprova, codigo 1)")
            return True
        print("selftest: REAL-MAIN-LOG-AUSENTE FALHOU (nao reprovou - esperava exit 1)", file=sys.stderr)
        return False
    finally:
        out.unlink(missing_ok=True)


def selftest_real_main_reproves_empty_block():
    """Ponta a ponta (achado da revisao main, mutante m2 sobrevivente em
    24/09/2026): selftest_empty_block_reproves acima so' confere
    split_into_test_blocks()/extract_report_lines() PUROS - nunca passa
    pelo `if not lines: fail(...)` de real_main(). Um mutante que troca
    esse `if` por `if False:` (piso "bloco achado, mas vazio" desligado)
    sobrevivia com "os 5 controles OK", porque nenhum controle chamava
    real_main() sobre o log so'-com-bloco-vazio. Este controle fecha o
    buraco: real_main() sobre _FAKE_EMPTY_ORACLE_LOG tem de sair 1."""
    log = _write_temp(_FAKE_EMPTY_ORACLE_LOG)
    out = _write_temp("")
    try:
        try:
            real_main(["--out", str(out), str(log)])
        except SystemExit as exc:
            if exc.code != 1:
                print(f"selftest: REAL-MAIN-BLOCO-VAZIO FALHOU (codigo {exc.code}, esperava 1)",
                      file=sys.stderr)
                return False
            print("selftest: REAL-MAIN-BLOCO-VAZIO OK (real_main reprova bloco achado e vazio, codigo 1)")
            return True
        print("selftest: REAL-MAIN-BLOCO-VAZIO FALHOU (nao reprovou - esperava exit 1)", file=sys.stderr)
        return False
    finally:
        log.unlink(missing_ok=True)
        out.unlink(missing_ok=True)


def selftest_real_main_reproves_selftest_only_log():
    """Ponta a ponta: real_main() sobre o log so'-com-selftest tem de
    sair com codigo 1."""
    log = _write_temp(_FAKE_SELFTEST_ONLY_LOG)
    out = _write_temp("")
    try:
        try:
            real_main(["--out", str(out), str(log)])
        except SystemExit as exc:
            if exc.code != 1:
                print(f"selftest: REAL-MAIN-FURO FALHOU (codigo {exc.code}, esperava 1)", file=sys.stderr)
                return False
            print("selftest: REAL-MAIN-FURO OK (real_main reprova o log so'-com-selftest, codigo 1)")
            return True
        print("selftest: REAL-MAIN-FURO FALHOU (real_main nao reprovou - esperava exit 1)", file=sys.stderr)
        return False
    finally:
        log.unlink(missing_ok=True)
        out.unlink(missing_ok=True)


def selftest_real_main_succeeds_on_real_block():
    """Ponta a ponta: real_main() sobre o log com os dois blocos grava
    so' as tres linhas do bloco real no arquivo de saida."""
    log = _write_temp(_FAKE_REAL_ORACLE_LOG)
    out = _write_temp("")
    try:
        try:
            real_main(["--out", str(out), str(log)])
        except SystemExit as exc:
            print(f"selftest: REAL-MAIN-POSITIVO FALHOU (reprovou inesperadamente, codigo {exc.code})",
                  file=sys.stderr)
            return False
        written = out.read_text(encoding="utf-8").splitlines()
        if len(written) != 3:
            print(
                f"selftest: REAL-MAIN-POSITIVO FALHOU (esperava 3 linhas gravadas, achei "
                f"{len(written)}: {written!r})",
                file=sys.stderr,
            )
            return False
        print(f"selftest: REAL-MAIN-POSITIVO OK ({len(written)} linha(s) gravadas em {out})")
        return True
    finally:
        log.unlink(missing_ok=True)
        out.unlink(missing_ok=True)


def selftest_main():
    controls = [
        selftest_furo_reproduzido_e_documentado(),
        selftest_positive_control_ignores_selftest_block(),
        selftest_empty_block_reproves(),
        selftest_real_main_reproves_missing_log_file(),
        selftest_real_main_reproves_empty_block(),
        selftest_real_main_reproves_selftest_only_log(),
        selftest_real_main_succeeds_on_real_block(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
