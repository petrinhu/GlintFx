// SPDX-License-Identifier: AGPL-3.0-or-later
//
// gap_optional_vector_assignment.cpp - fixture de LACUNA CONHECIDA de
// tests/tools/check_noexcept_alloc.py (nao e sabotagem - nao entra em
// DIRTY_FIXTURES nem em CLEAN_FIXTURES). VEREDITO ESPERADO HOJE:
// ABSOLVIDA PELO MOTOR ATUAL - e o proprio ponto desta fixture, nao um
// erro do motor a cacar.
//
// Reproduz, isolado, o sitio real medido em
// src/platform/gl/gl_context_facade.cpp:329 ANTES do conserto
// (commit 7a34bb2, 18/09/2026): uma funcao PUBLICA `noexcept` copia
// um `std::vector` comum, por ATRIBUICAO (`destino = origem;`), para
// dentro de um `std::optional<std::vector<...>>` - sem `try` ao
// redor. Isto NAO e familia A (nao depende do modo de depuracao da
// Microsoft, _ITERATOR_DEBUG_LEVEL): mata o processo do consumidor em
// QUALQUER um dos cinco sistemas, hoje, em Release. E NAO e
// composicao (a lacuna que o cabecalho deste script ja declarava
// antes desta fatia) - e uma copia comum de contentor por atribuicao.
//
// O MOTOR NAO VE PORQUE: container_hits() em check_noexcept_alloc.py
// so reconhece CONSTRUCAO direta de std::vector/std::optional
// (declaracao, temporario, return) por casar o token literal
// `std::vector`/`std::optional` no ponto do achado - nunca uma
// ATRIBUICAO para um MEMBRO ja existente de outro objeto
// (`holder->campo = outra_coisa;`), cujo texto nao contem nenhum
// desses tokens. Achado do time-lead, 18/09/2026: extraiu a arvore
// real para fora, mutou o conserto de volta para este mesmo padrao, e
// rodou o portao - devolveu familia_B_achados=0, veredito APROVADO.
//
// ESTA FIXTURE E UM MARCADOR DE LACUNA, NAO UM ALVO DE CONSERTO DO
// MOTOR (decisao do lider, 18/09/2026, por AskUserQuestion, registrada
// em ESCOPO.md Decisao 13): corrigir AGORA o que o portao DECLARA (o
// texto impresso em toda rodada, ver BLIND_SPOTS_TEXT neste script) e
// alargar o motor em fatia PROPRIA, futura. O dia em que alguem
// alargar check_noexcept_alloc.py para rastrear atribuicao de
// contentor, o controle selftest que le esta fixture
// (selftest_known_gap_optional_vector_assignment_currently_absolved)
// vai comecar a falhar - isso e SINAL DE ACERTO, nunca regressao: nesse
// dia, mova esta fixture para DIRTY_FIXTURES e apague o
// controle-marcador (GODS_LAWS.md L-67: o que se revoga se apaga, nao
// se arquiva).

#include <optional>
#include <vector>

namespace {

struct gfx_option_entry {
    int id = 0;
};

struct window_state {
    std::optional<std::vector<gfx_option_entry>> fixed_open_only_gfx_options;
};

// Espelha gltfx_gl_context::open() em
// src/platform/gl/gl_context_facade.cpp ANTES do conserto de 7a34bb2:
// `holder->fixed_open_only_gfx_options = fixation.fixed;` aloca por
// ATRIBUICAO dentro de funcao noexcept, sem try eficaz ao redor.
// `source_fixed` chega por REFERENCIA de proposito (como o campo
// `fixation.fixed` real, lido de um objeto que ja existe) - o ponto
// desta fixture e a ATRIBUICAO em si, nao mais uma declaracao direta
// de `std::vector` (essa forma ja e family A rastreada, ver
// tests/noexcept_alloc_family_a_baseline.txt linha do proprio
// gfx_open_only_fixation.hpp - conflar as duas mascararia o achado).
void apply_fixation(window_state *holder,
                    const std::vector<gfx_option_entry> &source_fixed) noexcept {
    holder->fixed_open_only_gfx_options = source_fixed;
}

} // namespace
