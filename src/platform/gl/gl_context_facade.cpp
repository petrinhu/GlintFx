// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cassert>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/gl/gpu.hpp>
#include <glintfx/platform/window/window.hpp>

#include "platform/gl/auto_preset_rule.hpp"
#include "platform/gl/gfx_open_only_fixation.hpp"
#include "platform/gl/gfx_option_registry.hpp"
#include "platform/gl/gfx_option_validation.hpp"
#include "platform/gl/gfx_preset_table.hpp"
#include "platform/gl/gl_context_access.hpp"
#include "platform/gl/gl_context_desc_validation.hpp"
#include "platform/gl/gl_context_impl.hpp"
#include "platform/gl/preset_expansion.hpp"
#include "platform/port/gl_context_adapter_port.hpp"
#include "platform/window/window_impl.hpp"

// gl_context_facade.cpp - GL-CONTEXT (docs/plano-w6b-placa-e-laco.md
// fatia 2b, D-W6b-1/2/16/17/25, GODS_LAWS.md L-17/L-19/L-22): the ONE
// translation unit that knows what glintfx::gl_context_impl actually
// IS - the exact same "allocation and deallocation on the SAME side of
// the boundary" shape display_facade.cpp/window_facade.cpp already
// give their own opaque handles.
//
// NOT YET WIRED INTO glintfx_library's OWN BUILD (src/platform/gl/
// CMakeLists.txt's own header comment explains why, in full): this
// file PIMPLs over platform::selected_gl_context_adapter, a type only
// fatia 3 (Wayland/EGL) or fatia 4 (Win32/WGL) provides. Written and
// reviewed here, as part of fatia 2b's own frozen contract; the LAST
// step of whichever of those two fatias lands second is adding this
// file's own name to that target_sources() call, in the SAME commit
// that finally makes the library build with a real GL context inside.
//
// THE OPEN() SEQUENCE, IN THE EXACT ORDER sec. 14.2 OF THE PLAN NAMES
// (never reordered - the mutation the plan's own §14.2/§4-fatia-5 rows
// name, "fixation depois do suporte", is exactly what reordering this
// would reintroduce):
//
//   1. gl_context_desc_validation.hpp's own validate_gl_context_desc()
//      - the WHOLE descriptor's shape (unknown id, out-of-range value,
//      read_only entry, a repeated id, gpu_preference reserved) -
//      before either fixation or any adapter is ever consulted.
//   2. gfx_open_only_fixation.hpp's own resolve_gfx_open_only_
//      fixation() - identical on every system, BY CONSTRUCTION, since
//      it runs before step 3 ever asks whether THIS driver happens to
//      support anything (D-W6b-25: this is what makes "recusa por
//      fixacao" and "recusa por falta de suporte do sistema" two
//      genuinely different, and differently-coded, outcomes rather
//      than the SAME refusal on one system and a silent accept on the
//      other).
//   3. The concrete platform::selected_gl_context_adapter's own
//      open() - the only step that can fail for a reason neither of
//      the two pure atoms above could ever catch (the driver genuinely
//      does not have a config, the GL version negotiated is not 3.3
//      core, and so on).
//
// THE FIXATION IS COMMITTED TO THE WINDOW ONLY ON SUCCESS, AND ONLY
// WHEN THIS WAS THE FIRST CONTEXT TO FIX IT (D-W6b-25's own rule 3,
// "a fixacao morre com a janela, nunca com o contexto"): a `fix_now`
// outcome whose adapter open() call then fails must NOT leave the
// window believing something is fixed that no context ever actually
// opened with - see the `fix_now` branch below for exactly where this
// is enforced.

