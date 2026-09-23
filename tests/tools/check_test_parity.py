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
#     permanente por desenho). Um quinto campo opcional,
#     "PROVA-PARCIAL=<nome_do_teste_gemeo>", declara que o gemeo
#     citado no quarto campo (gemeo) EXISTE como ctest de verdade do
#     lado que falta, mas so prova uma parte (ex.: compila limpo, nao
#     executa a mutacao real) - o nome apos "PROVA-PARCIAL=" e
#     conferido por maquina contra o inventario do sistema declarado
#     em missing_on, nunca aceito de leitura humana (PARITY-ALIAS-
#     HYGIENE, D10, 23/09/2026).
#   - tests/parity_aliases.txt: PRESENCAS sob nome diferente (o par
#     existe nos dois lados, mecanismo diferente, mesmo nome logico
#     nao) - nunca checado contra TODO.md, porque nao ha "concluido
#     sem par" possivel quando o par ja existe dos dois lados. Um
#     terceiro campo opcional, "bilateral=<motivo>", declara que o
#     nome do lado "exclusivo" do par TAMBEM roda no outro sistema -
#     sem essa declaracao o portao reprova (PARITY-ALIAS-HYGIENE, D8,
#     23/09/2026: "reprovar salvo declaracao", nunca so avisar).
#   - TODO.md: usado so para ler o Status (coluna 9) da linha cujo ID
#     (coluna 2) uma excecao cita - a REGRA que da nome a este portao:
#     "excecao que aponta para item marcado como concluido reprova,
#     porque e exatamente a regra 'concluido sem par'" (verbatim do
#     CTO, TODO.md). Uma excecao esquecida depois que o item fechou e
#     o sintoma exato que motivou esta fatia.
#
# HIGIENE DA TABELA DE APELIDOS E DE EXCECOES (PARITY-ALIAS-HYGIENE,
# TODO.md W7-C, 23/09/2026): as duas tabelas acima eram checadas so
# quanto USO (um apelido/excecao cobre uma lacuna real hoje), nunca
# quanto a VERDADE DO PROPRIO REGISTRO - um apelido cujo nome nao
# existe em inventario nenhum (teste renomeado ou apagado, ninguem
# limpou a linha) passava calado, e o mesmo valia para uma excecao
# cujo par ja fechou ou cujo teste sumiu dos dois lados. GODS_LAWS.md
# L-40 exige o piso de varredura: toda execucao real imprime "N
# apelidos, M mortos, K bilaterais declarados, J bilaterais sem
# declaracao" e "N excecoes, M mortas", e reprova em M>0 (apelido
# morto), J>0 (bilateral sem declaracao) e M>0 (excecao morta) -
# gemeo direto da GODS_LAWS.md L-17 (o mesmo defeito, o mesmo remedio,
# nos dois lados da tabela).
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

# ENCODING-WIN (05/09/2026, GODS_LAWS.md L-40): TODO.md's own status
# column carries markers outside the Basic Latin/Latin-1 range (`✅`
# U+2705, `⏳` U+23F3) - the two symbols validate_exceptions() above
# embeds verbatim in a reproving message when an exception points at a
# CONCLUIDO item. Python's default stdout/stderr encoding is a FACT OF
# THE MACHINE, not of this script: on Linux/CI it is UTF-8, but the
# GitHub Actions Windows runner's console defaults to the legacy
# code page (cp1252), which has no slot for either symbol - MEDIDO ao
# vivo, run 33986752839, job "Windows - compartilhado": `print()`
# crashed with `UnicodeEncodeError: 'charmap' codec can't encode
# character '✅'` the moment a reproving message carrying `entry
# ['status_text']` reached stdout, both in --selftest (a fixture with
# the same fixture text real_main() also handles) and, unfixed, in the
# real `--compare` path too. Force UTF-8 here rather than downstream at
# every print call: `errors="backslashreplace"` is the fail-safe for
# whatever THIRD symbol TODO.md's status column grows next.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")

SCRIPT_NAME = "check_test_parity.py"

SEM_PENDENCIA = "SEM-PENDENCIA"
SISTEMAS_VALIDOS = ("linux", "windows")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# --- parsing -----------------------------------------------------------


# Aceita tanto a saida crua de `ctest -N` ("  Test #12: nome_test",
# mais a linha final "Total Tests: N", que e descartada) quanto uma
# lista ja limpa de nomes, um por linha - o mesmo ARQUIVO, e ate a
# mesma UNIAO de arquivos concatenados (ver o corolario abaixo), pode
# misturar as duas sem o chamador precisar normalizar antes.
_CTEST_LINE_RE = re.compile(r"^\s*Test\s+#\d+:\s+(\S+)\s*$")

# ESTREIA-CTEST-HEADER (05/09/2026, achado do PRIMEIRO run real deste
# portao no servidor, 33989546515): `ctest -N` sempre abre com esta
# linha, ANTES de qualquer "Test #N:" - testes ou nao ("Test project
# /__w/GlintFx/GlintFx/build-shared" no Linux, "Test project D:/a/
# GlintFx/GlintFx/build-shared" no Windows, medido ao vivo baixando os
# dois artefatos reais).
_CTEST_PROJECT_HEADER_RE = re.compile(r"^Test project\b")

# O rodape que fecha uma listagem `ctest -N` com pelo menos um caso -
# "Total Tests: N", sempre a ultima linha desse bloco especifico.
_CTEST_TOTAL_RE = re.compile(r"^Total Tests:\s*\d+$")

# A OUTRA forma real de rodape (CTest imprime isto, sem "Total Tests:"
# nenhum, quando a listagem nao encontra teste algum) - nomeada
# explicitamente, nunca coberta por um "descarta o que nao bater com
# nada" generico (ver o corolario abaixo para o porque).
_CTEST_NO_TESTS_FOUND = "No tests were found!!!"

# Um nome de fixture/teste DESTE projeto, na convencao snake_case que
# GODS_LAWS.md L-21 exige para todo identificador de codigo/teste -
# e' o UNICO formato que `echo "<nome>" >> parity_inventory.txt`
# (Containerfile/ci.yml, P-0) ou um `ctest -N` real jamais escrevem
# como linha solta. Usado so para reconhecer uma linha de LISTA LIMPA
# (nunca para validar o grupo capturado por _CTEST_LINE_RE acima, que
# fica deliberadamente permissivo - `\S+` - porque aquele nome vem
# sempre acompanhado do proprio prefixo "Test #N:" que ja o identifica
# sem ambiguidade).
_CLEAN_LIST_NAME_RE = re.compile(r"^[a-z][a-z0-9_]*$")


