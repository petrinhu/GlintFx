# SPDX-License-Identifier: AGPL-3.0-or-later
#
# src/core/CMakeLists.txt
#
# Core layer (GODS_LAWS.md L-19): pure types and values (today, the
# runtime version) that know nothing about the operating system. No
# file here includes an OS header or a header from a layer above; the
# gate in tests/tools/check_layers.sh reproves the violation instead
# of trusting the discipline of whoever writes the code.
#
# Single subject: the sources of this layer. Properties of the
# `glintfx` target stay in src/CMakeLists.txt; this file only adds
# sources to a target already created in the parent directory.

target_sources(glintfx PRIVATE version.cpp)
