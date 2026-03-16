# data

Data not included in repo due to large size, but recreation steps are included below:

## Data Creation
- Pcap data downloaded from [UNB CSE-CIC-IDS2018 on AWS](https://www.unb.ca/cic/datasets/ids-2018.html)
    - [Download link](https://registry.opendata.aws/cse-cic-ids2018/)
- `capinfos` and `pcapfix` used to check health of each pcap file and repair if corrupted
- `mergecap` used to combine all pcap files into one large (52GB) pcap file (combined.pcap)
- `editcap -F pcap -s 1514 -c [n] combined.pcap chunk.pcap` used to get pcapng chunk
    - `-F pcap` used to output as pcap file instead of pcapng
    - `-s 1514` truncates packet lengths to 1514 bytes (1500 byte length + 14 byte eth header)
    - `-c [n]` is the number of packets saved to each chunk (360,000 ~= 128mb)


