#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_claim_citations.py - CLAIM-CITATIONS sub-fatia E3 (docs/plano-
# w7c.md sec. 3.E, decisoes D14/D15; docs/plano-w7c-adendo-revalidacao.
# md sec. 3.E). Cabecalhos publicos afirmam comportamento igual entre
# sistemas ou medido, sem citar quem mediu (a mesma doenca de DOCS-
# COUNT-VOCAB, achado do CTO em 06/09/2026: "measured"/"identical"/
# "proved" em prosa, sem apontar para o teste que carrega a medida).
# Este portao varre os COMENTARIOS publicos (include/**/*.hpp) atras
# do vocabulario fechado abaixo e reprova qualquer ocorrencia sem uma
# das tres formas de citacao aceitas.
#
# VOCABULARIO FECHADO (D15):
#   - Igualdade entre sistemas: identical, identically, same on both,
#     same on all, both systems, both platforms, all five, every
#     platform.
#   - Medicao: measured, proved, proven.
#   - FORA, PERDA DECLARADA (D15): "never"/"always" - 378 e 26
#     ocorrencias em include/ na varredura de 06/09/2026 (F15 do
#     plano), quase todas contrato de funcao ("never allocates",
#     "always returns"), nao alegacao de medida especifica; incluir
#     as duas produziria um portao que reprova a propria prosa
#     estrutural deste projeto, nao a doenca que ele existe pra
#     corrigir.
#
# TRES FORMAS DE CITACAO ACEITAS, no MESMO bloco de comentario
# contiguo (GODS_LAWS.md L-40: enumeradas, fechado):
#   1. Teste citado: "Proved by:"/"proven by"/"see" + nome que EXISTE
#      no inventario fechado (test_name_inventory.py, E1) -
#      citation_grammar.has_cited_test_reference().
#   2. static_assert nomeado: o bloco menciona a palavra "static_
#      assert" E um static_assert(...) REAL aparece nas proximas
#      _STATIC_ASSERT_WINDOW_LINES linhas do MESMO arquivo (a prova
#      compila com o proprio build do consumidor - nao ha "citacao
#      falsa" possivel aqui, um static_assert que nao existe nao
#      compila).
#   3. "by construction": aceita, mas CONTADA A PARTE, impressa como
#      "N alegacao(oes) por construcao, sem teste" (D15) - uma
#      alegacao de que o comportamento e' garantido pela FORMA do
#      codigo (ex.: uma validacao comum antes de qualquer backend ver
#      a requisicao), nunca por medicao em cada sistema.
#
# QUARTA SAIDA, NAO UMA FORMA DE CITACAO: tests/claim_exceptions.txt,
# MESMA forma e MESMA regra de morte de tests/parity_exceptions.txt
# (GODS_LAWS.md L-17 - "arquivo | trecho-ancora | item", item tem que
# existir em TODO.md e NAO estar concluido; entrada morre e reprova
# quando o item fecha).
#
# GODS_LAWS.md L-40 (piso de varredura nao-vazia): zero blocos de
# comentario varridos, ou zero arquivo .hpp varrido, reprova - sinal
# de leitor quebrado, nunca "nenhum cabecalho existe".
#
# O QUE ESTE PORTAO NAO VE (GODS_LAWS.md L-40, declarado tambem no
# relatorio de cada execucao):
#   - Um nome de teste citado longe (outro paragrafo) da frase-
#     gatilho, dentro do MESMO bloco de comentario contiguo, e aceito
#     por coincidencia - citation_grammar.py's own header comment
#     declara o mesmo limite.
#   - static_assert cuja mensagem NAO prova de fato a alegacao do
#     comentario (o portao so confere que um static_assert real
#     existe perto, nunca que o PREDICADO dele corresponde ao texto
#     em prosa - isso e' revisao humana, nao mecanizavel aqui).
#   - "by construction" e aceito pela FRASE, nunca verificado - uma
#     alegacao falsa que usa essa frase escapa deste portao (a mesma
#     fronteira que L-36 ja aceita para qualquer prosa nao executavel).
#   - Comentario fora de include/**/*.hpp (src/, tests/, docs/) nunca
#     e varrido - este portao e' sobre a SUPERFICIE PUBLICA, o
#     vocabulario de DOCS-COUNT-VOCAB (numero de contagem, nao o
#     mesmo vocabulario) cobre a prosa versionada em geral.
#
# Usage:
#   check_claim_citations.py --check <repo-root>
#   check_claim_citations.py --selftest

