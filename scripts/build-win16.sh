#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

required_tools=(wcc wlink)
for tool in "${required_tools[@]}"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Missing required Open Watcom tool: $tool" >&2
    exit 1
  fi
done

make -f "$ROOT_DIR/lltdDaemon/win16/Makefile.watcom" -C "$ROOT_DIR/lltdDaemon/win16"

mkdir -p "$ROOT_DIR/dist/windows/win16"
cp "$ROOT_DIR/lltdDaemon/win16/lltd16.exe" "$ROOT_DIR/dist/windows/win16/lltd16.exe"
