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
    fileDescriptor = socket(AF_NDRV, SOCK_RAW, 0);
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
        
        parseFrame(buffer, currentNetworkInterface, fileDescriptor, &socketAddress);
        
        cyclNo++;
        
        asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: === cycle %d stopped ===\n", __FUNCTION__, cyclNo - 1);
    }
    
    //
    // Cleanup
    //
    free(buffer);
    close(fileDescriptor);
    return 0;

}



//==============================================================================
//
// This is the Hello answer to any Discovery package.
//
//==============================================================================
void answerHello(void *inFrame, void *networkInterface, int socketDescriptor, const struct sockaddr_ndrv *socketAddr){
    network_interface_t *currentNetworkInterface = networkInterface;

    void *buffer = malloc(currentNetworkInterface->MTU);
    memset(buffer, 0, currentNetworkInterface->MTU);
    uint64_t offset = 0;
    
    lltd_demultiplex_header_t *inFrameHeader = inFrame;
    lltd_demultiplex_header_t *lltdHeader = buffer;

    lltd_hello_upper_header_t *helloHeader = (void *)lltdHeader + sizeof(lltdHeader);
    lltd_discover_upper_header_t *discoverHeader = (void *)inFrameHeader + sizeof(lltdHeader);
    
    //
    //Validate that real mac address == src address
    //If it's not, silently fail.
    //
    if (!compareEthernetAddress(&lltdHeader->realSource, &lltdHeader->frameHeader.source)){
        asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: Discovery validation failed real mac is not equal to source.\n", __FUNCTION__);
//        return;
    }
    offset = setLltdHeader(buffer, (ethernet_address_t *)&(currentNetworkInterface->hwAddress), (ethernet_address_t *) &EthernetBroadcast, inFrameHeader->seqNumber, opcode_hello, //inFrameHeader->tos);
                    0x01);
    //offset = setLltdHeader(buffer, currentNetworkInterface->hwAddress, (ethernet_address_t *) &EthernetBroadcast, inFrameHeader->seqNumber, opcode_hello, tos_quick_discovery);
    
    offset += setHelloHeader(buffer, offset, &inFrameHeader->frameHeader.source, &inFrameHeader->realSource, discoverHeader->generation );
    offset += setHostIdTLV(buffer, offset, currentNetworkInterface);
    offset += setCharacteristicsTLV(buffer, offset, currentNetworkInterface);
    offset += setPhysicalMediumTLV(buffer, offset, currentNetworkInterface);
    //offset += setIPv4TLV(buffer, offset, currentNetworkInterface);
    //offset += setIPv6TLV(buffer, offset, currentNetworkInterface);
    offset += setPerfCounterTLV(buffer, offset);
    offset += setLinkSpeedTLV(buffer, offset, currentNetworkInterface);
    offset += setHostnameTLV(buffer, offset);
    // FIXME: we really need to write them properly
    offset += setQosCharacteristicsTLV(buffer, offset);
    offset += setEndOfPropertyTLV(buffer, offset);

    size_t write = sendto(socketDescriptor, buffer, offset, 0, socketAddr, sizeof(socketAddr));
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

    setIPv6TLV();
    setLinkSpeedTLV();
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



//==============================================================================
//
// Here we validate the frame and make sure that the TOS/OpCode is a valid
// combination.
// TODO: Add a method to validate that we have the correct TLV combinations
// TODO: for each frame.
//
//==============================================================================
void parseFrame(void *frame, void *networkInterface, int socketDescriptor, const struct sockaddr_ndrv *socketAddr){
    lltd_demultiplex_header_t *header = frame;
    network_interface_t *currentNetworkInterface = networkInterface;
    
    asl_log(asl, log_msg, ASL_LEVEL_DEBUG, "%s: %s: ethertype:%4x, opcode:%4x, tos:%4x, version: %x", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName, 0), header->frameHeader.ethertype , header->opcode, header->tos, header->version);
    
    //
    // We validate the message demultiplex
    // Anything else is b0rken
    //
    switch (header->tos){
        case tos_discovery:
            switch (header->opcode) {
                case opcode_discover:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Discover (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    answerHello(frame, currentNetworkInterface, socketDescriptor, socketAddr);
                    break;
                case opcode_hello:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Hello (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_emit:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Emit (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_train:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Train (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_probe:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Probe (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_ack:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s ACK (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_query:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Query (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_queryResp:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s QueryResponse (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_reset:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Reset (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    
                    break;
                case opcode_charge:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Charge (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_flat:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s Flat (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
                    break;
                case opcode_queryLargeTlv:
                    asl_log(asl, log_msg, ASL_LEVEL_ALERT, "%s: %s QueryLargeTLV (%d) for TOS_Discovery", __FUNCTION__, CFStringGetCStringPtr(currentNetworkInterface->deviceName,0), header->opcode);
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