<!-- Origem: /var/tmp/cto-w8/t0/auditoria-api-texture.md, md5 f8d6415d18fb06bb9707815ebafdc709. Texto do parecer com as errata aplicadas no próprio texto (a errata vence o parecer). Ver "Errata aplicadas" logo abaixo. -->

# Auditoria de API: R2D-TEXTURE (T0), textura 2D, atlas e desenho com textura

**Revisor:** revisor de API da T0 (distinto do CTO, do revisor da S0 e de qualquer implementador da W8, L-12).
**Natureza:** DESENHO. Aceito pelo CTO com as errata abaixo. Esta cópia em `docs/` é o texto que a T1 copia para o repositório (D-W8-303).
**Árvore revisada:** `bd179b077fbb4000cb8ae26148cb15fe1a266afd` (ramo `onda-w8`), clone `/var/tmp/cto-w8/t0/fixture`. Durante o trabalho a árvore real passou de `a2a63ea4` para `bd179b07` (só documentação: `git diff a2a63ea4 bd179b07 -- include src` vazio); as duas cópias de sonda foram refeitas no mesmo SHA.
**Modelo:** parecer da S0 (`/var/tmp/cto-w8/s0/auditoria-api-shapes.md`), mesmo formato de 8 seções.
**Carimbo de início:** [11/10/26 - 12:13:13]

**Errata aplicadas (CTO; r3 D-W8-288 a 303, r4 D-W8-304, D-W8-308):**
- **D-W8-288 (DIV-T01):** destruir a textura adia a liberação para o fim do quadro, sem enviar o lote. Atualizar não envia o lote. Seção 2, `texture_2d.hpp` e `renderer_2d.hpp`.
- **D-W8-289 (DIV-T02, opção c):** a região é valor (retângulo em texels e espelhamento). A textura entra em cada chamada de desenho. O atlas usa o slot com geração.
- **D-W8-290 (Q-T01):** lista única de motivos de textura, acrescentada depois da S0. Sem `light_below_piece` (D-W8-334).
- **D-W8-291 (Q-T02):** código `no_free_space`, no fim de `gltfx_err_code`.
- **D-W8-292 (Q-T03):** `texture_switches` conta toda mudança de ligação, inclusive de e para nenhuma textura.
- **D-W8-293 e D-W8-304 (Q-T06):** a promessa do filtro é pelo formato e é provada em cada sistema. Texto público em `texture_2d_desc.hpp` (D-W8-308). A pergunta ao líder (seção 3 do parecer) foi respondida com "pesquise e implemente"; a escolha (d) é decisão do CTO, com ratificação retroativa pendente.
- **D-W8-294 (Q-T04):** `mipmap_levels` é contagem nos dois descritores; `gltfx_all_mipmap_levels` pede a cadeia inteira (valor `0xFFFFFFFFU`, D-W8-353).
- **D-W8-295 (Q-T05):** `unspecified = 0` é recusado pelo nome; os formatos são 1 a 4; o filtro tem padrão `linear`.
- **D-W8-296 (Q-T07):** tipos de texel em `draw2d/texture_region.hpp`; enum de formato em `core/image_view.hpp`. Sem mudança de texto.
- **D-W8-297 (Q-T08):** espelhamento por extenso: `flip_left_right` e `flip_top_bottom`. O valor dos dois eixos é `flip_left_right_and_top_bottom` (D-W8-353).
- **D-W8-298 (Q-T09):** regra `margem >= 2^k`, com teste na fronteira. Se reprovar com `2^k`, a regra vira `2^(k+1) - 1`.
- **D-W8-299 (IMP-T07):** a justificativa do WebRender sai do texto.
- **D-W8-300 (IMP-T03, IMP-T04, IMP-T10, IMP-T11, COS-T01 a COS-T03):** regras e textos que entram nas fatias indicadas; marcadores de planejamento saem do texto público.
- **D-W8-301 e D-W8-302:** a T2 se parte em T2a (handle, subida e descritor) e T2b (amostragem e texto do filtro). A T1 entra na fila logo depois da S1.
- **D-W8-353 a 356 (r13):** nomes finais: `flip_left_right_and_top_bottom`, `release_slot`, `gltfx_all_mipmap_levels = 0xFFFFFFFFU` (com a regra reescrita), `draw_atlas_image` e `gltfx_light_tint` (no parecer H0). Os desenhos com textura passam a ter quatro parâmetros ou menos, por `gltfx_texture_paint` e `gltfx_atlas_paint` (D-W8-354). Ver a nota D-W8-354 ao lado de cada assinatura trocada. As seções 3 a 7 são análise e citam, onde indicam, os nomes anteriores; o texto vigente é o da seção 2.
- **Errata da T2a-B (CTO, 12/10/2026; uma cerca e uma fonte):** a cerca vira o texto que a T2a publica, para que nenhuma fatia seguinte reescreva o que já saiu. `texture_2d_desc.hpp`: o comentário de `linear`, o de `mipmap_levels` (a contagem dos níveis REDUZIDOS, com o exemplo 256 x 1) e o de `gltfx_all_mipmap_levels` passam a ser os da T2a-A; o parágrafo FILTERING AND COLOUR continua aqui e entra na T2b, como bloco, sem mudar uma linha do resto. `renderer_2d.hpp`: o comentário de `create_texture` passa a ser o da T2a-B (a ordem das recusas, o erro do contexto, `platform_failure` com o código GL e o estado de desempacotamento). `texture_2d.hpp`: sem mudança de texto; a T2a publica a cerca SEM as linhas 18 a 36 do bloco (os três itens de LIFETIME depois do primeiro, a linha `//` e o `Proved by` de `renderer_2d_impl_test`), e a T3 as acrescenta como estão, depois de `//     rule as the window and the context).`, junto com o teste que as prova. Os `#include` de `texture_region.hpp` e `texture_atlas.hpp` e os blocos de `update_texture` e dos desenhos entram nas fatias deles, como estão.
- **Errata da T2a-fix1 (CTO, 12/10/2026):** os `Proved by` de `create_texture` passam a citar o que cada teste exercita de fato: `texture_upload_test` prova a criação para a qual o método encaminha, sobre GL falso; `draw2d_parity_test` prova o próprio método e a alça sobre um contexto real (revisão N1 da T2a, achados 5 e 6).

- **Errata da T2b (CTO, 12/10/2026; D-W8-372 e D-W8-374):** o parágrafo FILTERING AND COLOUR de `texture_2d_desc.hpp` NÃO entra na T2b. Ele termina em `Proved by: draw2d_texture_parity_test`, e esse teste (as células de filtro da D-W8-305, que desenham com textura) nasce na T4b; citar teste que ainda não existe reprova pela D-W8-374. O parágrafo entra na T4b, como bloco, junto com o teste que ele cita. A T2b publica só o comportamento interno (a tabela de `src/draw2d/texture_sampling.hpp`: filtro de ampliação e de redução, com e sem níveis reduzidos, e as bordas presas ao texel da borda, `GL_CLAMP_TO_EDGE`); o único texto público que muda nela é o comentário de `linear` (e, na T2b-fix1, também o de `nearest`), porque o filtro com níveis reduzidos é o trilinear (`GL_LINEAR_MIPMAP_LINEAR`, decisão D-T2b-3 da ordem T2b, registrada pelo main em `DECISOES_AUTONOMAS.md` em 12/10). O texto vigente é o da errata da T2b-fix1, logo abaixo. Porta de mão única: vai à ratificação do líder.

- **Errata da T2b-fix1 (CTO, 12/10/2026; achado F-1 da revisão N1 da T2b):** a frase da T2b sobre `linear` dizia que o pixel mistura os dois níveis mais próximos sempre que há níveis reduzidos; na AMPLIAÇÃO isso é falso (o GL só usa o nível de tamanho cheio e não aceita filtro de níveis na ampliação). Os comentários de `nearest` e de `linear`, no cabeçalho `texture_2d_desc.hpp` e na cerca desta página, passam a dizer o que acontece em cada caso: desenhado no tamanho ou maior, desenhado menor sem níveis reduzidos, e desenhado menor com eles. Linha a linha, trocadas uma vez e conferidas iguais nos dois lugares. Porta de mão única: vai à ratificação do líder.

- **Errata da T3, etapa A (CTO, 12/10/2026; D-W8-288):** o bloco de liberação de `texture_2d.hpp` entra como está na cerca (as linhas que a errata da T2a-B reservou à T3), junto com o teste que o prova (`renderer_2d_impl_test`). A fila que segura a textura destruída com o quadro aberto mora em `src/draw2d/texture_release_queue.hpp` e corre pelos próprios registros: segurar não pede memória e não falha. O relatório do quadro NÃO ganha contador de liberações adiadas: o layout da D-W8-30 é fechado para a onda, a DIV-T01 diz que o relatório "pode" contar (não que deve), e a liberação que falha já é dita pelo evento `texture_release_failed`. O último item do bloco cita `update_texture` em `renderer_2d.hpp`; o método nasce na etapa C desta mesma fatia.

- **Errata da T3, etapa C (CTO, 12/10/2026; D-W8-288):** a cerca de `update_texture` passa a ser o texto que a T3 publica. (1) "A texture of another renderer is refused and counted as `texture_other_renderer`" era falso para este método: `update_texture` devolve `gltfx_rslt<void>`, e o relatório conta peças, não chamadas de textura (`frame_2d_report.hpp`, "WHAT A PIECE IS"); a textura de outro desenhador, e a alça que não está aberta, são recusadas como `invalid_argument`, `rejected_value()` "texture", sem contar. O motivo `texture_other_renderer` continua sendo dos métodos de desenho (T4). (2) A lista de recusas entra na ordem em que se aplicam, a mesma de `create_texture` para a vista e o contexto, mais "rect", e as falhas de memória e da placa com o token "texture_update". (3) Textura com níveis reduzidos: a mudança recalcula os texels reduzidos que ela toca, exatamente como a criação os calcula; para isso, a textura ATUALIZÁVEL com níveis reduzidos guarda os texels de todos os níveis na memória da biblioteca, com o custo corrigido na T3-fix1 (errata abaixo). (3a) Campo público novo `gltfx_texture_2d_desc::updatable`, desligado por padrão (decisão do líder, verbatim: "Só quem declarar (Recomendado)"; como o sokol, o bgfx, o WebGPU, o Vulkan, o Direct3D e o Metal): ele governa `update_texture` para TODA textura, e não só as com níveis reduzidos (refinamento do advisor); atualizar uma textura que não o declarou é recusado como `invalid_argument`, `rejected_value()` "updatable", logo depois de "texture"; os texels só ficam guardados quando a textura é atualizável E tem níveis reduzidos. A cerca de `create_texture` e a do campo novo dizem isso; a sonda de colisão de nome público roda com ele. (3b) Imagem sem largura ou sem altura é atualização vazia: depois das outras verificações (a vista e o retângulo), a chamada dá sucesso e não faz nada, sem texel escrito, sem chamada GL e sem tornar o contexto corrente (décima consulta ao advisor: zero é recusado para OBJETOS que não podem existir e aceito para OPERAÇÕES vazias); o relatório do quadro não conta atualizações de textura, então não há contador a dizer zero. (4) O estado de desempacotamento e a ligação deixados são os de `create_texture`. (5) Os `Proved by` citam os testes que existem no commit.

- **Errata da T3-fix1 (CTO, 14/10/2026; D-W8-383 e D-W8-384; achados IMPORTANTE-1 e IMPORTANTE-2 da revisão N1 da T3):** (1) O custo de memória: a cerca de `updatable` e a de `update_texture` diziam "about 4/3 of width x height x 4 bytes", que só vale para textura quadrada com todos os níveis; a conta da revisão dá 1,999 para 1024 × 1 com 10 níveis reduzidos. As duas passam a dar a regra exata (4 bytes por texel dos níveis 0 a k, o nível i com max(1, largura >> i) × max(1, altura >> i) texels), o intervalo (pelo menos largura × altura × 4 bytes, sempre menos que o dobro), um exemplo de cada extremo e um do meio, e que é memória da biblioteca na CPU, além do que o driver guardar. (2) A recusa da placa: o bloco guardado mudava antes da escrita e ficava com os texels recusados, que um update posterior mandava à placa. A cerca de `update_texture` passa a dizer o estado depois de cada recusa: sem memória na biblioteca, nada tocado; recusa da placa, a memória da biblioteca volta exatamente ao que era, e os texels recusados nunca chegam à placa depois; na placa, o retângulo e os texels reduzidos que ele toca ficam indeterminados até um update que o cubra dar certo; com out_of_memory da placa, o OpenGL deixa o estado inteiro indefinido, e o caminho de volta é refazer a textura. Todo erro GL que a escrita levantou é lido pela chamada, e nenhum fica para uma chamada seguinte. Promessa pública: vai à ratificação do líder como errata.

