// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glintfx/core/err.hpp>
#include <glintfx/export.hpp>

// platform/gl/gpu.hpp - GL-CONTEXT/GL-GPU-KIND (docs/plano-w6b-placa-
// e-laco.md fatia 2b, sec. 10, D-W6b-13/14; docs/plano-w6b-fatias-5.md
// sec. 4, D-W6b-30; docs/plano-w6b-fatias-5b-revisao.md, D-W6b-37/38,
// GODS_LAWS.md L-19/L-26): the ONE thing this library promises about
// which GPU(s) render a consumer's contexts - INFORMING, never
// CHOOSING (sec. 10.1's own busca: choosing a GPU is a request the
// executable itself has to make on Windows - exported variables an
// .exe carries, not a .dll - and a best-effort environment variable
// read by the DRIVER on Linux; neither is something this library can
// guarantee alone, on either system).
//
// `unknown` IS THE HONEST DEFAULT, NEVER "PROVAVELMENTE INTEGRADA"
// (D-W6b-13, verbatim in the plan): a system that never told this
// library which GPU it is using, or how to classify it, reports
// `unknown`, not a guess.
//
// WHO CLASSIFIES (the 06/09/2026 order this whole fatia executes,
// verbatim: "quem identifica as placas de video e o OS... nos nao
// precisamos assumir tarefas do OS quanto a hardware"): the CLASSIFI-
// CATION rules themselves are never this header's job - they live in
// the platform-specific atoms GL-GPU-KIND adds, each one a QUESTION TO
// THE SYSTEM, never a verdict this library invents.
//
// GLINTFX-DRM-DRIVER-TABLE: amdgpu i915 xe nouveau
// (tests/tools/check_drm_driver_table.sh's own marked block - the FOUR
// DRM driver names the kernel answers a classification question for,
// in src/platform/wayland/drm_gpu_kind.cpp's own if-chain; `nvidia-
// drm`, and every driver name not on this line, falls through to the
// two vias below INSTEAD of a kernel answer - it is deliberately NOT
// on this line, and the gate would refuse it if it appeared here
// without a matching branch in the .cpp).
//   - Linux: the kernel's own DRM ioctl on the render node the driver
//     is using (drm_gpu_kind.hpp) - `amdgpu` answers "is this an APU",
//     `i915`/`xe` answer "does this device have its own VRAM region",
//     `nouveau` answers its own platform/bus-type. When the kernel has
//     no question of its own for the driver (`nvidia-drm`, and any
//     driver not yet written here), TWO vias the lider ordered on
//     06/09/2026 ask the system two DIFFERENT ways before giving up:
//     (1) exclusion - "is some OTHER GPU on this machine `shared`,
//     answered by the kernel" (gpu_kind_exclusion.hpp); (2) memory
//     separation - "does GL_NVX_gpu_memory_info report this device's
//     own memory pool as separate from system RAM" (gl_memory_facts.
//     hpp/memory_separation_kind.hpp, shared with Windows below).
//   - Windows: DXCore's own `IsHardware`/`IsIntegrated` properties, per
//     adapter, WITHOUT counting how many adapters exist (dxcore_gpu_
//     kind.hpp) - DXGI plays no part in this decision (revisao.md sec.
//     3, D-W6b-37: "DXGI sai do desenho inteiro"). The GL_NVX via 2
//     above serves as Windows' own reserve for when DXCore itself does
//     not answer `IsIntegrated` (D-W6b-37 regra 4).
//
// `name` (below) is NEVER INTERPRETED BY TEXT (sec. 10's CTO
// instruction, verbatim): GL_RENDERER has no standardized format,
// changes without notice between driver versions, and can fail to
// identify anything meaningful at all - this is exactly the mistake
// the busca's own finding names ("alguem no mundo real acusou de
// software um driver acelerado por conter certa palavra no nome").
// `kind` is decided by TYPE (the atoms above), never by scanning this
// string - `name` exists for a human support log, not for program
// logic.
//
// THE HYBRID-LAPTOP DOR (docs/plano-w6b-fatias-5.md sec. 1, market
// research the lider's own 06/09/2026 order required): a game running
// on the WRONG GPU of a hybrid laptop is a real, widely-reported
// complaint (Godot #81870/#28437, godot-proposals #5230, Road to
// Vostok/Tom's Hardware forum threads) - and so is the opposite ("I
// want the integrated one and it keeps waking the dedicated one"). V1
// does NOT pick a GPU for the consumer (that stays `GL-GPU-PREFERENCE`,
// out of scope); it answers the two questions a consumer needs to
// SHOW the player which GPU is active and that another one exists:
// gltfx_gl_context::gpu() (context.hpp) for "where am I running right
// now", and gltfx_gpu_enumeration::query() (below) for "what else does
// this machine have" - a UI built on both can render the exact message
// the research above named: "rodando na integrada; ha uma dedicada;
// mude em Configuracoes > Graficos (Windows) ou DRI_PRIME=1 (Linux)".
//
// CLASSIFICATION IS MEASURED IN CI ONLY FOR `software` (the one value
// every executor - the Mesa D3D12 "Microsoft Basic Render Driver" on
// Windows, `llvmpipe` in the Linux container - actually has); `shared`/
// `dedicated` are proved by fixture and by mutation against what the
// kernel/DXCore themselves document (drm_gpu_kind_test, dxcore_gpu_
// kind_test), never against real hardware this project's CI does not
// have. Windows older than version 2004 (DXCore's own minimum) answers
// `unknown` - it never had the property to ask.

