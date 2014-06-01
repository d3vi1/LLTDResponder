/******************************************************************************
 *                                                                            *
 *   lltdBlock.c                                                              *
 *   LLMNRd                                                                   *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 23.03.2014.                      *
 *   Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.       *
 *                                                                            *
 ******************************************************************************/

#include "llmnrd.h"
#include "darwin-ops.h"
//==============================================================================
//
// This is the thread that is opened for each valid interface
//
//==============================================================================
void lltdBlock (void *data){

    network_interface_t *currentNetworkInterface = data;
    
    int                       fileDescriptor;
    int                       bytesRecv;
    struct ndrv_protocol_desc protocolDescription;
    struct ndrv_demux_desc    demuxDescription[1];
    struct sockaddr_ndrv      socketAddress;
    void                     *buffer;
    
    //
    // Open the AF_NDRV RAW socket
    //
    fileDescriptor = socket(AF_NDRV, SOCK_RAW, htons(0x88D9));
    currentNetworkInterface->socket = fileDescriptor;

    if (fileDescriptor < 0) {
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not create socket on %s.\n", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, 0));
        return -1;
    }
    
    //
    // Bind to the socket
    //
    CFStringGetCString(currentNetworkInterface->deviceName, (char *) socketAddress.snd_name, sizeof(socketAddress.snd_name), 0);
    socketAddress.snd_len = sizeof(socketAddress);
    socketAddress.snd_family = AF_NDRV;
    if (bind(fileDescriptor, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0) {
        asl_log(asl, log_msg, ASL_LEVEL_ERR, "%s: Could not bind socket on %s.\n", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, 0));
        return -1;
    }
    
    //
    // Create the reference to the array that will store the probes viewed on the network
    //
    
    currentNetworkInterface->seelist = CFArrayCreateMutable(NULL, 0, NULL);;

    //
    // Define protocol
    //
    protocolDescription.version = (u_int32_t)1;
    protocolDescription.protocol_family = lltdEtherType;
    protocolDescription.demux_count = (u_int32_t)1;
    protocolDescription.demux_list = (struct ndrv_demux_desc*)&demuxDescription;
    
    //
    // Define protocol DEMUX
    //
    demuxDescription[0].type = NDRV_DEMUXTYPE_ETHERTYPE;
    demuxDescription[0].length = 2;
    demuxDescription[0].data.ether_type = htons(lltdEtherType);

    //
    // Set the protocol on the socket
    //
    setsockopt(fileDescriptor, SOL_NDRVPROTO, NDRV_SETDMXSPEC, (caddr_t)&protocolDescription, sizeof(protocolDescription));
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: Successfully binded to interface %s\n", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, 0));

    //
    // Start the run loop
    //
    unsigned int cyclNo = 0;
    buffer = malloc(currentNetworkInterface->MTU);

    for(;;){
        bytesRecv = recvfrom(fileDescriptor, buffer, currentNetworkInterface->MTU, 0, NULL, NULL);
        
        parseFrame(buffer, currentNetworkInterface);
        
        cyclNo++;
        
        asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: === cycle %d stopped, %d bytes received ===\n", __FUNCTION__, cyclNo - 1, bytesRecv);
    }
    
    //
    // Cleanup
    //
    free(buffer);
    close(fileDescriptor);
    return 0;

}

