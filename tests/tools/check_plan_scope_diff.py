#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_plan_scope_diff.py - PLAN-SCOPE-DIFF (achado do CTO, 06/09/2026,
# registrado em TODO.md sob "PLAN-SCOPE-DIFF"): a peneira de fechamento
# que teria pego a causa da noite inteira, não o sintoma. A fatia da
# fachada de janela (docs/plano-w6a-janela.md, linha da fatia 6, "W-D'
# headers públicos, fachada comum, concepts, `adapter()`") prometia DEZ
# caminhos como entregável e o commit que a fechou (`841e7ab`) só
# tinha CINCO - o fechamento declarou "compilação limpa, nenhum
# símbolo sem definição, N casos", tudo verdadeiro, e nada disso
# COMPARA o prometido com a árvore. Este script é essa comparação,
# feita mecanicamente em vez de por leitura.
#
# O QUE ELE FAZ: recebe o plano (um arquivo Markdown) e um MARCADOR -
# uma substring que aparece SÓ na linha da tabela da fatia em questão
# (nunca o número da linha sozinho: este mesmo documento tem outra
# tabela, a de riscos na seção 5, que também numera suas próprias
# linhas 1-8, e "a linha 6" sem mais contexto casaria com a errada -
# achado feito ao escrever este script, não hipotético). Extrai da
# COLUNA "Nasce / muda" dessa linha todo token entre crases que TEM
# CARA de caminho de arquivo (extensão reconhecida, sem parênteses,
# sem `::`, sem estar entre `<...>` como um header de sistema),
# expande a notação de chave `nome.{hpp,cpp}` em duas entradas, e
# confere cada uma contra a árvore: caminho completo (`a/b/c.hpp`)
# checa literal; nome solto (`c.hpp`, quando a prosa do plano some com
# o diretório por elipse - "e window_facade.cpp" depois de já ter dito
# o diretório da peça anterior) procura por NOME BASE em qualquer lugar
# da árvore rastreada - mais fraco que caminho completo, mas nunca
# falso-negativo por causa da elipse do português.
#
# GODS_LAWS.md L-40 (piso de varredura não-vazia): zero caminho
# EXTRAÍDO da linha é sempre sinal de o marcador não ter casado (linha
# errada, ou o marcador mudou de forma no documento) - nunca "esta
# fatia não promete nada", e reprova. Isso é diferente de "algum
# caminho promtido está ausente da árvore", que é o resultado real que
# decide se a onda fecha - impresso sempre, com a lista, nunca
# escondido atrás de um veredito único.
#
# ESCOPO DESTA FATIA (ordem explícita do time-lead, 06/09/2026): este
# script é o PASSO DO FECHAMENTO desta onda (w6a-janela), não um
# portão genérico para todo plano futuro - a generalização (aplicar a
# TODO plano, não só a uma linha citada à mão) é uma proposta do CTO
# que o líder ainda não decidiu, e expandir por conta própria aqui
# seria repetir exatamente o erro que abriu esta fatia (escopo que
# cresce sem ninguém notar).

import argparse
import re
import subprocess
import sys
import unicodedata
from pathlib import Path

# GATE-ENV-SWEEP, categoria OUTPUT_ENCODING (TODO.md, mesmo remedio de
# tests/tools/check_test_parity.py, arquivo inteiro por declaracao, nao
# janela - aquele script's own header comment explica por que): este
# script printa `status['status_text']` (real_main()/validate_absence_
# deaths()), que pode conter "✅" quando a chave morta aponta para um
# item de verdade CONCLUIDO em TODO.md - print() em modo texto estrito
# quebra com UnicodeEncodeError num console Windows de codepage
# restrita sem este reconfigure.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")

SCRIPT_NAME = "check_plan_scope_diff.py"

# Extensões que fazem um token entre crases "ter cara de caminho de
# arquivo" - deliberadamente CURTA e só de extensões reais deste
# projeto (GODS_LAWS.md stack), para não confundir `desc.logical_size`
# (não é extensão de arquivo nenhuma) com `display.hpp` (é).
FILE_EXTENSIONS = {"hpp", "cpp", "hxx", "cxx", "cc", "h", "py", "sh", "md", "txt", "yml", "in"}

BACKTICK_TOKEN = re.compile(r"`([^`]+)`")
BRACE_FORM = re.compile(r"^([\w./\-]+)\.\{([a-zA-Z,]+)\}$")
SIMPLE_FORM = re.compile(r"^([\w./\-]+)\.([a-zA-Z]+)$")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def find_row_line(plan_text, marker):
    """Returns the ONE line of `plan_text` that both starts a markdown
    table row (`| <digits><letras?> |`) and contains `marker` - never
    just "row number N", because this document has more than one table
    that numbers its own rows starting at 1 (this script's own header
    comment explains why that ambiguity is real, not hypothetical).

    CONSERTO (TODO.md, PLAN-SCOPE-REGEX-BLIND, achado 08/09/2026 pelo
    agente de fechamento da W6b): a forma antiga (`\\d+` sozinho) exigia
    identificador de fatia PURAMENTE numerico - as fatias `2a`, `5b` e
    `5c` do plano da onda W6b (docs/plano-w6b-placa-e-laco.md) NUNCA
    casavam, entao find_row_line() nunca encontrava a linha e fail()
    disparava com "nenhuma linha... encontrada" - uma mensagem que nao
    fala nada sobre o conteudo real da fatia (o caminho prometido que
    faltava). Foi assim que a fatia 5c (GFX-PRESET) ficou sem ser
    entregue e sem ninguem notar por maquina: o portao nunca chegou
    perto de checar a existencia dos caminhos, so errou cedo demais,
    com um motivo que nada tem a ver com escopo. O padrao novo aceita
    um sufixo de letras minusculas depois do digito (`2a`, `5b`, `5c`),
    a forma real usada nos planos deste projeto - e continua rejeitando
    a linha de cabecalho (`| # | Fatia | ...`, `#` nao e digito) e
    qualquer prosa que nao comece por `| <numero><letras?> |`."""
    matches = [
        line
        for line in plan_text.splitlines()
        if re.match(r"^\|\s*\d+[a-z]*\s*\|", line) and marker in line
    ]
    if len(matches) == 0:
        fail(f"nenhuma linha de tabela (\"| N | ...\") contendo o marcador {marker!r} encontrada")
    if len(matches) > 1:
        fail(
            f"{len(matches)} linhas de tabela contendo o marcador {marker!r} - marcador ambíguo, "
            "escolha uma substring que apareça em UMA só linha de tabela"
        )
    return matches[0]


def split_row_columns(row_line):
    """Splits a markdown table row on unescaped `|`, respecting `\\|`
    (the literal-pipe escape this same plan document already uses,
    e.g. fatia 1's own "...\\|windows\\|nenhum\\|SEM-PENDENCIA")."""
    placeholder = "\x00"
    protected = row_line.replace("\\|", placeholder)
    columns = [cell.strip() for cell in protected.split("|")]
    return [cell.replace(placeholder, "|") for cell in columns]


