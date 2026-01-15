#ifndef LLTD_RESPONDER_WIRE_UTILS_H
#define LLTD_RESPONDER_WIRE_UTILS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "lltdProtocol.h"

static const ethernet_address_t EthernetBroadcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

bool compareEthernetAddress(const ethernet_address_t *A, const ethernet_address_t *B);

size_t setLltdHeader(void *buffer,
                     ethernet_address_t *source,
                     ethernet_address_t *destination,
                     uint16_t seqNumber,
                     uint8_t opcode,
                     uint8_t tos);

size_t setLltdHeaderEx(void *buffer,
                       const ethernet_address_t *ethSource,
                       const ethernet_address_t *ethDest,
                       const ethernet_address_t *realSource,
                       const ethernet_address_t *realDest,
                       uint16_t seqNumber,
                       uint8_t opcode,
                       uint8_t tos);

size_t setHelloHeader(void *buffer,
                      uint64_t offset,
                      ethernet_address_t *apparentMapper,
                      ethernet_address_t *currentMapper,
                      uint16_t generation);

#endif
