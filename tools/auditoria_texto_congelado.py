#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# auditoria_texto_congelado.py - GFX-PRESET e R2D-BATCH, D-FECH-3
# (/var/tmp/cto-w7d/plano-fechamento-w7d.md, G4; DECISOES_AUTONOMAS.md
# D-A68).
#
# O QUE FAZ: cada parecer de revisao de API dedicada
# (docs/auditoria-api-gfx-preset.md = P0, docs/auditoria-api-draw2d.md =
# B0) congela, na sua secao 2, o texto dos cabecalhos publicos. Depois
# do parecer vieram emendas do CTO, registradas na errata (copiada em
# docs/auditoria-plano-w7d.md): E1 e E2 no P0, E3 (secao 12), E4
# (secao 17) e D-B4-1 (secao 19) no B0. O "texto final congelado" e o
# parecer MAIS as emendas. Este script aplica as emendas, uma a uma, ao
# texto das cercas da secao 2 e escreve o resultado como APENDICE no
# fim do proprio parecer, entre dois marcadores. O apendice e saida de
# script: nunca se edita a mao (o modo `verificar` reprova se alguem o
# fizer ou se o corpo do parecer mudar).
#
# FAIL-CLOSED (GODS_LAWS.md L-36/L-40): uma emenda cujo texto antigo nao
# aparece EXATAMENTE UMA VEZ nas cercas reprova com codigo 1, nomeando a
# emenda. Zero cercas encontradas na secao 2 tambem reprova (varredura
# vazia nao e "nada a emendar").
#
# O QUE E MECANICO E O QUE NAO: o texto novo de cada emenda esta neste
# arquivo, escrito a partir do que a errata decide. Onde a errata fixa a
# frase (E1) ela vale verbatim; onde a errata fixa so o sentido (E2: os
# nomes finais sao do implementador de P3; E3 e E4: a redacao e a
# proposta do api-review; D-B4-1: o arquivo pronto da errata), a redacao
# aqui e deste script, e a comparacao com o cabecalho publicado pode
# deixar um hunk residual, que o orquestrador registra um por um.
#
# USO
#   auditoria_texto_congelado.py gerar <parecer.md>...
#   auditoria_texto_congelado.py verificar <parecer.md>...
#   auditoria_texto_congelado.py --selftest
#
# Cada funcao abaixo faz uma coisa so (GODS_LAWS.md L-17 do projeto).

import os
import sys

SCRIPT_NAME = "auditoria_texto_congelado.py"

MARK_BEGIN = "<!-- APENDICE-INICIO: saida de tools/auditoria_texto_congelado.py, nunca editar a mao -->"
MARK_END = "<!-- APENDICE-FIM -->"

FENCE = "````"  # as cercas da secao 2 dos pareceres usam 4 crases


def fail(message):
    print(f"{SCRIPT_NAME}: {message}", file=sys.stderr)
    sys.exit(1)


class EmendaError(Exception):
    pass


def plus(text):
    """Prefixa '+' em cada linha: o texto do P0 e um diff (so linhas '+' mudam)."""
    return "".join("+" + line for line in text.splitlines(keepends=True))


# --- as emendas, na ordem em que a errata as registra ---------------------

E1_OLD = plus(
    "// An out-of-range `preset` or `row_index` degrades (docs/api-\n"
    "// conventions.md R4) to the entry {preset, k_gltfx_preset_manual}:\n"
    "// the one entry that, if applied by mistake, changes the value of no\n"
    "// row - it only sets the preset label to `manual` (rule 3 above). It\n"
    "// never degrades to a default-constructed entry, because {vsync, 0}\n"
    "// would silently turn vsync off.\n"
)
E1_NEW = plus(
    "// An out-of-range `preset` or `row_index` degrades (docs/api-\n"
    "// conventions.md R4) to the entry {suggested_preset,\n"
    "// k_gltfx_preset_manual}: an id that is read_only, so handing it to\n"
    "// set_option() by mistake is refused with invalid_argument and\n"
    "// changes nothing - neither a row nor your preset label. It is never\n"
    "// a default-constructed entry, because {vsync, 0} would silently\n"
    "// turn vsync off.\n"
)

E2_OLD = plus("// `auto_choice_reason` (id 6, `read_only`): WHY `suggested_preset`\n")
E2_NEW = plus(
    "// `vsync` (id 0, `live`): the numbers of the option, wherever it is\n"
    "// read - including the entries gltfx_gfx_preset_row_at() hands back.\n"
    "// Same data-contract rule as the rows above: never reused, never\n"
    "// renumbered.\n"
    "inline constexpr std::int64_t k_gltfx_vsync_off = 0;\n"
    "inline constexpr std::int64_t k_gltfx_vsync_on = 1;\n"
    "inline constexpr std::int64_t k_gltfx_vsync_adaptive = 2;\n"
    "\n"
) + E2_OLD

