#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_format.py - pre-commit gate for clang-format compliance
# (GODS_LAWS.md L-23 portao 1 / L-24, literal: "clang-format com base
# LLVM, indentacao de 4, colunas 100").
#
# ORIGEM (LOOP-FMT-CLASS, 10/09/2026): o job `Lint` do CI reprovou por
# "code should be clang-formatted" TRES vezes seguidas no mesmo dia
# (runs 34511755389, 34516285084, 34518300110) - sempre um arquivo
# diferente, porque cada conserto tratava a INSTANCIA que o servidor
# tinha acabado de apontar, nunca a CLASSE. A causa raiz medida: os
# agentes implementadores rodam em container proprio SEM clang-format
# instalado (GODS_LAWS.md L-51 proibe instalar pacote sem autorizacao),
# entao um commit deles pode carregar codigo desformatado sem que
# ninguem tenha faltado com cuidado - o unico lugar que via a
# formatacao era o servidor, minutos depois.
#
# A CORRECAO DE CLASSE: este gate roda no `pre-commit` LOCAL (a cadeia
# de ganchos - ver tools/git-hooks/pre-commit), que TODO agente
# atravessa ao commitar, mesmo os que nao conseguem rodar o espelho
# completo (tools/preci.sh --lint-only, que builda a arvore inteira e
# custa minutos - GODS_LAWS.md L-11, um trabalho pesado por vez). Se o
# gancho reprovar aqui, o defeito morre na maquina de quem escreveu,
# nunca viaja ate o servidor.
#
# CRITERIO IDENTICO ao do CI: is_relevant_path() abaixo replica, byte a
# byte, o pathspec que tools/preci.sh's stage_format() usa via
# enumerate_tracked_cpp_hpp() ('*.cpp' '*.hpp' ':!:tests/preci_fixtures/
# *') - um arquivo que este gancho deixa passar e exatamente um arquivo
# que o job `lint` tambem deixaria passar, e vice-versa (GODS_LAWS.md
# L-04: servidor e local concordam). Only DIFFERENCE deliberada: este
# gate roda em modo STAGED (conteudo do INDICE, nunca da working tree -
# a mesma razao de check_dep_zero.py --staged: um `git add -p` parcial
# pode deixar a working tree com hunks que NUNCA vao ser commitados, e
# checar a working tree correria o risco de reprovar/aprovar pelo texto
# errado). tools/preci.sh's stage_format() continua sendo quem checa a
# ARVORE inteira (working tree apos checkout, forma que o CI usa) - os
# dois nunca competem, cada um cobre o momento que lhe cabe.
#
# GODS_LAWS.md L-40 ("portao que nunca reprova nao e portao", "o
# defeito que afirma medir e nao mede"): quando 'clang-format' nao
# esta no PATH, este gate NUNCA finge ter checado - imprime um aviso
# impossivel de ignorar (MISSING_TOOL_BANNER) e deixa o commit passar
# (warn-and-continue, nao fail-closed): o mesmo downgrade que
# tools/preci.sh's --selftest ja pratica para as mesmas tres
# ferramentas (ver o comentario de preci_selftest em
# tests/CMakeLists.txt) - o job `Lint` do CI, que SEMPRE tem
# clang-format instalado, continua sendo a segunda rede de protecao
# para quem nao pode rodar esta primeira. Bloquear o commit inteiro
# por falta de uma ferramenta que GODS_LAWS.md L-51 proibe instalar
# sem autorizacao trocaria um defeito medido (formatacao vazando ate o
# servidor) por outro pior (agente sem clang-format nunca mais
# consegue commitar nada, inclusive as duas outras gates desta mesma
# cadeia). A ausencia de clang-format e sempre DECLARADA em texto que
# nao se perde no meio de uma saida longa - nunca um "WARNING" de uma
# linha so.
#
# GODS_LAWS.md L-36 (loop item-a-item, nunca lote): check_format_paths()
# roda 'clang-format --dry-run -Werror' UM arquivo por vez, contando
# found/checked/violations sempre, mesmo em zero - um clang-format que
# travasse no meio de uma lista batched nunca teria como aparecer aqui,
# porque nao existe lista batched.
#
# A CONFIGURACAO DE ESTILO (.clang-format, BasedOnStyle: LLVM +
# IndentWidth 4 + ColumnLimit 100) tem que ser a MESMA que o CI usa -
# clang-format busca um '.clang-format' subindo a arvore de diretorios
# a partir do arquivo, e o conteudo staged e materializado num diretorio
# temporario ISOLADO (git checkout-index) que nao tem o '.clang-format'
# do repositorio dentro dele por padrao. materialize_staged_relevant()
# copia o '.clang-format' real para a raiz do diretorio temporario
# antes de chamar clang-format - sem isso, o gate cairia no estilo LLVM
# puro (indentacao 2, coluna 80) e reprovaria/aprovaria pelo criterio
# ERRADO, silenciosamente.

