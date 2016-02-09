//
//  automata.c
//  Automata
//
//  Created by Alex Georoceanu on 06/10/14.
//  Copyright (c) 2014 Alex Georoceanu. All rights reserved.
//

#include "lltdAutomata.h"

automata* init_automata_mapping() {
    automata *autom = malloc( sizeof(automata) );
    autom->states_no = 3;
    autom->transitions_no = 13;
    autom->last_ts = mach_absolute_time() / 1e9;
    
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

//TODO: when timeout arrives, process it and process also the input arriving next
automata* switch_state_mapping(automata* autom, int input, char* debug) {
    uint8_t new_state = autom->current_state;
    state* current_state = &autom->states_table[ autom->current_state ];
    bool timeout = FALSE;
    
    uint64_t now = mach_absolute_time() / 1e9;
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
        printf("Switching from %s with %d ", current_state->name, input);
        if (timeout) printf("(timeout) ");
        printf("to %s\n", autom->states_table[new_state].name);
        
        autom->current_state = new_state;
    }

    autom->last_ts = now;
    
    if (timeout) {
        return switch_state_mapping(autom, input, "timeout");
    }

    return autom;
}

//NOTE: this is actually RepeatBand Algorithm!
automata* init_automata_enumeration() {
    automata *autom = malloc( sizeof(automata) );
    autom->states_no = 3;
    autom->transitions_no = 13;
    autom->last_ts = mach_absolute_time() / 1e9;
    
    state quiescent, pausing, wait;
    quiescent.name = "Quiescent";
    quiescent.timeout = 0;
    pausing.name = "Pausing";
    pausing.timeout = 60;
    wait.name = "Wait";
    wait.timeout = 30;
    
    autom->current_state = 0;
    
    autom->states_table[0] = quiescent;
    autom->states_table[1] = pausing;
    autom->states_table[2] = wait;
    
    transition *t = autom->transitions_table;
    
    return autom;
}