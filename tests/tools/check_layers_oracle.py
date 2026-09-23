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
import os
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
        "stub_generated_paths", "standard_paths",
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


def build_preprocess_command(ctx, file_path, out_scratch_dir):
    is_c = file_path.lower().endswith(".c")
    include_flags_gnu = [f"-I{d}" for d in ctx.include_dirs]
    if ctx.dialect == "GNU":
        lang = "c" if is_c else "c++"
        command = [
            ctx.compiler, *ctx.flags, *include_flags_gnu,
            "-x", lang, "-E", "-H", "-o", os.devnull, file_path,
        ]
        return command, (os.path.dirname(file_path) or ".")
    lang_flag = "/TC" if is_c else "/TP"
    include_flags_msvc = [f"/I{d}" for d in ctx.include_dirs]
    out_file = os.path.join(out_scratch_dir, "out.i")
    command = [
        ctx.compiler, "/nologo", *ctx.flags, *include_flags_msvc,
        lang_flag, "/P", f"/Fi{out_file}", "/showIncludes", file_path,
    ]
    return command, (os.path.dirname(file_path) or ".")


# --- leitura da saida: arvore, nao lista (secao 3.2) --------------------


def parse_gnu_dash_h_tree(output_text):
    """`-H` do GCC/Clang: cada linha `<pontos><espaco><caminho>` vira um
    no; a PRIMEIRA linha que nao casa ENCERRA a leitura (e' o comeco da
    lista de guardas de inclusao - traduzida pela localidade, entao
    NUNCA reconhecida pelo conteudo, so' pela forma que para de casar -
    O-1). Salto de profundidade (>1 de uma vez) e' falha de instrumento
    (O-7)."""
    nodes = []
    previous_depth = 0
    for line in output_text.splitlines():
        match = _GNU_TREE_LINE_PATTERN.match(line)
        if not match:
            break
        depth = len(match.group(1))
        if depth > previous_depth + 1:
            raise _IncludeTreeError(
                f"salto de profundidade no formato -H: de {previous_depth} para {depth} em {match.group(2)!r}"
            )
        nodes.append((depth, match.group(2)))
        previous_depth = depth
    return nodes


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


def normalize_compiler_path(dialect, raw_path, workdir):
    """Resolve o caminho como o compilador o ABRIU: absoluto pro de
    sistema, relativo ao DIRETORIO DE TRABALHO pro de aspas (docs/
    plano-layers-l5.md §1.1/§3.1, medicao do g++)."""
    candidate = raw_path if os.path.isabs(raw_path) else os.path.join(workdir, raw_path)
    return _normcase_for_dialect(dialect, os.path.realpath(candidate))


