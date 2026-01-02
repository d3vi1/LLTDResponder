# Win9x VxD Stub (Open Watcom)

This directory contains a minimal Win9x VxD stub that is built with Open Watcom
on Linux for CI packaging. The VxD is intentionally minimal and does not perform
LLTD packet processing yet.

## Build

```sh
make -f lltdDaemon/windows9x_vxd/Makefile.watcom
```

## Output

`lltd.vxd` is produced in the local directory. The CI scripts copy it to
`dist/windows/win9x/`.
