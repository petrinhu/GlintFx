#!/usr/bin/env bash
# runner_up.sh — sobe o container do runner self-hosted "glintfx-heavy" para o repo
# petrinhu/GlintFx, a partir da imagem local já construída `localhost/glintfx-runner:f44`.
#
# Este script NÃO constrói a imagem (isso é tarefa de um Containerfile próprio, ainda
# não versionado — ver GODS_LAWS.md L-05/L-06 antes de escrever um: a imagem herdada
# carrega Xvfb e libxkbcommon-devel, que são proibidos neste projeto).
#
# Uso:
#   GLINTFX_RUNNER_TOKEN=<token-de-registro-efêmero> ./tools/ci/runner_up.sh
#
# O token NUNCA é lido de arquivo neste repositório, nunca vira variável de ambiente do
# container (docker inspect a exporia) e nunca vira argumento de `docker run` (ps a
# exporia); ele é passado por UMA linha via stdin, como o entrypoint da imagem exige.
# Gere-o na hora com:
#   gh api -X POST repos/petrinhu/GlintFx/actions/runners/registration-token --jq .token
#
# Cada função abaixo faz uma coisa (GODS_LAWS.md L-17).

set -euo pipefail

readonly RUNNER_IMAGE="localhost/glintfx-runner:f44"
readonly RUNNER_CONTAINER_NAME="glintfx-heavy-runner"
readonly RUNNER_REPO_URL="https://github.com/petrinhu/GlintFx"
readonly RUNNER_LABELS="self-hosted,linux,x64,fedora44,glintfx-heavy"
readonly RUNNER_MEMORY="8g"
readonly RUNNER_CPUS="4"
readonly RUNNER_PIDS_LIMIT="4096"
readonly RUNNER_CLIENT_LOG="/var/tmp/glintfx-heavy-runner.client.log"

fail() {
	echo "runner_up.sh: $1" >&2
	exit 1
}

require_registration_token() {
	if [[ -z "${GLINTFX_RUNNER_TOKEN:-}" ]]; then
		fail "defina GLINTFX_RUNNER_TOKEN (token de registro efêmero, gerado por 'gh api -X POST repos/petrinhu/GlintFx/actions/runners/registration-token --jq .token')"
	fi
}

require_image_present() {
	if ! docker image inspect "$RUNNER_IMAGE" >/dev/null 2>&1; then
		fail "imagem '$RUNNER_IMAGE' não encontrada localmente; construa-a antes (ver nota no topo do arquivo)"
	fi
}

fail_if_container_exists() {
	if docker ps -a --format '{{.Names}}' | grep -qx "$RUNNER_CONTAINER_NAME"; then
		fail "container '$RUNNER_CONTAINER_NAME' já existe; remova-o antes (docker rm -f $RUNNER_CONTAINER_NAME)"
	fi
}

build_docker_run_command() {
	# Uma string, não array: precisa atravessar `setsid bash -c '...'` inteira (ver
	# launch_runner_client). O token entra só no `printf` do lado esquerdo do pipe.
	printf 'docker run -i --name %q --memory=%q --memory-swap=%q --cpus=%q --pids-limit=%q --cap-drop=ALL --security-opt=no-new-privileges -e %q -e %q %q' \
		"$RUNNER_CONTAINER_NAME" "$RUNNER_MEMORY" "$RUNNER_MEMORY" "$RUNNER_CPUS" "$RUNNER_PIDS_LIMIT" \
		"GLINTFX_RUNNER_REPO_URL=$RUNNER_REPO_URL" "GLINTFX_RUNNER_LABELS=$RUNNER_LABELS" "$RUNNER_IMAGE"
}

launch_runner_client() {
	# `docker run -d -i` NÃO repassa stdin canalizado nesta instalação de Docker (medido:
	# container ficou com stdin vazio e morreu no timeout de 30s do entrypoint). O jeito
	# que funciona de verdade é `docker run -i` em PRIMEIRO PLANO, destacado do processo
	# atual via `setsid` + `disown` (a mesma técnica que um `systemd --user` faria por
	# conta própria, se esta subida estivesse sob a unit em vez de manual).
	local run_cmd
	run_cmd=$(build_docker_run_command)
	: >"$RUNNER_CLIENT_LOG"
	setsid bash -c "printf '%s\n' \"\$GLINTFX_RUNNER_TOKEN\" | $run_cmd" \
		>"$RUNNER_CLIENT_LOG" 2>&1 </dev/null &
	disown
}

wait_for_container_running() {
	local tries=0
	while ! docker ps --format '{{.Names}}' | grep -qx "$RUNNER_CONTAINER_NAME"; do
		tries=$((tries + 1))
		if ((tries > 20)); then
			fail "container '$RUNNER_CONTAINER_NAME' não apareceu em 'docker ps' a tempo; veja $RUNNER_CLIENT_LOG"
		fi
		sleep 0.5
	done
}

report_started_container() {
	echo "runner_up.sh: container '$RUNNER_CONTAINER_NAME' iniciado a partir de '$RUNNER_IMAGE'."
	echo "runner_up.sh: log do cliente docker: $RUNNER_CLIENT_LOG"
	echo "runner_up.sh: confira o registro em: gh api repos/petrinhu/GlintFx/actions/runners"
}

main() {
	require_registration_token
	require_image_present
	fail_if_container_exists
	launch_runner_client
	wait_for_container_running
	report_started_container
}

main "$@"
