# containers

## Instructions

### Build and Run Docker Containers
- To build (from top folder): `docker build -t dpdk-pktreplay -f containers/dpdk-pktreplay/Dockerfile .`
    - To build in debug mode, add docker `--build-arg BUILD_TYPE=debug`
        - Ex: `docker build --build-arg BUILD_TYPE=debug -t pktgen-debug .`

- To run sender container: 
```bash
docker run -it --privileged \
--name sender \
--network host \
-v /dev/hugepages:/dev/hugepages \
-v /lib/modules:/lib/modules \
dpdk-pktreplay /bin/bash
```

- To run receiver container: 
```bash
docker run -it --privileged \
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
- `pcap show`
- `enable 0 pcap`
- `set 0 count [n]`
    - set n to 0 to go back to infinite looping ?
- `start 0`
- `stop 0`


