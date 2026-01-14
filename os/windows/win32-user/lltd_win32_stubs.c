/******************************************************************************
 *                                                                            *
 *   lltd_win32_stubs.c                                                       *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"

void lltdBlock(void *data) {
    (void)data;
}

void parseFrame(void *frame, void *networkInterface) {
    (void)frame;
    (void)networkInterface;
}

void sendHelloMessage(void *networkInterface) {
    (void)networkInterface;
}
