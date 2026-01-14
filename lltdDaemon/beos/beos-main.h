/******************************************************************************
 *                                                                            *
 *   beos-main.h                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDD_BEOS_MAIN_H
#define LLTDD_BEOS_MAIN_H

#include <stdint.h>
#include <stddef.h>

struct {
    void   *smallIcon;
    size_t  smallIconSize;
    void   *largeIcon;
    size_t  largeIconSize;
} globalInfo;

typedef struct {
    const char *deviceName;
    int         socket;
    uint32_t    MTU;
    uint8_t     macAddress[6];
    uint8_t     MapperHwAddress[6];
    uint8_t     MapperApparentAddress[6];
    uint8_t     MapperKnown;
    void       *seeList;
    uint32_t    seeListCount;
    uint16_t    MapperSeqNumber;
    uint16_t    MapperGenerationTopology;
    uint16_t    MapperGenerationQuick;
    uint64_t    LastHelloTxMs;
    uint64_t    LastHelloReplyMs;
    uint16_t    LastHelloReplyXid;
    uint16_t    LastHelloReplyGen;
    uint8_t     LastHelloReplyTos;
    void       *recvBuffer;
    int         helloSent;
} network_interface_t;

#ifndef lltdEtherType
#define lltdEtherType 0x88D9
#endif

#endif /* LLTDD_BEOS_MAIN_H */
