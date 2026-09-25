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

import contextlib
import io
import os
import re
import sys
import tempfile
from dataclasses import dataclass

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

# CI-SPLIT-PER-OS A1 (docs/plano-ci-split-per-os.md secao 4.3): antes
# desta fatia so' "linux"/"windows" existiam - o modo --compare uniao
# TODAS as pernas Linux (Fedora/Ubuntu/Arch/CachyOS x compartilhado/
# estatico) num so conjunto antes de comparar contra Windows, e uma
# lacuna exclusiva de UMA distro (ex.: um teste que roda em Fedora mas
# falta so' no CachyOS) fica invisivel dentro dessa uniao - o "nome
# ainda existe do lado Linux" porque outra distro o registrou. O modo
# novo (--per-system, abaixo) compara cada sistema contra a uniao de
# TODOS, e por isso os quatro slugs de distro entram como chave valida
# de sistema tambem (tests/parity_exceptions.txt aceita as duas formas
# ao mesmo tempo: "linux"/"windows" para --compare, os slugs para
# --per-system - nenhuma exceção antiga precisa mudar).
SISTEMAS_VALIDOS = ("linux", "windows", "fedora", "ubuntu", "arch", "cachyos")

# A lista de alvos que --per-system EXIGE inventario, hoje - as cinco
# plataformas de ESCOPO.md paragrafo 1 (Fedora/Ubuntu/Arch/CachyOS/
# Windows), antes das distros do astrometrica (fatias A7a-A7e,
# PLATFORMS-ASTRO-PARITY - fora do escopo da ordem que abriu esta
# fatia). "linux" fica de fora: e' a chave AGREGADA que so' --compare
# usa (a uniao que esta fatia existe para nao fazer mais); --per-
# system nunca aceita "linux" como slug de sistema. Quando A7 acrescentar
# uma distro nova, amplie SISTEMAS_VALIDOS e este tuple juntos - os
# dois sempre andam emparelhados por construcao (ver o comentario
# acima).
SISTEMAS_PER_SYSTEM_ESPERADOS = tuple(s for s in SISTEMAS_VALIDOS if s != "linux")


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


# PARITY-ALIAS-HYGIENE P-3, revisao independente (23/09/2026, achado
# bloqueante N6): todo `except SystemExit:` deste arquivo que nunca lia
# `.code` contava "levantou ALGUMA SystemExit" como prova de
# reprovacao - um mutante que troca sys.exit(1) por sys.exit(0) aqui
# em fail() sobrevivia a TODOS os controles de guarda do arquivo,
# porque sys.exit(0) tambem levanta SystemExit (GODS_LAWS.md L-45: o
# codigo de saida se le de uma VARIAVEL, nunca se infere do simples
# fato de a excecao ter sido levantada). Um controle so conta como
# "reprovou" quando o codigo e' exatamente o esperado.
def _expect_fail_exit(callable_fn, *args, **kwargs):
    try:
        callable_fn(*args, **kwargs)
    except SystemExit as exc:
        return exc.code == 1
    return False


# --- parsing -----------------------------------------------------------


# Aceita tanto a saida crua de `ctest -N` ("  Test #12: nome_test",
# mais a linha final "Total Tests: N", que e descartada) quanto uma
# lista ja limpa de nomes, um por linha - o mesmo ARQUIVO, e ate a
# mesma UNIAO de arquivos concatenados (ver o corolario abaixo), pode
# misturar as duas sem o chamador precisar normalizar antes.
_CTEST_LINE_RE = re.compile(r"^\s*Test\s+#\d+:\s+(\S+)\s*$")

# LAYERS-GATE-GFSS-GFUI sub-fatia L-5 (docs/plano-layers-l5.md, GODS_LAWS.md
# L-40/L-45, decisao do main em DECISOES_AUTONOMAS.md 23/09/2026): CTest
# marca um teste com a propriedade SKIP_RETURN_CODE que nunca chegou a
# rodar do MESMO jeito de sempre em `ctest -N` - so que quando o teste tem
# a propriedade DISABLED (nao usada por este projeto ate esta fatia, mas
# generica do CTest), a linha ganha o sufixo LITERAL " (Disabled)". MEDIDO
# ao vivo pelo implementador de L-5 antes deste conserto existir: o parser
# de ENTAO nao reconhecia essa forma e reprovava a listagem inteira como
# "entrada de inventario nao reconhecida" - nao uma paridade vermelha
# comum, um CRASH do proprio parser, na PRIMEIRA vez que qualquer perna
# alimentasse este portao com um teste desligado. `_CTEST_DISABLED_LINE_RE`
# reconhece a forma; `parse_inventory_text()` abaixo NAO conta esses nomes
# no conjunto de comparacao (um teste desligado no CTest nao prova
# comportamento nenhum, entao nao pode "salvar" a paridade por presenca
# calada) - mas tambem nunca os descarta em silencio: quem passa um
# `disabled_collector` (real_main - ver `_load_real_main_inputs()`) recebe
# a lista, impressa SEMPRE, mesmo vazia (GODS_LAWS.md L-40).
_CTEST_DISABLED_LINE_RE = re.compile(r"^\s*Test\s+#\d+:\s+(\S+)\s+\(Disabled\)\s*$")

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
def parse_inventory_text(text, disabled_collector=None):
    """`disabled_collector`, se dado (um `set`, mutado in place): recebe o
    nome de todo teste listado com o sufixo " (Disabled)" - NUNCA entram
    em `names` (um teste desligado nao prova comportamento, entao nao
    pode contar pra paridade por presenca calada), mas tambem nunca
    desaparecem sem que quem chamou tenha PEDIDO explicitamente pra
    guarda-los (LAYERS-GATE-GFSS-GFUI L-5, GODS_LAWS.md L-40)."""
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
        m_disabled = _CTEST_DISABLED_LINE_RE.match(line)
        if m_disabled:
            if disabled_collector is not None:
                disabled_collector.add(m_disabled.group(1))
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
            f"'Test #N: nome', nem 'Test #N: nome (Disabled)', nem nome de lista limpa "
            f"em snake_case): {line!r}"
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
# PARITY-ALIAS-HYGIENE, sub-fatia P-3 (23/09/2026, GODS_LAWS.md L-17,
# achado bloqueante da revisao independente de P-2,
# /var/tmp/glintfx-plan/revisao-parity-p2.md secao 3): compute_alias_
# hygiene() cresceu de 31 para 74 linhas fisicas em P-2, estourando o
# teto duro de 40 (CONTRACT.md SS6.2). A checagem de "irmao vivo"
# estava DUPLICADA linha a linha entre o ramo linux_absent e o ramo
# windows_absent - so o dicionario e o inventario trocados - e o
# classificador por-apelido (dead/half_dead/bilateral) fazia o
# trabalho de tres perguntas ("e morto? e meio-morto, de que lado? e
# bilateral, declarado ou nao?") dentro de um laco so. Extraidos os
# tres atomos que a propria revisao nomeou: _build_alias_sibling_maps
# (o agrupamento, sem mudanca de logica), _has_live_sibling (a
# checagem repetida) e _classify_alias (a decisao por apelido,
# devolvendo so' a categoria e o lado - nunca decide O QUE FAZER com
# ela, so' fabricas menores). compute_alias_hygiene() vira so' o laco
# que despacha a categoria pro balde certo.
# PARITY-ALIAS-HYGIENE, sub-fatia P-3, 2a verificacao L-12 do
# orquestrador (23/09/2026, GODS_LAWS.md L-17, achado bloqueante da
# revisao independente: `_classify_alias` nascia com 6 parametros -
# `alias, combined_inventory, linux_inventory, windows_inventory,
# linux_to_win, win_to_linux` - estourando o teto duro de 4. Os quatro
# ultimos SEMPRE viajam juntos, nos quatro pontos onde a funcao e'
# chamada - por isso viram um unico parametro estruturado, nunca
# quatro soltos. `frozen=True` porque o contexto nao muda depois de
# montado (o mesmo espirito de imutabilidade que o dict de CONJUNTOS
# ja seguia).
@dataclass(frozen=True)
class AliasSiblingContext:
    combined_inventory: frozenset
    linux_inventory: frozenset
    windows_inventory: frozenset
    linux_to_win: dict
    win_to_linux: dict


def _build_alias_sibling_maps(aliases):
    # Dict de CONJUNTOS, nunca dict simples (GODS_LAWS.md L-40/achado
    # do lider 05/09/2026, ver o comentario de apply_aliases() acima -
    # o mesmo motivo vale aqui: mais de um apelido pode apontar pro
    # mesmo parceiro, e nenhum pode ser sobrescrito em silencio).
    linux_to_win = {}
    win_to_linux = {}
    for alias in aliases:
        linux_to_win.setdefault(alias["linux_name"], set()).add(alias["windows_name"])
        win_to_linux.setdefault(alias["windows_name"], set()).add(alias["linux_name"])
    return linux_to_win, win_to_linux


def _build_alias_sibling_context(aliases, linux_inventory, windows_inventory):
    linux_to_win, win_to_linux = _build_alias_sibling_maps(aliases)
    return AliasSiblingContext(
        combined_inventory=linux_inventory | windows_inventory,
        linux_inventory=linux_inventory,
        windows_inventory=windows_inventory,
        linux_to_win=linux_to_win,
        win_to_linux=win_to_linux,
    )


# A checagem que estava duplicada entre os dois ramos de _classify_
# alias: um candidato so' tem "irmao vivo" quando ALGUM nome do grupo
# de apelidos que aponta pro MESMO parceiro aparece DE FATO no
# inventario do lado certo (nunca no inventario do lado errado, nem so
# "o grupo nao esta vazio" - os controles C1b-siblings-*/C1b-lado-
# errado-* abaixo provam as duas distincoes por mutacao).
def _has_live_sibling(partner_name, sibling_groups, live_inventory):
    return any(name in live_inventory for name in sibling_groups.get(partner_name, ()))


# Classifica UM apelido, devolvendo (categoria, lado) - lado so' e
# usado por half_dead/half_dead_suppressed, None nos demais casos, e
# categoria None significa "apelido normal, nada a reportar" (a
# checagem de is_bilateral pode nao achar problema nenhum: par nao-
# bilateral sem declaracao e o caso comum, coberto em outro lugar por
# apply_aliases()/run_comparison(), nao aqui). So' 2 parametros -
# `alias` (o que varia por chamada) e `context` (o que viaja junto,
# imutavel, ver AliasSiblingContext acima).
def _classify_alias(alias, context):
    linux_name = alias["linux_name"]
    windows_name = alias["windows_name"]
    linux_absent = linux_name not in context.combined_inventory
    windows_absent = windows_name not in context.combined_inventory

    if linux_absent and windows_absent:
        return "dead", None
    if linux_absent:
        suppressed = _has_live_sibling(windows_name, context.win_to_linux, context.linux_inventory)
        return ("half_dead_suppressed" if suppressed else "half_dead"), "linux"
    if windows_absent:
        suppressed = _has_live_sibling(linux_name, context.linux_to_win, context.windows_inventory)
        return ("half_dead_suppressed" if suppressed else "half_dead"), "windows"

    is_bilateral = linux_name in context.windows_inventory or windows_name in context.linux_inventory
    if not is_bilateral:
        # P-2, achado IMPORTANTE: declaracao bilateral cujo fato
        # descrito nao e' verdade - nao pode passar calada so porque o
        # par nao e' candidato a lacuna hoje.
        return ("bilateral_false" if alias["bilateral_reason"] else None), None
    return ("bilateral_declared" if alias["bilateral_reason"] else "bilateral_undeclared"), None


