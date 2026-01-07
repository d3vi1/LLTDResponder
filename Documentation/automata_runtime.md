# LLTD Automata Runtime Integration

This document describes how the three LLTD finite state machines (Mapping Engine, Session Table, Enumeration Engine) are integrated into the Darwin runtime.

## Architecture Overview

```
+------------------+
|   lltdLoop()     |  Per-interface receive thread
+--------+---------+
         |
         | select() with 100ms timeout
         |
    +----v----+
    | Packet  |  Data available?
    |  RX?    |
    +----+----+
         |
    +----v----+
    |automata |  Called every 100ms regardless of RX
    | _tick() |
    +----+----+
         |
  +------+------+------+
  |      |      |      |
  v      v      v      v
Mapping  Session Enum  Session
Engine   Table   Engine Table
(FSM)    (entries)(FSM) Timeouts
```

## Component Initialization

When a network interface is validated in `deviceAppeared()`:

```c
currentNetworkInterface->mappingAutomata = init_automata_mapping();
currentNetworkInterface->sessionAutomata = init_automata_session();
currentNetworkInterface->enumerationAutomata = init_automata_enumeration();
currentNetworkInterface->sessionTable = session_table_create();
```

Each automaton starts in its initial state:
- **Mapping Engine**: Quiescent (state 0)
- **Session Automaton**: Nascent (state 1)
- **Enumeration Engine**: Quiescent (state 0)

## Periodic Tick Mechanism

The receive loop uses `select()` with a 100ms timeout to ensure timeouts are processed even without incoming packets:

```c
for(;;) {
    fd_set readfds;
    struct timeval timeout = {0, 100000}; // 100ms

    int result = select(fd + 1, &readfds, NULL, NULL, &timeout);

    // Always call tick (handles all timeout-based behavior)
    automata_tick(mapping, enumeration, sessionTable, interface);

    if (result > 0) {
        // Process received frame...
    }
}
```

## Mapping Engine Integration

The Mapping Engine FSM tracks the current mapping session:

### States
| State | Index | Timeout | Description |
|-------|-------|---------|-------------|
| Quiescent | 0 | None | Idle, waiting for Discover |
| Command | 1 | 1s | Processing commands |
| Emit | 2 | 30s | Transmitting probes |

### Extended State (`mapping_state`)
```c
typedef struct mapping_state {
    uint8_t     ctc;                // Charge timeout counter
    uint64_t    charge_timeout_ts;  // Charge timeout timestamp
    uint64_t    inactive_timeout_ts;// Inactive timeout timestamp (30s)
} mapping_state;
```

### Runtime Behavior

1. **On Frame Receipt**: Reset inactive timeout, update FSM state
2. **On Charge Opcode**: Increment `ctc`, restart 1s charge timeout
3. **On Charge Timeout**: Reset `ctc` to 0
4. **On Inactive Timeout**: Return to Quiescent, clear session table

## Session Table Integration

The session table tracks multiple concurrent mapper sessions:

### Data Structure
```c
typedef struct session_entry {
    uint8_t     mapper_mac[6];      // Mapper hardware address
    uint16_t    generation;         // Session generation number
    uint16_t    seq_number;         // Current sequence number
    uint8_t     state;              // Session state (sess_* constants)
    bool        complete;           // Session complete flag
    bool        valid;              // Entry in use
    uint64_t    last_activity_ts;   // Last activity timestamp
} session_entry;
```

### Session Event Derivation

The `derive_session_event()` function analyzes incoming frames to produce session-level events:

| Opcode | Condition | Event |
|--------|-----------|-------|
| Discover | Empty station list OR we're in list | `sess_discover_acking` |
| Discover | Not in station list | `sess_discover_noack` |
| Discover | Seq changed | `*_chgd_xid` variants |
| Discover | Different mapper, same gen | `sess_discover_conflicting` |
| Hello | Any | `sess_hello` |
| Reset | Broadcast dest | `sess_topo_reset` |
| Reset | Unicast dest | `sess_reset` |

### Runtime Behavior

