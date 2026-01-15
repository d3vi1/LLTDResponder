/******************************************************************************
 *                                                                            *
 *   lltdBlock.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 23.03.2014.                      *
 *   Copyright (c) 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.  *
 *                                                                            *
 ******************************************************************************/

#include "lltdDaemon.h"

#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

static uint16_t lltd_bswap16(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint64_t lltd_now_ms(void) {
    return lltd_monotonic_milliseconds();
}

static char lltd_hex_digit(uint8_t value) {
    value &= 0x0Fu;
    return (value < 10u) ? (char)('0' + value) : (char)('a' + (value - 10u));
}

static void format_mac_str(const uint8_t *mac, char *out, size_t out_len) {
    if (!mac || !out || out_len < 18) {
        return;
    }
    size_t pos = 0;
    for (size_t i = 0; i < 6; i++) {
        out[pos++] = lltd_hex_digit((uint8_t)(mac[i] >> 4));
        out[pos++] = lltd_hex_digit(mac[i]);
        if (i != 5) {
            out[pos++] = ':';
        }
    }
    out[pos] = '\0';
}

static void log_lltd_frame(const char *direction,
                           const char *ifname,
                           uint8_t tos,
                           uint8_t opcode,
                           uint16_t xid,
                           uint16_t generation,
                           uint16_t seq,
                           const char *reason) {
    if (!direction) {
        direction = "UNK";
    }
    if (!ifname) {
        ifname = "?";
    }
    if (reason) {
        log_debug("t=%llu %s %s tos=%u op=%u xid=0x%04x gen=0x%04x seq=0x%04x reason=%s",
                  (unsigned long long)lltd_now_ms(),
                  direction, ifname, tos, opcode, xid, generation, seq, reason);
    } else {
        log_debug("t=%llu %s %s tos=%u op=%u xid=0x%04x gen=0x%04x seq=0x%04x",
                  (unsigned long long)lltd_now_ms(),
                  direction, ifname, tos, opcode, xid, generation, seq);
    }
}

static void log_generation_warning_if_swapped(network_interface_t *iface,
                                              uint16_t generation_host,
                                              uint8_t tos,
                                              const char *context) {
    uint16_t prior = (tos == tos_quick_discovery)
        ? iface->MapperGenerationQuick
        : iface->MapperGenerationTopology;
    if (prior != 0 && generation_host == lltd_bswap16(prior)) {
        log_warning("%s: %s tos=%u generation looks byte-swapped: prev=0x%04x now=0x%04x",
                    iface->deviceName, context, tos, prior, generation_host);
    }
}

static uint16_t *mapper_generation_for_tos(network_interface_t *iface, uint8_t tos) {
    return (tos == tos_quick_discovery)
        ? &iface->MapperGenerationQuick
        : &iface->MapperGenerationTopology;
}

static const char *mapper_generation_label(uint8_t tos) {
    return (tos == tos_quick_discovery) ? "quick" : "topology";
}

static bool mapper_matches(network_interface_t *iface, const ethernet_address_t *real_src) {
    if (!iface->MapperKnown) {
        return true;
    }
    return compareEthernetAddress((const ethernet_address_t *)iface->MapperHwAddress, real_src);
}

static void set_active_mapper(network_interface_t *iface,
                              const ethernet_address_t *real_src,
                              const ethernet_address_t *eth_src) {
    if (!iface->MapperKnown) {
        memcpy(iface->MapperHwAddress, real_src->a, sizeof(iface->MapperHwAddress));
        memcpy(iface->MapperApparentAddress, eth_src->a, sizeof(iface->MapperApparentAddress));
        iface->MapperKnown = 1;
    }
}

static void warn_generation_store_mismatch(network_interface_t *iface,
                                           uint8_t tos,
                                           const char *context,
                                           const uint16_t *slot) {
    if (tos == tos_quick_discovery && slot != &iface->MapperGenerationQuick) {
        log_warning("%s: %s tos=%u using topology generation for quick hello",
                    iface->deviceName, context, tos);
    } else if (tos == tos_discovery && slot != &iface->MapperGenerationTopology) {
        log_warning("%s: %s tos=%u using quick generation for topology hello",
                    iface->deviceName, context, tos);
    }
}

boolean_t sendProbeMsg(ethernet_address_t src, ethernet_address_t dst, void *networkInterface, int pause, uint8_t type, boolean_t ack) {
    network_interface_t *currentNetworkInterface = networkInterface;
    size_t packageSize = sizeof(lltd_demultiplex_header_t);
    lltd_demultiplex_header_t *probe = malloc(packageSize);
    if (!probe) {
        log_crit("sendProbeMsg: malloc failed");
        return false;
    }
    memset(probe, 0, packageSize);

    uint8_t code = (type == 0x01) ? opcode_probe : opcode_train;

    /*
     * Probe/Train frames per MS-LLTD:
     * - Ethernet dest = emitee dest (apparent, next-hop)
     * - Ethernet src  = emitee src (we're spoofing the sender)
     * - LLTD realDest = mapper's real address (end-to-end)
     * - LLTD realSrc  = our MAC (end-to-end identity)
     * The emitee src/dst are what the mapper told us to use on the wire.
     */
    setLltdHeaderEx(probe,
                    (const ethernet_address_t *)&src,                               /* ethSource: emitee src */
                    (const ethernet_address_t *)&dst,                               /* ethDest: emitee dst */
                    (const ethernet_address_t *)&currentNetworkInterface->macAddress,  /* realSource: our MAC */
                    (const ethernet_address_t *)currentNetworkInterface->MapperHwAddress, /* realDest: mapper real */
                    0, code, tos_discovery);

    log_lltd_frame("TX",
                   currentNetworkInterface->deviceName,
                   tos_discovery,
                   code,
                   0,
                   0,
                   0,
                   "probe_train");

    usleep(1000 * pause);
    ssize_t written = sendto(currentNetworkInterface->socket, probe, packageSize, 0,
                             (struct sockaddr *)&currentNetworkInterface->socketAddr,
                             sizeof(currentNetworkInterface->socketAddr));
    if (written < 0) {
        int err = errno;
        log_crit("sendProbeMsg: sendto failed on %s (%zu bytes, opcode=%d): errno %d (%s)",
                 currentNetworkInterface->deviceName, packageSize, code, err, strerror(err));
        free(probe);
        return false;
    }

    if (ack) {
        /*
         * ACK frame back to mapper:
         * - Ethernet dest = apparent mapper address (next-hop through bridge)
         * - LLTD realDest = real mapper address (end-to-end identity)
         */
        setLltdHeaderEx(probe,
                        (const ethernet_address_t *)&currentNetworkInterface->macAddress,
                        (const ethernet_address_t *)currentNetworkInterface->MapperApparentAddress,
                        (const ethernet_address_t *)&currentNetworkInterface->macAddress,
                        (const ethernet_address_t *)currentNetworkInterface->MapperHwAddress,
                        currentNetworkInterface->MapperSeqNumber, opcode_ack, tos_discovery);

        log_lltd_frame("TX",
                       currentNetworkInterface->deviceName,
                       tos_discovery,
                       opcode_ack,
                       0,
                       0,
                       currentNetworkInterface->MapperSeqNumber,
                       "probe_ack");
        written = sendto(currentNetworkInterface->socket, probe, packageSize, 0,
                         (struct sockaddr *)&currentNetworkInterface->socketAddr,
                         sizeof(currentNetworkInterface->socketAddr));
        if (written < 0) {
            int err = errno;
            log_crit("sendProbeMsg: ACK sendto failed on %s: errno %d (%s)",
                     currentNetworkInterface->deviceName, err, strerror(err));
        } else {
            log_debug("sendProbeMsg: ACK sent to mapper");
        }
    }

    free(probe);
    return true;
}

//TODO: validate Query
void parseQuery(void *inFrame, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    lltd_demultiplex_header_t *inHeader = (lltd_demultiplex_header_t *)inFrame;

    /*
     * Debug log for Query frame (task D requirement).
     */
    log_lltd_frame("RX",
                   currentNetworkInterface->deviceName,
                   inHeader->tos,
                   inHeader->opcode,
                   0,
                   0,
                   ntohs(inHeader->seqNumber),
                   NULL);

    /*
     * Update mapper session info from Query frame.
     * Query is a session-defining frame.
     */
    currentNetworkInterface->MapperSeqNumber = ntohs(inHeader->seqNumber);
    memcpy(currentNetworkInterface->MapperHwAddress, inHeader->realSource.a, 6);
    memcpy(currentNetworkInterface->MapperApparentAddress, inHeader->frameHeader.source.a, 6);

    size_t packageSize = currentNetworkInterface->MTU + sizeof(ethernet_header_t);
    size_t offset = 0;
    void *buffer = malloc(packageSize);
    memset(buffer, 0, packageSize);

    /*
     * Per MS-LLTD 2.2.3.6 (QueryResp Frame):
     * If Real Source Address != Ethernet source address (i.e., behind bridge/AP),
     * QueryResp MUST be sent to broadcast; otherwise send to Real Source.
     */
    const ethernet_address_t *destAddr;
    if (memcmp(inHeader->realSource.a, inHeader->frameHeader.source.a, sizeof(inHeader->realSource.a)) != 0) {
        log_debug("parseQuery: mapper behind bridge, using broadcast for QueryResp");
        destAddr = &EthernetBroadcast;
    } else {
        destAddr = &inHeader->realSource;
    }

    offset = setLltdHeader(buffer, (ethernet_address_t *)&(currentNetworkInterface->macAddress),
                           (ethernet_address_t *)destAddr,
                           currentNetworkInterface->MapperSeqNumber, opcode_queryResp, tos_discovery);
    
    qry_resp_upper_header_t *respH  = buffer + sizeof(lltd_demultiplex_header_t);
    // HERE I AM
    
    // NOTE: `seeList` is a linked list of `probe_t` nodes. The wire format of a QueryResp
    // descriptor must NOT include the host-only `nextProbe` pointer.
    typedef struct __attribute__((packed)) {
        uint16_t type;
        ethernet_address_t realSourceAddr;
        ethernet_address_t sourceAddr;
        ethernet_address_t destAddr;
    } lltd_probe_desc_wire_t;

    log_crit("I've seen %d probes", currentNetworkInterface->seeListCount);

    // TODO: split into multiple QueryResp frames if needed.
    respH->numDescs = htons(currentNetworkInterface->seeListCount);
    offset += sizeof(qry_resp_upper_header_t);

    probe_t *node = (probe_t *)currentNetworkInterface->seeList;
    long remaining = currentNetworkInterface->seeListCount;
    while (node != NULL && remaining > 0) {
        lltd_probe_desc_wire_t wire;
        memset(&wire, 0, sizeof(wire));
        wire.type = node->type; // already stored in network order
        wire.realSourceAddr = node->realSourceAddr;
        wire.sourceAddr = node->sourceAddr;
        wire.destAddr = node->destAddr;

        {
            char src_str[18];
            char dst_str[18];
            char real_str[18];
            format_mac_str(wire.sourceAddr.a, src_str, sizeof(src_str));
            format_mac_str(wire.destAddr.a, dst_str, sizeof(dst_str));
            format_mac_str(wire.realSourceAddr.a, real_str, sizeof(real_str));
            log_crit("\tType %d, Source: %s, Dest: %s, RealSource: %s",
                     ntohs(wire.type), src_str, dst_str, real_str);
        }

        if (offset + sizeof(wire) > packageSize) {
            log_crit("QryResp buffer too small (%zu) for %zu bytes", packageSize, offset + sizeof(wire));
            break;
        }
        memcpy(((uint8_t *)buffer) + offset, &wire, sizeof(wire));
        offset += sizeof(wire);

        probe_t *next = node->nextProbe;
        free(node);
        node = next;
        remaining--;
    }

    currentNetworkInterface->seeList = NULL;
    currentNetworkInterface->seeListCount = 0;
    
    
    log_lltd_frame("TX",
                   currentNetworkInterface->deviceName,
                   tos_discovery,
                   opcode_queryResp,
                   0,
                   0,
                   currentNetworkInterface->MapperSeqNumber,
                   "query_resp");
    ssize_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                          sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        int err = errno;
        log_crit("Socket write failed on QryResp: %s", strerror(err));
    }
    
    free(buffer);
}

