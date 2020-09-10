#ifndef NFV_SOCKET_INTERF_HPP
#define NFV_SOCKET_INTERF_HPP

#include "constants.h"

#ifdef __cplucplus
extern "C"
{
#endif // __cplusplus

    using buffer_t = struct byte_t *;
    using config_ptr = struct config *;
    using nfv_socket_handle = struct nfv_socket *;

    nfv_socket_handle nfv_socket_factory(config_ptr conf);
    void nfv_socket_free(nfv_socket_handle);

    // Functions
    extern size_t nfv_socket_request_out_buffers(nfv_socket_handle handle, buffer_t buffers[], size_t howmany);
    extern ssize_t nfv_socket_send(nfv_socket_handle handle, buffer_t buffers[], size_t howmany);
    extern ssize_t nfv_socket_recv(nfv_socket_handle handle, buffer_t buffers[], size_t howmany);
    extern ssize_t nfv_socket_send_back(nfv_socket_handle handle, buffer_t buffers[], size_t howmany);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // NFV_SOCKET_INTERF_HPP
