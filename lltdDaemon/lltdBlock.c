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

//==============================================================================
//
// This is the thread that is opened for each valid interface
//
//==============================================================================
void lltdBlock (void *data){

    network_interface_t *currentNetworkInterface = data;
    
    int                       fileDescriptor;
    struct ndrv_protocol_desc protocolDescription;
    struct ndrv_demux_desc    demuxDescription[1];
    struct sockaddr_ndrv      socketAddress;
    
    //
    // Open the AF_NDRV RAW socket
    //
    fileDescriptor = socket(AF_NDRV, SOCK_RAW, htons(0x88D9));
    currentNetworkInterface->socket = fileDescriptor;

    if (fileDescriptor < 0) {
        log_err("Could not create socket on %s.", currentNetworkInterface->deviceName);
        return -1;
    }
    
    //
    // Bind to the socket
    //
    strcpy((char *)&(socketAddress.snd_name), currentNetworkInterface->deviceName);
    socketAddress.snd_len = sizeof(socketAddress);
    socketAddress.snd_family = AF_NDRV;
    if (bind(fileDescriptor, (struct sockaddr *)&socketAddress, sizeof(socketAddress)) < 0) {
        log_err("Could not bind socket on %s.", currentNetworkInterface->deviceName);
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
    log_notice("Successfully binded to interface %s", currentNetworkInterface->deviceName);

    //
    // Start the run loop
    //
    unsigned int cyclNo = 0;
    currentNetworkInterface->recvBuffer = malloc(currentNetworkInterface->MTU);

    for(;;){
        recvfrom(fileDescriptor, currentNetworkInterface->recvBuffer, currentNetworkInterface->MTU, 0, NULL, NULL);
        parseFrame(currentNetworkInterface->recvBuffer, currentNetworkInterface);
    }
    
    //
    // Cleanup
    //
    free(currentNetworkInterface->recvBuffer);
    currentNetworkInterface->recvBuffer=NULL;
    close(fileDescriptor);
    return 0;

}

boolean_t sendProbeMsg(ethernet_address_t src, ethernet_address_t dst, void *networkInterface, int pause, uint8_t type, boolean_t ack) {
    network_interface_t *currentNetworkInterface = networkInterface;
    // TODO: why, we'll see..
    int packageSize = sizeof(lltd_demultiplex_header_t);
    lltd_demultiplex_header_t *probe = malloc( packageSize );
   
    // This one should be correct
    uint8_t code = type == 0x01 ? opcode_probe : opcode_train;
    setLltdHeader(probe, (ethernet_address_t *) &src,
                  (ethernet_address_t *) &dst,
                  0x00, code, tos_discovery);


    log_alert("Trying to send probe/train with seqNumber %d", ntohs(probe->seqNumber));
    
    usleep(1000 * pause);
    ssize_t write = sendto(currentNetworkInterface->socket, probe, packageSize, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                          sizeof(currentNetworkInterface->socketAddr));
    if (write < 0) {
        log_crit("Socket write failed on PROBE/TRAIN: %s", strerror(write));
        return false;
    } else if (ack) {
        // write an ACK too with the seq number, the algorithm will not conitnue without it
        setLltdHeader(probe, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                      (ethernet_address_t *) &(currentNetworkInterface->MapperHwAddress),
                      currentNetworkInterface->MapperSeqNumber, opcode_ack, tos_discovery);
         
        write = sendto(currentNetworkInterface->socket, probe, packageSize, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                              sizeof(currentNetworkInterface->socketAddr));
        if (write < 0) {
            log_crit("Socket write failed on ACK: %s\n", strerror(write));
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
    
    offset = setLltdHeader(buffer, (ethernet_address_t *) &(currentNetworkInterface->macAddress),
                                   (ethernet_address_t *) &(currentNetworkInterface->MapperHwAddress),
                                    currentNetworkInterface->MapperSeqNumber, opcode_queryResp, tos_discovery);
    
    qry_resp_upper_header_t *respH  = buffer + sizeof(lltd_demultiplex_header_t);
    // HERE I AM
    
    int probesNo = (int) CFArrayGetCount(currentNetworkInterface->seelist);
    log_crit("I've seen %d probes", probesNo);
    
    for(long i = 0; i < probesNo; i++) {
        const probe_t *probe = CFArrayGetValueAtIndex(currentNetworkInterface->seelist , i);
        log_crit("\tType %d, Source: "ETHERNET_ADDR_FMT", Dest: "ETHERNET_ADDR_FMT", RealSource: "ETHERNET_ADDR_FMT" \n",
                    probe->type, ETHERNET_ADDR(probe->sourceAddr.a), ETHERNET_ADDR(probe->destAddr.a), ETHERNET_ADDR(probe->realSourceAddr.a) );
        
    }
    // TODO: split this into multiple messages if it's the case
    respH->numDescs                 = htons(probesNo);
    offset += sizeof(qry_resp_upper_header_t);
    
    if (probesNo) {
        
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
    if (write < 1) {
        log_crit("Socket write failed on QryResp: %s\n", strerror(write));
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
        log_crit("Socket write failed on sendImage: %s\n", strerror(write));
    }
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
        log_crit("Image request, responding with QryLargeResp, offset=%d\n", ntohs( header->offset ) );
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
        probe_t * probe = malloc( sizeof(probe_t) );
        
        // initialize the probe, then search for it in our array
        probe->type             = 0x00;
        probe->sourceAddr       = header->frameHeader.source;
        probe->destAddr         = header->frameHeader.destination;
        probe->realSourceAddr   = header->realSource;
        
        long count = CFArrayGetCount(currentNetworkInterface->seelist);
        
        boolean_t found = false;
        
        //if (count > 1) {
            log_crit("Searching through already %ld seen probes", count);
            for(long i = 0; i < count; i++) {
                const probe_t *searchProbe = CFArrayGetValueAtIndex(currentNetworkInterface->seelist, i);
                // destination and type are already equal, we'll compare just the source addresses
                log_crit("\tSource1: "ETHERNET_ADDR_FMT", Source2: "ETHERNET_ADDR_FMT" ,RealSource1: "ETHERNET_ADDR_FMT", RealSource2: "ETHERNET_ADDR_FMT",\n",
                        ETHERNET_ADDR(probe->sourceAddr.a), ETHERNET_ADDR(searchProbe->sourceAddr.a), ETHERNET_ADDR(probe->realSourceAddr.a), ETHERNET_ADDR(searchProbe->realSourceAddr.a) );
                if ( compareEthernetAddress( &(probe->sourceAddr), &(searchProbe->sourceAddr)) &&
                    compareEthernetAddress( &(probe->realSourceAddr), &(searchProbe->realSourceAddr)) ) {
                    found = true;
                }
            }
        //}
        
        if (!found) {
            CFArrayAppendValue(currentNetworkInterface->seelist, probe);
            log_crit("Adding probe to seen list");
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
    log_alert("Emit parsed, number of descs: %x\n", ntohs(emitHeader->numDescs));
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
    // FIXME: we really need to write them properly
    offset += setQosCharacteristicsTLV(buffer, offset);
    offset += setIconImageTLV(buffer, offset);
    offset += setEndOfPropertyTLV(buffer, offset);

    size_t write = sendto(currentNetworkInterface->socket, buffer, offset, 0, (struct sockaddr *) &currentNetworkInterface->socketAddr,
                            sizeof(currentNetworkInterface->socketAddr));
    if (write < 1) {
        log_crit("Socket write failed: %s", strerror(write));
    }
    
    setPromiscuous(networkInterface, true);
    
    free(buffer);

/*    if (currentNetworkInterface->interfaceType = "IEEE80211") {
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
                case opcode_hello:
                    log_debug("%s Hello (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
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
                case opcode_ack:
                    log_debug("%s ACK (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_query:
                    log_debug("%s Query (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    parseQuery(frame, currentNetworkInterface);
                    break;
                case opcode_queryResp:
                    log_debug("%s QueryResponse (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_reset:
                    log_debug("%s Reset (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    helloSent = 0;
                    CFArrayRemoveAllValues(currentNetworkInterface->seelist);
                    setPromiscuous(currentNetworkInterface, false);
                    break;
                case opcode_charge:
                    log_debug("%s Charge (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    
                    break;
                case opcode_flat:
                    log_debug("%s Flat (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_queryLargeTlv:
                    log_debug("%s QueryLargeTLV (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    parseQueryLargeTlv(frame, currentNetworkInterface);
                    setPromiscuous(currentNetworkInterface, false);
                    break;
                case opcode_queryLargeTlvResp:
                    log_debug("%s QueryLargeTLVResponse (%d) for TOS_Discovery", currentNetworkInterface->deviceName, header->opcode);
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
                    log_debug("%s Hello (%d) for TOS_Quick_Discovery", currentNetworkInterface->deviceName, header->opcode);
                    break;
                case opcode_reset:
                    log_debug("%s Reset (%d) for TOS_Quick_Discovery", currentNetworkInterface->deviceName, header->opcode);
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