static void sendLargeTlvResponse(void *networkInterface, void *inFrame, void *data, size_t dataSize, uint16_t dataOffset) {
    network_interface_t *currentNetworkInterface = networkInterface;
    lltd_demultiplex_header_t *inHeader = (lltd_demultiplex_header_t *)inFrame;
    uint16_t maxPayload = currentNetworkInterface->MTU - sizeof(lltd_demultiplex_header_t)
                        - sizeof(qry_large_tlv_resp_t);

    /*
     * Per MS-LLTD 2.2.3.8 (QueryLargeTlvResp Frame):
     * If Real Source Address != Ethernet source address (i.e., behind bridge/AP),
     * response MUST be sent to broadcast; otherwise send to Real Source.
     */
    const ethernet_address_t *destAddr;
    if (memcmp(inHeader->realSource.a, inHeader->frameHeader.source.a, sizeof(inHeader->realSource.a)) != 0) {
        destAddr = &EthernetBroadcast;
    } else {
        destAddr = &inHeader->realSource;
    }

    size_t bufferSize = sizeof(lltd_demultiplex_header_t) + sizeof(qry_large_tlv_resp_t) + maxPayload;
    void *buffer = malloc(bufferSize);
    memset(buffer, 0, bufferSize);

    setLltdHeader(buffer, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                          (ethernet_address_t *) destAddr,
                          currentNetworkInterface->MapperSeqNumber, opcode_queryLargeTlvResp, tos_discovery);

    qry_large_tlv_resp_t *header = buffer + sizeof(lltd_demultiplex_header_t);

    uint16_t bytesToWrite = 0;

    if (data == NULL || dataSize == 0) {
        // No data available - send empty response
        header->length = 0;
        log_crit("QueryLargeTLV: No data available, sending empty response");
    } else if (dataSize > dataOffset + maxPayload) {
        // More data to come
        bytesToWrite = maxPayload;
        header->length = bytesToWrite | 0x8000;  // Set "more" flag
    } else if (dataSize > dataOffset) {
        // Final chunk
        bytesToWrite = dataSize - dataOffset;
        header->length = bytesToWrite;
    } else {
        // Offset beyond data - empty response
        header->length = 0;
    }
    header->length = htons(header->length);

    size_t packageSize = sizeof(lltd_demultiplex_header_t) + sizeof(qry_large_tlv_resp_t) + bytesToWrite;
    if (bytesToWrite > 0 && data != NULL) {
        memcpy(buffer + sizeof(lltd_demultiplex_header_t) + sizeof(qry_large_tlv_resp_t),
               (uint8_t *)data + dataOffset, bytesToWrite);
    }

    log_lltd_frame("TX",
                   currentNetworkInterface->deviceName,
                   tos_discovery,
                   opcode_queryLargeTlvResp,
                   0,
                   0,
                   currentNetworkInterface->MapperSeqNumber,
                   "query_large_tlv_resp");
    ssize_t write = sendto(currentNetworkInterface->socket, buffer, packageSize, 0,
                           (struct sockaddr *) &currentNetworkInterface->socketAddr, sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        int err = errno;
        log_crit("Socket write failed on QueryLargeTLVResp: %s", strerror(err));
    }
    free(buffer);
}

