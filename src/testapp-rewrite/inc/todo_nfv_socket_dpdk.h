#ifndef NFV_SOCKET_DPDK_H
#define NFV_SOCKET_DPDK_H

#include "nfv_socket.h"

/* --------------------------- METHOD  PROTOTYPES --------------------------- */


/* ---------------------------- TYPE DEFINITIONS ---------------------------- */

struct nfv_socket_dpdk {
    /* Base class holder */
    struct nfv_socket super;

    /* Header structures, typically the same for each message */
    struct rte_ether_hdr pkt_eth_hdr;
    struct rte_ipv4_hdr pkt_ip_hdr;
    struct rte_udp_hdr pkt_udp_hdr;

    // Set of messages to be send/received
    struct rte_mbuf **pkts_burst; // Sized to the burst_size
    // struct rte_mbuf *pkt; // Current packet pointer

    // MISSING ARGUMENTS:
    // port
    // mbufs

    // Number of packets prepared and sent in current iteration
    size_t pkts_prepared; // This should be a parameter to send
    // size_t pkts_tx; // This is the result of the send
};

/* --------------------------- METHOD  PROTOTYPES --------------------------- */

#define NFV_SOCKET_DPDK_SIGNATURE(return_t, name, ...) \
    NFV_METHOD_SIGNATURE(return_t, dpdk_##name, ##__VA_ARGS__)

extern NFV_SOCKET_DPDK_SIGNATURE(void, init, config_ptr conf);

extern NFV_SOCKET_DPDK_SIGNATURE(size_t, request_out_buffers, byte_ptr_t *buffers, size_t howmany);

extern NFV_SOCKET_DPDK_SIGNATURE(ssize_t, send);

extern NFV_SOCKET_DPDK_SIGNATURE(ssize_t, recv, byte_ptr_t *buffers, size_t size, size_t howmany);

extern NFV_SOCKET_DPDK_SIGNATURE(ssize_t, send_back);

extern NFV_SOCKET_DPDK_SIGNATURE(void, free_buffers);

#endif /* NFV_SOCKET_DPDK_H */
