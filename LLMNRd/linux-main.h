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

int MessageHandler(struct sockaddr_nl NetLinkAddr, struct nlmsghdr *Message)
int ReadEvent     (int NetLinkSock, int (*msg_handler)(struct sockaddr_nl *, struct nlmsghdr *))

#endif /* linux_main_h */
