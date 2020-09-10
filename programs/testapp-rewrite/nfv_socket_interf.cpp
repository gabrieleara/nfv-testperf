#include "nfv_socket_interf.h"
#include "nfv_socket.hpp"

extern "C"
{

    size_t nfv_socket_request_out_buffers(nfv_socket_handle handle, buffer_t buffers[], size_t howmany) {
        handle->request_out_buffers(buffers, howmany);
    }

    ssize_t nfv_socket_send(nfv_socket_handle handle, buffer_t buffers[], size_t howmany) {
        handle->send(buffers, howmany);
    }

    ssize_t nfv_socket_recv(nfv_socket_handle handle, buffer_t buffers[], size_t howmany) {
        handle->recv(buffers, howmany);
    }

    ssize_t nfv_socket_send_back(nfv_socket_handle handle, buffer_t buffers[], size_t howmany) {
        handle->send_back(buffers, howmany);
    }
}