import os
import re
import sys

import citation_grammar
import check_selftest_orphan
import test_name_inventory

SCRIPT_NAME = "check_claim_citations.py"
EXCEPTIONS_RELPATH = os.path.join("tests", "claim_exceptions.txt")
_STATIC_ASSERT_WINDOW_LINES = 60

# --- vocabulario fechado (D15) -------------------------------------------

_EQUALITY_TERMS = (
    "identical",
    "identically",
    "same on both",
    "same on all",
    "both systems",
    "both platforms",
    "all five",
    "every platform",
)
_MEASUREMENT_TERMS = ("measured", "proved", "proven")
_ALL_TERMS = _EQUALITY_TERMS + _MEASUREMENT_TERMS


def _term_pattern(term):
    return re.compile(r"\b" + r"\s+".join(re.escape(word) for word in term.split()) + r"\b", re.IGNORECASE)


_TERM_PATTERNS = tuple((term, _term_pattern(term)) for term in _ALL_TERMS)

_BY_CONSTRUCTION_RE = re.compile(r"\bby\s+construction\b", re.IGNORECASE)
_STATIC_ASSERT_MENTION_RE = re.compile(r"\bstatic_assert\b")
_STATIC_ASSERT_CALL_RE = re.compile(r"\bstatic_assert\s*\(")


def find_claim_terms(spaced_text):
    """Devolve a lista (ordem do vocabulario, D15) dos termos do
    vocabulario fechado que aparecem em `spaced_text` - lista vazia
    quando o bloco nao alega nada."""
    return [term for term, pattern in _TERM_PATTERNS if pattern.search(spaced_text)]


# --- extracao de blocos de comentario -------------------------------------

_LINE_COMMENT_RE = re.compile(r"^\s*//(.*)$")


def extract_comment_blocks(text):
    """Devolve a lista de blocos de comentario de LINHA contiguos (nao
    ha comentario de bloco /* */ em include/**/*.hpp - confirmado por
    `git ls-files -z 'include/*.hpp' | xargs -0 grep -cE '/\\*'`, zero
    ocorrencias em toda a arvore em 24/09/2026; se isso mudar um dia,
    este extrator PRECISA aprender a forma nova, e o piso de varredura
    abaixo e' o que vai denunciar um bloco /* */ sendo ignorado
    silenciosamente, nunca este comentario sozinho).

    Cada bloco carrega DUAS juncoes do mesmo conteudo (ver o
    cabecalho de citation_grammar.py: um identificador longo pode ser
    quebrado pelo reflow do comentario sem nenhum espaco no ponto da
    quebra - so a juncao SEM separador reconstroi esse caso):
      - spaced_text: linhas unidas por um espaco (o caso comum, quebra
        numa fronteira de palavra de verdade).
      - tight_text: linhas concatenadas sem separador nenhum."""
    blocks = []
    current = []

    def flush():
        if not current:
            return
        start_line = current[0][0]
        end_line = current[-1][0]
        spaced_text = " ".join(content.strip() for _, content in current if content.strip())
        # tight_text usa strip() COMPLETO por linha (nunca so' um
        # espaco): uma linha de continuacao de item de lista chega
        # aqui com indentacao (varios espacos) preservada por
        # desenho - essa indentacao e' formatacao do comentario, nao
        # parte da prosa, e sobrevivendo ela quebra a reconstrucao de
        # um identificador partido no meio pelo reflow (achado real,
        # loop.hpp:165-172, "loop_callbacks_type_" + "\n    test.cpp"
        # - sem o strip() completo aqui, o tight_text vira "..type_
        # "    "test.cpp.." com os 4 espacos de indentacao no meio, e
        # a busca por substring falha mesmo com o nome certo presente).
        tight_text = "".join(content.strip() for _, content in current)
        blocks.append(
            {
                "start_line": start_line,
                "end_line": end_line,
                "spaced_text": spaced_text,
                "tight_text": tight_text,
            }
        )
        current.clear()

    for line_no, raw_line in enumerate(text.splitlines(), start=1):
        match = _LINE_COMMENT_RE.match(raw_line)
        if match is None:
            flush()
            continue
        content = match.group(1)
        if content.startswith(" "):
            content = content[1:]
        current.append((line_no, content))
    flush()
    return blocks


# --- resolucao de cada bloco alegado --------------------------------------


