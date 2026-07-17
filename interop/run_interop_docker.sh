#!/usr/bin/env bash
# two containers so each stack gets its own IP, vsomeip drops SD from its own unicast
set -uo pipefail

DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
BUILD="$DIR/build"
DIRECTION="${1:-esomeip-pub}"

NET="${INTEROP_NET:-someip-interop}"
SUBNET="${INTEROP_SUBNET:-172.30.0.0/16}"
ESOMEIP_IP="${ESOMEIP_IP:-172.30.0.2}"        # used when embeddedSOMEIP is the publisher
VSOMEIP_IP="${VSOMEIP_IP:-172.30.0.3}"
IMAGE="${INTEROP_IMAGE:-soa-tracer-node:latest}"
COUNT="${COUNT:-30}"
INTERVAL="${INTERVAL:-300}"
SIZE="${SIZE:-64}"
EMBEDDED_ROOT="${EMBEDDED_ROOT:-/home/davidk/embedded}"
VSOMEIP_LIB="${VSOMEIP_LIB:-$EMBEDDED_ROOT/vsomeip-install/lib}"

EMB="$BUILD/embeddedsomeip_interop"
VSO="$BUILD/vsomeip_interop"
[ -x "$EMB" ] && [ -x "$VSO" ] || { echo "build interop/build first (cmake --build interop/build)" >&2; exit 1; }

RO="-v $EMBEDDED_ROOT:$EMBEDDED_ROOT:ro -v /usr/lib/x86_64-linux-gnu:/hostlib:ro"
LDP="$VSOMEIP_LIB:/hostlib"

docker network inspect "$NET" >/dev/null 2>&1 || docker network create --subnet "$SUBNET" "$NET" >/dev/null

cleanup() { docker rm -f esm_node vso_node >/dev/null 2>&1; }
cleanup
trap cleanup EXIT

# Subscriber config must not declare services[], else vsomeip treats 0x7777 as local.
mk_vcfg() { sed "s/\"unicast\": \"[^\"]*\"/\"unicast\": \"$2\"/" "$DIR/$1" > "/tmp/${1%.json}_run.json"; echo "/tmp/${1%.json}_run.json"; }

echo "direction=$DIRECTION network=$NET (esomeip=$ESOMEIP_IP vsomeip=$VSOMEIP_IP)"

case "$DIRECTION" in
  esomeip-pub)
    VCFG=$(mk_vcfg vsomeip_subscriber.json "$VSOMEIP_IP")
    docker run -d --name esm_node --network "$NET" --ip "$ESOMEIP_IP" $RO -e LD_LIBRARY_PATH="$LDP" \
      "$IMAGE" "$EMB" --role pub --local-ip "$ESOMEIP_IP" --base-port 40000 \
      --count "$COUNT" --interval-ms "$INTERVAL" --size "$SIZE" >/dev/null
    sleep 2
    docker run -d --name vso_node --network "$NET" --ip "$VSOMEIP_IP" $RO \
      -e LD_LIBRARY_PATH="$LDP" -e VSOMEIP_CONFIGURATION=/cfg/v.json -e VSOMEIP_APPLICATION_NAME=vsomeip_interop \
      -v "$VCFG":/cfg/v.json:ro "$IMAGE" "$VSO" --role sub --count "$COUNT" >/dev/null
    PUB=esm_node; SUB=vso_node
    ;;
  vsomeip-pub)
    VCFG=$(mk_vcfg vsomeip_provider.json "$ESOMEIP_IP")   # vsomeip publishes from .2
    docker run -d --name vso_node --network "$NET" --ip "$ESOMEIP_IP" $RO \
      -e LD_LIBRARY_PATH="$LDP" -e VSOMEIP_CONFIGURATION=/cfg/v.json -e VSOMEIP_APPLICATION_NAME=vsomeip_interop \
      -v "$VCFG":/cfg/v.json:ro "$IMAGE" "$VSO" --role pub \
      --count "$COUNT" --interval-ms "$INTERVAL" --size "$SIZE" >/dev/null
    sleep 2
    docker run -d --name esm_node --network "$NET" --ip "$VSOMEIP_IP" $RO -e LD_LIBRARY_PATH="$LDP" \
      "$IMAGE" "$EMB" --role sub --local-ip "$VSOMEIP_IP" --base-port 40010 >/dev/null
    PUB=vso_node; SUB=esm_node
    ;;
  *) echo "usage: $0 esomeip-pub|vsomeip-pub" >&2; exit 2 ;;
esac

SECS=$(( COUNT * INTERVAL / 1000 + 8 ))
sleep "$SECS"

echo "================= RESULT ($DIRECTION) ================="
echo "--- subscriber ($SUB) ---"
docker logs "$SUB" 2>&1 | grep -iE "AVAILABLE|recv #|received [0-9]+ notif|discovered=" | tail -8
echo "--- publisher ($PUB) ---"
docker logs "$PUB" 2>&1 | grep -iE "subscriber|sent seq|subscription (added|accepted)|done" | tail -4
