<!-- Origem: /var/tmp/cto-w8/h0/auditoria-api-sprite-shade.md, md5 a36080994717c465b6e160abe14d9f9e. Texto do parecer com as errata aplicadas no próprio texto (a errata vence o parecer). Ver "Errata aplicadas" logo abaixo. -->

# Auditoria de API: R2D-SPRITE-SHADE (H0), luz e sombra projetada de sprite

**Revisor:** revisor de API da H0 (distinto do CTO, dos revisores da S0, da T0 e da R0, e de qualquer implementador da W8, L-12).
**Natureza:** DESENHO. Aceito pelo CTO com as errata abaixo. Esta cópia em `docs/` é o texto que a H1 a H4 copiam para o repositório, depois da errata da T0 (D-W8-303).
**Árvore revisada:** `542df882bf07691fb9b18f5c498dc2beed3faeb2` (ramo `onda-w8`), clone `/var/tmp/cto-w8/h0/fixture`. A árvore real tinha alterações não commitadas de outros agentes (`tests/CMakeLists.txt`, `tests/tools/check_examples_consume.py`); o clone pega só o commit.
**Carimbo de início:** [11/10/26 - 13:09:35] (medido com `date`, primeira leitura de disco)

**Errata aplicadas (CTO; r7 D-W8-319 a 326, na parte H0):**
- **D-W8-319 (DIV-H01, CRI-H01, opção a):** o escurecimento único vale por SEGMENTO entre barreiras (`flush()` e `finish_frame()`), sem par e sem chave. Toda peça do segmento participa. Uma peça opaca libera a marca; a semitransparente não libera. Sombra depois de barreira escurece de novo. Seção 2 (`renderer_2d.hpp`).
- **D-W8-320 (DIV-H02):** `light_below_piece` sai do relatório. A recusa da luz abaixo da peça é da função pura, por `invalid_argument`, `rejected_value()` "light_height".
- **D-W8-321 (DIV-H03):** a cor da sombra é PARÂMETRO obrigatório, misturado por `src-over`. Não há cor padrão. A frase "a multiplicação da D-W8-13 basta" sai.
- **D-W8-322 (DIV-H04):** `shadow_segments` conta os segmentos que tiveram sombra, na barreira que os fecha. Segmento sem sombra não conta.
- **D-W8-323 (Q-H01):** a tolerância entre sistemas é medida na escala da conta: cada componente fica a no máximo `4·ulp(M)` do valor esperado, sendo `M` o maior valor absoluto entre as entradas e as saídas. Vale também para o componente que deve ser zero. Não há piso absoluto.
- **D-W8-324 (Q-H02):** faixas de elevação `(0, 90]`, azimute finito, altura da peça finita e não negativa (motivo `piece_height`), altura da luz finita e estritamente maior que a da peça, coordenadas finitas.
- **D-W8-325 (Q-H03, IMP-H06):** a luz sobre a peça é função pura do núcleo, com a lei de queda como escolha obrigatória do consumidor. Ver a seção 2.1.
- **D-W8-326:** IMP-H04 (base `(0,0)-(0,10)`), IMP-H05 (ordem dos papéis), IMP-H08 (textura por referência, região como valor) e COS-H01 (`Proved by:` só com o teste existindo).
- **D-W8-353 a 356 (r13):** `gltfx_light_tint` tem cabeçalho próprio em `core/light_tint.hpp` (seção 2.1, D-W8-355), com o tipo de luz `gltfx_light_source_2d`. `draw_shadow_texture` recebe `gltfx_texture_paint` (D-W8-354), e o `tint` é a cor da sombra. As seções 3 a 7 são análise e citam, onde indicam, os nomes anteriores; o texto vigente é o da seção 2.

A seção 5 traz a tabela de cada achado com a decisão que o fecha. A seção 7 marca cada divergência e pergunta.

---

## 1. Veredito

**APROVADO COM CONDIÇÕES, como DESENHO, com UMA condição bloqueante.** Nada entra no repositório antes da DEMO-1 fechar (D-W8-251).

- **Condição 1, bloqueante para a fatia H3a (não para H1 e H2): a regra de participação do escurecimento único (CRI-H01 e DIV-H01).** O texto proposto adota o segmento entre barreiras como recomendação. Sem decisão do CTO, a promessa "no máximo uma vez" da D-W8-16 é falsa com a ordem por camada. A pergunta extra da H0 (forma do grupo) é respondida na seção 4.2: nenhuma das duas formas do plano resolve, e a escolha real é quem participa.
- **Condição 2, para a H1 e a H2 (geometria pura): Q-H01 e Q-H02.** O piso absoluto da tolerância para componentes que devem ser zero, e as faixas de elevação e de altura. O texto já trata elevação 90 por caso explícito e recusa por nome as entradas fora da faixa.
- **Condição 3, para a H4 e para o fechamento do cabeçalho de textura: Q-H03 e IMP-H08.** A superfície pública da H4 (lei de queda, luz, ambiente) não está definida no plano. A assinatura de `draw_shadow_texture` depende da errata da T0 (D-W8-303).
- **Divergências ao CTO (seção 7):** DIV-H01 a DIV-H04. Nenhuma foi decidida aqui.
- **Contagem:** CRÍTICO 1, IMPORTANTE 10, COSMÉTICO 1. Colisão de nome: nenhuma (seção 6).

---

## 2. Texto proposto

Cercas dos cabeçalhos NOVOS, como ficam na cópia de sonda `/var/tmp/cto-w8/h0/fixture`. Nada disto está no repositório.

### `include/glintfx/core/light_2d.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/angle.hpp>
#include <glintfx/core/vec2.hpp>

// core/light_2d.hpp - R2D-SPRITE-SHADE (docs/auditoria-api-sprite-shade.md,
// H0; decision D-W8-16 of the W8 plan). The two kinds of light that cast a
// shadow on the ground plane.
//
// TWO TYPES, NOT ONE WITH A KIND FIELD: a field that one kind ignores
// would describe the other kind wrongly (GODS_LAWS.md L-28), and two
// distinct aggregates keep the two projection functions apart at the call
// site.
//
// THE GROUND AND THE HEIGHT: positions are WORLD positions on the ground
// plane, in double precision (core/vec2.hpp). A height is the distance
// above that plane, in the same world units. Screen y grows DOWN, so a
// point higher on the screen has a smaller y.
//
// Proved by: projected_shadow_test (the directional and point cases, and
// the refusal of each field by name).