E3_OLD = (
    "    //   - blending on, as (ONE, ONE_MINUS_SRC_ALPHA) for color and\n"
    "    //     alpha;\n"
    "    //   - depth test, stencil test, scissor test and face culling off;\n"
    "    //     color mask all on;\n"
    "    //   - the viewport covering the whole surface;\n"
)
E3_NEW = (
    "    //   - blending on, as (ONE, ONE_MINUS_SRC_ALPHA) for color and\n"
    "    //     alpha, with the blend equation ADD for both;\n"
    "    //   - the polygon mode FILL for both faces;\n"
    "    //   - depth test, stencil test, scissor test and face culling off;\n"
    "    //     color mask all on;\n"
    "    //   - rasterizer discard, color logic op and sample-alpha-to-coverage\n"
    "    //     off;\n"
    "    //   - the DRAW framebuffer 0 (the surface itself);\n"
    "    //   - the viewport covering the whole surface;\n"
)

E4_OPEN_OLD = (
    "    //   - `out_of_memory`: the room asked for by `desc` could not be\n"
    "    //     reserved.\n"
)
E4_OPEN_NEW = (
    "    //   - `platform_failure`, rejected_value() \"vertex_array_create\":\n"
    "    //     the graphics card did not give the renderer its vertex array\n"
    "    //     or buffers (the GL error code, when there is one, is in\n"
    "    //     os_error_code()).\n"
    "    //   - `out_of_memory`: the room asked for by `desc` could not be\n"
    "    //     reserved, or the graphics card itself ran out of memory while\n"
    "    //     creating the renderer's objects (rejected_value()\n"
    "    //     \"vertex_array_create\", the GL error code in os_error_code()).\n"
)
E4_FINISH_OLD = (
    "    //   - `out_of_memory`: pieces were dropped for lack of memory;\n"
    "    //   - `platform_failure`: the graphics card refused the frame's data\n"
    "    //     (rejected_value() \"vertex_upload\" or \"draw\"), or the context\n"
    "    //     could not be made current (the context's own error, passed\n"
    "    //     on);\n"
)
E4_FINISH_NEW = (
    "    //   - `out_of_memory`: pieces were dropped for lack of memory, or\n"
    "    //     the graphics card itself ran out of memory (then\n"
    "    //     rejected_value() names the step and os_error_code() holds the\n"
    "    //     GL error code);\n"
    "    //   - `platform_failure`: the graphics card refused the frame's data\n"
    "    //     (rejected_value() \"vertex_upload\", \"index_upload\" or \"draw\",\n"
    "    //     the GL error code in os_error_code()), or the context could\n"
    "    //     not be made current (the context's own error, passed on);\n"
)

D_B4_1_RULE_OLD = (
    "// submitted while a frame is open ends in exactly one of three places:\n"
    "//\n"
    "//   pieces_submitted == pieces_drawn + pieces_refused\n"
    "//                       + pieces_dropped_out_of_memory\n"
)
D_B4_1_RULE_NEW = (
    "// submitted while a frame is open ends in exactly one of four places:\n"
    "//\n"
    "//   pieces_submitted == pieces_drawn + pieces_refused\n"
    "//                       + pieces_dropped_out_of_memory\n"
    "//                       + pieces_dropped_graphics_failure\n"
)
D_B4_1_FIELD_OLD = (
    "    // Pieces dropped because the renderer could not get memory to store\n"
    "    // them. When this is not zero, finish_frame() returns an error.\n"
    "    std::uint64_t pieces_dropped_out_of_memory = 0;\n"
)
D_B4_1_FIELD_NEW = (
    "    // Pieces dropped because the renderer could not get memory of its own\n"
    "    // to store them. When this is not zero, finish_frame() returns an\n"
    "    // error.\n"
    "    std::uint64_t pieces_dropped_out_of_memory = 0;\n"
    "    // Pieces accepted while the frame was open that never reached the\n"
    "    // screen because the graphics side failed: the context could not be\n"
    "    // made current, or the graphics card refused the frame's data (the\n"
    "    // card running out of ITS memory counts here, not in\n"
    "    // pieces_dropped_out_of_memory). When this is not zero,\n"
    "    // finish_frame() returns an error, and the error says why.\n"
    "    std::uint64_t pieces_dropped_graphics_failure = 0;\n"
)

