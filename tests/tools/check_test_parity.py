#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_test_parity.py - TODO.md item PARITY-GATE (GODS_LAWS.md L-04:
# "O comportamento deve ser igual em qualquer OS ... e cada fatia
# entregue prova isso em cada sistema", ordem do lider de 02/09/2026).
#
# Nasce do retrabalho descrito no proprio item: tres ondas foram
# declaradas fechadas com entrega existindo em UM sistema so, porque
# "fechou" dependia do julgamento do orquestrador em vez de uma
# medida mecanica. Este portao tira essa decisao da mao de quem
# revisa: confronta o inventario de testes REALMENTE registrados
# (`ctest -N`, GODS_LAWS.md L-45 - nunca o que o codigo-fonte
# PRETENDE registrar) de dois sistemas, e reprova todo nome que
# existe de um lado e falta do outro SEM uma linha em
# tests/parity_exceptions.txt cobrindo essa falta especifica.
#
# Tres arquivos de entrada, e so um deles e outro portao verificando
# ESTE portao:
#   - inventario Linux e inventario Windows: uma lista de nomes de
#     teste, um por linha (aceita tanto a saida crua de `ctest -N`
#     quanto uma lista ja limpa - ver parse_inventory_text() abaixo).
#   - tests/parity_exceptions.txt: AUSENCIAS aceitas (o par nao
#     existe no sistema que falta, com um motivo e um item de
#     TODO.md - ou o sentinela SEM-PENDENCIA quando a ausencia e
#     permanente por desenho).
#   - tests/parity_aliases.txt: PRESENCAS sob nome diferente (o par
#     existe nos dois lados, mecanismo diferente, mesmo nome logico
#     nao) - nunca checado contra TODO.md, porque nao ha "concluido
#     sem par" possivel quando o par ja existe dos dois lados.
#   - TODO.md: usado so para ler o Status (coluna 9) da linha cujo ID
#     (coluna 2) uma excecao cita - a REGRA que da nome a este portao:
#     "excecao que aponta para item marcado como concluido reprova,
#     porque e exatamente a regra 'concluido sem par'" (verbatim do
#     CTO, TODO.md). Uma excecao esquecida depois que o item fechou e
#     o sintoma exato que motivou esta fatia.
#
# LIMITACAO DECLARADA (para quem for confiar neste portao ler ANTES de
# confiar, GODS_LAWS.md L-40: um portao vendido como mais forte do que
# e' vira falso conforto): a comparacao real (.github/workflows/ci.yml,
# job `parity`) une TODOS os legs Linux (Fedora/Ubuntu/Arch/CachyOS x
# compartilhado/estatico) num so conjunto antes de comparar contra a
# uniao dos legs Windows - ela compara SISTEMAS, nao distros. Uma
# lacuna exclusiva de UMA UNICA distro Linux (um teste que roda em
# Fedora mas nao em Arch, por exemplo) fica INVISIVEL a este portao,
# escondida dentro da uniao: o nome ainda "existe do lado Linux" porque
# alguma distro o registrou. Este portao nunca provou, e nunca
# prometeu provar, paridade ENTRE distros - so entre Linux e Windows
# como um todo.
#
# Usage:
#   check_test_parity.py --compare <inv-linux> <inv-windows> \
#       <exceptions.txt> <aliases.txt> <TODO.md>
#   check_test_parity.py --selftest
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

import re
import sys

SCRIPT_NAME = "check_test_parity.py"

SEM_PENDENCIA = "SEM-PENDENCIA"
SISTEMAS_VALIDOS = ("linux", "windows")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- parsing -----------------------------------------------------------


# Aceita tanto a saida crua de `ctest -N` ("  Test #12: nome_test",
# mais a linha final "Total Tests: N", que e descartada) quanto uma
# lista ja limpa de nomes, um por linha - o mesmo arquivo pode vir de
# qualquer uma das duas fontes sem o chamador precisar normalizar
# antes.
_CTEST_LINE_RE = re.compile(r"^\s*Test\s+#\d+:\s+(\S+)\s*$")


def parse_inventory_text(text):
    names = set()
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        m = _CTEST_LINE_RE.match(line)
        if m:
            names.add(m.group(1))
            continue
        if line.startswith("Total Tests:"):
            continue
        names.add(line)
    return names


def _strip_pipe_fields(line, expected_fields, source_label):
    parts = [p.strip() for p in line.split("|")]
    if len(parts) != expected_fields:
        fail(
            f"{source_label}: linha malformada (esperava {expected_fields} campos "
            f"separados por '|', achou {len(parts)}): {line!r}"
        )
    return parts


