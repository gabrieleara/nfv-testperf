#include "config.h"
#include "nfv_socket.h"
#include "nfv_socket_simple.h"
#include "nfv_socket_dpdk.h"

/* ----------------------- CLASS FUNCTION DEFINITIONS ----------------------- */

// This is the only one that behaves differently, in the sense that it is
// defined like this and not as a wrapper of an init method
NFV_METHOD_SIGNATURE(void, init, config_ptr conf)
{
    self->packet_size = conf->pkt_size;
    self->burst_size = conf->bst_size;
    self->payload_size = self->packet_size - PKT_HEADER_SIZE;

    self->payloads = malloc(sizeof(buffer_t *) * self->burst_size);
}

/* ----------------------- CLASS FACTORY DECLARATIONS ----------------------- */

nfv_socket_ptr nfv_socket_factory_get(config_ptr conf)
{
    struct nfv_socket base;
    struct nfv_socket_simple *socket_simple;
    // struct nfv_socket_dpdk *socket_dpdk;

    // Initialize methods of base class
    base.request_out_buffers = nfv_socket_request_out_buffers;
    base.send = nfv_socket_send;
    base.recv = nfv_socket_recv;
    base.send_back = nfv_socket_send_back;
    base.free_buffers = nfv_socket_free_buffers;

    // Initialize base attributes
    nfv_socket_init(&base, conf);

    switch (conf->sock_type)
    {
    case NFV_SOCK_DGRAM:
    case NFV_SOCK_RAW:
        socket_simple = malloc(sizeof(struct nfv_socket_simple));

        socket_simple->super = base;

        // Initialize methods of subclass
        socket_simple->super.request_out_buffers = nfv_socket_simple_request_out_buffers;
        socket_simple->super.send = nfv_socket_simple_send;
        socket_simple->super.recv = nfv_socket_simple_recv;
        socket_simple->super.send_back = nfv_socket_simple_send_back;
        socket_simple->super.free_buffers = nfv_socket_simple_free_buffers;

        nfv_socket_simple_init((nfv_socket_ptr) socket_simple, conf);

        return (nfv_socket_ptr)socket_simple;
    case NFV_SOCK_DPDK:
        // socket_dpdk = malloc(sizeof(struct nfv_socket_dpdk));

        // socket_dpdk->super = base;

        // // Initialize methods of subclass
        // socket_dpdk->super.request_out_buffers = nfv_socket_dpdk_request_out_buffers;
        // socket_dpdk->super.send = nfv_socket_dpdk_send;
        // socket_dpdk->super.recv = nfv_socket_dpdk_recv;
        // socket_dpdk->super.send_back = nfv_socket_dpdk_send_back;
        // socket_dpdk->super.free_buffers = nfv_socket_dpdk_free_buffers;

        // nfv_socket_simple_init((nfv_socket_ptr) socket_dpdk, conf);

        // return (nfv_socket_ptr)socket_dpdk;
    default:
        assert(false);
        return NULL;
    }
}
