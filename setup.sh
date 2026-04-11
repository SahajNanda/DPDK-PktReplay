#!/bin/bash
# Allocate hugepages for DPDK

NR_PAGES=${1:-256}

echo "Allocating $NR_PAGES x 2MB hugepages..."
echo $NR_PAGES | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

sudo mkdir -p /dev/hugepages
sudo mount -t hugetlbfs nodev /dev/hugepages

echo "Done. Allocated $(cat /proc/sys/vm/nr_hugepages) hugepages"
