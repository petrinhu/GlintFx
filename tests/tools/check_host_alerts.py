#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_host_alerts.py - HOST-ALERT-GATE (13/10/2026; alertas-fix1, alertas-fix2 e o
# redesenho do selftest, 14/10/2026):
# portao que REPROVA quando um teste da casa gera, no hospedeiro, um alerta que
# chega a mesa do lider: uma negacao do SELinux (AVC) de um conteiner nosso, ou
# um core dump que o systemd-coredump recebe de um processo nosso (cada um vira
# um aviso do setroubleshoot ou do DrKonqi). GODS_LAWS.md L-09 e L-50: teste
# nenhum chega a superficie do lider.
#
# ATRIBUICAO (o portao nunca culpa a casa pelo que e de outro projeto):
#   - AVC: o `scontext` termina na MESMA categoria MCS do conteiner que o
#     portao subiu (`docker inspect -f '{{.ProcessLabel}}'`, por exemplo
#     s0:c123,c456). Medido em 13/10/2026: os AVCs daquele dia eram de outro
#     projeto, com outras categorias; nenhum era nosso.
#   - core dump: um COMPONENTE do COREDUMP_CGROUP e, inteiro, o escopo PROPRIO
#     em que o portao roda o exec_fixture.sh --selftest no hospedeiro
#     (`systemd-run --user --scope --unit glintfx-host-alert-gate-<pid>`; o filho
#     que morre herda o escopo) ou o docker-<id>.scope do conteiner que ele
#     subiu. Componente inteiro, nunca prefixo: o escopo do pid 1234 nao casa o
#     do pid 12345 (achado IMPORTANTE-3 da revisao N3).
#   - LIMITE DECLARADO (IMPORTANTE-4): um AVC sem categoria MCS (um processo do
#     hospedeiro, unconfined, ou de outro software) nao e atribuivel: o registro
#     do AVC nao traz cgroup nem escopo. O portao nao o culpa nem o esconde:
#     conta-o como "nao_atribuiveis" e imprime cada um, linha a linha.
#
# QUANDO MEDE, QUANDO REPROVA SEM MEDIR, QUANDO PULA (IMPORTANTE-2; a regra do
# Automake e do Meson: 77 e "nao se aplica aqui", 99 e "deveria medir e nao
# conseguiu", erro duro, nunca confundido com pulo):
#   - cada fonte de alerta se aplica ou nao a este hospedeiro: o AVC, se o
#     SELinux esta LIGADO, em enforcing OU em permissive (alertas-fix2: em
#     permissive o kernel registra o AVC com permissive=1 e o setroubleshoot
#     avisa do mesmo jeito, com o texto "SELinux is in permissive mode. This
#     access was not denied."; so o SELinux desligado, sem o selinuxfs, nao
#     tem AVC); o core dump, se o kernel.core_pattern e um pipe para o
#     systemd-coredump;
#   - o core dump so se aplica no namespace de PID INICIAL (D-W8-433, CI de
#     14/10/2026, run 38533813188): num conteiner, o /proc/sys/kernel/core_pattern
#     e o do HOSPEDEIRO, e o kernel entrega o core dump ao systemd-coredump do
#     hospedeiro, num diario que de dentro do conteiner nao se le nem se
#     atribui. O fato e medido, nunca o nome do ambiente: o link
#     /proc/self/ns/pid aponta para o namespace inicial, PID_NS_INIT_INO
#     (0xEFFFFFFC, include/uapi/linux/nsfs.h). Sem /proc legivel, presume-se o
#     inicial (a medicao continua exigida). LIMITE DECLARADO, impresso: quem
#     mede o core dump de um teste nosso e a rodada no hospedeiro;
#   - nenhuma se aplica: pulo DECLARADO (rc 77, "Skipped" no ctest), com a
#     razao; nesse hospedeiro, alerta nenhum chega a mesa nenhuma;
#   - um programa que falta (o CLI do docker, o systemd-run, o journalctl) e
#     uma fonte ausente como as outras, nunca um traceback (alertas-fix2): ou
#     o pulo, se nada se aplica, ou a reprovacao com o que falta;
#   - SEM O CLI DO DOCKER, o AVC nao se aplica (D-W8-394): sem conteiner da
#     casa nao ha categoria MCS nossa, `ours_avc` nunca casa, e o AVC do
#     selftest do hospedeiro (unconfined) nao e atribuivel; o portao diz isso na
#     saida e segue so com o core dump, ou pula. Com o CLI e sem a IMAGEM da
#     casa, os conteineres dos outros testes ainda rodam e o portao nao os
#     amostra: fonte ausente, reprova;
#   - alguma se aplica e falta o que a mede (docker e a imagem da casa e o
#     diario de auditoria legivel, para o AVC; o diario do sistema legivel e o
#     escopo do systemd-run, para o core dump): REPROVA (rc 1), com o que falta.
#     Fonte ausente reprova, nunca passa (L-40 do projeto);
#   - "legivel" e medido por CONTEUDO, nunca so pelo rc do journalctl: ao menos
#     um registro de auditoria (o AVC), ao menos um registro do kernel (o
#     diario do sistema). Sem permissao, o journalctl sai 0 e nao mostra nada;
#   - durante a medicao, uma leitura que falha depois do sentinela REPROVA
#     (achado CRITICO-1 da revisao N3: antes, virava lista vazia e passava);
#   - a legibilidade vale para a JANELA, nao so para antes dela (alertas-fix2):
#     a partida do conteiner da casa deixa no diario registros de auditoria
#     (BPF, NETFILTER_CFG) e do kernel (veth, docker0), medido em 14/10/2026;
#     nenhum deles lido na janela, ou o diario ilegivel de novo DEPOIS da
#     leitura, REPROVA. LIMITE DECLARADO: com o core dump aplicavel e SEM docker,
#     nao ha registro do kernel nosso na janela; valem o antes e o depois.
#
# MODOS:
#   --selftest   (check_host_alerts_selftest.py) so registros de exemplo e
#                fatos de hospedeiro escritos a mao: o vermelho e o verde SEM
#                gerar alerta nenhum.
#   --live       mede neste hospedeiro, pelas regras acima.
#
# A COSTURA DE TESTE (redesenho do selftest, 14/10/2026, D-W8-427): toda chamada
# de programa passa por `run` e toda espera por `sleep`, dois parametros que
# descem de live() e measure() ate a ultima funcao que os usa. O default e None,
# resolvido NA HORA DA CHAMADA (subprocess.run e time.sleep), nunca capturado na
# definicao. O selftest troca os dois por um hospedeiro de mentira e roda a
# MEDICAO inteira, measure() e live(), sem programa nenhum e sem alerta nenhum.
# Programa que falta ou que nao executa e fonte ausente tambem no logger e no
# exec_fixture.sh do hospedeiro (D-W8-428): reprova com a razao, sem traceback.
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

