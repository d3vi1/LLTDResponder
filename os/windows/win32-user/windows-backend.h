/******************************************************************************
 *                                                                            *
 *   windows-backend.h                                                        *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDD_WINDOWS_BACKEND_H
#define LLTDD_WINDOWS_BACKEND_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char **names;
    size_t count;
} windows_interface_list_t;

typedef struct {
    void *handle;
    unsigned char macAddress[6];
    unsigned int mtu;
} windows_interface_handle_t;

typedef struct {
    bool (*initialize)(void);
    void (*shutdown)(void);
    bool (*listInterfaces)(windows_interface_list_t *list);
    void (*freeInterfaceList)(windows_interface_list_t *list);
    bool (*openInterface)(const char *name, windows_interface_handle_t *handle);
    void (*closeInterface)(windows_interface_handle_t *handle);
    int (*receiveFrame)(windows_interface_handle_t *handle, void *buffer, size_t length);
    int (*sendFrame)(windows_interface_handle_t *handle, const void *buffer, size_t length);
} windows_backend_ops_t;

const windows_backend_ops_t *getWindowsBackendNdis(void);
const windows_backend_ops_t *getWindowsBackendLegacy(void);

#endif /* LLTDD_WINDOWS_BACKEND_H */