import os
import shutil
import stat
import subprocess
import sys
import tempfile

SCRIPT_NAME = "check_format.py"

FORMAT_EXTENSIONS = (".cpp", ".hpp")
FIXTURES_PREFIX = "tests/preci_fixtures/"

MISSING_TOOL_BANNER = f"""\
================================================================
{SCRIPT_NAME}: 'clang-format' NAO ENCONTRADO NO PATH.
A FORMATACAO NAO FOI VERIFICADA NESTE COMMIT.
Este gancho DECLARA a lacuna (GODS_LAWS.md L-40: um portao que cala
quando a ferramenta falta e o defeito "afirma medir e nao mede") -
nunca finge ter passado. O commit segue em frente porque
GODS_LAWS.md L-51 proibe instalar pacote sem autorizacao do lider;
o job `Lint` do CI (fedora:latest, clang-format sempre presente)
continua sendo a segunda rede de protecao para esta maquina.
================================================================\
"""

# Mesma convencao de check_container_fixture_link.py/check_win32_test_
# link.py: "declarado PULADO" (CTest SKIP_RETURN_CODE), nem PASS nem
# FAIL - reservado a lacuna de FERRAMENTA, nunca a reprovacao real do
# gate (real_main() acima nunca sai com este codigo; so selftest_main()
# usa, e so por este UM motivo).
GATE_SKIP_RETURN_CODE = 77

# CONSERTO (10/09/2026, item LOOP-FMT-CLASS, achado no run 34526530748
# / commit a59a297): check_format_selftest foi registrado SEM guarda
# de ferramenta (be66fa6) - a mesma classe de defeito que este proprio
# gate existe para varrer em codigo C++ (GODS_LAWS.md L-36/L-40, "o
# defeito que afirma medir e nao mede"). O controle NEGATIVO do
# selftest (selftest_negative_control) so prova algo se clang-format
# de fato reprovar um arquivo desformatado - sem a ferramenta,
# check_format_staged() cai direto no ramo do MISSING_TOOL_BANNER
# acima (aprova SEMPRE, warn-and-continue por desenho), entao o
# controle negativo reprova o SELFTEST, nao porque o gate esta
# quebrado, mas porque o proprio teste que deveria provar o gate
# nao consegue provar nada sem a ferramenta.
#
# MEDIDO, nao presumido, antes de escolher SKIP_RETURN_CODE em vez de
# registro condicional (o outro precedente da casa, preci_selftest):
# nenhum job do CI hoje EXECUTA check_format_selftest com clang-format
# presente. Os jobs `linux` (Fedora/Ubuntu/CachyOS/Arch) e `windows`
# rodam `ctest --test-dir <builddir> --output-on-failure` (a suite
# inteira, onde este teste de fato roda) mas instalam so
# "gcc-c++ cmake ninja-build pkgconf-pkg-config git" - sem
# clang-tools-extra, sem clang-format. O unico job que instala
# clang-format e' `lint` (dnf install ... clang-tools-extra ...,
# .github/workflows/ci.yml), mas `lint` nunca chama stage_ctest (so
# run_lint_only, que para em clang-tidy/cppcheck) - o unico ctest que
# `lint` roda e' `ctest --test-dir build-preci -N`, que so LISTA os
# testes (para o inventario de paridade), nunca os EXECUTA. Ou seja:
# hoje, com a ferramenta presente, este autoteste so roda de verdade
# em maquina de desenvolvedor (`ctest --test-dir build -R
# check_format_selftest`, ou este script direto com --selftest) - em
# CI ele so e' EXERCIDO onde a ferramenta falta, nunca onde ela esta.
# Isto e uma lacuna de cobertura conhecida e relatada ao lider (nao
# corrigida aqui por conta propria - abrir job/passo novo em CI e'
# decisao dele, GODS_LAWS.md L-51/L-14), nao escondida por este skip.
#
# Por que SKIP_RETURN_CODE (como win_vm_lab_isolamento_selftest) em
# vez de registro condicional (como preci_selftest, tests/CMakeLists.
# txt): registro condicional faz o teste SUMIR do inventario de
# paridade Linux x Windows em metade da matriz - exatamente a
# assimetria silenciosa que GODS_LAWS.md L-04 proibe. Com
# SKIP_RETURN_CODE, o teste continua LISTADO (ctest -N) em toda perna,
# so nao EXECUTA onde a ferramenta falta - "Not Run", nunca "Passed"
# nem ausente do registro.
SELFTEST_SKIP_BANNER = f"""\
================================================================
{SCRIPT_NAME} --selftest: 'clang-format' NAO ENCONTRADO NO PATH.
O AUTOTESTE FOI PULADO (CTest SKIP_RETURN_CODE {GATE_SKIP_RETURN_CODE}),
NUNCA REPROVADO NEM APROVADO.
O controle NEGATIVO deste autoteste (selftest_negative_control) so
prova algo executando clang-format de verdade contra um arquivo
desformatado - sem a ferramenta ele so provaria que
check_format_staged() aprova por causa do MISSING_TOOL_BANNER, e essa
lacuna especifica ja tem controle proprio (selftest_missing_tool_
control, que simula a ausencia via PATH privado e roda sempre, com ou
sem a ferramenta real disponivel neste host). GODS_LAWS.md L-51
proibe instalar clang-format sem autorizacao do lider.
================================================================\
"""


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


