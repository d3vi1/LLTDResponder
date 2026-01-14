/******************************************************************************
 *                                                                            *
 *   windows-main.c                                                           *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"
#include "windows-backend.h"
#include "windows-shared.h"

windows_global_info_t globalInfo;

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    installWindowsSignals();

#if defined(_WIN32)
    log_notice("Windows backend: NDIS/Packet");
    return runWindowsBackend(getWindowsBackendNdis());
#else
    log_notice("Windows backend: Legacy");
    return runWindowsBackend(getWindowsBackendLegacy());
#endif
}