namespace glintfx {

namespace {

static_assert(platform::gl_context_adapter_port<platform::selected_gl_context_adapter>,
              "selected_gl_context_adapter must satisfy gl_context_adapter_port - GODS_LAWS.md "
              "L-19/L-40: gltfx_gl_context's own facade wraps it directly, and a selection "
              "missing any one member has to fail to COMPILE here, never link a context handle "
              "whose set_option()/gpu() silently does nothing on one platform");

// Every row of the registry, at its resolved value: `open_only` rows
// take `fixed_open_only` (the COMPLETE set gfx_open_only_fixation.hpp
// already resolved, requested-or-default); `live`/`read_only` rows
// take whatever `requested` asks for, or the row's own default
// otherwise. This is what seeds gl_context_impl::current_values AND
// what the concrete adapter's own open() receives - the adapter never
// has to separately reason about "what wins between the opening list
// and the registry's own default", this atom already resolved it once.
// GODS_LAWS.md L-22/L-17 (gemeo de gfx_open_only_fixation.cpp's own
// resolve_gfx_open_only_fixation() - a fatia acima, no mesmo arquivo
// que ESTA funcao roda logo em seguida, dentro de open()): reserve()/
// push_back() abaixo podem lancar std::bad_alloc dentro de uma funcao
// noexcept; deixar escapar chamaria std::terminate() e derrubaria o
// processo do consumidor bem na hora de abrir uma janela - a mesma
// regra R3 de docs/api-conventions.md. Como esta funcao JA tem um
// canal de erro pronto no seu unico chamador (open() retorna gltfx_
// rslt<gltfx_gl_context>), o desfecho honesto aqui e devolver o erro,
// nao inventar um valor - a mesma escolha que enumerate_dxcore_
// adapters() (src/platform/win32/dxcore_adapter_enumeration.cpp) ja
// documenta para o formato gltfx_rslt<T>.
[[nodiscard]] gltfx_rslt<std::vector<gltfx_gfx_option_entry>>
resolve_full_option_table(std::span<const gltfx_gfx_option_entry> fixed_open_only,
                          std::span<const gltfx_gfx_option_entry> requested) noexcept {
    // WIN-DEBUG-CTORALLOC (07/09/2026 - ver o achado gemeo no header
    // comment de gfx_open_only_fixation.cpp, mesma onda): `resolved`
    // precisa nascer DENTRO do try{} - um std::vector default-
    // construido nao aloca no Linux/libstdc++, mas o construtor padrao
    // do MSVC em build Debug (`_ITERATOR_DEBUG_LEVEL == 2`) pode alocar
    // o proprio bookkeeping de depuracao de iterador pelo MESMO
    // alocador que reserve()/push_back() usam - uma declaracao ANTES
    // do try{} deixaria essa alocacao fora da guarda.
    try {
        std::vector<gltfx_gfx_option_entry> resolved;
        resolved.reserve(platform::k_gfx_option_table.size());

        for (const platform::gfx_option_row &row : platform::k_gfx_option_table) {
            if (row.when == gltfx_gfx_option_when::open_only) {
                for (const gltfx_gfx_option_entry &fixed_entry : fixed_open_only) {
                    if (fixed_entry.id == row.id) {
                        resolved.push_back(fixed_entry);
                        break;
                    }
                }
                continue;
            }

            std::int64_t value = row.default_value;
            for (const gltfx_gfx_option_entry &requested_entry : requested) {
                if (requested_entry.id == row.id) {
                    value = requested_entry.value;
                    break;
                }
            }
            resolved.push_back(gltfx_gfx_option_entry{.id = row.id, .value = value});
        }

        return gltfx_rslt<std::vector<gltfx_gfx_option_entry>>::ok(std::move(resolved));
    } catch (const std::bad_alloc &) {
        return gltfx_rslt<std::vector<gltfx_gfx_option_entry>>::err(
            gltfx_err(gltfx_err_code::out_of_memory));
    }
}

// The power-source enum (an internal value type) and the public numbers of the `power_source` row
// are the same three numbers (D-P1-1): one assert each, at the ONE place the conversion happens.
static_assert(static_cast<std::int64_t>(platform::gltfx_power_source::unknown) ==
              k_gltfx_power_source_unknown);
static_assert(static_cast<std::int64_t>(platform::gltfx_power_source::mains) ==
              k_gltfx_power_source_mains);
static_assert(static_cast<std::int64_t>(platform::gltfx_power_source::battery) ==
              k_gltfx_power_source_battery);

// The suggestion of RIGHT NOW, computed from two facts the system reports (the kind of GPU this
// context runs on and where the power comes from). Reading it writes NOTHING (D-W6b-35 rule 7).
[[nodiscard]] platform::auto_preset_choice suggestion_now(const gl_context_impl &impl) noexcept {
    return platform::choose_preset_automatically(impl.adapter.gpu().kind, impl.power.read());
}

[[nodiscard]] gltfx_gfx_option_entry *find_current(gl_context_impl &impl,
                                                   gltfx_gfx_option id) noexcept {
    for (gltfx_gfx_option_entry &current : impl.current_values) {
        if (current.id == id) {
            return &current;
        }
    }
    return nullptr;
}

// A row of a preset that this system cannot honor is refused BY NAME, before anything is applied
// (D-W6b-16's "nunca degrada em silencio", D-W6b-35 rule 1).
[[nodiscard]] gltfx_rslt<void> check_preset_rows(const gl_context_impl &impl,
                                                 const platform::preset_expansion &rows) noexcept;

// Applies the rows of the CONCRETE preset `preset`, ALL OR NOTHING, and records the rows and the
// label (which is `preset` itself, never `automatic`). Rows are checked BEFORE any is applied; if
// the adapter still refuses one, the rows already applied go back to their previous values.
[[nodiscard]] gltfx_rslt<void> apply_concrete_preset(gl_context_impl &impl,
                                                     std::int64_t preset) noexcept;

// The opening list asked for `preset = automatic`: apply the rows of the CONCRETE suggestion the
// list did not name (the entries it DID name were already applied by the adapter's open();
// expand_preset keeps them and they are applied again, harmlessly), then record the label - the
// concrete preset, never `automatic`.
[[nodiscard]] gltfx_rslt<void>
apply_preset_at_open(gl_context_impl &impl, std::int64_t concrete,
                     std::span<const gltfx_gfx_option_entry> requested) noexcept;

[[nodiscard]] std::string_view option_name_or_placeholder(gltfx_gfx_option id) noexcept {
    const platform::gfx_option_row *row = platform::find_gfx_option_row(id);
    return row != nullptr ? row->name : std::string_view("gfx_option");
}

gltfx_rslt<void> check_preset_rows(const gl_context_impl &impl,
                                   const platform::preset_expansion &rows) noexcept {
    for (std::size_t i = 0; i < rows.count; ++i) {
        const gltfx_gfx_option_entry &row = rows.entries[i];
        if (const gltfx_rslt<void> shape =
                platform::validate_gfx_option_entry(row, /*already_open=*/true);
            shape.has_error()) {
            return gltfx_rslt<void>::err(shape.err());
        }
        if (impl.adapter.option_support(row.id) != gltfx_gfx_option_support::supported) {
            return gltfx_rslt<void>::err(
                gltfx_err(gltfx_err_code::unsupported)
                    .with_rejected_value(option_name_or_placeholder(row.id)));
        }
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> apply_preset_at_open(gl_context_impl &impl, std::int64_t concrete,
                                      std::span<const gltfx_gfx_option_entry> requested) noexcept {
    const platform::preset_expansion rows = platform::expand_preset(concrete, requested);
    for (std::size_t i = 0; i < rows.count; ++i) {
        const gltfx_gfx_option_entry &row = rows.entries[i];
        const platform::gfx_option_row *described = platform::find_gfx_option_row(row.id);
        if (row.id == gltfx_gfx_option::preset || described == nullptr ||
            described->when != gltfx_gfx_option_when::live) {
            continue; // the label is set below; open_only rows were fixed at open
        }
        if (impl.adapter.option_support(row.id) != gltfx_gfx_option_support::supported) {
            return gltfx_rslt<void>::err(
                gltfx_err(gltfx_err_code::unsupported)
                    .with_rejected_value(option_name_or_placeholder(row.id)));
        }
        if (const gltfx_rslt<void> result = impl.adapter.apply_option(row); result.has_error()) {
            return result;
        }
        if (gltfx_gfx_option_entry *held = find_current(impl, row.id)) {
            held->value = row.value;
        }
    }
    if (gltfx_gfx_option_entry *label = find_current(impl, gltfx_gfx_option::preset)) {
        label->value = concrete;
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> apply_concrete_preset(gl_context_impl &impl, std::int64_t preset) noexcept {
    const platform::preset_expansion rows = platform::expand_preset(preset, {});
    if (const gltfx_rslt<void> checked = check_preset_rows(impl, rows); checked.has_error()) {
        return checked;
    }
    // ALL OR NOTHING: remember what each row held, apply, and undo on the first refusal.
    std::size_t applied = 0;
    std::array<std::int64_t, platform::k_preset_expansion_capacity> previous{};
    for (; applied < rows.count; ++applied) {
        const gltfx_gfx_option_entry *held = find_current(impl, rows.entries[applied].id);
        previous[applied] = held != nullptr ? held->value : 0;
        if (const gltfx_rslt<void> result = impl.adapter.apply_option(rows.entries[applied]);
            result.has_error()) {
            for (std::size_t undo = 0; undo < applied; ++undo) {
                (void)impl.adapter.apply_option(
                    gltfx_gfx_option_entry{rows.entries[undo].id, previous[undo]});
            }
            return result;
        }
    }
    for (std::size_t i = 0; i < rows.count; ++i) {
        if (gltfx_gfx_option_entry *held = find_current(impl, rows.entries[i].id)) {
            held->value = rows.entries[i].value;
        }
    }
    if (gltfx_gfx_option_entry *label = find_current(impl, gltfx_gfx_option::preset)) {
        label->value = preset;
    }
    return gltfx_rslt<void>::ok();
}

// GFX-PRESET (D-W6b-35): applying a preset is ALWAYS the consumer's own request, all or
// nothing, once. `manual` only records the label; `automatic` is a shortcut for "apply the
// suggestion of right now" and records THAT concrete preset - option(preset) never reads
// `automatic`. Changing any OTHER option never touches the label: it is the consumer's.
gltfx_rslt<void> apply_preset_request(gl_context_impl &impl, std::int64_t value) noexcept {
    if (value == k_gltfx_preset_manual) {
        if (gltfx_gfx_option_entry *label = find_current(impl, gltfx_gfx_option::preset)) {
            label->value = k_gltfx_preset_manual;
        }
        return gltfx_rslt<void>::ok();
    }
    const std::int64_t concrete =
        value == k_gltfx_preset_automatic ? suggestion_now(impl).preset : value;
    return apply_concrete_preset(impl, concrete);
}

} // namespace

gltfx_rslt<gltfx_gl_context> gltfx_gl_context::open(gltfx_window &window,
                                                    const gltfx_gl_context_desc &desc) noexcept {
    if (!window.is_open()) {
        return gltfx_rslt<gltfx_gl_context>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("window"));
    }

    // Step 1 (this file's own top comment): the whole descriptor's
    // shape, before either fixation or any adapter ever sees it.
    if (const gltfx_rslt<void> shape_ok = platform::validate_gl_context_desc(desc);
        shape_ok.has_error()) {
        return gltfx_rslt<gltfx_gl_context>::err(shape_ok.err());
    }

    window_impl *window_impl_ptr = window_internal_access::get(window);
    // window.is_open() already proved m_impl != nullptr above - this
    // assert documents that invariant at THIS call site too, the same
    // "precondition, not a recoverable failure" shape window_facade.
    // cpp's own gltfx_window::open() already carries for the identical
    // class of bug.
    assert(window_impl_ptr != nullptr &&
           "gltfx_gl_context::open() called with a gltfx_window that reported is_open() but has "
           "a null internal impl - precondition violated, not a recoverable error");

    const std::span<const gltfx_gfx_option_entry> requested(desc.options, desc.option_count);

    // Step 2: D-W6b-25's own fixation, identical by construction on
    // every system.
    std::optional<std::span<const gltfx_gfx_option_entry>> already_fixed;
    if (window_impl_ptr->fixed_open_only_gfx_options.has_value()) {
        already_fixed =
            std::span<const gltfx_gfx_option_entry>(*window_impl_ptr->fixed_open_only_gfx_options);
    }

    // FIX-OOM-B9 (fatia de conserto 19/09/2026, GODS_LAWS.md L-17/L-22
    // do projeto, docs/api-conventions.md R3): `fixation` NAO e' const -
    // a razao mora no ponto de escrita la embaixo (fix_now branch),
    // onde `fixation.fixed` precisa ser MOVIDO, nunca copiado. Nada
    // entre esta linha e aquele branch MUTA `fixation`; a ausencia de
    // `const` existe so' para permitir o unico std::move() que este
    // arquivo faz sobre ela.
    platform::gfx_open_only_fixation_result fixation =
        platform::resolve_gfx_open_only_fixation(already_fixed, requested);

    if (fixation.outcome == platform::gfx_open_only_fixation_outcome::refuse) {
        return gltfx_rslt<gltfx_gl_context>::err(
            gltfx_err(gltfx_err_code::invalid_argument)
                .with_rejected_value(option_name_or_placeholder(fixation.refused_id)));
    }

    // docs/api-conventions.md R3: resolve_gfx_open_only_fixation()'s own
    // header comment - `alloc_failed` means `fixed`/`refused_id` are
    // both meaningless, and this open() must fail cleanly instead of
    // reading either.
    if (fixation.outcome == platform::gfx_open_only_fixation_outcome::alloc_failed) {
        return gltfx_rslt<gltfx_gl_context>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }

    // X-WGL (docs/plano-w6b-placa-e-laco.md fatia 4, 06/09/2026): this
    // file's own CMakeLists.txt comment already predicted it - wiring
    // gl_context_facade.cpp into glintfx_library's own build for the
    // FIRST time (this commit, once both fatia 3 and fatia 4 existed)
    // is the first time clang-tidy ever analyzed it, and bugprone-
    // unchecked-optional-access caught exactly this line: `already_
    // fixed` and `fixation.outcome` are two SEPARATE variables, and
    // gfx_open_only_fixation.cpp's own resolve_gfx_open_only_fixation()
    // sets `outcome` to `accept` ONLY when `already_fixed.has_value()`
    // is true (its own body, verbatim: "result.outcome = already_fixed.
    // has_value() ? accept : fix_now") - a real invariant, but not one
    // the analyzer can see across two variables with no visible
    // correlation between them. Checking has_value() directly, right
    // here, makes the guard local and provable - and degrades to an
    // empty span (never actually reached, by the invariant above)
    // instead of undefined behavior if that invariant is ever broken
    // by a future edit to either file, the same "never trust the
    // caller's own promise, verify locally" discipline this project's
    // own API boundary already applies at the PUBLIC edge (docs/api-
    // conventions.md), now proven cheap enough to also apply here.
    const std::span<const gltfx_gfx_option_entry> fixed_open_only =
        fixation.outcome == platform::gfx_open_only_fixation_outcome::fix_now
            ? std::span<const gltfx_gfx_option_entry>(fixation.fixed)
        : already_fixed.has_value() ? *already_fixed
                                    : std::span<const gltfx_gfx_option_entry>{};

    // GFX-PRESET (D-W6b-35 rule 5): a CONCRETE `preset` in the opening list is expanded BEFORE the
    // adapter opens - the rows of the preset the list did not name are added, the entries the list
    // DID name win (expand_preset), and the label is the preset. The list was validated in step 1,
    // so each option appears at most once: refusing a repeated option is
    // validate_gl_context_desc()'s job, done BEFORE any expansion (D-P1-4). `automatic` waits until
    // the adapter is open (it needs the kind of GPU): see apply_preset_at_open() below.
    std::int64_t requested_preset = k_gltfx_preset_manual;
    for (const gltfx_gfx_option_entry &entry : requested) {
        if (entry.id == gltfx_gfx_option::preset) {
            requested_preset = entry.value;
        }
    }
    const bool concrete_preset = requested_preset >= k_gltfx_preset_power_saving &&
                                 requested_preset <= k_gltfx_preset_performance;
    const platform::preset_expansion expansion =
        concrete_preset ? platform::expand_preset(requested_preset, requested)
                        : platform::preset_expansion{};
    const std::span<const gltfx_gfx_option_entry> effective_request =
        concrete_preset
            ? std::span<const gltfx_gfx_option_entry>(expansion.entries.data(), expansion.count)
            : requested;

    gltfx_rslt<std::vector<gltfx_gfx_option_entry>> resolved_rslt =
        resolve_full_option_table(fixed_open_only, effective_request);
    if (resolved_rslt.has_error()) {
        return gltfx_rslt<gltfx_gl_context>::err(resolved_rslt.err());
    }
    std::vector<gltfx_gfx_option_entry> resolved = std::move(resolved_rslt.value());

    // FACADE-PIN (docs/plano-conserto-fachadas-uaf.md sec. 3/4/7.3/7.4,
    // varredura #7): impl is allocated FIRST, at the address it will
    // live at for the rest of its life - THEN the adapter it owns is
    // opened in place. Before this fatia, `adapter` was a local
    // variable, opened on the STACK, and only afterward moved into this
    // exact allocation - safe today only because wayland_egl_context_
    // adapter's own attach_frame_listener() (registers `this` with the
    // OS) never runs from open(), only from swap_buffers(), always
    // AFTER this move already happened (this plan's own sec. 3 #7: "e'
    // bug na primeira refatoracao que mude a ordem"). gl_context_
    // adapter_port now requires pinned_adapter<A>, which forbids the
    // type this old sequence needed to move - the ordering accident is
    // replaced by a construction that is safe BY DESIGN.
    // WIN-DEBUG-CTORALLOC (07/09/2026, gemeo do achado em gfx_open_
    // only_fixation.cpp) - CORRIGIDO 19/09/2026, L-67: uma versao
    // anterior deste comentario dava a entender que o `try` logo
    // abaixo PROTEGE a construcao de `gl_context_impl{}` (seu membro
    // `current_values`, um `std::vector`, tem construcao padrao sob o
    // mesmo risco de depuracao da Microsoft que este comentario ja
    // citava). NAO PROTEGE, e ha prova, nao so' suspeita: o caso `c6`
    // de /var/tmp/builds/claude-1000/varredura-noexcept/RELATORIO.md
    // reproduziu, ao vivo, uma struct com membro `vector` construida
    // DENTRO de um `try` de funcao comum (nem precisa ser `noexcept`)
    // sob `_ITERATOR_DEBUG_LEVEL != 0` - o processo TERMINA mesmo
    // assim. A falha nao aparece como `std::bad_alloc` capturavel;
    // ela morre por outro caminho, dentro do proprio construtor
    // chamado, antes de qualquer `catch` ter chance de rodar. O `try`/
    // `catch` abaixo continua escrito porque nao ha razao para tira-lo
    // (ele nao piora nada, e cobre o `throw` hipotetico de qualquer
    // outro compilador/modo onde o construtor realmente lance uma
    // excecao capturavel), mas ele NAO e' o que torna este sitio
    // seguro - hoje, nao e'. Este e' um sitio de FAMILIA A (construcao
    // padrao de um TIPO DO PROJETO que carrega `std::vector` por
    // membro - a parte "composicao" que a regua automatica de
    // check_noexcept_alloc.py declara nao enxergar, ESCOPO.md Decisao
    // 13), e o conserto dele e' trabalho FUTURO, ja sequenciado pelo
    // lider: ESCOPO.md Decisao 15 fixa a ordem "1: os 60 ja
    // localizaveis, 2: descobrir como localizar os outros ~80, 3:
    // localiza-los, 4: conserta-los" - este sitio, por carregar o
    // `vector` por composicao (nao construcao direta), cai nos ~80 do
    // passo 2/3, nao no passo 1. Nao consertado aqui; nomeado com
    // precisao para o proximo leitor nao repetir a leitura otimista
    // que este comentario, na forma anterior, convidava.
    gl_context_impl *impl = nullptr;
    try {
        impl = new (std::nothrow) gl_context_impl{};
    } catch (const std::bad_alloc &) {
        return gltfx_rslt<gltfx_gl_context>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }
    if (impl == nullptr) {
        return gltfx_rslt<gltfx_gl_context>::err(gltfx_err(gltfx_err_code::out_of_memory));
    }

    // Step 3: the concrete adapter's own open() - the same call on
    // both platforms, since platform::selected_window_adapter already
    // resolved to the right concrete type by the time this TU was
    // compiled (window_impl.hpp's own #if), and both concrete gl
    // context adapters (fatias 3/4) accept the identical (window
    // adapter, resolved option list) shape.
    const gltfx_rslt<void> opened = impl->adapter.open(
        window_impl_ptr->adapter, std::span<const gltfx_gfx_option_entry>(resolved));

    if (opened.has_error()) {
        delete impl;
        return gltfx_rslt<gltfx_gl_context>::err(opened.err());
    }

    // Only NOW, after a genuinely successful open(), does a `fix_now`
    // outcome get written back to the window - see this file's own
    // top comment for why a failed open() must never leave a window
    // believing something was fixed.
    //
    // FIX-OOM-B9 (achado do lint real do Windows, 19/09/2026, GODS_
    // LAWS.md L-17/L-22 do projeto): esta linha ERA `= fixation.fixed`
    // (copia). `window_impl_ptr->fixed_open_only_gfx_options` e' um
    // `std::optional<std::vector<gltfx_gfx_option_entry>>`
    // (window_impl.hpp); copiar um `std::vector` no lado direito de
    // um `optional<vector>::operator=` invoca o construtor de COPIA do
    // vector, que ALOCA e NAO e' `noexcept` - em QUALQUER sistema,
    // Release incluido (familia B, nao a familia A/MSVC-Debug do
    // achado vizinho duas telas abaixo). `fixation.fixed` nao e' mais
    // lido depois deste ponto (o span `fixed_open_only` que apontava
    // para ele so' foi usado na chamada a resolve_full_option_table(),
    // la em cima, ja terminada) - mover em vez de copiar e' o PRIMEIRO
    // degrau da escada (nao alocar), nao so' um degrau de emergencia:
    // o move-construtor de `std::vector` e' `noexcept` e nunca aloca,
    // ele so' rouba o ponteiro.
    impl->current_values = std::move(resolved);

    // `preset = automatic` in the opening list: resolved NOW, right after the adapter opened (it
    // needs the kind of GPU), before the fixation is committed - a failure here must not leave the
    // window believing something was fixed that no context ever opened with.
    if (requested_preset == k_gltfx_preset_automatic) {
        if (const gltfx_rslt<void> applied_preset =
                apply_preset_at_open(*impl, suggestion_now(*impl).preset, requested);
            applied_preset.has_error()) {
            delete impl;
            return gltfx_rslt<gltfx_gl_context>::err(applied_preset.err());
        }
    }

    if (fixation.outcome == platform::gfx_open_only_fixation_outcome::fix_now) {
        window_impl_ptr->fixed_open_only_gfx_options = std::move(fixation.fixed);
    }

    return gltfx_rslt<gltfx_gl_context>::ok(gltfx_gl_context(impl));
}

gltfx_gl_context::gltfx_gl_context(gltfx_gl_context &&other) noexcept : m_impl(other.m_impl) {
    other.m_impl = nullptr;
}

gltfx_gl_context &gltfx_gl_context::operator=(gltfx_gl_context &&other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = other.m_impl;
        other.m_impl = nullptr;
    }
    return *this;
}

gltfx_gl_context::~gltfx_gl_context() { delete m_impl; }

bool gltfx_gl_context::is_open() const noexcept {
    return m_impl != nullptr && m_impl->adapter.is_open();
}

gltfx_rslt<void> gltfx_gl_context::make_current() noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::make_current() called on a moved-from context - the object no "
           "longer owns an adapter");
    return m_impl->adapter.make_current();
}

gltfx_rslt<gltfx_present_outcome> gltfx_gl_context::swap_buffers() noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::swap_buffers() called on a moved-from context - the object no "
           "longer owns an adapter");
    return m_impl->adapter.swap_buffers();
}

