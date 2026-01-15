/******************************************************************************
 *                                                                            *
 *   lltdBlock.c                                                              *
 *   lltdDaemon                                                               *
 *                                                                            *
 *   Created by Răzvan Corneliu C.R. VILT on 23.03.2014.                      *
 *   Copyright (c) 2014-2026 Răzvan Corneliu C.R. VILT. All rights reserved.  *
 *                                                                            *
 ******************************************************************************/

#include "lltdBlock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lltdEndian.h"
#include "lltdPort.h"
#include "lltdTlvOps.h"
#include "lltdWire.h"

typedef struct lltd_iface_state {
    void *iface_ctx;
    struct lltd_iface_state *next;

    probe_t *see_list;
    uint32_t see_list_count;

    ethernet_address_t mapper_real;
    ethernet_address_t mapper_apparent;
    uint8_t mapper_known;
    uint16_t mapper_seq;
    uint16_t mapper_gen_topology;
    uint16_t mapper_gen_quick;

    void *small_icon;
    size_t small_icon_size;
} lltd_iface_state;

static lltd_iface_state *g_iface_states = NULL;

#define log_debug(...) lltd_port_log_debug(__VA_ARGS__)
#define log_warning(...) lltd_port_log_warning(__VA_ARGS__)
#define log_err(...) lltd_port_log_warning(__VA_ARGS__)
#define log_crit(...) lltd_port_log_warning(__VA_ARGS__)
#define log_alert(...) lltd_port_log_warning(__VA_ARGS__)

static lltd_iface_state *lltd_state_for_iface(void *iface_ctx) {
    for (lltd_iface_state *cur = g_iface_states; cur != NULL; cur = cur->next) {
        if (cur->iface_ctx == iface_ctx) {
            return cur;
        }
    }

    lltd_iface_state *st = (lltd_iface_state *)lltd_port_malloc(sizeof(*st));
    if (!st) {
        return NULL;
    }
    lltd_port_memset(st, 0, sizeof(*st));
    st->iface_ctx = iface_ctx;
    st->next = g_iface_states;
    g_iface_states = st;
    return st;
}

static void lltd_state_clear_seen_probes(lltd_iface_state *st) {
    if (!st) {
        return;
    }
    probe_t *cur = st->see_list;
    while (cur) {
        probe_t *next = (probe_t *)cur->nextProbe;
        lltd_port_free(cur);
        cur = next;
    }
    st->see_list = NULL;
    st->see_list_count = 0;
}

static void lltd_state_clear_icon_cache(lltd_iface_state *st) {
    if (!st) {
        return;
    }
    if (st->small_icon) {
        lltd_port_free(st->small_icon);
    }
    st->small_icon = NULL;
    st->small_icon_size = 0;
}

static uint64_t lltd_now_ms(void) {
    return lltd_port_monotonic_milliseconds();
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
        lltd_port_log_debug("t=%llu %s %s tos=%u op=%u xid=0x%04x gen=0x%04x seq=0x%04x reason=%s",
                            (unsigned long long)lltd_now_ms(),
                            direction, ifname, tos, opcode, xid, generation, seq, reason);
    } else {
        lltd_port_log_debug("t=%llu %s %s tos=%u op=%u xid=0x%04x gen=0x%04x seq=0x%04x",
                            (unsigned long long)lltd_now_ms(),
                            direction, ifname, tos, opcode, xid, generation, seq);
    }
}

static void log_generation_warning_if_swapped(lltd_iface_state *st,
                                              uint16_t generation_host,
                                              uint8_t tos,
                                              const char *context) {
    uint16_t prior = (tos == tos_quick_discovery)
        ? st->mapper_gen_quick
        : st->mapper_gen_topology;
    if (prior != 0 && generation_host == lltd_bswap16(prior)) {
        log_warning("%s tos=%u generation looks byte-swapped: prev=0x%04x now=0x%04x",
                    context ? context : "frame", tos, prior, generation_host);
    }
}

static uint16_t *mapper_generation_for_tos(lltd_iface_state *st, uint8_t tos) {
    return (tos == tos_quick_discovery)
        ? &st->mapper_gen_quick
        : &st->mapper_gen_topology;
}

static const char *mapper_generation_label(uint8_t tos) {
    return (tos == tos_quick_discovery) ? "quick" : "topology";
}

