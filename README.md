# DPDK-PktReplay

A project built on DPDK + Pktgen to enable continuous packet transmission of large PCAP files. This project aims to bypass the memory constraints of Pktgen by continously feeding new data into memory as Pktgen loops through the memory to send packets, allowing a large packet capture to be replayed while maintaining Pktgens speed and performance.

## Quick Start
- In WSL in this folder, run `./start.sh [-p|--hugepages COUNT]` to build image and allocate hugepages (each page is 2048 kB, default 1024 pages)
- Run `./start.sh [-p|--hugepages COUNT] [s|sender|r|receiver|d|debug|a|allocate]` to run container or change hugepages allocation
- Once container is open, run `./start.sh [s|sender|r|receiver]`

## Structure Overview

### [DPDK](/dpdk-25.11/) - Data Plane Development Kit: A Linux Foundation project that consists of libraries to accelerate packet processing workloads running on a wide variety of CPU architectures.

### [Pktgen](/Pktgen-DPDK/) - DPDK Traffic Generator: high‑performance, scriptable packet generator capable of wire‑rate transmission with 64‑byte frames

### [containers](/containers/) - Server/Client Docker Containers for testing PktReplay functionality

## dpdk-25.11
- Compiled and installed first
- Requires hugepages to be enabled (should be enabled by default on most systems)

## Pktgen-DPDK
- Compiled against DPDK headers/libs, and links to DPDK libraries during runtime to send packets
- Requires hugepages to be allocated and mounted
    - Allocation can be done with `echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages`
        - Pktgen docs say to check if it's enabled with `grep -i huge /boot/config-X.X.XX-XX-generic` which is not available in wsl, can instead be checked with `zcat /proc/config.gz | grep HUGETLB`
            - Ensure `CONFIG_HUGETLBFS=y` and `CONFIG_HUGETLB_PAGE=y`
        - Hugepage amounts can be checked with `grep -i huge /proc/meminfo`
    - Mounting can be done with `sudo mkdir -p /dev/hugepages` followed by `sudo mount -t hugetlbfs nodev /dev/hugepages`


## containers
- Docker containers testing server and client functionality
- For testing purposes, a virtual ethernet (veth) pair is being used to connect the server and client
    - This can be done by running `ip link add veth0 type veth peer name veth1`, which creates a virtual veth cable with two ends, veth0 and veth1, with a peer connection which automatically sends data from one end to another
    - The two ends must be enabled with `ip link set veth0 up` and `ip link set veth1 up` 
    - This can be verified with `ip link show veth0` and `ip link show veth1`