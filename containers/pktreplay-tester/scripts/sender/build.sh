#!/usr/bin/env bash
set -euo pipefail

PKTGEN_DIR="/DPDK-PktReplay/Pktgen-DPDK"

cd "$PKTGEN_DIR"

meson setup --wipe builddir -Denable_lua=true \
    && meson compile -C builddir \
    && meson install -C builddir \
    && ldconfig