- **Errata da T4b-A (CTO, 14/10/2026; D-W8-455 a 462 e D-W8-464):** (1) a tinta que o consumidor não escolhe é BRANCO OPACO, `{1, 1, 1, 1}`, em `gltfx_texture_paint::tint` e em `gltfx_textured_vertex_2d::tint`: decisão do líder via AskUserQuestion, 14/10/2026 (D-W8-464); a cerca abaixo já mostra o valor novo. (2) A cerca de `texture_region.hpp` entra com o `Proved by:` de `texture_region_uv_test` (a leitura em pixels de cada espelho, `texture_region_mirror_test`, entra na T4b-B junto com o teste, D-W8-374); sem a frase de `draw_shadow_texture`, método da H3 que ainda não existe; e com as cinco recusas que tocam a região (D-W8-461). (3) `degenerate_quad` inclui o quadrilátero côncavo: o critério da cerca é "não define mapeamento projetivo", e o côncavo não define (D-W8-460). A tinta de `gltfx_atlas_paint`, mais abaixo, continua `tint{}` até decisão própria.
- **Errata da T4b-B1 (CTO, 14/10/2026; D-W8-468 a 474):** `draw_texture` e `draw_texture_quad` entram em `renderer_2d.hpp` com quatro diferenças contra a cerca abaixo. (1) A lista de recusas vem completa e na ordem em que se aplica (D-W8-472): `not_a_number` e `infinite` (retângulo ou cantos, região e tinta), `negative_size`, `unknown_option`, `texture_not_open`, `texture_other_renderer`, `region_out_of_range`, `degenerate_quad` (só o quadrilátero) e `pixel_position_out_of_range`. A cerca citava só a origem da região e o espelho, e é a ordem de `src/draw2d/texture_region_check.hpp` (T4b-A) que vale. (2) A frase de que a troca de textura nunca reordena, que a cerca não tinha e que vem da confirmação da D-W8-13 (mais abaixo, nesta página): a peça texturizada entra na ordem de pintura como qualquer outra. (3) O `Proved by:` cita só `renderer_2d_impl_test`, que existe na T4b-B1; o de `draw2d_texture_parity_test` entra na T4b-B2, junto com o teste (D-W8-374). (4) `texture_switches` entra no FIM de `gltfx_frame_2d_report`, depois dos quatro contadores da T4b-A (o layout da S0 o punha logo depois de `pieces_refused_pixel_position_out_of_range`, posição que a T4b-A já ocupou; D-W8-471), e é contado onde a ligação é feita: cada textura que a biblioteca liga para desenhar conta uma vez, a ida a nenhuma textura e a primeira textura de cada flush incluídas, porque todo flush começa sem textura ligada (D-W8-292, com a frase pública refinada pela D-W8-501 da T4b-B1-fix1). Porta de mão única: vai à ratificação do líder.
- **Errata da T4b-A-fix1 (CTO, 14/10/2026; D-W8-493 a 500; revisão N1 da T4b-A, I-5):** a cerca de `texture_region.hpp`, abaixo, é reescrita NO LUGAR com o texto do cabeçalho publicado, como a errata da T3-fix1 fez com a de `update_texture`: o `Proved by:` de `texture_region_uv_test` (com a prova, na compilação, de que tipo de texel e tipo de mundo não se misturam); sem a frase de `draw_shadow_texture`; as recusas de origem OU tamanho que não são número finito, de tamanho negativo e de região fora da textura; "quatro números e um enumerador" no lugar de "dois números e um nome"; sem o `#include` de `rect.hpp`, que o cabeçalho não usa; e sem a citação de R7, que trata de tokens e não de nomes de espelhamento. A leitura em pixels de cada espelho, que a errata da T4b-A chamava `texture_region_mirror_test`, não terá esse nome: é uma célula de `draw2d_texture_parity_test` (T4b-B2), e o `Proved by:` dela entra na cerca e no cabeçalho junto com o teste (D-W8-374). Porta de mão única: vai à ratificação do líder.
- **Errata da T4b-B1-fix1 (CTO, 14/10/2026; D-W8-501 a 508; revisão N1 da T4b-B1, I-1 e C-1):** (1) a frase pública de `texture_switches` em `draw_texture` passa a dizer o que o contador conta: cada textura que a biblioteca liga para desenhar conta uma vez, a troca entre duas chamadas de desenho, a ida e a volta de nenhuma textura, e a PRIMEIRA textura de cada flush (a contagem recomeça de nenhuma textura a cada flush). A frase da cerca, "each change of the bound texture between two calls", dava 2 para as texturas A, B, A, A, e o contador dá 3, como `frame_2d_report.hpp` já dizia. O contador não muda (D-W8-471): ele mede o custo que a placa paga, e uma contagem "entre chamadas do consumidor" exigiria guardar estado de um flush para o outro. (2) O item (2) da errata da T4b-B1 passa a dizer de onde vem a frase de que a troca nunca reordena (a confirmação da D-W8-13), e o item (4), a contagem refinada. Porta de mão única: a frase pública vai à ratificação do líder.
- **Errata da T4b-B2 (CTO, 19/10/2026; D-W8-475 a 480 e D-W8-545 a 550):** nasce `tests/parity/draw2d_texture_parity_test.cpp`, a prova por pixel do desenho com textura, e com ele entram os textos que o citam. (1) O parágrafo FILTERING AND COLOUR de `texture_2d_desc.hpp` entra como a cerca acima o fixa, linha por linha (errata da T2b, D-W8-374). (2) `draw_texture`, `draw_texture_quad`, a leitura de cada espelho e a tinta (`texture_region.hpp`) ganham o `Proved by:` de `draw2d_texture_parity_test`; o nome `texture_region_mirror_test` não existe, a leitura em pixels dos espelhos é célula desse teste (errata da fix1); e a promessa do halo, que a fix1 deixou para cá (D-W8-499), passa a citar as células do halo e da tinta (D-W8-549). (3) As cercas de `draw_texture`, `draw_texture_quad` e `texture_region.hpp` são reescritas NO LUGAR com o texto do cabeçalho publicado (D-W8-545, como a errata da fix1 fez): a cerca de `draw_texture` ainda trazia "each change of the bound texture between two calls", o gêmeo da frase velha que a B1-fix1 trocou no cabeçalho (revisão N1 da B1-fix1, I-1). (4) A errata da B1-fix1 cita D-W8-501 a 508, as que existem (D-W8-546). (5) A frase "(each flush starts with none bound)" descrevia o modelo do contador e não o GL, onde o consumidor pode deixar uma textura ligada entre dois flushes; passa a ser "(the count starts again from no texture at every flush)" (D-W8-547). (6) A célula `filter_linear_no_dark_halo` da D-W8-305 espera o valor DA FÓRMULA em cada modo: em x = 100 o texel vermelho pesa 1 - 0,505 = 0,495; com a superfície que codifica, 255 * srgb(0,495) = 186,7; com a que não codifica, 255 * 0,495 = 126,2; a tabela da D-W8-305 dizia 188,3, o peso do BRANCO de `filter_linear_srgb_order` em x = 100. (7) PONTO DE RATIFICAÇÃO DO LÍDER, sem decisão aqui (D-W8-548): um flush cujo desenho termina em erro GL não soma as suas trocas de textura (nem as suas chamadas de desenho) ao relatório, e as frases públicas não ressalvavam esse caso: a de `draw_texture` (`renderer_2d.hpp`, "Each texture the library binds to draw counts once in texture_switches") e as de `texture_switches` e `draw_calls` (`frame_2d_report.hpp`), lidas sem exceção, prometiam a contagem também no erro (revisão N1 da T4b-B2, IMPORTANTE-1; corrigido no lugar pela errata da T4b-B2-fix1, que decide). Porta de mão única: as frases públicas vão à ratificação do líder.
- **Errata da T4b-B2-fix1 (CTO, 20/10/2026; D-W8-581 a 585; revisão N1 da T4b-B2, IMPORTANTE-1 e COSMÉTICOS 1 a 3):** (1) D-W8-548 decidida pelo CTO, para a ratificação do líder (D-W8-581): os dois contadores do que a placa paga, `draw_calls` e `texture_switches`, contam só o flush que a placa aceitou; um flush cujos dados ou cujo desenho a placa recusa não soma nenhuma das suas chamadas de desenho nem das suas trocas, e as suas peças contam em `pieces_dropped_graphics_failure`, como já contavam. O código não muda; três frases públicas passam a dizer isso: a de `draw_texture` (`renderer_2d.hpp`) e as de `draw_calls` e `texture_switches` (`frame_2d_report.hpp`). A razão: no relatório, "reached the graphics card" já quer dizer "a placa aceitou" (`pieces_drawn` e `pieces_dropped_graphics_failure`), e contar no erro daria a um flush sem nenhuma peça desenhada chamadas de desenho que "chegaram à placa". Alternativa nomeada, não adotada: contar no erro, como o sokol (`sg_frame_stats`), o bgfx (`bgfx::Stats::numDraw`) e o three.js (`renderer.info.render.calls`) contam na submissão; nenhum deles lê erro GL fora do modo de depuração, e contar no erro exigiria mudar o retorno de `draw_batch`. (2) A prova é o caso novo `renderer_2d_impl_a_refused_flush_adds_no_draw_call_and_no_switch`, citado no `Proved by:` de `draw_texture`; o vermelho dele é o dos mutantes que implementam a alternativa (D-W8-583). (3) O item (7) da errata da T4b-B2 é corrigido no lugar: ele dizia que nenhuma frase pública tratava do erro, e as três, sem ressalva, prometiam a contagem nele. (4) A cerca de `draw_texture`, abaixo, é reescrita NO LUGAR com o texto do cabeçalho (D-W8-545). Gêmeos que NÃO mudam (D-W8-582): a cerca de `draw_calls` em `docs/auditoria-api-draw2d.md` é o layout da B0, sem os contadores da W8; o texto de `texture_switches` em `docs/auditoria-api-shapes.md` é a proposta da S0, já corrigida pela errata da T4b-B1. (5) O trecho de `renderer_2d.hpp` da seção 2 traz, além dos quatro métodos que o cabeçalho já publica e que são iguais a ele, o `#include` de `texture_atlas.hpp`, `draw_atlas_image`, `fill_textured_triangles` e `fill_indexed_textured_triangles`: são a proposta da T0 para as fatias seguintes, e ainda não estão no cabeçalho (COSMÉTICO-2). (6) A seção 3 dizia "Dois dos novos" e listava seis nomes; passa a dizer seis (COSMÉTICO-3, D-W8-584). Porta de mão única: as três frases vão à ratificação do líder.
- **Errata da T4b-A-PHRASE (CTO, 20/10/2026; D-W8-653 a 660; decisão do líder sobre a D-W8-555 por AskUserQuestion, "Frase completa (Recomendado)"):** a cerca de `draw_texture_quad`, mais abaixo, é reescrita NO LUGAR com o texto do cabeçalho publicado (precedente D-W8-545). (1) A frase de exatidão que a T4b-A-fix4 cortou volta: se os cantos definem um mapeamento projetivo é decidido de forma exata, para quaisquer cantos finitos, em qualquer tamanho, lugar e mistura de magnitudes (T4b-A-fix4, D-W8-551 a 553; com o filtro da T4b-A-FILTER, o caminho exato continua sendo o árbitro de todo sinal que o filtro não certifica). (2) A única recusa de um quadrilátero que TEM mapeamento passa a ser dita: as diagonais se cruzam tão perto de um canto (a menos de cerca de 2^-1025 do comprimento da diagonal) que um peso projetivo, arredondado a double, passaria do maior double; conta como `degenerate_quad`, como já contava. Era a lacuna que fez a D-W8-555 cortar a frase na fix4: exatidão dita ao lado de uma recusa que depende de arredondamento e que o texto público não citava. (3) O `Proved by:` ganha `projective_quad_test` (a decisão nos kites de magnitude mista e em todo quadrilátero pequeno de cantos inteiros; o quadrilátero cujo peso passaria do maior double, recusado). Os gêmeos mudam junto (L-17 global): o comentário do motivo `degenerate_quad` e o de `pieces_refused` em `frame_2d_report.hpp`, e três comentários internos (`texture_region_check.hpp`, `textured_quad_corners.hpp`, `renderer_2d_impl_textures.cpp`). Nenhum código muda. A wiki não tem página de textura e não muda.
- **Errata da T5 (CTO, 20/10/2026; D-W8-671 a D-W8-680 e D-W8-740; etapas T5-A e T5-B):** `fill_textured_triangles` e `fill_indexed_textured_triangles` entram em `renderer_2d.hpp`, e a cerca deles, abaixo, é reescrita NO LUGAR com o texto do cabeçalho (D-W8-545). Diferenças contra a proposta da T0: (1) a ordem das recusas fica escrita por inteiro, sete passos, a do `fill_triangles` com a coordenada do texel na regra dos valores (D-W8-673), depois as duas da textura, depois `region_out_of_range` para a coordenada fora de 0..largura por 0..altura, bordas dentro (D-W8-672: recusada, nunca grampeada nem repetida; a repetição é a `R2D-TEXTURE-TILED`), depois a faixa de pixel; a proposta não tinha a recusa da faixa da textura nem a da faixa de pixel; (2) a leitura é AFIM em cada triângulo (q = 1), e o texto diz que o quadrilátero projetivo é `draw_texture_quad()`; (3) a chamada vazia não é peça, qualquer que seja a textura (D-W8-287); (4) o par textura e região do `draw_texture` não se aplica: cada vértice traz a própria coordenada; (5) a peça de triângulos texturizada conta a textura em `texture_switches` como o `draw_texture`, e logo depois de uma peça texturizada da mesma textura sai na mesma chamada de desenho; (6) pela D-W8-740 (pesquisa de 20/10: a documentação de SDL3, Dear ImGui, raylib, sokol_gl, SFML, Skia, NanoVG, pixi.js e Godot, e quinze queixas de usuários delas), a cerca diz também o que a proposta e essas nove bibliotecas calam: onde fica o texel (0, 0) e o centro do primeiro texel; que nenhum triângulo é descartado pela ordem dos seus vértices (face culling desligado, o estado de `flush()`), provado na TT1 com dois triângulos de orientações opostas; que a mistura é a de toda peça; e o custo, linear no número de vértices e índices (mais o custo fixo de toda peça), com a checagem e a cópia na própria chamada. A frase do custo é a única sem teste que meça o tempo: ela descreve o que as checagens e a cópia fazem, e o `Proved by:` dela prova só a cópia. O comentário de `gltfx_textured_vertex_2d` (cerca de `texture_region.hpp`, acima) ganha a faixa do texel e o `Proved by:`; a frase de `region_out_of_range` e a de `pieces_refused` em `frame_2d_report.hpp` ganham o gêmeo do texel do triângulo, e "WHAT A PIECE IS" ganha os dois métodos. O item (5) da errata da T4b-B2-fix1 ("ainda não estão no cabeçalho") deixa de valer para estes dois; `draw_atlas_image` continua proposta da T6. Porta de mão única: a frase pública e a recusa da D-W8-672 vão à ratificação do líder.
- **Errata da T4b-A-FLOAT (CTO, 20/10/2026; D-W8-716 a 735; revisão N1 da T4b-A-PHRASE, CRÍTICO-1, IMPORTANTE-1 e COSMÉTICO-1):** a cerca de `draw_texture_quad`, mais abaixo, é reescrita NO LUGAR com o texto do cabeçalho publicado (precedente D-W8-545). (1) O desenho converte o peso projetivo para float (`textured_quad_corners.cpp`, T4b-B1-fix1), e a frase da T4b-A-PHRASE dava como fronteira o maior double: entre o maior float e o maior double o quadrilátero era aceito e chegava ao vértice com peso infinito, contado como desenhado (a revisão mediu 3563 de 5623 quadriláteros aceitos numa amostra de escalas sorteadas). (2) Os quatro pesos de todo quadrilátero desenhado passam a chegar ao vértice multiplicados por UMA potência de dois, que não muda o mapeamento (o programa divide u q e v q por q): 1 enquanto o maior peso está abaixo de 2^32; acima, a que o traz para baixo de 2^32. Cada peso fica entre 2^-32 e 2^32 (o OpenGL 3.3, seção 2.1.1, promete coordenada de textura até 2^32 em magnitude), e nenhum chega à conversão para float fora da faixa do float. (3) A única recusa de um quadrilátero que TEM mapeamento passa a ser a do maior peso de 2^63 ou mais (as diagonais se cruzam a menos de cerca de 2^-64 do comprimento da diagonal, perto de um canto), que inclui a do peso acima do maior double; conta como `degenerate_quad`, como já contava. (4) A frase fala de uma família de quadriláteros, no plural (COSMÉTICO-1). (5) O `Proved by:` ganha `textured_quad_corners_test`, com os casos de maior peso 2^64/3 levado e de 2^65/3, 2^201/3 e 2^1030 recusados (IMPORTANTE-1). Os gêmeos mudam junto (L-17 global): o motivo `degenerate_quad` e `pieces_refused` em `frame_2d_report.hpp`, e dois comentários internos (`texture_region_check.hpp`, `renderer_2d_impl_textures.cpp`); `projective_quad.hpp` não muda, porque fala da recusa da função dele, que continua a do maior double. As linhas desta página com o critério antigo (IMP-T11 e Q-T01) são registro de achado e não mudam. Porta de mão única: a frase pública vai à ratificação do líder.

