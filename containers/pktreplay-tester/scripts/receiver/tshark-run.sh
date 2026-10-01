#!/usr/bin/env bash
set -euo pipefail

tshark -i veth1 -n -s 128 -f "ip or ip6 or arp" -F pcap