#include "nfv_socket_interf.h"
#include "nfv_socket_auto.hpp"

#ifdef __cplusplus
extern "C" {
#endif

nfv_socket_handle nfv_socket_factory_get(const char c_str[], config_ptr conf) {
    return nfv_socket::_create(std::string(c_str), conf);
}

size_t nfv_socket_request_out_buffers(nfv_socket_handle handle,
                                      buffer_t buffers[], size_t howmany) {
    return handle->request_out_buffers(buffers, howmany);
}

ssize_t nfv_socket_send(nfv_socket_handle handle, size_t howmany) {
    return handle->send(howmany);
}

ssize_t nfv_socket_recv(nfv_socket_handle handle, buffer_t buffers[],
                        size_t howmany) {
    return handle->recv(buffers, howmany);
}

ssize_t nfv_socket_send_back(nfv_socket_handle handle, size_t howmany) {
    return handle->send_back(howmany);
}

#ifdef __cplusplus
}
#endif