boolean_t sendProbeMsg(ethernet_address_t src, ethernet_address_t dst, void *networkInterface, int pause) {
    network_interface_t *currentNetworkInterface = networkInterface;
    // TODO: why, we'll see..
    int packageSize = sizeof(lltd_demultiplex_header_t);
    lltd_demultiplex_header_t *probe = malloc( packageSize );
    
    setLltdHeader(probe, (ethernet_address_t *) &(currentNetworkInterface->hwAddress),
                  (ethernet_address_t *) &(currentNetworkInterface->mapper.hwAddress),
                  0x00, opcode_probe, tos_discovery);

    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: Trying to send probe with seqNumber %d\n", __FUNCTION__, ntohs(probe->seqNumber));
    
    usleep(1000 * pause);
    ssize_t write = sendto(currentNetworkInterface->socket, probe, packageSize, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                          sizeof(currentNetworkInterface->socketAddr));
    if (write < 0) {
        asl_log(asl, log_msg, ASL_LEVEL_CRIT, "%s: Socket write failed on PROBE: %s\n", __FUNCTION__, strerror(write));
        return false;
    } else {
        // write an ACK too with the seq number, the algorithm will not conitnue without it
        setLltdHeader(probe, (ethernet_address_t *) &(currentNetworkInterface->hwAddress),
                      (ethernet_address_t *) &(currentNetworkInterface->mapper.hwAddress),
                      currentNetworkInterface->mapper.seqNumber, opcode_ack, tos_discovery);
         
        write = sendto(currentNetworkInterface->socket, probe, packageSize, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                              sizeof(currentNetworkInterface->socketAddr));
        if (write < 0) {
            asl_log(asl, log_msg, ASL_LEVEL_CRIT, "%s: Socket write failed on ACK: %s\n", __FUNCTION__, strerror(write));
        }
    }
}

//TODO: validate Query
void parseQuery(void *inFrame, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    int packageSize = currentNetworkInterface->MTU + sizeof(ethernet_header_t),
        offset = 0;
    void *buffer = malloc( packageSize );
    bzero(buffer, packageSize);
    
    offset = setLltdHeader(buffer, (ethernet_address_t *) &(currentNetworkInterface->hwAddress),
                                   (ethernet_address_t *) &(currentNetworkInterface->mapper.hwAddress),
                                    currentNetworkInterface->mapper.seqNumber, opcode_queryResp, tos_discovery);
    
    qry_resp_upper_header_t *respH  = buffer + sizeof(lltd_demultiplex_header_t);
    // HERE I AM
    
    int probesNo = (int) CFArrayGetCount(currentNetworkInterface->seelist);
    asl_log(asl, log_msg, ASL_LEVEL_CRIT, "%s: I've seen %d probes\n", __FUNCTION__, probesNo);
    
    for(long i = 0; i < probesNo; i++) {
        const probe_t *probe = CFArrayGetValueAtIndex(currentNetworkInterface->seelist , i);
        asl_log(asl, log_msg, ASL_LEVEL_CRIT, "\t%s: Type %d, Source: "ETHERNET_ADDR_FMT", Dest: "ETHERNET_ADDR_FMT", RealSource: "ETHERNET_ADDR_FMT" \n", __FUNCTION__,
                    probe->type, ETHERNET_ADDR(probe->sourceAddr.a), ETHERNET_ADDR(probe->destAddr.a), ETHERNET_ADDR(probe->realSourceAddr.a) );
        
    }
    // TODO: split this into multiple messages if it's the case
    respH->numDescs                 = htons(probesNo);
    offset += sizeof(qry_resp_upper_header_t);
    
    if (probesNo) {
        //CFArrayGetValues( currentNetworkInterface->seelist, CFRangeMake(0, probesNo), buffer + offset);
        
        void * viewList = malloc(sizeof(probe_t) * probesNo);
        for (int i = 0; i< probesNo; i++) {
            memcpy(viewList + (i * sizeof(probe_t)),CFArrayGetValueAtIndex(currentNetworkInterface->seelist, i) ,sizeof(probe_t));
        }
        
        memcpy( buffer + offset, viewList, sizeof(probe_t) * probesNo );
        free(viewList);
        
        offset += sizeof(probe_t) * probesNo;
        
        CFArrayRemoveAllValues(currentNetworkInterface->seelist);
    }
    
    
    ssize_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                          sizeof(currentNetworkInterface->socketAddr));
//    if (write < 0) {
//        asl_log(asl, log_msg, ASL_LEVEL_CRIT, "%s: Socket write failed on QryResp: %s\n", __FUNCTION__, strerror(write));
//    }
    
    free(buffer);
}

