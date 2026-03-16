# data
- `capinfos` and `pcapfix` used to check health of each pcap file and repair if corrupted
- `mergecap` used to combine all pcap files into one large (52GB) pcap file (combined.pcap)
- `editcap -s 1514 -c [n] -F pcap combined.pcap chunk.pcap` used to get pcapng chunk
    - n = 360000 ~= 128mb
    - `-s 1514` truncates packet lengths to 1514 bytes (1500 byte length + 14 byte eth header)
    - `-F pcap` used to output as pcap file instead of pcapng