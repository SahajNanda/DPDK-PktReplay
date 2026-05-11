# data

Data not included in repo due to large size, but recreation steps are included below:

## Data Creation
- Pcap data downloaded from [UNB CSE-CIC-IDS2018 on AWS](https://www.unb.ca/cic/datasets/ids-2018.html)
    - [Download link](https://registry.opendata.aws/cse-cic-ids2018/)
- `capinfos` and `pcapfix` used to check health of each pcap file and repair if corrupted
- `mergecap` used to combine all pcap files into one large (52GB) pcap file `combined.pcap`
- `editcap -F pcap -s 1514 -c [n] combined.pcap chunk.pcap` used to get pcap chunk
    - `-F pcap` used to output as pcap file instead of pcap
    - `-s 1514` truncates packet lengths to 1514 bytes (1500 byte length + 14 byte eth header)
    - `-c [n]` is the number of packets saved to each chunk (360,000 ~= 128mb)
- For pcap chunk with packets greater than 1514 bytes removed rather than truncated, use `tshark -r combined.pcap -Y "frame.len <= 1514" -c 1000000 -w filter.pcap`
    - `-r combined.pcap` select the pcap file to take a chunk of
    - `-Y "frame.len <= 1514"` limits packet length to 1514 bytes
    - `-c 1000000` sets number of packets read from input pcap file
    - `-w filter.pcap` sets the output pcap file name

dumpcap -i veth1 -s 64 -B 512 -w /dev/shm/rem2.pcapng
tshark -r /dev/shm/rem2.pcapng > captures/rem2.txt

## Data Scripts

This repository also includes helper scripts in `data/scripts` for generating and post-processing packet data used in replay testing.

### 1) `generate_tester_pcap.py`

Generates a valid Ethernet/IPv4 PCAP file for replay validation.

Sequence can be exposed in `tshark` Info either as ICMP echo sequence (plainly visible), or encoded in UDP ports.

Each packet payload can also include an ASCII marker:

`DPDK_TESTER SEQ=<sequence> LEN=<frame_len>`

#### Usage

```bash
python3 data/scripts/generate_tester_pcap.py -o data/tester_generated.pcap -n 50000
```

#### Common options

- `-n, --count`: number of packets to generate.
- `--min-len`: minimum frame length (default `64`).
- `--max-len`: maximum frame length (default `1514`).
- `--length-mode`: `random` (default) or `cycle`.
- `--seed`: set random seed for deterministic runs.
- `--info-mode`: `icmp-seq` (default), `udp-ports`, or `payload`.

#### Info column behavior

- `icmp-seq`: `tshark` Info shows `Echo (ping) request ... seq=<n>` directly.
- `udp-ports`: `tshark` Info shows ports; sequence is encoded as:

    `((udp.srcport-20000) << 8) | (udp.dstport-30000)`
- `payload`: sequence remains only in payload marker text.

#### Example (deterministic range walk)

```bash
python3 data/scripts/generate_tester_pcap.py \
    -o data/tester_cycle.pcap \
    -n 2000 \
    --min-len 64 \
    --max-len 1514 \
    --length-mode cycle \
    --info-mode icmp-seq
```

#### Example `tshark` checks

```bash
tshark -r data/tester_generated.pcap -c 5
tshark -r data/tester_generated.pcap -Y icmp -T fields -e frame.number -e frame.len -e icmp.seq
tshark -r data/tester_generated.pcap -Y udp -T fields -e frame.number -e udp.srcport -e udp.dstport
```

### 2) `strip_second_column_time.sh`

Removes the second whitespace-delimited column from each line in tshark captures for diff checking.

`<packet_no> <time> <details...>` becomes `<packet_no> <details...>`

#### Usage from repository root

```bash
./data/scripts/strip_second_column_time.sh data/captures/log5.txt data/captures/log5.no-time.txt
```

#### In-place edit

```bash
./data/scripts/strip_second_column_time.sh -i data/captures/log5.txt
```

#### Notes

- Keeps column 1 and the rest of the line.
- Removes only column 2.
- Preserves original spacing after the removed column as much as possible.



