#ifndef DPDK_MINE_H
#define DPDK_MINE_H

// -------------------------- UNUSED INCLUDE -----------------------------------

// INCLUDES
#include <rte_ether.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>

#include "common.h"
#include "timestamp.h"

#define IP_DEFAULT_TTL 64 /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HEADER_LEN 0x05 /* default IP header length == five 32-bits words. */
#define IP_VERSION_HDRLEN (IP_VERSION | IP_HEADER_LEN)

extern int dpdk_init(int argc, char *argv[], struct config *conf);

#define OFFSET_ETHER (0)
#define OFFSET_IPV4 (OFFSET_ETHER + sizeof(struct ether_hdr))
#define OFFSET_UDP (OFFSET_IPV4 + sizeof(struct ipv4_hdr))
#define OFFSET_PAYLOAD (OFFSET_UDP + sizeof(struct udp_hdr))
// #define OFFSET_PAYLOAD_tsc (OFFSET_PAYLOAD)

#define PKT_HEADER_SIZE (OFFSET_PAYLOAD - OFFSET_ETHER)

extern void dpdk_setup_pkt_headers(
    struct ether_hdr *eth_hdr,
    struct ipv4_hdr *ip_hdr,
    struct udp_hdr *udp_hdr,
    struct config *conf);

// Assuming that a packet will always fit into a buffer
static inline void copy_buf_to_pkt(
    void *buf, unsigned len, struct rte_mbuf *pkt, unsigned offset)
{
    rte_memcpy(rte_pktmbuf_mtod_offset(pkt, char *, offset), buf, (size_t)len);
}

static inline void copy_pkt_to_buf(
    void *buf, unsigned len, struct rte_mbuf *pkt, unsigned offset)
{
    rte_memcpy(buf, rte_pktmbuf_mtod_offset(pkt, char *, offset), (size_t)len);
}

static inline void dpdk_pkt_prepare(struct rte_mbuf *pkt,
                                    struct config *conf,
                                    struct ether_hdr *pkt_eth_hdr,
                                    struct ipv4_hdr *pkt_ip_hdr,
                                    struct udp_hdr *pkt_udp_hdr)
{
    rte_pktmbuf_reset_headroom(pkt);
    pkt->data_len = conf->pkt_size;
    pkt->pkt_len = pkt->data_len;
    pkt->next = NULL;

    copy_buf_to_pkt(pkt_eth_hdr,
                    sizeof(struct ether_hdr),
                    pkt,
                    OFFSET_ETHER);
    copy_buf_to_pkt(pkt_ip_hdr,
                    sizeof(struct ipv4_hdr),
                    pkt,
                    OFFSET_IPV4);
    copy_buf_to_pkt(pkt_udp_hdr,
                    sizeof(struct udp_hdr),
                    pkt,
                    OFFSET_UDP);
    pkt->nb_segs = 1;
    pkt->ol_flags = 0;
    pkt->vlan_tci = 0;
    pkt->vlan_tci_outer = 0;
    pkt->l2_len = sizeof(struct ether_hdr);
    pkt->l3_len = sizeof(struct ipv4_hdr);
}

static inline void dpdk_put_tsc(struct rte_mbuf *pkt, tsc_t tsc_value)
{
    tsc_t tsc_be = htobe64(tsc_value);
    copy_buf_to_pkt(&tsc_be,
                    sizeof(tsc_be),
                    pkt,
                    OFFSET_PAYLOAD);
}

static inline tsc_t dpdk_get_tsc(struct rte_mbuf *pkt)
{
    tsc_t tsc_be;
    copy_pkt_to_buf(&tsc_be,
                    sizeof(tsc_be),
                    pkt,
                    OFFSET_PAYLOAD);
    return be64toh(tsc_be);
}

// Swap the memory areas pointed by p1 and p2, using the area pointed by tmp as
// support memory.
// All the pointers must point to an area of at least size bytes.
static inline void swap_ptrs(void *p1, void *p2, void *tmp, size_t size)
{
    memcpy(tmp, p1, size);
    memcpy(p1, p2, size);
    memcpy(p2, tmp, size);
}