def extract_candidate_paths(column_text):
    """Returns the list of file-path-shaped tokens found in
    `column_text`'s own backtick spans - see this script's own header
    comment for the exact shape a token has to have."""
    candidates = []
    for token in BACKTICK_TOKEN.findall(column_text):
        if token.startswith("<") and token.endswith(">"):
            continue  # system header/URL form, e.g. `<windows.h>` - never ours to check.
        if "(" in token or ")" in token or "::" in token or " " in token:
            continue  # a call/method/template token, e.g. `adapter()`, `display_connection<A>::adapter()`.
        brace_match = BRACE_FORM.match(token)
        if brace_match:
            base, exts = brace_match.groups()
            for ext in exts.split(","):
                if ext in FILE_EXTENSIONS:
                    candidates.append(f"{base}.{ext}")
            continue
        simple_match = SIMPLE_FORM.match(token)
        if simple_match:
            _, ext = simple_match.groups()
            if ext in FILE_EXTENSIONS:
                candidates.append(token)
    return candidates


def path_exists_in_worktree(root, path):
    return (Path(root) / path).is_file()


def basename_exists_in_worktree(root, name):
    result = subprocess.run(
        ["git", "-C", str(root), "ls-files"], capture_output=True, text=True, check=True
    )
    return any(Path(line).name == name for line in result.stdout.splitlines())


def path_exists_in_ref(root, ref, path):
    result = subprocess.run(
        ["git", "-C", str(root), "cat-file", "-e", f"{ref}:{path}"],
        capture_output=True,
        check=False,
    )
    return result.returncode == 0


def basename_exists_in_ref(root, ref, name):
    result = subprocess.run(
        ["git", "-C", str(root), "ls-tree", "-r", "--name-only", ref],
        capture_output=True,
        text=True,
        check=True,
    )
    return any(Path(line).name == name for line in result.stdout.splitlines())


def check_existence(root, ref, candidates):
    """Returns (existing, missing) - `ref` of None means "the live
    working tree", matching this script's own --ref CLI default."""
    existing, missing = [], []
    for path in candidates:
        is_bare_name = "/" not in path
        if ref is None:
            found = (
                basename_exists_in_worktree(root, path)
                if is_bare_name
                else path_exists_in_worktree(root, path)
            )
        else:
            found = (
                basename_exists_in_ref(root, ref, path)
                if is_bare_name
                else path_exists_in_ref(root, ref, path)
            )
        (existing if found else missing).append(path)
    return existing, missing


# --- declared absences (tests/parity_absences.txt) -----------------
#
# Mesma forma, mesma regra de morte de tests/parity_exceptions.txt
# (check_test_parity.py's own validate_exceptions()/parse_todo_status_
# text() - a leitura por coluna de TODO.md e' copiada literalmente
# daquele script, mesmo indice: 12 colunas apos o split por "|", ID no
# indice 2, Status no indice 9).

_TODO_ROW_RE = re.compile(r"^\|.*\|$")


def parse_todo_status(todo_text):
    status_by_item = {}
    for line in todo_text.splitlines():
        line = line.rstrip("\n")
        if not _TODO_ROW_RE.match(line.strip()):
            continue
        parts = line.split("|")
        if len(parts) != 12:
            continue
        item_id = parts[2].strip()
        status_text = parts[9].strip()
        if not item_id or item_id in ("ID", "---") or set(item_id) <= {"-"}:
            continue
        status_by_item[item_id] = {
            "status_text": status_text,
            "concluded": status_text.startswith("✅"),
        }
    return status_by_item


def parse_absences(absences_text):
    """Returns {path: (reason, item)} - blank lines and '#' comments
    skipped, same shape tests/parity_exceptions.txt's own parser
    already uses one file over."""
    absences = {}
    for raw_line in absences_text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|")
        if len(parts) != 3:
            continue
        path, reason, item = (part.strip() for part in parts)
        absences[path] = (reason, item)
    return absences


def validate_absence_deaths(missing, absences, todo_status, todo_text=""):
    """Returns (declared, undeclared, dead) - `dead` is the death-rule
    violation this function exists to catch: a declared absence whose
    own item is already CONCLUDED in TODO.md, the exact 'concluido sem
    par' shape tests/parity_exceptions.txt's own validate_exceptions()
    already catches one file over, applied here to a promised PATH
    instead of a promised ctest name.

    An item NOT found as a table row (`status_by_item`) but mentioned
    ANYWHERE else in `todo_text` is treated as open, never dead - this
    project's own L-63 convention lands new work in the INBOX first,
    as free prose, before it is ever promoted into the 10-column table
    (WL-DISPLAY-BACKEND-SEAT itself, this fatia's own item, is exactly
    that: registered, real, genuinely not concluded, and NOT yet a
    table row). Only an item absent from the WHOLE file, table and
    prose alike, is treated as a real error (a typo'd or invented
    item name)."""
    declared, undeclared, dead = [], [], []
    for path in missing:
        entry = absences.get(path)
        if entry is None:
            undeclared.append(path)
            continue
        reason, item = entry
        status = todo_status.get(item)
        if status is None:
            if item in todo_text:
                declared.append((path, reason, item))
                continue
            dead.append((path, reason, item, f"item {item!r} nao existe em TODO.md (nem na tabela, nem na INBOX)"))
        elif status["concluded"]:
            dead.append(
                (
                    path,
                    reason,
                    item,
                    f"item {item!r} ja esta CONCLUIDO ({status['status_text']}) - regra "
                    "'concluido sem par': apague esta linha de tests/parity_absences.txt, o "
                    "caminho que ela desculpava ja deveria existir",
                )
            )
        else:
            declared.append((path, reason, item))
    return declared, undeclared, dead


