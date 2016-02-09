//
//  lltdDaemon.h
//  LLMNRd
//
//  Created by Răzvan Corneliu C.R. VILT on 09.02.2016.
//  Copyright © 2016 Răzvan Corneliu C.R. VILT. All rights reserved.
//

#ifndef lltdDaemon_h
#define lltdDaemon_h

#ifdef __APPLE__
#include "darwin-ops.h"
#include "darwin-main.h"
#endif /*__darwin__ */

#ifdef __linux__
#include "linux-ops.h"
#include "linux-main.h"
#endif

#ifdef __FreeBSD__
#include "linux-ops.h"
#include "linux-main.h"
#endif

#ifdef __sunos_5_10
#include "sunos-ops.h"
#include "sunos-main.h"
#endif

#ifdef __BEOS__
#include "beos-ops.h"
#include "beos-main.h"
#endif

#endif /* lltdDaemon_h */
