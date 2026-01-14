/******************************************************************************
 *                                                                            *
 *   darwin-main.c                                                            *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 16.03.2014.                      *
 *   Copyright © 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.    *
 *                                                                            *
 ******************************************************************************/


#include "../lltdDaemon.h"
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

void SignalHandler(int Signal) {
    log_debug("Interrupted by signal #%d", Signal);
    printf("\nInterrupted by signal #%d\n", Signal);
    exit(0);
}

static void free_automata(automata *autom) {
    if (!autom) {
        return;
    }
    free(autom->extra);
    free(autom);
}

//==============================================================================
//
// Send a Hello message on the given interface (used by RepeatBand algorithm)
//
//==============================================================================
void sendHelloMessageEx(
    network_interface_t *currentNetworkInterface,
    uint16_t seqNumber,
    uint8_t tos,
    const ethernet_address_t *mapperRealAddress,
    const ethernet_address_t *mapperApparentAddress,
    uint16_t generation,
    hello_tx_reason_t reason
) {
    uint8_t *buffer = calloc(1, currentNetworkInterface->MTU);
    if (!buffer) {
        log_err("calloc failed in sendHelloMessageEx");
        return;
    }

    uint64_t offset = 0;

    /*
     * Per MS-LLTD 2.2.3.4 (Hello Frame):
     * - Ethernet destination MUST be FF:FF:FF:FF:FF:FF (broadcast)
     * - Base header Real Destination Address SHOULD be FF:FF:FF:FF:FF:FF
     * - Hello header contains Apparent_Mapper_Address and Current_Mapper_Address
     *   (set below in setHelloHeader)
     */
    offset = setLltdHeader(
        (void *)buffer,
        (ethernet_address_t *)&(currentNetworkInterface->macAddress),
        (ethernet_address_t *)&EthernetBroadcast,
        seqNumber,
        opcode_hello,
        tos
    );

    lltd_demultiplex_header_t *lltdHeader =
        (lltd_demultiplex_header_t *)buffer;
    lltd_hello_upper_header_t *helloHeader =
        (lltd_hello_upper_header_t *)(buffer + offset);

    // Set the Hello upper header
    offset += setHelloHeader(
        (void *)buffer,
        offset,
        (ethernet_address_t *)(uintptr_t)mapperApparentAddress,
        (ethernet_address_t *)(uintptr_t)mapperRealAddress,
        generation
    );

    const char *gen_store = (tos == tos_quick_discovery) ? "quick" : "topology";
    const char *reason_str = (reason == hello_reason_reply_to_discover) ? "reply_to_discover" : "hello_timeout";
    log_debug("sendHelloMessageEx(): %s mapper_id="ETHERNET_ADDR_FMT" tos=%u opcode=0x%x seq=%u seq_wire=0x%02x%02x gen_host=0x%04x gen_wire=0x%02x%02x gen_store=%s reason=%s curMapper="
              ETHERNET_ADDR_FMT" appMapper="ETHERNET_ADDR_FMT,
              currentNetworkInterface->deviceName,
              ETHERNET_ADDR(mapperRealAddress->a),
              tos,
              opcode_hello,
              seqNumber,
              ((uint8_t *)&lltdHeader->seqNumber)[0],
              ((uint8_t *)&lltdHeader->seqNumber)[1],
              generation,
              ((uint8_t *)&helloHeader->generation)[0],
              ((uint8_t *)&helloHeader->generation)[1],
              gen_store,
              reason_str,
              ETHERNET_ADDR(mapperRealAddress->a),
              ETHERNET_ADDR(mapperApparentAddress->a));
    log_debug("t=%llu TX %s tos=%u op=%u xid=0x%04x gen=0x%04x seq=0x%04x reason=%s",
              (unsigned long long)lltd_monotonic_milliseconds(),
              currentNetworkInterface->deviceName,
              tos,
              opcode_hello,
              0,
              generation,
              seqNumber,
              reason_str);

    // Add Station TLVs
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
    offset += setQosCharacteristicsTLV(buffer, offset);
    offset += setIconImageTLV(buffer, offset);     // Length 0, data via QueryLargeTLV
    offset += setFriendlyNameTLV(buffer, offset);  // Length 0, data via QueryLargeTLV

    // Add ending TLV
    offset += setEndOfPropertyTLV(buffer, offset);

    // Send the LLTD response
    if (sendto(
            currentNetworkInterface->socket,
            buffer,
            offset,
            0,
            (struct sockaddr *)&currentNetworkInterface->socketAddr,
            sizeof(currentNetworkInterface->socketAddr)
        ) == -1) {
        log_err("sendHelloMessageEx(): failed to send Hello on %s (%llu bytes): errno %d (%s)",
                currentNetworkInterface->deviceName, (unsigned long long)offset, errno, strerror(errno));
    } else {
        log_debug("sendHelloMessageEx(): Hello (%llu bytes) sent on %s", (unsigned long long)offset, currentNetworkInterface->deviceName);
    }

    free(buffer);
}

void sendHelloMessage(void *networkInterface) {
    network_interface_t *currentNetworkInterface = networkInterface;

    sendHelloMessageEx(
        currentNetworkInterface,
        0,
        tos_discovery,
        (const ethernet_address_t *)(const void *)&currentNetworkInterface->MapperHwAddress,
        (const ethernet_address_t *)(const void *)&currentNetworkInterface->MapperHwAddress,
        currentNetworkInterface->MapperGenerationTopology,
        hello_reason_periodic_timeout
    );
}