def real_main(args):
    parser = argparse.ArgumentParser(prog=SCRIPT_NAME, add_help=False)
    parser.add_argument("plan", help="caminho do plano (Markdown)")
    parser.add_argument("--marker", required=True)
    parser.add_argument("--ref", default=None, help="SHA/branch a conferir - omitido = árvore de trabalho")
    parser.add_argument("--root", default=".")
    parser.add_argument(
        "--absences",
        default=None,
        help="tests/parity_absences.txt - ausencias declaradas, mesma forma/regra de morte de "
        "tests/parity_exceptions.txt",
    )
    parser.add_argument("--todo", default=None, help="TODO.md - exigido junto de --absences")
    parsed = parser.parse_args(args)

    plan_text = Path(parsed.plan).read_text(encoding="utf-8")
    row_line = find_row_line(plan_text, parsed.marker)
    columns = split_row_columns(row_line)
    # split_row_columns() splits on EVERY `|`, including the leading
    # one before the first cell and the trailing one after the last -
    # columns[0] is always "" (before "| # |"), so column 4 (not 3) is
    # "Nasce / muda": [0]="", [1]="#", [2]="Fatia", [3]="Lado",
    # [4]="Nasce / muda" (docs/plano-w6a-janela.md's own table header,
    # section 2.2) - an off-by-one here silently reads "Lado" ("comum"/
    # "Wayland"/"Windows", never a file path) instead, which the
    # extraction floor below would ALSO have caught as zero candidates,
    # but proving the right cause here is cheaper than making the
    # reader guess (this exact bug was caught live while writing this
    # script, against the real plan file, not hypothetical).
    if len(columns) < 5:
        fail(
            f"linha da tabela tem {len(columns)} coluna(s), esperava pelo menos 5 "
            "(vazio | # | Fatia | Lado | Nasce/muda | ...)"
        )
    deliverable_column = columns[4]

    candidates = extract_candidate_paths(deliverable_column)
    print(f"{SCRIPT_NAME}: {len(candidates)} caminho(s) citado(s) na coluna de entregáveis")
    if len(candidates) == 0:
        fail(
            "0 caminho(s) citado(s) - varredura vazia (GODS_LAWS.md L-40): o marcador achou a "
            "linha errada, ou a coluna de entregáveis mudou de forma sem este script acompanhar"
        )

    existing, missing = check_existence(parsed.root, parsed.ref, candidates)
    ref_label = parsed.ref if parsed.ref is not None else "árvore de trabalho"

    absences = {}
    todo_status = {}
    todo_text = ""
    if parsed.absences is not None:
        if parsed.todo is None:
            fail("--absences exige --todo junto (a regra de morte le o status do item la)")
        absences = parse_absences(Path(parsed.absences).read_text(encoding="utf-8"))
        todo_text = Path(parsed.todo).read_text(encoding="utf-8")
        todo_status = parse_todo_status(todo_text)

    declared, undeclared, dead = validate_absence_deaths(missing, absences, todo_status, todo_text)

    print(
        f"{SCRIPT_NAME}: contra {ref_label} - {len(existing)} existe(m), {len(missing)} falta(m) "
        f"({len(declared)} declarada(s), {len(undeclared)} sem declaracao, {len(dead)} morta(s))"
    )
    for path in candidates:
        if path in existing:
            print(f"  [OK ] {path}")
        elif path in absences and any(p == path for p, _, _ in declared):
            reason, item = absences[path]
            print(f"  [DECLARADA] {path} (item {item}: {reason})")
        else:
            print(f"  [FALTA] {path}")

    errors = []
    for path in undeclared:
        errors.append(f"{path}: ausente da arvore, sem linha em tests/parity_absences.txt")
    for path, _reason, item, why in dead:
        errors.append(f"{path}: ausencia declarada mas MORTA - {why}")

    if errors:
        fail(
            f"{len(errors)} problema(s) de escopo ({ref_label}):\n  " + "\n  ".join(errors)
        )

# --- PLAN-SCOPE-COLUMNS, esquema v1 e lista de legado (D-A7, docs/plano-w7c-
# adendo-revalidacao.md secao 3.F, sub-fatia F0) --------------------------
#
# O portao antigo (real_main/find_row_line acima) reconhecia UMA tabela de
# UM plano por marcador e posicao fixa de coluna (columns[4]). D17 propunha
# reconhecer pelo CABECALHO - mas so foi medido contra 16 planos versionados
# depois de escrito, e a heuristica "achar a linha com # + Fatia/Sub-fatia"
# so alcanca 5 dos 16 (docs/plano-w7c-adendo-revalidacao.md, secao 1.B). O
# problema nao e' o regex: e' a falta de um ESQUEMA DECLARADO (a licao de
# Sphinx-Needs e Doorstop, mesma secao, "2. Pesquisa nova").
#
# O que segue e' um modo NOVO (--audit), que varre TODOS os
# docs/plano-*.md, classifica cada um (tem tabela no esquema v1 | nao tem,
# e por que) e confere essa classificacao contra uma lista de legado
# (tests/plan_scope_legacy.txt) escrita e VERIFICADA por maquina - nunca
# aceita a palavra do autor da linha de legado sem prova (a mesma doenca
# que PARITY-ALIAS-HYGIENE pagou quatro sub-fatias para curar: uma
# declaracao "bilateral=<motivo>" que passava calada ate o motivo ser
# conferido).

# Nome exato de coluna que marca uma tabela como "candidata a tabela de
# fatia" - so estas duas formas, e so por igualdade EXATA da celula (uma
# celula de PROSA que so MENCIONA a palavra "Fatia" - ex.: um cabecalho de
# resumo como "O portao desenhado em D17 (`#` + `Fatia`/`Sub-fatia`) acha?"
# - nao e' uma coluna chamada "Fatia", e nao pode contar).
FATIA_COLUMN_NAMES = {"Fatia", "Sub-fatia"}

# As colunas do esquema v1 (D-A7): tres exigidas por nome EXATO, mais a
# coluna de Fatia/Sub-fatia (uma das duas, por definicao de candidata) e
# pelo menos uma coluna cujo NOME COMECA POR "Prova" (prefixo, nunca
# substring no meio - e' o mutante nomeado no proprio item F0: "aceitar
# qualquer coluna que contenha 'Prova' no meio do nome").
V1_EXACT_REQUIRED_COLUMNS = ("#", "Nasce / muda", "Par no portão")
V1_PROVA_PREFIX = "Prova"

_TABLE_ROW_RE = re.compile(r"^\|.*\|$")
_TABLE_SEP_RE = re.compile(r"^\|[\s:|-]+\|$")


def parse_markdown_tables(text):
    """Returns every markdown table found in `text` as a list of
    {"line_no": <1-indexed header line>, "columns": [...]} - a table is
    recognized by a header row immediately followed by a separator row
    (`|---|---|`), never by content alone (a normal prose row that
    happens to look like "| a | b |" without a separator under it is NOT
    a table header - GitHub-Flavored Markdown's own rule)."""
    lines = text.splitlines()
    tables = []
    for i in range(len(lines) - 1):
        header_line = lines[i].strip()
        sep_line = lines[i + 1].strip()
        if not _TABLE_ROW_RE.match(header_line):
            continue
        if not _TABLE_SEP_RE.match(sep_line) or "-" not in sep_line:
            continue
        columns = [cell.strip() for cell in header_line.strip("|").split("|")]
        tables.append({"line_no": i + 1, "columns": columns})
    return tables


def find_fatia_tables(text):
    """Tables whose header has a column named EXACTLY "Fatia" or
    "Sub-fatia" - the "candidate" tables a plan needs at least one of to
    ever be legitimately classified `tabela-fora-do-esquema` instead of
    `sem-tabela-de-fatia` (D-A7)."""
    return [t for t in parse_markdown_tables(text) if FATIA_COLUMN_NAMES & set(t["columns"])]


def missing_v1_columns(columns):
    """Returns the list of scheme-v1 required columns `columns` is
    missing, by NAME (never a generic "malformed table" message) - the
    F0 estreia vermelha explicitly requires a table with `Entrega`
    instead of `Nasce / muda` to reprove NAMING the missing column."""
    colset = set(columns)
    missing = [name for name in V1_EXACT_REQUIRED_COLUMNS if name not in colset]
    if not (FATIA_COLUMN_NAMES & colset):
        missing.append("Fatia ou Sub-fatia")
    if not any(col.startswith(V1_PROVA_PREFIX) for col in columns):
        missing.append(f'uma coluna que comece por "{V1_PROVA_PREFIX}"')
    return missing


def table_is_v1(columns):
    return len(missing_v1_columns(columns)) == 0


