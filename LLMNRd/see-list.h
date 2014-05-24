//
//  see-list.h
//  LLMNRd
//
//  Created by Alex Georoceanu on 24/05/14.
//  Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.
//

#ifndef LLMNRd_see_list_h
#define LLMNRd_see_list_h
//#include "lltdBlock.h"

typedef struct {
    void *      contents;
    int         length;
} see_list;


typedef struct {
    int  type;
    char realSourceAddr[6];
    char sourceAddr[6];
    char destAddr[6];
} probe_t;

#endif
