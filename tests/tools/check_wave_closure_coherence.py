#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_wave_closure_coherence.py - TODO.md item WAVE-CLOSURE-COHERENCE.
#
# Nasce do achado do lider, 09/09/2026: a linha de fechamento da onda W5
# (`CI-VERDE-W5`, GODS_LAWS.md L-11, emenda de 09/09/2026) estava marcada
# "Concluido" enquanto QUATRO itens da propria onda ainda estavam abertos
# (RSLT-ERR-RENAME e EXPORTS-PARITY-WIN entre eles). A onda foi declarada
# fechada, ganhou marca de versao e rotulo publicados, carregando divida
# dentro. Causa: os quatro itens entraram na onda DEPOIS que o plano dela
# foi escrito - vindos de varreduras de paridade - e ninguem reconferiu o
# criterio de fechamento quando entraram. Corrigido no commit `deae7e1`;
# este portao existe para que a proxima vez que isso acontecer seja
# pegada mecanicamente, nao por um lider revendo a tabela a olho.
#
# A REGRA: para toda linha de tabela cujo ID comeca com `CI-VERDE-`
# (GODS_LAWS.md L-11, emenda de 09/09/2026 - "a partir de W5, toda onda
# carrega uma linha propria de fechamento"), se essa linha esta com
# Status = marcador de concluido, TODO item da MESMA onda (mesmo valor
# normalizado da coluna Onda) tem que estar tambem concluido. Item
# pendente, em verificacao, bloqueado, em design ou com estado que este
# portao nao reconhece, com o fecho verde, reprova - a mensagem nomeia a
# onda, a linha de fecho, e cada item fora de conformidade.
#
# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero linhas de item
# lidas, ou zero linhas `CI-VERDE-*` encontradas numa tabela que tem
# itens, e' sinal de parser quebrado ou de convencao mudada - nunca de
# "nada a examinar" - e reprova. Onda SEM linha de fechamento e' outra
# coisa: a convencao so vale a partir da W5 (ordem do lider), entao
# ondas mais antigas (W1-W4) legitimamente nao tem `CI-VERDE-*` nenhuma
# - contadas e impressas como "fora do exame", nunca tratadas como erro.
#
# GODS_LAWS.md L-45 (grep -c/`|| true`): toda contagem aqui e' feita em
# Python puro sobre o texto ja lido, nunca por `grep -c` em subprocesso -
# a armadilha do "zero devolve status 1" nem se aplica.
#
# NORMALIZACAO DO NOME DA ONDA - achado ao escrever este portao, nao
# hipotetico: o sufixo do ID de fechamento e o valor da coluna Onda NAO
# usam sempre a mesma grafia. Medido contra a tabela real: `CI-VERDE-
# W11A` fecha a onda cuja coluna Onda diz `W11a` (maiuscula no ID,
# minuscula na coluna); `CI-VERDE-W9B` fecha `W9-B` (sem hifen no ID,
# com hifen na coluna); `CI-VERDE-W6b` fecha `W6b` (identico). A unica
# normalizacao que junta as tres formas sem colidir com nenhuma outra
# onda da tabela real e' maiusculizar e remover hifen dos dois lados
# antes de comparar - `normalize_wave_label()` abaixo.
#
# Registrado em tests/CMakeLists.txt (LABELS consume roda contra o
# TODO.md real do projeto; LABELS selftest roda so' os fixtures deste
# arquivo) e em tools/preci.sh (mesma lista de gates puros em Python que
# ja roda ali, ex. check_dup_laws.py/check_readme_volatile_numbers.py -
# nenhuma delas compila nada, entao nao entra na trava de "um trabalho
# pesado por vez" de GODS_LAWS.md L-11).
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

import re
import sys
from pathlib import Path

# Mesmo motivo de check_test_parity.py/check_plan_scope_diff.py: a
# coluna Status carrega simbolos fora de Basic Latin/Latin-1 (o proprio
# marcador de concluido, "✅"), e o console do runner Windows usa uma
# code page legada sem slot pra eles.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")

SCRIPT_NAME = "check_wave_closure_coherence.py"
CLOSURE_PREFIX = "CI-VERDE-"