def compute_alias_hygiene(aliases, linux_inventory, windows_inventory):
    context = _build_alias_sibling_context(aliases, linux_inventory, windows_inventory)

    dead, half_dead, half_dead_suppressed = [], [], []
    bilateral_declared, bilateral_undeclared, bilateral_false = [], [], []
    # GODS_LAWS.md L-40 (achado P-2, "metade provada vira provado"):
    # supressao por irmao vivo fica contada e visivel, nunca calada.
    buckets = {
        "dead": dead,
        "bilateral_declared": bilateral_declared,
        "bilateral_undeclared": bilateral_undeclared,
        "bilateral_false": bilateral_false,
    }

    for alias in aliases:
        category, side = _classify_alias(alias, context)
        if category is None:
            continue
        if category == "half_dead":
            half_dead.append((alias, side))
        elif category == "half_dead_suppressed":
            half_dead_suppressed.append((alias, side))
        else:
            buckets[category].append(alias)

    return (
        dead,
        bilateral_declared,
        bilateral_undeclared,
        bilateral_false,
        half_dead,
        half_dead_suppressed,
    )


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


# PARITY-ALIAS-HYGIENE, sub-fatia P-3 (23/09/2026, GODS_LAWS.md L-17,
# achado de seguimento SS4.3 da revisao de P-2): run_comparison() ja
# violava o teto de 40 linhas ANTES de P-2 (73 linhas em 5d0c173) e
# P-2 agravou (+15, os dois lacos novos de half-dead/bilateral-falso).
# Cada bloco "calcula uma categoria, formata as mensagens dela" vira
# uma funcao nomeada - a mesma receita de extracao usada em
# compute_alias_hygiene() acima. run_comparison() fica so' a ORDEM em
# que as checagens rodam, nunca a formatacao delas.
def _empty_inventory_errors(linux_inventory, windows_inventory):
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
    return errors


def _format_alias_hygiene_errors(aliases, linux_inventory, windows_inventory):
    (
        alias_dead,
        _alias_bilateral_declared,
        alias_bilateral_undeclared,
        alias_bilateral_false,
        alias_half_dead,
        _alias_half_dead_suppressed,
    ) = compute_alias_hygiene(aliases, linux_inventory, windows_inventory)

    errors = []
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
    return errors


def _format_exception_hygiene_errors(exceptions, linux_inventory, windows_inventory):
    exception_dead = compute_exception_hygiene(exceptions, linux_inventory, windows_inventory)
    return [
        f"excecao morta: {exc['test_name']} (missing_on={exc['missing_on']}) em "
        "tests/parity_exceptions.txt nao aparece em inventario nenhum, ou ja existe "
        "tambem do lado declarado como faltante - apague a linha"
        for exc in exception_dead
    ]


def _format_gap_errors(linux_only, windows_only, exception_index):
    errors = []
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


# O veredicto inteiro, como uma lista de erros - lista vazia significa
# que o portao passa. Reune os controles que --selftest exige provar
# em vermelho: piso de varredura vazia (GODS_LAWS.md L-40), excecao
# invalida (validate_exceptions acima), lacuna sem excecao
# registrada, e a higiene das duas tabelas acrescentada por PARITY-
# ALIAS-HYGIENE (apelido morto, apelido bilateral sem declaracao,
# excecao morta, PROVA-PARCIAL com gemeo nao conferido).
def run_comparison(linux_inventory, windows_inventory, exceptions, aliases, todo_status):
    errors = _empty_inventory_errors(linux_inventory, windows_inventory)
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
    errors.extend(_format_alias_hygiene_errors(aliases, linux_inventory, windows_inventory))
    errors.extend(_format_exception_hygiene_errors(exceptions, linux_inventory, windows_inventory))
    errors.extend(validate_prova_parcial(exceptions, linux_inventory, windows_inventory))

    exception_index = {(exc["test_name"], exc["missing_on"]) for exc in exceptions}
    errors.extend(_format_gap_errors(linux_only, windows_only, exception_index))

    return errors


# --- real mode -------------------------------------------------------