def parse_exceptions_text(text, source_label="tests/parity_exceptions.txt"):
    exceptions = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        test_name, missing_on, gemeo, item = _strip_pipe_fields(line, 4, source_label)
        if missing_on not in SISTEMAS_VALIDOS:
            fail(
                f"{source_label}: sistema_onde_falta invalido {missing_on!r} para "
                f"{test_name!r} (esperado 'linux' ou 'windows')"
            )
        exceptions.append(
            {"test_name": test_name, "missing_on": missing_on, "gemeo": gemeo, "item": item}
        )
    return exceptions


def parse_aliases_text(text, source_label="tests/parity_aliases.txt"):
    aliases = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        linux_name, windows_name = _strip_pipe_fields(line, 2, source_label)
        aliases.append((linux_name, windows_name))
    return aliases


# TODO.md e uma tabela markdown de 10 colunas, uma linha por item,
# sem pipe literal dentro de nenhuma celula (confirmado: zero pipe
# escapado no arquivo) - dividir por "|" e seguro. Cabecalho real:
# "| WSJF | ID | Onda | Grupo | Descricao Tecnica | Prioridade |
# Pre-requisito | Dificuldade | Status | Estado Auditado |" - ID e o
# indice 2, Status o indice 9, apos o split (indice 0 e o vazio antes
# do primeiro "|", indice 11 o vazio depois do ultimo).
_TODO_ROW_RE = re.compile(r"^\|.*\|$")


def parse_todo_status_text(text):
    status_by_item = {}
    for line in text.splitlines():
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


# --- comparison logic ----------------------------------------------------


# Aplica os apelidos: um nome que so aparece de um lado NAO e uma
# lacuna se o PARCEIRO dele (nome sob o outro sistema) existir no
# INVENTARIO INTEIRO do outro lado - deliberadamente checado contra o
# inventario completo, nao contra o conjunto "so daquele lado", para
# nao depender de o parceiro tambem ser exclusivo (um apelido correto
# quase sempre aponta pra um nome exclusivo do outro lado, mas checar
# a inclusao geral e o que torna a logica robusta por construcao, em
# vez de robusta so nos casos ja pensados). Devolve (so_linux,
# so_windows), os dois conjuntos que sobram depois de descontar
# apelidos - candidatos reais a lacuna.
def apply_aliases(linux_only, windows_only, linux_inventory, windows_inventory, aliases):
    linux_to_win = {a: b for a, b in aliases}
    win_to_linux = {b: a for a, b in aliases}

    remaining_linux_only = set()
    for name in linux_only:
        partner = linux_to_win.get(name)
        if partner is not None and partner in windows_inventory:
            # o parceiro existe de fato no lado Windows - par
            # confirmado sob outro nome, nao e lacuna.
            continue
        remaining_linux_only.add(name)

    remaining_windows_only = set()
    for name in windows_only:
        partner = win_to_linux.get(name)
        if partner is not None and partner in linux_inventory:
            continue
        remaining_windows_only.add(name)

    return remaining_linux_only, remaining_windows_only


# A regra que da nome ao portao: uma excecao cujo item aponta para
# uma linha CONCLUIDA em TODO.md e invalida, e reprova - mesmo que
# nenhuma lacuna real esteja usando ela agora. SEM-PENDENCIA e o
# unico item que escapa da checagem contra TODO.md (e so quando
# gemeo == "nenhum" - GODS_LAWS.md L-32 corolario: sentinela e para
# ausencia permanente por desenho, nao para lacuna real disfarcada).
def validate_exceptions(exceptions, todo_status):
    errors = []
    for exc in exceptions:
        item = exc["item"]
        if item == SEM_PENDENCIA:
            if exc["gemeo"] != "nenhum":
                errors.append(
                    f"{exc['test_name']}: excecao com item=SEM-PENDENCIA exige gemeo=nenhum "
                    f"(ausencia permanente nao tem par a documentar) - veio gemeo={exc['gemeo']!r}"
                )
            continue
        entry = todo_status.get(item)
        if entry is None:
            errors.append(
                f"{exc['test_name']}: excecao cita item {item!r}, que nao existe em TODO.md"
            )
            continue
        if entry["concluded"]:
            errors.append(
                f"{exc['test_name']}: excecao aponta para o item {item!r}, marcado como "
                f"CONCLUIDO ({entry['status_text']}) em TODO.md - regra 'concluido sem par' "
                "(GODS_LAWS.md L-04/TODO.md PARITY-GATE): apague esta linha de "
                "tests/parity_exceptions.txt, o par que ela promete ja deveria existir"
            )
    return errors


