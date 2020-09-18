#ifndef NFV_SOCKET_DPDK_H
#define NFV_SOCKET_DPDK_H

#include "hdr_tools.h"
#include "nfv_socket.h"

#include <rte_mbuf.h>

/* ---------------------------- TYPE DEFINITIONS ---------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rte_mbuf *rte_buffer_t;

/* -------------------- MACROS (FOR FUNCTION PROTOTYPES) -------------------- */

#define NFV_DPDK_SIGNATURE(return_t, name, ...)                                \
    NFV_SIGNATURE(return_t, dpdk_##name, ##__VA_ARGS__)

/* ----------------------- CLASS FUNCTION PROTOTYPES
   ------------------------ */

extern NFV_DPDK_SIGNATURE(void, init, config_ptr conf);

extern NFV_DPDK_SIGNATURE(size_t, request_out_buffers, buffer_t buffers[],
                          size_t howmany);

extern NFV_DPDK_SIGNATURE(ssize_t, send, size_t howmany);

extern NFV_DPDK_SIGNATURE(ssize_t, recv, buffer_t buffers[], size_t howmany);

extern NFV_DPDK_SIGNATURE(ssize_t, send_back, size_t howmany);

/* ---------------------------- CLASS DEFINITION ---------------------------- */

struct nfv_socket_dpdk {
    /* Base class holder */
    struct nfv_socket super;

    /* Header structures, typically the same for each message */
    struct pkt_hdr outgoing_hdr;
    struct pkt_hdr incoming_hdr;

    // Set of messages to be send/received
    rte_buffer_t *packets;

    int portid;
    struct rte_mempool *mbufs;

    size_t active_buffers; // TODO: = 0;
    size_t used_buffers;   // TODO: = 0;
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NFV_SOCKET_DPDK_H */
