/******************************************************************************
 *                                                                            *
 *   windows-main.h                                                           *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDD_WINDOWS_MAIN_H
#define LLTDD_WINDOWS_MAIN_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    void   *smallIcon;
    size_t  smallIconSize;
    void   *largeIcon;
    size_t  largeIconSize;
} windows_global_info_t;

extern windows_global_info_t globalInfo;

typedef struct {
    const char *deviceName;
    uint32_t    MTU;
    uint8_t     macAddress[6];
    uint8_t     MapperHwAddress[6];
    void       *seeList;
    uint32_t    seeListCount;
    uint16_t    MapperSeqNumber;
    void       *recvBuffer;
} network_interface_t;

#define lltdEtherType 0x88D9

#endif /* LLTDD_WINDOWS_MAIN_H */
