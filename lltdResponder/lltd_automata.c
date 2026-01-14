/*
 * Protocol-core automata and session tracking.
 *
 * This file must remain OS-agnostic: no OS-specific conditional compilation
 * and no OS headers. Platform integration is done via lltd_port.* symbols.
 */

#include "lltd_automata.h"

#include "lltd_endian.h"
#include "lltd_port.h"

uint64_t lltd_monotonic_seconds(void) {
    return lltd_port_monotonic_seconds();
}

uint64_t lltd_monotonic_milliseconds(void) {
    return lltd_port_monotonic_milliseconds();
}

automata *init_automata_mapping(void) {
    automata *autom = malloc(sizeof(automata));
    autom->states_no = 3;
    autom->transitions_no = 13;
    autom->last_ts = lltd_monotonic_seconds();
    autom->name = "Mapping";

    mapping_state *mstate = malloc(sizeof(mapping_state));
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
    command.timeout = 5;
    emit.name = "Emit";
    emit.timeout = 30;

    autom->current_state = 0;
    autom->states_table[0] = quiescent;
    autom->states_table[1] = command;
    autom->states_table[2] = emit;

    transition *t = autom->transitions_table;
    t[0].from = 0; t[0].to = 1; t[0].with = opcode_discover;
    t[1].from = 1; t[1].to = 0; t[1].with = -1;
    t[2].from = 1; t[2].to = 0; t[2].with = opcode_reset;
    t[3].from = 1; t[3].to = 1; t[3].with = opcode_query;
    t[4].from = 1; t[4].to = 1; t[4].with = opcode_queryLargeTlv;
    t[5].from = 1; t[5].to = 1; t[5].with = opcode_charge;
    t[6].from = 1; t[6].to = 1; t[6].with = opcode_probe;
    t[7].from = 1; t[7].to = 2; t[7].with = opcode_emit;
    t[8].from = 2; t[8].to = 2; t[8].with = opcode_probe;
    t[9].from = 2; t[9].to = 2; t[9].with = -2;
    t[10].from = 2; t[10].to = 1; t[10].with = -3;
    t[11].from = 2; t[11].to = 0; t[11].with = -1;
    t[12].from = 2; t[12].to = 0; t[12].with = opcode_reset;

    return autom;
}

automata *switch_state_mapping(automata *autom, int input, char *debug) {
    (void)debug;
    uint8_t new_state = autom->current_state;
    state *current_state = &autom->states_table[autom->current_state];
    bool timeout = false;

    uint64_t now = lltd_monotonic_seconds();
    uint64_t diff = (now - autom->last_ts);

    if (current_state->timeout != 0 && diff > (uint64_t)current_state->timeout) {
        timeout = true;
        input = -1;
    }

    int find = -1;
    for (int i = 0; i < autom->transitions_no; i++) {
        if (autom->current_state == autom->transitions_table[i].from &&
            autom->transitions_table[i].with == input) {
            new_state = autom->transitions_table[i].to;
            find = i;
        }
    }

    if (autom->current_state != new_state || find >= 0 || timeout) {
        lltd_port_log_debug("%s: Switching from %s with %d%s to %s",
                            autom->name,
                            current_state->name,
                            input,
                            timeout ? " (timeout)" : "",
                            autom->states_table[new_state].name);
        autom->current_state = new_state;
    }

    autom->last_ts = now;
    if (timeout) {
        return switch_state_mapping(autom, input, "timeout");
    }
    return autom;
}

automata *init_automata_enumeration(void) {
    automata *autom = malloc(sizeof(automata));
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

    autom->extra = malloc(sizeof(band_state));
    band_state *band = (band_state *)autom->extra;
    band->begun = false;
    band->Ni = BAND_ALPHA;
    band->r = 0;
    band->hello_timeout_ts = 0;
    band->block_timeout_ts = 0;

    autom->current_state = 0;
    autom->states_table[0] = quiescent;
    autom->states_table[1] = pausing;
    autom->states_table[2] = wait;

    transition *t = autom->transitions_table;
    t[0].from = 0; t[0].to = 1; t[0].with = enum_sess_not_complete;
    t[1].from = 0; t[1].to = 1; t[1].with = enum_new_session;
    t[2].from = 1; t[2].to = 2; t[2].with = enum_sess_complete;
    t[3].from = 1; t[3].to = 1; t[3].with = enum_hello;
    t[4].from = 1; t[4].to = 1; t[4].with = enum_new_session;
    t[5].from = 2; t[5].to = 1; t[5].with = enum_sess_not_complete;
    t[6].from = 2; t[6].to = 0; t[6].with = enum_sess_complete;
    t[7].from = 2; t[7].to = 1; t[7].with = enum_new_session;

    return autom;
}