def _static_assert_follows(file_lines, block_end_line):
    """True quando um static_assert(...) REAL aparece nas proximas
    _STATIC_ASSERT_WINDOW_LINES linhas do arquivo, apos o fim do
    bloco (indice 1-based `block_end_line`). file_lines e' 0-based
    (splitlines()); block_end_line ja e' a ULTIMA linha do bloco de
    comentario, entao a janela comeca em file_lines[block_end_line]
    (a linha seguinte, indice 0-based == numero de linha 1-based do
    fim do bloco)."""
    window = file_lines[block_end_line : block_end_line + _STATIC_ASSERT_WINDOW_LINES]
    return any(_STATIC_ASSERT_CALL_RE.search(line) for line in window)


def resolve_block(block, file_lines, known_test_names):
    """Devolve um dict {"status": ..., "detail": ...} para um bloco
    que JA contem ao menos um termo do vocabulario fechado (o
    chamador so invoca isto depois de find_claim_terms() nao-vazio).
    status um de: "cited_test", "static_assert", "by_construction",
    "unresolved" (excecao e' resolvida por quem chama, comparando
    contra tests/claim_exceptions.txt - este resolvedor nao conhece
    o arquivo de excecoes)."""
    cited_name = citation_grammar.has_cited_test_reference(
        block["spaced_text"], block["tight_text"], known_test_names
    )
    if cited_name is not None:
        return {"status": "cited_test", "detail": cited_name}
    if _STATIC_ASSERT_MENTION_RE.search(block["spaced_text"]) and _static_assert_follows(
        file_lines, block["end_line"]
    ):
        return {"status": "static_assert", "detail": None}
    if _BY_CONSTRUCTION_RE.search(block["spaced_text"]):
        return {"status": "by_construction", "detail": None}
    return {"status": "unresolved", "detail": None}


# --- tests/claim_exceptions.txt -------------------------------------------


def parse_claim_exceptions_text(text, source_label="tests/claim_exceptions.txt"):
    """Formato: "arquivo | trecho-ancora | item" - um registro por
    linha, "#" comentario, linha em branco ignorada. MESMA forma de
    tests/parity_exceptions.txt (check_test_parity.py's own parse_
    exceptions_text()), tres campos em vez de quatro/cinco porque
    esta excecao nao tem "sistema_onde_falta" nem apelido - so aponta
    para um trecho de comentario e o item que ainda deixa a alegacao
    sem citacao."""
    exceptions = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) != 3:
            fail(
                f"{source_label}: linha malformada (esperava 3 campos separados por "
                f"'|', achou {len(parts)}): {line!r}"
            )
        file_relpath, anchor, item = parts
        if not anchor:
            fail(f"{source_label}: trecho-ancora vazio: {line!r}")
        exceptions.append({"file": file_relpath, "anchor": anchor, "item": item, "used": False})
    return exceptions


def validate_claim_exceptions(exceptions, todo_status):
    """Mesma regra de morte de check_test_parity.py's own validate_
    exceptions(): item que nao existe em TODO.md, ou que ja esta
    CONCLUIDO (✅), invalida a linha - a alegacao ou ja foi resolvida
    (e a excecao deveria ter sumido junto) ou nunca devia ter apontado
    para ali."""
    errors = []
    for exc in exceptions:
        item = exc["item"]
        entry = todo_status.get(item)
        if entry is None:
            errors.append(f"{exc['file']}: excecao cita item {item!r}, que nao existe em TODO.md")
            continue
        if entry["concluded"]:
            errors.append(
                f"{exc['file']}: excecao aponta para o item {item!r}, marcado como CONCLUIDO "
                f"({entry['status_text']}) em TODO.md - a alegacao deveria ter sido resolvida "
                "(citar teste ou reescrever) e esta linha apagada de tests/claim_exceptions.txt"
            )
    return errors


def find_matching_exception(file_relpath, block, exceptions):
    """Devolve a primeira excecao (nao usada ainda) cujo `file` bate
    com `file_relpath` e cujo `anchor` e substring do bloco (spaced ou
    tight - mesma razao das duas juncoes do resolvedor acima: o
    trecho-ancora escrito a mao pode ter sido copiado da prosa
    original, que pode ter sido reflow depois). Marca `used=True` na
    excecao encontrada - quem chama depois soma as NAO usadas como
    mortas (gemeo L-17 do apelido morto de check_test_parity.py)."""
    for exc in exceptions:
        if exc["file"] != file_relpath:
            continue
        if exc["anchor"] in block["spaced_text"] or exc["anchor"] in block["tight_text"]:
            exc["used"] = True
            return exc
    return None


