#!/bin/bash

# 1. Allocate HugePages (2GB)
echo "Allocating 2GB of HugePages..."
echo 1024 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# 2. Mount HugePage Filesystem
if ! mount | grep -q /dev/hugepages; then
    echo "Mounting hugetlbfs..."
    sudo mkdir -p /dev/hugepages
    sudo mount -t hugetlbfs nodev /dev/hugepages
else
    echo "hugetlbfs already mounted."
fi

# 3. Create the Virtual Ethernet Pipe (veth pair)
# Check if they exist first to avoid errors
if ! ip link show veth0 > /dev/null 2>&1; then
    echo "Creating veth pair (veth0 <-> veth1)..."
    sudo ip link add veth0 type veth peer name veth1
    sudo ip link set veth0 up
    sudo ip link set veth1 up
else
    echo "veth pair already exists."
fi

# 4. Final Status Check
echo "---------------------------------------"
echo "HugePages Total: $(cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages)"
echo "Veth Status: $(ip link show veth1 | grep -o "state [A-Z]*")"
echo "Lab is ready for Docker."