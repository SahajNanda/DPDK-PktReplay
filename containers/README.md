# containers

## Instructions

### Build and Run Docker Containers
- To build (from top folder): `docker build -t dpdk-pktreplay -f containers/server/Dockerfile .`
    - To build in debug mode, add docker `--build-arg BUILD_TYPE=debug`
        - Ex: `docker build --build-arg BUILD_TYPE=debug -t pktgen-debug .`

- To run sender container: 
```bash
docker run -it --privileged \ 
--network host \ 
-v /dev/hugepages:/dev/hugepages \ 
-v /lib/modules:/lib/modules \ 
dpdk-pktreplay /bin/bash
```

- To run receiver container: 
```bash
docker run -it --privileged \ 
--network host \ 
-v /dev/hugepages:/dev/hugepages \ 
-v /lib/modules:/lib/modules \ 
dpdk-receiver /bin/bash
```

### Run Pktgen within the container
- To run Pktgen in docker: `./pktgen -l 0-4 -n 4 --vdev="net_pcap0,iface=veth1" -- -m "[1:2].0"`