# --- TODO.md status (mesma leitura de check_test_parity.py's own
# parse_todo_status_text(), reusada por importacao para nao duplicar a
# tabela de 12 colunas / indice do Status) --------------------------------


def parse_todo_status_text(text):
    import check_test_parity

    return check_test_parity.parse_todo_status_text(text)


# --- IO -------------------------------------------------------------------


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def read_text_lenient(path):
    try:
        with open(path, "r", encoding="utf-8", errors="surrogateescape") as handle:
            return handle.read()
    except OSError:
        return None


# --- modo real -------------------------------------------------------------


def scan_claims(hpp_paths, hpp_texts, known_test_names, exceptions):
    """Funcao pura sobre texto ja lido - o mesmo caminho de codigo
    roda em real_main() e em --selftest. Devolve (results, counts):
    results e a lista de todo bloco alegado (status "cited_test"/
    "static_assert"/"by_construction"/"excepted"/"unresolved"),
    counts e um dict com a contagem de cada status mais "blocks_
    scanned" (denominador do piso de varredura)."""
    results = []
    blocks_scanned = 0
    for relpath in hpp_paths:
        text = hpp_texts.get(relpath)
        if text is None:
            continue
        file_lines = text.splitlines()
        for block in extract_comment_blocks(text):
            terms = find_claim_terms(block["spaced_text"])
            if not terms:
                continue
            blocks_scanned += 1
            resolved = resolve_block(block, file_lines, known_test_names)
            status = resolved["status"]
            detail = resolved["detail"]
            if status == "unresolved":
                exc = find_matching_exception(relpath, block, exceptions)
                if exc is not None:
                    status = "excepted"
                    detail = exc["item"]
            results.append(
                {
                    "file": relpath,
                    "line": block["start_line"],
                    "terms": terms,
                    "status": status,
                    "detail": detail,
                }
            )
    counts = {"blocks_scanned": blocks_scanned}
    for status in ("cited_test", "static_assert", "by_construction", "excepted", "unresolved"):
        counts[status] = sum(1 for r in results if r["status"] == status)
    return results, counts


def real_main(args):
    if len(args) != 1:
        fail("usage: check_claim_citations.py --check <repo-root>")
    root = args[0]

    known_test_names = test_name_inventory.real_main([root])

    hpp_relpaths, ok = check_selftest_orphan.scanned_paths(root)
    if not ok:
        fail(f"'git ls-files' falhou em '{root}' (nao e repositorio git, ou git indisponivel)")
    hpp_relpaths = sorted(p for p in hpp_relpaths if p.startswith("include/") and p.endswith(".hpp"))
    if not hpp_relpaths:
        fail("varredura vazia: 0 cabecalho(s) .hpp em include/ - GODS_LAWS.md L-40")
    hpp_texts = {p: read_text_lenient(os.path.join(root, p)) for p in hpp_relpaths}

    exceptions_path = os.path.join(root, EXCEPTIONS_RELPATH)
    exceptions_text = read_text_lenient(exceptions_path)
    if exceptions_text is None:
        fail(f"nao foi possivel ler '{exceptions_path}'")
    exceptions = parse_claim_exceptions_text(exceptions_text)

    todo_path = os.path.join(root, "TODO.md")
    todo_text = read_text_lenient(todo_path)
    if todo_text is None:
        fail(f"nao foi possivel ler '{todo_path}'")
    todo_status = parse_todo_status_text(todo_text)

    exc_errors = validate_claim_exceptions(exceptions, todo_status)
    if exc_errors:
        for error in exc_errors:
            print(f"{SCRIPT_NAME}: {error}", file=sys.stderr)
        fail(f"{len(exc_errors)} excecao(oes) invalida(s) em {EXCEPTIONS_RELPATH} (ver acima)")

    results, counts = scan_claims(hpp_relpaths, hpp_texts, known_test_names, exceptions)

    dead_exceptions = [exc for exc in exceptions if not exc["used"]]

    print(
        f"{SCRIPT_NAME}: {len(hpp_relpaths)} cabecalho(s) .hpp varrido(s), "
        f"{counts['blocks_scanned']} bloco(s) de comentario com alegacao do vocabulario fechado"
    )
    print(
        f"{SCRIPT_NAME}: {counts['cited_test']} citada(s) por teste, "
        f"{counts['static_assert']} por static_assert, "
        f"{counts['by_construction']} por construcao (sem teste), "
        f"{counts['excepted']} por excecao, "
        f"{counts['unresolved']} sem citacao"
    )
    print(f"{SCRIPT_NAME}: {len(exceptions)} excecao(oes) em {EXCEPTIONS_RELPATH}, {len(dead_exceptions)} morta(s)")

    if counts["blocks_scanned"] == 0:
        fail(
            "varredura vazia: nenhum bloco de comentario com vocabulario fechado encontrado - "
            "GODS_LAWS.md L-40, isto e sinal de vocabulario/extrator quebrado, nunca 'nenhuma "
            "alegacao existe' (o vocabulario ja apareceu em dezenas de cabecalhos, medido em "
            "24/09/2026)"
        )

    unresolved = [r for r in results if r["status"] == "unresolved"]
    if unresolved:
        for r in unresolved:
            print(
                f"{SCRIPT_NAME}: {r['file']}:{r['line']}: alegacao sem citacao "
                f"(termo(s): {', '.join(r['terms'])})",
                file=sys.stderr,
            )
        fail(f"{len(unresolved)} alegacao(oes) sem citacao (ver acima)")

    if dead_exceptions:
        for exc in dead_exceptions:
            print(
                f"{SCRIPT_NAME}: excecao morta - {exc['file']} | {exc['anchor']!r} | {exc['item']} "
                "nao casou com nenhum bloco alegado real (a alegacao ja foi resolvida de outro jeito, "
                "ou o trecho-ancora nao existe mais)",
                file=sys.stderr,
            )
        fail(f"{len(dead_exceptions)} excecao(oes) morta(s) em {EXCEPTIONS_RELPATH} (ver acima)")

    return results