//==============================================================================
//
// This is the thread that is opened for each valid interface
//
//==============================================================================
void lltdLoop (void *data){
    
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
    
    currentNetworkInterface->seeList = NULL;
    
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
    log_notice("Successfully binded to %s", currentNetworkInterface->deviceName);

    //
    // Set receive timeout to avoid select() issues on AF_NDRV
    //
    struct timeval recvTimeout;
    recvTimeout.tv_sec = 0;
    recvTimeout.tv_usec = 100000; // 100ms tick interval
    if (setsockopt(fileDescriptor, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout)) != 0) {
        log_err("Failed to set SO_RCVTIMEO on %s: %s", currentNetworkInterface->deviceName, strerror(errno));
    }

    //
    //Rename the thread to listener: $ifName
    //
    char *threadName = malloc(strlen(currentNetworkInterface->deviceName)+strlen("listener: ")+1);
    if (threadName) {
        strcpy(threadName, "listener: ");
        strcat(threadName, currentNetworkInterface->deviceName);
        pthread_setname_np(threadName);
        free(threadName);
    }
    
    //
    // Start the run loop with SO_RCVTIMEO-based tick mechanism
    //
    currentNetworkInterface->recvBuffer = malloc(currentNetworkInterface->MTU);

    if(currentNetworkInterface->recvBuffer == NULL) {
        log_err("The receive buffer couldn't be allocated. We've failed miserably on interface %s", currentNetworkInterface->deviceName);
    } else for(;;){
        ssize_t recvLen = recvfrom(fileDescriptor, currentNetworkInterface->recvBuffer,
                                   currentNetworkInterface->MTU, 0, NULL, NULL);
        if (recvLen < 0) {
            int err = errno;
            if (err != EAGAIN && err != EWOULDBLOCK && err != EINTR) {
                log_err("recvfrom() failed on %s: %s", currentNetworkInterface->deviceName, strerror(err));
            }
            // Always run the automata tick, even on timeout
            lltd_automata_tick_port tick_port = {
                .network_interface = currentNetworkInterface,
                .last_hello_tx_ms = &currentNetworkInterface->LastHelloTxMs,
                .send_hello = sendHelloMessage,
            };
            automata_tick(currentNetworkInterface->mappingAutomata,
                          currentNetworkInterface->enumerationAutomata,
                          currentNetworkInterface->sessionTable,
                          &tick_port);
            continue;
        }

        if(currentNetworkInterface->recvBuffer == NULL) {
            log_err("For some reason the receive buffer has been freed by someone. HELP");
            break;
        }

        lltd_demultiplex_header_t *header = currentNetworkInterface->recvBuffer;

        // Derive session event from the received frame
        int sess_event = derive_session_event(currentNetworkInterface->recvBuffer,
                                              currentNetworkInterface->sessionTable,
                                              currentNetworkInterface->macAddress);

        // Update session table based on received frame
        if (header->opcode == opcode_discover) {
            lltd_discover_upper_header_t *disc_header =
                (lltd_discover_upper_header_t*)(header + 1);
            uint16_t generation = ntohs(disc_header->generation);

            {
                char mapper_id[18];
                char eth_src[18];
                snprintf(mapper_id, sizeof(mapper_id), "%02x:%02x:%02x:%02x:%02x:%02x",
                         header->realSource.a[0], header->realSource.a[1], header->realSource.a[2],
                         header->realSource.a[3], header->realSource.a[4], header->realSource.a[5]);
                snprintf(eth_src, sizeof(eth_src), "%02x:%02x:%02x:%02x:%02x:%02x",
                         header->frameHeader.source.a[0], header->frameHeader.source.a[1], header->frameHeader.source.a[2],
                         header->frameHeader.source.a[3], header->frameHeader.source.a[4], header->frameHeader.source.a[5]);
                log_debug("%s: mapper_id=%s ethSrc=%s tos=%u xid=%u",
                          currentNetworkInterface->deviceName,
                          mapper_id,
                          eth_src,
                          header->tos,
                          ntohs(header->seqNumber));
            }
            // Add/update session entry
            session_entry* entry = session_table_add(currentNetworkInterface->sessionTable,
                                                     header->realSource.a,
                                                     generation,
                                                     ntohs(header->seqNumber));
            if (entry) {
                entry->state = (uint8_t)sess_event;
                entry->last_activity_ts = lltd_monotonic_seconds();
                if (sess_event == sess_discover_acking || sess_event == sess_discover_acking_chgd_xid) {
                    entry->complete = true;
                }
                // Update interface-level mapper info
                memcpy(currentNetworkInterface->MapperHwAddress, header->realSource.a, 6);
                if (header->tos == tos_quick_discovery) {
                    currentNetworkInterface->MapperGenerationQuick = generation;
                } else {
                    currentNetworkInterface->MapperGenerationTopology = generation;
                }
            }
            session_table_update_complete_status(currentNetworkInterface->sessionTable);
        } else if (header->opcode == opcode_reset) {
            // Clear session table on reset
            session_table_clear(currentNetworkInterface->sessionTable);
        }

        // Update mapping automaton with opcode
        // Track previous state to detect Quiescent transition
        uint8_t prev_mapping_state = currentNetworkInterface->mappingAutomata->current_state;
        switch_state_mapping(currentNetworkInterface->mappingAutomata, header->opcode, "rx");

        // If Mapping just transitioned to Quiescent (timeout or otherwise), clear session table
        // This ensures RepeatBand stops when the mapping session ends
        if (prev_mapping_state != 0 && currentNetworkInterface->mappingAutomata->current_state == 0) {
            log_debug("Mapping transitioned to Quiescent, clearing session table");
            session_table_clear(currentNetworkInterface->sessionTable);
        }

        // Reset inactive timeout on any valid frame
        if (currentNetworkInterface->mappingAutomata->extra) {
            mapping_reset_inactive_timeout((mapping_state*)currentNetworkInterface->mappingAutomata->extra);
        }

        // Handle charge opcode specially for charge timeout counter
        if (header->opcode == opcode_charge && currentNetworkInterface->mappingAutomata->extra) {
            mapping_on_charge((mapping_state*)currentNetworkInterface->mappingAutomata->extra);
        }

        // Update session automaton with derived session event
        if (sess_event >= 0) {
            switch_state_session(currentNetworkInterface->sessionAutomata, sess_event, "rx");
        }

        // Update enumeration automaton based on frame type
        if (header->opcode == opcode_hello) {
            // Received Hello from another station
            if (currentNetworkInterface->enumerationAutomata->extra) {
                band_on_hello_received((band_state*)currentNetworkInterface->enumerationAutomata->extra);
            }
            switch_state_enumeration(currentNetworkInterface->enumerationAutomata, enum_hello, "rx");
        } else if (header->opcode == opcode_discover) {
            // New session starting - initialize RepeatBand if needed
            if (currentNetworkInterface->enumerationAutomata->current_state == 0) {
                // In Quiescent, start enumeration
                if (currentNetworkInterface->enumerationAutomata->extra) {
                    band_init_stats((band_state*)currentNetworkInterface->enumerationAutomata->extra);
                    band_choose_hello_time((band_state*)currentNetworkInterface->enumerationAutomata->extra);
                }
            } else if (currentNetworkInterface->enumerationAutomata->extra) {
                // Already enumerating, mark begun
                ((band_state*)currentNetworkInterface->enumerationAutomata->extra)->begun = true;
            }
            switch_state_enumeration(currentNetworkInterface->enumerationAutomata, enum_new_session, "rx");
        }

        // Parse and handle the frame
        parseFrame(currentNetworkInterface->recvBuffer, currentNetworkInterface);

        // Run periodic automata tick after handling frame
        lltd_automata_tick_port tick_port = {
            .network_interface = currentNetworkInterface,
            .last_hello_tx_ms = &currentNetworkInterface->LastHelloTxMs,
            .send_hello = sendHelloMessage,
        };
        automata_tick(currentNetworkInterface->mappingAutomata,
                      currentNetworkInterface->enumerationAutomata,
                      currentNetworkInterface->sessionTable,
                      &tick_port);
    }
    
    //
    // Cleanup
    //
    free(currentNetworkInterface->recvBuffer);
    currentNetworkInterface->recvBuffer=NULL;
    close(fileDescriptor);
    return 0;
    
}