def plan_v1_status(plan_text):
    """Classifies one plan document's text. Returns a dict:
    - "category": None (has at least one scheme-v1 table - not legacy),
      "sem-tabela-de-fatia" (no table has a Fatia/Sub-fatia column at
      all), or "tabela-fora-do-esquema" (has such a table, but none of
      them satisfy the full v1 scheme).
    - "fatia_tables": every candidate table found (line_no + columns).
    - "missing_by_table": {line_no: [missing column names]} for every
      candidate table that does NOT satisfy v1 (empty when the plan has
      at least one v1 table already)."""
    fatia_tables = find_fatia_tables(plan_text)
    v1_tables = [t for t in fatia_tables if table_is_v1(t["columns"])]
    if v1_tables:
        category = None
    elif fatia_tables:
        category = "tabela-fora-do-esquema"
    else:
        category = "sem-tabela-de-fatia"
    missing_by_table = {
        t["line_no"]: missing_v1_columns(t["columns"]) for t in fatia_tables if t not in v1_tables
    }
    return {
        "category": category,
        "fatia_tables": fatia_tables,
        "v1_tables": v1_tables,
        "missing_by_table": missing_by_table,
    }


# --- tests/plan_scope_legacy.txt: forma `caminho|categoria|motivo`, mesma
# regra de morte de tests/parity_exceptions.txt, mais a verificacao NOVA
# que aquele arquivo nao tinha (24/09/2026, ataque-w7c.md): a CATEGORIA
# se confere contra o proprio plano, nunca so contra a palavra do autor.

LEGACY_CATEGORIES = {"sem-tabela-de-fatia", "tabela-fora-do-esquema"}

# Lista fechada de motivo-marcador (D-A7): cada um destes, sozinho (depois
# de tirar acento/pontuacao/caixa), e' motivo trivial - mesmo que, hoje,
# todos ja tenham menos de cinco palavras e por isso ja' cassem na regra
# (a) antes de chegar aqui. A lista fica escrita mesmo assim (L-43: a
# definicao se fixa ANTES do dado, nao se poda por redundancia observada
# hoje).
TRIVIAL_REASON_MARKERS = {
    "legado",
    "antigo",
    "historico",
    "n/a",
    "na",
    "todo",
    "idem",
    "ver acima",
    "mesmo motivo",
    "sem motivo",
}

_WORD_RE = re.compile(r"[^\W_]+", re.UNICODE)


def normalize_reason(text):
    """Sem acento, sem pontuacao, em minusculas, espacos colapsados -
    a mesma normalizacao serve para comparar contra a lista fechada de
    marcadores E para achar motivo duplicado (D-A7 diz "a mesma
    normalizacao" para os dois)."""
    decomposed = unicodedata.normalize("NFKD", text)
    without_accents = "".join(ch for ch in decomposed if not unicodedata.combining(ch))
    lowered = without_accents.lower()
    without_punctuation = re.sub(r"[^\w\s]", "", lowered, flags=re.UNICODE).replace("_", "")
    return re.sub(r"\s+", " ", without_punctuation).strip()


def reason_word_count(text):
    """Palavra = sequencia de letras ou digitos (D-A7, literal)."""
    return len(_WORD_RE.findall(text))


def reason_is_empty(reason_text):
    return reason_text.strip() == ""


def reason_is_trivial(reason_text):
    if reason_word_count(reason_text) < 5:
        return True
    return normalize_reason(reason_text) in TRIVIAL_REASON_MARKERS


