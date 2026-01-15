#include "lltdWire.h"

#include "lltdEndian.h"

bool compareEthernetAddress(const ethernet_address_t *A, const ethernet_address_t *B) {
    return A->a[0]==B->a[0] &&
           A->a[1]==B->a[1] &&
           A->a[2]==B->a[2] &&
           A->a[3]==B->a[3] &&
           A->a[4]==B->a[4] &&
           A->a[5]==B->a[5];
}

size_t setLltdHeader(void *buffer,
                     ethernet_address_t *source,
                     ethernet_address_t *destination,
                     uint16_t seqNumber,
                     uint8_t opcode,
                     uint8_t tos) {

    lltd_demultiplex_header_t *lltdHeader = (lltd_demultiplex_header_t *)buffer;

    lltdHeader->frameHeader.ethertype = lltd_htons(lltdEtherType);
    lltdHeader->frameHeader.source = *source;
    lltdHeader->frameHeader.destination = *destination;
    lltdHeader->realSource = *source;
    lltdHeader->realDestination = *destination;
    lltdHeader->seqNumber = lltd_htons(seqNumber);
    lltdHeader->opcode = opcode;
    lltdHeader->tos = tos;
    lltdHeader->version = 1;

    return sizeof(lltd_demultiplex_header_t);
}

size_t setLltdHeaderEx(void *buffer,
                       const ethernet_address_t *ethSource,
                       const ethernet_address_t *ethDest,
                       const ethernet_address_t *realSource,
                       const ethernet_address_t *realDest,
                       uint16_t seqNumber,
                       uint8_t opcode,
                       uint8_t tos) {

    lltd_demultiplex_header_t *lltdHeader = (lltd_demultiplex_header_t *)buffer;

    lltdHeader->frameHeader.ethertype = lltd_htons(lltdEtherType);
    lltdHeader->frameHeader.source = *ethSource;
    lltdHeader->frameHeader.destination = *ethDest;
    lltdHeader->realSource = *realSource;
    lltdHeader->realDestination = *realDest;
    lltdHeader->seqNumber = lltd_htons(seqNumber);
    lltdHeader->opcode = opcode;
    lltdHeader->tos = tos;
    lltdHeader->version = 1;

    return sizeof(lltd_demultiplex_header_t);
}

size_t setHelloHeader(void *buffer,
                      uint64_t offset,
                      ethernet_address_t *apparentMapper,
                      ethernet_address_t *currentMapper,
                      uint16_t generation) {

    lltd_hello_upper_header_t *helloHeader =
        (lltd_hello_upper_header_t *)((uint8_t *)buffer + offset);

    helloHeader->apparentMapper = *apparentMapper;
    helloHeader->currentMapper = *currentMapper;
    helloHeader->generation = lltd_htons(generation);

    return sizeof(lltd_hello_upper_header_t);
}