# O veredicto inteiro, como uma lista de erros - lista vazia significa
# que o portao passa. Reune os tres controles que --selftest exige
# provar em vermelho: piso de varredura vazia (GODS_LAWS.md L-40),
# excecao invalida (validate_exceptions acima), e lacuna sem excecao
# registrada.
def run_comparison(linux_inventory, windows_inventory, exceptions, aliases, todo_status):
    errors = []

    if not linux_inventory:
        errors.append(
            "varredura vazia: o inventario Linux tem 0 testes - GODS_LAWS.md L-40, "
            "isto e sinal de coleta quebrada, nunca de paridade"
        )
    if not windows_inventory:
        errors.append(
            "varredura vazia: o inventario Windows tem 0 testes - GODS_LAWS.md L-40, "
            "isto e sinal de coleta quebrada, nunca de paridade"
        )
    if not linux_inventory or not windows_inventory:
        # Sem os dois lados a comparacao de nomes nao tem sentido -
        # devolve so o(s) erro(s) de varredura vazia, nao um "todo
        # mundo falta do outro lado" espurio.
        return errors

    linux_only = linux_inventory - windows_inventory
    windows_only = windows_inventory - linux_inventory
    linux_only, windows_only = apply_aliases(
        linux_only, windows_only, linux_inventory, windows_inventory, aliases
    )

    errors.extend(validate_exceptions(exceptions, todo_status))

    exception_index = {(exc["test_name"], exc["missing_on"]) for exc in exceptions}

    for name in sorted(linux_only):
        if (name, "windows") not in exception_index:
            errors.append(
                f"{name}: existe no inventario Linux e falta no Windows, sem excecao "
                "registrada em tests/parity_exceptions.txt"
            )
    for name in sorted(windows_only):
        if (name, "linux") not in exception_index:
            errors.append(
                f"{name}: existe no inventario Windows e falta no Linux, sem excecao "
                "registrada em tests/parity_exceptions.txt"
            )

    return errors


# --- real mode -------------------------------------------------------


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


def real_main(args):
    if len(args) != 5:
        fail(
            "usage: check_test_parity.py --compare <inv-linux> <inv-windows> "
            "<exceptions.txt> <aliases.txt> <TODO.md>"
        )
    linux_inv_path, windows_inv_path, exceptions_path, aliases_path, todo_path = args

    linux_inventory = parse_inventory_text(_read_file(linux_inv_path))
    windows_inventory = parse_inventory_text(_read_file(windows_inv_path))
    exceptions = parse_exceptions_text(_read_file(exceptions_path))
    aliases = parse_aliases_text(_read_file(aliases_path))
    todo_status = parse_todo_status_text(_read_file(todo_path))

    print(
        f"{SCRIPT_NAME}: inventario Linux={len(linux_inventory)} teste(s), "
        f"Windows={len(windows_inventory)} teste(s), "
        f"{len(exceptions)} excecao(oes), {len(aliases)} apelido(s), "
        f"{len(todo_status)} item(ns) lido(s) de TODO.md"
    )

    errors = run_comparison(linux_inventory, windows_inventory, exceptions, aliases, todo_status)
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: paridade OK - nenhuma lacuna sem excecao registrada")


# --- fixtures and controls for --selftest -----------------------------


def _todo_fixture(item_id, status_text):
    return (
        "| WSJF | ID | Onda | Grupo | Descricao Tecnica | Prioridade | "
        "Pre-requisito | Dificuldade | Status | Estado Auditado |\n"
        f"| (pontuar) | {item_id} | W1 | Fundacao | descricao ficticia de selftest | "
        f"Alta | — | Media | {status_text} | — |\n"
    )


