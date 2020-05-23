#ifndef NFV_SOCKET_SIMPLE_H
#define NFV_SOCKET_SIMPLE_H

#include "nfv_socket.h"

/* --------------------------- METHOD  PROTOTYPES --------------------------- */

// typedef struct nfv_socket_simple *nfv_socket_simple_t;

/* ---------------------------- TYPE DEFINITIONS ---------------------------- */

struct nfv_socket_simple {
    /* Base class holder */
    struct nfv_socket super;

    int sock_fd;

    bool is_raw;
    bool use_mmsg;

    /* The actual size of buffers that need to be allocated */
    size_t used_size;
    /* The starting point for packet payload, zero for UDP sockets */
    size_t base_offset;

    /* Header structures, used by raw sockets only */
    struct rte_ether_hdr pkt_eth_hdr;
    struct rte_ipv4_hdr pkt_ip_hdr;
    struct rte_udp_hdr pkt_udp_hdr;

    /* Data structure used to hold the frame header, used by raw sockets only */
    byte_t frame_hdr[PKT_HEADER_SIZE];

    /* Holds pointers to all allocated packets */
    buffer_t *packets;

    /* These two are used for sendmmsg/recvmmsg API (both normal and raw
     * sockets) */
    struct iovec **iov;
    struct mmsghdr *datagrams;
    
    // Non connected sockets variables (useful for server application only)
    struct sockaddr_in *src_addresses;
    // socklen_t addrlen;
};

/* --------------------------- METHOD  PROTOTYPES --------------------------- */

#define NFV_SOCKET_SIMPLE_SIGNATURE(return_t, name, ...) \
    NFV_METHOD_SIGNATURE(return_t, socket_##name, ##__VA_ARGS__)

extern NFV_SOCKET_SIMPLE_SIGNATURE(void, init, nfv_conf_t conf);

extern NFV_SOCKET_SIMPLE_SIGNATURE(void, request_out_buffers, buffer_t *buffers, size_t size, size_t howmany);

extern NFV_SOCKET_SIMPLE_SIGNATURE(ssize_t, send);

extern NFV_SOCKET_SIMPLE_SIGNATURE(ssize_t, recv, buffer_t *buffers, size_t size, size_t howmany);

extern NFV_SOCKET_SIMPLE_SIGNATURE(ssize_t, send_back);

extern NFV_SOCKET_SIMPLE_SIGNATURE(void, free_buffers);

#endif /* NFV_SOCKET_SIMPLE_H */