namespace glintfx {

// Four values, closed (sec. 10.1's own "as regras sao FECHADAS", plus
// D-W6b-13's own contract): `unknown` is both the numeric first value
// AND gltfx_gpu_info's own default-constructed state below - the same
// "a value nobody set reads back as the honest default" convention
// gltfx_gfx_option_when::read_only plays for gltfx_gfx_option_info
// (gfx_option.hpp). std::uint8_t: a closed, structural vocabulary that
// never grows the way gltfx_gfx_option's own append-only ids do.
enum class gltfx_gpu_kind : std::uint8_t {
    unknown,
    software,
    shared,
    dedicated,
};

// The sentinel gltfx_gpu_info::enumeration_index (below) reads back
// when this GPU's position in gltfx_gpu_enumeration::at() is not known
// - EITHER because no gltfx_gpu_enumeration has been queried yet, OR
// because the enumeration ran and this GPU could not be matched inside
// it (D-W6b-33: NOT the same thing as index 0, a real, addressable
// position). The maximum std::uint32_t value: no real enumeration this
// library builds ever has four billion entries, so this can never
// collide with a genuine index.
inline constexpr std::uint32_t k_gltfx_gpu_index_unknown = 0xFFFFFFFFu;

// What a concrete, open gltfx_gl_context knows about the GPU it is
// rendering on, right now (D-W6b-13's own consolidation of the two
// separate accessors an earlier draft of this plan proposed - gpu_
// kind()/gpu_name() - into the ONE value type gltfx_gl_context::gpu()
// returns). `name` is whatever GL_RENDERER the driver reports - always
// available once a context is current, even when `kind` itself is
// `unknown` - so a consumer always has SOMETHING to put in a support
// log, even on a system this library cannot classify.
//
// `enumeration_index` is the position this GPU has in gltfx_gpu_
// enumeration::at() (GL-GPU-KIND, this fatia) - `k_gltfx_gpu_index_
// unknown` until a gltfx_gpu_enumeration has actually been queried and
// this GPU located inside it (a consumer that never calls query() sees
// the sentinel forever, never a fabricated 0 - CHANGE OF BEHAVIOR,
// dated 06/09/2026: an earlier draft of this same field, before GL-
// GPU-KIND existed, always read back 0). NOT spelled `index` (docs/
// api-conventions.md R6): the bare form collides with a REAL, active
// macro this project's own five target platforms can still reach
// transitively through a windowing system header this project's own
// dependency-zero rule does not forbid a CONSUMER from also including
// alongside glintfx - a legacy pre-POSIX BSD compatibility `#define`
// aliasing `index` to `strchr` - found live by tests/tools/check_
// public_name_collision.py's own real-compiler scan, the same class of
// finding that already renamed gltfx_err_code away from `error_code`.
struct gltfx_gpu_info {
    gltfx_gpu_kind kind = gltfx_gpu_kind::unknown;
    std::string_view name;
    std::uint32_t enumeration_index = k_gltfx_gpu_index_unknown;
};

// Opaque (GODS_LAWS.md L-19): the real layout - a std::vector<gltfx_
// gpu_info> plus the std::string storage its own string_view names
// point into (gpu_enumeration_impl.hpp) - lives entirely inside gpu_
// enumeration_facade.cpp, never installed, never public. The SAME
// shape gl_context_impl/window_impl already use one header over.
struct gpu_enumeration_impl;

// gltfx_gpu_enumeration - "what else does this machine have" (D-W6b-
// 20/33, this header's own top comment): every GPU this system's own
// mechanism (EGL device enumeration + kernel DRM ioctl on Linux,
// DXCore adapter enumeration on Windows) reports, DEDUPLICATED (a
// single physical card enumerated twice by the platform's own API,
// F3's own measured NVIDIA-seen-twice case, never counted as two
// GPUs), in the SAME gltfx_gpu_kind/name shape gltfx_gl_context::gpu()
// already uses.
//
// query() NEVER APPLIES THE EXCLUSION/MEMORY-SEPARATION VIAS this same
// fatia's atoms give gltfx_gl_context::gpu() for the CURRENT context
// (gpu_kind_exclusion.hpp/memory_separation_kind.hpp) - it reports
// each entry's PLAIN kernel/DXCore answer. A driver the kernel cannot
// classify on its own (`nvidia-drm`) can therefore show `unknown` here
// even on a machine where gpu().kind resolves it to `dedicated` by
// exclusion - the two answer DIFFERENT questions ("what does the
// system say about EVERY GPU" vs "what does THIS context's own
// adapter conclude about the one it is running on"), and this header
// does not blur them into one number.
//
// Move-only (a live enumeration owns its own storage) - the SAME
// resource shape gltfx_gl_context/gltfx_window already have.
class gltfx_gpu_enumeration {
  public:
    // Never fails by returning an EMPTY enumeration for "this system
    // has no way to enumerate" - it fails with gltfx_rslt's own error
    // path, `gltfx_err_code::unsupported`, `rejected_value()` naming
    // the missing mechanism ("egl_device_enumeration" on Linux,
    // "dxcore" on Windows) - GODS_LAWS.md L-40's own "contou zero,
    // reprova" is a caller-side floor (`count() >= 1`), not something
    // this factory silently swallows into an empty success.
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_gpu_enumeration> query() noexcept;

