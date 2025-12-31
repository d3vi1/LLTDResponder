#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <integration_helper> <daemon_binary>" >&2
  exit 1
fi

INTEGRATION_HELPER="$1"
DAEMON_BINARY="$2"

if [ ! -x "$INTEGRATION_HELPER" ]; then
  echo "Integration helper not executable: $INTEGRATION_HELPER" >&2
  exit 1
fi

if [ ! -x "$DAEMON_BINARY" ]; then
  echo "Daemon binary not executable: $DAEMON_BINARY" >&2
  exit 1
fi

VETH_A="lltd-test0"
VETH_B="lltd-test1"

cleanup() {
  set +e
  if [ -n "${DAEMON_PID:-}" ]; then
    kill "$DAEMON_PID" >/dev/null 2>&1 || true
    wait "$DAEMON_PID" >/dev/null 2>&1 || true
  fi
  sudo ip link del "$VETH_A" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sudo ip link add "$VETH_A" type veth peer name "$VETH_B"
sudo ip link set "$VETH_A" up
sudo ip link set "$VETH_B" up

sudo setcap cap_net_raw+ep "$INTEGRATION_HELPER"
sudo setcap cap_net_raw+ep "$DAEMON_BINARY"

LLTD_INTERFACE="$VETH_A" "$DAEMON_BINARY" >/dev/null 2>&1 &
DAEMON_PID=$!

sleep 1

"$INTEGRATION_HELPER" --interface "$VETH_B" --timeout 3000
