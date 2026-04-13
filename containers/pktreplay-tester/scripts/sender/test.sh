#!/usr/bin/env bash
set -euo pipefail

PKTGEN_DIR="/DPDK-PktReplay/Pktgen-DPDK"

cd "$PKTGEN_DIR"

./builddir/app/pktgen \
	-l 0-2 \
	-n 4 \
	--vdev=net_pcap0,iface=veth0 \
	-- \
	-Tv \
	-m [1:2].0 \
	-s 0:pcap/test5.pcap\
