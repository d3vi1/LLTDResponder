# Topology Discovery and Integration Daemons

## Details

This product wants to implement the Microsoft Link Layer Topology Discovery Protocol (OS X, Linux, ESXi, Solaris, FreeBSD, Windows, BeOS/Haiku as native services/daemons and ESP32 as a library)

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
