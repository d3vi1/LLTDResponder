#ifndef LLTD_RESPONDER_ENDIAN_H
#define LLTD_RESPONDER_ENDIAN_H

#include <stdint.h>

static inline uint16_t lltd_bswap16(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static inline uint32_t lltd_bswap32(uint32_t value) {
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) |
           ((value & 0xFF000000u) >> 24);
}

static inline int lltd_is_little_endian(void) {
    const uint16_t one = 1;
    return *((const uint8_t *)&one) == 1;
}

static inline uint16_t lltd_htons(uint16_t value) {
    return lltd_is_little_endian() ? lltd_bswap16(value) : value;
}

static inline uint16_t lltd_ntohs(uint16_t value) {
    return lltd_htons(value);
}

static inline uint32_t lltd_htonl(uint32_t value) {
    return lltd_is_little_endian() ? lltd_bswap32(value) : value;
}

static inline uint32_t lltd_ntohl(uint32_t value) {
    return lltd_htonl(value);
}

#endif

