#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_layers_oracle.py - oraculo diferencial do portao de camadas
# (GODS_LAWS.md projeto L-5, docs/plano-layers-l5.md - o plano e' a
# fonte executavel; este comentario resume, nao substitui).
#
# O QUE ISTO PROVA: check_layers.py (o portao, GODS_LAWS.md L-19) le
# TEXTO e decide se um arquivo de camada pura puxa cabecalho proibido.
# Este oraculo pergunta a MESMA coisa ao COMPILADOR DE VERDADE (`-H` no
# GCC/Clang, `/showIncludes` no MSVC) e reprova quando o compilador viu
# um cabecalho proibido ser puxado e o portao, pra aquela MESMA
# fixture, deixou passar. So' essa direcao e' promessa (docs/plano-
# layers-l5.md §5.2) - o portao ve TODOS os ramos de #if (semantica de
# uniao), o compilador ve UM so'; o contrario (portao reprovou,
# compilador so' puxou permitido) e' relatado como "super-aproximacao",
# NUNCA reprova o oraculo.
#
# GODS_LAWS.md L-45, INTEIRA (decisao do lider, 23/09/2026): o oraculo
# SO' RODA NO CI DO SERVIDOR. `real_compiler_executor()` abaixo recusa
# ser chamado durante `--selftest` (levanta excecao - O-0), e
# `oracle_main()` recusa QUALQUER processo quando `GITHUB_ACTIONS` nao
# e' "true" (O-12). Nesta maquina, o unico jeito de exercitar este
# arquivo e' `--selftest`, que usa um EXECUTOR FALSO com saidas
# ENLATADAS - nenhum compilador roda nunca aqui, nem no autoteste.
#
# FONTE DAS SAIDAS ENLATADAS (O-0): tres, cada uma rotulada no proprio
# dado - (1) "medido, verbatim do plano de L-5 §1.1" (g++ 16.2.1, uma
# medicao unica ja feita pelo planejador, citada aqui, nao repetida);
# (2) "documentado" (formato descrito pela pagina oficial do GCC ou da
# Microsoft, citada com URL no plano); (3) "sintetica" (variacao
# escrita a mao pra exercitar um caso de borda que nenhuma das duas
# fontes acima cobre sozinha).
#
# ENV-DRIFT (GODS_LAWS.md L-04): a leitura de `GITHUB_ACTIONS` abaixo
# e' fato do AMBIENTE, nao do repositorio - documentada pelo GitHub
# como "Always set to true when GitHub Actions is running the
# workflow" (docs.github.com/actions/reference/workflows-and-actions/
# variables). O oraculo tambem pode citar a sequencia \r\n em texto
# (a normalizacao de fim de linha do CR sozinho - a mesma armadilha
# de LINE_ENDING que check_env_sweep.py vigia): toda ocorrencia aqui
# vem com esta mesma declaracao por perto.
#
# Each function below does one thing (GODS_LAWS.md L-17): <=40 linhas,
# <=4 parametros (agrupados em _OracleContext quando um dado e' comum a
# varias chamadas - ver a classe abaixo), <=3 niveis de aninhamento.

import collections
import json
import ntpath
import os
import posixpath
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_layers_oracle.py"

_KNOWN_DIALECTS = ("GNU", "MSVC")
_INVOCATION_TIMEOUT_SECONDS = 60
_MIN_STDLIB_PATHS = 60
_MIN_FORBIDDEN_PULLS = 20
_MANIFEST_FILENAME = "manifest.json"

# DECISOES_AUTONOMAS.md 23/09/2026 (achado do implementador de L-5,
# medido: com DISABLED no add_test, `ctest -N` grava o sufixo literal
# " (Disabled)" e tests/tools/check_test_parity.py QUEBRA no parse
# dessa linha - nao uma paridade vermelha comum, um crash do proprio
# parser). Decisao do main: o teste nunca usa DISABLED - fica
# registrado igual em TODO sistema (paridade de inventario de
# verdade, sem sufixo nenhum), com a propriedade CTest SKIP_RETURN_
# CODE. Codigo 77 e' o valor convencional do CTest pra "pulado"
# (documentado por CMake, nao inventado aqui) - ctest --test-dir <b>
# -R layers_oracle mostra "Skipped" (visivel, GODS_LAWS.md L-40) em
# vez de "Not Run (Disabled)".
_SKIP_RETURN_CODE = 77

_GNU_TREE_LINE_PATTERN = re.compile(r"^(\.+) (.+)$")

_LANGUAGE_STD_FLAG_PATTERNS = {
    "GNU": re.compile(r"^-std="),
    "MSVC": re.compile(r"^/std:|^-std:"),
}
_PREPROCESSOR_MODE_FLAG_PATTERN = re.compile(r"^/Zc:preprocessor-?$|^/experimental:preprocessor$")
_SOURCE_CHARSET_FLAG_PATTERNS = {
    "GNU": re.compile(r"^-finput-charset="),
    "MSVC": re.compile(r"^/utf-8$|^/source-charset:"),
}


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


class _IncludeTreeError(Exception):
    """Falha de INSTRUMENTO (docs/plano-layers-l5.md §3.2/§7): a saida
    do compilador nao bate com o formato esperado, ou a calibracao/
    sentinela nao confirma o instrumento. NUNCA um pulo silencioso -
    sempre reprova a fixture inteira, nomeando o motivo."""


class _RealExecutorDuringSelftestError(Exception):
    """O-0 (docs/plano-layers-l5.md §10.1): o executor REAL foi chamado
    com a marca de autoteste ativa - GODS_LAWS.md L-45 proibe qualquer
    compilador nesta maquina, inclusive por engano dentro do
    autoteste."""


_SELFTEST_MODE_ACTIVE = False


class _ExecResult:
    __slots__ = ("returncode", "output", "timed_out")

    def __init__(self, returncode, output, timed_out=False):
        self.returncode = returncode
        self.output = output
        self.timed_out = timed_out


def real_compiler_executor(command, cwd, timeout):
    """O UNICO lugar deste arquivo que chamaria um compilador de
    verdade - injetado como parametro em toda funcao que precisa
    (docs/plano-layers-l5.md §10.1, controle O-0). Levanta excecao se
    for chamado durante `--selftest` - a prova de que o autoteste
    inteiro roda sem compilador nenhum."""
    if _SELFTEST_MODE_ACTIVE:
        raise _RealExecutorDuringSelftestError(
            "executor real chamado durante --selftest (GODS_LAWS.md L-45): "
            "nenhum compilador roda fora do CI do servidor"
        )
    env = dict(os.environ)
    env["LC_ALL"] = "C"
    try:
        completed = subprocess.run(
            command, cwd=cwd, timeout=timeout, env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
    except subprocess.TimeoutExpired:
        return _ExecResult(None, "", timed_out=True)
    return _ExecResult(completed.returncode, completed.stdout.decode("utf-8", errors="replace"))


class _OracleContext:
    """Agrupa o que uma invocacao de compilador precisa (GODS_LAWS.md
    L-17: sem isto quase toda funcao de baixo nivel passaria de quatro
    parametros). `layer_dirs` muda a cada CASO (raiz propria); o resto
    e' fixo pro trabalho de CI inteiro - ver run_oracle()."""

    __slots__ = (
        "dialect", "compiler", "flags", "include_dirs", "executor",
        "msvc_prefix", "camadas_puras", "layer_dirs",
        "stub_generated_paths", "standard_paths", "out_of_tree_lines_total",
        "case_include_dirs",
    )

    def __init__(self, dialect, compiler, flags, include_dirs):
        self.dialect = dialect
        self.compiler = compiler
        self.flags = flags
        self.include_dirs = include_dirs
        self.executor = None
        self.msvc_prefix = None
        self.camadas_puras = ()
        self.layer_dirs = ()
        self.stub_generated_paths = frozenset()
        self.standard_paths = frozenset()
        # docs/plano-layers-l5-adendo-calibracao.md L-5f: as DUAS listas
        # de diretorios EXPLICITAS (secao 4, "nenhuma funcao descobre a
        # configuracao por efeito colateral") - `include_dirs` continua
        # sendo a REAL/calibracao (sem std_stubs/); `case_include_dirs`
        # e' ela + std_stubs/ por ULTIMO, so' pra judging de caso e
        # sentinela (O-23).
        self.case_include_dirs = ()
        # docs/plano-layers-l5-adendo-calibracao.md L-5d: acumulador de
        # linhas fora do formato de arvore (-H/showIncludes), somado a
        # CADA chamada de _parse_tree() pro trabalho INTEIRO (calibracao
        # + sentinelas + todos os casos) - impresso sempre por
        # run_oracle(), nunca so' na falha (GODS_LAWS.md L-40).
        self.out_of_tree_lines_total = 0


# --- dialeto: enumeracao fechada (O-17) --------------------------------


def validate_dialect(dialect):
    if dialect not in _KNOWN_DIALECTS:
        raise _IncludeTreeError(f"dialeto desconhecido: {dialect!r} (so' GNU/MSVC sao suportados)")


# --- leitura de compile_commands.json (O-16) ---------------------------


def find_compile_command_entry(entries):
    """Uma entrada de src/core/ - qualquer uma serve, pois as flags da
    familia fechada (§3.3) sao as mesmas pro build inteiro."""
    for entry in entries:
        posix_file = entry.get("file", "").replace(os.sep, "/")
        if "/src/core/" in posix_file:
            return entry
    return None


def tokenize_compile_command(entry):
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry.get("command", ""), posix=(os.name != "nt"))


def extract_language_family_flags(tokens, dialect):
    """docs/plano-layers-l5.md §3.3: SO' tres familias fechadas - padrao
    da linguagem, modo do pre-processador (so' existe no MSVC),
    conjunto de caracteres da fonte. Padrao da linguagem AUSENTE
    reprova - sem ele o oraculo perguntaria a um compilador em C++17
    sobre `#elifdef`."""
    std_pattern = _LANGUAGE_STD_FLAG_PATTERNS[dialect]
    charset_pattern = _SOURCE_CHARSET_FLAG_PATTERNS[dialect]
    kept = []
    std_found = False
    for token in tokens:
        if std_pattern.match(token):
            kept.append(token)
            std_found = True
        elif dialect == "MSVC" and _PREPROCESSOR_MODE_FLAG_PATTERN.match(token):
            kept.append(token)
        elif charset_pattern.match(token):
            kept.append(token)
    if not std_found:
        raise _IncludeTreeError(
            "compile_commands.json: nenhuma flag de padrao da linguagem encontrada (instrumento cego)"
        )
    return kept


# --- linha de comando por dialeto (secao 3.1) ---------------------------


def build_preprocess_command(ctx, file_path, out_scratch_dir, include_dirs):
    """docs/plano-layers-l5-adendo-calibracao.md secao 4 (L-5f):
    `include_dirs` chega EXPLICITO (nunca `ctx.include_dirs` lido por
    conta propria) - o chamador decide se e' a configuracao REAL ou a
    de CASO (`ctx.case_include_dirs`, com `std_stubs/` por ULTIMO)."""
    is_c = file_path.lower().endswith(".c")
    include_flags_gnu = [f"-I{d}" for d in include_dirs]
    if ctx.dialect == "GNU":
        lang = "c" if is_c else "c++"
        command = [
            ctx.compiler, *ctx.flags, *include_flags_gnu,
            "-x", lang, "-E", "-H", "-o", os.devnull, file_path,
        ]
        return command, (os.path.dirname(file_path) or ".")
    lang_flag = "/TC" if is_c else "/TP"
    include_flags_msvc = [f"/I{d}" for d in include_dirs]
    out_file = os.path.join(out_scratch_dir, "out.i")
    command = [
        ctx.compiler, "/nologo", *ctx.flags, *include_flags_msvc,
        lang_flag, "/P", f"/Fi{out_file}", "/showIncludes", file_path,
    ]
    return command, (os.path.dirname(file_path) or ".")


# --- leitura da saida: arvore, nao lista (secao 3.2) --------------------


def parse_gnu_dash_h_tree(output_text):
    """`-H` do GCC/Clang: cada linha `<pontos><espaco><caminho>` vira um
    no. docs/plano-layers-l5-adendo-calibracao.md L-5d (C-1): a leitura
    NUNCA para na primeira linha que nao casa - um `#warning` (ex.:
    backward_warning.h) pode aparecer NO MEIO do fluxo de `-H`, e os
    nos que vem DEPOIS dele continuam fazendo parte da arvore. Cada
    linha fora do formato (inclusive o bloco final de guardas de
    inclusao - traduzido pela localidade, entao NUNCA reconhecido pelo
    CONTEUDO, so' pela forma que nao casa - O-1) e' CONTADA, nunca
    encerra o laco (O-20). Salto de profundidade (>1 de uma vez) e'
    falha de instrumento (O-7)."""
    nodes = []
    previous_depth = 0
    out_of_format_count = 0
    for line in output_text.splitlines():
        match = _GNU_TREE_LINE_PATTERN.match(line)
        if not match:
            out_of_format_count += 1
            continue
        depth = len(match.group(1))
        if depth > previous_depth + 1:
            raise _IncludeTreeError(
                f"salto de profundidade no formato -H: de {previous_depth} para {depth} em {match.group(2)!r}"
            )
        nodes.append((depth, match.group(2)))
        previous_depth = depth
    return nodes, out_of_format_count


_MSVC_NOTE_LINE_PATTERN = re.compile(r"^(.*\S)( +)(\S+)$")