void parseQueryLargeTlv(void *inFrame, void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;
    lltd_demultiplex_header_t *lltdHeader = inFrame;
    if (ntohs(lltdHeader->seqNumber) == 0) {
        // as per spec, ignore LargeTLV with zeroed sequence Number
        return;
    }
    qry_large_tlv_t *header = inFrame + sizeof(lltd_demultiplex_header_t);
    uint16_t offset = ntohs(header->offset);

    void *data = NULL;
    size_t dataSize = 0;

    switch (header->type) {
        case tlv_iconImage:
            log_crit("QueryLargeTLV: Icon Image request, offset=%d", offset);
            // Use cached icon if available
            if (globalInfo.smallIcon == NULL || globalInfo.smallIconSize == 0) {
                if (globalInfo.smallIcon) {
                    free(globalInfo.smallIcon);
                    globalInfo.smallIcon = NULL;
                }
                getIconImage(&data, &dataSize);
                globalInfo.smallIcon = data;
                globalInfo.smallIconSize = dataSize;
            } else {
                data = globalInfo.smallIcon;
                dataSize = globalInfo.smallIconSize;
            }
            break;

        case tlv_friendlyName:
            log_crit("QueryLargeTLV: Friendly Name request, offset=%d", offset);
            getFriendlyName((char **)&data, &dataSize);
            break;

        case tlv_hwIdProperty:
            log_crit("QueryLargeTLV: Hardware ID request, offset=%d", offset);
            // Hardware ID is max 64 bytes in UCS-2LE
            data = malloc(64);
            memset(data, 0, 64);
            getHwId(data);
            // Find actual length (look for null terminator in UCS-2LE)
            dataSize = 64;
            for (size_t i = 0; i < 64; i += 2) {
                if (((uint8_t *)data)[i] == 0 && ((uint8_t *)data)[i+1] == 0) {
                    dataSize = i;
                    break;
                }
            }
            break;

        default:
            log_crit("QueryLargeTLV: Unknown TLV type 0x%02x requested", header->type);
            // Send empty response for unknown types
            break;
    }

    sendLargeTlvResponse(networkInterface, inFrame, data, dataSize, offset);

    // Free non-cached data
    if (header->type == tlv_friendlyName || header->type == tlv_hwIdProperty) {
        if (data) {
            free(data);
        }
    }
}