# (id, texto antigo, texto novo)
EMENDAS_GFX_PRESET = [
    ("E1", E1_OLD, E1_NEW),
    ("E2", E2_OLD, E2_NEW),
]
EMENDAS_DRAW2D = [
    ("E3", E3_OLD, E3_NEW),
    ("E4 (open)", E4_OPEN_OLD, E4_OPEN_NEW),
    ("E4 (finish_frame)", E4_FINISH_OLD, E4_FINISH_NEW),
    ("D-B4-1 (regra dos destinos)", D_B4_1_RULE_OLD, D_B4_1_RULE_NEW),
    ("D-B4-1 (campo)", D_B4_1_FIELD_OLD, D_B4_1_FIELD_NEW),
]
EMENDAS_POR_ARQUIVO = {
    "auditoria-api-gfx-preset.md": EMENDAS_GFX_PRESET,
    "auditoria-api-draw2d.md": EMENDAS_DRAW2D,
}


# --- extracao e aplicacao -------------------------------------------------

def split_appendix(text):
    """Devolve (corpo, apendice_ou_vazio). O corpo termina antes do marcador."""
    start = text.find(MARK_BEGIN)
    if start == -1:
        return text, ""
    return text[:start], text[start:]


def section2_fences(body):
    """Lista de (titulo, texto) das cercas de 4 crases entre '## 2.' e '## 3.'."""
    lines = body.splitlines(keepends=True)
    in_section = False
    title = ""
    fences = []
    current = None
    for line in lines:
        if current is None:
            if line.startswith("## 2."):
                in_section = True
                continue
            if line.startswith("## 3."):
                break
            if in_section and line.startswith("### "):
                title = line.strip()
            if in_section and line.startswith(FENCE):
                current = [line[len(FENCE):].strip(), []]
        else:
            if line.startswith(FENCE) and line.strip() == FENCE:
                fences.append((title, current[0], "".join(current[1])))
                current = None
            else:
                current[1].append(line)
    return fences


def apply_emendas(fences, emendas):
    """Aplica cada emenda a exatamente uma ocorrencia; senao EmendaError."""
    texts = [text for (_title, _lang, text) in fences]
    for emenda_id, old, new in emendas:
        total = sum(text.count(old) for text in texts)
        if total != 1:
            raise EmendaError(
                f"emenda {emenda_id}: o texto antigo aparece {total} vez(es) nas cercas, "
                "esperado exatamente 1"
            )
        texts = [text.replace(old, new, 1) if old in text else text for text in texts]
    return [(title, lang, text) for ((title, lang, _), text) in zip(fences, texts)]


def diff_to_final_text(text):
    """O P0 e um diff: o texto final de cada hunk e contexto + linhas '+'."""
    out = []
    for line in text.splitlines(keepends=True):
        if line.startswith(("--- ", "+++ ")):
            continue
        if line.startswith("@@"):
            out.append("// ---- " + line.strip() + " ----\n")
        elif line.startswith("+"):
            out.append(line[1:])
        elif line.startswith("-"):
            continue
        elif line.startswith(" "):
            out.append(line[1:])
        else:
            out.append(line)
    return "".join(out)


def render_appendix(fences, emendas):
    parts = [MARK_BEGIN + "\n", "\n", "## Apêndice: texto final congelado\n", "\n"]
    ids = ", ".join(emenda_id for emenda_id, _old, _new in emendas)
    parts.append(
        "Gerado a partir das cercas da seção 2, com as emendas aplicadas ("
        + ids
        + "). Cada hunk de diff aparece como o texto final (contexto e linhas "
        "acrescentadas, sem as removidas).\n\n"
    )
    for title, lang, text in fences:
        if title:
            parts.append("### " + title.lstrip("# ").strip() + " (final)\n\n")
        if lang == "diff":
            text = diff_to_final_text(text)
            lang = "cpp"
        parts.append(FENCE + lang + "\n" + text + FENCE + "\n\n")
    parts.append(MARK_END + "\n")
    return "".join(parts)


def build(text, emendas):
    body, _old_appendix = split_appendix(text)
    fences = section2_fences(body)
    if not fences:
        raise EmendaError("nenhuma cerca de 4 crases na secao 2 (varredura vazia)")
    final = apply_emendas(fences, emendas)
    return body + render_appendix(final, emendas)


def emendas_for(path):
    name = os.path.basename(path)
    if name not in EMENDAS_POR_ARQUIVO:
        fail(f"sem lista de emendas para {name}")
    return EMENDAS_POR_ARQUIVO[name]


