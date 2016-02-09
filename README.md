# Topology Discovery and Integration Daemons

## Details

This product implements the following Microsoft Protocols Responders
* Link Layer Topology Discovery (OS X, Linux, ESXi, Solaris, FreeBSD)
* Link-Local Multicast Name Resolution (OS X, Linux, Solaris, FreeBSD)
* Link Layer Discovery Protocol (OS X, Linux, ESXi, Solaris, FreeBSD)

## LLTD

The implementation is done from scratch based on the Microsoft provided
documentation. No elements of the LLTD Porting kit from Microsoft were
used in producing this code.

The end-license will be GNU GPLv3 for the with commercial products
available for ESXi, Solaris, OS X and OEM Linux.

## LLMNR

The implementation is done from scratch based on the Microsoft provided
documentation.

The end-license will be GNU GPLv3 for the with commercial products
available for Solaris, OS X and OEM Linux.

ESXi is specifically excluded as it cannot have a file-serving role.

## LLDP

The implementation is adapted from the OpenLLDP project clean integration
for the target OSs. It automatically discovers device topologies (bridge and
others and correctly informs the neighbors about it).
Licensing is TBD.
