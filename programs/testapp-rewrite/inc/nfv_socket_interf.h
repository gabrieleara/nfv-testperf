#ifndef NFV_SOCKET_INTERF_HPP
#define NFV_SOCKET_INTERF_HPP

// #include "constants.h"

//#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t byte_t;
typedef byte_t *buffer_t;
typedef struct config *config_ptr;
typedef struct nfv_socket *nfv_socket_handle;

extern nfv_socket_handle nfv_socket_factory_get(const char c_str[],
                                                config_ptr conf);
extern void nfv_socket_free(nfv_socket_handle);

// Functions
extern size_t nfv_socket_request_out_buffers(nfv_socket_handle handle,
                                             buffer_t buffers[],
                                             size_t howmany);
extern ssize_t nfv_socket_send(nfv_socket_handle handle, size_t howmany);
extern ssize_t nfv_socket_recv(nfv_socket_handle handle, buffer_t buffers[],
                               size_t howmany);
extern ssize_t nfv_socket_send_back(nfv_socket_handle handle, size_t howmany);

#ifdef __cplusplus
}
#endif

#endif // NFV_SOCKET_INTERF_HPP
