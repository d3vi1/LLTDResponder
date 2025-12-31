//
//  lltdAutomata.c
//  Automata
//
//  Created by Alex Georoceanu on 06/10/14.
//  Copyright (c) 2014 Alex Georoceanu. All rights reserved.
//

#include "lltdDaemon.h"

#if defined(ESP_PLATFORM)
#include "esp_timer.h"
#elif !defined(__APPLE__)
#include <time.h>
#endif

static uint64_t lltd_monotonic_seconds(void) {
#if defined(__APPLE__)
    return mach_absolute_time() / 1000000000ULL;
#elif defined(ESP_PLATFORM)
    return (uint64_t)(esp_timer_get_time() / 1000000ULL);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec;
#endif
}

#ifndef __enumeration_constants__
#define __enumeration_constants__       0x00
#define enum_sess_complete              0x00
#define enum_sess_not_complete          0x01
#define enum_hello                      0x02
#define enum_new_session                0x03
#endif

#pragma mark -
#pragma mark Mapping
automata* init_automata_mapping() {
    automata *autom = malloc( sizeof(automata) );
    autom->states_no = 3;
    autom->transitions_no = 13;
    autom->last_ts = lltd_monotonic_seconds();
    autom->name = "Mapping";
    
    state quiescent, command, emit;
    quiescent.name = "Quiescent";
    quiescent.timeout = 0;
    command.name = "Command";
    command.timeout = 1;
    emit.name = "Emit";
    emit.timeout = 30;
    
    autom->current_state = 0;
    
    autom->states_table[0] = quiescent;
    autom->states_table[1] = command;
    autom->states_table[2] = emit;
    
    transition *t = autom->transitions_table;
    
    t[ 0].from = 0; t[ 0].to = 1; t[ 0].with = opcode_discover;
    t[ 1].from = 1; t[ 1].to = 0; t[ 1].with = -1; // timeout
    t[ 2].from = 1; t[ 2].to = 0; t[ 2].with = opcode_reset;
    
    t[ 3].from = 1; t[ 3].to = 1; t[ 3].with = opcode_query;
    t[ 4].from = 1; t[ 4].to = 1; t[ 4].with = opcode_queryLargeTlv;
    t[ 5].from = 1; t[ 5].to = 1; t[ 5].with = opcode_charge;
    t[ 6].from = 1; t[ 6].to = 1; t[ 6].with = opcode_probe;
    
    t[ 7].from = 1; t[ 7].to = 2; t[ 7].with = opcode_emit;
    t[ 8].from = 2; t[ 8].to = 2; t[ 8].with = opcode_probe;
    t[ 9].from = 2; t[ 9].to = 2; t[ 9].with = -2; //TODO: special timeout
    t[10].from = 2; t[10].to = 1; t[10].with = -3; //TODO: special code, "DONE"
    
    t[11].from = 2; t[11].to = 0; t[11].with = -1; // timeout
    t[12].from = 2; t[12].to = 0; t[12].with = opcode_reset;
    
    return autom;
}

automata* switch_state_mapping(automata* autom, int input, char* debug) {
    uint8_t new_state = autom->current_state;
    state* current_state = &autom->states_table[ autom->current_state ];
    bool timeout = FALSE;
    
    uint64_t now = lltd_monotonic_seconds();
    uint64_t diff = (now - autom->last_ts);
    
    if (current_state->timeout != 0 && diff > current_state->timeout) {
        // timeout in effect
        timeout = TRUE;
        input = -1;
    }
    
    int find = -1;
    for (int i = 0; i < autom->transitions_no; i++) {
        if ( autom->current_state == autom->transitions_table[i].from
            && autom->transitions_table[i].with == input ) {
            new_state = autom->transitions_table[i].to;
            find = i;
        }
    }
    
    if (autom->current_state != new_state || find >= 0 || timeout) {
        log_debug("%s: Switching from %s with %d ", autom->name, current_state->name, input);
        if (timeout) log_debug("(timeout) ");
        log_debug("to %s\n", autom->states_table[new_state].name);
        
        autom->current_state = new_state;
    }
    
    autom->last_ts = now;
    
    if (timeout) {
        return switch_state_mapping(autom, input, "timeout");
    }
    
    return autom;
}

#pragma mark -
#pragma mark RepeatBand Algorithm
automata* init_automata_enumeration() {
    automata *autom = malloc( sizeof(automata) );
    autom->states_no = 3;
    autom->transitions_no = 7;
    autom->last_ts = lltd_monotonic_seconds();
    autom->name = "RepeatBand";
    
    state quiescent, pausing, wait;
    quiescent.name = "Quiescent";
    quiescent.timeout = 0;
    pausing.name = "Pausing";
    pausing.timeout = 60;
    wait.name = "Wait";
    wait.timeout = 30;
    
    autom->extra  = malloc(sizeof(band_state));
    band_state* band = (band_state*) autom->extra;
    band->begun = FALSE;
    band->Ni    = BAND_NMAX;
    band->r     = BAND_BLOCK_TIME;
    
    autom->current_state = 0;
    
    autom->states_table[0] = quiescent;
    autom->states_table[1] = pausing;
    autom->states_table[2] = wait;
    
    transition *t = autom->transitions_table;
    t[ 0].from = 0; t[ 0].to = 1; t[ 0].with = enum_sess_not_complete;
    t[ 1].from = 0; t[ 1].to = 2; t[ 1].with = enum_sess_complete;
    t[ 2].from = 1; t[ 2].to = 1; t[ 2].with = enum_sess_complete;
    t[ 3].from = 1; t[ 3].to = 1; t[ 3].with = enum_hello;
    t[ 4].from = 1; t[ 4].to = 1; t[ 4].with = enum_new_session;
    t[ 5].from = 2; t[ 5].to = 1; t[ 5].with = enum_sess_not_complete;
    t[ 6].from = 2; t[ 6].to = 2; t[ 6].with = enum_sess_complete;
    
    return autom;
}