SKIP = 77
COREDUMP_MESSAGE_ID = "fc2e22bc6ee647b6b90729ab34a250b1"  # systemd catalog: process dumped core
INITIAL_PID_NAMESPACE = f"pid:[{0xEFFFFFFC}]"  # PID_NS_INIT_INO, include/uapi/linux/nsfs.h
AVC_SCONTEXT = re.compile(r"scontext=(\S+)")
MCS_SUFFIX = re.compile(r":(s0:c\d+,c\d+)$")


@dataclass(frozen=True)
class Avc:
    message: str
    mcs: str  # "s0:cX,cY", ou "" quando o scontext nao tem categoria


@dataclass(frozen=True)
class Coredump:
    cgroup: str
    cmdline: str
    signal: str


@dataclass(frozen=True)
class HostFacts:
    """O que este hospedeiro tem: o que faz um alerta possivel, e o que o mede."""
    selinux_enabled: bool  # ligado: enforcing ou permissive
    core_pipe_to_systemd: bool
    docker_cli: bool  # o programa docker existe (sem ele, conteiner nosso nenhum)
    docker_image: bool
    audit_readable: bool
    system_journal_readable: bool
    user_scope: bool
    initial_pid_namespace: bool  # fora de conteiner: o core dump vai a um diario que se le daqui