A seção 5 traz a tabela de cada achado com a decisão que o fecha. A seção 7 marca cada divergência e pergunta com a sua decisão.

---

## 1. Veredito

**APROVADO COM CONDIÇÕES, com 2 CRÍTICOS.** A T1 (vista de imagem e conversão de pixel) pode começar: `core/image_view.hpp` e o enum de formato estão confirmados e compilam. A T2 em diante depende de três coisas que não decido aqui: a barreira implícita da destruição (DIV-T01), a forma do campo `texture` na região (DIV-T02, CRI-T02) e a lista única de motivos de recusa (Q-T01).

- **CRI-T01 (divergência do plano, vai à seção 7):** D-W8-12 manda enviar o lote ao destruir textura. Esse envio é uma barreira, e a barreira muda a ordem de camadas. A promessa de `flush()` é publicada.
- **CRI-T02 (defeito do rascunho desta revisão):** `gltfx_texture_region::texture` como ponteiro para objeto que o consumidor destrói deixa a biblioteca desreferenciar memória liberada. A forma final é decisão, seção 7.
- **Rodada negativa da sonda de colisão: `rc_ctest=0` na primeira rodada.** Nenhum nome proposto colidiu, então nenhum nome foi corrigido e a sonda não produziu CRÍTICO de nome. O controle positivo (`major` em várias linhas) reprovou com `rc_ctest=8`.
- **Contagem:** CRÍTICO 2, IMPORTANTE 15, COSMÉTICO 3.
- **Decisões do plano:** confirmadas 8 (D-W8-10, 11, 13, 14, 23, 25, 26 e 252, com condições), divergente 1 (D-W8-12). A pergunta extra tem resposta: `image_view` mora em `core/`.

---

## 2. Texto proposto

Os seis cabeçalhos novos e o trecho da fachada, exatamente como foram copiados para a sonda (`/var/tmp/cto-w8/t0/proposta/`). Nada disto está no repositório. Nenhum texto proposto existia antes desta revisão: o plano descreve os cabeçalhos só pelos nomes e pelas decisões, então o texto foi redigido a partir das decisões D-W8-10 a 14, 23, 25, 26 e 252, e cada desvio das decisões está marcado no texto e nas seções 5 e 7.

### `include/glintfx/core/image_view.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// core/image_view.hpp - R2D-TEXTURE. A NON-OWNING view of
// pixels the caller already has in memory. Pure value type, no GL, no
// operating system: it lives in core/ next to gltfx_rgba8 (color.hpp),
// the 8-bit boundary type it describes.
//
// THE POINTER LIFETIME (docs/api-conventions.md R5): the library reads
// `pixels` only during the call that receives the view, and copies what
// it keeps. The caller owns the bytes and frees them; the library never
// frees them and never keeps the pointer.
//
// ROW STRIDE: `bytes_per_row` is the distance in BYTES from the first
// byte of one row to the first byte of the next. It may be larger than
// width * 4 (padded rows); it may not be smaller. A smaller value, or a
// `pixels` span shorter than height * bytes_per_row, is refused by the
// call that receives the view with invalid_argument, rejected_value()
// "bytes_per_row" or "pixels".
//
// PIXEL FORMAT: append-only (GODS_LAWS.md L-26). A value is never
// renumbered and never reused. Names are spelled out (GODS_LAWS.md
// L-28): the reader sees the color space and the alpha convention
// without opening this header. An 8-bit alpha-only format for glyph
// coverage is added later by appending a value, in R2D-TEXT.
// A value this build does not know is refused by name, never read as
// undefined behaviour (docs/api-conventions.md R4).

namespace glintfx {

enum class gltfx_pixel_format : std::uint8_t {
    // Not a format. It is the value of a view that does not say its format,
    // and it is REFUSED by name (invalid_argument, rejected_value()
    // "pixel_format"): a view without a format gets an error, never a wrong
    // colour.
    unspecified = 0,
    // Four 8-bit channels per pixel. Red, green and blue are sRGB-encoded
    // (the way a PNG or a camera stores them); alpha is straight, that
    // is, not multiplied into the colour.
    rgba8_srgb_straight = 1,
    // Same channels, sRGB-encoded, but the colour is ALREADY multiplied
    // by alpha in the encoding the caller chose.
    rgba8_srgb_premultiplied = 2,
    // Red, green and blue are linear light (the library's own convention,
    // core/color.hpp); alpha is straight.
    rgba8_linear_straight = 3,
    // Linear light, colour already multiplied by alpha.
    rgba8_linear_premultiplied = 4,
};

struct gltfx_image_view {
    // Read only during the call that receives this view (see above).
    std::span<const std::byte> pixels{};
    // Size in pixels. Zero is refused by the texture call, not here.
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Bytes from one row to the next (see above).
    std::size_t bytes_per_row = 0;
    // No default on purpose: the field must be written. The value
    // `unspecified` is refused by name.
    gltfx_pixel_format pixel_format = gltfx_pixel_format::unspecified;
};

} // namespace glintfx
```

### `include/glintfx/draw2d/texture_2d_desc.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// draw2d/texture_2d_desc.hpp - R2D-TEXTURE. What the
// texture is sampled with, decided when the texture is created.
//
// APPEND-ONLY (GODS_LAWS.md L-26, the same growth rule as
// gltfx_gfx_option): a value is never renumbered and never reused, and
// this enum GROWS in later fatias (the pixel-art mode is a new value in
// R2D-SCALE-PIXEL-ART, W9-B). Do not write a `switch` over it that has no
// default: a value this build does not know is refused by name, never
// read as undefined behaviour (docs/api-conventions.md R4).
//
// Proved by: texture_desc_validation_test (a value outside the enum is
// refused with rejected_value() "filter").

namespace glintfx {

enum class gltfx_texture_filter : std::uint8_t {
    // Each screen pixel takes the colour of ONE texel, never a blend. Drawn
    // at its size or larger, or smaller without reduced levels: the nearest
    // texel of the full-size level; drawn smaller with reduced levels: the
    // nearest texel of the nearest level. Sharp edges: the right choice for
    // pixel art drawn at its own size.
    nearest = 0,
    // Each screen pixel blends texels. Drawn at its size or larger: the four
    // nearest texels of the full-size level. Drawn smaller: the same without
    // reduced levels; with them, the four nearest texels of each of the two
    // nearest levels, and then those two results.
    linear = 1,
};

// FILTERING AND COLOUR: a texture keeps sRGB-encoded colour that is already
// multiplied by alpha (core/image_view.hpp). With the linear filter, the
// graphics card turns each texel into linear light BEFORE it blends the
// neighbours, so an edge between two colours does not come out darker
// than it should. OpenGL lets a driver do the conversion after the blend
// instead; this library treats that as a driver defect, and the test below
// checks the order on every system it is tested on, failing when a driver
// converts after the blend.
// Proved by: draw2d_texture_parity_test

struct gltfx_texture_2d_desc {
    gltfx_texture_filter filter = gltfx_texture_filter::linear;
    // Number of REDUCED levels (mipmaps) below the full-size image, the
    // same count the atlas uses. Zero: no mipmap. k: levels 1 to k, each
    // half the size of the one above it, rounded down and never below one
    // texel. A texture of width w and height h allows at most
    // floor(log2(max(w, h))) of them (a 256 x 1 texture allows 8, a 1 x 1
    // texture none). gltfx_all_mipmap_levels (below) asks for all of them;
    // ANY OTHER count larger than that is refused by name
    // (invalid_argument, rejected_value() "mipmap_levels").
    // The reduced levels are computed by the library, not by the graphics
    // driver: each texel is the area average of the texels it covers in
    // the level above, in linear light, with the colour multiplied by
    // alpha.
    // Proved by: texture_desc_validation_test (the limit, at the boundary).
    // Proved by: mipmap_reduce_test (the average in linear light, and on
    // a side of odd length).
    std::uint32_t mipmap_levels = 0;
    // Whether update_texture() may change this texture. False, the
    // default: the texture is fixed, and update_texture() refuses it by
    // name (invalid_argument, rejected_value() "updatable"). True: the
    // texture can be updated and, with reduced levels, keeps the texels of
    // every level in the library's memory (on the CPU, besides whatever the
    // graphics driver holds), from its creation to its destruction, so
    // that an update recomputes the reduced texels it touches
    // (renderer_2d.hpp, update_texture()). That is 4 bytes for each texel
    // of levels 0 to k, k the reduced levels the texture has, level i
    // having max(1, width >> i) x max(1, height >> i) texels: at least
    // width x height x 4 bytes, and always less than twice that. It is
    // about 4/3 of it for a square texture with every level (1024 x 1024:
    // 1.333), nearly twice for a texture one texel high (256 x 1 with 8
    // levels: 511 texels, 2044 bytes, 1.996), and in between otherwise
    // (4096 x 2 with 12 levels: 1.5). A texture without reduced levels
    // keeps nothing either way.
    // Proved by: texture_update_test (the texels kept only by an updatable
    // texture with reduced levels, and how many a row and a square keep).
    // Proved by: texture_update_region_test (a texture made without it,
    // refused by name).
    bool updatable = false;
};

// The value that asks for every reduced level the texture size allows. It
// is the largest uint32, which is never a real count.
inline constexpr std::uint32_t gltfx_all_mipmap_levels = 0xFFFFFFFFU;

} // namespace glintfx
```

### `include/glintfx/draw2d/texture_region.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/color.hpp>
#include <glintfx/core/vec2.hpp>

// draw2d/texture_region.hpp - R2D-TEXTURE. A part of a
// texture (in TEXELS, the texture's own pixels), how it is mirrored, and
// the vertex that carries texel coordinates to the textured triangles.
//
// TEXEL COORDINATES are their own type, not world and not screen
// (GODS_LAWS.md L-19): a world rectangle passed where a texel rectangle
// is expected must not compile. Double precision, as the world is, so a
// source rectangle can start between two texels.
//
// MIRRORING is a named value, never a sign: `flip_left_right` swaps the
// left and right edges of the source, `flip_top_bottom` the top and bottom,
// `flip_left_right_and_top_bottom` both: the name says what the reader sees.
// A value outside this list is refused for the whole call, counted as
// `unknown_option`.
//
// Proved by: texture_region_uv_test (each mirror value puts the corners of
// the source on the corner roles of core/quad.hpp; and, at compile time, a
// world vector or rectangle neither converts to nor builds a texel one,
// nor the other way round).
// Proved by: draw2d_texture_parity_test (each mirror value read back as
// pixels from a real surface: every texel of a 2 x 2 texture in the quarter
// the mirror puts it).

namespace glintfx {

struct gltfx_vec2_texel {
    double x = 0.0;
    double y = 0.0;
};

struct gltfx_rect_texel {
    gltfx_vec2_texel corner{};
    gltfx_vec2_texel size{};
};

enum class gltfx_texture_mirror : std::uint8_t {
    none = 0,
    flip_left_right = 1,
    flip_top_bottom = 2,
    flip_left_right_and_top_bottom = 3,
};

// A region of a texture, as a VALUE: the rectangle in texels and the
// mirroring. It holds NO reference to the texture. The texture comes in each
// drawing call, as in SDL_RenderTexture, so a region can never point to a
// texture that was released. Copying a region copies four numbers (the
// corner and the size) and an enumerator (the mirroring).
//
// A texture that is not open, or that belongs to another renderer, is refused
// by the drawing call (`texture_not_open`, `texture_other_renderer`). A
// region whose origin or size is not a finite number is refused as
// `not_a_number` or `infinite`, one with a negative width or height as
// `negative_size`, and one that reaches outside the texture as
// `region_out_of_range`.
struct gltfx_texture_region {
    gltfx_rect_texel source{};
    gltfx_texture_mirror mirror = gltfx_texture_mirror::none;
};

// What a textured drawing call paints with, grouped (D-W8-354): the region and
// the tint. Grouping keeps each drawing call within four parameters. The
// tint you do not choose is opaque white, which leaves the texture as it is
// (D-W8-464).
// Proved by: draw2d_texture_parity_test (on a real context, in both values
// of srgb_framebuffer: a white texel tinted white with alpha 0.5 over black,
// and an opaque red texel filtered next to a transparent one, read back at
// the value the formula gives, the red with green and blue exactly zero: no
// dark halo).
struct gltfx_texture_paint {
    gltfx_texture_region region{};
    gltfx_rgba tint{1.0F, 1.0F, 1.0F, 1.0F};
};

// The vertex of the textured triangles. Position in WORLD space, like
// every world position; `tint` is linear light with straight alpha, as
// every gltfx_rgba. The library multiplies the premultiplied texel by the
// tint in two parts: the colour by `tint` times `tint.alpha`, and the alpha
// by `tint.alpha`. So a tint of alpha 0.5 over a texel of partial alpha
// gives the right colour and the right alpha, not a dark halo.
// `texel` is in texel units of the texture being drawn, from 0 to its width
// and height (the edges are inside); fill_textured_triangles() and
// fill_indexed_textured_triangles() refuse a coordinate outside that, never
// clamp nor repeat it (renderer_2d.hpp).
// The tint you do not choose is opaque white, as in gltfx_texture_paint
// (D-W8-464).
// Proved by: draw2d_texture_parity_test (on a real context: a vertex tint of
// alpha 0.5 over an opaque white texel and black reads as the tint of
// draw_texture()).
struct gltfx_textured_vertex_2d {
    gltfx_vec2_world position{};
    gltfx_rgba tint{1.0F, 1.0F, 1.0F, 1.0F};
    gltfx_vec2_texel texel{};
};

} // namespace glintfx
```

### `include/glintfx/draw2d/texture_2d.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/export.hpp>

