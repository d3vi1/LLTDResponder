//
//  see-list.c
//  LLMNRd
//
//  Created by Alex Georoceanu on 24/05/14.
//  Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.
//
/*
#include "see-list.h"
#include <stdio.h>
#include <stdlib.h>
#include <CoreFoundation/CFArray.h>

int main(int argc, const char * argv[]) {
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, NULL);
    
    int createCount = 10;
    probe_t* array = malloc( createCount *  sizeof *array);
    
    for(int i = 0; i < createCount; i++) {
        array[i].type = i+1;
        CFArrayAppendValue(list, &array[i]);
    }
    
    long count = CFArrayGetCount(list);
    CFRange rangeAll = CFRangeMake(0, count);
    
    const void* values[count];
    CFArrayGetValues( list, rangeAll, values);
    
    CFArrayRemoveAllValues(list);
    
    CFRelease(list);
    
    for (long i = 0; i < count; i++) {
        probe_t *p = values[i];
        printf("Let's see the array in action: val = %d \n", p->type);
    }

    
    return 0;
} */