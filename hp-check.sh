#!/bin/bash

echo "Kernel hugepage config (required):"
zcat /proc/config.gz | grep -E --color=always 'CONFIG_HUGETLBFS=y|CONFIG_HUGETLB_PAGE=y'

echo ""

echo "Current hugepage memory info:"
grep -i huge /proc/meminfo