#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>

// These contains data structures useful for raw sockets
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
//#include "dpdk.h"

#include "timestamp.h"

#include "newcommon.h"
#include "stats.h"

// main loop function shall not return because the signal handler for SIGINT
// terminates the program non-gracefully
static inline void main_loop(struct config *conf) __attribute__((noreturn));

#define ever \
    ;        \
    ;

static inline void main_loop(struct config *conf)
{
    // ----------------------------- Constants ------------------------------ //

    const ssize_t frame_len = conf->pkt_size;
    const ssize_t payload_len = conf->pkt_size - OFFSET_PAYLOAD;

    // The actual size of buffers that need to be allocated
    const ssize_t used_len = (conf->socktype == SOCK_RAW) ? frame_len : payload_len;

    // The starting point for packet payload, zero for UDP sockets
    const ssize_t base_offset = (conf->socktype == SOCK_RAW) ? OFFSET_PAYLOAD : 0;

    // ------------------- Variables and data structures -------------------- //

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
    tsc_t tsc_cur, tsc_prev;

    // Stats variables
    struct stats stats = STATS_INIT;
    stats.type = STATS_RX;
    stats_ptr = &stats;

    struct stats_data_rx stats_period = {0};

    // Non connected sockets variables
    struct sockaddr_in src_addr;
    socklen_t addrlen;

    struct sockaddr_in src_addresses[conf->bst_size];

    // --------------------------- Initialization --------------------------- //

    // Initialize addrlen
    addrlen = sizeof(src_addr);

    // Structures initialization
    memset(pkt, 0, sizeof(pkt));

    // For sendmmsg/recvmmsg we need to build them for each message in the burst
    memset(buf, 0, sizeof(buf));
    memset(iov, 0, sizeof(iov));
    memset(datagrams, 0, sizeof(datagrams));

    // Building data structures for all the messages in the burst
    for (unsigned int i = 0; i < conf->bst_size; ++i)
    {
        iov[i][0].iov_base = buf[i];
        iov[i][0].iov_len = used_len;

        datagrams[i].msg_hdr.msg_iov = iov[i];
        datagrams[i].msg_hdr.msg_iovlen = 1; // Only one vector per message

        // Since addresses are NOT automatic (NOT A CONNECTED SOCKET) we must
        // specify where to put them
        datagrams[i].msg_hdr.msg_name = &src_addresses[i];
        datagrams[i].msg_hdr.msg_namelen = sizeof(src_addresses[i]);
    }

    // ------------------ Infinite loop variables and body ------------------ //

    int res;

    tsc_cur = tsc_prev = tsc_get_update();

    for (ever)
    {
        tsc_cur = tsc_get_update();

        // If more than a second elapsed, print stats
        if (tsc_cur - tsc_prev > tsc_out)
        {
            // Save stats
            stats_save(&stats, (union stats_data *)&stats_period);

            // If not silent, print them
            if (!conf->silent)
            {
                stats_print(STATS_RX, (union stats_data *)&stats_period);
            }

            // Reset stats for the new period
            stats_period.rx = 0;

            // Update timers
            tsc_prev = tsc_cur;
        }

        if (conf->use_mmsg)
        {
            // Use one only system call
            res = recvmmsg(conf->sock_fd, datagrams, conf->bst_size, 0, NULL);
            //fprintf(stderr, "RES=%d: %s\n", res, strerror(errno));
            if (res < 0)
                res = 0;

            if (res > 0)
            {
                // If data should be consumed, do that
                if (conf->touch_data)
                {
                    for (int i = 0; i < res; ++i)
                        consume_data(buf[i] + base_offset, payload_len);
                }
            }

            stats_period.rx += res;
        }
        else
        {
            res = recvfrom(conf->sock_fd, pkt, payload_len, 0, &src_addr, &addrlen);
            //fprintf(stderr, "RES=%d: %s\n", res, strerror(errno));

            // In UDP/RAW you either receive it all or not receive anything at all
            if (res == payload_len)
            {
                // If data should be consumed, do that
                if (conf->touch_data)
                    consume_data(pkt + base_offset, payload_len);

                ++stats_period.rx;
            }
        }
    }

    __builtin_unreachable();
}

int recv_body(int argc, char *argv[])
{
    struct config conf = CONFIG_STRUCT_INITIALIZER;

    // Default configuration for local program
    conf.local_port = RECV_PORT;
    conf.remote_port = SEND_PORT;
    conf.socktype = SOCK_DGRAM;
    strcpy(conf.local_ip, RECV_ADDR_IP);
    strcpy(conf.remote_ip, SEND_ADDR_IP);
    strcpy(conf.local_mac, RECV_ADDR_MAC);
    strcpy(conf.remote_mac, SEND_ADDR_MAC);

    parameters_parse(argc, argv, &conf);

    print_config(&conf);

    int res = sock_create(&conf, O_NONBLOCK, false);
    if (res)
        return EXIT_FAILURE;

    signal(SIGINT, handle_sigint);

    tsc_init();

    main_loop(&conf);

    // Should never get here actually
    close(conf.sock_fd);

    return EXIT_SUCCESS;
}