# Controle POSITIVO: inventarios identicos, sem excecao nenhuma
# precisa. Esperado: passa, zero erro.
def selftest_positive_control():
    linux_inv = {"a_test", "b_test"}
    windows_inv = {"a_test", "b_test"}
    errors = run_comparison(linux_inv, windows_inv, [], [], {})
    if errors:
        print(f"selftest: controle POSITIVO FALHOU (esperava zero erros, veio {errors})", file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (inventarios identicos, zero erro)")
    return True


# VERMELHO #1 (o gatilho central do portao): um teste existe no
# Linux, falta no Windows, e NENHUMA excecao cobre isso. Esperado:
# reprova, citando o nome do teste.
def selftest_unregistered_gap_reproves():
    linux_inv = {"a_test", "somente_linux_test"}
    windows_inv = {"a_test"}
    errors = run_comparison(linux_inv, windows_inv, [], [], {})
    if not errors:
        print("selftest: VERMELHO#1 FALHOU (lacuna sem excecao deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("somente_linux_test" in e for e in errors):
        print(f"selftest: VERMELHO#1 FALHOU (reprovou, mas nao citou o teste): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#1 OK (lacuna sem excecao pega): {errors}")
    return True


# VERMELHO #2 (a regra que da nome ao portao): uma excecao aponta
# para um item marcado CONCLUIDO em TODO.md. Esperado: reprova, mesmo
# que a excecao "resolvesse" uma lacuna real presente nos
# inventarios.
def selftest_exception_pointing_to_concluded_item_reproves():
    linux_inv = {"a_test", "so_linux_test"}
    windows_inv = {"a_test"}
    exceptions = [
        {"test_name": "so_linux_test", "missing_on": "windows", "gemeo": "algum_gemeo", "item": "FAKE-CONCLUIDO"}
    ]
    todo_status = parse_todo_status_text(_todo_fixture("FAKE-CONCLUIDO", "✅ Concluído"))
    errors = run_comparison(linux_inv, windows_inv, exceptions, [], todo_status)
    if not errors:
        print("selftest: VERMELHO#2 FALHOU (excecao para item concluido deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("CONCLUIDO" in e or "FAKE-CONCLUIDO" in e for e in errors):
        print(f"selftest: VERMELHO#2 FALHOU (reprovou, mas nao citou o item concluido): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#2 OK (excecao para item concluido pega): {errors}")
    return True


# Controle: a MESMA excecao do vermelho #2, mas contra um item ainda
# Pendente. Esperado: passa - excecao legitima, lacuna coberta.
def selftest_exception_pointing_to_pending_item_passes():
    linux_inv = {"a_test", "so_linux_test"}
    windows_inv = {"a_test"}
    exceptions = [
        {"test_name": "so_linux_test", "missing_on": "windows", "gemeo": "algum_gemeo", "item": "FAKE-PENDENTE"}
    ]
    todo_status = parse_todo_status_text(_todo_fixture("FAKE-PENDENTE", "⏳ Pendente"))
    errors = run_comparison(linux_inv, windows_inv, exceptions, [], todo_status)
    if errors:
        print(f"selftest: controle ITEM-PENDENTE FALHOU (excecao para item pendente deveria ter passado): {errors}", file=sys.stderr)
        return False
    print("selftest: controle ITEM-PENDENTE OK (excecao para item ainda pendente aceita)")
    return True


# VERMELHO #3 (piso de varredura nao-vazia, GODS_LAWS.md L-40):
# inventario vazio de um lado. Esperado: reprova, mensagem cita
# "varredura vazia".
def selftest_empty_inventory_reproves():
    errors = run_comparison(set(), {"a_test"}, [], [], {})
    if not errors:
        print("selftest: VERMELHO#3 FALHOU (inventario Linux vazio deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("varredura vazia" in e for e in errors):
        print(f"selftest: VERMELHO#3 FALHOU (reprovou, mas sem 'varredura vazia'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#3 OK (inventario vazio pego): {errors}")
    return True


# Controle de APELIDO: dois nomes diferentes, um por sistema,
# declarados equivalentes - nao deve virar lacuna, mesmo sem nenhuma
# excecao.
def selftest_alias_control():
    linux_inv = {"a_test", "display_connect_failure_test"}
    windows_inv = {"a_test", "win32_display_connect_test"}
    aliases = [("display_connect_failure_test", "win32_display_connect_test")]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if errors:
        print(f"selftest: controle APELIDO FALHOU (par com apelido nao deveria reprovar): {errors}", file=sys.stderr)
        return False
    print("selftest: controle APELIDO OK (par sob nomes diferentes nao conta como lacuna)")
    return True


# Controle SEM-PENDENCIA: excecao para ausencia permanente por
# desenho, sem item de TODO.md - deve passar, e NAO deve ser checada
# contra TODO.md (o dicionario de status fica vazio de proposito).
def selftest_sem_pendencia_control():
    linux_inv = {"a_test"}
    windows_inv = {"a_test", "so_windows_probe_test"}
    exceptions = [
        {"test_name": "so_windows_probe_test", "missing_on": "linux", "gemeo": "nenhum", "item": SEM_PENDENCIA}
    ]
    errors = run_comparison(linux_inv, windows_inv, exceptions, [], {})
    if errors:
        print(f"selftest: controle SEM-PENDENCIA FALHOU (deveria ter passado): {errors}", file=sys.stderr)
        return False
    print("selftest: controle SEM-PENDENCIA OK (ausencia permanente por desenho aceita sem item)")
    return True


# Controle: SEM-PENDENCIA com gemeo preenchido (diferente de
# "nenhum") e uma excecao malformada - "nenhum item cobre isto" e
# "existe um gemeo" sao afirmacoes incompativeis. Esperado: reprova.
def selftest_sem_pendencia_with_gemeo_reproves():
    exceptions = [
        {"test_name": "algum_teste", "missing_on": "linux", "gemeo": "outro_teste", "item": SEM_PENDENCIA}
    ]
    errors = validate_exceptions(exceptions, {})
    if not errors:
        print("selftest: controle SEM-PENDENCIA+GEMEO FALHOU (deveria ter reprovado)", file=sys.stderr)
        return False
    print(f"selftest: controle SEM-PENDENCIA+GEMEO OK (inconsistencia pega): {errors}")
    return True


# Controle: excecao citando item que nao existe em TODO.md nenhum -
# nunca deve passar em silencio (evita excecao com ID digitado
# errado escapando pela mesma porta de SEM-PENDENCIA).
def selftest_unknown_item_reproves():
    exceptions = [
        {"test_name": "algum_teste", "missing_on": "linux", "gemeo": "outro_teste", "item": "ITEM-QUE-NAO-EXISTE"}
    ]
    errors = validate_exceptions(exceptions, {})
    if not errors:
        print("selftest: controle ITEM-DESCONHECIDO FALHOU (deveria ter reprovado)", file=sys.stderr)
        return False
    print(f"selftest: controle ITEM-DESCONHECIDO OK (item inexistente pego): {errors}")
    return True


# Prova de parsing: exercita parse_exceptions_text/parse_aliases_text/
# parse_todo_status_text/parse_inventory_text contra texto real, nao
# so as funcoes internas com dict ja pronto.
def selftest_parsing_round_trip():
    exceptions_text = (
        "# comentario\n"
        "\n"
        "meu_teste|windows|outro_teste|ITEM-1\n"
        "outro|linux|nenhum|SEM-PENDENCIA\n"
    )
    exceptions = parse_exceptions_text(exceptions_text)
    if exceptions != [
        {"test_name": "meu_teste", "missing_on": "windows", "gemeo": "outro_teste", "item": "ITEM-1"},
        {"test_name": "outro", "missing_on": "linux", "gemeo": "nenhum", "item": SEM_PENDENCIA},
    ]:
        print(f"selftest: PARSING FALHOU (exceptions): {exceptions}", file=sys.stderr)
        return False

    aliases_text = "# comentario\na_linux|a_windows\n"
    aliases = parse_aliases_text(aliases_text)
    if aliases != [("a_linux", "a_windows")]:
        print(f"selftest: PARSING FALHOU (aliases): {aliases}", file=sys.stderr)
        return False

    todo_text = _todo_fixture("ITEM-1", "✅ Concluído")
    status = parse_todo_status_text(todo_text)
    if "ITEM-1" not in status or not status["ITEM-1"]["concluded"]:
        print(f"selftest: PARSING FALHOU (todo status): {status}", file=sys.stderr)
        return False

    ctest_text = "  Test #1: foo_test\n  Test #2: bar_test\nTotal Tests: 2\n"
    inv = parse_inventory_text(ctest_text)
    if inv != {"foo_test", "bar_test"}:
        print(f"selftest: PARSING FALHOU (inventory): {inv}", file=sys.stderr)
        return False

    print("selftest: PARSING OK (excecoes, apelidos, status TODO.md e inventario ctest)")
    return True


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_unregistered_gap_reproves(),
        selftest_exception_pointing_to_concluded_item_reproves(),
        selftest_exception_pointing_to_pending_item_passes(),
        selftest_empty_inventory_reproves(),
        selftest_alias_control(),
        selftest_sem_pendencia_control(),
        selftest_sem_pendencia_with_gemeo_reproves(),
        selftest_unknown_item_reproves(),
        selftest_parsing_round_trip(),
    ]
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
    else:
        fail("usage: check_test_parity.py --compare <inv-linux> <inv-windows> <exceptions.txt> <aliases.txt> <TODO.md>  |  --selftest")


if __name__ == "__main__":
    main()
