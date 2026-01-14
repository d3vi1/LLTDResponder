/******************************************************************************
 *                                                                            *
 *   linux-main.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 09.02.2016.                      *
 *   Copyright © 2016 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#ifndef linux_main_h
#define linux_main_h

#include <net/if.h>
#include <netpacket/packet.h>
#include <stdint.h>
#include <stddef.h>

struct automata;

typedef struct {
    const char   *deviceName;
    uint32_t      ifType;
    enum { NetworkInterfaceTypeBond, NetworkInterfaceTypeBridge, NetworkInterfaceTypeEthernet, NetworkInterfaceTypeIEEE80211, NetworkInterfaceTypeVLAN} interfaceType;
    int           ifIndex;
    int           socket;
    struct sockaddr_ll socketAddr;
    uint32_t      MediumType;
    uint32_t      MTU;
    uint32_t      LinkSpeed;
    uint32_t      flags;
    uint8_t       macAddress[6];
    uint8_t       MapperHwAddress[6];
    uint8_t       MapperApparentAddress[6];
    uint8_t       MapperKnown;
    void         *seeList;
    uint32_t      seeListCount;
    uint16_t      MapperSeqNumber;
    uint16_t      MapperGenerationTopology;
    uint16_t      MapperGenerationQuick;
    uint64_t      LastHelloTxMs;
    uint64_t      LastHelloReplyMs;
    uint16_t      LastHelloReplyXid;
    uint16_t      LastHelloReplyGen;
    uint8_t       LastHelloReplyTos;
    void         *recvBuffer;
    struct automata *enumerationAutomata;
    int           helloSent;
} network_interface_t;

#ifndef lltdEtherType
#define lltdEtherType     0x88D9
#endif

#endif /* linux_main_h */
