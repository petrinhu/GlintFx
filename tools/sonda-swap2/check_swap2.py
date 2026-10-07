# SPDX-License-Identifier: AGPL-3.0-or-later
"""Piso da sonda swap2: exige as 9 linhas MEASURED variante=1..9 e a linha DRAW_TEST_EXIT da copia
do teste. Contou menos, reprova. O resultado da medicao em si nunca reprova."""
import re
import sys

text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
draw = open(sys.argv[2], encoding="utf-8", errors="replace").read() if len(sys.argv) > 2 else ""
rows = set(re.findall(r"MEASURED variante=([1-9]) ", text))
print(f"check_swap2.py: {len(rows)} variante(s) medida(s) de 9")
for line in re.findall(r"MEASURED variante=.*", text):
    print(line)
draw_ok = re.search(r"DRAW_TEST_EXIT codigo=\d+", draw) is not None
print(f"check_swap2.py: linha DRAW_TEST_EXIT {'presente' if draw_ok else 'AUSENTE'}")
sys.exit(0 if len(rows) == 9 and draw_ok else 1)
