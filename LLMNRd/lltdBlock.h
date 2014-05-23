//
//  lltdBlock.h
//  LLMNRd
//
//  Created by Răzvan Corneliu C.R. VILT on 23.04.2014.
//  Copyright (c) 2014 Răzvan Corneliu C.R. VILT. All rights reserved.
//

#ifndef LLMNRd_lltdBlock_h
#define LLMNRd_lltdBlock_h
#include <netinet/in.h>
#include <net/if.h>
#include <net/ndrv.h>
#include "llmnrd.h"
#include "darwin-ops.h"
//#include "tlv-ops.h"


void lltdBlock (void *data);
void parseFrame(void *frame, void *networkInterface);

#pragma pack( push )
#pragma pack( 2 )

typedef struct {
    uint8_t            a[6];
} ethernet_address_t;

typedef struct {
    ethernet_address_t destination;
    ethernet_address_t source;
    uint16_t           ethertype;
} ethernet_header_t;

typedef struct {
    ethernet_header_t  frameHeader;
    uint8_t            version;
    uint8_t            tos;
    uint8_t            reserved;
    uint8_t            opcode;
    ethernet_address_t realDestination;
    ethernet_address_t realSource;
    uint16_t           seqNumber;
} lltd_demultiplex_header_t;

typedef struct {
    uint16_t           generation;
    uint16_t           stationNumber;
    ethernet_header_t  stationList[1];
} lltd_discover_upper_header_t;

typedef struct {
    uint16_t           numDescs;
} lltd_emit_upper_header_t;

typedef struct {
    uint8_t            type;
    uint8_t            pause;
    ethernet_address_t sourceAddr;
    ethernet_address_t destAddr;
} emitee_descs;

typedef struct {
    uint16_t           generation;
    ethernet_address_t currentMapper;
    ethernet_address_t apparentMapper;
} lltd_hello_upper_header_t;

typedef struct {
    uint16_t           numDescs;
} qry_resp_upper_header_t;

typedef struct {
    uint16_t           type;
    ethernet_address_t realSourceAddr;
    ethernet_address_t sourceAddr;
    ethernet_address_t destAddr;
} probe_t;

typedef struct {
    uint8_t            type;
    uint8_t            length;
} tlv_header;

#pragma pack ( pop )
#endif

#define tos_discovery             0x00
#define tos_quick_discovery       0x01
#define tos_qos_diagnostics       0x02
#define opcode_discover           0x00
#define opcode_hello              0x01
#define opcode_emit               0x02
#define opcode_train              0x03
#define opcode_probe              0x04
#define opcode_ack                0x05
#define opcode_query              0x06
#define opcode_queryResp          0x07
#define opcode_reset              0x08
#define opcode_charge             0x09
#define opcode_flat               0x0A
#define opcode_queryLargeTlv      0x0B
#define opcode_queryLargeTlvResp  0x0C
#define opcode_qosInitializeSink  0x00
#define opcode_qosReady           0x01
#define opcode_qosProbe           0x02
#define opcode_qosQuery           0x03
#define opcode_qosQueryResp       0x04
#define opcode_qosReset           0x05
#define opcode_qosError           0x06
#define opcode_qosAck             0x07
#define opcode_qosCounterSnapshot 0x08
#define opcode_qosCounterResult   0x09
#define opcode_qosCounterLease    0x0A
#define tlv_hostId                0x01
#define tlv_characterisics        0x02
#define tlv_ifType                0x03
#define tlv_wifimode              0x04
#define tlv_bssid                 0x05
#define tlv_ssid                  0x06
#define tlv_ipv4                  0x07
#define tlv_ipv6                  0x08
#define tlv_wifiMaxRate           0x09
#define tlv_perfCounterFrequency  0x0A
#define tlv_linkSpeed             0x0C
#define tlv_wifiRssi              0x0D
#define tlv_iconImage             0x0E
#define tlv_hostname              0x0F
#define tlv_supportUrl            0x10
#define tlv_friendlyName          0x11
#define tlv_uuid                  0x12
#define tlv_hwIdProperty          0x13
#define tlv_qos_characteristics   0x14
#define tlv_component_table       0x1a
#define Config_TLV_PublicNAT_Value              0x8000
#define Config_TLV_PrivateNAT_Value             0x4000
#define Config_TLV_NetworkInterfaceDuplex_Value 0x2000
#define Config_TLV_HasManagementURL_Value       0x1000
#define Config_TLV_InterfaceIsLoopback_Value    0x800
#define Config_TLV_QOS_L2Fwd      0x8000
#define Config_TLV_QOS_VLAN       0x4000
#define Config_TLV_QOS_PrioTag    0x2000
#define eofpropmarker             0x00