static bool mapper_matches(lltd_iface_state *st, const ethernet_address_t *real_src) {
    if (!st->mapper_known) {
        return true;
    }
    return compareEthernetAddress(&st->mapper_real, real_src);
}

static void set_active_mapper(lltd_iface_state *st,
                              const ethernet_address_t *real_src,
                              const ethernet_address_t *eth_src) {
    if (!st->mapper_known) {
        st->mapper_real = *real_src;
        st->mapper_apparent = *eth_src;
        st->mapper_known = 1;
    }
}

static void warn_generation_store_mismatch(lltd_iface_state *st,
                                           uint8_t tos,
                                           const char *context,
                                           const uint16_t *slot) {
    if (tos == tos_quick_discovery && slot != &st->mapper_gen_quick) {
        log_warning("%s tos=%u using topology generation for quick hello",
                    context ? context : "frame", tos);
    } else if (tos == tos_discovery && slot != &st->mapper_gen_topology) {
        log_warning("%s tos=%u using quick generation for topology hello",
                    context ? context : "frame", tos);
    }
}

static bool sendProbeMsg(ethernet_address_t src,
                         ethernet_address_t dst,
                         lltd_iface_state *st,
                         void *iface_ctx,
                         int pause_ms,
                         uint8_t type,
                         bool ack) {
    size_t packageSize = sizeof(lltd_demultiplex_header_t);
    lltd_demultiplex_header_t *probe = (lltd_demultiplex_header_t *)lltd_port_malloc(packageSize);
    if (!probe) {
        log_crit("sendProbeMsg: malloc failed");
        return false;
    }
    lltd_port_memset(probe, 0, packageSize);

    uint8_t code = (type == 0x01) ? opcode_probe : opcode_train;

    ethernet_address_t our_mac = {{0, 0, 0, 0, 0, 0}};
    (void)lltd_port_get_mac_address(iface_ctx, &our_mac);

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
                    (const ethernet_address_t *)&our_mac,                            /* realSource: our MAC */
                    &st->mapper_real,                                               /* realDest: mapper real */
                    0, code, tos_discovery);

    log_lltd_frame("TX",
                   NULL,
                   tos_discovery,
                   code,
                   0,
                   0,
                   0,
                   "probe_train");

    lltd_port_sleep_ms((uint32_t)pause_ms);
    if (lltd_port_send_frame(iface_ctx, probe, packageSize) < 0) {
        log_warning("sendProbeMsg: send_frame failed (%zu bytes, opcode=%u)", packageSize, code);
        lltd_port_free(probe);
        return false;
    }

    if (ack) {
        /*
         * ACK frame back to mapper:
         * - Ethernet dest = apparent mapper address (next-hop through bridge)
         * - LLTD realDest = real mapper address (end-to-end identity)
         */
        setLltdHeaderEx(probe,
                        &our_mac,
                        &st->mapper_apparent,
                        &our_mac,
                        &st->mapper_real,
                        st->mapper_seq, opcode_ack, tos_discovery);

        log_lltd_frame("TX",
                       NULL,
                       tos_discovery,
                       opcode_ack,
                       0,
                       0,
                       st->mapper_seq,
                       "probe_ack");
        if (lltd_port_send_frame(iface_ctx, probe, packageSize) < 0) {
            log_warning("sendProbeMsg: send_frame failed on ACK (%zu bytes)", packageSize);
        }
    }

    lltd_port_free(probe);
    return true;
}

