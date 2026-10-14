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
#
# REDESENHO (14/10/2026, D-W8-427 a D-W8-432): as celulas 45 a 57 e a 60 rodam
# a MEDICAO inteira, measure() e live(), contra um hospedeiro de mentira
# (FakeHost) passado pela costura `run`/`sleep` do portao. Um tropeco
# (`no_real_process`) troca o subprocess.run de verdade por um erro durante essas
# celulas: um caminho que escape da costura reprova a celula, em vez de rodar um
# programa de verdade.
# As celulas 58 a 61 (D-W8-433) sao o caso do CI de 14/10/2026: num conteiner,
# fora do namespace de PID inicial, o core dump nao e mensuravel daqui.
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import types
from contextlib import contextmanager, redirect_stdout
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


def facts(selinux, core, docker, audit, system, scope, cli=True, pidns=True):
    return gate.HostFacts(selinux_enabled=selinux, core_pipe_to_systemd=core, docker_cli=cli,
                          docker_image=docker, audit_readable=audit,
                          system_journal_readable=system, user_scope=scope,
                          initial_pid_namespace=pidns)


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


class FakeRun:
    """Um subprocess.run de mentira: por programa (argv[0]), o rc e a saida, ou a excecao."""

    def __init__(self, table):
        self.table = table

    def __call__(self, argv, **_):
        rc, out = self.table.get(argv[0], (0, ""))
        if isinstance(rc, Exception):
            raise rc
        return types.SimpleNamespace(returncode=rc, stdout=out, stderr="")


ONE = '{"MESSAGE": "um registro"}\n'
READABLE = FakeRun({"journalctl": (0, ONE), "docker": (0, ""), "systemd-run": (0, "")})


def fake_root(enforce, pattern, pidns=None):
    """Um /sys e um /proc de mentira, so com o que host_facts le (o link do namespace de PID so
    quando `pidns` e dado; sem ele, host_facts presume o inicial)."""
    root = Path(tempfile.mkdtemp(prefix="glintfx-hostfacts-"))
    if enforce is not None:
        (root / "sys/fs/selinux").mkdir(parents=True)
        (root / "sys/fs/selinux/enforce").write_text(enforce)
    (root / "proc/sys/kernel").mkdir(parents=True)
    (root / "proc/sys/kernel/core_pattern").write_text(pattern)
    if pidns is not None:
        (root / "proc/self/ns").mkdir(parents=True)
        os.symlink(pidns, root / "proc/self/ns/pid")
    return root


PIPE = "|/usr/lib/systemd/systemd-coredump %P %u %g %s %t %c %h %d %F\n"


def facts_of(enforce, pattern, run=READABLE, pidns=None):
    root = fake_root(enforce, pattern, pidns)
    try:
        return quiet(lambda: gate.host_facts(root, run))
    finally:
        shutil.rmtree(root)


