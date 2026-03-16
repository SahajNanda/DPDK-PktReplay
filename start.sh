#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"

IMAGE_NAME="dpdk-pktreplay"
DOCKERFILE_PATH="containers/dpdk-pktreplay/Dockerfile"

usage() {
	cat <<'EOF'
Usage: ./start.sh [MODE]

Modes:
	s | sender      Start the sender container
	r | receiver    Start the receiver container
	d | debug       Start a debug container shell
	h | help        Show this help message

No arguments:
	Rebuilds the image, then shows this help message.
EOF
}

rebuild_image() {
	local build_type="$1"

	echo "Rebuilding Docker image (${build_type})..."

	if [[ "${build_type}" == "debug" ]]; then
		docker build \
			--build-arg BUILD_TYPE=debug \
			-t "${IMAGE_NAME}" \
			-f "${DOCKERFILE_PATH}" \
			"${REPO_ROOT}"
	else
		docker build \
			-t "${IMAGE_NAME}" \
			-f "${DOCKERFILE_PATH}" \
			"${REPO_ROOT}"
	fi
}

remove_if_exists() {
	local container_name="$1"
	if docker ps -a --format '{{.Names}}' | grep -Fxq "${container_name}"; then
		echo "Removing existing container '${container_name}'..."
		docker rm -f "${container_name}" > /dev/null
	fi
}

run_container() {
	local container_name="$1"

	remove_if_exists "${container_name}"

	docker run -it --privileged \
		--name "${container_name}" \
		--network host \
		-v /dev/hugepages:/dev/hugepages \
		-v /lib/modules:/lib/modules \
		"${IMAGE_NAME}" /bin/bash
}

MODE="${1:-}"

case "${MODE}" in
	"")
		rebuild_image "release"
		echo
		usage
		;;
	s|sender|-s|--sender)
		run_container "sender"
		;;
	r|receiver|-r|--receiver)
		run_container "receiver"
		;;
	d|debug|-d|--debug)
		run_container "debug"
		;;
	h|help|-h|--help)
		usage
		;;
	*)
		echo "Error: unknown mode '${MODE}'."
		echo
		usage
		exit 1
		;;
esac
