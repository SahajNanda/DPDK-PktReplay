# DPDK-PktReplay

A project built on DPDK + Pktgen to enable continuous packet transmission of large PCAP files. This project aims to bypass the memory constraints of Pktgen by using two memory pools that alternate transmitting and reloading packets, allowing a large packet capture to be replayed while maintaining Pktgens performance.

## Environment Info

- This project was built/tested in Ubuntu 24.04

## Quick Start

### In the project root
- `./setup.sh [HUGEPAGES COUNT]` sets up hugepages
- `./build.sh` builds the DPDK Docker image
- `./run.sh [s|sender]` builds and starts the PktReplay docker container
- `./run.sh [r|receiver]` builds and starts a test receiver container
- `./hp-check` checks hugepage allocation/usage
### In the sender container
- `./scripts/veth-setup.sh` sets up a test veth connection
- `./scripts/build.sh` builds Pktgen
- `./scripts/run.sh path/to/file.pcap` runs Pktgen with PCAP file input
- `./scripts/run.sh path/to/folder` runs Pktgen with directory of PCAP files
### In the receiver container
- `./scripts/tshark-run.sh` to capture packets (after veth setup in sender container)
### In the Pktgen process
- `enable 0 pcap` enables pcap transmission on port 0
- `set 0 count [PKT COUNT]` limits total packets transmitted (if needed)
- `set 0 rate [RATE]` limits transmission rate (if needed)
- `start 0` begins transmission
- `stop 0` ends transmission
- `pcap show` shows PCAP info

## Direct Installation (with DPDK/hugepages already installed and configured)

### Inside the Pktgen-DPDK folder
- `meson setup --wipe builddir -Denable_lua=true`
- `meson compile -C builddir`
- `meson install -C builddir`
### Start PktReplay
- `sudo pktgen -l 0,2 -n 4 -a 41:00.1 --huge-unlink=always -- -T -P -m 2.0 -s [file|directory]`