// draw2d/texture_2d.hpp - R2D-TEXTURE. A texture on the
// graphics card: a move-only handle, opaque (PIMPL), in the same shape as
// gltfx_renderer_2d. Created by gltfx_renderer_2d::create_texture()
// (renderer_2d.hpp), the way SDL_CreateTexture takes its renderer: the
// texture belongs to ONE renderer and to that renderer's context.
//
// LIFETIME (docs/api-conventions.md R3, R5):
//   - The texture does NOT outlive its renderer. Destroying the renderer
//     while a texture is alive is a precondition violation (the same
//     rule as the window and the context).
//   - Destroying the texture takes the graphics object out of the consumer's
//     hands at once. The renderer keeps the GL name in a list and releases it
//     at finish_frame(), when its context is already current. If a frame is
//     abandoned, the release happens at the next begin_frame(), or when the
//     renderer is destroyed. Destroying the texture OUTSIDE an open frame
//     releases it at once, after making the renderer's context current on the
//     calling thread (the rule begin_frame() follows, D-API-07).
//   - If the release fails, the graphics object leaks, the destructor does not
//     throw and does not return an error, and the event
//     `texture_release_failed` says so (identifier plus typed fields, the same
//     shape as `draw2d_program_rejected`).
//   - A piece already queued that uses the texture still draws with it, and
//     the order of layers does not change. Destruction is NOT a barrier: it
//     does not send the batch (see renderer_2d.hpp, update_texture).
//
// Proved by: renderer_2d_impl_test (destruction with a piece in the queue
// draws the piece and keeps layer order; the release happens at
// finish_frame(); a release outside a frame makes the context current first
// and emits the event on a failing release; pure, with a fake GL table).

namespace glintfx {

class gltfx_renderer_2d;
struct texture_2d_impl;

class gltfx_texture_2d {
  public:
    gltfx_texture_2d(const gltfx_texture_2d &) = delete;
    gltfx_texture_2d &operator=(const gltfx_texture_2d &) = delete;
    GLINTFX_API gltfx_texture_2d(gltfx_texture_2d &&other) noexcept;
    GLINTFX_API gltfx_texture_2d &operator=(gltfx_texture_2d &&other) noexcept;
    GLINTFX_API ~gltfx_texture_2d();

    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;
    // Size in texels, fixed at creation.
    [[nodiscard]] GLINTFX_API std::uint32_t width() const noexcept;
    [[nodiscard]] GLINTFX_API std::uint32_t height() const noexcept;

  private:
    friend class gltfx_renderer_2d;
    explicit gltfx_texture_2d(texture_2d_impl *interior) noexcept : impl(interior) {}
    texture_2d_impl *impl = nullptr;
};

} // namespace glintfx
```

### `include/glintfx/core/atlas_allocator.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/err.hpp>
#include <glintfx/export.hpp>

// core/atlas_allocator.hpp - R2D-TEXTURE. A PURE
// packing allocator: it decides WHERE a rectangle goes inside a fixed
// square of pixels, and it takes space BACK when the rectangle is
// released. It knows no texture, no GL and no operating system, so it is
// testable without a graphics card (CONTRACT.md, core layer).
//
// WHY RELEASE EXISTS: a dynamic atlas (a glyph, a sprite loaded and
// unloaded) must reuse the space of what left. An allocator without
// release fills up for good.
//
// STALE SLOTS (the ABA problem): a slot that was released can hand its
// space to a new allocation. A second release of the OLD slot would then
// free the NEW owner's space. Every slot therefore carries a generation,
// and release() refuses a slot whose generation is no longer current
// (invalid_argument, rejected_value() "stale_slot"). Proved by:
// atlas_allocator_test (release, reallocate into the hole, then release
// the old slot: refused, the new owner's space untouched).
//
// NO ROOM is a result, never a null or an empty optional (R1): allocate()
// returns gltfx_rslt<gltfx_atlas_slot>. The error code for "no room" is
// `no_free_space`, appended to the end of gltfx_err_code, with
// rejected_value() "atlas_full". It says what is missing and does not read as
// a shortage of process memory (out_of_memory would say that).
//
// Proved by: atlas_allocator_test (allocate, release, reallocate into the
// hole, full atlas refused, a rectangle larger than the atlas refused).

namespace glintfx {

// A rectangle of pixels inside the atlas, in whole pixels. Trivial value.
struct gltfx_atlas_rect {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// The receipt of one allocation: where it is, and which generation of
// the allocator handed it out.
struct gltfx_atlas_slot {
    gltfx_atlas_rect rect{};
    std::uint32_t id = 0;
    std::uint32_t generation = 0;
};

class gltfx_atlas_allocator {
  public:
    // A square atlas of `side` pixels per edge. Zero is refused with
    // invalid_argument, rejected_value() "side".
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_atlas_allocator>
    open(std::uint32_t side) noexcept;

    gltfx_atlas_allocator(const gltfx_atlas_allocator &) = delete;
    gltfx_atlas_allocator &operator=(const gltfx_atlas_allocator &) = delete;
    GLINTFX_API gltfx_atlas_allocator(gltfx_atlas_allocator &&other) noexcept;
    GLINTFX_API gltfx_atlas_allocator &operator=(gltfx_atlas_allocator &&other) noexcept;
    GLINTFX_API ~gltfx_atlas_allocator();

    // A rectangle of `width` by `height` pixels. Zero is refused
    // (invalid_argument, "width" or "height"); larger than the atlas, or no
    // free place of that size, is refused as described above.
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_atlas_slot>
    allocate(std::uint32_t width, std::uint32_t height) noexcept;

    // Gives the space of `slot` back. A stale slot is refused (see above).
    [[nodiscard]] GLINTFX_API gltfx_rslt<void>
    release(gltfx_atlas_slot slot) noexcept;

  private:
    explicit gltfx_atlas_allocator(struct atlas_allocator_impl *interior) noexcept
        : impl(interior) {}
    struct atlas_allocator_impl *impl = nullptr;
};

} // namespace glintfx
```

### `include/glintfx/draw2d/texture_atlas.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

#include <glintfx/core/atlas_allocator.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/export.hpp>

// draw2d/texture_atlas.hpp - R2D-TEXTURE.
// A convenience over the pure allocator (core/atlas_allocator.hpp): one
// texture, a packing allocator, and the upload of each image into its
// place. The caller hands an image and gets back the slot of the cell that holds it.
//
// MARGIN AND EXTRUSION: each image is written with `margin_pixels` of
// copied border on every side, so a sample near its edge reads the
// image's own colour and never the neighbour's (the bleed of a linear
// filter). The region returned covers the image only, not the
// margin.
//
// MIPMAPS AND MARGIN: a mipmap level k averages 2^k pixels, so the margin
// must cover it. The rule is:
//     mipmap_levels == k  requires  margin_pixels >= 2^k
// A request that breaks it is refused at open() with invalid_argument,
// rejected_value() "margin_pixels". The rule and its refusal are proved
// by atlas_extrude_test, which tests one step beyond the border, in a cell
// that is NOT aligned to 2^k: margin 2^k - 1 must be refused (or must fail),
// margin 2^k must pass. If the test refuses margin 2^k, the rule becomes
// margin 2^(k+1) - 1. A cell at 0.5x minification must not bleed.
//
// THE CELL IS A SLOT, WITH A GENERATION. add_image() returns the
// gltfx_atlas_slot of the cell (its place and its generation), not a region.
// A drawing call that takes a cell receives that slot. The atlas checks the
// generation: a slot whose cell was released is refused as `stale_region`.
// The drawing call that takes a slot is draw_atlas_image (D-W8-353), which
// receives a gltfx_atlas_paint (D-W8-354).
//
// REMOVAL: `release_slot` gives the cell back to the allocator. The slot it
// was handed is then STALE, and every drawing call refuses it as
// `stale_region`. A second release of the same slot is refused as
// `stale_slot` (see atlas_allocator.hpp).
//
// Proved by: atlas_extrude_test (see above); atlas_allocator_test (the
// allocator under the atlas, through release and reuse).

namespace glintfx {

class gltfx_renderer_2d;

struct gltfx_texture_atlas_desc {
    std::uint32_t side = 0;             // square edge in texels; zero refused
    std::uint32_t margin_pixels = 1;    // copied border per side, see above
    std::uint32_t mipmap_levels = 0;    // zero: no mipmap; k: exactly k levels
};

// What an atlas cell is painted with (D-W8-354): the slot that names the cell,
// with its generation; the mirroring; and the tint.
struct gltfx_atlas_paint {
    gltfx_atlas_slot slot{};
    gltfx_texture_mirror mirror = gltfx_texture_mirror::none;
    gltfx_rgba tint{};
};

class gltfx_texture_atlas {
  public:
    // Opens the atlas on `renderer`. Refusals: invalid_argument on
    // "side", "margin_pixels" (the rule above) or "mipmap_levels" (more
    // levels than `side` allows).
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_texture_atlas>
    open(gltfx_renderer_2d &renderer, const gltfx_texture_atlas_desc &desc) noexcept;

    gltfx_texture_atlas(const gltfx_texture_atlas &) = delete;
    gltfx_texture_atlas &operator=(const gltfx_texture_atlas &) = delete;
    GLINTFX_API gltfx_texture_atlas(gltfx_texture_atlas &&other) noexcept;
    GLINTFX_API gltfx_texture_atlas &operator=(gltfx_texture_atlas &&other) noexcept;
    GLINTFX_API ~gltfx_texture_atlas();

    // Places `image` in the atlas and returns the slot of the cell that
    // holds it. No room: refused with the allocator's error (`no_free_space`,
    // see atlas_allocator.hpp).
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_atlas_slot>
    add_image(gltfx_image_view image) noexcept;

    // Gives the cell of `slot` back. A slot already released, or from another
    // atlas, is refused (invalid_argument, "slot").
    [[nodiscard]] GLINTFX_API gltfx_rslt<void>
    release_slot(gltfx_atlas_slot slot) noexcept;

  private:
    explicit gltfx_texture_atlas(struct texture_atlas_impl *interior) noexcept
        : impl(interior) {}
    struct texture_atlas_impl *impl = nullptr;
};

} // namespace glintfx
```

### `include/glintfx/draw2d/renderer_2d.hpp` (trecho: includes e cinco métodos de textura, dentro da classe, antes de `private:`)

```cpp
// TRECHO para inserir em include/glintfx/draw2d/renderer_2d.hpp.
// Includes novos, junto aos existentes:
#include <cstdint>
#include <span>
#include <glintfx/core/image_view.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/draw2d/texture_atlas.hpp>
// Dentro da classe gltfx_renderer_2d, antes de "private:":

    // Makes a texture from `image`, on this renderer's context. The pixels
    // are read only during this call (docs/api-conventions.md R5): the
    // library converts them (core/image_view.hpp says to what) and keeps
    // its own copy on the graphics card (made updatable and with reduced
    // levels, one in the library's memory as well: see update_texture()).
    // Refused as gltfx_err, never an
    // exception (R1, R3), and the first refusal of this list wins:
    //   - invalid_argument: "width" or "height" is zero; "pixel_format" is
    //     `unspecified` or a value this build does not know; "bytes_per_row"
    //     or "pixels" does not hold the image (see image_view.hpp); "filter"
    //     is not a value of gltfx_texture_filter; "mipmap_levels" is more
    //     reduced levels than this size allows (texture_2d_desc.hpp).
    //   - the error of this renderer's context, as it came, when the
    //     context cannot be made current (nothing was touched).
    //   - unsupported, rejected_value() "width" or "height": larger than
    //     the card allows (the limit is read from the card, never assumed).
    //   - out_of_memory, rejected_value() "texture_create": the library ran
    //     out of memory (os_error_code() zero), or the card did
    //     (os_error_code() the GL code).
    //   - platform_failure, rejected_value() "texture_create": the card
    //     refused the texture for another reason (os_error_code() the GL
    //     code).
    // Like begin_frame(), this call makes this renderer's context current
    // before it touches GL. It sets the pixel-unpack state it reads with (no
    // unpack buffer, alignment 1, row length and skips zero) and leaves it
    // so, with no texture bound and texture unit 0 active.
    // Proved by: texture_desc_validation_test (each invalid_argument, by
    // name and in this order, in the check this call makes first).
    // Proved by: texture_upload_test (the creation this call forwards to,
    // over a fake GL: the context first, the card's limit, the two memory
    // failures, the unpack state set and left so, the stored texels
    // uploaded as sRGB, every reduced level).
    // Proved by: draw2d_parity_test (this call on a real context, over a
    // pixel-unpack buffer and unpack state left by the caller: the texture
    // opens with its size, the unpack state, the binding and the unit read
    // back as said above, and a zero width is refused by name).
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_texture_2d>
    create_texture(gltfx_image_view image, gltfx_texture_2d_desc desc = {}) noexcept;