void sendImage(void *networkInterface, uint16_t offset) {
    network_interface_t *currentNetworkInterface = networkInterface;
    uint16_t maxSize = currentNetworkInterface->MTU - sizeof(lltd_demultiplex_header_t)
                    - sizeof(qry_large_tlv_resp_t) + sizeof(ethernet_header_t); //TODO: check if this can be bigger
    
    // TODO: Figure out a way to store the icon on all threads, calculate the first time it's req and keep bytes in RAM
    void *icon = NULL;
    size_t size = 0;
    if (currentNetworkInterface->icon == NULL || currentNetworkInterface->iconSize == 0) {
        getIconImage(&icon, &size);
        currentNetworkInterface->icon = icon;
        currentNetworkInterface->iconSize = size;
    } else {
        icon = currentNetworkInterface->icon;
        size = currentNetworkInterface->iconSize;
    }
    
    
    void *buffer = malloc( maxSize );
    bzero(buffer, maxSize);
    
    setLltdHeader(buffer, (ethernet_address_t *) &(currentNetworkInterface->hwAddress),
                           (ethernet_address_t *) &(currentNetworkInterface->mapper.hwAddress),
                           currentNetworkInterface->mapper.seqNumber, opcode_queryLargeTlvResp, tos_discovery);
    
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
//    free(icon);
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
        asl_log(asl, log_msg, ASL_LEVEL_CRIT, "%s: Image request, responding with QryLargeResp, offset=%d\n", __FUNCTION__, ntohs( header->offset ) );
        sendImage(networkInterface, ntohs(header->offset));
    }
}

void parseProbe(void *inFrame, void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;
    // if the probe is destined to us, we record it
    lltd_demultiplex_header_t * header = inFrame;
    
    // see if it's intended for us
    if ( compareEthernetAddress(&(header->frameHeader.destination), (ethernet_address_t *) &currentNetworkInterface->hwAddress) )  {
        // store it then, unless we have it already??
        probe_t * probe = malloc( sizeof(probe_t) );
        
        // initialize the probe, then search for it in our array
        probe->type             = header->opcode;
        probe->sourceAddr       = header->frameHeader.source;
        probe->destAddr         = header->frameHeader.destination;
        probe->realSourceAddr   = header->realSource;
        
        long count = CFArrayGetCount(currentNetworkInterface->seelist);
        
        boolean_t found = false;
        
        //if (count > 1) {
            asl_log(asl, log_msg, ASL_LEVEL_CRIT, "%s: Searching through already %ld seen probes\n", __FUNCTION__, count);
            for(long i = 0; i < count; i++) {
                const probe_t *searchProbe = CFArrayGetValueAtIndex(currentNetworkInterface->seelist, i);
                // destination and type are already equal, we'll compare just the source addresses
                asl_log(asl, log_msg, ASL_LEVEL_CRIT, "%s:\tSource1: "ETHERNET_ADDR_FMT", Source2: "ETHERNET_ADDR_FMT" ,RealSource1: "ETHERNET_ADDR_FMT", RealSource2: "ETHERNET_ADDR_FMT",\n", __FUNCTION__,
                        ETHERNET_ADDR(probe->sourceAddr.a), ETHERNET_ADDR(searchProbe->sourceAddr.a), ETHERNET_ADDR(probe->realSourceAddr.a), ETHERNET_ADDR(searchProbe->realSourceAddr.a) );
                if ( compareEthernetAddress( &(probe->sourceAddr), &(searchProbe->sourceAddr)) &&
                    compareEthernetAddress( &(probe->realSourceAddr), &(searchProbe->realSourceAddr)) ) {
                    found = true;
                }
            }
        //}
        
        if (!found) {
            CFArrayAppendValue(currentNetworkInterface->seelist, probe);
            asl_log(asl, log_msg, ASL_LEVEL_CRIT, "%s: Adding probe to seen list\n", __FUNCTION__ );
        }
    }
}