def _parse_tree(ctx, raw_output, workdir):
    """Parse + normalizacao num passo so' - tudo a jusante trabalha com
    caminho JA normalizado, sem precisar carregar dialeto/workdir por
    toda parte (GODS_LAWS.md L-17)."""
    if ctx.dialect == "GNU":
        nodes = parse_gnu_dash_h_tree(raw_output)
    else:
        nodes = parse_msvc_showincludes_tree(raw_output, ctx.msvc_prefix)
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
    outra coisa -> "proibido"."""
    for layer_dir in ctx.layer_dirs:
        if normalized_path == layer_dir or normalized_path.startswith(layer_dir + os.sep):
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


def build_calibration_fixture(scratch_dir, stdlib_permitidos, layer_dir_parts):
    """Um arquivo, numa pasta pura de uma raiz de CALIBRACAO (propria,
    nunca a raiz de um caso real), tentando incluir CADA nome permitido
    via `__has_include` (ausencia de um cabecalho num compilador velho
    nao mata a calibracao inteira), mais `sentinela_projeto.hpp`
    (arquivo IRMAO que por sua vez inclui `<cstdint>`)."""
    layer_dir = os.path.join(scratch_dir, *layer_dir_parts)
    os.makedirs(layer_dir, exist_ok=True)
    sentinel_path = os.path.join(layer_dir, "sentinela_projeto.hpp")
    with open(sentinel_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("#include <cstdint>\n")
    lines = []
    for name in stdlib_permitidos:
        lines.append(f"#if __has_include(<{name}>)")
        lines.append(f"#include <{name}>")
        lines.append("#endif")
    lines.append('#include "sentinela_projeto.hpp"')
    calibration_path = os.path.join(layer_dir, "calibration_probe.hpp")
    with open(calibration_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    return calibration_path, sentinel_path


def evaluate_calibration(ctx, normalized_nodes, sentinel_norm, stdlib_permitidos):
    """Reprova (falha de instrumento, docs/plano-layers-l5.md §7.1),
    NUNCA pulo, se: sentinela_projeto.hpp nao aparece como filho
    direto; `<cstdint>` nao aparece nem direto nem sob a sentinela; ou
    o conjunto de caminhos padrao achado tem menos que
    `_MIN_STDLIB_PATHS` nomes (instrumento cego, O-8)."""
    del ctx  # mantido na assinatura por simetria com as demais funcoes de baixo nivel
    stdlib_names = set(stdlib_permitidos)
    depth1 = [(idx, path) for idx, (depth, path, _parent) in enumerate(normalized_nodes) if depth == 1]
    sentinel_matches = [idx for idx, path in depth1 if path == sentinel_norm]
    if not sentinel_matches:
        raise _IncludeTreeError("calibracao: sentinela_projeto.hpp nao apareceu como filho direto (falha de instrumento)")
    sentinel_index = sentinel_matches[0]

    direct_cstdint = any(os.path.basename(path) == "cstdint" for _idx, path in depth1)
    under_sentinel_cstdint = any(
        parent == sentinel_index and os.path.basename(path) == "cstdint"
        for _depth, path, parent in normalized_nodes
    )
    if not (direct_cstdint or under_sentinel_cstdint):
        raise _IncludeTreeError(
            "calibracao: <cstdint> nao apareceu nem direto nem sob sentinela_projeto.hpp (falha de instrumento)"
        )

    standard_paths = {path for _idx, path in depth1 if os.path.basename(path) in stdlib_names}
    if len(standard_paths) < _MIN_STDLIB_PATHS:
        raise _IncludeTreeError(
            f"calibracao: so' {len(standard_paths)} caminhos padrao encontrados, piso e {_MIN_STDLIB_PATHS} "
            "(instrumento cego)"
        )
    return standard_paths


def run_calibration(ctx, scratch, manifest):
    """UM processo, antes de qualquer fixture de caso (docs/plano-
    layers-l5.md §6.1/§7.1)."""
    calib_dir = os.path.join(scratch, "calibration")
    os.makedirs(calib_dir, exist_ok=True)
    layer_parts = manifest["camadas_puras"][0]["partes"]
    calib_path, sentinel_path = build_calibration_fixture(calib_dir, manifest["stdlib_permitidos"], layer_parts)
    raw_output, _returncode, workdir = _run_one_alvo(ctx, calib_dir, calib_path)
    if ctx.dialect == "MSVC":
        ctx.msvc_prefix = learn_msvc_prefix(raw_output, os.path.basename(sentinel_path))
    normalized_nodes = _parse_tree(ctx, raw_output, workdir)
    sentinel_norm = normalize_compiler_path(ctx.dialect, sentinel_path, workdir)
    return evaluate_calibration(ctx, normalized_nodes, sentinel_norm, manifest["stdlib_permitidos"])


# --- sentinelas por fixture (secao 7.2) ----------------------------------


def _judge_sentinel(ctx, scratch_dir, content, expect_forbidden):
    layer_parts = ctx.camadas_puras[0]["partes"]
    layer_dir = os.path.join(scratch_dir, *layer_parts)
    os.makedirs(layer_dir, exist_ok=True)
    path = os.path.join(layer_dir, "sentinel_probe.hpp")
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)
    raw, returncode, workdir = _run_one_alvo(ctx, scratch_dir, path)
    forbidden = find_forbidden_pulls(ctx, _parse_tree(ctx, raw, workdir))
    if expect_forbidden and not forbidden:
        raise _IncludeTreeError("sentinela negativa: <fstream> nao apareceu como puxado (falha de instrumento)")
    if not expect_forbidden and (forbidden or returncode != 0):
        raise _IncludeTreeError("sentinela positiva: <cstdint> nao saiu limpa (falha de instrumento)")


def run_sentinels(ctx, scratch):
    """docs/plano-layers-l5.md §7.2: `<fstream>` tem de sair "puxou
    proibido"; `<cstdint>` tem de sair "so' permitidos, codigo 0" -
    qualquer outro resultado e' falha de instrumento, medida a cada
    rodada."""
    ctx.layer_dirs = _layer_dirs_for_root(ctx, os.path.join(scratch, "sentinels"))
    _judge_sentinel(ctx, os.path.join(scratch, "sentinels_neg"), "#include <fstream>\n", expect_forbidden=True)
    _judge_sentinel(ctx, os.path.join(scratch, "sentinels_pos"), "#include <cstdint>\n", expect_forbidden=False)


# --- execucao sequencial, um caso por vez (secao 3, 9) -------------------


def _run_one_alvo(ctx, scratch_dir, alvo_abs_path):
    """UMA invocacao SEQUENCIAL de compilador (docs/plano-layers-l5.md
    §3.1: "nada de pool de processos" - a L-45 abre excecao a L-11 so'
    no servidor e SEM paralelismo). Devolve o texto BRUTO - quem chama
    decide como interpretar (a calibracao MSVC precisa do texto cru pra
    aprender o prefixo ANTES de qualquer parse)."""
    command, workdir = build_preprocess_command(ctx, alvo_abs_path, scratch_dir)
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
        raw_output, returncode, workdir = _run_one_alvo(ctx, scratch_dir, alvo_abs)
        normalized_nodes = _parse_tree(ctx, raw_output, workdir)
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
    ctx.standard_paths = run_calibration(ctx, scratch, manifest)
    run_sentinels(ctx, scratch)

    calibracao_cases, fora_de_escopo_cases, compilar_cases = summarize_manifest_cases(manifest)
    buckets = collections.Counter()
    violations = []
    for case in compilar_cases:
        root_abs = os.path.join(export_dir, case["raiz"])
        bucket = _judge_one_case(ctx, scratch, case, root_abs)
        buckets[bucket] += 1
        if bucket == "violacao":
            violations.append(case)

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
    texto PT-BR medido no plano logo depois."""
    del scratch, capture
    nodes = parse_gnu_dash_h_tree(_MEASURED_GNU_POSITIVE_CONTROL)
    ok = len(nodes) == 4 and nodes[-1] == (2, "/usr/include/c++/16/cstddef")
    label = "selftest: O-1"
    print(f"{label} OK" if ok else f"{label} FALHOU (nodes={nodes!r})", file=(sys.stdout if ok else sys.stderr))
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
    real_nodes = parse_gnu_dash_h_tree(_MEASURED_GNU_POSITIVE_CONTROL)
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
    nodes = parse_gnu_dash_h_tree(_SYNTHETIC_O2_O3_O4)
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