# Uma linha de item de verdade tem exatamente 10 colunas de tabela (12
# elementos depois do split por "|", contando a celula vazia antes do
# primeiro e depois do ultimo pipe - mesma contagem de check_plan_
# scope_diff.py/check_test_parity.py para este MESMO arquivo) e a
# PRIMEIRA (WSJF) e' um numero ou o marcador `(pontuar)` de item ainda
# nao pontuado (medido: 46 linhas reais usam esse marcador hoje). Isso
# exclui cabecalho ("WSJF"), separador (":---") e qualquer tabela que um
# dia apareca no preambulo sem essa forma exata.
_TABLE_ROW_RE = re.compile(r"^\|.*\|$")
_WSJF_ITEM_RE = re.compile(r"^\(pontuar\)$|^-?\d+(?:[.,]\d+)?$")
_EXPECTED_COLUMNS = 12
_VOCAB_BULLET_RE = re.compile(r"^-\s+\*\*(.+?)\*\*")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def split_row_columns(row_line):
    """Splits a markdown table row on unescaped `|`, respeitando `\\|`
    (a mesma forma que check_plan_scope_diff.py ja usa uma linha acima
    deste projeto - texto de item real usa `\\|` dentro de comando de
    shell citado em prosa, ex. `grep -rl "bad_alloc\\|force_alloc"` na
    linha DOCS-COUNT-VOCAB de hoje; um split ingenuo por "|" cru
    deslocaria toda coluna depois dessa celula)."""
    placeholder = "\x00"
    protected = row_line.replace("\\|", placeholder)
    columns = [cell.strip() for cell in protected.split("|")]
    return [cell.replace(placeholder, "|") for cell in columns]


def parse_status_vocabulary(todo_text):
    """Le o vocabulario REAL de estado na secao "## Vocabulario de
    Status (fechado)" do proprio TODO.md - nunca hardcoded na cabeca de
    quem escreveu este portao (GODS_LAWS.md L-17/L-40: o conjunto vem
    do arquivo, e cresce quando o arquivo cresce). Retorna a lista de
    marcadores na ordem em que aparecem, ex. ["✅ Concluído", "⏳
    Pendente", ...]."""
    lines = todo_text.splitlines()
    try:
        start = next(i for i, ln in enumerate(lines) if ln.strip() == "## Vocabulário de Status (fechado)")
    except StopIteration:
        return []
    vocabulary = []
    for line in lines[start + 1:]:
        if line.startswith("## "):
            break
        match = _VOCAB_BULLET_RE.match(line.strip())
        if match:
            vocabulary.append(match.group(1))
    return vocabulary


def find_done_marker(vocabulary):
    """O marcador de "concluido" e' o unico token do vocabulario que
    comeca com "✅" - se zero ou mais de um baterem, o portao nao pode
    decidir o que "fechado" significa e reprova em vez de adivinhar."""
    candidates = [token for token in vocabulary if token.startswith("✅")]
    if len(candidates) != 1:
        fail(
            f"vocabulario de status tem {len(candidates)} marcador(es) comecando com '✅' "
            f"(esperava exatamente 1): {candidates!r} - portao nao pode decidir o que e 'concluido'"
        )
    return candidates[0]


def parse_table_rows(todo_text):
    """Retorna a lista de itens de tabela reais - cada um um dict com
    id/onda/status - filtrando cabecalho, separador e qualquer ruido do
    preambulo pela forma da coluna WSJF (ver _WSJF_ITEM_RE acima)."""
    rows = []
    for line in todo_text.splitlines():
        stripped = line.strip()
        if not _TABLE_ROW_RE.match(stripped):
            continue
        columns = split_row_columns(stripped)
        if len(columns) != _EXPECTED_COLUMNS:
            continue
        wsjf, item_id, onda, status = columns[1], columns[2], columns[3], columns[9]
        if not _WSJF_ITEM_RE.match(wsjf):
            continue
        if not item_id:
            continue
        rows.append({"id": item_id, "onda": onda, "status": status})
    return rows


def normalize_wave_label(label):
    """Junta as tres grafias reais de nome de onda vistas na tabela
    (ver o comentario de cabecalho deste arquivo: "W11a" no ID vira
    "W11A", "W9-B" perde o hifen) sem colidir com nenhuma outra onda -
    confirmado contra as 31 ondas distintas da tabela real."""
    return label.strip().upper().replace("-", "").replace(" ", "")


