// SPDX-License-Identifier: AGPL-3.0-or-later
// Reproducao minima do achado real do commit 0628ad8 (GlintFx, 06/09/2026):
// std::setvbuf(stdout, nullptr, _IOLBF, 0) - size=0 e invalido para o modo
// _IOLBF/_IOFBF na documentacao da Microsoft (2 <= size <= INT_MAX), e o
// runtime da Microsoft chama seu invalid-parameter-handler, que ENCERRA O
// PROCESSO, em vez de devolver erro. glibc nunca validou essa faixa.
#include <cstdio>
int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    return 0;
}
