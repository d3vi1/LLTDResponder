/******************************************************************************
 *                                                                            *
 *   windows-shared.h                                                         *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTDD_WINDOWS_SHARED_H
#define LLTDD_WINDOWS_SHARED_H

#include "windows-backend.h"

int runWindowsBackend(const windows_backend_ops_t *backend);
void installWindowsSignals(void);

#endif /* LLTDD_WINDOWS_SHARED_H */
