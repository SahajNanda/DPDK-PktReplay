#!/usr/bin/env python3
"""Generate a valid PCAP file for packet replay testing.

Supports sequence visibility in tshark's Info column via:
- ICMP Echo sequence numbers (recommended)
- UDP source/destination ports derived from sequence
"""

import argparse
import os
import random
import struct
import time


PCAP_GLOBAL_HEADER = struct.pack(
    "<IHHIIII",
    0xA1B2C3D4,  # magic
    2,           # version major
    4,           # version minor
    0,           # thiszone
    0,           # sigfigs
    65535,       # snaplen
    1,           # network: LINKTYPE_ETHERNET
)

ETH_HEADER_LEN = 14
IP_HEADER_LEN = 20
UDP_HEADER_LEN = 8
ICMP_HEADER_LEN = 8
MIN_FRAME_LEN = 64
MAX_FRAME_LEN = 1514


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate a PCAP with configurable packet count and frame lengths "
            "between 64 and 1514 bytes."
        )
    )
    parser.add_argument(
        "-o",
        "--output",
        default="tester_generated.pcap",
        help="Output PCAP path (default: tester_generated.pcap)",
    )
    parser.add_argument(
        "-n",
        "--count",
        type=int,
        default=1000,
        help="Number of packets to generate (default: 1000)",
    )
    parser.add_argument(
        "--min-len",
        type=int,
        default=MIN_FRAME_LEN,
        help="Minimum frame length in bytes, inclusive (default: 64)",
    )
    parser.add_argument(
        "--max-len",
        type=int,
        default=MAX_FRAME_LEN,
        help="Maximum frame length in bytes, inclusive (default: 1514)",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=None,
        help="Optional random seed for reproducible lengths",
    )
    parser.add_argument(
        "--length-mode",
        choices=["random", "cycle"],
        default="random",
        help=(
            "Length selection mode: random picks uniformly in range; cycle "
            "walks min->max repeatedly (default: random)."
        ),
    )
    parser.add_argument(
        "--info-mode",
        choices=["icmp-seq", "udp-ports", "payload"],
        default="icmp-seq",
        help=(
            "How to expose sequence in tshark: icmp-seq shows ICMP sequence in "
            "Info column, udp-ports encodes sequence in UDP ports, payload keeps "
            "sequence text only in payload."
        ),
    )
    return parser.parse_args()


def checksum16(data: bytes) -> int:
    if len(data) % 2:
        data += b"\x00"
    total = 0
    for i in range(0, len(data), 2):
        total += (data[i] << 8) + data[i + 1]
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def build_eth_header() -> bytes:
    dst_mac = bytes.fromhex("020000000001")
    src_mac = bytes.fromhex("020000000002")
    ether_type_ipv4 = b"\x08\x00"
    return dst_mac + src_mac + ether_type_ipv4


def build_ipv4_header(total_ip_len: int, identification: int, protocol: int) -> bytes:
    version_ihl = 0x45
    dscp_ecn = 0
    flags_fragment = 0x4000  # don't fragment
    ttl = 64
    src_ip = bytes([198, 18, 0, 1])
    dst_ip = bytes([198, 18, 0, 2])

    header_wo_checksum = struct.pack(
        "!BBHHHBBH4s4s",
        version_ihl,
        dscp_ecn,
        total_ip_len,
        identification & 0xFFFF,
        flags_fragment,
        ttl,
        protocol,
        0,
        src_ip,
        dst_ip,
    )
    ip_checksum = checksum16(header_wo_checksum)
    return struct.pack(
        "!BBHHHBBH4s4s",
        version_ihl,
        dscp_ecn,
        total_ip_len,
        identification & 0xFFFF,
        flags_fragment,
        ttl,
        protocol,
        ip_checksum,
        src_ip,
        dst_ip,
    )


def build_udp_header(payload: bytes, src_ip: bytes, dst_ip: bytes, src_port: int, dst_port: int) -> bytes:
    udp_len = UDP_HEADER_LEN + len(payload)
    header_wo_checksum = struct.pack("!HHHH", src_port, dst_port, udp_len, 0)
    pseudo_header = struct.pack("!4s4sBBH", src_ip, dst_ip, 0, 17, udp_len)
    udp_checksum = checksum16(pseudo_header + header_wo_checksum + payload)
    if udp_checksum == 0:
        udp_checksum = 0xFFFF
    return struct.pack("!HHHH", src_port, dst_port, udp_len, udp_checksum)


def build_icmp_echo_header(payload: bytes, identifier: int, sequence: int) -> bytes:
    header_wo_checksum = struct.pack("!BBHHH", 8, 0, 0, identifier & 0xFFFF, sequence & 0xFFFF)
    icmp_checksum = checksum16(header_wo_checksum + payload)
    return struct.pack("!BBHHH", 8, 0, icmp_checksum, identifier & 0xFFFF, sequence & 0xFFFF)


