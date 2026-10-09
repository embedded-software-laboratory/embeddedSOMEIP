#!/usr/bin/env bash
set -uo pipefail

DIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
BUILD="$DIR/build"
ROLE="${1:-esomeip-server}"

NET="${INTEROP_NET:-someip-interop}"
SUBNET="${INTEROP_SUBNET:-172.30.0.0/16}"
SERVER_IP="${SERVER_IP:-172.30.0.2}"
CLIENT_IP="${CLIENT_IP:-172.30.0.3}"
IMAGE="${INTEROP_IMAGE:-ubuntu:24.04}"
COUNT="${COUNT:-10}"
# directory holding the vsomeip install, mounted read-only into the containers
VSOMEIP_PREFIX="${VSOMEIP_PREFIX:?set VSOMEIP_PREFIX to the vsomeip install prefix}"
VSOMEIP_LIB="${VSOMEIP_LIB:-$VSOMEIP_PREFIX/lib}"

ERR="$BUILD/embeddedsomeip_rr"
VRR="$BUILD/vsomeip_rr"
[ -x "$ERR" ] && [ -x "$VRR" ] || { echo "build interop/build first (cmake --build interop/build)" >&2; exit 1; }

RO="-v $VSOMEIP_PREFIX:$VSOMEIP_PREFIX:ro -v /usr/lib/x86_64-linux-gnu:/hostlib:ro"
LDP="$VSOMEIP_LIB:/hostlib"

docker network inspect "$NET" >/dev/null 2>&1 || docker network create --subnet "$SUBNET" "$NET" >/dev/null
cleanup() { docker rm -f rr_server rr_client >/dev/null 2>&1; }
cleanup; trap cleanup EXIT

# provider config declares 0x7777 external, subscriber config has no services[]
mk_vcfg() { sed "s/\"unicast\": \"[^\"]*\"/\"unicast\": \"$2\"/" "$DIR/$1" > "/tmp/${1%.json}_rr.json"; echo "/tmp/${1%.json}_rr.json"; }

echo "role=$ROLE network=$NET (server=$SERVER_IP client=$CLIENT_IP)"

case "$ROLE" in
  esomeip-server)
    docker run -d --name rr_server --network "$NET" --ip "$SERVER_IP" $RO -e LD_LIBRARY_PATH="$LDP" \
      "$IMAGE" "$ERR" --role server --local-ip "$SERVER_IP" --base-port 40000 >/dev/null
    sleep 2
    VCFG=$(mk_vcfg vsomeip_subscriber.json "$CLIENT_IP")
    docker run -d --name rr_client --network "$NET" --ip "$CLIENT_IP" $RO \
      -e LD_LIBRARY_PATH="$LDP" -e VSOMEIP_CONFIGURATION=/cfg/v.json -e VSOMEIP_APPLICATION_NAME=vsomeip_interop \
      -v "$VCFG":/cfg/v.json:ro "$IMAGE" "$VRR" --role client --count "$COUNT" >/dev/null
    ;;
  vsomeip-server)
    VCFG=$(mk_vcfg vsomeip_provider.json "$SERVER_IP")
    docker run -d --name rr_server --network "$NET" --ip "$SERVER_IP" $RO \
      -e LD_LIBRARY_PATH="$LDP" -e VSOMEIP_CONFIGURATION=/cfg/v.json -e VSOMEIP_APPLICATION_NAME=vsomeip_interop \
      -v "$VCFG":/cfg/v.json:ro "$IMAGE" "$VRR" --role server >/dev/null
    sleep 2
    docker run -d --name rr_client --network "$NET" --ip "$CLIENT_IP" $RO -e LD_LIBRARY_PATH="$LDP" \
      "$IMAGE" "$ERR" --role client --local-ip "$CLIENT_IP" --base-port 40010 --count "$COUNT" >/dev/null
    ;;
  *) echo "usage: $0 esomeip-server|vsomeip-server" >&2; exit 2 ;;
esac

sleep $(( COUNT / 2 + 8 ))
echo "================= RESULT ($ROLE) ================="
echo "--- client (rr_client) ---"
docker logs rr_client 2>&1 | grep -iE "discover|AVAILABLE|resp #|received [0-9]+/" | tail -6
echo "--- server (rr_server) ---"
docker logs rr_server 2>&1 | grep -iE "req #|offering|handled" | tail -4
