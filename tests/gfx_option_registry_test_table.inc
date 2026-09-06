// SPDX-License-Identifier: AGPL-3.0-or-later
//
// gfx_option_registry_test_table.inc - the eight literal rows of
// gfx_option_registry_test.cpp's own k_expected_table (see that file's
// own header comment for what this data proves and how it was cross-
// checked against docs/plano-w6b-placa-e-laco.md sec. 11.2's own
// table). Kept as a separate #include rather than inline in the .cpp
// (GODS_LAWS.md L-17), the same precedent tests/gfss_property_
// registry_test_table.inc already set for this project's other
// append-only registry: "eight rows of ratified data" and "the test
// logic that reads them" are two different reasons to change. Not a
// header (no #pragma once, no include guard): it is spliced into
// exactly one aggregate-initializer list, at exactly one call site, by
// design.

    {glintfx::gltfx_gfx_option::vsync, 0, "vsync", glintfx::gltfx_gfx_option_kind::choice,
     glintfx::gltfx_gfx_option_when::live, 0, 2, 1},
    {glintfx::gltfx_gfx_option::frame_rate_cap, 1, "frame_rate_cap",
     glintfx::gltfx_gfx_option_kind::integer, glintfx::gltfx_gfx_option_when::live, 0, 1000, 0},
    {glintfx::gltfx_gfx_option::gpu_preference, 2, "gpu_preference",
     glintfx::gltfx_gfx_option_kind::choice, glintfx::gltfx_gfx_option_when::open_only, 0, 2, 0},
    {glintfx::gltfx_gfx_option::msaa_samples, 3, "msaa_samples",
     glintfx::gltfx_gfx_option_kind::integer, glintfx::gltfx_gfx_option_when::open_only, 0, 16, 0},
    {glintfx::gltfx_gfx_option::srgb_framebuffer, 4, "srgb_framebuffer",
     glintfx::gltfx_gfx_option_kind::toggle, glintfx::gltfx_gfx_option_when::open_only, 0, 1, 0},
    {glintfx::gltfx_gfx_option::preset, 5, "preset", glintfx::gltfx_gfx_option_kind::choice,
     glintfx::gltfx_gfx_option_when::live, 0, 4, 0},
    {glintfx::gltfx_gfx_option::auto_choice_reason, 6, "auto_choice_reason",
     glintfx::gltfx_gfx_option_kind::choice, glintfx::gltfx_gfx_option_when::read_only, 0, 5, 0},
    {glintfx::gltfx_gfx_option::power_source, 7, "power_source", glintfx::gltfx_gfx_option_kind::choice,
     glintfx::gltfx_gfx_option_when::read_only, 0, 2, 0},