# GODS_LAWS.md L-40 corolario (achado nesta mesma auditoria, 05/09/2026,
# conserto de um SEGUNDO defeito medido no mesmo run 33995142570 - o
# apply_aliases() acima e este parser tem a MESMA forma de bug,
# perda silenciosa, em dois lugares diferentes): o job `parity`
# (ci.yml) CONCATENA a saida crua de varias legs `ctest -N` COM a
# lista ja limpa que o job `wayland-container` publica (P-0,
# parity-inv-linux-container) num SO arquivo (`cat inventarios/
# linux/*/parity_inventory.txt > uniao.txt`) antes de chamar este
# script - a uniao real MISTURA os dois formatos no MESMO arquivo, um
# ou mais blocos de ctest cru seguidos da lista limpa do container.
#
# A VERSAO ANTERIOR deste parser decidia o formato do ARQUIVO INTEIRO
# pela PRIMEIRA linha de conteudo - que numa uniao real e' SEMPRE um
# cabecalho de ctest (a lista limpa do container e' concatenada por
# ultimo). Uma vez em "modo ctest cru", a extracao virava uma lista
# branca que so aceitava "Test #N: nome" - a linha solta do container
# NUNCA bate esse padrao, e desaparecia em SILENCIO, mesmo fisicamente
# presente no arquivo (medido: `shell_smoke`/`window_smoke` estao na
# uniao real do run 33995142570 e o parser antigo nao os via).
#
# O CONSERTO decide por LINHA, nao pelo arquivo inteiro - as DUAS
# formas convivem no mesmo texto por desenho, entao a leitura tem que
# aceitar as duas ao mesmo tempo, nunca escolher uma. E cada forma
# reconhecida hoje e' NOMEADA explicitamente (cabecalho, "Test #N:
# nome", "Total Tests: N", "No tests were found!!!", ou um nome de
# lista limpa na convencao snake_case do projeto) - uma linha que nao
# bate com NENHUMA delas e' ENTRADA CORROMPIDA e reprova (fail(),
# GODS_LAWS.md L-40 aplicado a leitura: a estrutura tem que reprovar o
# que nao sabe guardar, nunca descartar em silencio nem aceitar como
# se fosse um nome valido). LIMITACAO DECLARADA, ao trocar a lista
# branca generica de antes por esta enumeracao fechada: um formato de
# rodape que uma versao futura do CMake ainda nao escreveu hoje vai
# reprovar aqui como "corrompido" ate alguem nomea-lo explicitamente -
# um custo aceito de proposito (a alternativa, engolir qualquer coisa
# desconhecida, e' exatamente o defeito que este conserto existe para
# fechar).
def parse_inventory_text(text):
    content_lines = [ln.strip() for ln in text.splitlines()]
    content_lines = [ln for ln in content_lines if ln and not ln.startswith("#")]

    names = set()
    for line in content_lines:
        if _CTEST_PROJECT_HEADER_RE.match(line):
            continue
        m = _CTEST_LINE_RE.match(line)
        if m:
            names.add(m.group(1))
            continue
        if _CTEST_TOTAL_RE.match(line):
            continue
        if line == _CTEST_NO_TESTS_FOUND:
            continue
        if _CLEAN_LIST_NAME_RE.match(line):
            names.add(line)
            continue
        fail(
            f"linha de inventario nao reconhecida (nem cabecalho/rodape de ctest, nem "
            f"'Test #N: nome', nem nome de lista limpa em snake_case): {line!r}"
        )
    return names


_PROVA_PARCIAL_PREFIX = "PROVA-PARCIAL="
_BILATERAL_PREFIX = "bilateral="


def parse_exceptions_text(text, source_label="tests/parity_exceptions.txt"):
    exceptions = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) == 4:
            test_name, missing_on, gemeo, item = parts
            prova_parcial_gemeo = None
        elif len(parts) == 5:
            test_name, missing_on, gemeo, item, quinto = parts
            if not quinto.startswith(_PROVA_PARCIAL_PREFIX):
                fail(
                    f"{source_label}: quinto campo tem de comecar com "
                    f"{_PROVA_PARCIAL_PREFIX!r} (achou {quinto!r}): {line!r}"
                )
            prova_parcial_gemeo = quinto[len(_PROVA_PARCIAL_PREFIX):].strip()
            if not prova_parcial_gemeo:
                fail(
                    f"{source_label}: {_PROVA_PARCIAL_PREFIX!r} exige o nome do teste "
                    f"gemeo, nao vazio: {line!r}"
                )
        else:
            fail(
                f"{source_label}: linha malformada (esperava 4 ou 5 campos separados "
                f"por '|', achou {len(parts)}): {line!r}"
            )
        if missing_on not in SISTEMAS_VALIDOS:
            fail(
                f"{source_label}: sistema_onde_falta invalido {missing_on!r} para "
                f"{test_name!r} (esperado 'linux' ou 'windows')"
            )
        exceptions.append(
            {
                "test_name": test_name,
                "missing_on": missing_on,
                "gemeo": gemeo,
                "item": item,
                "prova_parcial_gemeo": prova_parcial_gemeo,
            }
        )
    return exceptions


def parse_aliases_text(text, source_label="tests/parity_aliases.txt"):
    aliases = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) == 2:
            linux_name, windows_name = parts
            bilateral_reason = None
        elif len(parts) == 3:
            linux_name, windows_name, terceiro = parts
            if not terceiro.startswith(_BILATERAL_PREFIX):
                fail(
                    f"{source_label}: terceiro campo tem de comecar com "
                    f"{_BILATERAL_PREFIX!r} (achou {terceiro!r}): {line!r}"
                )
            bilateral_reason = terceiro[len(_BILATERAL_PREFIX):].strip()
            if not bilateral_reason:
                fail(
                    f"{source_label}: {_BILATERAL_PREFIX!r} exige um motivo, nao vazio: "
                    f"{line!r}"
                )
        else:
            fail(
                f"{source_label}: linha malformada (esperava 2 ou 3 campos separados "
                f"por '|', achou {len(parts)}): {line!r}"
            )
        aliases.append(
            {
                "linux_name": linux_name,
                "windows_name": windows_name,
                "bilateral_reason": bilateral_reason,
            }
        )
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
# lacuna se QUALQUER UM dos parceiros dele (nome sob o outro sistema)
# existir no INVENTARIO INTEIRO do outro lado - deliberadamente
# checado contra o inventario completo, nao contra o conjunto "so
# daquele lado", para nao depender de o parceiro tambem ser exclusivo
# (um apelido correto quase sempre aponta pra um nome exclusivo do
# outro lado, mas checar a inclusao geral e o que torna a logica
# robusta por construcao, em vez de robusta so nos casos ja
# pensados). Devolve (so_linux, so_windows), os dois conjuntos que
# sobram depois de descontar apelidos - candidatos reais a lacuna.
#
# QUALQUER UM, NUNCA "O ULTIMO ESCRITO" (achado do lider, 05/09/2026,
# medido ao vivo no run 33995142570 - selftest_alias_shared_windows_
# partner_control acima reproduz a forma exata): um nome de UM lado
# pode ter MAIS DE UM apelido legitimo do OUTRO lado (um mecanismo
# Windows cobrindo tres fixtures Linux diferentes, por exemplo -
# tests/parity_aliases.txt tem hoje tres linhas para win32_display_
# connect_test). {b: a for a, b in aliases} e' um dict chaveado pelo
# nome que se repete - so guarda UM parceiro por chave, e o ULTIMO
# apelido escrito na lista SOBRESCREVE OS ANTERIORES EM SILENCIO
# (GODS_LAWS.md L-40: "a estrutura nao pode guardar tudo que foi
# escrito" e' erro, nao detalhe de implementacao). A estrutura certa
# e' um dict de CONJUNTOS (setdefault(...).add(...)) - nada e'
# descartado, cada apelido escrito sobrevive, e o candidato so vira
# lacuna quando NENHUM dos parceiros registrados aparece no
# inventario do outro lado.
def apply_aliases(linux_only, windows_only, linux_inventory, windows_inventory, aliases):
    linux_to_win = {}
    win_to_linux = {}
    for alias in aliases:
        linux_name = alias["linux_name"]
        windows_name = alias["windows_name"]
        linux_to_win.setdefault(linux_name, set()).add(windows_name)
        win_to_linux.setdefault(windows_name, set()).add(linux_name)

    remaining_linux_only = set()
    for name in linux_only:
        partners = linux_to_win.get(name, ())
        if any(partner in windows_inventory for partner in partners):
            # pelo menos um parceiro existe de fato no lado Windows -
            # par confirmado sob outro nome, nao e lacuna.
            continue
        remaining_linux_only.add(name)

    remaining_windows_only = set()
    for name in windows_only:
        partners = win_to_linux.get(name, ())
        if any(partner in linux_inventory for partner in partners):
            continue
        remaining_windows_only.add(name)

    return remaining_linux_only, remaining_windows_only


