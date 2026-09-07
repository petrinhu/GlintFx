// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <optional>
#include <print>
#include <vector>

#include <glintfx/platform/gl/gpu.hpp>

#include "platform/gl/gpu_kind_seam.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gpu_kind_seam_test.cpp - GL-GPU-KIND, A COSTURA (CONSERTO 07/09/2026,
// revisão adversarial da fatia 5b, GODS_LAWS.md L-17/L-20/L-27): antes
// deste arquivo, classify_current_gpu() (egl_context_adapter.cpp e
// wgl_context_adapter.cpp) não tinha NENHUM teste, nos dois lados -
// os três mecanismos que ela costura (leitor do kernel/DXCore, via 1,
// via 2) já tinham 27+8+5 células; a costura entre eles, zero. Este
// arquivo testa gpu_kind_seam.hpp/.cpp, a costura extraída sem
// nenhuma chamada ao SO/GL.
//
// A CONTAGEM DE CAMINHOS DA COSTURA (L-17, "enumerar o espaço
// inteiro"): resolve_kernel_and_exclusion() tem 3 caminhos de controle
// (kernel já resolveu; identidade desconhecida; identidade conhecida
// com a via 1 chamada - cujo resultado pode ou não resolver, tratado
// como dado, não como um 4º caminho); resolve_memory_separation() tem
// 4 (kind_so_far já resolvido; kind_so_far unknown e sem certeza;
// kind_so_far unknown, com certeza, sem resposta da via 2; kind_so_far
// unknown, com certeza, com resposta da via 2). TOTAL: 7 caminhos de
// controle na costura inteira, os 7 com teste (nenhum sem), cobertos
// por 11 célula(s) de dado (mais de uma variação de enum onde o
// caminho é o mesmo mas o valor concreto importa para quem lê o
// teste).
//
// RED, VISTO (07/09/2026, antes do conserto - saída literal colada no
// commit do conserto): o teste `resolve_memory_separation_gate_de_
// certeza` reprovava em DUAS células (a via 2 respondia mesmo com
// `definitely_not_software=false`) contra a extração fiel do que
// classify_current_gpu() fazia até então nos dois arquivos - a mesma
// forma exata do achado da revisão adversarial ("quando a consulta ao
// dispositivo falha, o código nunca sabe se está em software e vai
// direto à via da memória").

using glintfx::gltfx_gpu_kind;
using glintfx::k_gltfx_gpu_index_unknown;
using glintfx::platform::resolve_kernel_and_exclusion;
using glintfx::platform::resolve_memory_separation;

namespace {
constexpr gltfx_gpu_kind unk = gltfx_gpu_kind::unknown;
constexpr gltfx_gpu_kind sw = gltfx_gpu_kind::software;
constexpr gltfx_gpu_kind shr = gltfx_gpu_kind::shared;
constexpr gltfx_gpu_kind ded = gltfx_gpu_kind::dedicated;
} // namespace

GLINTFX_TEST(resolve_kernel_and_exclusion_3_caminhos) {
    int analyzed = 0;

    // Caminho 1: o kernel/DXCore já respondeu - devolve exatamente
    // isso, sem olhar índice nem enumeração (3 variações de valor, o
    // MESMO caminho de controle).
    GLINTFX_CHECK(resolve_kernel_and_exclusion(sw, k_gltfx_gpu_index_unknown, {}) == sw);
    ++analyzed;
    GLINTFX_CHECK(resolve_kernel_and_exclusion(shr, k_gltfx_gpu_index_unknown, {}) == shr);
    ++analyzed;
    GLINTFX_CHECK(resolve_kernel_and_exclusion(ded, k_gltfx_gpu_index_unknown, {}) == ded);
    ++analyzed;

    // Caminho 2: kernel unknown, identidade desconhecida - a via 1
    // NUNCA roda sem uma posição real (nunca chuta a posição 0).
    GLINTFX_CHECK(resolve_kernel_and_exclusion(unk, k_gltfx_gpu_index_unknown, {}) == unk);
    ++analyzed;

    // Caminho 3: kernel unknown, identidade conhecida - a via 1 roda;
    // duas células de DADO (resolve / não resolve), o mesmo caminho de
    // controle desta função.
    {
        const std::array<gltfx_gpu_kind, 2> kinds{unk, shr};
        GLINTFX_CHECK(resolve_kernel_and_exclusion(unk, 0, kinds) == ded);
        ++analyzed;
    }
    {
        const std::array<gltfx_gpu_kind, 2> kinds{unk, unk};
        GLINTFX_CHECK(resolve_kernel_and_exclusion(unk, 0, kinds) == unk);
        ++analyzed;
    }

    GLINTFX_CHECK_EQ(analyzed, 6);
    std::println("gpu_kind_seam_test (kernel+via1): {} celula(s) conferida(s), 3 caminho(s) "
                 "de controle, 3 com teste, 0 sem teste",
                 analyzed);
}

GLINTFX_TEST(resolve_memory_separation_gate_de_certeza) {
    int analyzed = 0;

    // Caminho 1: kind_so_far já resolvido (pelo kernel ou pela via 1) -
    // via 2 NUNCA sobrepõe, mesmo que `memory_separation_kind` diga
    // outra coisa e `definitely_not_software` seja falso.
    GLINTFX_CHECK(resolve_memory_separation(ded, false, std::optional{shr}) == ded);
    ++analyzed;

    // Caminho 2, O DEFEITO (RED contra a extração fiel do código de
    // hoje, GREEN depois do conserto): kind_so_far unknown, SEM
    // certeza de que não é software - a via 2 tem de ser ignorada,
    // mesmo que `memory_separation_kind` tenha uma resposta.
    GLINTFX_CHECK(resolve_memory_separation(unk, false, std::optional{ded}) == unk);
    ++analyzed;
    GLINTFX_CHECK(resolve_memory_separation(unk, false, std::optional{shr}) == unk);
    ++analyzed;
    // A mesma ausência de certeza, sem nenhuma resposta de via 2 -
    // já era `unknown` mesmo antes do conserto, célula de controle.
    GLINTFX_CHECK(resolve_memory_separation(unk, false, std::nullopt) == unk);
    ++analyzed;

    // Caminho 3: kind_so_far unknown, COM certeza, mas a via 2 não
    // respondeu (extensão ausente, ou o chamador nunca leu) -
    // continua `unknown`, nunca um chute.
    GLINTFX_CHECK(resolve_memory_separation(unk, true, std::nullopt) == unk);
    ++analyzed;

    // Caminho 4: kind_so_far unknown, COM certeza, via 2 respondeu -
    // só AQUI o valor da via 2 é usado (duas variações de valor).
    GLINTFX_CHECK(resolve_memory_separation(unk, true, std::optional{shr}) == shr);
    ++analyzed;
    GLINTFX_CHECK(resolve_memory_separation(unk, true, std::optional{ded}) == ded);
    ++analyzed;

    GLINTFX_CHECK_EQ(analyzed, 7);
    std::println("gpu_kind_seam_test (via2): {} celula(s) conferida(s), 4 caminho(s) de "
                 "controle, 4 com teste, 0 sem teste",
                 analyzed);
    std::println("gpu_kind_seam_test: TOTAL 7 caminho(s) de controle na costura, 7 com "
                 "teste, 0 sem teste");
}
