# SPDX-License-Identifier: AGPL-3.0-or-later
"""Piso da sonda swap2: exige as 4 linhas MEASURED variante=1..4. Contou menos de 4, reprova."""
import re
import sys

text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
rows = re.findall(r"MEASURED variante=([1-4]) .*", text)
print(f"check_swap2.py: {len(set(rows))} variante(s) medida(s) de 4")
for line in re.findall(r"MEASURED variante=.*", text):
    print(line)
sys.exit(0 if len(set(rows)) == 4 else 1)