def selftest_oracle_o8_calibration_instrument_failures(scratch, capture):
    """O-8: calibracao SEM a sentinela -> falha; com menos de
    `_MIN_STDLIB_PATHS` caminhos padrao -> falha."""
    del scratch, capture
    ctx = _make_ctx_for_test("GNU")
    missing_sentinel_nodes = [(1, "/usr/include/c++/16/cstdint", None)]
    raised_missing_sentinel = False
    try:
        evaluate_calibration(ctx, missing_sentinel_nodes, "/proj/src/core/sentinela_projeto.hpp", ("cstdint",))
    except _IncludeTreeError:
        raised_missing_sentinel = True

    too_few_nodes = [
        (1, "/proj/src/core/sentinela_projeto.hpp", None),
        (1, "/usr/include/c++/16/cstdint", None),
    ]
    raised_too_few = False
    try:
        evaluate_calibration(
            ctx, too_few_nodes, "/proj/src/core/sentinela_projeto.hpp",
            tuple(f"nome{i}" for i in range(200)),
        )
    except _IncludeTreeError:
        raised_too_few = True

    ok = raised_missing_sentinel and raised_too_few
    label = "selftest: O-8"
    print(
        f"{label} OK" if ok else f"{label} FALHOU (sentinela={raised_missing_sentinel}, piso={raised_too_few})",
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
    (selftest_oracle_o9_msvc_prefix_learned_any_locale,),
    (selftest_oracle_o10_msvc_nesting_one_space_per_level,),
    (selftest_oracle_o11_msvc_case_insensitive_path,),
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
