#!/bin/bash

set -euo pipefail

echo "Configuring veth pair (veth0 <-> veth1)..."

if ip link show veth0 > /dev/null 2>&1; then
    echo "veth0 already exists, skipping create."
else
    ip link add veth0 type veth peer name veth1
fi

ip link set veth0 up
ip link set veth1 up

echo "veth0 status: $(ip link show veth0)"
echo "veth1 status: $(ip link show veth1)"
echo "veth setup complete."
