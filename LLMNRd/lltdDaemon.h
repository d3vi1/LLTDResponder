/******************************************************************************
 *                                                                            *
 *   lltdDaemon.h                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 09.02.2016.                      *
 *   Copyright © 2016 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#ifndef lltdDaemon_h
#define lltdDaemon_h


#ifdef __APPLE__
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdbool.h>
    #include <string.h>
    #include <asl.h>                                            // Apple System Logging instead of printf()
    #include <launch.h>                                         // LaunchD Notification
    #include <pthread.h>                                        // For POSIX Threads
    #include <ifaddrs.h>
    #include <arpa/inet.h>
    #include <mach/mach.h>                                      // For RunLoop Notifications
    #include <mach/mach_time.h>
    #include <net/if.h>                                         // For IFFlags
    #include <net/ndrv.h>
    #include <netinet/in.h>
    #include <sys/ioctl.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include <CoreFoundation/CFArray.h>
    #include <CoreServices/CoreServices.h>                      // darwin-ops::getIconImage()
    #include <ImageIO/ImageIO.h>                                // darwin-ops::getIconImage() For ICNS to ICO
                                                                //                conversion using Thumbnails
    #include <IOKit/network/IONetworkInterface.h>               // darwinMain::validateInterface()
    #include <IOKit/network/IONetworkController.h>              // darwinMain::validateInterface()
    #include <IOKit/network/IOEthernetController.h>             // darwinMain::validateInterface()
    #include <IOKit/network/IONetworkMedium.h>                  // darwinMain::validateInterface()
    #include <IOKit/IOKitLib.h>                                 // darwinMain::validateInterface()
    #include <IOKit/IOBSD.h>                                    // darwinMain::validateInterface()
    #include <IOKit/IOMessage.h>                                // darwinMain::validateInterface()
    #include <IOKit/IOCFPlugIn.h>                               // darwinMain::validateInterface()
    #include <SystemConfiguration/SCNetwork.h>                  // darwinMain::validateInterface()
    #include <SystemConfiguration/SCNetworkConfiguration.h>     // For IP Configuration
    #include <SystemConfiguration/SCNetworkConnection.h>        // For Connection status
    #include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h> // For DHCPInfoGetOptionData
    #include "msIcoFormat.h"
    #include "lltdBlock.h"
    #include "darwin-main.h"
    #include "darwin-ops.h"

#endif /*__darwin__ */

#ifdef __linux__
    #include <linux/netlink.h>
    #include <linux/rtnetlink.h>
    #include <net/if.h>
    #include <netinet/in.h>
    #include <asm/types.h>
    #include <sys/socket.h>
    #include <errno.h>
    #include <stdio.h>
    #include <string.h>
    #include <unistd.h>
    #include "linux-main.h"
    #include "linux-ops.h"
    #define  MYPROTO NETLINK_ROUTE
    #define  MYMGRP RTMGRP_IPV4_ROUTE
#endif /*__linux__*/

#ifdef __FreeBSD__
    #include "freebsd-main.h"
    #include "freebsd-ops.h"
#endif /*__FreeBSD__*/

#ifdef __sunos_5_10
    #include "sunos-main.h"
    #include "sunos-ops.h"
#endif /*__sunos_5_10__*/

#ifdef __BEOS__
    #include "beos-main.h"
    #include "beos-ops.h"
#endif /*__BEOS__*/

#include "lltdBlock.h"
#include "lltdTlvOps.h"


#endif /* lltdDaemon_h */
