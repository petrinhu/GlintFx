<!-- Origem: /var/tmp/cto-w8/s0/auditoria-api-shapes.md, md5 2fb68acc39757cce3053f9f7e9c568e8. Texto do parecer com as errata aplicadas no próprio texto (a errata vence o parecer). Ver "Errata aplicadas" logo abaixo. -->

# Auditoria de API: R2D-SHAPES (S0), formas 2D e relatório do quadro

**Revisor:** revisor de API da S0 (distinto do CTO e de qualquer implementador da W8, L-12).
**Natureza:** DESENHO. Aceito pelo CTO com as errata abaixo. Esta cópia em `docs/` é o texto que a S1 copia para o repositório.
**Árvore revisada:** `66a823e1072cbad1ac962e8d3f2069a8274a0ac8` (ramo `onda-w8`), clone `/var/tmp/cto-w8/s0/fixture`.
**Carimbo de início:** [11/10/26 - 11:41:40]

**Errata aplicadas (CTO; r2 §12 e r7 D-W8-334):**
- **D-W8-281 (DIV-01):** o alinhamento fica fora de `gltfx_stroke`, padrão `centered`. Desde a D-W8-354 ele vive em `gltfx_rect_outline_style`, lido por `stroke_rect`.
- **D-W8-282 (DIV-02):** dois nomes, `fill_triangles(vertices, layer)` e `fill_indexed_triangles(vertices, indices, layer)`. Condição: a sonda de colisão roda de novo com `fill_indexed_triangles` no passo 0 da S1.
- **D-W8-283 (Q-01):** opção (a), quebra de layout pré-1.0 aceita e anunciada pela versão, com "recompilar obrigatório" na nota de versão. Decisão do LÍDER pela L-26 (`GODS_LAWS.md:699` e `:701`), aplicada pelo CTO (D-W8-356).
- **D-W8-284 (Q-02):** sem intervalo inventado. Valor válido é finito e maior que zero; o custo tem teto de 512 segmentos por elipse (D-W8-269).
- **D-W8-285 (Q-03):** valor de enum fora da faixa recusa a chamada inteira, com o motivo `unknown_option`.
- **D-W8-286 (Q-04):** `flat`, com `butt` (SVG e Canvas) citado como sinônimo.
- **D-W8-287 (IMP-03, IMP-04):** uma chamada é uma peça; casos de borda escritos nos cabeçalhos; novo motivo `too_many_vertices`.
- **D-W8-334 (errata do layout):** saem `image_without_owner` e `pieces_refused_image_without_owner`; `shadow_groups` vira `shadow_segments` (D-W8-322); `light_below_piece` não entra (D-W8-320). O motivo `image_without_owner` nasce com a primeira ponte de pintura (D-W8-336, R0).
- **D-W8-353 a 356 (r13):** nomes finais e assinaturas com quatro parâmetros ou menos. `stroke_line` e `stroke_rect` recebem `gltfx_segment_world`, `gltfx_line_style` e `gltfx_rect_outline_style` (D-W8-354), e a nova cerca `core/segment.hpp` entra na seção 2. Ver a nota D-W8-354 ao lado de cada assinatura trocada. As seções 3 a 7 são análise e citam, onde indicam, as assinaturas anteriores; o texto vigente é o da seção 2.
- **S2b-fix1 (CTO, revisão N1 de `00cdbb4e`):** (1) a cerca de `fill_triangles` trazia o texto da proposta ("a vertex whose position is not a finite number: not_a_number") e contradizia a decisão IMP-04 (c) desta mesma página; vale a decisão: NaN é `not_a_number` e infinito é `infinite`, em posição e em cor, na ordem do vocabulário, como o quadrado (`piece_refusal.hpp`). (2) O limite de vértices fica escrito por extenso: 2^32 vértices são aceitos (o índice de 32 bits endereça 0 a 2^32 - 1) e 2^32 + 1 são recusados. (3) Motivo novo `pixel_position_out_of_range = 8`, com o contador `pieces_refused_pixel_position_out_of_range` acrescentado logo depois de `pieces_refused_too_many_vertices` e antes de `texture_switches`: posição finita que a transformação do lote leva para fora da faixa que o programa de vértice escala sem estourar (achado I-7). A borda é `FLT_MAX / 2` menos 3 unidades na última casa (3 × 2^103; errata S2b-fix2: o texto dizia 4, e a constante dava 8), derivada da linha `a_position.x / u_viewport_pixels.x * 2.0 - 1.0` de `src/draw2d/embedded_program.cpp` (o `* 2.0` numa superfície de 1 pixel, com a divisão até 2,5 unidades fora, pelo GL_ARB_shader_precision); a derivação está em `src/draw2d/quad_vertices.cpp`. As regras da cerca passam a vir numeradas, na ordem em que se aplicam. A cerca e `include/glintfx/draw2d/renderer_2d.hpp` continuam iguais byte a byte.
- **Errata da S3 (CTO, 12/10/2026; uma cerca e uma fonte):** a cerca de `stroke_line` passa a ser o texto que a S3 publica: o parágrafo da cerca, sem mudar uma linha, mais o que a D-W8-270, a D-W8-285, a D-W8-287 e a S2b-fix1 já tinham decidido e o cabeçalho tem de dizer (uma chamada é uma peça; espessura zero, ou comprimento zero com `flat`, é peça de área zero, desenhada, como o quadrilátero de área zero; o quadrado do comprimento zero alinhado aos eixos do espaço da unidade; a ponta redonda a no máximo 0,25 pixel do meio círculo verdadeiro; as quatro regras de recusa, na ordem do vocabulário) e os dois `Proved by`; a assinatura em duas linhas, como o `clang-format` do projeto a escreve. A cerca nova `core/segment.hpp` entra na S3 como está. A ponta redonda usa o erro padrão de 0,25 pixel (D-W8-07) como constante interna; se ela passa a ler `ellipse_max_error_pixels` é decisão da S5, que cria o campo.
- **Errata da S3-fix1 (CTO, 12/10/2026; achado I-1 da revisão N1 da S3):** a cerca de `stroke_line` prometia a ponta redonda a no máximo 0,25 pixel do meio círculo verdadeiro em qualquer raio, mas o limite de 512 segmentos da D-W8-269 vale a partir de um raio de 13280 pixels na tela (a flecha de 256 segmentos por metade chega a 0,94 pixel em 50000). A cerca e o cabeçalho passam a dizer a GARANTIA e o raio até onde ela vale (13280 pixels na tela, o maior raio em que 512 segmentos ficam dentro de 0,25 pixel: `0.25 / (1 - cos(pi/512))` = 13280,41, conferido por `stroke_line_pixels_test` a partir das duas constantes do código), e que acima dele a precisão não é garantida; o teto de segmentos e o exemplo numérico ficam só no comentário interno de `stroke_line_pixels.hpp` (troca da linha "circle on the screen. Refused, ...", conferida igual nos dois). O conserto completo, a tesselação limitada à área visível que o Skia faz (`PreChopPathCurves`: curvas e pedaços fora da área visível viram retas, e o resto fica a 1/4 de pixel), é do átomo de arco que a S5 reusa, e fica como linha da tabela (`ARC-VIEWPORT-TESSELLATION`). Porta de mão única: vai à ratificação do líder.
- **Errata da S4 (CTO, 12/10/2026; uma cerca e uma fonte):** a cerca de `stroke_rect` passa a ser o texto que a S4 publica: o parágrafo da cerca, sem mudar uma linha, mais o que a D-W8-271, a D-W8-285 e a D-W8-287 já tinham decidido e o cabeçalho tem de dizer (uma chamada é uma peça; cantos retos, cada lado encontrando o seguinte em ângulo reto; o limiar do furo, corrigido na S4-fix1 (errata abaixo); retângulo sem largura ou sem altura, sem buraco, com o resultado dito por alinhamento (centrado, a barra da espessura em volta do segmento; dentro, peça de área zero, desenhada; fora, o retângulo crescido, cheio), porque as fontes divergem aqui (o Skia desenha a barra, o SVG não desenha nada, o canvas desenha uma linha); espessura zero é peça de área zero, desenhada; as quatro regras de recusa, na ordem do vocabulário) e os três `Proved by`; a assinatura em duas linhas, como o `clang-format` do projeto a escreve. A junção, quando entrar, é do estilo do traço e vale para toda forma, como no canvas, no Skia, no Cairo, no NanoVG e no SVG: vai em `gltfx_stroke`, ao lado da ponta, e o `stroke_rect` a lê de lá, com o padrão de cantos retos (décima consulta ao advisor; linha R2D-STROKE-PATH).
- **Errata da S4-fix1 (CTO, 13/10/2026; achado IMPORTANTE-1 da revisão N1 da S4):** a cerca de `stroke_rect` dava um limiar de furo que só valia para o alinhamento dentro, sem dizer isso. A regra do código (`stroke_rect_geometry.hpp`, THE HOLE) é por lado: o furo some quando a parte do contorno que cresce para DENTRO alcança o meio do lado menor (os lados medidos na unidade da espessura: no mundo, ou na tela com a unidade pixel). Centrado, o padrão, isso é espessura de pelo menos o lado menor inteiro; dentro, de pelo menos a metade; fora, nunca (só o retângulo sem largura ou sem altura, que a frase seguinte da cerca já trata). Conta do revisor: retângulo 40 x 20, centrado, espessura 10 (a metade da altura) ainda tem furo de altura 10. A cerca e o cabeçalho passam a dizer o limiar por alinhamento, e a tabela do furo ganha uma célula de cada lado de cada limiar que faltava (centrado 10 e 20, fora 10). Promessa pública: vai à ratificação do líder como errata.
- **Errata da S5-C1 (CTO, 20/10/2026; D-W8-565 a D-W8-570; duas cercas e o campo):** as cercas de `fill_ellipse` e `stroke_ellipse` passam a ser o texto que a S5-C1 publica, e a do campo do descritor também. Três divergências do texto da S0 com decisões já tomadas e com o código ficam corrigidas: (1) raio zero NÃO é "nada, não contado": é peça de área zero, desenhada e contada, e o contorno é o contínuo (disco ou barra de pontas redondas), pela D-W8-414 (desempate do advisor de 14/10, na lista de ratificação do líder) e por `src/draw2d/ellipse_check.hpp`; (2) o teto de custo não é mais "512 segmentos, 1e-12 dá 512": desde a S5-A são até 1024 segmentos por chamada no preenchimento e até 2048 pontos em cada borda do contorno, e a garantia de distância tem as duas condições da D-W8-398 (o nível cabe nos 1024 segmentos, raio na tela até 2^40), ditas no texto público; (3) `ellipse_tessellation_test` não existe: os `Proved by:` citam os testes que existem. **O campo muda de nome, de `ellipse_max_error_pixels` para `curve_max_error_pixels` (D-W8-568),** porque a ponta redonda de `stroke_line` passa a lê-lo na S5-C2 (Q4, D-W8-417) e as curvas de `R2D-STROKE-PATH` e `R2D-CORNER-SHAPE` vêm depois: o nome antigo mentiria para eles. O campo nunca entrou no repositório antes desta errata (custo de ABI zero). `open()` o recusa logo depois do contexto, antes de qualquer coisa ser feita, com `invalid_argument` e `rejected_value()` "curve_max_error_pixels" (D-W8-566). Porta de mão única: o nome e as frases vão ao advisor e à ratificação do líder.
- **Errata da S5-C2 (CTO, 22/10/2026; D-W8-830 a D-W8-840; a cerca de `stroke_line` e o campo):** a ponta redonda de `stroke_line` passa a ler `curve_max_error_pixels` (Q4, D-W8-572) e a ser cortada por `arc_parameters`, como a elipse: cada ponta é meia elipse na tela, com os seus próprios parâmetros, e a garantia de distância é a de `fill_ellipse`, com as mesmas duas condições (o nível que a distância pede cabe nos 1024 segmentos de uma ponta; o raio maior na tela até 2^40 pixels). Sai do texto público a condição "while its radius there is at most 13280 pixels; above that, the distance is not guaranteed", que vinha do teto de 512 segmentos da D-W8-269 (errata da S3-fix1, acima), e o comentário do campo deixa de dizer que a ponta "does not read it yet". A recusa 4 de `stroke_line` ganha a ponta cujo raio maior na tela passa o maior double (D-W8-833), que antes era recusada pelos pontos fora da faixa e que, sem esta regra, sairia sem a ponta. Os `Proved by:` citam `stroke_line_pixels_test`, `renderer_2d_impl_test` e `arc_parameters_test`. Fecha o texto de `ARC-VIEWPORT-TESSELLATION`.
- **Errata da S5-C2-fix1 (CTO, 25/10/2026; D-W8-1050 a D-W8-1059; os `Proved by:` da cerca de `stroke_line`):** a revisão N1 da S5-C2 achou a frase da ponta redonda provada, na meia extensão, num raio só (50000 pixels), e a varredura de `arc_parameters_test` desenhando só curvas inteiras (I1). O texto da garantia não muda; os dois `Proved by:` passam a citar o que a fix1 acrescenta: `arc_parameters_test` varre 400 meias curvas, de raio 1 a 2^40 pixels, com eixos que não precisam ser perpendiculares e distância de 1/16 a 8 pixels; `stroke_line_pixels_test` varre 400 linhas com uma ponta sobre a superfície, nas duas unidades, sob transformação girada e escalada de forma desigual, de raio 1 a 2^20 pixels, conferindo os vértices em precisão simples contra a meia elipse verdadeira, e ganha a ponta logo fora da superfície, cortada sem alcance, e a contagem da ponta abaixo do corte conferida contra a fórmula de antes da S5-C2. Declarado: com vértices em precisão simples a varredura foi até 2^20 pixels de raio, numa superfície de 800 por 600 pixels. (Corrigido no lugar pela errata da CURVE-FLOAT-NARROW, revisão N1 da S5-C2-fix1, I-2: a frase dava como causa o raio; a variável é a coordenada dos vértices, o tamanho da superfície junto com o raio: a sonda do CTO mediu +0,00064 pixel além da distância numa superfície de 16384 com raio de 5e4, e até 0,08 pixel em 2^40.) O assunto toca também `fill_ellipse` e `stroke_ellipse` e foi encaminhado ao main como item novo, fora desta errata.
- **Errata da CURVE-FLOAT-NARROW (CTO, 25/10/2026; D-W8-1065 a D-W8-1069; as cercas de `fill_ellipse`, `stroke_ellipse` e `stroke_line`):** a sonda da pré-execução da S5-C2-fix1 mediu, na biblioteca real, que o arredondamento para precisão simples das pontas dos trechos que cruzam a superfície levava a borda além de `curve_max_error_pixels` quando o raio é grande (+0,083 pixel em 2^40, 23% de uma distância de 0,019), e que, numa superfície de 16384 pixels, um resto de cerca de 1/1000 de pixel existe em qualquer raio, porque todo vértice é precisão simples. As três cercas prometiam a distância sem esse resto. (1) O corte passa a terminar todo trecho que toca a superfície a no máximo 1024 pixels dela (ou 652 vezes a distância, quando é mais; o regime uniforme já ficava nisso), tentado primeiro no nível que a distância pede e, quando não cabe, o corte de antes, sem nada a mais. (2) O texto público passa a dizer o resto: a borda sobre a superfície fica a no máximo a distância pedida mais o arredondamento dos cantos, que move um canto em até 2^-24 da distância dele à origem da tela, com os números de exemplo (menos de 1/4000 de pixel em 1920 por 1080 e de 1/600 em 16384 por 16384, na distância padrão), e o caso em que os 1024 trechos dão para a distância mas não para trazer as pontas para perto. (3) As três frases começam por "On the surface" (o C1 da revisão N1 da S5-C2, adiado pela D-W8-1056 para entrar aqui). (4) Os `Proved by:` citam as varreduras novas: as pontas perto em `arc_parameters_test` (400 curvas inteiras e meias, raio 1 a 2^39,5, alcance até 4096), a borda do preenchimento em precisão simples em `ellipse_pixels_test` (400 elipses, raio 512 a 2^39,5) e a ponta redonda em `stroke_line_pixels_test` estendida de 2^20 a 2^40; as superfícies provadas ficam ditas em cada `Proved by:` (800 por 600 nas duas varreduras em precisão simples; 1920 por 1080 e 16384 por 16384 na do corte), e o maior raio sorteado por cada varredura é conferido (o C-2 da revisão N1 da S5-C2-fix1: o alcance era só do gerador); a citação de `arc_parameters_test` em `stroke_line` diz que a meia curva é provada nos parâmetros, em double (o I-1 da mesma revisão). A borda do contorno de `stroke_ellipse` é provada no corte compartilhado (as pontas perto com o alcance da espessura), sem varredura própria dos vértices em precisão simples: rebaixamento declarado. O limite de 2^40 não muda (D-W8-378). Porta de mão única: o texto público vai à ratificação do líder.
- **Errata da CURVE-FLOAT-NARROW-fix2 (CTO, 26/10/2026; D-W8-1090 a D-W8-1096; as cercas de `fill_ellipse`, `stroke_ellipse` e `stroke_line`; revisão N1 da CURVE-FLOAT-NARROW, NÃO LIMPA):** nenhum comportamento muda; o texto passa a dizer o que o código faz e o que o teste mede. (1) A distância "1024 pixels" é medida da FAIXA, a superfície crescida por um pixel (e, no contorno, pela meia espessura), como o código e a varredura sempre mediram; o texto dizia "dela", da superfície, e a ponta chegava a 1024,78 pixels da superfície (I-1). Os números 1/4000 e 1/600 já eram os da faixa (1/4635 e 1/681). (2) A ressalva do corte só pela distância entra na mesma frase da garantia (I-2). Medido pelo CTO: das curvas que a revisão contou como "reserva" (12 e 8 em 4000), nenhuma era desse ramo; eram trechos do regime uniforme, dentro de 652 vezes a distância, que o texto já cobria; em 8000 curvas do gerador da varredura o ramo não aparece, e o único caso em que o corte com as pontas perto não cabe foi também um em que a distância não cabe. (3) Um caso fechado em `arc_parameters_test` prova o ramo, numa meia curva construída para ele (I-3). (4) Os cantos do contorno de `stroke_ellipse` em precisão simples não são varridos: o `Proved by:` passa a dizer isso, e o arredondamento de um canto fica preso pelos dois lados na conversão única da biblioteca, por um caso novo em `ellipse_pixels_test` (I-4 e o mutante R5 da revisão). (5) Os geradores da meia curva e das pontas perto passam a sortear o RAIO MAIOR, a grandeza que o texto nomeia, de 1 a 2^40, e conferem que passa de 2^39,5; antes sorteavam o eixo até 2^39,5 e o texto da meia curva dizia 2^40 (I-5). A varredura do preenchimento mede o raio maior na elipse verdadeira (de 64 a 2^39,5), não o limite do sorteio, e conta os vértices do aro que não caem na elipse (zero). (6) O oráculo da borda (distância à corda, casco, faixa, arredondamento permitido, raio maior) sai das três cópias para `tests/harness/curve_oracle.hpp` (I-6, regra de três da L-33 global). Porta de mão única: o texto público vai à ratificação do líder.
- **Achados de texto:** IMP-08 (`measured` sai de `fill_ellipse`) e COS-01 (`planned for fatia S3` sai de `stroke.hpp`) aplicados no texto da seção 2. IMP-02 (quatro sítios de contrato desatualizados) não está nos trechos da seção 2: é corrigido nos commits da S2b e da S5, como manda a r2 §12.

