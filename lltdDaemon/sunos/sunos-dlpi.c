/******************************************************************************
 *                                                                            *
 *   sunos-dlpi.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../lltdDaemon.h"
#include "../lltdAutomata.h"
#include "../lltdBlock.h"

#include <alloca.h>
#include <fcntl.h>
#include <libdlpi.h>
#include <libnwam.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t exitFlag = 0;

typedef struct {
    char *name;
    dlpi_handle_t handle;
    pthread_t thread;
    automata *mappingAutomata;
    automata *sessionAutomata;
    automata *enumerationAutomata;
} sunos_interface_t;

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

static void freeInterfaceList(char **list, size_t count) {
    if (!list) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}

static int collectNwamLinks(char ***names, size_t *count) {
    nwam_error_t err;
    nwam_ncp_handle_t ncp = NULL;
    nwam_ncu_handle_t *ncus = NULL;
    uint_t ncuCount = 0;

    err = nwam_ncp_get_active(&ncp);
    if (err != NWAM_SUCCESS) {
        return -1;
    }

    err = nwam_ncp_get_ncus(ncp, &ncus, &ncuCount);
    if (err != NWAM_SUCCESS) {
        nwam_ncp_free(ncp);
        return -1;
    }

    char **local = calloc(ncuCount, sizeof(char *));
    size_t localCount = 0;
    for (uint_t i = 0; i < ncuCount; i++) {
        nwam_ncu_class_t ncuClass;
        if (nwam_ncu_get_ncu_class(ncus[i], &ncuClass) != NWAM_SUCCESS || ncuClass != NWAM_NCU_CLASS_LINK) {
            continue;
        }
        char *name = NULL;
        if (nwam_ncu_get_name(ncus[i], &name) == NWAM_SUCCESS && name) {
            local[localCount++] = strdup(name);
            free(name);
        }
    }

    nwam_ncp_free_ncus(ncus, ncuCount);
    nwam_ncp_free(ncp);

    if (localCount == 0) {
        free(local);
        return -1;
    }

    *names = local;
    *count = localCount;
    return 0;
}

static void *dlpiLoop(void *data) {
    sunos_interface_t *state = data;
    uint8_t buffer[2048];
    size_t length = sizeof(buffer);

    while (!exitFlag) {
        length = sizeof(buffer);
        if (dlpi_recv(state->handle, NULL, buffer, &length, DLPI_NOFLAGS) != DLPI_SUCCESS) {
            continue;
        }
        if (length < sizeof(lltd_demultiplex_header_t)) {
            continue;
        }
        lltd_demultiplex_header_t *header = (lltd_demultiplex_header_t *)buffer;
        switch_state_mapping(state->mappingAutomata, header->opcode, "rx");
        switch_state_session(state->sessionAutomata, header->opcode, "rx");
        if (header->opcode == opcode_hello) {
            switch_state_enumeration(state->enumerationAutomata, enum_hello, "rx");
        } else if (header->opcode == opcode_discover) {
            switch_state_enumeration(state->enumerationAutomata, enum_new_session, "rx");
        } else {
            switch_state_enumeration(state->enumerationAutomata, enum_sess_complete, "rx");
        }
    }

    return NULL;
}

int runSunosDlpi(void) {
    if (signal(SIGINT, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGINT handler.");
    }
    if (signal(SIGTERM, SignalHandler) == SIG_ERR) {
        log_err("ERROR: Could not establish SIGTERM handler.");
    }

    char **names = NULL;
    size_t count = 0;
    if (collectNwamLinks(&names, &count) != 0) {
        log_err("NWAM did not return any active links.");
        return 1;
    }

    sunos_interface_t *states = calloc(count, sizeof(*states));
    if (!states) {
        freeInterfaceList(names, count);
        return 1;
    }

    size_t started = 0;
    for (size_t i = 0; i < count; i++) {
        dlpi_handle_t handle;
        if (dlpi_open(names[i], &handle, 0) != DLPI_SUCCESS) {
            continue;
        }
        if (dlpi_bind(handle, lltdEtherType, 0) != DLPI_SUCCESS) {
            dlpi_close(handle);
            continue;
        }
        states[started].name = names[i];
        states[started].handle = handle;
        states[started].mappingAutomata = init_automata_mapping();
        states[started].sessionAutomata = init_automata_session();
        states[started].enumerationAutomata = init_automata_enumeration();
        pthread_create(&states[started].thread, NULL, dlpiLoop, &states[started]);
        started++;
    }

    while (!exitFlag) {
        sleep(1);
    }

    for (size_t i = 0; i < started; i++) {
        pthread_join(states[i].thread, NULL);
        dlpi_close(states[i].handle);
        freeAutomata(states[i].mappingAutomata);
        freeAutomata(states[i].sessionAutomata);
        freeAutomata(states[i].enumerationAutomata);
    }
    free(states);
    freeInterfaceList(names, count);
    return 0;
}
