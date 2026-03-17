#!/bin/bash

set -euo pipefail

SCRIPT_ROOT="/DPDK-PktReplay"
SCRIPTS_DIR="${SCRIPT_ROOT}/scripts"

NETWORK_SETUP_SCRIPT="${SCRIPTS_DIR}/network-setup.sh"
SENDER_SCRIPT="${SCRIPTS_DIR}/sender-start.sh"
RECEIVER_SCRIPT="${SCRIPTS_DIR}/receiver-start.sh"

usage() {
	cat <<'EOF'
Usage: ./start.sh [MODE] [ARGS...]

Modes:
  s | sender      Run sender-start.sh
  r | receiver    Run receiver-start.sh
  h | help        Show this help message

No arguments:
	Run network-setup.sh, then show this help message

Examples:
  ./start.sh
  ./start.sh s /DPDK-PktReplay/data/example.pcap
  ./start.sh r
EOF
}

for required_script in "${NETWORK_SETUP_SCRIPT}" "${SENDER_SCRIPT}" "${RECEIVER_SCRIPT}"; do
	if [[ ! -x "${required_script}" ]]; then
		echo "Error: required script not found or not executable: ${required_script}" >&2
		exit 1
	fi
done

MODE="${1:-}"

case "${MODE}" in
	"")
		"${NETWORK_SETUP_SCRIPT}"
		usage
		;;
	s|sender)
		shift
		"${NETWORK_SETUP_SCRIPT}"
		exec "${SENDER_SCRIPT}" "$@"
		;;
	r|receiver)
		shift
		"${NETWORK_SETUP_SCRIPT}"
		exec "${RECEIVER_SCRIPT}" "$@"
		;;
	h|help|-h|--help)
		usage
		;;
	*)
		echo "Error: unknown mode '${MODE}'." >&2
		echo >&2
		usage >&2
		exit 1
		;;
esac
