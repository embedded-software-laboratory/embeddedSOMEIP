# Interoperability between embeddedSOMEIP and vsomeip

Two standalone tools that test SOME/IP and SOME/IP-SD on the wire, used to get interopability working between embeddedSOMEIP and COVESA vsomeip. 

Shared IDs (must match across both stacks and `vsomeip.json`):
`service=0x7777 instance=0x0001 major=1 minor=0 eventgroup=0x0001 event=0x8001`, UDP.

## Build

```sh
cmake -S interop -B interop/build -DCMAKE_PREFIX_PATH=/home/davidk/embedded/vsomeip-install
cmake --build interop/build -j
```

Produces `embeddedsomeip_interop` + `vsomeip_interop` (events) and `embeddedsomeip_rr` +
`vsomeip_rr` (request/response). Without a `CMAKE_PREFIX_PATH` that finds vsomeip3, the
`vsomeip_*` peers are skipped with only a warning, so check the configure output for
`vsomeip3 found`.

## Run

Runs as two Docker containers on an isolated bridge. vsomeip drops SD messages whose
source IP is its own `unicast`, so both stacks need distinct IPs.

```sh
# events: embeddedSOMEIP publishes, vsomeip subscribes (or: vsomeip-pub)
./interop/run_interop_docker.sh esomeip-pub

# request/response: embeddedSOMEIP serves, vsomeip calls (or: vsomeip-server)
./interop/run_rr_docker.sh esomeip-server
```

`run_interop.sh` runs both on the host, which cross-stack always yields `subscribers=0`
for the reason above. Single-stack wire debugging only:

```sh
# needs dumpcap privileges
CAPTURE=1 LOCAL_IP=192.168.178.133 ./interop/run_interop.sh esomeip-pub
```

Each tool can also be run by hand:
```sh
./interop/build/embeddedsomeip_interop --role pub --local-ip 192.168.178.133 --base-port 40000 --count 20
VSOMEIP_CONFIGURATION=interop/vsomeip.json ./interop/build/vsomeip_interop --role sub
```


## Debugging the wire

```sh
tshark -r interop/interop_esomeip-pub.pcap -Y someipsd -O someipsd   # SD: OFFER/FIND/SUBSCRIBE/ACK fields
tshark -r interop/interop_esomeip-pub.pcap -Y someip                 # notifications
```

Success is measured as notifications flowing in both directions and no reports of malformed SD/SOME/IP frames from embeddedSOMEIP.
