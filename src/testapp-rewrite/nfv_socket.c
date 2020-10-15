#include "nfv_socket.h"
#include "config.h"
#include "constants.h"

#include "nfv_socket_dpdk.h"
#include "nfv_socket_simple.h"

/* ----------------------- CLASS FUNCTION DEFINITIONS ----------------------- */

// This is the only one that behaves differently, in the sense that it is
// defined like this and not as a wrapper of an init method
NFV_SIGNATURE(void, init, config_ptr conf)
{
    self->packet_size = conf->pkt_size;
    self->burst_size = conf->bst_size;
    self->payload_size = self->packet_size - PKT_HEADER_SIZE;

    self->payloads = malloc(sizeof(buffer_t) * self->burst_size);
}

/* ----------------------- CLASS FACTORY DECLARATIONS ----------------------- */

nfv_socket_ptr nfv_socket_factory_get(config_ptr conf) {
    struct nfv_socket base;
    struct nfv_socket_simple *socket_simple;
    struct nfv_socket_dpdk *socket_dpdk;

    // Initialize base attributes
    nfv_socket_init(&base, conf);

    switch (conf->sock_type) {
    case NFV_SOCK_DGRAM:
    case NFV_SOCK_RAW:
        // case NFV_SOCK_SIMPLE:
        socket_simple = malloc(sizeof(struct nfv_socket_simple));

#ifdef USE_FPTRS
        // Update methods and set base class pointer
        base.request_out_buffers = nfv_socket_simple_request_out_buffers;
        base.send = nfv_socket_simple_send;
        base.recv = nfv_socket_simple_recv;
        base.send_back = nfv_socket_simple_send_back;
#else
        base.classcode = NFV_SOCK_SIMPLE;
#endif
        socket_simple->super = base;
        nfv_socket_simple_init((nfv_socket_ptr)socket_simple, conf);
        return (nfv_socket_ptr)socket_simple;

    case NFV_SOCK_DPDK:
        socket_dpdk = malloc(sizeof(struct nfv_socket_dpdk));

#ifdef USE_FPTRS
        // Initialize methods of subclass
        base.request_out_buffers = nfv_socket_dpdk_request_out_buffers;
        base.send = nfv_socket_dpdk_send;
        base.recv = nfv_socket_dpdk_recv;
        base.send_back = nfv_socket_dpdk_send_back;
#else
        base.classcode = NFV_SOCK_DPDK;
#endif

        socket_dpdk->super = base;
        nfv_socket_dpdk_init((nfv_socket_ptr)socket_dpdk, conf);
        return (nfv_socket_ptr)socket_dpdk;
    default:
        assert(false);
        return NULL;
    }
}

#ifndef USE_FPTRS

#define NFV_CALL_RETURN(self, method, ...)                                     \
    if ((self->classcode & NFV_SOCK_SIMPLE) != 0)                              \
        return nfv_socket_simple_##method(self, ##__VA_ARGS__);                \
    else if ((self->classcode & NFV_SOCK_DPDK) != 0)                           \
        return nfv_socket_dpdk_##method(self, ##__VA_ARGS__);                  \
    return 0;

NFV_SIGNATURE(size_t, request_out_buffers, buffer_t buffers[], size_t howmany) {
    NFV_CALL_RETURN(self, request_out_buffers, buffers, howmany);
}

NFV_SIGNATURE(ssize_t, send, size_t howmany) {
    NFV_CALL_RETURN(self, send, howmany);
}

NFV_SIGNATURE(ssize_t, recv, buffer_t buffers[], size_t howmany) {
    NFV_CALL_RETURN(self, recv, buffers, howmany);
}
NFV_SIGNATURE(ssize_t, send_back, size_t howmany) {
    NFV_CALL_RETURN(self, send_back, howmany);
}
#endif
