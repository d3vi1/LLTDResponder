/******************************************************************************
 *                                                                            *
 *   esxi-main.h                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDD_ESXI_MAIN_H
#define LLTDD_ESXI_MAIN_H

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
    int           helloSent;
} network_interface_t;

#define lltdEtherType 0x88D9

#endif /* LLTDD_ESXI_MAIN_H */
