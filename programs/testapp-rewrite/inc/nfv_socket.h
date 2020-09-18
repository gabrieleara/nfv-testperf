#ifndef NFV_SOCKET_H
#define NFV_SOCKET_H

/* -------------------------------- INCLUDES -------------------------------- */

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------- TYPE DEFINITIONS ---------------------------- */

typedef uint8_t byte_t;
typedef byte_t *byte_ptr_t;
typedef byte_ptr_t buffer_t;
typedef struct nfv_socket *nfv_socket_ptr;
typedef struct config *config_ptr;

/* -------------------- MACROS (FOR FUNCTION PROTOTYPES) -------------------- */

/**
 * Defines a new macro that declares a new method prototype.
 * */
#define NFV_SIGNATURE(return_t, name, ...)                                     \
    return_t nfv_socket_##name(nfv_socket_ptr self, ##__VA_ARGS__)

/**
 * Defines a new pointer-to-method type based on the given prototype.
 * */
#define NFV_METHOD_POINTER(return_t, name, ...)                                \
    return_t (*nfv_socket_##name##_t)(nfv_socket_ptr self, ##__VA_ARGS__)

/**
 * Performs a declaration of both a method prototype and its pointer type.
 * */
#define NFV_METHOD(return_t, name, ...)                                        \
    NFV_SIGNATURE(return_t, name, ##__VA_ARGS__);                              \
    typedef NFV_METHOD_POINTER(return_t, name, ##__VA_ARGS__)

/* --------------------------------- MACROS --------------------------------- */

/**
 * Performs a method call on the given object.
 * */
#define NFV_CALL(self, method, ...) (self->method(self, ##__VA_ARGS__))

/* ----------------------- CLASS FUNCTION PROTOTYPES ------------------------ */

// FIXME: remove this from the "public" part of this header
/**
 * Initializes a nfv_socket data structure (or one of its "subclasses"), from
 * the given configuration structure.
 * */
extern NFV_METHOD(void, init, config_ptr conf);

/**
 * Requests a certain number of buffers from the nfv_socket.
 *
 * This operation shall be performed only when new packets should be sent.
 * Receiving packets will be filled automatically with new buffers. This also
 * frees all previously allocated buffers, if any.
 *
 * \returns the number of buffers that have been allocated, 0 or a negative
 * number on error.
 * */
static inline NFV_METHOD(size_t, request_out_buffers, buffer_t buffers[],
                         size_t howmany);

/**
 * Sends a burst of packets. Accepts the number of packets to be sent. Packets
 * position is automatically detected from the previous request_out_buffers
 * call.
 *
 * After this call, a certain number of buffers cannot be used anymore. This
 * call can be repeated multiple times in case of failure, until all packets are
 * sent. Otherwise, a call to request_out_buffers will re-initialize packets
 * data structures correctly.
 *
 * \returns the number of packets sent correctly.
 */
static inline NFV_METHOD(ssize_t, send, size_t howmany);

/**
 * Attempts a new receive operation. This call will re-initialize all previously
 * acquired buffers, similarly to calling request_out_buffers.
 *
 * After this call, buffers will contain the desired payload data.
 *
 * This call will also check whether the received packets were meant for the
 * current application when raw sockets or other low-level frameworks are used.
 *
 * After this call, use send_back to send back the packets to the original
 * sender.
 *
 * NOTICE: some sockets may act as connected sockets and accept to receive
 * packets coming from a single source only. Check on each concrete socket
 * documentation for more details.
 *
 * \returns the number of packets received correctly, 0 or a negative number on
 * error.
 */
static inline NFV_METHOD(ssize_t, recv, buffer_t buffers[], size_t howmany);

/**
 * Sends back a burst of data received from a previous recv call.
 *
 * This call, similarly to send, can be performed multiple times until all
 * packets are signaled as correctly sent, or you can call recv once again to
 * re-initialize data structures.
 *
 * \returns the number of packets correctly sent back.
 */
static inline NFV_METHOD(ssize_t, send_back, size_t howmany);

/* ---------------------------- CLASS DEFINITION ---------------------------- */

struct nfv_socket {
    /* ------------------------------ Methods ------------------------------- */
    nfv_socket_request_out_buffers_t request_out_buffers;
    nfv_socket_send_t send;
    nfv_socket_recv_t recv;
    nfv_socket_send_back_t send_back;

    /* --------------------------- "Private" data --------------------------- */

    /* const */ size_t packet_size;
    /* const */ size_t payload_size;
    /* const */ size_t burst_size;

    buffer_t *payloads;
};

/* ----------------------- CLASS FUNCTION DEFINITIONS ----------------------- */

static inline NFV_SIGNATURE(size_t, request_out_buffers, buffer_t buffers[],
                            size_t howmany) {
    return NFV_CALL(self, request_out_buffers, buffers, howmany);
}

static inline NFV_SIGNATURE(ssize_t, send, size_t howmany) {
    return NFV_CALL(self, send, howmany);
}

static inline NFV_SIGNATURE(ssize_t, recv, buffer_t buffers[], size_t howmany) {
    return NFV_CALL(self, recv, buffers, howmany);
}

static inline NFV_SIGNATURE(ssize_t, send_back, size_t howmany) {
    return NFV_CALL(self, send_back, howmany);
}

/* ----------------------- CLASS FACTORY DECLARATIONS ----------------------- */

extern nfv_socket_ptr nfv_socket_factory_get(config_ptr conf);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NFV_SOCKET_H */
