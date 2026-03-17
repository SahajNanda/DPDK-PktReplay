# Base Image
FROM ubuntu:24.04

# Prevent interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Install Build Tools and DPDK Dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    meson \
    ninja-build \
    python3-pyelftools \
    libnuma-dev \
    libpcap-dev \
    libbsd-dev \
    pkg-config \
    git \
    iproute2 \
    pciutils \
    && rm -rf /var/lib/apt/lists/*

# Set compiler path for DPDK (for Pktgen)
ENV PKG_CONFIG_PATH=/usr/local/lib/x86_64-linux-gnu/pkgconfig

# Copy DPDK Repository
COPY dpdk-25.11 /DPDK-PktReplay/dpdk-25.11

# Change working directory to DPDK
WORKDIR /DPDK-PktReplay/dpdk-25.11

# Set build type (can be changed to debug if needed)
ARG BUILD_TYPE=release

# Build and Install DPDK
# Remove examples later?
RUN rm -rf build \
    && meson setup \
    -Dplatform=generic \
    -Dexamples=l2fwd,l3fwd,helloworld,skeleton \
    -Dbuildtype=${BUILD_TYPE} \
    build \
    && ninja -C build \
    && meson install -C build \
    && ldconfig

# Environment variables for Pktgen to find DPDK
ENV RTE_SDK=/DPDK-PktReplay/dpdk-25.11
ENV RTE_TARGET=build

# Copy Pktgen Repository
COPY Pktgen-DPDK /DPDK-PktReplay/Pktgen-DPDK

# Change working directory to Pktgen
WORKDIR /DPDK-PktReplay/Pktgen-DPDK

# Build and Install Pktgen
RUN meson setup builddir \
    && meson compile -C builddir \
    && meson install -C builddir \
    && ldconfig

# Copy Startup Scripts
COPY containers/dpdk-pktreplay/scripts /DPDK-PktReplay/scripts
COPY containers/dpdk-pktreplay/start.sh /DPDK-PktReplay/start.sh
RUN chmod +x /DPDK-PktReplay/scripts/*
RUN chmod +x /DPDK-PktReplay/start.sh

# Copy Data
COPY data /DPDK-PktReplay/data

# Change working directory to Pktgen for running tests
WORKDIR /DPDK-PktReplay