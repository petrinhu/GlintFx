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


def safe(check):
    """Uma celula que chama o que pode nao existir (o portao antes da alertas-fix1) FALHA por
    asserção, com a razao, em vez de derrubar a tabela inteira."""
    try:
        return bool(check())
    except Exception as error:  # noqa: BLE001 - a celula reprova e diz por que
        print(f"check_host_alerts_selftest: excecao na celula: {type(error).__name__}: {error}")
        return False


def quiet(call):
    with redirect_stdout(io.StringIO()):
        return call()


def facts(selinux, core, docker, audit, system, scope):
    return gate.HostFacts(selinux_enforcing=selinux, core_pipe_to_systemd=core,
                          docker_image=docker, audit_readable=audit,
                          system_journal_readable=system, user_scope=scope)


GATE_SCOPE = "glintfx-host-alert-gate-1234.scope"
APP = "/user.slice/user-1000.slice/user@1000.service/app.slice/"


def fix1_cells():
    """alertas-fix1 (revisao N3 de ddec1bbe): o prefixo do escopo (IMPORTANTE-3), o AVC sem
    categoria (IMPORTANTE-4), a leitura que falha (CRITICO-1) e o pular/reprovar/medir
    (IMPORTANTE-2). Cada veredito escrito da regra do cabecalho do portao."""
    no_mcs = gate.Avc("avc:  denied { read } scontext=unconfined_u:unconfined_r:unconfined_t:s0", "")
    return [
        ("14 escopo do pid 12345 nao e o do pid 1234 (prefixo)", safe(lambda: not gate.ours_coredump(
            gate.Coredump(APP + "glintfx-host-alert-gate-12345.scope", "", ""), GATE_SCOPE, ""))),
        ("15 o escopo exato e nosso", safe(lambda: gate.ours_coredump(
            gate.Coredump(APP + GATE_SCOPE, "", ""), GATE_SCOPE, ""))),
        ("16 conteiner cujo id so comeca igual nao e nosso", safe(lambda: not gate.ours_coredump(
            gate.Coredump("/system.slice/docker-" + CONTAINER + ".scope", "", ""), "", "05c4"))),
        ("17 AVC sem categoria: nao atribuivel, impresso, nao reprova", safe(
            lambda: quiet(lambda: gate.verdict([no_mcs], [], OURS, OWN_SCOPE, "")) == 0
            and printed_counts([no_mcs], [], OURS, OWN_SCOPE, "")[0]
            == "avc: lidos=1 nossos=0 alheios=0 nao_atribuiveis=1")),
        ("18 leitura de AVC que falhou reprova", safe(
            lambda: quiet(lambda: gate.judge(None, [], OURS, OWN_SCOPE, "")) == 1)),
        ("19 leitura de core dump que falhou reprova", safe(
            lambda: quiet(lambda: gate.judge([], None, OURS, OWN_SCOPE, "")) == 1)),
        ("20 nenhum alerta possivel: pula", safe(
            lambda: gate.plan(facts(False, False, True, True, True, True))[0] == "pular")),
        ("21 nenhum alerta possivel, mesmo sem nada para medir: pula", safe(
            lambda: gate.plan(facts(False, False, False, False, False, False))[0] == "pular")),
        ("22 SELinux, tudo presente: mede", safe(
            lambda: gate.plan(facts(True, False, True, True, False, False))[0] == "medir")),
        ("23 SELinux sem a imagem: reprova", safe(
            lambda: gate.plan(facts(True, False, False, True, True, True))[0] == "reprovar")),
        ("24 SELinux com o diario de auditoria ilegivel: reprova", safe(
            lambda: gate.plan(facts(True, False, True, False, True, True))[0] == "reprovar")),
        ("25 so core dump, sem docker: mede", safe(
            lambda: gate.plan(facts(False, True, False, False, True, True))[0] == "medir")),
        ("26 so core dump, sem o escopo: reprova", safe(
            lambda: gate.plan(facts(False, True, True, True, True, False))[0] == "reprovar")),
        ("27 so core dump, diario do sistema ilegivel: reprova", safe(
            lambda: gate.plan(facts(False, True, True, True, False, True))[0] == "reprovar")),
        ("28 os dois, tudo presente: mede", safe(
            lambda: gate.plan(facts(True, True, True, True, True, True))[0] == "medir")),
    ]


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
        ("13 a lista inteira: 1 AVC nosso, 1 alheio e 1 sem categoria; 1 core dump nosso e 2 alheios",
         printed_counts(avcs, dumps, OURS, OWN_SCOPE, "abc") == [
             "avc: lidos=3 nossos=1 alheios=1 nao_atribuiveis=1",
             "coredump: lidos=3 nossos=1 alheios=2"]),
    ] + fix1_cells()


def main(argv: list[str]) -> int:
    if argv[1:] != ["--selftest"]:
        print("uso: check_host_alerts_selftest.py --selftest")
        return 2
    table = cells()
    failed = [name for name, ok in table if not ok]
    for name in failed:
        print(f"check_host_alerts_selftest: FALHOU a celula {name}")
    print(f"check_host_alerts_selftest: celulas={len(table)} falhas={len(failed)}")
    if len(table) < 28:
        print("check_host_alerts_selftest: menos celulas que o piso (varredura quebrada)")
        return 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