static void parseQuery(void *inFrame, lltd_iface_state *st, void *iface_ctx) {
    lltd_demultiplex_header_t *inHeader = (lltd_demultiplex_header_t *)inFrame;

    st->mapper_seq = lltd_ntohs(inHeader->seqNumber);
    st->mapper_real = inHeader->realSource;
    st->mapper_apparent = inHeader->frameHeader.source;
    st->mapper_known = 1;

    size_t mtu = 0;
    if (lltd_port_get_mtu(iface_ctx, &mtu) != 0 || mtu == 0) {
        mtu = 1500;
    }

    uint8_t *buffer = (uint8_t *)lltd_port_malloc(mtu);
    if (!buffer) {
        return;
    }
    lltd_port_memset(buffer, 0, mtu);

    const ethernet_address_t *destAddr;
    if (!compareEthernetAddress(&inHeader->realSource, &inHeader->frameHeader.source)) {
        destAddr = &EthernetBroadcast;
    } else {
        destAddr = &inHeader->realSource;
    }

    ethernet_address_t our_mac = {{0, 0, 0, 0, 0, 0}};
    (void)lltd_port_get_mac_address(iface_ctx, &our_mac);

    size_t offset = setLltdHeader(buffer,
                                  &our_mac,
                                  (ethernet_address_t *)(uintptr_t)destAddr,
                                  st->mapper_seq,
                                  opcode_queryResp,
                                  tos_discovery);

    qry_resp_upper_header_t *respH =
        (qry_resp_upper_header_t *)(buffer + sizeof(lltd_demultiplex_header_t));

    typedef struct {
        uint16_t type;
        ethernet_address_t realSourceAddr;
        ethernet_address_t sourceAddr;
        ethernet_address_t destAddr;
    } lltd_probe_desc_wire_t;

    size_t max_descs = 0;
    if (mtu > sizeof(lltd_demultiplex_header_t) + sizeof(*respH)) {
        max_descs = (mtu - sizeof(lltd_demultiplex_header_t) - sizeof(*respH)) / sizeof(lltd_probe_desc_wire_t);
    }

    uint16_t num_descs = (st->see_list_count > max_descs) ? (uint16_t)max_descs : (uint16_t)st->see_list_count;
    respH->numDescs = lltd_htons(num_descs);
    offset += sizeof(*respH);

    probe_t *node = st->see_list;
    uint16_t remaining = num_descs;
    while (node != NULL && remaining > 0) {
        lltd_probe_desc_wire_t wire;
        lltd_port_memset(&wire, 0, sizeof(wire));
        wire.type = node->type; /* already network-order */
        wire.realSourceAddr = node->realSourceAddr;
        wire.sourceAddr = node->sourceAddr;
        wire.destAddr = node->destAddr;

        if (offset + sizeof(wire) > mtu) {
            break;
        }
        lltd_port_memcpy(buffer + offset, &wire, sizeof(wire));
        offset += sizeof(wire);

        node = (probe_t *)node->nextProbe;
        remaining--;
    }

    (void)lltd_port_send_frame(iface_ctx, buffer, offset);
    lltd_port_free(buffer);

    lltd_state_clear_seen_probes(st);
}

static void sendLargeTlvResponse(lltd_iface_state *st,
                                 void *iface_ctx,
                                 void *inFrame,
                                 const void *data,
                                 size_t dataSize,
                                 uint16_t dataOffset) {
    lltd_demultiplex_header_t *inHeader = (lltd_demultiplex_header_t *)inFrame;

    size_t mtu = 0;
    if (lltd_port_get_mtu(iface_ctx, &mtu) != 0 || mtu == 0) {
        mtu = 1500;
    }
    uint16_t maxPayload = 0;
    if (mtu > sizeof(lltd_demultiplex_header_t) + sizeof(qry_large_tlv_resp_t)) {
        maxPayload = (uint16_t)(mtu - sizeof(lltd_demultiplex_header_t) - sizeof(qry_large_tlv_resp_t));
    }

    /*
     * Per MS-LLTD 2.2.3.8 (QueryLargeTlvResp Frame):
     * If Real Source Address != Ethernet source address (i.e., behind bridge/AP),
     * response MUST be sent to broadcast; otherwise send to Real Source.
     */
    const ethernet_address_t *destAddr;
    if (!compareEthernetAddress(&inHeader->realSource, &inHeader->frameHeader.source)) {
        destAddr = &EthernetBroadcast;
    } else {
        destAddr = &inHeader->realSource;
    }

    size_t bufferSize = sizeof(lltd_demultiplex_header_t) + sizeof(qry_large_tlv_resp_t) + (size_t)maxPayload;
    uint8_t *buffer = (uint8_t *)lltd_port_malloc(bufferSize);
    if (!buffer) {
        return;
    }
    lltd_port_memset(buffer, 0, bufferSize);

    ethernet_address_t our_mac = {{0, 0, 0, 0, 0, 0}};
    (void)lltd_port_get_mac_address(iface_ctx, &our_mac);

    setLltdHeader(buffer,
                  &our_mac,
                  (ethernet_address_t *)(uintptr_t)destAddr,
                  st->mapper_seq,
                  opcode_queryLargeTlvResp,
                  tos_discovery);

    qry_large_tlv_resp_t *header = (qry_large_tlv_resp_t *)(buffer + sizeof(lltd_demultiplex_header_t));

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
    header->length = lltd_htons(header->length);

    size_t packageSize = sizeof(lltd_demultiplex_header_t) + sizeof(qry_large_tlv_resp_t) + (size_t)bytesToWrite;
    if (bytesToWrite > 0 && data != NULL) {
        lltd_port_memcpy(buffer + sizeof(lltd_demultiplex_header_t) + sizeof(qry_large_tlv_resp_t),
                         (const uint8_t *)data + dataOffset,
                         bytesToWrite);
    }

    log_lltd_frame("TX", NULL, tos_discovery, opcode_queryLargeTlvResp, 0, 0, st->mapper_seq, "query_large_tlv_resp");
    (void)lltd_port_send_frame(iface_ctx, buffer, packageSize);
    lltd_port_free(buffer);
}

