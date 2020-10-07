#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// These contains data structures useful for raw sockets
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
//#include "dpdk.h"

#include "newcommon.h"
#include "stats.h"
#include "timestamp.h"

// main loop function shall not return because the signal handler for SIGINT
// terminates the program non-gracefully
static inline void main_loop(struct config *conf) __attribute__((noreturn));

#define ever                                                                   \
    ;                                                                          \
    ;

static inline void main_loop(struct config *conf) {
    // ----------------------------- Constants ------------------------------ //

    const ssize_t frame_len = conf->pkt_size;
    const ssize_t payload_len = conf->pkt_size - OFFSET_PAYLOAD;

    // The actual size of buffers that need to be allocated
    const ssize_t used_len =
        (conf->socktype == SOCK_RAW) ? frame_len : payload_len;

    // The starting point for packet payload, zero for UDP sockets
    const ssize_t base_offset =
        (conf->socktype == SOCK_RAW) ? OFFSET_PAYLOAD : 0;

    // ------------------- Variables and data structures -------------------- //

    // Header structures, used by raw sockets only
    struct rte_ether_hdr pkt_eth_hdr;
    struct rte_ipv4_hdr pkt_ip_hdr;
    struct rte_udp_hdr pkt_udp_hdr;

    // Data structure used to hold the frame header, used by raw sockets only
    byte_t frame_hdr[PKT_HEADER_SIZE];

    // Data structures used to hold packets
    // This one is used for simple sockets send/recv
    byte_t pkt[used_len];

    // These three are used for sendmmsg/recvmmsg API
    byte_t buf[conf->bst_size][used_len];
    struct iovec iov[conf->bst_size][1];
    struct mmsghdr datagrams[conf->bst_size];

    // Timers and counters
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz;
    const tsc_t tsc_incr = tsc_hz * conf->bst_size / conf->rate;
    tsc_t tsc_cur, tsc_prev, tsc_next;

    // Stats variables
    struct stats stats = STATS_INIT;
    stats.type = STATS_TX;
    stats_ptr = &stats;

    struct stats_data_tx stats_period = {0, 0};

    // --------------------------- Initialization --------------------------- //

    // Structures initialization
    memset(frame_hdr, 0, sizeof(frame_hdr));
    memset(pkt, 0, sizeof(pkt));

    // For sendmmsg/recvmmsg we need to build them for each message in the burst
    memset(buf, 0, sizeof(buf));
    memset(iov, 0, sizeof(iov));
    memset(datagrams, 0, sizeof(datagrams));

    // Building data structures for all the messages in the burst
    for (unsigned int i = 0; i < conf->bst_size; ++i) {
        iov[i][0].iov_base = buf[i];
        iov[i][0].iov_len = used_len;

        datagrams[i].msg_hdr.msg_iov = iov[i];
        datagrams[i].msg_hdr.msg_iovlen = 1; // Only one vector per message

        // For UDP sockets, addresses are put automatically since they are
        // connected sockets.
        // For RAW sockets we will build below a common header for all the
        // packets
    }

    if (conf->socktype == SOCK_RAW) {
        // For RAW sockets we need to build a frame header, the same for each
        // packet
        dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, conf);
        memcpy(frame_hdr + OFFSET_ETHER, &pkt_eth_hdr,
               sizeof(struct rte_ether_hdr));
        memcpy(frame_hdr + OFFSET_IPV4, &pkt_ip_hdr,
               sizeof(struct rte_ipv4_hdr));
        memcpy(frame_hdr + OFFSET_UDP, &pkt_udp_hdr,
               sizeof(struct rte_udp_hdr));

        // Set the socket sending buffer size equal to the desired frame size
        // It doesn't seem to affect the system so much actually
        // TODO: change this if necessary
        // sock_set_sndbuff(conf->sock_fd, 128 * 1024 * 1024UL);
    }

    // ------------------ Infinite loop variables and body ------------------ //

    int res;
    unsigned int nb_pkt;
    unsigned int i;

    tsc_cur = tsc_prev = tsc_next = tsc_get_update();

    for (ever) {
        tsc_cur = tsc_get_update();

        // If more than a second elapsed
        if (tsc_cur - tsc_prev > tsc_out) {
            // Save stats
            stats_save(&stats, (union stats_data *)&stats_period);

            // If not silent, print them
            if (!conf->silent) {
                stats_print(STATS_TX, (union stats_data *)&stats_period);
            }

            // Reset stats for the new period
            stats_period.tx = 0;
            stats_period.dropped = 0;

            // Update timers
            tsc_prev = tsc_cur;
        }

        // If it is already time for the next burst (or in general tsc_next is
        // less than tsc_cur), send the next burst
        if (tsc_cur > tsc_next) {
            tsc_next += tsc_incr;

            if (conf->use_mmsg) {
                // If using raw sockets, copy frame header into each packet
                if (conf->socktype == SOCK_RAW) {
                    for (i = 0; i < conf->bst_size; ++i)
                        memcpy(buf[i], frame_hdr, PKT_HEADER_SIZE);
                }

                // Put payload data in each packet
                for (size_t i = 0; i < conf->bst_size; ++i) {
                    // The first element will have the current value of the tsc,
                    // then a dummy payload will be inserted to fill the packet
                    // The tsc value is taken after producing the dummy data

                    // If data should be produced, fill each packet
                    if (conf->touch_data) {
                        produce_data_offset(buf[i] + base_offset,
                                            payload_len - sizeof(tsc_cur),
                                            sizeof(tsc_cur));
                    }

                    tsc_cur = tsc_get_update();
                    put_i64_offset(buf[i], base_offset, tsc_cur);
                }

                // Use one only system call
                res = sendmmsg(conf->sock_fd, datagrams, conf->bst_size, 0);

                // Errors are considered all dropped packets
                if (res < 0)
                    res = 0;

                stats_period.tx += res;
                stats_period.dropped += conf->bst_size - res;
            } else {
                // Use multiple system calls
                for (nb_pkt = 0; nb_pkt < conf->bst_size; ++nb_pkt) {
                    // If using raw sockets, copy fram header
                    if (conf->socktype == SOCK_RAW)
                        memcpy(pkt, frame_hdr, PKT_HEADER_SIZE);

                    // If data should be produced, fill each packet
                    if (conf->touch_data)
                        produce_data(pkt + base_offset, payload_len);

                    // Send a single packet at a time
                    res = send(conf->sock_fd, pkt, used_len, 0);
                    if (res < used_len) {
                        // In UDP/RAW either you drop a packet or send it all
                        ++stats_period.dropped;
                    } else {
                        ++stats_period.tx;
                    }
                }
            }
        }
    }

    __builtin_unreachable();
}

int send_body(int argc, char *argv[]) {
    struct config conf = CONFIG_STRUCT_INITIALIZER;

    // Default configuration for local program
    conf.local_port = SEND_PORT;
    conf.remote_port = RECV_PORT;
    conf.socktype = SOCK_DGRAM;
    strcpy(conf.local_ip, SEND_ADDR_IP);
    strcpy(conf.remote_ip, RECV_ADDR_IP);
    strcpy(conf.local_mac, SEND_ADDR_MAC);
    strcpy(conf.remote_mac, RECV_ADDR_MAC);

    parameters_parse(argc, argv, &conf);

    print_config(&conf);

    int res = sock_create(&conf, O_NONBLOCK, true);
    if (res)
        return EXIT_FAILURE;

    signal(SIGINT, handle_sigint);

    tsc_init();

    main_loop(&conf);

    // Should never get here actually
    close(conf.sock_fd);

    return EXIT_SUCCESS;
}
