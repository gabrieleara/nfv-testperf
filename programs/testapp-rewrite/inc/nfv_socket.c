#include "config.h"
#include "nfv_socket.h"

/* ----------------------- CLASS FUNCTION DEFINITIONS ----------------------- */

// This is the only one that behaves differently, in the sense that it is
// defined like this and not as a wrapper of an init method
NFV_METHOD_SIGNATURE(void, init, config_ptr conf)
{
    self->packet_size = conf->pkt_size;
    self->burst_size  = conf->bst_size;
    self->payload_size = self->packet_size - PKT_HEADER_SIZE;

    self->payloads = malloc(sizeof(buffer_t *) * self->burst_size);
}

NFV_METHOD_SIGNATURE(void, request_out_buffers, buffer_t *buffers, size_t packet_size, size_t burst_size)
{
    NFV_CALL(self, request_out_buffers, buffers, packet_size, burst_size);
}

NFV_METHOD_SIGNATURE(ssize_t, send)
{
    return NFV_CALL(self, send);
}

NFV_METHOD_SIGNATURE(ssize_t, recv, buffer_t *buffers, size_t packet_size, size_t burst_size)
{
    return NFV_CALL(self, recv, buffers, packet_size, burst_size);
}

NFV_METHOD_SIGNATURE(ssize_t, send_back)
{
    return NFV_CALL(self, send_back);
}

NFV_METHOD_SIGNATURE(void, free_buffers)
{
    NFV_CALL(self, free_buffers);
}
