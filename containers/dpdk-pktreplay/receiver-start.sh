#!/bin/bash

set -euo pipefail

IFACE="${IFACE:-veth0}"
LCORES="${LCORES:-0-2}"
MEM_CHANNELS="${MEM_CHANNELS:-4}"
STATS_PERIOD="${STATS_PERIOD:-1}"

if ! ip link show "${IFACE}" > /dev/null 2>&1; then
	echo "Error: interface ${IFACE} not found. Run network-setup.sh first." >&2
	exit 1
fi

if command -v dpdk-testpmd > /dev/null 2>&1; then
	TESTPMD_BIN="dpdk-testpmd"
elif [[ -x /opt/dpdk/build/app/dpdk-testpmd ]]; then
	TESTPMD_BIN="/opt/dpdk/build/app/dpdk-testpmd"
else
	echo "Error: dpdk-testpmd binary not found in PATH or /opt/dpdk/build/app." >&2
	exit 1
fi

echo "Starting receiver on ${IFACE} with ${TESTPMD_BIN}..."
echo "Press Ctrl+C to stop."

exec "${TESTPMD_BIN}" \
	-l "${LCORES}" \
	-n "${MEM_CHANNELS}" \
	--vdev="net_pcap0,iface=${IFACE}" \
	-- \
	--forward-mode=rxonly \
	--auto-start \
	--stats-period="${STATS_PERIOD}"