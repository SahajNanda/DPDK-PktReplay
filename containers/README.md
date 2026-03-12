to run in debug mode, add docker `--build-arg BUILD_TYPE=debug`
ex: `docker build --build-arg BUILD_TYPE=debug -t pktgen-debug .`

to build (from top folder): `docker build -t dpdk-pktreplay -f containers/server/Dockerfile .`
to run: `docker run -it --privileged --network host -v /dev/hugepages:/dev/hugepages -v /lib/modules:/lib/modules dpdk-pktreplay /bin/bash`

to run pktgen in docker: `./pktgen -l 0-4 -n 4 --vdev="net_pcap0,iface=veth1" -- -m "[1:2].0"`