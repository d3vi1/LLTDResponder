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
    #include <ImageIO/ImageIO.h>                                // darwin-ops::getIconImage() For ICNS to ICO conversion using Thumbnails
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
    #include "darwin-main.h"
    #include "darwin-ops.h"
    #include "msIcoFormat.h"
    #include "lltdBlock.h"
    #include "lltdTlvOps.h"
    #include "lltdAutomata.h"
    #define log_debug(x, ...)   asl_log(asl, log_msg, ASL_LEVEL_DEBUG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_info(x, ...)    asl_log(asl, log_msg, ASL_LEVEL_INFO,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_notice(x, ...)  asl_log(asl, log_msg, ASL_LEVEL_NOTICE,  "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_warning(x, ...) asl_log(asl, log_msg, ASL_LEVEL_WARNING, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_err(x, ...)     asl_log(asl, log_msg, ASL_LEVEL_ERR,     "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_crit(x, ...)    asl_log(asl, log_msg, ASL_LEVEL_CRIT,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_alert(x, ...)   asl_log(asl, log_msg, ASL_LEVEL_ALERT,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_emerg(x, ...)   asl_log(asl, log_msg, ASL_LEVEL_EMERG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
#endif /*__darwin__ */

/*
 Optionally use posix threads. Otherwise use forking.
 Optionally use SystemD with instances.
 Optionally use Syslog of not on systemd.
 Optionally get the interfaces from NetworkManager via D-Bus (kernel agnostic).
 */
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
    #ifdef USE_SYSTEMD

    #else
        #define log_debug(x, ...)   syslog(LOG_DEBUG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define log_info(x, ...)    syslog(LOG_INFO,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define log_notice(x, ...)  syslog(LOG_NOTICE,  "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define log_warning(x, ...) syslog(LOG_WARNING, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define log_err(x, ...)     syslog(LOG_ERR,     "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define log_crit(x, ...)    syslog(LOG_CRIT,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define log_alert(x, ...)   syslog(LOG_ALERT,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define log_emerg(x, ...)   syslog(LOG_EMERG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #endif /* USE_SYSTEMD */
#endif /*__linux__*/

/*
 IFNET(9)
 struct ifnethead or DevD for listing and events
 RC Script and optional RelaunchD support
 */
#ifdef __FreeBSD__
    #include "freebsd-main.h"
    #include "freebsd-ops.h"
#endif /*__FreeBSD__*/

/*
 Optionally use NWAM for listing the interfaces.
 Fallback to if.h otherwise.
 Optionally use SMF.
 */
#ifdef __sunos_5_10
    #include "sunos-main.h"
    #include "sunos-ops.h"
#endif /*__sunos_5_10__*/

#ifdef __BEOS__
    #include "beos-main.h"
    #include "beos-ops.h"
#endif /*__BEOS__*/


#endif /* lltdDaemon_h */
