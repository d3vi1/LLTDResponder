#ifndef LLTD_RESPONDER_AUTOMATA_H
#define LLTD_RESPONDER_AUTOMATA_H

#include <stdbool.h>
#include <stdint.h>

#include "lltdProtocol.h"

#define MAX_STATES 5 // LLTD protocol only needs 4 maximum
#define MAX_TRANSITIONS 128

#ifndef __enumeration_constants__
#define __enumeration_constants__ 0x00
#define enum_sess_complete        0x00
#define enum_sess_not_complete    0x01
#define enum_hello                0x02
#define enum_new_session          0x03
#endif

// RepeatBand Constants (milliseconds)
#define BAND_NMAX 10000
#define BAND_ALPHA 45
#define BAND_BETA 2
#define BAND_GAMMA 10
#define BAND_TXC 4
#define BAND_FRAME_TIME 6.667
#define BAND_BLOCK_TIME 300
#define HELLO_MIN_INTERVAL_MS 1000
// Want 6.67 ms per frame. Equals 20/3.
#define BAND_MUL_FRAME(_x) (((_x) * 20) / 3)

typedef struct state {
    short timeout; // timeout in seconds
    char *name;
} state;

typedef struct transition {
    uint8_t from;
    uint8_t to;
    int8_t  with;
} transition;

typedef struct automata {
    uint64_t   last_ts;
    uint8_t    current_state;
    transition transitions_table[MAX_TRANSITIONS];
    uint8_t    transitions_no;
    state      states_table[MAX_STATES];
    uint8_t    states_no;
    char      *name;
    void      *extra;
} automata;

typedef struct band_state {
    uint32_t Ni;
    uint32_t r;
    bool     begun;
    uint64_t hello_timeout_ts; // Next hello timeout timestamp (milliseconds)
    uint64_t block_timeout_ts; // Next block timeout timestamp (milliseconds)
} band_state;

// Session table entry - keyed by mapper MAC + generation/seq
#define SESSION_TABLE_MAX_ENTRIES 16

typedef struct session_entry {
    uint8_t  mapper_mac[6];
    uint16_t generation;
    uint16_t seq_number;
    uint8_t  state;
    bool     complete;
    bool     valid;
    uint64_t last_activity_ts;
    uint64_t created_ts;
} session_entry;

typedef struct session_table {
    session_entry entries[SESSION_TABLE_MAX_ENTRIES];
    uint8_t       count;
    bool          all_complete;
} session_table;

// Session state constants
#define sess_discover_conflicting     0x00
#define sess_reset                    0x01
#define sess_discover_noack           0x02
#define sess_discover_acking          0x03
#define sess_discover_noack_chgd_xid  0x04
#define sess_discover_acking_chgd_xid 0x05
#define sess_topo_reset               0x06
#define sess_hello                    0x07

typedef struct mapping_state {
    uint8_t  ctc;
    uint64_t charge_timeout_ts;
    uint64_t inactive_timeout_ts;
} mapping_state;

typedef struct lltd_automata_tick_port {
    void    *network_interface;
    uint64_t *last_hello_tx_ms;
    void   (*send_hello)(void *network_interface);
} lltd_automata_tick_port;

automata *init_automata_mapping(void);
automata *switch_state_mapping(automata *autom, int input, char *debug);

automata *init_automata_enumeration(void);
automata *switch_state_enumeration(automata *autom, int input, char *debug);

automata *init_automata_session(void);
automata *switch_state_session(automata *autom, int input, char *debug);

session_table *session_table_create(void);
void session_table_destroy(session_table *table);
session_entry *session_table_find(session_table *table, const uint8_t *mapper_mac, uint16_t generation, uint16_t seq);
session_entry *session_table_add(session_table *table, const uint8_t *mapper_mac, uint16_t generation, uint16_t seq);
void session_table_remove(session_table *table, const uint8_t *mapper_mac, uint16_t generation);
void session_table_update_complete_status(session_table *table);
bool session_table_is_empty(session_table *table);
bool session_table_all_complete(session_table *table);
void session_table_clear(session_table *table);

int derive_session_event(const void *frame, session_table *table, const uint8_t *our_mac);

void band_init_stats(band_state *band);
void band_update_stats(band_state *band);
uint64_t band_choose_hello_time(band_state *band);
void band_do_hello(band_state *band);
void band_on_hello_received(band_state *band);

void mapping_reset_charge(mapping_state *mstate);
void mapping_on_charge(mapping_state *mstate);
bool mapping_check_charge_timeout(mapping_state *mstate);
bool mapping_check_inactive_timeout(mapping_state *mstate);
void mapping_reset_inactive_timeout(mapping_state *mstate);

void automata_tick(automata *mapping,
                   automata *enumeration,
                   session_table *sessions,
                   const lltd_automata_tick_port *port);

uint64_t lltd_monotonic_seconds(void);
uint64_t lltd_monotonic_milliseconds(void);

#endif
