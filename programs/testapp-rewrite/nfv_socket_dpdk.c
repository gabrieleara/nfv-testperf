#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "nfv_socket_dpdk.h"

#include <sys/socket.h>

#define UNUSED(x) ((void)x)

NFV_SOCKET_DPDK_SIGNATURE(void, init, config_ptr conf)
{
    // TODO: must know the type of socket to be initialized
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);

    // Initialize constant variables
    sself->dpdk_port = conf->sock_fd;

    // Initialize addrlen sself->addrlen = sizeof(struct sockaddr_in);

    /* ------------------------- Memory allocation -------------------------- */

    // I need to allocate `burst_size` pointers to DPDK buffers
    pkts_burst = malloc(sizeof(struct rte_mbuf *) * sself->super.burst_size);

    /* --------------------- Structures initialization ---------------------- */

    // Setup packet headers
    dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, conf);

    /* ---------------------------------------------------------------------- */

    // FIXME: pointers in super-class must be changed in request_out_buffers
}

// FIXME: if I give less than burst_size here, I should remember it because
// otherwise the send would send too many packets
NFV_SOCKET_DPDK_SIGNATURE(void, request_out_buffers, buffer_t *buffers, size_t size, size_t howmany)
{
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);

    struct rte_mbuf *pkt;

    // ASSUMES howmany <= burst_size AND THAT PREVIOUS BUFFERS HAVE BEEN RELEASED!
    for (sself->pkts_prepared = 0; sself->pkts_prepared < howmany; sself->pkts_prepared++)
    {
        pkt = rte_mbuf_raw_alloc(sself->mbufs);

        if (unlikely(pkt == NULL))
        {
            fprintf(
                stderr,
                "WARN: Could not allocate a buffer, using less packets than required burst size!\n");
            return sself->pkts_prepared;
        }

        // TODO: should prepare in advance the header of each packet
        dpdk_pkt_prepare(pkt, conf, &sself->pkt_eth_hdr, &sself->pkt_ip_hdr, &sself->pkt_udp_hdr);

        buffers[sself->pkts_prepared] = pkt + SOME_DPDK_OFFSET_FOR_PAYLOAD;
    }
}

NFV_SOCKET_DPDK_SIGNATURE(ssize_t, send)
{
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);

    int num_sent = 0;

    num_sent = rte_eth_tx_burst(sself->dpdk_port, 0, sself->pkts_burst, sself->pkts_prepared);

    // TODO: DO NOT FREE BUFFERS HERE, FREE BUFFERS ONLY WHEN EXPLICITLY CALLED
    // TODO: INSTEAD, MOVE THE POINTERS TO THE BUFFERS BACKWARDS
    for (int pkt_idx = num_sent; pkt_idx < sself->pkts_prepared; ++pkt_idx)
    {
        rte_pktmbuf_free(sself->pkts_burst[pkt_idx]);
    }

    return num_sent;
}

//FIXME:
// THIS IS USED TO INFORM LEARNING SWITCHES WHERE THE MAC ADDRESS OF
// THIS APPLICATION RESIDES (A SORT OF ARP ADVERTISEMENT)
// if (adv_counter >= advertisement_period) {
//     dpdk_advertise_host_mac(conf);
//     adv_counter = 0;
// }

// BUG: for the server there is no way to check who sent a message and construct
// the appropriate headers as of now. ALSO BUG: for raw sockets should check
// that the destination address is this address
NFV_SOCKET_DPDK_SIGNATURE(ssize_t, recv, buffer_t *buffers, size_t size, size_t howmany)
{
    size_t pkts_actually_rx;
    size_t pkts_rx;
    size_t i;

    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);

    int num_recv = 0;

    pkts_rx = rte_eth_rx_burst(sself->dpdk_port, 0, sself->pkts_burst, howmany); // ASSUMES howmany <= burst_size
    pkts_actually_rx = 0;

    // Filter-out packets NOT meant for this application
    for (i = 0; i < pkts_rx; ++i)
    {
        if (dpdk_check_dst_addr_port(sself->pkts_burst[i], conf)) // TODO
        {
            // Packet was meant for this application!
            sself->pkts_burst[pkts_actually_rx] = sself->pkts_burst[i];
            ++pkts_actually_rx;
        }
        else
        {
            // Free this buffer and do not increase the counter
            rte_pktmbuf_free(sself->pkts_burst[i]);
        }
    }

    return pkts_actually_rx;
}

// CHECK: should this be simply equal to send with some arrangements of the
// headers? CHECK: I think for sendmmsg it could be as easy as calling simply
// sendmsg/sendmmsg, because the headers are updated by recvmsg/recvmmsg calls
NFV_SOCKET_DPDK_SIGNATURE(ssize_t, send_back)
{
    struct nfv_socket_dpdk *sself = (struct nfv_socket_dpdk *)(self);
    struct rte_ipv4_hdr *pkt_ip_hdr;

    for (size_t i = 0; i < howmany; ++i)
    {
        // Swap sender and receiver addresses/ports
        dpdk_swap_ether_addr(sself->pkts_burst[i]);
        dpdk_swap_ipv4_addr(sself->pkts_burst[i]);
        dpdk_swap_udp_port(sself->pkts_burst[i]);

        pkt_ip_hdr = dpdk_pkt_offset(sself->pkts_burst[i], struct rte_ipv4_hdr *, OFFSET_PKT_IPV4);
        pkt_ip_hdr->hdr_checksum = ipv4_hdr_checksum(pkt_ip_hdr);
    }

    sself->pkts_prepared = howmany;

    NFV_CALL(self, send);

    return 0;
}

NFV_SOCKET_DPDK_SIGNATURE(void, free_buffers)
{
    // TODO: FREE RECEIVED BUFFERS THAT HAVE NOT BEEN FREED BEFORE!
    UNUSED(self);
}