def parse_legacy_file(legacy_text):
    """Returns [{"line_no", "path", "category", "reason"}] - blank lines
    and `#` comments skipped; splits on the FIRST TWO `|` only, so a
    motivo that itself contains `|` is never truncated."""
    entries = []
    for line_no, raw_line in enumerate(legacy_text.splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split("|", 2)
        if len(parts) != 3:
            continue
        path, category, reason = (part.strip() for part in parts)
        entries.append({"line_no": line_no, "path": path, "category": category, "reason": reason})
    return entries


def find_duplicate_reasons(entries):
    """Returns [(entry, first_entry_with_same_normalized_reason)] for
    every entry whose motivo (normalized) already belongs to an earlier
    entry - empty-after-normalization reasons are skipped here (already
    caught by reason_is_empty/reason_is_trivial, never double-reported
    as "duplicate of nothing")."""
    seen = {}
    duplicates = []
    for entry in entries:
        key = normalize_reason(entry["reason"])
        if not key:
            continue
        if key in seen:
            duplicates.append((entry, seen[key]))
        else:
            seen[key] = entry
    return duplicates


def validate_legacy_entries(root, entries):
    """Cross-checks every tests/plan_scope_legacy.txt entry against the
    REAL plan it names - never trusts the declared categoria/motivo on
    their own. Returns a list of error strings (empty = all entries are
    truthful)."""
    errors = []
    status_cache = {}

    def status_for(path):
        if path not in status_cache:
            full_path = Path(root) / path
            if not full_path.is_file():
                status_cache[path] = None
            else:
                status_cache[path] = plan_v1_status(full_path.read_text(encoding="utf-8"))
        return status_cache[path]

    for entry in entries:
        path, category, reason, line_no = entry["path"], entry["category"], entry["reason"], entry["line_no"]

        if reason_is_empty(reason):
            errors.append(f"tests/plan_scope_legacy.txt:{line_no}: {path} - motivo vazio")
        elif reason_is_trivial(reason):
            errors.append(
                f"tests/plan_scope_legacy.txt:{line_no}: {path} - motivo trivial "
                f"(menos de 5 palavras, ou marcador da lista fechada): {reason!r}"
            )

        if category not in LEGACY_CATEGORIES:
            errors.append(
                f"tests/plan_scope_legacy.txt:{line_no}: {path} - categoria {category!r} fora "
                f"da lista fechada {sorted(LEGACY_CATEGORIES)}"
            )
            continue

        status = status_for(path)
        if status is None:
            errors.append(f"tests/plan_scope_legacy.txt:{line_no}: {path} - plano nao existe na arvore")
        elif status["category"] is None:
            errors.append(
                f"tests/plan_scope_legacy.txt:{line_no}: {path} - LEGADO MORTO: o plano ja tem "
                "tabela no esquema v1 (regra de morte igual a tests/parity_exceptions.txt); apague esta linha"
            )
        elif status["category"] != category:
            errors.append(
                f"tests/plan_scope_legacy.txt:{line_no}: {path} - categoria declarada {category!r} "
                f"e' motivo falso: a categoria real, conferida contra o proprio plano, e' "
                f"{status['category']!r}"
            )

    for dup_entry, first_entry in find_duplicate_reasons(entries):
        errors.append(
            f"tests/plan_scope_legacy.txt:{dup_entry['line_no']}: {dup_entry['path']} - motivo "
            f"identico (depois de normalizar acento/caixa/pontuacao) ao da linha "
            f"{first_entry['line_no']} ({first_entry['path']}): {dup_entry['reason']!r}"
        )

    return errors


def real_audit_main(args):
    parser = argparse.ArgumentParser(prog=f"{SCRIPT_NAME} --audit", add_help=False)
    parser.add_argument("--root", default=".")
    parser.add_argument("--plans-glob", default="docs/plano-*.md")
    parser.add_argument("--legacy", default="tests/plan_scope_legacy.txt")
    parsed = parser.parse_args(args)

    root = Path(parsed.root)
    legacy_path = root / parsed.legacy
    if not legacy_path.is_file():
        fail(f"{parsed.legacy} nao existe")
    entries = parse_legacy_file(legacy_path.read_text(encoding="utf-8"))
    legacy_by_path = {}
    for entry in entries:
        legacy_by_path.setdefault(entry["path"], []).append(entry)

    errors = validate_legacy_entries(root, entries)

    plan_paths = sorted(root.glob(parsed.plans_glob))
    v1_count = 0
    legacy_count = 0
    for plan_path in plan_paths:
        rel = plan_path.relative_to(root).as_posix()
        status = plan_v1_status(plan_path.read_text(encoding="utf-8"))
        if status["category"] is None:
            v1_count += 1
        else:
            legacy_count += 1
            if rel not in legacy_by_path:
                errors.append(
                    f"{rel}: nao tem tabela no esquema v1 (categoria real {status['category']!r}) "
                    f"e esta AUSENTE de {parsed.legacy} - declare a linha ou construa a tabela v1"
                )

    print(
        f"{SCRIPT_NAME} --audit: {len(plan_paths)} plano(s) varrido(s), {v1_count} com tabela v1, "
        f"{legacy_count} de legado, {len(entries)} linha(s) em {parsed.legacy}"
    )
    if len(plan_paths) == 0:
        fail(f'0 planos varridos por "{parsed.plans_glob}" - varredura vazia (GODS_LAWS.md L-40)')

    if errors:
        fail(f"{len(errors)} problema(s) de escopo de plano:\n  " + "\n  ".join(errors))


#-- - selftest -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

def selftest_extraction_matches_real_row():
    row_text = (
        '`include/glintfx/platform/window/display.hpp` e `window.hpp` (forma); '
        '`src/platform/port/display_backend_port.hpp` e `window_adapter_port.hpp` (concepts); '
        '`display_connection<A>::adapter()`; '
        '`src/platform/window/display_facade.cpp` e `window_facade.cpp` sobre '
        '`selected_display_backend`/`selected_window_adapter`; '
        '`src/platform/wayland/display_backend.{hpp,cpp}` (compõe); '
        '`tests/hostile_win32_macros_shim.hpp`; cobertura de higiene; '
        '`prepare_arch_ports_fixture.sh` estagia `src/platform/window/` e os `.cpp` novos. '
        'cada célula conferida em `learn.microsoft.com`, `<windows.h>`, `adapter()`, `desc.logical_size`'
    )
    candidates = extract_candidate_paths(row_text)
    expected = {
        "include/glintfx/platform/window/display.hpp",
        "window.hpp",
        "src/platform/port/display_backend_port.hpp",
        "window_adapter_port.hpp",
        "src/platform/window/display_facade.cpp",
        "window_facade.cpp",
        "src/platform/wayland/display_backend.hpp",
        "src/platform/wayland/display_backend.cpp",
        "tests/hostile_win32_macros_shim.hpp",
        "prepare_arch_ports_fixture.sh",
    }
    if set(candidates) != expected:
        print(
            f"selftest: extracao nao bateu com a linha real da fatia 6 - achou {sorted(candidates)}, "
            f"esperava {sorted(expected)}",
            file=sys.stderr,
        )
        return False
    print("selftest: extração bateu os 10 caminhos exatos da linha real da fatia 6 - ok")
    return True


def selftest_end_to_end_column_offset():
    """Exercises find_row_line() + split_row_columns() TOGETHER
    against a fake table shaped exactly like docs/plano-w6a-janela.md's
    own (# | Fatia | Lado | Nasce/muda | Prova Linux | Prova Windows |
    Par) - selftest_extraction_matches_real_row() above only calls
    extract_candidate_paths() directly and would NOT have caught the
    off-by-one this script actually shipped with once (column 3 read
    "Lado", never the deliverables column 4) - real_main() itself
    caught it, live, against the real plan file, which is exactly the
    gap this end-to-end control now closes."""
    import tempfile

    content = (
        "| # | Fatia | Lado | Nasce / muda | Prova Linux | Prova Windows | Par |\n"
        "|---|---|---|---|---|---|---|\n"
        "| 6 | **MARCADOR-E2E** | comum | `a/b/real_path.hpp` | prova | prova | par |\n"
    )
    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as handle:
        handle.write(content)
        plan_path = handle.name
    root_dir = Path(plan_path).parent
    (root_dir / "a").mkdir(exist_ok=True)
    (root_dir / "a" / "b").mkdir(exist_ok=True)
    (root_dir / "a" / "b" / "real_path.hpp").write_text("", encoding="utf-8")
    try:
        try:
            real_main([plan_path, "--marker", "MARCADOR-E2E", "--root", str(root_dir)])
        except SystemExit as exc:
            print(f"selftest: pipeline completa reprovou inesperadamente (codigo {exc.code}) - "
                  "a coluna certa nao foi lida", file=sys.stderr)
            return False
        print("selftest: pipeline completa (linha->colunas->extracao->existencia) leu a coluna "
              "certa (4, 'Nasce/muda', nunca 3, 'Lado') - ok")
        return True
    finally:
        Path(plan_path).unlink(missing_ok=True)


def _write_plan(tmp_path, marker, missing_path):
    content = (
        "| # | Fatia | Lado | Nasce / muda | Prova Linux | Prova Windows | Par |\n"
        "|---|---|---|---|---|---|---|\n"
        f"| 6 | **{marker}** | comum | `{missing_path}` | prova | prova | par |\n"
    )
    plan_path = tmp_path / "plano.md"
    plan_path.write_text(content, encoding="utf-8")
    return plan_path


def selftest_declared_absence_with_open_item_accepted(tmp_path):
    """An absence declared in tests/parity_absences.txt, pointing at
    an item still OPEN in TODO.md, must NOT reprove - the exact
    'decidido, com dono' shape item 1 of this fatia's own briefing
    describes."""
    marker = "MARCADOR-ABSENCE-OPEN"
    missing_path = "does/not/exist.hpp"
    plan_path = _write_plan(tmp_path, marker, missing_path)
    absences_path = tmp_path / "absences.txt"
    absences_path.write_text(f"{missing_path}|ainda nao construido, W6b|ITEM-ABERTO\n", encoding="utf-8")
    todo_path = tmp_path / "TODO.md"
    todo_path.write_text(
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        "| 1.0 | ITEM-ABERTO | W1 | X | y | Alta | - | Media | ⏳ Pendente | - |\n",
        encoding="utf-8",
    )
    try:
        real_main(
            [
                str(plan_path),
                "--marker",
                marker,
                "--root",
                str(tmp_path),
                "--absences",
                str(absences_path),
                "--todo",
                str(todo_path),
            ]
        )
    except SystemExit as exc:
        print(f"selftest: ausencia declarada com item ABERTO reprovou inesperadamente (codigo {exc.code})",
              file=sys.stderr)
        return False
    print("selftest: ausencia declarada com item aberto em TODO.md e' aceita (nao reprova) - ok")
    return True


def selftest_declared_absence_with_concluded_item_dies(tmp_path):
    """The death rule: an absence whose OWN item is already CONCLUDED
    in TODO.md must reprove - the work that would have closed the gap
    already happened, and the line excusing it should already be gone
    (mirrors check_test_parity.py's own validate_exceptions() for the
    identical shape, one file over)."""
    marker = "MARCADOR-ABSENCE-DEAD"
    missing_path = "does/not/exist2.hpp"
    plan_path = _write_plan(tmp_path, marker, missing_path)
    absences_path = tmp_path / "absences2.txt"
    absences_path.write_text(f"{missing_path}|deveria ter sido feito|ITEM-CONCLUIDO\n", encoding="utf-8")
    todo_path = tmp_path / "TODO2.md"
    todo_path.write_text(
        "| WSJF | ID | Onda | Grupo | Descricao | Prioridade | Pre-requisito | Dificuldade | Status | Estado |\n"
        "|---|---|---|---|---|---|---|---|---|---|\n"
        "| 1.0 | ITEM-CONCLUIDO | W1 | X | y | Alta | - | Media | ✅ Concluído | - |\n",
        encoding="utf-8",
    )
    try:
        real_main(
            [
                str(plan_path),
                "--marker",
                marker,
                "--root",
                str(tmp_path),
                "--absences",
                str(absences_path),
                "--todo",
                str(todo_path),
            ]
        )
    except SystemExit as exc:
        if exc.code == 1:
            print("selftest: ausencia cujo item ja esta CONCLUIDO reprova (regra de morte) - ok")
            return True
        print(f"selftest: codigo inesperado {exc.code} para ausencia morta", file=sys.stderr)
        return False
    print("selftest: ausencia com item concluido NAO reprovou - esperado exit 1 (regra de morte)",
          file=sys.stderr)
    return False


def selftest_undeclared_absence_reproves(tmp_path):
    """A path missing from the tree with NO line in tests/parity_
    absences.txt at all is never accepted - "ausencia sem item nao e'
    aceita" (this fatia's own briefing, item 1)."""
    marker = "MARCADOR-ABSENCE-UNDECLARED"
    missing_path = "does/not/exist3.hpp"
    plan_path = _write_plan(tmp_path, marker, missing_path)
    absences_path = tmp_path / "absences3.txt"
    absences_path.write_text("", encoding="utf-8")
    todo_path = tmp_path / "TODO3.md"
    todo_path.write_text("| WSJF | ID |\n|---|---|\n", encoding="utf-8")
    try:
        real_main(
            [
                str(plan_path),
                "--marker",
                marker,
                "--root",
                str(tmp_path),
                "--absences",
                str(absences_path),
                "--todo",
                str(todo_path),
            ]
        )
    except SystemExit as exc:
        if exc.code == 1:
            print("selftest: ausencia sem linha em parity_absences.txt reprova (nao aceita silencio) - ok")
            return True
        print(f"selftest: codigo inesperado {exc.code} para ausencia sem declaracao", file=sys.stderr)
        return False
    print("selftest: ausencia sem declaracao NAO reprovou - esperado exit 1", file=sys.stderr)
    return False


def selftest_zero_candidates_reproves():
    import contextlib
    import io

    row_text = "sem caminho nenhum aqui, so' `adapter()` e `<windows.h>` e `desc.logical_size`"
    import tempfile

    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as handle:
        handle.write(f"| 1 | **MARCADOR-TESTE** | comum | {row_text} | a | b | c |\n")
        plan_path = handle.name
    try:
        buffer = io.StringIO()
        with contextlib.redirect_stderr(buffer):
            try:
                real_main([plan_path, "--marker", "MARCADOR-TESTE"])
            except SystemExit as exc:
                if exc.code == 1 and "varredura vazia" in buffer.getvalue():
                    print("selftest: zero caminhos citados reprova (RED, piso L-40) - ok")
                    return True
                print(f"selftest: codigo/mensagem inesperados: {exc.code} / {buffer.getvalue()}",
                      file=sys.stderr)
                return False
        print("selftest: zero caminhos citados NAO reprovou - esperado exit 1", file=sys.stderr)
        return False
    finally:
        Path(plan_path).unlink(missing_ok=True)


def selftest_ambiguous_marker_reproves():
    """This document has TWO tables that both number their own rows
    starting at 1 (this script's own header comment) - a marker that
    accidentally matches a row in each of them must reprove, never
    silently pick the first one it finds."""
    import tempfile

    content = (
        "| 6 | **fatia A, MARCADOR-COMUM** | x | `a.hpp` | p | q | r |\n"
        "| 6 | **risco B, MARCADOR-COMUM** | y | `b.hpp` | p | q | r |\n"
    )
    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as handle:
        handle.write(content)
        plan_path = handle.name
    try:
        try:
            real_main([plan_path, "--marker", "MARCADOR-COMUM"])
        except SystemExit as exc:
            if exc.code == 1:
                print("selftest: marcador que casa DUAS linhas reprova (nunca escolhe a primeira "
                      "em silencio) - ok")
                return True
            print(f"selftest: codigo inesperado {exc.code} para marcador ambiguo", file=sys.stderr)
            return False
        print("selftest: marcador ambiguo NAO reprovou - esperado exit 1", file=sys.stderr)
        return False
    finally:
        Path(plan_path).unlink(missing_ok=True)


def selftest_alphanumeric_fatia_id_found_and_absolved(tmp_path):
    """PLAN-SCOPE-REGEX-BLIND (TODO.md): uma fatia `5c` (a forma REAL
    usada em docs/plano-w6b-placa-e-laco.md, linha 127 - GFX-PRESET)
    cujo unico caminho prometido EXISTE tem que ser encontrada e
    absolvida (exit 0) - antes do conserto, find_row_line() nunca
    encontrava a linha, entao nem chegava a checar existencia nenhuma."""
    marker = "MARCADOR-5C-ABSOLVE"
    good_path = "src/platform/gl/gfx_preset_table.hpp"
    (tmp_path / good_path).parent.mkdir(parents=True, exist_ok=True)
    (tmp_path / good_path).write_text("", encoding="utf-8")
    plan_path = tmp_path / "plano-5c-ok.md"
    plan_path.write_text(
        "| # | Fatia | Lado | Nasce / muda | Prova Linux | Prova Windows | Par |\n"
        "|---|---|---|---|---|---|---|\n"
        f"| 5c | **{marker}** | comum | `{good_path}` | p | q | r |\n",
        encoding="utf-8",
    )
    try:
        real_main([str(plan_path), "--marker", marker, "--root", str(tmp_path)])
    except SystemExit as exc:
        print(
            f"selftest: fatia alfanumerica ('5c') com caminho existente reprovou inesperadamente "
            f"(codigo {exc.code}) - find_row_line() ainda esta cega para identificador nao-numerico",
            file=sys.stderr,
        )
        return False
    print("selftest: fatia alfanumerica ('5c') com caminho existente e' encontrada e absolvida - ok")
    return True


def selftest_alphanumeric_fatia_id_catches_real_gap(tmp_path):
    """A METADE que faz a fatia PLAN-SCOPE-REGEX-BLIND grave: antes do
    conserto, uma fatia `5c` com caminho FALTANDO nao reprovava pelo
    motivo real (arquivo ausente) - reprovava, se reprovasse, com
    'nenhuma linha... encontrada', uma mensagem que nao fala nada sobre
    o caminho prometido. Isto e' o que deixou GFX-PRESET (a fatia 5c
    real de docs/plano-w6b-placa-e-laco.md) sem ser entregue e sem
    ninguem notar por maquina. O conserto tem que reprovar CITANDO o
    caminho ausente, nao com a mensagem antiga de linha nao encontrada."""
    marker = "MARCADOR-5C-GAP"
    missing_path = "does/not/exist_gfx_preset.cpp"
    plan_path = tmp_path / "plano-5c-gap.md"
    plan_path.write_text(
        "| # | Fatia | Lado | Nasce / muda | Prova Linux | Prova Windows | Par |\n"
        "|---|---|---|---|---|---|---|\n"
        f"| 5c | **{marker}** | comum | `{missing_path}` | p | q | r |\n",
        encoding="utf-8",
    )
    import contextlib
    import io

    buffer = io.StringIO()
    with contextlib.redirect_stderr(buffer):
        try:
            real_main([str(plan_path), "--marker", marker, "--root", str(tmp_path)])
        except SystemExit as exc:
            stderr_text = buffer.getvalue()
            if exc.code == 1 and missing_path in stderr_text and "ausente da arvore" in stderr_text:
                print(
                    "selftest: fatia alfanumerica ('5c') com caminho ausente reprova CITANDO o "
                    "caminho real (nao mais 'linha nao encontrada') - ok"
                )
                return True
            print(
                f"selftest: reprovou pelo motivo ERRADO ou codigo errado (codigo {exc.code}): "
                f"{stderr_text!r}",
                file=sys.stderr,
            )
            return False
    print(
        "selftest: fatia alfanumerica ('5c') com caminho ausente NAO reprovou - esperado exit 1",
        file=sys.stderr,
    )
    return False


# --- selftests do esquema v1 e da lista de legado (F0) --------------------


def _write_legacy(tmp_path, name, lines):
    path = tmp_path / name
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def _write_docs_plan(tmp_path, name, body):
    docs_dir = tmp_path / "docs"
    docs_dir.mkdir(exist_ok=True)
    path = docs_dir / name
    path.write_text(body, encoding="utf-8")
    return path


_V1_TABLE = (
    "| # | Fatia | Lado | Nasce / muda | Prova Linux | Par no portão |\n"
    "|---|---|---|---|---|---|\n"
    "| 1 | X | comum | `a.hpp` | `x_test` | `x_test` |\n"
)

_NO_FATIA_TABLE = (
    "| Caso | Mata |\n"
    "|---|---|\n"
    "| c1 | m1 |\n"
)

_ENTREGA_TABLE = (
    "| # | Sub-fatia | Entrega | Fechamento |\n"
    "|---|---|---|---|\n"
    "| P0 | X | algo | pronto |\n"
)


def selftest_missing_column_named_when_entrega_used():
    """Estreia vermelha F0: uma tabela com `Entrega` no lugar de `Nasce /
    muda` reprova NOMEANDO a coluna que falta, nunca com um erro
    generico."""
    columns = ["#", "Sub-fatia", "Entrega", "Fechamento"]
    missing = missing_v1_columns(columns)
    if "Nasce / muda" not in missing:
        print(f"selftest: 'Entrega' deveria acusar falta de 'Nasce / muda' - achou {missing}", file=sys.stderr)
        return False
    print("selftest: tabela com 'Entrega' no lugar de 'Nasce / muda' nomeia a coluna que falta - ok")
    return True


def selftest_prova_prefix_not_substring():
    """Mutante nomeado no proprio item: aceitar qualquer coluna que
    CONTENHA 'Prova' no meio do nome. Uma coluna 'Melhor Prova de Todas'
    (Prova no meio, nao no inicio) NAO pode satisfazer o requisito."""
    columns = ["#", "Fatia", "Nasce / muda", "Melhor Prova de Todas", "Par no portão"]
    if table_is_v1(columns):
        print("selftest: coluna com 'Prova' NO MEIO do nome foi aceita - mutante vivo", file=sys.stderr)
        return False
    columns_prefix = ["#", "Fatia", "Nasce / muda", "Prova única", "Par no portão"]
    if not table_is_v1(columns_prefix):
        print("selftest: coluna que COMECA por 'Prova' deveria satisfazer o requisito", file=sys.stderr)
        return False
    print("selftest: requisito de 'Prova' e' por PREFIXO, nunca substring no meio - ok")
    return True


def selftest_audit_plan_without_v1_and_without_legacy_reproves(tmp_path):
    """'plano novo sem tabela v1 reprova': nenhuma linha em
    plan_scope_legacy.txt para um plano que nao tem tabela no esquema
    v1 tem que reprovar a auditoria inteira."""
    _write_docs_plan(tmp_path, "plano-novo-sem-v1.md", _NO_FATIA_TABLE)
    _write_legacy(tmp_path, "legacy_empty.txt", [])
    code, _out, err = _run_audit_with(tmp_path, "legacy_empty.txt")
    if code == 1 and "plano-novo-sem-v1.md" in err and "AUSENTE" in err:
        print("selftest: plano novo sem tabela v1 e sem linha de legado reprova - ok")
        return True
    print(f"selftest: esperava reprovar citando o plano ausente - codigo {code}, stderr {err!r}", file=sys.stderr)
    return False


def _run_audit_with(tmp_path, legacy_name):
    import contextlib
    import io

    out, err = io.StringIO(), io.StringIO()
    try:
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            real_audit_main(["--root", str(tmp_path), "--legacy", legacy_name])
    except SystemExit as exc:
        return exc.code, out.getvalue(), err.getvalue()
    return 0, out.getvalue(), err.getvalue()


def selftest_audit_legacy_plan_that_gained_v1_table_dies(tmp_path):
    """'plano de legado que ganhou tabela v1 reprova como legado morto'."""
    _write_docs_plan(tmp_path, "plano-graduado.md", _V1_TABLE)
    _write_legacy(
        tmp_path,
        "legacy_dead.txt",
        ["docs/plano-graduado.md|sem-tabela-de-fatia|escrito antes de a tabela real nascer no documento"],
    )
    code, _out, err = _run_audit_with(tmp_path, "legacy_dead.txt")
    if code == 1 and "LEGADO MORTO" in err:
        print("selftest: plano de legado que ganhou tabela v1 reprova como legado morto - ok")
        return True
    print(f"selftest: esperava 'LEGADO MORTO' - codigo {code}, stderr {err!r}", file=sys.stderr)
    return False


def selftest_audit_reason_empty_reproves(tmp_path):
    _write_docs_plan(tmp_path, "plano-motivo-vazio.md", _NO_FATIA_TABLE)
    _write_legacy(tmp_path, "legacy_empty_reason.txt", ["docs/plano-motivo-vazio.md|sem-tabela-de-fatia|"])
    code, _out, err = _run_audit_with(tmp_path, "legacy_empty_reason.txt")
    if code == 1 and "motivo vazio" in err:
        print("selftest: motivo vazio reprova - ok")
        return True
    print(f"selftest: esperava 'motivo vazio' - codigo {code}, stderr {err!r}", file=sys.stderr)
    return False


def selftest_audit_reason_four_words_reproves(tmp_path):
    _write_docs_plan(tmp_path, "plano-motivo-curto.md", _NO_FATIA_TABLE)
    _write_legacy(
        tmp_path, "legacy_short.txt", ["docs/plano-motivo-curto.md|sem-tabela-de-fatia|so quatro palavras aqui"]
    )
    code, _out, err = _run_audit_with(tmp_path, "legacy_short.txt")
    if code == 1 and "motivo trivial" in err:
        print("selftest: motivo com quatro palavras reprova como trivial - ok")
        return True
    print(f"selftest: esperava 'motivo trivial' - codigo {code}, stderr {err!r}", file=sys.stderr)
    return False


def selftest_audit_reason_marker_legado_reproves(tmp_path):
    _write_docs_plan(tmp_path, "plano-motivo-marcador.md", _NO_FATIA_TABLE)
    _write_legacy(tmp_path, "legacy_marker.txt", ["docs/plano-motivo-marcador.md|sem-tabela-de-fatia|legado"])
    code, _out, err = _run_audit_with(tmp_path, "legacy_marker.txt")
    if code == 1 and "motivo trivial" in err:
        print("selftest: motivo 'legado' (marcador da lista fechada) reprova - ok")
        return True
    print(f"selftest: esperava 'motivo trivial' - codigo {code}, stderr {err!r}", file=sys.stderr)
    return False


def selftest_audit_duplicate_reasons_reprove_case_and_accent_insensitive(tmp_path):
    """Mutante nomeado: comparar motivos SEM normalizar acento e caixa.
    As duas linhas usam o MESMO motivo com acento/caixa diferentes - so'
    reprovam como duplicata se a comparacao normalizar os dois lados."""
    _write_docs_plan(tmp_path, "plano-dup-a.md", _NO_FATIA_TABLE)
    _write_docs_plan(tmp_path, "plano-dup-b.md", _NO_FATIA_TABLE)
    _write_legacy(
        tmp_path,
        "legacy_dup.txt",
        [
            "docs/plano-dup-a.md|sem-tabela-de-fatia|documento cobre discussao futura ainda nao fatiada",
            "docs/plano-dup-b.md|sem-tabela-de-fatia|DOCUMENTO COBRE DISCUSSÃO FUTURA AINDA NÃO FATIADA",
        ],
    )
    code, _out, err = _run_audit_with(tmp_path, "legacy_dup.txt")
    if code == 1 and "motivo" in err and "identico" in err:
        print("selftest: motivos identicos apos normalizar acento/caixa reprovam como duplicata - ok")
        return True
    print(f"selftest: esperava duplicata detectada - codigo {code}, stderr {err!r}", file=sys.stderr)
    return False


def selftest_audit_category_mismatch_is_false_reason(tmp_path):
    """'categoria sem-tabela-de-fatia num plano que tem tabela com
    coluna Fatia reprova como motivo falso' - a categoria conferida
    contra o proprio plano (tabela-fora-do-esquema, por ter Entrega no
    lugar de Nasce/muda) desmente a categoria declarada."""
    _write_docs_plan(tmp_path, "plano-categoria-falsa.md", _ENTREGA_TABLE)
    _write_legacy(
        tmp_path,
        "legacy_false.txt",
        ["docs/plano-categoria-falsa.md|sem-tabela-de-fatia|categoria escrita errada de proposito aqui"],
    )
    code, _out, err = _run_audit_with(tmp_path, "legacy_false.txt")
    if code == 1 and "motivo falso" in err and "tabela-fora-do-esquema" in err:
        print("selftest: categoria que o conteudo do plano desmente reprova como motivo falso - ok")
        return True
    print(f"selftest: esperava 'motivo falso' citando a categoria real - codigo {code}, stderr {err!r}",
          file=sys.stderr)
    return False


def selftest_audit_positive_control_passes(tmp_path):
    """Controle positivo: categoria verdadeira, motivo de cinco palavras
    de verdade, sem duplicata - a auditoria tem que passar (exit 0)."""
    _write_docs_plan(tmp_path, "plano-legado-de-verdade.md", _NO_FATIA_TABLE)
    _write_legacy(
        tmp_path,
        "legacy_ok.txt",
        [
            "docs/plano-legado-de-verdade.md|sem-tabela-de-fatia|"
            "descreve mutacoes por caso sem nenhuma tabela de fatia"
        ],
    )
    code, out, err = _run_audit_with(tmp_path, "legacy_ok.txt")
    if code == 0 and "1 plano(s) varrido(s)" in out:
        print("selftest: categoria verdadeira com motivo real de cinco palavras passa (controle positivo) - ok")
        return True
    print(f"selftest: controle positivo deveria passar - codigo {code}, stdout {out!r}, stderr {err!r}",
          file=sys.stderr)
    return False


def selftest_main():
    import tempfile
    from pathlib import Path as _Path

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = _Path(tmp)
        controls = [
            selftest_extraction_matches_real_row(),
            selftest_end_to_end_column_offset(),
            selftest_zero_candidates_reproves(),
            selftest_ambiguous_marker_reproves(),
            selftest_alphanumeric_fatia_id_found_and_absolved(tmp_path),
            selftest_alphanumeric_fatia_id_catches_real_gap(tmp_path),
            selftest_declared_absence_with_open_item_accepted(tmp_path),
            selftest_declared_absence_with_concluded_item_dies(tmp_path),
            selftest_undeclared_absence_reproves(tmp_path),
        ]
    controls += [
        selftest_missing_column_named_when_entrega_used(),
        selftest_prova_prefix_not_substring(),
    ]
    for fn in (
        selftest_audit_plan_without_v1_and_without_legacy_reproves,
        selftest_audit_legacy_plan_that_gained_v1_table_dies,
        selftest_audit_reason_empty_reproves,
        selftest_audit_reason_four_words_reproves,
        selftest_audit_reason_marker_legado_reproves,
        selftest_audit_duplicate_reasons_reprove_case_and_accent_insensitive,
        selftest_audit_category_mismatch_is_false_reason,
        selftest_audit_positive_control_passes,
    ):
        with tempfile.TemporaryDirectory() as case_tmp:
            controls.append(fn(_Path(case_tmp)))
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--compare":
        real_main(args[1:])
    elif args and args[0] == "--audit":
        real_audit_main(args[1:])
    else:
        fail(
            "usage: check_plan_scope_diff.py --compare <plano.md> --marker <substring> "
            "[--ref <sha>] [--root <dir>]  |  --audit [--root <dir>] [--plans-glob <glob>] "
            "[--legacy <arquivo>]  |  --selftest"
        )


if __name__ == "__main__":
    main()