def mcs_of(context: str) -> str:
    match = MCS_SUFFIX.search(context)
    return match.group(1) if match else ""


def avcs_from(records: list[dict]) -> list[Avc]:
    avcs = []
    for record in records:
        message = str(record.get("MESSAGE", ""))
        if "avc:" not in message or "denied" not in message:
            continue
        scontext = AVC_SCONTEXT.search(message)
        avcs.append(Avc(message, mcs_of(scontext.group(1)) if scontext else ""))
    return avcs


def coredumps_from(records: list[dict]) -> list[Coredump]:
    return [Coredump(str(r.get("COREDUMP_CGROUP", "")), str(r.get("COREDUMP_CMDLINE", "")),
                     str(r.get("COREDUMP_SIGNAL_NAME", "")))
            for r in records if r.get("MESSAGE_ID") == COREDUMP_MESSAGE_ID]


def ours_avc(avc: Avc, mcs: str) -> bool:
    return bool(mcs) and avc.mcs == mcs


def avc_class(avc: Avc, mcs: str) -> str:
    """'nosso', 'alheio' (categoria de outro conteiner) ou 'nao_atribuivel' (sem categoria)."""
    if ours_avc(avc, mcs):
        return "nosso"
    return "alheio" if avc.mcs else "nao_atribuivel"


def ours_coredump(dump: Coredump, scope: str, container_id: str) -> bool:
    parts = dump.cgroup.split("/")
    if scope and scope in parts:
        return True
    return bool(container_id) and f"docker-{container_id}.scope" in parts


def verdict(avcs: list[Avc], dumps: list[Coredump], mcs: str, scope: str,
            container_id: str) -> int:
    classes = [avc_class(a, mcs) for a in avcs]
    our_dumps = [d for d in dumps if ours_coredump(d, scope, container_id)]
    print(f"avc: lidos={len(avcs)} nossos={classes.count('nosso')} "
          f"alheios={classes.count('alheio')} nao_atribuiveis={classes.count('nao_atribuivel')}")
    print(f"coredump: lidos={len(dumps)} nossos={len(our_dumps)} "
          f"alheios={len(dumps) - len(our_dumps)}")
    for avc, kind in zip(avcs, classes):
        if kind == "nosso":
            print(f"REPROVADO, AVC nosso: {avc.message}")
        elif kind == "nao_atribuivel":
            print(f"NAO ATRIBUIVEL (AVC sem categoria MCS, limite declarado): {avc.message}")
    for d in our_dumps:
        print(f"REPROVADO, core dump nosso: {d.signal} {d.cmdline} ({d.cgroup})")
    return 1 if "nosso" in classes or our_dumps else 0


def avc_applies(facts: HostFacts) -> bool:
    """O AVC so e atribuivel a nos com o SELinux ligado E um conteiner da casa possivel."""
    return facts.selinux_enabled and facts.docker_cli


def core_dump_applies(facts: HostFacts) -> bool:
    """O core dump so e mensuravel com o systemd-coredump E no namespace de PID inicial
    (D-W8-433)."""
    return facts.core_pipe_to_systemd and facts.initial_pid_namespace


