//
//  main.c
//  Automata
//
//  Created by Alex Georoceanu on 06/10/14.
//  Copyright (c) 2014 Alex Georoceanu. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include "automata.c"


int main(int argc, const char * argv[]) {
    /*
    automata *automat = init_automata_sample();
    switch_state_sample(automat, 1);
    sleep(3);
    switch_state_sample(automat, 3);
    switch_state_sample(automat, 1);
    sleep(5);
    switch_state_sample(automat, 2);
    */
    
    automata *en = init_automata_mapping();
    switch_state_mapping(en, opcode_ack);
    switch_state_mapping(en, opcode_charge);
    switch_state_mapping(en, opcode_discover);
    
    switch_state_mapping(en, opcode_reset);
    switch_state_mapping(en, opcode_discover);
    
    switch_state_mapping(en, opcode_query);
    switch_state_mapping(en, opcode_queryLargeTlv);
    switch_state_mapping(en, opcode_charge);
    
    switch_state_mapping(en, opcode_emit);
    switch_state_mapping(en, opcode_charge);
    switch_state_mapping(en, -2);
    switch_state_mapping(en, -3);
    
    switch_state_mapping(en, opcode_reset);
    
    return 0;
}