# PARITY-ALIAS-HYGIENE (a), TODO.md W7-C, GODS_LAWS.md L-17/L-40:
# um apelido MORTO e uma linha cuja dupla de nomes nao aparece em
# NENHUM inventario (nem Linux, nem Windows) - o teste que ele
# apontava foi renomeado ou apagado, e ninguem limpou tests/
# parity_aliases.txt. apply_aliases() acima nunca pega isto porque so
# olha para os nomes que JA sao candidatos a lacuna (linux_only/
# windows_only); um apelido morto nunca chega la, porque nenhum dos
# dois lados dele esta em inventario nenhum para comecar - por isso a
# checagem precisa iterar a TABELA DE APELIDOS diretamente, nao os
# conjuntos de lacuna.
#
# PARITY-ALIAS-HYGIENE (b), D8: um apelido BILATERAL e uma linha cujo
# nome do lado "exclusivo" TAMBEM roda no outro sistema (o parceiro
# nao e exclusivo de lado nenhum - ex.: dep_zero_binary_test roda
# incondicional, nos dois sistemas, mas o par declara windows como se
# fosse so dele). Nao esconde cobertura (o outro lado tem MAIS, nunca
# menos), mas fere a semantica que o cabecalho do arquivo declara -
# por isso reprova SALVO quando a linha traz o terceiro campo
# "bilateral=<motivo>" (D8: "reprovar salvo declaracao", nunca so
# avisar - a licao do ESLint #18665).
#
# PARITY-ALIAS-HYGIENE, sub-fatia P-2, achado IMPORTANTE do revisor
# independente (23/09/2026, /var/tmp/glintfx-plan/revisao-parity-
# alias-hygiene.md secao 4): a metade que faltava do proprio par (b) -
# uma linha com "bilateral=<motivo>" cujo par NAO e de fato bilateral
# (cada nome so existe do seu proprio lado) passava em silencio, sem
# entrar em nenhuma das tres listas acima, porque o `continue` do
# `not is_bilateral` disparava ANTES de olhar `bilateral_reason`. E a
# MESMA familia de defeito que esta fatia inteira existe para banir -
# "prosa nunca conferida por maquina" - so que na direcao oposta (falta
# de declaracao e punida; declaracao superflua/falsa nao era). Reprova
# sempre, sem excecao possivel: uma declaracao bilateral so e' honesta
# quando o fato que ela descreve e' verdadeiro.
#
# PARITY-ALIAS-HYGIENE, sub-fatia P-2, achado COSMETICO do revisor
# (mesma secao): um apelido MEIO-MORTO - um lado inteiro ausente de
# QUALQUER inventario, enquanto o outro lado existe de verdade (so do
# seu proprio lado, nunca bilateral) - nao e "morto" (exige os DOIS
# lados ausentes) nem "bilateral" (exige os dois lados presentes em
# ALGUM inventario para is_bilateral fazer sentido), entao escapava
# tambem do `continue`, silencioso, e so era pego mais tarde pela
# analise de lacuna generica no fim de run_comparison() - com uma
# mensagem que aponta para o nome orfao, nunca para a LINHA de apelido
# podre que o citou. Categoria propria, mensagem que nomeia qual lado
# sumiu - diagnostico melhor, nao lacuna nova (a analise generica
# continua reprovando o mesmo nome do jeito de sempre).
#
# SUPRESSAO POR IRMAO VIVO (achado do PROPRIO implementador, medido
# rodando --selftest contra este conserto antes de commitar): um nome
# de UM lado pode legitimamente ter VARIOS apelidos apontando pro
# MESMO parceiro do outro lado (tests/parity_aliases.txt tem hoje tres
# apelidos Linux para win32_display_connect_test - achado do lider,
# 05/09/2026, run 33995142570, ver apply_aliases() acima e o controle
# selftest_alias_shared_windows_partner_control). Sem a supressao
# abaixo, esta checagem quebrava EXATAMENTE aquele controle: a linha
# shell_smoke|win32_display_connect_test (shell_smoke e' fixture do
# container, so' publicado por um artefato separado - P-0 - nunca por
# `ctest -N`) passava a reprovar como "meio-morta" mesmo com DOIS
# irmaos (display_connect_failure_test, shell_requirements_test)
# provando que o parceiro Windows continua vivo. A linha so' conta
# como meio-morta de verdade quando NENHUM irmao do mesmo grupo (outro
# apelido apontando pro MESMO parceiro presente) tambem esta' vivo -
# um apelido redundante/nao confirmado nesta chamada nao e' o mesmo
# que um apelido comprovadamente podre.
def compute_alias_hygiene(aliases, linux_inventory, windows_inventory):
    combined_inventory = linux_inventory | windows_inventory

    # Mesmo agrupamento de apply_aliases() acima (dict de CONJUNTOS,
    # nunca dict simples - GODS_LAWS.md L-40/achado do lider
    # 05/09/2026): usado so' para a supressao por irmao vivo, nunca
    # para decidir dead/bilateral, que continuam por PAR individual.
    linux_to_win = {}
    win_to_linux = {}
    for alias in aliases:
        linux_to_win.setdefault(alias["linux_name"], set()).add(alias["windows_name"])
        win_to_linux.setdefault(alias["windows_name"], set()).add(alias["linux_name"])

    dead = []
    half_dead = []
    bilateral_declared = []
    bilateral_undeclared = []
    bilateral_false = []
    for alias in aliases:
        linux_name = alias["linux_name"]
        windows_name = alias["windows_name"]
        linux_absent = linux_name not in combined_inventory
        windows_absent = windows_name not in combined_inventory
        if linux_absent and windows_absent:
            dead.append(alias)
            continue
        if linux_absent:
            sibling_alive = any(
                name in linux_inventory for name in win_to_linux.get(windows_name, ())
            )
            if not sibling_alive:
                half_dead.append((alias, "linux"))
            continue
        if windows_absent:
            sibling_alive = any(
                name in windows_inventory for name in linux_to_win.get(linux_name, ())
            )
            if not sibling_alive:
                half_dead.append((alias, "windows"))
            continue
        is_bilateral = linux_name in windows_inventory or windows_name in linux_inventory
        if not is_bilateral:
            if alias["bilateral_reason"]:
                # P-2, achado IMPORTANTE: declaracao bilateral cujo
                # fato descrito nao e' verdade - nao pode passar calada
                # so porque o par nao e' candidato a lacuna hoje.
                bilateral_false.append(alias)
            continue
        if alias["bilateral_reason"]:
            bilateral_declared.append(alias)
        else:
            bilateral_undeclared.append(alias)
    return dead, bilateral_declared, bilateral_undeclared, bilateral_false, half_dead


