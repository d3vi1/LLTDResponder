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
    #include <SystemConfiguration/CaptiveNetwork.h>             // darwin-ops WiFi SSID/BSSID
    #include "darwin/darwin-main.h"
    #include "darwin/darwin-ops.h"
    #include "msIcoFormat.h"
    #include "lltdBlock.h"
    #include "lltdTlvOps.h"
    #include "lltdAutomata.h"
    #define log_debug(x, ...)   asl_log(globalInfo.asl, globalInfo.log_msg, ASL_LEVEL_DEBUG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_info(x, ...)    asl_log(globalInfo.asl, globalInfo.log_msg, ASL_LEVEL_INFO,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_notice(x, ...)  asl_log(globalInfo.asl, globalInfo.log_msg, ASL_LEVEL_NOTICE,  "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_warning(x, ...) asl_log(globalInfo.asl, globalInfo.log_msg, ASL_LEVEL_WARNING, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_err(x, ...)     asl_log(globalInfo.asl, globalInfo.log_msg, ASL_LEVEL_ERR,     "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_crit(x, ...)    asl_log(globalInfo.asl, globalInfo.log_msg, ASL_LEVEL_CRIT,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_alert(x, ...)   asl_log(globalInfo.asl, globalInfo.log_msg, ASL_LEVEL_ALERT,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_emerg(x, ...)   asl_log(globalInfo.asl, globalInfo.log_msg, ASL_LEVEL_EMERG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
#endif /*__darwin__ */

/*
 Optionally use posix threads. Otherwise use forking.
 Optionally use SystemD with instances.
 Optionally use Syslog of not on systemd.
 Optionally get the interfaces from NetworkManager via D-Bus (kernel agnostic).
 */
#ifdef __linux__
    #include <asm/types.h>
    #include <stdarg.h>
    #include <stdio.h>
    #include <sys/socket.h>
    #include <linux/netlink.h>
    #include <linux/rtnetlink.h>
    #include <memory.h>
    #include <errno.h>
    #include <signal.h>
    #include <syslog.h>
    #include <time.h>
    #include "linux/linux-main.h"
    #include "linux/linux-ops.h"
    #define  MYPROTO NETLINK_ROUTE
    #define  MYMGRP RTMGRP_IPV4_ROUTE
    #if defined(LLTD_USE_SYSTEMD)
        #include <systemd/sd-journal.h>
        #define  log_debug(x, ...)   sd_journal_print(LOG_DEBUG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_info(x, ...)    sd_journal_print(LOG_INFO,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_notice(x, ...)  sd_journal_print(LOG_NOTICE,  "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_warning(x, ...) sd_journal_print(LOG_WARNING, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_err(x, ...)     sd_journal_print(LOG_ERR,     "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_crit(x, ...)    sd_journal_print(LOG_CRIT,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_alert(x, ...)   sd_journal_print(LOG_ALERT,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_emerg(x, ...)   sd_journal_print(LOG_EMERG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #elif defined(LLTD_USE_CONSOLE)
        static inline void lltd_console_log(int priority, const char *func, const char *fmt, ...) {
            FILE *console = fopen("/dev/console", "a");
            FILE *out = console ? console : stderr;
            va_list args;
            (void)priority;
            va_start(args, fmt);
            fprintf(out, "%s(): ", func);
            vfprintf(out, fmt, args);
            fprintf(out, "\n");
            va_end(args);
            if (console) {
                fclose(console);
            }
        }
        #define  log_debug(x, ...)   lltd_console_log(LOG_DEBUG,   __FUNCTION__, x, ##__VA_ARGS__)
        #define  log_info(x, ...)    lltd_console_log(LOG_INFO,    __FUNCTION__, x, ##__VA_ARGS__)
        #define  log_notice(x, ...)  lltd_console_log(LOG_NOTICE,  __FUNCTION__, x, ##__VA_ARGS__)
        #define  log_warning(x, ...) lltd_console_log(LOG_WARNING, __FUNCTION__, x, ##__VA_ARGS__)
        #define  log_err(x, ...)     lltd_console_log(LOG_ERR,     __FUNCTION__, x, ##__VA_ARGS__)
        #define  log_crit(x, ...)    lltd_console_log(LOG_CRIT,    __FUNCTION__, x, ##__VA_ARGS__)
        #define  log_alert(x, ...)   lltd_console_log(LOG_ALERT,   __FUNCTION__, x, ##__VA_ARGS__)
        #define  log_emerg(x, ...)   lltd_console_log(LOG_EMERG,   __FUNCTION__, x, ##__VA_ARGS__)
    #else
        #define  log_debug(x, ...)   syslog(LOG_DEBUG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_info(x, ...)    syslog(LOG_INFO,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_notice(x, ...)  syslog(LOG_NOTICE,  "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_warning(x, ...) syslog(LOG_WARNING, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_err(x, ...)     syslog(LOG_ERR,     "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_crit(x, ...)    syslog(LOG_CRIT,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_alert(x, ...)   syslog(LOG_ALERT,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
        #define  log_emerg(x, ...)   syslog(LOG_EMERG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #endif