void *gltfx_gl_context::proc_address(std::string_view name) const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::proc_address() called on a moved-from context - the object no "
           "longer owns an adapter");
    return m_impl->adapter.proc_address(name);
}

gltfx_rslt<void> gltfx_gl_context::set_option(gltfx_gfx_option_entry entry) noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::set_option() called on a moved-from context - the object no "
           "longer owns an adapter");

    if (const gltfx_rslt<void> shape_ok =
            platform::validate_gfx_option_entry(entry, /*already_open=*/true);
        shape_ok.has_error()) {
        return gltfx_rslt<void>::err(shape_ok.err());
    }

    // D-W6b-16's own "nunca degrada em silencio": a system that does
    // not have this option refuses BY NAME, never silently ignoring
    // the request.
    if (m_impl->adapter.option_support(entry.id) == gltfx_gfx_option_support::unsupported_here) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::unsupported)
                .with_rejected_value(option_name_or_placeholder(entry.id)));
    }

    if (entry.id == gltfx_gfx_option::preset) {
        return apply_preset_request(*m_impl, entry.value);
    }

    if (const gltfx_rslt<void> applied = m_impl->adapter.apply_option(entry); applied.has_error()) {
        return gltfx_rslt<void>::err(applied.err());
    }

    if (gltfx_gfx_option_entry *held = find_current(*m_impl, entry.id)) {
        held->value = entry.value;
    }

    return gltfx_rslt<void>::ok();
}

