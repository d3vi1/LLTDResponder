#!/usr/bin/env python3
"""Offline LLTD regression checks for Hello sequence/generation behavior."""

import sys
from collections import defaultdict

LLTD_ETHERTYPE = 0x88D9
OPCODE_DISCOVER = 0x00
OPCODE_HELLO = 0x01
HELLO_RATE_THRESHOLD = 10  # hellos/sec per responder MAC


def format_mac(raw: bytes) -> str:
    return ":".join(f"{b:02x}" for b in raw)


def bswap16(value: int) -> int:
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF)


def iter_packets(path: str):
    try:
        from scapy.all import PcapNgReader, PcapReader
    except ImportError:
        print("ERROR: scapy is required to run this script (pip install scapy)")
        sys.exit(2)

    reader = None
    try:
        reader = PcapNgReader(path)
    except Exception:
        reader = PcapReader(path)

    try:
        for pkt in reader:
            yield pkt
    finally:
        reader.close()


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("Usage: tests/pcap_regression.py <pcapng/pcap> [pcap...]")
        return 2

    violations = []
    generation_by_mapper = {}
    hello_counts = defaultdict(int)

    for path in argv[1:]:
        for pkt in iter_packets(path):
            try:
                data = bytes(pkt)
            except Exception:
                continue

            if len(data) < 32:
                continue

            ethertype = int.from_bytes(data[12:14], "big")
            if ethertype != LLTD_ETHERTYPE:
                continue

            tos = data[15]
            opcode = data[17]
            seq = int.from_bytes(data[30:32], "big")
            real_source = data[24:30]

            if opcode == OPCODE_DISCOVER:
                if len(data) < 36:
                    continue
                generation = int.from_bytes(data[32:34], "big")
                mapper_mac = format_mac(real_source)
                generation_by_mapper[mapper_mac] = generation

            if opcode == OPCODE_HELLO:
                if seq != 0:
                    violations.append(
                        f"{path}: Hello seq != 0 (seq={seq}) src={format_mac(real_source)} tos={tos}"
                    )

                if len(data) >= 46:
                    generation = int.from_bytes(data[32:34], "big")
                    current_mapper = format_mac(data[34:40])
                    expected = generation_by_mapper.get(current_mapper)
                    if expected is not None and generation != expected:
                        if generation == bswap16(expected):
                            violations.append(
                                f"{path}: Hello generation looks byte-swapped src={format_mac(real_source)} "
                                f"mapper={current_mapper} expected=0x{expected:04x} got=0x{generation:04x}"
                            )
                        else:
                            violations.append(
                                f"{path}: Hello generation mismatch src={format_mac(real_source)} "
                                f"mapper={current_mapper} expected=0x{expected:04x} got=0x{generation:04x}"
                            )

                ts_bucket = int(getattr(pkt, "time", 0))
                hello_counts[(format_mac(real_source), ts_bucket)] += 1

    for (src, ts_bucket), count in sorted(hello_counts.items()):
        if count > HELLO_RATE_THRESHOLD:
            violations.append(
                f"Hello storm: src={src} t={ts_bucket} count={count} > {HELLO_RATE_THRESHOLD}/s"
            )

    if violations:
        print("LLTD regression violations:")
        for entry in violations:
            print(f"- {entry}")
        return 1

    print("LLTD regression checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