---

## 1. Veredito

**APROVADO COM CONDIÇÕES, como DESENHO.** Nada entra no repositório antes da DEMO-1 fechar (D-W8-251).

- **Condição 1, feita neste parecer:** o enumerador `round` colide com macro do sistema (`tgmath.h:972`). Corrigido para `rounded`. A sonda reprovou o texto antigo e aprovou o novo (seção 6).
- **Condição 2, decisão do CTO:** a sobrecarga `fill_triangles(v, {})` é ambígua em `g++` e `clang++` (FATO, IMP-06). Opções na seção 7, DIV-02.
- **Condição 3, decisão do líder (L-01):** a mudança de layout de `gltfx_renderer_2d_desc` e de `gltfx_frame_2d_report` quebra consumidor que não recompila (IMP-07), e isso pede decisão de ABI pré-1.0 (seção 7, Q-01).
- **Condição 4, contrato a fechar antes da S1:** oito achados IMPORTANTES (seção 5), entre eles a definição de "peça", os casos de borda e o intervalo da tolerância.

Contagem: CRÍTICO 1 (corrigido aqui), IMPORTANTE 8, COSMÉTICO 3. Nada na proposta foi decidido pelo revisor: as divergências com o plano estão na seção 7.

---

## 2. Texto proposto

Os cabeçalhos novos e os trechos dos existentes, como ficam na cópia de sonda `/var/tmp/cto-w8/s0/fixture`. Nada disto está no repositório.

