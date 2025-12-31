# Topology Discovery and Integration Daemons

## Details

This product wants to implement the following Protocols Responders
* Microsoft Link Layer Topology Discovery (OS X, Linux, ESXi, Solaris, FreeBSD)
* Microsoft Link-Local Multicast Name Resolution (OS X, Linux, Solaris, FreeBSD)
* Link Layer Discovery Protocol (OS X, Linux, ESXi, Solaris, FreeBSD)

## LLTD

The implementation is done from scratch based on the Microsoft provided documentation. No elements of the LLTD Porting kit from Microsoft were used in producing this code.

It does not yet implement the WiFi elements since linking to CoreWLAN is a bit more complex than apparent at first sight. Some of the information is not available directly in IOKit. It links strictly to CoreFoundation, LaunchD and ASL. One thread per CSMA/C[A|D] interface and the threads get created/killed as soon as an interface arrives or dissapears using I/OKit notifications.

On Linux there are two paths: a systemd/NetworkManager-backed daemon (journald logging, NetworkManager interface discovery), and a minimal embedded UAPI path that uses classic sockets and /dev/console logging.

The LLTD codebase is now organized by platform under `lltdDaemon/`:
* `darwin/` – macOS implementation and LaunchD plist
* `linux/` – systemd/NetworkManager daemon and embedded Linux main
* `freebsd/` – BPF-based draft implementation
* `sunos/` – draft implementation (backend TODO)
* `beos/` – draft implementation (backend TODO)
* `esp32/` – reusable ESP32 library with build-time icon data
* `esxi/` – ESXi userworld draft based on Linux flow
* `windows/` – Windows draft shared across Win16/Win9x/WinNT/2000

## Building LLTD on Linux

The Linux build supports two backends selected via `PLATFORM`:

* `linux-systemd` (default) – systemd/journald logging and systemd DBus helpers.
* `linux-embedded` – UAPI/POSIX only (no systemd deps), suitable for embedded Linux.

Examples:

```sh
make PLATFORM=linux-systemd
make PLATFORM=linux-embedded
```

Dependencies:
* `linux-systemd`: `libsystemd-dev` (Ubuntu/Debian) for `sd-journal`/`sd-bus`.
* `linux-embedded`: no additional dependencies beyond libc.

## Testing

Unit tests use a minimal in-tree harness and LLTD golden vectors:

```sh
make test
```

Linux-only integration smoke test (requires `ip` and `setcap`):

```sh
make PLATFORM=linux-embedded
make integration-test PLATFORM=linux-embedded
```

## Service integration

### systemd (linux-systemd)

Install the provided unit file and enable the service:

```sh
sudo install -m 644 packaging/systemd/lltd.service /etc/systemd/system/lltd.service
sudo systemctl daemon-reload
sudo systemctl enable --now lltd.service
```

The systemd unit runs with hardening enabled and grants only `CAP_NET_RAW` so the
daemon can open raw sockets. Logs are emitted to journald.

### init.d (embedded/non-systemd)

An example init script is provided under `packaging/init.d/lltd`. Adjust the
interface list and install it to your system's init.d location. Logs go to
syslog/console depending on the platform configuration.

## LLDP

The implementation should be adapted from the lldpd project clean integration for the target OSs. It automatically discovers device topologies (bridge and others and correctly informs the neighbors about it).
Licensing is TBD.
