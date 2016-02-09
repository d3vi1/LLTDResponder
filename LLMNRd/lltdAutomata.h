//
//  automata.h
//  Automata
//
//  Created by Alex Georoceanu on 06/10/14.
//  Copyright (c) 2014 Alex Georoceanu. All rights reserved.
//

#ifndef __LLTDd__automata__
#define __LLTDd__automata__

#define MAX_STATES 5 // LLTD protocol only needs 4 maximum
#define MAX_TRANSITIONS 128

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

#ifndef __opcode_constants__

#define opcode_discover           0x00
#define opcode_hello              0x01
#define opcode_emit               0x02
#define opcode_train              0x03
#define opcode_probe              0x04
#define opcode_ack                0x05
#define opcode_query              0x06
#define opcode_queryResp          0x07
#define opcode_reset              0x08
#define opcode_charge             0x09
#define opcode_flat               0x0A
#define opcode_queryLargeTlv      0x0B
#define opcode_queryLargeTlvResp  0x0C
#define opcode_qosInitializeSink  0x00
#define opcode_qosReady           0x01
#define opcode_qosProbe           0x02
#define opcode_qosQuery           0x03
#define opcode_qosQueryResp       0x04
#define opcode_qosReset           0x05
#define opcode_qosError           0x06
#define opcode_qosAck             0x07
#define opcode_qosCounterSnapshot 0x08
#define opcode_qosCounterResult   0x09
#define opcode_qosCounterLease    0x0A

#endif


typedef struct state {
    short       timeout; //timeout in seconds
    char       *name;
} state;

typedef struct transition {
    uint8_t     from;
    uint8_t     to;
    int8_t      with;
} transition;

typedef struct automata {
    uint64_t    last_ts;
    uint8_t     current_state;
    transition  transitions_table[MAX_TRANSITIONS];
    uint8_t     transitions_no;
    state       states_table[MAX_STATES];
    uint8_t     states_no;
} automata;

automata* init_automata_mapping(void);
automata* switch_state_mapping(automata*, int, char*);

automata* init_automata_enumeration(void);
automata* switch_state_enumeration(automata*, int);

#endif /* defined(__LLTDd__automata__) */