def is_relevant_path(path):
    """Mesmo criterio de tools/preci.sh's enumerate_tracked_cpp_hpp():
    '*.cpp'/'*.hpp', exceto tests/preci_fixtures/* (a arvore de
    fixtures de --selftest, deliberadamente suja - nunca deve reprovar
    um commit real, GODS_LAWS.md L-40)."""
    return path.endswith(FORMAT_EXTENSIONS) and not path.startswith(FIXTURES_PREFIX)


# --- git plumbing (DUPLICADO de check_dep_zero.py's proprias funcoes
# do mesmo nome/formato - convencao da casa: cada check_*.py e um atomo
# autossuficiente, GODS_LAWS.md L-34; ver o header daquele arquivo) ----


def git_diff_cached_name_only_z_raw(root):
    try:
        result = subprocess.run(
            ["git", "-C", root, "diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR"],
            capture_output=True,
        )
    except FileNotFoundError:
        return b"", False
    if result.returncode != 0:
        return b"", False
    return result.stdout, True


def decode_z_listing(raw):
    if not raw:
        return []
    body = raw[:-1] if raw.endswith(b"\0") else raw
    encoding = sys.getfilesystemencoding()
    return [chunk.decode(encoding, errors="surrogateescape") for chunk in body.split(b"\0")]


def encode_z_listing(paths):
    encoding = sys.getfilesystemencoding()
    if not paths:
        return b""
    return b"\0".join(p.encode(encoding, errors="surrogateescape") for p in paths) + b"\0"


def is_git_repo(root):
    try:
        result = subprocess.run(
            ["git", "-C", root, "rev-parse", "--is-inside-work-tree"], capture_output=True
        )
    except FileNotFoundError:
        return False
    return result.returncode == 0