# --- controles do --selftest -----------------------------------------------

_FIXTURE_HPP_TEXT_UNCITED = (
    "// This value is measured to be identical on every platform, but\n"
    "// nothing here says who measured it.\n"
    "struct thing {};\n"
)

_FIXTURE_HPP_TEXT_CITED = (
    "// This value is measured to be identical on every platform.\n"
    "// Proved by: real_registrado_test (both systems, same result).\n"
    "struct thing {};\n"
)

_FIXTURE_HPP_TEXT_STATIC_ASSERT = (
    "// This aggregate is trivially copyable, measured the same on\n"
    "// every platform via a static_assert right below.\n"
    "static_assert(std::is_trivially_copyable_v<thing>, \"must stay flat\");\n"
)

_FIXTURE_HPP_TEXT_BY_CONSTRUCTION = (
    "// The refusal is identical on every platform by construction,\n"
    "// not by measurement - the common validation layer runs before\n"
    "// any backend sees the request.\n"
    "struct thing {};\n"
)

_FIXTURE_HPP_TEXT_NO_CLAIM = "// A comment with no claim vocabulary at all.\nstruct thing {};\n"


def _selftest_find_claim_terms():
    terms = find_claim_terms("This value is measured to be identical on every platform.")
    if set(terms) != {"measured", "identical", "every platform"}:
        print(f"selftest: TERMOS FALHOU: {terms}", file=sys.stderr)
        return False
    if find_claim_terms("This function never allocates and always returns a value."):
        print("selftest: TERMOS FALHOU ('never'/'always' nao deveriam contar - D15)", file=sys.stderr)
        return False
    print(f"selftest: TERMOS OK: {sorted(set(terms))}")
    return True


def _selftest_extract_comment_blocks():
    text = "// linha um\n// linha dois\n\nint code();\n// bloco separado\n"
    blocks = extract_comment_blocks(text)
    if len(blocks) != 2:
        print(f"selftest: BLOCOS FALHOU (esperava 2 blocos): {blocks}", file=sys.stderr)
        return False
    if blocks[0]["spaced_text"] != "linha um linha dois":
        print(f"selftest: BLOCOS FALHOU (spaced_text errado): {blocks[0]}", file=sys.stderr)
        return False
    print(f"selftest: BLOCOS OK: {[b['spaced_text'] for b in blocks]}")
    return True


