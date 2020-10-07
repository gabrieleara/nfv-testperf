#include "config.h"
#include "constants.h"
#include "nfv_socket_auto.hpp"

#include <sys/socket.h>
#include <vector>

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

class nfv_socket_simple : public nfv_socket {
protected:
    nfv_socket_simple()
        : sock_fd(0), is_raw(false), use_mmsg(false), used_size(0),
          base_offset(0){};

    bool _match(const std::string &type) const override {
        return type.compare("simple") == 0;
    };

    stubptr _create_empty() const override { return new nfv_socket_simple(); }

    stubptr _factory_create(config *conf) const override {
        return new nfv_socket_simple(conf);
    };

protected:
    nfv_socket_simple(config *conf)
        : nfv_socket(conf->pkt_size, conf->payload_size, conf->bst_size),
          sock_fd(conf->sock_fd), is_raw((conf->sock_type & NFV_SOCK_RAW) != 0),
          use_mmsg(conf->use_mmsg),
          used_size(is_raw ? packet_size : payload_size),
          base_offset(is_raw ? OFFSET_PKT_PAYLOAD : 0),
          packets(burst_size, std::vector<byte_t>(used_size, 0)),
          // TODO: check if this form of initialization actually fills all data
          // structures with zeroes
          corr_addresses(burst_size, sockaddr_in{}),
          iovecs(burst_size, iovec{}), datagrams(burst_size, mmsghdr{}) {
        for (auto i = decltype(burst_size){0}; i < burst_size; ++i) {
            // No scatter-gather I/O, all packet is in one piece
            iovecs[i].iov_base = packets[i].data();
            iovecs[i].iov_len = used_size;

            // Only one vector element per message
            datagrams[i].msg_hdr.msg_iov = &iovecs[i];
            datagrams[i].msg_hdr.msg_iovlen = 1;

            if (!is_raw) {
                // These two lines must be repeated for each recv loop.
                // FIXME: you sure?
                datagrams[i].msg_hdr.msg_name =
                    &corr_addresses[i]; // FIXME: rename this data structure
                datagrams[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);

                // Set the address of the corresponding element
                // (necessary only for outgoing packets)
                corr_addresses[i] = conf->remote.ip;
            }
        }

        // For UDP sockets, the corresponding pair IP addr/UDP port is selected
        // automatically using connected sockets. For RAW sockets, we will build
        // a common header to be used by all packets.

        // TODO: probably frame_hdr riesco a tenerlo all'interno del costruttore
        if (is_raw) {
            // For RAW sockets we need to build a frame header, the same for
            // each packet
            // FIXME: Implement packet headers initialization
            // dpdk_setup_pkt_headers(&sself->pkt_eth_hdr, &sself->pkt_ip_hdr,
            // &sself->pkt_udp_hdr, conf);
            memcpy(frame_hdr + OFFSET_PKT_ETHER, &pkt_eth_hdr,
                   sizeof(struct rte_ether_hdr));
            memcpy(frame_hdr + OFFSET_PKT_IPV4, &pkt_ip_hdr,
                   sizeof(struct rte_ipv4_hdr));
            memcpy(frame_hdr + OFFSET_PKT_UDP, &pkt_udp_hdr,
                   sizeof(struct rte_udp_hdr));

            // Set the socket sending buffer size equal to the desired frame
            // size.
            // TODO: investigate if changes something, didn't see much
            // difference so far.
            // sock_set_sndbuff(conf->sock_fd, 128 * 1024 * 1024UL);

            // Fill the packet headers of all the buffers at initialization
            // time, to save some time later if possible

            // If using raw sockets, copy frame header into each packet
            // TODO: this operation needed only for outgoing packets
            for (auto i = decltype(burst_size){0}; i < burst_size; ++i)
                memcpy(packets[i].data(), frame_hdr, PKT_HEADER_SIZE);
        }

        // Finally, initialize payload pointers in base class
        for (auto i = decltype(burst_size){0}; i < burst_size; ++i)
            payloads[i] = packets[i].data() + base_offset;
    };

public:
    virtual ~nfv_socket_simple() = default;

    size_t request_out_buffers(buffer_t buffers[], size_t howmany) {
        if (howmany > burst_size)
            howmany = burst_size;

        // Implicit free of all previously acquired buffers
        if (likely(active_buffers > 0))
            active_buffers = 0;

        for (size_t i = 0; i < howmany; ++i)
            buffers[i] = payloads[i];

        active_buffers += howmany;
        used_buffers = 0;

        return howmany;
    };

    virtual ssize_t send(size_t howmany) override {
        ssize_t num_sent;

        if (unlikely(howmany > active_buffers - used_buffers))
            howmany = active_buffers - used_buffers;

        if (use_mmsg) {
            // NOTICE: Assumes all sent messages are fully sent
            num_sent =
                sendmmsg(sock_fd, datagrams.data() + used_buffers, howmany, 0);
        } else {
            for (num_sent = 0; size_t(num_sent) < howmany; ++num_sent) {
                ssize_t res = sendmsg(
                    sock_fd, &datagrams[num_sent + used_buffers].msg_hdr, 0);
                if (res < 0 || size_t(res) != used_size)
                    break;
            }
        }

        if (likely(num_sent > 0))
            used_buffers += size_t(num_sent);

        return num_sent;
    }

    ssize_t recv(buffer_t buffers[], size_t howmany) override {
        ssize_t num_recv = 0;

        if (howmany > burst_size)
            howmany = burst_size;

        // Implicit free of all previously acquired buffers
        active_buffers = 0;
        used_buffers = 0;

        if (use_mmsg) {
            // FIXME: Why should I repeat it each time???
            // for (auto i = decltype(burst_size){0}; i < burst_size; ++i)
            //     datagrams[i].msg_hdr.msg_namelen = sizeof(struct
            //     sockaddr_in);

            // Use one only system call, fills header data too
            // NOTICE: Assumes all received messages are fully received
            num_recv = recvmmsg(sock_fd, datagrams.data(), howmany, 0, NULL);
        } else {
            // FIXME: why was this payload_len?
            for (num_recv = 0; size_t(num_recv) < howmany; ++num_recv) {
                ssize_t res = recvmsg(sock_fd, &datagrams[num_recv].msg_hdr, 0);
                if (res < 0 || size_t(res) != used_size)
                    break;
            }
        }

        // TODO: likely or unlikely?
        if (unlikely(num_recv > 0)) {
            active_buffers = size_t(num_recv);
            for (size_t i = 0; i < active_buffers; ++i)
                buffers[i] = payloads[i];
        }

        return num_recv;
    };

    ssize_t send_back(size_t howmany) override { return send(howmany); };

private:
    const int sock_fd;
    const bool is_raw;   // TODO: test when is_raw is false
    const bool use_mmsg; // TODO: test when use_mmsg is true

    /* The actual size of buffers that need to be allocated */
    const size_t used_size;

    /* The starting point for packet payload, zero for UDP sockets */
    const size_t base_offset;

    size_t last_recv_howmany = 0;

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

    size_t active_buffers = 0;
    size_t used_buffers = 0;

private:
    AUTOFACTORY_CONCRETE(nfv_socket_simple)
};

AUTOFACTORY_DEFINE(nfv_socket_simple)
