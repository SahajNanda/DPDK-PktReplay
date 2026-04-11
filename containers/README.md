# containers

## Instructions

### Build and Run Docker Containers
- To build (from top folder): `docker build -t dpdk-pktreplay -f containers/dpdk-pktreplay/Dockerfile .`
    - To build in debug mode, add docker `--build-arg BUILD_TYPE=debug`
        - Ex: `docker build --build-arg BUILD_TYPE=debug -t pktgen-debug .`
- The container build enables Pktgen Lua support by default (`-Denable_lua=true`), so `script <file.lua>` is available in the Pktgen CLI.

- To run sender container: 
```bash
docker run -it --rm --privileged \
--name sender \
--network host \
-v /dev/hugepages:/dev/hugepages \
-v /lib/modules:/lib/modules \
dpdk-pktreplay /bin/bash
```

- To run receiver container: 
```bash
docker run -it --rm --privileged \
--name receiver \
--network host \
-v /dev/hugepages:/dev/hugepages \
-v /lib/modules:/lib/modules \
dpdk-pktreplay /bin/bash
```

### Run Pktgen within the container
- To run Pktgen: `./pktgen -l 0-2 -n 4 --vdev="net_pcap0,iface=veth1" -- -m "[1:2].0"`
- To run Pktgen with input file: `# ./Pktgen-DPDK/builddir/app/pktgen -l 0-2 -n 4 --vdev=net_pcap0,iface=veth1 -- -m [1:2].0 -s 0:data/chunk_19_0.pcap`

### Once running Pktgen
- Verify Lua is enabled: `script --help` (should not print "build with Lua enabled")
- `pcap show`
- `enable 0 pcap`
- `set 0 count [n]`
    - set n to 0 to go back to infinite looping ?
- `start 0`
- `stop 0`

`tcpdump -i veth0 -nn -s 0 -U udp -w /tmp/rx-udp.pcap`
`sudo tshark -i veth0 -n -s 128 -F pcap`
`sudo tshark -i veth0 -n -s 128 -f "ip or ip6 or arp" -F pcap -w /tmp/res.pcap`

`./Pktgen-DPDK/builddir/app/pktgen -l 0-2 -n 4 --vdev=net_pcap0,iface=veth1 -- -m [1:2].0 -s 0:data/chunk_20_0.pcap -f Pktgen-DPDK/scripts/pcap-window-loop.lua`

`./builddir/app/pktgen -l 0-2 -n 4 --vdev=net_pcap0,iface=veth1 -- -m [1:2].0 -s 0:../data/chunk_20_0.pcap -f scripts/pcap-window-loop.lua`
