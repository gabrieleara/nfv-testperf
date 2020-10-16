// For sendmmsg
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/socket.h>

#include "config.h"
#include "constants.h"
#include "nfv_socket_simple.h"

// FIXME: all kinds of error checking for mallocs...

NFV_SIMPLE_SIGNATURE(void, init, config_ptr conf) {
    struct nfv_socket_simple *sself = (struct nfv_socket_simple *)(self);

    sself->active_buffers = 0;
    sself->used_buffers = 0;

    // nfv_socket(conf->pkt_size, conf->payload_size, conf->bst_size),
    sself->sock_fd = conf->sock_fd;
    sself->is_raw = (conf->sock_type & NFV_SOCK_RAW) != 0;
    sself->use_mmsg = conf->use_mmsg;
    sself->used_size = sself->is_raw ? self->packet_size : self->payload_size;
    sself->base_offset = sself->is_raw ? OFFSET_PKT_PAYLOAD : 0;

    // Allocate all vectors of data
    sself->packets = malloc(sizeof(buffer_t) * self->burst_size);
    sself->corr_addresses =
        calloc(self->burst_size, sizeof(struct sockaddr_in));
    sself->iovecs = calloc(self->burst_size, sizeof(struct iovec));
    sself->datagrams = calloc(self->burst_size, sizeof(struct mmsghdr));

    // Allocate and initialize the entire memory area that will be used for data
    // buffers
    /*
    byte_t *base_buffers_ptr =
        calloc(self->burst_size * sself->used_size, sizeof(byte_t));
        */

    // Initialize all data structures
    for (size_t i = 0; i < self->burst_size; ++i) {
        // Packets are all in a contiguous memory area allocated above, each
        // separated by packet_size bytes
        // NOTICE: ignoring alignment problems for now!
        /*
        sself->packets[i] =
            base_buffers_ptr + (sizeof(byte_t) * sself->used_size) * i;
            */

        sself->packets[i] = calloc(sself->used_size, sizeof(byte_t));

        // No scatter-gather I/O, all packet is in one piece
        // CHECKED: OK
        sself->iovecs[i].iov_base = sself->packets[i];
        sself->iovecs[i].iov_len = sself->used_size;

        // Only one vector element per message
        // CHECKED: OK
        sself->datagrams[i].msg_hdr.msg_iov = &sself->iovecs[i];
        sself->datagrams[i].msg_hdr.msg_iovlen = 1;

        // THIS IS RETURNED BY THE sendmmsg, I DO NOT HAVE TO SET IT
        // sself->datagrams[i].msg_len = sself->used_size;

        if (!sself->is_raw) {
            // These two lines must be repeated for each recv loop.
            // FIXME: you sure?
            // FIXME: rename this data structure
            sself->datagrams[i].msg_hdr.msg_name = &sself->corr_addresses[i];
            sself->datagrams[i].msg_hdr.msg_namelen =
                sizeof(struct sockaddr_in);

            // Set the address of the corresponding element
            // (necessary only for outgoing packets)
            sself->corr_addresses[i] = conf->remote.ip;
        }
    }

    // For UDP sockets, the corresponding pair IP addr/UDP port is selected
    // automatically using connected sockets. For RAW sockets, we will build
    // a common header to be used by all packets.

    if (sself->is_raw) {
        pkt_hdr_setup(&sself->incoming_hdr, conf, DIR_INCOMING);
        pkt_hdr_setup(&sself->outgoing_hdr, conf, DIR_OUTGOING);

        rte_memcpy(sself->frame_hdr, &sself->incoming_hdr,
                   OFFSET_PKT_PAYLOAD - OFFSET_PKT_ETHER);

        /*
        rte_memcpy(sself->frame_hdr + OFFSET_PKT_ETHER, &sself->ether_hdr,
                   sizeof(struct rte_ether_hdr));
        rte_memcpy(sself->frame_hdr + OFFSET_PKT_IPV4, &sself->ip_hdr,
                   sizeof(struct rte_ipv4_hdr));
        rte_memcpy(sself->frame_hdr + OFFSET_PKT_UDP, &sself->udp_hdr,
                   sizeof(struct rte_udp_hdr));
        */

        // Set the socket sending buffer size equal to the desired frame
        // size.
        // TODO: investigate if changes something, didn't see much
        // difference so far.
        // sock_set_sndbuff(conf->sock_fd, 128 * 1024 * 1024UL);

        // Fill the packet headers of all the buffers at initialization
        // time, to save some time later if possible

        // If using raw sockets, copy frame header into each packet
        for (size_t i = 0; i < self->burst_size; ++i)
            rte_memcpy(sself->packets[i], sself->frame_hdr, PKT_HEADER_SIZE);
    }

    // Finally, initialize payload pointers in base class
    for (size_t i = 0; i < self->burst_size; ++i)
        self->payloads[i] = sself->packets[i] + sself->base_offset;
}

// FIXME: if I give less than burst_size here, I should remember it because
// otherwise the send would send too many packets
NFV_SIMPLE_SIGNATURE(size_t, request_out_buffers, buffer_t buffers[],
                     size_t howmany) {
    struct nfv_socket_simple *sself = (struct nfv_socket_simple *)(self);

    if (unlikely(howmany > self->burst_size))
        howmany = self->burst_size;

    // Implicit free of all previously acquired buffers
    sself->active_buffers = 0;

    for (size_t i = 0; i < howmany; ++i)
        buffers[i] = self->payloads[i];

    sself->active_buffers += howmany;
    sself->used_buffers = 0;

    return howmany;
}

