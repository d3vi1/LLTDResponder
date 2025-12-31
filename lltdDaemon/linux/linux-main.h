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

struct {
    void   *smallIcon;
    size_t  smallIconSize;
    void   *largeIcon;
    size_t  largeIconSize;
} globalInfo;

typedef struct {
    const char   *deviceName;
    uint32_t      ifType;
    int           ifIndex;
    int           socket;
    struct sockaddr_ll socketAddr;
    uint32_t      MTU;
    uint8_t       macAddress[6];
    uint8_t       MapperHwAddress[6];
    void         *seeList;
    uint32_t      seeListCount;
    uint16_t      MapperSeqNumber;
    void         *recvBuffer;
} network_interface_t;

#define lltdEtherType     0x88D9

#endif /* linux_main_h */