# PARITY-ALIAS-HYGIENE (c), D9 - gemeo direto do apelido morto acima
# aplicado a tests/parity_exceptions.txt (GODS_LAWS.md L-17: o mesmo
# defeito, o mesmo remedio, no outro lado da tabela). Uma excecao e
# MORTA de duas formas: (1) o teste que ela cita nao existe em
# inventario nenhum (renomeado/apagado, orfa); (2) o teste que ela
# cita ja existe TAMBEM do lado declarado "onde falta" - a lacuna
# fechou e ninguem apagou a linha (a mesma regra "concluido sem par"
# que validate_exceptions ja aplica via TODO.md, aqui aplicada
# diretamente contra o inventario real, que e mais forte: pega mesmo
# quando ninguem lembrou de marcar o item como Concluido).
def compute_exception_hygiene(exceptions, linux_inventory, windows_inventory):
    combined_inventory = linux_inventory | windows_inventory
    dead = []
    for exc in exceptions:
        test_name = exc["test_name"]
        missing_side_inventory = (
            windows_inventory if exc["missing_on"] == "windows" else linux_inventory
        )
        if test_name not in combined_inventory:
            dead.append(exc)
            continue
        if test_name in missing_side_inventory:
            dead.append(exc)
    return dead


# PARITY-ALIAS-HYGIENE (d), D10: a forma PROVA-PARCIAL so e honesta
# quando o gemeo que ela cita EXISTE de verdade, como ctest
# registrado, no inventario do sistema declarado em missing_on - a
# mesma disciplina que compute_alias_hygiene/compute_exception_
# hygiene aplicam: nunca aceitar de leitura humana o que da para
# conferir por maquina (GODS_LAWS.md L-40). Sem essa conferencia, a
# forma PROVA-PARCIAL seria so um jeito novo de calar o portao com
# prosa nunca verificada - exatamente o defeito que tests/
# parity_exceptions.txt:317 (gpu_kind_report_smoke) tem hoje, citando
# um ARQUIVO-FONTE (wgl_context_adapter.cpp), nunca um nome de ctest -
# essa linha continua como excecao comum ate ganhar um gemeo de
# verdade (D10: "se nao existir, a linha e uma ausencia e fica como
# esta").
def validate_prova_parcial(exceptions, linux_inventory, windows_inventory):
    errors = []
    for exc in exceptions:
        gemeo_nome = exc.get("prova_parcial_gemeo")
        if gemeo_nome is None:
            continue
        target_inventory = windows_inventory if exc["missing_on"] == "windows" else linux_inventory
        if gemeo_nome not in target_inventory:
            errors.append(
                f"{exc['test_name']}: excecao marca PROVA-PARCIAL com gemeo {gemeo_nome!r}, "
                f"mas esse nome nao existe no inventario {exc['missing_on']} (gemeo nao "
                "conferido por maquina - GODS_LAWS.md L-40)"
            )
    return errors


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
# que o portao passa. Reune os controles que --selftest exige provar
# em vermelho: piso de varredura vazia (GODS_LAWS.md L-40), excecao
# invalida (validate_exceptions acima), lacuna sem excecao
# registrada, e a higiene das duas tabelas acrescentada por PARITY-
# ALIAS-HYGIENE (apelido morto, apelido bilateral sem declaracao,
# excecao morta, PROVA-PARCIAL com gemeo nao conferido).
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

    (
        alias_dead,
        _alias_bilateral_declared,
        alias_bilateral_undeclared,
        alias_bilateral_false,
        alias_half_dead,
    ) = compute_alias_hygiene(aliases, linux_inventory, windows_inventory)
    for alias in alias_dead:
        errors.append(
            f"apelido morto: {alias['linux_name']}|{alias['windows_name']} nao aparece em "
            "inventario nenhum (nem Linux, nem Windows) - tests/parity_aliases.txt"
        )
    for alias in alias_bilateral_undeclared:
        errors.append(
            f"apelido bilateral sem declaracao: {alias['linux_name']}|{alias['windows_name']} "
            "roda nos dois sistemas, precisa do terceiro campo 'bilateral=<motivo>' em "
            "tests/parity_aliases.txt"
        )
    for alias in alias_bilateral_false:
        errors.append(
            f"declaracao bilateral falsa: {alias['linux_name']}|{alias['windows_name']} traz "
            f"'bilateral={alias['bilateral_reason']}' em tests/parity_aliases.txt, mas o par "
            "NAO roda nos dois sistemas de verdade (nenhum dos dois nomes aparece no "
            "inventario do outro lado) - uma declaracao bilateral so e honesta quando o "
            "fato que ela descreve e verdadeiro (PARITY-ALIAS-HYGIENE P-2, GODS_LAWS.md L-40)"
        )
    for alias, missing_side in alias_half_dead:
        errors.append(
            f"apelido meio-morto: {alias['linux_name']}|{alias['windows_name']} tem o lado "
            f"{missing_side} ausente de inventario nenhum (nem Linux, nem Windows) - "
            "tests/parity_aliases.txt aponta para um nome que foi renomeado ou apagado "
            "de um dos lados (PARITY-ALIAS-HYGIENE P-2)"
        )

    exception_dead = compute_exception_hygiene(exceptions, linux_inventory, windows_inventory)
    for exc in exception_dead:
        errors.append(
            f"excecao morta: {exc['test_name']} (missing_on={exc['missing_on']}) em "
            "tests/parity_exceptions.txt nao aparece em inventario nenhum, ou ja existe "
            "tambem do lado declarado como faltante - apague a linha"
        )

    errors.extend(validate_prova_parcial(exceptions, linux_inventory, windows_inventory))

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

    # PARITY-ALIAS-HYGIENE (TODO.md W7-C, GODS_LAWS.md L-40): estas
    # duas linhas imprimem SEMPRE, ganhe ou perca o portao - o piso de
    # varredura exige a contagem, nao so o veredicto. Calculadas aqui
    # (nao dentro de run_comparison) porque valem mesmo quando um
    # inventario vem vazio: nesse caso todo apelido/excecao aparece
    # "morto" por definicao (nao ha nada em inventario nenhum para
    # bater), o que e' verdade honesta, nao um segundo erro de
    # varredura vazia disfarcado - a lista de erros que decide
    # REPROVADO continua vindo so de run_comparison().
    (
        alias_dead,
        alias_bilateral_declared,
        alias_bilateral_undeclared,
        alias_bilateral_false,
        alias_half_dead,
    ) = compute_alias_hygiene(aliases, linux_inventory, windows_inventory)
    print(
        f"{SCRIPT_NAME}: {len(aliases)} apelido(s), {len(alias_dead)} morto(s), "
        f"{len(alias_bilateral_declared)} bilateral(is) declarado(s), "
        f"{len(alias_bilateral_undeclared)} bilateral(is) sem declaracao, "
        f"{len(alias_bilateral_false)} bilateral(is) falso(s), "
        f"{len(alias_half_dead)} meio-morto(s)"
    )
    exception_dead = compute_exception_hygiene(exceptions, linux_inventory, windows_inventory)
    print(f"{SCRIPT_NAME}: {len(exceptions)} excecao(oes), {len(exception_dead)} morta(s)")

    errors = run_comparison(linux_inventory, windows_inventory, exceptions, aliases, todo_status)
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)

    print(f"{SCRIPT_NAME}: paridade OK - nenhuma lacuna sem excecao registrada")