### `include/glintfx/draw2d/vertex_2d.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>

// draw2d/vertex_2d.hpp - R2D-SHAPES (docs/auditoria-api-draw2d.md, section 9).
// The PUBLIC vertex of fill_triangles(). The 32-byte internal vertex the
// renderer uploads to the graphics card stays internal: it is not part of
// this header and never will be (GODS_LAWS.md L-19).
//
// Position: WORLD space, double precision, like every world position in the
// library. Colour: linear light with straight alpha, like every gltfx_rgba.
// The library premultiplies it itself before upload.
//
// Aggregate, trivially copyable; the layout IS the contract (GODS_LAWS.md L-19, L-26).
// It is NOT trivially default-constructible: position and color carry {} (errata E1, D-W8-366).

namespace glintfx {

struct gltfx_vertex_2d_world {
    gltfx_vec2_world position{};
    gltfx_rgba color{};
};

} // namespace glintfx
```

### `include/glintfx/draw2d/stroke.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// draw2d/stroke.hpp - R2D-SHAPES (docs/auditoria-api-draw2d.md, section 9;
// decisions D-W8-05 and D-W8-08 of the W8 plan).
//
// THE THICKNESS UNIT is explicit (D-W8-05). World is the default: a line
// drawn with thickness 2.0 in world units scales with the batch transform,
// the way raylib, Cairo and the browser canvas do. Pixel keeps the thickness
// on the screen whatever the zoom, the way Dear ImGui does; the library
// expands a pixel-unit stroke AFTER the batch transform is applied.
//
// THE LINE CAP is a value, never a sign: `flat` ends the line at its exact
// end point (the default; `butt` in SVG and in the browser canvas), `square`
// extends it by half the thickness, and `rounded` closes it with a half-disc.
// Append-only: a value is never renumbered and never reused.
//
// A VALUE OUTSIDE THESE ENUMERATIONS (a consumer's static_cast) refuses the
// whole call, counted as `unknown_option` in the frame report (D-W8-285).
// It never falls back to a default silently.
//
// THE RECTANGLE ALIGNMENT lives in gltfx_rect_outline_style, read by
// stroke_rect(), not here: lines and ellipses have no inside or outside, and a
// field that a method ignores is a lie about what that method does.

namespace glintfx {

enum class gltfx_stroke_unit : std::uint8_t {
    world = 0,
    pixel = 1,
};

enum class gltfx_line_cap : std::uint8_t {
    flat = 0,
    square = 1,
    rounded = 2,
};

enum class gltfx_stroke_align : std::uint8_t {
    centered = 0,
    inside = 1,
    outside = 2,
};

struct gltfx_stroke {
    // Zero draws nothing and covers no pixel. A negative value is refused
    // and counted in the frame report (refusal `negative_size`).
    double thickness = 1.0;
    gltfx_stroke_unit unit = gltfx_stroke_unit::world;
};

// The style of a line (D-W8-354): the thickness and the end cap. Grouped so
// that stroke_line() keeps within four parameters (CONTRACT.md §6.2).
struct gltfx_line_style {
    gltfx_stroke stroke{};
    gltfx_line_cap cap = gltfx_line_cap::flat;
};

// The style of a rectangle outline (D-W8-354): the thickness and the
// alignment on the edge. The alignment belongs to the rectangle, and not to
// gltfx_stroke, because lines and ellipses ignore it.
struct gltfx_rect_outline_style {
    gltfx_stroke stroke{};
    gltfx_stroke_align align = gltfx_stroke_align::centered;
};

} // namespace glintfx
```

### `include/glintfx/core/segment.hpp` (novo, D-W8-354)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/vec2.hpp>

// core/segment.hpp - R2D-SHAPES. A straight segment in WORLD space: its two
// ends. A value type, like gltfx_rect_world: the layout is the contract
// (GODS_LAWS.md L-19, L-26). Pure geometry, no drawing, no GL, no operating
// system.

namespace glintfx {

struct gltfx_segment_world {
    gltfx_vec2_world start;
    gltfx_vec2_world end;
};

} // namespace glintfx
```

### `include/glintfx/core/ellipse.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/angle.hpp>
#include <glintfx/core/vec2.hpp>

// core/ellipse.hpp - R2D-SHAPES (docs/auditoria-api-draw2d.md, section 9;
// decision D-W8-07 of the W8 plan). An ellipse in WORLD space: centre,
// two radii, and a rotation about the centre. A circle is an ellipse whose
// two radii are equal; there is no second type for it (GODS_LAWS.md L-17).
//
// The radii are NAMED (radius_x, radius_y), not a pair: the reader sees which
// one is horizontal before the rotation is applied. The rotation is
// gltfx_angle, never a bare double, so degrees cannot pass for radians
// (GODS_LAWS.md L-19).
//
// Pure geometry, like core/quad.hpp: no drawing, no GL, no operating system.

