//
//  lltdAutomata.c
//  Automata
//
//  Created by Alex Georoceanu on 06/10/14.
//  Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.
//  Copyright © 2014 Alex GEOROCEANU. All rights reserved.
//

#include "lltdDaemon.h"

#if defined(ESP_PLATFORM)
#include "esp_timer.h"
#elif defined(_WIN32)
#include <windows.h>
#elif !defined(__APPLE__)
#include <time.h>
#endif

uint64_t lltd_monotonic_seconds(void) {
#if defined(__APPLE__)
    static mach_timebase_info_data_t timebase = {0, 0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    uint64_t now = mach_absolute_time();
    // Convert to nanoseconds then to seconds
    return (now * timebase.numer / timebase.denom) / 1000000000ULL;
#elif defined(ESP_PLATFORM)
    return (uint64_t)(esp_timer_get_time() / 1000000ULL);
#elif defined(_WIN32)
    return (uint64_t)(GetTickCount64() / 1000ULL);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec;
#endif
}

uint64_t lltd_monotonic_milliseconds(void) {
#if defined(__APPLE__)
    static mach_timebase_info_data_t timebase = {0, 0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    uint64_t now = mach_absolute_time();
    // Convert to nanoseconds then to milliseconds
    return (now * timebase.numer / timebase.denom) / 1000000ULL;
#elif defined(ESP_PLATFORM)
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
#elif defined(_WIN32)
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
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

    // Initialize mapping state for charge/inactive timeout tracking
    mapping_state* mstate = malloc(sizeof(mapping_state));
    if (mstate) {
        mstate->ctc = 0;
        mstate->charge_timeout_ts = 0;
        mstate->inactive_timeout_ts = 0;
    }
    autom->extra = mstate;

    state quiescent, command, emit;
    quiescent.name = "Quiescent";
    quiescent.timeout = 0;
    command.name = "Command";
    command.timeout = 5;  // Allow 5 seconds between frames during mapping
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
    bool timeout = false;
    
    uint64_t now = lltd_monotonic_seconds();
    uint64_t diff = (now - autom->last_ts);
    
    if (current_state->timeout != 0 && diff > current_state->timeout) {
        // timeout in effect
        timeout = true;
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
    autom->transitions_no = 8;
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
    band->begun = false;
    band->Ni    = BAND_ALPHA;  // Start with ALPHA, not NMAX
    band->r     = 0;           // No hellos received yet
    band->hello_timeout_ts = 0;
    band->block_timeout_ts = 0;
    
    autom->current_state = 0;
    
    autom->states_table[0] = quiescent;
    autom->states_table[1] = pausing;
    autom->states_table[2] = wait;
    
    transition *t = autom->transitions_table;
    t[ 0].from = 0; t[ 0].to = 1; t[ 0].with = enum_sess_not_complete;  // Quiescent -> Pausing
    t[ 1].from = 0; t[ 1].to = 1; t[ 1].with = enum_new_session;        // Quiescent -> Pausing (on Discover)
    // When all sessions are complete, stop emitting Hello frames.
    // Park in Wait so we can quickly re-enter Pausing on a new session.
    t[ 2].from = 1; t[ 2].to = 2; t[ 2].with = enum_sess_complete;      // Pausing -> Wait (sessions complete)
    t[ 3].from = 1; t[ 3].to = 1; t[ 3].with = enum_hello;              // Pausing -> Pausing (on Hello)
    t[ 4].from = 1; t[ 4].to = 1; t[ 4].with = enum_new_session;        // Pausing -> Pausing
    t[ 5].from = 2; t[ 5].to = 1; t[ 5].with = enum_sess_not_complete;  // Wait -> Pausing
    t[ 6].from = 2; t[ 6].to = 0; t[ 6].with = enum_sess_complete;      // Wait -> Quiescent (table empty)
    t[ 7].from = 2; t[ 7].to = 1; t[ 7].with = enum_new_session;        // Wait -> Pausing (new session)

    return autom;
}

automata* switch_state_enumeration(automata* autom, int input, char* debug) {
    uint8_t new_state = autom->current_state;
    state* current_state = &autom->states_table[ autom->current_state ];
    bool timeout = false;
    
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
    autom->name = "Session";
    autom->extra = NULL;

    state nascent, pending, temporary, complete;
    nascent.name = "Nascent";
    nascent.timeout = 1;
    temporary.name = "Temporary";
    temporary.timeout = 1;
    complete.name = "Complete";
    complete.timeout = 1;
    pending.name = "Pending";
    pending.timeout = 1;

    // Session automaton starts in Nascent state (index 1)
    autom->current_state = 1;

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
    bool timeout = false;

    uint64_t now = lltd_monotonic_seconds();
    uint64_t diff = (now - autom->last_ts);

    if (current_state->timeout != 0 && diff > current_state->timeout) {
        // timeout in effect
        timeout = true;
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
        // Recursively process the timeout transition within session automaton
        return switch_state_session(autom, input, "timeout");
    }

    return autom;
}

#pragma mark -
#pragma mark Session Table Management

static bool mac_equal(const uint8_t* a, const uint8_t* b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] &&
           a[3] == b[3] && a[4] == b[4] && a[5] == b[5];
}

static void mac_copy(uint8_t* dst, const uint8_t* src) {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
    dst[3] = src[3]; dst[4] = src[4]; dst[5] = src[5];
}

session_table* session_table_create(void) {
    session_table* table = malloc(sizeof(session_table));
    if (table) {
        memset(table, 0, sizeof(session_table));
        table->count = 0;
        table->all_complete = true;  // Empty table is considered "all complete"
    }
    return table;
}

void session_table_destroy(session_table* table) {
    if (table) {
        free(table);
    }
}

session_entry* session_table_find(session_table* table, const uint8_t* mapper_mac, uint16_t generation, uint16_t seq) {
    if (!table || !mapper_mac) return NULL;

    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        session_entry* entry = &table->entries[i];
        if (entry->valid &&
            mac_equal(entry->mapper_mac, mapper_mac) &&
            entry->generation == generation) {
            // Found matching session by mapper + generation
            // Seq number can change within a session
            return entry;
        }
    }
    return NULL;
}

session_entry* session_table_add(session_table* table, const uint8_t* mapper_mac, uint16_t generation, uint16_t seq) {
    if (!table || !mapper_mac) return NULL;

    // First check if entry already exists
    session_entry* existing = session_table_find(table, mapper_mac, generation, seq);
    if (existing) {
        existing->seq_number = seq;
        existing->last_activity_ts = lltd_monotonic_seconds();
        return existing;
    }

    // Find a free slot
    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        session_entry* entry = &table->entries[i];
        if (!entry->valid) {
            mac_copy(entry->mapper_mac, mapper_mac);
            entry->generation = generation;
            entry->seq_number = seq;
            entry->state = sess_discover_noack;  // Initial state
            entry->complete = false;
            entry->valid = true;
            entry->created_ts = lltd_monotonic_seconds();
            entry->last_activity_ts = entry->created_ts;
            table->count++;
            table->all_complete = false;
            return entry;
        }
    }

    // Table is full - evict oldest entry
    uint64_t oldest_ts = UINT64_MAX;
    int oldest_idx = 0;
    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        if (table->entries[i].last_activity_ts < oldest_ts) {
            oldest_ts = table->entries[i].last_activity_ts;
            oldest_idx = i;
        }
    }

    session_entry* entry = &table->entries[oldest_idx];
    mac_copy(entry->mapper_mac, mapper_mac);
    entry->generation = generation;
    entry->seq_number = seq;
    entry->state = sess_discover_noack;
    entry->complete = false;
    entry->valid = true;
    entry->created_ts = lltd_monotonic_seconds();
    entry->last_activity_ts = entry->created_ts;
    table->all_complete = false;
    return entry;
}

