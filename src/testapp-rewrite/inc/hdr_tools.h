#ifndef HDR_TOOLS_H
#define HDR_TOOLS_H

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "constants.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pkt_hdr {
    struct rte_ether_hdr ether;
    struct rte_ipv4_hdr ip;
    struct rte_udp_hdr udp;
} __attribute__((aligned(2)));

enum hdr_direction {
    DIR_INCOMING = 0,
    DIR_OUTGOING = 1,
};

static inline void pkt_hdr_setup(struct pkt_hdr *hdr, struct config *conf,
                                 enum hdr_direction dir) {
    uint16_t pkt_len;
    uint16_t payload_len = conf->payload_size;

    struct portaddr *dst = (dir == DIR_INCOMING) ? &conf->local : &conf->remote;
    struct portaddr *src = (dir == DIR_INCOMING) ? &conf->remote : &conf->local;

    // Initialize ETH header (assumes outgoing packets)
    // FIXME: to print ethernet addresses use rte_ether_format_addr, to read
    // them from a string use rte_ether_unformat_addr
    rte_ether_addr_copy((struct rte_ether_addr *)src->mac.sll_addr,
                        &hdr->ether.s_addr);
    rte_ether_addr_copy((struct rte_ether_addr *)dst->mac.sll_addr,
                        &hdr->ether.d_addr);
    hdr->ether.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // Initialize IP header
    pkt_len = (uint16_t)(payload_len + sizeof(struct rte_ipv4_hdr));
    hdr->ip.version_ihl = IP_VERSION_HDRLEN;
    hdr->ip.type_of_service = 0;
    hdr->ip.fragment_offset = 0;
    hdr->ip.time_to_live = IP_DEFAULT_TTL;
    hdr->ip.next_proto_id = IPPROTO_UDP;
    hdr->ip.packet_id = 0;
    hdr->ip.total_length = rte_cpu_to_be_16(pkt_len);
    hdr->ip.src_addr = rte_cpu_to_be_32(src->ip.sin_addr.s_addr);
    hdr->ip.dst_addr = rte_cpu_to_be_32(dst->ip.sin_addr.s_addr);

    // Compute IP header checksum
    hdr->ip.hdr_checksum = 0;
    hdr->ip.hdr_checksum = rte_ipv4_cksum(&hdr->ip);

    // Initialize UDP header
    pkt_len = (uint16_t)(payload_len + sizeof(struct rte_udp_hdr));
    hdr->udp.src_port = rte_cpu_to_be_16(src->ip.sin_port);
    hdr->udp.dst_port = rte_cpu_to_be_16(dst->ip.sin_port);
    hdr->udp.dgram_len = rte_cpu_to_be_16(pkt_len);
    hdr->udp.dgram_cksum = 0; /* No UDP checksum. */
}

/**
 * Swaps the content of the memory area pointed by p1 with the one pointed by
 * p2. The memory area pointed by tmp is used as support memory for the swap
 * operation.
 *
 * All pointers must point to an area of at least size bytes.
 * */
static inline void swap_ptrs(void *p1, void *p2, void *tmp, size_t size) {
    rte_memcpy(tmp, p1, size);
    rte_memcpy(p1, p2, size);
    rte_memcpy(p2, tmp, size);
}

static inline void swap_ether_addr(struct rte_ether_hdr *hdr) {
    struct rte_ether_addr support;
    struct rte_ether_addr *s_addr = &hdr->s_addr;
    struct rte_ether_addr *d_addr = &hdr->d_addr;
    swap_ptrs(s_addr, d_addr, &support, sizeof(struct rte_ether_addr));
}

static inline void swap_ipv4_addr(struct rte_ipv4_hdr *hdr) {
    uint32_t support;
    byte_t *src_addr = (byte_t *)&hdr->src_addr;
    byte_t *dst_addr = (byte_t *)&hdr->dst_addr;
    swap_ptrs(src_addr, dst_addr, &support, sizeof(uint32_t));
}

static inline void swap_udp_port(struct rte_udp_hdr *hdr) {
    uint16_t support;
    byte_t *src_port = (byte_t *)&hdr->src_port;
    byte_t *dst_port = (byte_t *)&hdr->dst_port;
    swap_ptrs(src_port, dst_port, &support, sizeof(uint16_t));
}

static inline bool hdr_check_incoming(const struct pkt_hdr *received_hdr,
                                      const struct pkt_hdr *expected_hdr) {
    // Check if destination MAC address is correct
    if (!rte_is_same_ether_addr(&received_hdr->ether.d_addr,
                                &expected_hdr->ether.d_addr))
        return false;

    // Check if destination IP address is correct
    if (received_hdr->ip.dst_addr != expected_hdr->ip.dst_addr)
        return false;

    // Check if destination UDP port is correct
    if (received_hdr->udp.dst_port != expected_hdr->udp.dst_port)
        return false;

    return true;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HDR_TOOLS_H