automata *switch_state_enumeration(automata *autom, int input, char *debug) {
    uint8_t new_state = autom->current_state;
    state *current_state = &autom->states_table[autom->current_state];

    int find = -1;
    for (int i = 0; i < autom->transitions_no; i++) {
        if (autom->current_state == autom->transitions_table[i].from &&
            autom->transitions_table[i].with == input) {
            new_state = autom->transitions_table[i].to;
            find = i;
        }
    }

    if (new_state != autom->current_state || find >= 0) {
        lltd_port_log_debug("%s: Switching from %s with %d (%s) to %s",
                            autom->name,
                            current_state->name,
                            input,
                            debug ? debug : "?",
                            autom->states_table[new_state].name);
        autom->current_state = new_state;
    }

    autom->last_ts = lltd_monotonic_seconds();
    return autom;
}

automata *init_automata_session(void) {
    automata *autom = malloc(sizeof(automata));
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

    autom->current_state = 1;
    autom->states_table[0] = temporary;
    autom->states_table[1] = nascent;
    autom->states_table[2] = pending;
    autom->states_table[3] = complete;

    transition *t = autom->transitions_table;
    t[0].from = 1; t[0].to = 0; t[0].with = sess_discover_conflicting;
    t[1].from = 0; t[1].to = 1; t[1].with = -1;
    t[2].from = 0; t[2].to = 1; t[2].with = sess_reset;
    t[3].from = 0; t[3].to = 1; t[3].with = sess_hello;
    t[4].from = 0; t[4].to = 1; t[4].with = sess_topo_reset;
    t[5].from = 1; t[5].to = 2; t[5].with = sess_discover_noack;
    t[6].from = 2; t[6].to = 1; t[6].with = -1;
    t[7].from = 2; t[7].to = 1; t[7].with = sess_reset;
    t[8].from = 2; t[8].to = 2; t[8].with = sess_discover_noack_chgd_xid;
    t[9].from = 2; t[9].to = 3; t[9].with = sess_discover_acking;
    t[10].from = 2; t[10].to = 3; t[10].with = sess_discover_acking_chgd_xid;
    t[11].from = 1; t[11].to = 3; t[11].with = sess_discover_acking;
    t[12].from = 3; t[12].to = 1; t[12].with = opcode_reset;
    t[13].from = 3; t[13].to = 1; t[13].with = -1;
    t[14].from = 3; t[14].to = 3; t[14].with = sess_discover_acking_chgd_xid;
    t[15].from = 3; t[15].to = 2; t[15].with = sess_discover_noack_chgd_xid;

    return autom;
}

automata *switch_state_session(automata *autom, int input, char *debug) {
    uint8_t new_state = autom->current_state;
    state *current_state = &autom->states_table[autom->current_state];
    bool timeout = false;

    uint64_t now = lltd_monotonic_seconds();
    uint64_t diff = (now - autom->last_ts);

    if (current_state->timeout != 0 && diff > (uint64_t)current_state->timeout) {
        timeout = true;
        input = -1;
    }

    int find = -1;
    for (int i = 0; i < autom->transitions_no; i++) {
        if (autom->current_state == autom->transitions_table[i].from &&
            autom->transitions_table[i].with == input) {
            new_state = autom->transitions_table[i].to;
            find = i;
        }
    }

    if (autom->current_state != new_state || find >= 0 || timeout) {
        lltd_port_log_debug("%s: Switching from %s with %d (%s)%s to %s",
                            autom->name,
                            current_state->name,
                            input,
                            debug ? debug : "?",
                            timeout ? " (timeout)" : "",
                            autom->states_table[new_state].name);
        autom->current_state = new_state;
    }

    autom->last_ts = now;
    if (timeout) {
        return switch_state_session(autom, input, "timeout");
    }
    return autom;
}