namespace glintfx {

struct gltfx_ellipse_world {
    gltfx_vec2_world center{};
    double radius_x = 0.0;
    double radius_y = 0.0;
    gltfx_angle rotation{};
};

} // namespace glintfx
```

### `include/glintfx/draw2d/renderer_2d.hpp` (trecho: 5 métodos, dentro da classe `gltfx_renderer_2d`, antes de `private:`, mais dois includes novos)

```cpp
    // Filled triangles, each corner with its own colour. `vertices` is read
    // only during this call (docs/api-conventions.md R5); the library copies
    // what it needs. `fill_triangles` reads the vertices in order, three at a
    // time. `fill_indexed_triangles` reads each three indices as three
    // vertices of the span. Two names, not one overload with an empty span:
    // `fill_triangles(v, {})` is ambiguous in g++ and clang++ (D-W8-282).
    //
    // One call is one piece, whatever the method (D-W8-287). A refused call is
    // refused WHOLE and counted once, never triangle by triangle. An empty
    // call (no vertices, or no indices) does nothing and is not a piece.
    // Refused, counted in the report, never an exception. The first rule
    // that applies names the reason:
    //   1. more than 2^32 vertices, which a 32-bit index cannot address:
    //      too_many_vertices;
    //   2. a vertex count (no indices) or an index count that is not a
    //      multiple of three: count_not_multiple_of_three;
    //   3. an index that is not a vertex of this span: index_out_of_range;
    //   4. a NaN in any position or colour: not_a_number; then an
    //      infinity in any of them: infinite;
    //   5. a position that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the
    //      vertex program would overflow: pixel_position_out_of_range.
    GLINTFX_API void fill_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                    gltfx_draw_layer layer = {}) noexcept;
    GLINTFX_API void fill_indexed_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                            std::span<const std::uint32_t> indices,
                                            gltfx_draw_layer layer = {}) noexcept;

    // A straight line from `segment.start` to `segment.end`, `style.stroke.thickness`
    // wide, with the end caps `style.cap`. The thickness is in the unit
    // `style.stroke.unit` (stroke.hpp): world by default, pixel on request. A
    // zero-length line (start equals end) draws the cap alone: nothing for `flat`,
    // a square for `square`, a disc for `rounded`, as SVG 2 defines it (D-W8-270).
    // D-W8-354: segment, style and colour are grouped; four parameters at most.
    //
    // One call is one piece. A line of zero thickness, or of zero length with
    // `flat`, is a piece of zero area: drawn, covering no pixel. The square of
    // a zero-length line is aligned with the axes of the space of its unit: the
    // world's (turned and scaled with the batch) or the screen's. A rounded cap
    // is half a disc of radius half the thickness: on the screen, half an
    // ellipse when a world-unit line is scaled unevenly. On the surface, its
    // edge is made of straight segments at most `curve_max_error_pixels`
    // (renderer_2d_desc.hpp, default 0.25) away from the true curve on the
    // screen, plus the rounding of their corners that fill_ellipse() states,
    // under the two conditions of fill_ellipse(): its larger radius on the
    // screen is at most 2^40 pixels, and the 1024 segments one cap may use
    // are enough for that distance. The part of a cap that lies off the
    // surface may be drawn coarser; no pixel on the surface changes for that.
    // Refused, counted in the report, never an exception.
    // The first rule that applies names the reason:
    //   1. a NaN in the start, the end, the thickness or the colour:
    //      not_a_number; then an infinity in any of them: infinite;
    //   2. a negative thickness: negative_size;
    //   3. a thickness unit or a cap outside its enumeration: unknown_option;
    //   4. a point of the outline that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow, or a rounded cap whose larger radius on the
    //      screen passes the largest double: pixel_position_out_of_range.
    // Proved by: stroke_geometry_test (the outline of each cap and unit).
    // Proved by: stroke_line_check_test (the order of the refusals).
    // Proved by: stroke_line_pixels_test (caps in both units, under a
    // turned and unevenly scaled transform, of radius 1 to 2^40 pixels on
    // the screen and distance 1/16 to 8 pixels, swept on a surface of 800 by
    // 600 pixels with their float corners within the distance plus the
    // rounding, and the ends of every segment on it within their reach of the
    // band, the largest larger radius checked to pass 2^39; a cap of radius 50000
    // pixels; the distance read by the
    // cap; a cap just off the surface cut with no reach; the cap that cannot
    // be measured, refused).
    // Proved by: renderer_2d_impl_test (the distance given to open() sets the
    // segments of a cap).
    // Proved by: arc_parameters_test (the two conditions of the distance; the
    // half curve of a cap swept with a larger radius of 1 to 2^40 pixels, the
    // largest checked to pass 2^39.5, and distance 1/16 to 8 pixels, on
    // surfaces of 1920 by 1080 and 16384 by 16384 pixels, on its parameters,
    // in double, before any rounding to single precision; the coarser edge
    // off the surface).
    GLINTFX_API void stroke_line(gltfx_segment_world segment, gltfx_line_style style,
                                 gltfx_rgba color, gltfx_draw_layer layer = {}) noexcept;

    // The outline of an axis-aligned rectangle, grown by `style.align`: centered
    // on the edge (the default), inside the rectangle, or outside it. The
    // four sides do not overlap at the corners, so a half-transparent
    // outline has no darker corner. D-W8-354: four parameters at most.
    //
    // One call is one piece. The corners are square: each side meets the next
    // at a right angle. The thickness is in the unit `style.stroke.unit`
    // (stroke.hpp): in world unit the outline is the world outline carried by
    // the batch transform; in pixel unit it keeps its thickness on the screen
    // at every zoom. When the part of the outline that grows inward reaches
    // the middle of the rectangle's shorter side (the sides measured in the
    // unit of the thickness), there is no hole: the outline fills its outer
    // rectangle. Centered, that is a thickness of the whole shorter side or
    // more; inside, of half of it or more; outside, never. A rectangle
    // with no width or no height has no hole either, and the outline is, by
    // alignment: centered, the bar of the thickness around the segment;
    // inside, a piece of zero area, drawn, covering no pixel; outside, the
    // rectangle grown by the thickness, filled. A thickness of zero is a piece
    // of zero area: drawn, covering no pixel. Refused, counted in the report,
    // never an exception. The first rule that applies names the reason:
    //   1. a NaN in the corner, the size, the thickness or the colour:
    //      not_a_number; then an infinity in any of them: infinite;
    //   2. a negative width, height or thickness: negative_size;
    //   3. a thickness unit or an alignment outside its enumeration:
    //      unknown_option;
    //   4. a point of the outline that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow: pixel_position_out_of_range.
    // Proved by: stroke_rect_geometry_test (the outline of each alignment and unit).
    // Proved by: stroke_rect_check_test (the order of the refusals).
    // Proved by: draw2d_shapes_parity_test (the half-transparent corner).
    GLINTFX_API void stroke_rect(gltfx_rect_world rect, gltfx_rect_outline_style style,
                                 gltfx_rgba color, gltfx_draw_layer layer = {}) noexcept;

    // A filled ellipse (core/ellipse.hpp; a circle when its two radii are
    // equal): `ellipse.radius_x` and `ellipse.radius_y` turned by
    // `ellipse.rotation` about `ellipse.center`, in the world, then placed by
    // the batch transform like every other piece. D-W8-354: four parameters
    // at most.
    //
    // One call is one piece, painted as a fan of triangles from the centre.
    // On the surface, its edge is made of straight segments at most
    // `curve_max_error_pixels` (renderer_2d_desc.hpp, default 0.25) away from
    // the true edge on the screen, plus the rounding of their corners, while
    // two conditions hold: the larger radius on the screen is at most 2^40
    // pixels, and the 1024 segments one call may use are enough for that
    // distance. A very small distance on a very large surface can need more,
    // and then the distance drawn is larger than asked for. The part of the
    // edge that lies off the surface may be drawn coarser; no pixel on the
    // surface changes for that. THE ROUNDING OF THE CORNERS: every position
    // becomes single precision last (PRECISION, above), and rounding moves a
    // corner by at most 2^-24 of its distance from the origin of the screen,
    // or by 2^-149 pixel, whichever is more.
    // So the library ends every segment that reaches the surface within 1024
    // pixels of the BAND, the surface grown by one pixel on every side (or
    // within 652 times the distance, when that is more), unless the 1024
    // segments are enough for the distance but not for ending them that
    // near: then the segments are cut for the distance alone, and their
    // corners are rounded where they lie. Where the ends lie that near, the
    // rounding added to the distance is at most 2^-24 of the distance from
    // the origin of the farthest point the band and that reach allow: with
    // the default distance, less than 1/4000 of a pixel on a surface of 1920
    // by 1080 pixels, and less than 1/600 on one of 16384 by 16384. A radius
    // of zero is not refused: with one or
    // both radii zero, the filled ellipse is a piece of zero area, drawn,
    // covering no pixel. Refused, counted in the report, never an exception.
    // The first rule that applies names the reason:
    //   1. a NaN in the centre, a radius, the rotation or the colour:
    //      not_a_number; then an infinity in any of them: infinite;
    //   2. a radius below zero (-0.0 is zero): negative_size;
    //   3. a point of the edge that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow: pixel_position_out_of_range.
    // Proved by: ellipse_check_test (the order of the refusals).
    // Proved by: ellipse_pixels_test (the edge within the distance on the
    // surface; 400 ellipses of a larger radius of 64 to 2^39.5 pixels on the
    // screen, under a turned and unevenly scaled transform, at distances of
    // 1/16 to 8 pixels, swept on a surface of 800 by 600 pixels with their
    // float corners within the distance plus the rounding, and the ends of
    // every segment on it within their reach of the band, the largest larger
    // radius, measured on the true ellipse, checked to pass 2^39; the
    // rounding of a corner by the library's one narrowing, at most 2^-24 of
    // its distance from the origin, and all of it at a tie).
    // Proved by: arc_parameters_test (the two conditions of the distance; the
    // coarser edge off the surface; the ends of every segment on the surface
    // within their reach of the band, swept on 400 whole and half curves of a
    // larger radius of 1 to 2^40 pixels, the largest checked to pass 2^39.5,
    // at distances of 1/16 to 8 pixels, with reaches up to 4096 pixels, on
    // surfaces of 1920 by 1080 and 16384 by 16384 pixels; the segments cut
    // for the distance alone when the 1024 are too few to end them near, on
    // a curve built for it).
    // Proved by: renderer_2d_impl_test (one call is one piece; the distance
    // given to open() sets the segments).
    // Proved by: draw2d_shapes_parity_test (a filled circle read back as
    // pixels; a distance of 8 pixels draws the circle as a pentagon).
    GLINTFX_API void fill_ellipse(gltfx_ellipse_world ellipse, gltfx_rgba color,
                                  gltfx_draw_layer layer = {}) noexcept;

    // The outline of an ellipse, `stroke.thickness` wide, centred on its
    // curve: half the thickness on each side. There is no alignment for an
    // ellipse (D-W8-281). D-W8-354: four parameters at most.
    //
    // One call is one piece. The thickness is in the unit `stroke.unit`
    // (stroke.hpp): in world unit the outline is the world outline carried by
    // the batch transform (a non-uniform scale makes it thicker on one axis);
    // in pixel unit it is built on the screen, half the thickness away from
    // the screen ellipse across its curve, at every zoom. On the surface, its
    // two edges are within `curve_max_error_pixels` of their true curves,
    // plus the rounding of their corners that fill_ellipse() states, under
    // the two conditions of fill_ellipse(). Its band is the surface grown by
    // one pixel and by the reach of half the thickness on the screen (half
    // the thickness in pixel unit; in world unit, half the thickness times
    // the largest stretch of the batch transform), and the corners lie that
    // reach or less off the curve, so their rounding is at most that of a
    // point twice the reach farther out. Where the curve bends
    // tighter than half the thickness, the inner edge is the true boundary of
    // the points farther than half the thickness from the curve, so it never
    // folds over itself, and a half-transparent outline covers each pixel
    // once. When half the thickness reaches the shorter half axis of the
    // ellipse (measured in the unit of the thickness), there is no hole: the
    // outline fills its outer edge. A radius of zero is not refused: the
    // outline is the one the same thickness gives every other ellipse,
    // continued (a disc of radius half the thickness when both radii are
    // zero, a bar with rounded ends when one is). A thickness of zero is a
    // piece of zero area: drawn, covering no pixel. Refused, counted in the
    // report, never an exception. The first rule that applies names the
    // reason:
    //   1. a NaN in the centre, a radius, the rotation, the thickness or the
    //      colour: not_a_number; then an infinity in any of them: infinite;
    //   2. a radius or a thickness below zero (-0.0 is zero): negative_size;
    //   3. a thickness unit outside its enumeration: unknown_option;
    //   4. a point of the outline that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow: pixel_position_out_of_range.
    // Proved by: ellipse_check_test (the order of the refusals).
    // Proved by: ellipse_outline_test (the inner edge within the true
    // boundary; the ring turning with the outline; no hole once half the
    // thickness reaches the shorter half axis).
    // Proved by: ellipse_pixels_test (the outline of each unit; the shapes of
    // radius zero).
    // Proved by: renderer_2d_impl_test (one call is one piece).
    // Proved by: arc_parameters_test (the ends of every segment of the curve
    // on the surface within their reach of the band, the half thickness
    // included, swept with reaches up to 4096 pixels, in double). The
    // corners of the outline in single precision are not swept: their
    // rounding is the one of the library's narrowing that
    // ellipse_pixels_test pins.
    // Proved by: draw2d_shapes_parity_test (a half-transparent outline of a
    // long ellipse, bent tighter than half its thickness, reads the same
    // value along its band and black in its hole).
    GLINTFX_API void stroke_ellipse(gltfx_ellipse_world ellipse, gltfx_stroke stroke,
                                    gltfx_rgba color, gltfx_draw_layer layer = {}) noexcept;

