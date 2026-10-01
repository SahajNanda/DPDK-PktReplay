#!/usr/bin/env bash
set -euo pipefail

PKTGEN_DIR="/DPDK-PktReplay/Pktgen-DPDK"

cd "$PKTGEN_DIR"

PCAP_ARGS=()
if [[ $# -ge 1 ]]; then
	PCAP_ARGS+=( -s "0:../$1" )
fi
if [[ $# -ge 2 ]]; then
	PCAP_ARGS+=( -s "1:../$2" )
fi

./builddir/app/pktgen \
	-l 0-2 \
	-n 4 \
	--vdev=net_pcap0,iface=veth0 \
	--huge-dir=/DPDK-PktReplay/hugepages \
	--huge-unlink=always \
	-- \
	-T \
	-m [1:2].0 \
	"${PCAP_ARGS[@]}"
