#!/bin/bash

set -euo pipefail

usage() {
    echo "Usage: $0 [d|r]"
    echo "  no arg  create or bring up the veth pair"
    echo "  d       delete the veth pair"
    echo "  r       reset the pair by deleting and recreating it"
}

veth_exists() {
    ip link show "$1" > /dev/null 2>&1
}

create_veth_pair() {
    echo "Configuring veth pair (veth0 <-> veth1)..."

    if veth_exists veth0; then
        echo "veth0 already exists, skipping create."
    else
        ip link add veth0 type veth peer name veth1
    fi

    ip link set veth0 up
    ip link set veth1 up

    echo "veth0 status: $(ip link show veth0)"
    echo "veth1 status: $(ip link show veth1)"
    echo "veth setup complete."
}

delete_veth_pair() {
    echo "Deleting veth pair (veth0 <-> veth1)..."

    if veth_exists veth0; then
        ip link delete veth0
    elif veth_exists veth1; then
        ip link delete veth1
    fi

    echo "veth pair deleted."
}

reset_veth_pair() {
    echo "Resetting veth pair (veth0 <-> veth1)..."

    if veth_exists veth0; then
        ip link delete veth0
    elif veth_exists veth1; then
        ip link delete veth1
    fi

    create_veth_pair
}

case "${1:-}" in
    "")
        create_veth_pair
        ;;
    d)
        delete_veth_pair
        ;;
    r)
        reset_veth_pair
        ;;
    -h|--help)
        usage
        ;;
    *)
        echo "Unknown argument: $1"
        usage
        exit 1
        ;;
esac