def fix2_cells():
    """alertas-fix2 (conferencia da fix1): o que a fix1 criou e nenhuma celula guardava."""
    def on():
        return facts(True, True, True, True, True, True)

    return [
        ("29 categoria que so comeca igual (c7,c80) e alheia", safe(
            lambda: gate.avc_class(gate.Avc("x", "s0:c7,c80"), OURS) == "alheio")),
        ("30 escopo como sufixo de outro componente nao e nosso", safe(lambda: not gate.ours_coredump(
            gate.Coredump(APP + "xxx-" + GATE_SCOPE, "", ""), GATE_SCOPE, ""))),
        ("31 journalctl que sai 1: None, nunca lista vazia", safe(
            lambda: quiet(lambda: gate.journal(["x"], FakeRun({"journalctl": (1, "")}))) is None)),
        ("32 journalctl que falta: None, sem traceback", safe(lambda: quiet(lambda: gate.journal(
            ["x"], FakeRun({"journalctl": (FileNotFoundError(2, "journalctl"), "")}))) is None)),
        ("33 journalctl 0 e vazio: ilegivel", safe(
            lambda: gate.has_records(["x"], FakeRun({"journalctl": (0, "")})) is False)),
        ("34 journalctl com um registro: legivel", safe(
            lambda: gate.has_records(["x"], FakeRun({"journalctl": (0, ONE)})) is True)),
        ("35 enforcing e pipe do systemd-coredump", safe(
            lambda: facts_of("1", PIPE) == on())),
        ("36 permissive tambem tem AVC (setroubleshoot avisa)", safe(
            lambda: facts_of("0", PIPE).selinux_enabled is True)),
        ("37 SELinux desligado e core para arquivo: pula", safe(
            lambda: gate.plan(facts_of(None, "core\n"))[0] == "pular")),
        ("38 sem o CLI do docker e sem o systemd-run: fatos ausentes, sem traceback", safe(
            lambda: facts_of("1", PIPE, FakeRun({
                "journalctl": (0, ONE), "docker": (FileNotFoundError(2, "docker"), ""),
                "systemd-run": (FileNotFoundError(2, "systemd-run"), "")})) == facts(
                True, True, False, True, True, False, cli=False))),
        ("39 nenhum registro de auditoria na janela: falta", safe(lambda: quiet(
            lambda: gate.window_gaps(on(), True, [], [{}], READABLE)) == [
            "nenhum registro de auditoria na janela, nem os da partida do conteiner"])),
        ("40 tudo lido na janela e legivel depois: nada falta", safe(lambda: quiet(
            lambda: gate.window_gaps(on(), True, [{}], [{}], READABLE)) == [])),
        ("41 ilegivel DEPOIS da leitura: falta", safe(lambda: quiet(
            lambda: gate.window_gaps(facts(True, False, True, True, True, True), True, [{}], [],
                                     FakeRun({"journalctl": (0, "")}))) == [
            "o diario de auditoria ficou ilegivel depois da leitura"])),
        ("42 nenhum registro do kernel na janela: falta", safe(lambda: quiet(
            lambda: gate.window_gaps(on(), True, [{}], [], READABLE)) == [
            "nenhum registro do kernel na janela, nem os da rede do conteiner"])),
        # D-W8-394 (o advisor, 14/10): sem o CLI do docker nao ha conteiner nosso, e nenhum AVC e
        # atribuivel a nos (ours_avc exige a categoria MCS do conteiner da casa): o AVC nao se aplica.
        ("43 SELinux ligado sem o CLI do docker, sem core dump: pula e diz por que", safe(
            lambda: (lambda d: d[0] == "pular" and any("sem o CLI do docker" in r for r in d[1]))(
                gate.plan(facts(True, False, False, True, True, True, cli=False))))),
        ("44 SELinux ligado sem o CLI do docker, com core dump: mede e declara o limite", safe(
            lambda: (lambda d: d[0] == "medir" and any(r.startswith("LIMITE DECLARADO") for r in d[1]))(
                gate.plan(facts(True, True, False, False, True, True, cli=False))))),
    ]


AUDIT_BPF = {"MESSAGE": "audit: BPF prog-id=4242 op=LOAD", "_TRANSPORT": "audit"}
KERNEL_VETH = {"MESSAGE": "docker0: port 1(veth4242aaa) entered forwarding state",
               "_TRANSPORT": "kernel"}
OUR_AVC = {"MESSAGE": "avc:  denied { write } for pid=42 comm=\"sh\" "
                      "scontext=system_u:system_r:container_t:s0:c7,c8 "
                      "tcontext=system_u:object_r:user_home_t:s0 tclass=dir permissive=0"}
SCENE = {"host_rc": 0, "create_rc": 0, "label": "system_u:system_r:container_t:" + OURS,
         "extra_mount": "", "raise": {}, "after": {}, "our_dump": False,
         "window": {"_TRANSPORT=audit": [AUDIT_BPF], "_TRANSPORT=kernel": [KERNEL_VETH]}}


