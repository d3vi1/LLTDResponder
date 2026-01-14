/******************************************************************************
 *                                                                            *
 *   sunos-main.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"
#include "../../../lltdResponder/lltdAutomata.h"
#include "sunos-dlpi.h"

#include <signal.h>
#include <stdlib.h>

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

    return runSunosDlpi();
}
