// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <glintfx/platform/gl/gpu.hpp>

// platform/gl/gpu_kind_seam.hpp - GL-GPU-KIND, A COSTURA (CONSERTO
// 07/09/2026, revisão adversarial da fatia 5b, GODS_LAWS.md L-17/L-19/
// L-20/L-27): antes deste arquivo, a ordem kernel/DXCore -> via 1
// (exclusão) -> via 2 (separação de memória) -> `unknown` vivia
// inline, duplicada, dentro de classify_current_gpu() em egl_context_
// adapter.cpp e wgl_context_adapter.cpp - os TRÊS mecanismos que ela
// costura (leitor do kernel, gpu_kind_exclusion.hpp, gl_memory_facts.
// hpp/memory_separation_kind.hpp) têm 27, 8 e 5 células de teste cada;
// a COSTURA em si não tinha nenhuma, nos dois lados, porque estava
// amarrada a uma EGLDisplay ou a um contexto WGL reais - impossível de
// testar sem container. Estas DUAS funções puras são exatamente essa
// costura, extraída sem nenhuma chamada ao SO/GL, e por isso testável
// em `tests/gpu_kind_seam_test.cpp` com fixtures simples.
//
// O DEFEITO QUE ESTA EXTRAÇÃO FECHA: a via 2 só pode ser CONSULTADA -
// e, mais fundo, só pode ser LIDA de verdade (uma chamada GL real) -
// quando há certeza, respondida pelo sistema, de que o dispositivo
// atual NÃO é o rasterizador por software. Antes do conserto, "a
// consulta ao dispositivo falhou" (extensão ausente, `queried=false`)
// e "a consulta respondeu que É hardware" eram tratadas como a MESMA
// coisa - as duas deixavam `kind == unknown` e o código caía direto
// na via 2, confiando no que quer que o token de memória dissesse.
// `resolve_memory_separation()` abaixo é o único lugar que decide isso
// agora, e a decisão é EXPLÍCITA: `definitely_not_software` tem de ser
// `true`, nunca inferido do valor default de uma consulta que não
// respondeu.
//
// PLATAFORMA-AGNÓSTICO DE PROPÓSITO (a mesma razão de gpu_kind_
// exclusion.hpp/memory_separation_kind.hpp, um diretório acima nesta
// mesma pasta): os DOIS lados (egl_context_adapter.cpp no Linux,
// wgl_context_adapter.cpp no Windows) chamam as MESMAS duas funções -
// paridade estrutural garantida pelo compilador, nunca por dois
// arquivos lidos lado a lado (GODS_LAWS.md do projeto, L-04).
//
// POR QUE DUAS FUNÇÕES, NÃO UMA: `resolve_kernel_and_exclusion()` NÃO
// tem efeito colateral nenhum e pode sempre ser chamada; ela é quem
// decide SE a via 2 ainda é necessária. `resolve_memory_separation()`
// só recebe o resultado de uma leitura GL real (`memory_facts`) que o
// CHAMADOR já decidiu fazer ou não - dividir assim é o que permite ao
// chamador nunca pagar a leitura GL quando `definitely_not_software`
// é falso, em vez de ler e descartar (a mesma leitura que produziu o
// achado de `gl_memory_facts_test.cpp`: memória não-inicializada não
// tem custo zero de ignorar, só de nunca ler).

namespace glintfx::platform {

// resolve_kernel_and_exclusion() - kernel/DXCore, depois via 1.
//
// `kernel_kind` é o que o LEITOR de cada plataforma já respondeu para
// o dispositivo ATUAL, sozinho, antes de qualquer via (classify_drm_
// gpu() no Linux, classify_dxcore_gpu() no Windows) - pode já vir
// `software`, `shared` ou `dedicated`, e este atomo devolve esse valor
// sem tocar em mais nada (o kernel/DXCore já respondeu, a via 1 é só
// para quando ele NÃO respondeu).
//
// `enumeration_index`/`enumeration_kinds` são exatamente a posição do
// dispositivo atual e a lista de TODOS os outros já classificados pelo
// kernel/DXCore sozinho (a matéria-prima de gpu_kind_exclusion.hpp) -
// `k_gltfx_gpu_index_unknown` (gpu.hpp) significa "a identidade do
// dispositivo atual dentro da enumeração não pôde ser estabelecida", e
// a via 1 nunca roda nesse caso (nunca chuta uma posição).
[[nodiscard]] gltfx_gpu_kind
resolve_kernel_and_exclusion(gltfx_gpu_kind kernel_kind, std::uint32_t enumeration_index,
                             std::span<const gltfx_gpu_kind> enumeration_kinds) noexcept;

// resolve_memory_separation() - via 2, com o portão de certeza.
//
// `kind_so_far` é o que resolve_kernel_and_exclusion() já devolveu -
// se já não é `unknown`, este átomo devolve exatamente o mesmo valor,
// SEM olhar `definitely_not_software` nem `memory_separation_kind`
// (via 2 nunca sobrepõe uma resposta que o kernel/DXCore ou a via 1 já
// deram).
//
// `definitely_not_software` É O PORTÃO INTEIRO DESTE CONSERTO: só
// quando é `true` a via 2 é usada - e só é `true` quando o CHAMADOR
// tem uma resposta POSITIVA do sistema de que este dispositivo não é
// o rasterizador por software (nunca o valor default de uma consulta
// que falhou ou nunca rodou). `memory_separation_kind` é o resultado
// que classify_by_memory_separation() (memory_separation_kind.hpp) já
// calculou sobre uma leitura GL real - `std::nullopt` quando o
// chamador nunca leu (porque `definitely_not_software` já era falso,
// ou porque a extensão não respondeu) - as duas razões produzem o
// MESMO `unknown` aqui, porque "não li" e "li e não bateu" são,
// para este átomo, a mesma ausência de resposta.
[[nodiscard]] gltfx_gpu_kind
resolve_memory_separation(gltfx_gpu_kind kind_so_far, bool definitely_not_software,
                          std::optional<gltfx_gpu_kind> memory_separation_kind) noexcept;

} // namespace glintfx::platform