def plan(facts: HostFacts) -> tuple[str, list[str]]:
    """'pular' (nenhum alerta nosso possivel), 'reprovar' (fonte ausente) ou 'medir'; e as razoes,
    impressas na saida em toda decisao (os limites declarados tambem)."""
    notes = []
    if facts.selinux_enabled and not facts.docker_cli:
        notes.append("AVC nao se aplica: sem o CLI do docker nao ha conteiner nosso, e um AVC do "
                     "hospedeiro (unconfined, sem categoria MCS) nao e atribuivel")
    if facts.core_pipe_to_systemd and not facts.initial_pid_namespace:
        notes.append("core dump nao se aplica: este processo nao esta no namespace de PID inicial "
                     "(um conteiner), e o kernel entrega o core dump ao systemd-coredump do "
                     "hospedeiro, num diario que daqui nao se le nem se atribui. LIMITE DECLARADO: "
                     "quem mede o core dump de um teste nosso e a rodada no hospedeiro")
    if not avc_applies(facts) and not core_dump_applies(facts):
        return "pular", notes + ["nenhum alerta nosso possivel: sem AVC atribuivel e sem core "
                                 "dump mensuravel daqui"]
    missing = []
    if avc_applies(facts) and not facts.docker_image:
        missing.append(f"AVC: a imagem {HOUSE_IMAGE} ausente")
    if avc_applies(facts) and not facts.audit_readable:
        missing.append("AVC: diario de auditoria ilegivel (nenhum registro lido)")
    if core_dump_applies(facts) and not facts.system_journal_readable:
        missing.append("core dump: diario do sistema ilegivel (nenhum registro do kernel lido)")
    if core_dump_applies(facts) and not facts.user_scope:
        missing.append("core dump: systemd-run --user --scope indisponivel (sem escopo proprio)")
    if missing:
        return "reprovar", notes + missing
    if core_dump_applies(facts) and not facts.docker_cli:
        notes.append("LIMITE DECLARADO: sem docker, nao ha registro do kernel nosso na janela; "
                     "valem a leitura antes e a de depois")
    return "medir", notes


def judge(avc_records: list[dict] | None, dump_records: list[dict] | None, mcs: str,
          scope: str, container_id: str) -> int:
    """O veredito sobre o que foi lido; None e uma leitura que falhou: REPROVA (CRITICO-1)."""
    if avc_records is None or dump_records is None:
        print("REPROVADO: a leitura do diario falhou depois do sentinela (fonte ausente)")
        return 1
    return verdict(avcs_from(avc_records), coredumps_from(dump_records), mcs, scope,
                   container_id)


def read_jsonl(path: Path) -> list[dict]:
    """Um JSON por linha; linhas vazias e de comentario (#, o cabecalho SPDX) ficam de fora."""
    return [json.loads(line) for line in path.read_text().splitlines()
            if line.strip() and not line.startswith("#")]


# --- modo --live ---------------------------------------------------------------

HOUSE_IMAGE = "glintfx-devbuild:verify"
SENTINEL_TAG = "glintfx-host-alert-gate"


def journal(filters: list[str], run=None) -> list[dict] | None:
    """Registros do diario pelos filtros dados, ou None se o journalctl falhar ou faltar."""
    run = subprocess.run if run is None else run
    try:
        done = run(["journalctl", "-o", "json", "--no-pager", *filters],
                   capture_output=True, text=True)
    except OSError as error:
        print(f"journalctl indisponivel: {error}")
        return None
    if done.returncode != 0:
        print(f"journalctl {' '.join(filters)} saiu {done.returncode}: {done.stderr.strip()}")
        return None
    return [json.loads(line) for line in done.stdout.splitlines() if line.strip()]


def journal_since(since: float, filters: list[str], run=None) -> list[dict] | None:
    return journal(["--since", f"@{since:.6f}", *filters], run)


def has_records(filters: list[str], run=None) -> bool:
    """Legivel por CONTEUDO: ao menos um registro (sem permissao, o journalctl sai 0 vazio)."""
    return bool(journal([*filters, "-n", "1"], run))


def runs_ok(command: list[str], run=None) -> bool:
    """O comando roda e sai 0; o programa que falta (OSError) e False, nunca um traceback."""
    run = subprocess.run if run is None else run
    try:
        return run(command, capture_output=True).returncode == 0
    except OSError:
        return False


def in_initial_pid_namespace(root: Path) -> bool:
    """O link /proc/self/ns/pid aponta para o namespace inicial; sem /proc legivel, presume-se o
    inicial, e a medicao do core dump continua exigida (nunca um pulo por falta de dado)."""
    try:
        return os.readlink(root / "proc/self/ns/pid") == INITIAL_PID_NAMESPACE
    except OSError as error:
        print(f"namespace de PID ilegivel ({error}): presume-se o inicial, a medicao segue exigida")
        return True


