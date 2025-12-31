/******************************************************************************
 *                                                                            *
 *   beos-legacy-main.c                                                       *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../lltdDaemon.h"
#include "beos-shared.h"

#include <net/if.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static beos_interface_list_t *listInterfacesLegacy(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return NULL;
    }

    struct ifconf ifc;
    char buffer[8192];
    memset(&ifc, 0, sizeof(ifc));
    ifc.ifc_len = sizeof(buffer);
    ifc.ifc_buf = buffer;

    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
        close(sock);
        return NULL;
    }

    beos_interface_list_t *list = calloc(1, sizeof(*list));
    if (!list) {
        close(sock);
        return NULL;
    }

    struct ifreq *ifr = ifc.ifc_req;
    int count = ifc.ifc_len / sizeof(struct ifreq);
    for (int i = 0; i < count; i++) {
        struct ifreq *entry = &ifr[i];
        if (entry->ifr_name[0] == '\0') {
            continue;
        }

        char **expanded = realloc(list->names, sizeof(char *) * (list->count + 1));
        if (!expanded) {
            continue;
        }
        list->names = expanded;
        list->names[list->count] = strdup(entry->ifr_name);
        if (list->names[list->count]) {
            list->count++;
        }
    }

    close(sock);
    return list;
}

static void freeInterfaceListLegacy(beos_interface_list_t *list) {
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->names[i]);
    }
    free(list->names);
    free(list);
}

int runBeosLegacyResponder(void) {
    return runBeosResponder(listInterfacesLegacy, freeInterfaceListLegacy);
}