def build_packet(seq: int, frame_len: int, info_mode: str) -> bytes:
    if frame_len < MIN_FRAME_LEN or frame_len > MAX_FRAME_LEN:
        raise ValueError(f"Frame length {frame_len} out of valid range {MIN_FRAME_LEN}-{MAX_FRAME_LEN}")

    if info_mode in ("payload", "udp-ports"):
        l4_header_len = UDP_HEADER_LEN
    elif info_mode == "icmp-seq":
        l4_header_len = ICMP_HEADER_LEN
    else:
        raise ValueError(f"Unsupported --info-mode: {info_mode}")

    l2_l3_l4_len = ETH_HEADER_LEN + IP_HEADER_LEN + l4_header_len
    payload_len = frame_len - l2_l3_l4_len
    if payload_len < 0:
        raise ValueError("Requested frame length too small for headers")

    payload_prefix = f"DPDK_TESTER SEQ={seq:08d} LEN={frame_len:04d} ".encode("ascii")
    if len(payload_prefix) >= payload_len:
        payload = payload_prefix[:payload_len]
    else:
        payload = payload_prefix + b"X" * (payload_len - len(payload_prefix))

    src_ip = bytes([198, 18, 0, 1])
    dst_ip = bytes([198, 18, 0, 2])

    if info_mode == "icmp-seq":
        l4_header = build_icmp_echo_header(payload, identifier=0xD00D, sequence=seq)
        ip_protocol = 1
    elif info_mode == "udp-ports":
        src_port = 20000 + ((seq >> 8) & 0xFF)
        dst_port = 30000 + (seq & 0xFF)
        l4_header = build_udp_header(payload, src_ip, dst_ip, src_port, dst_port)
        ip_protocol = 17
    else:
        l4_header = build_udp_header(payload, src_ip, dst_ip, 12345, 23456)
        ip_protocol = 17

    ip_total_len = IP_HEADER_LEN + len(l4_header) + len(payload)

    eth_header = build_eth_header()
    ip_header = build_ipv4_header(ip_total_len, identification=seq, protocol=ip_protocol)

    packet = eth_header + ip_header + l4_header + payload
    if len(packet) != frame_len:
        raise RuntimeError(f"Internal error: packet length {len(packet)} != requested {frame_len}")
    return packet


def length_for_packet(index: int, args: argparse.Namespace) -> int:
    if args.length_mode == "cycle":
        span = args.max_len - args.min_len + 1
        return args.min_len + (index % span)
    return random.randint(args.min_len, args.max_len)


def validate_args(args: argparse.Namespace) -> None:
    if args.count <= 0:
        raise ValueError("--count must be greater than 0")
    if args.min_len < MIN_FRAME_LEN:
        raise ValueError(f"--min-len must be >= {MIN_FRAME_LEN}")
    if args.max_len > MAX_FRAME_LEN:
        raise ValueError(f"--max-len must be <= {MAX_FRAME_LEN}")
    if args.min_len > args.max_len:
        raise ValueError("--min-len must be <= --max-len")


def write_pcap(output_path: str, args: argparse.Namespace) -> None:
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)

    ts_sec = int(time.time())
    ts_usec = 0

    with open(output_path, "wb") as f:
        f.write(PCAP_GLOBAL_HEADER)

        for i in range(args.count):
            seq = i + 1
            frame_len = length_for_packet(i, args)
            packet = build_packet(seq, frame_len, args.info_mode)

            packet_header = struct.pack(
                "<IIII",
                ts_sec,
                ts_usec,
                len(packet),
                len(packet),
            )
            f.write(packet_header)
            f.write(packet)

            ts_usec += 100
            if ts_usec >= 1_000_000:
                ts_sec += 1
                ts_usec -= 1_000_000


def main() -> int:
    args = parse_args()
    try:
        validate_args(args)
    except ValueError as exc:
        print(f"Error: {exc}")
        return 2

    if args.seed is not None:
        random.seed(args.seed)

    write_pcap(args.output, args)

    print(
        "Generated {count} packets in {path} with length mode={mode}, range={min_len}-{max_len}, info-mode={info_mode}".format(
            count=args.count,
            path=args.output,
            mode=args.length_mode,
            min_len=args.min_len,
            max_len=args.max_len,
            info_mode=args.info_mode,
        )
    )
    if args.info_mode == "icmp-seq":
        print("tshark Info will include ICMP echo seq=<sequence>.")
    elif args.info_mode == "udp-ports":
        print("tshark Info will include sequence encoded in UDP ports.")
        print("Decode seq as: ((udp.srcport-20000) << 8) | (udp.dstport-30000)")
    else:
        print("Payload marker format: DPDK_TESTER SEQ=<sequence> LEN=<frame_len>")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