void parseProbe(void *inFrame, void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;
    lltd_demultiplex_header_t *header = inFrame;

    /*
     * Debug log for mapping frame reception (task D requirement).
     * Log opcode, tos, seq, all addresses, and acceptance decision.
     */
    int forUs = compareEthernetAddress(&header->realDestination,
                                       (ethernet_address_t *)&currentNetworkInterface->macAddress);
    log_lltd_frame("RX",
                   currentNetworkInterface->deviceName,
                   header->tos,
                   header->opcode,
                   0,
                   0,
                   ntohs(header->seqNumber),
                   NULL);
    log_debug("parseProbe: forUs=%d", forUs);

    /*
     * Accept frames where LLTD realDestination == our MAC (end-to-end identity).
     * In bridged topologies, Ethernet destination may differ (rewritten by bridge/AP).
     */
    if (forUs) {
        probe_t *probe = malloc(sizeof(probe_t));
        
        // initialize the probe, then search for it in our array
        probe->type             = htons((header->opcode == opcode_probe) ? 1 : 0);
        probe->sourceAddr       = header->frameHeader.source;
        probe->destAddr         = header->frameHeader.destination;
        probe->realSourceAddr   = header->realSource;
        probe->nextProbe        = NULL;

        boolean_t found    = false;
        probe_t  *nextProbe = currentNetworkInterface->seeList;
        
        log_crit("Searching through already %d seen probes in the seenLinkedList", currentNetworkInterface->seeListCount);
        for(long i = 0; i < currentNetworkInterface->seeListCount; i++) {
            
                const probe_t *searchProbe = nextProbe;
                // destination and type are already equal, we'll compare just the source addresses
                {
                    char source1[18];
                    char source2[18];
                    char realSource1[18];
                    char realSource2[18];
                    format_mac_str(probe->sourceAddr.a, source1, sizeof(source1));
                    format_mac_str(searchProbe->sourceAddr.a, source2, sizeof(source2));
                    format_mac_str(probe->realSourceAddr.a, realSource1, sizeof(realSource1));
                    format_mac_str(searchProbe->realSourceAddr.a, realSource2, sizeof(realSource2));
                    log_crit("\tSource1: %s, Source2: %s, RealSource1: %s, RealSource2: %s",
                             source1, source2, realSource1, realSource2);
                }
                if ( compareEthernetAddress( &(probe->sourceAddr),     &(searchProbe->sourceAddr    )) &&
                     compareEthernetAddress( &(probe->realSourceAddr), &(searchProbe->realSourceAddr)) ) {
                    found = true;
                }
            
                // don't go past the last probe
                if (nextProbe->nextProbe != NULL) {
                    nextProbe=nextProbe->nextProbe;
                }

        }
        
        //If we've discovered a new probe from a new computer, we add it to the seelist
        if (!found) {
            //If the list is not initialized, we now initialize it
            if (currentNetworkInterface->seeListCount == 0){
                currentNetworkInterface->seeList      = probe;
                currentNetworkInterface->seeListCount = 1;
            //Otherwise, we just add to it
            } else {
                // we're already pointing to the last probe in `nextProbe`
                nextProbe->nextProbe=probe;
                currentNetworkInterface->seeListCount += 1;
                log_crit("Added probe to seen list. Now have %d probes.", currentNetworkInterface->seeListCount);
            }
        } else {
            free(probe);
        }
    }
}