NFV_SIMPLE_SIGNATURE(ssize_t, send, size_t howmany) {
    struct nfv_socket_simple *sself = (struct nfv_socket_simple *)(self);

    ssize_t num_sent;

    byte_t base_buffers_ptr[sself->used_size * self->burst_size];

    rte_memcpy(base_buffers_ptr, sself->packets[0],
               sizeof(byte_t) * sself->used_size * self->burst_size);

    if (unlikely(howmany > sself->active_buffers - sself->used_buffers))
        howmany = sself->active_buffers - sself->used_buffers;

    if (unlikely(howmany == 0))
        return 0;

    if (sself->use_mmsg) {
        // NOTICE: Assumes all sent messages are fully sent
        num_sent =
            sendmmsg(sself->sock_fd,
                     sself->datagrams /* + sself->used_buffers */, howmany, 0);
    } else {
        for (num_sent = 0; ((size_t)(num_sent)) < howmany; ++num_sent) {
            ssize_t res = sendmsg(
                sself->sock_fd,
                &(sself->datagrams[num_sent + sself->used_buffers].msg_hdr), 0);
            if (res < 0 || ((size_t)(res)) != sself->used_size) {
                break;
            }
        }
    }

    if (likely(num_sent > 0))
        sself->used_buffers += ((size_t)(num_sent));

    return num_sent;
}

// FIXME: check incoming packet headers?
NFV_SIMPLE_SIGNATURE(ssize_t, recv, byte_ptr_t *buffers, size_t howmany) {
    struct nfv_socket_simple *sself = (struct nfv_socket_simple *)(self);

    ssize_t num_recv;
    ssize_t num_recv_good;

    if (unlikely(howmany > self->burst_size))
        howmany = self->burst_size;

    // Implicit free of all previously acquired buffers
    sself->active_buffers = 0;
    sself->used_buffers = 0;

    if (sself->use_mmsg) {
        // FIXME: Why should I repeat it each time???
        // for (auto i = decltype(burst_size){0}; i < burst_size; ++i)
        //     datagrams[i].msg_hdr.msg_namelen = sizeof(struct
        //     sockaddr_in);

        // Use one only system call, fills header data too
        // NOTICE: Assumes all received messages are fully received
        num_recv = recvmmsg(sself->sock_fd, sself->datagrams, howmany, 0, NULL);
    } else {
        // FIXME: why was this payload_len?
        for (num_recv = 0; ((size_t)(num_recv)) < howmany; ++num_recv) {
            ssize_t res =
                recvmsg(sself->sock_fd, &sself->datagrams[num_recv].msg_hdr, 0);
            if (res < 0 || ((size_t)(res)) != sself->used_size)
                break;
        }
    }

    num_recv_good = num_recv;

    // I put a "likely" here to prefer scenarios in which there is actually
    // something to do with the incoming packets.
    if (likely(num_recv > 0)) {
        if (sself->is_raw) {
            num_recv_good = 0;

            // Filter-out packets NOT meant for this application
            for (ssize_t i = 0; i < num_recv; ++i) {
                const struct pkt_hdr *header =
                    (struct pkt_hdr *)sself->packets[i];

                if (hdr_check_incoming(header, &sself->incoming_hdr)) {
                    // Packet was meant for this application!
                    sself->packets[num_recv_good] = sself->packets[i];
                    ++num_recv_good;
                } else {
                    // Implicit free of this buffer by not increasing
                    // num_recv_good
                }
            }
        }

        sself->active_buffers += (size_t)num_recv_good;

        for (ssize_t i = 0; i < num_recv_good; ++i)
            buffers[i] = self->payloads[i];
    }

    return num_recv_good;
}

NFV_SIMPLE_SIGNATURE(ssize_t, send_back, size_t howmany) {
    struct nfv_socket_simple *sself = (struct nfv_socket_simple *)(self);

    if (unlikely(howmany > sself->active_buffers - sself->used_buffers))
        howmany = sself->active_buffers - sself->used_buffers;

    if (unlikely(howmany == 0))
        return 0;

    // If using raw sockets, swap addresses information too
    if (sself->is_raw) {
        struct rte_ipv4_hdr *ip_hdr;

        for (size_t i = 0; i < howmany; ++i) {
            size_t j = i + sself->used_buffers;

            swap_ether_addr(
                (struct rte_ether_hdr *)(sself->packets[j] + OFFSET_PKT_ETHER));
            swap_ipv4_addr(
                (struct rte_ipv4_hdr *)(sself->packets[j] + OFFSET_PKT_IPV4));
            swap_udp_port(
                (struct rte_udp_hdr *)(sself->packets[j] + OFFSET_PKT_UDP));

            ip_hdr =
                (struct rte_ipv4_hdr *)(sself->packets[j] + OFFSET_PKT_IPV4);
            ip_hdr->hdr_checksum = 0;
            ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
        }
    }

    return nfv_socket_simple_send(self, howmany);
}