class FakeHost:
    """O hospedeiro de mentira da MEDICAO inteira (D-W8-427): responde como o systemd-run, o
    docker, o logger, o journalctl e o exec_fixture.sh, pelo cenario; check=True com rc
    diferente de 0 levanta CalledProcessError, como o subprocess.run. Nenhum programa roda."""

    def __init__(self, repo, **scene):
        self.repo, self.scene = str(repo), {**SCENE, **scene}
        self.calls, self.marker, self.unit = [], "", ""

    def __call__(self, argv, check=False, **_):
        self.calls.append(argv[0])
        failure = self.scene["raise"].get(argv[0])
        if failure is not None:
            raise failure
        rc, out = self.answer(argv)
        if check and rc != 0:
            raise subprocess.CalledProcessError(rc, argv)
        return types.SimpleNamespace(returncode=rc, stdout=out, stderr="")

    def answer(self, argv):
        if argv[0] == "systemd-run" and "--unit" in argv:
            self.unit = argv[argv.index("--unit") + 1]
            return self.scene["host_rc"], ""
        if argv[0].endswith("exec_fixture.sh"):
            return self.scene["host_rc"], ""
        if argv[0] == "logger":
            self.marker = argv[-1]
        if argv[0] == "docker":
            return self.docker(argv[1:])
        return 0, self.journal(argv) if argv[0] == "journalctl" else ""

    def docker(self, args):
        if args[0] == "create":
            return self.scene["create_rc"], CONTAINER + "\n"
        if args[0] == "inspect" and args[2] == "{{.ProcessLabel}}":
            return 0, self.scene["label"] + "\n"
        if args[0] == "inspect":
            return 0, f"{self.repo} {self.scene['extra_mount']}\n"
        return 0, ""

    def journal(self, argv):
        if "--since" not in argv:  # has_records: [filtro, "-n", "1"], antes e depois da janela
            return ONE if self.scene["after"].get(argv[-3], True) else ""
        wanted = argv[-1]
        if wanted.startswith("SYSLOG_IDENTIFIER="):
            return json.dumps({"MESSAGE": self.marker}) + "\n"
        records = list(self.scene["window"].get(wanted, []))
        if wanted == f"MESSAGE_ID={gate.COREDUMP_MESSAGE_ID}" and self.scene["our_dump"]:
            records.append({"MESSAGE_ID": gate.COREDUMP_MESSAGE_ID, "COREDUMP_CMDLINE": "sh",
                            "COREDUMP_CGROUP": APP + self.unit + ".scope",
                            "COREDUMP_SIGNAL_NAME": "SIGSEGV"})
        return "".join(json.dumps(r) + "\n" for r in records)


@contextmanager
def no_real_process():
    """O tropeco: durante a celula, um subprocess.run de verdade e um erro, nunca um programa."""
    real = subprocess.run

    def tripwire(argv, **_):
        raise RuntimeError(f"processo real chamado fora da costura: {argv}")

    subprocess.run = tripwire
    try:
        yield
    finally:
        subprocess.run = real


def fake_repo():
    repo = Path(tempfile.mkdtemp(prefix="glintfx-hostgate-repo-"))
    (repo / "tests/container").mkdir(parents=True)
    (repo / "tests/container/exec_fixture.sh").write_text("")
    return repo


def measured(**scene):
    """(rc, saida) de measure() com os fatos todos presentes e o hospedeiro do cenario."""
    repo, out = fake_repo(), io.StringIO()
    try:
        with no_real_process(), redirect_stdout(out):
            rc = gate.measure(repo, "python3", facts(True, True, True, True, True, True),
                              FakeHost(repo, **scene), lambda _seconds: None)
        return rc, out.getvalue()
    finally:
        shutil.rmtree(repo)


def lived(enforce, pattern, pidns=None, **scene):
    """(rc, programas chamados) de live() com raiz e hospedeiro de mentira."""
    repo, root = fake_repo(), fake_root(enforce, pattern, pidns)
    host = FakeHost(repo, **scene)
    try:
        with no_real_process(), redirect_stdout(io.StringIO()):
            rc = gate.live(repo, "python3", host, lambda _seconds: None, root)
        return rc, host.calls
    finally:
        shutil.rmtree(repo)
        shutil.rmtree(root)