# VERMELHO da tabela E3: "measured sem citacao reprova".
def _selftest_uncited_claim_unresolved():
    known = {"real_registrado_test"}
    blocks = extract_comment_blocks(_FIXTURE_HPP_TEXT_UNCITED)
    file_lines = _FIXTURE_HPP_TEXT_UNCITED.splitlines()
    resolved = [resolve_block(b, file_lines, known) for b in blocks if find_claim_terms(b["spaced_text"])]
    if not resolved or any(r["status"] != "unresolved" for r in resolved):
        print(f"selftest: SEM-CITACAO FALHOU (deveria ficar 'unresolved'): {resolved}", file=sys.stderr)
        return False
    print("selftest: SEM-CITACAO OK (alegacao sem nenhuma das tres formas de citacao fica 'unresolved')")
    return True


# VERMELHO da tabela E3: "citacao de teste que nao existe reprova".
def _selftest_cited_but_unknown_test_unresolved():
    known = {"outro_test_qualquer"}
    blocks = extract_comment_blocks(_FIXTURE_HPP_TEXT_CITED)
    file_lines = _FIXTURE_HPP_TEXT_CITED.splitlines()
    resolved = [resolve_block(b, file_lines, known) for b in blocks if find_claim_terms(b["spaced_text"])]
    if not resolved or any(r["status"] != "unresolved" for r in resolved):
        print(f"selftest: TESTE-FANTASMA FALHOU (nome fora do inventario deveria ficar 'unresolved'): {resolved}", file=sys.stderr)
        return False
    print("selftest: TESTE-FANTASMA OK (citacao de teste que nao existe no inventario nunca resolve)")
    return True


def _selftest_cited_real_test_resolved():
    known = {"real_registrado_test"}
    blocks = extract_comment_blocks(_FIXTURE_HPP_TEXT_CITED)
    file_lines = _FIXTURE_HPP_TEXT_CITED.splitlines()
    resolved = [resolve_block(b, file_lines, known) for b in blocks if find_claim_terms(b["spaced_text"])]
    if not resolved or any(r["status"] != "cited_test" or r["detail"] != "real_registrado_test" for r in resolved):
        print(f"selftest: CITACAO-REAL FALHOU: {resolved}", file=sys.stderr)
        return False
    print("selftest: CITACAO-REAL OK (nome real do inventario resolve como 'cited_test')")
    return True


def _selftest_static_assert_nearby_resolved():
    blocks = extract_comment_blocks(_FIXTURE_HPP_TEXT_STATIC_ASSERT)
    file_lines = _FIXTURE_HPP_TEXT_STATIC_ASSERT.splitlines()
    resolved = [resolve_block(b, file_lines, set()) for b in blocks if find_claim_terms(b["spaced_text"])]
    if not resolved or any(r["status"] != "static_assert" for r in resolved):
        print(f"selftest: STATIC-ASSERT FALHOU: {resolved}", file=sys.stderr)
        return False
    print("selftest: STATIC-ASSERT OK ('static_assert' mencionado + static_assert( real perto resolve)")
    return True


# Mutante que mata: aceitar 'static_assert' mencionado SEM um
# static_assert( real por perto (a mencao sozinha, em prosa, sem o
# codigo, nao prova nada).
def _selftest_static_assert_mentioned_without_real_one_unresolved():
    text = "// This is measured the same on every platform via a static_assert.\nstruct thing {};\n"
    blocks = extract_comment_blocks(text)
    file_lines = text.splitlines()
    resolved = [resolve_block(b, file_lines, set()) for b in blocks if find_claim_terms(b["spaced_text"])]
    if not resolved or any(r["status"] != "unresolved" for r in resolved):
        print(f"selftest: STATIC-ASSERT-FANTASMA FALHOU: {resolved}", file=sys.stderr)
        return False
    print("selftest: STATIC-ASSERT-FANTASMA OK ('static_assert' mencionado sem um real por perto nao resolve)")
    return True


def _selftest_by_construction_resolved_and_counted_apart():
    blocks = extract_comment_blocks(_FIXTURE_HPP_TEXT_BY_CONSTRUCTION)
    file_lines = _FIXTURE_HPP_TEXT_BY_CONSTRUCTION.splitlines()
    resolved = [resolve_block(b, file_lines, set()) for b in blocks if find_claim_terms(b["spaced_text"])]
    if not resolved or any(r["status"] != "by_construction" for r in resolved):
        print(f"selftest: POR-CONSTRUCAO FALHOU: {resolved}", file=sys.stderr)
        return False
    print("selftest: POR-CONSTRUCAO OK ('by construction' resolve numa categoria propria)")
    return True