static bool mac_equal(const uint8_t *a, const uint8_t *b) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] &&
           a[3] == b[3] && a[4] == b[4] && a[5] == b[5];
}

static void mac_copy(uint8_t *dst, const uint8_t *src) {
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
    dst[3] = src[3]; dst[4] = src[4]; dst[5] = src[5];
}

session_table *session_table_create(void) {
    session_table *table = malloc(sizeof(session_table));
    if (table) {
        memset(table, 0, sizeof(session_table));
        table->count = 0;
        table->all_complete = true;
    }
    return table;
}

void session_table_destroy(session_table *table) {
    free(table);
}

session_entry *session_table_find(session_table *table,
                                  const uint8_t *mapper_mac,
                                  uint16_t generation,
                                  uint16_t seq) {
    (void)seq;
    if (!table || !mapper_mac) {
        return NULL;
    }
    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        session_entry *entry = &table->entries[i];
        if (entry->valid &&
            mac_equal(entry->mapper_mac, mapper_mac) &&
            entry->generation == generation) {
            return entry;
        }
    }
    return NULL;
}

session_entry *session_table_add(session_table *table,
                                 const uint8_t *mapper_mac,
                                 uint16_t generation,
                                 uint16_t seq) {
    if (!table || !mapper_mac) {
        return NULL;
    }

    session_entry *existing = session_table_find(table, mapper_mac, generation, seq);
    if (existing) {
        existing->seq_number = seq;
        existing->last_activity_ts = lltd_monotonic_seconds();
        return existing;
    }

    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        session_entry *entry = &table->entries[i];
        if (!entry->valid) {
            mac_copy(entry->mapper_mac, mapper_mac);
            entry->generation = generation;
            entry->seq_number = seq;
            entry->state = sess_discover_noack;
            entry->complete = false;
            entry->valid = true;
            entry->created_ts = lltd_monotonic_seconds();
            entry->last_activity_ts = entry->created_ts;
            table->count++;
            table->all_complete = false;
            return entry;
        }
    }

    lltd_port_log_warning("Session table full, cannot add mapper");
    return NULL;
}

void session_table_remove(session_table *table, const uint8_t *mapper_mac, uint16_t generation) {
    if (!table || !mapper_mac) {
        return;
    }

    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        session_entry *entry = &table->entries[i];
        if (entry->valid &&
            mac_equal(entry->mapper_mac, mapper_mac) &&
            entry->generation == generation) {
            entry->valid = false;
            if (table->count > 0) {
                table->count--;
            }
            break;
        }
    }

    session_table_update_complete_status(table);
}

void session_table_update_complete_status(session_table *table) {
    if (!table) {
        return;
    }
    bool all_complete = true;
    bool any_valid = false;
    for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
        session_entry *entry = &table->entries[i];
        if (entry->valid) {
            any_valid = true;
            if (!entry->complete) {
                all_complete = false;
                break;
            }
        }
    }
    table->all_complete = !any_valid || all_complete;
}

bool session_table_is_empty(session_table *table) {
    if (!table) {
        return true;
    }
    return table->count == 0;
}

bool session_table_all_complete(session_table *table) {
    if (!table) {
        return true;
    }
    return table->all_complete;
}

void session_table_clear(session_table *table) {
    if (!table) {
        return;
    }
    memset(table->entries, 0, sizeof(table->entries));
    table->count = 0;
    table->all_complete = true;
}

