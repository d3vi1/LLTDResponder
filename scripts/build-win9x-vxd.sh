#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

required_tools=(wcc386 wlink)
for tool in "${required_tools[@]}"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Missing required Open Watcom tool: $tool" >&2
    exit 1
  fi
done

make -f "$ROOT_DIR/lltdDaemon/windows9x_vxd/Makefile.watcom" -C "$ROOT_DIR/lltdDaemon/windows9x_vxd"

mkdir -p "$ROOT_DIR/dist/windows/win9x"
cp "$ROOT_DIR/lltdDaemon/windows9x_vxd/lltd.vxd" "$ROOT_DIR/dist/windows/win9x/lltd.vxd"