void parseEmit(void *inFrame, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;

    lltd_demultiplex_header_t *lltdHeader = inFrame;
    lltd_emit_upper_header_t *emitHeader = (void *)((void *)lltdHeader + sizeof(*lltdHeader));

    /*
     * Debug log for Emit frame (task D requirement).
     */
    log_lltd_frame("RX",
                   currentNetworkInterface->deviceName,
                   lltdHeader->tos,
                   lltdHeader->opcode,
                   0,
                   0,
                   ntohs(lltdHeader->seqNumber),
                   NULL);
    log_debug("parseEmit: numDescs=%d", ntohs(emitHeader->numDescs));

    /*
     * Update mapper session info from Emit frame.
     * Emit is a session-defining frame that advances the mapping process.
     */
    currentNetworkInterface->MapperSeqNumber = ntohs(lltdHeader->seqNumber);
    memcpy(currentNetworkInterface->MapperHwAddress, lltdHeader->realSource.a, 6);
    memcpy(currentNetworkInterface->MapperApparentAddress, lltdHeader->frameHeader.source.a, 6);

    int numDescs = ntohs(emitHeader->numDescs);
    uint16_t offsetEmitee = 0;

    for (int i = 0; i < numDescs; i++) {
        boolean_t ack = (i == numDescs - 1) ? true : false;
        emitee_descs *emitee = (void *)emitHeader + sizeof(*emitHeader) + offsetEmitee;
        if (emitee->type == 1) {
            // Probe
            sendProbeMsg(emitee->sourceAddr, emitee->destAddr, networkInterface, emitee->pause, emitee->type, ack);
        } else if (emitee->type == 0) {
            // Train
            sendProbeMsg(emitee->sourceAddr, emitee->destAddr, networkInterface, emitee->pause, emitee->type, ack);
        } else {
            log_alert("parseEmit: Unknown emitee type=%d!", emitee->type);
        }
        offsetEmitee += sizeof(emitee_descs);
    }
}