static void parseQueryLargeTlv(void *inFrame, lltd_iface_state *st, void *iface_ctx) {
    lltd_demultiplex_header_t *lltdHeader = (lltd_demultiplex_header_t *)inFrame;
    if (lltd_ntohs(lltdHeader->seqNumber) == 0) {
        // as per spec, ignore LargeTLV with zeroed sequence Number
        return;
    }
    st->mapper_seq = lltd_ntohs(lltdHeader->seqNumber);
    set_active_mapper(st, &lltdHeader->realSource, &lltdHeader->frameHeader.source);

    qry_large_tlv_t *header = (qry_large_tlv_t *)((uint8_t *)inFrame + sizeof(lltd_demultiplex_header_t));
    uint16_t offset = lltd_ntohs(header->offset);

    void *data = NULL;
    size_t dataSize = 0;
    bool should_free = false;

    switch (header->type) {
        case tlv_iconImage:
            log_crit("QueryLargeTLV: Icon Image request, offset=%d", offset);
            if (!st->small_icon && st->small_icon_size == 0) {
                if (lltd_port_get_icon_image(&st->small_icon, &st->small_icon_size) != 0) {
                    st->small_icon = NULL;
                    st->small_icon_size = 0;
                }
            }
            data = st->small_icon;
            dataSize = st->small_icon_size;
            break;

        case tlv_friendlyName:
            log_crit("QueryLargeTLV: Friendly Name request, offset=%d", offset);
            if (lltd_port_get_friendly_name(&data, &dataSize) == 0) {
                should_free = true;
            }
            break;

        case tlv_hwIdProperty:
            log_crit("QueryLargeTLV: Hardware ID request, offset=%d", offset);
            data = lltd_port_malloc(64);
            if (data) {
                lltd_port_memset(data, 0, 64);
                (void)lltd_port_get_hw_id(data, 64);
                dataSize = 64;
                for (size_t i = 0; i + 1 < 64; i += 2) {
                    if (((uint8_t *)data)[i] == 0 && ((uint8_t *)data)[i + 1] == 0) {
                        dataSize = i;
                        break;
                    }
                }
                should_free = true;
            }
            break;

        default:
            log_crit("QueryLargeTLV: Unknown TLV type 0x%02x requested", header->type);
            // Send empty response for unknown types
            break;
    }

    sendLargeTlvResponse(st, iface_ctx, inFrame, data, dataSize, offset);

    // Free non-cached data
    if (should_free && data) {
        lltd_port_free(data);
    }
}

static void parseProbe(void *inFrame, lltd_iface_state *st, void *iface_ctx) {
    lltd_demultiplex_header_t *header = (lltd_demultiplex_header_t *)inFrame;

    ethernet_address_t our_mac = {{0, 0, 0, 0, 0, 0}};
    (void)lltd_port_get_mac_address(iface_ctx, &our_mac);

    bool forUs = compareEthernetAddress(&header->realDestination, &our_mac);
    if (!forUs) {
        return;
    }

    probe_t *probe = (probe_t *)lltd_port_malloc(sizeof(*probe));
    if (!probe) {
        return;
    }

    probe->type = lltd_htons((header->opcode == opcode_probe) ? 1 : 0);
    probe->sourceAddr = header->frameHeader.source;
    probe->destAddr = header->frameHeader.destination;
    probe->realSourceAddr = header->realSource;
    probe->nextProbe = NULL;

    bool found = false;
    for (probe_t *cur = st->see_list; cur != NULL; cur = (probe_t *)cur->nextProbe) {
        if (compareEthernetAddress(&probe->sourceAddr, &cur->sourceAddr) &&
            compareEthernetAddress(&probe->realSourceAddr, &cur->realSourceAddr)) {
            found = true;
            break;
        }
    }

    if (found) {
        lltd_port_free(probe);
        return;
    }

    probe->nextProbe = st->see_list;
    st->see_list = probe;
    st->see_list_count++;
}

