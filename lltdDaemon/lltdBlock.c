/******************************************************************************
 *                                                                            *
 *   lltdBlock.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 23.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/

#include "lltdDaemon.h"

boolean_t sendProbeMsg(ethernet_address_t src, ethernet_address_t dst, void *networkInterface, int pause, uint8_t type, boolean_t ack) {
    network_interface_t *currentNetworkInterface = networkInterface;
    // TODO: why, we'll see..
    int packageSize = sizeof(lltd_demultiplex_header_t);
    lltd_demultiplex_header_t *probe = malloc( packageSize );
   
    // This one should be correct
    uint8_t code = type == 0x01 ? opcode_probe : opcode_train;
    setLltdHeader(probe, (ethernet_address_t *) &src, (ethernet_address_t *) &dst, 0x00, code, tos_discovery);


    log_alert("Trying to send probe/train with seqNumber %d", ntohs(probe->seqNumber));
    
    usleep(1000 * pause);
    ssize_t write = sendto(currentNetworkInterface->socket, probe, packageSize, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr, sizeof(currentNetworkInterface->socketAddr));
    if (write < 0) {
        log_crit("Socket write failed on PROBE/TRAIN: %s", strerror(errno));
        return false;
    } else if (ack) {
        // write an ACK too with the seq number, the algorithm will not conitnue without it
        setLltdHeader(probe, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                      (ethernet_address_t *) &(currentNetworkInterface->MapperHwAddress),
                      currentNetworkInterface->MapperSeqNumber, opcode_ack, tos_discovery);
         
        write = sendto(currentNetworkInterface->socket, probe, packageSize, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                              sizeof(currentNetworkInterface->socketAddr));
        if (write < 0) {
            log_crit("Socket write failed on ACK: %s", strerror(errno));
        }
    }
}

//TODO: validate Query
void parseQuery(void *inFrame, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    int packageSize = currentNetworkInterface->MTU + sizeof(ethernet_header_t);
    size_t offset = 0;
    void *buffer = malloc( packageSize );
    memset(buffer, 0, packageSize);
    
    offset = setLltdHeader(buffer, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                                   (ethernet_address_t *) &(currentNetworkInterface->MapperHwAddress),
                                    currentNetworkInterface->MapperSeqNumber, opcode_queryResp, tos_discovery);
    
    qry_resp_upper_header_t *respH  = buffer + sizeof(lltd_demultiplex_header_t);
    // HERE I AM
    
    log_crit("I've seen %d probes", currentNetworkInterface->seeListCount);
    probe_t currentProbe;
    
    for(long i = 0; i < currentNetworkInterface->seeListCount; i++) {
        memcpy(&currentProbe, currentNetworkInterface->seeList + (i * sizeof(probe_t)), sizeof(probe_t));
        log_crit("\tType %d, Source: "ETHERNET_ADDR_FMT", Dest: "ETHERNET_ADDR_FMT", RealSource: "ETHERNET_ADDR_FMT,
                    currentProbe.type, ETHERNET_ADDR(currentProbe.sourceAddr.a), ETHERNET_ADDR(currentProbe.destAddr.a), ETHERNET_ADDR(currentProbe.realSourceAddr.a) );
    }
    // TODO: split this into multiple messages if it's the case
    respH->numDescs                 = htons(currentNetworkInterface->seeListCount);
    offset += sizeof(qry_resp_upper_header_t);
    
    if (currentNetworkInterface->seeListCount) {
        
        memcpy( buffer + offset, currentNetworkInterface->seeList, sizeof(probe_t) * currentNetworkInterface->seeListCount );
        
        offset += sizeof(probe_t) * currentNetworkInterface->seeListCount;
        
        free(currentNetworkInterface->seeList);
        currentNetworkInterface->seeList=NULL;
        currentNetworkInterface->seeListCount = 0;
        
    }
    
    
    ssize_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                          sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        log_crit("Socket write failed on QryResp: %s", strerror(write));
    }
    
    free(buffer);
}

void sendImage(void *networkInterface, uint16_t offset) {
    network_interface_t *currentNetworkInterface = networkInterface;
    uint16_t maxSize = currentNetworkInterface->MTU - sizeof(lltd_demultiplex_header_t)
                    - sizeof(qry_large_tlv_resp_t) + sizeof(ethernet_header_t); //TODO: check if this can be bigger
    
    // TODO: Figure out a way to store the icon on all threads, calculate the first time it's req and keep bytes in RAM
    void *icon = NULL;
    size_t size = 0;
    if (globalInfo.smallIcon == NULL || globalInfo.smallIconSize == 0) {
        if (globalInfo.smallIcon) {
            free(globalInfo.smallIcon);
            globalInfo.smallIcon=NULL;
        }
        getIconImage(&icon, &size);
        globalInfo.smallIcon = icon;
        globalInfo.smallIconSize = size;
    } else {
        icon = globalInfo.smallIcon;
        size = globalInfo.smallIconSize;
    }
    
    
    void *buffer = malloc( maxSize );
    memset(buffer, 0, maxSize);
    
    setLltdHeader(buffer, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                           (ethernet_address_t *) &(currentNetworkInterface->MapperHwAddress),
                           currentNetworkInterface->MapperSeqNumber, opcode_queryLargeTlvResp, tos_discovery);
    
    qry_large_tlv_resp_t *header = buffer + sizeof(lltd_demultiplex_header_t);
    
    uint16_t bytesToWrite = 0;
    
    if (size >= offset + maxSize) {
        bytesToWrite     = maxSize;
        header->length   = bytesToWrite;
        // set "more to come" byte to 1
        header->length  |=  0x8000;
    } else {
        bytesToWrite     = size - offset;
        header->length   = bytesToWrite;
    }
    header->length = htons(header->length);
    
    size_t packageSize = bytesToWrite + sizeof(qry_large_tlv_resp_t) + sizeof(lltd_demultiplex_header_t);
    memcpy( buffer + (packageSize - bytesToWrite), icon + offset, bytesToWrite );
    
    ssize_t write = sendto(currentNetworkInterface->socket, buffer, packageSize, 0,
                           (struct sockaddr *) &currentNetworkInterface->socketAddr, sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        log_crit("Socket write failed on sendImage: %s", strerror(write));
    }
    //free(icon);
}


void parseQueryLargeTlv(void *inFrame, void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;
    lltd_demultiplex_header_t *lltdHeader = inFrame;
    if (lltdHeader->seqNumber == 0) {
        // as per spec, ignore LargeTLV with zeroed sequence Number
        return;
    }
    qry_large_tlv_t *header = inFrame + sizeof(lltd_demultiplex_header_t);
    if (header->type == tlv_iconImage) {
        log_crit("Image request, responding with QryLargeResp, offset=%d", ntohs( header->offset ) );
        sendImage(networkInterface, ntohs(header->offset));
    }
}

void parseProbe(void *inFrame, void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;
    // if the probe is destined to us, we record it
    lltd_demultiplex_header_t * header = inFrame;
    
    // see if it's intended for us
    if ( compareEthernetAddress(&(header->frameHeader.destination), (ethernet_address_t *) &currentNetworkInterface->macAddress) || true	 )  {

        // store it then, unless we have it already??
        probe_t *probe          = malloc( sizeof(probe_t) );
        
        // initialize the probe, then search for it in our array
        probe->type             = 0x00;
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
    lltd_discover_upper_header_t *discoverHeader = (void *)inFrameHeader + sizeof(lltdHeader);
    
    //
    //Validate that real mac address == src address
    //If it's not, silently fail.
    //
    if (!compareEthernetAddress(&lltdHeader->realSource, &lltdHeader->frameHeader.source)){
        log_debug("Discovery validation failed real mac is not equal to source.");
        return;
    }
    
    offset = setLltdHeader(buffer, (ethernet_address_t *)&(currentNetworkInterface->macAddress), (ethernet_address_t *) &EthernetBroadcast, 0x00, opcode_hello, inFrameHeader->tos);
    
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
        offset += set80211MediumTLV(buffer, offset, currentNetworkInterface);
        offset += setAPAssociationTableTLV(buffer, offset, currentNetworkInterface);
        offset += setRepeaterAPLineageTLV(buffer, offset, currentNetworkInterface);
        offset += setRepeaterAPTableTLV(buffer, offset, currentNetworkInterface);
    }
    offset += setQosCharacteristicsTLV (buffer, offset);
    offset += setIconImageTLV          (buffer, offset);
    offset += setFriendlyNameTLV       (buffer, offset);
    offset += setUuidTLV               (buffer, offset);
    offset += setHardwareIdTLV         (buffer, offset);
    offset += setDetailedIconTLV       (buffer, offset);
    offset += setComponentTableTLV     (buffer, offset);
    offset += setEndOfPropertyTLV      (buffer, offset);

    size_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                            sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        log_crit("Socket write failed: %s", strerror(write));
    }
    
    setPromiscuous(networkInterface, true);
    
    free(buffer);

}

int helloSent = 0;

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
                    if (!helloSent) {
                        helloSent = 1;
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
                    helloSent = 0;
                    
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
                case opcode_hello:
                case opcode_reset:
                    log_debug("%s Ignorable Hello|Reset (%d) for TOS_Quick_Discovery", currentNetworkInterface->deviceName, header->opcode);
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