void session_table_remove(session_table* table, const uint8_t* mapper_mac, uint16_t generation) {
    if (!table || !mapper_mac) return;

    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        session_entry* entry = &table->entries[i];
        if (entry->valid &&
            mac_equal(entry->mapper_mac, mapper_mac) &&
            entry->generation == generation) {
            memset(entry, 0, sizeof(session_entry));
            entry->valid = false;
            if (table->count > 0) table->count--;
            break;
        }
    }
    session_table_update_complete_status(table);
}

void session_table_update_complete_status(session_table* table) {
    if (!table) return;

    if (table->count == 0) {
        table->all_complete = true;
        return;
    }

    bool all_complete = true;
    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        if (table->entries[i].valid && !table->entries[i].complete) {
            all_complete = false;
            break;
        }
    }
    table->all_complete = all_complete;
}

bool session_table_is_empty(session_table* table) {
    return table ? (table->count == 0) : true;
}

bool session_table_all_complete(session_table* table) {
    return table ? table->all_complete : true;
}

void session_table_clear(session_table* table) {
    if (!table) return;
    memset(table->entries, 0, sizeof(table->entries));
    table->count = 0;
    table->all_complete = true;
}

#pragma mark -
#pragma mark Session Event Derivation

// Forward declaration for frame structures (defined in lltdBlock.h)
#ifndef LLTD_TESTING
#include "lltdBlock.h"
#endif