def host_facts(root: Path = Path("/"), run=None) -> HostFacts:
    """Os fatos deste hospedeiro; `root` e `run` existem para o selftest dar /sys, /proc e os
    programas de mentira (alertas-fix2: host_facts nao tinha celula nenhuma)."""
    enforce = root / "sys/fs/selinux/enforce"
    pattern_file = root / "proc/sys/kernel/core_pattern"
    pattern = pattern_file.read_text().strip() if pattern_file.exists() else ""
    return HostFacts(
        selinux_enabled=enforce.exists(),
        core_pipe_to_systemd=pattern.startswith("|") and "systemd-coredump" in pattern,
        docker_cli=runs_ok(["docker", "--version"], run),
        docker_image=runs_ok(["docker", "image", "inspect", HOUSE_IMAGE], run),
        audit_readable=has_records(["_TRANSPORT=audit"], run),
        system_journal_readable=has_records(["_TRANSPORT=kernel"], run),
        user_scope=runs_ok(["systemd-run", "--user", "--scope", "--quiet", "true"], run),
        initial_pid_namespace=in_initial_pid_namespace(root))


def window_gaps(facts: HostFacts, container_ran: bool, audit_records: list[dict],
                kernel_records: list[dict], run=None) -> list[str]:
    """O que falta para a leitura valer na JANELA: os registros que a partida do conteiner da casa
    deixa (auditoria e kernel), e o diario ainda legivel DEPOIS da leitura (alertas-fix2)."""
    gaps = []
    if avc_applies(facts) and container_ran and not audit_records:
        gaps.append("nenhum registro de auditoria na janela, nem os da partida do conteiner")
    if core_dump_applies(facts) and container_ran and not kernel_records:
        gaps.append("nenhum registro do kernel na janela, nem os da rede do conteiner")
    if avc_applies(facts) and not has_records(["_TRANSPORT=audit"], run):
        gaps.append("o diario de auditoria ficou ilegivel depois da leitura")
    if core_dump_applies(facts) and not has_records(["_TRANSPORT=kernel"], run):
        gaps.append("o diario do sistema ficou ilegivel depois da leitura")
    return gaps


def run_house_container(repo: Path, run=None) -> tuple[str, str, list[str]]:
    """Sobe o conteiner da casa: (id, MCS, origens das montagens). Roda dentro
    dele o python sem -I e o exec_fixture.sh --selftest como root."""
    run = subprocess.run if run is None else run
    # `:ro,z`, minusculo, de proposito (D-W8-429): o repositorio e o mesmo que os outros testes
    # de conteiner da casa montam AO MESMO TEMPO (o ctest roda em paralelo), e o CLAUDE.md do
    # projeto manda o `:z` compartilhado nesse caso. O `:Z` privado reetiquetaria a pasta so
    # para este conteiner e tiraria o acesso dos outros (incidente de 07/09/2026: 10 mil
    # alertas por minuto).
    created = run(
        ["docker", "create", "-v", f"{repo}:/src:ro,z", "-e", "GLINTFX_PYTHON3=python3",
         HOUSE_IMAGE, "sh", "-c",
         "python3 -c 'import site, sys; print(site.ENABLE_USER_SITE, sys.path)'; "
         "/src/tests/container/exec_fixture.sh --selftest"],
        capture_output=True, text=True, check=True)
    cid = created.stdout.strip()
    label = run(["docker", "inspect", "-f", "{{.ProcessLabel}}", cid],
                capture_output=True, text=True, check=True).stdout.strip()
    sources = run(["docker", "inspect", "-f", "{{range .Mounts}}{{.Source}} {{end}}", cid],
                  capture_output=True, text=True, check=True).stdout.split()
    started = run(["docker", "start", "-a", cid], capture_output=True, text=True)
    print(f"conteiner {cid[:12]} rc={started.returncode} rotulo={label}")
    run(["docker", "rm", "-f", cid], capture_output=True)
    return cid, mcs_of(label), sources


def wait_for_sentinel(since: float, marker: str, run=None, sleep=None) -> bool:
    """Escreve um registro neutro e espera le-lo: prova que a leitura do diario
    alcanca a janela (piso nao-vazio, L-40). Depois, 5 s para o systemd-coredump.
    O logger que falta ou falha e fonte ausente: False, com a razao (D-W8-428)."""
    run = subprocess.run if run is None else run
    sleep = time.sleep if sleep is None else sleep
    try:
        run(["logger", "-t", SENTINEL_TAG, marker], check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"logger indisponivel, o sentinela nao foi escrito: {error}")
        return False
    for _ in range(40):
        seen = journal_since(since, [f"SYSLOG_IDENTIFIER={SENTINEL_TAG}"], run) or []
        if any(marker in str(r.get("MESSAGE", "")) for r in seen):
            sleep(5)
            return True
        sleep(0.5)
    return False


