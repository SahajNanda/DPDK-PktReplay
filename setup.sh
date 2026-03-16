#!/bin/bash

set -euo pipefail

HUGEPAGE_PATH="/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages"
TARGET_HUGEPAGES="1024"

# Allocate HugePages only if none are currently allocated
CURRENT_HUGEPAGES="$(cat "${HUGEPAGE_PATH}")"
if [[ "${CURRENT_HUGEPAGES}" -gt 0 ]]; then
    echo "HugePages already allocated (${CURRENT_HUGEPAGES}); skipping allocation."
else
    echo "No HugePages allocated. Attempting to allocate ${TARGET_HUGEPAGES} (2GB total)..."
    echo "${TARGET_HUGEPAGES}" | sudo tee "${HUGEPAGE_PATH}" > /dev/null
fi

# Mount HugePage Filesystem
if ! mount | grep -q /dev/hugepages; then
    echo "Mounting hugetlbfs..."
    sudo mkdir -p /dev/hugepages
    sudo mount -t hugetlbfs nodev /dev/hugepages
else
    echo "hugetlbfs already mounted."
fi

# Final Status Check
echo "---------------------------------------"
echo "$(grep -i huge /proc/meminfo)"
echo "Lab is ready for Docker."