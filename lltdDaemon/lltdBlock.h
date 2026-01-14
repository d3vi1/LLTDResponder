/******************************************************************************
 *                                                                            *
 *   lltdBlock.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 23.03.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDBlock_h
#define LLTDBlock_h

#include "../lltdResponder/lltdProtocol.h"

void lltdBlock(void *data);
void parseFrame(void *frame, void *networkInterface);

#endif /* defined(LLTDBlock_h) */