```

### `include/glintfx/draw2d/renderer_2d_desc.hpp` (trecho: campo novo, ao fim da struct)

```cpp

    // The largest distance, in PHYSICAL pixels of the surface, between a
    // true curve and the straight segments drawn in its place. Read by
    // fill_ellipse(), stroke_ellipse() and the rounded cap of stroke_line()
    // (renderer_2d.hpp). A method added later that draws a curve says in its
    // own comment whether it reads it.
    // Smaller is smoother and costs more triangles. Must be a finite number
    // greater than zero; anything else is refused by open() as
    // invalid_argument, rejected_value() "curve_max_error_pixels", before
    // anything is made. There is no other interval (D-W8-284). The cost has
    // a ceiling whatever the value: one ellipse is drawn with at most 1024
    // straight segments for its fill, and its outline with at most 2048
    // points on each edge; each rounded cap of a line with at most 1024
    // segments. A value so small that the ceiling is reached is
    // still drawn, with a distance larger than asked for (renderer_2d.hpp,
    // fill_ellipse(), says when the distance is kept). Default 0.25.
    double curve_max_error_pixels = 0.25;
```

### `include/glintfx/draw2d/frame_2d_report.hpp` (trecho D-W8-30: valores novos do enum, e contadores novos ao fim da struct)

```cpp
    // Appended in W8 (D-W8-30, errata D-W8-334 and D-W8-285, D-W8-287).
    // Triangle refusals and the new kinds of refusal share this one enum, so
    // first_refusal names any of them.
    index_out_of_range = 4,
    count_not_multiple_of_three = 5,
    unknown_option = 6,
    too_many_vertices = 7,
    pixel_position_out_of_range = 8, // errata S2b-fix1
// ... e ao fim de gltfx_frame_2d_report:
    // ---- W8 (D-W8-30): appended, never inserted. Each refusal reason has
    // its own counter; the counters of ALL reasons add up to pieces_refused.
    std::uint64_t pieces_refused_not_a_number = 0;
    std::uint64_t pieces_refused_infinite = 0;
    std::uint64_t pieces_refused_negative_size = 0;
    std::uint64_t pieces_refused_index_out_of_range = 0;
    std::uint64_t pieces_refused_count_not_multiple_of_three = 0;
    std::uint64_t pieces_refused_unknown_option = 0;
    std::uint64_t pieces_refused_too_many_vertices = 0;
    std::uint64_t pieces_refused_pixel_position_out_of_range = 0; // errata S2b-fix1
    // Times the bound texture changed between two draw calls (a cost, not a
    // refusal). Zero until the texture fatia (R2D-TEXTURE) starts counting.
    std::uint64_t texture_switches = 0;
    // Segments that had a shadow, counted at the barrier that closes each one
    // (R2D-SHADOW, D-W8-322). A segment without shadow does not count. Zero
    // until that fatia.
    std::uint64_t shadow_segments = 0;