#endif /*__linux__*/

/*
 IFNET(9)
 struct ifnethead or DevD for listing and events
 RC Script and optional RelaunchD support
 */
#ifdef __FreeBSD__
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdbool.h>
    #include <string.h>
    #include <errno.h>
    #include <signal.h>
    #include <syslog.h>
    #include "freebsd/freebsd-main.h"
    #include "freebsd/freebsd-ops.h"
    #include "lltdBlock.h"
    #include "lltdTlvOps.h"
    #include "lltdAutomata.h"
    #define log_debug(x, ...)   syslog(LOG_DEBUG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_info(x, ...)    syslog(LOG_INFO,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_notice(x, ...)  syslog(LOG_NOTICE,  "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_warning(x, ...) syslog(LOG_WARNING, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_err(x, ...)     syslog(LOG_ERR,     "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_crit(x, ...)    syslog(LOG_CRIT,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_alert(x, ...)   syslog(LOG_ALERT,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_emerg(x, ...)   syslog(LOG_EMERG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
#endif /*__FreeBSD__*/

/*
 ESXi (VMkernel userworld) - draft userland implementation.
 */
#if defined(__VMKERNEL__) || defined(__ESXI__)
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdbool.h>
    #include <string.h>
    #include <errno.h>
    #include <signal.h>
    #include <syslog.h>
    #include "esxi/esxi-main.h"
    #include "esxi/esxi-ops.h"
    #include "lltdBlock.h"
    #include "lltdTlvOps.h"
    #include "lltdAutomata.h"
    #define log_debug(x, ...)   syslog(LOG_DEBUG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_info(x, ...)    syslog(LOG_INFO,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_notice(x, ...)  syslog(LOG_NOTICE,  "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_warning(x, ...) syslog(LOG_WARNING, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_err(x, ...)     syslog(LOG_ERR,     "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_crit(x, ...)    syslog(LOG_CRIT,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_alert(x, ...)   syslog(LOG_ALERT,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_emerg(x, ...)   syslog(LOG_EMERG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
#endif

/*
 Optionally use NWAM for listing the interfaces.
 Fallback to if.h otherwise.
 Optionally use SMF.
 */
#ifdef __sunos_5_10
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdbool.h>
    #include <string.h>
    #include <errno.h>
    #include <signal.h>
    #include <syslog.h>
    #include "sunos/sunos-main.h"
    #include "sunos/sunos-ops.h"
    #include "lltdBlock.h"
    #include "lltdTlvOps.h"
    #include "lltdAutomata.h"
    #define log_debug(x, ...)   syslog(LOG_DEBUG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_info(x, ...)    syslog(LOG_INFO,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_notice(x, ...)  syslog(LOG_NOTICE,  "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_warning(x, ...) syslog(LOG_WARNING, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_err(x, ...)     syslog(LOG_ERR,     "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_crit(x, ...)    syslog(LOG_CRIT,    "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_alert(x, ...)   syslog(LOG_ALERT,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_emerg(x, ...)   syslog(LOG_EMERG,   "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
#endif /*__sunos_5_10__*/

#ifdef __BEOS__
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdbool.h>
    #include <string.h>
    #include "beos/beos-main.h"
    #include "beos/beos-ops.h"
    #include "lltdBlock.h"
    #include "lltdTlvOps.h"
    #include "lltdAutomata.h"
    #define log_debug(x, ...)   fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_info(x, ...)    fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_notice(x, ...)  fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_warning(x, ...) fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_err(x, ...)     fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_crit(x, ...)    fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_alert(x, ...)   fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_emerg(x, ...)   fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
#endif /*__BEOS__*/

/*
 Windows family (Win16/Win9x/WinNT/2000).
 */
#if defined(_WIN32) || defined(_WIN16)
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdbool.h>
    #include <string.h>
    #include "windows/windows-main.h"
    #include "windows/windows-ops.h"
    #include "lltdBlock.h"
    #include "lltdTlvOps.h"
    #include "lltdAutomata.h"
    #define log_debug(x, ...)   fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_info(x, ...)    fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_notice(x, ...)  fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_warning(x, ...) fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_err(x, ...)     fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_crit(x, ...)    fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_alert(x, ...)   fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
    #define log_emerg(x, ...)   fprintf(stderr, "%s(): " x "\n", __FUNCTION__, ##__VA_ARGS__)
#endif


#endif /* lltdDaemon_h */
