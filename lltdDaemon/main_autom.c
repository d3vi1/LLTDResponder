//
//  main_autom.c
//  Automata
//
//  Created by Alex Georoceanu on 06/10/14.
//  Copyright (c) 2014 Alex Georoceanu. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include "lltdAutomata.c"


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
    
    /*
     for (int i = 0; i < en->transitions_no; i++) {
     transition tr = en->transitions_table[i];
     printf("%d -> %d with %d\n", tr.from, tr.to, tr.with);
     }
     */
    
    /*
     switch_state_mapping(en, opcode_ack, "ack");
     switch_state_mapping(en, opcode_charge, "charge");
     switch_state_mapping(en, opcode_discover, "disc");
     
     switch_state_mapping(en, opcode_reset, "reset");
     switch_state_mapping(en, opcode_discover, "discover");
     
     sleep(2);
     switch_state_mapping(en, opcode_query, "query");
     switch_state_mapping(en, opcode_queryLargeTlv, "qryLrg");
     switch_state_mapping(en, opcode_charge, "charge");
     
     switch_state_mapping(en, opcode_emit, "emit");
     switch_state_mapping(en, opcode_charge, "charge");
     switch_state_mapping(en, -2, "-2");
     switch_state_mapping(en, -3, "-3");
     
     switch_state_mapping(en, opcode_reset, "reset");
     
     */
    
    automata *st = init_automata_session();
    switch_state_session(st, sess_hello, "sess_hello");
    switch_state_session(st, opcode_reset, "opcode_reset");
    switch_state_session(st, sess_hello, "sess_hello");
    switch_state_session(st, sess_discover_noack, "sess_discover_noack");
    switch_state_session(st, sess_discover_acking, "sess_discover_acking");
    switch_state_session(st, sess_discover_acking_chgd_xid, "sess_discover_acking_chgd_xid");
    sleep(2);
    switch_state_session(st, opcode_reset, "opcode_reset");
    switch_state_session(st, sess_discover_conflicting, "sess_discover_conflicting");
    sleep(2);
    switch_state_session(st, sess_discover_acking_chgd_xid, "sess_discover_acking_chgd_xid");
    
    
    automata *se = init_automata_enumeration();
    switch_state_enumeration(se, enum_sess_not_complete, "enum_sess_not_complete");
    switch_state_enumeration(se, enum_new_session, "enum_new_session");
    switch_state_enumeration(se, enum_sess_complete, "enum_sess_complete");
    switch_state_enumeration(se, enum_sess_not_complete, "enum_sess_not_complete");
    switch_state_enumeration(se, enum_hello, "enum_hello");
    
    return 0;
}
