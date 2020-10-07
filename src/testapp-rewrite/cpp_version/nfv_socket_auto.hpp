#ifndef NFV_SOCKET_AUTO_HPP
#define NFV_SOCKET_AUTO_HPP

#include "autofactory.hpp"
#include <cstdint>

using byte_t = uint8_t;
using byte_ptr_t = byte_t *;
using buffer_t = byte_ptr_t;

using nfv_socket_ptr = struct nfv_socket *;
using config_ptr = struct config *;

class nfv_socket : public autofactory<nfv_socket, config *> {
public:
    // TODO: copy, move and all constructors/destructors
    nfv_socket(size_t packet_size, size_t payload_size, size_t burst_size)
        : packet_size(packet_size), payload_size(payload_size),
          burst_size(burst_size), payloads(burst_size, 0){};

    virtual ~nfv_socket() = default;

    virtual size_t request_out_buffers(buffer_t buffers[], size_t howmany) = 0;
    virtual ssize_t send(size_t howmany) = 0;
    virtual ssize_t recv(buffer_t buffers[], size_t howmany) = 0;
    virtual ssize_t send_back(size_t howmany) = 0;

protected:
    const size_t packet_size;
    const size_t payload_size;
    const size_t burst_size;

    std::vector<buffer_t> payloads;

    nfv_socket()
        : packet_size(0), payload_size(0), burst_size(0),
          payloads(burst_size, 0){};
};

#endif // NFV_SOCKET_AUTO_HPP
