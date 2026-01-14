# Win16 Diagnostic Utility (Open Watcom)

This directory contains a simple Win16 diagnostic utility that outputs a
fixed TLV vector to `OUTPUT.BIN` and a human-readable summary to
`OUTPUT.TXT`. It does not perform raw Ethernet I/O.

## Build

```sh
make -f os/windows/win16-diagnostic/Makefile.watcom
```

## Output

`lltd16.exe` is produced in the local directory. The CI scripts copy it to
`dist/windows/win16/`.