# --- fixtures and controls for --selftest -----------------------------


def _alias_fixture(linux_name, windows_name, bilateral_reason=None):
    return {
        "linux_name": linux_name,
        "windows_name": windows_name,
        "bilateral_reason": bilateral_reason,
    }


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


# VERMELHO #3b (P-2, PARITY-ALIAS-HYGIENE, achado do orquestrador
# 23/09/2026: sabotagem numa copia de 7beb354 desligando SO o `if not
# windows_inventory:` deixava o --selftest inteiro verde, porque
# VERMELHO#3 acima so exercita o lado Linux vazio - o piso de
# varredura L-40 vale para os DOIS lados, e faltava o controle
# simetrico que prova isso do lado Windows). Esperado: reprova,
# mensagem cita "varredura vazia", com o inventario Linux desta vez
# NAO vazio (para isolar exatamente o ramo Windows do `if`).
def selftest_empty_inventory_windows_reproves():
    errors = run_comparison({"a_test"}, set(), [], [], {})
    if not errors:
        print("selftest: VERMELHO#3b FALHOU (inventario Windows vazio deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("varredura vazia" in e for e in errors):
        print(f"selftest: VERMELHO#3b FALHOU (reprovou, mas sem 'varredura vazia'): {errors}", file=sys.stderr)
        return False
    print(f"selftest: VERMELHO#3b OK (inventario Windows vazio pego): {errors}")
    return True


# Controle de APELIDO: dois nomes diferentes, um por sistema,
# declarados equivalentes - nao deve virar lacuna, mesmo sem nenhuma
# excecao.
def selftest_alias_control():
    linux_inv = {"a_test", "display_connect_failure_test"}
    windows_inv = {"a_test", "win32_display_connect_test"}
    aliases = [_alias_fixture("display_connect_failure_test", "win32_display_connect_test")]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if errors:
        print(f"selftest: controle APELIDO FALHOU (par com apelido nao deveria reprovar): {errors}", file=sys.stderr)
        return False
    print("selftest: controle APELIDO OK (par sob nomes diferentes nao conta como lacuna)")
    return True


# Controle APELIDO-COMPARTILHADO (achado do líder, 05/09/2026, medido
# ao vivo no run 33995142570 - win32_display_connect_test reprovou
# como "faltando no Linux" mesmo tendo TRES apelidos legítimos em
# tests/parity_aliases.txt: display_connect_failure_test,
# shell_requirements_test e shell_smoke, todos apontando para o MESMO
# nome Windows). apply_aliases() montava win_to_linux como um dict
# simples ({b: a for a, b in aliases}) - chaveado pelo nome Windows,
# ele so guarda UM parceiro por chave, e o ULTIMO apelido escrito na
# lista SOBRESCREVE OS ANTERIORES em silêncio. A perda é muda: não
# aparece como erro de parsing, só como uma reprovação (ou, pior,
# uma aprovação indevida) sem explicação.
#
# Este controle reproduz a FORMA EXATA da reprovação real (run
# 33995142570): tres apelidos para o mesmo parceiro Windows, na
# MESMA ordem de tests/parity_aliases.txt (display_connect_failure_
# test, depois shell_requirements_test, depois shell_smoke por
# ultimo). shell_smoke e' um fixture do CONTAINER (P-0) - so entra no
# inventario Linux por um artefato SEPARADO (parity-inv-linux-
# container), nunca por `ctest -N` - entao um inventario Linux real
# pode legitimamente ter os dois PRIMEIROS nomes e nao ter o
# TERCEIRO. Contra o dict simples de hoje ({b: a for a, b in
# aliases}), a chave "win32_display_connect_test" so guarda o
# ULTIMO valor escrito - "shell_smoke" - e perde silenciosamente os
# dois primeiros. Como "shell_smoke" nao esta neste inventario Linux
# sintetico (os dois primeiros estao), apply_aliases() de hoje falha
# em achar QUALQUER parceiro valido e reprova um par que na verdade
# existe sob outros dois nomes - a MESMA forma do falso-vermelho
# medido ao vivo.
def selftest_alias_shared_windows_partner_control():
    linux_inv = {
        "a_test",
        "display_connect_failure_test",
        "shell_requirements_test",
        # "shell_smoke" DELIBERADAMENTE AUSENTE - e' o fixture do
        # container, publicado por um artefato separado (P-0), nunca
        # por `ctest -N`; um inventario Linux real pode legitimamente
        # nao o ter ainda neste ponto da comparacao.
    }
    windows_inv = {"a_test", "win32_display_connect_test"}
    aliases = [
        _alias_fixture("display_connect_failure_test", "win32_display_connect_test"),
        _alias_fixture("shell_requirements_test", "win32_display_connect_test"),
        _alias_fixture("shell_smoke", "win32_display_connect_test"),
    ]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if errors:
        print(
            "selftest: controle APELIDO-COMPARTILHADO FALHOU (win32_display_connect_test "
            "tem DOIS parceiros Linux presentes no inventario - display_connect_failure_test "
            f"e shell_requirements_test - nenhum deveria sobrar como lacuna): {errors}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle APELIDO-COMPARTILHADO OK (varios nomes Linux para o mesmo "
        "parceiro Windows sobrevivem todos, nenhum apelido descartado em silencio)"
    )
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
        "gpu_teste|windows|arquivo.cpp|ITEM-2|PROVA-PARCIAL=gemeo_win_test\n"
    )
    exceptions = parse_exceptions_text(exceptions_text)
    if exceptions != [
        {
            "test_name": "meu_teste",
            "missing_on": "windows",
            "gemeo": "outro_teste",
            "item": "ITEM-1",
            "prova_parcial_gemeo": None,
        },
        {
            "test_name": "outro",
            "missing_on": "linux",
            "gemeo": "nenhum",
            "item": SEM_PENDENCIA,
            "prova_parcial_gemeo": None,
        },
        {
            "test_name": "gpu_teste",
            "missing_on": "windows",
            "gemeo": "arquivo.cpp",
            "item": "ITEM-2",
            "prova_parcial_gemeo": "gemeo_win_test",
        },
    ]:
        print(f"selftest: PARSING FALHOU (exceptions): {exceptions}", file=sys.stderr)
        return False

    aliases_text = "# comentario\na_linux|a_windows\nb_linux|b_windows|bilateral=roda nos dois\n"
    aliases = parse_aliases_text(aliases_text)
    if aliases != [
        _alias_fixture("a_linux", "a_windows"),
        _alias_fixture("b_linux", "b_windows", bilateral_reason="roda nos dois"),
    ]:
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