def measure_cells():
    """Redesenho: a ligacao das regras dentro de measure(), que as celulas 1 a 44 nao chamam
    (os sobreviventes MZ1, MZ2 e MZ3 da conferencia da alertas-fix2)."""
    return [
        ("45 janela lida e legivel depois: passa, com o veredito impresso", safe(
            lambda: (lambda r: r[0] == 0 and "montagens: 1, fora do repositorio: []" in r[1]
                     and "avc: lidos=0 nossos=0" in r[1])(measured()))),
        ("46 janela sem registro de auditoria: reprova", safe(lambda: measured(
            window={"_TRANSPORT=kernel": [KERNEL_VETH]})[0] == 1)),
        ("47 diario de auditoria ilegivel depois da leitura: reprova", safe(lambda: measured(
            after={"_TRANSPORT=audit": False})[0] == 1)),
        ("48 rotulo do conteiner sem categoria MCS: reprova", safe(lambda: measured(
            label="system_u:system_r:container_t:s0")[0] == 1)),
        ("49 selftest do hospedeiro saiu 1: reprova", safe(lambda: measured(host_rc=1)[0] == 1)),
        ("50 AVC com a categoria do conteiner da casa: reprova", safe(lambda: measured(
            window={"_TRANSPORT=audit": [AUDIT_BPF, OUR_AVC],
                    "_TRANSPORT=kernel": [KERNEL_VETH]})[0] == 1)),
        ("51 logger ausente: reprova com a razao, sem traceback", safe(
            lambda: (lambda r: r[0] == 1 and "logger indisponivel" in r[1])(measured(**{
                "raise": {"logger": FileNotFoundError(2, "logger")}})))),
        ("52 exec_fixture.sh que nao executa: reprova com a razao, sem traceback", safe(
            lambda: (lambda r: r[0] == 1 and "nao executou" in r[1])(measured(**{
                "raise": {"systemd-run": PermissionError(13, "exec_fixture.sh")}})))),
        ("53 core dump no escopo proprio do portao: reprova", safe(
            lambda: measured(our_dump=True)[0] == 1)),
        ("54 o conteiner montou algo fora do repositorio: reprova", safe(
            lambda: measured(extra_mount="/home/lider")[0] == 1)),
        ("55 o conteiner da casa nao sobe: reprova com a razao, sem traceback", safe(
            lambda: (lambda r: r[0] == 1 and "nao subiu" in r[1])(measured(create_rc=125)))),
    ]


def live_cells():
    """Redesenho: live() inteiro, da raiz de mentira ate o veredito."""
    return [
        ("56 live() sem alerta possivel: pulo declarado (77)", safe(
            lambda: lived(None, "core\n")[0] == gate.SKIP)),
        ("57 live() com SELinux e systemd-coredump: mede ate o fim e passa", safe(
            lambda: (lambda r: r[0] == 0 and "logger" in r[1])(lived("1", PIPE)))),
    ]


INITIAL = "pid:[4026531836]"  # PID_NS_INIT_INO, 0xEFFFFFFC (include/uapi/linux/nsfs.h)
CONTAINER_NS = "pid:[4026534125]"  # o de um conteiner desta maquina, medido em 14/10/2026
ABSENT = {name: FileNotFoundError(2, name) for name in ("journalctl", "systemd-run", "docker",
                                                       "logger")}


def container_cells():
    """D-W8-433 (o CI de 14/10/2026, run 38533813188): num conteiner o core_pattern e o do
    hospedeiro, e o core dump vai a um diario que daqui nao se le: pula e declara o limite."""
    return [
        ("58 core dump fora do namespace de PID inicial, sem AVC: pula e declara o limite", safe(
            lambda: (lambda d: d[0] == "pular" and any("LIMITE DECLARADO" in r for r in d[1]))(
                gate.plan(facts(False, True, False, False, False, False, cli=False,
                                pidns=False))))),
        ("59 host_facts le o namespace de PID: o inicial sim, o do conteiner nao", safe(
            lambda: facts_of("1", PIPE, pidns=INITIAL).initial_pid_namespace is True
            and facts_of("1", PIPE, pidns=CONTAINER_NS).initial_pid_namespace is False)),
        ("60 live() no conteiner do CI (sem journalctl, systemd-run, docker): 77", safe(
            lambda: lived(None, PIPE, CONTAINER_NS, **{"raise": ABSENT})[0] == gate.SKIP)),
        ("61 AVC mensuravel e o core dump num conteiner: mede so o AVC", safe(
            lambda: gate.plan(facts(True, True, True, True, False, False, pidns=False))[0]
            == "medir")),
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
    ] + fix1_cells() + fix2_cells() + measure_cells() + live_cells() + container_cells()


def main(argv: list[str]) -> int:
    if argv[1:] != ["--selftest"]:
        print("uso: check_host_alerts_selftest.py --selftest")
        return 2
    table = cells()
    failed = [name for name, ok in table if not ok]
    for name in failed:
        print(f"check_host_alerts_selftest: FALHOU a celula {name}")
    print(f"check_host_alerts_selftest: celulas={len(table)} falhas={len(failed)}")
    if len(table) < 61:
        print("check_host_alerts_selftest: menos celulas que o piso (varredura quebrada)")
        return 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
