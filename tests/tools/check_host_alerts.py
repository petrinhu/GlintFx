#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# check_host_alerts.py - HOST-ALERT-GATE (13/10/2026): portao que REPROVA
# quando um teste da casa gera, no hospedeiro, um alerta que chega a mesa do
# lider: uma negacao do SELinux (AVC) de um conteiner nosso, ou um core dump
# que o systemd-coredump recebe de um processo nosso (cada um vira um aviso do
# setroubleshoot ou do DrKonqi). GODS_LAWS.md L-09 e L-50: teste nenhum chega
# a superficie do lider.
#
# ATRIBUICAO (o portao nunca culpa a casa pelo que e de outro projeto):
#   - AVC: o `scontext` termina na MESMA categoria MCS do conteiner que o
#     portao subiu (`docker inspect -f '{{.ProcessLabel}}'`, por exemplo
#     s0:c123,c456). Medido em 13/10/2026: os AVCs daquele dia eram de outro
#     projeto, com outras categorias; nenhum era nosso.
#   - core dump: o COREDUMP_CGROUP contem o escopo PROPRIO em que o portao roda
#     o exec_fixture.sh --selftest no hospedeiro (`systemd-run --user --scope
#     --unit glintfx-host-alert-gate-<pid>`; o filho que morre herda o escopo)
#     ou o escopo docker-<id>.scope do conteiner que o portao subiu. Nunca o
#     cgroup da sessao inteira: outros agentes da mesma aba rodam ao mesmo tempo.
#
# MODOS:
#   --selftest   so leitura de registros de exemplo (tests/tools/fixtures/
#                host_alerts/): prova o vermelho e o verde SEM gerar alerta
#                nenhum (a regra da fatia: a prova nao pode acender a mesa).
#   --live       sobe um conteiner da casa e roda o exec_fixture.sh
#                --selftest no hospedeiro, espera o diario assentar e le os
#                registros desde o inicio. Sai 77 (pulo DECLARADO, contado
#                pelo ctest como Skipped) quando o hospedeiro nao tem como
#                provar: sem docker, sem a imagem, SELinux fora de enforcing
#                ou diario de auditoria ilegivel.
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


def ours_coredump(dump: Coredump, scope: str, container_id: str) -> bool:
    if scope and scope in dump.cgroup:
        return True
    return bool(container_id) and f"docker-{container_id}.scope" in dump.cgroup


def verdict(avcs: list[Avc], dumps: list[Coredump], mcs: str, scope: str,
            container_id: str) -> int:
    our_avcs = [a for a in avcs if ours_avc(a, mcs)]
    our_dumps = [d for d in dumps if ours_coredump(d, scope, container_id)]
    print(f"avc: lidos={len(avcs)} nossos={len(our_avcs)} alheios={len(avcs) - len(our_avcs)}")
    print(f"coredump: lidos={len(dumps)} nossos={len(our_dumps)} "
          f"alheios={len(dumps) - len(our_dumps)}")
    for a in our_avcs:
        print(f"REPROVADO, AVC nosso: {a.message}")
    for d in our_dumps:
        print(f"REPROVADO, core dump nosso: {d.signal} {d.cmdline} ({d.cgroup})")
    return 1 if our_avcs or our_dumps else 0


def read_jsonl(path: Path) -> list[dict]:
    """Um JSON por linha; linhas vazias e de comentario (#, o cabecalho SPDX) ficam de fora."""
    return [json.loads(line) for line in path.read_text().splitlines()
            if line.strip() and not line.startswith("#")]


# --- modo --live: so roda no hospedeiro que tem como provar -------------------

HOUSE_IMAGE = "glintfx-devbuild:verify"
SENTINEL_TAG = "glintfx-host-alert-gate"


def journal_since(since: float, filters: list[str]) -> list[dict] | None:
    """Registros do diario desde `since` (epoch), ou None se a leitura falhar."""
    done = subprocess.run(["journalctl", "-o", "json", "--no-pager", "--since",
                           f"@{since:.6f}", *filters], capture_output=True, text=True)
    if done.returncode != 0:
        print(f"journalctl {' '.join(filters)} saiu {done.returncode}: {done.stderr.strip()}")
        return None
    return [json.loads(line) for line in done.stdout.splitlines() if line.strip()]


def why_cannot_prove(repo: Path) -> str:
    """'' quando este hospedeiro prova; senao, a razao do pulo declarado."""
    enforce = Path("/sys/fs/selinux/enforce")
    if not enforce.exists() or enforce.read_text().strip() != "1":
        return "SELinux fora de enforcing (nao ha AVC a medir)"
    if subprocess.run(["docker", "image", "inspect", HOUSE_IMAGE],
                      capture_output=True).returncode != 0:
        return f"docker ou a imagem {HOUSE_IMAGE} ausente"
    if journal_since(time.time() - 60, ["_TRANSPORT=audit", "-n", "1"]) is None:
        return "diario de auditoria ilegivel para este usuario"
    if not (repo / "tests/container/exec_fixture.sh").is_file():
        return f"repositorio sem tests/container/exec_fixture.sh: {repo}"
    if subprocess.run(["systemd-run", "--user", "--scope", "--quiet", "true"],
                      capture_output=True).returncode != 0:
        return "systemd-run --user --scope indisponivel (sem escopo proprio, sem atribuicao)"
    return ""


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


def live(repo: Path, python: str) -> int:
    reason = why_cannot_prove(repo)
    if reason:
        print(f"host_alert_gate: PULO DECLARADO (rc {SKIP}): {reason}")
        return SKIP
    since = time.time()
    scope = f"glintfx-host-alert-gate-{os.getpid()}"
    host = subprocess.run(["systemd-run", "--user", "--scope", "--quiet", "--unit", scope,
                           str(repo / "tests/container/exec_fixture.sh"), "--selftest"],
                          capture_output=True, text=True,
                          env={**os.environ, "GLINTFX_PYTHON3": python})
    print(f"exec_fixture.sh --selftest no hospedeiro: rc={host.returncode}")
    cid, mcs, sources = run_house_container(repo)
    foreign = [s for s in sources if Path(s).resolve() != repo.resolve()]
    print(f"montagens: {len(sources)}, fora do repositorio: {foreign}")
    if not mcs or not wait_for_sentinel(since, f"marca-{os.getpid()}-{since:.0f}"):
        print("REPROVADO: sem categoria MCS do conteiner ou sem ler o diario na janela")
        return 1
    avcs = avcs_from(journal_since(since, ["_TRANSPORT=audit"]) or [])
    dumps = coredumps_from(journal_since(since, [f"MESSAGE_ID={COREDUMP_MESSAGE_ID}"]) or [])
    result = verdict(avcs, dumps, mcs, f"{scope}.scope", cid)
    if host.returncode != 0 or foreign:
        print("REPROVADO: o selftest do hospedeiro falhou, ou o conteiner montou algo fora do repo")
        return 1
    return result


def main(argv: list[str]) -> int:
    if len(argv) == 4 and argv[1] == "--live":
        return live(Path(argv[2]), argv[3])
    print("uso: check_host_alerts.py --live <raiz-do-repo> <python3>")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