1. **On Discover**: Add/update session entry, update MapperHwAddress
2. **On Reset**: Clear session table
3. **On Tick**: Remove stale entries (>1s inactivity)
4. **Status Query**: `session_table_is_empty()`, `session_table_all_complete()`

## Enumeration Engine (RepeatBand) Integration

The Enumeration Engine implements the RepeatBand algorithm for periodic Hello transmission:

### States
| State | Index | Timeout | Description |
|-------|-------|---------|-------------|
| Quiescent | 0 | None | No active sessions |
| Pausing | 1 | 60s | Active enumeration |
| Wait | 2 | 30s | All sessions complete |

### RepeatBand State (`band_state`)
```c
typedef struct band_state {
    uint32_t    Ni;                 // Repetition count
    uint32_t    r;                  // Hello counter for current block
    bool        begun;              // Enumeration started flag
    uint64_t    hello_timeout_ts;   // Next Hello time (ms)
    uint64_t    block_timeout_ts;   // Next block timeout (ms)
} band_state;
```

### RepeatBand Constants
```c
#define BAND_NMAX      10000  // Maximum repetition count
#define BAND_ALPHA     45     // Scaling factor
#define BAND_BETA      2      // Exponent for Ni calculation
#define BAND_GAMMA     10     // Min Hellos before stats
#define BAND_TXC       4      // Transmit count
#define BAND_FRAME_TIME 6.667 // Frame time (ms)
#define BAND_BLOCK_TIME 300   // Block time (ms)
```

### Runtime Behavior

1. **On New Session (Quiescent)**:
   - Call `band_init_stats()` - sets `Ni=ALPHA`, `r=0`
   - Call `band_choose_hello_time()` - schedules first Hello
   - Transition to Pausing

2. **On Hello Received (Pausing)**:
   - Call `band_on_hello_received()` - increments `r`

3. **On Hello Timeout (Pausing)**:
   - Call `sendHelloMessage()` - transmit Hello frame
   - Call `band_do_hello()` - reschedule next Hello

4. **On Block Timeout (Pausing)**:
   - Call `band_update_stats()` - update `Ni = min(NMAX, ALPHA * r^BETA)`
   - Call `band_choose_hello_time()` - schedule next Hello

5. **On Session Table Complete**: Transition to Wait
6. **On Session Table Empty**: Transition to Quiescent

## Frame Processing Flow

```
Frame Received
      |
      v
derive_session_event() --> session event (sess_*)
      |
      v
Update session_table with mapper info
      |
      v
switch_state_mapping(opcode)
      |
      v
mapping_reset_inactive_timeout()
      |
      v
[if opcode == charge] mapping_on_charge()
      |
      v
switch_state_session(sess_event)
      |
      v
[if opcode == hello] band_on_hello_received()
      |
      v
switch_state_enumeration(enum_event)
      |
      v
parseFrame() --> generate response
```

## Timeout Processing (automata_tick)

Called every 100ms from the receive loop:

```
automata_tick()
      |
      +-- Check mapping_check_inactive_timeout()
      |       --> Clear session table, go Quiescent
      |
      +-- Check mapping_check_charge_timeout()
      |       --> Reset ctc to 0
      |
      +-- Check session entries for timeout
      |       --> Remove stale entries
      |
      +-- Update session_table_all_complete()
      |
      +-- Check enumeration hello_timeout
      |       --> sendHelloMessage(), band_do_hello()
      |
      +-- Check enumeration block_timeout
              --> band_update_stats(), band_choose_hello_time()
```

## Thread Safety

Each interface runs in its own thread with its own automata instances. No cross-thread access occurs. The main CFRunLoop only handles IOKit notifications for interface add/remove events.

## Cleanup

On interface removal (`deviceDisappeared`):

```c
free_automata(currentNetworkInterface->mappingAutomata);
free_automata(currentNetworkInterface->sessionAutomata);
free_automata(currentNetworkInterface->enumerationAutomata);
session_table_destroy(currentNetworkInterface->sessionTable);
```

## Testing

The automata module supports the `LLTD_TESTING` compile flag which:
- Stubs out platform-specific functions
- Disables actual frame transmission
- Enables unit testing of state machine logic

See `tests/test_lltd_tlv_ops.c` for TLV-related tests.
