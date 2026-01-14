/******************************************************************************
 *                                                                            *
 *   windows-shared.c                                                         *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"
#include "../../../lltdResponder/lltdAutomata.h"
#include "../../daemon/lltdBlock.h"
#include "windows-backend.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static volatile int exitFlag = 0;

typedef struct {
    windows_interface_handle_t handle;
    automata *mappingAutomata;
    automata *sessionAutomata;
    automata *enumerationAutomata;
} windows_interface_state_t;

static void freeAutomata(automata *autom) {
    if (!autom) {
        return;
    }
    free(autom->extra);
    free(autom);
}

static void SignalHandler(int Signal) {
    log_debug("Interrupted by signal #%d", Signal);
    exitFlag = 1;
}

#ifdef _WIN32
static BOOL WINAPI ConsoleHandler(DWORD type) {
    (void)type;
    exitFlag = 1;
    return TRUE;
}
#endif

int runWindowsBackend(const windows_backend_ops_t *backend) {
    if (!backend) {
        return 1;
    }

    if (!backend->initialize()) {
        log_err("Windows backend initialization failed.");
        return 1;
    }

    windows_interface_list_t list = {0};
    if (!backend->listInterfaces(&list) || list.count == 0) {
        log_err("No Windows interfaces found.");
        backend->shutdown();
        return 1;
    }

    windows_interface_state_t *states = calloc(list.count, sizeof(*states));
    if (!states) {
        backend->freeInterfaceList(&list);
        backend->shutdown();
        return 1;
    }

    size_t startedCount = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (!backend->openInterface(list.names[i], &states[startedCount].handle)) {
            continue;
        }
        states[startedCount].mappingAutomata = init_automata_mapping();
        states[startedCount].sessionAutomata = init_automata_session();
        states[startedCount].enumerationAutomata = init_automata_enumeration();
        startedCount++;
    }

    while (!exitFlag) {
        for (size_t i = 0; i < startedCount; i++) {
            uint8_t buffer[2048];
            int bytes = backend->receiveFrame(&states[i].handle, buffer, sizeof(buffer));
            if (bytes <= 0) {
                continue;
            }
            lltd_demultiplex_header_t *header = (lltd_demultiplex_header_t *)buffer;
            switch_state_mapping(states[i].mappingAutomata, header->opcode, "rx");
            switch_state_session(states[i].sessionAutomata, header->opcode, "rx");
            if (header->opcode == opcode_hello) {
                switch_state_enumeration(states[i].enumerationAutomata, enum_hello, "rx");
            } else if (header->opcode == opcode_discover) {
                switch_state_enumeration(states[i].enumerationAutomata, enum_new_session, "rx");
            } else {
                switch_state_enumeration(states[i].enumerationAutomata, enum_sess_complete, "rx");
            }
        }
    }

    for (size_t i = 0; i < startedCount; i++) {
        backend->closeInterface(&states[i].handle);
        freeAutomata(states[i].mappingAutomata);
        freeAutomata(states[i].sessionAutomata);
        freeAutomata(states[i].enumerationAutomata);
    }
    free(states);
    backend->freeInterfaceList(&list);
    backend->shutdown();
    return 0;
}

void installWindowsSignals(void) {
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
    (void)SignalHandler;
#endif
}
