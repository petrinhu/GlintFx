#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# cmake_lexer.py - CLAIM-CITATIONS sub-fatia E0 (docs/plano-w7c-adendo-
# revalidacao.md sec. 3.E, decisao D-A8). Um analisador lexico de CMake
# so, consumido por test_name_inventory.py (E1) e pelos tres leitores
# que ate hoje cortavam comentario CMake cada um com o proprio regex,
# cada um errando um pedaco diferente da gramatica real:
#
#   - check_selftest_orphan.py's own extract_add_test_blocks(): nao
#     pulava comentario NENHUM - "add_test(" dentro de um comentario
#     de bloco (#[[ ... ]]) virava bloco de verdade. Direcao do erro:
#     VERDE FALSO (uma ferramenta orfa passa por registrada porque o
#     texto do "registro" mora dentro de um comentario).
#   - check_win32_test_link.py's own _strip_cmake_comments_from_text():
#     cortava no primeiro '#' da linha, sem ver aspas nem colchete.
#     Direcao do erro: VERMELHO FALSO (um '#' dentro de uma string
#     entre aspas trunca a linha antes do fim real do argumento).
#   - check_facade_export_boundary.py's own strip_cmake_comments():
#     mesmo corte ingenuo, mesma classe de erro, mais um terceiro
#     modo: um comentario de colchete que se estende por VARIAS linhas
#     sem nenhum '#' nas linhas seguintes sobrevive inteiro ao corte
#     ingenuo (que so' olha o primeiro '#' de CADA linha), entao uma
#     linha de PROSA dentro do comentario que comece por
#     "glintfx_add_test(" vira alvo fantasma.
#
# A regra de tres (GODS_LAWS.md L-33) esta cumprida por MEDICAO, nao
# por previsao: tres leitores, tres pontos cegos diferentes da mesma
# gramatica.
#
# GRAMATICA (cmake-language(7), lida em 24/09/2026 - GODS_LAWS.md
# L-43/L-44 do projeto - https://cmake.org/cmake/help/latest/manual/
# cmake-language.7.html):
#
#   - Um '#' que NAO e' imediatamente seguido por um bracket_open forma
#     um "line comment": vai ate o fim da linha (a propria quebra de
#     linha nao faz parte do comentario).
#   - Um '#' imediatamente seguido por um bracket_open forma um
#     "bracket comment": #[[...]], #[=[...]=], ..., com o MESMO numero
#     de '=' na abertura e no fechamento. "No evaluation of the
#     enclosed content... is performed" - nada dentro e' comentario,
#     string ou codigo, e' conteudo opaco ate' o fechamento exato.
#   - Um bracket_argument (sem o '#' na frente - [[...]], [=[...]=]) e'
#     um ARGUMENTO literal, nao um comentario, mas segue a MESMA regra
#     de casamento de '='; nada dentro dele e' comentario nem string.
#   - Um quoted_argument ("...") aceita sequencia de escape (\", \\,
#     \n, etc. - qualquer '\' seguido de um caractere e' consumido como
#     par, nunca interpretado por si) e continuacao de linha por barra
#     invertida no fim da linha; um '#' dentro de um quoted_argument
#     NUNCA inicia comentario.
#   - Um comentario comeca com um '#' que NAO esta dentro de um
#     Bracket Argument, Quoted Argument, nem escapado por '\'.
#
# O QUE ESTE MODULO NAO VE (GODS_LAWS.md L-40 aplicado ao proximo
# leitor, nao so' ao lider): variaveis (${...}) nunca sao expandidas -
# a remocao de comentario nao depende disso. Um bracket_open ([, [=[,
# ...) fora de qualquer posicao de argumento (ex.: dentro de um
# Unquoted Argument que por acaso comeca com '[') e' tratado como
# bracket_argument de qualquer forma - o manual define a sintaxe assim
# em qualquer posicao, e este lexer nao valida se aquele '[' esta de
# fato numa posicao onde um argumento e' esperado. Nenhum arquivo real
# deste repositorio (tests/CMakeLists.txt, src/**/CMakeLists.txt) tem
# colchete cru fora de comentario/argumento (confirmado por
# `grep -n '\[' tests/CMakeLists.txt` antes de escrever este modulo -
# toda ocorrencia e' `${...}` ou `[[...]]`/`#[[...]]`).
#
# Usage:
#   cmake_lexer.py --selftest

import sys