static void parseEmit(void *inFrame, lltd_iface_state *st, void *iface_ctx) {
    lltd_demultiplex_header_t *lltdHeader = (lltd_demultiplex_header_t *)inFrame;
    lltd_emit_upper_header_t *emitHeader =
        (lltd_emit_upper_header_t *)((uint8_t *)lltdHeader + sizeof(*lltdHeader));

    st->mapper_seq = lltd_ntohs(lltdHeader->seqNumber);
    set_active_mapper(st, &lltdHeader->realSource, &lltdHeader->frameHeader.source);

    int numDescs = (int)lltd_ntohs(emitHeader->numDescs);
    uint16_t offsetEmitee = 0;

    for (int i = 0; i < numDescs; i++) {
        bool ack = (i == numDescs - 1);
        emitee_descs *emitee = (emitee_descs *)((uint8_t *)emitHeader + sizeof(*emitHeader) + offsetEmitee);
        if (emitee->type == 1 || emitee->type == 0) {
            (void)sendProbeMsg(emitee->sourceAddr,
                               emitee->destAddr,
                               st,
                               iface_ctx,
                               emitee->pause,
                               emitee->type,
                               ack);
        } else {
            log_warning("parseEmit: Unknown emitee type=%u", emitee->type);
        }
        offsetEmitee += (uint16_t)sizeof(emitee_descs);
    }
}


//==============================================================================
//
// This is the Hello answer to any Discovery package.
// FIXME: Hello header casting is b0rken.
//
//==============================================================================
static void answerHello(void *inFrame, lltd_iface_state *st, void *iface_ctx) {
    lltd_demultiplex_header_t *inFrameHeader = (lltd_demultiplex_header_t *)inFrame;
    lltd_discover_upper_header_t *discoverHeader =
        (lltd_discover_upper_header_t *)((uint8_t *)inFrameHeader + sizeof(*inFrameHeader));

    size_t mtu = 0;
    if (lltd_port_get_mtu(iface_ctx, &mtu) != 0 || mtu == 0) {
        mtu = 1500;
    }

    uint8_t *buffer = (uint8_t *)lltd_port_malloc(mtu);
    if (!buffer) {
        return;
    }
    lltd_port_memset(buffer, 0, mtu);

    ethernet_address_t our_mac = {{0, 0, 0, 0, 0, 0}};
    (void)lltd_port_get_mac_address(iface_ctx, &our_mac);

    set_active_mapper(st, &inFrameHeader->realSource, &inFrameHeader->frameHeader.source);
    st->mapper_seq = lltd_ntohs(inFrameHeader->seqNumber);

    uint16_t discover_generation = lltd_ntohs(discoverHeader->generation);
    uint16_t *generation_slot = mapper_generation_for_tos(st, inFrameHeader->tos);
    if (*generation_slot == 0 && discover_generation != 0) {
        *generation_slot = discover_generation;
    }
    warn_generation_store_mismatch(st, inFrameHeader->tos, "answerHello", generation_slot);

    size_t offset = setLltdHeader(buffer,
                                  &our_mac,
                                  (ethernet_address_t *)(uintptr_t)&EthernetBroadcast,
                                  0,
                                  opcode_hello,
                                  inFrameHeader->tos);

    offset += setHelloHeader(buffer,
                             offset,
                             &inFrameHeader->frameHeader.source,
                             &inFrameHeader->realSource,
                             *generation_slot);

    offset += setHostIdTLV(buffer, offset, iface_ctx);
    offset += setCharacteristicsTLV(buffer, offset, iface_ctx);
    offset += setPhysicalMediumTLV(buffer, offset, iface_ctx);
    offset += setIPv4TLV(buffer, offset, iface_ctx);
    offset += setIPv6TLV(buffer, offset, iface_ctx);
    offset += setPerfCounterTLV(buffer, offset);
    offset += setLinkSpeedTLV(buffer, offset, iface_ctx);
    offset += setHostnameTLV(buffer, offset);

    uint8_t wifi_mode = 0;
    if (lltd_port_get_wifi_mode(iface_ctx, &wifi_mode) == 0) {
        offset += setWirelessTLV(buffer, offset, iface_ctx);
        offset += setBSSIDTLV(buffer, offset, iface_ctx);
        offset += setSSIDTLV(buffer, offset, iface_ctx);
        offset += setWifiMaxRateTLV(buffer, offset, iface_ctx);
        offset += setWifiRssiTLV(buffer, offset, iface_ctx);
        offset += setAPAssociationTableTLV(buffer, offset, iface_ctx);
        offset += setRepeaterAPLineageTLV(buffer, offset, iface_ctx);
        offset += setRepeaterAPTableTLV(buffer, offset, iface_ctx);
    }

    offset += setQosCharacteristicsTLV(buffer, offset);
    offset += setIconImageTLV(buffer, offset);
    offset += setFriendlyNameTLV(buffer, offset);
    offset += setEndOfPropertyTLV(buffer, offset);

    (void)lltd_port_send_frame(iface_ctx, buffer, offset);
    lltd_port_free(buffer);
}

