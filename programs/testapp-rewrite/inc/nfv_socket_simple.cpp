#ifndef NFV_SOCKET_SIMPLE_HPP
#define NFV_SOCKET_SIMPLE_HPP

#include "nfv_socket.hpp"
#include "config.h"

#include <vector>
#include <sys/socket.h>

class nfv_socket_simple : public nfv_socket
{
public:
    nfv_socket_simple(config *conf)
        : nfv_socket(conf->pkt_size, conf->payload_size, conf->bst_size),
          sock_fd(conf->sock_fd),
          is_raw((conf->sock_type & NFV_SOCK_RAW) != 0),
          use_mmsg(conf->use_mmsg),
          used_size(is_raw ? packet_size : payload_size),
          base_offset(is_raw ? OFFSET_PKT_PAYLOAD : 0),
          packets(burst_size, std::vector<byte_t>(used_size, 0)),
          iovecs(burst_size, {0}),
          datagrams(burst_size, {0}),
          corr_addresses(burst_size, {0})
    {
        for (auto i = decltype(burst_size){0}; i < burst_size; ++i)
        {
            iovecs[i].iov_base = packets[i].data();
            iovecs[i].iov_len = 1; // No scatter-gather I/O, all packet is in one piece

            datagrams[i].msg_hdr.msg_iov = &iovecs[i];
            datagrams[i].msg_hdr.msg_iovlen = 1; // Only one vector element per message

            if (!is_raw)
            {
                // These two lines must be repeated for each recv loop FIXME: you sure?
                datagrams[i].msg_hdr.msg_name = &corr_addresses[i]; // FIXME: rename this data structure
                datagrams[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);

                // Set the address of the corresponding element
                corr_addresses[i] = conf->remote.ip;
            }
        }

        // For UDP sockets, the corresponding pair IP addr/UDP port is selected automatically using connected sockets.
        // For RAW sockets, we will build a common header to be used by all packets.

        // TODO: probably frame_hdr riesco a tenerlo all'interno del costruttore
        if (is_raw)
        {
            // For RAW sockets we need to build a frame header, the same for each
            // packet
            // FIXME: Implement packet headers initialization
            // dpdk_setup_pkt_headers(&sself->pkt_eth_hdr, &sself->pkt_ip_hdr, &sself->pkt_udp_hdr, conf);
            memcpy(frame_hdr + OFFSET_PKT_ETHER, &pkt_eth_hdr, sizeof(struct rte_ether_hdr));
            memcpy(frame_hdr + OFFSET_PKT_IPV4, &pkt_ip_hdr, sizeof(struct rte_ipv4_hdr));
            memcpy(frame_hdr + OFFSET_PKT_UDP, &pkt_udp_hdr, sizeof(struct rte_udp_hdr));

            // Set the socket sending buffer size equal to the desired frame size
            // TODO: investigate if changes something, didn't see much difference so far
            // sock_set_sndbuff(conf->sock_fd, 128 * 1024 * 1024UL);

            // Fill the packet headers of all the buffers at initialization time,
            // to save some time later if possible

            // If using raw sockets, copy frame header into each packet
            // TODO: this operation needed only for outgoing packets
            for (size_t i = 0; i < burst_size; ++i)
                memcpy(packets[i].data(), frame_hdr, PKT_HEADER_SIZE);
        }

        // Finally, initialize payload pointers in base class
        for (auto i = decltype(burst_size){0}; i < burst_size; ++i)
            payloads[i] = packets[i].data() + base_offset;
    };

    ~nfv_socket_simple() = default;

    size_t request_out_buffers(buffer_t buffers[], size_t howmany)
    {
        if (howmany > burst_size)
            howmany = burst_size;

        for (size_t i = 0; i < howmany; ++i)
            buffers[i] = payloads[i];

        return howmany;
    }

    ssize_t send(size_t howmany)
    {
        ssize_t num_sent;

        if (use_mmsg)
        {
            // NOTICE: Assumes all sent messages are fully sent
            num_sent = sendmmsg(sock_fd, datagrams.data(), burst_size, 0);
        }
        else
        {
            for (num_sent = 0; size_t(num_sent) < burst_size; ++num_sent)
            {
                ssize_t res = sendmsg(sock_fd, &datagrams[num_sent].msg_hdr, 0);
                if (res < 0 || size_t(res) != used_size)
                    break;
            }
        }

        return num_sent;
    }

    ssize_t recv(buffer_t buffers[], size_t howmany)
    {
        ssize_t num_recv = 0;

        if (howmany > burst_size)
            howmany = burst_size;

        if (use_mmsg)
        {
            // FIXME: Why should I repeat it each time???
            // for (auto i = decltype(burst_size){0}; i < burst_size; ++i)
            //     datagrams[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);

            // Use one only system call, fills header data too
            // NOTICE: Assumes all received messages are fully received
            num_recv = recvmmsg(sock_fd, datagrams.data(), howmany, 0, NULL);
        }
        else
        {
            // FIXME: why was this payload_len?
            for (num_recv = 0; size_t(num_recv) < howmany; ++num_recv)
            {
                ssize_t res = recvmsg(sock_fd, &datagrams[num_recv].msg_hdr, 0);
                if (res < 0 || size_t(res) != used_size)
                    break;
            }
        }

        for (auto i = decltype(num_recv){0}; i < num_recv; ++i)
            buffers[i] = payloads[i];

        return num_recv;
    }

    ssize_t send_back(buffer_t buffers[], size_t howmany);

private:
    const int sock_fd;

    /* FIXME: assuming is_raw is always false at the beginning */
    const bool is_raw;

    /* FIXME: assuming use_mmsg is always false at the beginning */
    const bool use_mmsg;

    /* The actual size of buffers that need to be allocated */
    const size_t used_size;

    /* The starting point for packet payload, zero for UDP sockets */
    const size_t base_offset;

    /* Header structures, used by raw sockets only */
    struct rte_ether_hdr pkt_eth_hdr;
    struct rte_ipv4_hdr pkt_ip_hdr;
    struct rte_udp_hdr pkt_udp_hdr;

    /* Data structure used to hold the frame header, used by raw sockets only */
    byte_t frame_hdr[PKT_HEADER_SIZE];

    /* Holds all allocated packets */
    std::vector<std::vector<byte_t>> packets;

    /* Holds all sockaddr_in addresses
     * (for outgoing packets they all are the same structure) */
    std::vector<struct sockaddr_in> corr_addresses;

    /* API uses always sendmsg/recvmsg or sendmmsg/recvmmsg API */
    std::vector<struct iovec> iovecs;
    std::vector<struct mmsghdr> datagrams;
};

#endif // NFV_SOCKET_SIMPLE_HPP
