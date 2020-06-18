# Topology Discovery and Integration Daemons

## Details

This product wants to implement the following Protocols Responders
* Microsoft Link Layer Topology Discovery (OS X, Linux, ESXi, Solaris, FreeBSD)
* Microsoft Link-Local Multicast Name Resolution (OS X, Linux, Solaris, FreeBSD)
* Link Layer Discovery Protocol (OS X, Linux, ESXi, Solaris, FreeBSD)

## LLTD

The implementation is done from scratch based on the Microsoft provided documentation. No elements of the LLTD Porting kit from Microsoft were used in producing this code.

The end-license will be GNU GPLv3 for the with commercial products available for ESXi, Solaris, OS X and OEM Linux.

It does not yet implement the WiFi elements since linking to CoreWLAN is a bit more complex than apparent at first sight. Some of the information is not available directly in IOKit. It links strictly to CoreFoundation, LaunchD and ASL. One thread per CSMA/C[A|D] interface and the threads get created/killed as soon as an interface arrives or dissapears using I/OKit notifications.

On Linux it's supposed to be a daemon that gets the information directly from the Linux kernel, bypassing any NetworkManager style solutions that might be present since they are not standard in any way. The Linux port should work seamlessly on ESXi since the Kernel ABI is identical WRT to network interfaces.

## LLMNR

The implementation will be done from scratch based on the Microsoft provided documentation. Fundamentally it's almost identical to mDNS except that it can only resolve "IN A(AAA)" queries and no SRV. So it's only a host discovery and no service discovery protocol. I wonder why Microsoft simply didn't implement mDNS. It uses DNS Service Discovery for AD, why not for local services? Because it clashes with it's choice for UPnP.

The end-license will be GNU GPLv3 for the with commercial products available for Solaris, OS X and OEM Linux.

ESXi is specifically excluded as it cannot have a file-serving role.

## LLDP

The implementation should be adapted from the lldpd project clean integration for the target OSs. It automatically discovers device topologies (bridge and others and correctly informs the neighbors about it).
Licensing is TBD.