def run_host_selftest(repo: Path, python: str, scope: str, in_scope: bool, run=None) -> int:
    """O exec_fixture.sh --selftest no hospedeiro, no escopo proprio quando ha core dump a medir.
    O programa que falta ou nao executa (OSError) devolve 127, e o portao reprova (D-W8-428)."""
    run = subprocess.run if run is None else run
    prefix = ["systemd-run", "--user", "--scope", "--quiet", "--unit", scope] if in_scope else []
    try:
        host = run([*prefix, str(repo / "tests/container/exec_fixture.sh"), "--selftest"],
                   capture_output=True, text=True,
                   env={**os.environ, "GLINTFX_PYTHON3": python})
    except OSError as error:
        print(f"exec_fixture.sh --selftest no hospedeiro nao executou: {error}")
        return 127
    print(f"exec_fixture.sh --selftest no hospedeiro: rc={host.returncode}")
    return host.returncode


def measure(repo: Path, python: str, facts: HostFacts, run=None, sleep=None) -> int:
    since = time.time()
    scope = f"glintfx-host-alert-gate-{os.getpid()}"
    host_rc = run_host_selftest(repo, python, scope, core_dump_applies(facts), run)
    try:
        cid, mcs, sources = (run_house_container(repo, run) if facts.docker_image
                             else ("", "", []))
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"REPROVADO: o conteiner da casa nao subiu (fonte ausente): {error}")
        return 1
    foreign = [s for s in sources if Path(s).resolve() != repo.resolve()]
    print(f"montagens: {len(sources)}, fora do repositorio: {foreign}")
    if (avc_applies(facts) and not mcs) or not wait_for_sentinel(
            since, f"marca-{os.getpid()}-{since:.0f}", run, sleep):
        print("REPROVADO: sem categoria MCS do conteiner ou sem ler o diario na janela")
        return 1
    avc_records = (journal_since(since, ["_TRANSPORT=audit"], run)
                   if avc_applies(facts) else [])
    dump_records = (journal_since(since, [f"MESSAGE_ID={COREDUMP_MESSAGE_ID}"], run)
                    if core_dump_applies(facts) else [])
    kernel_records = (journal_since(since, ["_TRANSPORT=kernel"], run)
                      if core_dump_applies(facts) and cid else [])
    gaps = window_gaps(facts, bool(cid), avc_records or [], kernel_records or [], run)
    for gap in gaps:
        print(f"REPROVADO: fonte ausente na janela: {gap}")
    result = 1 if gaps else judge(avc_records, dump_records, mcs, f"{scope}.scope", cid)
    if host_rc != 0 or foreign:
        print("REPROVADO: o selftest do hospedeiro falhou, ou o conteiner montou algo fora do repo")
        return 1
    return result


def live(repo: Path, python: str, run=None, sleep=None, root: Path = Path("/")) -> int:
    if not (repo / "tests/container/exec_fixture.sh").is_file():
        print(f"REPROVADO: repositorio sem tests/container/exec_fixture.sh: {repo}")
        return 1
    facts = host_facts(root, run)
    decision, reasons = plan(facts)
    for reason in reasons:
        print(f"host_alert_gate: {reason}")
    if decision == "pular":
        print(f"host_alert_gate: PULO DECLARADO (rc {SKIP})")
        return SKIP
    if decision == "reprovar":
        print("REPROVADO: fonte de medida ausente num hospedeiro onde o alerta e possivel")
        return 1
    return measure(repo, python, facts, run, sleep)


def main(argv: list[str]) -> int:
    if len(argv) == 4 and argv[1] == "--live":
        return live(Path(argv[2]), argv[3])
    print("uso: check_host_alerts.py --live <raiz-do-repo> <python3>")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
