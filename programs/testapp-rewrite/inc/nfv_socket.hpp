#ifndef NFV_SOCKET_HPP
#define NFV_SOCKET_HPP

/* -------------------------------- INCLUDES -------------------------------- */
#include <stdint.h>
#include <sys/types.h>

#include "constants.h"

/* ---------------------------- TYPE DEFINITIONS ---------------------------- */

typedef uint8_t byte_t;
typedef byte_t *byte_ptr_t;
typedef byte_t *buffer_t;

typedef struct nfv_socket *nfv_socket_ptr;
typedef struct config *config_ptr;

/* ---------------------------- CLASS DEFINITION ---------------------------- */

enum nfv_sock_type
{
    NFV_SOCK_NONE = 0, /* This is only for error-checking */
    NFV_SOCK_DGRAM = 0x1,
    NFV_SOCK_RAW = 0x2,
    NFV_SOCK_DPDK = 0x4,
};

#define NFV_SOCK_SIMPLE (NFV_SOCK_DGRAM | NFV_SOCK_RAW)

class nfv_socket
{
protected:
    const size_t packet_size;
    const size_t payload_size;
    const size_t burst_size;

    byte_ptr_t *payloads;

public:
    nfv_socket(size_t packet_size, size_t payload_size, size_t burst_size)
        : packet_size(packet_size),
          payload_size(payload_size),
          burst_size(burst_size){}; // TODO: private?
    // TODO: copy, move and all constructors/destructors

    virtual size_t request_out_buffers(buffer_t buffers[], size_t howmany) = 0;
    virtual ssize_t send(buffer_t buffers[], size_t howmany) = 0;
    virtual ssize_t recv(buffer_t buffers[], size_t howmany) = 0;
    virtual ssize_t send_back(buffer_t buffers[], size_t howmany) = 0;
};

#endif // NFV_SOCKET_HPP