def _read_file(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        fail(f"arquivo nao encontrado: {path} ({exc})")


# PARITY-ALIAS-HYGIENE P-3 (23/09/2026, GODS_LAWS.md L-17, achado da
# verificacao L-12 do orquestrador): real_main() estava em 49 linhas
# fisicas em `5d0c173` e `07499e3` (fundacao/P-2 - nao violava) e
# chegou a 57 nesta fatia - dividido por responsabilidade, o mesmo
# criterio que a revisao original usou pra `compute_alias_hygiene()`:
# ler os argumentos, carregar os cinco arquivos de entrada, imprimir
# as contagens (o piso L-40), e decidir o codigo de saida. Cada parte
# nomeada, real_main() fica so' a ORDEM em que elas rodam.
#
# 2a verificacao L-12 (achado bloqueante da revisao independente):
# `_print_real_main_counts` nascia com 5 parametros soltos - os
# mesmos cinco que `_load_real_main_inputs` ja devolve juntos.
# Agrupados numa unica estrutura nomeada, o mesmo remedio de
# AliasSiblingContext acima.
@dataclass(frozen=True)
class RealMainInputs:
    linux_inventory: frozenset
    windows_inventory: frozenset
    linux_disabled: frozenset
    windows_disabled: frozenset
    exceptions: list
    aliases: list
    todo_status: dict


def _parse_real_main_args(args):
    if len(args) != 5:
        fail(
            "usage: check_test_parity.py --compare <inv-linux> <inv-windows> "
            "<exceptions.txt> <aliases.txt> <TODO.md>"
        )
    return args


def _load_real_main_inputs(args):
    linux_inv_path, windows_inv_path, exceptions_path, aliases_path, todo_path = args
    linux_disabled = set()
    windows_disabled = set()
    linux_inventory = parse_inventory_text(_read_file(linux_inv_path), linux_disabled)
    windows_inventory = parse_inventory_text(_read_file(windows_inv_path), windows_disabled)
    return RealMainInputs(
        linux_inventory=linux_inventory,
        windows_inventory=windows_inventory,
        linux_disabled=frozenset(linux_disabled),
        windows_disabled=frozenset(windows_disabled),
        exceptions=parse_exceptions_text(_read_file(exceptions_path)),
        aliases=parse_aliases_text(_read_file(aliases_path)),
        todo_status=parse_todo_status_text(_read_file(todo_path)),
    )


# PARITY-ALIAS-HYGIENE (TODO.md W7-C, GODS_LAWS.md L-40): as tres
# linhas abaixo imprimem SEMPRE, ganhe ou perca o portao - o piso de
# varredura exige a contagem, nao so o veredicto. Calculadas aqui (nao
# dentro de run_comparison) porque valem mesmo quando um inventario
# vem vazio: nesse caso todo apelido/excecao aparece "morto" por
# definicao (nao ha nada em inventario nenhum para bater), o que e'
# verdade honesta, nao um segundo erro de varredura vazia disfarcado -
# a lista de erros que decide REPROVADO continua vindo so de
# run_comparison().
def _print_real_main_counts(inputs):
    print(
        f"{SCRIPT_NAME}: inventario Linux={len(inputs.linux_inventory)} teste(s), "
        f"Windows={len(inputs.windows_inventory)} teste(s), "
        f"{len(inputs.exceptions)} excecao(oes), {len(inputs.aliases)} apelido(s), "
        f"{len(inputs.todo_status)} item(ns) lido(s) de TODO.md"
    )
    # LAYERS-GATE-GFSS-GFUI L-5 (GODS_LAWS.md L-40): piso de varredura
    # impresso SEMPRE, ganhe ou perca o portao, mesmo com as duas
    # contagens em zero - um teste "(Disabled)" nunca conta pra
    # paridade (nao prova comportamento), mas tambem nunca some calado.
    print(
        f"{SCRIPT_NAME}: desligados (Disabled) Linux={len(inputs.linux_disabled)} teste(s) "
        f"{sorted(inputs.linux_disabled)}, Windows={len(inputs.windows_disabled)} teste(s) "
        f"{sorted(inputs.windows_disabled)}"
    )
    (
        alias_dead,
        alias_bilateral_declared,
        alias_bilateral_undeclared,
        alias_bilateral_false,
        alias_half_dead,
        alias_half_dead_suppressed,
    ) = compute_alias_hygiene(inputs.aliases, inputs.linux_inventory, inputs.windows_inventory)
    print(
        f"{SCRIPT_NAME}: {len(inputs.aliases)} apelido(s), {len(alias_dead)} morto(s), "
        f"{len(alias_bilateral_declared)} bilateral(is) declarado(s), "
        f"{len(alias_bilateral_undeclared)} bilateral(is) sem declaracao, "
        f"{len(alias_bilateral_false)} bilateral(is) falso(s), "
        f"{len(alias_half_dead)} meio-morto(s), "
        f"{len(alias_half_dead_suppressed)} meio-morto(s) suprimido(s) por irmao vivo"
    )
    exception_dead = compute_exception_hygiene(inputs.exceptions, inputs.linux_inventory, inputs.windows_inventory)
    print(f"{SCRIPT_NAME}: {len(inputs.exceptions)} excecao(oes), {len(exception_dead)} morta(s)")


def _exit_with_verdict(errors, success_message="paridade OK - nenhuma lacuna sem excecao registrada"):
    if errors:
        print(f"{SCRIPT_NAME}: REPROVADO ({len(errors)} problema(s)):", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        sys.exit(1)
    print(f"{SCRIPT_NAME}: {success_message}")


def real_main(args):
    args = _parse_real_main_args(args)
    inputs = _load_real_main_inputs(args)
    _print_real_main_counts(inputs)
    errors = run_comparison(
        inputs.linux_inventory, inputs.windows_inventory, inputs.exceptions, inputs.aliases, inputs.todo_status
    )
    _exit_with_verdict(errors)


# --- textual mode (PARITY-LOCAL-MIRROR, TODO.md W7-C, GODS_LAWS.md ------
# L-04/L-17/L-40) ----------------------------------------------------------
#
# --compare (real_main acima) so roda contra o servidor: precisa do
# inventario `ctest -N` REAL dos dois sistemas, que so existe dentro de
# um run de CI. Duas vezes em dois dias (N2, N3 - TODO.md,
# docs/plano-w7c-adendo-revalidacao.md SS3.I) a regra "excecao aponta
# para item concluido reprova" (validate_exceptions() acima) so mordeu
# la, nunca nesta maquina, porque nada localmente chama essa funcao
# contra dado real - so' o --selftest, com fixture sintetica.
#
# --textual fecha a METADE que nao depende de inventario nenhum: a
# FORMA de cada linha de tests/parity_exceptions.txt e tests/
# parity_aliases.txt (parse_exceptions_text/parse_aliases_text ja
# reprovam malformado via fail(), reusadas aqui sem copiar - GODS_LAWS.md
# L-17), e o item de cada excecao contra o Status de TODO.md
# (validate_exceptions(), a MESMA funcao que --compare chama, nunca uma
# copia). O que fica de fora, declarado (GODS_LAWS.md L-40: um portao
# vendido como mais forte do que e' vira falso conforto): lacuna de
# INVENTARIO (teste que existe de um lado e falta do outro sem excecao)
# so' o servidor prova, porque o inventario do sistema que falta nesta
# maquina nao existe aqui - tentar prever lendo o CMakeLists ja foi
# medido enganoso ("ctest -N mede so metade").
@dataclass(frozen=True)
class TextualMainInputs:
    exceptions: list
    aliases: list
    todo_status: dict


def _parse_textual_main_args(args):
    if len(args) != 3:
        fail(
            "usage: check_test_parity.py --textual <exceptions.txt> <aliases.txt> <TODO.md>"
        )
    return args


def _load_textual_main_inputs(args):
    exceptions_path, aliases_path, todo_path = args
    return TextualMainInputs(
        exceptions=parse_exceptions_text(_read_file(exceptions_path)),
        aliases=parse_aliases_text(_read_file(aliases_path)),
        todo_status=parse_todo_status_text(_read_file(todo_path)),
    )


# GODS_LAWS.md L-40: piso impresso SEMPRE, mesmo com tudo zerado - as
# contagens de excecoes e apelidos podem legitimamente ser zero (um
# projeto sem nenhuma lacuna aceita hoje), entao nao entram no piso de
# reprovacao abaixo; so a contagem de itens de TODO.md entra (ver
# _textual_empty_scan_errors).
def _print_textual_main_counts(inputs):
    print(
        f"{SCRIPT_NAME} --textual: {len(inputs.exceptions)} excecao(oes), "
        f"{len(inputs.aliases)} apelido(s), {len(inputs.todo_status)} item(ns) lido(s) de TODO.md"
    )


# TODO.md tem, hoje e em toda execucao real deste portao, centenas de
# linhas de item - ler zero e sinal de coleta quebrada (caminho errado,
# arquivo truncado, formato de tabela mudou sob o pe), nunca "tabela
# genuinamente vazia" (GODS_LAWS.md L-40, o mesmo piso que
# _empty_inventory_errors aplica ao inventario do lado --compare).
def _textual_empty_scan_errors(inputs):
    if not inputs.todo_status:
        return [
            "varredura vazia: TODO.md nao tem item de tabela nenhum reconhecido - "
            "GODS_LAWS.md L-40, isto e sinal de leitura quebrada, nunca de tabela vazia"
        ]
    return []


def textual_main(args):
    args = _parse_textual_main_args(args)
    inputs = _load_textual_main_inputs(args)
    _print_textual_main_counts(inputs)
    errors = _textual_empty_scan_errors(inputs)
    errors.extend(validate_exceptions(inputs.exceptions, inputs.todo_status))
    _exit_with_verdict(
        errors,
        "modo --textual OK - nenhuma excecao aponta para item concluido, nenhuma linha "
        "malformada (lacuna de INVENTARIO nao conferida aqui, so no servidor)",
    )


# --- per-system mode (CI-SPLIT-PER-OS A1, docs/plano-ci-split-per-os.md ---
# secao 4.3, C2/C3) -------------------------------------------------------
#
# O DEFEITO QUE ESTA FATIA CONSERTA: --compare (real_main() acima) une
# TODAS as pernas Linux (Fedora/Ubuntu/Arch/CachyOS x compartilhado/
# estatico) num so conjunto antes de comparar contra a uniao Windows -
# ele compara SISTEMAS OPERACIONAIS como um todo (Linux x Windows),
# nunca DISTRO CONTRA DISTRO. Uma lacuna exclusiva de uma UNICA distro
# (um teste que roda em Fedora/Ubuntu/Arch mas falta so' no CachyOS)
# fica invisivel: o nome "existe do lado Linux" porque as outras tres
# distros o registraram, entao a uniao nunca nota a distro que faltou.
# --per-system fecha esse buraco: a UNIAO DE REFERENCIA passa a ser a
# uniao de TODOS os sistemas (as quatro distros MAIS Windows, nunca so'
# dois lados), e CADA sistema e' comparado contra ela por conta propria
# - uma lacuna exclusiva do CachyOS reprova citando "cachyos", nao
# "Linux" generico.
#
# LIMITACAO DECLARADA DESTA FATIA (GODS_LAWS.md L-40: portao vendido
# como mais forte do que e' vira falso conforto): --per-system NAO
# aplica tests/parity_aliases.txt (apelido cross-sistema) nem a higiene
# de apelido/excecao "morta" que --compare ja aplica - as duas foram
# desenhadas para o par Linux/Windows (um nome por LADO, nao por
# distro) e generaliza-las para N sistemas e' desenho que esta fatia
# (A1) nao recebeu especificacao para fazer; --per-system so' aplica a
# regra de morte de excecao contra item concluido (validate_exceptions(),
# a MESMA funcao que --compare ja usa) e a comparacao de presenca por
# sistema. Se um teste precisar de nome diferente por sistema um dia,
# essa extensao volta ao planejamento antes de ser escrita aqui.
@dataclass(frozen=True)
class PerSystemInputs:
    system_inventories: dict  # {slug: frozenset(nomes)}
    exceptions: list
    todo_status: dict


def _parse_per_system_args(args):
    if len(args) < 3:
        fail(
            "usage: check_test_parity.py --per-system <exceptions.txt> <TODO.md> "
            "<slug>=<inventario> [<slug>=<inventario> ...]"
        )
    exceptions_path, todo_path = args[0], args[1]
    system_paths = {}
    for entry in args[2:]:
        if "=" not in entry:
            fail(
                f"--per-system: argumento de sistema malformado (esperava 'slug=caminho'): {entry!r}"
            )
        slug, path = entry.split("=", 1)
        if slug not in SISTEMAS_PER_SYSTEM_ESPERADOS:
            fail(
                f"--per-system: slug de sistema invalido {slug!r} (validos: "
                f"{SISTEMAS_PER_SYSTEM_ESPERADOS})"
            )
        if slug in system_paths:
            fail(f"--per-system: slug {slug!r} repetido")
        system_paths[slug] = path
    return exceptions_path, todo_path, system_paths


def _load_per_system_inputs(exceptions_path, todo_path, system_paths):
    system_inventories = {
        slug: frozenset(parse_inventory_text(_read_file(path))) for slug, path in system_paths.items()
    }
    return PerSystemInputs(
        system_inventories=system_inventories,
        exceptions=parse_exceptions_text(_read_file(exceptions_path)),
        todo_status=parse_todo_status_text(_read_file(todo_path)),
    )


def _print_per_system_counts(inputs):
    for slug in sorted(inputs.system_inventories):
        print(
            f"{SCRIPT_NAME} --per-system: sistema {slug!r} = "
            f"{len(inputs.system_inventories[slug])} teste(s)"
        )
    print(
        f"{SCRIPT_NAME} --per-system: {len(inputs.system_inventories)} sistema(s) recebido(s), "
        f"{len(inputs.exceptions)} excecao(oes), {len(inputs.todo_status)} item(ns) lido(s) de TODO.md"
    )


# GODS_LAWS.md L-04 item 2 ("fatia nao fecha com paridade parcial") +
# L-40 (piso de varredura nao-vazia): um sistema da lista de alvos SEM
# inventario nenhum (nunca recebeu --per-system slug=caminho) e' o
# mutante nomeado no proprio plano - "piso de sistemas desligado -> um
# sistema sem inventario passa calado" - e tem de reprovar citando o
# slug que faltou, nunca ser pulado em silencio. Inventario recebido
# mas VAZIO (0 testes) e' o mesmo defeito que _empty_inventory_errors()
# ja cobre para --compare, generalizado por sistema aqui.
def _per_system_piso_errors(system_inventories):
    errors = []
    recebidos = set(system_inventories)
    esperados = set(SISTEMAS_PER_SYSTEM_ESPERADOS)
    faltando = sorted(esperados - recebidos)
    if faltando:
        errors.append(
            "piso de sistemas (GODS_LAWS.md L-40): sistema(s) da lista de alvos sem "
            f"inventario nenhum recebido, nunca pulado(s) em silencio: {faltando}"
        )
    inesperados = sorted(recebidos - esperados)
    if inesperados:
        errors.append(
            f"sistema(s) fora da lista de alvos deste portao ({SISTEMAS_PER_SYSTEM_ESPERADOS}): "
            f"{inesperados} - amplie SISTEMAS_PER_SYSTEM_ESPERADOS antes de usar um slug novo"
        )
    for slug in sorted(system_inventories):
        if not system_inventories[slug]:
            errors.append(
                f"varredura vazia: o inventario do sistema {slug!r} tem 0 testes - "
                "GODS_LAWS.md L-40, isto e sinal de coleta quebrada, nunca de paridade"
            )
    return errors


# CI-SPLIT-PER-OS A1, achado da classificacao dos 215 problemas do run
# 36085750444 (categoria "a", 8 pares/2 nomes): uma excecao com
# missing_on="linux" e' o AGREGADO que --compare ja usava ANTES de
# --per-system existir - ela cobria TODA distro Linux de uma vez, nunca
# uma so'. Sem esta expansao, toda excecao linux/windows pre-existente
# (tests/parity_exceptions.txt inteiro) parava de cobrir qualquer coisa
# no modo por sistema, no dia em que ele nasceu.
FAMILIA_POR_SLUG = {
    "linux": frozenset(s for s in SISTEMAS_PER_SYSTEM_ESPERADOS if s != "windows"),
    "windows": frozenset({"windows"}),
}


def _exception_covers_slug(nome, slug, exception_index):
    """Uma excecao cobre um slug se citar o slug exato OU a familia
    dele (FAMILIA_POR_SLUG acima) - `missing_on="windows"` NUNCA cobre
    um slug Linux, e vice-versa (a familia errada nao cobre nada)."""
    if (nome, slug) in exception_index:
        return True
    return any(
        slug in slugs_da_familia and (nome, familia) in exception_index
        for familia, slugs_da_familia in FAMILIA_POR_SLUG.items()
    )


def compute_per_system_union_and_gaps(system_inventories, exceptions):
    """Retorna (uniao, lacunas) - `uniao` e' o conjunto de referencia
    (todo nome que aparece em QUALQUER sistema); `lacunas` e' {slug:
    [nomes]} com todo nome da uniao que falta EXATAMENTE nesse sistema
    e nao tem excecao (test_name, slug) NEM (test_name, familia-do-
    slug) em tests/parity_exceptions.txt."""
    uniao = set()
    for nomes in system_inventories.values():
        uniao |= nomes
    exception_index = {(exc["test_name"], exc["missing_on"]) for exc in exceptions}
    lacunas = {}
    for slug, nomes in system_inventories.items():
        faltando = sorted(uniao - nomes)
        lacunas[slug] = [
            nome for nome in faltando if not _exception_covers_slug(nome, slug, exception_index)
        ]
    return uniao, lacunas


def _format_per_system_gap_errors(lacunas):
    errors = []
    for slug in sorted(lacunas):
        for nome in lacunas[slug]:
            errors.append(
                f"{nome}: existe na uniao de todos os sistemas e falta em {slug!r}, sem "
                f"excecao registrada em tests/parity_exceptions.txt (linha "
                f"'{nome}|{slug}|<gemeo>|<item>')"
            )
    return errors


def run_per_system_comparison(system_inventories, exceptions, todo_status):
    errors = _per_system_piso_errors(system_inventories)
    if errors:
        # Sistema faltando ou vazio: a uniao ficaria mentirosa (um
        # nome pareceria faltar em TODO MUNDO menos quem tem), entao
        # nao compara - o mesmo desvio que run_comparison() ja faz
        # quando um dos dois lados vem vazio.
        return errors
    errors.extend(validate_exceptions(exceptions, todo_status))
    _uniao, lacunas = compute_per_system_union_and_gaps(system_inventories, exceptions)
    errors.extend(_format_per_system_gap_errors(lacunas))
    return errors


def per_system_main(args):
    exceptions_path, todo_path, system_paths = _parse_per_system_args(args)
    inputs = _load_per_system_inputs(exceptions_path, todo_path, system_paths)
    _print_per_system_counts(inputs)
    errors = run_per_system_comparison(inputs.system_inventories, inputs.exceptions, inputs.todo_status)
    _exit_with_verdict(
        errors,
        "modo --per-system OK - nenhum sistema tem lacuna sem excecao registrada para ele",
    )


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


# GODS_LAWS.md L-17 P-3, 3a verificacao L-12 (achado bloqueante N7 da
# revisao independente, DIVIDA DA FUNDACAO desde `5d0c173` - a mesma
# doenca que motivou a fatia inteira, achada agora numa familia que
# P-2/P-3 nunca tocaram): VERMELHO#1 acima so' confere que o NOME do
# teste aparece na mensagem, nunca a DIRECAO. Um mutante que troca o
# texto "existe no inventario Linux e falta no Windows"/"existe no
# inventario Windows e falta no Linux" entre os dois lacos de
# _format_gap_errors() sobrevive - o nome continua la', so' a direcao
# mentiria. Este controle tem lacuna dos DOIS lados AO MESMO TEMPO,
# com nomes distintos, e confere a frase inteira de cada lado.
def selftest_gap_message_names_correct_direction_reproves():
    linux_inv = {"a_test", "somente_linux_direcao_test"}
    windows_inv = {"a_test", "somente_windows_direcao_test"}
    errors = run_comparison(linux_inv, windows_inv, [], [], {})
    linux_ok = any(
        "somente_linux_direcao_test" in e and "existe no inventario Linux e falta no Windows" in e
        for e in errors
    )
    windows_ok = any(
        "somente_windows_direcao_test" in e and "existe no inventario Windows e falta no Linux" in e
        for e in errors
    )
    if not (linux_ok and windows_ok):
        print(
            f"selftest: GAP-DIRECAO FALHOU (a mensagem de lacuna nao nomeia a direcao certa "
            f"para cada lado - texto pode ter trocado de lugar entre os dois lacos de "
            f"_format_gap_errors): {errors}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: GAP-DIRECAO OK (lacuna Linux-falta-Windows e Windows-falta-Linux nao "
        "trocam de texto entre si)"
    )
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
    if _expect_fail_exit(parse_inventory_text, garbled_text):
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


# LAYERS-GATE-GFSS-GFUI L-5 (GODS_LAWS.md L-40/L-45, DECISOES_AUTONOMAS.md
# 23/09/2026): CTest marca um teste registrado com SKIP_RETURN_CODE mas
# tambem com a propriedade DISABLED (generica do CTest) com o sufixo " (Disabled)"
# em `ctest -N`. MEDIDO ao vivo pelo implementador de L-5, ANTES deste
# conserto existir: essa linha derrubava `parse_inventory_text()` inteiro
# com "linha de inventario nao reconhecida", nao uma paridade vermelha
# comum - um CRASH do proprio parser. Este controle prova as DUAS metades:
# (1) a forma NOVA e' reconhecida sem quebrar, fica fora de `names` (um
# teste desligado nao prova comportamento) e aparece no disabled_collector
# quando pedido; (2) reproduz, sem reimplementar o parser inteiro, que a
# MESMA linha bateria em fail() se a regra nova nao existisse - a mesma
# forma do crash medido ao vivo.
def _check_disabled_line_parsed():
    """Metade 1: a linha `(Disabled)` e' reconhecida sem quebrar o
    parse, fica FORA de `names` e DENTRO do `disabled_collector`."""
    text = (
        "Test project /__w/GlintFx/GlintFx/build-shared\n"
        "  Test #1: layers_test\n"
        "  Test #2: layers_oracle_test (Disabled)\n"
        "Total Tests: 2\n"
    )
    disabled = set()
    names = parse_inventory_text(text, disabled)
    if names != {"layers_test"}:
        print(
            f"selftest: DISABLED-LINE FALHOU (names esperava so' layers_test, veio {names})",
            file=sys.stderr,
        )
        return False
    if disabled != {"layers_oracle_test"}:
        print(
            f"selftest: DISABLED-LINE FALHOU (disabled_collector esperava "
            f"layers_oracle_test, veio {disabled})",
            file=sys.stderr,
        )
        return False
    return True


def _check_disabled_line_would_crash_without_fix():
    """Metade 2: reproduz a forma do parser DE ANTES desta fatia (sem
    `_CTEST_DISABLED_LINE_RE`) - confere que a MESMA linha bateria em
    NENHUMA outra forma reconhecida, ou seja, cairia em `fail()` no
    parser real, exatamente como foi medido ao vivo (achado da revisao
    independente, 23/09/2026: extraida da funcao principal pra manter
    as duas sob o teto de 40 linhas de L-17)."""
    problem_line = "  Test #2: layers_oracle_test (Disabled)"
    old_form_matches = bool(_CTEST_LINE_RE.match(problem_line))
    also_clean_list = bool(_CLEAN_LIST_NAME_RE.match(problem_line.strip()))
    if old_form_matches or also_clean_list:
        print(
            "selftest: DISABLED-LINE FALHOU (o mutante nao reproduziu o crash - a "
            "linha bateria em outra forma reconhecida, o que invalidaria a prova)",
            file=sys.stderr,
        )
        return False
    return True


def selftest_disabled_line_recognized_not_swallowed():
    ok = _check_disabled_line_parsed() and _check_disabled_line_would_crash_without_fix()
    if ok:
        print(
            "selftest: DISABLED-LINE OK (reconhecido sem quebrar o parse, fora de "
            "'names', dentro do disabled_collector; confirmado que SEM esta regra a "
            "mesma linha cairia em fail() - o crash real medido pelo implementador de L-5)"
        )
    return ok


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
#
# CONSERTO DA TAUTOLOGIA (P-3, 23/09/2026, achado bloqueante da
# revisao independente de P-2, /var/tmp/glintfx-plan/revisao-parity-
# p2.md secao 2.3): a versao anterior usava fixtures chamadas
# "meio_morto_linux_ok_test"/"meio_morto_windows_apagado_test" e
# conferia "linux" in e / "windows" in e na MENSAGEM formatada - a
# asserção passava so' porque o proprio NOME DO APELIDO contem a
# substring do lado, nunca porque checou o lado de verdade (provado
# por mutacao: trocar o rotulo do lado sobrevivia aos 31 controles
# antigos). O conserto tem duas partes, as duas obrigatorias: (1) os
# nomes de fixture daqui pra baixo NUNCA contem "linux"/"windows" como
# substring; (2) a asserção confere o `side` ESTRUTURADO que
# compute_alias_hygiene() devolve (chamado direto, sem passar pela
# formatacao de string de run_comparison()), por igualdade exata -
# nunca por substring de mensagem.
# Helper comum aos seis controles C1b* abaixo (P-3, verificacao L-12
# rodada 2 do orquestrador): confere as DUAS coisas que a rodada 2
# achou que o "side" estruturado sozinho nao prova - (1) a mensagem
# CITA o apelido certo, (2) o TEXTO dela nomeia o lado certo na frase
# exata "tem o lado X ausente" (nunca substring solta, que colidiria
# com o "(nem Linux, nem Windows)" fixo de toda mensagem meio-morta).
# Um mutante que troca so' o {missing_side} da FORMATACAO (nunca o
# dado que compute_alias_hygiene() devolve) sobrevivia sem isto.
def _check_half_dead_message(errors, alias_substring, expected_side, label):
    if not any("apelido meio-morto" in e and alias_substring in e for e in errors):
        print(
            f"selftest: {label} FALHOU (reprovou, mas nao citou o apelido meio-morto): {errors}",
            file=sys.stderr,
        )
        return False
    if not any(f"tem o lado {expected_side} ausente" in e for e in errors):
        print(
            f"selftest: {label} FALHOU (o TEXTO da mensagem nao nomeia o lado "
            f"{expected_side!r} - so' o dado estruturado nao basta): {errors}",
            file=sys.stderr,
        )
        return False
    return True


# Variante para os controles com DOIS apelidos meio-mortos do mesmo
# lado (C1b-siblings-*): cada substring precisa aparecer numa mensagem
# PROPRIA que tambem nomeia o lado certo no texto - devolve quantos
# dos `alias_substrings` bateram os dois criterios juntos.
def _count_half_dead_messages(errors, alias_substrings, expected_side):
    phrase = f"tem o lado {expected_side} ausente"
    return sum(
        1
        for substring in alias_substrings
        if any("apelido meio-morto" in e and substring in e and phrase in e for e in errors)
    )


def selftest_alias_half_dead_reproves():
    linux_inv = {"a_test", "alfa_presente_test"}
    windows_inv = {"a_test"}
    aliases = [_alias_fixture("alfa_presente_test", "alfa_sumido_test")]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C1b FALHOU (apelido meio-morto deveria ter reprovado)", file=sys.stderr)
        return False
    if not _check_half_dead_message(errors, "alfa_presente_test", "windows", "PARITY-ALIAS-HYGIENE C1b"):
        return False
    _dead, _decl, _undecl, _false, half_dead, _supp = compute_alias_hygiene(
        aliases, linux_inv, windows_inv
    )
    if half_dead != [(aliases[0], "windows")]:
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b FALHOU (lado estruturado errado - esperava "
            f"'windows', veio {half_dead}): rotulo trocado nao morre mais so por substring",
            file=sys.stderr,
        )
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C1b OK (apelido meio-morto pego, lado 'windows' conferido por valor): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C1b-linux (VERMELHO, achado do orquestrador,
# verificacao L-12 sobre c1de2ad, familia "metade provada vira
# provado" - feedback_metade_provada_vira_provado.md): C1b acima so'
# exercita o ramo `windows_absent` de compute_alias_hygiene (lado
# Windows apagado, lado Linux vivo). O ramo IRMAO, `linux_absent`
# (lado Linux apagado, lado Windows vivo), nunca tinha controle
# proprio - um mutante que troca a checagem real de pertencimento
# (`name in linux_inventory for name in win_to_linux.get(...)`) por
# `True for name in ...` SOBREVIVE contra os 25 controles anteriores,
# porque o proprio apelido sempre aparece no seu proprio grupo
# (`win_to_linux[windows_name]` inclui `linux_name`) - o mutante conta
# a si mesmo como "irmao vivo" sem nunca checar o inventario de
# verdade. Esperado: reprova, com a categoria "apelido meio-morto" e
# o lado "linux" nomeado - conferido por VALOR (side, P-3), nomes de
# fixture sem "linux"/"windows" embutido (ver comentario de C1b acima).
def selftest_alias_half_dead_linux_side_reproves():
    linux_inv = {"a_test"}
    windows_inv = {"a_test", "beta_presente_test"}
    aliases = [_alias_fixture("beta_sumido_test", "beta_presente_test")]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C1b-linux FALHOU (apelido meio-morto deveria ter reprovado)", file=sys.stderr)
        return False
    if not _check_half_dead_message(errors, "beta_sumido_test", "linux", "PARITY-ALIAS-HYGIENE C1b-linux"):
        return False
    _dead, _decl, _undecl, _false, half_dead, _supp = compute_alias_hygiene(
        aliases, linux_inv, windows_inv
    )
    if half_dead != [(aliases[0], "linux")]:
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b-linux FALHOU (lado estruturado errado - "
            f"esperava 'linux', veio {half_dead})",
            file=sys.stderr,
        )
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C1b-linux OK (ramo linux_absent pego, lado 'linux' conferido por valor): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C1b-siblings-linux (VERMELHO, mesma
# verificacao L-12): a supressao por irmao vivo tem que checar
# PERTENCIMENTO REAL no inventario, nunca so' "o grupo nao esta
# vazio". Aqui DOIS apelidos Linux, nenhum dos dois existe em lugar
# nenhum, apontam para o MESMO parceiro Windows vivo - um mutante que
# conta qualquer nome do grupo como prova de vida (em vez de checar se
# ele esta' de fato no inventario) suprimiria os DOIS em silencio,
# porque cada um "encontraria" o outro (tambem morto) no mesmo grupo.
# Esperado: os dois reprovam como meio-mortos, nenhum suprimido. Nomes
# de fixture sem "linux"/"windows" embutido, side conferido por valor
# (P-3, ver comentario de C1b acima).
def selftest_alias_half_dead_linux_dead_siblings_not_counted_reproves():
    linux_inv = {"a_test"}
    windows_inv = {"a_test", "gama_presente_test"}
    aliases = [
        _alias_fixture("gama_orfa_um_test", "gama_presente_test"),
        _alias_fixture("gama_orfa_dois_test", "gama_presente_test"),
    ]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    ok_count = _count_half_dead_messages(errors, ["gama_orfa_um_test", "gama_orfa_dois_test"], "linux")
    if ok_count != 2:
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b-siblings-linux FALHOU (esperava os dois "
            f"irmaos mortos reprovando com 'tem o lado linux ausente' cada um, achou "
            f"{ok_count}): {errors}",
            file=sys.stderr,
        )
        return False
    _dead, _decl, _undecl, _false, half_dead, _supp = compute_alias_hygiene(
        aliases, linux_inv, windows_inv
    )
    sides = {alias["linux_name"]: side for alias, side in half_dead}
    if sides != {"gama_orfa_um_test": "linux", "gama_orfa_dois_test": "linux"}:
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b-siblings-linux FALHOU (lado estruturado "
            f"errado, esperava os dois em 'linux', veio {sides})",
            file=sys.stderr,
        )
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C1b-siblings-linux OK (irmao morto nao conta como vivo, lado conferido por valor): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C1b-siblings-windows: gemeo direto do controle
# acima no OUTRO ramo (`windows_absent`) - o mesmo defeito, o mesmo
# remedio, no lado espelhado (GODS_LAWS.md L-17). Um nome Linux vivo
# com DOIS apelidos Windows, nenhum dos dois existe em lugar nenhum.
def selftest_alias_half_dead_windows_dead_siblings_not_counted_reproves():
    linux_inv = {"a_test", "delta_presente_test"}
    windows_inv = {"a_test"}
    aliases = [
        _alias_fixture("delta_presente_test", "delta_orfa_um_test"),
        _alias_fixture("delta_presente_test", "delta_orfa_dois_test"),
    ]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    ok_count = _count_half_dead_messages(errors, ["delta_orfa_um_test", "delta_orfa_dois_test"], "windows")
    if ok_count != 2:
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b-siblings-windows FALHOU (esperava os dois "
            f"irmaos mortos reprovando com 'tem o lado windows ausente' cada um, achou "
            f"{ok_count}): {errors}",
            file=sys.stderr,
        )
        return False
    _dead, _decl, _undecl, _false, half_dead, _supp = compute_alias_hygiene(
        aliases, linux_inv, windows_inv
    )
    sides = {alias["windows_name"]: side for alias, side in half_dead}
    if sides != {"delta_orfa_um_test": "windows", "delta_orfa_dois_test": "windows"}:
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b-siblings-windows FALHOU (lado estruturado "
            f"errado, esperava os dois em 'windows', veio {sides})",
            file=sys.stderr,
        )
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C1b-siblings-windows OK (irmao morto nao conta como vivo, lado conferido por valor): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C1b-lado-errado-linux (VERMELHO, achado do
# orquestrador, segunda rodada de verificacao L-12 sobre c77b215): os
# dois mutantes "True for name in ..." morrem agora (C1b-linux/
# C1b-siblings-*), mas um mutante de OUTRA familia sobrevivia contra
# os 29 controles anteriores - trocar `name in linux_inventory` (linha
# 496) por `name in (linux_inventory | windows_inventory)` (a UNIAO)
# deixa um "irmao" cujo nome so existe do lado WINDOWS contar como
# prova de vida do lado LINUX, o que e' logicamente impossivel (um
# nome que so aparece no inventario Windows nunca rodou como teste
# Linux - ele nao da NENHUMA cobertura Linux ao parceiro). O irmao
# "epsilon_fantasma_test" aqui existe de verdade (aparece em
# windows_inv, entao nao e' ele mesmo um apelido morto - e' um segundo
# apelido legitimo bilateral, so' para forjar o grupo de irmaos do
# parceiro), mas NUNCA no lado Linux - nao pode suprimir o meio-morto
# de epsilon_orfa_test. Esperado: reprova, "apelido meio-morto" com o
# lado "linux" (conferido por VALOR, P-3), apesar do irmao existir do
# lado errado. Nome de fixture sem "linux"/"windows" embutido.
def selftest_alias_half_dead_linux_sibling_must_be_linux_side_reproves():
    linux_inv = {"a_test"}
    windows_inv = {"a_test", "epsilon_presente_test", "epsilon_fantasma_test"}
    aliases = [
        _alias_fixture("epsilon_orfa_test", "epsilon_presente_test"),
        _alias_fixture(
            "epsilon_fantasma_test",
            "epsilon_presente_test",
            bilateral_reason=(
                "fixture de teste - este nome existe so do lado windows, usado so "
                "para forjar o grupo de irmaos do parceiro (nao e' o que este "
                "controle prova, so' precisa nao gerar erro proprio)"
            ),
        ),
    ]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if not _check_half_dead_message(
        errors, "epsilon_orfa_test", "linux", "PARITY-ALIAS-HYGIENE C1b-lado-errado-linux"
    ):
        return False
    _dead, _decl, _undecl, _false, half_dead, _supp = compute_alias_hygiene(
        aliases, linux_inv, windows_inv
    )
    half_dead_orfa = [side for alias, side in half_dead if alias["linux_name"] == "epsilon_orfa_test"]
    if half_dead_orfa != ["linux"]:
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b-lado-errado-linux FALHOU (lado estruturado "
            f"errado, esperava ['linux'], veio {half_dead_orfa})",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: PARITY-ALIAS-HYGIENE C1b-lado-errado-linux OK (irmao do lado errado nao "
        f"suprime, lado conferido por valor): {errors}"
    )
    return True


# PARITY-ALIAS-HYGIENE C1b-lado-errado-windows: gemeo direto do
# controle acima no ramo `windows_absent` (linha 505, mesma familia de
# mutante - GODS_LAWS.md L-17). O irmao "zeta_fantasma_test"
# compartilha o MESMO linux_name do candidato (`linux_to_win` e'
# chaveado por linux_name nesse ramo), existe de verdade do lado
# Linux, mas nunca do lado Windows - nao pode suprimir o meio-morto de
# zeta_orfa_test. Nome de fixture sem "linux"/"windows" embutido, side
# conferido por VALOR (P-3).
def selftest_alias_half_dead_windows_sibling_must_be_windows_side_reproves():
    linux_inv = {"a_test", "zeta_presente_test", "zeta_fantasma_test"}
    windows_inv = {"a_test"}
    aliases = [
        _alias_fixture("zeta_presente_test", "zeta_orfa_test"),
        _alias_fixture(
            "zeta_presente_test",
            "zeta_fantasma_test",
            bilateral_reason=(
                "fixture de teste - este nome existe so do lado linux, usado so "
                "para forjar o grupo de irmaos do parceiro (nao e' o que este "
                "controle prova, so' precisa nao gerar erro proprio)"
            ),
        ),
    ]
    errors = run_comparison(linux_inv, windows_inv, [], aliases, {})
    if not _check_half_dead_message(
        errors, "zeta_orfa_test", "windows", "PARITY-ALIAS-HYGIENE C1b-lado-errado-windows"
    ):
        return False
    _dead, _decl, _undecl, _false, half_dead, _supp = compute_alias_hygiene(
        aliases, linux_inv, windows_inv
    )
    half_dead_orfa = [side for alias, side in half_dead if alias["windows_name"] == "zeta_orfa_test"]
    if half_dead_orfa != ["windows"]:
        print(
            f"selftest: PARITY-ALIAS-HYGIENE C1b-lado-errado-windows FALHOU (lado estruturado "
            f"errado, esperava ['windows'], veio {half_dead_orfa})",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: PARITY-ALIAS-HYGIENE C1b-lado-errado-windows OK (irmao do lado errado nao "
        f"suprime, lado conferido por valor): {errors}"
    )
    return True


# Controle do PISO (GODS_LAWS.md L-40): a supressao por irmao vivo tem
# que deixar rastro contavel, nunca ser silenciosa. Reusa o cenario
# exato de selftest_alias_shared_windows_partner_control (tres
# apelidos Linux para o MESMO parceiro Windows, shell_smoke ausente do
# inventario sintetico de proposito) e confere o RETORNO CRU de
# compute_alias_hygiene(): zero meio-mortos de verdade (a supressao
# funcionou), mas exatamente UM suprimido, e e' o shell_smoke.
def selftest_half_dead_suppressed_count_visible():
    linux_inv = {
        "a_test",
        "display_connect_failure_test",
        "shell_requirements_test",
        # shell_smoke DELIBERADAMENTE ausente - ver o comentario de
        # selftest_alias_shared_windows_partner_control acima.
    }
    windows_inv = {"a_test", "win32_display_connect_test"}
    aliases = [
        _alias_fixture("display_connect_failure_test", "win32_display_connect_test"),
        _alias_fixture("shell_requirements_test", "win32_display_connect_test"),
        _alias_fixture("shell_smoke", "win32_display_connect_test"),
    ]
    _dead, _decl, _undecl, _false, half_dead, half_dead_suppressed = compute_alias_hygiene(
        aliases, linux_inv, windows_inv
    )
    if half_dead:
        print(
            f"selftest: PISO-MEIO-MORTO-SUPRIMIDO FALHOU (nao deveria haver meio-morto de "
            f"verdade aqui, os tres tem irmao vivo): {half_dead}",
            file=sys.stderr,
        )
        return False
    if len(half_dead_suppressed) != 1 or half_dead_suppressed[0][0]["linux_name"] != "shell_smoke":
        print(
            f"selftest: PISO-MEIO-MORTO-SUPRIMIDO FALHOU (esperava exatamente 1 suprimido, "
            f"o shell_smoke): {half_dead_suppressed}",
            file=sys.stderr,
        )
        return False
    print(
        f"selftest: PISO-MEIO-MORTO-SUPRIMIDO OK (1 suprimido por irmao vivo, contado e "
        f"visivel, nunca calado): {half_dead_suppressed}"
    )
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


# GODS_LAWS.md L-17 P-3, 3a verificacao L-12 (achado bloqueante N4 da
# revisao independente): a mensagem "excecao morta" nunca tinha o
# TEXTO conferido alem do nome do teste - um espaco duplo inserido no
# MEIO da frase (`"nao aparece em  inventario"`) sobrevivia. Mesmo
# padrao de _check_half_dead_message (rodada 2): a frase EXATA, nao so
# o nome.
#
# Revisao FINAL (achado bloqueante NOVO A, mesma familia de N4): a
# mensagem tambem cita `(missing_on={exc['missing_on']})`, e nenhum
# controle conferia ESSE campo - um mutante que fixa a constante
# "missing_on=linux" na formatacao, independente do valor real,
# sobrevivia por inteiro (os dois cenarios C3a/C3b usavam "windows"
# nos dois, entao a lacuna nunca foi exercitada). Conferido aqui, e os
# dois cenarios abaixo passam a usar lados DIFERENTES entre si - senao
# a checagem vira tautologica pelo MESMO motivo que motivou N7 (o
# valor certo por coincidencia bater com o unico valor testado).
def _check_exception_dead_message(errors, test_name, missing_on, label):
    if not any(
        "excecao morta" in e
        and test_name in e
        and f"missing_on={missing_on}" in e
        and "nao aparece em inventario nenhum, ou ja existe tambem do lado declarado" in e
        for e in errors
    ):
        print(
            f"selftest: {label} FALHOU (reprovou, mas a mensagem nao bate a frase exata "
            f"de 'excecao morta' com missing_on={missing_on!r}): {errors}",
            file=sys.stderr,
        )
        return False
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
    if not _check_exception_dead_message(errors, "so_linux_antes_test", "windows", "PARITY-ALIAS-HYGIENE C3a"):
        return False
    print(f"selftest: PARITY-ALIAS-HYGIENE C3a OK (excecao com par ja fechado pega): {errors}")
    return True


# PARITY-ALIAS-HYGIENE C3b (VERMELHO): excecao cujo teste nao existe
# em inventario nenhum - orfa, teste renomeado ou apagado. Esperado:
# reprova. `missing_on="linux"` (revisao FINAL, achado NOVO A) - lado
# DIFERENTE de C3a de proposito, para a checagem de missing_on acima
# nao virar tautologica testando so' um valor possivel.
def selftest_exception_dead_orphaned_reproves():
    linux_inv = {"a_test"}
    windows_inv = {"a_test"}
    exceptions = [
        {
            "test_name": "teste_que_nao_existe_mais",
            "missing_on": "linux",
            "gemeo": "nenhum",
            "item": SEM_PENDENCIA,
            "prova_parcial_gemeo": None,
        }
    ]
    errors = run_comparison(linux_inv, windows_inv, exceptions, [], {})
    if not errors:
        print("selftest: PARITY-ALIAS-HYGIENE C3b FALHOU (excecao orfa deveria ter reprovado)", file=sys.stderr)
        return False
    if not _check_exception_dead_message(errors, "teste_que_nao_existe_mais", "linux", "PARITY-ALIAS-HYGIENE C3b"):
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


# --- P-3, achados de seguimento SS4.1 da revisao de P-2 --------------
#
# TRES GUARDAS DE PARSING NUNCA VISTAS VERMELHAS (GODS_LAWS.md L-36:
# "portao so conta depois de PROVADO vermelho"). As tres guardas ja
# existiam antes de P-2 e nunca tinham controle proprio - MUT-5, MUT-6
# e MUT-8 da revisao de P-2 (remover cada guarda) sobreviviam aos 31
# controles antigos porque nenhum deles chamava o parser com a entrada
# malformada especifica. Mesmo padrao de
# selftest_corrupted_inventory_line_reproves() acima: try/except
# SystemExit, nunca ler codigo de saida da tela (GODS_LAWS.md L-45).


# MUT-5: motivo vazio de "bilateral=" em parse_aliases_text().
def selftest_alias_bilateral_reason_empty_reproves():
    if _expect_fail_exit(parse_aliases_text, "a_linux|a_windows|bilateral=\n"):
        print(
            "selftest: BILATERAL-MOTIVO-VAZIO OK (terceiro campo 'bilateral=' sem motivo "
            "reprova, GODS_LAWS.md L-40, em vez de aceitar motivo vazio calado)"
        )
        return True
    print(
        "selftest: BILATERAL-MOTIVO-VAZIO FALHOU (motivo vazio de 'bilateral=' deveria ter "
        "reprovado)",
        file=sys.stderr,
    )
    return False


# GODS_LAWS.md L-36 (achado bloqueante da revisao independente de P-3,
# 23/09/2026, MUT-N5): o prefixo "bilateral=" e' case-sensitive por
# desenho (so' a forma minuscula exata e' aceita), mas nenhum controle
# jamais provou isso - uma regressao pra case-insensitive (ex.:
# `terceiro.lower().startswith(...)`) sobrevivia inteira. Duas
# variantes de maiuscula, as duas tem de reprovar.
def selftest_alias_bilateral_prefix_is_case_sensitive_reproves():
    for variante in ("Bilateral=motivo\n", "BILATERAL=motivo\n"):
        linha = f"a_linux|a_windows|{variante}"
        if not _expect_fail_exit(parse_aliases_text, linha):
            print(
                f"selftest: BILATERAL-PREFIXO-CASE-SENSITIVE FALHOU (variante {variante!r} "
                f"deveria ter reprovado - 'bilateral=' e' case-sensitive por desenho): {linha!r}",
                file=sys.stderr,
            )
            return False
    print(
        "selftest: BILATERAL-PREFIXO-CASE-SENSITIVE OK (variantes de maiuscula do prefixo "
        "'bilateral=' reprovam, GODS_LAWS.md L-36)"
    )
    return True


# GODS_LAWS.md L-36, revisao FINAL (achado bloqueante NOVO B, mesma
# familia de N5): o guard real e' `terceiro.startswith(_BILATERAL_
# PREFIX)` - duas invariantes independentes, (a) maiuscula exata (ja
# provada acima) e (b) POSICAO no inicio do campo. O controle acima
# so' prova (a). Um mutante que troca `startswith` por `_BILATERAL_
# PREFIX in terceiro` (aceita o prefixo em QUALQUER posicao do
# terceiro campo, nao so' no inicio) sobrevivia por inteiro - nenhuma
# fixture do arquivo tinha "bilateral=" fora da posicao 0.
def selftest_alias_bilateral_prefix_must_be_at_start_reproves():
    linha = "a_linux|a_windows|xbilateral=motivo\n"
    if not _expect_fail_exit(parse_aliases_text, linha):
        print(
            f"selftest: BILATERAL-PREFIXO-POSICAO FALHOU (prefixo fora do inicio do campo "
            f"deveria ter reprovado - 'bilateral=' so' e' aceito na POSICAO 0, "
            f"GODS_LAWS.md L-36): {linha!r}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: BILATERAL-PREFIXO-POSICAO OK (prefixo 'bilateral=' fora da posicao "
        "inicial reprova, GODS_LAWS.md L-36)"
    )
    return True


# MUT-6: gemeo vazio de "PROVA-PARCIAL=" em parse_exceptions_text().
def selftest_prova_parcial_gemeo_empty_reproves():
    linha = "meu_teste|windows|outro_teste|ITEM-1|PROVA-PARCIAL=\n"
    if _expect_fail_exit(parse_exceptions_text, linha):
        print(
            "selftest: PROVA-PARCIAL-GEMEO-VAZIO OK (quinto campo 'PROVA-PARCIAL=' sem nome "
            "reprova, GODS_LAWS.md L-40, em vez de aceitar gemeo vazio calado)"
        )
        return True
    print(
        "selftest: PROVA-PARCIAL-GEMEO-VAZIO FALHOU (gemeo vazio de 'PROVA-PARCIAL=' deveria "
        "ter reprovado)",
        file=sys.stderr,
    )
    return False


# MUT-8: missing_on fora de SISTEMAS_VALIDOS em parse_exceptions_text().
def selftest_exception_invalid_missing_on_reproves():
    linha = "meu_teste|macos|outro_teste|ITEM-1\n"
    if _expect_fail_exit(parse_exceptions_text, linha):
        print(
            "selftest: MISSING-ON-INVALIDO OK (sistema_onde_falta fora de "
            "('linux', 'windows') reprova, GODS_LAWS.md L-40)"
        )
        return True
    print(
        "selftest: MISSING-ON-INVALIDO FALHOU (sistema_onde_falta invalido deveria ter "
        "reprovado)",
        file=sys.stderr,
    )
    return False


# PARITY-ALIAS-HYGIENE P-3, achado do orquestrador ao aceitar a
# revisao final (23/09/2026, GODS_LAWS.md L-20/L-40): o comentario
# acima de _expect_fail_exit() explica por que o codigo de saida tem
# de vir de uma VARIAVEL (`exc.code == 1`) e nunca do simples fato de
# SystemExit ter sido levantada - sys.exit(0) tambem levanta
# SystemExit. Mas nenhum dos 39 controles anteriores chama
# _expect_fail_exit() com uma funcao que sai com codigo DIFERENTE de
# 1: todo `callable_fn` real deste arquivo, quando reprova, sai com
# exatamente 1. Um mutante que trocasse a funcao por
# `except SystemExit: return True` sobrevivia a todos eles - o
# proprio guardiao dos outros 39 controles ficava sem guardiao. Este
# meta-controle fecha o buraco, testando _expect_fail_exit() em SI
# MESMO com (a) uma funcao que sai com codigo 0 e (b) uma funcao que
# RETORNA sem sair nenhuma - as duas tem de ser recusadas (False).
def selftest_expect_fail_exit_meta_control():
    def _exits_zero():
        sys.exit(0)

    def _returns_without_exit():
        return "nao levantou nada"

    if _expect_fail_exit(_exits_zero):
        print(
            "selftest: EXPECT-FAIL-EXIT-META FALHOU (sys.exit(0) deveria ter sido recusado)",
            file=sys.stderr,
        )
        return False
    if _expect_fail_exit(_returns_without_exit):
        print(
            "selftest: EXPECT-FAIL-EXIT-META FALHOU (retorno sem sair deveria ter sido recusado)",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: EXPECT-FAIL-EXIT-META OK (exit(0) e retorno-sem-sair, ambos recusados)"
    )
    return True


# --- P-3, achado de seguimento SS4.2 da revisao de P-2 ----------------
#
# real_main() (--compare, o MODO REAL) nunca tinha cobertura nenhuma
# de ctest/--selftest - MUT-7 da revisao de P-2 (trocar as contagens
# impressas alias_dead <-> alias_half_dead no piso L-40) sobrevivia
# porque nenhum dos 31 controles antigos jamais invocava real_main().
# As duas funcoes abaixo fecham isso ponta a ponta, sobre arquivos
# sinteticos em diretorio temporario descartavel (nunca a arvore
# real), capturando stdout e o codigo de saida DA VARIAVEL da excecao
# SystemExit (GODS_LAWS.md L-45 - nunca inferido da tela).
#
# 2a verificacao L-12 (achado bloqueante da revisao independente):
# `_run_real_main_capturing` nascia com 5 parametros de texto soltos -
# agrupados no mesmo espirito de RealMainInputs acima, so' que com o
# TEXTO CRU (antes do parse) que os controles escrevem em arquivo.
@dataclass(frozen=True)
class RealMainFixtureTexts:
    linux_text: str
    windows_text: str
    exceptions_text: str
    aliases_text: str
    todo_text: str


def _write_temp(tmpdir, name, content):
    path = os.path.join(tmpdir, name)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(content)
    return path


def _run_real_main_capturing(fixture_texts):
    with tempfile.TemporaryDirectory() as tmpdir:
        args = [
            _write_temp(tmpdir, "linux.txt", fixture_texts.linux_text),
            _write_temp(tmpdir, "windows.txt", fixture_texts.windows_text),
            _write_temp(tmpdir, "exceptions.txt", fixture_texts.exceptions_text),
            _write_temp(tmpdir, "aliases.txt", fixture_texts.aliases_text),
            _write_temp(tmpdir, "TODO.md", fixture_texts.todo_text),
        ]
        buffer = io.StringIO()
        exit_code = None
        with contextlib.redirect_stdout(buffer):
            try:
                real_main(args)
            except SystemExit as exc:
                exit_code = exc.code
        return exit_code, buffer.getvalue()


# Cenario com os dois inventarios identicos e nenhuma tabela auxiliar:
# tudo em zero (0 apelidos, 0 excecoes) - prova que o piso L-40 imprime
# as duas linhas de contagem MESMO quando nada existe pra contar,
# nunca so quando ha problema.
def selftest_real_main_zero_counts_prints_piso():
    exit_code, output = _run_real_main_capturing(
        RealMainFixtureTexts("a_test\nb_test\n", "a_test\nb_test\n", "", "", "")
    )
    if exit_code not in (None, 0):
        print(
            f"selftest: REAL-MAIN-ZERO FALHOU (esperava sucesso, saiu com codigo {exit_code!r}): "
            f"{output}",
            file=sys.stderr,
        )
        return False
    if "0 apelido(s), 0 morto(s)" not in output or "0 excecao(oes), 0 morta(s)" not in output:
        print(
            f"selftest: REAL-MAIN-ZERO FALHOU (piso L-40 nao imprimiu as contagens zeradas): "
            f"{output!r}",
            file=sys.stderr,
        )
        return False
    print("selftest: REAL-MAIN-ZERO OK (--compare imprime as contagens mesmo todas zeradas)")
    return True


# Cenario com 1 apelido morto e 2 meio-mortos (contagens DIFERENTES de
# proposito): mata MUT-7 (trocar alias_dead <-> alias_half_dead no
# print de real_main()) porque a mensagem certa so aparece quando as
# duas contagens NAO estao trocadas.
def selftest_real_main_counts_distinguish_dead_from_half_dead():
    exit_code, output = _run_real_main_capturing(
        RealMainFixtureTexts(
            "a_test\nalive_um_test\nalive_dois_test\n",
            "a_test\n",
            "",
            "morto_linux_test|morto_windows_test\n"
            "alive_um_test|sumido_um_test\n"
            "alive_dois_test|sumido_dois_test\n",
            "",
        )
    )
    if exit_code != 1:
        print(
            f"selftest: REAL-MAIN-CONTAGENS FALHOU (esperava reprovar com codigo 1, veio "
            f"{exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    if "3 apelido(s), 1 morto(s)" not in output or "2 meio-morto(s)" not in output:
        print(
            f"selftest: REAL-MAIN-CONTAGENS FALHOU (esperava '3 apelido(s), 1 morto(s)' e "
            f"'2 meio-morto(s)' na saida, contagens trocadas ou erradas): {output!r}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: REAL-MAIN-CONTAGENS OK (--compare distingue morto de meio-morto na linha "
        "impressa, nunca trocados)"
    )
    return True


# PARITY-ALIAS-HYGIENE P-3, verificacao L-12 RODADA 2 do orquestrador
# (achado bloqueante novo, mesma tautologia com outra roupa): os dois
# cenarios de real_main() acima nunca tinham meio-morto E suprimido
# JUNTOS na mesma chamada - selftest_real_main_zero_counts_prints_piso
# tem os dois zerados, selftest_real_main_counts_distinguish_dead_
# from_half_dead tem 2 meio-mortos e 0 suprimidos. Um mutante que troca
# a contagem IMPRESSA de suprimidos (`len(alias_half_dead_suppressed)`)
# por uma constante `0` sobrevive aos dois, porque "suprimido = 0" ja'
# era o valor certo nos dois cenarios - a mutacao nunca muda a saida
# observavel. Este cenario tem os DOIS categorias com contagem
# DIFERENTE de zero ao mesmo tempo (1 meio-morto de verdade, 1
# suprimido por irmao vivo), sobre o mesmo padrao de
# selftest_half_dead_suppressed_count_visible() (varios apelidos
# apontando pro MESMO parceiro, um deles com irmao vivo) - so' que
# passando pelo modo REAL (--compare/real_main), nunca so' pela funcao
# interna.
def selftest_real_main_half_dead_and_suppressed_counts_distinct():
    exit_code, output = _run_real_main_capturing(
        RealMainFixtureTexts(
            "a_test\nvivo_um_test\nvivo_dois_test\n",
            "a_test\nparceiro_test\n",
            "",
            # vivo_um_test e orfa_suprimida_test apontam pro MESMO
            # parceiro Windows vivo ("parceiro_test") -
            # orfa_suprimida_test nao existe em lugar nenhum, mas tem
            # irmao vivo (vivo_um_test) no mesmo grupo, entao e'
            # SUPRIMIDO, nao meio-morto de verdade. vivo_dois_test
            # aponta pra um parceiro que nao existe em lugar nenhum,
            # SEM irmao - e' meio-morto de verdade.
            "vivo_um_test|parceiro_test\n"
            "orfa_suprimida_test|parceiro_test\n"
            "vivo_dois_test|sumido_test\n",
            "",
        )
    )
    if exit_code != 1:
        print(
            f"selftest: REAL-MAIN-SUPRIMIDO-E-MEIO-MORTO FALHOU (esperava reprovar com codigo "
            f"1, veio {exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    if "1 meio-morto(s), 1 meio-morto(s) suprimido(s) por irmao vivo" not in output:
        print(
            f"selftest: REAL-MAIN-SUPRIMIDO-E-MEIO-MORTO FALHOU (esperava '1 meio-morto(s), 1 "
            f"meio-morto(s) suprimido(s) por irmao vivo' na saida - contagem de suprimidos "
            f"trocada por constante nao morre mais so' com os dois cenarios zerados): "
            f"{output!r}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: REAL-MAIN-SUPRIMIDO-E-MEIO-MORTO OK (--compare imprime meio-morto e "
        "suprimido corretos quando os dois valem mais que zero ao mesmo tempo)"
    )
    return True


# PARITY-LOCAL-MIRROR I1 (24/09/2026, TODO.md W7-C, GODS_LAWS.md
# L-04/L-20/L-40) - os controles do modo --textual, ponta a ponta
# (arquivo em disco -> textual_main() -> stdout+stderr capturados),
# no mesmo molde de _run_real_main_capturing acima. Diferente
# daquele helper, redireciona TAMBEM stderr para o mesmo buffer: a
# mensagem de "REPROVADO" que prova a estreia contra o caso real
# (gpu_kind_report_smoke/WIN-RUNNER-PROPRIO, docs/plano-w7c-adendo-
# revalidacao.md SS3.I) sai por stderr (_exit_with_verdict acima),
# e a citacao literal do nome/item so e verificavel capturando as
# duas saidas - a mesma coisa que quem roda o comando no terminal
# ve junta.
@dataclass(frozen=True)
class TextualMainFixtureTexts:
    exceptions_text: str
    aliases_text: str
    todo_text: str


def _run_textual_main_capturing(fixture_texts):
    with tempfile.TemporaryDirectory() as tmpdir:
        args = [
            _write_temp(tmpdir, "exceptions.txt", fixture_texts.exceptions_text),
            _write_temp(tmpdir, "aliases.txt", fixture_texts.aliases_text),
            _write_temp(tmpdir, "TODO.md", fixture_texts.todo_text),
        ]
        buffer = io.StringIO()
        exit_code = None
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            try:
                textual_main(args)
            except SystemExit as exc:
                exit_code = exc.code
        return exit_code, buffer.getvalue()


# Controle POSITIVO: excecao apontando para item PENDENTE, apelido
# bem formado - passa, e o piso L-40 imprime as tres contagens.
def selftest_textual_positive_control():
    exit_code, output = _run_textual_main_capturing(
        TextualMainFixtureTexts(
            "algum_teste|windows|motivo qualquer, com mais de quatro palavras|ITEM-A\n",
            "nome_linux_test|nome_windows_test\n",
            _todo_fixture("ITEM-A", "⏳ Pendente"),
        )
    )
    if exit_code not in (None, 0):
        print(
            f"selftest: TEXTUAL-POSITIVO FALHOU (esperava sucesso, saiu com codigo {exit_code!r}): "
            f"{output}",
            file=sys.stderr,
        )
        return False
    if "1 excecao(oes), 1 apelido(s), 1 item(ns) lido(s) de TODO.md" not in output:
        print(
            f"selftest: TEXTUAL-POSITIVO FALHOU (piso L-40 nao imprimiu as tres contagens): "
            f"{output!r}",
            file=sys.stderr,
        )
        return False
    print("selftest: TEXTUAL-POSITIVO OK (modo --textual passa com excecao pendente)")
    return True


# O VERMELHO central desta sub-fatia: reproduz, com fixture sintetica,
# a FORMA EXATA do caso real medido em 023e1c7 (excecao de
# gpu_kind_report_smoke apontando para WIN-RUNNER-PROPRIO, que virou
# Concluido em 84877aa e so foi consertado em c0fb96f depois de
# avisado ao main - docs/plano-w7c-adendo-revalidacao.md N2). Mutante
# que mata: pular a chamada a validate_exceptions() dentro de
# textual_main() - sem ela este controle passaria calado (exit 0),
# exatamente o furo que esta sub-fatia existe para fechar.
def selftest_textual_concluded_item_reproves():
    exit_code, output = _run_textual_main_capturing(
        TextualMainFixtureTexts(
            "gpu_kind_report_smoke|windows|mutacao real nao provada no lado Windows nesta "
            "maquina|WIN-RUNNER-PROPRIO\n",
            "",
            _todo_fixture("WIN-RUNNER-PROPRIO", "✅ Concluído"),
        )
    )
    if exit_code != 1:
        print(
            f"selftest: TEXTUAL-VERMELHO-023e1c7 FALHOU (esperava reprovar com codigo 1, veio "
            f"{exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    if "gpu_kind_report_smoke" not in output or "WIN-RUNNER-PROPRIO" not in output:
        print(
            f"selftest: TEXTUAL-VERMELHO-023e1c7 FALHOU (reprovou, mas nao citou o caso real "
            f"de 023e1c7/c0fb96f): {output!r}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: TEXTUAL-VERMELHO-023e1c7 OK (excecao apontando para item concluido "
        "reprova no modo --textual - reproduz o caso real de 023e1c7, consertado em c0fb96f)"
    )
    return True


# Linha malformada de tests/parity_exceptions.txt (nem 4 nem 5 campos)
# tem de reprovar DENTRO do modo --textual, nao so quando chamado por
# --compare: parse_exceptions_text() e reusada, nunca copiada
# (GODS_LAWS.md L-17), e fail() ali chama sys.exit(1) direto.
def selftest_textual_malformed_exception_reproves():
    exit_code, _output = _run_textual_main_capturing(
        TextualMainFixtureTexts(
            "nome_quebrado|so|tres_campos\n",
            "",
            _todo_fixture("ITEM-A", "⏳ Pendente"),
        )
    )
    if exit_code != 1:
        print(
            f"selftest: TEXTUAL-EXCECAO-MALFORMADA FALHOU (esperava reprovar com codigo 1, "
            f"veio {exit_code!r})",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: TEXTUAL-EXCECAO-MALFORMADA OK (linha malformada de parity_exceptions.txt "
        "reprova no modo --textual)"
    )
    return True


# O gemeo para tests/parity_aliases.txt: linha malformada (nem 2 nem
# 3 campos) tambem reprova no modo --textual.
def selftest_textual_malformed_alias_reproves():
    exit_code, _output = _run_textual_main_capturing(
        TextualMainFixtureTexts(
            "",
            "so_um_campo_sem_par\n",
            _todo_fixture("ITEM-A", "⏳ Pendente"),
        )
    )
    if exit_code != 1:
        print(
            f"selftest: TEXTUAL-APELIDO-MALFORMADO FALHOU (esperava reprovar com codigo 1, "
            f"veio {exit_code!r})",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: TEXTUAL-APELIDO-MALFORMADO OK (linha malformada de parity_aliases.txt "
        "reprova no modo --textual)"
    )
    return True


# Piso L-40 do proprio modo --textual: TODO.md sem NENHUMA linha de
# tabela reconhecida (formato mudou sob o pe, caminho errado, arquivo
# truncado) tem de reprovar como varredura vazia, nunca passar calado
# so porque nao havia excecao nenhuma para citar.
def selftest_textual_empty_todo_reproves():
    exit_code, output = _run_textual_main_capturing(
        TextualMainFixtureTexts("", "", "so prosa aqui, nenhuma linha de tabela\n")
    )
    if exit_code != 1:
        print(
            f"selftest: TEXTUAL-TODO-VAZIO FALHOU (esperava reprovar com codigo 1, veio "
            f"{exit_code!r}): {output}",
            file=sys.stderr,
        )
        return False
    if "varredura vazia" not in output:
        print(
            f"selftest: TEXTUAL-TODO-VAZIO FALHOU (reprovou, mas nao com a mensagem de "
            f"varredura vazia - GODS_LAWS.md L-40): {output!r}",
            file=sys.stderr,
        )
        return False
    print("selftest: TEXTUAL-TODO-VAZIO OK (TODO.md sem item nenhum reconhecido reprova)")
    return True


def _textual_mode_controls():
    return [
        selftest_textual_positive_control(),
        selftest_textual_concluded_item_reproves(),
        selftest_textual_malformed_exception_reproves(),
        selftest_textual_malformed_alias_reproves(),
        selftest_textual_empty_todo_reproves(),
    ]


# --- CI-SPLIT-PER-OS A1: controles do modo --per-system ------------------


def _per_system_full_inventory(faltando_em=None, nome_faltante="b_test"):
    """Cinco sistemas identicos ({a_test, b_test}), exceto o slug
    `faltando_em` (se dado), que perde `nome_faltante` - a fixtura
    compartilhada por quase todo controle abaixo, para nao repetir os
    cinco dicionarios em cada funcao (GODS_LAWS.md L-17, regra de 3)."""
    inventario = {slug: frozenset({"a_test", "b_test"}) for slug in SISTEMAS_PER_SYSTEM_ESPERADOS}
    if faltando_em is not None:
        inventario[faltando_em] = frozenset(inventario[faltando_em] - {nome_faltante})
    return inventario


def selftest_per_system_positive_control():
    inventario = _per_system_full_inventory()
    errors = run_per_system_comparison(inventario, [], {})
    if errors:
        print(f"selftest: cinco sistemas identicos, sem lacuna, reprovou - erros: {errors}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-POSITIVO OK (cinco sistemas identicos, nenhuma lacuna)")
    return True


# C2 (docs/plano-ci-split-per-os.md secao 5, A1): "autoteste com
# inventario sem um nome so' no CachyOS ... reprova citando 'cachyos'"
# - o TESTE VERMELHO DE ESTREIA desta fatia. Antes desta fatia existir,
# nao havia run_per_system_comparison() nenhuma para chamar (NameError,
# vermelho genuino, visto antes desta funcao ser escrita).
def selftest_per_system_gap_only_in_one_system_reproves():
    inventario = _per_system_full_inventory(faltando_em="cachyos")
    errors = run_per_system_comparison(inventario, [], {})
    encontrou = any("b_test" in e and "'cachyos'" in e for e in errors)
    if not encontrou:
        print(
            f"selftest: lacuna exclusiva do cachyos nao foi reprovada citando o slug - erros: {errors}",
            file=sys.stderr,
        )
        return False
    print("selftest: PER-SYSTEM-LACUNA-UNICA OK (lacuna so' no cachyos reprova citando 'cachyos')")
    return True


# Mutante nomeado no proprio plano (secao 7, linha A1: "Mutantes:
# voltar a uniao -> C2 passa (tem de reprovar)"): esta e' a PROVA POR
# ESCRITO de que a comparacao antiga (--compare, run_comparison(), a
# uniao Linux inteira contra Windows) e' cega exatamente a esta lacuna
# - a mesma entrada que selftest_per_system_gap_only_in_one_system_
# reproves() acima reprova, aqui passa calada quando reduzida a uniao
# Linux x Windows (fedora+ubuntu+arch+cachyos unidos = so' perde
# b_test se TODAS perderem, nao so' uma).
def selftest_per_system_naive_union_would_miss_the_gap_control():
    linux_uniao = frozenset({"a_test", "b_test"})  # fedora+ubuntu+arch+cachyos unidos
    windows_uniao = frozenset({"a_test", "b_test"})
    errors = run_comparison(linux_uniao, windows_uniao, [], [], {})
    if errors:
        print(
            "selftest: premissa do controle quebrada - a comparacao por uniao (antiga) "
            f"deveria passar calada aqui, mas reprovou: {errors}",
            file=sys.stderr,
        )
        return False
    print(
        "selftest: PER-SYSTEM-MUTANTE-UNIAO OK (comparacao por uniao, antiga, passa calada na "
        "mesma lacuna que --per-system pega - prova de por que esta fatia existe)"
    )
    return True


def selftest_per_system_declared_exception_suppresses_gap():
    inventario = _per_system_full_inventory(faltando_em="cachyos")
    excecoes = [
        {
            "test_name": "b_test",
            "missing_on": "cachyos",
            "gemeo": "nenhum",
            "item": SEM_PENDENCIA,
            "prova_parcial_gemeo": None,
        }
    ]
    errors = run_per_system_comparison(inventario, excecoes, {})
    if errors:
        print(f"selftest: excecao declarada para cachyos nao suprimiu a lacuna - erros: {errors}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-EXCECAO-DECLARADA OK (excecao 'b_test|cachyos|...' suprime a lacuna)")
    return True


# CI-SPLIT-PER-OS A1, achado da classificacao dos 215 do run
# 36085750444 (categoria "a"): uma excecao com missing_on="linux" (o
# AGREGADO que --compare ja usava) tem de cobrir QUALQUER slug Linux,
# nunca so' um. O vermelho REAL que motivou este controle: `win32_
# runner_probe_test|linux|...|SEM-PENDENCIA` (tests/parity_exceptions.txt)
# nao cobria 'arch' no --per-system de ae5dd7e - 8 pares reprovaram
# exatamente por isso na estreia do modo em produção.
def selftest_per_system_family_exception_covers_all_linux_slugs():
    inventario = _per_system_full_inventory(faltando_em="arch")
    excecoes = [
        {"test_name": "b_test", "missing_on": "linux", "gemeo": "nenhum", "item": SEM_PENDENCIA, "prova_parcial_gemeo": None}
    ]
    errors = run_per_system_comparison(inventario, excecoes, {})
    if errors:
        print(f"selftest: excecao de familia 'linux' nao cobriu o slug 'arch' - erros: {errors}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-EXCECAO-FAMILIA-LINUX OK ('b_test|linux|...' cobre qualquer slug Linux)")
    return True


# Gemeo negativo: excecao de familia ERRADA (windows) nunca pode cobrir
# um slug Linux - prova que a expansao e' por familia, nao "qualquer
# excecao existente para este nome vale".
def selftest_per_system_windows_exception_never_covers_linux_slug():
    inventario = _per_system_full_inventory(faltando_em="arch")
    excecoes = [
        {"test_name": "b_test", "missing_on": "windows", "gemeo": "nenhum", "item": SEM_PENDENCIA, "prova_parcial_gemeo": None}
    ]
    errors = run_per_system_comparison(inventario, excecoes, {})
    encontrou = any("b_test" in e and "'arch'" in e for e in errors)
    if not encontrou:
        print(f"selftest: excecao 'windows' cobriu indevidamente o slug 'arch' - erros: {errors}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-EXCECAO-FAMILIA-WINDOWS-NAO-VAZA OK (familia errada nunca cobre)")
    return True


# O outro mutante nomeado no plano: "piso de sistemas desligado -> um
# sistema sem inventario passa calado". Aqui "windows" nunca recebe
# --per-system slug=caminho nenhum (dict sem a chave, nao inventario
# vazio - os dois defeitos sao distintos e os dois tem de reprovar).
def selftest_per_system_missing_system_reproves():
    inventario = _per_system_full_inventory()
    del inventario["windows"]
    errors = run_per_system_comparison(inventario, [], {})
    encontrou = any("windows" in e for e in errors)
    if not encontrou:
        print(f"selftest: sistema esperado (windows) sem inventario nenhum nao reprovou - erros: {errors}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-PISO-SISTEMA-AUSENTE OK (sistema esperado sem inventario reprova)")
    return True


def selftest_per_system_empty_inventory_reproves():
    inventario = _per_system_full_inventory()
    inventario["arch"] = frozenset()
    errors = run_per_system_comparison(inventario, [], {})
    encontrou = any("arch" in e and "0 testes" in e for e in errors)
    if not encontrou:
        print(f"selftest: sistema com inventario vazio (arch) nao reprovou - erros: {errors}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-VARREDURA-VAZIA OK (inventario recebido mas com 0 testes reprova)")
    return True


def selftest_per_system_unexpected_slug_rejected():
    ok = _expect_fail_exit(_parse_per_system_args, ["exc.txt", "TODO.md", "bogus-slug=/tmp/x"])
    if not ok:
        print("selftest: slug fora de SISTEMAS_PER_SYSTEM_ESPERADOS nao foi recusado por --per-system", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-SLUG-INVALIDO OK (slug desconhecido em --per-system e' recusado)")
    return True


def selftest_per_system_exception_pointing_to_concluded_item_reproves():
    """--per-system reusa validate_exceptions() sem copiar - a mesma
    regra de morte 'concluido sem par' que --compare ja aplica vale
    aqui tambem, por construcao (a funcao e' a mesma, nao uma copia)."""
    inventario = _per_system_full_inventory(faltando_em="cachyos")
    excecoes = [
        {
            "test_name": "b_test",
            "missing_on": "cachyos",
            "gemeo": "outro_test",
            "item": "ITEM-FECHADO",
            "prova_parcial_gemeo": None,
        }
    ]
    todo_status = {"ITEM-FECHADO": {"status_text": "✅ Concluido", "concluded": True}}
    errors = run_per_system_comparison(inventario, excecoes, todo_status)
    encontrou = any("CONCLUIDO" in e for e in errors)
    if not encontrou:
        print(f"selftest: excecao apontando para item concluido nao reprovou em --per-system - erros: {errors}", file=sys.stderr)
        return False
    print("selftest: PER-SYSTEM-EXCECAO-CONCLUIDA OK (regra de morte de --compare vale em --per-system)")
    return True


def _per_system_mode_controls():
    return [
        selftest_per_system_positive_control(),
        selftest_per_system_gap_only_in_one_system_reproves(),
        selftest_per_system_naive_union_would_miss_the_gap_control(),
        selftest_per_system_declared_exception_suppresses_gap(),
        selftest_per_system_family_exception_covers_all_linux_slugs(),
        selftest_per_system_windows_exception_never_covers_linux_slug(),
        selftest_per_system_missing_system_reproves(),
        selftest_per_system_empty_inventory_reproves(),
        selftest_per_system_unexpected_slug_rejected(),
        selftest_per_system_exception_pointing_to_concluded_item_reproves(),
    ]


# PARITY-ALIAS-HYGIENE P-3 (23/09/2026, GODS_LAWS.md L-17): selftest_
# main() cresceu de 31 para 36 chamadas nesta fatia - mais uma linha
# nova a cada controle acrescentado, PARA SEMPRE, e' exatamente o
# padrao "quem paga a proxima feature" que a lei pede pra vigiar.
# Divide a lista, agrupada por o que cada bloco prova, em quatro
# funcoes nomeadas - cada uma so' MONTA a lista do seu grupo (a
# chamada de cada selftest_* continua acontecendo aqui, na mesma
# ordem de antes, print incluso).
def _core_controls():
    return [
        selftest_positive_control(),
        selftest_unregistered_gap_reproves(),
        selftest_gap_message_names_correct_direction_reproves(),
        selftest_exception_pointing_to_concluded_item_reproves(),
        selftest_exception_pointing_to_pending_item_passes(),
        selftest_empty_inventory_reproves(),
        selftest_empty_inventory_windows_reproves(),
        selftest_alias_control(),
        selftest_alias_shared_windows_partner_control(),
        selftest_sem_pendencia_control(),
        selftest_sem_pendencia_with_gemeo_reproves(),
        selftest_unknown_item_reproves(),
    ]


def _inventory_parsing_controls():
    return [
        selftest_parsing_round_trip(),
        selftest_ctest_project_header_not_swallowed(),
        selftest_ctest_header_only_yields_empty_inventory(),
        selftest_mixed_ctest_and_clean_list_control(),
        selftest_corrupted_inventory_line_reproves(),
        selftest_disabled_line_recognized_not_swallowed(),
    ]


def _alias_hygiene_controls():
    return [
        selftest_alias_dead_reproves(),
        selftest_alias_half_dead_reproves(),
        selftest_alias_half_dead_linux_side_reproves(),
        selftest_alias_half_dead_linux_dead_siblings_not_counted_reproves(),
        selftest_alias_half_dead_windows_dead_siblings_not_counted_reproves(),
        selftest_alias_half_dead_linux_sibling_must_be_linux_side_reproves(),
        selftest_alias_half_dead_windows_sibling_must_be_windows_side_reproves(),
        selftest_half_dead_suppressed_count_visible(),
        selftest_alias_bilateral_undeclared_reproves(),
        selftest_alias_bilateral_declared_control(),
        selftest_alias_bilateral_false_declaration_reproves(),
        selftest_exception_dead_gap_closed_reproves(),
        selftest_exception_dead_orphaned_reproves(),
        selftest_prova_parcial_gemeo_absent_reproves(),
        selftest_prova_parcial_gemeo_present_control(),
    ]


def _parsing_guard_and_real_main_controls():
    return [
        selftest_alias_bilateral_reason_empty_reproves(),
        selftest_alias_bilateral_prefix_is_case_sensitive_reproves(),
        selftest_alias_bilateral_prefix_must_be_at_start_reproves(),
        selftest_prova_parcial_gemeo_empty_reproves(),
        selftest_exception_invalid_missing_on_reproves(),
        selftest_expect_fail_exit_meta_control(),
        selftest_real_main_zero_counts_prints_piso(),
        selftest_real_main_counts_distinguish_dead_from_half_dead(),
        selftest_real_main_half_dead_and_suppressed_counts_distinct(),
    ]


def selftest_main():
    controls = [
        *_core_controls(),
        *_inventory_parsing_controls(),
        *_alias_hygiene_controls(),
        *_parsing_guard_and_real_main_controls(),
        *_textual_mode_controls(),
        *_per_system_mode_controls(),
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
    elif args and args[0] == "--textual":
        textual_main(args[1:])
    elif args and args[0] == "--per-system":
        per_system_main(args[1:])
    else:
        fail(
            "usage: check_test_parity.py --compare <inv-linux> <inv-windows> <exceptions.txt> "
            "<aliases.txt> <TODO.md>  |  --textual <exceptions.txt> <aliases.txt> <TODO.md>  |  "
            "--per-system <exceptions.txt> <TODO.md> <slug>=<inventario> [...]  |  --selftest"
        )


if __name__ == "__main__":
    main()