def learn_msvc_prefix(output_text, sentinel_basename):
    """docs/plano-layers-l5.md §7.1(b): o prefixo (traduzido pela
    localidade, doc do Ninja citada no plano) e' o texto ANTES do
    RECUO que antecede o caminho, na linha que TERMINA no caminho de
    sentinela_projeto.hpp - nunca escrito no codigo, sempre aprendido
    (O-9). O caminho inteiro (pasta + arquivo) nunca tem espaco (R-6 do
    plano: scratch e' sempre ASCII sem espaco), entao o UNICO run de
    espacos da linha e' o recuo - `_MSVC_NOTE_LINE_PATTERN` acha essa
    fronteira sem presumir o texto do rotulo."""
    for line in output_text.splitlines():
        if not line.endswith(sentinel_basename):
            continue
        match = _MSVC_NOTE_LINE_PATTERN.match(line)
        if match:
            return match.group(1)
    raise _IncludeTreeError(
        f"calibracao MSVC: nenhuma linha terminando em {sentinel_basename!r} pra aprender o prefixo"
    )


def parse_msvc_showincludes_tree(output_text, learned_prefix):
    """`/showIncludes`: profundidade = numero de ESPACOS entre o
    prefixo aprendido e o caminho ("one space for each level of
    nesting", doc da Microsoft citada no plano - O-10)."""
    nodes = []
    previous_depth = 0
    for line in output_text.splitlines():
        if not line.startswith(learned_prefix):
            continue
        rest = line[len(learned_prefix):]
        stripped = rest.lstrip(" ")
        depth = len(rest) - len(stripped)
        if depth == 0:
            raise _IncludeTreeError(f"linha /showIncludes sem recuo apos o prefixo: {line!r}")
        if depth > previous_depth + 1:
            raise _IncludeTreeError(f"salto de profundidade no formato /showIncludes: de {previous_depth} para {depth}")
        nodes.append((depth, stripped))
        previous_depth = depth
    return nodes


def build_parent_map(nodes):
    """docs/plano-layers-l5.md §3.2: o pai de um no de profundidade `d`
    e' o ULTIMO no de profundidade `d-1` visto antes dele; profundidade
    1 tem como pai o arquivo compilado (`parent_index=None`)."""
    last_at_depth = {0: None}
    result = []
    for index, (depth, raw_path) in enumerate(nodes):
        parent_index = last_at_depth.get(depth - 1)
        result.append((depth, raw_path, parent_index))
        last_at_depth[depth] = index
        for stale_depth in [d for d in last_at_depth if d > depth]:
            del last_at_depth[stale_depth]
    return result


# --- normalizacao de caminho (secao 3.1) --------------------------------


def _normcase_for_dialect(dialect, path):
    """Dobra caixa SEMPRE por argumento explicito (dialeto), NUNCA por
    `os.path.normcase()`/`sys.platform` - que so' dobram caixa se o
    HOST for Windows. O dialeto MSVC roda em CI real no Windows, mas o
    autoteste (O-11) prova o comportamento em QUALQUER host, inclusive
    Linux (GODS_LAWS.md L-04)."""
    return path.lower() if dialect == "MSVC" else path


def _path_module_for_dialect(dialect):
    """achado do conserto de 23/09/2026 (run 35906529355, job Windows,
    layers_oracle_selftest FALHOU: caminho POSIX enlatado do autoteste
    virou caminho de unidade Windows - GODS_LAWS.md L-04): a sintaxe do
    caminho e' do DIALETO do compilador, nunca do HOST que roda o
    Python. `os.path` (isabs/join/realpath) e' escolhido pelo HOST
    (ntpath se o processo roda em Windows, posixpath se roda em Linux),
    e por isso um caminho enlatado POSIX (`/usr/include/...`) que passa
    por `os.path.realpath()` num host Windows ganha letra de unidade
    (`D:\\usr\\include\\...`) mesmo sem tocar disco nenhum. Em producao
    isto nunca aparecia porque dialeto e host sempre coincidem (GNU so'
    roda em job Linux, MSVC so' no windows-debug); o autoteste, que roda
    IGUAL nos seis trabalhos (GODS_LAWS.md L-04), e' o unico lugar onde
    dialeto e host podem divergir - MSVC dialeto Windows, autoteste GNU
    rodando no MESMO processo Windows."""
    return ntpath if dialect == "MSVC" else posixpath


def normalize_compiler_path(dialect, raw_path, workdir):
    """Resolve o caminho como o compilador o ABRIU: absoluto pro de
    sistema, relativo ao DIRETORIO DE TRABALHO pro de aspas (docs/
    plano-layers-l5.md §1.1/§3.1, medicao do g++). `isabs`/`join`/
    `realpath` vem do modulo de caminho do DIALETO (`_path_module_for_
    dialect`), nunca de `os.path` cru - ver o comentario ali pro
    defeito real que isto fecha."""
    path_mod = _path_module_for_dialect(dialect)
    candidate = raw_path if path_mod.isabs(raw_path) else path_mod.join(workdir, raw_path)
    return _normcase_for_dialect(dialect, path_mod.realpath(candidate))


def _parse_tree(ctx, raw_output, workdir):
    """Parse + normalizacao num passo so' - tudo a jusante trabalha com
    caminho JA normalizado, sem precisar carregar dialeto/workdir por
    toda parte (GODS_LAWS.md L-17). docs/plano-layers-l5-adendo-
    calibracao.md L-5d: "repassa a contagem" de linhas fora do formato
    - via EFEITO COLATERAL em `ctx.out_of_tree_lines_total` (o mesmo
    padrao ja usado por `ctx.msvc_prefix`/`ctx.layer_dirs`), nao por um
    retorno em tupla, pra nao alargar a assinatura de TODO chamador
    (`_judge_one_case`, `_judge_sentinel`, `run_calibration` e varios
    selftests). MSVC nao tem contador proprio (parse_msvc_showincludes_
    tree ja PULA linha sem contar, nunca parou o laco); a contagem sai
    por diferenca (total de linhas - nos lidos), mesma semantica."""
    if ctx.dialect == "GNU":
        nodes, out_of_format_count = parse_gnu_dash_h_tree(raw_output)
    else:
        nodes = parse_msvc_showincludes_tree(raw_output, ctx.msvc_prefix)
        out_of_format_count = len(raw_output.splitlines()) - len(nodes)
    ctx.out_of_tree_lines_total += out_of_format_count
    with_parent = build_parent_map(nodes)
    return [
        (depth, normalize_compiler_path(ctx.dialect, raw_path, workdir), parent_index)
        for depth, raw_path, parent_index in with_parent
    ]


# --- classificacao (secao 5.1) ------------------------------------------


def classify_include_path(ctx, normalized_path):
    """docs/plano-layers-l5.md §5.1, NESTA ORDEM: (1) dentro de uma
    camada pura da fixture -> "projeto" (examinavel - os filhos dele
    TAMBEM sao examinados, O-4); (2) e' um dos cabecalhos GERADOS ->
    "gerado" (folha, O-3); (3) esta no conjunto de caminhos PADRAO
    aprendido na calibracao -> "padrao" (folha, O-3); (4) qualquer
    outra coisa -> "proibido". O separador de subpasta vem do DIALETO
    (`_path_module_for_dialect`), nunca de `os.sep` cru - mesmo defeito
    e mesmo conserto de `normalize_compiler_path` (achado 23/09/2026,
    run 35906529355): `os.sep` e' do HOST, e um `layer_dir` ja'
    normalizado em POSIX (dialeto GNU) rodando num host Windows nunca
    bate contra `normalized_path + "\\"` - o proprio no do caminho da
    camada pura caia em "proibido" por essa fresta."""
    sep = _path_module_for_dialect(ctx.dialect).sep
    for layer_dir in ctx.layer_dirs:
        if normalized_path == layer_dir or normalized_path.startswith(layer_dir + sep):
            return "projeto"
    if normalized_path in ctx.stub_generated_paths:
        return "gerado"
    if normalized_path in ctx.standard_paths:
        return "padrao"
    return "proibido"


def find_forbidden_pulls(ctx, normalized_nodes):
    """Percorre a arvore RECONSTRUIDA - so' desce em filho de no
    classificado "projeto" ou do arquivo raiz (docs/plano-layers-l5.md
    §5.1, O-2/O-3/O-4)."""
    classifications = {}
    forbidden = []
    for index, (_depth, path, parent_index) in enumerate(normalized_nodes):
        parent_ok = parent_index is None or classifications.get(parent_index) == "projeto"
        if not parent_ok:
            continue
        classe = classify_include_path(ctx, path)
        classifications[index] = classe
        if classe == "proibido":
            forbidden.append(path)
    return forbidden


# --- as duas direcoes (secao 5.2) ---------------------------------------


def compare_direction(pulled_forbidden, returncode, veredito_real):
    """docs/plano-layers-l5.md §5.2: a promessa e' SO' a direcao 1
    ("compilador puxou proibido" => "portao reprovou"). O julgamento e'
    pelo que foi PUXADO, NUNCA pelo codigo de saida (O-6) - so' quando
    NADA proibido foi puxado e' que o codigo de saida decide entre
    "concorda-passa"/"super-aproximacao" e "recusado"."""
    portao_reprovou = veredito_real == "reprovou"
    if pulled_forbidden:
        return "concorda-reprova" if portao_reprovou else "violacao"
    if returncode != 0:
        return "recusado"
    return "super-aproximacao" if portao_reprovou else "concorda-passa"


# --- calibracao (secao 7.1) ----------------------------------------------


class _CalibrationExpectedPaths:
    """Agrupa os dois caminhos ESPERADOS que `evaluate_calibration()`
    confere contra a arvore reconstruida (GODS_LAWS.md L-17: reduz a
    assinatura de `evaluate_calibration` de 5 pra 4 parametros - mesmo
    motivo de `_OracleContext`/`_CalibrationRawResult`)."""

    __slots__ = ("sentinel_norm", "leaf_norm")

    def __init__(self, sentinel_norm, leaf_norm):
        self.sentinel_norm = sentinel_norm
        self.leaf_norm = leaf_norm


class _CalibrationResult:
    """docs/plano-layers-l5-adendo-calibracao.md L-5e/L-5f:
    `standard_paths` (o conjunto de caminhos REAIS achados na
    calibracao - informativo desde L-5f, que repoe o papel dele na
    classificacao de CASO pelos vazios de `std_stubs/`), `census`
    (nomes-base do C-2, pro piso e pro relatorio), `absent` (nomes
    permitidos FORA do censo, sempre impressos - GODS_LAWS.md L-40) e
    `cstdint_children` (os filhos do `<cstdint>` REAL sob a sentinela,
    na ORDEM em que apareceram - de onde `X` da sentinela de sombra e'
    escolhido, secao 1.6)."""

    __slots__ = ("standard_paths", "census", "absent", "cstdint_children")

    def __init__(self, standard_paths, census, absent, cstdint_children):
        self.standard_paths = standard_paths
        self.census = census
        self.absent = absent
        self.cstdint_children = cstdint_children