namespace glintfx {

// Parallel rays, all coming from one direction (a sun far away).
struct gltfx_directional_light_2d {
    // The direction the light COMES FROM, on the ground plane: 0 degrees is
    // from +x, 90 degrees is from +y (the bottom of the screen). The shadow
    // of a piece points the other way.
    gltfx_angle azimuth{};
    // The angle between the rays and the ground. Accepted: more than 0
    // degrees and at most 90 degrees. Zero would make the shadow infinitely
    // long; above 90 the rays would come from the other side. Anything else
    // is refused by name: invalid_argument, rejected_value() "elevation".
    gltfx_angle elevation{};
};

// Rays that spread out from one lamp, at a point.
struct gltfx_point_light_2d {
    // The lamp's position on the ground plane.
    gltfx_vec2_world position{};
    // The lamp's height above the ground. It must be finite and strictly greater than
    // the height of the piece it lights: a lamp at or below the top of a
    // piece casts no shadow of that piece. Refused by name: invalid_argument,
    // rejected_value() "light_height".
    double height = 0.0;
};

} // namespace glintfx
```

### `include/glintfx/core/projected_shadow.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/err.hpp>
#include <glintfx/core/light_2d.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/export.hpp>

// core/projected_shadow.hpp - R2D-SPRITE-SHADE (docs/auditoria-api-sprite-
// shade.md, H0; decisions D-W8-16 and D-W8-27). The shadow of an upright
// piece on the ground plane, as a quadrilateral. Pure geometry, like
// core/quad.hpp: no drawing, no GL, no operating system.
//
// DETERMINISM, as promised: the same inputs give the same bits on the same
// binary. Between systems each component agrees within 4 ULP of the scale of
// the account: at most 4 * ulp(M) from the expected value, M being the largest
// absolute value among the inputs and the outputs of that projection
// (coordinates, heights and result). The same rule covers a component that
// must be zero; no absolute floor is used.
// Proved by: projected_shadow_test.
//
// THE PIECE. A standing piece has its base segment on the ground and stands
// `height` above it. Each base point is a ground point; its top is the same
// point lifted by `height`. The shadow of a point at height h is where the
// ray from the light, through that point, meets the ground.
//
// THE RESULT, and the corner roles it uses. The quadrilateral takes the
// roles of gltfx_quad_world (top_left, top_right, bottom_right, bottom_left),
// chosen so that a texture drawn on it reads correctly:
//   top_left     = the shadow of the top of base_start (the far end of the shadow)
//   top_right    = the shadow of the top of base_end
//   bottom_right = base_end   (on the ground: the piece's own base point)
//   bottom_left  = base_start
// A sprite's texture, whose top-left texel is at the top of the sprite and
// whose bottom-left texel is at base_start, lands on its own shadow with no
// swap of corners.
//
// Two examples the tests check, corners in the order above:
//   directional, azimuth 0 degrees, elevation 45, base (0,0)-(0,10), height
//     20: the shadow is (-20,0), (-20,10), (0,10), (0,0).
//   point at (10,-30), height 40, base (5,0)-(15,0), height 20: the shadow is
//     (0,30), (20,30), (15,0), (5,0).
//
// Proved by: projected_shadow_test.

