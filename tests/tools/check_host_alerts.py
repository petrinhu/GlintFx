#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_host_alerts.py - HOST-ALERT-GATE (13/10/2026; alertas-fix1, 14/10/2026):
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
#     SELinux esta em enforcing; o core dump, se o kernel.core_pattern e um
#     pipe para o systemd-coredump;
#   - nenhuma se aplica: pulo DECLARADO (rc 77, "Skipped" no ctest), com a
#     razao; nesse hospedeiro, alerta nenhum chega a mesa nenhuma;
#   - alguma se aplica e falta o que a mede (docker e a imagem da casa e o
#     diario de auditoria legivel, para o AVC; o diario do sistema legivel e o
#     escopo do systemd-run, para o core dump): REPROVA (rc 1), com o que falta.
#     Fonte ausente reprova, nunca passa (L-40 do projeto);
#   - "legivel" e medido por CONTEUDO, nunca so pelo rc do journalctl: ao menos
#     um registro de auditoria (o AVC), ao menos um registro do kernel (o
#     diario do sistema). Sem permissao, o journalctl sai 0 e nao mostra nada;
#   - durante a medicao, uma leitura que falha depois do sentinela REPROVA
#     (achado CRITICO-1 da revisao N3: antes, virava lista vazia e passava).
#
# MODOS:
#   --selftest   (check_host_alerts_selftest.py) so registros de exemplo e
#                fatos de hospedeiro escritos a mao: o vermelho e o verde SEM
#                gerar alerta nenhum.
#   --live       mede neste hospedeiro, pelas regras acima.
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
    selinux_enforcing: bool
    core_pipe_to_systemd: bool
    docker_image: bool
    audit_readable: bool
    system_journal_readable: bool
    user_scope: bool


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


def plan(facts: HostFacts) -> tuple[str, list[str]]:
    """'pular' (nenhum alerta possivel), 'reprovar' (fonte ausente) ou 'medir'; e as razoes."""
    if not facts.selinux_enforcing and not facts.core_pipe_to_systemd:
        return "pular", ["nenhum alerta possivel: SELinux fora de enforcing e "
                         "kernel.core_pattern sem systemd-coredump"]
    missing = []
    if facts.selinux_enforcing and not facts.docker_image:
        missing.append(f"AVC: docker ou a imagem {HOUSE_IMAGE} ausente")
    if facts.selinux_enforcing and not facts.audit_readable:
        missing.append("AVC: diario de auditoria ilegivel (nenhum registro lido)")
    if facts.core_pipe_to_systemd and not facts.system_journal_readable:
        missing.append("core dump: diario do sistema ilegivel (nenhum registro do kernel lido)")
    if facts.core_pipe_to_systemd and not facts.user_scope:
        missing.append("core dump: systemd-run --user --scope indisponivel (sem escopo proprio)")
    return ("reprovar", missing) if missing else ("medir", [])


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


def journal(filters: list[str]) -> list[dict] | None:
    """Registros do diario pelos filtros dados, ou None se o journalctl falhar."""
    done = subprocess.run(["journalctl", "-o", "json", "--no-pager", *filters],
                          capture_output=True, text=True)
    if done.returncode != 0:
        print(f"journalctl {' '.join(filters)} saiu {done.returncode}: {done.stderr.strip()}")
        return None
    return [json.loads(line) for line in done.stdout.splitlines() if line.strip()]


def journal_since(since: float, filters: list[str]) -> list[dict] | None:
    return journal(["--since", f"@{since:.6f}", *filters])


def has_records(filters: list[str]) -> bool:
    """Legivel por CONTEUDO: ao menos um registro (sem permissao, o journalctl sai 0 vazio)."""
    return bool(journal([*filters, "-n", "1"]))


def host_facts() -> HostFacts:
    enforce = Path("/sys/fs/selinux/enforce")
    pattern = Path("/proc/sys/kernel/core_pattern").read_text().strip()
    return HostFacts(
        selinux_enforcing=enforce.exists() and enforce.read_text().strip() == "1",
        core_pipe_to_systemd=pattern.startswith("|") and "systemd-coredump" in pattern,
        docker_image=subprocess.run(["docker", "image", "inspect", HOUSE_IMAGE],
                                    capture_output=True).returncode == 0,
        audit_readable=has_records(["_TRANSPORT=audit"]),
        system_journal_readable=has_records(["_TRANSPORT=kernel"]),
        user_scope=subprocess.run(["systemd-run", "--user", "--scope", "--quiet", "true"],
                                  capture_output=True).returncode == 0)