    // Overwrites the part of `texture` that starts at texel (x, y) with
    // `image`, on this renderer's context. The pixels are read only during
    // this call, as create_texture() reads them. Only a texture made with
    // gltfx_texture_2d_desc::updatable can be changed. Refused as gltfx_err,
    // never an exception, and the first refusal of this list wins:
    //   - invalid_argument, rejected_value() "texture": the texture is not
    //     open, or belongs to another renderer;
    //   - invalid_argument, rejected_value() "updatable": the texture was
    //     made without gltfx_texture_2d_desc::updatable;
    //   - invalid_argument: "pixel_format", "bytes_per_row" or "pixels" as
    //     for create_texture(); "rect": the rectangle from (x, y), of the
    //     image's size, reaches past the texture (an image with no width may
    //     start at x equal to the texture's width, and likewise for y);
    //   - the error of this renderer's context, as it came, when the
    //     context cannot be made current (nothing was touched);
    //   - out_of_memory, rejected_value() "texture_update": the library ran
    //     out of memory (os_error_code() zero), or the card did
    //     (os_error_code() the GL code);
    //   - platform_failure, rejected_value() "texture_update": the card
    //     refused the writing for another reason (os_error_code() the GL
    //     code).
    // Every GL error the writing raised is read by this call, so none is
    // left for a later call to report. When the library runs out of
    // memory, nothing was touched. When the card refuses (out_of_memory or
    // platform_failure with a GL code), the library's memory is given back
    // exactly as it was before the call, so no refused texel reaches the
    // card through a later update; on the card, what the rectangle and the
    // reduced texels it touches hold is unspecified until an update that
    // covers the rectangle succeeds (OpenGL ignores the write it refuses,
    // not the writes before it). After out_of_memory from the card, OpenGL
    // leaves its whole state undefined: the texture's content on the card
    // is unspecified, and the way back is to make the texture again.
    //
    // An image with no width or no height is an empty update: once the
    // checks above pass, the call succeeds and does nothing (no texel
    // written, no GL call, the context not made current).
    //
    // The texture is READ at the send: updating it changes what the pieces
    // not yet sent show. This call does NOT send the batch. To keep the
    // previous content in those pieces, call flush() before it. The barrier
    // belongs to the consumer and is explicit.
    //
    // A texture with reduced levels has the reduced texels the change
    // touches recomputed, exactly as create_texture() computes them. For
    // that, an updatable texture with reduced levels keeps the texels of
    // every level in the library's memory too, from its creation to its
    // destruction: at least width x height x 4 bytes and less than twice
    // that (gltfx_texture_2d_desc::updatable gives the exact count); any
    // other texture keeps none.
    // Like create_texture(), this call makes this renderer's context current
    // before it touches GL, sets the pixel-unpack state it reads with (no
    // unpack buffer, alignment 1, row length and skips zero) and leaves it
    // so, with no texture bound and texture unit 0 active.
    // Proved by: texture_update_region_test (each invalid_argument, by name
    // and in this order, and the empty update).
    // Proved by: texture_update_test (the writing, over a fake GL: the
    // context first, the memory, the rectangle of each level, the reduced
    // texels equal to those of a texture made from the whole new image, the
    // unpack state set and left so; a writing the card refuses gives the
    // library's memory back, leaves no GL error behind, and never reaches
    // the card through a later update; the texels kept, counted).
    // Proved by: texture_update_kept_test (the rectangle each level
    // touches; the same swap twice gives the kept texels back, byte for
    // byte).
    // Proved by: renderer_2d_impl_test (an update in an open frame sends
    // nothing; an empty update makes no GL call).
    // Proved by: draw2d_parity_test (this call on a real context, over a
    // hostile unpack state: the state read back as said above; a rectangle
    // past the texture and a texture made without updatable refused by
    // name; an empty update succeeds).
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> update_texture(gltfx_texture_2d &texture,
                                                              std::uint32_t x, std::uint32_t y,
                                                              gltfx_image_view image) noexcept;

    // Draws the region of `paint.region` from `texture` into the axis-aligned
    // rectangle `rect` (world space, like fill_rect), multiplied by `paint.tint`
    // as texture_region.hpp says. Mirroring is applied to the source, not to
    // the rectangle. Refused as ONE piece, counted in the report, never an
    // exception, for the first of these reasons, in this order:
    //   - not_a_number, then infinite: a value of the rectangle, of the
    //     source region or of the tint that is not a finite number;
    //   - negative_size: the rectangle or the source region with a negative
    //     width or height;
    //   - unknown_option: the mirror is a value outside its enumeration;
    //   - texture_not_open: the texture is released or was never opened;
    //   - texture_other_renderer: the texture belongs to another renderer;
    //   - region_out_of_range: the source rectangle is outside the texture;
    //   - pixel_position_out_of_range: the batch transform takes a corner
    //     outside the range the drawing program can scale, as for fill_rect.
    // A textured piece is painted in the same order as every other piece
    // (layer, then submission); a change of texture never reorders. In a
    // flush the graphics card accepts, each texture the library binds to
    // draw counts once in texture_switches: a change of texture between two
    // draw calls, a change to or from no texture, and the first texture of
    // every flush (the count starts again from no texture at every flush).
    // A flush the card refuses counts none (frame_2d_report.hpp).
    // D-W8-354: texture, paint and rect with the layer; four parameters at most.
    // Proved by: renderer_2d_impl_test (each reason in this order, over a
    // fake GL; the corners read the region by role and mirror; a texture
    // between two untextured pieces is three draw calls in paint order and
    // two switches; a flush the card refuses adds no switch).
    // Proved by: draw2d_texture_parity_test (on a real context, in both
    // values of srgb_framebuffer: textures A, B, A painted in that order
    // with three switches; a solid, a textured and a solid piece with two;
    // the four mirrors read back texel by texel; the tint over black).
    GLINTFX_API void draw_texture(const gltfx_texture_2d &texture, gltfx_texture_paint paint,
                                  gltfx_rect_world rect, gltfx_draw_layer layer = {}) noexcept;

    // Draws the region of `paint.region` from `texture` into a free
    // quadrilateral, with PROJECTIVE interpolation: a straight line of the
    // texture stays a straight line on the screen, even when the quad is a
    // trapezoid. The corner roles of gltfx_quad_world say where the texture's
    // top-left lands. Refused as ONE piece, counted in the report, for the
    // reasons of draw_texture (negative_size only for the source region: a
    // quadrilateral has no size), in its order, with `degenerate_quad` just
    // before pixel_position_out_of_range: the corners do not define a
    // projective mapping (three of them on one line, a quad whose edges
    // cross, or a corner that points inward; a quad of zero area has its
    // corners on one line, so it is refused the same way). Whether the
    // corners define a mapping is decided exactly, for any finite corners,
    // at any size, place and mix of magnitudes: no rounding enters that
    // answer. Quads that do define a mapping are refused as
    // `degenerate_quad` in one case only: when the diagonals cross so close
    // to one corner (closer than about 2^-64 of the diagonal's length) that
    // a projective weight of the quad would be 2^63 or more. Every such quad
    // that is drawn carries its four weights to the graphics card multiplied
    // by one common power of two, which leaves the mapping as it is, so that
    // each lies between 2^-32 and 2^32 (OpenGL promises texture coordinates
    // up to 2^32 in magnitude).
    // D-W8-354: four parameters at most.
    // Proved by: renderer_2d_impl_test (degenerate_quad after the texture's
    // reasons; the projective weight of each corner reaches the vertex).
    // Proved by: textured_quad_corners_test (a quad whose largest weight is
    // 2^63 or more, refused, the one past the largest float included; one
    // of 2^64/3, carried with the others below 2^32 by one power of two).
    // Proved by: projective_quad_test (the decision for quads of mixed
    // magnitudes, from slivers a few multiples of the smallest double tall
    // to half widths of 2^1023, and for every small whole-number quad; a
    // quad whose weight would pass the largest double, refused).
    // Proved by: draw2d_texture_parity_test (on a real context: a
    // checkerboard drawn into a trapezoid reads back, pixel by pixel, the
    // square the homography of the four corners puts there, including
    // pixels a drawing that ignored the projective weight would get wrong).
    GLINTFX_API void draw_texture_quad(const gltfx_texture_2d &texture, gltfx_texture_paint paint,
                                       gltfx_quad_world corners,
                                       gltfx_draw_layer layer = {}) noexcept;

    // Draws one cell of an atlas, named by `paint.slot`, into `rect`. A slot whose
    // cell was released is refused as `stale_region`. D-W8-353 and D-W8-354: the
    // name is draw_atlas_image, and the parameters are the atlas, the paint and
    // the rectangle, with the layer; four parameters at most.
    GLINTFX_API void draw_atlas_image(const gltfx_texture_atlas &atlas,
                                      gltfx_atlas_paint paint, gltfx_rect_world rect,
                                      gltfx_draw_layer layer = {}) noexcept;

