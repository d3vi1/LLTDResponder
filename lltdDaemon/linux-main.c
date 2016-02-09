/******************************************************************************
 *                                                                            *
 *   linux-main.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 09.02.2016.                      *
 *   Copyright © 2016 Răzvan Corneliu C.R. VILT. All rights reserved.         *
 *                                                                            *
 ******************************************************************************/

#include "lltdDaemon.h"


int MessageHandler(struct sockaddr_nl NetLinkAddr, struct nlmsghdr *Message){
    switch (Message->nlmsg_type) {
        case RTM_NEWADDR:
            printf("MessageHandler: RTM_NEWADDR\n");
            break;
        case RTM_DELADDR:
            printf("MessageHandler: RTM_DELADDR\n");
            break;
        case RTM_NEWROUTE:
            printf("MessageHandler: RTM_NEWROUTE\n");
            break;
        case RTM_DELROUTE:
            printf("MessageHandler: RTM_DELROUTE\n");
            break;
        case RTM_NEWLINK:
            printf("MessageHandler: RTM_NEWLINK\n");
            break;
        case RTM_DELLINK:
            printf("MessageHandler: RTM_DELLINK\n");
            break;
        default:
            printf("MessageHandler: Unknown Netlink Message Type %d\n", Message->nlmsg_type);
            break;
    }
}

int ReadEvent(int NetLinkSock, int (*msg_handler)(struct sockaddr_nl *, struct nlmsghdr *)){
    int status;
    int ret = 0;
    char buf[4096];
    struct iovec iov = { buf, sizeof buf };
    struct sockaddr_nl snl;
    struct msghdr msg = { (void*)&snl, sizeof snl, &iov, 1, NULL, 0, 0};
    struct nlmsghdr *h;
    
    status = recvmsg(NetLinkSock, &msg, 0);
    
    if(status < 0)
    {
        /* Socket non-blocking so bail out once we have read everything */
        if (errno == EWOULDBLOCK || errno == EAGAIN)
            return ret;
        
        /* Anything else is an error */
        printf("read_netlink: Error recvmsg: %d\n", status);
        perror("read_netlink: Error: ");
        return status;
    }
    
    if(status == 0)
    {
        printf("read_netlink: EOF\n");
    }
    
    /* We need to handle more than one message per 'recvmsg' */
    for(h = (struct nlmsghdr *) buf; NLMSG_OK (h, (unsigned int)status);
        h = NLMSG_NEXT (h, status))
    {
        /* Finish reading */
        if (h->nlmsg_type == NLMSG_DONE)
            return ret;
        
        /* Message is some kind of error */
        if (h->nlmsg_type == NLMSG_ERROR)
        {
            printf("read_netlink: Message is an error - decode TBD\n");
            return -1; // Error
        }
        
        /* Call message handler */
        if(msg_handler)
        {
            ret = (*msg_handler)(&snl, h);
            if(ret < 0)
            {
                printf("read_netlink: Message hander error %d\n", ret);
                return ret;
            }
        }
        else
        {
            printf("read_netlink: Error NULL message handler\n");
            return -1;
        }
    }
    
    
    
    return ret;
}

int main (int argc, const char *argv[]){
    //
    // Setup the signal handlers
    //
    handler = signal(SIGINT, SignalHandler);
    if (handler == SIG_ERR) printf(stderr,"ERROR: Could not establish SIGINT handler.\n");
    handler = signal(SIGTERM, SignalHandler);
    if (handler == SIG_ERR) printf(stderr,"ERROR: Could not establish SIGTERM handler.\n");
    
    //
    //TODO: Add systemd notification
    //
    
    
    //
    //TODO: Use syslog() instead of printf
    //
    
    
    //
    // Open the notification port
    //
    int NetLinkSock = socket(AF_NETLINK,SOCK_RAW,NETLINK_ROUTE);
    struct sockaddr_nl NetLinkAddr;
    memset((void*)&NetLinkAddr, 0, sizeof(NetLinkAddr));
    
    if (NetLinkSock < 0){
        printf(stderr,"Open Error!\n");
        return -1;
    }
    
    NetLinkAddr.nl_family = AF_NETLINK;
    NetLinkAddr.nl_pid = getpid():
    NetLinkAddr.nl_groups = RTMGRP_LINK|RTMGRP_IPV4_IFADDR|RTMGRP_IPV6_IFADDR;
    
    if (bind(NetLinkSock,(struct sockaddr *)&NetLinkAddr, sizeof(NetLinkAddr)) < 0)){
        printf(stderr,"Open Error!\n");
        return -1;
    }
    
    
    //
    // Start the runloop
    //
    while (true) read_event (NetLinkSock, MessageHandler);
    
    return 0;
}