//==============================================================================
//
// This is the Hello answer to any Discovery package.
// FIXME: Hello header casting is b0rken.
//
//==============================================================================
void answerHello(void *inFrame, void *networkInterface){

    network_interface_t *currentNetworkInterface = networkInterface;
    void                *buffer = malloc(currentNetworkInterface->MTU);
    size_t               offset = 0;

    memset(buffer, 0, currentNetworkInterface->MTU);
    
    lltd_demultiplex_header_t    *inFrameHeader  = inFrame;
    lltd_demultiplex_header_t    *lltdHeader     = buffer;
    // The Discover upper header immediately follows the demultiplex header.
    lltd_discover_upper_header_t *discoverHeader = (lltd_discover_upper_header_t *)((uint8_t *)inFrameHeader + sizeof(*inFrameHeader));
    
    //
    // Check if real MAC address != Ethernet src address.
    // This mismatch is expected behind bridges/APs where the Ethernet source
    // is rewritten. We log for diagnostics but continue processing.
    //
    if (!compareEthernetAddress(&inFrameHeader->realSource, &inFrameHeader->frameHeader.source)) {
        char real_src[18];
        char eth_src[18];
        format_mac_str(inFrameHeader->realSource.a, real_src, sizeof(real_src));
        format_mac_str(inFrameHeader->frameHeader.source.a, eth_src, sizeof(eth_src));
        log_debug("Discover: Real_Source_Address (%s) != Ethernet src (%s) - behind bridge/AP",
                  real_src, eth_src);
    }
    
    // Hello frames are unacknowledged and must carry Sequence Number = 0.
    offset = setLltdHeader(buffer, (ethernet_address_t *)&(currentNetworkInterface->macAddress), (ethernet_address_t *) &EthernetBroadcast,
                          0, opcode_hello, inFrameHeader->tos);
    
    // Store mapper info for later use (e.g., Query responses, Emit ACKs).
    // MapperHwAddress = real (LLTD-level) mapper address (end-to-end identity)
    // MapperApparentAddress = Ethernet source (next-hop, may be bridge MAC)
    if (!mapper_matches(currentNetworkInterface, &inFrameHeader->realSource)) {
        char mapper_id[18];
        format_mac_str(inFrameHeader->realSource.a, mapper_id, sizeof(mapper_id));
        log_debug("answerHello: ignoring Discover from non-active mapper mapper_id=%s",
                  mapper_id);
        return;
    }
    set_active_mapper(currentNetworkInterface, &inFrameHeader->realSource, &inFrameHeader->frameHeader.source);
    currentNetworkInterface->MapperSeqNumber = ntohs(inFrameHeader->seqNumber);
    uint16_t discover_generation = ntohs(discoverHeader->generation);
    uint16_t *generation_slot = mapper_generation_for_tos(currentNetworkInterface, inFrameHeader->tos);
    if (*generation_slot == 0 && discover_generation != 0) {
        *generation_slot = discover_generation;
    }
    warn_generation_store_mismatch(currentNetworkInterface, inFrameHeader->tos, "answerHello", generation_slot);
    uint64_t now_ms = lltd_now_ms();
    // Hello upper header per LLTD spec:
    //   apparentMapper = Ethernet header source MAC (may be bridge/AP behind NAT)
    //   currentMapper  = LLTD Real_Source_Address (true mapper MAC)
    lltd_hello_upper_header_t *helloHeader =
        (lltd_hello_upper_header_t *)((uint8_t *)buffer + offset);
    offset += setHelloHeader(buffer, offset,
                             &inFrameHeader->frameHeader.source,   /* apparentMapper */
                             &inFrameHeader->realSource,           /* currentMapper (real) */
                             *generation_slot);
    uint64_t hello_deadline = 0;
    if (currentNetworkInterface->enumerationAutomata && currentNetworkInterface->enumerationAutomata->extra) {
        band_state *band = (band_state *)currentNetworkInterface->enumerationAutomata->extra;
        hello_deadline = band->hello_timeout_ts;
    }
    {
        char mapper_id[18];
        char eth_src[18];
        format_mac_str(inFrameHeader->realSource.a, mapper_id, sizeof(mapper_id));
        format_mac_str(inFrameHeader->frameHeader.source.a, eth_src, sizeof(eth_src));
        log_debug("tx HELLO reply_to_discover t=%llu mapper_id=%s tos=%u seq=%u seq_wire=0x%02x%02x gen_host=0x%04x gen_wire=0x%02x%02x gen_store=%s deadline=%llu curMapper=%s appMapper=%s",
                  (unsigned long long)now_ms,
                  mapper_id,
                  inFrameHeader->tos,
                  ntohs(lltdHeader->seqNumber),
                  ((uint8_t *)&lltdHeader->seqNumber)[0],
                  ((uint8_t *)&lltdHeader->seqNumber)[1],
                  *generation_slot,
                  ((uint8_t *)&helloHeader->generation)[0],
                  ((uint8_t *)&helloHeader->generation)[1],
                  mapper_generation_label(inFrameHeader->tos),
                  (unsigned long long)hello_deadline,
                  mapper_id,
                  eth_src);
    }
    offset += setHostIdTLV(buffer, offset, currentNetworkInterface);
    offset += setCharacteristicsTLV(buffer, offset, currentNetworkInterface);
    offset += setPhysicalMediumTLV(buffer, offset, currentNetworkInterface);
    offset += setIPv4TLV(buffer, offset, currentNetworkInterface);
    offset += setIPv6TLV(buffer, offset, currentNetworkInterface);
    offset += setPerfCounterTLV(buffer, offset);
    offset += setLinkSpeedTLV(buffer, offset, currentNetworkInterface);
    offset += setHostnameTLV(buffer, offset);
    if (currentNetworkInterface->interfaceType == NetworkInterfaceTypeIEEE80211) {
        offset += setWirelessTLV(buffer, offset, currentNetworkInterface);
        offset += setBSSIDTLV(buffer, offset, currentNetworkInterface);
        offset += setSSIDTLV(buffer, offset, currentNetworkInterface);
        offset += setWifiMaxRateTLV(buffer, offset, currentNetworkInterface);
        offset += setWifiRssiTLV(buffer, offset, currentNetworkInterface);
        // Note: Physical Medium TLV already set above with correct IANA type
        offset += setAPAssociationTableTLV(buffer, offset, currentNetworkInterface);
        offset += setRepeaterAPLineageTLV(buffer, offset, currentNetworkInterface);
        offset += setRepeaterAPTableTLV(buffer, offset, currentNetworkInterface);
    }
    offset += setQosCharacteristicsTLV (buffer, offset);
    offset += setIconImageTLV          (buffer, offset);   // Length 0, data via QueryLargeTLV
    offset += setFriendlyNameTLV       (buffer, offset);   // Length 0, data via QueryLargeTLV
    // Note: UUID (0x12) excluded - parsing issue to investigate later
    // Note: Hardware ID (0x13), Detailed Icon, and Component Table are
    // QueryLargeTLV-only and should not appear in Hello frames
    offset += setEndOfPropertyTLV      (buffer, offset);

    log_lltd_frame("TX",
                   currentNetworkInterface->deviceName,
                   inFrameHeader->tos,
                   opcode_hello,
                   ntohs(inFrameHeader->seqNumber),
                   *generation_slot,
                   0,
                   "reply_to_discover");
    size_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                            sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        int err = errno;
        log_crit("Socket write failed: %s", strerror(err));
    }
    currentNetworkInterface->LastHelloTxMs = now_ms;
    currentNetworkInterface->LastHelloReplyMs = now_ms;
    currentNetworkInterface->LastHelloReplyXid = ntohs(inFrameHeader->seqNumber);
    currentNetworkInterface->LastHelloReplyGen = discover_generation;
    currentNetworkInterface->LastHelloReplyTos = inFrameHeader->tos;
    if (currentNetworkInterface->enumerationAutomata && currentNetworkInterface->enumerationAutomata->extra) {
        band_state *band = (band_state *)currentNetworkInterface->enumerationAutomata->extra;
        band_do_hello(band);
        if (band->hello_timeout_ts < now_ms + HELLO_MIN_INTERVAL_MS) {
            band->hello_timeout_ts = now_ms + HELLO_MIN_INTERVAL_MS;
        }
    }

    free(buffer);

}