SCRIPT_NAME = "cmake_lexer.py"


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- bracket open/close (bracket comment E bracket argument, mesma
# regra de casamento de '=' para os dois) -------------------------------


def _bracket_open(text, pos):
    """Se text[pos] comeca um bracket_open ('[' + N '=' + '['),
    devolve (N, indice_do_conteudo); caso contrario, None. N e' o
    numero de '=' entre os dois colchetes - tem que casar exatamente
    no fechamento (mutante que mata: um fechamento com numero
    diferente de '=' nao pode ser aceito)."""
    n = len(text)
    if pos >= n or text[pos] != "[":
        return None
    j = pos + 1
    while j < n and text[j] == "=":
        j += 1
    if j < n and text[j] == "[":
        return (j - pos - 1, j + 1)
    return None


def _bracket_close_end(text, content_start, equals_count):
    """Indice logo APOS o fechamento ']' + N '=' + ']' que casa
    equals_count, procurando a partir de content_start. Se nao achar
    (bracket nao fechado), consome ate o fim do texto - nunca lanca,
    nunca trava: um CMakeLists.txt real sempre fecha o que abre, e um
    fragmento sintetico de teste sem fechamento e' um caso degenerado,
    nao um crash."""
    closer = "]" + ("=" * equals_count) + "]"
    idx = text.find(closer, content_start)
    if idx == -1:
        return len(text)
    return idx + len(closer)


# --- API principal -------------------------------------------------------


def _consume_quoted_argument(text, start):
    """text[start] == '"'. Devolve o indice logo apos a aspa de
    fechamento NAO-escapada (o quoted_argument inteiro e' copiado
    verbatim pelo chamador - ver strip_comments()). Um '\\' escapa
    QUALQUER proximo caractere como par (cobre \\" e a continuacao de
    linha \\<newline> pela mesma regra: o par nunca fecha a aspa nem
    inicia comentario)."""
    n = len(text)
    j = start + 1
    while j < n:
        c = text[j]
        if c == "\\" and j + 1 < n:
            j += 2
            continue
        j += 1
        if c == '"':
            break
    return j


def _consume_line_comment(text, start):
    """text[start] == '#', ja' confirmado que NAO abre um bracket
    comment. Devolve o indice do fim da linha (a propria quebra de
    linha nunca faz parte do comentario, e sobra para o chamador)."""
    n = len(text)
    j = start
    while j < n and text[j] != "\n":
        j += 1
    return j


def _consume_hash(text, i):
    """text[i] == '#'. Devolve (texto_a_acrescentar, novo_indice):
    bracket comment (todo o conteudo some, so' a contagem de '\n'
    fica, para preservar numero de linha) ou line comment (some ate' o
    fim da linha, sem acrescentar nada)."""
    bracket = _bracket_open(text, i + 1)
    if bracket is None:
        return "", _consume_line_comment(text, i)
    equals_count, content_start = bracket
    end = _bracket_close_end(text, content_start, equals_count)
    removed = text[i:end]
    return "\n" * removed.count("\n"), end


def _consume_one(text, i):
    """Um passo do lexer a partir de text[i]. Devolve (texto_a_
    acrescentar_na_saida, novo_indice) - a peca que strip_comments()
    repete ate' o fim do texto. Quatro casos, na ordem que a gramatica
    exige (aspas e colchete de argumento fecham SEM olhar '#' por
    dentro; so' depois disso um '#' e' avaliado como possivel
    comentario; escape de '\\' fora de aspas e' o ultimo, mais raro)."""
    ch = text[i]

    if ch == '"':
        end = _consume_quoted_argument(text, i)
        return text[i:end], end  # quoted_argument: copiado verbatim

    if ch == "[":
        bracket = _bracket_open(text, i)
        if bracket is not None:
            equals_count, content_start = bracket
            end = _bracket_close_end(text, content_start, equals_count)
            return text[i:end], end  # bracket ARGUMENT: conteudo real, copia inteira

    if ch == "#":
        return _consume_hash(text, i)

    if ch == "\\" and i + 1 < len(text):
        # '#' escapado fora de aspas (\#) nunca inicia comentario; o
        # par e' copiado verbatim, sem interpretacao.
        return text[i : i + 2], i + 2

    return ch, i + 1


def strip_comments(text):
    """Devolve `text` com todo comentario (de linha e de colchete)
    removido, preservando o numero de linha de tudo que sobra (um
    comentario de colchete multi-linha e' substituido pelo mesmo
    numero de '\n' que continha, nunca colapsado numa linha so').
    Argumento entre aspas e' copiado verbatim (nenhum '#' dentro dele
    e' tratado como comentario, nem seu conteudo e' varrido por
    bracket_open). Argumento de colchete (sem o '#' na frente) tambem
    e' copiado verbatim, pela mesma razao.

    GODS_LAWS.md L-40: texto vazio e' varredura vazia - reprova alto,
    nunca devolve string vazia calada."""
    if not text:
        fail("strip_comments: texto vazio - GODS_LAWS.md L-40 (varredura vazia)")

    out = []
    i = 0
    n = len(text)
    while i < n:
        appended, i = _consume_one(text, i)
        out.append(appended)
    return "".join(out)


# --- controles do --selftest ---------------------------------------------


def _selftest_line_comment_stripped():
    text = "add_test(NAME real_test COMMAND real_tool) # comentario real\n"
    got = strip_comments(text)
    if "comentario real" in got:
        print(f"selftest: LINE-COMMENT FALHOU (sobrou texto de comentario): {got!r}", file=sys.stderr)
        return False
    if "add_test(NAME real_test COMMAND real_tool)" not in got:
        print(f"selftest: LINE-COMMENT FALHOU (codigo real sumiu): {got!r}", file=sys.stderr)
        return False
    print("selftest: LINE-COMMENT OK (comentario de linha removido, codigo intacto)")
    return True


# VERMELHO da tabela do adendo: "#[[ add_test(NAME fantasma) ]]" e' um
# comentario de BLOCO inteiro - nada dentro dele pode sobreviver.
# Mutante que mata: tratar "#[[" como comentario de LINHA (so' cortaria
# ate' o fim daquela linha, deixando "add_test(NAME fantasma) ]]" numa
# linha seguinte como se fosse codigo real).
def _selftest_bracket_comment_hides_fake_add_test():
    text = (
        "add_test(NAME real_test COMMAND real_tool)\n"
        "#[[\n"
        "add_test(NAME fantasma COMMAND nada)\n"
        "]]\n"
        "add_test(NAME outro_real COMMAND outro_tool)\n"
    )
    got = strip_comments(text)
    if "fantasma" in got:
        print(f"selftest: BRACKET-COMMENT FALHOU (add_test fantasma sobreviveu): {got!r}", file=sys.stderr)
        return False
    for needle in ("real_test", "outro_real"):
        if needle not in got:
            print(f"selftest: BRACKET-COMMENT FALHOU (codigo real '{needle}' sumiu): {got!r}", file=sys.stderr)
            return False
    if got.count("\n") != text.count("\n"):
        print(
            f"selftest: BRACKET-COMMENT FALHOU (numero de linha nao preservado: "
            f"antes {text.count(chr(10))}, depois {got.count(chr(10))})",
            file=sys.stderr,
        )
        return False
    print("selftest: BRACKET-COMMENT OK (add_test fantasma dentro de #[[ ]] removido, numero de linha preservado)")
    return True


# VERMELHO da tabela: "#[=[ ... ]=]" contendo "]]" - o fechamento tem
# que casar o MESMO numero de '=' (aqui, um), nunca parar no primeiro
# "]]" que aparecer dentro. Mutante que mata: ignorar o numero de '='
# no fechamento (aceitar qualquer "]]" como fim).
def _selftest_bracket_comment_equals_count_matched():
    text = (
        "#[=[\n"
        "isto contem ]] no meio, mas nao fecha aqui\n"
        "add_test(NAME fantasma_dois COMMAND nada)\n"
        "]=]\n"
        "add_test(NAME real_depois COMMAND tool)\n"
    )
    got = strip_comments(text)
    if "fantasma_dois" in got:
        print(f"selftest: EQUALS-COUNT FALHOU (parou no ']]' interno em vez de ']=]'): {got!r}", file=sys.stderr)
        return False
    if "real_depois" not in got:
        print(f"selftest: EQUALS-COUNT FALHOU (codigo real depois do fechamento sumiu): {got!r}", file=sys.stderr)
        return False
    print("selftest: EQUALS-COUNT OK (fechamento so' casa com o mesmo numero de '=', ']]' interno ignorado)")
    return True


# VERMELHO da tabela: um '#' DENTRO de aspas nunca e' comentario.
# Mutante que mata: tratar '#' dentro de aspas como comentario (cortar
# a string no meio, perdendo o fechamento dela e o que vem depois).
def _selftest_hash_inside_quotes_never_a_comment():
    text = 'message("glintfx_add_test(x) # nao e comentario")\nadd_test(NAME depois COMMAND tool)\n'
    got = strip_comments(text)
    if '# nao e comentario' not in got:
        print(f"selftest: HASH-EM-ASPAS FALHOU ('#' dentro de aspas foi tratado como comentario): {got!r}", file=sys.stderr)
        return False
    if 'message("glintfx_add_test(x) # nao e comentario")' not in got:
        print(f"selftest: HASH-EM-ASPAS FALHOU (string nao sobreviveu inteira): {got!r}", file=sys.stderr)
        return False
    if "depois" not in got:
        print(f"selftest: HASH-EM-ASPAS FALHOU (codigo apos a string sumiu): {got!r}", file=sys.stderr)
        return False
    print("selftest: HASH-EM-ASPAS OK ('#' dentro de aspas preservado como dado, nunca comentario)")
    return True


# VERMELHO da tabela: aspas com \" (aspa escapada, nao fecha a string)
# e continuacao de linha por barra invertida (a string continua na
# linha seguinte, e um '#' real so' depois do fechamento vira
# comentario de verdade).
def _selftest_quoted_escape_and_continuation():
    text = (
        'set(X "valor com \\"aspa\\" no meio") # comentario depois\n'
        'set(Y "linha um \\\n'
        'linha dois") # este tambem e comentario\n'
    )
    got = strip_comments(text)
    if 'comentario depois' in got or 'este tambem e comentario' in got:
        print(f"selftest: ASPA-ESCAPADA FALHOU (comentario real nao foi removido): {got!r}", file=sys.stderr)
        return False
    if '\\"aspa\\"' not in got:
        print(f"selftest: ASPA-ESCAPADA FALHOU (aspa escapada nao sobreviveu, string fechou cedo demais): {got!r}", file=sys.stderr)
        return False
    if 'linha dois")' not in got:
        print(f"selftest: ASPA-ESCAPADA FALHOU (continuacao de linha nao preservou o fechamento real): {got!r}", file=sys.stderr)
        return False
    if got.count("\n") != text.count("\n"):
        print(
            f"selftest: ASPA-ESCAPADA FALHOU (numero de linha nao preservado: "
            f"antes {text.count(chr(10))}, depois {got.count(chr(10))})",
            file=sys.stderr,
        )
        return False
    print("selftest: ASPA-ESCAPADA OK (aspa escapada e continuacao de linha nao fecham a string cedo, comentario real depois removido)")
    return True


def _selftest_bracket_argument_not_a_comment():
    # bracket_argument (sem '#' na frente) e' ARGUMENTO, nao
    # comentario - o conteudo dele (mesmo parecendo um '#') fica.
    text = "set(Z [[texto com # dentro, nao e comentario]])\n"
    got = strip_comments(text)
    if "texto com # dentro, nao e comentario" not in got:
        print(f"selftest: BRACKET-ARGUMENT FALHOU (conteudo do argumento de colchete sumiu): {got!r}", file=sys.stderr)
        return False
    print("selftest: BRACKET-ARGUMENT OK ([[ ]] sem '#' na frente e' argumento, conteudo preservado)")
    return True


def _selftest_empty_text_reproves():
    try:
        strip_comments("")
    except SystemExit as exc:
        if exc.code == 1:
            print("selftest: TEXTO-VAZIO OK (strip_comments reprova texto vazio, GODS_LAWS.md L-40)")
            return True
        print(f"selftest: TEXTO-VAZIO FALHOU (codigo de saida errado: {exc.code})", file=sys.stderr)
        return False
    print("selftest: TEXTO-VAZIO FALHOU (deveria ter reprovado, devolveu normalmente)", file=sys.stderr)
    return False


def selftest_main():
    controls = [
        _selftest_line_comment_stripped(),
        _selftest_bracket_comment_hides_fake_add_test(),
        _selftest_bracket_comment_equals_count_matched(),
        _selftest_hash_inside_quotes_never_a_comment(),
        _selftest_quoted_escape_and_continuation(),
        _selftest_bracket_argument_not_a_comment(),
        _selftest_empty_text_reproves(),
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
        fail("usage: cmake_lexer.py --selftest")


if __name__ == "__main__":
    main()