int derive_session_event(const void* frame, session_table* table, const uint8_t* our_mac) {
    if (!frame) return -1;

#ifdef LLTD_TESTING
    // For testing, just return a default
    (void)table;
    (void)our_mac;
    return sess_discover_noack;
#else
    const lltd_demultiplex_header_t* header = (const lltd_demultiplex_header_t*)frame;

    // Handle Reset opcode
    if (header->opcode == opcode_reset) {
        // Check if it's a topology reset (broadcast destination)
        static const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if (mac_equal(header->realDestination.a, broadcast)) {
            return sess_topo_reset;
        }
        return sess_reset;
    }

    // Handle Hello opcode
    if (header->opcode == opcode_hello) {
        return sess_hello;
    }

    // Handle Discover opcode - this is where we derive the complex session events
    if (header->opcode == opcode_discover) {
        // Find existing session for this mapper
        session_entry* existing = NULL;
        if (table) {
            existing = session_table_find(table, header->realSource.a,
                                          ntohs(((lltd_discover_upper_header_t*)(header + 1))->generation),
                                          ntohs(header->seqNumber));
        }

        // Check if this discover is for us (acking) or not (no ack needed)
        bool acking = false;
        if (our_mac) {
            // Check the station list in the discover frame
            const lltd_discover_upper_header_t* disc_header =
                (const lltd_discover_upper_header_t*)(header + 1);
            uint16_t station_count = ntohs(disc_header->stationNumber);
            if (station_count == 0) {
                // Empty station list means we should respond
                acking = true;
            } else {
                // Check if we're in the station list
                const ethernet_header_t* stations = disc_header->stationList;
                for (uint16_t i = 0; i < station_count; i++) {
                    if (mac_equal(stations[i].source.a, our_mac)) {
                        acking = true;
                        break;
                    }
                }
            }
        }

        // Check for changed XID (sequence number changed)
        bool changed_xid = false;
        if (existing && existing->seq_number != ntohs(header->seqNumber)) {
            changed_xid = true;
        }

        // Check for conflicting mapper (different mapper MAC for same generation)
        if (existing && !mac_equal(existing->mapper_mac, header->realSource.a)) {
            return sess_discover_conflicting;
        }

        // Return appropriate event
        if (acking) {
            return changed_xid ? sess_discover_acking_chgd_xid : sess_discover_acking;
        } else {
            return changed_xid ? sess_discover_noack_chgd_xid : sess_discover_noack;
        }
    }

    return -1;  // Unknown opcode for session events
#endif
}

#pragma mark -
#pragma mark RepeatBand Algorithm

void band_init_stats(band_state* band) {
    if (!band) return;
    band->Ni = BAND_ALPHA;
    band->r = 0;
    band->begun = false;
    band->hello_timeout_ts = 0;
    band->block_timeout_ts = lltd_monotonic_milliseconds() + BAND_BLOCK_TIME;
}

void band_update_stats(band_state* band) {
    if (!band) return;

    // Update Ni based on received Hello count
    // If r > 0 and begun: Ni = min(NMAX, ALPHA * r^BETA)
    if (band->r > 0 && band->begun) {
        uint32_t r_pow_beta = band->r;
        for (int i = 1; i < BAND_BETA; i++) {
            r_pow_beta *= band->r;
        }
        uint32_t new_ni = BAND_ALPHA * r_pow_beta;
        band->Ni = (new_ni > BAND_NMAX) ? BAND_NMAX : new_ni;
    }

    // Reset r counter for next block
    band->r = 0;
    band->block_timeout_ts = lltd_monotonic_milliseconds() + BAND_BLOCK_TIME;
}

uint64_t band_choose_hello_time(band_state* band) {
    if (!band) return 0;

    // Calculate next hello time based on Ni and FRAME_TIME
    // hello_interval = FRAME_TIME * TXC * (Ni / GAMMA)
    uint64_t now = lltd_monotonic_milliseconds();
    uint64_t interval_ms = (uint64_t)(BAND_FRAME_TIME * BAND_TXC * band->Ni / BAND_GAMMA);

    // Ensure minimum interval of one frame time
    if (interval_ms < (uint64_t)BAND_FRAME_TIME) {
        interval_ms = (uint64_t)BAND_FRAME_TIME;
    }

    band->hello_timeout_ts = now + interval_ms;
    return band->hello_timeout_ts;
}

void band_do_hello(band_state* band) {
    if (!band) return;

    // After sending hello, choose next hello time
    band_choose_hello_time(band);

    // Mark enumeration as begun
    band->begun = true;
}

void band_on_hello_received(band_state* band) {
    if (!band) return;

    // Increment r counter
    band->r++;

    // If r >= GAMMA, we have enough samples to update stats
    if (band->r >= BAND_GAMMA && !band->begun) {
        band->begun = true;
    }
}

#pragma mark -
#pragma mark Mapping Engine Extended Functions

void mapping_reset_charge(mapping_state* mstate) {
    if (!mstate) return;
    mstate->ctc = 0;
    mstate->charge_timeout_ts = 0;
}