def materialize_staged_relevant(root, tmp_root, relevant_paths):
    """Materializa APENAS o conteudo do INDICE (nunca a working tree,
    mesma razao de check_dep_zero.py's materialize_staged_index) dos
    caminhos RELEVANTES (*.cpp/*.hpp fora de tests/preci_fixtures/) via
    'git checkout-index -z --stdin' - escopo menor que o de
    check_dep_zero.py de proposito: este gate nao precisa do resto do
    commit, so dos arquivos que vai realmente rodar clang-format em
    cima. Copia o '.clang-format' real do repo para a raiz de tmp_root
    em seguida - ver o header deste arquivo para o porque (resolucao de
    estilo do clang-format sobe diretorios a partir do arquivo)."""
    raw = encode_z_listing(relevant_paths)
    try:
        result = subprocess.run(
            ["git", "-C", root, "checkout-index", "-z", "--stdin", f"--prefix={tmp_root}{os.sep}"],
            input=raw,
            capture_output=True,
        )
    except FileNotFoundError:
        return False
    if result.returncode != 0:
        return False
    style_src = os.path.join(root, ".clang-format")
    if os.path.isfile(style_src):
        shutil.copyfile(style_src, os.path.join(tmp_root, ".clang-format"))
    return True


def _clear_readonly_and_retry(func, path, _exc_info_or_exc):
    for target in (os.path.dirname(path), path):
        if target and os.path.exists(target):
            try:
                os.chmod(target, stat.S_IWRITE | stat.S_IREAD | stat.S_IEXEC)
            except OSError:
                pass
    func(path)


def remove_tree_tolerant(path, ignore_errors=False):
    if not os.path.exists(path):
        return
    kwargs = {"onexc": _clear_readonly_and_retry} if sys.version_info >= (3, 12) else {"onerror": _clear_readonly_and_retry}
    try:
        shutil.rmtree(path, **kwargs)
    except OSError:
        if not ignore_errors:
            raise


# --- o gate propriamente dito -------------------------------------------


def clang_format_available():
    return shutil.which("clang-format") is not None


def run_clang_format_dry_run(path):
    """Roda 'clang-format --dry-run -Werror' num UNICO arquivo. Retorna
    (ok, stderr_text). Um clang-format ausente NUNCA chega aqui - o
    chamador ja checou clang_format_available() antes de entrar no
    loop (GODS_LAWS.md L-36: a ausencia e verificada uma vez, fora do
    loop, nunca por arquivo)."""
    result = subprocess.run(["clang-format", "--dry-run", "-Werror", path], capture_output=True, text=True)
    return result.returncode == 0, result.stderr


def check_format_paths(content_root, relevant_paths):
    """Loop item-a-item (GODS_LAWS.md L-36) - NUNCA uma chamada em lote
    de clang-format sobre todos os arquivos, que esconderia qual
    arquivo travou no meio da lista. Retorna (found, checked,
    violations); found == checked sempre aqui por construcao (nao ha
    'arquivo recusou abrir' possivel - o proprio git checkout-index ja
    teria falhado antes), mas o chamador ainda confere a igualdade
    explicitamente (GODS_LAWS.md L-40: nunca presumida)."""
    found = len(relevant_paths)
    checked = 0
    violations = []
    for p in relevant_paths:
        full = os.path.join(content_root, *p.split("/"))
        ok, text = run_clang_format_dry_run(full)
        checked += 1
        if not ok:
            violations.append((p, text))
    return found, checked, violations