static inline struct ether_hdr *dpdk_get_ether_hdr(struct rte_mbuf *pkt)
{
    return rte_pktmbuf_mtod_offset(pkt, struct ether_hdr *, OFFSET_ETHER);
}

static inline struct ipv4_hdr *dpdk_get_ipv4_hdr(struct rte_mbuf *pkt)
{
    return rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, OFFSET_IPV4);
}

static inline struct udp_hdr *dpdk_get_udp_hdr(struct rte_mbuf *pkt)
{
    return rte_pktmbuf_mtod_offset(pkt, struct udp_hdr *, OFFSET_UDP);
}

static inline byte_t *dpdk_get_payload(struct rte_mbuf *pkt)
{
    return rte_pktmbuf_mtod_offset(pkt, byte_t *, OFFSET_PAYLOAD);
}

static inline byte_t *dpdk_get_payload_offset(struct rte_mbuf *pkt, ssize_t offset)
{
    return dpdk_get_payload(pkt) + offset;
}

// Swap source with destination mac addresses
static inline void dpdk_swap_ether_addr(struct rte_mbuf *pkt)
{
    struct ether_hdr *ether_hdr_ptr = dpdk_get_ether_hdr(pkt);
    struct ether_addr *src_mac_ptr = &ether_hdr_ptr->s_addr;
    struct ether_addr *dst_mac_ptr = &ether_hdr_ptr->d_addr;
    struct ether_addr support;

    swap_ptrs(src_mac_ptr, dst_mac_ptr, &support, sizeof(struct ether_addr));
}

// Swap source with destination ip addresses
static inline void dpdk_swap_ipv4_addr(struct rte_mbuf *pkt)
{
    struct ipv4_hdr *ipv4_hdr_ptr = dpdk_get_ipv4_hdr(pkt);
    uint32_t *src_ip_ptr = &ipv4_hdr_ptr->src_addr;
    uint32_t *dst_ip_ptr = &ipv4_hdr_ptr->dst_addr;
    uint32_t support;

    swap_ptrs(src_ip_ptr, dst_ip_ptr, &support, sizeof(uint32_t));
}

static inline void dpdk_swap_udp_port(struct rte_mbuf *pkt)
{
    struct udp_hdr *udp_hdr_ptr = dpdk_get_udp_hdr(pkt);
    uint16_t *src_port = &udp_hdr_ptr->src_port;
    uint16_t *dst_port = &udp_hdr_ptr->dst_port;
    uint16_t support;

    swap_ptrs(src_port, dst_port, &support, sizeof(uint16_t));
}

// TODO: ORGANIZE THIS MESS!
static inline void swap_ether_addr(struct ether_hdr *ether_hdr_ptr)
{
    struct ether_addr *src_mac_ptr = &ether_hdr_ptr->s_addr;
    struct ether_addr *dst_mac_ptr = &ether_hdr_ptr->d_addr;
    struct ether_addr support;

    swap_ptrs(src_mac_ptr, dst_mac_ptr, &support, sizeof(struct ether_addr));
}

static inline void swap_ipv4_addr(struct ipv4_hdr *ipv4_hdr_ptr)
{
    uint32_t *src_ip_ptr = &ipv4_hdr_ptr->src_addr;
    uint32_t *dst_ip_ptr = &ipv4_hdr_ptr->dst_addr;
    uint32_t support;

    swap_ptrs(src_ip_ptr, dst_ip_ptr, &support, sizeof(uint32_t));
}

static inline void swap_udp_port(struct udp_hdr *udp_hdr_ptr)
{
    uint16_t *src_port = &udp_hdr_ptr->src_port;
    uint16_t *dst_port = &udp_hdr_ptr->dst_port;
    uint16_t support;

    swap_ptrs(src_port, dst_port, &support, sizeof(uint16_t));
}

static inline void dpdk_produce_data(struct rte_mbuf *pkt, ssize_t offset, ssize_t payload_size)
{
    produce_data(payload_size, dpdk_get_payload_offset(pkt, offset));
}

#endif // DPDK_MINE_H