void mapping_on_charge(mapping_state* mstate) {
    if (!mstate) return;
    mstate->ctc++;
    // Restart charge timeout (1 second)
    mstate->charge_timeout_ts = lltd_monotonic_seconds() + 1;
}

bool mapping_check_charge_timeout(mapping_state* mstate) {
    if (!mstate || mstate->charge_timeout_ts == 0) return false;
    uint64_t now = lltd_monotonic_seconds();
    if (now >= mstate->charge_timeout_ts) {
        mstate->ctc = 0;
        mstate->charge_timeout_ts = 0;
        return true;
    }
    return false;
}

bool mapping_check_inactive_timeout(mapping_state* mstate) {
    if (!mstate || mstate->inactive_timeout_ts == 0) return false;
    uint64_t now = lltd_monotonic_seconds();
    return now >= mstate->inactive_timeout_ts;
}

void mapping_reset_inactive_timeout(mapping_state* mstate) {
    if (!mstate) return;
    // Inactive timeout is 30 seconds as per spec
    mstate->inactive_timeout_ts = lltd_monotonic_seconds() + 30;
}

#pragma mark -
#pragma mark Tick Function

// Forward declarations for platform-specific functions
#ifndef LLTD_TESTING
extern void sendHelloMessage(void* networkInterface);
#endif

void automata_tick(automata* mapping, automata* enumeration, session_table* sessions, void* network_interface) {
    uint64_t now_ms = lltd_monotonic_milliseconds();
    uint64_t now_s = lltd_monotonic_seconds();

    // Check mapping automaton for timeouts
    if (mapping && mapping->extra) {
        mapping_state* mstate = (mapping_state*)mapping->extra;

        // Check inactive timeout
        if (mapping_check_inactive_timeout(mstate)) {
            log_debug("Mapping: Inactive timeout triggered");
            // Reset the timeout so it doesn't keep firing
            mstate->inactive_timeout_ts = 0;
            // Transition to Quiescent
            switch_state_mapping(mapping, -1, "inactive_timeout");
            mapping_reset_charge(mstate);
            if (sessions) {
                session_table_clear(sessions);
            }
        }

        // Check charge timeout
        if (mapping_check_charge_timeout(mstate)) {
            log_debug("Mapping: Charge timeout triggered, ctc reset");
        }
    }

    // Process session state machines for timeout
    if (sessions) {
        for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
            session_entry* entry = &sessions->entries[i];
            if (entry->valid) {
                // Check for session timeout (60 second inactivity removes session)
                if (now_s > entry->last_activity_ts + 60) {
                    log_debug("Session timeout for mapper %02x:%02x:%02x:%02x:%02x:%02x",
                              entry->mapper_mac[0], entry->mapper_mac[1], entry->mapper_mac[2],
                              entry->mapper_mac[3], entry->mapper_mac[4], entry->mapper_mac[5]);
                    entry->valid = false;
                    if (sessions->count > 0) sessions->count--;
                }
            }
        }
        session_table_update_complete_status(sessions);
    }

    // Check enumeration automaton for timeouts
    if (enumeration && enumeration->extra) {
        band_state* band = (band_state*)enumeration->extra;

        // Check if session table state changed
        bool table_empty = session_table_is_empty(sessions);
        bool all_complete = session_table_all_complete(sessions);

        // Determine enumeration input based on session table state
        // Only send inputs when NOT in Quiescent (state 0) to avoid spam
        if (enumeration->current_state != 0) {
            if (table_empty) {
                // Table empty (Reset received) - go to Quiescent from any active state
                log_debug("RepeatBand: Table empty, returning to Quiescent");
                enumeration->current_state = 0;  // Quiescent
                band->hello_timeout_ts = 0;
                band->block_timeout_ts = 0;
                band->begun = false;
            } else if (all_complete) {
                switch_state_enumeration(enumeration, enum_sess_complete, "tick");
            } else {
                switch_state_enumeration(enumeration, enum_sess_not_complete, "tick");
            }
        }

        // Only process hello/block timeouts if in Pausing state (index 1)
        if (enumeration->current_state == 1) {
            // Check hello timeout
            if (band->hello_timeout_ts > 0 && now_ms >= band->hello_timeout_ts) {
                log_debug("Enumeration: Hello timeout - sending Hello");
#ifndef LLTD_TESTING
                if (network_interface) {
                    sendHelloMessage(network_interface);
                }
#else
                (void)network_interface;
#endif
                band_do_hello(band);
                switch_state_enumeration(enumeration, enum_hello, "hello_timeout");
            }

            // Check block timeout
            if (band->block_timeout_ts > 0 && now_ms >= band->block_timeout_ts) {
                log_debug("Enumeration: Block timeout - updating stats");
                band_update_stats(band);
                band_choose_hello_time(band);
            }
        }
    }
}