    gltfx_gpu_enumeration(const gltfx_gpu_enumeration &) = delete;
    gltfx_gpu_enumeration &operator=(const gltfx_gpu_enumeration &) = delete;

    GLINTFX_API gltfx_gpu_enumeration(gltfx_gpu_enumeration &&other) noexcept;
    GLINTFX_API gltfx_gpu_enumeration &operator=(gltfx_gpu_enumeration &&other) noexcept;

    GLINTFX_API ~gltfx_gpu_enumeration();

    [[nodiscard]] GLINTFX_API std::size_t count() const noexcept;

    // `index >= count()` returns a default-constructed gltfx_gpu_info
    // (`unknown`, empty name, `k_gltfx_gpu_index_unknown`) - an ordinary
    // out-of-range lookup miss, the same "no gltfx_err for a query that
    // can only ever be a caller mistake against a size it can already
    // read" reasoning std::vector::operator[] vs at() already embodies
    // elsewhere in this library's own public surface (docs/api-
    // conventions.md R1).
    [[nodiscard]] GLINTFX_API gltfx_gpu_info at(std::size_t index) const noexcept;

  private:
    explicit gltfx_gpu_enumeration(gpu_enumeration_impl *impl) noexcept : m_impl(impl) {}

    gpu_enumeration_impl *m_impl = nullptr;
};

} // namespace glintfx
