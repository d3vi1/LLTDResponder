/******************************************************************************
 *                                                                            *
 *   freebsd-main.h                                                           *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDD_FREEBSD_MAIN_H
#define LLTDD_FREEBSD_MAIN_H

#include <net/if.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>

typedef struct {
    void   *smallIcon;
    size_t  smallIconSize;
    void   *largeIcon;
    size_t  largeIconSize;
} global_info_t;

extern global_info_t globalInfo;

typedef struct {
    const char   *deviceName;
    int           socket;
    struct sockaddr socketAddr;
    uint32_t      MTU;
    uint8_t       macAddress[6];
    uint8_t       MapperHwAddress[6];
    void         *seeList;
    uint32_t      seeListCount;
    uint16_t      MapperSeqNumber;
    void         *recvBuffer;
    size_t        recvBufferSize;
} network_interface_t;

#define lltdEtherType 0x88D9

#endif /* LLTDD_FREEBSD_MAIN_H */