//==============================================================================
//
// Here we validate the frame and make sure that the TOS/OpCode is a valid
// combination.
// TODO: Add a method to validate that we have the correct TLV combinations for each frame.
//
//==============================================================================
void parseFrame(void *frame, void *networkInterface){
    lltd_demultiplex_header_t *header = frame;
    network_interface_t *currentNetworkInterface = networkInterface;

    uint16_t seq_or_xid = ntohs(header->seqNumber);
    const char *seq_label = (header->opcode == opcode_discover || header->opcode == opcode_reset) ? "xid" : "seq";
    char eth_src[18];
    char eth_dst[18];
    char real_src[18];
    char real_dst[18];
    format_mac_str(header->frameHeader.source.a, eth_src, sizeof(eth_src));
    format_mac_str(header->frameHeader.destination.a, eth_dst, sizeof(eth_dst));
    format_mac_str(header->realSource.a, real_src, sizeof(real_src));
    format_mac_str(header->realDestination.a, real_dst, sizeof(real_dst));
    log_debug("parseFrame(): t=%llu %s op=%u tos=%u ver=0x%02x ethSrc=%s ethDst=%s realSrc=%s realDst=%s %s=%u",
              (unsigned long long)lltd_now_ms(),
              currentNetworkInterface->deviceName,
              header->opcode,
              header->tos,
              header->version,
              eth_src,
              eth_dst,
              real_src,
              real_dst,
              seq_label,
              seq_or_xid);

    if (!compareEthernetAddress(&header->realSource, &header->frameHeader.source)) {
        log_debug("parseFrame(): %s tos=%u opcode=%u Real_Source != Ethernet src (allowed behind bridge/AP)",
                  currentNetworkInterface->deviceName, header->tos, header->opcode);
    }

    {
        uint16_t xid = 0;
        uint16_t gen = 0;
        uint16_t seq = 0;
        if (header->opcode == opcode_discover) {
            lltd_discover_upper_header_t *disc_header =
                (lltd_discover_upper_header_t *)((uint8_t *)frame + sizeof(*header));
            xid = seq_or_xid;
            gen = ntohs(disc_header->generation);
        } else if (header->opcode == opcode_hello) {
            lltd_hello_upper_header_t *hello_header =
                (lltd_hello_upper_header_t *)((uint8_t *)frame + sizeof(*header));
            seq = seq_or_xid;
            gen = ntohs(hello_header->generation);
        } else {
            seq = seq_or_xid;
        }
        log_lltd_frame("RX",
                       currentNetworkInterface->deviceName,
                       header->tos,
                       header->opcode,
                       xid,
                       gen,
                       seq,
                       NULL);
    }

    if (header->opcode == opcode_discover) {
        lltd_discover_upper_header_t *disc_header =
            (lltd_discover_upper_header_t *)((uint8_t *)frame + sizeof(*header));
        uint16_t generation_host = ntohs(disc_header->generation);
        uint16_t stations = ntohs(disc_header->stationNumber);
        char mapper_id[18];
        char eth_src_mac[18];
        format_mac_str(header->realSource.a, mapper_id, sizeof(mapper_id));
        format_mac_str(header->frameHeader.source.a, eth_src_mac, sizeof(eth_src_mac));
    log_debug("parseFrame(): t=%llu %s discover mapper_id=%s ethSrc=%s tos=%u xid=%u gen_host=0x%04x",
              (unsigned long long)lltd_now_ms(),
              currentNetworkInterface->deviceName,
              mapper_id,
              eth_src_mac,
              header->tos,
              seq_or_xid,
              generation_host);
        log_debug("parseFrame(): %s tos=%u discover gen_raw=0x%02x%02x gen_host=0x%04x stations=%u",
                  currentNetworkInterface->deviceName,
                  header->tos,
                  ((uint8_t *)&disc_header->generation)[0],
                  ((uint8_t *)&disc_header->generation)[1],
                  generation_host,
                  stations);
        log_generation_warning_if_swapped(currentNetworkInterface, generation_host, header->tos, "Discover");
        if (!mapper_matches(currentNetworkInterface, &header->realSource)) {
            char mapper_id[18];
            format_mac_str(header->realSource.a, mapper_id, sizeof(mapper_id));
            log_debug("parseFrame(): discover from non-active mapper ignored mapper_id=%s",
                      mapper_id);
            return;
        }
        set_active_mapper(currentNetworkInterface, &header->realSource, &header->frameHeader.source);
        uint16_t *generation_slot = mapper_generation_for_tos(currentNetworkInterface, header->tos);
        bool should_update_generation = false;
        if (*generation_slot == 0 && generation_host != 0) {
            should_update_generation = true;
        } else if (*generation_slot != generation_host) {
            should_update_generation = true;
            log_debug("%s: Discover tos=%u (%s) generation changed: 0x%04x -> 0x%04x",
                      currentNetworkInterface->deviceName,
                      header->tos,
                      mapper_generation_label(header->tos),
                      *generation_slot,
                      generation_host);
        }

    if (should_update_generation) {
        *generation_slot = generation_host;
        log_debug("rx DISCOVER from mapper: set active_generation=0x%04x tos=%u mapper_id=%s",
                  generation_host,
                  header->tos,
                  mapper_id);
    }
    } else if (header->opcode == opcode_hello) {
        lltd_hello_upper_header_t *hello_header =
            (lltd_hello_upper_header_t *)((uint8_t *)frame + sizeof(*header));
        uint16_t generation_host = ntohs(hello_header->generation);
    log_debug("parseFrame(): t=%llu %s tos=%u hello gen_raw=0x%02x%02x gen_host=0x%04x stations=n/a",
              (unsigned long long)lltd_now_ms(),
              currentNetworkInterface->deviceName,
              header->tos,
              ((uint8_t *)&hello_header->generation)[0],
              ((uint8_t *)&hello_header->generation)[1],
              generation_host);
        log_generation_warning_if_swapped(currentNetworkInterface, generation_host, header->tos, "Hello");
    {
        char mapper_id[18];
        format_mac_str(header->realSource.a, mapper_id, sizeof(mapper_id));
        log_debug("rx HELLO from non-mapper: ignored (no state change) src=%s gen=0x%04x",
                  mapper_id,
                  generation_host);
    }
    }

    /*
     * NOTE: Do NOT update MapperSeqNumber here for every frame.
     * Sequence number should only be updated on session-defining frames:
     * Discover, Emit, Query (done in their respective handlers).
     */

    //
    // We validate the message demultiplex
    //
    switch (header->tos){
        case tos_discovery:
            switch (header->opcode) {
                case opcode_discover:
                    log_debug("%s Discover (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    if (!mapper_matches(currentNetworkInterface, &header->realSource)) {
                        char mapper_id[18];
                        format_mac_str(header->realSource.a, mapper_id, sizeof(mapper_id));
                        log_debug("%s Discover ignored from non-active mapper mapper_id=%s",
                                  currentNetworkInterface->deviceName,
                                  mapper_id);
                        break;
                    }
                    usleep(10000);
                    answerHello(frame, currentNetworkInterface);
                    break;
                case opcode_emit:
                    log_debug("%s Emit (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    parseEmit(frame, currentNetworkInterface);
                    break;
                case opcode_train:
                    log_debug("%s Train (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    parseProbe(frame, currentNetworkInterface);
                    break;
                case opcode_probe:
                    log_debug("%s Probe (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    parseProbe(frame, currentNetworkInterface);
                    break;
                case opcode_query:
                    log_debug("%s Query (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    parseQuery(frame, currentNetworkInterface);
                    break;
                case opcode_reset:
                    log_debug("%s Reset (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    currentNetworkInterface->helloSent = 0;

                    //Clean the linked list
                    if(currentNetworkInterface->seeList){
                        probe_t *currentProbe = currentNetworkInterface->seeList;
                        void    *nextProbe;
                        for(uint32_t i = 0; i<currentNetworkInterface->seeListCount; i++){
                                nextProbe    = currentProbe->nextProbe;
                                free(currentProbe);
                                currentProbe = nextProbe;
                        }
                        currentNetworkInterface->seeList=NULL;
                    }
                    currentNetworkInterface->seeListCount = 0;
                    //Return to non-promisc mode.
                    break;
                case opcode_queryLargeTlv:
                    log_debug("%s QueryLargeTLV (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    parseQueryLargeTlv(frame, currentNetworkInterface);
                    break;
                case opcode_ack:
                case opcode_hello:
                case opcode_queryResp:
                case opcode_charge:
                case opcode_flat:
                case opcode_queryLargeTlvResp:
                    log_debug("%s Ignorable Hello|ACK|QueryResponse|Charge|Flat|QLTLVResp (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    break;
                default:
                    log_alert("%s Invalid opcode (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    break;
            }
            break;
        case tos_quick_discovery:
            switch (header->opcode) {
                case opcode_discover:
                    log_debug("%s Discover (%d) for TOS_Quick_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    if (!mapper_matches(currentNetworkInterface, &header->realSource)) {
                        char mapper_id[18];
                        format_mac_str(header->realSource.a, mapper_id, sizeof(mapper_id));
                        log_debug("%s Quick Discover ignored from non-active mapper mapper_id=%s",
                                  currentNetworkInterface->deviceName,
                                  mapper_id);
                        break;
                    }
                    answerHello(frame, currentNetworkInterface);
                    break;

                case opcode_hello: {
                    /*
                     * Quick-Discovery HELLOs from other responders are not solicitation
                     * frames. We log and ignore without updating mapper state.
                     */
                    lltd_hello_upper_header_t *hello_header =
                        (lltd_hello_upper_header_t *)((uint8_t *)frame + sizeof(lltd_demultiplex_header_t));
                    uint16_t generation_host = ntohs(hello_header->generation);
        {
            char mapper_id[18];
            format_mac_str(header->realSource.a, mapper_id, sizeof(mapper_id));
            log_debug("%s Quick HELLO (%d) for TOS_Quick_Discovery: ignored src=%s gen=0x%04x",
                      currentNetworkInterface->deviceName,
                      header->opcode,
                      mapper_id,
                      generation_host);
        }
        break;
    }

                case opcode_reset:
                    log_debug("%s Reset (%d) for TOS_Quick_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    /* Mapper aborted quick discovery; allow a new round. */
                    currentNetworkInterface->helloSent = 0;
                    break;

                default:
                    log_alert("%s Invalid opcode (%d) for TOS_Quick_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    break;
            }
            break;
        case tos_qos_diagnostics:
            switch (header->opcode) {
                case opcode_qosInitializeSink:
                    log_debug("%s Initialize Sink (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosReady:
                    log_debug("%s Ready (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosProbe:
                    log_debug("%s Probe (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosQuery:
                    log_debug("%s Query (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosQueryResp:
                    log_debug("%s Query Response (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosReset:
                    log_debug("%s Reset (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosError:
                    log_debug("%s Error (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosAck:
                    log_debug("%s Ack (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosCounterSnapshot:
                    log_debug("%s Counter Snapshot (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosCounterResult:
                    log_debug("%s Counter Result (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_qosCounterLease:
                    log_debug("%s Counter Lease (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
                default:
                    log_alert("%s Invalid opcode (%d) for TOS_QOS", currentNetworkInterface->deviceName, header->opcode);
                    break;
            }
            break;
        default:
            log_alert("Invalid Type of Service Code: %x", header->tos);
    }
    
}