    // Filled triangles textured from `texture`, each vertex with its own position (world space,
    // like fill_triangles), tint and texel coordinate (gltfx_textured_vertex_2d,
    // texture_region.hpp). `vertices` and `indices` are read only during this call
    // (docs/api-conventions.md R5). `fill_textured_triangles` reads the vertices in order, three at
    // a time; `fill_indexed_textured_triangles` reads each three indices as three vertices of the
    // span (two names, as fill_triangles and fill_indexed_triangles). The texture is read
    // AFFINELY across each triangle, with the filter of the texture (draw_texture_quad() is the
    // projective reading), and each texel is multiplied by the tint of its vertex, interpolated
    // across the triangle, as texture_region.hpp says.
    //
    // Texel (0, 0) is the top-left corner of the texture: the outer corner of the first texel of
    // its first row, the first row of the image it was made from. x grows along the row and y
    // grows DOWN, from one row to the next. The edges of a texel lie on whole numbers, so the
    // center of the first texel is (0.5, 0.5), as PIXELS says for the surface.
    // A triangle is drawn whatever the order of its three vertices, clockwise or counter-clockwise
    // on the screen: none is dropped for its orientation, because face culling is off whenever the
    // renderer draws (flush() lists the state it sets).
    // The triangles are blended over what the surface already holds as every piece is: the color
    // premultiplied, (ONE, ONE_MINUS_SRC_ALPHA), as COLOR and flush() say.
    // The call checks each vertex and each index a fixed number of times and copies the vertices,
    // their texel coordinates and the indices into the frame's storage; its work and the memory it
    // takes grow linearly with the number of vertices and indices, and the checking and the
    // copying happen in this call, before the frame is sent.
    //
    // One call is one piece. An empty call (no vertices, or no indices) does nothing and
    // is not a piece, whatever the texture. Refused WHOLE, counted once in the report, never an
    // exception, for the first of these reasons, in this order:
    //   1. more than 2^32 vertices, which a 32-bit index cannot address: too_many_vertices;
    //   2. a vertex count (no indices) or an index count that is not a multiple of three:
    //      count_not_multiple_of_three;
    //   3. an index that is not a vertex of this span: index_out_of_range;
    //   4. a NaN in any position, tint or texel coordinate: not_a_number; then an infinity in
    //      any of them: infinite;
    //   5. texture_not_open, then texture_other_renderer, as for draw_texture();
    //   6. a texel coordinate outside 0..width by 0..height of the texture (its edges are inside):
    //      region_out_of_range. A coordinate past an edge is refused, never clamped nor repeated;
    //   7. a position that the batch transform turns into a pixel coordinate larger in magnitude
    //      than about 1.7e38, where the vertex program would overflow: pixel_position_out_of_range.
    // A textured triangle piece is painted in the same order as every other piece and counts its
    // texture in texture_switches as draw_texture() says; right after a textured piece of the same
    // texture, it is drawn in the same draw call.
    // Proved by: renderer_2d_impl_test (each reason in this order but too_many_vertices, over a
    // fake GL; an empty call asks nothing of the texture; each vertex reaches the card with its
    // texel coordinate divided by the texture's size, q = 1 and the tint premultiplied; a
    // rectangle and a triangle of one texture are one draw call).
    // Proved by: triangle_list_check_test (too_many_vertices, which needs a span of more than
    // 2^32 vertices that no test can hold: the counts alone refuse 2^32 + 1 vertices, before the
    // multiple of three; and, within one call, a NaN before an infinity).
    // Proved by: draw2d_texture_parity_test (on a real context, in both values of
    // srgb_framebuffer: two indexed triangles of opposite orientations put each texel of a 2 x 2
    // texture in its quarter, the first texel of the first row at the top left; a vertex tint of
    // alpha 0.5 over black reads the value the tint of draw_texture() is held to; a rectangle and
    // a triangle of one texture are one draw call and one switch).
    // Proved by: draw2d_parity_test (front faces culled before the frame, and face culling read
    // back off after flush()).
    // Proved by: piece_list_test and triangle_store_test (a textured triangle piece keeps its
    // texture and one read per vertex in the frame's storage; an untextured one grows no read
    // storage).
    GLINTFX_API void fill_textured_triangles(const gltfx_texture_2d &texture,
                                             std::span<const gltfx_textured_vertex_2d> vertices,
                                             gltfx_draw_layer layer = {}) noexcept;
    GLINTFX_API void fill_indexed_textured_triangles(
        const gltfx_texture_2d &texture, std::span<const gltfx_textured_vertex_2d> vertices,
        std::span<const std::uint32_t> indices, gltfx_draw_layer layer = {}) noexcept;
    GLINTFX_API void fill_indexed_textured_triangles(
        const gltfx_texture_2d &texture,
        std::span<const gltfx_textured_vertex_2d> vertices,
        std::span<const std::uint32_t> indices,
        gltfx_draw_layer layer = {}) noexcept;
```


---

## 3. R1 a R10 de `docs/api-conventions.md`, uma a uma

Cada regra: **confirmada** (o desenho proposto cumpre), **confirmada com condição** (cumpre se a T1 a T6 fizerem X), ou **não se aplica**. Achado fica na seção 5.

- **R1 (retorno único `gltfx_rslt<T>`): confirmada com nota.** `create_texture`, `update_texture`, `open`, `add_image`, `release_region`, `allocate` e `release` devolvem `gltfx_rslt`. Os métodos de desenho (`draw_texture`, `draw_texture_quad`, `fill_textured_triangles`, `fill_indexed_textured_triangles`) devolvem `void`, pela decisão 2 do líder (19/09): o problema sai no relatório. O plano pedia "cheio devolve vazio" no alocador, que é a segunda forma de retorno que R1 proíbe: IMP-T01.
- **R2 (`[[nodiscard]]` estrutural): confirmada.** Todo método que devolve valor leva `[[nodiscard]]`, inclusive `is_open()`, `width()` e `height()`. Os métodos de desenho não devolvem valor, então não se aplica a eles.
- **R3 (`noexcept` na borda): confirmada com condição.** Todo método público é `noexcept`, inclusive construtores de movimento e destrutores. Condição para a T2 e a T6: a alocação de prateleiras, de cópias de pixel e de tabelas do atlas tem de virar `out_of_memory` (degrau 2 da R3), nunca `bad_alloc` que escape. O gate `noexcept_alloc_test` precisa passar nesses arquivos.
- **R4 (campo ou código desconhecido nunca é comportamento indefinido): NÃO confirmada no texto proposto; corrigível.** `pixel_format` e `filter` fora da faixa são recusados por nome (`invalid_argument`). Falta a mesma regra para `gltfx_texture_mirror` (um `static_cast` com valor 9 hoje passa sem recusa) e para a origem não finita de uma região. Também `gltfx_texture_region::texture` com ponteiro pendurado é UB (CRI-T02). Achados IMP-T10 e CRI-T02.
- **R5 (alocação e liberação do mesmo lado): confirmada.** `gltfx_image_view` não possui bytes: `std::span` é lido só durante a chamada, e o texto diz isso. A textura copia os pixels para a placa dentro da chamada. O alocador é dono das próprias prateleiras e as libera no destrutor. Nota de ABI igual à da S0: `std::span` é template de cabeçalho, sem objeto no `.so`.
- **R6 (nome público sem colisão com macro nem com nome da biblioteca padrão): confirmada com limite declarado.** Todos os nomes novos passaram pela sonda de colisão (seção 6), e os que o portão não varre estão listados lá. Nomes evitados por R6 e pelo item 3 da ordem: `format`, `data`, `stride`, `clamp`, `index`, `min`, `max`, `near`, `far`, `remove`. Os campos são `pixel_format`, `bytes_per_row`, `pixels`.
- **R7 (formatação é vocabulário de token, nunca frase): confirmada.** Os tokens do texto são identificadores: `invalid_argument`, `rejected_value()` com `"pixel_format"`, `"bytes_per_row"`, `"stale_slot"`, `"atlas_full"` (provisório, Q-T02), `"margin_pixels"`. Nenhum campo novo leva texto livre.
- **R8 (callback com ponteiro de função + contexto): não se aplica.** Nenhum callback novo.
- **R9 (evento de log é identificador mais campos tipados): aplica-se ao evento `texture_release_failed` (D-W8-26), e fica confirmado.** FATO: o evento irmão `draw2d_program_rejected` é constante interna em `src/draw2d/embedded_program.hpp:49` (`k_program_rejected_event`), com campo de texto do driver como campo tipado (`k_driver_log_field`), e não aparece em cabeçalho público. O evento de textura segue a mesma forma: constante em `src/`, campo do código de erro da placa como inteiro tipado. Ele sai de um destrutor, então os campos precisam ser tipados, sem texto livre.
- **R10 (callbacks entregues à biblioteca): não se aplica.** A T0 não entrega callback à biblioteca.

**Citação de comentário público (regra de abertura de `docs/api-conventions.md`):** os cabeçalhos citam testes com `Proved by:` (`renderer_2d_impl_test`, `draw2d_parity_test` e os novos). Seis dos novos não existiam na T0: `texture_desc_validation_test`, `texture_region_mirror_test`, `atlas_allocator_test`, `atlas_extrude_test`, `projective_quad_test` e `texture_update_region_test` não aparecem em `tests/`, `CMakeLists.txt` nem `cmake/`. É o mesmo estado da S0 (seção 3, COS-01 de lá): o texto só entra no repositório junto com o teste que cita. Achado COS-T02.

---

## 4. As decisões D-W8-10 a 14, 23, 25, 26 e 252, e a pergunta extra

### 4.1 Decisões, uma a uma

- **D-W8-10 (cor pré-multiplicada em luz linear, codificada em sRGB na placa; alfa linear): CONFIRMADA.** A escolha (c) é a única que filtra em luz linear com alfa pré-multiplicado, e o formato `SRGB8_ALPHA8` deixa o alfa linear por definição, o que o texto respeita. O enum de formato de entrada tem quatro valores por extenso e cresce por acréscimo, como o plano pede. Nota: "conversão por tabela" vale só para a decodificação de 8 bits (256 entradas). A codificação depois da multiplicação é função de ponto flutuante, e isso é aceito porque o `CONTRACT.md` §8.3 manda que a decodificação de asset pesado fique fora do caminho quente (FATO, lido). Sem divergência.
- **D-W8-11 (amostragem e mipmap no descritor; `image-rendering` fica na W9-B; repetição vira linha nova): CONFIRMADA na substância, com condições.** O descritor tem `filter` e `mipmaps`. Condições: a forma do mipmap (IMP-T12, Q-T04), o padrão de filtro (IMP-T13, Q-T05). A repetição em mosaico fica de fora, como o plano pede.
- **D-W8-12 (tempo de vida: não sobrevive ao desenhador; destruir envia o lote antes): DIVERGENTE, na parte de envio.** Confirmada a parte "não sobrevive ao desenhador". Divergente a parte "destruir envia o lote", porque o envio é barreira e o contrato de `flush()` diz que a ordem de camadas não atravessa barreira (CRI-T01). Vai à seção 7, DIV-T01. **Decidido: D-W8-288.**
- **D-W8-13 (desenho com textura: região com espelhamento, quadrilátero projetivo, tinta que multiplica, triângulos texturizados; troca de textura não reordena): CONFIRMADA na substância, com condições.** Sem a correção projetiva, o trapézio da sombra de luz pontual (D-W8-16) entorta, e por isso a decisão é necessária. A troca de textura parte o lote sem reordenar, e isso precisa de teste "A, B, A dá 3 lotes na ordem" (já previsto no plano). Condições: regra de premultiplicação da tinta (IMP-T03), casos de borda do quad (IMP-T11), significado de `texture_switches` (IMP-T09), nomes do espelhamento (IMP-T15). Ponto de contrato a registrar: a tinta multiplica a cor pré-multiplicada do texel, e isso não é a multiplicação direta de canais.
- **D-W8-14 (atlas: alocador público com liberação no núcleo, e atlas de conveniência com margem e extrusão; opção c): CONFIRMADA na decisão, com condições.** A liberação é requisito do atlas dinâmico (glifo, sprite carregado e descarregado), e isso não depende de citação externa. Condições: reuso após liberação (ABA, IMP-T02), código de "atlas cheio" (IMP-T01, Q-T02), e justificativa factual (IMP-T07). A justificativa de WebRender do plano não se sustenta com as fontes que achei (IMP-T07), e a decisão não depende dela. **Decidido: D-W8-299** (a frase do WebRender sai do texto).
- **D-W8-23 (vértice interno de 9 floats, 36 bytes, sem preenchimento, como interno): CONFIRMADA.** A conta é posição (2) mais cor (4) mais texel (2) mais coordenada homogênea (1) = 9 floats de 4 bytes, 36 bytes, sem preenchimento. É interno: não aparece em cabeçalho público, e o tipo público `gltfx_textured_vertex_2d` tem 8 floats, sem a coordenada homogênea, que o desenho calcula por dentro. Custo registrado: toda peça sólida também paga 4 bytes a mais por vértice (+12,5% de largura de banda do vértice), e o plano não diz isso. A alternativa de dois formatos de vértice foi descartada pelo plano e não a reabro, mas o custo fica escrito. O teste que trava 32 bytes em `batch_vertex` (static_assert, fato 1.7 do plano) muda na T2 e na T4.
- **D-W8-25 (tolerância de 1 nível em `double`; promessa honesta sobre o filtro; margem para atlas com mipmap): CONFIRMADA com condições.** A tolerância fica fixada antes do teste (L-43 global), e isso está correto. Condições: a promessa sobre o filtro depende do driver no texto do plano (IMP-T05, Q-T06), e a regra de margem `2^k` precisa de alinhamento e de teste na fronteira (IMP-T06, Q-T09). **Decidido: D-W8-295** (formato obrigatório, filtro `linear`), **D-W8-304** (promessa pelo formato) e **D-W8-298** (margem).
- **D-W8-26 (destruir torna corrente o contexto do próprio desenhador antes de tocar GL; falha vaza, sem travar, e emite `texture_release_failed`): CONFIRMADA com condição.** O texto segue a forma de `begin_frame()` (D-API-07), e o evento segue R9 (seção 3). Condição: a ordem de envio depende de DIV-T01. A prova prevista, `renderer_2d_impl_test` com tabela de funções GL falsa, tem o nome em dois arquivos de `tests/`; não li se a tabela falsa já cobre o caminho de destruição, e isso fica como pergunta de implementação, não como achado.
- **D-W8-252 (enum de amostragem nasce com dois valores e texto de "só cresce"; cabeçalho não promete `switch` exaustivo): CONFIRMADA.** `gltfx_texture_filter` tem `nearest` e `linear`, e o cabeçalho diz "APPEND-ONLY" e pede que o consumidor não escreva `switch` sem `default` (texto de `texture_2d_desc.hpp`). O texto não promete que o enum é exaustivo, que era o pedido do plano. FATO lido na documentação do SDL 3.4: `SDL_SCALEMODE_PIXELART` existe desde 3.4.0 e é linha futura do plano (W9-B), e não entra aqui. Sem divergência.

### 4.2 Pergunta extra: `image_view` mora em `core/`?

**Sim, em `core/`.** Critério: é valor puro, sem GL e sem sistema operacional, como `core/quad.hpp` e `core/rect.hpp`; o formato de pixel é propriedade dos bytes, não da placa; e o tipo de 8 bits `gltfx_rgba8` (em `core/color.hpp`) já está lá e descreve exatamente a mesma fronteira. A dependência fica para baixo (`draw2d` usa `core`, L-19). O cabeçalho junta a vista e o enum de formato, que são um só assunto (dado de pixel, L-17). A pergunta de se o enum merece arquivo próprio não é decisão minha: fica como Q-T07, junto com o lugar dos tipos de texel.

### 4.3 L-28 do projeto: "quem nunca viu entende lendo uma vez?", nome por nome

| Nome | Lê de uma vez? | Nota |
|---|---|---|
| `gltfx_image_view` (`pixels`, `width`, `height`, `bytes_per_row`, `pixel_format`) | sim | `bytes_per_row` diz a unidade no nome |
| `gltfx_pixel_format::rgba8_srgb_premultiplied` (e os outros três) | sim | a cor e o alfa estão no nome; por isso o padrão é o problema (IMP-T13) |
| `gltfx_texture_2d_desc::filter` / `mipmaps` | parcial | `mipmaps` é bool e não diz quantos níveis (IMP-T12) |
| `gltfx_texture_filter::nearest` / `linear` | sim | os nomes do SDL, e a palavra "linear" é a da luz, por D-W8-10 |
| `gltfx_texture_mirror::horizontal` / `vertical` | NÃO | "horizontal" lê-se como linha de espelho ou como direção do flip (IMP-T15) |
| `gltfx_texture_region` (`texture`, `source`, `mirror`) | parcial | `texture` é ponteiro com vida fora do controle (CRI-T02) |
| `gltfx_rect_texel`, `gltfx_vec2_texel` | sim | o tipo diz que a unidade é o texel |
| `gltfx_textured_vertex_2d` (`position`, `tint`, `texel`) | sim | `tint` diz a função, e a regra de multiplicação está em IMP-T03 |
| `create_texture`, `update_texture`, `draw_texture`, `draw_texture_quad` | sim | |
| `fill_textured_triangles` / `fill_indexed_textured_triangles` | sim | mesmo par de nomes da S0 (D-W8-282) |
| `gltfx_atlas_allocator::allocate` / `release`, `gltfx_atlas_slot` (`generation`) | sim | |
| `gltfx_texture_atlas::add_image` / `release_region` | sim | nome antigo `remove` foi trocado por `release_region` (R6) |
| `gltfx_texture_atlas_desc::side`, `margin_pixels`, `mipmap_levels` | sim | `mipmap_levels` diz contagem, em oposição ao bool do descritor |


---

## 5. Achados por classe

Classes: **CRÍTICO** quebra o que já está publicado ou aponta comportamento indefinido no desenho; **IMPORTANTE** é contrato a fechar antes da fatia que o usa; **COSMÉTICO** é nome, comentário ou citação. Nenhum achado é divergência com decisão do plano: essas estão na seção 7.

**Decisões que fecham os achados (a errata vence o texto dos achados abaixo):**

| Achado | Decisão |
|---|---|
| CRI-T01 | D-W8-288: destruir adia a liberação para o fim do quadro, sem enviar o lote; atualizar também não envia o lote. |
| CRI-T02 | D-W8-289: `gltfx_texture_region` é valor (retângulo em texels e espelhamento); a textura entra em cada chamada de desenho. |
| IMP-T01 | `allocate` devolve `gltfx_rslt`; o código de "sem lugar" é `no_free_space` (D-W8-291). |
| IMP-T02 | D-W8-289 e D-W8-290: slot com geração; `release` recusa slot antigo como `stale_slot`; o desenho recusa a célula velha como `stale_region`. |
| IMP-T03 | Regra escrita no comentário de `gltfx_textured_vertex_2d`; teste na fatia de tinta (D-W8-300). |
| IMP-T04 | D-W8-300: a lista de chamadas que tornam o contexto corrente entra no cabeçalho do desenhador (`create_texture` e `update_texture` incluídos); a paridade com `srgb_framebuffer` ligado e desligado entra na fatia de tinta. |
| IMP-T05 | D-W8-304: a promessa é pelo formato e é provada em cada sistema; o texto público está em `texture_2d_desc.hpp` (D-W8-308). |
| IMP-T06 | D-W8-298: regra `margem >= 2^k`, com teste na fronteira em célula desalinhada; se reprovar com `2^k`, a regra vira `2^(k+1) - 1`. |
| IMP-T07 | D-W8-299: a justificativa do WebRender sai do texto. |
| IMP-T08 | D-W8-290: lista única de motivos de textura, acrescentada depois da S0. |
| IMP-T09 | D-W8-292: toda mudança da textura ligada conta, inclusive de e para nenhuma. |
| IMP-T10 | Origem de região não finita: `not_a_number`; espelhamento fora da faixa: `unknown_option` (D-W8-285, D-W8-297). |
| IMP-T11 | D-W8-290: `degenerate_quad`, com o critério escrito (três cantos alinhados ou cantos que se cruzam). |
| IMP-T12 | D-W8-294: `mipmap_levels` é contagem nos dois descritores. |
| IMP-T13 | D-W8-295: `pixel_format` sem padrão (`unspecified` é recusado); o filtro tem padrão `linear`. |
| IMP-T14 | D-W8-288: a peça na fila mostra o pixel do momento do envio; atualizar não envia o lote. |
| IMP-T15 | D-W8-297: `flip_left_right` e `flip_top_bottom`; o valor dos dois eixos é `flip_left_right_and_top_bottom` (D-W8-353). |
| COS-T01 | Sai com a D-W8-289. |
| COS-T02 | `Proved by:` só entra quando o teste existir (D-W8-300). |
| COS-T03 | Os marcadores de planejamento saem do texto público (seção 2). |

### CRÍTICO: 2

- **CRI-T01. Destruir textura com peça na fila envia o lote, e esse envio é barreira (D-W8-12, parte b).**
  FATO: o texto de `flush()` em `renderer_2d.hpp` diz: "Layer order does not cross a barrier: a piece submitted after flush() is painted over everything before it, whatever its layer." O plano manda enviar o lote ao destruir textura, e o envio é a mesma operação de `flush()`.
  INFERÊNCIA (não rodada): destruir uma textura não relacionada no meio do quadro passa a mudar a ordem de camadas das peças seguintes, sem chamada explícita de barreira. Quem usa camadas para empilhar (fundo, personagem, interface) perde a ordem sem saber por quê.
  Classe CRÍTICO porque quebra promessa publicada. Correção: DIV-T01 (seção 7). Enquanto a decisão não sai, a T3 (sub-retângulo e destruição) não começa.

- **CRI-T02. `gltfx_texture_region::texture` é ponteiro para um objeto cuja vida o consumidor controla (defeito do rascunho desta revisão, `draw2d/texture_region.hpp`).**
  FATO: o consumidor pode destruir o `gltfx_texture_2d` e depois desenhar uma região que ainda guarda o endereço dele. A biblioteca desreferencia memória liberada, o que é comportamento indefinido do lado dela, e R4 proíbe que um campo inválido chegue a esse ponto. O mesmo vale para uma região de outro desenhador.
  Não substituí o campo por outro tipo nesta revisão, porque a forma é decisão (DIV-T02, seção 7). O texto como está não pode ser implementado: a T2 não começa com ponteiro.

### IMPORTANTE: 15

- **IMP-T01. "Cheio devolve vazio" no alocador (T6, plano-w8.md:267) fere R1.** FATO: o plano pede que `allocate` devolva "vazio" quando não há lugar, e isso é uma segunda forma de retorno. Corrigido no texto: `allocate` devolve `gltfx_rslt<gltfx_atlas_slot>`. O código de erro para "sem lugar" está provisório em `out_of_memory` com `rejected_value()` `"atlas_full"`, e a escolha é Q-T02.

- **IMP-T02. Reuso de espaço depois de liberação (problema ABA).** FATO do desenho: um slot liberado pode ter o espaço entregue a outro dono. Uma segunda liberação do slot antigo liberaria o espaço do novo, e uma região antiga desenharia a imagem nova sem erro nenhum. O plano não menciona isso. Corrigido no texto do alocador: cada slot leva uma geração, e `release` recusa slot de geração antiga com `stale_slot`. Para a região: depois de `release_region`, o desenho recusa a região (a forma exata da recusa depende de DIV-T02 e Q-T01).

- **IMP-T03. A tinta multiplicada precisa de regra de premultiplicação escrita.** D-W8-13 diz "tinta que multiplica", mas o texel está pré-multiplicado (D-W8-10). Conta: texel de cor `t` e alfa `a_t` (pré-multiplicado `P = t·a_t`), tinta de cor `k` e alfa `a_k`. O resultado pré-multiplicado é `P·k·a_k` nos canais de cor e `a_t·a_k` no alfa. Ou seja: o texel pré-multiplicado é multiplicado por `k·a_k` na cor e por `a_k` no alfa. Multiplicar só por `k` produz halo escuro ou cor errada com tinta semitransparente. INFERÊNCIA (não rodada): o teste precisa de tinta de alfa 0,5 e de texel de alfa parcial, nos dois valores de `srgb_framebuffer`. Entra na T4.

- **IMP-T04. Contrato de cor e de GL congelados não cobre textura.** (a) `renderer_2d.hpp`, "COLOR, frozen": quando `srgb_framebuffer` está desligado, a cor é codificada em sRGB antes da mistura. A cor amostrada da textura entra no mesmo caminho, e a T4 precisa de paridade com o `srgb_framebuffer` ligado e desligado (`draw2d_parity_test`). O plano não pede isso. (b) O texto de `begin_frame`, `flush` e `finish_frame` lista quais chamadas tornam o contexto corrente. `create_texture` e `update_texture` tocam GL e não estão na lista. O texto proposto já diz isso no próprio método, mas o cabeçalho do desenhador precisa da mesma lista.

- **IMP-T05. Promessa de filtro condicionada ao driver (D-W8-25).** FATO: o plano manda prometer "o filtro age em luz linear quando o driver decodifica sRGB antes de filtrar". Isso põe a promessa pública no comportamento de cada driver. INFERÊNCIA a confirmar antes de virar texto público: a especificação do OpenGL 3.3 core trata o formato sRGB como decodificação obrigatória antes da filtragem, e não como opção. Não li a especificação nesta sessão. Recomendação: promessa pelo formato, e um driver que não cumpre é defeito que a sonda prova; "ausência declarada" fica só para extensão opcional. Pergunta Q-T06.

- **IMP-T06. Regra de margem `margem ≥ 2^k` (D-W8-25) é aritmética não medida e o teste é insuficiente.** INFERÊNCIA: o filtro bilinear no nível `k` lê até um texel desse nível (2^k pixels da base) além da borda. A margem cobre se a célula estiver alinhada a múltiplo de 2^k; célula desalinhada precisa de 2^k − 1 a mais, ou o alinhamento vira regra de alocação. O plano prova só 0,5x, que é o nível `k = 1`. Exigência para `atlas_extrude_test`: testar cada `k` de 1 até o máximo permitido, e a fronteira, com margem `2^k − 1` (tem de falhar ou ser recusada) e `2^k` (tem de passar), em célula desalinhada. É o "um passo além da fronteira" da L-43 global. Pergunta Q-T09.

- **IMP-T07. Justificativa factual de D-W8-14 não confirmada (L-44).** O plano diz que a liberação "foi isso que levou o WebRender a trocar de algoritmo". FATO das fontes que li: o alocador atual do WebRender é `ShelfAllocator`, alias do crate etagere, de prateleiras com coalescência de prateleiras vazias; o módulo guilhotina continua na árvore, e não achei registro da troca nem do motivo. "Skyline sem liberação (stb, fontstash)" não foi verificado. A decisão (c) não depende disso, porque liberação é requisito do atlas dinâmico, mas o texto da decisão não pode afirmar o motivo. Correção: reescrever a justificativa com a fonte real ou sem a comparação histórica.

- **IMP-T08. Recusas do desenho com textura não cabem no layout travado de D-W8-30.** O desenho com textura precisa de recusas que o layout da S0 não tem: textura não aberta ou liberada, textura de outro desenhador, região fora da textura, texel ou tinta não finitos, região nula, quad sem homografia. O layout da S0 tem seis motivos, e D-W8-30 diz que o layout é decidido uma vez para a onda inteira. Sem a lista, a T4 não tem onde contar a recusa. Pergunta Q-T01, junto com a lista única de motivos.

- **IMP-T09. Significado de `texture_switches` não está definido.** FATO: o layout da S0 diz "vezes que a textura ligada mudou entre duas chamadas de desenho". Não diz se uma peça de cor sólida depois de uma texturizada conta. Se a sólida usar textura branca de 1x1, conta; se usar programa sem textura, não conta. O número muda de sentido conforme a resposta. Pergunta Q-T03.

- **IMP-T10. Valores fora da faixa e origem não finita de região (R4).** O texto recusa `pixel_format` e `filter` por nome, mas não recusa `gltfx_texture_mirror` fora da faixa nem origem de região não finita. Correção: recusa por nome (`invalid_argument` com `"mirror"`) e recusa de não finito (motivo de Q-T01). Não apliquei no texto, porque a sonda de nomes não depende disso, e a recusa depende da lista de Q-T01.

- **IMP-T11. Quad degenerado no desenho projetivo.** Três cantos alinhados e quad que se cruza não têm homografia. O texto diz "refused" sem critério de área zero e sem motivo. Desenhar quad de área zero "não cobre pixel" (regra de `fill_quad`), mas sem homografia a textura não tem o que mostrar. Mesma família da IMP-04 da S0: cada caso de borda precisa de linha no cabeçalho e de teste citado. Pergunta Q-T01.

- **IMP-T12. Duas formas de dizer mipmap.** `gltfx_texture_2d_desc::mipmaps` é bool (cadeia completa) e `gltfx_texture_atlas_desc::mipmap_levels` é contagem. São dois conceitos com o mesmo nome de família, e o bool não diz quantos níveis (L-28). Pergunta Q-T04.

- **IMP-T13. Padrões escondem decisão de cor e de filtro.** `gltfx_image_view::pixel_format` tem padrão `rgba8_srgb_straight`, e `gltfx_texture_2d_desc::filter` tem padrão `linear`. O formato de cor é a decisão que D-W8-10 quer explícita ("o formato diz o alfa e a codificação por extenso"). Um consumidor que esquece o campo recebe cor errada sem erro. Recomendação: formato obrigatório, sem padrão. O padrão de filtro é escolha de produto. Pergunta Q-T05.

- **IMP-T14. Atualizar sub-retângulo com peça na fila (T3).** O contrato não diz o que a peça já enfileirada mostra. A amostragem acontece no envio, então a peça mostra o pixel novo. Se `update_texture` também enviar o lote, cai em CRI-T01. O texto precisa dizer uma das duas coisas. Entra em DIV-T01.

- **IMP-T15. Nomes de espelhamento ambíguos (L-28).** `horizontal` e `vertical` podem ler-se como "linha de espelho horizontal" (que inverte de cima para baixo) ou como "direção do flip". Sugestão, não aplicada (a sonda de nomes já foi rodada com os nomes atuais): `flip_left_right` e `flip_top_bottom`, ou outro par que o CTO escolher. Pergunta Q-T08.

### COSMÉTICO: 2

- **COS-T01. Forma elaborada no campo `texture`.** `const class gltfx_texture_2d *texture` usa o especificador elaborado sem necessidade. Some com a decisão de CRI-T02.
- **COS-T02. Citação de testes inexistentes.** Os nomes de `Proved by:` listados na seção 3 ainda não existem no repositório. O portão de citação reprova o cabeçalho enquanto isso for verdade (INFERÊNCIA: não rodei o portão de citação nesta revisão). Declarado, mesmo estado da S0.
- **COS-T03. Comentários de planejamento dentro de texto público.** Os cabeçalhos propostos `texture_2d.hpp`, `texture_region.hpp`, `atlas_allocator.hpp` e o trecho de `renderer_2d.hpp` citam "seção 7", "SEÇÃO 7" ou IDs de revisão (CRI-T02, Q-T01 e outros). Esses marcadores envelhecem dentro de um cabeçalho público, como a COS-01 da S0. Saem antes de o texto entrar no repositório.


---

## 6. Sonda de colisão

Rodadas dentro da trava (`flock -w 7200 -E 99 /var/tmp/cto-w8/heavy.lock`), em contêiner (`glintfx-devbuild:verify`), pelo `run_ctest.sh` da ordem, filtro `public_name_collision_test`. Base: `bd179b077fbb4000cb8ae26148cb15fe1a266afd`. Disco antes da rodada negativa: `Device unallocated` 21,23 GiB, `Free (estimated)` min 25,00 GiB.

| Rodada | Cópia | Texto | rc_configure | rc_ctest | Log | Resultado |
|---|---|---|---|---|---|---|
| 1, negativa | `fixture` | seis cabeçalhos e trecho, nomes propostos | 0 | **0** | `col.log` | passou |
| 1, controle | `fixture-pos` | o mesmo, mais `controle_positivo_major.hpp` (enumerador `major` em linha própria) | 0 | **8** | `col-pos.log` | reprovou: `/usr/include/sys/sysmacros.h:60:major` |

Leitura, em duas linhas, como a ordem pede:
- **Negativa (`col.log`):** `rc_ctest=0`, na primeira rodada. Nenhum nome proposto colidiu, então nenhum CRÍTICO de nome e nenhuma correção de nome. A frase "0 colisão real" não aparece no log: o `ctest` oculta a saída de um teste que passa. A prova é o rc.
- **Controle positivo (`col-pos.log`):** `rc_ctest=8`, nomeando `major`. A sonda enxerga o que deve enxergar. O critério de parada da ordem ("controle passa verde") não ocorreu.
- **Neutralizadas, sem relação com a T0:** `/usr/lib/gcc/.../stdbool.h:36:bool` e `/usr/include/fcntl.h:215:open` (guarda inativa por padrão). `open` é o nome já publicado da fachada, e não é causado por esta fatia.
- **Rodada 2:** não houve, porque a rodada 1 foi limpa. Não houve correção de texto.
- **Repetição com os nomes finais (D-W8-290, 291, 294, 295, 297 e `flip_left_right_and_top_bottom`, D-W8-303):** pendente. Roda no passo 0 da T1, antes de qualquer linha de código (D-W8-303).

**Prova de sintaxe (sem execução de teste de produto):** `probe/base.cpp` usa todos os nomes novos e as chamadas previstas, com `-std=c++23 -fsyntax-only -Wall -Wextra -Werror`:
- `g++` 16.2.1: rc=0. `clang++` 22.1.8: rc=0.
- `fill_textured_triangles(tex, {})` compila nos dois: a função é única, não há ambiguidade como a da S0 (IMP-06 de lá).
- Negativas, que têm de falhar: `gltfx_rect_world` atribuído a `gltfx_rect_texel` (rc=1 nos dois); cópia de `gltfx_texture_2d` (rc=1 nos dois). As duas afirmações dos comentários de `texture_region.hpp` e `texture_2d.hpp` estão provadas.

**Limites declarados desta sonda:**
1. Nomes de parâmetro não são varridos (`tests/tools/check_public_name_collision.py:37-38`). Os parâmetros novos são `pixels`, `width`, `layer`, `tint`, `region`, `rect`, `corners`, `vertices`, `indices`, `texture`, `image`, `desc`, `slot`, `x`, `y`, e nenhum é macro conhecida na sonda. A garantia é de revisão.
2. O portão local roda no Linux. Os nomes do SDK do Windows (`near`, `far`, `min`, `max`) não são varridos aqui. A perna Windows do mesmo portão (`NAMES-PARITY-WIN`, pela variável `INCLUDE`) só roda na CI. Não rodei a perna Windows.
3. A segunda metade de R6 (nome da biblioteca padrão) o portão não cobre: é revisão manual, com a lista da seção 3 (`format`, `data`, `stride`, `clamp`, `index`, `remove` evitados).
4. O portão checa macros, não nomes de função da biblioteca padrão: `create_texture`, `draw_texture` e os demais passam por ausência de macro, não por checagem de `std::`.

---

## 7. Divergências e perguntas

Quem decide é o CTO, e o líder onde o item diz. O revisor não decide nenhum deles. Cada item traz o que o plano diz, o que o texto diz e a recomendação, que é só recomendação.

### 7.1 Divergências

**DIV-T01 (CRI-T01 e IMP-T14). D-W8-12, parte "destruir envia o lote": a destruição e a atualização de textura são barreira?** **Decidido (D-W8-288): opção (b). Atualizar também não envia o lote.**
- O plano diz: destruir textura com peça na fila envia o lote antes (D-W8-12, b). O plano não diz nada sobre atualizar.
- O contrato publicado de `flush()` diz: a ordem de camadas não atravessa barreira.
- Opções:
  - (a) manter o envio: destruir ou atualizar vira barreira implícita, e a ordem de camadas muda sem que o consumidor chame nada. Contradiz o contrato publicado.
  - (b) adiar a liberação: a textura sai do objeto do consumidor, e o desenhador guarda o objeto da placa numa fila que libera depois de `finish_frame()`. Não há barreira. Custo: a memória da textura fica até o fim do quadro, e o relatório pode contar as liberações adiadas.
  - (c) recusar a destruição com peça na fila: o consumidor precisa esperar o fim do quadro para soltar um sprite. Quebra o uso comum.
- Para a atualização (IMP-T14): a peça enfileirada mostra o pixel do momento do envio, sem barreira. O texto precisa dizer isso, ou então copiar o pixel antigo na atualização.
- Recomendação: (b) para a destruição, e o contrato escrito para a atualização. Quem decide: CTO.

**DIV-T02 (CRI-T02). A região guarda a textura por ponteiro, e a forma precisa mudar.** **Decidido (D-W8-289): opção (c).**
- O plano diz: "região de origem como tipo próprio (textura mais retângulo em texels mais espelhamento nomeado)" (D-W8-13). Não diz se a região guarda a textura.
- O texto guarda ponteiro. O defeito está em CRI-T02.
- Opções:
  - (a) identificador com geração, validado pelo desenhador a cada desenho. Sem UB, recusa região velha por nome. Custo: uma tabela no desenhador.
  - (b) referência `const gltfx_texture_2d &`. Mesmo problema do ponteiro quando o objeto morre.
  - (c) a região não guarda textura: `draw_texture` recebe a textura em cada chamada, como faz o `SDL_RenderTexture`. A região fica só com o retângulo em texels e o espelhamento, que são valores. Custo: o consumidor passa dois argumentos.
- Recomendação: (c), porque a região fica um valor puro e a vida da textura não entra no tipo (L-19). Quem decide: CTO.

### 7.2 Perguntas

- **Q-T01 (IMP-T02, IMP-T08, IMP-T10, IMP-T11). Lista única de motivos de recusa para a onda inteira, antes da T2.** **Decidido (D-W8-290): opção (a), com a lista acrescentada depois dos motivos da S0.** A lista é `texture_not_open`, `texture_other_renderer`, `region_out_of_range`, `degenerate_quad` e `stale_region`. `not_a_number` já existe. O espelhamento fora da faixa usa `unknown_option` (D-W8-285), sem motivo próprio. `light_below_piece` sai (D-W8-334).
  - Motivos que o desenho com textura precisa e o layout da S0 não tem: `texture_not_open` (textura liberada ou não aberta), `texture_other_renderer`, `region_out_of_range`, `invalid_mirror`, `degenerate_homography` (quad sem homografia), `stale_region` (região após liberação). `not_a_number` já existe.
  - Opções: (a) acrescentar todos agora, em um lote, na mesma forma de D-W8-285 (`unknown_option`); (b) reusar motivos existentes (não serve: `not_a_number` não diz "textura liberada"); (c) não contar (viola D-W8-30).
  - Recomendação: (a), em um lote, porque D-W8-30 diz que o layout é decidido uma vez.

- **Q-T02 (IMP-T01). Código de erro para "atlas cheio".** **Decidido (D-W8-291): opção (b), com o nome `no_free_space`** (no fim de `gltfx_err_code`; diz o que falta, sem confundir com memória do processo).
  - (a) `out_of_memory` com `rejected_value()` `"atlas_full"`: não cria código novo, mas quem trata `out_of_memory` lê que o processo ficou sem memória.
  - (b) código novo no fim de `gltfx_err_code`, por exemplo `no_room`: acréscimo, nome por extenso (R7).
  - Recomendação: (b).

- **Q-T03 (IMP-T09). O que é uma troca de textura em `texture_switches`.** **Decidido (D-W8-292): opção (a).**
  - Opções: (a) conta cada mudança da textura ligada, inclusive de e para "nenhuma" (uma peça sólida entre duas texturizadas custa duas trocas); (b) só conta de textura para textura, e a sólida não conta.
  - Recomendação: (a), porque o contador mede custo de troca de ligação na placa.

- **Q-T04 (IMP-T12). Forma do mipmap.** **Decidido (D-W8-294): opção (b).** `mipmap_levels` é contagem nos dois descritores; `gltfx_all_mipmap_levels` pede a cadeia inteira; contagem maior que a cadeia possível é recusada pelo nome.
  - Opções: (a) bool no descritor e contagem no atlas (texto atual); (b) contagem nos dois (`mipmap_levels`, zero é sem mipmap, `k` é até `k` níveis, e o máximo dá a cadeia inteira); (c) enumeração.
  - Recomendação: (b), porque um só conceito com um só nome (L-28).

- **Q-T05 (IMP-T13). Padrões de `pixel_format` e de `filter`.** **Decidido (D-W8-295):** `pixel_format` ganha `unspecified = 0`, o padrão do campo, recusado pelo nome; os quatro formatos passam a 1, 2, 3 e 4. O filtro tem padrão `linear`, o mesmo do SDL 3.
  - Recomendação: `pixel_format` sem padrão (o formato de cor é decisão do consumidor, D-W8-10). `filter`: padrão `linear` é escolha minha, e o plano não diz. O CTO escolhe `linear` ou `nearest`.

- **Q-T06 (IMP-T05). Promessa do filtro sobre o driver.** **Resolvida (D-W8-304, opção d, decisão do CTO):** a promessa é pelo formato, e a célula de paridade reprova em cada sistema onde o driver converte depois de filtrar. A ordem do líder, transcrita de `plano-fechamento-w8-r4.md`, foi "pesquise as outras libs, comunidade e depois impremente [sic] e teste". O líder não escolheu entre as três opções do parecer; a escolha (d) está pendente de ratificação retroativa pelo líder (L-01). Texto público em `texture_2d_desc.hpp` (D-W8-308).
  - (a) promessa pelo formato da especificação, com driver não conforme como defeito que a sonda prova; (b) promessa condicionada ao driver, como o plano escreve.
  - Recomendação: (a), depois de ler a especificação do OpenGL 3.3 para o formato sRGB (ainda não lida).

- **Q-T07. Lugar dos tipos de texel e do enum de formato.** **Decidido (D-W8-296): manter como está.**
  - O texto põe `gltfx_vec2_texel` e `gltfx_rect_texel` em `draw2d/texture_region.hpp`, e o enum de formato em `core/image_view.hpp`. A alternativa é mover os tipos de texel para `core/`.
  - Recomendação: manter como está. O alocador de `core/` trabalha em pixels inteiros e não precisa do tipo de texel. A pergunta só existe se o CTO quiser arquivo próprio para o enum de formato (L-17).

- **Q-T08 (IMP-T15). Nomes do espelhamento.** **Decidido (D-W8-297): opção (b),** `flip_left_right` e `flip_top_bottom`, com a sonda de colisão de novo. O valor dos dois eixos é `flip_left_right_and_top_bottom` (D-W8-353).
  - Opções: (a) manter `horizontal`, `vertical`; (b) `flip_left_right`, `flip_top_bottom`; (c) `mirror_x`, `mirror_y`.
  - Recomendação: (b), porque "verboso vence curto" (L-28). Trocar o nome exige nova sonda de colisão.

- **Q-T09 (IMP-T06). Alinhamento da célula do atlas com mipmap.** **Decidido (D-W8-298): opção (c),** com o teste na fronteira em célula desalinhada. Se reprovar com `2^k`, a regra vira `2^(k+1) - 1`, opção (b).
  - (a) célula alinhada a `2^k`, com desperdício de espaço; (b) margem `2^k − 1` a mais na célula; (c) manter a regra `margem ≥ 2^k` e o teste de célula desalinhada, que mostra se falha.
  - Recomendação: (c), com o teste na fronteira. Se o teste reprovar, (b). Quem decide: CTO.

---

## 8. FATO e INFERÊNCIA

**FATO (medido ou lido, com fonte):**
- md5 de `plano-fechamento-w8.md`: `647a3e354e4c3ca5f4ccc36f436b286c`; de `plano-fechamento-w8-r2.md`: `44a4d21abb45b6387e87b0411ddbddd9`; do parecer da S0: `2fb68acc39757cce3053f9f7e9c568e8`. Os três conferiram no início.
- Disco, `btrfs filesystem usage /`: início com `Device unallocated` 21,23 GiB e `Free (estimated)` min 25,20 GiB; durante a sonda, min 25,00 GiB (negativa) e 24,94 GiB (controle). Acima do piso de 20 GiB.
- Árvore: `bd179b077fbb4000cb8ae26148cb15fe1a266afd` (`git rev-parse HEAD` da cópia). A árvore real passou de `a2a63ea4` para `bd179b07` durante o trabalho, e `git diff a2a63ea4 bd179b07 -- include src` está vazio. As cópias foram refeitas no mesmo SHA.
- Rcs da sonda (seção 6), lidos dos arquivos `rc_ctest` e `rc_configure` das duas pastas `out-col` e `out-col-pos`, e da linha `COLISAO` de `ctest.out` do controle.
- Compilação só-sintaxe (`probe/base-g++.log`, `probe/base-clang++.log` e as negativas), com `-fsyntax-only`, sem executar binário.
- `include/glintfx/core/rect.hpp:92`: `gltfx_rect_screen` é "SCREEN space (single precision)". `gltfx_rect_world` é de precisão dupla.
- `include/glintfx/draw2d/renderer_2d.hpp`, texto de `flush()`: "Layer order does not cross a barrier".
- `src/draw2d/embedded_program.hpp:49`: `k_program_rejected_event = "draw2d_program_rejected"`, constante de `src/`, não de cabeçalho público.
- `include/glintfx/core/err.hpp:344`: `gltfx_rslt` é a classe de retorno único de R1.
- `CONTRACT.md` §8.3 (linhas 524 a 527): "Heavy asset decode ... MUST be feasible fora do caminho quente do render" e "Avoid creating/destroying render objects em laço apertado".
- SDL 3.4 (`wiki.libsdl.org/SDL3/SDL_ScaleMode`): `SDL_SCALEMODE_NEAREST`, `SDL_SCALEMODE_LINEAR` e `SDL_SCALEMODE_PIXELART`, este último "available since SDL 3.4.0". SDL 3 (`SDL_RenderTextureAffine`): transformação afim por três pontos (`origin`, `right`, `down`) do canto superior esquerdo do `srcrect`. Não li a página de `SDL_TextureAccess` (não estava na resposta).
- WebRender (fontes: `doc.servo.org` do módulo `texture_pack` e do alias `ShelfAllocator`, do crate etagere): o alocador atual é de prateleiras com coalescência de prateleiras vazias, e o módulo guilhotina continua na árvore. Não encontrei registro da troca de algoritmo nem do motivo.
- RmlUi: NÃO lido. A página de documentação da interface de renderização retornou 404, e a busca não trouxe a documentação primária. Não citei o formato de alfa do RmlUi como fato.
- Os nomes de teste de `Proved by:` `texture_desc_validation_test`, `texture_region_mirror_test`, `atlas_allocator_test`, `atlas_extrude_test`, `projective_quad_test` e `texture_update_region_test` não aparecem em `tests/`, `CMakeLists.txt` nem `cmake/` da cópia. `renderer_2d_impl_test` e `draw2d_parity_test` existem.

**INFERÊNCIA (não medida):**
- Que a destruição de textura com barreira muda a ordem de camadas de peças não relacionadas (CRI-T01). Vem do texto de `flush()` e do plano, não de rodar o desenho.
- Que o filtro bilinear no nível `k` lê até `2^k` pixels da base além da borda, e que a margem `2^k` só cobre célula alinhada (IMP-T06). Aritmética de footprint, não medida.
- Que a especificação do OpenGL 3.3 core torna a decodificação sRGB obrigatória antes da filtragem (IMP-T05). Não li a especificação nesta sessão. A decisão D-W8-304 não depende desta inferência: a promessa é pelo formato, e a célula de paridade prova em cada sistema.
- Que os 36 bytes do vértice interno (D-W8-23) custam +12,5% por vértice. É aritmética da largura do vértice, não medida na placa.
- Que o portão de citação reprova o cabeçalho enquanto os testes citados não existirem (COS-T02). Não rodei o portão de citação.

**NÃO executado, por ordem:** nenhum teste de produto compilado ou rodado; nenhum `preci`; nenhum commit, push, `TODO.md` ou `DECISOES_AUTONOMAS.md`; nenhum toque em `watchcode`; nada na árvore real. Os únicos processos foram `run_ctest.sh` (filtro `public_name_collision_test`, dentro da trava) e a compilação só-sintaxe. Ao fim, os clones `fixture` e `fixture-pos` foram apagados pelo caminho absoluto, conferido antes (L-53). Os logs, `out-col`, `out-col-pos`, `probe/` e `proposta/` ficam.


md5 do corpo deste parecer, sem esta linha: `2e6c2e9be621144d285e5284391c864f` (corpo de origem, antes das errata; o texto desta cópia traz as errata e tem outro md5).
Arquivo de origem: `/var/tmp/cto-w8/t0/auditoria-api-texture.md`, md5 `f8d6415d18fb06bb9707815ebafdc709`.