# VERMELHO NOVO (ESTREIA-CTEST-HEADER, 05/09/2026 - o defeito real
# medido no run 33989546515): a linha "Test project <caminho>" que
# `ctest -N` sempre imprime primeiro NUNCA pode virar nome de teste,
# nos dois formatos de caminho reais (Linux com "/", Windows com
# "D:/"). Sem este controle, o parser antigo passava aqui em silencio
# - foi exatamente essa ausencia que deixou a estreia no servidor
# reprovar citando o caminho da maquina como se fosse um nome de
# teste igual dos dois lados.
def selftest_ctest_project_header_not_swallowed():
    linux_ctest_text = (
        "Test project /__w/GlintFx/GlintFx/build-shared\n"
        "  Test  #1: foo_test\n"
        "  Test  #2: bar_test\n"
        "\n"
        "Total Tests: 2\n"
    )
    windows_ctest_text = (
        "Test project D:/a/GlintFx/GlintFx/build-shared\n"
        "  Test  #1: foo_test\n"
        "\n"
        "Total Tests: 1\n"
    )
    linux_inv = parse_inventory_text(linux_ctest_text)
    windows_inv = parse_inventory_text(windows_ctest_text)
    if linux_inv != {"foo_test", "bar_test"}:
        print(
            f"selftest: CABECALHO-CTEST FALHOU (lado Linux engoliu cabecalho/rodape): {linux_inv}",
            file=sys.stderr,
        )
        return False
    if windows_inv != {"foo_test"}:
        print(
            f"selftest: CABECALHO-CTEST FALHOU (lado Windows engoliu cabecalho/rodape): {windows_inv}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: CABECALHO-CTEST OK ('Test project <caminho>' nunca vira nome de "
        "teste, Linux e Windows)"
    )
    return True


