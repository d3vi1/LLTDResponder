/******************************************************************************
 *                                                                            *
 *   lltd_esp32.c                                                             *
 *   lltdDaemon                                                               *
 *                                                                            *
 ******************************************************************************/

#include "lltd_esp32.h"

#ifndef LLTD_ESP32_ICON_DATA
static const uint8_t lltd_esp32_default_icon[] = { 0x00 };
#define LLTD_ESP32_ICON_DATA lltd_esp32_default_icon
#define LLTD_ESP32_ICON_SIZE sizeof(lltd_esp32_default_icon)
#endif

#ifndef enum_sess_complete
#define enum_sess_complete     0x00
#define enum_sess_not_complete 0x01
#define enum_hello             0x02
#define enum_new_session       0x03
#endif

const uint8_t *lltd_esp32_get_icon(size_t *size) {
    if (size) {
        *size = LLTD_ESP32_ICON_SIZE;
    }
    return LLTD_ESP32_ICON_DATA;
}

void lltd_esp32_init(lltd_esp32_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    ctx->mapping = init_automata_mapping();
    ctx->session = init_automata_session();
    ctx->enumeration = init_automata_enumeration();
    ctx->icon = LLTD_ESP32_ICON_DATA;
    ctx->icon_size = LLTD_ESP32_ICON_SIZE;
    ctx->hostname = NULL;
    ctx->uuid = NULL;
}

void lltd_esp32_set_identity(lltd_esp32_ctx_t *ctx, const char *hostname, const char *uuid) {
    if (!ctx) {
        return;
    }
    ctx->hostname = hostname;
    ctx->uuid = uuid;
}

void lltd_esp32_handle_frame(lltd_esp32_ctx_t *ctx, const void *frame, size_t length) {
    if (!ctx || !frame || length < sizeof(lltd_demultiplex_header_t)) {
        return;
    }

    const lltd_demultiplex_header_t *header = frame;
    switch_state_mapping(ctx->mapping, header->opcode, "rx");
    switch_state_session(ctx->session, header->opcode, "rx");
    if (header->opcode == opcode_hello) {
        switch_state_enumeration(ctx->enumeration, enum_hello, "rx");
    } else if (header->opcode == opcode_discover) {
        switch_state_enumeration(ctx->enumeration, enum_new_session, "rx");
    } else {
        switch_state_enumeration(ctx->enumeration, enum_sess_complete, "rx");
    }
}
