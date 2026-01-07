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

boolean_t sendProbeMsg(ethernet_address_t src, ethernet_address_t dst, void *networkInterface, int pause, uint8_t type, boolean_t ack) {
    network_interface_t *currentNetworkInterface = networkInterface;
    // TODO: why, we'll see..
    size_t packageSize = sizeof(lltd_demultiplex_header_t);
    lltd_demultiplex_header_t *probe = malloc( packageSize );
   
    // This one should be correct
    uint8_t code = type == 0x01 ? opcode_probe : opcode_train;
    // Probe/Train frames belong to the current mapping session; they must carry the mapper's seqNumber.
    setLltdHeader(probe, (ethernet_address_t *) &src, (ethernet_address_t *) &dst,
                  currentNetworkInterface->MapperSeqNumber, code, tos_discovery);


    log_alert("Trying to send probe/train with seqNumber %d", ntohs(probe->seqNumber));
    
    usleep(1000 * pause);
    ssize_t write = sendto(currentNetworkInterface->socket, probe, packageSize, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr, sizeof(currentNetworkInterface->socketAddr));
    if (write < 0) {
        int err = errno;
        log_crit("Socket write failed on PROBE/TRAIN: %s", strerror(err));
        return false;
    } else if (ack) {
        // write an ACK too with the seq number, the algorithm will not conitnue without it
        setLltdHeader(probe, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                      (ethernet_address_t *) &(currentNetworkInterface->MapperHwAddress),
                      currentNetworkInterface->MapperSeqNumber, opcode_ack, tos_discovery);
         
        write = sendto(currentNetworkInterface->socket, probe, packageSize, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                              sizeof(currentNetworkInterface->socketAddr));
        if (write < 0) {
            int err = errno;
            log_crit("Socket write failed on ACK: %s", strerror(err));
        }
    }
}

//TODO: validate Query
void parseQuery(void *inFrame, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    size_t packageSize = currentNetworkInterface->MTU + sizeof(ethernet_header_t);
    size_t offset = 0;
    void *buffer = malloc( packageSize );
    memset(buffer, 0, packageSize);
    
    offset = setLltdHeader(buffer, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                                   (ethernet_address_t *) &(currentNetworkInterface->MapperHwAddress),
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

        log_crit("\tType %d, Source: "ETHERNET_ADDR_FMT", Dest: "ETHERNET_ADDR_FMT", RealSource: "ETHERNET_ADDR_FMT,
                 ntohs(wire.type), ETHERNET_ADDR(wire.sourceAddr.a), ETHERNET_ADDR(wire.destAddr.a), ETHERNET_ADDR(wire.realSourceAddr.a));

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
    
    
    ssize_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                          sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        int err = errno;
        log_crit("Socket write failed on QryResp: %s", strerror(err));
    }
    
    free(buffer);
}

