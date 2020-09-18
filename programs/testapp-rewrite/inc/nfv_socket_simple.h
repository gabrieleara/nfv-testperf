#ifndef NFV_SOCKET_SIMPLE_H
#define NFV_SOCKET_SIMPLE_H

#include "hdr_tools.h"
#include "nfv_socket.h"

#include <stdbool.h>

/* ---------------------------- TYPE DEFINITIONS ---------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------- MACROS (FOR FUNCTION PROTOTYPES) -------------------- */

#define NFV_SIMPLE_SIGNATURE(return_t, name, ...)                              \
    NFV_SIGNATURE(return_t, simple_##name, ##__VA_ARGS__)

/* ----------------------- CLASS FUNCTION PROTOTYPES ------------------------ */

extern NFV_SIMPLE_SIGNATURE(void, init, config_ptr conf);

extern NFV_SIMPLE_SIGNATURE(size_t, request_out_buffers, buffer_t buffers[],
                            size_t howmany);

extern NFV_SIMPLE_SIGNATURE(ssize_t, send, size_t howmany);

extern NFV_SIMPLE_SIGNATURE(ssize_t, recv, buffer_t buffers[], size_t howmany);

extern NFV_SIMPLE_SIGNATURE(ssize_t, send_back, size_t howmany);

/* ---------------------------- CLASS DEFINITION ---------------------------- */

struct nfv_socket_simple {
    /* Base class holder */
    struct nfv_socket super;

    /* const */ int sock_fd;
    /* const */ bool is_raw;   // TODO: test when is_raw is false
    /* const */ bool use_mmsg; // TODO: test when use_mmsg is true

    /* The actual size of buffers that need to be allocated */
    /* const */ size_t used_size;

    /* The starting point for packet payload, zero for UDP sockets */
    /* const */ size_t base_offset;

    /* Header structures, used by raw sockets only */
    struct pkt_hdr outgoing_hdr;
    struct pkt_hdr incoming_hdr;

    /* Data structure used to hold the frame header, used by raw sockets only */
    byte_t frame_hdr[PKT_HEADER_SIZE];

    /* Holds all allocated packets */
    buffer_t *packets;

    /* Holds all sockaddr_in addresses
     * (for outgoing packets they all are the same structure) */
    struct sockaddr_in *corr_addresses;

    /* API uses always sendmsg/recvmsg or sendmmsg/recvmmsg API */
    struct iovec *iovecs;
    struct mmsghdr *datagrams;

    size_t active_buffers; // TODO: = 0;
    size_t used_buffers;   // TODO: = 0;
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NFV_SOCKET_SIMPLE_H */
