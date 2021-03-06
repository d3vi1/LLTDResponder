/******************************************************************************
 *                                                                            *
 *   linux-main.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 09.02.2016.                      *
 *   Copyright © 2016 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 *   http://iijean.blogspot.ro/2010/03/                                       *
 *                         howto-get-list-of-network-interfaces-in.html       *
 *   http://stackoverflow.com/questions/27008067/how-to-get-notified-about-n  *
 *                      etwork-interface-changes-with-netlist-and-rtmgrp-link *
 *   http://www.thegeekstuff.com/2012/03/catch-signals-sample-c-code/         *
 *   https://github.com/ia/connect/blob/master/samples/netif/epoll-linux_1.c  *
 *   http://bitsup.blogspot.ro/2008/04/monitoring-ip-changes-with-netlink.html*
 *                                                                            *
 ******************************************************************************/

#include "lltdDaemon.h"

static int exit_flag = 0;

void SignalHandler(int Signal) {
    log_debug("Interrupted by signal #%d", Signal);
    exit_flag = 1;
}

//void lltdLoop (void *data);

//void validateInterface(void *data);

static int deviceNotification(struct sockaddr_nl *NetLinkAddr, struct nlmsghdr *Message){
    switch (Message->nlmsg_type) {
        case RTM_NEWADDR:
            log_debug("RTM_NEWADDR\n");
            break;
        case RTM_DELADDR:
            log_debug("RTM_DELADDR\n");
            break;
        case RTM_NEWROUTE:
            log_debug("RTM_NEWROUTE\n");
            break;
        case RTM_DELROUTE:
            log_debug("RTM_DELROUTE\n");
            break;
        case RTM_NEWLINK:
            log_debug("RTM_NEWLINK\n");
            break;
        case RTM_DELLINK:
            log_debug("RTM_DELLINK\n");
            break;
        default:
            log_err("Unknown Netlink Message Type %d\n", Message->nlmsg_type);
            break;
    }
}

int deviceAppeared(int NetLinkSock, int (*msg_handler)(struct sockaddr_nl *, struct nlmsghdr *)){
    int    status;
    int    ret = 0;
    char   buf[4096];
    struct iovec iov = { buf, sizeof buf };
    struct sockaddr_nl snl;
    struct msghdr msg = { (void*)&snl, sizeof snl, &iov, 1, NULL, 0, 0};
    struct nlmsghdr *h;
    
    status = recvmsg(NetLinkSock, &msg, 0);
    
    if(status < 0) {
        /* Socket non-blocking so bail out once we have read everything */
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return ret;
        
        /* Anything else is an error */
        log_err("read_netlink: Error recvmsg: %d\n", status);
        return status;
    }
    
    if(status == 0) log_debug("read_netlink: EOF\n");
    
    /* We need to handle more than one message per 'recvmsg' */
    for(h = (struct nlmsghdr *) buf; NLMSG_OK (h, (unsigned int)status); h = NLMSG_NEXT (h, status)){
        /* Finish reading */
        if (h->nlmsg_type == NLMSG_DONE) return ret;
        
        /* Message is some kind of error */
        if (h->nlmsg_type == NLMSG_ERROR) {
            log_err("read_netlink: Message is an error - decode TBD");
            return -1; // Error
        }
        
        /* Call message handler */
        if(msg_handler) {
            ret = (*msg_handler)(&snl, h);
            if(ret < 0) {
                log_err("read_netlink: Message hander error %d", ret);
                return ret;
            }
        } else {
            log_err("read_netlink: Error NULL message handler");
            return -1;
        }
    }    
    return ret;
}

int main (int argc, const char *argv[]){
    //
    // Open the syslog handler
    //

    openlog (NULL, LOG_PID, LOG_DAEMON);
    
    
    //
    // Setup the signal handlers
    //
    if(signal(SIGINT,  SignalHandler) == SIG_ERR) log_err("ERROR: Could not establish SIGINT handler." );
    if(signal(SIGTERM, SignalHandler) == SIG_ERR) log_err("ERROR: Could not establish SIGTERM handler.");
    
    
    //
    // Open the notification port
    //
    int NetLinkSock = socket(AF_NETLINK,SOCK_RAW,NETLINK_ROUTE);
    struct sockaddr_nl NetLinkAddr;
    memset((void*)&NetLinkAddr, 0, sizeof(NetLinkAddr));
    
    if (NetLinkSock < 0){
        log_err("Open Error!");
        return -1;
    }
    
    NetLinkAddr.nl_family = AF_NETLINK;
    NetLinkAddr.nl_pid = getpid();
    NetLinkAddr.nl_groups = RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR;
    
    log_debug("Opening the notification socket!");

    if (bind(NetLinkSock,(struct sockaddr *)&NetLinkAddr, sizeof(NetLinkAddr)) < 0){
        log_err("Open Error!");
        return -1;
    }
    
    
    //
    // Start the runloop
    //
    while (!exit_flag) deviceAppeared(NetLinkSock, deviceNotification);

    log_debug("And we're out");
    closelog();
    return 0;
}