def group_rows_by_wave(rows):
    groups = {}
    for row in rows:
        key = normalize_wave_label(row["onda"])
        groups.setdefault(key, []).append(row)
    return groups


def evaluate_closures(rows, vocabulary, done_marker):
    """Roda a regra central deste portao contra `rows` (ja parseadas) e
    devolve um relatorio - nunca decide sozinho o que imprimir, so'
    calcula; real_main() imprime e decide o codigo de saida."""
    groups = group_rows_by_wave(rows)
    closures = [row for row in rows if row["id"].startswith(CLOSURE_PREFIX)]

    covered_keys = set()
    examined = []       # (closure, key, members, is_done)
    orphan_closures = []  # fechamento verde sem NENHUM outro item da onda
    violations = []      # (closure, key, offenders, unknowns)

    for closure in closures:
        suffix = closure["id"][len(CLOSURE_PREFIX):]
        key = normalize_wave_label(suffix)
        covered_keys.add(key)
        members = [row for row in groups.get(key, []) if row is not closure]
        is_done = closure["status"].startswith(done_marker)
        examined.append((closure, key, members, is_done))

        if is_done and not members:
            orphan_closures.append(closure)
            continue
        if not is_done:
            continue  # a regra so' se aplica quando o fecho esta verde

        offenders, unknowns = [], []
        for member in members:
            if member["status"].startswith(done_marker):
                continue
            if any(member["status"].startswith(token) for token in vocabulary):
                offenders.append(member)
            else:
                unknowns.append(member)
        if offenders or unknowns:
            violations.append((closure, key, offenders, unknowns))

    uncovered_keys = sorted(set(groups.keys()) - covered_keys)
    uncovered_labels = [groups[key][0]["onda"] for key in uncovered_keys]

    return {
        "closures": closures,
        "examined": examined,
        "orphan_closures": orphan_closures,
        "violations": violations,
        "uncovered_labels": uncovered_labels,
    }


def format_item(row):
    return f"{row['id']} ({row['status']})"


def real_main(args):
    if len(args) != 1:
        fail("usage: check_wave_closure_coherence.py <TODO.md>  |  --selftest")
    todo_path = Path(args[0])
    todo_text = todo_path.read_text(encoding="utf-8")

    vocabulary = parse_status_vocabulary(todo_text)
    if not vocabulary:
        fail(f"secao '## Vocabulario de Status (fechado)' nao encontrada em {todo_path}")
    done_marker = find_done_marker(vocabulary)

    rows = parse_table_rows(todo_text)
    print(f"{SCRIPT_NAME}: {len(rows)} item(ns) de tabela lido(s) em {todo_path}, "
          f"{len(vocabulary)} estado(s) no vocabulario")
    if len(rows) == 0:
        fail(f"varredura vazia (GODS_LAWS.md L-40): nenhuma linha de item encontrada em {todo_path} "
             "- parser quebrado ou tabela vazia, nunca 'nada a examinar'")

    report = evaluate_closures(rows, vocabulary, done_marker)
    closures = report["closures"]
    print(f"{SCRIPT_NAME}: {len(closures)} linha(s) de fechamento '{CLOSURE_PREFIX}<onda>' encontrada(s)")
    if len(closures) == 0:
        fail(f"varredura vazia (GODS_LAWS.md L-40): {len(rows)} item(ns) na tabela mas nenhuma linha "
             f"'{CLOSURE_PREFIX}<onda>' - convencao mudou ou parser quebrou, nunca 'nenhuma onda fechada'")

    for closure, key, members, is_done in report["examined"]:
        if not is_done:
            print(f"  [--] {closure['id']}: onda ainda nao fechada (status atual: {closure['status']}), "
                  f"{len(members)} item(ns) na onda, nao examinados")
        elif not members:
            print(f"  [SEM-PAR] {closure['id']}: fechamento {done_marker} mas nenhum outro item da onda "
                  "foi encontrado - convencao de nome pode ter mudado")
        else:
            done_count = sum(1 for m in members if m["status"].startswith(done_marker))
            if done_count == len(members):
                print(f"  [OK] {closure['id']}: onda fechada, {done_count}/{len(members)} item(ns) concluido(s)")
            else:
                print(f"  [REPROVADO] {closure['id']}: onda fechada mas so' "
                      f"{done_count}/{len(members)} item(ns) concluido(s)")

    print(f"{SCRIPT_NAME}: {len(report['uncovered_labels'])} onda(s) sem linha de fechamento "
          f"('{CLOSURE_PREFIX}<onda>'), fora do exame: {report['uncovered_labels']}")

    errors = []
    for closure in report["orphan_closures"]:
        errors.append(
            f"{closure['id']}: fechamento {done_marker} mas nenhum item da onda foi encontrado na tabela"
        )
    for closure, key, offenders, unknowns in report["violations"]:
        parts = []
        if offenders:
            parts.append(
                f"{len(offenders)} item(ns) nao concluido(s): " + ", ".join(format_item(m) for m in offenders)
            )
        if unknowns:
            parts.append(
                f"{len(unknowns)} item(ns) com ESTADO DESCONHECIDO (fora do vocabulario da tabela): "
                + ", ".join(format_item(m) for m in unknowns)
            )
        errors.append(f"onda {key} (fechamento {closure['id']}, {closure['status']}): " + "; ".join(parts))

    if errors:
        fail(f"{len(errors)} onda(s) fechada(s) com item(ns) pendente(s):\n  " + "\n  ".join(errors))

    print(f"{SCRIPT_NAME}: nenhuma onda fechada com item pendente - ok")