//==============================================================================
//
// Helper: Recursively search for a CF property in IORegistry by walking parent
// chain. This is needed for macOS 26+ where properties may be on parent nodes
// in the Skywalk stack. Returns a retained CFTypeRef or NULL.
// Compatible with Tiger+ (uses IORegistryEntryCreateCFProperty +
// IORegistryEntryGetParentEntry instead of IORegistryEntrySearchCFProperty).
//
//==============================================================================
static CFTypeRef CopyCFPropertyRecursive(io_service_t entry, CFStringRef key) {
    CFTypeRef property = NULL;
    io_service_t currentEntry = entry;
    io_service_t parentEntry = MACH_PORT_NULL;
    kern_return_t kr;

    // Retain the initial entry since we'll be releasing it in the loop
    IOObjectRetain(currentEntry);

    while (currentEntry != MACH_PORT_NULL) {
        // Try to get the property from the current entry
        property = IORegistryEntryCreateCFProperty(currentEntry, key, kCFAllocatorDefault, kNilOptions);
        if (property != NULL) {
            IOObjectRelease(currentEntry);
            return property; // Found it, return retained property
        }

        // Property not found, try parent
        kr = IORegistryEntryGetParentEntry(currentEntry, kIOServicePlane, &parentEntry);
        IOObjectRelease(currentEntry);

        if (kr != KERN_SUCCESS || parentEntry == MACH_PORT_NULL) {
            break; // No parent or error, stop searching
        }

        currentEntry = parentEntry;
    }

    return NULL; // Not found in chain
}

