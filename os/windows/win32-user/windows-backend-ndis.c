/******************************************************************************
 *                                                                            *
 *   windows-backend-ndis.c                                                   *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"
#include "windows-backend.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#endif

static bool ndisInitialize(void) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        log_err("WSAStartup failed");
        return false;
    }
#endif
    return true;
}

static void ndisShutdown(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static bool ndisListInterfaces(windows_interface_list_t *list) {
    if (!list) {
        return false;
    }
#ifdef _WIN32
    ULONG size = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &size);
    IP_ADAPTER_ADDRESSES *buffer = malloc(size);
    if (!buffer) {
        return false;
    }
    if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, buffer, &size) != NO_ERROR) {
        free(buffer);
        return false;
    }

    size_t count = 0;
    for (IP_ADAPTER_ADDRESSES *adapter = buffer; adapter; adapter = adapter->Next) {
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        count++;
    }

    list->names = calloc(count, sizeof(char *));
    list->count = 0;
    if (!list->names) {
        free(buffer);
        return false;
    }

    for (IP_ADAPTER_ADDRESSES *adapter = buffer; adapter; adapter = adapter->Next) {
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        if (adapter->AdapterName) {
            list->names[list->count++] = _strdup(adapter->AdapterName);
        }
    }

    free(buffer);
    return list->count > 0;
#else
    list->names = NULL;
    list->count = 0;
    return false;
#endif
}

static void ndisFreeInterfaceList(windows_interface_list_t *list) {
    if (!list || !list->names) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->names[i]);
    }
    free(list->names);
    list->names = NULL;
    list->count = 0;
}

static bool ndisOpenInterface(const char *name, windows_interface_handle_t *handle) {
    (void)name;
    if (!handle) {
        return false;
    }
    memset(handle, 0, sizeof(*handle));
    return true;
}

static void ndisCloseInterface(windows_interface_handle_t *handle) {
    if (!handle) {
        return;
    }
    handle->handle = NULL;
}

static int ndisReceiveFrame(windows_interface_handle_t *handle, void *buffer, size_t length) {
    (void)handle;
    (void)buffer;
    (void)length;
    return 0;
}

static int ndisSendFrame(windows_interface_handle_t *handle, const void *buffer, size_t length) {
    (void)handle;
    (void)buffer;
    (void)length;
    return 0;
}

static windows_backend_ops_t ndisOps = {
    .initialize = ndisInitialize,
    .shutdown = ndisShutdown,
    .listInterfaces = ndisListInterfaces,
    .freeInterfaceList = ndisFreeInterfaceList,
    .openInterface = ndisOpenInterface,
    .closeInterface = ndisCloseInterface,
    .receiveFrame = ndisReceiveFrame,
    .sendFrame = ndisSendFrame
};

const windows_backend_ops_t *getWindowsBackendNdis(void) {
    return &ndisOps;
}
