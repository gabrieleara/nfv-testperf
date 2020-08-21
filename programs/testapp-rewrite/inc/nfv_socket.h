#ifndef NFV_SOCKET_H
#define NFV_SOCKET_H

/* -------------------------------- INCLUDES -------------------------------- */
#include <stdint.h>
#include <sys/types.h>

#include "constants.h"

/* ---------------------------- TYPE DEFINITIONS ---------------------------- */

typedef uint8_t byte_t;
typedef byte_t *buffer_t;

typedef struct nfv_socket *nfv_socket_ptr;
typedef struct config *config_ptr;

/* ----------------------- CLASS FUNCTION PROTOTYPES ------------------------ */

#define NFV_CALL(self, method, ...) self->method(self, ##__VA_ARGS__)

#define NFV_METHOD_SIGNATURE(return_t, name, ...) \
    return_t nfv_socket_##name(nfv_socket_ptr self, ##__VA_ARGS__)

#define NFV_METHOD_POINTER(return_t, name, ...) \
    return_t (*nfv_socket_##name##_t)(nfv_socket_ptr self, ##__VA_ARGS__)

#define NFV_METHOD_DECLARATION(return_t, name, ...)             \
    NFV_METHOD_SIGNATURE(return_t, name, ##__VA_ARGS__); \
    typedef NFV_METHOD_POINTER(return_t, name, ##__VA_ARGS__)

/**
 * Initializes all the buffers of the socket.
 *
 * The socket will have `burst_size` packet buffers able to hold at least `packet_size`
 * bytes of payload.
 */
// void nfv_socket_init(nfv_socket_ptr self, size_t packet_size, size_t burst_size);
extern NFV_METHOD_DECLARATION(void, init, config_ptr conf);

/**
 * Requests `burst_size` data buffers associated with the provided socket to be
 * used for sending purposes.
 *
 * On success, returns the number of buffers pointed by `buffers`, each with the
 * requested packet_size. On failure, 0 or a negative error is returned.
 */
// void nfv_request_out_buffers(nfv_socket_ptr self, buffer_t *buffers, size_t
// packet_size, size_t burst_size);
static inline NFV_METHOD_DECLARATION(void, request_out_buffers, buffer_t *buffers, size_t packet_size, size_t burst_size);

/**
 * Sends a burst of data.
 *
 * Assumes that the last requested buffers are all ready to be sent for
 * simplicity. After this call, all those buffers cannot be used anymore and
 * must be acquired once again.
 */
//void nfv_send(nfv_socket_ptr self);
static inline NFV_METHOD_DECLARATION(ssize_t, send);

/**
 * Checks (non-blocking-ly) is there is new data to be received and if so it
 * fills at most `burst_size` buffers with at most `packet_size` bytes, one buffer per
 * packet.
 *
 * NOTICE: after a recv, the returned buffers can also be used in
 * `nfv_send_back` to send back a reply to the original host.
 */
// void nfv_recv(nfv_socket_ptr self, byte_t **buffers, size_t packet_size, size_t
// burst_size);
static inline NFV_METHOD_DECLARATION(ssize_t, recv, buffer_t *buffers, size_t packet_size, size_t burst_size);

/**
 * Sends back a burst of data after a `nfv_recv`.
 *
 * Assumes that the last requested buffers are all ready to be sent for
 * simplicity. After this call, all those buffers cannot be used anymore and
 * must be acquired once again.
 */
//void nfv_send_back(nfv_socket_ptr self);
static inline NFV_METHOD_DECLARATION(ssize_t, send_back);

/**
 * Frees all the buffers that are currently borrowed by the application on the
 * given socket.
 */
//void nfv_free_buffers(nfv_socket_ptr self);
static inline NFV_METHOD_DECLARATION(void, free_buffers);

/* ---------------------------- CLASS DEFINITION ---------------------------- */

enum nfv_sock_type
{
    NFV_SOCK_NONE  = 0,     /* This is only for error-checking */
    NFV_SOCK_DGRAM = 0x1,
    NFV_SOCK_RAW   = 0x2,
    NFV_SOCK_DPDK  = 0x4,
};

#define NFV_SOCK_SIMPLE (NFV_SOCK_DGRAM | NFV_SOCK_RAW)

struct nfv_socket
{
    /* ------------------------------ Methods ------------------------------- */

    // nfv_socket_init_t init;
    nfv_socket_request_out_buffers_t request_out_buffers;
    nfv_socket_send_t send;
    nfv_socket_recv_t recv;
    nfv_socket_send_back_t send_back;
    nfv_socket_free_buffers_t free_buffers;

    /* --------------------------- "Private" data --------------------------- */

    /* const */ size_t packet_size;
    /* const */ size_t payload_size;
    /* const */ size_t burst_size;

    buffer_t *payloads;
};

/* ----------------------- CLASS FUNCTION DEFINITIONS ----------------------- */

static inline NFV_METHOD_SIGNATURE(void, request_out_buffers, buffer_t *buffers, size_t packet_size, size_t burst_size)
{
    NFV_CALL(self, request_out_buffers, buffers, packet_size, burst_size);
}

static inline NFV_METHOD_SIGNATURE(ssize_t, send)
{
    return NFV_CALL(self, send);
}

static inline NFV_METHOD_SIGNATURE(ssize_t, recv, buffer_t *buffers, size_t packet_size, size_t burst_size)
{
    return NFV_CALL(self, recv, buffers, packet_size, burst_size);
}

static inline NFV_METHOD_SIGNATURE(ssize_t, send_back)
{
    return NFV_CALL(self, send_back);
}

static inline NFV_METHOD_SIGNATURE(void, free_buffers)
{
    NFV_CALL(self, free_buffers);
}

/* ----------------------- CLASS FACTORY DECLARATIONS ----------------------- */

extern nfv_socket_ptr nfv_socket_factory_get(config_ptr conf);

#endif /* NFV_SOCKET_H */