# --- selftest -----------------------------------------------------------

_VOCAB_FIXTURE = (
    "## Vocabulário de Status (fechado)\n\n"
    "- **✅ Concluído** — finalizada.\n"
    "- **⏳ Pendente** — não iniciado.\n"
    "- **🔍 Pendente verificação** — implementado, aguarda validação.\n"
    "- **⛔ Bloqueado** — parou por causa externa.\n"
    "- **🎨 Pendente design** — aguarda decisão do líder.\n\n"
)
_TABLE_HEADER = (
    "| WSJF | ID | Onda | Grupo | Descrição Técnica | Prioridade | Pré-requisito | Dificuldade | Status | Estado Auditado |\n"
    "| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |\n"
)


def _write_fixture(tmp_path, name, table_rows):
    text = _VOCAB_FIXTURE + "## TABELA UNIFICADA\n\n" + _TABLE_HEADER + table_rows
    path = tmp_path / name
    path.write_text(text, encoding="utf-8")
    return path


def _expect_pass(path, label):
    try:
        real_main([str(path)])
    except SystemExit as exc:
        print(f"selftest: {label} reprovou inesperadamente (codigo {exc.code})", file=sys.stderr)
        return False
    print(f"selftest: {label} - ok (verde)")
    return True


def _expect_fail(path, label, must_contain):
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stderr(buffer):
        try:
            real_main([str(path)])
        except SystemExit as exc:
            if exc.code != 1:
                print(f"selftest: {label} saiu com codigo {exc.code}, esperava 1", file=sys.stderr)
                return False
            message = buffer.getvalue()
            missing = [needle for needle in must_contain if needle not in message]
            if missing:
                print(f"selftest: {label} reprovou mas a mensagem nao cita {missing}: {message!r}",
                      file=sys.stderr)
                return False
            print(f"selftest: {label} - ok (vermelho, mensagem correta)")
            return True
    print(f"selftest: {label} NAO reprovou - esperava exit 1", file=sys.stderr)
    return False


# CONTROLE POSITIVO (GODS_LAWS.md L-36): onda com fechamento verde e
# todo item concluido nao reprova.
def selftest_conforming_wave_passes(tmp_path):
    rows = (
        "| 1.00 | ITEM-A | WX | G | desc | Alta | — | Baixa | ✅ Concluído | — |\n"
        "| 2.00 | ITEM-B | WX | G | desc | Alta | — | Baixa | ✅ Concluído | — |\n"
        "| 1.00 | CI-VERDE-WX | WX | G | fecho | Alta | — | Baixa | ✅ Concluído | — |\n"
    )
    path = _write_fixture(tmp_path, "conforme.md", rows)
    return _expect_pass(path, "onda conforme (fecho verde, itens todos concluidos)")