# Controle-irmao, e o mais perigoso de esquecer: o MESMO cabecalho,
# mas com ZERO testes reais depois dele - o caso exato que uma
# correcao demasiado agressiva (uma lista negra generica demais, por
# exemplo "descarta qualquer linha que pareca cabecalho") poderia
# apagar em silencio, fazendo o inventario parecer nao-vazio quando
# na verdade nao existe teste nenhum. Esperado: conjunto REALMENTE
# vazio, para o piso de varredura (VERMELHO#3 acima, GODS_LAWS.md
# L-40) continuar pegando este caso como coleta quebrada em vez de
# aceita-lo como paridade.
def selftest_ctest_header_only_yields_empty_inventory():
    ctest_text = "Test project /__w/GlintFx/GlintFx/build-shared\n\nTotal Tests: 0\n"
    inv = parse_inventory_text(ctest_text)
    if inv:
        print(
            f"selftest: CABECALHO-SOZINHO FALHOU (esperava inventario vazio, veio {inv})",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: CABECALHO-SOZINHO OK (cabecalho sem teste nenhum continua vazio, "
        "piso de L-40 intacto)"
    )
    return True


# Controle UNIAO-MISTA (achado do lider, 05/09/2026, medido ao vivo no
# run 33995142570 - a mesma sessao que motivou o conserto de apply_
# aliases() acima, mas um defeito DIFERENTE e mais grave: P-0 (docs/
# plano-w6a-janela.md fatia 1) publica o inventario do container
# (parity-inv-linux-container) como uma TERCEIRA fonte Linux, e o job
# `parity` (ci.yml) CONCATENA essa lista ja limpa (bare names, um por
# linha, sem "Test project"/"Test #N:" nenhum) DEPOIS da uniao de
# varias saidas cruas de `ctest -N` (uma por leg) no MESMO arquivo -
# exatamente a forma real medida: `parity_linux_union.txt` do run
# citado tem nove blocos "Test project .../Test #N: nome/.../Total
# Tests: N" seguidos por uma linha solta "shell_smoke" no final.
#
# parse_inventory_text() de ANTES deste conserto decidia o formato do
# ARQUIVO INTEIRO pela PRIMEIRA linha de conteudo - que e sempre um
# cabecalho de ctest numa uniao real, porque a lista limpa do
# container e sempre concatenada por ultimo. Uma vez em "modo ctest
# cru", a extracao vira uma lista branca que so aceita "Test #N: nome"
# - a linha solta do container NUNCA bate esse padrao, e desaparece em
# SILENCIO, mesmo fisicamente presente no arquivo. Este controle
# reproduz a MESMA forma: multiplos blocos de ctest cru, terminando
# com uma linha limpa de nome de fixture do container - contra o
# parser de antes, o nome do container simplesmente nao aparece no
# conjunto resultante.
def selftest_mixed_ctest_and_clean_list_control():
    mixed_text = (
        "Test project /__w/GlintFx/GlintFx/build-shared\n"
        "  Test  #1: foo_test\n"
        "  Test  #2: bar_test\n"
        "\n"
        "Total Tests: 2\n"
        "Test project /__w/GlintFx/GlintFx/build-static\n"
        "  Test  #1: foo_test\n"
        "\n"
        "Total Tests: 1\n"
        "shell_smoke\n"
        "window_smoke\n"
    )
    names = parse_inventory_text(mixed_text)
    expected = {"foo_test", "bar_test", "shell_smoke", "window_smoke"}
    if names != expected:
        print(
            f"selftest: controle UNIAO-MISTA FALHOU (esperava {expected}, veio {names} - "
            "nome(s) do container perdido(s) na mistura com saida crua de ctest)",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: controle UNIAO-MISTA OK (saida crua de ctest e lista limpa do "
        "container no MESMO arquivo, nenhum nome perdido)"
    )
    return True


# Controle ENTRADA-CORROMPIDA (o outro lado do mesmo achado, L-40
# aplicado a LEITURA em vez de a tabela de apelidos): uma linha que
# nao e cabecalho, nao e "Test #N: nome", nao e o rodape "Total Tests:
# N", nao e a mensagem "No tests were found!!!" e nao tem a forma de
# um nome de fixture deste projeto (snake_case, GODS_LAWS.md L-21) nao
# bate com NENHUMA das formas reconhecidas - e tem que doer (fail(),
# GODS_LAWS.md L-40), nunca virar um nome de teste fantasma nem
# desaparecer em silencio.
def selftest_corrupted_inventory_line_reproves():
    garbled_text = (
        "Test project /__w/GlintFx/GlintFx/build-shared\n"
        "  Test  #1: foo_test\n"
        "isto aqui nao e saida crua de ctest nem um nome limpo!!\n"
        "Total Tests: 1\n"
    )
    try:
        parse_inventory_text(garbled_text)
    except SystemExit:
        print(
            "selftest: controle ENTRADA-CORROMPIDA OK (linha sem forma reconhecida "
            "reprova, GODS_LAWS.md L-40, em vez de virar nome fantasma ou sumir)"
        )
        return True
    print(
        "selftest: controle ENTRADA-CORROMPIDA FALHOU (linha corrompida deveria ter "
        "reprovado)",
        file=sys.stderr,
    )
    return False


# PARITY-ALIAS-HYGIENE C1 (VERMELHO): um apelido cujos dois nomes nao
# aparecem em inventario nenhum - o teste que ele apontava sumiu e a
# linha ficou orfa. Esperado: reprova, citando o par.
def selftest_alias_dead_reproves():
    linux_inv = {"a_test"}
    windows_inv = {"a_test"}
    aliases = [_alias_fixture("apelido_morto_linux", "apelido_morto_windows")]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C1 FALHOU (apelido morto deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("apelido morto" in e and "apelido_morto_linux" in e for e in errors):
        print(f"selftest: PARITY-ALIAS-HYGIENE C1 FALHOU (reprovou, mas nao citou o apelido morto): {errors}", file=sys.stderr)
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C1 OK (apelido morto pego): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C1b (VERMELHO, P-2, achado COSMETICO do
# revisor): apelido MEIO-morto - um lado inteiro ausente de inventario
# nenhum, enquanto o outro lado existe de verdade (so do seu proprio
# lado, nunca bilateral). Diferente de C1 (os DOIS lados ausentes):
# aqui o lado Windows do par foi renomeado/apagado, mas o lado Linux
# continua existindo, hoje, como um nome real. Esperado: reprova com
# a categoria propria "apelido meio-morto", nomeando qual lado sumiu -
# a analise de lacuna generica tambem reprova o mesmo nome (nao e'
# suprimida), entao os dois erros coexistem.
def selftest_alias_half_dead_reproves():
    linux_inv = {"a_test", "meio_morto_linux_ok_test"}
    windows_inv = {"a_test"}
    aliases = [_alias_fixture("meio_morto_linux_ok_test", "meio_morto_windows_apagado_test")]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C1b FALHOU (apelido meio-morto deveria ter reprovado)", file=sys.stderr)
        return False
    if not any(
        "apelido meio-morto" in e and "meio_morto_linux_ok_test" in e and "windows" in e
        for e in errors
    ):
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b FALHOU (reprovou, mas nao citou o apelido "
            f"meio-morto com o lado ausente): {errors}",
            file=sys.stderr,
        )
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C1b OK (apelido meio-morto pego, com o lado ausente nomeado): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C2 (VERMELHO): apelido cujo lado "exclusivo"
# tambem roda no outro sistema, sem o terceiro campo declarando isso
# - D8, "reprovar salvo declaracao". Esperado: reprova.
def selftest_alias_bilateral_undeclared_reproves():
    linux_inv = {"a_test", "dep_zero_binary_test"}
    windows_inv = {"a_test", "dep_zero_binary_test", "dep_zero_binary_win_test"}
    aliases = [_alias_fixture("dep_zero_binary_test", "dep_zero_binary_win_test")]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C2 FALHOU (apelido bilateral sem declaracao deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("bilateral sem declaracao" in e and "dep_zero_binary_test" in e for e in errors):
        print(f"selftest: PARITY-ALIAS-HYGIENE C2 FALHOU (reprovou, mas nao citou o bilateral): {errors}", file=sys.stderr)
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C2 OK (bilateral sem declaracao pego): {errors}")
    return True


# Controle-irmao de C2: a MESMA forma bilateral, mas COM o terceiro
# campo preenchido - deve contar e passar, nunca reprovar.
def selftest_alias_bilateral_declared_control():
    linux_inv = {"a_test", "dep_zero_binary_test"}
    windows_inv = {"a_test", "dep_zero_binary_test", "dep_zero_binary_win_test"}
    aliases = [
        _alias_fixture(
            "dep_zero_binary_test",
            "dep_zero_binary_win_test",
            bilateral_reason="roda nos dois lados por ser fonte incondicional",
        )
    ]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if errors:
        print(f"selftest: controle BILATERAL-DECLARADO FALHOU (deveria ter passado): {errors}", file=sys.stderr)
        return False
    print("selftest: controle BILATERAL-DECLARADO OK (bilateral com motivo conta e passa)")
    return True


# PARITY-ALIAS-HYGIENE C2b (VERMELHO, P-2, achado IMPORTANTE do
# revisor independente, 23/09/2026, provado sem mutacao contra o
# commit 5d0c173: uma linha com "bilateral=<motivo>" cujo par NAO e'
# de fato bilateral (cada nome so existe do seu proprio lado, nenhum
# aparece no inventario do OUTRO sistema) passava calada - o `continue`
# do `not is_bilateral` disparava antes de olhar bilateral_reason, e a
# declaracao nunca entrava em nenhuma das tres listas antigas (nao
# conta como morta, nao conta como bilateral declarada, nao conta como
# bilateral sem declaracao). Esperado: reprova, citando "declaracao
# bilateral falsa" e o par. Os dois nomes SAO um par de apelido valido
# (existem cada um so do seu lado, cobrindo a lacuna um do outro) -
# isola exatamente o defeito da declaracao, sem lacuna generica junto.
def selftest_alias_bilateral_false_declaration_reproves():
    linux_inv = {"a_test", "falso_bilateral_linux_test"}
    windows_inv = {"a_test", "falso_bilateral_windows_test"}
    aliases = [
        _alias_fixture(
            "falso_bilateral_linux_test",
            "falso_bilateral_windows_test",
            bilateral_reason="mentira, na verdade nao roda nos dois sistemas",
        )
    ]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if not errors:
        print(
            "selftest: PARITY-ALIAS-HYGIENE C2b FALHOU (declaracao bilateral falsa deveria "
            "ter reprovado)",
            file=sys.stderr,
        )
        return False
    if not any(
        "declaracao bilateral falsa" in e and "falso_bilateral_linux_test" in e for e in errors
    ):
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C2b FALHOU (reprovou, mas nao citou a declaracao "
            f"bilateral falsa): {errors}",
            file=sys.stderr,
        )
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C2b OK (declaracao bilateral falsa pega): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C3a (VERMELHO): excecao cujo teste ja existe
# TAMBEM do lado declarado como faltante - a lacuna fechou e a linha
# ficou para tras (gemeo L-17 do apelido morto). Esperado: reprova.
def selftest_exception_dead_gap_closed_reproves():
    linux_inv = {"a_test", "so_linux_antes_test"}
    windows_inv = {"a_test", "so_linux_antes_test"}
    exceptions = [
        {
            "test_name": "so_linux_antes_test",
            "missing_on": "windows",
            "gemeo": "nenhum",
            "item": SEM_PENDENCIA,
            "prova_parcial_gemeo": None,
        }
    ]
    errors = run_comparison(linux_inv, windows_inv, exceptions, [], {})
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C3a FALHOU (excecao com lacuna ja fechada deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("excecao morta" in e and "so_linux_antes_test" in e for e in errors):
        print(f"selftest: PARITY-ALIAS-HYGIENE C3a FALHOU (reprovou, mas nao citou a excecao morta): {errors}", file=sys.stderr)
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C3a OK (excecao com par ja fechado pega): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C3b (VERMELHO): excecao cujo teste nao existe
# em inventario nenhum - orfa, teste renomeado ou apagado. Esperado:
# reprova.
def selftest_exception_dead_orphaned_reproves():
    linux_inv = {"a_test"}
    windows_inv = {"a_test"}
    exceptions = [
        {
            "test_name": "teste_que_nao_existe_mais",
            "missing_on": "windows",
            "gemeo": "nenhum",
            "item": SEM_PENDENCIA,
            "prova_parcial_gemeo": None,
        }
    ]
    errors = run_comparison(linux_inv, windows_inv, exceptions, [], {})
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C3b FALHOU (excecao orfa deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("excecao morta" in e and "teste_que_nao_existe_mais" in e for e in errors):
        print(f"selftest: PARITY-ALIAS-HYGIENE C3b FALHOU (reprovou, mas nao citou a excecao orfa): {errors}", file=sys.stderr)
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C3b OK (excecao orfa pega): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C4a (VERMELHO): excecao com PROVA-PARCIAL
# cujo gemeo citado NAO existe no inventario do sistema declarado em
# missing_on - a forma nao pode ser aceita de leitura humana.
# Esperado: reprova.
def selftest_prova_parcial_gemeo_absent_reproves():
    linux_inv = {"a_test", "gpu_kind_report_smoke"}
    windows_inv = {"a_test"}
    exceptions = [
        {
            "test_name": "gpu_kind_report_smoke",
            "missing_on": "windows",
            "gemeo": "wgl_context_adapter.cpp",
            "item": "WIN-RUNNER-PROPRIO",
            "prova_parcial_gemeo": "gpu_kind_report_compile_win_test",
        }
    ]
    todo_status = parse_todo_status_text(_todo_fixture("WIN-RUNNER-PROPRIO", "🔍 Em verificação"))
    errors = run_comparison(linux_inv, windows_inv, exceptions, [], todo_status)
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C4a FALHOU (PROVA-PARCIAL com gemeo ausente deveria ter reprovado)", file=sys.stderr)
        return False
    if not any("PROVA-PARCIAL" in e and "gpu_kind_report_compile_win_test" in e for e in errors):
        print(f"selftest: PARITY-ALIAS-HYGIENE C4a FALHOU (reprovou, mas nao citou o gemeo ausente): {errors}", file=sys.stderr)
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C4a OK (PROVA-PARCIAL com gemeo nao conferido pego): {errors}")
    return True


# Controle-irmao de C4a: o MESMO cenario, mas com o gemeo REALMENTE
# presente no inventario do sistema que falta - deve contar e passar.
def selftest_prova_parcial_gemeo_present_control():
    linux_inv = {"a_test", "gpu_kind_report_smoke"}
    windows_inv = {"a_test", "gpu_kind_report_compile_win_test"}
    exceptions = [
        {
            "test_name": "gpu_kind_report_smoke",
            "missing_on": "windows",
            "gemeo": "wgl_context_adapter.cpp",
            "item": "WIN-RUNNER-PROPRIO",
            "prova_parcial_gemeo": "gpu_kind_report_compile_win_test",
        },
        # O proprio gemeo (o teste de compilacao Windows-only) precisa
        # da sua propria excecao, como qualquer nome exclusivo de um
        # lado - sem ela, o gap analysis abaixo reprovaria um segundo
        # problema, sem relacao com o que este controle prova.
        {
            "test_name": "gpu_kind_report_compile_win_test",
            "missing_on": "linux",
            "gemeo": "nenhum",
            "item": SEM_PENDENCIA,
            "prova_parcial_gemeo": None,
        },
    ]
    todo_status = parse_todo_status_text(_todo_fixture("WIN-RUNNER-PROPRIO", "🔍 Em verificação"))
    errors = run_comparison(linux_inv, windows_inv, exceptions, [], todo_status)
    if errors:
        print(f"selftest: controle PROVA-PARCIAL-PRESENTE FALHOU (deveria ter passado): {errors}", file=sys.stderr)
        return False
    print("selftest: controle PROVA-PARCIAL-PRESENTE OK (gemeo conferido no inventario, aceito)")
    return True


def selftest_main():
    controls = [
        selftest_positive_control(),
        selftest_unregistered_gap_reproves(),
        selftest_exception_pointing_to_concluded_item_reproves(),
        selftest_exception_pointing_to_pending_item_passes(),
        selftest_empty_inventory_reproves(),
        selftest_empty_inventory_windows_reproves(),
        selftest_alias_control(),
        selftest_alias_shared_windows_partner_control(),
        selftest_sem_pendencia_control(),
        selftest_sem_pendencia_with_gemeo_reproves(),
        selftest_unknown_item_reproves(),
        selftest_parsing_round_trip(),
        selftest_ctest_project_header_not_swallowed(),
        selftest_ctest_header_only_yields_empty_inventory(),
        selftest_mixed_ctest_and_clean_list_control(),
        selftest_corrupted_inventory_line_reproves(),
        selftest_alias_dead_reproves(),
        selftest_alias_half_dead_reproves(),
        selftest_alias_bilateral_undeclared_reproves(),
        selftest_alias_bilateral_declared_control(),
        selftest_alias_bilateral_false_declaration_reproves(),
        selftest_exception_dead_gap_closed_reproves(),
        selftest_exception_dead_orphaned_reproves(),
        selftest_prova_parcial_gemeo_absent_reproves(),
        selftest_prova_parcial_gemeo_present_control(),
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
