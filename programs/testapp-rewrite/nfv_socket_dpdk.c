
#include <rte_ethdev.h>

#include "config.h"
#include "constants.h"
#include "nfv_socket_dpdk.h"

#define dpdk_packet_start(p, t) rte_pktmbuf_mtod(p, t)
#define dpdk_packet_offset(p, t, o) rte_pktmbuf_mtod_offset(p, t, o)

/*
static inline struct rte_ether_hdr *dpdk_ether_hdr(struct rte_mbuf *pkt) {
    return dpdk_packet_offset(pkt, struct rte_ether_hdr *, OFFSET_PKT_ETHER);
}

static inline struct rte_ipv4_hdr *dpdk_ipv4_hdr(struct rte_mbuf *pkt) {
    return dpdk_packet_offset(pkt, struct rte_ipv4_hdr *, OFFSET_PKT_IPV4);
}

static inline struct rte_udp_hdr *dpdk_udp_hdr(struct rte_mbuf *pkt) {
    return dpdk_packet_offset(pkt, struct rte_udp_hdr *, OFFSET_PKT_UDP);
}
*/

static inline byte_t *dpdk_payload(struct rte_mbuf *pkt) {
    return dpdk_packet_offset(pkt, byte_t *, OFFSET_PKT_PAYLOAD);
}

/*
static inline void dpdk_swap_ether_addr(struct rte_mbuf *pkt) {
    struct rte_ether_hdr *ether_hdr = dpdk_ether_hdr(pkt);
    swap_ether_addr(ether_hdr);
}

static inline void dpdk_swap_ipv4_addr(struct rte_mbuf *pkt) {
    struct rte_ipv4_hdr *ipv4_hdr = dpdk_ipv4_hdr(pkt);
    swap_ipv4_addr(ipv4_hdr);
}

static inline void dpdk_swap_udp_port(struct rte_mbuf *pkt) {
    struct rte_udp_hdr *udp_hdr = dpdk_udp_hdr(pkt);
    swap_udp_port(udp_hdr);
}
*/

static inline NFV_DPDK_SIGNATURE(void, free_buffers) {
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);
    if (unlikely(sself->used_buffers < sself->active_buffers)) {
        for (; sself->used_buffers < sself->active_buffers;
             ++sself->used_buffers) {
            rte_pktmbuf_free(sself->packets[sself->used_buffers]);
        }
        sself->active_buffers = 0;
    }
    sself->used_buffers = 0;
}

static inline NFV_DPDK_SIGNATURE(void, fill_buffer_array, buffer_t buffers[],
                                 size_t howmany) {
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);
    for (size_t i = 0; i < howmany; ++i) {
        buffers[i] = dpdk_payload(sself->packets[i]);
    }
}

static inline void dpdk_prepare_packet(struct rte_mbuf *pkt, size_t size,
                                       struct rte_ether_hdr *eth_hdr,
                                       struct rte_ipv4_hdr *ip_hdr,
                                       struct rte_udp_hdr *udp_hdr) {
    rte_pktmbuf_reset_headroom(pkt);

    // Only one segment containing the packet of size size
    pkt->data_len = size;
    pkt->pkt_len = pkt->data_len;
    pkt->next = NULL;
    pkt->nb_segs = 1;
    pkt->ol_flags = 0;
    pkt->vlan_tci = 0;
    pkt->vlan_tci_outer = 0;
    pkt->l2_len = sizeof(struct rte_ether_hdr);
    pkt->l3_len = sizeof(struct rte_ipv4_hdr);

    byte_t *start = dpdk_packet_start(pkt, byte_t *);

    // Copy all headers in the packet
    rte_memcpy(start + OFFSET_PKT_ETHER, eth_hdr, sizeof(struct rte_ether_hdr));
    rte_memcpy(start + OFFSET_PKT_IPV4, ip_hdr, sizeof(struct rte_ipv4_hdr));
    rte_memcpy(start + OFFSET_PKT_UDP, udp_hdr, sizeof(struct rte_udp_hdr));
}

NFV_DPDK_SIGNATURE(void, init, config_ptr conf) {
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);

    sself->active_buffers = 0;
    sself->used_buffers = 0;

    sself->portid = conf->dpdk.portid;
    sself->mbufs = conf->dpdk.mbufs;

    sself->packets = malloc(sizeof(rte_buffer_t) * self->burst_size);

    // Setup packet headers
    pkt_hdr_setup(&sself->incoming_hdr, conf, DIR_INCOMING);
    pkt_hdr_setup(&sself->outgoing_hdr, conf, DIR_OUTGOING);
}