int derive_session_event(const void *frame, session_table *table, const uint8_t *our_mac) {
    if (!frame) {
        return -1;
    }

#ifdef LLTD_TESTING
    (void)table;
    (void)our_mac;
    return sess_discover_noack;
#else
    const lltd_demultiplex_header_t *header = (const lltd_demultiplex_header_t *)frame;

    if (header->opcode == opcode_reset) {
        static const uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if (mac_equal(header->realDestination.a, broadcast)) {
            return sess_topo_reset;
        }
        return sess_reset;
    }

    if (header->opcode == opcode_hello) {
        return sess_hello;
    }

    if (header->opcode == opcode_discover) {
        session_entry *existing = NULL;
        const lltd_discover_upper_header_t *disc_header =
            (const lltd_discover_upper_header_t *)(header + 1);
        uint16_t generation = lltd_ntohs(disc_header->generation);
        uint16_t xid = lltd_ntohs(header->seqNumber);

        if (table) {
            existing = session_table_find(table, header->realSource.a, generation, xid);
        }

        bool acking = false;
        if (our_mac) {
            uint16_t station_count = lltd_ntohs(disc_header->stationNumber);
            if (station_count == 0) {
                acking = true;
            } else {
                const ethernet_header_t *stations = disc_header->stationList;
                for (uint16_t i = 0; i < station_count; i++) {
                    if (mac_equal(stations[i].source.a, our_mac)) {
                        acking = true;
                        break;
                    }
                }
            }
        }

        bool changed_xid = false;
        if (existing && existing->seq_number != xid) {
            changed_xid = true;
        }

        if (existing && !mac_equal(existing->mapper_mac, header->realSource.a)) {
            return sess_discover_conflicting;
        }

        if (acking) {
            return changed_xid ? sess_discover_acking_chgd_xid : sess_discover_acking;
        }
        return changed_xid ? sess_discover_noack_chgd_xid : sess_discover_noack;
    }

    return -1;
#endif
}

void band_init_stats(band_state *band) {
    if (!band) {
        return;
    }
    band->Ni = BAND_ALPHA;
    band->r = 0;
    band->begun = false;
    band->hello_timeout_ts = 0;
    band->block_timeout_ts = lltd_monotonic_milliseconds() + BAND_BLOCK_TIME;
}

void band_update_stats(band_state *band) {
    if (!band) {
        return;
    }

    if (band->r > 0 && band->begun) {
        uint32_t r_pow_beta = band->r;
        for (int i = 1; i < BAND_BETA; i++) {
            r_pow_beta *= band->r;
        }
        uint32_t new_ni = BAND_ALPHA * r_pow_beta;
        band->Ni = (new_ni > BAND_NMAX) ? BAND_NMAX : new_ni;
    }

    band->r = 0;
    band->block_timeout_ts = lltd_monotonic_milliseconds() + BAND_BLOCK_TIME;
}

uint64_t band_choose_hello_time(band_state *band) {
    if (!band) {
        return 0;
    }

    uint64_t now = lltd_monotonic_milliseconds();
    uint64_t numerator = (uint64_t)BAND_TXC * band->Ni * 20;
    uint64_t denominator = (uint64_t)BAND_GAMMA * 3;
    uint64_t interval_ms = numerator / denominator;
    if (numerator % denominator != 0) {
        interval_ms += 1;
    }
    if (interval_ms < (uint64_t)BAND_MUL_FRAME(1)) {
        interval_ms = (uint64_t)BAND_MUL_FRAME(1);
    }

    band->hello_timeout_ts = now + interval_ms;
    return band->hello_timeout_ts;
}

void band_do_hello(band_state *band) {
    if (!band) {
        return;
    }
    band_choose_hello_time(band);
    band->begun = true;
}

void band_on_hello_received(band_state *band) {
    if (!band) {
        return;
    }
    band->r++;
    if (band->r >= BAND_GAMMA && !band->begun) {
        band->begun = true;
    }
}

void mapping_reset_charge(mapping_state *mstate) {
    if (!mstate) {
        return;
    }
    mstate->ctc = 0;
    mstate->charge_timeout_ts = 0;
}

void mapping_on_charge(mapping_state *mstate) {
    if (!mstate) {
        return;
    }
    mstate->ctc++;
    mstate->charge_timeout_ts = lltd_monotonic_seconds() + 1;
}

bool mapping_check_charge_timeout(mapping_state *mstate) {
    if (!mstate || mstate->charge_timeout_ts == 0) {
        return false;
    }
    uint64_t now = lltd_monotonic_seconds();
    if (now >= mstate->charge_timeout_ts) {
        mstate->ctc = 0;
        mstate->charge_timeout_ts = 0;
        return true;
    }
    return false;
}

bool mapping_check_inactive_timeout(mapping_state *mstate) {
    if (!mstate || mstate->inactive_timeout_ts == 0) {
        return false;
    }
    uint64_t now = lltd_monotonic_seconds();
    return now >= mstate->inactive_timeout_ts;
}

void mapping_reset_inactive_timeout(mapping_state *mstate) {
    if (!mstate) {
        return;
    }
    mstate->inactive_timeout_ts = lltd_monotonic_seconds() + 30;
}

