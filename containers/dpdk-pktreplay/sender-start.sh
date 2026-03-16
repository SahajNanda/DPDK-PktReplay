#!/bin/bash

set -euo pipefail

# EAL options
LCORES="${LCORES:-0-4}"
MEM_CHANNELS="${MEM_CHANNELS:-4}"
IFACE="${IFACE:-veth1}"

# Pktgen options
PORT_MAP="${PORT_MAP:-[1:2].0}"
PCAP_PORT="${PCAP_PORT:-0}"
PCAP_FILE="${PCAP_FILE:-}"

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] [PCAP_FILE]

Start Pktgen on ${IFACE} in sender mode.

Arguments:
  PCAP_FILE             Optional path to a .pcap file to replay on port ${PCAP_PORT}.

Options:
  -h | --help           Show this help message.

Environment variables (override defaults):
  LCORES=${LCORES}
  MEM_CHANNELS=${MEM_CHANNELS}
  IFACE=${IFACE}
  PORT_MAP=${PORT_MAP}
  PCAP_PORT=${PCAP_PORT}

Examples:
  $(basename "$0")                          # send default synthetic traffic
  $(basename "$0") /data/traffic.pcap       # replay a pcap file
  IFACE=veth3 $(basename "$0")             # use a different interface
EOF
}

# Parse arguments
for arg in "$@"; do
    case "${arg}" in
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Error: unknown option '${arg}'." >&2
            echo >&2
            usage >&2
            exit 1
            ;;
        *)
            PCAP_FILE="${arg}"
            ;;
    esac
done

# Build optional pcap stream flag
PCAP_OPT=""
if [[ -n "${PCAP_FILE}" ]]; then
    if [[ ! -f "${PCAP_FILE}" ]]; then
        echo "Error: PCAP file not found: ${PCAP_FILE}" >&2
        exit 1
    fi
    PCAP_OPT="-s ${PCAP_PORT}:${PCAP_FILE}"
fi

echo "Starting Pktgen sender..."
echo "  LCORES:      ${LCORES}"
echo "  MEM_CHANNELS:${MEM_CHANNELS}"
echo "  IFACE:       ${IFACE}"
echo "  PORT_MAP:    ${PORT_MAP}"
[[ -n "${PCAP_FILE}" ]] && echo "  PCAP_FILE:   ${PCAP_FILE} (port ${PCAP_PORT})"

exec ./pktgen \
    -l "${LCORES}" \
    -n "${MEM_CHANNELS}" \
    --vdev="net_pcap0,iface=${IFACE}" \
    -- \
    -m "${PORT_MAP}" \
    ${PCAP_OPT}