def check_format_staged(root):
    if not is_git_repo(root):
        print(f"{SCRIPT_NAME}: not a git repository: {root}", file=sys.stderr)
        return False

    listing_raw, ok = git_diff_cached_name_only_z_raw(root)
    if not ok:
        print(f"{SCRIPT_NAME}: 'git diff --cached' failed in {root}", file=sys.stderr)
        return False
    staged = decode_z_listing(listing_raw)
    staged_count = len(staged)
    relevant = sorted(p for p in staged if is_relevant_path(p))

    if not relevant:
        print(
            f"{SCRIPT_NAME}: 0 arquivo(s) relevante(s) entre {staged_count} staged; "
            "*.cpp/*.hpp fora de tests/preci_fixtures/ intocados por este commit "
            "(GODS_LAWS.md L-40: declarado, nunca silencioso)"
        )
        return True

    if not clang_format_available():
        print(MISSING_TOOL_BANNER, file=sys.stderr)
        return True

    tmp_root = tempfile.mkdtemp(
        prefix="glintfx-format-staged-", dir=os.environ.get("TMPDIR", tempfile.gettempdir())
    )
    try:
        if not materialize_staged_relevant(root, tmp_root, relevant):
            print(
                f"{SCRIPT_NAME}: 'git checkout-index' failed materializing staged content in {root}",
                file=sys.stderr,
            )
            return False
        found, checked, violations = check_format_paths(tmp_root, relevant)
    finally:
        remove_tree_tolerant(tmp_root, ignore_errors=True)

    if checked != found or violations:
        print(
            f"{SCRIPT_NAME}: PROIBIDO (GODS_LAWS.md L-23 portao 1 / L-24): codigo "
            "staged nao esta clang-formatado:",
            file=sys.stderr,
        )
        for p, text in violations:
            print(f"{SCRIPT_NAME}: {p}: nao formatado", file=sys.stderr)
            if text.strip():
                print(text.rstrip("\n"), file=sys.stderr)
        if checked != found:
            print(
                f"{SCRIPT_NAME}: varredura incompleta - {checked}/{found} arquivo(s) "
                "verificados (GODS_LAWS.md L-40 fail-closed)",
                file=sys.stderr,
            )
        print(
            f"{SCRIPT_NAME}: {len(violations)} violacao(oes) entre {checked}/{found} "
            f"arquivo(s) c++ verificados, {staged_count} staged no total - rode "
            "'clang-format -i <arquivo>' e reestagie ('git add') antes de commitar",
            file=sys.stderr,
        )
        return False

    print(
        f"{SCRIPT_NAME}: 0 violacao(oes) - {checked} arquivo(s) c++ entre "
        f"{staged_count} staged, clang-formatados OK"
    )
    return True


def real_main(args):
    if len(args) == 2 and args[0] == "--staged":
        root = args[1]
        if not os.path.isdir(root):
            fail(f"directory not found: {root}")
        if not check_format_staged(root):
            fail("clang-format reprovou arquivo(s) staged (ver mensagem acima)")
        return
    fail("usage: check_format.py --staged <source-root-directory>")


# --- selftest fixtures and controls -------------------------------------

CLEAN_CXX = (
    "// SPDX-License-Identifier: AGPL-3.0-or-later\n"
    "int f() {\n"
    "    if (true) {\n"
    "        return 1;\n"
    "    }\n"
    "    return 0;\n"
    "}\n"
)

# Indentacao de 2 espacos (LLVM puro) e 6 no bloco interno - errada nos
# dois niveis contra .clang-format (IndentWidth 4). Serve tambem de
# controle indireto de que o .clang-format REAL esta sendo copiado
# para o diretorio temporario: se copy falhasse silenciosamente,
# clang-format cairia no LLVM puro (2 espacos) e este mesmo texto
# passaria por acidente.
DIRTY_CXX = (
    "// SPDX-License-Identifier: AGPL-3.0-or-later\n"
    "int g() {\n"
    "  if (true) {\n"
    "      return 1;\n"
    "  }\n"
    "  return 0;\n"
    "}\n"
)


def make_scratch_workdir():
    return tempfile.mkdtemp(prefix="glintfx-format-selftest-")


def init_fixture_repo(root):
    os.makedirs(root, exist_ok=True)
    subprocess.run(["git", "-C", root, "init", "-q"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.email", "selftest@check-format.invalid"], check=True)
    subprocess.run(["git", "-C", root, "config", "user.name", "check_format selftest"], check=True)


def write_file(root, relative_path, content):
    full_path = os.path.join(root, *relative_path.split("/"))
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, "w", encoding="utf-8") as handle:
        handle.write(content)


def stage(root, *relative_paths):
    subprocess.run(["git", "-C", root, "add", "--", *relative_paths], check=True, cwd=root)


def write_house_style(root):
    write_file(
        root,
        ".clang-format",
        "BasedOnStyle: LLVM\nIndentWidth: 4\nColumnLimit: 100\n",
    )


class _Captured:
    def __init__(self, result, text):
        self.result = result
        self.text = text


def _make_capture():
    import contextlib
    import io

    def capture(fn):
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer), contextlib.redirect_stderr(buffer):
            result = fn()
        return _Captured(result, buffer.getvalue())

    return capture