# CONTROLE NEGATIVO (GODS_LAWS.md L-36): a exata forma do achado real
# de 09/09/2026 - fechamento verde com item aberto dentro.
def selftest_violated_wave_reproves(tmp_path):
    rows = (
        "| 1.00 | ITEM-A | WY | G | desc | Alta | — | Baixa | ✅ Concluído | — |\n"
        "| 2.00 | ITEM-B | WY | G | desc | Alta | — | Baixa | 🔍 Pendente verificação | — |\n"
        "| 1.00 | CI-VERDE-WY | WY | G | fecho | Alta | — | Baixa | ✅ Concluído | — |\n"
    )
    path = _write_fixture(tmp_path, "violada.md", rows)
    return _expect_fail(
        path, "onda violada (fecho verde, um item aberto)",
        must_contain=["WY", "CI-VERDE-WY", "ITEM-B", "🔍 Pendente verificação"],
    )


# CONTROLE DE VARREDURA VAZIA (GODS_LAWS.md L-40): tabela sem NENHUMA
# linha de item e' parser quebrado, nao "nada a examinar" - reprova.
def selftest_empty_sweep_reproves(tmp_path):
    path = tmp_path / "vazia.md"
    path.write_text(_VOCAB_FIXTURE + "## TABELA UNIFICADA\n\n" + _TABLE_HEADER, encoding="utf-8")
    return _expect_fail(path, "tabela sem nenhum item (varredura vazia)", must_contain=["varredura vazia"])


# CUIDADO 1 do briefing: celula com "\|" escapado nao pode deslocar a
# coluna Status - se deslocasse, este item (status pendente) seria lido
# como outra coluna e desapareceria da contagem, produzindo falso verde.
def selftest_escaped_pipe_does_not_shift_columns(tmp_path):
    rows = (
        '| 1.00 | ITEM-A | WZ | G | grep -rl "bad_alloc\\|force_alloc" no meio da frase | Alta | — | Baixa | 🔍 Pendente verificação | — |\n'
        "| 1.00 | CI-VERDE-WZ | WZ | G | fecho | Alta | — | Baixa | ✅ Concluído | — |\n"
    )
    path = _write_fixture(tmp_path, "pipe_escapado.md", rows)
    return _expect_fail(
        path, "celula com pipe escapado ainda e' contada certo",
        must_contain=["WZ", "ITEM-A"],
    )


# CUIDADO 3 do briefing: onda sem linha de fechamento nao e' violacao.
def selftest_wave_without_closure_line_is_not_a_violation(tmp_path):
    rows = (
        "| 1.00 | ITEM-OPEN | WA | G | desc | Alta | — | Baixa | ⏳ Pendente | — |\n"
        "| 1.00 | ITEM-B | WB | G | desc | Alta | — | Baixa | ✅ Concluído | — |\n"
        "| 1.00 | CI-VERDE-WB | WB | G | fecho | Alta | — | Baixa | ✅ Concluído | — |\n"
    )
    path = _write_fixture(tmp_path, "onda_sem_fecho.md", rows)
    return _expect_pass(path, "onda WA sem linha CI-VERDE nao reprova (so' fica fora do exame)")


# CUIDADO 4 do briefing: estado fora do vocabulario da tabela nunca e'
# tratado como "provavelmente ok" - reprova com mensagem propria.
def selftest_unknown_status_reproves(tmp_path):
    rows = (
        "| 1.00 | ITEM-A | WV | G | desc | Alta | — | Baixa | 🤷 Estado Inventado | — |\n"
        "| 1.00 | CI-VERDE-WV | WV | G | fecho | Alta | — | Baixa | ✅ Concluído | — |\n"
    )
    path = _write_fixture(tmp_path, "estado_desconhecido.md", rows)
    return _expect_fail(
        path, "estado fora do vocabulario reprova com mensagem propria",
        must_contain=["ESTADO DESCONHECIDO", "ITEM-A"],
    )


def selftest_main():
    import tempfile

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        controls = [
            selftest_conforming_wave_passes(tmp_path),
            selftest_violated_wave_reproves(tmp_path),
            selftest_empty_sweep_reproves(tmp_path),
            selftest_escaped_pipe_does_not_shift_columns(tmp_path),
            selftest_wave_without_closure_line_is_not_a_violation(tmp_path),
            selftest_unknown_status_reproves(tmp_path),
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
