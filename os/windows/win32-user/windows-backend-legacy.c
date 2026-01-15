/******************************************************************************
 *                                                                            *
 *   windows-backend-legacy.c                                                 *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"
#include "windows-backend.h"

static bool legacyInitialize(void) {
    return true;
}

static void legacyShutdown(void) {
}

static bool legacyListInterfaces(windows_interface_list_t *list) {
    if (!list) {
        return false;
    }
    list->names = NULL;
    list->count = 0;
    return false;
}

static void legacyFreeInterfaceList(windows_interface_list_t *list) {
    if (!list) {
        return;
    }
    list->names = NULL;
    list->count = 0;
}

static bool legacyOpenInterface(const char *name, windows_interface_handle_t *handle) {
    (void)name;
    if (!handle) {
        return false;
    }
    memset(handle, 0, sizeof(*handle));
    return true;
}

static void legacyCloseInterface(windows_interface_handle_t *handle) {
    if (!handle) {
        return;
    }
    handle->handle = NULL;
}

static int legacyReceiveFrame(windows_interface_handle_t *handle, void *buffer, size_t length) {
    (void)handle;
    (void)buffer;
    (void)length;
    return 0;
}

static int legacySendFrame(windows_interface_handle_t *handle, const void *buffer, size_t length) {
    (void)handle;
    (void)buffer;
    (void)length;
    return 0;
}

static windows_backend_ops_t legacyOps = {
    .initialize = legacyInitialize,
    .shutdown = legacyShutdown,
    .listInterfaces = legacyListInterfaces,
    .freeInterfaceList = legacyFreeInterfaceList,
    .openInterface = legacyOpenInterface,
    .closeInterface = legacyCloseInterface,
    .receiveFrame = legacyReceiveFrame,
    .sendFrame = legacySendFrame
};

const windows_backend_ops_t *getWindowsBackendLegacy(void) {
    return &legacyOps;
}
