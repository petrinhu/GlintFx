#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# die_quietly.py <SINAL> - TEST-CRASH-QUIET: morre pelo sinal dado (SEGV,
# ABRT, ...) SEM gerar core dump, para os controles negativos que provam a
# leitura "rc=128+n signal=NOME" (tests/container/exec_fixture.sh --selftest).
#
# POR QUE NAO `sh -c 'kill -SEGV $$'`: o processo morto assim e "dumpable", e
# nesta maquina o kernel.core_pattern e um pipe para o systemd-coredump. Com
# pipe, o kernel ignora RLIMIT_CORE (fs/coredump.c, coredump_pipe(): "core
# limits are irrelevant to pipes"; o limite vira RLIM_INFINITY), o
# systemd-coredump so deixa de GUARDAR o arquivo (coredump-submit.c, "Resource
# limits disable core dumping") e o DrKonqi avisa mesmo sem o arquivo
# (drkonqi src/coredump/processor/main.cpp: "we may still wish to notify that
# something has crashed"). Medido em 13/10/2026: 230 pares SEGV/ABRT de
# `sh -c 'kill -<SINAL> $$'` no coredump do hospedeiro desde 11/10, um par por
# rodada da suite, cada um um alerta "programa travou" na mesa do lider.
#
# A FORMA CERTA, a mesma de tests/precondition_fixtures/precondition_fixture.cpp
# e de tests/tools/fixtures/loop_mark_fixture.cpp: prctl(PR_SET_DUMPABLE, 0).
# Processo nao-dumpable faz o do_coredump() do kernel voltar ANTES de qualquer
# tratador, pipe ou arquivo (fs/coredump.c, coredump_skip():
# TASK_DUMPABLE_OFF). O sinal e o codigo de saida nao mudam: o pai continua
# vendo 128 + n.
#
# Nunca cai para o caminho que gera core: se o prctl falhar, ou se a leitura de
# volta (PR_GET_DUMPABLE) nao der 0, sai 3 com a razao, SEM se matar.
# Sinal desconhecido: sai 2. Se o processo sobreviver ao proprio sinal: sai 4.
import ctypes
import os
import signal
import sys

PR_GET_DUMPABLE = 3  # include/uapi/linux/prctl.h
PR_SET_DUMPABLE = 4


def signal_of(name: str) -> signal.Signals | None:
    try:
        return signal.Signals["SIG" + name]
    except KeyError:
        return None


def become_non_dumpable() -> str:
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0:
        return f"prctl(PR_SET_DUMPABLE, 0) falhou: errno {ctypes.get_errno()}"
    state = libc.prctl(PR_GET_DUMPABLE, 0, 0, 0, 0)
    if state != 0:
        return f"PR_GET_DUMPABLE deu {state}, nao 0"
    return ""


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("uso: die_quietly.py <SINAL>", file=sys.stderr)
        return 2
    sig = signal_of(argv[1])
    if sig is None:
        print(f"die_quietly: sinal desconhecido: {argv[1]}", file=sys.stderr)
        return 2
    refusal = become_non_dumpable()
    if refusal:
        print(f"die_quietly: RECUSA, {refusal}", file=sys.stderr)
        return 3
    print(f"die_quietly: morrendo por SIG{argv[1]} sem core dump", flush=True)
    signal.signal(sig, signal.SIG_DFL)
    os.kill(os.getpid(), sig)
    print("die_quietly: sobreviveu ao proprio sinal", file=sys.stderr)
    return 4


if __name__ == "__main__":
    sys.exit(main(sys.argv))
