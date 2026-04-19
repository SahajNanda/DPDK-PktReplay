# DPDK-PktReplay

A project built on DPDK + Pktgen to enable continuous packet transmission of large PCAP files. This project aims to bypass the memory constraints of Pktgen by using two memory pools that alternate transmitting and reloading packets, allowing a large packet capture to be replayed while maintaining Pktgens performance.

## Environment Info

- This project is built in Ubuntu 24.04

## Quick Start

### In the project root
- `./setup.sh [HUGEPAGES COUNT]` sets up hugepages
- `./build.sh` builds the DPDK Docker image
- `./run.sh [s|sender]` builds the Pktgen docker container
- `./run.sh [r|receiver]` builds a test receiver container
    - In this container, run `./scripts/tshark-run.sh` to capture packets (after veth setup in sender container)
- `./hp-check` checks hugepage allocation/usage
### In the sender container
- `./scripts/veth-setup.sh` sets up a test veth connection
- `./scripts/build.sh` builds Pktgen
- `./scripts/run.sh path/to/file.pcap` runs Pktgen
### In the Pktgen process
- `enable 0 pcap` enables pcap transmission on port 0
- `set 0 count [PKT COUNT]` limits total packets transmitted
- `start 0` begins transmission
