#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"

IMAGE_NAME="dpdk-pktreplay"
DOCKERFILE_PATH="containers/dpdk-pktreplay/Dockerfile"
HUGEPAGE_PATH="/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages"
DEFAULT_HUGEPAGES="1024"

MODE=""
TARGET_HUGEPAGES="${DEFAULT_HUGEPAGES}"
HUGEPAGES_EXPLICIT="false"

usage() {
	cat <<'EOF'
Usage: ./start.sh [OPTIONS] [MODE]

Options:
	-p | --hugepages COUNT    Set hugepages target to COUNT (Default: 1024)
	-h | --help               Show this help message

Modes:
	a | allocate    Set hugepages to COUNT, overwriting any existing value, then exit
	s | sender      Start the sender container
	r | receiver    Start the receiver container
	d | debug       Start a debug container shell

No arguments:
	Rebuilds the image, then shows this help message.

HugePages are checked automatically before starting a container.
EOF
}

prepare_hugepages() {
	local force_overwrite="${1:-false}"
	local current_hugepages total_megabytes

	if [[ ! -f "${HUGEPAGE_PATH}" ]]; then
		echo "Error: HugePage configuration path '${HUGEPAGE_PATH}' does not exist." >&2
		exit 1
	fi

	current_hugepages="$(<"${HUGEPAGE_PATH}")"
	if [[ "${force_overwrite}" == "true" ]]; then
		total_megabytes="$(( TARGET_HUGEPAGES * 2 ))"
		if [[ "${current_hugepages}" -eq "${TARGET_HUGEPAGES}" ]]; then
			echo "HugePages already set to ${TARGET_HUGEPAGES} (${total_megabytes}MB total)."
		else
			echo "Updating HugePages from ${current_hugepages} to ${TARGET_HUGEPAGES} (${total_megabytes}MB total)..."
			echo "${TARGET_HUGEPAGES}" | sudo tee "${HUGEPAGE_PATH}" > /dev/null
		fi
	elif [[ "${current_hugepages}" -gt 0 ]]; then
		echo "HugePages already allocated (${current_hugepages}); skipping allocation."
	else
		total_megabytes="$(( TARGET_HUGEPAGES * 2 ))"
		echo "No HugePages allocated. Attempting to allocate ${TARGET_HUGEPAGES} (${total_megabytes}MB total)..."
		echo "${TARGET_HUGEPAGES}" | sudo tee "${HUGEPAGE_PATH}" > /dev/null
	fi

	if ! mount | grep -q ' on /dev/hugepages '; then
		echo "Mounting hugetlbfs..."
		sudo mkdir -p /dev/hugepages
		sudo mount -t hugetlbfs nodev /dev/hugepages
	fi
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

	docker run -it --rm --privileged \
		--name "${container_name}" \
		--network host \
		-v /dev/hugepages:/dev/hugepages \
		-v /lib/modules:/lib/modules \
		"${IMAGE_NAME}" /bin/bash
}
parse_args() {
	while [[ $# -gt 0 ]]; do
		case "$1" in
			-p|--hugepages)
				if [[ $# -lt 2 ]]; then
					echo "Error: '$1' requires a positive integer value." >&2
					exit 1
				fi
				TARGET_HUGEPAGES="$2"
				HUGEPAGES_EXPLICIT="true"
				shift 2
				;;
			--hugepages=*)
				TARGET_HUGEPAGES="${1#*=}"
				HUGEPAGES_EXPLICIT="true"
				shift
				;;
			a|allocate|-a|--allocate|s|sender|-s|--sender|r|receiver|-r|--receiver|d|debug|-d|--debug)
				if [[ -n "${MODE}" ]]; then
					echo "Error: multiple modes provided ('${MODE}' and '$1')." >&2
					exit 1
				fi
				MODE="$1"
				shift
				;;
			h|help|-h|--help)
				MODE="help"
				shift
				;;
			*)
				echo "Error: unknown argument '$1'." >&2
				echo
				usage
				exit 1
				;;
		esac
	done

	if [[ ! "${TARGET_HUGEPAGES}" =~ ^[1-9][0-9]*$ ]]; then
		echo "Error: hugepage count must be a positive integer, got '${TARGET_HUGEPAGES}'." >&2
		exit 1
	fi
}

parse_args "$@"

case "${MODE}" in
	"")
		prepare_hugepages "${HUGEPAGES_EXPLICIT}"
		rebuild_image "release"
		echo
		usage
		;;
	a|allocate|-a|--allocate)
		prepare_hugepages true
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
	help)
		usage
		;;
	*)
		echo "Error: unknown mode '${MODE}'."
		echo
		usage
		exit 1
		;;
esac
