# Windows Open Watcom (Win16 + Win9x VxD)

This document describes the Open Watcom build flow for the Win16 diagnostic
utility and the Win9x VxD stub. These builds are intended for Linux-hosted CI
runs and produce artifacts under `dist/windows/`.

## Prerequisites

- Open Watcom v2 installed and available on `PATH` (`wcc`, `wcc386`, `wlink`).
- Python 3 (for INF template generation).

## Build commands

```sh
make win16
make win9x-vxd
```

To build both:

```sh
make windows-openwatcom
```

## Outputs

- `dist/windows/win16/lltd16.exe`
- `dist/windows/win9x/lltd.vxd`

INF templates are generated alongside the binaries:

- `dist/windows/win16/oemsetup.inf`
- `dist/windows/win9x/lltd.inf`

The Win16 utility writes deterministic output files (`OUTPUT.BIN` and
`OUTPUT.TXT`) when executed. The Win9x VxD is currently a minimal stub used for
packaging.
