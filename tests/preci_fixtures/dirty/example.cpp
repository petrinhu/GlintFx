// SPDX-License-Identifier: AGPL-3.0-or-later
//
// example.cpp - dirty control fixture for tools/preci.sh --selftest
// (FUND-4). Two independent defects on purpose: bad formatting (proves
// the clang-format stage) and a real null-pointer dereference (proves
// the clang-tidy/cppcheck stages). Never fix this file; it exists to
// stay red.

int main() {
  int x;
      x = *(int*)0;
  return x;
}
