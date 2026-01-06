//
//  main_autom.c
//  Automata
//
//  Created by Alex Georoceanu on 06/10/14.
//  Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.
//  Copyright © 2014 Alex GEOROCEANU. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include "lltdAutomata.c"


int main(int argc, const char * argv[]) {
    automata *en = init_automata_mapping();
    automata *st = init_automata_session();
    switch_state_session(st, sess_hello, "sess_hello");
    switch_state_session(st, opcode_reset, "opcode_reset");
    switch_state_session(st, sess_hello, "sess_hello");
    switch_state_session(st, sess_discover_noack, "sess_discover_noack");
    switch_state_session(st, sess_discover_acking, "sess_discover_acking");
    switch_state_session(st, sess_discover_acking_chgd_xid, "sess_discover_acking_chgd_xid");
    printf("Preparing to sleep 2s\n");
    sleep(2);
    printf("Slept 2s\n");
    switch_state_session(st, opcode_reset, "opcode_reset");
    switch_state_session(st, sess_discover_conflicting, "sess_discover_conflicting");
    printf("Preparing to sleep another 2s\n");
    sleep(2);
    printf("Slept another 2s\n");
    switch_state_session(st, sess_discover_acking_chgd_xid, "sess_discover_acking_chgd_xid");
    
    
    automata *se = init_automata_enumeration();
    switch_state_enumeration(se, enum_sess_not_complete, "enum_sess_not_complete");
    switch_state_enumeration(se, enum_new_session, "enum_new_session");
    switch_state_enumeration(se, enum_sess_complete, "enum_sess_complete");
    switch_state_enumeration(se, enum_sess_not_complete, "enum_sess_not_complete");
    switch_state_enumeration(se, enum_hello, "enum_hello");
    
    return 0;
}