namespace glintfx {

struct gltfx_standing_piece_2d {
    // The two ends of the base on the ground, in the order left, right, as
    // the camera sees the piece.
    gltfx_vec2_world base_start{};
    gltfx_vec2_world base_end{};
    // The height above the ground of the piece's top. Finite, and zero or
    // greater. A negative or not finite height is refused: invalid_argument,
    // rejected_value() "piece_height". Zero is not refused: the shadow is
    // degenerate and covers no pixel.
    double height = 0.0;
};

// REFUSALS, all gltfx_err with code invalid_argument and the field named in
// rejected_value() (docs/api-conventions.md R1, R4):
//   "elevation"    : the elevation is not in (0, 90] degrees;
//   "light_height" : the point light's height is not finite, or not greater
//                    than piece.height;
//   "piece_height" : the piece's height is not finite or is negative;
//   "position"     : a base coordinate or a light position is not finite;
//   "azimuth"      : the azimuth of a directional light is not finite.
// A zero-length base is NOT refused: the quadrilateral degenerates to a
// segment and covers no pixel.
//
// An elevation of exactly 90 degrees gives a shadow of exactly zero length.
// The code does not rely on the cosine of 90 degrees, which is not zero in
// double precision.
// Proved by: projected_shadow_test.
[[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_quad_world>
gltfx_project_directional_shadow(gltfx_directional_light_2d light,
                                 gltfx_standing_piece_2d piece) noexcept;

[[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_quad_world>
gltfx_project_point_shadow(gltfx_point_light_2d light,
                           gltfx_standing_piece_2d piece) noexcept;

} // namespace glintfx
```

### `include/glintfx/draw2d/renderer_2d.hpp` (trecho: dois métodos de sombra e as declarações provisórias da textura, dentro da classe e antes de `private:`)

```cpp
    // A SHADOW: a filled quadrilateral that darkens what is already drawn
    // under it. gltfx_project_directional_shadow() and
    // gltfx_project_point_shadow() (core/projected_shadow.hpp) give one.
    // `tint` is the colour of the shadow, mixed over what is under it by
    // src-over. A black tint with alpha a gives dst * (1 - a): darkening. Any
    // other colour mixes towards that colour. `tint` is a REQUIRED parameter
    // with no default colour: the look of a shadow belongs to the consumer.
    //
    // SHADOW SEGMENT. A pixel is darkened by the shadows of a segment at most
    // once, until an opaque piece of that segment is drawn over it. An opaque
    // piece is one whose colour has alpha 1 on that pixel: a half-transparent
    // piece does not release the mark. A segment is the drawing between two
    // barriers, flush() or finish_frame(). Every piece drawn in the segment
    // takes part, shadow or not, so a shadow drawn over an opaque piece darkens
    // that piece. A barrier ends the segment, the same way it ends the layer
    // order: a shadow drawn after a barrier darkens again. begin_batch() does
    // not end a segment.
    //
    // A shadow is one piece, as draw2d/frame_2d_report.hpp counts pieces.
    // Refused as one piece, counted in the report, never an exception:
    //   - a corner that is not a finite number: not_a_number;
    //   - a corner that is infinite: infinite.
    // A shadow of zero area is drawn and covers no pixel.
    // Proved by: shadow_group_test (the stencil sequence of a batch) and
    // draw2d_shadow_parity_test (the pixels, on both systems).
    GLINTFX_API void fill_shadow(gltfx_quad_world shadow, gltfx_rgba tint,
                                 gltfx_draw_layer layer = {}) noexcept;

    // The silhouette of `texture` as a shadow, through `paint.region`: the
    // texture's alpha is the shape, and `paint.tint` (the colour of the shadow,
    // D-W8-321) and the segment rule are those of fill_shadow(). The corners of
    // `shadow` carry the corners of the region in the roles of gltfx_quad_world
    // (core/projected_shadow.hpp). The texture comes by reference, the paint as
    // a value. D-W8-354: four parameters at most, with the layer.
    GLINTFX_API void draw_shadow_texture(const gltfx_texture_2d &texture,
                                         gltfx_texture_paint paint,
                                         gltfx_quad_world shadow,
                                         gltfx_draw_layer layer = {}) noexcept;

```

### `include/glintfx/draw2d/frame_2d_report.hpp` (trecho: contador novo ao fim)

```cpp
    // W8, H0 (D-W8-16): shadow segments with at least one shadow, counted at
    // the barrier that closes them. A segment without shadows counts nothing.
    std::uint64_t shadow_segments = 0;
};
```

### 2.1 Luz sobre a peça (H4): contrato decidido (D-W8-325)

A luz sobre a peça é uma função PURA do núcleo, `gltfx_light_tint`, em `core/light_tint.hpp` (D-W8-353). Ela recebe os vértices, as luzes pontuais e a luz ambiente, e escreve a tinta de cada vértice num `span` de saída, sem alocar. O consumidor passa a tinta ao desenho com cor por vértice (`fill_triangles` ou `fill_indexed_triangles`, do R2D-SHAPES). O desenhador não ganha estado novo.

- Cada luz pontual tem `position`, `range` (finito e maior que zero), `color` (luz linear), `intensity` (zero ou maior) e `falloff`. É um tipo PRÓPRIO, `gltfx_light_source_2d`, e não o `gltfx_point_light_2d` da sombra: aquele tem altura, que a tinta não usa.
- `falloff` é um enum SEM padrão: o valor `unspecified` é recusado pelo nome. A lei de queda é escolha obrigatória do consumidor, entre:
  - `linear`: `f = clamp(1 - d/range, 0, 1)`;
  - `smooth`: `f = clamp(1 - (d/range)^2, 0, 1)^2`.
  - A queda pelo inverso do quadrado não entra: em 2D ela depende da escala da unidade de mundo.
- A conta: `tinta = clamp(ambiente + soma(cor · intensidade · f(d)), 0, 1)`, canal a canal. `d` é a distância NO PLANO, da luz ao vértice. A altura da luz serve à sombra, não à tinta.
- Recusa, com `invalid_argument` e o nome do campo: `out_tints` com tamanho diferente de `vertices`; um vértice que não é número finito (`vertices`); e os campos da luz fora da faixa (`range`, `intensity`, `falloff`, `position`).

Valores fixados (luz em `(0,0)`, `r = 10`, branca, intensidade 1):

| Caso | Esperado |
|---|---|
| `linear`, `d = 5` | 0,5 |
| `smooth`, `d = 5` | 0,5625 |
| `d = 0` | 1 |
| `d = 10` | 0 |
| `d = 12` | 0 |
| ambiente `(0,2; 0,2; 0,2)` mais `linear`, `d = 5` | 0,7 |
| duas luzes de 0,6 no mesmo vértice | 1 (satura) |
| `falloff` `unspecified` ou fora do enum | recusado pelo nome (`falloff`) |

Cabeçalho (D-W8-355, texto vigente):

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/vec2.hpp>
#include <glintfx/export.hpp>

// core/light_tint.hpp - R2D-SPRITE-SHADE (H4; D-W8-325 and D-W8-355). The light
// that point lights put on ground vertices: a PURE function of the core, with
// no drawing, no GL and no operating system. The consumer passes the result to
// the drawing as a colour per vertex.
//
// The light's height does not enter here: it belongs to the shadow
// (core/light_2d.hpp). A field that this function ignored would describe it
// wrongly (GODS_LAWS.md L-28), so the light has its own type.

namespace glintfx {

enum class gltfx_light_falloff : std::uint8_t {
    unspecified = 0,  // the default of the field; refused by name: the law of fall-off is the consumer's choice
    linear = 1,       // f = clamp(1 - d / range, 0, 1)
    smooth = 2,       // f = clamp(1 - (d / range)^2, 0, 1)^2
};

struct gltfx_light_source_2d {
    gltfx_vec2_world position{};
    double range = 0.0;        // finite and greater than zero
    gltfx_rgba color{};        // linear light
    double intensity = 0.0;    // finite, zero or more
    gltfx_light_falloff falloff = gltfx_light_falloff::unspecified;
};

// Writes in out_tints[i] the light that reaches vertices[i]: ambient plus, for each light, color * intensity *
// f(d), with d the distance ON THE PLANE, clamped to [0, 1] per channel. Refused with invalid_argument and the
// field's name: out_tints.size() != vertices.size() ("out_tints"), a vertex not finite ("vertices"), and each
// light field out of range ("range", "intensity", "falloff", "position").
[[nodiscard]] GLINTFX_API gltfx_rslt<void>
gltfx_light_tint(std::span<const gltfx_vec2_world> vertices, std::span<const gltfx_light_source_2d> lights,
                 gltfx_rgba ambient, std::span<gltfx_rgba> out_tints) noexcept;

} // namespace glintfx
```

Os quatro parâmetros de `gltfx_light_tint` ficam no limite de `CONTRACT.md` §6.2 (D-W8-354).

---

## 3. R1 a R10 de `docs/api-conventions.md`, uma a uma

- **R1 (retorno único `gltfx_rslt<T>`): confirmada.** As duas funções de projeção devolvem `gltfx_rslt<gltfx_quad_world>`. Os métodos de desenho (`fill_shadow`, `draw_shadow_texture`) devolvem `void`, por decisão 2 do líder (19/09): o problema de um desenho vai ao relatório do quadro.
- **R2 (`[[nodiscard]]` estrutural): confirmada.** As funções de projeção levam `[[nodiscard]]` explícito, e o resultado já é protegido pela classe `gltfx_rslt<T>`. Nenhum método de desenho devolve valor.
- **R3 (`noexcept` na borda): confirmada com condição.** Todas as funções novas são `noexcept`. Condição para a fatia H3a e a H3b: `fill_shadow` copia o quadrilátero para o armazenamento do quadro, e isso aloca. A falta de memória vira contagem (`pieces_dropped_out_of_memory`) pelo degrau 2 da R3, como `fill_rect` já faz. O gate `noexcept_alloc_test` precisa passar nos arquivos novos.
- **R4 (campo ou código desconhecido nunca é comportamento indefinido): não se aplica ao texto atual.** Não há enumeração nova de consumidor nesta fatia. Os dois tipos de luz são agregados, e a escolha entre eles é por nome de tipo, não por valor. Se o CTO acrescentar um enum de tipo de sombra, a regra da D-W8-285 (`unknown_option`) passa a valer.
- **R5 (alocação e liberação do mesmo lado): confirmada.** O quadrilátero é valor. A textura entra por referência a cada chamada, sem guardar ponteiro (D-W8-289, T0).
- **R6 (nome público sem colisão com macro nem com a biblioteca padrão): confirmada pela sonda da seção 6, com o limite declarado.** Os nomes novos são lidos na sonda. Nomes de parâmetro não são varridos (`tests/tools/check_public_name_collision.py:37-38`); os parâmetros novos são `light`, `piece`, `lamp`, `shadow`, `tint`, `layer`, `texture`, `region`, `azimuth`, `elevation`, `position`, `height`, `base_start`, `base_end`, e nenhum deles é macro conhecida nos cinco sistemas. A garantia dos parâmetros é de revisão, não de gate.
- **R7 (formatação é vocabulário de token, nunca frase): confirmada.** Os tokens de recusa dos campos (`elevation`, `light_height`, `piece_height`, `position`, `azimuth`) são identificadores (D-W8-324).
- **R8 (callback): não se aplica.** Nenhum callback novo.
- **R9 (evento de log): não se aplica.** Nenhum evento novo nesta fatia.
- **R10 (callbacks entregues à biblioteca): não se aplica.**

**Regra de citação de comentário público (`CLAIM-CITATIONS`).** O texto proposto usa "same bits", "agree within 4 ULP" e "identical", e cada afirmação de garantia traz `Proved by:` com nome de teste. Os testes citados ainda não existem (`projected_shadow_test`, `shadow_group_test`, `draw2d_shadow_parity_test`). O portão de citação reprova o cabeçalho enquanto eles não existirem. Os cabeçalhos entram no repositório junto com os testes, como a COS-T02 da T0 já pede.

---

## 4. As decisões D-W8-16, 27 e 28, D-W8-277, e a pergunta extra

### 4.1 Decisões, uma a uma

- **D-W8-16 (luz e sombra: mecanismo genérico, quatro formas na W8, mapa de luz suave fora): CONFIRMADA na substância, DIVERGENTE em dois pontos.**
  - **Confirmado:** as quatro formas (a) direção fixa, (b) fonte com posição, (c) chão, (d) sobre outros sprites têm cobertura no texto. (c) e (a) são a luz direcional; (b) é a luz pontual; (d) depende do modelo de segmento da seção 4.2, e a ordem de profundidade continua vindo da chave de camada. O `R2D-LIGHTMAP` continua fora, em W10. O `R2D-PIXEL-SNAP` também continua fora, na INBOX.
  - **Divergente, DIV-H03 (decidido: D-W8-321):** a frase "a multiplicação da D-W8-13 basta, sem modo novo" está incorreta em geral. A D-W8-13 define o `tint` como multiplicação do texel pela cor (modulação), não como modo de mistura que multiplica o destino. O destino é escurecido por `src-over` com cor preta e alfa `a`, que dá `dst * (1 - a)`. Só a cor neutra escurece; uma cor colorida mistura em direção a ela. Vai à seção 7.
  - **Divergente, DIV-H01 (decidido: D-W8-319):** a promessa "um pixel escurecido no máximo uma vez" só vale com a regra de quem participa do escurecimento, que o plano não fixa (CRI-H01).
- **D-W8-27 (determinismo: byte a byte no mesmo binário; entre sistemas, 4 ULP): CONFIRMADA com uma condição, Q-H02.** O texto proposto usa a promessa nova, e não a da D-W8-16. Condição: a regra "dentro de 4 ULP" não define a tolerância para componentes que deveriam dar zero. Medido em Python sobre `double`: `cos(90 graus)` dá `6.12e-17`; a sombra de elevação 90 dá comprimento `1.22e-15`, não zero; o componente `x` do azimute 90 dá `-1.22e-15`, não zero. Uma comparação "dentro de 4 ULP" de um valor esperado 0 não tem ULP útil. Por isso o texto proposto trata elevação 90 por caso explícito, sem depender do cosseno. A tolerância absoluta é número do CTO (Q-H02). **Decidido (D-W8-323): escala da conta, sem piso absoluto.**
- **D-W8-28 (grupo de sombra em duas fatias: pura e de pixel; descarte para o stencil só com grupo aberto): CONFIRMADA na divisão, DIVERGENTE no objeto.** A divisão em duas fatias fica. O objeto muda: o "grupo" passa a ser o segmento entre barreiras (seção 4.2), e a condição de ativação do descarte passa a ser "sombra no segmento". Vai à seção 7 como DIV-H01.
- **D-W8-277 (convenções do bloco H): CONFIRMADA, com verificação aritmética.** O azimute diz de onde vem a luz (0 de +x, 90 de +y), e a sombra aponta para o lado oposto. Confiro os valores fixados da §7 do plano r2:
  - H1, azimute 0, elevação 45, altura 20: o topo da sombra desloca `(-20, 0)`. Confere (`-20.000000000000004`, dentro de 4 ULP).
  - H1, azimute 90: `(0, -20)`. Confere, com a ressalva de zero da Q-H02.
  - H1, elevação 30: comprimento `34.641016`. Confere.
  - H1, elevação 90: comprimento zero. Não confere exato sem caso explícito (ver acima).
  - H2, luz em `(10,-30)` a altura 40, base `(5,0)-(15,0)` a altura 20: os cantos da sombra são `(0,30)`, `(20,30)`, `(15,0)`, `(5,0)`. Confere com a fórmula `S = L + (P - L) * H / (H - h)`, em que `h` é a altura do ponto projetado.
  - **Achado sobre o exemplo H1 de azimute 0 (IMP-H04):** com base `(0,0)-(10,0)`, a base é paralela à direção da sombra, e a área da sombra é zero. O caso aceita qualquer fórmula que dê o deslocamento certo, mesmo errada na forma. O texto proposto usa base `(0,0)-(0,10)`, que dá um quadrilátero não degenerado.
  - **Achado sobre a ordem dos cantos (IMP-H05):** a lista do H2 no plano está em outra ordem que a dos papéis de `gltfx_quad_world`. O texto proposto fixa a ordem dos papéis, que é a que a textura precisa.

### 4.2 A pergunta extra: a forma do grupo

**Resposta curta:** nenhuma das duas formas do plano resolve o problema. A escolha real é **quem participa do escurecimento único**. Com a ordem por camada (D-API-04), o par begin/end e a chave por peça têm o mesmo defeito (CRI-H01). Recomendo uma terceira forma: **o segmento entre barreiras**, sem par e sem chave. **Decidido (D-W8-319): a terceira forma, opção (a).**

**CRI-H01 (CRÍTICO).** A promessa "um pixel escurecido no máximo uma vez" é falsa com as duas formas do plano. Contraexemplo concreto, por ordem de desenho (a ordem é por camada, depois por submissão):
1. Sombra `S1` na camada 0, dentro do grupo, cobre o chão no ponto `p`.
2. Sprite opaco `B` na camada 1, submetido FORA do grupo, cobre `p`.
3. Sombra `S2` na camada 2, dentro do grupo, cobre `p` também.

A ordem de desenho é `S1`, `B`, `S2`. `S1` marca `p` no estêncil. `B` não pertence ao grupo, então não libera a marca. `S2` vê a marca e não escurece `B` em `p`. Mas `S2` está acima de `B` e deveria escurecê-lo. O resultado está errado. Com a chave por peça, o mesmo caso acontece, e ainda por cima a chave força reordenação entre grupos, contrariando a regra da D-W8-13 e da L-35 de que a troca "NUNCA reordena".

**Opções para o CTO decidir (DIV-H01):**
- **(a) Segmento entre barreiras (recomendado no texto proposto).** O segmento é o desenho entre `flush()` ou `finish_frame()`. Todas as peças do segmento participam: uma peça opaca libera a marca no que cobre, e a sombra seguinte escurece de novo. Sem par begin/end, sem chave, sem estado que pode ficar aberto por erro. Custo: uma peça opaca só escreve no estêncil a partir da primeira sombra do segmento, na ordem de desenho, e é esse o custo que fica (não o do segmento inteiro). Perda: o consumidor não escolhe um subconjunto de peças; a fronteira de "uma vez" é a barreira, como a ordem de camadas já é. Contrato: `shadow_segments` conta segmentos com sombra, na barreira que os fecha; segmento sem sombra não conta.
- **(b) Par begin/end.** Corrige o caso só se TODAS as peças submetidas com o grupo aberto participarem, e aí vira o segmento com mais estados a validar. Acrescenta tokens de recusa (`shadow_group_nested`, `shadow_group_not_open`, grupo aberto numa barreira). Dá ao consumidor a escolha do subconjunto, mas o plano não pede isso.
- **(c) Chave por peça.** Mesmo contraexemplo, e reordenação entre chaves. Não recomendo.

**Consequências de (a) para o texto:** `flush()` e `finish_frame()` encerram o segmento, `begin_batch()` não encerra (a sombra é mundo, e o estêncil não depende da transformação). `shadow_groups` do plano vira `shadow_segments` (DIV-H04). A condição do descarte para o estêncil no programa interno passa a ser "sombra no segmento".

### 4.3 L-28 do projeto: "quem nunca viu entende lendo uma vez?", nome por nome

| Nome | Lê de uma vez? | Nota |
|---|---|---|
| `gltfx_directional_light_2d` / `gltfx_point_light_2d` | sim | dois tipos, não um com campo de tipo |
| `azimuth` / `elevation` | sim, com o comentário | o sentido de "de onde vem" está no comentário, não no nome; a unidade está no tipo `gltfx_angle` |
| `position` / `height` (luz) | sim | |
| `gltfx_standing_piece_2d` (`base_start`, `base_end`, `height`) | sim | |
| `gltfx_project_directional_shadow` / `gltfx_project_point_shadow` | sim | nomes distintos evitam a ambiguidade com `{}` (ver seção 6) |
| `fill_shadow` | sim, com o comentário | "sombra" está no nome; o escurecimento único é regra do segmento, no comentário |
| `draw_shadow_texture` | sim | |
| `shadow_segments` | sim | substitui `shadow_groups` (DIV-H04) |

Nenhum nome exige abrir o cabeçalho para ser entendido, exceto a regra do segmento, que está escrita no comentário de `fill_shadow`.

---

## 5. Achados por classe

Classes: **CRÍTICO** quebra promessa pública ou bloqueia o desenho das fatias de sombra; **IMPORTANTE** é contrato ou teste a fechar antes de código; **COSMÉTICO** é texto. Divergência com decisão do plano está na seção 7, não aqui.

**Decisões que fecham os achados (a errata vence o texto dos achados abaixo):**

| Achado | Decisão |
|---|---|
| CRI-H01 | D-W8-319: segmento entre barreiras. A H3 acrescenta a célula do contraexemplo e a célula "semitransparente não libera". |
| IMP-H01 | D-W8-324: faixas e recusas por nome. |
| IMP-H02 | D-W8-320: `light_below_piece` sai do relatório. |
| IMP-H03 | D-W8-323: tolerância na escala da conta; elevação 90 por caso explícito. |
| IMP-H04 | D-W8-326: base `(0,0)-(0,10)`. |
| IMP-H05 | D-W8-326: ordem dos papéis de `gltfx_quad_world`. |
| IMP-H06 | D-W8-325: luz pura do núcleo, lei de queda obrigatória do consumidor (seção 2.1). |
| IMP-H07 | D-W8-321: sombra por `src-over`, cor como parâmetro obrigatório. |
| IMP-H08 | D-W8-326 e D-W8-289 (T0): textura por referência, região como valor; o marcador PROVISIONAL sai. |
| IMP-H09 | Definição de peça opaca (alfa 1 no pixel) mantida no texto. |
| IMP-H10 | D-W8-319: "no máximo uma vez" vale dentro do segmento. |
| COS-H01 | D-W8-326: `Proved by:` só entra com o teste existindo. |

### CRÍTICO: 1

- **CRI-H01. A promessa "no máximo uma vez" (D-W8-16) é falsa com a ordem por camada e com as duas formas do plano.** FATO, por contraexemplo de ordem de desenho (seção 4.2). Corrigido no texto proposto pelo modelo de segmento, que é a recomendação. A escolha da regra vai à seção 7 (DIV-H01). Se o CTO escolher o par begin/end, a regra "todas as peças do segmento participam" tem de estar escrita no texto mesmo assim, senão o contraexemplo continua valendo. O H3 precisa acrescentar a célula do contraexemplo ("peça de fora do grupo, camada entre duas sombras do grupo") à lista enumerada, e a célula "peça semitransparente não libera".

### IMPORTANTE: 10

- **IMP-H01. Faixas e recusas de luz e de peça não estão no plano.** Faltam: elevação menor ou igual a zero (sombra infinita) e maior que 90 graus (raios do outro lado); altura negativa ou não finita; luz na altura da peça ou abaixo dela; coordenada não finita. Corrigido no texto proposto, com recusa por nome (`invalid_argument`, `rejected_value()`). Os valores (0, 90] e a regra de luz abaixo da peça são números de contrato: Q-H02.
- **IMP-H02. O token `light_below_piece` seria morto no relatório.** A D-W8-290 o põe na lista única, mas a projeção é função pura que recusa antes do desenho; `fill_shadow` recebe o quadrilátero já projetado e nunca vê a luz. Um token que o relatório não consegue carregar quebra a regra de que o relatório conta tudo (R7, L-28). Vai à seção 7 como DIV-H02.
- **IMP-H03. Zero exato não é alcançável em ponto flutuante sem tratamento de caso.** Medido: `cos(90 graus) = 6.12e-17`; comprimento da sombra a 90 graus de elevação `1.22e-15`; componente de azimute 90 `-1.22e-15`. O texto proposto trata 90 graus por caso explícito. A tolerância para os componentes que deveriam ser zero é Q-H01.
- **IMP-H04. O exemplo H1 de azimute 0 é degenerado.** Com base `(0,0)-(10,0)`, a base é paralela à sombra e a área é zero: o teste não distingue fórmula certa de errada na forma. Corrigido no texto proposto: base `(0,0)-(0,10)`. O teste precisa de pelo menos um caso não degenerado em cada azimute.
- **IMP-H05. A ordem dos cantos do H2 no plano não é a dos papéis de `gltfx_quad_world`.** Corrigido no texto proposto: a ordem dos papéis é a que a textura precisa (topo-esquerdo do texel no topo da peça, na base de `base_start`). Sem essa fixação, a silhueta sai espelhada na vertical num teste de textura.
- **IMP-H06. A superfície pública da H4 não está definida.** O plano diz que a lei de queda é decisão do H0 (r2 §7), mas o texto não diz: a lei, o alcance, a cor ou intensidade da luz, a luz ambiente e como uma peça recebe a luz. Sem isso, a H4 inventa API pública, que é porta de mão única. Não está no texto proposto. Vai à seção 7 como Q-H03.
- **IMP-H07. A premissa "a multiplicação da D-W8-13 basta" é falsa em geral.** Ver DIV-H03 na seção 4.1. O texto proposto explica o escurecimento pelo `src-over` com cor preta.
- **IMP-H08. Dependência da T0 não fechada.** `draw_shadow_texture` usa `gltfx_texture_2d` e `gltfx_texture_region`, de uma fatia que o r3 ainda muda (região como valor, textura por referência em cada chamada, D-W8-289). A assinatura do texto é provisória, e as duas declarações de encaminhamento são provisórias. O H0 só fecha depois da errata da T0 (D-W8-303).
- **IMP-H09. Regra de "peça opaca" e de "peça semitransparente".** O texto proposto define opaca como cor com alfa 1 no pixel, e diz que a semitransparente não libera a marca. A célula da H3 precisa dessa definição.
- **IMP-H10. "No máximo uma vez" vale por segmento, não através de barreira.** Uma sombra depois de `flush()` escurece de novo o que a anterior já escureceu. O texto proposto diz isso. É o contrato que o CTO aceita ao decidir DIV-H01.

### COSMÉTICO: 1

- **COS-H01. Citações `Proved by:` de testes que ainda não existem.** `projected_shadow_test`, `shadow_group_test` e `draw2d_shadow_parity_test` não estão no inventário hoje. O portão de citação reprova o cabeçalho até os testes existirem, o mesmo estado que a S0 descreveu. O texto não usa "planned", como a COS-01 da S0 pediu.

### Verificações pedidas pela ordem

- **Encaixe em pixel (`R2D-PIXEL-SNAP`, INBOX).** O desenho não o bloqueia. O encaixe é uma função aditiva sobre o quadrilátero antes do envio, e o padrão continua sem encaixe. Até lá a sombra que gira cintila em pixel art, que é o risco que o líder do consumidor aceitou. Nada na H0 promete pixel exato para sombra girando, e o texto não diz que promete.
- **Consumidor (L-21 global).** A cobertura das quatro formas do consumidor foi conferida pelo que o texto permite, não pelo uso de nenhum repositório consumidor. A forma (d), sombra sobre outros sprites, depende da regra do segmento (CRI-H01).
- **Obrigação do H0 sobre o determinismo.** A promessa da D-W8-27 está no texto; a da D-W8-16 ("PIXEL idêntico..."), não. O texto diz: mesmo binário, byte a byte; entre sistemas, 4 ULP para componentes não nulos; pixel igual na mesma máquina e driver, nunca entre placas.

---

## 6. Sonda de colisão e de sintaxe

**Colisão de nome público (`public_name_collision_test`, por `run_ctest.sh`, dentro de `flock -w 7200 -E 99 /var/tmp/cto-w8/heavy.lock`, em contêiner):**

| Rodada | Cópia | Conteúdo | rc_configure | rc_ctest | Log | Resultado |
|---|---|---|---|---|---|---|
| 1 | `fixture` (negativa) | cercas da seção 2, sem nome de controle | 0 | **0** | `col.log` | passou; 0 linhas `COLISAO` |
| 1 | `fixture-pos` (controle positivo) | as mesmas cercas, mais `gltfx_probe_control` com `major` numa lista de duas linhas | 0 | **8** | `col-pos.log` | `COLISAO /usr/include/sys/sysmacros.h:60:major` |

- O controle positivo reprovou, como exige o item 4 da ordem. A sonda não está quebrada.
- Nenhum nome colidiu, então não houve segunda rodada da negativa.
- **Repetição com os nomes finais** (`piece_height`, `azimuth`, `unknown_option`, `too_many_vertices` quando couber, e `shadow_segments`): pendente, no passo 0 da fatia que os usa (D-W8-303).
- Contagem de nomes públicos: o controle diz `FASE nomes publicos: 381` (com o `major` a mais). A negativa não mostra a contagem, porque o `ctest` esconde a saída de teste que passa.
- Leitura: `ctest` esconde a saída de teste que passa. A prova da negativa é o `rc_ctest=0`, não uma linha de contagem.
- Limite declarado: nomes de parâmetro não são varridos (`tests/tools/check_public_name_collision.py:37-38`). Ver R6, seção 3.

**Sintaxe no host (`g++` e `clang++`, `-std=c++23 -fsyntax-only -Wall -Wextra -Werror`, com shim de `GLINTFX_API` vazio, sem executar binário):**

| Probe | Arquivo | g++ rc | clang++ rc | Resultado |
|---|---|---|---|---|
| Uso de todos os nomes propostos | `/var/tmp/cto-w8/h0/probe/base.cpp` | 0 | 0 | compila |
| Sobrecarga única para duas luzes, com `{}` | `/var/tmp/cto-w8/h0/probe/ambig.cpp` | 1 | 1 | "is ambiguous" nos dois |

- A segunda linha é o controle que justifica os nomes distintos (`gltfx_project_directional_shadow` e `gltfx_project_point_shadow`). Uma sobrecarga única com `{}` é ambígua, como a S0 mediu em IMP-06.
- Um primeiro `clang++` deu erro de função não usada no próprio probe (`draw`). Foi erro do probe, corrigido com `[[maybe_unused]]`, e o resultado acima é o de depois.

**Não rodado:** nenhum teste de produto (`projected_shadow_test`, `shadow_group_test` e `draw2d_shadow_parity_test` não existem). Nenhum `preci`. Nada de execução.

---

## 7. Divergências e perguntas

Quem decide é o CTO, e o líder onde o item diz. O revisor não decide nenhum deles. Cada item traz o que o plano diz, o que o texto propõe e a recomendação, que é só recomendação.

**DIV-H01. Quem participa do escurecimento único (D-W8-16, D-W8-28; pergunta extra da H0).** **Decidido (D-W8-319): opção (a).**
- O plano diz: grupo de sombra com stencil; uma peça comum desenhada por cima libera; forma do grupo, par begin/end ou chave por peça, a decidir.
- O texto propõe: segmento entre barreiras; todas as peças do segmento participam; sem par e sem chave.
- Motivo: CRI-H01. Com par begin/end ou chave, o contraexemplo da seção 4.2 quebra a promessa.
- Opções: (a) segmento (recomendado); (b) par begin/end, com todas as peças do grupo aberto participando; (c) chave por peça.
- Recomendação: (a). Quem decide: CTO.

**DIV-H02. Token `light_below_piece` (D-W8-290).** **Decidido (D-W8-320): sai do relatório.**
- O plano diz: o token entra na lista única da onda, da H2.
- O texto propõe: a recusa fica no `gltfx_err` da função pura (`invalid_argument`, `rejected_value()` "light_height"), e o token sai da lista de relatório.
- Motivo: IMP-H02. Alternativa: `draw` recebe luz e peça, e aí o token volta a ter caminho de relatório, mas a projeção vai para dentro do desenhador.
- Quem decide: CTO.

**DIV-H03. A premissa de multiplicação e a cor da sombra (D-W8-16).** **Decidido (D-W8-321): cor é parâmetro obrigatório, misturada por `src-over`.**
- O plano diz: "a multiplicação da D-W8-13 basta, sem modo novo".
- O texto propõe: a sombra é `src-over` com a cor passada (cor preta dá multiplicação; cor colorida mistura). A cor é parâmetro.
- Motivo: IMP-H07. A D-W8-13 define modulação do texel, não multiplicação do destino.
- Alternativa: cor fixa preta, com alfa como única opacidade. Menos API, e o consumidor não tem sombra colorida.
- Quem decide: CTO.

**DIV-H04. Nome e sentido do contador (D-W8-30).** **Decidido (D-W8-322): `shadow_segments`.**
- O plano (e o texto da S0, §2) diz: `shadow_groups`, "shadow groups drawn in the frame".
- O texto propõe: `shadow_segments`, contado na barreira que fecha o segmento, com sombra.
- Motivo: L-28 e DIV-H01. Se a opção (b) for escolhida, o nome volta a grupo.
- Quem decide: CTO. A S0 já aceita, e o texto da S0 precisa do mesmo nome se mudar.

**Q-H01. Tolerância para componentes que devem ser zero (D-W8-27).** **Decidido (D-W8-323): escala da conta, sem piso absoluto.** A regra "dentro de 4 ULP" não define a tolerância no zero. Número de piso absoluto: CTO. Sugestão sem medida, como a S0 fez com a tolerância da elipse: `1e-12` unidades de mundo. Quem decide: CTO.

**Q-H02. Faixas e recusas de luz (IMP-H01).** **Decidido (D-W8-324):** elevação em (0, 90]; altura da luz estritamente maior que a da peça; altura da peça finita e não negativa. Elevação em (0, 90] graus; altura da luz finita e maior que a da peça; altura da peça finita e não negativa. Quem decide: CTO.

**Q-H03. Superfície pública da H4 (IMP-H06).** **Decidido (D-W8-325):** a luz é função pura do núcleo; a lei de queda é escolha obrigatória do consumidor, entre `linear` e `smooth`, sem padrão (seção 2.1). Opções para a lei de queda e para a luz da peça:
- (a) queda linear até um alcance, com alcance e cor como campos da luz pontual (limitada, barata; recomendada);
- (b) queda do inverso do quadrado, sem alcance (precisa de corte);
- (c) o consumidor manda a cor por vértice, e a biblioteca não faz luz.
- Quem decide: CTO. Se a escolha mudar o que o consumidor vê, o CTO leva ao líder por `AskUserQuestion`.

---

## 8. FATO e INFERÊNCIA

**FATO (medido ou lido, com fonte):**
- Árvore revisada: `542df882bf07691fb9b18f5c498dc2beed3faeb2`, o HEAD do ramo `onda-w8` no momento do clone (`git -C /var/tmp/cto-w8/h0/fixture rev-parse HEAD`). A árvore real tinha alterações não commitadas de outros agentes; o clone não as pega.
- `plano-fechamento-w8.md`: md5 `647a3e354e4c3ca5f4ccc36f436b286c`, conferido no início. `plano-fechamento-w8-r2.md`: md5 `44a4d21abb45b6387e87b0411ddbddd9`, conferido no início.
- Disco, `btrfs filesystem usage /`: `Device unallocated` 21,23 GiB; `Free (estimated)` min 24,45 GiB no início (acima do limite de 20 GiB; medido de novo às 13:22, `min` 24,45 GiB).
- Valores numéricos medidos por `python3 -I` sobre `double`: `cos(pi/2) = 6.123233995736766e-17`; comprimento de elevação 90 `1.2246467991473533e-15`; componente de azimute 90 `-1.2246467991473535e-15`; elevação 30 `34.64101615137755`; azimute 0 e elevação 45 com altura 20 dá `-20.000000000000004`.
- Aritmética do H2 conferida à mão: `S = L + (P - L) * H / (H - h)`, com `L = (10,-30)`, `H = 40`, `h = 20`, dá `(0,30)` e `(20,30)`.
- Estado do repositório: `tests/CMakeLists.txt` e `tests/tools/check_examples_consume.py` têm alterações não commitadas de outros agentes. O clone do H0 não as contém.
- Trava: `/var/tmp/cto-w8/heavy.lock` estava ocupada por outros agentes (D7-fix1, R0, R6, SG) durante toda a sessão de sondas.
- Cercas: `include/glintfx/core/light_2d.hpp`, `include/glintfx/core/projected_shadow.hpp`, dois métodos de sombra em `renderer_2d.hpp` e o contador `shadow_segments` em `frame_2d_report.hpp`, todos no fixture, nunca na árvore real.
- Regras de conta: `public_name_collision_test` e `run_ctest.sh` lidos em `tests/CMakeLists.txt:3132` e `d1fixb/scripts/run_ctest.sh`.
- Leis e manuais lidos: `GODS_LAWS.md` do projeto (L-12, L-17, L-19, L-21, L-22, L-26, L-27, L-28, L-29, L-32), `leis-paralelo.md` (L-09, L-11, L-12, L-27), `ordem-comum-paralelo.md`, `docs/api-conventions.md` (R1 a R10), o parecer da S0 (modelo) e as decisões r2 §7, §12 e r3.

**INFERÊNCIA (não medida):**
- Que o custo do modelo de segmento (a) fica limitado a partir da primeira sombra do segmento, na ordem de desenho. Vem da lógica do estêncil, não de implementação.
- Que a cobertura das quatro formas do consumidor é suficiente. Vem da leitura do texto, não de execução.
- Que a assinatura de `draw_shadow_texture` sobrevive à errata da T0. Depende de a T0 fechar a ordem dos parâmetros.
- Que a luz pontual de alcance linear (Q-H03, opção a) é suficiente para a H4. Não foi medida nem desenhada.

**NÃO executado, por ordem:** nenhum teste de produto compilado ou rodado. Nenhum commit, push, `TODO.md` ou `DECISOES_AUTONOMAS.md`. Nenhum toque em `watchcode`. Nada na árvore real. A sonda de sintaxe no host (`g++ -fsyntax-only`) é o único comando fora dos portões, e não executa binário.

md5 do corpo deste parecer, sem esta linha: f2c3d7b7956f2e1ea767b9f6f2ab7e38 (corpo de origem, antes das errata; o texto desta cópia traz as errata e tem outro md5).
Arquivo de origem: `/var/tmp/cto-w8/h0/auditoria-api-sprite-shade.md`, md5 `a36080994717c465b6e160abe14d9f9e`.