def run_house_container(repo: Path) -> tuple[str, str, list[str]]:
    """Sobe o conteiner da casa: (id, MCS, origens das montagens). Roda dentro
    dele o python sem -I e o exec_fixture.sh --selftest como root."""
    created = subprocess.run(
        ["docker", "create", "-v", f"{repo}:/src:ro,z", "-e", "GLINTFX_PYTHON3=python3",
         HOUSE_IMAGE, "sh", "-c",
         "python3 -c 'import site, sys; print(site.ENABLE_USER_SITE, sys.path)'; "
         "/src/tests/container/exec_fixture.sh --selftest"],
        capture_output=True, text=True, check=True)
    cid = created.stdout.strip()
    label = subprocess.run(["docker", "inspect", "-f", "{{.ProcessLabel}}", cid],
                           capture_output=True, text=True, check=True).stdout.strip()
    sources = subprocess.run(["docker", "inspect", "-f", "{{range .Mounts}}{{.Source}} {{end}}",
                              cid], capture_output=True, text=True, check=True).stdout.split()
    started = subprocess.run(["docker", "start", "-a", cid], capture_output=True, text=True)
    print(f"conteiner {cid[:12]} rc={started.returncode} rotulo={label}")
    subprocess.run(["docker", "rm", "-f", cid], capture_output=True)
    return cid, mcs_of(label), sources


def wait_for_sentinel(since: float, marker: str) -> bool:
    """Escreve um registro neutro e espera le-lo: prova que a leitura do diario
    alcanca a janela (piso nao-vazio, L-40). Depois, 5 s para o systemd-coredump."""
    subprocess.run(["logger", "-t", SENTINEL_TAG, marker], check=True)
    for _ in range(40):
        seen = journal_since(since, [f"SYSLOG_IDENTIFIER={SENTINEL_TAG}"]) or []
        if any(marker in str(r.get("MESSAGE", "")) for r in seen):
            time.sleep(5)
            return True
        time.sleep(0.5)
    return False


def run_host_selftest(repo: Path, python: str, scope: str, in_scope: bool) -> int:
    """O exec_fixture.sh --selftest no hospedeiro, no escopo proprio quando ha core dump a medir."""
    prefix = ["systemd-run", "--user", "--scope", "--quiet", "--unit", scope] if in_scope else []
    host = subprocess.run([*prefix, str(repo / "tests/container/exec_fixture.sh"), "--selftest"],
                          capture_output=True, text=True,
                          env={**os.environ, "GLINTFX_PYTHON3": python})
    print(f"exec_fixture.sh --selftest no hospedeiro: rc={host.returncode}")
    return host.returncode


def measure(repo: Path, python: str, facts: HostFacts) -> int:
    since = time.time()
    scope = f"glintfx-host-alert-gate-{os.getpid()}"
    host_rc = run_host_selftest(repo, python, scope, facts.core_pipe_to_systemd)
    cid, mcs, sources = run_house_container(repo) if facts.docker_image else ("", "", [])
    foreign = [s for s in sources if Path(s).resolve() != repo.resolve()]
    print(f"montagens: {len(sources)}, fora do repositorio: {foreign}")
    if (facts.selinux_enforcing and not mcs) or not wait_for_sentinel(
            since, f"marca-{os.getpid()}-{since:.0f}"):
        print("REPROVADO: sem categoria MCS do conteiner ou sem ler o diario na janela")
        return 1
    avc_records = journal_since(since, ["_TRANSPORT=audit"]) if facts.selinux_enforcing else []
    dump_records = (journal_since(since, [f"MESSAGE_ID={COREDUMP_MESSAGE_ID}"])
                    if facts.core_pipe_to_systemd else [])
    result = judge(avc_records, dump_records, mcs, f"{scope}.scope", cid)
    if host_rc != 0 or foreign:
        print("REPROVADO: o selftest do hospedeiro falhou, ou o conteiner montou algo fora do repo")
        return 1
    return result


def live(repo: Path, python: str) -> int:
    if not (repo / "tests/container/exec_fixture.sh").is_file():
        print(f"REPROVADO: repositorio sem tests/container/exec_fixture.sh: {repo}")
        return 1
    facts = host_facts()
    decision, reasons = plan(facts)
    for reason in reasons:
        print(f"host_alert_gate: {reason}")
    if decision == "pular":
        print(f"host_alert_gate: PULO DECLARADO (rc {SKIP})")
        return SKIP
    if decision == "reprovar":
        print("REPROVADO: fonte de medida ausente num hospedeiro onde o alerta e possivel")
        return 1
    return measure(repo, python, facts)


def main(argv: list[str]) -> int:
    if len(argv) == 4 and argv[1] == "--live":
        return live(Path(argv[2]), argv[3])
    print("uso: check_host_alerts.py --live <raiz-do-repo> <python3>")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