gltfx_rslt<std::int64_t> gltfx_gl_context::option(gltfx_gfx_option id) const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::option() called on a moved-from context - the object no longer "
           "owns an adapter");

    // Three rows are COMPUTED at the moment they are read, from two facts the system reports, and
    // reading them writes NOTHING (D-W6b-35 rule 7): `power_source` (where the power comes from),
    // `suggested_preset` and `auto_choice_reason` (the suggestion of right now and why).
    switch (id) {
    case gltfx_gfx_option::power_source:
        return gltfx_rslt<std::int64_t>::ok(static_cast<std::int64_t>(m_impl->power.read()));
    case gltfx_gfx_option::suggested_preset:
        return gltfx_rslt<std::int64_t>::ok(suggestion_now(*m_impl).preset);
    case gltfx_gfx_option::auto_choice_reason:
        return gltfx_rslt<std::int64_t>::ok(suggestion_now(*m_impl).reason);
    case gltfx_gfx_option::vsync:
    case gltfx_gfx_option::frame_rate_cap:
    case gltfx_gfx_option::gpu_preference:
    case gltfx_gfx_option::msaa_samples:
    case gltfx_gfx_option::srgb_framebuffer:
    case gltfx_gfx_option::preset:
        break;
    }

    for (const gltfx_gfx_option_entry &current : m_impl->current_values) {
        if (current.id == id) {
            return gltfx_rslt<std::int64_t>::ok(current.value);
        }
    }

    return gltfx_rslt<std::int64_t>::err(
        gltfx_err(gltfx_err_code::not_found).with_rejected_value(option_name_or_placeholder(id)));
}