def selftest_positive_control(scratch, capture):
    root = os.path.join(scratch, "positive")
    init_fixture_repo(root)
    write_house_style(root)
    write_file(root, "src/foo.cpp", CLEAN_CXX)
    stage(root, ".clang-format", "src/foo.cpp")

    output = capture(lambda: check_format_staged(root))
    if not output.result:
        print("selftest: controle POSITIVO FALHOU (arquivo ja formatado foi reprovado)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    print("selftest: controle POSITIVO OK (arquivo staged ja formatado passa, .clang-format da casa foi respeitado)")
    return True


def selftest_negative_control(scratch, capture):
    root = os.path.join(scratch, "negative")
    init_fixture_repo(root)
    write_house_style(root)
    write_file(root, "src/bar.cpp", DIRTY_CXX)
    stage(root, ".clang-format", "src/bar.cpp")

    output = capture(lambda: check_format_staged(root))
    if output.result:
        print("selftest: controle NEGATIVO FALHOU (src/bar.cpp desformatado deveria ter sido reprovado)", file=sys.stderr)
        return False
    if "src/bar.cpp" not in output.text:
        print("selftest: controle NEGATIVO FALHOU (reprovou, mas nao citou src/bar.cpp)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    print("selftest: controle NEGATIVO OK (src/bar.cpp desformatado citado e reprovado)")
    return True


def selftest_fixtures_exclusion_control(scratch, capture):
    """tests/preci_fixtures/* fica de fora do criterio (mesmo pathspec
    do CI) - um arquivo DELIBERADAMENTE sujo la dentro nunca pode
    reprovar um commit real."""
    root = os.path.join(scratch, "fixtures_exclusion")
    init_fixture_repo(root)
    write_house_style(root)
    write_file(root, "tests/preci_fixtures/dirty/whatever.cpp", DIRTY_CXX)
    stage(root, ".clang-format", "tests/preci_fixtures/dirty/whatever.cpp")

    output = capture(lambda: check_format_staged(root))
    if not output.result:
        print("selftest: controle de EXCLUSAO DE FIXTURES FALHOU (arquivo sob tests/preci_fixtures/ reprovou o commit)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    print("selftest: controle de EXCLUSAO DE FIXTURES OK (arquivo sujo sob tests/preci_fixtures/ nao e coberto)")
    return True


def selftest_empty_scan_control(scratch, capture):
    root = os.path.join(scratch, "empty")
    init_fixture_repo(root)
    write_file(root, "README.md", "# doc, fora do criterio\n")
    stage(root, "README.md")

    output = capture(lambda: check_format_staged(root))
    if not output.result:
        print("selftest: controle de VARREDURA VAZIA FALHOU (commit sem *.cpp/*.hpp deveria passar declarado)", file=sys.stderr)
        return False
    if "0 arquivo(s) relevante(s)" not in output.text:
        print("selftest: controle de VARREDURA VAZIA FALHOU (passou, mas nao declarou a contagem zero)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        return False
    print("selftest: controle de VARREDURA VAZIA OK (0 relevante declarado, nunca silencioso)")
    return True


def selftest_not_a_repo_control(scratch, capture):
    root = os.path.join(scratch, "not_a_repo")
    os.makedirs(root, exist_ok=True)

    output = capture(lambda: check_format_staged(root))
    if output.result:
        print("selftest: controle NAO-E-REPO FALHOU (diretorio sem git deveria ter sido recusado)", file=sys.stderr)
        return False
    print("selftest: controle NAO-E-REPO OK")
    return True


def selftest_missing_tool_control(scratch, capture):
    """Simula 'clang-format' ausente do PATH (o exato cenario dos
    containers de implementer sem a ferramenta) - prova que o gancho
    DECLARA a lacuna (banner) e deixa o commit passar, nunca finge ter
    checado (GODS_LAWS.md L-40)."""
    root = os.path.join(scratch, "missing_tool")
    init_fixture_repo(root)
    write_house_style(root)
    write_file(root, "src/baz.cpp", DIRTY_CXX)
    stage(root, ".clang-format", "src/baz.cpp")

    # PATH privado com SO' 'git' (via symlink) - reflete o container
    # real do implementer (git presente, clang-format nao). Zerar o
    # PATH inteiro quebraria 'git' tambem (ambos moram em /usr/bin
    # nesta maquina) e derrubaria o proprio is_git_repo()/git diff
    # antes de chegar no caminho que este controle quer provar.
    git_real = shutil.which("git")
    if git_real is None:
        print("selftest: controle de FERRAMENTA AUSENTE FALHOU ('git' nao encontrado para montar o PATH privado)", file=sys.stderr)
        return False
    private_bin = os.path.join(scratch, "private_bin_missing_clang_format")
    os.makedirs(private_bin, exist_ok=True)
    os.symlink(git_real, os.path.join(private_bin, "git"))
    saved_path = os.environ.get("PATH", "")
    os.environ["PATH"] = private_bin
    try:
        output = capture(lambda: check_format_staged(root))
    finally:
        os.environ["PATH"] = saved_path

    ok = True
    if not output.result:
        print("selftest: controle de FERRAMENTA AUSENTE FALHOU (deveria deixar o commit passar, com aviso)", file=sys.stderr)
        ok = False
    if "NAO ENCONTRADO NO PATH" not in output.text:
        print("selftest: controle de FERRAMENTA AUSENTE FALHOU (nao declarou a ausencia)", file=sys.stderr)
        print(output.text, file=sys.stderr)
        ok = False
    if ok:
        print("selftest: controle de FERRAMENTA AUSENTE OK (banner declarado, commit segue em frente)")
    return ok


def selftest_readonly_cleanup_control():
    """DUPLICADO do mesmo controle em check_dep_zero.py/check_spdx.py -
    prova que remove_tree_tolerant() de fato remove uma arvore com
    arquivo read-only (o que shutil.rmtree(ignore_errors=True) sozinho
    apenas ENGOLE, sem remover, no Windows - ver o header daquele
    controle nos outros dois arquivos para o incidente MEDIDO)."""
    scratch = tempfile.mkdtemp(prefix="glintfx-format-selftest-readonly-")
    target = os.path.join(scratch, "readonly.txt")
    with open(target, "w", encoding="utf-8") as handle:
        handle.write("conteudo\n")
    os.chmod(target, stat.S_IREAD)
    remove_tree_tolerant(scratch, ignore_errors=True)
    if os.path.exists(scratch):
        print("selftest: controle de LIMPEZA READ-ONLY FALHOU (arvore com arquivo read-only nao foi removida)", file=sys.stderr)
        return False
    print("selftest: controle de LIMPEZA READ-ONLY OK")
    return True


def selftest_main():
    # Guarda ANTES de tocar disco (GODS_LAWS.md L-40/L-36): 'clang-format'
    # ausente do PATH REAL deste processo (nao do PATH privado que
    # selftest_missing_tool_control monta por conta propria mais abaixo -
    # aquele controle continua rodando sempre, simulando a ausencia
    # mesmo em host que TEM a ferramenta). Ver o comentario de
    # SELFTEST_SKIP_BANNER acima para o porque de SKIP_RETURN_CODE em
    # vez de reprovar ou de registro condicional.
    if not clang_format_available():
        print(SELFTEST_SKIP_BANNER, file=sys.stderr)
        sys.exit(GATE_SKIP_RETURN_CODE)

    scratch = make_scratch_workdir()
    capture = _make_capture()
    try:
        controls = [
            selftest_positive_control(scratch, capture),
            selftest_negative_control(scratch, capture),
            selftest_fixtures_exclusion_control(scratch, capture),
            selftest_empty_scan_control(scratch, capture),
            selftest_not_a_repo_control(scratch, capture),
            selftest_missing_tool_control(scratch, capture),
            selftest_readonly_cleanup_control(),
        ]
        if not all(controls):
            print(f"{SCRIPT_NAME} --selftest: FALHOU (ver acima)", file=sys.stderr)
            sys.exit(1)
        print(f"{SCRIPT_NAME} --selftest: os {len(controls)} controles OK")
    finally:
        remove_tree_tolerant(scratch, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--selftest":
        selftest_main()
    else:
        real_main(args)


if __name__ == "__main__":
    main()