//==============================================================================
//
// Helper: Detect if interface is wireless using IORegistry properties.
// Checks both legacy IOObjectConformsTo and fallback IO80211 properties.
// Returns true if wireless, false otherwise.
//
//==============================================================================
static bool IsWirelessInterface(io_service_t IONetworkInterface) {
    // Primary check: legacy IOObjectConformsTo
    if (IOObjectConformsTo(IONetworkInterface, "IO80211Interface")) {
        return true;
    }

    // Fallback: check for IO80211-specific properties in provider chain
    CFTypeRef ssid = CopyCFPropertyRecursive(IONetworkInterface, CFSTR("IO80211SSID"));
    if (ssid != NULL) {
        CFRelease(ssid);
        return true;
    }

    CFTypeRef countryCode = CopyCFPropertyRecursive(IONetworkInterface, CFSTR("IO80211CountryCode"));
    if (countryCode != NULL) {
        CFRelease(countryCode);
        return true;
    }

    return false;
}

//==============================================================================
//
// We are checking if it's an Ethernet Interface, if it's up, if it has an
// IPv4 stack and if it has an IPv6 stack. If all are met, we are updating the
// networkInterface object object with that information and starting listner
// threads on each of the IP addresses of the interface that we can safely
// kill when we get a ServiceIsTerminated kernel message.
// If the interface is valid, but otherwise fails a check for link or flags, we
// we put a notification for both the Controller and the Interface IOKit objects
// in order to be called again if something changes. It might pass the checks
// after the changes. It happens if the daemon is started before the network.
//
//==============================================================================
void validateInterface(void *refCon, io_service_t IONetworkInterface) {
    kern_return_t         kernel_return;
    io_service_t          IONetworkController;
    network_interface_t  *currentNetworkInterface = (network_interface_t*) refCon;
    
    
    //
    // Get the object's parent (the interface controller) in order to get the
    // Mac Address, Link Status, Link Speed and Active Medium from it.
    //
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    if (kernel_return != KERN_SUCCESS) log_err("%s Could not get the parent of the interface", currentNetworkInterface->deviceName);
    
    if(IONetworkController) {
        
        
        //
        // Get the Mac Address
        //
        CFTypeRef macAddressAsData = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMACAddress), kCFAllocatorDefault, 0);
        if(macAddressAsData) {
            CFDataGetBytes((CFDataRef)macAddressAsData, CFRangeMake(0,CFDataGetLength(macAddressAsData)), currentNetworkInterface->macAddress);
            CFRelease(macAddressAsData);
        } else {
            // Not being able to read the Mac Address is fatal and we ignore the
            // interface from here on.
            log_err("%s Could not read linkStatus.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            free((void *)currentNetworkInterface->deviceName);
            free(currentNetworkInterface);
            return;
        }
        
        
        //
        // Get/Check the LinkStatus
        //
        CFNumberRef CFLinkStatus = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkStatus), kCFAllocatorDefault, 0);
        if (CFLinkStatus){

             // Check if we have link on the controller
            int  LinkStatus = 0;
            CFNumberGetValue(CFLinkStatus, kCFNumberIntType, &LinkStatus);
            currentNetworkInterface->linkStatus=LinkStatus;
        
            // First check for error scenarios
            if((LinkStatus & (kIONetworkLinkValid|kIONetworkLinkActive)) == (kIONetworkLinkValid|kIONetworkLinkActive)){
                //This is the best scenario. I've put it here to simplify the expressions below.
            } else if (LinkStatus & kIONetworkLinkActive) {
                //Having an invalid LinkStatus is not fatal. We just put an
                //interest notification and we get called to validate again if
                //something changes.
                log_debug("%s Validation Failed: Interface doesn't have a Valid Link but has an Active one", currentNetworkInterface->deviceName);
                IOServiceAddInterestNotification(globalInfo.notificationPort, IONetworkController, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
                IOServiceAddInterestNotification(globalInfo.notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
                CFRelease(CFLinkStatus);
                return;
            } else if (LinkStatus & kIONetworkLinkValid) {
                //Having an invalid LinkStatus is not fatal. We just put an
                //interest notification and we get called to validate again if
                //something changes.
                log_debug("%s Validation Failed: Interface doesn't have an Active Link but has a Valid one", currentNetworkInterface->deviceName);
                IOServiceAddInterestNotification(globalInfo.notificationPort, IONetworkController, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
                IOServiceAddInterestNotification(globalInfo.notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
                CFRelease(CFLinkStatus);
                return;
            }
            //This is the best scenario (kIONetworkLinkValid | kIONetworkLinkActive).
            CFRelease(CFLinkStatus);
            
        } else {
            //Not being able to read the LinkStatus (even if it's invalid) is fatal
            //and we ignore the interface from here on.
            log_err("%s Could not read linkStatus.", currentNetworkInterface->deviceName);
            IOObjectRelease(IONetworkController);
            free((void *)currentNetworkInterface->deviceName);
            free(currentNetworkInterface);
            return;
        }
        

        //
        // Get/Check the Medium Speed (non-fatal on modern macOS)
        //
        CFTypeRef ioLinkSpeed = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOLinkSpeed), kCFAllocatorDefault, 0);
        if (ioLinkSpeed) {
            CFNumberGetValue((CFNumberRef)ioLinkSpeed, kCFNumberLongLongType, &currentNetworkInterface->LinkSpeed);
            CFRelease(ioLinkSpeed);
        } else {
            // LinkSpeed may be missing on modern macOS (Skywalk stacks).
            // Set to 0 and continue - this is best-effort information.
            log_debug("%s Could not read kIOLinkSpeed, setting to 0 (non-fatal).", currentNetworkInterface->deviceName);
            currentNetworkInterface->LinkSpeed = 0;
        }


        //
        // Get/Check the Medium Type (non-fatal on modern macOS, with fallback)
        //
        // On macOS 26+ with Skywalk, kIOMediumDictionary may be missing and
        // kIOActiveMedium may be a CFString instead of CFNumber. We set a
        // deterministic fallback and only override if we can read the properties.
        //
        bool isWireless = IsWirelessInterface(IONetworkInterface);

        // Set fallback medium type based on interface type
        if (isWireless) {
            currentNetworkInterface->MediumType = kIOMediumIEEE80211Auto;
        } else {
            currentNetworkInterface->MediumType = kIOMediumEthernetAuto;
        }

        // Try to read the medium dictionary (may be on parent nodes in Skywalk)
        CFDictionaryRef properties = CopyCFPropertyRecursive(IONetworkController, CFSTR(kIOMediumDictionary));
        if (properties && CFGetTypeID(properties) == CFDictionaryGetTypeID()) {
            // Try to read the active medium key (may be CFNumber or CFString)
            CFTypeRef activeMediumKey = CopyCFPropertyRecursive(IONetworkController, CFSTR(kIOActiveMedium));
            if (activeMediumKey != NULL) {
                // Use the key (CFNumber or CFString) to look up the active medium
                CFDictionaryRef activeMedium = (CFDictionaryRef)CFDictionaryGetValue(properties, activeMediumKey);
                if (activeMedium != NULL && CFGetTypeID(activeMedium) == CFDictionaryGetTypeID()) {
                    CFNumberRef mediumType = (CFNumberRef)CFDictionaryGetValue(activeMedium, CFSTR(kIOMediumType));
                    if (mediumType != NULL && CFGetTypeID(mediumType) == CFNumberGetTypeID()) {
                        // Successfully read medium type, override the fallback
                        CFNumberGetValue(mediumType, kCFNumberLongLongType, &(currentNetworkInterface->MediumType));
                    }
                }
                CFRelease(activeMediumKey);
            }
            CFRelease(properties);
        }

        // If we couldn't read the medium type, the fallback is already set
        log_debug("%s MediumType: 0x%llx (%s)", currentNetworkInterface->deviceName,
                  currentNetworkInterface->MediumType, isWireless ? "wireless" : "ethernet");


        //
        // Get the MTU from the controller using kIOMaxPacketSize.
        // This is more reliable on modern macOS with Skywalk.
        //
        CFTypeRef ioMaxPacketSize = IORegistryEntryCreateCFProperty(IONetworkController, CFSTR(kIOMaxPacketSize), kCFAllocatorDefault, 0);
        if (ioMaxPacketSize) {
            CFNumberGetValue((CFNumberRef)ioMaxPacketSize, kCFNumberLongType, &currentNetworkInterface->MTU);
            CFRelease(ioMaxPacketSize);
        } else {
            // Fallback to interface's kIOMaxTransferUnit if controller doesn't have kIOMaxPacketSize
            CFTypeRef ioInterfaceMTU = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOMaxTransferUnit), kCFAllocatorDefault, 0);
            if (ioInterfaceMTU) {
                CFNumberGetValue((CFNumberRef)ioInterfaceMTU, kCFNumberLongType, &currentNetworkInterface->MTU);
                CFRelease(ioInterfaceMTU);
            } else {
                log_err("%s Could not read MTU from controller or interface", currentNetworkInterface->deviceName);
                IOObjectRelease(IONetworkController);
                IOObjectRelease(IONetworkInterface);
                free((void*)currentNetworkInterface->deviceName);
                free(currentNetworkInterface);
                return;
            }
        }


    } else {
        log_err("%s Could not get the interface controller", currentNetworkInterface->deviceName);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }

    IOObjectRelease(IONetworkController);


    //
    // Check if we're UP, BROADCAST and not LOOPBACK.
    //
    CFNumberRef CFFlags = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOInterfaceFlags), kCFAllocatorDefault, kNilOptions);
    if (CFFlags){
        int64_t         flags = 0;
        CFNumberGetValue(CFFlags, kCFNumberIntType, &flags);
        currentNetworkInterface->flags = flags;

        if( (!(currentNetworkInterface->flags & (IFF_UP | IFF_BROADCAST))) | (currentNetworkInterface->flags & IFF_LOOPBACK)){
            log_err("%s Failed the flags check", currentNetworkInterface->deviceName);
            // Note: IONetworkController was already released, so we only register on the interface
            IOServiceAddInterestNotification(globalInfo.notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
            IOObjectRelease(IONetworkInterface);
            CFRelease(CFFlags);
            return;
        } else CFRelease(CFFlags);
    } else {
        log_err("%s Could not read Interface Flags", currentNetworkInterface->deviceName);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    
    //
    // Get the Interface Type.
    // A WiFi always conforms to 802.11 and Ethernet, so we check the 802.11
    // class first. On macOS 26+, we also check for IO80211 properties as fallback.
    // Also set ifType to the IANA interface type for the PhysicalMediumTLV.
    //
    if ( IsWirelessInterface(IONetworkInterface) ) {
        currentNetworkInterface->interfaceType=NetworkInterfaceTypeIEEE80211;
        currentNetworkInterface->ifType = 71;  // IANA ieee80211
    } else if ( IOObjectConformsTo( IONetworkInterface, "IOEthernetInterface" ) ) {
        currentNetworkInterface->interfaceType=NetworkInterfaceTypeEthernet;
        currentNetworkInterface->ifType = 6;   // IANA ethernetCsmacd
    } else {
        currentNetworkInterface->interfaceType=NetworkInterfaceTypeEthernet;
        currentNetworkInterface->ifType = 6;   // IANA ethernetCsmacd (default)
    }
    
    //
    // Add a a notification for anything happening to the device to the
    // run loop. The actual event gets filtered in the deviceDisappeared
    // function. We are also giving it a pointer to our networkInterface
    // so that we don't keep a global array of all interfaces.
    //
    kernel_return = IOServiceAddInterestNotification(globalInfo.notificationPort, IONetworkInterface, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
    if (kernel_return!=KERN_SUCCESS) {
        log_err("%s IOServiceAddInterestNofitication Interface error: 0x%08x.", currentNetworkInterface->deviceName, kernel_return);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    

    //
    // Get the object's parent (the interface controller) in order to watch the
    // controller for link state changes and add that to the runLoop
    // notifications list. The desired message is kIOMessageServicePropertyChange
    // and it affects kIOLinkStatus.
    //
    kernel_return = IORegistryEntryGetParentEntry( IONetworkInterface, kIOServicePlane, &IONetworkController);
    
    if (kernel_return != KERN_SUCCESS) log_err("%s Could not get the parent of the interface", currentNetworkInterface->deviceName);
    
    if(IONetworkController) {

        kernel_return = IOServiceAddInterestNotification(globalInfo.notificationPort, IONetworkController, kIOGeneralInterest, deviceDisappeared, currentNetworkInterface, &(currentNetworkInterface->notification));
        if (kernel_return!=KERN_SUCCESS){
            log_err("%s IOServiceAddInterestNofitication Controller error: 0x%08x.", currentNetworkInterface->deviceName, kernel_return);
            IOObjectRelease(IONetworkInterface);
            free((void*)currentNetworkInterface->deviceName);
            free(currentNetworkInterface);
            return;
        }
        IOObjectRelease(IONetworkController);
        
    } else {
        log_err("%s Could not get the parent of the interface", currentNetworkInterface->deviceName);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    

    
    //
    // After documenting the interface inside our currentNetworkInterface we
    // know that it's up, broadcast capable, not a loopback and has a link
    // So we move on to adding the thread
    //
    pthread_attr_t  threadAttributes;
    pthread_t       posixThreadID;
    int             returnVal;
    
    returnVal = pthread_attr_init(&threadAttributes);
    
    if (returnVal!=noErr){
        log_err("%s Could not init pthread attributes: %d", currentNetworkInterface->deviceName, returnVal);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    
    returnVal = pthread_attr_setdetachstate(&threadAttributes, PTHREAD_CREATE_DETACHED);
    if (returnVal!=noErr){
        log_err("%s Could not set pthread attributes: %d", currentNetworkInterface->deviceName, returnVal);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    

    int threadError = pthread_create(&posixThreadID, &threadAttributes, (void *)&lltdLoop, currentNetworkInterface);
    currentNetworkInterface->posixThreadID = posixThreadID;
    returnVal = pthread_attr_destroy(&threadAttributes);
    
    assert(!returnVal);
    
    if (threadError != KERN_SUCCESS) {
        log_err("%s Could not launch thread with error %d.", currentNetworkInterface->deviceName, threadError);
        IOObjectRelease(IONetworkInterface);
        free((void*)currentNetworkInterface->deviceName);
        free(currentNetworkInterface);
        return;
    }
    
    

}


//==============================================================================
//
// Print out the device that died on us to srdout, clean-up the notification and
// the network_interface_t object.
//
//==============================================================================
void deviceDisappeared(void *refCon, io_service_t service, natural_t messageType, void *messageArgument){
    
    network_interface_t *currentNetworkInterface = (network_interface_t*) refCon;
    io_name_t nameString;
    IORegistryEntryGetName(service, nameString);
#ifdef DEBUG
    if (IOObjectConformsTo(service, kIONetworkInterfaceClass)){
        log_debug("Notification received: %s type: %x networkInterfaceClass: %s" , currentNetworkInterface->deviceName, messageType, nameString);
    } else if (IOObjectConformsTo(service, kIONetworkControllerClass)){
        log_debug("Notification received: %s type: %x networkControllerClass: %s", currentNetworkInterface->deviceName, messageType, nameString);
    } else {
        log_debug("Notification received: %s type: %x networkSomethingClass: %s" , currentNetworkInterface->deviceName, messageType, nameString);
    }
#endif
    /*
     * Clean everything (thread, data, etc.)
     */
    if (messageType == kIOMessageServiceIsTerminated) {
        log_debug("Interface removed: %s", currentNetworkInterface->deviceName);
        pthread_cancel(currentNetworkInterface->posixThreadID);
        free((void *)currentNetworkInterface->deviceName);
        IOObjectRelease(currentNetworkInterface->notification);
        if(currentNetworkInterface->recvBuffer) {
            free(currentNetworkInterface->recvBuffer);
            currentNetworkInterface->recvBuffer = NULL;
        }
        //Clean the linked list
        probe_t *currentProbe = currentNetworkInterface->seeList;
        void    *nextProbe;
        if (currentProbe){
            for(uint32_t i=0; i < currentNetworkInterface->seeListCount; i++){
                nextProbe     = currentProbe->nextProbe;
                free(currentProbe);
                currentProbe  = nextProbe;
            }
        }
        currentNetworkInterface->seeList = NULL;
        free_automata(currentNetworkInterface->mappingAutomata);
        free_automata(currentNetworkInterface->sessionAutomata);
        free_automata(currentNetworkInterface->enumerationAutomata);
        session_table_destroy(currentNetworkInterface->sessionTable);
        free(currentNetworkInterface);
    /*
     * Keep the struct, the notification and the deviceName.
     * The notification is there to let us know when the device comes back
     * to life.
     * The device name is for logging only.
     */
    } else if (messageType == kIOMessageDeviceWillPowerOff) {
        log_debug("Interface powered off: %s", currentNetworkInterface->deviceName);
        if(currentNetworkInterface->recvBuffer) {
            free(currentNetworkInterface->recvBuffer);
            currentNetworkInterface->recvBuffer = NULL;
        }
        //Clean the linked list
        probe_t *currentProbe = currentNetworkInterface->seeList;
        void    *nextProbe;
        if (currentProbe){
            for(uint32_t i = 0; i<currentNetworkInterface->seeListCount; i++){
                nextProbe     = currentProbe->nextProbe;
                free(currentProbe);
                currentProbe  = nextProbe;
            }
        }
        currentNetworkInterface->seeList = NULL;
        pthread_cancel(currentNetworkInterface->posixThreadID);
    /*
     * Restart like new.
     * I initially wanted to compare the data and correct the deltas,
     * but it's easier to just let it stabilize for 100miliseconds and try
     * one more time from scratch.
     */
    } else if (messageType == kIOMessageServicePropertyChange) {
        //We should have some cleanup over here. There are only two properties that we care about.
        log_debug("A property has changed. Reloading thread. %s", currentNetworkInterface->deviceName);
        pthread_cancel(currentNetworkInterface->posixThreadID);
        usleep(100000);
        if(currentNetworkInterface->recvBuffer) {
            free(currentNetworkInterface->recvBuffer);
            currentNetworkInterface->recvBuffer=NULL;
        }
        //Kill the notification. We'll get a new one on validateInterface.
        IOObjectRelease(currentNetworkInterface->notification);
        //Clean the linked list
        probe_t *currentProbe = currentNetworkInterface->seeList;
        void    *nextProbe;
        if (currentProbe){
            for(uint32_t i = 0; i<currentNetworkInterface->seeListCount; i++){
                nextProbe     = currentProbe->nextProbe;
                free(currentProbe);
                currentProbe  = nextProbe;
            }
        }
        currentNetworkInterface->seeList = NULL;
        close(currentNetworkInterface->socket);
        validateInterface(currentNetworkInterface, service);
    /*
     * Restart. Since everything except for the device name and notification was
     * removed on DeviceWillPowerOff, we are now removing also the notification and
     * device name.
     */
    } else if (messageType == kIOMessageDeviceWillPowerOn) {
        log_debug("Interface powered on: %s", currentNetworkInterface->deviceName);
        free((void *)currentNetworkInterface->deviceName);
        IOObjectRelease(currentNetworkInterface->notification);
        validateInterface(currentNetworkInterface, service);
    }
    
}


//==============================================================================
//
// We identify the network interface and set the IOKit interest notification
// for any events regarding to it!
//
//==============================================================================
void deviceAppeared(void *refCon, io_iterator_t iterator){

    kern_return_t      kernel_return;
    io_service_t       IONetworkInterface;
    
    //
    // Let's loop through the interfaces that we've got
    //
    while ((IONetworkInterface = IOIteratorNext(iterator))){

        //
        // Let's initialize the networkInterface Data Structure
        //
        network_interface_t *currentNetworkInterface = NULL;
        currentNetworkInterface = malloc(sizeof(network_interface_t));
        memset(currentNetworkInterface, 0, sizeof(network_interface_t));
        currentNetworkInterface->mappingAutomata = init_automata_mapping();
        currentNetworkInterface->sessionAutomata = init_automata_session();
        currentNetworkInterface->enumerationAutomata = init_automata_enumeration();
        currentNetworkInterface->sessionTable = session_table_create();
        
        //
        // Let's get the device name. If we don't have a BSD name, we are
        // letting it stabilize for 50ms since SCNetwork didn't do it's thing
        // yet.
        //
        CFStringRef deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        if (deviceName == NULL) {
            usleep(100000);
            deviceName = IORegistryEntryCreateCFProperty(IONetworkInterface, CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
        }
        
        currentNetworkInterface->deviceName=malloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(deviceName), kCFStringEncodingUTF8));
        CFStringGetCString(deviceName, (char *)currentNetworkInterface->deviceName, CFStringGetMaximumSizeForEncoding(CFStringGetLength(deviceName), kCFStringEncodingUTF8), kCFStringEncodingUTF8);
        CFRelease(deviceName);

        validateInterface(currentNetworkInterface, IONetworkInterface);
        
        //
        // Clean-up.
        //
        IOObjectRelease(IONetworkInterface);
        
    } // while ((IONetworkInterface = IOIteratorNext(iterator))

}


//==============================================================================
//
// Check if another instance of lltdDaemon is already running.
// Uses an exclusive lock on a file to ensure single instance.
// Returns the lock fd on success, -1 if another instance is running.
//
//==============================================================================
static int acquireInstanceLock(void) {
    const char *lockPath = "/tmp/lltdDaemon.lock";
    int lockFd = open(lockPath, O_CREAT | O_RDWR, 0644);
    if (lockFd < 0) {
        fprintf(stderr, "Warning: Could not open lock file %s: %s\n", lockPath, strerror(errno));
        return -1;
    }

    if (flock(lockFd, LOCK_EX | LOCK_NB) < 0) {
        if (errno == EWOULDBLOCK) {
            fprintf(stderr, "Error: Another instance of lltdDaemon is already running.\n");
            fprintf(stderr, "Kill the existing process or wait for it to exit.\n");
        } else {
            fprintf(stderr, "Warning: Could not acquire lock: %s\n", strerror(errno));
        }
        close(lockFd);
        return -1;
    }

    // Write our PID to the lock file for debugging
    ftruncate(lockFd, 0);
    char pidStr[32];
    snprintf(pidStr, sizeof(pidStr), "%d\n", getpid());
    write(lockFd, pidStr, strlen(pidStr));

    return lockFd;
}

//==============================================================================
//
// main
// TODO: Convert to a Launch Daemon
//
//==============================================================================
int main(int argc, const char *argv[]){
    sig_t                 handler;
    kern_return_t         kernel_return;
    mach_port_t           masterPort;
    CFRunLoopSourceRef    runLoopSource;
    io_iterator_t         newDevicesIterator;
    int                   lockFd;

    //
    // Check for existing instance before doing anything else
    //
    lockFd = acquireInstanceLock();
    if (lockFd < 0) {
        return EXIT_FAILURE;
    }

    //
    // Create a new Apple System Log facility entry
    // of Daemon type with the LLTD name
    //
    globalInfo.asl     = NULL;
    globalInfo.log_msg = NULL;
    globalInfo.asl     = asl_open("LLTD", "Daemon", ASL_OPT_STDERR);
    globalInfo.log_msg = asl_new(ASL_TYPE_MSG);
    asl_set(globalInfo.log_msg, ASL_KEY_SENDER, "LLTD");

    
    //
    // Set up a signal handler so we can clean up when we're interrupted from
    // the command line. Otherwise we stay in our run loop forever.
    // SigInt = Ctrl+C
    // SigTerm = Killed by launchd
    //
    handler = signal(SIGINT, SignalHandler);
    if (handler == SIG_ERR) log_crit("Could not establish SIGINT handler.");
    handler = signal(SIGTERM, SignalHandler);
    if (handler == SIG_ERR) log_crit("Could not establish SIGTERM handler.");
    
    
    //
    // Creates and returns a notification object for receiving
    // IOKit notifications of new devices or state changes
    //
    masterPort        = MACH_PORT_NULL;
    kernel_return     = IOMasterPort(bootstrap_port, &masterPort);
    if (kernel_return!=KERN_SUCCESS) {
        log_err("IOMasterPort returned 0x%x", kernel_return);
        return EXIT_FAILURE;
    }
    
    //
    // Let's get the Run Loop ready. It needs a notification
    // port and a clean iterator.
    //
    newDevicesIterator = 0;
    globalInfo.notificationPort   = IONotificationPortCreate(masterPort);
    runLoopSource                 = IONotificationPortGetRunLoopSource(globalInfo.notificationPort);
    globalInfo.runLoop            = CFRunLoopGetCurrent();
    CFRunLoopAddSource(globalInfo.runLoop, runLoopSource, kCFRunLoopDefaultMode);

    //
    // Add the Service Notification to the Run Loop. It will
    // get the loop moving we we get a device added notification.
    // The device removed notifications are added in the Run Loop
    // from the actual "deviceAppeared" function.
    //
    kernel_return = IOServiceAddMatchingNotification(globalInfo.notificationPort, kIOFirstMatchNotification, IOServiceMatching(kIONetworkInterfaceClass), deviceAppeared, NULL, &newDevicesIterator);
    if (kernel_return!=KERN_SUCCESS) log_crit("IOServiceAddMatchingNotification(deviceAppeared) returned 0x%x", kernel_return);
    
    //
    // Do an initial device scan since the Run Loop is not yet
    // started. This will also add the notifications:
    // * Device disappeared;
    // * IOKit Property changed;
    // * From IOKit PowerManagement we get the sleep/hibernate
    //   shutdown and wake notifications;
    // * From SystemConfiguration we get the IPv4/IPv6 add,
    //   change, removed notifications (NOTYET);
    //
    deviceAppeared(NULL, newDevicesIterator);

    
    //
    // Start the run loop.
    //
    CFRunLoopRun();
    
    //
    // We should never get here
    //
    log_crit("Unexpectedly back from CFRunLoopRun()!");
    if (masterPort != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), masterPort);
    asl_close(globalInfo.asl);
    return EXIT_FAILURE;
    
}