gltfx_gfx_option_support gltfx_gl_context::option_support(gltfx_gfx_option id) const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::option_support() called on a moved-from context - the object no "
           "longer owns an adapter");
    return m_impl->adapter.option_support(id);
}

gltfx_gpu_info gltfx_gl_context::gpu() const noexcept {
    assert(m_impl != nullptr &&
           "gltfx_gl_context::gpu() called on a moved-from context - the object no longer owns "
           "an adapter");
    return m_impl->adapter.gpu();
}

// The interior-based access of the drawing layer (gl_context_access.hpp): each is one call over the
// inline accessors of gl_context_impl.hpp, so there is no second copy of any of them.
void *gl_context_interior_resolve(void *interior, const char *name) noexcept {
    return gl_context_resolve_proc(interior, name);
}

std::pair<std::uint32_t, std::uint32_t> gl_context_interior_surface_size(void *interior) noexcept {
    return gl_context_surface_pixel_size(interior);
}

gltfx_rslt<void> gl_context_interior_make_current(void *interior) noexcept {
    return static_cast<gl_context_impl *>(interior)->adapter.make_current();
}

// gl_context_internal_access::get() - the ONLY definition of this
// symbol in the whole library (context.hpp's own header comment on
// gl_context_internal_access explains why the definition living HERE,
// out-of-line, is what makes the passkey work at all - the same
// reasoning display_internal_access::get() already documents in
// display_facade.cpp). No GLINTFX_API on this line: the same
// per-symbol dllexport convention this project's own header comments
// document elsewhere means this stays out of the dynamic symbol table
// of the public glintfx::glintfx target.
gl_context_impl *gl_context_internal_access::get(gltfx_gl_context &context) noexcept {
    return context.m_impl;
}

} // namespace glintfx
