// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gfx_open_only_fixation.hpp"

#include <new>

#include "platform/gl/gfx_option_registry.hpp"

namespace glintfx::platform {

namespace {

// The value this entry set would carry for `id`: whatever `requested`
// asks for, or the row's own default when `id` is omitted from
// `requested` entirely (D-W6b-25's own "a pedida, ou o default quando
// nao pedida").
std::int64_t resolve_requested_or_default(const gfx_option_row &row,
                                          std::span<const gltfx_gfx_option_entry> requested) {
    for (const gltfx_gfx_option_entry &entry : requested) {
        if (entry.id == row.id) {
            return entry.value;
        }
    }
    return row.default_value;
}

std::optional<std::int64_t> find_fixed_value(gltfx_gfx_option id,
                                             std::span<const gltfx_gfx_option_entry> fixed) {
    for (const gltfx_gfx_option_entry &entry : fixed) {
        if (entry.id == id) {
            return entry.value;
        }
    }
    return std::nullopt;
}

} // namespace

gfx_open_only_fixation_result
resolve_gfx_open_only_fixation(std::optional<std::span<const gltfx_gfx_option_entry>> already_fixed,
                               std::span<const gltfx_gfx_option_entry> requested) noexcept {
    // docs/api-conventions.md R3 ("a lib NUNCA aborta o processo do
    // consumidor"): `result.fixed.push_back()` below grows a
    // std::vector - on the machine's own allocator running out of
    // memory it throws std::bad_alloc, and letting that escape this
    // noexcept function would call std::terminate(), killing the
    // consumer's process on the very path that opens a window. Caught
    // here and degraded to `alloc_failed`, the same shape err.cpp's own
    // with_path()/with_rejected_value() already use for a best-effort
    // std::string::assign() that can fail the identical way. push_back()
    // is NOT noexcept, so a std::bad_alloc it throws propagates
    // normally to this catch - this part of the design was always
    // correct and stays unchanged.
    //
    // WIN-DEBUG-CTORALLOC (07/09/2026) MOVEU `gfx_open_only_
    // fixation_result result{}` pra DENTRO deste try{} (antes ficava
    // ANTES dele) - SEM EFEITO NENHUM no CI real (run 34178782241,
    // job "Windows - Debug" continuou vermelho identico DEPOIS desse
    // commit estar em main). CAUSA REAL, provada por documentacao
    // oficial (GODS_LAWS.md L-22, ver o comentario do gancho de teste
    // em tests/gfx_open_only_fixation_test.cpp pras fontes completas):
    // no MSVC em build Debug (`_ITERATOR_DEBUG_LEVEL == 2`, o default
    // de Debug), o PROPRIO construtor default de std::vector aloca um
    // `_Container_proxy` de bookkeeping de iterador - MESMO para um
    // vetor vazio - e o compilador declara esse construtor noexcept
    // apesar disso. Uma falha ali tenta escapar de um construtor que o
    // MSVC marcou noexcept, e o runtime chama std::terminate() DENTRO
    // do construtor do vector - antes de a pilha sequer voltar pra
    // este try{}, nao importa em que linha ele comeca. NENHUM try/catch
    // deste arquivo alcanca essa fronteira: quem quebra o proprio
    // contrato noexcept e o std::vector do MSVC, nao este codigo. O
    // DEFEITO ERA DO GANCHO DE INJECAO DE FALHA DO TESTE (fazia TODA
    // alocacao da chamada falhar, inclusive essa bookkeeping interna),
    // nao deste arquivo - o try/catch abaixo, que so precisa proteger
    // a alocacao de CRESCIMENTO do push_back(), sempre esteve certo. A
    // posicao de `result{}` (dentro ou fora do try{}) e indiferente
    // pra esse cenario especifico, mas fica aqui dentro por ser onde o
    // valor e usado pela primeira vez.
    try {
        gfx_open_only_fixation_result result{};

        for (const gfx_option_row &row : k_gfx_option_table) {
            // rule 1 (D-W6b-25/14.2): live/read_only options never
            // enter fixation.
            if (row.when != gltfx_gfx_option_when::open_only) {
                continue;
            }

            const std::int64_t resolved = resolve_requested_or_default(row, requested);

            if (!already_fixed.has_value()) {
                result.fixed.push_back(gltfx_gfx_option_entry{.id = row.id, .value = resolved});
                continue;
            }

            const std::optional<std::int64_t> fixed_value =
                find_fixed_value(row.id, *already_fixed);
            if (!fixed_value.has_value() || *fixed_value != resolved) {
                return gfx_open_only_fixation_result{
                    .outcome = gfx_open_only_fixation_outcome::refuse,
                    .fixed = {},
                    .refused_id = row.id,
                };
            }
        }

        result.outcome = already_fixed.has_value() ? gfx_open_only_fixation_outcome::accept
                                                   : gfx_open_only_fixation_outcome::fix_now;
        return result;
    } catch (const std::bad_alloc &) { // NOLINT(bugprone-empty-catch) reason: intentionally
                                       // empty, see the comment above try{} - `result` (if it
                                       // was constructed at all) keeps whatever partial content
                                       // it grew before the allocation that failed, but
                                       // `alloc_failed` below tells the caller never to read it.
        return gfx_open_only_fixation_result{
            .outcome = gfx_open_only_fixation_outcome::alloc_failed,
            .fixed = {},
            .refused_id = gltfx_gfx_option::vsync,
        };
    }
}

} // namespace glintfx::platform