static void sendLargeTlvResponse(void *networkInterface, void *data, size_t dataSize, uint16_t dataOffset) {
    network_interface_t *currentNetworkInterface = networkInterface;
    uint16_t maxPayload = currentNetworkInterface->MTU - sizeof(lltd_demultiplex_header_t)
                        - sizeof(qry_large_tlv_resp_t);

    size_t bufferSize = sizeof(lltd_demultiplex_header_t) + sizeof(qry_large_tlv_resp_t) + maxPayload;
    void *buffer = malloc(bufferSize);
    memset(buffer, 0, bufferSize);

    setLltdHeader(buffer, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                          (ethernet_address_t *) &(currentNetworkInterface->MapperHwAddress),
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
    if (lltdHeader->seqNumber == 0) {
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

    sendLargeTlvResponse(networkInterface, data, dataSize, offset);

    // Free non-cached data
    if (header->type == tlv_friendlyName || header->type == tlv_hwIdProperty) {
        if (data) {
            free(data);
        }
    }
}

void parseProbe(void *inFrame, void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;
    // if the probe is destined to us, we record it
    lltd_demultiplex_header_t * header = inFrame;
    
    // Only record probes/train frames intended for us.
    if (compareEthernetAddress(&(header->frameHeader.destination), (ethernet_address_t *)&currentNetworkInterface->macAddress)) {

        // store it then, unless we have it already??
        probe_t *probe          = malloc( sizeof(probe_t) );
        
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
                log_crit("\tSource1: "ETHERNET_ADDR_FMT", Source2: "ETHERNET_ADDR_FMT" ,RealSource1: "ETHERNET_ADDR_FMT", RealSource2: "ETHERNET_ADDR_FMT, ETHERNET_ADDR(probe->sourceAddr.a), ETHERNET_ADDR(searchProbe->sourceAddr.a), ETHERNET_ADDR(probe->realSourceAddr.a), ETHERNET_ADDR(searchProbe->realSourceAddr.a) );
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
    
    lltd_emit_upper_header_t *emitHeader = (void *) ( (void *)lltdHeader + sizeof(*lltdHeader) );
    currentNetworkInterface->MapperSeqNumber = lltdHeader->seqNumber;
    
    int numDescs = ntohs(emitHeader->numDescs);
    uint16_t offsetEmitee = 0;
    log_alert("Emit parsed, number of descs: %x", ntohs(emitHeader->numDescs));
    for (int i = 0; i < numDescs; i++) {
        boolean_t ack = i == numDescs -1 ? true : false;
        emitee_descs *emitee = ( (void *)emitHeader + sizeof(*emitHeader) + offsetEmitee );
        if (emitee->type == 1) {
            // this probes
            sendProbeMsg(emitee->sourceAddr, emitee->destAddr, networkInterface, emitee->pause, emitee->type, ack);
        } else if (emitee->type == 0) {
            // send trains
            sendProbeMsg(emitee->sourceAddr, emitee->destAddr, networkInterface, emitee->pause, emitee->type, ack);
        } else {
          log_alert("Unknown emitee type=%d !", emitee->type);
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
    uint64_t             offset = 0;

    memset(buffer, 0, currentNetworkInterface->MTU);
    
    lltd_demultiplex_header_t    *inFrameHeader  = inFrame;
    lltd_demultiplex_header_t    *lltdHeader     = buffer;
    // The Discover upper header immediately follows the demultiplex header.
    lltd_discover_upper_header_t *discoverHeader = (lltd_discover_upper_header_t *)((uint8_t *)inFrameHeader + sizeof(*inFrameHeader));
    
    //
    //Validate that real mac address == src address
    //If it's not, silently fail.
    //
    if (!compareEthernetAddress(&inFrameHeader->realSource, &inFrameHeader->frameHeader.source)) {
        log_debug("Discovery validation failed real mac is not equal to source.");
        free(buffer);
        return;
    }
    
    // Hello frames are part of the same discovery session: preserve the mapper seqNumber.
    offset = setLltdHeader(buffer, (ethernet_address_t *)&(currentNetworkInterface->macAddress), (ethernet_address_t *) &EthernetBroadcast,
                          inFrameHeader->seqNumber, opcode_hello, inFrameHeader->tos);
    
    //offset = setLltdHeader(buffer, currentNetworkInterface->hwAddress, (ethernet_address_t *) &EthernetBroadcast, inFrameHeader->seqNumber, opcode_hello, tos_quick_discovery);
    
    currentNetworkInterface->MapperSeqNumber = inFrameHeader->seqNumber;
    currentNetworkInterface->MapperHwAddress[0] = inFrameHeader->realSource.a[0];
    currentNetworkInterface->MapperHwAddress[1] = inFrameHeader->realSource.a[1];
    currentNetworkInterface->MapperHwAddress[2] = inFrameHeader->realSource.a[2];
    currentNetworkInterface->MapperHwAddress[3] = inFrameHeader->realSource.a[3];
    currentNetworkInterface->MapperHwAddress[4] = inFrameHeader->realSource.a[4];
    currentNetworkInterface->MapperHwAddress[5] = inFrameHeader->realSource.a[5];
    
    offset += setHelloHeader(buffer, offset, &inFrameHeader->frameHeader.source, &inFrameHeader->realSource, discoverHeader->generation );
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

    size_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                            sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        int err = errno;
        log_crit("Socket write failed: %s", strerror(err));
    }
    
    setPromiscuous(networkInterface, true);
    
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

    log_debug("%s: ethertype:0x%4x, opcode:0x%x, tos:0x%x, version: 0x%x", currentNetworkInterface->deviceName, ntohs(header->frameHeader.ethertype) , header->opcode, header->tos, header->version);

    // FIXME: set the seqNumber each frame we get (for now)
    currentNetworkInterface->MapperSeqNumber = header->seqNumber;

    //
    // We validate the message demultiplex
    //
    switch (header->tos){
        case tos_discovery:
            switch (header->opcode) {
                case opcode_discover:
                    log_debug("%s Discover (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    if (!currentNetworkInterface->helloSent) {
                        currentNetworkInterface->helloSent = 1;
                        usleep(150000);
                        answerHello(frame, currentNetworkInterface);
                    }
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
                    setPromiscuous(currentNetworkInterface, false);
                    break;
                case opcode_queryLargeTlv:
                    log_debug("%s QueryLargeTLV (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    parseQueryLargeTlv(frame, currentNetworkInterface);
                    setPromiscuous(currentNetworkInterface, false);
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
                    answerHello(frame, currentNetworkInterface);
                    break;

                case opcode_hello: {
                    /*
                     * Windows mappers use Quick Discovery to populate the Network Map.
                     * Treat a Quick-Discovery HELLO as a solicitation and respond with our
                     * station HELLO using the mapper's real and apparent addresses.
                     */
                    lltd_hello_upper_header_t *inHello =
                        (lltd_hello_upper_header_t *)((uint8_t *)frame + sizeof(lltd_demultiplex_header_t));

                    currentNetworkInterface->MapperHwAddress  = header->realSource;
                    currentNetworkInterface->MapperSeqNumber  = header->seqNumber;
                    currentNetworkInterface->MapperGeneration = inHello->generation;

                    log_debug("%s Quick HELLO (%d) for TOS_Quick_Discovery: replying", currentNetworkInterface->deviceName, header->opcode);
                    sendHelloMessageEx(
                        currentNetworkInterface,
                        header->seqNumber,
                        tos_quick_discovery,
                        &header->realSource,
                        &header->frameHeader.source,
                        inHello->generation
                    );
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
