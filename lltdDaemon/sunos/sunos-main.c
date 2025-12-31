/******************************************************************************
 *                                                                            *
 *   sunos-main.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../lltdDaemon.h"
#include "../lltdAutomata.h"
#include "sunos-dlpi.h"

#include <signal.h>
#include <stdlib.h>

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    return runSunosDlpi();
}
