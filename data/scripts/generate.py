import struct
import socket
import random
import time

def create_variable_fast_pcap(filename, num_packets):
    # PCAP Global Header
    pcap_global_hdr = struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 65535, 1)
    
    # Static Network Headers & Addresses
    eth_hdr = struct.pack("!6s6sH", b'\x00\x11\x22\x33\x44\x55', b'\x66\x77\x88\x99\xaa\xbb', 0x0800)
    src_ip = socket.inet_aton("192.168.1.1")
    dst_ip = socket.inet_aton("192.168.1.2")
    src_port = 1234
    dst_port = 5678
    
    seq_num_size = 8 # 64-bit sequence number

    # --- SPEED OPTIMIZATION ---
    # The requested frame sizes are 64 to 1514 bytes.
    # Base headers = 14 (Eth) + 20 (IP) + 8 (UDP) = 42 bytes.
    # Base packet with sequence number = 42 + 8 = 50 bytes.
    # Therefore, we need between 14 and 1464 bytes of padding.
    
    # Pre-generate a pool of 1000 random padding lengths to cycle through
    # to avoid the massive CPU hit of calling random.randint() per packet.
    random_pad_lengths = [random.randint(14, 1464) for _ in range(1000)]
    
    # Create a maximum-sized block of zeros to slice from rapidly
    zero_padding = b'\x00' * 1500 

    print(f"Generating {num_packets:,} variable-length packets...")
    start_time = time.time()

    with open(filename, "wb") as f:
        f.write(pcap_global_hdr)
        
        for seq in range(num_packets):
            # Grab a semi-random padding length from our pre-computed pool
            pad_len = random_pad_lengths[seq % 1000]
            
            # Dynamically calculate IP and UDP lengths
            udp_len = 8 + seq_num_size + pad_len
            ip_len = 20 + udp_len
            full_packet_len = 14 + ip_len
            
            # Repack IP and UDP headers per packet with the new lengths
            ip_hdr = struct.pack("!BBHHHBBH4s4s", 0x45, 0, ip_len, 0, 0, 64, 17, 0, src_ip, dst_ip)
            udp_hdr = struct.pack("!HHHH", src_port, dst_port, udp_len, 0)
            
            # PCAP Packet Header: Ts_sec, Ts_usec, Incl_len, Orig_len
            pcap_pkt_hdr = struct.pack("<IIII", seq // 1000000, seq % 1000000, full_packet_len, full_packet_len)
            
            # Embed the 64-bit sequence number in big-endian format
            payload = struct.pack("!Q", seq)
            
            # Write standard header structure
            f.write(pcap_pkt_hdr)
            f.write(eth_hdr)
            f.write(ip_hdr)
            f.write(udp_hdr)
            
            # Write sequence number at absolute offset 42
            f.write(payload)
            
            # Append variable padding to the end of the packet
            f.write(zero_padding[:pad_len])

    print(f"Done in {time.time() - start_time:.2f} seconds.")

if __name__ == "__main__":
    # Generate 1,000,000 packets to test standard throughput
    create_variable_fast_pcap("variable_sequence_test.pcap", 1000000)