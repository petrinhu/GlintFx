#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_host_alerts_selftest.py - HOST-ALERT-GATE: o vermelho e o verde do
# portao (check_host_alerts.py) SO sobre registros de exemplo do diario
# (tests/tools/fixtures/host_alerts/), escritos a mao a partir dos registros
# REAIS de 13/10/2026 (o AVC c400,c612 e de outro projeto; os core dumps de
# `sh -c 'kill -SEGV $$'` sao os do exec_fixture.sh --selftest de antes). A
# prova nao gera alerta nenhum na mesa do lider (L-09, L-50). Uma TABELA
# FECHADA de celulas, cada veredito escrito da regra do cabecalho do portao,
# nunca lido da saida dele. Imprime celulas e falhas, sempre (L-40).
import io
import sys
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_host_alerts as gate  # noqa: E402

HERE = Path(__file__).resolve().parent / "fixtures" / "host_alerts"
OURS = "s0:c7,c8"
OWN_SCOPE = "tab(4202).scope"
CONTAINER = "05c48f7f9318962edbccdf721a750a09b768040c4fd4f7b8664def569e216f04"


def quiet_verdict(avcs, dumps, mcs, cgroup, cid):
    with redirect_stdout(io.StringIO()):
        return gate.verdict(avcs, dumps, mcs, cgroup, cid)


def printed_counts(avcs, dumps, mcs, cgroup, cid):
    """As linhas de contagem que o veredito imprime (o que ele atribuiu a quem)."""
    out = io.StringIO()
    with redirect_stdout(out):
        gate.verdict(avcs, dumps, mcs, cgroup, cid)
    return [line for line in out.getvalue().splitlines() if line.startswith(("avc:", "coredump:"))]


def cells():
    avcs = gate.avcs_from(gate.read_jsonl(HERE / "audit.jsonl"))
    dumps = gate.coredumps_from(gate.read_jsonl(HERE / "coredump.jsonl"))
    foreign_avcs = [a for a in avcs if a.mcs != OURS]
    our_avcs = [a for a in avcs if a.mcs == OURS]
    foreign_dumps = [d for d in dumps if "brave" in d.cmdline]
    return [
        ("1 le 3 AVCs dos 4 registros de auditoria", len(avcs) == 3),
        ("2 le 3 core dumps dos 4 registros", len(dumps) == 3),
        ("3 categoria do conteiner", gate.mcs_of("system_u:system_r:container_t:s0:c250,c881")
         == "s0:c250,c881"),
        ("4 faixa do usuario nao e categoria", gate.mcs_of(
            "unconfined_u:unconfined_r:unconfined_t:s0-s0:c0.c1023") == ""),
        ("5 sem categoria", gate.mcs_of("system_u:system_r:container_t:s0") == ""),
        ("6 categoria vazia nunca casa", not gate.ours_avc(gate.Avc("x", ""), "")),
        ("7 verde: so AVC de outro projeto", quiet_verdict(foreign_avcs, [], OURS, OWN_SCOPE,
                                                            CONTAINER) == 0),
        # S4: a celula 8 reprova SO pelo AVC nosso. Com a lista inteira, um veredito que culpasse o AVC
        # alheio (o mutante MB1, `!=`) tambem dava 1, e a celula nao distinguia (medido em 13/10/2026).
        ("8 vermelho: so o AVC com a nossa categoria", quiet_verdict(our_avcs, [], OURS, OWN_SCOPE,
                                                                     CONTAINER) == 1),
        ("9 vermelho: core dump do nosso escopo", quiet_verdict([], dumps[:1], OURS, OWN_SCOPE,
                                                                "") == 1),
        ("10 vermelho: core dump do nosso conteiner", quiet_verdict([], dumps[1:2], OURS, "",
                                                                    CONTAINER) == 1),
        ("11 verde: core dump alheio", quiet_verdict([], foreign_dumps, OURS, OWN_SCOPE,
                                                     CONTAINER) == 0),
        ("12 verde: outro conteiner", quiet_verdict([], dumps[1:2], OURS, OWN_SCOPE,
                                                    "abc") == 0),
        ("13 a lista inteira: 1 AVC nosso e 2 alheios, 1 core dump nosso e 2 alheios",
         printed_counts(avcs, dumps, OURS, OWN_SCOPE, "abc") == [
             "avc: lidos=3 nossos=1 alheios=2", "coredump: lidos=3 nossos=1 alheios=2"]),
    ]


def main(argv: list[str]) -> int:
    if argv[1:] != ["--selftest"]:
        print("uso: check_host_alerts_selftest.py --selftest")
        return 2
    table = cells()
    failed = [name for name, ok in table if not ok]
    for name in failed:
        print(f"check_host_alerts_selftest: FALHOU a celula {name}")
    print(f"check_host_alerts_selftest: celulas={len(table)} falhas={len(failed)}")
    if len(table) < 13:
        print("check_host_alerts_selftest: menos celulas que o piso (varredura quebrada)")
        return 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
