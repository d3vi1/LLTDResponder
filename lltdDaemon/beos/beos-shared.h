/******************************************************************************
 *                                                                            *
 *   beos-shared.h                                                            *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDD_BEOS_SHARED_H
#define LLTDD_BEOS_SHARED_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char **names;
    size_t count;
} beos_interface_list_t;

int runBeosResponder(beos_interface_list_t *(*listInterfaces)(void), void (*freeList)(beos_interface_list_t *));

#endif /* LLTDD_BEOS_SHARED_H */
