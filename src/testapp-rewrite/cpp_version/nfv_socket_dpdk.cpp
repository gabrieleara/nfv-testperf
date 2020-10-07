#include "config.h"
#include "constants.h"
#include "nfv_socket_auto.hpp"

#include <sys/socket.h>
#include <vector>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

class nfv_socket_dpdk : public nfv_socket {
protected:
    nfv_socket_dpdk() : dpdk_port(0){};

    bool _match(const std::string &type) const override {
        return type.compare("dpdk") == 0;
    };

    stubptr _create_empty() const override { return new nfv_socket_dpdk(); }

    stubptr _factory_create(config *conf) const override {
        return new nfv_socket_dpdk(conf);
    };

protected:
    nfv_socket_dpdk(config *conf)
        // FIXME: really? DPDK port is the sock_fd? Should I change the name
        // maybe?
        : nfv_socket(conf->pkt_size, conf->payload_size, conf->bst_size),
          dpdk_port(conf->sock_fd),
          packets(burst_size, nullptr){
              // FIXME: Implement packet headers initialization
              // dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr,
              // conf);
          };

public:
    virtual ~nfv_socket_dpdk() = default;

    size_t request_out_buffers(buffer_t buffers[], size_t howmany) {
        if (howmany > burst_size) howmany = burst_size;

        struct rte_mbuf *pkt;

        if (pkts_prepared > 0)
            return 0; // FIXME: this should never happen in normal usage, change
                      // behavior to take into account extra-ordinary usage

        // TODO: convert to bulk API
        for (pkts_prepared = 0; pkts_prepared < howmany; ++pkts_prepared) {
            pkt = rte_mbuf_raw_alloc(mbufs);

            if (unlikely(pkt == nullptr)) {
                fprintf(stderr, "WARN: Could not allocate a buffer, using less "
                                "packets than required burst size!\n");
                return pkts_prepared;
            }

            // TODO: should prepare in advance the header of each packet
            /*dpdk_pkt_prepare(pkt, conf, &pkt_eth_hdr, &pkt_ip_hdr,
                             &pkt_udp_hdr);
*/
            packets[pkts_prepared] = pkt;
            buffers[pkts_prepared] = pkt + SOME_OFFSET_FOR_DPDK_PAYLOAD;
        }

        return pkts_prepared;
    };

    virtual ssize_t send(size_t howmany) override {
        ssize_t num_sent;

        // TODO: put likely and unlikey everywhere on these checks
        if (unlikely(howmany > pkts_prepared)) howmany = pkts_prepared;

        num_sent =
            rte_eth_tx_burst(dpdk_port, 0, packets.data() + pkts_used, howmany);

        pkts_used += num_sent < 0 ? 0 : num_sent;

        /*
        // TODO: DO NOT FREE BUFFERS HERE, FREE BUFFERS ONLY WHEN EXPLICITLY
        // CALLED
        // TODO: INSTEAD, MOVE THE POINTERS TO THE BUFFERS BACKWARDS
        for (int pkt_idx = num_sent; pkt_idx < pkts_prepared; ++pkt_idx) {
            rte_pktmbuf_free(pkts_burst[pkt_idx]);
        }
        */

        return num_sent;
    }

    ssize_t recv(buffer_t buffers[], size_t howmany) override {
        ssize_t num_recv = 0;

        if (howmany > burst_size) howmany = burst_size;

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
                if (res < 0 || size_t(res) != used_size) break;
            }
        }

        for (auto i = decltype(num_recv){0}; i < num_recv; ++i)
            buffers[i] = payloads[i];

        last_recv_howmany = num_recv < 0 ? 0 : size_t(num_recv);

        return num_recv;
    };

    ssize_t send_back(size_t howmany) override {
        // TODO: has this sense?
        if (howmany > last_recv_howmany) howmany = burst_size;
        return send(howmany);
    };

    // FIXME: free buffers

private:
    // Header structures, typically the same for each message
    struct rte_ether_hdr pkt_eth_hdr;
    struct rte_ipv4_hdr pkt_ip_hdr;
    struct rte_udp_hdr pkt_udp_hdr;

    // Set of messages to be send/received
    // Sized to the burst_size
    std::vector<struct rte_mbuf *> packets;
    // struct rte_mbuf *packets[]; // FIXME: change to vector

    const dpdk_port_t dpdk_port;

    // TODO: MISSING ARGUMENTS:
    // mbufs (right now should use always same ones)

    // Number of packets prepared and sent/received in current iteration
    size_t pkts_prepared = 0;

    // Number of packets already sent in a previous iteration
    size_t pkts_used = 0;

private:
    AUTOFACTORY_CONCRETE(nfv_socket_dpdk)
};

AUTOFACTORY_DEFINE(nfv_socket_dpdk)