void parseEmit(void *inFrame, void *networkInterface){
    network_interface_t *currentNetworkInterface = networkInterface;
    
    lltd_demultiplex_header_t *lltdHeader = inFrame;
    
    // Works correctly!!!
    lltd_emit_upper_header_t *emitHeader = (void *) ( (void *)lltdHeader + sizeof(*lltdHeader) );
    currentNetworkInterface->mapper.seqNumber = lltdHeader->seqNumber;
    
    int numDescs = ntohs(emitHeader->numDescs);
    uint16_t offsetEmitee = 0;
    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: Emit parsed, number of descs: %x\n", __FUNCTION__, ntohs(emitHeader->numDescs));
    for (int i = 0; i < numDescs; i++) {
        emitee_descs *emitee = ( (void *)emitHeader + sizeof(*emitHeader) + offsetEmitee );
        if (emitee->type == 1) {
            // this is where the magic really happens
            sendProbeMsg(emitee->sourceAddr, emitee->destAddr, networkInterface, emitee->pause);
        } else if (emitee->type == 2) {
          asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: Emitee type=%d !", __FUNCTION__, emitee->type);
        } else {
          asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: Unknown emitee type=%d !", __FUNCTION__, emitee->type);
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

    void *buffer = malloc(currentNetworkInterface->MTU);
    memset(buffer, 0, currentNetworkInterface->MTU);
    uint64_t offset = 0;
    
    lltd_demultiplex_header_t *inFrameHeader = inFrame;
    lltd_demultiplex_header_t *lltdHeader = buffer;
    
    lltd_discover_upper_header_t *discoverHeader = (void *)inFrameHeader + sizeof(lltdHeader);
    
    //
    //Validate that real mac address == src address
    //If it's not, silently fail.
    //
    if (!compareEthernetAddress(&lltdHeader->realSource, &lltdHeader->frameHeader.source)){
        asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: Discovery validation failed real mac is not equal to source.\n", __FUNCTION__);
//        return;
    }
    
    offset = setLltdHeader(buffer, (ethernet_address_t *)&(currentNetworkInterface->hwAddress), (ethernet_address_t *) &EthernetBroadcast, 0x00, opcode_hello, inFrameHeader->tos);
    
    //offset = setLltdHeader(buffer, currentNetworkInterface->hwAddress, (ethernet_address_t *) &EthernetBroadcast, inFrameHeader->seqNumber, opcode_hello, tos_quick_discovery);
    
    currentNetworkInterface->mapper.seqNumber = inFrameHeader->seqNumber;
    currentNetworkInterface->mapper.hwAddress = inFrameHeader->realSource;
    
    offset += setHelloHeader(buffer, offset, &inFrameHeader->frameHeader.source, &inFrameHeader->realSource, discoverHeader->generation );
    offset += setHostIdTLV(buffer, offset, currentNetworkInterface);
    offset += setCharacteristicsTLV(buffer, offset, currentNetworkInterface);
    offset += setPhysicalMediumTLV(buffer, offset, currentNetworkInterface);
    offset += setIPv4TLV(buffer, offset, currentNetworkInterface);
    offset += setIPv6TLV(buffer, offset, currentNetworkInterface);
    offset += setPerfCounterTLV(buffer, offset);
    offset += setLinkSpeedTLV(buffer, offset, currentNetworkInterface);
    offset += setHostnameTLV(buffer, offset);
//     FIXME: we really need to write them properly
    offset += setQosCharacteristicsTLV(buffer, offset);
    offset += setIconImageTLV(buffer, offset);
    offset += setEndOfPropertyTLV(buffer, offset);

    size_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                            sizeof(currentNetworkInterface->socketAddr));
    setPromiscuous(networkInterface, true);
    
    free(buffer);

/*    if (CFStringCompare(currentNetworkInterface->interfaceType, CFSTR("IEEE80211"), 0) == kCFCompareEqualTo) {
        setWirelessTLV();
        setBSSIDTLV();
        setSSIDTLV();
        setWifiMaxRateTLV();
        setWifiRssiTLV();
        set80211MediumTLV();
        setAPAssociationTableTLV();
        setRepeaterAPLineageTLV();
        setRepeaterAPTableTLV();
    }
    setIconImageTLV();
    setMachineNameTLV(); //TODO: asta e de fapt setHostnameTLV!!
    setSupportInfoTLV();
    setFriendlyNameTLV();
    setUuidTLV();
    setHardwareIdTLV();

    setDetailedIconTLV();
    //setSeeslistWorkingSetTLV();
    setComponentTableTLV();
  */
}

int helloSent = 0;

//==============================================================================
//
// Here we validate the frame and make sure that the TOS/OpCode is a valid
// combination.
// TODO: Add a method to validate that we have the correct TLV combinations
// TODO: for each frame.
//
//==============================================================================
void parseFrame(void *frame, void *networkInterface){
    lltd_demultiplex_header_t *header = frame;
    network_interface_t *currentNetworkInterface = networkInterface;
    
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: %s: ethertype:%4x, opcode:%4x, tos:%4x, version: %x", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, 0), header->frameHeader.ethertype , header->opcode, header->tos, header->version);
    
    // FIXME: set the seqNumber each frame we get (for now)
    currentNetworkInterface->mapper.seqNumber = header->seqNumber;
    
    //
    // We validate the message demultiplex
    //
    switch (header->tos){
        case tos_discovery:
            switch (header->opcode) {
                case opcode_discover:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Discover (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    if (!helloSent) {
                        helloSent = 1;
                        usleep(150000);
                        answerHello(frame, currentNetworkInterface);
                    }
                    break;
                case opcode_hello:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Hello (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_emit:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Emit (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    parseEmit(frame, currentNetworkInterface);
                    break;
                case opcode_train:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Train (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_probe:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Probe (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    parseProbe(frame, currentNetworkInterface);
                    break;
                case opcode_ack:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s ACK (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_query:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Query (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    parseQuery(frame, currentNetworkInterface);
                    break;
                case opcode_queryResp:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s QueryResponse (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_reset:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Reset (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    helloSent = 0;
                    CFArrayRemoveAllValues(currentNetworkInterface->seelist);
                    setPromiscuous(currentNetworkInterface, false);
                    break;
                case opcode_charge:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Charge (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    
                    break;
                case opcode_flat:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Flat (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_queryLargeTlv:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s QueryLargeTLV (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    parseQueryLargeTlv(frame, currentNetworkInterface);
                    //setPromiscuous(currentNetworkInterface, false);
                    break;
                case opcode_queryLargeTlvResp:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s QueryLargeTLVResponse (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                default:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Invalid opcode (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
            }
            break;
        case tos_quick_discovery:
            switch (header->opcode) {
                case opcode_discover:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Discover (%d) for TOS_Quick_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    answerHello(frame, currentNetworkInterface);
                    break;
                case opcode_hello:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Hello (%d) for TOS_Quick_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_reset:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Reset (%d) for TOS_Quick_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                default:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Invalid opcode (%d) for TOS_Quick_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
            }
            break;
        case tos_qos_diagnostics:
            switch (header->opcode) {
                case opcode_qosInitializeSink:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Initialize Sink (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosReady:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Ready (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosProbe:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Probe (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosQuery:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Query (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosQueryResp:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Query Response (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosReset:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Reset (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosError:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Error (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosAck:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Ack (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosCounterSnapshot:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Counter Snapshot (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosCounterResult:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Counter Result (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_qosCounterLease:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Counter Lease (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                default:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Invalid opcode (%d) for TOS_QOS", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
            }
            break;
        default:
            asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: Invalid Type of Service Code: %x", __FUNCTION__, header->tos);
    }
    
}