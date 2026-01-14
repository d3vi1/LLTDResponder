/******************************************************************************
 *                                                                            *
 *   haiku-main.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"
#include "beos-shared.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static beos_interface_list_t *listInterfacesHaiku(void) {
    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) != 0) {
        return NULL;
    }

    beos_interface_list_t *list = calloc(1, sizeof(*list));
    if (!list) {
        freeifaddrs(ifaddr);
        return NULL;
    }

    for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_name || !(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }

        bool exists = false;
        for (size_t i = 0; i < list->count; i++) {
            if (strcmp(list->names[i], ifa->ifa_name) == 0) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        char **expanded = realloc(list->names, sizeof(char *) * (list->count + 1));
        if (!expanded) {
            continue;
        }
        list->names = expanded;
        list->names[list->count] = strdup(ifa->ifa_name);
        if (list->names[list->count]) {
            list->count++;
        }
    }

    freeifaddrs(ifaddr);
    return list;
}

static void freeInterfaceListHaiku(beos_interface_list_t *list) {
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->names[i]);
    }
    free(list->names);
    free(list);
}

int runHaikuResponder(void) {
    return runBeosResponder(listInterfacesHaiku, freeInterfaceListHaiku);
}