//==============================================================================
//
// Here we validate the frame and make sure that the TOS/OpCode is a valid
// combination.
// TODO: Add a method to validate that we have the correct TLV combinations for each frame.
//
//==============================================================================
void parseFrame(void *frame, void *iface_ctx) {
    if (!frame) {
        return;
    }

    lltd_demultiplex_header_t *header = (lltd_demultiplex_header_t *)frame;
    lltd_iface_state *st = lltd_state_for_iface(iface_ctx);
    if (!st) {
        return;
    }

    if (header->opcode == opcode_discover) {
        lltd_discover_upper_header_t *disc_header =
            (lltd_discover_upper_header_t *)((uint8_t *)frame + sizeof(*header));
        uint16_t generation_host = lltd_ntohs(disc_header->generation);
        log_generation_warning_if_swapped(st, generation_host, header->tos, "Discover");

        if (!mapper_matches(st, &header->realSource)) {
            return;
        }
        set_active_mapper(st, &header->realSource, &header->frameHeader.source);

        uint16_t *slot = mapper_generation_for_tos(st, header->tos);
        if (*slot == 0 && generation_host != 0) {
            *slot = generation_host;
        } else if (*slot != generation_host) {
            *slot = generation_host;
        }
    } else if (header->opcode == opcode_hello) {
        lltd_hello_upper_header_t *hello_header =
            (lltd_hello_upper_header_t *)((uint8_t *)frame + sizeof(*header));
        uint16_t generation_host = lltd_ntohs(hello_header->generation);
        log_generation_warning_if_swapped(st, generation_host, header->tos, "Hello");
    }

    switch (header->tos) {
        case tos_discovery:
            switch (header->opcode) {
                case opcode_discover:
                    if (mapper_matches(st, &header->realSource)) {
                        lltd_port_sleep_ms(10);
                        answerHello(frame, st, iface_ctx);
                    }
                    break;
                case opcode_emit:
                    parseEmit(frame, st, iface_ctx);
                    break;
                case opcode_train:
                case opcode_probe:
                    parseProbe(frame, st, iface_ctx);
                    break;
                case opcode_query:
                    parseQuery(frame, st, iface_ctx);
                    break;
                case opcode_queryLargeTlv:
                    parseQueryLargeTlv(frame, st, iface_ctx);
                    break;
                case opcode_reset:
                    lltd_state_clear_seen_probes(st);
                    lltd_state_clear_icon_cache(st);
                    st->mapper_known = 0;
                    st->mapper_seq = 0;
                    st->mapper_gen_topology = 0;
                    st->mapper_gen_quick = 0;
                    break;
                default:
                    break;
            }
            break;
        case tos_quick_discovery:
            switch (header->opcode) {
                case opcode_discover:
                    if (mapper_matches(st, &header->realSource)) {
                        answerHello(frame, st, iface_ctx);
                    }
                    break;
                case opcode_queryLargeTlv:
                    parseQueryLargeTlv(frame, st, iface_ctx);
                    break;
                case opcode_reset:
                    st->mapper_known = 0;
                    st->mapper_gen_quick = 0;
                    break;
                default:
                    break;
            }
            break;
        case tos_qos_diagnostics:
        default:
            break;
    }
}