def _selftest_no_claim_vocabulary_no_blocks_flagged():
    blocks = extract_comment_blocks(_FIXTURE_HPP_TEXT_NO_CLAIM)
    flagged = [b for b in blocks if find_claim_terms(b["spaced_text"])]
    if flagged:
        print(f"selftest: SEM-ALEGACAO FALHOU (bloco sem vocabulario foi sinalizado): {flagged}", file=sys.stderr)
        return False
    print("selftest: SEM-ALEGACAO OK (bloco sem vocabulario fechado nunca e sinalizado)")
    return True


# VERMELHO da tabela E3: "exceção apontando para item concluído
# reprova".
def _selftest_exception_pointing_to_concluded_item_rejected():
    todo_status = {
        "ITEM-ABERTO": {"status_text": "🔍 Pendente verificação", "concluded": False},
        "ITEM-FECHADO": {"status_text": "✅ Feito", "concluded": True},
    }
    exceptions = parse_claim_exceptions_text(
        "include/glintfx/x.hpp | trecho de exemplo | ITEM-FECHADO\n"
    )
    errors = validate_claim_exceptions(exceptions, todo_status)
    if not errors:
        print("selftest: EXCECAO-ITEM-CONCLUIDO FALHOU (deveria reprovar)", file=sys.stderr)
        return False
    print(f"selftest: EXCECAO-ITEM-CONCLUIDO OK (excecao para item ✅ reprova): {errors}")
    return True


def _selftest_exception_pointing_to_open_item_accepted():
    todo_status = {"ITEM-ABERTO": {"status_text": "⏳ Pendente", "concluded": False}}
    exceptions = parse_claim_exceptions_text("include/glintfx/x.hpp | trecho de exemplo | ITEM-ABERTO\n")
    errors = validate_claim_exceptions(exceptions, todo_status)
    if errors:
        print(f"selftest: EXCECAO-ITEM-ABERTO FALHOU (nao deveria reprovar): {errors}", file=sys.stderr)
        return False
    print("selftest: EXCECAO-ITEM-ABERTO OK (excecao para item aberto passa)")
    return True


# Gemeo L-17 do apelido morto de check_test_parity.py: excecao cujo
# trecho-ancora nunca casa com bloco alegado real fica "nao usada" e
# tem de reprovar como morta.
def _selftest_dead_exception_detected():
    known = set()
    exceptions = parse_claim_exceptions_text(
        "include/glintfx/gfss/value.hpp | frase que nao existe em lugar nenhum | ITEM-ABERTO\n"
    )
    _, counts = scan_claims(
        ["include/glintfx/gfss/value.hpp"],
        {"include/glintfx/gfss/value.hpp": _FIXTURE_HPP_TEXT_UNCITED},
        known,
        exceptions,
    )
    dead = [exc for exc in exceptions if not exc["used"]]
    if not dead:
        print("selftest: EXCECAO-MORTA FALHOU (deveria ter ficado sem uso)", file=sys.stderr)
        return False
    print(f"selftest: EXCECAO-MORTA OK (excecao cujo trecho-ancora nunca casa fica marcada morta): {counts}")
    return True


def _selftest_matching_exception_resolves_and_is_used():
    known = set()
    exceptions = parse_claim_exceptions_text(
        "include/glintfx/gfss/value.hpp | measured to be identical | ITEM-ABERTO\n"
    )
    results, counts = scan_claims(
        ["include/glintfx/gfss/value.hpp"],
        {"include/glintfx/gfss/value.hpp": _FIXTURE_HPP_TEXT_UNCITED},
        known,
        exceptions,
    )
    if counts["excepted"] != 1 or counts["unresolved"] != 0:
        print(f"selftest: EXCECAO-CASA FALHOU: {counts} / {results}", file=sys.stderr)
        return False
    if not exceptions[0]["used"]:
        print("selftest: EXCECAO-CASA FALHOU (excecao deveria ter sido marcada usada)", file=sys.stderr)
        return False
    print(f"selftest: EXCECAO-CASA OK (trecho-ancora presente no bloco resolve como 'excepted'): {counts}")
    return True


