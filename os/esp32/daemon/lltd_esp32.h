/******************************************************************************
 *                                                                            *
 *   lltd_esp32.h                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Draft ESP32 integration as a reusable library.                           *
 *                                                                            *
 ******************************************************************************/

#ifndef LLTD_ESP32_H
#define LLTD_ESP32_H

#include <stddef.h>
#include <stdint.h>

#include "../../../lltdResponder/lltdAutomata.h"
#include "../../../lltdResponder/lltdBlock.h"

typedef struct {
    automata *mapping;
    automata *session;
    automata *enumeration;
    const uint8_t *icon;
    size_t icon_size;
    const char *hostname;
    const char *uuid;
} lltd_esp32_ctx_t;

void lltd_esp32_init(lltd_esp32_ctx_t *ctx);
void lltd_esp32_set_identity(lltd_esp32_ctx_t *ctx, const char *hostname, const char *uuid);
const uint8_t *lltd_esp32_get_icon(size_t *size);
void lltd_esp32_handle_frame(lltd_esp32_ctx_t *ctx, const void *frame, size_t length);

#endif /* LLTD_ESP32_H */
