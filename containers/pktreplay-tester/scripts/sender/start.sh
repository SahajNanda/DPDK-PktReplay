#!/usr/bin/env bash
set -euo pipefail

PKTGEN_DIR="/DPDK-PktReplay/Pktgen-DPDK"

cd "$PKTGEN_DIR"

PCAP_ARGS=()
if [[ $# -ge 1 ]]; then
	PCAP_ARGS+=( -s "0:../$1" )
fi

./builddir/app/pktgen \
	-l 0,2 \
	-n 1 \
	--vdev=net_pcap0,iface=veth0 \
	--huge-unlink=always \
	-- \
	-T \
	-m 2.0 \
	"${PCAP_ARGS[@]}"