```


---

## 3. R1 a R10 de `docs/api-conventions.md`, uma a uma

Cada regra: **confirmada** (o desenho proposto cumpre), **confirmada com condição** (cumpre se a S1-S5 fizerem X), ou **não se aplica**. Achado fica na seção 5.

- **R1 (retorno único `gltfx_rslt<T>`): confirmada com nota.** `open()` mantém `gltfx_rslt<gltfx_renderer_2d>` e ganha a recusa de `ellipse_max_error_pixels` (`invalid_argument`). Os cinco métodos de desenho retornam `void`: é a decisão 2 do líder (19/09), "a chamada de desenho não retorna nada, o problema sai no relatório", e não é contradição com R1, porque o problema de um desenho não é um erro da chamada. Nota: o cabeçalho de `renderer_2d.hpp` precisa dizer isso perto de cada método novo, senão o leitor compara com R1 e acha falta.
- **R2 (`[[nodiscard]]` estrutural): não se aplica.** Nenhum método novo devolve valor.
- **R3 (`noexcept` na borda, degradar em vez de lançar): confirmada com condição.** Todos os cinco métodos e os dois `fill_triangles` são `noexcept` no desenho. Condição para a S2a e a S2b: a cópia dos vértices e dos índices cresce o armazenamento, então a falta de memória tem de virar contagem (`pieces_dropped_out_of_memory`) pelo degrau 2 da R3 (captura e conversão), como `fill_rect` já faz, nunca por `bad_alloc` que escape. O gate `noexcept_alloc_test` precisa passar nesses arquivos.
- **R4 (campo ou código ausente/desconhecido nunca é comportamento indefinido): NÃO confirmada no texto proposto.** Veja o achado IMP-01 (valor de enum fora da faixa vindo do consumidor) e IMP-02 (lista de tokens desatualizada). O nome de refusal com valor novo precisa cair em `"unknown"` quando o build não o conhece, e o comentário do cabeçalho precisa listar os tokens novos.
- **R5 (alocação e liberação do mesmo lado): confirmada.** `std::span` é só leitura durante a chamada e não é guardado; o chamador não recebe nenhum contêiner de volta. `gltfx_vertex_2d_world` é agregado trivial (`vec2_world` mais `rgba`). Nota de ABI: `std::span` é template de cabeçalho, sem objeto próprio no `.so`; a assinatura exportada leva o nome mangled do `span`, o que vale só para o mesmo toolchain, como já vale para o resto da biblioteca.
- **R6 (nome público sem colisão com macro nem com nome da biblioteca padrão): NÃO confirmada no texto da rodada 1; confirmada no texto corrigido, com limite declarado.** Todo nome novo passou pela sonda da seção 6. A sonda NÃO cobre nome de PARÂMETRO (`check_public_name_collision.py:37-38`), e os parâmetros novos são `start`, `end`, `stroke`, `cap`, `align`, `unit`, `thickness`, `center`, `radius_x`, `radius_y`, `rotation`, `vertices`, `indices`, `ellipse`, `rect`, `color`, `layer`. Nenhum deles é macro conhecida nas cinco plataformas, mas a garantia é de revisão, não de gate. O enumerador `round` de `gltfx_line_cap` COLIDIU na sonda da rodada 1 com `/usr/include/tgmath.h:972` (macro `round(Val)`, ver seção 6). É o achado CRI-01 da seção 5, corrigido no texto proposto para `rounded`. A rodada 2 da sonda confirma a correção.
- **R7 (formatação é vocabulário de token, nunca frase): confirmada.** Os tokens novos (`index_out_of_range`, `count_not_multiple_of_three`, `unknown_option`, `too_many_vertices`) são identificadores. Nenhum campo novo carrega texto livre.
- **R8 (callback com ponteiro de função + contexto): não se aplica.** Nenhum callback novo.
- **R9 (evento de log é identificador mais campos tipados): não se aplica.** Nenhum evento de log novo nesta fatia.
- **R10 (callbacks entregues à biblioteca): não se aplica.**

**Regra de citação de comentário público (parágrafo de abertura de `docs/api-conventions.md`):** o texto proposto de `fill_ellipse` diz "measured on the SCREEN". "Measured" está no vocabulário fechado de `check_claim_citations.py`, e sem `Proved by:` o portão reprova. Corrigir na S5 (ver COS-01). Os `Proved by:` que o texto cita (`stroke_geometry_test`, `ellipse_tessellation_test` e os de S2a a S5) **não existem** no inventário hoje. O portão de citação reprova o cabeçalho enquanto esses testes não existirem: é o mesmo estado do B0 (fato 6.5), esperado, mas a S0 precisa dizer que o texto só entra no repositório junto com o teste que cita.

---

## 4. As quatro perguntas abertas (§9) e as decisões D-W8-05 a 08, 18 e 30

### 4.1 As quatro perguntas de `docs/auditoria-api-draw2d.md` §9

| Pergunta | Resposta proposta | Onde |
|---|---|---|
| Espessura, mundo ou pixel? | Os dois, com unidade explícita, padrão mundo | `stroke.hpp`, `gltfx_stroke::unit` |
| Contorno de retângulo para dentro, fora ou centrado? | Alinhamento explícito, padrão centrado, parâmetro de `stroke_rect` | `stroke_rect(..., align)` |
| Elipse ou círculo? | Uma elipse com rotação; círculo é elipse de raios iguais | `core/ellipse.hpp` |
| Tipo público de vértice? | `gltfx_vertex_2d_world` separado do formato interno de 32 bytes, com `std::span` lido só durante a chamada | `draw2d/vertex_2d.hpp` |

### 4.2 Decisões do plano, uma a uma

- **D-W8-05 (espessura em mundo por padrão, unidade pixel explícita): CONFIRMADA.** Campo `unit` em `gltfx_stroke`. Espessura negativa vira `negative_size`. Zero desenha nada.
- **D-W8-06 (contorno de retângulo com alinhamento dentro, centrado ou fora, padrão centrado, sem sobreposição nos cantos): CONFIRMADA na substância, DIVERGENTE na localização do campo.** O plano diz "campo de alinhamento". Proponho que o alinhamento seja **parâmetro de `stroke_rect`**, e não campo de `gltfx_stroke`, porque `stroke_line` e `stroke_ellipse` ignorariam o campo, e um campo que o método ignora mente sobre o que o método faz (L-28). Vai à seção 7 como DIV-01.
- **D-W8-07 (elipse com rotação, preenchida e contornada; círculo é elipse de raios iguais; tesselação pelo erro na tela, com 0,25 pixel como padrão e campo no descritor): CONFIRMADA.** Raios nomeados `radius_x` e `radius_y`; rotação em `gltfx_angle`. O campo `ellipse_max_error_pixels` vai no descritor, com padrão 0,25.
- **D-W8-08 (vértice público separado, índices opcionais de 32 bits, pontas reta, quadrada e redonda): CONFIRMADA na substância, DIVERGENTE na forma da opcionalidade.** O plano diz "índices opcionais (vazio = ordem sequencial)". Proponho **duas sobrecargas**, `fill_triangles(vertices, layer)` e `fill_triangles(vertices, indices, layer)`, em vez de um `span` vazio, porque `fill_triangles(v, {})` é ilegível para quem nunca viu. Vai à seção 7 como DIV-02. Pontas: as três estão em `gltfx_line_cap`; o padrão é `flat`, que é o nome do que o plano chama "reta". Nome alternativo `butt` (Canvas) é cosmético (COS-02).
- **D-W8-18 (fachada dividida por assunto em arquivos de implementação): CONFIRMADA.** Não é API pública. A classe continua uma só (`gltfx_renderer_2d`), e os cinco métodos novos são membros dela.
- **D-W8-30 (layout do relatório do quadro decidido UMA vez, na S0, para a onda inteira): CONFIRMADA, com o layout abaixo.** Motivos de recusa ganham valor no enum (`index_out_of_range`, `count_not_multiple_of_three`, `unknown_option`, `too_many_vertices`; `image_without_owner` saiu, errata D-W8-334). Cada motivo ganha um contador próprio, e a soma dos contadores de todos os motivos é igual a `pieces_refused` (invariante escrita no cabeçalho). Dois contadores de custo entram sem recusa: `texture_switches` e `shadow_segments` (D-W8-322, que troca o nome de `shadow_groups`), zerados até as fatias de textura e de sombra. Ficam decididos aqui, para T0 e R0 não reabrirem o layout.

**Errata D-W8-334 (CTO):** o motivo `image_without_owner` e o contador `pieces_refused_image_without_owner` SAEM deste layout. O motivo nasce com a primeira ponte de pintura, e não aqui (D-W8-336, R0). Os motivos `unknown_option` (D-W8-285) e `too_many_vertices` (D-W8-287) entram no fim do enum, contíguos, porque a lista ainda não foi commitada.

Layout final de `gltfx_frame_2d_report` (todos acrescentados ao fim, nenhum reordenado): `pieces_refused_not_a_number`, `pieces_refused_infinite`, `pieces_refused_negative_size`, `pieces_refused_index_out_of_range`, `pieces_refused_count_not_multiple_of_three`, `pieces_refused_unknown_option`, `pieces_refused_too_many_vertices`, `pieces_refused_pixel_position_out_of_range` (errata S2b-fix1), `texture_switches`, `shadow_segments`.

---

### 4.3 L-28 do projeto: "quem nunca viu entende lendo uma vez?", nome por nome

| Nome | Lê de uma vez? | Nota |
|---|---|---|
| `gltfx_vertex_2d_world` | sim | vértice, 2D, mundo: os três conceitos estão no nome |
| campos `position`, `color` | sim | |
| `fill_triangles(vertices, layer)` | sim | |
| `fill_triangles(vertices, indices, layer)` | sim | |
| `gltfx_stroke` (`thickness`, `unit`) | sim | `stroke.unit = gltfx_stroke_unit::pixel` se lê sem o cabeçalho |
| `gltfx_stroke_unit::world` / `pixel` | sim | |
| `stroke_line(start, end, ...)` | sim | `from`/`to` foi trocado por `start`/`end` na proposta |
| `gltfx_line_cap::flat` / `square` / `rounded` | parcial | `flat` é o cap reto (COS-02); `rounded` foi `round`, renomeado por CRI-01 |
| `stroke_rect(rect, stroke, color, align, layer)` | sim | |
| `gltfx_stroke_align::centered` / `inside` / `outside` | sim | |
| `fill_ellipse`, `stroke_ellipse` | sim | |
| `gltfx_ellipse_world` (`center`, `radius_x`, `radius_y`, `rotation`) | sim | `rotation` é `gltfx_angle`, a unidade está no tipo |
| `ellipse_max_error_pixels` | sim | longo de propósito |
| `count_not_multiple_of_three`, `index_out_of_range`, `unknown_option`, `too_many_vertices` | sim | tokens de identificador (R7) |
| `pieces_refused_*`, `texture_switches`, `shadow_segments` | sim | |

Nenhum nome exige o cabeçalho aberto para ser entendido. O único que pede uma linha de explicação é o cap `flat`.


## 5. Achados por classe

Classes: **CRÍTICO** bloqueia a S1 ou quebra o que já está fechado; **IMPORTANTE** é contrato a fechar antes de qualquer código; **COSMÉTICO** é nome ou comentário. Nenhum achado é divergência com decisão do plano: essas estão na seção 7.

### CRÍTICO: 1 (corrigido no texto proposto)

- **CRI-01. Enumerador `round` colide com macro do sistema (R6). FATO, medido pela sonda, rodada 1.** `gltfx_line_cap::round` casa com `/usr/include/tgmath.h:972` (`round(Val)`, macro de função). O portão de colisão reprova com rc=8 (seção 6). Como o texto proposto não compilaria com o portão no lugar, é CRÍTICO para a S1. **Correção aplicada no texto proposto: `rounded`.** A rodada 2 da sonda, com o texto corrigido, é a prova de que a correção basta (seção 6). A colisão nunca chegou ao repositório, porque nada foi commitado.

### IMPORTANTE: 8

- **IMP-01. Valor de enum fora da faixa vindo do consumidor (R4).** `gltfx_stroke_unit`, `gltfx_line_cap` e `gltfx_stroke_align` entram por parâmetro. Um consumidor que faz `static_cast<gltfx_line_cap>(9)` deixa a proposta sem comportamento definido. Opções: (a) recusar a chamada com um motivo novo, `unknown_option`, contado no relatório (recomendado: silêncio esconde erro do consumidor); (b) cair no padrão e contar. **Decidido (D-W8-285, opção a):** recusa da chamada inteira com `unknown_option`; cada enum ganha sua célula na fatia que o usa primeiro (S3 e S4).
- **IMP-02. Texto de contrato desatualizado (R4, R7).** Após a onda, ficam falsos: (1) o comentário de `gltfx_draw_2d_refusal_name()` diz que há quatro tokens; (2) o cabeçalho de `frame_2d_report.hpp` diz "WHAT A PIECE IS: one call of fill_rect() or fill_quad()"; (3) o `pieces_refused` diz "not a finite number, or a rectangle had a negative width or height", sem os motivos novos; (4) `renderer_2d.hpp` lista só o que `open()` recusa hoje, e o `open()` ganha `ellipse_max_error_pixels`. Corrigir no próprio commit da S2b e da S5, não depois.
- **IMP-03. Definição de "peça" não cobre os métodos novos.** Hoje "peça" é uma chamada de `fill_rect` ou `fill_quad`, e o invariante `pieces_submitted == drawn + refused + dropped_oom + dropped_graphics` depende dessa definição. Proposta: **uma chamada de desenho é uma peça**, qualquer que seja o método; `fill_triangles` com cem triângulos é uma peça; `stroke_rect`, com quatro lados, é uma peça; e uma recusa por índice ruim recusa a chamada inteira, nunca triângulo a triângulo. Sem essa definição escrita, o invariante deixa de fechar. **Decidido (D-W8-287, IMP-03):** a proposta é aceita; a definição está escrita nos cabeçalhos de `renderer_2d.hpp`.
- **IMP-04. Casos de borda não especificados.** A proposta não diz o que acontece com: (a) `stroke_line` de comprimento zero, com `square` ou `rounded` (desenhar a ponta sozinha, ou nada); (b) raio zero (proposta: nada) ou negativo (proposta: `negative_size`); (c) centro, raio, ou vértice que não é número finito (`not_a_number`); (d) `fill_triangles` com mais de 2^32 vértices, que o índice de 32 bits não endereça. Cada caso precisa de uma linha no cabeçalho e de um teste citado. **Decidido (D-W8-287, IMP-04):** (a) `stroke_line` de comprimento zero desenha só a ponta, como a SVG 2 (D-W8-270); (b) raio zero não desenha nem conta, negativo é `negative_size`; (c) NaN é `not_a_number` e infinito é `infinite`; (d) acima de 2^32 vértices, recusa com `too_many_vertices`. Os quatro estão nos cabeçalhos da seção 2. O nome `too_many_vertices` passa pela sonda de colisão do passo 0 da S1, como os demais novos.
- **IMP-05. Tolerância de elipse sem piso nem teto.** `ellipse_max_error_pixels` aceita qualquer número maior que zero, e um valor de 1e-12 faz o desenhador gerar uma quantidade enorme de segmentos por elipse, num valor que o consumidor escolheu. Proposta: `open()` recusa valores fora de um intervalo, por exemplo de 0,01 a 64 pixels, e o intervalo entra no cabeçalho com o teste que o prova. O intervalo é decisão do CTO (números), o achado é a falta dele. **Decidido (D-W8-284, Q-02):** sem intervalo inventado; a faixa válida é finita e maior que zero, e o teto de custo é o de 512 segmentos por elipse (D-W8-269).
- **IMP-06. Sobrecarga `fill_triangles(v, {})` é AMBÍGUA. FATO, medido por compilação só-sintaxe (sem execução) no host.** Com `fill_triangles(span, gltfx_draw_layer = {})` e `fill_triangles(span, span<const uint32_t>, gltfx_draw_layer = {})`, a chamada `r.fill_triangles(v, {})` é recusada por `g++` (rc=1, "call ... is ambiguous") e por `clang++` (rc=1, "call to member function 'fill_triangles' is ambiguous"). É erro de compilação, não silêncio, e o caso é justamente a forma sem índices que o plano pedia. A correção é decisão de desenho: ver DIV-02 na seção 7. O texto proposto NÃO foi alterado para a correção, para que a divergência vá ao CTO inteira. **Decidido (D-W8-282, DIV-02):** a correção está aplicada na seção 2 (`fill_triangles` e `fill_indexed_triangles`).
- **IMP-07. Mudança de layout de `gltfx_renderer_2d_desc` e de `gltfx_frame_2d_report` (decisão de ABI do líder, L-01).** Os dois structs ganham campos ao fim. FATO: nenhum teste em `tests/` fixa o `sizeof` nem o layout desses dois tipos (busca por `sizeof(gltfx_frame_2d_report)`, `sizeof(gltfx_renderer_2d_desc)` e `is_trivial` sem resultado para eles; só `draw2d_parity_test` e `renderer_2d_impl_test` os usam). O cabeçalho diz que "antes de 1.0 é permitido e anunciado pela versão". Mas o consumidor que compilou com o cabeçalho antigo e liga no `.so` novo passa um objeto menor do que a biblioteca lê (no `desc`) ou recebe um relatório que a biblioteca escreve além do tamanho dele (no `report`, por valor dentro de `gltfx_rslt`). Como o `SOVERSION` está em zero, nenhuma compatibilidade é prometida. Mas a base de consumidores é desconhecida (L-21), e a marca B (D-W8-19) anuncia "compatível". Isto não é do revisor decidir: o líder escolhe entre (a) aceitar a quebra de layout pré-1.0 e anunciar "recompilar obrigatório" na nota de versão; (b) trocar o `desc` por tipo novo com sobrecarga de `open()`, como o próprio cabeçalho sugere para depois de 1.0; (c) reservar campos de folga agora. Tratado na seção 7 como pergunta. **Decidido (D-W8-283, Q-01, opção a):** quebra de layout pré-1.0 aceita, com "recompilar obrigatório" na nota de versão. Decisão do líder pela L-26 (`GODS_LAWS.md:699` e `:701`), aplicada pelo CTO (D-W8-356).
- **IMP-08. Texto proposto de `fill_ellipse` usa "measured" sem citação.** "Measured on the SCREEN" está no vocabulário fechado de `check_claim_citations.py`, e sem `Proved by:` o portão reprova o cabeçalho quando ele entrar. Corrigir o texto ou citar o teste, na S5. **Decidido (IMP-08, r2 §12):** o texto de `fill_ellipse` já traz `Proved by: ellipse_tessellation_test` e não usa a palavra `measured`; o teste entra com a S5.

### COSMÉTICO: 3

- **COS-01. Texto de planejamento dentro de cabeçalho público.** `stroke.hpp` diz "planned for fatia S3". Um cabeçalho público envelhece com essa frase. Trocar por um `Proved by:` com nome de teste, ou apagar. **Decidido (r2 §12):** saiu na S1; o cabeçalho de `stroke.hpp` traz `Proved by: stroke_geometry_test`.
- **COS-02. Nome do cap reto.** `gltfx_line_cap::flat` é o nome que propus. Quem vem de Canvas ou SVG lê `butt`, e quem vem de desenho técnico lê `flat`. Os dois são legíveis; a escolha é do CTO e não muda a forma. **Decidido (D-W8-286, Q-04):** `flat`, com `butt` citado como sinônimo no cabeçalho.
- **COS-03. Nomes de parâmetro não varridos pelo portão (R6).** Declarado por `check_public_name_collision.py:37-38`, não é defeito de código. Fica como lacuna conhecida: nenhum parâmetro novo é macro conhecida na sonda, mas a garantia é de revisão.

---

## 6. Sonda de colisão

Rodadas dentro da trava (`flock -w 7200 -E 99 /var/tmp/cto-w8/heavy.lock`), em contêiner, pelo `run_ctest.sh` da ordem, filtro `public_name_collision_test`. Árvore de base: `66a823e1072cbad1ac962e8d3f2069a8274a0ac8`, com os três cabeçalhos novos e os trechos aplicados na cópia `fixture` (sem tocar o repositório).

| Rodada | Cópia | Texto de `gltfx_line_cap` | rc_configure | rc_ctest | Log | Resultado |
|---|---|---|---|---|---|---|
| 1 | `fixture` (negativa) | `round` | 0 | **8** | `col-r1.log` | `COLISAO /usr/include/tgmath.h:972:round`; 407 nomes públicos |
| 1 | `fixture-pos` (controle) | `round` + `major` plantado | 0 | 8 | `col-pos-r1.log` | `COLISAO /usr/include/sys/sysmacros.h:60:major` e `tgmath.h:972:round`; 410 nomes |
| 2 | `fixture` (negativa) | `rounded` | 0 | **0** | `col.log` | passou |
| 2 | `fixture-pos` (controle) | `rounded` + `major` plantado | 0 | **8** | `col-pos.log` | `COLISAO /usr/include/sys/sysmacros.h:60:major`; 410 nomes |

Leitura, em duas linhas separadas, como a ordem pede:
- **Negativa (`col.log`):** `rc_ctest=0`. A frase "0 colisao real" que a ordem espera NÃO aparece no log: o `ctest` esconde a saída de teste que passa (`--output-on-failure`), e só as rodadas que reprovam imprimem a contagem. A prova é o rc, e a contagem de 407 e 410 nomes vem só das rodadas reprovadas.
- **Controle positivo (`col-pos.log`):** `rc_ctest=8`, nomeando `major`. A sonda não está quebrada: ela enxerga o que deve enxergar. O critério de parada da ordem ("controle passa verde") não ocorreu.

**Ressalva da rodada 1, a saída esperada não veio:** a ordem diz que `col.log` deve sair com `rc_ctest=0`. Na primeira rodada saiu 8, por `round`. Pela ordem, isso mandaria parar. Eu não parei, porque a sonda existe para achar exatamente isso, e a correção é uma mudança de nome no texto proposto, não execução. Registrei a rodada 1 com o nome `col-r1.log` e refiz a rodada 2 com o texto corrigido. Quem lê o relatório decide se aceita a correção.

**Limite declarado (seção de cabeçalho do portão):** nomes de PARÂMETRO não são varridos (`tests/tools/check_public_name_collision.py:38`). Os parâmetros novos estão listados na seção 3, R6.

**Fato observado na rodada 1, sem relação com o texto novo:** o portão reconhece `open` como colisão NEUTRALIZADA por guarda inativa em `/usr/include/fcntl.h:215` e em `X11/Xw32defs.h:41`. `open` é nome público já existente (`gltfx_renderer_2d::open`), e não é causado pela W8. Vale registrar, porque a regra R6 diz que nome neutralizado não é nome seguro.

---

## 7. Divergências

Quem decide é o CTO, e o líder onde o item diz. O revisor não decide nenhum deles. Cada um traz o que o plano diz, o que a proposta diz e a recomendação, que é só recomendação.

**DIV-01. Alinhamento de `stroke_rect` (D-W8-06).** **Decidido (D-W8-281):** fora de `gltfx_stroke`, padrão `centered`; agrupado em `gltfx_rect_outline_style` (D-W8-354).
- O plano diz: "campo de alinhamento com os três, padrão centrado".
- A proposta diz: parâmetro `align` de `stroke_rect`, padrão `centered`, fora de `gltfx_stroke`.
- Motivo: `stroke_line` e `stroke_ellipse` ignorariam o campo (um campo que o método ignora mente sobre o método, L-28).
- Recomendação do revisor: parâmetro. Quem decide: CTO.

**DIV-02. Forma da opcionalidade dos índices (D-W8-08).** **Decidido (D-W8-282):** opção (b), dois nomes.
- O plano diz: "índices de 32 bits opcionais (vazio = ordem sequencial)".
- A proposta diz: duas sobrecargas `fill_triangles`. IMP-06 (FATO) mostra que `fill_triangles(v, {})` fica ambígua.
- Opções: (a) uma função, `fill_triangles(vertices, indices = {}, layer = {})`, com `{}` explícito quando não há índice; (b) dois nomes, `fill_triangles(vertices, layer)` e `fill_indexed_triangles(vertices, indices, layer)`; (c) manter as sobrecargas e aceitar a ambiguidade. A opção (c) não serve.
- Recomendação do revisor: (b). Ressalva: `fill_indexed_triangles` é nome novo e ainda não passou pela sonda de colisão. Antes de aceitar, a sonda roda de novo com o nome novo.
- Quem decide: CTO.

**Q-01. Mudança de layout de `gltfx_renderer_2d_desc` e de `gltfx_frame_2d_report` (IMP-07).** **Decidido (D-W8-283):** opção (a). Decisão do líder pela L-26, aplicada pelo CTO (D-W8-356).
- O plano diz: marca B, "recurso novo compatível" (D-W8-19).
- A proposta muda o layout dos dois tipos, e o consumidor que não recompila pode ter corrupção de memória.
- Opções do revisor: (a) quebra pré-1.0 aceita, com "recompilar obrigatório" na nota de versão; (b) tipo de descritor novo com sobrecarga de `open()`; (c) campos de folga agora.
- Quem decide: o LÍDER, por L-01 (decisão de ABI de produto com base de consumidores desconhecida). O CTO leva a pergunta.

**Q-02. Intervalo de `ellipse_max_error_pixels` (IMP-05).** Números de piso e teto: CTO. Sugestão do revisor: 0,01 a 64 pixels, sem base medida. **Decidido (D-W8-284):** sem intervalo; finito e maior que zero.

**Q-03. Valor de enum fora da faixa (IMP-01).** Recusar com `unknown_option` ou cair no padrão e contar: CTO. Recomendação: recusar. **Decidido (D-W8-285):** recusar, com `unknown_option`.

**Q-04. Nome do cap reto (COS-02).** `flat` ou `butt`: CTO. Não muda a forma da API. **Decidido (D-W8-286):** `flat`.

Nenhuma divergência tem efeito sobre as decisões D-W8-05, 07, 18 e 30 além do que está acima.


---

## 8. FATO e INFERÊNCIA

**FATO (medido ou lido, com fonte):**
- Árvore de base do parecer: `66a823e1072cbad1ac962e8d3f2069a8274a0ac8` (`git -C fixture rev-parse HEAD`). Depois dela, o `HEAD` da árvore real avançou para `5cd68dae` (só documentação; `git diff --stat` sobre `include/glintfx/draw2d` e `include/glintfx/core` não tem diferença). A proposta vale para os dois.
- `plano-fechamento-w8.md`: md5 `647a3e354e4c3ca5f4ccc36f436b286c`, conferido no início e na escrita.
- Disco, `btrfs filesystem usage /`: `Device unallocated` 21,23 GiB; `Free (estimated)` min 25,31 GiB no início e 25,17 GiB durante a sonda. Acima do limite de 20 GiB.
- Sonda: rcs e linhas de colisão da seção 6, lidos dos logs.
- Compilação só-sintaxe no host, sem execução (`g++ -std=c++23 -fsyntax-only -Wall -Wextra`, e `clang++` do mesmo modo), com os cabeçalhos do fixture e o `export.hpp` gerado pelo CMake da rodada 1: `base.cpp` (todos os nomes novos em uso) rc=0 nos dois compiladores; `ambig.cpp` (`fill_triangles(v, {})`) rc=1 nos dois, com "is ambiguous". Arquivos em `probe/`.
- Testes em `tests/` sem `sizeof` nem layout fixado para `gltfx_renderer_2d_desc` e `gltfx_frame_2d_report` (busca por nome e por `static_assert`).
- Gate `check_public_name_collision.py`: `:38` diz que parâmetro não é varrido.
- Regra de citação de comentário público e vocabulário fechado ("measured", "identical"...): parágrafo de abertura de `docs/api-conventions.md`, linha 7.
- L-28 do projeto: `GODS_LAWS.md:737` remete a régua de legibilidade a `ESCOPO.md` §4. Não li `ESCOPO.md` §4 nesta sessão: a pergunta de legibilidade foi feita nome por nome com o texto da ordem, não com a régua verbatim.

**INFERÊNCIA (não medida):**
- Que `measured` no texto de `fill_ellipse` reprova o `check_claim_citations.py` quando o texto entrar (IMP-08). Vem do vocabulário escrito no manual, não de rodar o portão.
- Que um intervalo de tolerância de 1e-12 gera contagem enorme de segmentos (IMP-05). É aritmética da forma de tesselação, não medida.
- Que um consumidor com `gltfx_renderer_2d_desc` antigo lê além do próprio objeto (IMP-07). Vem da regra de layout e do tamanho dos tipos, não de executar o caso.
- Que `fill_indexed_triangles` (DIV-02, opção b) não colide. Não foi rodado pela sonda. A sonda roda com `fill_indexed_triangles`, `unknown_option` e `too_many_vertices` no passo 0 da S1 (D-W8-282, D-W8-285, D-W8-287).
- Que a sobrecarga ambígua é o único caso ambíguo do conjunto (a compilação só testou o `{}`).

**NÃO executado, por ordem:** nenhum teste de produto compilado ou rodado; nenhum `preci`; nenhum commit, push ou `TODO.md`; nenhum toque em `watchcode`; nada na árvore real. A compilação só-sintaxe da seção 8 é o único comando fora dos portões, e não executa nenhum binário.

---

md5 do corpo deste parecer, sem esta linha: `b3d6816bc44885aef84571feb8b6ca67` (corpo de origem, antes das errata; o texto desta cópia traz as errata e tem outro md5).
Arquivo de origem: `/var/tmp/cto-w8/s0/auditoria-api-shapes.md`, md5 `2fb68acc39757cce3053f9f7e9c568e8`.