automata* switch_state_enumeration(automata* autom, int input, char* debug) {
    uint8_t new_state = autom->current_state;
    state* current_state = &autom->states_table[ autom->current_state ];
    bool timeout = FALSE;
    
    uint64_t now = lltd_monotonic_seconds();
    uint64_t diff = (now - autom->last_ts);
    
    
    int find = -1;
    for (int i = 0; i < autom->transitions_no; i++) {
        if ( autom->current_state == autom->transitions_table[i].from
            && autom->transitions_table[i].with == input ) {
            new_state = autom->transitions_table[i].to;
            find = i;
        }
    }
    
    if (new_state != autom->current_state || find >= 0) {
        log_debug("%s: Switching from %s with %d (%s) ", autom->name, current_state->name, input, debug);
        log_debug("to %s\n", autom->states_table[new_state].name);
        
        autom->current_state = new_state;
    }
    
    autom->last_ts = now;
    return autom;
}

#pragma mark -
#pragma mark Session
automata* init_automata_session() {
    automata *autom = malloc( sizeof(automata) );
    autom->states_no = 4;
    autom->transitions_no = 16;
    autom->last_ts = lltd_monotonic_seconds();
    autom->name = "Sesssion";
    
    state nascent, pending, temporary, complete;
    nascent.name = "Nascent";
    nascent.timeout = 1;
    temporary.name = "Temporary";
    temporary.timeout = 1;
    complete.name = "Complete";
    complete.timeout = 1;
    pending.name = "Pending";
    pending.timeout = 1;
    
    autom->current_state = 0;
    
    autom->states_table[0] = temporary;
    autom->states_table[1] = nascent;
    autom->states_table[2] = pending;
    autom->states_table[3] = complete;
    
    transition *t = autom->transitions_table;
    t[ 0].from = 1; t[ 0].to = 0; t[ 0].with = sess_discover_conflicting;
    t[ 1].from = 0; t[ 1].to = 1; t[ 1].with = -1; // timeout
    t[ 2].from = 0; t[ 2].to = 1; t[ 2].with = sess_reset;
    t[ 3].from = 0; t[ 3].to = 1; t[ 3].with = sess_hello;
    t[ 4].from = 0; t[ 4].to = 1; t[ 4].with = sess_topo_reset;
    t[ 5].from = 1; t[ 5].to = 2; t[ 5].with = sess_discover_noack;
    t[ 6].from = 2; t[ 6].to = 1; t[ 6].with = -1; // timeout
    t[ 7].from = 2; t[ 7].to = 1; t[ 7].with = sess_reset;
    t[ 8].from = 2; t[ 8].to = 2; t[ 8].with = sess_discover_noack_chgd_xid;
    t[ 9].from = 2; t[ 9].to = 3; t[ 9].with = sess_discover_acking;
    t[10].from = 2; t[10].to = 3; t[10].with = sess_discover_acking_chgd_xid;
    t[11].from = 1; t[11].to = 3; t[11].with = sess_discover_acking;
    t[12].from = 3; t[12].to = 1; t[12].with = opcode_reset;
    t[13].from = 3; t[13].to = 1; t[13].with = -1; // timeout
    t[14].from = 3; t[14].to = 3; t[14].with = sess_discover_acking_chgd_xid;
    t[15].from = 3; t[15].to = 2; t[15].with = sess_discover_noack_chgd_xid;
    
    return autom;
}

automata* switch_state_session(automata* autom, int input, char* debug) {
    uint8_t new_state = autom->current_state;
    state* current_state = &autom->states_table[ autom->current_state ];
    bool timeout = FALSE;
    
    uint64_t now = lltd_monotonic_seconds();
    uint64_t diff = (now - autom->last_ts);
    
    if (current_state->timeout != 0 && diff > current_state->timeout) {
        // timeout in effect
        timeout = TRUE;
        input = -1;
    }
    
    int find = -1;
    for (int i = 0; i < autom->transitions_no; i++) {
        if ( autom->current_state == autom->transitions_table[i].from
            && autom->transitions_table[i].with == input ) {
            new_state = autom->transitions_table[i].to;
            find = i;
        }
    }
    
    if (autom->current_state != new_state || find >= 0 || timeout) {
        log_debug("%s: Switching from %s with %d (%s) ", autom->name, current_state->name, input, debug);
        if (timeout) log_debug("(timeout) ");
        log_debug("to %s\n", autom->states_table[new_state].name);
        
        autom->current_state = new_state;
    }
    
    autom->last_ts = now;
    
    if (timeout) {
        return switch_state_mapping(autom, input, "timeout");
    }
    
    return autom;
}