void automata_tick(automata *mapping,
                   automata *enumeration,
                   session_table *sessions,
                   const lltd_automata_tick_port *port) {
    uint64_t now_ms = lltd_monotonic_milliseconds();
    uint64_t now_s = lltd_monotonic_seconds();

    if (mapping && mapping->extra) {
        mapping_state *mstate = (mapping_state *)mapping->extra;
        if (mapping_check_inactive_timeout(mstate)) {
            lltd_port_log_debug("Mapping: Inactive timeout triggered");
            mstate->inactive_timeout_ts = 0;
            switch_state_mapping(mapping, -1, "inactive_timeout");
            mapping_reset_charge(mstate);
            if (sessions) {
                session_table_clear(sessions);
            }
        }
        if (mapping_check_charge_timeout(mstate)) {
            lltd_port_log_debug("Mapping: Charge timeout triggered, ctc reset");
        }
    }

    if (sessions) {
        for (int i = 0; i < SESSION_TABLE_MAX_ENTRIES; i++) {
            session_entry *entry = &sessions->entries[i];
            if (entry->valid) {
                if (now_s > entry->last_activity_ts + 60) {
                    lltd_port_log_debug("Session timeout for mapper %02x:%02x:%02x:%02x:%02x:%02x",
                                        entry->mapper_mac[0], entry->mapper_mac[1], entry->mapper_mac[2],
                                        entry->mapper_mac[3], entry->mapper_mac[4], entry->mapper_mac[5]);
                    entry->valid = false;
                    if (sessions->count > 0) {
                        sessions->count--;
                    }
                }
            }
        }
        session_table_update_complete_status(sessions);
    }

    if (enumeration && enumeration->extra) {
        band_state *band = (band_state *)enumeration->extra;

        bool table_empty = session_table_is_empty(sessions);
        bool all_complete = session_table_all_complete(sessions);

        if (enumeration->current_state != 0) {
            if (table_empty) {
                lltd_port_log_debug("RepeatBand: Table empty, returning to Quiescent");
                enumeration->current_state = 0;
                band->hello_timeout_ts = 0;
                band->block_timeout_ts = 0;
                band->begun = false;
            } else if (all_complete) {
                switch_state_enumeration(enumeration, enum_sess_complete, "tick");
            } else {
                switch_state_enumeration(enumeration, enum_sess_not_complete, "tick");
            }
        }

        if (enumeration->current_state == 1) {
            if (band->hello_timeout_ts > 0 && now_ms >= band->hello_timeout_ts) {
                uint64_t last_tx = 0;
                if (port && port->last_hello_tx_ms) {
                    last_tx = *port->last_hello_tx_ms;
                }
                if (last_tx > 0 && now_ms - last_tx < HELLO_MIN_INTERVAL_MS) {
                    lltd_port_log_debug("Enumeration: Hello timeout suppressed t=%llu last_tx=%llu next_min=%llu",
                                        (unsigned long long)now_ms,
                                        (unsigned long long)last_tx,
                                        (unsigned long long)(last_tx + HELLO_MIN_INTERVAL_MS));
                    band->hello_timeout_ts = last_tx + HELLO_MIN_INTERVAL_MS;
                } else {
                    lltd_port_log_debug("Enumeration: Hello timeout - sending Hello t=%llu deadline=%llu last_tx=%llu",
                                        (unsigned long long)now_ms,
                                        (unsigned long long)band->hello_timeout_ts,
                                        (unsigned long long)last_tx);
#ifndef LLTD_TESTING
                    if (port && port->send_hello && port->network_interface) {
                        port->send_hello(port->network_interface);
                        if (port->last_hello_tx_ms) {
                            *port->last_hello_tx_ms = now_ms;
                        }
                    }
#endif
                    band_do_hello(band);
                    if (band->hello_timeout_ts < now_ms + HELLO_MIN_INTERVAL_MS) {
                        band->hello_timeout_ts = now_ms + HELLO_MIN_INTERVAL_MS;
                    }
                    switch_state_enumeration(enumeration, enum_hello, "hello_timeout");
                }
            }

            if (band->block_timeout_ts > 0 && now_ms >= band->block_timeout_ts) {
                lltd_port_log_debug("Enumeration: Block timeout - updating stats");
                band_update_stats(band);
                band_choose_hello_time(band);
            }
        }
    }
}
