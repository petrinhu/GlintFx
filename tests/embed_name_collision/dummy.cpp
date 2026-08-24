// SPDX-License-Identifier: AGPL-3.0-or-later

// dummy.cpp - the only source of the consumer's OWN `glintfx` target
// (FIX-CONSUMO-2, achado QA-3). Content is irrelevant; what matters is
// that a target with this exact bare name builds alongside glintfx's
// own library without colliding (see CMakeLists.txt in this directory).

namespace collision_consumer {

int dummy_symbol() { return 0; }

} // namespace collision_consumer