# VERMELHO da tabela E3: "varredura vazia reprova" - zero blocos com
# vocabulario fechado no universo de arquivos varridos.
def _selftest_empty_scan_yields_zero_blocks():
    _, counts = scan_claims(
        ["include/glintfx/x.hpp"],
        {"include/glintfx/x.hpp": _FIXTURE_HPP_TEXT_NO_CLAIM},
        set(),
        [],
    )
    if counts["blocks_scanned"] != 0:
        print(f"selftest: VARREDURA-VAZIA FALHOU (deveria contar zero blocos): {counts}", file=sys.stderr)
        return False
    print("selftest: VARREDURA-VAZIA OK (universo sem vocabulario fechado mede zero blocos - real_main reprova nisso)")
    return True


# MEDIDO, NUNCA SUPOSTO: roda contra a arvore real deste repositorio
# (o mesmo caminho de codigo de real_main), sem as duas reprovacoes
# finais (sem citacao / excecao morta) - so prova que o piso de
# varredura nao esta zerado na arvore real e que tests/claim_
# exceptions.txt tem forma valida. Pula, declarado, se git nao
# funcionar aqui.
def _selftest_measured_scan_on_real_tree():
    here = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(here, "..", ".."))
    hpp_relpaths, ok = check_selftest_orphan.scanned_paths(repo_root)
    if not ok:
        print("selftest: MEDIDO-ARVORE-REAL PULADO (git ls-files indisponivel aqui - declarado, nao calado)")
        return True
    hpp_relpaths = sorted(p for p in hpp_relpaths if p.startswith("include/") and p.endswith(".hpp"))
    if not hpp_relpaths:
        print("selftest: MEDIDO-ARVORE-REAL PULADO (nenhum .hpp em include/ aqui - declarado, nao calado)")
        return True
    hpp_texts = {p: read_text_lenient(os.path.join(repo_root, p)) for p in hpp_relpaths}
    known_test_names, ok2 = _known_names_on_real_tree(repo_root)
    if not ok2:
        print("selftest: MEDIDO-ARVORE-REAL PULADO (inventario de testes indisponivel aqui - declarado, nao calado)")
        return True
    exceptions_text = read_text_lenient(os.path.join(repo_root, EXCEPTIONS_RELPATH))
    exceptions = parse_claim_exceptions_text(exceptions_text) if exceptions_text is not None else []
    results, counts = scan_claims(hpp_relpaths, hpp_texts, known_test_names, exceptions)
    print(
        f"selftest: MEDIDO-ARVORE-REAL: {len(hpp_relpaths)} cabecalho(s), "
        f"{counts['blocks_scanned']} bloco(s) alegado(s), {counts}"
    )
    if counts["blocks_scanned"] == 0:
        print("selftest: MEDIDO-ARVORE-REAL FALHOU (piso de varredura zerado contra a arvore real)", file=sys.stderr)
        return False
    return True


def _known_names_on_real_tree(repo_root):
    cmake_text = read_text_lenient(os.path.join(repo_root, "tests", "CMakeLists.txt"))
    if cmake_text is None:
        return set(), False
    containerfile_text = read_text_lenient(os.path.join(repo_root, "tests", "container", "Containerfile"))
    cpp_paths, ok = check_selftest_orphan.scanned_paths(repo_root)
    if not ok:
        return set(), False
    cpp_paths = [p for p in cpp_paths if p.endswith(".cpp")]
    cpp_texts = {p: read_text_lenient(os.path.join(repo_root, p)) for p in cpp_paths}
    names = test_name_inventory.build_inventory_from_texts(cmake_text, containerfile_text, cpp_texts)
    return names, True


def selftest_main():
    controls = [
        _selftest_find_claim_terms(),
        _selftest_extract_comment_blocks(),
        _selftest_uncited_claim_unresolved(),
        _selftest_cited_but_unknown_test_unresolved(),
        _selftest_cited_real_test_resolved(),
        _selftest_static_assert_nearby_resolved(),
        _selftest_static_assert_mentioned_without_real_one_unresolved(),
        _selftest_by_construction_resolved_and_counted_apart(),
        _selftest_no_claim_vocabulary_no_blocks_flagged(),
        _selftest_exception_pointing_to_concluded_item_rejected(),
        _selftest_exception_pointing_to_open_item_accepted(),
        _selftest_dead_exception_detected(),
        _selftest_matching_exception_resolves_and_is_used(),
        _selftest_empty_scan_yields_zero_blocks(),
        _selftest_measured_scan_on_real_tree(),
    ]
    if not all(controls):
        print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    elif args and args[0] == "--check":
        real_main(args[1:])
    else:
        fail("usage: check_claim_citations.py --check <repo-root>  |  --selftest")


if __name__ == "__main__":
    main()
