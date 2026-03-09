# DPDK-PktReplay

A project built on DPDK + Pktgen to enable continuous packet transmission of large PCAP files. This project aims to bypass the memory constraints of Pktgen by continously feeding new data into memory as Pktgen loops through the memory to send packets, allowing a large packet capture to be replayed while maintaining Pktgens speed and performance.

## Structure Overview

### [DPDK](/dpdk-25.11/) - Data Plane Development Kit: A Linux Foundation project that consists of libraries to accelerate packet processing workloads running on a wide variety of CPU architectures.

### [Pktgen](/Pktgen-DPDK/) - DPDK Traffic Generator: high‑performance, scriptable packet generator capable of wire‑rate transmission with 64‑byte frames

### [containers](/containers/) - Server/Client Docker Containers for testing PktReplay functionality

## dpdk-25.11
- Compiled and installed first

## Pktgen-DPDK
- Compiled against DPDK headers/libs, and links to DPDK libraries during runtime to send packets

## containers
- Docker containers testing server and client functionality