NFV_DPDK_SIGNATURE(size_t, request_out_buffers, buffer_t buffers[],
                   size_t howmany) {
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);

    if (unlikely(howmany > self->burst_size))
        howmany = self->burst_size;

    nfv_socket_dpdk_free_buffers(self);

    size_t i;
    for (i = 0; i < howmany; ++i) {
        // FIXME: from DPDK documentation, "for standard use, prefer
        // rte_pktmbuf_alloc()"
        sself->packets[i] = rte_mbuf_raw_alloc(sself->mbufs);
        if (unlikely(sself->packets[i] == NULL)) {
            fprintf(stderr,
                    "WARN: Could not allocate buffer, using %lu instead of the "
                    "%lu requested buffers!\n",
                    i, howmany);
            break;
        }

        dpdk_prepare_packet(sself->packets[i], self->packet_size,
                            &sself->outgoing_hdr.ether, &sself->outgoing_hdr.ip,
                            &sself->outgoing_hdr.udp);
    }

    sself->active_buffers += i;
    nfv_socket_dpdk_fill_buffer_array(self, buffers, howmany);

    return sself->active_buffers;
}

NFV_DPDK_SIGNATURE(ssize_t, send, size_t howmany) {
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);

    size_t num_sent;

    if (unlikely(howmany > sself->active_buffers - sself->used_buffers))
        howmany = sself->active_buffers - sself->used_buffers;

    if (unlikely(howmany == 0))
        return 0;

    // TODO: rte_eth_dev_configure with queues?
    num_sent = rte_eth_tx_burst(sself->portid, 0,
                                sself->packets + sself->used_buffers, howmany);

    if (likely(num_sent > 0))
        sself->used_buffers += num_sent;

    return num_sent;
}

// FIXME: put this somewhere maybe?
// THIS IS USED TO INFORM LEARNING SWITCHES WHERE THE MAC ADDRESS OF
// THIS APPLICATION RESIDES (A SORT OF ARP ADVERTISEMENT)
// if (adv_counter >= advertisement_period) {
//     dpdk_advertise_host_mac(conf);
//     adv_counter = 0;
// }

NFV_DPDK_SIGNATURE(ssize_t, recv, buffer_t buffers[], size_t howmany) {
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);
    size_t num_recv;
    size_t num_recv_good;
    size_t i;

    if (unlikely(howmany > self->burst_size))
        howmany = self->burst_size;

    nfv_socket_dpdk_free_buffers(self);

    num_recv = rte_eth_rx_burst(sself->portid, 0, sself->packets, howmany);
    num_recv_good = 0;

    // I put a "likely" here to prefer scenarios in which there is actually
    // something to do with the incoming packets.
    if (likely(num_recv > 0)) {
        // Filter-out packets NOT meant for this application
        for (i = 0; i < num_recv; ++i) {
            // TODO: implement this without conf
            const struct pkt_hdr *header =
                dpdk_packet_start(sself->packets[i], struct pkt_hdr *);

            if (hdr_check_incoming(header, &sself->incoming_hdr)) {
                // Packet was meant for this application!
                sself->packets[num_recv_good] = sself->packets[i];
                ++num_recv_good;
            } else {
                // Free this buffer, do not increase num_recv_good
                rte_pktmbuf_free(sself->packets[i]);
            }
        }

        sself->active_buffers += num_recv_good;
    }

    nfv_socket_dpdk_fill_buffer_array(self, buffers, howmany);

    return num_recv_good;
}

// CHECK: should this be simply equal to send with some arrangements of the
// headers? CHECK: I think for sendmmsg it could be as easy as calling
// simply sendmsg/sendmmsg, because the headers are updated by
// recvmsg/recvmmsg calls
NFV_DPDK_SIGNATURE(ssize_t, send_back, size_t howmany) {
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);

    if (unlikely(howmany > sself->active_buffers - sself->used_buffers))
        howmany = sself->active_buffers - sself->used_buffers;

    if (unlikely(howmany == 0))
        return 0;

    struct rte_ipv4_hdr *ip_hdr;

    for (size_t i = 0; i < howmany; ++i) {
        size_t j = i + sself->used_buffers;

        byte_t *packet_start = dpdk_packet_start(sself->packets[j], byte_t *);

        swap_ether_addr((struct rte_ether_hdr *)packet_start +
                        OFFSET_PKT_ETHER);
        swap_ipv4_addr((struct rte_ipv4_hdr *)packet_start + OFFSET_PKT_IPV4);
        swap_udp_port((struct rte_udp_hdr *)packet_start + OFFSET_PKT_UDP);

        // Calculate ip_hdr new checksum
        ip_hdr = (struct rte_ipv4_hdr *)(packet_start + OFFSET_PKT_IPV4);
        ip_hdr->hdr_checksum = 0;
        ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
    }

    return nfv_socket_dpdk_send(self, howmany);
}