def read(path):
    with open(path, "r", encoding="utf-8", newline="") as handle:
        return handle.read()


def cmd_gerar(paths):
    for path in paths:
        try:
            new_text = build(read(path), emendas_for(path))
        except EmendaError as exc:
            fail(f"{path}: {exc}")
        with open(path, "w", encoding="utf-8", newline="") as handle:
            handle.write(new_text)
        print(f"{SCRIPT_NAME}: apendice gerado em {path}")


def cmd_verificar(paths):
    bad = 0
    for path in paths:
        text = read(path)
        try:
            expected = build(text, emendas_for(path))
        except EmendaError as exc:
            print(f"{SCRIPT_NAME}: {path}: {exc}", file=sys.stderr)
            bad += 1
            continue
        if expected != text:
            print(f"{SCRIPT_NAME}: {path}: o apendice difere da saida do script", file=sys.stderr)
            bad += 1
    print(f"{SCRIPT_NAME}: analisados {len(paths)}, reprovados {bad}")
    if bad or not paths:
        sys.exit(1)


# --- selftest -------------------------------------------------------------

SYNTH = (
    "# Parecer\n\n## 1. Veredito\n\nok\n\n## 2. O texto congelado\n\n"
    "### `a.hpp`\n\n````cpp\nint velho = 1;\nint outro = 2;\n````\n\n"
    "## 3. Regras\n\nfim\n"
)
SYNTH_EMENDAS = [("T1", "int velho = 1;\n", "int novo = 1;\n")]


def expect_error(label, call):
    try:
        call()
    except EmendaError:
        return True
    print(f"selftest: controle {label} FALHOU (deveria ter reprovado)", file=sys.stderr)
    return False


def selftest_positive():
    out = build(SYNTH, SYNTH_EMENDAS)
    _body, appendix = split_appendix(out)
    ok = "int novo = 1;" in appendix and "int velho" not in appendix and "int outro = 2;" in appendix
    if not ok:
        print("selftest: controle POSITIVO FALHOU (emenda nao aplicada)", file=sys.stderr)
    return ok


def selftest_absent_and_ambiguous():
    absent = [("T2", "int inexistente;\n", "x\n")]
    twice = SYNTH.replace("int outro = 2;\n", "int velho = 1;\n")
    return all(
        (
            expect_error("NEGATIVO-AUSENTE", lambda: build(SYNTH, absent)),
            expect_error("NEGATIVO-AMBIGUA", lambda: build(twice, SYNTH_EMENDAS)),
            expect_error("NEGATIVO-SEM-CERCA", lambda: build("# x\n\n## 2. a\n\n## 3. b\n", SYNTH_EMENDAS)),
        )
    )


def selftest_body_untouched_and_idempotent():
    once = build(SYNTH, SYNTH_EMENDAS)
    twice = build(once, SYNTH_EMENDAS)
    body, _appendix = split_appendix(once)
    ok = body == SYNTH and once == twice
    if not ok:
        print("selftest: controle CORPO-INTACTO/IDEMPOTENTE FALHOU", file=sys.stderr)
    return ok


def selftest_verificar_catches_hand_edit():
    # O que verificar faz: regenerar e comparar. Apendice intacto bate;
    # apendice editado a mao nao bate.
    good = build(SYNTH, SYNTH_EMENDAS)
    edited = good.replace("int novo = 1;", "int novo = 999;")
    ok = good == build(good, SYNTH_EMENDAS) and edited != build(edited, SYNTH_EMENDAS)
    if not ok:
        print("selftest: controle EDICAO-A-MAO FALHOU", file=sys.stderr)
    return ok


def run_selftest():
    results = [
        selftest_positive(),
        selftest_absent_and_ambiguous(),
        selftest_body_untouched_and_idempotent(),
        selftest_verificar_catches_hand_edit(),
    ]
    print(f"selftest: controles {sum(results)}/{len(results)}")
    if not all(results):
        sys.exit(1)
    print(f"{SCRIPT_NAME}: selftest OK")


def main(argv):
    if argv[1:] == ["--selftest"]:
        run_selftest()
        return
    if len(argv) >= 3 and argv[1] == "gerar":
        cmd_gerar(argv[2:])
        return
    if len(argv) >= 3 and argv[1] == "verificar":
        cmd_verificar(argv[2:])
        return
    fail("uso: gerar|verificar <parecer.md>... | --selftest")


if __name__ == "__main__":
    main(sys.argv)
