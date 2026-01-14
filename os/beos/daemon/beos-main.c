/******************************************************************************
 *                                                                            *
 *   beos-main.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "../../daemon/lltdDaemon.h"
#include "../../../lltdResponder/lltdAutomata.h"
#include "beos-platform.h"

int main(int argc, const char *argv[]) {
    (void)argc;
    (void)argv;

#ifdef __HAIKU__
    return runHaikuResponder();
#else
    return runBeosLegacyResponder();
#endif
}