def build_calibration_fixture(scratch_dir, stdlib_permitidos, layer_dir_parts):
    """Um arquivo, numa pasta pura de uma raiz de CALIBRACAO (propria,
    nunca a raiz de um caso real). docs/plano-layers-l5-adendo-
    calibracao.md secao 3 (C-3 endurecido): a SONDA comeca pela
    sentinela (`#include "sentinela_projeto.hpp"` e' a PRIMEIRA linha -
    antes vinha por ultimo), e a sentinela inclui `<cstdint>` e DEPOIS
    `folha_projeto.hpp` (arquivo IRMAO, vazio, nome unico - so' prova
    atribuicao de pai). So' DEPOIS da sentinela vem a tentativa de
    incluir CADA nome permitido via `__has_include` (ausencia de um
    cabecalho num compilador velho nao mata a calibracao inteira)."""
    layer_dir = os.path.join(scratch_dir, *layer_dir_parts)
    os.makedirs(layer_dir, exist_ok=True)
    leaf_path = os.path.join(layer_dir, "folha_projeto.hpp")
    with open(leaf_path, "w", encoding="utf-8", newline="\n"):
        pass
    sentinel_path = os.path.join(layer_dir, "sentinela_projeto.hpp")
    with open(sentinel_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write('#include <cstdint>\n#include "folha_projeto.hpp"\n')
    lines = ['#include "sentinela_projeto.hpp"']
    for name in stdlib_permitidos:
        lines.append(f"#if __has_include(<{name}>)")
        lines.append(f"#include <{name}>")
        lines.append("#endif")
    calibration_path = os.path.join(layer_dir, "calibration_probe.hpp")
    with open(calibration_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    return calibration_path, sentinel_path, leaf_path


def _sentinel_first_depth1_index(depth1, sentinel_norm):
    """C-3 endurecido (docs/plano-layers-l5-adendo-calibracao.md secao
    3): a sentinela tem de ser o PRIMEIRO no de profundidade 1 - nao
    so' 'aparecer em algum lugar'. A valvula antiga ("`<cstdint>`
    direto") SAI: so' existia porque a sentinela vinha por ultimo."""
    if depth1 and depth1[0][1] == sentinel_norm:
        return depth1[0][0]
    return None


def _stdlib_census(normalized_nodes, depth1, stdlib_names):
    """docs/plano-layers-l5-adendo-calibracao.md secao 3 (C-2): nomes-
    base permitidos presentes em QUALQUER profundidade, cujo DIRETORIO
    e' o de pelo menos um filho DIRETO da sonda com nome permitido -
    barra homonimos em `tr1/`, `experimental/`, `ext/` (O-22)."""
    allowed_dirs = {os.path.dirname(path) for _idx, path in depth1 if os.path.basename(path) in stdlib_names}
    return {
        os.path.basename(path)
        for _depth, path, _parent in normalized_nodes
        if os.path.basename(path) in stdlib_names and os.path.dirname(path) in allowed_dirs
    }


def _cstdint_index_under(normalized_nodes, sentinel_index):
    return next(
        (idx for idx, (_d, path, parent) in enumerate(normalized_nodes)
         if parent == sentinel_index and os.path.basename(path) == "cstdint"),
        None,
    )


def _children_paths(normalized_nodes, parent_index):
    """docs/plano-layers-l5-adendo-calibracao.md secao 1.6: os filhos
    DIRETOS de um no, na ordem em que apareceram - usado pra achar `X`
    da sentinela de sombra (`cstdint_children` de `_CalibrationResult`)."""
    return tuple(path for _d, path, parent in normalized_nodes if parent == parent_index)


def evaluate_calibration(ctx, normalized_nodes, expected, stdlib_permitidos):
    """Reprova (falha de INSTRUMENTO, NUNCA pulo), nesta ordem, se: a
    sentinela nao e' o PRIMEIRO no de profundidade 1; a folha nao
    aparece como filha dela; `<cstdint>` nao aparece como filho dela
    (as tres exigencias duras de docs/plano-layers-l5-adendo-
    calibracao.md secao 3); ou o CENSO (secao 3, C-2) tem menos que
    `_MIN_STDLIB_PATHS` nomes (O-8/O-22)."""
    del ctx  # mantido na assinatura por simetria com as demais funcoes de baixo nivel
    stdlib_names = set(stdlib_permitidos)
    depth1 = [(idx, path) for idx, (depth, path, _parent) in enumerate(normalized_nodes) if depth == 1]
    sentinel_index = _sentinel_first_depth1_index(depth1, expected.sentinel_norm)
    if sentinel_index is None:
        raise _IncludeTreeError(
            "calibracao: sentinela_projeto.hpp nao e' o PRIMEIRO no de profundidade 1 (falha de instrumento)"
        )
    leaf_under = any(
        parent == sentinel_index and path == expected.leaf_norm for _d, path, parent in normalized_nodes
    )
    if not leaf_under:
        raise _IncludeTreeError("calibracao: folha_projeto.hpp nao apareceu sob a sentinela (falha de instrumento)")
    cstdint_index = _cstdint_index_under(normalized_nodes, sentinel_index)
    if cstdint_index is None:
        raise _IncludeTreeError("calibracao: <cstdint> nao apareceu sob a sentinela (falha de instrumento)")

    standard_paths = {path for _idx, path in depth1 if os.path.basename(path) in stdlib_names}
    census = _stdlib_census(normalized_nodes, depth1, stdlib_names)
    if len(census) < _MIN_STDLIB_PATHS:
        raise _IncludeTreeError(
            f"calibracao: so' {len(census)} nomes no censo, piso e {_MIN_STDLIB_PATHS} (instrumento cego)"
        )
    absent = sorted(stdlib_names - census)
    cstdint_children = _children_paths(normalized_nodes, cstdint_index)
    return _CalibrationResult(standard_paths, census, absent, cstdint_children)


class _CalibrationRawResult:
    """Agrupa o resultado cru da UMA invocacao de calibracao (raw_output
    + returncode) - existe so' pra `_format_calibration_diagnostics`
    caber em 4 parametros (GODS_LAWS.md L-17), mesmo motivo de
    `_OracleContext`."""

    __slots__ = ("raw_output", "returncode")

    def __init__(self, raw_output, returncode):
        self.raw_output = raw_output
        self.returncode = returncode


def _is_tree_output_line(ctx, line):
    """Uma linha e' "de arvore" (-H/showIncludes) se casa o formato do
    DIALETO - GNU pelo padrao de pontos, MSVC pelo prefixo JA' aprendido
    (`ctx.msvc_prefix`, sempre setado antes desta funcao rodar dentro de
    `run_calibration` - achado 23/09/2026, run 35906529355/35912952114:
    a mensagem de falha precisa separar linha de arvore de linha de
    DIAGNOSTICO do compilador, porque e' ali - nao na arvore - que um
    `#error`/mensagem fatal apareceria)."""
    if ctx.dialect == "MSVC":
        return ctx.msvc_prefix is not None and line.startswith(ctx.msvc_prefix)
    return bool(_GNU_TREE_LINE_PATTERN.match(line))


def _format_calibration_diagnostics(ctx, normalized_nodes, sentinel_norm, raw_result):
    """docs/plano-layers-l5.md §7.1, achado 23/09/2026 (run 35906529355,
    35912952114): a mensagem de falha de instrumento passa a IMPRIMIR o
    dado cru, nunca so' o nome do defeito (GODS_LAWS.md L-40/L-44) - os
    nos de profundidade 1 JA' normalizados, o `sentinel_norm` esperado,
    as 20 primeiras E as 20 ULTIMAS linhas CRUAS de `-H`/`/showIncludes`
    (a rodada anterior so' tinha as primeiras - o dado que faltou pra
    diagnosticar 35912952114 estava nas ultimas), o `returncode` do
    processo de calibracao (antes descartado, `run_calibration:542`), e
    as 20 primeiras E as 20 ultimas linhas que NAO sao de arvore (onde
    um `#error`/mensagem fatal do compilador apareceria, se houver -
    docs/plano-layers-l5-adendo-calibracao.md L-5d: antes so' as
    ultimas; a primeira metade do fluxo tambem pode carregar o
    diagnostico, ex. um `#warning` cedo na arvore)."""
    depth1 = [(idx, path) for idx, (depth, path, _parent) in enumerate(normalized_nodes) if depth == 1]
    all_lines = raw_result.raw_output.splitlines()
    non_tree_lines = [line for line in all_lines if not _is_tree_output_line(ctx, line)]
    lines = [
        f"  no(s) de profundidade 1 normalizados ({len(depth1)}): {[p for _i, p in depth1]!r}",
        f"  sentinel_norm esperado: {sentinel_norm!r}",
        f"  returncode da calibracao: {raw_result.returncode!r}",
        f"  20 primeiras linhas CRUAS do -H/showIncludes ({min(20, len(all_lines))} mostradas):",
    ]
    lines.extend(f"    {line!r}" for line in all_lines[:20])
    lines.append(f"  20 ultimas linhas CRUAS do -H/showIncludes ({min(20, len(all_lines))} mostradas):")
    lines.extend(f"    {line!r}" for line in all_lines[-20:])
    lines.append(f"  20 primeiras linhas NAO-arvore ({min(20, len(non_tree_lines))} mostradas):")
    lines.extend(f"    {line!r}" for line in non_tree_lines[:20])
    lines.append(f"  20 ultimas linhas NAO-arvore ({min(20, len(non_tree_lines))} mostradas):")
    lines.extend(f"    {line!r}" for line in non_tree_lines[-20:])
    return "\n".join(lines)


def _print_calibration_report(ctx, result):
    """docs/plano-layers-l5-adendo-calibracao.md L-5e: censo, ausentes
    e (no MSVC) o prefixo aprendido - impressos SEMPRE, nunca so' na
    falha (GODS_LAWS.md L-40)."""
    print(f"{SCRIPT_NAME}: calibracao: censo={len(result.census)} (piso {_MIN_STDLIB_PATHS})")
    print(f"{SCRIPT_NAME}: calibracao: ausentes ({len(result.absent)}): {result.absent!r}")
    if ctx.dialect == "MSVC":
        print(f"{SCRIPT_NAME}: calibracao: prefixo MSVC aprendido: {ctx.msvc_prefix!r}")


def run_calibration(ctx, scratch, manifest):
    """UM processo, antes de qualquer fixture de caso (docs/plano-
    layers-l5.md §6.1/§7.1). Falha de instrumento (`_IncludeTreeError`
    de `evaluate_calibration`) e' re-levantada com o diagnostico de
    `_format_calibration_diagnostics` anexado - achado 23/09/2026: a
    mensagem curta sozinha nao bastou pra diagnosticar a calibracao
    quebrando so' no servidor (run 35906529355/35912952114)."""
    calib_dir = os.path.join(scratch, "calibration")
    os.makedirs(calib_dir, exist_ok=True)
    layer_parts = manifest["camadas_puras"][0]["partes"]
    calib_path, sentinel_path, leaf_path = build_calibration_fixture(
        calib_dir, manifest["stdlib_permitidos"], layer_parts
    )
    raw_output, returncode, workdir = _run_one_alvo(ctx, calib_dir, calib_path, ctx.include_dirs)
    if ctx.dialect == "MSVC":
        ctx.msvc_prefix = learn_msvc_prefix(raw_output, os.path.basename(sentinel_path))
    normalized_nodes = _parse_tree(ctx, raw_output, workdir)
    expected = _CalibrationExpectedPaths(
        normalize_compiler_path(ctx.dialect, sentinel_path, workdir),
        normalize_compiler_path(ctx.dialect, leaf_path, workdir),
    )
    try:
        result = evaluate_calibration(ctx, normalized_nodes, expected, manifest["stdlib_permitidos"])
    except _IncludeTreeError as original:
        raw_result = _CalibrationRawResult(raw_output, returncode)
        diagnostics = _format_calibration_diagnostics(ctx, normalized_nodes, expected.sentinel_norm, raw_result)
        raise _IncludeTreeError(f"{original}\n{diagnostics}") from original
    _print_calibration_report(ctx, result)
    # docs/plano-layers-l5-adendo-calibracao.md L-5f: devolve o
    # _CalibrationResult INTEIRO (nao so' standard_paths) - quem chama
    # (run_oracle) precisa de `cstdint_children` pra escolher X da
    # sentinela de sombra (secao 1.6). `ctx.standard_paths` pra
    # classificacao de CASO deixa de vir daqui: L-5f a substitui pelos
    # caminhos de `std_stubs/` (secao 1.3).
    return result


# --- sentinelas por fixture (secao 7.2) ----------------------------------


def _assert_no_children_of_empty_leaves(ctx, normalized_nodes):
    """TRAVA DE FOLHA VAZIA (docs/plano-layers-l5-adendo-calibracao.md
    secao 1.6, L-5f) - funcao IRMA de `find_forbidden_pulls` (nao
    mistura logica de classificacao com esta prova estrutural, O-24):
    na CONFIGURACAO DE CASO, um arquivo GERADO ou de `std_stubs/` e'
    VAZIO por construcao e NAO PODE ter filho. Olha os caminhos direto
    (`ctx.stub_generated_paths` | `ctx.standard_paths`, que em
    configuracao de caso SAO os vazios), nao a classificacao - por
    isso nao interfere no O-2/O-3/O-4 (que simulam a classificacao
    'padrao' antiga, sem vir de arquivo vazio de verdade)."""
    empty_paths = ctx.stub_generated_paths | ctx.standard_paths
    path_by_index = {idx: path for idx, (_d, path, _p) in enumerate(normalized_nodes)}
    for _depth, path, parent_index in normalized_nodes:
        if parent_index is None:
            continue
        if path_by_index.get(parent_index) in empty_paths:
            raise _IncludeTreeError(
                f"trava de folha vazia: {path!r} e' filho de {path_by_index[parent_index]!r} "
                "(VAZIO por construcao, falha de instrumento)"
            )


def _judge_sentinel(ctx, scratch_dir, content, expect_forbidden):
    layer_parts = ctx.camadas_puras[0]["partes"]
    layer_dir = os.path.join(scratch_dir, *layer_parts)
    os.makedirs(layer_dir, exist_ok=True)
    path = os.path.join(layer_dir, "sentinel_probe.hpp")
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)
    # docs/plano-layers-l5-adendo-calibracao.md L-5f: sentinelas rodam
    # na configuracao de CASO (com std_stubs/) - a negativa mede, a
    # cada rodada, que um proibido real continua sendo visto mesmo
    # tropecando nos vazios.
    raw, returncode, workdir = _run_one_alvo(ctx, scratch_dir, path, ctx.case_include_dirs)
    normalized_nodes = _parse_tree(ctx, raw, workdir)
    _assert_no_children_of_empty_leaves(ctx, normalized_nodes)
    forbidden = find_forbidden_pulls(ctx, normalized_nodes)
    if expect_forbidden and not forbidden:
        raise _IncludeTreeError("sentinela negativa: <fstream> nao apareceu como puxado (falha de instrumento)")
    if not expect_forbidden and (forbidden or returncode != 0):
        raise _IncludeTreeError("sentinela positiva: <cstdint> nao saiu limpa (falha de instrumento)")


def _choose_shadow_sentinel_candidate(cstdint_children, stdlib_permitidos):
    """docs/plano-layers-l5-adendo-calibracao.md secao 1.6: `X` = o
    PRIMEIRO filho do `<cstdint>` REAL cujo nome-base NAO esta em
    `stdlib_permitidos` - a prova viva de que o `<cstdint>` do sistema
    abre por dentro algo que o portao proibiria se fosse escrito
    direto num arquivo de camada pura."""
    stdlib_names = set(stdlib_permitidos)
    for path in cstdint_children:
        if os.path.basename(path) not in stdlib_names:
            return path
    return None


def _run_shadow_probe(ctx, scratch_dir, shadow_candidate, include_dirs):
    """Roda `#include <cstdint>` seguido de `#include "X"` (caminho
    ABSOLUTO real, entre aspas) numa CONFIGURACAO dada, e devolve True
    se `X` aparece como FILHO DIRETO da sonda (profundidade 1) - visto
    de verdade, nao sombreado por uma abertura anterior na MESMA
    unidade de traducao (secao 1.6)."""
    layer_parts = ctx.camadas_puras[0]["partes"]
    layer_dir = os.path.join(scratch_dir, *layer_parts)
    os.makedirs(layer_dir, exist_ok=True)
    probe_path = os.path.join(layer_dir, "shadow_probe.hpp")
    with open(probe_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(f'#include <cstdint>\n#include "{shadow_candidate}"\n')
    raw, _returncode, workdir = _run_one_alvo(ctx, scratch_dir, probe_path, include_dirs)
    nodes = _parse_tree(ctx, raw, workdir)
    candidate_norm = normalize_compiler_path(ctx.dialect, shadow_candidate, workdir)
    return any(depth == 1 and path == candidate_norm for depth, path, _parent in nodes)


def _run_shadow_sentinel(ctx, scratch, calib_result, stdlib_permitidos):
    """docs/plano-layers-l5-adendo-calibracao.md secao 1.6: sem
    candidato `X` -> falha de instrumento (nada pra provar). Config.
    REAL: impressa SEMPRE, NUNCA reprova - mede o FENOMENO (a regra da
    casa e' que "sombra: sim" e' esperado, nao um defeito). Config. de
    CASO: `X` TEM que aparecer como filho direto - senao a construcao
    anti-sombra (secao 1.3) nao fechou, e isso e' falha de instrumento."""
    candidate = _choose_shadow_sentinel_candidate(calib_result.cstdint_children, stdlib_permitidos)
    if candidate is None:
        raise _IncludeTreeError(
            "sentinela de sombra: nenhum candidato X achado sob <cstdint> real (falha de instrumento)"
        )
    seen_real = _run_shadow_probe(ctx, os.path.join(scratch, "shadow_real"), candidate, ctx.include_dirs)
    print(f"{SCRIPT_NAME}: sombra presente neste compilador: {'nao' if seen_real else 'sim'}")
    seen_case = _run_shadow_probe(ctx, os.path.join(scratch, "shadow_case"), candidate, ctx.case_include_dirs)
    if not seen_case:
        raise _IncludeTreeError(
            f"sentinela de sombra: X={candidate!r} nao apareceu como filho direto na configuracao de "
            "caso (falha de instrumento - a construcao anti-sombra nao fechou)"
        )


def run_sentinels(ctx, scratch, calib_result, stdlib_permitidos):
    """docs/plano-layers-l5.md §7.2, reformado por docs/plano-layers-
    l5-adendo-calibracao.md L-5f: `<fstream>` tem de sair "puxou
    proibido"; `<cstdint>` tem de sair "so' permitidos, codigo 0" - as
    duas na CONFIGURACAO DE CASO (com `std_stubs/`); qualquer outro
    resultado e' falha de instrumento, medida a cada rodada. A
    sentinela de SOMBRA roda nas DUAS configuracoes (secao 1.6)."""
    ctx.layer_dirs = _layer_dirs_for_root(ctx, os.path.join(scratch, "sentinels"))
    _judge_sentinel(ctx, os.path.join(scratch, "sentinels_neg"), "#include <fstream>\n", expect_forbidden=True)
    _judge_sentinel(ctx, os.path.join(scratch, "sentinels_pos"), "#include <cstdint>\n", expect_forbidden=False)
    _run_shadow_sentinel(ctx, scratch, calib_result, stdlib_permitidos)


# --- execucao sequencial, um caso por vez (secao 3, 9) -------------------


def _run_one_alvo(ctx, scratch_dir, alvo_abs_path, include_dirs):
    """UMA invocacao SEQUENCIAL de compilador (docs/plano-layers-l5.md
    §3.1: "nada de pool de processos" - a L-45 abre excecao a L-11 so'
    no servidor e SEM paralelismo). Devolve o texto BRUTO - quem chama
    decide como interpretar (a calibracao MSVC precisa do texto cru pra
    aprender o prefixo ANTES de qualquer parse). `include_dirs` chega
    EXPLICITO do chamador (L-5f) - real (`ctx.include_dirs`) ou de caso
    (`ctx.case_include_dirs`)."""
    command, workdir = build_preprocess_command(ctx, alvo_abs_path, scratch_dir, include_dirs)
    result = ctx.executor(command, workdir, _INVOCATION_TIMEOUT_SECONDS)
    if result.timed_out:
        raise _IncludeTreeError(f"teto de {_INVOCATION_TIMEOUT_SECONDS}s estourado em {alvo_abs_path}")
    return result.output, result.returncode, workdir


def _layer_dirs_for_root(ctx, root_abs):
    return tuple(
        normalize_compiler_path(ctx.dialect, os.path.join(root_abs, *entry["partes"]), root_abs)
        for entry in ctx.camadas_puras
    )


def _judge_one_case(ctx, scratch_dir, case, root_abs):
    """docs/plano-layers-l5.md §3/§5: roda o(s) alvo(s) FONTE do caso,
    agrega "puxou proibido" por OU entre eles (o veredito_real do
    portao e' UM SO' pra fixture inteira - check_layers() nao devolve
    um veredito por arquivo)."""
    ctx.layer_dirs = _layer_dirs_for_root(ctx, root_abs)
    pulled_forbidden_overall = False
    worst_returncode = 0
    for alvo in case["alvos"]:
        if alvo["tipo"] != "fonte":
            continue
        alvo_abs = os.path.join(root_abs, *alvo["caminho"].split("/"))
        # L-5f: configuracao de CASO (com std_stubs/ por ultimo).
        raw_output, returncode, workdir = _run_one_alvo(ctx, scratch_dir, alvo_abs, ctx.case_include_dirs)
        normalized_nodes = _parse_tree(ctx, raw_output, workdir)
        _assert_no_children_of_empty_leaves(ctx, normalized_nodes)
        forbidden = find_forbidden_pulls(ctx, normalized_nodes)
        pulled_forbidden_overall = pulled_forbidden_overall or bool(forbidden)
        if returncode != 0:
            worst_returncode = returncode
    return compare_direction(pulled_forbidden_overall, worst_returncode, case["veredito_real"])


# --- particao dos casos exportados (secao 7.3) ---------------------------


def summarize_manifest_cases(manifest):
    """Particiona os casos exportados em tres grupos ANTES de qualquer
    compilador rodar: "calibracao" (C12 - nao compilados individualmente,
    a calibracao ja pergunta a MESMA coisa numa tacada so'), "fora_de_
    escopo" (alvo classificado nao-fonte pelo proprio portao - hoje so'
    D15), e "compilar" (o resto, um processo cada, O-15)."""
    calibracao, fora_de_escopo, compilar = [], [], []
    for case in manifest["casos"]:
        if case["modo_oraculo"] == "calibracao":
            calibracao.append(case)
            continue
        source_targets = [alvo for alvo in case["alvos"] if alvo["tipo"] == "fonte"]
        if not source_targets:
            fora_de_escopo.append(case)
            continue
        compilar.append(case)
    return calibracao, fora_de_escopo, compilar


# --- relatorio final e pisos (secao 7.3) ----------------------------------


def _final_report(buckets, violations, manifest, out_of_scope_counts):
    calibracao_n, fora_de_escopo_n = out_of_scope_counts
    comparados = sum(buckets.values())
    puxou_proibido = buckets["concorda-reprova"] + buckets["violacao"]
    total = comparados + calibracao_n + fora_de_escopo_n

    print(
        f"{SCRIPT_NAME}: exportados={manifest['total_casos']} calibracao={calibracao_n} "
        f"fora_de_escopo={fora_de_escopo_n} comparados={comparados}"
    )
    for bucket_name in ("concorda-passa", "concorda-reprova", "super-aproximacao", "recusado", "violacao"):
        print(f"{SCRIPT_NAME}: balde {bucket_name}: {buckets[bucket_name]}")

    ok = True
    if comparados == 0:
        print(f"{SCRIPT_NAME}: FALHOU (comparados=0)", file=sys.stderr)
        ok = False
    if total != manifest["total_casos"]:
        print(
            f"{SCRIPT_NAME}: FALHOU (comparados+fora_de_escopo+calibracao={total} != "
            f"exportados={manifest['total_casos']})",
            file=sys.stderr,
        )
        ok = False
    if puxou_proibido < _MIN_FORBIDDEN_PULLS:
        print(f"{SCRIPT_NAME}: FALHOU (puxou_proibido={puxou_proibido} < piso {_MIN_FORBIDDEN_PULLS})", file=sys.stderr)
        ok = False
    if violations:
        names = ", ".join(f"{case['tabela']}:{case['caso']}" for case in violations)
        print(f"{SCRIPT_NAME}: FALHOU (VIOLACAO em: {names})", file=sys.stderr)
        ok = False
    return ok


def _write_stub_headers(stub_dir, generated_names):
    """<stubs> = pasta com os DOIS cabecalhos gerados, VAZIOS, com os
    nomes lidos do MANIFESTO (a propria `_GENERATED_HEADERS` do portao -
    fonte unica)."""
    for name in generated_names:
        path = os.path.join(stub_dir, *name.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8"):
            pass


def _write_std_stubs(ctx, stub_dir, stdlib_permitidos):
    """docs/plano-layers-l5-adendo-calibracao.md secao 1.3 (D-L5d-1): um
    arquivo VAZIO, SEM guarda, por nome de `stdlib_permitidos` - fecha o
    falso negativo por SOMBRA por CONSTRUCAO (secao 1): um `<nome>`
    permitido resolve pra este vazio (a pasta entra como o ULTIMO `-I`,
    O-23), e um proibido continua resolvendo pro real. Devolve o
    conjunto de caminhos NORMALIZADOS - vira `ctx.standard_paths` pra
    classificacao de CASO, substituindo o papel que a calibracao
    (caminhos reais) tinha antes de L-5f."""
    os.makedirs(stub_dir, exist_ok=True)
    for name in stdlib_permitidos:
        with open(os.path.join(stub_dir, name), "w", encoding="utf-8"):
            pass
    return frozenset(
        normalize_compiler_path(ctx.dialect, os.path.join(stub_dir, name), stub_dir)
        for name in stdlib_permitidos
    )


def _compose_case_include_dirs(include_dirs, std_stub_dir):
    """docs/plano-layers-l5-adendo-calibracao.md O-23: `std_stubs/` e'
    sempre o ULTIMO diretorio de inclusao da configuracao de CASO,
    depois dos da fixture e dos gerados (`include_dirs`) - fatorado do
    corpo de `run_oracle` pra virar ponto de mutacao unico e testavel
    (M-O23a/b)."""
    return (*include_dirs, std_stub_dir)


def run_oracle(ctx, manifest, export_dir, scratch):
    """O laco principal (docs/plano-layers-l5.md §2): calibracao,
    sentinelas, depois um processo SEQUENCIAL por caso "compilar"."""
    ctx.camadas_puras = manifest["camadas_puras"]
    stub_dir = os.path.join(scratch, "stubs")
    _write_stub_headers(stub_dir, manifest["gerados"])
    ctx.include_dirs = (*ctx.include_dirs, stub_dir)
    ctx.stub_generated_paths = frozenset(
        normalize_compiler_path(ctx.dialect, os.path.join(stub_dir, *name.split("/")), stub_dir)
        for name in manifest["gerados"]
    )
    # docs/plano-layers-l5-adendo-calibracao.md L-5f: a configuracao de
    # CASO ganha std_stubs/ como ULTIMO diretorio (O-23); a calibracao
    # continua SEM ele (ctx.include_dirs, acima, fica intocado).
    std_stub_dir = os.path.join(scratch, "std_stubs")
    ctx.standard_paths = _write_std_stubs(ctx, std_stub_dir, manifest["stdlib_permitidos"])
    ctx.case_include_dirs = _compose_case_include_dirs(ctx.include_dirs, std_stub_dir)

    calib_result = run_calibration(ctx, scratch, manifest)
    run_sentinels(ctx, scratch, calib_result, manifest["stdlib_permitidos"])

    calibracao_cases, fora_de_escopo_cases, compilar_cases = summarize_manifest_cases(manifest)
    buckets = collections.Counter()
    violations = []
    for case in compilar_cases:
        root_abs = os.path.join(export_dir, case["raiz"])
        bucket = _judge_one_case(ctx, scratch, case, root_abs)
        buckets[bucket] += 1
        if bucket == "violacao":
            violations.append(case)

    # docs/plano-layers-l5-adendo-calibracao.md L-5d: total de linhas
    # fora do formato de arvore, SEMPRE impresso (GODS_LAWS.md L-40) -
    # acumulado por _parse_tree() em CADA chamada (calibracao,
    # sentinelas, todos os casos "compilar"), nunca so' na falha.
    print(f"{SCRIPT_NAME}: linhas fora da arvore (total do trabalho): {ctx.out_of_tree_lines_total}")
    out_of_scope_counts = (len(calibracao_cases), len(fora_de_escopo_cases))
    return _final_report(buckets, violations, manifest, out_of_scope_counts)


# --- modo real: CLI e recusa por GITHUB_ACTIONS (secao 6.2) --------------


def _oracle_should_run(oracle_option_flag):
    """docs/plano-layers-l5.md §6.2 item 2 (GODS_LAWS.md L-45),
    reformado por DECISOES_AUTONOMAS.md 23/09/2026: SO' roda quando as
    DUAS coisas sao verdade ao mesmo tempo - dentro do CI real do
    servidor (`GITHUB_ACTIONS == "true"`, fato do AMBIENTE, nunca do
    repositorio) E a opcao `GLINTFX_LAYERS_ORACLE` foi ligada
    EXPLICITAMENTE pra este trabalho de CMake (`oracle_option_flag ==
    "ON"`, passado pelo `add_test()` - ver tests/CMakeLists.txt).
    Faltando qualquer uma das duas - inclusive dentro do CI real, numa
    perna estatica ou num job fora dos seis escolhidos - PULA, nunca
    reprova."""
    return os.environ.get("GITHUB_ACTIONS") == "true" and oracle_option_flag == "ON"


def skip_if_not_enabled(oracle_option_flag):
    """Sai com `_SKIP_RETURN_CODE` (CTest SKIP_RETURN_CODE, docs/plano-
    layers-l5.md §6.2, reformado por DECISOES_AUTONOMAS.md 23/09/2026)
    ANTES de qualquer processo quando `_oracle_should_run()` for falso -
    nesta maquina (e em toda perna sem a opcao ligada) o oraculo e'
    desligado por LEI, nunca por "pulo" silencioso: a mensagem e'
    sempre impressa (GODS_LAWS.md L-40)."""
    if not _oracle_should_run(oracle_option_flag):
        print(
            f"{SCRIPT_NAME}: oraculo desligado nesta maquina - GODS_LAWS.md L-45, "
            "so' roda no CI do servidor com GLINTFX_LAYERS_ORACLE=ON"
        )
        sys.exit(_SKIP_RETURN_CODE)


def check_preci_sh_excludes_oracle(preci_sh_text):
    """O-13 (docs/plano-layers-l5.md §6.2 item 3): `tools/preci.sh` e' o
    espelho LOCAL do CI - a L-45 diz "nem no tools/preci.sh". Devolve
    True quando o texto NAO cita o nome da opcao."""
    return "GLINTFX_LAYERS_ORACLE" not in preci_sh_text


def _load_manifest(export_dir):
    with open(os.path.join(export_dir, _MANIFEST_FILENAME), "r", encoding="utf-8") as handle:
        return json.load(handle)


def oracle_main(args):
    if len(args) != 5:
        fail(
            "usage: check_layers_oracle.py <source-root> <build-dir> <check_layers.py> "
            "<dialect> <ON|OFF>"
        )
    source_root, build_dir, check_layers_path, dialect, oracle_option_flag = args
    skip_if_not_enabled(oracle_option_flag)
    validate_dialect(dialect)

    with open(os.path.join(build_dir, "compile_commands.json"), "r", encoding="utf-8") as handle:
        compile_commands = json.load(handle)
    entry = find_compile_command_entry(compile_commands)
    if entry is None:
        fail("compile_commands.json: nenhuma entrada em src/core/ encontrada")
    tokens = tokenize_compile_command(entry)
    flags = extract_language_family_flags(tokens[1:], dialect)

    scratch = tempfile.mkdtemp(prefix="glintfx-layers-oracle-", dir=os.environ.get("TMPDIR"))
    try:
        export_dir = os.path.join(scratch, "export")
        subprocess.run(
            [sys.executable, check_layers_path, "--export-fixtures", export_dir],
            check=True, cwd=source_root,
        )
        manifest = _load_manifest(export_dir)
        include_dirs = (os.path.join(source_root, "include"),)
        ctx = _OracleContext(dialect, tokens[0], flags, include_dirs)
        ctx.executor = real_compiler_executor
        ok = run_oracle(ctx, manifest, export_dir, scratch)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    if not ok:
        fail("oraculo de camadas: reprovado (ver mensagens acima)")
    print(f"{SCRIPT_NAME}: oraculo de camadas OK")


# =========================================================================
# --- autoteste (--selftest): roda em TODO lugar, sem compilador -----------
# =========================================================================
#
# Executor FALSO por chamada (docs/plano-layers-l5.md §10.1): cada
# controle constroi o seu proprio, devolvendo texto ENLATADO - nunca
# chama subprocess, nunca olha pro `command` recebido alem de contar
# quantas vezes foi chamado quando isso importa pro controle.


def _fake_executor(output_text, returncode=0, timed_out=False):
    """Fabrica um executor que sempre devolve a MESMA saida enlatada,
    qualquer que seja o comando pedido - o suficiente pra exercitar o
    parser/classificador sem nenhum compilador real."""

    def executor(command, cwd, timeout):
        del command, cwd, timeout
        return _ExecResult(returncode, output_text, timed_out=timed_out)

    return executor


def _make_ctx_for_test(dialect="GNU"):
    ctx = _OracleContext(dialect, "cxx", (), ())
    ctx.camadas_puras = tuple({"rotulo": label, "partes": list(parts)} for label, parts in (
        ("src/core", ("src", "core")),
    ))
    return ctx


# --- fonte 1: MEDIDO, verbatim do plano de L-5 secao 1.1 (g++ 16.2.1) ---
#
# Fragmentos citados literalmente no plano (docs/plano-layers-l5.md
# §1.1): formato "<pontos><espaco><caminho>", caminho de sistema
# ABSOLUTO, caminho de aspas RELATIVO ao diretorio de trabalho, e a
# lista de guardas de inclusao no fim - traduzida pela localidade (o
# texto abaixo e' o PT-BR que a maquina do planejador mediu SEM
# LC_ALL=C - prova exatamente por que o parser nao pode reconhecer o
# terminador pelo CONTEUDO, so' pela forma que para de casar "^\.+ ").
_MEASURED_GNU_POSITIVE_CONTROL = (
    ". /usr/include/c++/16/cstdint\n"
    ".. /usr/include/c++/16/x86_64-redhat-linux/bits/c++config.h\n"
    ". x.hpp\n"
    ".. /usr/include/c++/16/cstddef\n"
    "Múltiplos include guards podem ser úteis para:\n"
    "/usr/include/c++/16/cstdint\n"
)


def selftest_oracle_o0_real_executor_guard(scratch, capture):
    """O-0 (docs/plano-layers-l5.md §10.1): o executor REAL levanta
    excecao quando chamado com a marca de autoteste ativa - a prova de
    que TODO o resto deste autoteste roda sem compilador nenhum."""
    del scratch, capture
    global _SELFTEST_MODE_ACTIVE
    _SELFTEST_MODE_ACTIVE = True
    try:
        raised = False
        try:
            real_compiler_executor(["true"], ".", 1)
        except _RealExecutorDuringSelftestError:
            raised = True
        ok = raised
    finally:
        _SELFTEST_MODE_ACTIVE = False
    if ok:
        print("selftest: O-0 OK (executor real recusa rodar durante --selftest)")
    else:
        print("selftest: O-0 FALHOU (executor real NAO recusou durante --selftest)", file=sys.stderr)
    return ok, 1


def selftest_oracle_positive_control(scratch, capture):
    """Controle positivo da casa (secao 10.2): fixture limpa, compilador
    so' puxa permitido, portao passa -> verde. Nenhum mutante mata este
    controle sozinho - ele prova que o oraculo NAO reprova tudo."""
    del scratch, capture
    ctx = _make_ctx_for_test("GNU")
    ctx.layer_dirs = ("/tmp/x",)
    ctx.standard_paths = {"/usr/include/c++/16/cstdint", "/usr/include/c++/16/cstddef"}
    nodes = _parse_tree(ctx, _MEASURED_GNU_POSITIVE_CONTROL, "/tmp/x")
    forbidden = find_forbidden_pulls(ctx, nodes)
    bucket = compare_direction(bool(forbidden), 0, "passou")
    ok = bucket == "concorda-passa" and not forbidden
    label = "selftest: positivo"
    print(f"{label} OK" if ok else f"{label} FALHOU (balde={bucket!r}, forbidden={forbidden!r})",
          file=(sys.stdout if ok else sys.stderr))
    return ok, 1


_SYNTHETIC_GNU_NEGATIVE = (
    ". /usr/include/c++/16/fstream\n"
    ".. /usr/include/c++/16/bits/fstream.tcc\n"
)


def selftest_oracle_negative_control(scratch, capture):
    """Controle negativo da casa (M-O-neg, secao 10.2): compilador puxa
    `<fstream>`, portao "passou" -> VIOLACAO."""
    del scratch, capture
    ctx = _make_ctx_for_test("GNU")
    nodes = _parse_tree(ctx, _SYNTHETIC_GNU_NEGATIVE, "/tmp/x")
    forbidden = find_forbidden_pulls(ctx, nodes)
    bucket = compare_direction(bool(forbidden), 0, "passou")
    ok = bucket == "violacao"
    label = "selftest: negativo"
    print(f"{label} OK" if ok else f"{label} FALHOU (balde={bucket!r})", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_empty_scan_control(scratch, capture):
    """Varredura vazia (M-O7, secao 10.2): manifesto com ZERO casos no
    grupo "compilar" -> `_final_report` reprova por "comparados=0"."""
    del scratch, capture
    manifest = {"total_casos": 0, "casos": []}
    ok = _final_report(collections.Counter(), [], manifest, (0, 0)) is False
    label = "selftest: varredura vazia"
    print(f"{label} OK" if ok else f"{label} FALHOU", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o1_locale_guard_list_ignored(scratch, capture):
    """O-1: a lista de guardas traduzida no fim do `-H` e' IGNORADA -
    prova que so' os quatro nos de profundidade aparecem, mesmo com o
    texto PT-BR medido no plano logo depois. As duas linhas da lista de
    guardas (o rotulo traduzido + o caminho sem pontos) contam como
    "fora do formato" (L-5d), nunca viram no."""
    del scratch, capture
    nodes, out_of_format_count = parse_gnu_dash_h_tree(_MEASURED_GNU_POSITIVE_CONTROL)
    ok = len(nodes) == 4 and nodes[-1] == (2, "/usr/include/c++/16/cstddef") and out_of_format_count == 2
    label = "selftest: O-1"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (nodes={nodes!r}, fora_do_formato={out_of_format_count})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


# O mutante M-O1 que O-1 mata: aceitar QUALQUER linha, inclusive a lista
# de guardas, como no da arvore.
def _mutant_m_o1_parse_accepts_guard_list(output_text):
    nodes = []
    for line in output_text.splitlines():
        match = _GNU_TREE_LINE_PATTERN.match(line)
        if match:
            nodes.append((len(match.group(1)), match.group(2)))
        else:
            nodes.append((0, line))  # MUTANTE: nunca para
    return nodes


def selftest_oracle_m_o1_mutant_is_caught(scratch, capture):
    """Prova que o mutante M-O1 (aceitar a lista de guardas como no) da'
    um resultado DIFERENTE do parser real - se nao desse, O-1 nao
    estaria matando nada."""
    del scratch, capture
    real_nodes, _out_of_format_count = parse_gnu_dash_h_tree(_MEASURED_GNU_POSITIVE_CONTROL)
    mutant_nodes = _mutant_m_o1_parse_accepts_guard_list(_MEASURED_GNU_POSITIVE_CONTROL)
    ok = real_nodes != mutant_nodes and len(mutant_nodes) > len(real_nodes)
    label = "selftest: M-O1"
    print(f"{label} OK (mutante diverge do real)" if ok else f"{label} FALHOU", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


_SYNTHETIC_O2_O3_O4 = (
    # profundidade 1: cstdint (padrao, apos calibracao) - filho dele
    # (profundidade 2, sys/cdefs.h "proibido" na forma bruta) NUNCA e'
    # examinado (O-3), entao nao pode gerar violacao.
    ". /usr/include/c++/16/cstdint\n"
    ".. /usr/include/sys/cdefs.h\n"
    # profundidade 1: x.hpp, dentro da camada pura da fixture - filho
    # dele (profundidade 2, fstream) E' examinado e reprova (O-4).
    ". x.hpp\n"
    ".. /usr/include/c++/16/fstream\n"
)


def selftest_oracle_o2_o3_o4_depth_and_layer_gating(scratch, capture):
    """O-2/O-3/O-4 juntos (a mesma fixture prova os tres, secao 10.2):
    proibido na profundidade 2 SOB cabecalho padrao nao conta (O-2/O-3);
    proibido na profundidade 2 sob arquivo de CAMADA PURA conta (O-4)."""
    del scratch, capture
    ctx = _make_ctx_for_test("GNU")
    ctx.layer_dirs = ("/proj/src/core",)
    ctx.standard_paths = {"/usr/include/c++/16/cstdint"}
    nodes, _out_of_format_count = parse_gnu_dash_h_tree(_SYNTHETIC_O2_O3_O4)
    with_parent = build_parent_map(nodes)
    normalized = [
        (depth, ("/proj/src/core/x.hpp" if raw == "x.hpp" else "/usr/include/c++/16/" + raw.rsplit("/", 1)[-1]
                  if raw.startswith("/usr") is False else raw), parent)
        for depth, raw, parent in with_parent
    ]
    forbidden = find_forbidden_pulls(ctx, normalized)
    ok = forbidden == ["/usr/include/c++/16/fstream"]
    label = "selftest: O-2/O-3/O-4"
    print(f"{label} OK" if ok else f"{label} FALHOU (forbidden={forbidden!r})", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o5_compares_real_verdict(scratch, capture):
    """O-5: manifesto com `veredito_declarado="reproves_policy"` mas
    `veredito_real="passou"`, compilador puxa `<fstream>` -> VIOLACAO
    (compara com o REAL, nunca o declarado)."""
    del scratch, capture
    bucket_using_real = compare_direction(True, 0, "passou")
    ok = bucket_using_real == "violacao"
    label = "selftest: O-5"
    print(f"{label} OK" if ok else f"{label} FALHOU (balde={bucket_using_real!r})",
          file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o6_judges_by_pulled_not_exit_code(scratch, capture):
    """O-6: codigo de saida != 0 COM `<fstream>` puxado, portao "passou"
    -> VIOLACAO de qualquer jeito - o julgamento e' pelo que foi
    PUXADO, nunca pelo codigo de saida."""
    del scratch, capture
    bucket = compare_direction(True, 1, "passou")
    ok = bucket == "violacao"
    label = "selftest: O-6"
    print(f"{label} OK" if ok else f"{label} FALHOU (balde={bucket!r})", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o7_depth_jump_is_instrument_failure(scratch, capture):
    """O-7: salto de profundidade (1 -> 3) -> falha de instrumento
    (M-O7b: "salto aceito" e' o mutante que isto mata)."""
    del scratch, capture
    bad_output = ". a.hpp\n... b.hpp\n"  # 1 -> 3, sem passar por 2
    raised = False
    try:
        parse_gnu_dash_h_tree(bad_output)
    except _IncludeTreeError:
        raised = True
    label = "selftest: O-7"
    print(f"{label} OK" if raised else f"{label} FALHOU (nao levantou em salto de profundidade)",
          file=(sys.stdout if raised else sys.stderr))
    return raised, 1


def _raises_include_tree_error(fn, *args):
    """Roda `fn(*args)` e devolve True SO' se `_IncludeTreeError` for
    levantada - reduz o boilerplate de try/except repetido nos
    controles de calibracao (O-8, O-21, O-22)."""
    try:
        fn(*args)
    except _IncludeTreeError:
        return True
    return False


def _o21_o22_filler(count=_MIN_STDLIB_PATHS, dirpath="/usr/include/c++/16"):
    """Gera `count` cabecalhos padrao SINTETICOS, cada um filho DIRETO
    da sonda (profundidade 1) no MESMO diretorio - completa o censo
    (piso `_MIN_STDLIB_PATHS`) sem que O-8/O-21 precisem testar o
    filtro de diretorio (essa e' a tarefa exclusiva do O-22)."""
    names = tuple(f"filler{i}" for i in range(count))
    nodes = [(1, f"{dirpath}/{name}", None) for name in names]
    return nodes, names


def selftest_oracle_o8_calibration_instrument_failures(scratch, capture):
    """O-8 (forma nova, L-5e): calibracao SEM a sentinela -> falha;
    censo abaixo de `_MIN_STDLIB_PATHS` -> falha. A medida EXATA do
    censo (filtro de diretorio, homonimos) e' tarefa do O-22; aqui e'
    so' a borda grosseira."""
    del scratch, capture
    ctx = _make_ctx_for_test("GNU")
    expected = _CalibrationExpectedPaths(
        "/proj/src/core/sentinela_projeto.hpp", "/proj/src/core/folha_projeto.hpp",
    )
    missing_sentinel_nodes = [(1, "/usr/include/c++/16/cstdint", None)]
    raised_missing_sentinel = _raises_include_tree_error(
        evaluate_calibration, ctx, missing_sentinel_nodes, expected, ("cstdint",)
    )

    filler_nodes, filler_names = _o21_o22_filler(count=_MIN_STDLIB_PATHS - 1)
    too_few_nodes = [
        (1, expected.sentinel_norm, None),
        (2, expected.leaf_norm, 0),
        (2, "/usr/include/c++/16/cstdint", 0),
    ] + filler_nodes
    # "cstdint" fica DE FORA da lista de permitidos aqui de proposito:
    # o no <cstdint> sob a sentinela so' precisa satisfazer a exigencia
    # dura (basename literal, evaluate_calibration nao consulta a lista
    # pra isso); incluir "cstdint" nela inflaria o censo em +1 e o piso
    # de 59 fillers passaria a 60, escondendo o proprio caso que este
    # controle testa.
    raised_too_few = _raises_include_tree_error(evaluate_calibration, ctx, too_few_nodes, expected, filler_names)

    ok = raised_missing_sentinel and raised_too_few
    label = "selftest: O-8"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (sentinela={raised_missing_sentinel}, piso={raised_too_few})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def _o21_scenarios(filler_nodes, sentinel_norm, leaf_norm):
    """Os quatro cenarios (b)-(e) do O-21 (docs/plano-layers-l5-adendo-
    calibracao.md L-5e), fatorados pra `selftest_oracle_o21_*` caber no
    teto de linhas de L-17. `filler_nodes` (60 cabecalhos SINTETICOS,
    profundidade 1) completa o censo em TODOS os cenarios que chegam
    ate' o piso."""
    cstdint_path = "/usr/include/c++/16/cstdint"
    good = [(1, sentinel_norm, None), (2, leaf_norm, 0), (2, cstdint_path, 0)] + filler_nodes
    no_leaf = [(1, sentinel_norm, None), (2, cstdint_path, 0)] + filler_nodes
    old_valve = [(1, sentinel_norm, None), (2, leaf_norm, 0), (1, cstdint_path, None)] + filler_nodes
    not_first = [
        (1, filler_nodes[0][1], None), (1, sentinel_norm, None), (2, leaf_norm, 1), (2, cstdint_path, 1),
    ] + filler_nodes[1:]
    return (
        ("b_passa", good, True),
        ("c_sem_folha", no_leaf, False),
        ("d_valvula_antiga", old_valve, False),
        ("e_nao_primeiro", not_first, False),
    )


def selftest_oracle_o21_sentinel_first_and_leaf_required(scratch, capture):
    """O-21 (L-5e, C-3 endurecido): a sentinela tem de ser o PRIMEIRO no
    de profundidade 1, com a folha e `<cstdint>` sob ela - a valvula
    antiga ("`<cstdint>` direto") sai. (a) o ARQUIVO de sonda comeca
    pela sentinela; (b)-(e): quatro cenarios enlatados de
    `_o21_scenarios`."""
    del capture
    filler_nodes, filler_names = _o21_o22_filler()
    stdlib_names = filler_names + ("cstdint",)
    sentinel_norm = "/proj/src/core/sentinela_projeto.hpp"
    leaf_norm = "/proj/src/core/folha_projeto.hpp"
    expected = _CalibrationExpectedPaths(sentinel_norm, leaf_norm)
    ctx = _make_ctx_for_test("GNU")

    calib_dir = os.path.join(scratch, "o21a_probe")
    calib_path, _sentinel_disk, _leaf_disk = build_calibration_fixture(calib_dir, ("cstdint",), ("src", "core"))
    with open(calib_path, "r", encoding="utf-8") as handle:
        first_directive_ok = handle.readline().strip() == '#include "sentinela_projeto.hpp"'

    scenarios = _o21_scenarios(filler_nodes, sentinel_norm, leaf_norm)
    results = {
        name: _raises_include_tree_error(evaluate_calibration, ctx, nodes, expected, stdlib_names) == (not expect_pass)
        for name, nodes, expect_pass in scenarios
    }
    ok = first_directive_ok and all(results.values())
    label = "selftest: O-21"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (arquivo={first_directive_ok}, cenarios={results!r})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


_O22_ANCHOR_DIR = "/usr/include/c++/16"
_O22_ANCHOR_NAME = "vector"
_O22_HOMONYM_NAME = "tuple"


def _o22_nodes(depth2_count, include_homonym):
    """docs/plano-layers-l5-adendo-calibracao.md L-5e, O-22: sentinela +
    folha + `<cstdint>` (as tres exigencias duras, satisfeitas a parte)
    MAIS um filho DIRETO ancora (profundidade 1, no diretorio ANCHOR) e
    `depth2_count` nomes permitidos SINTETICOS em profundidade 2,
    filhos da ancora - "so' em profundidade 2 ou maior, dentro de um
    diretorio com filho direto permitido". Um homonimo opcional em
    `tr1/`, cujo diretorio NAO tem filho direto permitido."""
    sentinel_norm = "/proj/src/core/sentinela_projeto.hpp"
    leaf_norm = "/proj/src/core/folha_projeto.hpp"
    anchor_path = f"{_O22_ANCHOR_DIR}/{_O22_ANCHOR_NAME}"
    nodes = [
        (1, sentinel_norm, None),
        (2, leaf_norm, 0),
        (2, f"{_O22_ANCHOR_DIR}/cstdint", 0),
        (1, anchor_path, None),
    ]
    nodes.extend((2, f"{_O22_ANCHOR_DIR}/stdname{i}", 3) for i in range(depth2_count))
    if include_homonym:
        nodes.append((3, f"{_O22_ANCHOR_DIR}/tr1/{_O22_HOMONYM_NAME}", len(nodes) - 1))
    return nodes, sentinel_norm, leaf_norm


def selftest_oracle_o22_census_whole_tree_directory_filtered(scratch, capture):
    """O-22 (L-5e, C-2): o censo conta nomes permitidos em QUALQUER
    profundidade, filtrados pelo DIRETORIO de um filho direto permitido
    - um homonimo em `tr1/` (diretorio SEM filho direto permitido) NAO
    conta; 60 nomes passa (piso exato), 59 reprova."""
    del capture
    ctx = _make_ctx_for_test("GNU")
    stdlib_names = (_O22_ANCHOR_NAME, _O22_HOMONYM_NAME) + tuple(f"stdname{i}" for i in range(59))

    nodes_60, sentinel_norm, leaf_norm = _o22_nodes(59, include_homonym=True)
    expected = _CalibrationExpectedPaths(sentinel_norm, leaf_norm)
    # try/except explicito, NUNCA deixado explodir: um mutante que
    # quebra o filtro de profundidade (M-O22a) faz o censo CAIR abaixo
    # do piso pra este cenario "60 passa" - sem isto, o controle
    # crasharia em vez de reprovar NOMEANDO O-22 (GODS_LAWS.md L-27,
    # mesmo achado do O-20/StopIteration).
    census_60, exact_60 = set(), False
    try:
        result_60 = evaluate_calibration(ctx, nodes_60, expected, stdlib_names)
        census_60 = result_60.census
        exact_60 = len(census_60) == _MIN_STDLIB_PATHS and _O22_HOMONYM_NAME not in census_60
    except _IncludeTreeError:
        exact_60 = False

    nodes_59, _s, _l = _o22_nodes(58, include_homonym=False)
    reproves_59 = _raises_include_tree_error(evaluate_calibration, ctx, nodes_59, expected, stdlib_names)

    ok = exact_60 and reproves_59
    label = "selftest: O-22"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (exact_60={exact_60}, reproves_59={reproves_59}, "
        f"census={sorted(census_60)!r})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def selftest_oracle_o23_std_stubs_written_and_ordered(scratch, capture):
    """O-23 (L-5f, secao 1.3/1.6): `std_stubs/` tem EXATAMENTE os nomes
    do manifesto, todos com zero byte; o comando de CASO tem
    `std_stubs/` como ULTIMO `-I` (depois da fixture e dos gerados); o
    de CALIBRACAO nao tem."""
    del capture
    stub_dir = os.path.join(scratch, "o23_stubs")
    ctx = _make_ctx_for_test("GNU")
    names = ("cstdint", "vector", "flat_map")
    normalized = _write_std_stubs(ctx, stub_dir, names)

    on_disk = set(os.listdir(stub_dir))
    # os.path.getsize SO' pros nomes que de fato existem em disco - um
    # mutante que pula um nome (M-O23c) nao pode fazer isto EXPLODIR em
    # vez de reprovar nomeando O-23 (GODS_LAWS.md L-27, mesmo achado ja'
    # visto em O-20/O-22).
    all_empty = all(os.path.getsize(os.path.join(stub_dir, n)) == 0 for n in on_disk)
    names_match = on_disk == set(names) and len(normalized) == len(names)

    ctx.include_dirs = ("/fixture/include", "/fixture/generated")
    ctx.case_include_dirs = _compose_case_include_dirs(ctx.include_dirs, stub_dir)
    case_command, _wd = build_preprocess_command(ctx, "/fixture/src/core/x.hpp", scratch, ctx.case_include_dirs)
    calib_command, _wd2 = build_preprocess_command(ctx, "/fixture/calib/probe.hpp", scratch, ctx.include_dirs)
    case_flags = [t for t in case_command if t.startswith("-I")]
    calib_flags = [t for t in calib_command if t.startswith("-I")]

    case_ok = case_flags == [f"-I{d}" for d in ctx.include_dirs] + [f"-I{stub_dir}"]
    calib_ok = f"-I{stub_dir}" not in calib_flags and calib_flags == [f"-I{d}" for d in ctx.include_dirs]

    ok = names_match and all_empty and case_ok and calib_ok
    label = "selftest: O-23"
    print(
        f"{label} OK" if ok else
        f"{label} FALHOU (names_match={names_match}, all_empty={all_empty}, case_ok={case_ok}, calib_ok={calib_ok})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def selftest_oracle_o24_empty_leaf_trap(scratch, capture):
    """O-24 (L-5f, secao 1.6): qualquer no cujo PAI seja classificado
    'padrao' (`std_stubs/`) ou 'gerado' (cabecalho gerado vazio) e'
    IMPOSSIVEL por construcao - a trava de folha vazia (funcao IRMA de
    `find_forbidden_pulls`) reprova, nomeando o culpado."""
    del scratch, capture
    ctx = _make_ctx_for_test("GNU")
    ctx.standard_paths = frozenset({"/scratch/std_stubs/cstdint"})
    ctx.stub_generated_paths = frozenset({"/scratch/stubs/glintfx/export.hpp"})

    padrao_child_nodes = [
        (1, "/proj/src/core/x.hpp", None),
        (2, "/scratch/std_stubs/cstdint", 0),
        (3, "/usr/include/whatever.h", 1),
    ]
    raised_padrao = _raises_include_tree_error(_assert_no_children_of_empty_leaves, ctx, padrao_child_nodes)

    gerado_child_nodes = [
        (1, "/proj/src/core/x.hpp", None),
        (2, "/scratch/stubs/glintfx/export.hpp", 0),
        (3, "/usr/include/whatever.h", 1),
    ]
    raised_gerado = _raises_include_tree_error(_assert_no_children_of_empty_leaves, ctx, gerado_child_nodes)

    clean_nodes = [(1, "/proj/src/core/x.hpp", None), (2, "/scratch/std_stubs/cstdint", 0)]
    raised_clean = _raises_include_tree_error(_assert_no_children_of_empty_leaves, ctx, clean_nodes)

    ok = raised_padrao and raised_gerado and not raised_clean
    label = "selftest: O-24"
    print(
        f"{label} OK" if ok else
        f"{label} FALHOU (padrao={raised_padrao}, gerado={raised_gerado}, clean_falso_positivo={raised_clean})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def _fake_executor_by_command_marker(marker, when_marker_output, otherwise_output):
    """Executor FALSO (O-0/L-45: nunca chama compilador de verdade) que
    decide a saida ENLATADA pela PRESENCA de um token no COMANDO (ex.:
    o `-I` de `std_stubs/`) - o suficiente pra diferenciar configuracao
    REAL de configuracao de CASO na mesma bateria, sem sequencia
    implicita de chamadas (O-25)."""

    def executor(command, cwd, timeout):
        del cwd, timeout
        output = when_marker_output if any(marker in token for token in command) else otherwise_output
        return _ExecResult(0, output)

    return executor


def selftest_oracle_o25_shadow_sentinel(scratch, capture):
    """O-25 (L-5f, secao 1.6): (a) `X` = primeiro filho de `<cstdint>`
    REAL fora de `stdlib_permitidos`; (b) configuracao REAL imprime
    sim/nao e NUNCA reprova; (c) configuracao de CASO exige `X` como
    filho direto, senao falha de instrumento."""
    import contextlib
    import io

    del capture
    ctx = _make_ctx_for_test("GNU")
    cstdint_children = ("/usr/include/c++/16/stdint.h", "/usr/include/c++/16/bits/c++config.h")
    stdlib_permitidos = ("cstdint", "stdint.h")
    candidate = _choose_shadow_sentinel_candidate(cstdint_children, stdlib_permitidos)
    candidate_ok = candidate == "/usr/include/c++/16/bits/c++config.h"

    calib_result = _CalibrationResult(set(), set(), [], cstdint_children)
    ctx.include_dirs = ("/real_inc",)
    ctx.case_include_dirs = ("/real_inc", "/marker_case_stub")

    # (b)+(c) juntos: sombra presente na REAL (X sombreado, so' <cstdint>
    # aparece) e X aparece na config de CASO -> sucesso, sem reprovar.
    ctx.executor = _fake_executor_by_command_marker(
        "/marker_case_stub",
        f". /usr/include/c++/16/cstdint\n. {candidate}\n",
        ". /usr/include/c++/16/cstdint\n",
    )
    buffer = io.StringIO()
    ok_run = True
    with contextlib.redirect_stdout(buffer):
        try:
            _run_shadow_sentinel(ctx, scratch, calib_result, stdlib_permitidos)
        except _IncludeTreeError:
            ok_run = False
    message_ok = "sombra presente neste compilador: sim" in buffer.getvalue()

    # (c) invertido: X TAMBEM ausente na config de CASO -> falha de
    # instrumento (a construcao anti-sombra nao fechou).
    ctx.executor = _fake_executor_by_command_marker(
        "/marker_case_stub", ". /usr/include/c++/16/cstdint\n", ". /usr/include/c++/16/cstdint\n",
    )
    reproves_case_absent = _raises_include_tree_error(
        _run_shadow_sentinel, ctx, scratch, calib_result, stdlib_permitidos
    )

    ok = candidate_ok and ok_run and message_ok and reproves_case_absent
    label = "selftest: O-25"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (candidate={candidate!r}, ok_run={ok_run}, "
        f"message_ok={message_ok}, reproves_case_absent={reproves_case_absent})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def _build_o19_fake_raw_output():
    """25 linhas: 24 de arvore (`header_0`..`header_23`) mais UMA linha
    de erro fatal (nao casa o padrao de arvore) - grande o bastante pra
    a janela das 20 PRIMEIRAS (header_0..header_19) e a das 20 ULTIMAS
    (header_5..header_23 + a linha de erro) serem DIFERENTES, provando
    que o conserto de 23/09/2026 (run 35912952114) le as duas pontas,
    nao so' uma."""
    tree_lines = [f". /usr/include/c++/16/header_{i}.hpp" for i in range(24)]
    error_line = "check_layers_oracle_test.cpp:1:2: error: #error mensagem fatal de teste"
    return "\n".join(tree_lines + [error_line]) + "\n"


def selftest_oracle_o19_calibration_diagnostics_on_failure(scratch, capture):
    """O-19 (achado 23/09/2026, runs 35906529355/35912952114):
    `run_calibration()` tem de anexar o diagnostico
    (`_format_calibration_diagnostics`) na excecao de falha de
    instrumento - as SEIS pecas exigidas (profundidade 1 normalizada,
    `sentinel_norm` esperado, returncode, 20 primeiras linhas cruas, 20
    ultimas linhas cruas, 20 ultimas linhas NAO-arvore), nunca so' o
    nome do defeito. Usa um executor FALSO com `returncode=1` (nunca o
    real - O-0/L-45) que devolve uma saida -H sem a sentinela e maior
    que 20 linhas, pra reproduzir de verdade o caminho de
    `run_calibration` (nao so' `evaluate_calibration` isolada, que e' o
    que O-8 ja cobre) E provar que as janelas de primeiras/ultimas
    linhas sao DIFERENTES."""
    del capture
    fake_raw = _build_o19_fake_raw_output()
    ctx = _make_ctx_for_test("GNU")
    ctx.executor = _fake_executor(fake_raw, returncode=1)
    manifest = {
        "camadas_puras": [{"rotulo": "src/core", "partes": ["src", "core"]}],
        "stdlib_permitidos": ("cstdint",),
    }
    message = ""
    raised = False
    try:
        run_calibration(ctx, scratch, manifest)
    except _IncludeTreeError as exc:
        raised = True
        message = str(exc)
    checks = (
        raised,
        "falha de instrumento" in message,
        "profundidade 1 normalizados" in message,
        "sentinel_norm esperado" in message,
        "returncode da calibracao: 1" in message,
        "primeiras linhas CRUAS" in message and "header_0.hpp" in message,
        "ultimas linhas CRUAS" in message and "header_23.hpp" in message,
        "header_0.hpp" not in message.rsplit("ultimas linhas CRUAS", 1)[-1],  # janelas DIFERENTES
        # a mensagem fatal tem de estar DENTRO do bloco "NAO-arvore" especificamente -
        # ela TAMBEM aparece nas "ultimas linhas CRUAS" (e' a ultima linha crua de
        # verdade), entao checar "em algum lugar da mensagem" nao provaria nada. E o
        # PROPRIO ROTULO "NAO-arvore" tem de existir - um mutante que apague o bloco
        # inteiro (rotulo junto) faz `rsplit` devolver a mensagem INTEIRA sem separar
        # nada, e a checagem sozinha (sem esta primeira metade) sobrevivia.
        "ultimas linhas NAO-arvore" in message
        and "mensagem fatal de teste" in message.rsplit("ultimas linhas NAO-arvore", 1)[-1],
    )
    ok = all(checks)
    label = "selftest: O-19"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (checks={checks!r}, message={message!r})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


# --- fonte 3: SINTETICA - leitor nao para em linha estranha (L-5d/C-1) --
#
# docs/plano-layers-l5-adendo-calibracao.md secao 4 (L-5d): saida
# enlatada com um NO, um `#warning` de backward_warning.h NO MEIO do
# fluxo (nao casa o formato -H), MAIS NOS depois dele (inclusive a
# sentinela), uma linha de aviso citando um arquivo do PROPRIO projeto,
# e o bloco final de guardas com um caminho comecando por `../` (dois
# pontos seguidos de `/`, NUNCA de espaco - nao casa `^\.+ `).
_SYNTHETIC_O20_INTERLEAVED_WARNINGS = (
    ". root_a.hpp\n"
    ".. root_b.hpp\n"
    "/usr/include/c++/16/backward/backward_warning.h:32:2: warning: #warning "
    "This file includes at least one deprecated or antiquated header.\n"
    ". sentinela_projeto.hpp\n"
    ".. cstdint\n"
    "./src/core/x.cpp:1:2: warning: mensagem de teste\n"
    "Multiplos include guards podem ser uteis para:\n"
    "../header_qualquer.hpp\n"
)


def selftest_oracle_o20_reader_does_not_stop_on_odd_line(scratch, capture):
    """O-20 (L-5d, C-1): o leitor do `-H` NAO PARA na primeira linha
    fora do formato - le os nos que vem DEPOIS de um `#warning`, com o
    pai certo, e conta (sem virar no) as tres linhas estranhas MAIS o
    caminho de guarda `../...`. Mata M-O20a (recoloca o `break`) e
    M-O20b (zera a contagem)."""
    del scratch, capture
    nodes, out_of_format_count = parse_gnu_dash_h_tree(_SYNTHETIC_O20_INTERLEAVED_WARNINGS)
    with_parent = build_parent_map(nodes)
    depths_paths = [(depth, path) for depth, path, _parent in with_parent]
    # next(..., None): um mutante que PARA cedo (M-O20a) nao acha a
    # sentinela nem o cstdint - isto tem de reprovar LIMPO (L-27: "o
    # autoteste tem de reprovar E NOMEAR o controle esperado"), nunca
    # explodir com StopIteration antes de chegar no print do label.
    sentinel_idx = next((i for i, (_d, p, _pi) in enumerate(with_parent) if p == "sentinela_projeto.hpp"), None)
    cstdint_entry = next((entry for entry in with_parent if entry[1] == "cstdint"), None)
    ok = (
        depths_paths == [(1, "root_a.hpp"), (2, "root_b.hpp"), (1, "sentinela_projeto.hpp"), (2, "cstdint")]
        and sentinel_idx is not None
        and with_parent[sentinel_idx][2] is None
        and cstdint_entry is not None
        and cstdint_entry[2] == sentinel_idx
        and out_of_format_count == 4
    )
    label = "selftest: O-20"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (nodes={nodes!r}, fora_do_formato={out_of_format_count})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


# --- fonte 2: DOCUMENTADO pela Microsoft (formato /showIncludes) --------
#
# "Note: including file: d:\MyDir\include\stdio.h" e "one space for each
# level of nesting" (learn.microsoft.com/cpp/build/reference/
# showincludes-list-include-files, citado no plano de L-5 §1.1).
_DOCUMENTED_MSVC_ENGLISH = (
    "Note: including file: d:\\proj\\src\\core\\sentinela_projeto.hpp\n"
    "Note: including file:  d:\\msvc\\include\\cstdint\n"
)

# --- fonte 3: SINTETICA - prefixo traduzido (frances, exercitando O-9) --
_SYNTHETIC_MSVC_FRENCH = (
    "Remarque : inclusion du fichier : d:\\proj\\src\\core\\sentinela_projeto.hpp\n"
    "Remarque : inclusion du fichier :  d:\\msvc\\include\\cstdint\n"
)


def selftest_oracle_o9_msvc_prefix_learned_any_locale(scratch, capture):
    """O-9: prefixo MSVC traduzido (sintetico, frances) -> lido PELO
    PREFIXO APRENDIDO, nunca por um texto ingles fixo (M-O9)."""
    del scratch, capture
    prefix_en = learn_msvc_prefix(_DOCUMENTED_MSVC_ENGLISH, "sentinela_projeto.hpp")
    prefix_fr = learn_msvc_prefix(_SYNTHETIC_MSVC_FRENCH, "sentinela_projeto.hpp")
    ok = prefix_en != prefix_fr and prefix_en.startswith("Note") and prefix_fr.startswith("Remarque")
    label = "selftest: O-9"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (en={prefix_en!r}, fr={prefix_fr!r})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def selftest_oracle_o10_msvc_nesting_one_space_per_level(scratch, capture):
    """O-10: MSVC aninhado, UM espaco por nivel -> profundidade certa
    (doc da Microsoft citada acima)."""
    del scratch, capture
    text = (
        "Note: including file: d:\\proj\\src\\core\\x.hpp\n"
        "Note: including file:  d:\\msvc\\include\\fstream\n"
        "Note: including file:   d:\\msvc\\include\\bits\\fstream.tcc\n"
    )
    prefix = "Note: including file:"
    nodes = parse_msvc_showincludes_tree(text, prefix)
    ok = [depth for depth, _path in nodes] == [1, 2, 3]
    label = "selftest: O-10"
    print(f"{label} OK" if ok else f"{label} FALHOU (depths={[d for d, _ in nodes]!r})",
          file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o11_msvc_case_insensitive_path(scratch, capture):
    """O-11: MSVC, caixa DIFERENTE entre calibracao e fixture -> MESMO
    caminho - dobrado pelo ARGUMENTO de dialeto (_normcase_for_dialect),
    nunca por `os.path.normcase()` cru (que so' dobra em host Windows -
    GODS_LAWS.md L-04, o autoteste prova isto em QUALQUER host)."""
    del scratch, capture
    upper = normalize_compiler_path("MSVC", "D:\\Proj\\Src\\Core\\X.HPP", "/tmp")
    lower = normalize_compiler_path("MSVC", "d:\\proj\\src\\core\\x.hpp", "/tmp")
    gnu_upper = normalize_compiler_path("GNU", "/tmp/X.HPP", "/tmp")
    gnu_lower = normalize_compiler_path("GNU", "/tmp/x.hpp", "/tmp")
    ok = upper == lower and gnu_upper != gnu_lower
    label = "selftest: O-11"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (msvc upper={upper!r} lower={lower!r})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def _run_o18_scenario_with_simulated_windows_host():
    """Troca `os.path`/`os.sep` que o MODULO enxerga por `ntpath`/`"\\\\"`
    - simula "processo rodando em Windows" SEM host Windows real, sem
    tocar disco, sem compilador (GODS_LAWS.md L-45) - e roda a MESMA
    fixture GNU que quebrou no CI (`x.hpp` sob `/tmp/x`, run
    35906529355). Restaura os dois no `finally`, pra nao vazar estado
    pro resto da bateria; devolve so' a lista `forbidden`, que O-18
    interpreta."""
    module = sys.modules[__name__]
    original_os_path = module.os.path
    original_os_sep = module.os.sep
    module.os.path = ntpath
    module.os.sep = "\\"
    try:
        ctx = _make_ctx_for_test("GNU")
        ctx.layer_dirs = ("/tmp/x",)
        ctx.standard_paths = {"/usr/include/c++/16/cstdint", "/usr/include/c++/16/cstddef"}
        nodes = _parse_tree(ctx, _MEASURED_GNU_POSITIVE_CONTROL, "/tmp/x")
        return find_forbidden_pulls(ctx, nodes)
    finally:
        module.os.path = original_os_path
        module.os.sep = original_os_sep


def selftest_oracle_o18_dialect_not_host_path_syntax(scratch, capture):
    """O-18 (achado real, run 35906529355, job Windows, 23/09/2026):
    `layers_oracle_selftest` reprovava no trabalho Windows em DUAS
    metades - `normalize_compiler_path` (caminho POSIX enlatado virando
    caminho de unidade) e `classify_include_path` (comparacao de
    subpasta via `os.sep`, que num host Windows nunca bate contra um
    `layer_dir` normalizado em POSIX). As duas vinham da MESMA doenca:
    sintaxe de caminho decidida pelo HOST, nunca pelo DIALETO da
    fixture (GNU) - `_run_o18_scenario_with_simulated_windows_host()`
    reproduz as duas de uma vez (`M-O18b`, so' a metade de
    `classify_include_path`, sobrevivia a uma versao anterior que so'
    trocava `os.path`)."""
    del scratch, capture
    forbidden = _run_o18_scenario_with_simulated_windows_host()
    ok = not forbidden
    label = "selftest: O-18"
    print(
        f"{label} OK (dialeto GNU, os.path/os.sep simulados=Windows, sem forbidden)" if ok
        else f"{label} FALHOU (forbidden={forbidden!r} com os.path/os.sep simulados=Windows - "
             "regressao do achado do run 35906529355)",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def _run_skip_if_not_enabled(oracle_option_flag, github_actions_value):
    """Chama `skip_if_not_enabled()` com o ambiente simulado, capturando
    saida e o codigo de saida real (`SystemExit`, nao o `capture()`
    generico - que so redireciona stdout/stderr e NAO pega
    `sys.exit()`, memoria "codigo de saida lido de variavel")."""
    import contextlib
    import io

    saved = os.environ.pop("GITHUB_ACTIONS", None)
    if github_actions_value is not None:
        os.environ["GITHUB_ACTIONS"] = github_actions_value
    buffer = io.StringIO()
    exited_with = None
    try:
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            try:
                skip_if_not_enabled(oracle_option_flag)
            except SystemExit as exc:
                exited_with = exc.code
    finally:
        os.environ.pop("GITHUB_ACTIONS", None)
        if saved is not None:
            os.environ["GITHUB_ACTIONS"] = saved
    return exited_with, buffer.getvalue()


def _check_skip_combinations(combinations):
    """Roda `_run_skip_if_not_enabled()` pra cada `(GITHUB_ACTIONS,
    oracle_option_flag, esperado)` da lista e devolve True so' se TODAS
    baterem - fatorado pra manter os controles O-12/O-12b sob o teto de
    linhas de L-17 (achado da revisao independente, 23/09/2026)."""
    ok = True
    for github_actions_value, oracle_option_flag, expect_skip in combinations:
        exited_with, text = _run_skip_if_not_enabled(oracle_option_flag, github_actions_value)
        skipped = exited_with == _SKIP_RETURN_CODE
        if skipped != expect_skip:
            print(
                f"selftest: FALHOU (GITHUB_ACTIONS={github_actions_value!r}, "
                f"opcao={oracle_option_flag!r}: esperava pular={expect_skip}, "
                f"exit={exited_with!r}, saida={text!r})",
                file=sys.stderr,
            )
            ok = False
        elif expect_skip and "GODS_LAWS.md L-45" not in text:
            print(f"selftest: FALHOU (pulou mas nao citou L-45): {text!r}", file=sys.stderr)
            ok = False
    return ok


def _prove_m_o12_diverges():
    """M-O12 (mutante: a guarda vira "so' precisa de UMA das duas
    condicoes" em vez de EXIGIR as duas) - reproduz sem reimplementar
    `skip_if_not_enabled()`, so' troca o AND por OR na MESMA expressao
    que `_oracle_should_run()` calcula, e confere que diverge do real
    (achado da revisao independente, 23/09/2026: extraida de
    `selftest_oracle_o12_skips_outside_ci` pra manter as duas sob o
    teto de 40 linhas de L-17)."""
    mutant_should_run = (os.environ.get("GITHUB_ACTIONS") == "true") or ("ON" == "ON")
    real_should_run = _oracle_should_run("OFF")  # fora do CI (sem GITHUB_ACTIONS aqui) e opcao OFF
    if mutant_should_run == real_should_run:
        print(
            "selftest: FALHOU (o mutante OR nao diverge do AND real - a prova de "
            "M-O12 nao mataria nada)",
            file=sys.stderr,
        )
        return False
    return True


def selftest_oracle_o12_skips_outside_ci(scratch, capture):
    """O-12, reformado por DECISOES_AUTONOMAS.md 23/09/2026: PULA
    (`_SKIP_RETURN_CODE`, nunca `fail()`) a menos que as DUAS coisas
    sejam verdade - `GITHUB_ACTIONS=="true"` E a opcao de CMake ligada
    (`oracle_option_flag=="ON"`). As QUATRO combinacoes, inclusive a
    nova (dentro do CI real mas com a opcao desligada - as pernas
    estaticas e os jobs fora dos seis escolhidos): so' a ultima RODA."""
    del scratch, capture
    combinations = (
        (None, "OFF", True),   # fora do CI, opcao off -> pula
        (None, "ON", True),    # fora do CI, opcao on -> pula (L-45: nem ligando a mao)
        ("true", "OFF", True),  # dentro do CI, opcao off (perna estatica etc) -> pula
        ("true", "ON", False),  # dentro do CI, opcao on -> RODA (nao pula)
    )
    ok = _check_skip_combinations(combinations) and _prove_m_o12_diverges()
    label = "selftest: O-12"
    print(f"{label} OK (quatro combinacoes + mutante M-O12 divergindo)" if ok else f"{label} FALHOU (ver acima)",
          file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o12b_env_value_must_be_exact_true(scratch, capture):
    """O-12b (achado 3 da revisao independente, 23/09/2026): a guarda
    exige IGUALDADE ESTRITA com "true" - qualquer OUTRO valor truthy
    de `GITHUB_ACTIONS` ("1", "True", "TRUE") tem de PULAR igual a
    None/ausente. Sem este controle, o mutante M-ENV-ANY (trocar `==
    "true"` por `bool(os.environ.get("GITHUB_ACTIONS"))`) sobrevivia -
    os 21 controles antigos passavam, rc=0, mesmo com a guarda
    genérica demais (medido pelo revisor em cópia fora da árvore)."""
    del scratch, capture
    combinations = (
        ("1", "ON", True),
        ("True", "ON", True),
        ("TRUE", "ON", True),
        ("yes", "ON", True),
    )
    ok = _check_skip_combinations(combinations)
    label = "selftest: O-12b"
    print(f"{label} OK (valores truthy-mas-nao-'true' continuam pulando)" if ok else f"{label} FALHOU (ver acima)",
          file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o13_preci_sh_guard(scratch, capture):
    """O-13: `preci.sh` SINTETICO contendo `GLINTFX_LAYERS_ORACLE` ->
    reprova; o `tools/preci.sh` REAL deste repositorio -> passa, com o
    caminho impresso (docs/plano-layers-l5.md §6.2 item 3, R-8)."""
    del scratch, capture
    dirty = check_preci_sh_excludes_oracle("cmake -DGLINTFX_LAYERS_ORACLE=ON ...")
    real_path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), "tools", "preci.sh")
    real_ok = True
    if os.path.isfile(real_path):
        with open(real_path, "r", encoding="utf-8", errors="replace") as handle:
            real_text = handle.read()
        real_ok = check_preci_sh_excludes_oracle(real_text)
        print(f"selftest: O-13 tools/preci.sh real conferido em {real_path}")
    else:
        print(f"selftest: O-13 tools/preci.sh nao encontrado em {real_path} (fora da arvore real - OK no lab)")
    ok = (dirty is False) and real_ok
    label = "selftest: O-13"
    print(f"{label} OK" if ok else f"{label} FALHOU (dirty={dirty}, real_ok={real_ok})",
          file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o14_super_approximation_never_fails(scratch, capture):
    """O-14: portao reprovou, compilador so' puxou permitido -> balde
    super-aproximacao, SEMPRE verde (M-O14: "direcao 2 vira falha" e' o
    mutante que isto mata - nunca aplicado aqui)."""
    del scratch, capture
    bucket = compare_direction(False, 0, "reprovou")
    # Piso de puxou_proibido (>= 20) satisfeito a parte, pra isolar SO'
    # o que O-14 prova: a super-aproximacao sozinha nunca reprova.
    manifest = {"total_casos": 21, "casos": []}
    buckets = collections.Counter({bucket: 1, "concorda-reprova": 20})
    ok = bucket == "super-aproximacao" and _final_report(buckets, [], manifest, (0, 0)) is True
    label = "selftest: O-14"
    print(f"{label} OK" if ok else f"{label} FALHOU (balde={bucket!r})", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o15_named_counts_add_up(scratch, capture):
    """O-15: caso nao-fonte e casos de calibracao aparecem na contagem,
    NOMEADOS; a soma bate com exportados."""
    del scratch, capture
    manifest = {
        "total_casos": 3,
        "casos": [
            {"modo_oraculo": "calibracao", "alvos": [{"tipo": "fonte"}]},
            {"modo_oraculo": "compilar", "alvos": [{"tipo": "nao-fonte-conhecido"}]},
            {"modo_oraculo": "compilar", "alvos": [{"tipo": "fonte"}]},
        ],
    }
    calibracao, fora_de_escopo, compilar = summarize_manifest_cases(manifest)
    ok = len(calibracao) == 1 and len(fora_de_escopo) == 1 and len(compilar) == 1
    label = "selftest: O-15"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (cal={len(calibracao)} fora={len(fora_de_escopo)} "
        f"compilar={len(compilar)})",
        file=(sys.stdout if ok else sys.stderr),
    )
    return ok, 1


def selftest_oracle_o16_flags_family_closed(scratch, capture):
    """O-16: `compile_commands` sem padrao da linguagem -> reprova; com
    `/Zc:preprocessor` -> repassado e impresso (familia fechada, secao
    3.3)."""
    del scratch, capture
    raised_missing_std = False
    try:
        extract_language_family_flags(["/nologo", "/W4"], "MSVC")
    except _IncludeTreeError:
        raised_missing_std = True

    kept = extract_language_family_flags(["/std:c++23", "/Zc:preprocessor", "/utf-8", "/O2"], "MSVC")
    ok = raised_missing_std and set(kept) == {"/std:c++23", "/Zc:preprocessor", "/utf-8"}
    label = "selftest: O-16"
    print(f"{label} OK" if ok else f"{label} FALHOU (kept={kept!r})", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_o17_unknown_dialect_named(scratch, capture):
    """O-17: dialeto desconhecido (ex.: "Intel") -> reprova, NOMEADO -
    nunca cai no GNU por omissao (M-O17)."""
    del scratch, capture
    raised = False
    message = ""
    try:
        validate_dialect("Intel")
    except _IncludeTreeError as exc:
        raised = True
        message = str(exc)
    ok = raised and "Intel" in message
    label = "selftest: O-17"
    print(f"{label} OK" if ok else f"{label} FALHOU (message={message!r})", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


def selftest_oracle_build_parent_map(scratch, capture):
    """Controle auxiliar da casa: a reconstrucao de pai/filho por
    profundidade (secao 3.2) - base de todos os O-2..O-4."""
    del scratch, capture
    nodes = [(1, "a"), (2, "b"), (1, "c"), (2, "d"), (3, "e")]
    with_parent = build_parent_map(nodes)
    parents = [p for _d, _n, p in with_parent]
    # a(0,None) b(1,0) c(2,None) d(3,2) e(4,3)
    ok = parents == [None, 0, None, 2, 3]
    label = "selftest: build_parent_map"
    print(f"{label} OK" if ok else f"{label} FALHOU (parents={parents!r})", file=(sys.stdout if ok else sys.stderr))
    return ok, 1


_SELFTEST_ORACLE_GROUPS = (
    (selftest_oracle_o0_real_executor_guard,),
    (selftest_oracle_positive_control,),
    (selftest_oracle_negative_control,),
    (selftest_oracle_empty_scan_control,),
    (selftest_oracle_o1_locale_guard_list_ignored,),
    (selftest_oracle_m_o1_mutant_is_caught,),
    (selftest_oracle_o2_o3_o4_depth_and_layer_gating,),
    (selftest_oracle_o5_compares_real_verdict,),
    (selftest_oracle_o6_judges_by_pulled_not_exit_code,),
    (selftest_oracle_o7_depth_jump_is_instrument_failure,),
    (selftest_oracle_o8_calibration_instrument_failures,),
    (selftest_oracle_o21_sentinel_first_and_leaf_required,),
    (selftest_oracle_o22_census_whole_tree_directory_filtered,),
    (selftest_oracle_o23_std_stubs_written_and_ordered,),
    (selftest_oracle_o24_empty_leaf_trap,),
    (selftest_oracle_o25_shadow_sentinel,),
    (selftest_oracle_o19_calibration_diagnostics_on_failure,),
    (selftest_oracle_o20_reader_does_not_stop_on_odd_line,),
    (selftest_oracle_o9_msvc_prefix_learned_any_locale,),
    (selftest_oracle_o10_msvc_nesting_one_space_per_level,),
    (selftest_oracle_o11_msvc_case_insensitive_path,),
    (selftest_oracle_o18_dialect_not_host_path_syntax,),
    (selftest_oracle_o12_skips_outside_ci,),
    (selftest_oracle_o12b_env_value_must_be_exact_true,),
    (selftest_oracle_o13_preci_sh_guard,),
    (selftest_oracle_o14_super_approximation_never_fails,),
    (selftest_oracle_o15_named_counts_add_up,),
    (selftest_oracle_o16_flags_family_closed,),
    (selftest_oracle_o17_unknown_dialect_named,),
    (selftest_oracle_build_parent_map,),
)


def _make_capture():
    import contextlib
    import io

    class _Captured:
        __slots__ = ("result", "text")

        def __init__(self, result, text):
            self.result = result
            self.text = text

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


def selftest_main():
    """GODS_LAWS.md L-45: marca `_SELFTEST_MODE_ACTIVE` durante a rodada
    inteira, em cima da guarda do proprio executor real (defesa em
    profundidade, alem do controle O-0 que testa a guarda direto)."""
    global _SELFTEST_MODE_ACTIVE
    scratch = tempfile.mkdtemp(prefix="glintfx-layers-oracle-selftest-", dir=os.environ.get("TMPDIR"))
    capture = _make_capture()
    _SELFTEST_MODE_ACTIVE = True
    try:
        outcomes = [group[0](scratch, capture, *group[1:]) for group in _SELFTEST_ORACLE_GROUPS]
        results = [ok for ok, _count in outcomes]
        total_cases = sum(count for _ok, count in outcomes)
        if not all(results):
            print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"{SCRIPT_NAME} --selftest: {len(results)} controles OK ({total_cases} casos no total)")
    finally:
        _SELFTEST_MODE_ACTIVE = False
        shutil.rmtree(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        oracle_main(args)


if __name__ == "__main__":
    main()
