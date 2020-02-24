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

#include <pthread.h>
#include <endian.h>

#include "newcommon.h"
#include "timestamp.h"
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

    const size_t frame_len = conf->pkt_size;
    const size_t payload_len = calc_payload_len(conf->pkt_size);

    // The actual size of buffers that need to be allocated
    const size_t used_len = (conf->socktype == SOCK_RAW) ? frame_len : payload_len;

    // The starting point for packet payload, zero for UDP sockets
    const size_t base_offset = (conf->socktype == SOCK_RAW) ? OFFSET_PAYLOAD : 0;

    const size_t data_offset = base_offset + sizeof(tsc_t);
    const size_t data_len = calc_data_len(conf->pkt_size, sizeof(tsc_t));

    // ------------------- Variables and data structures -------------------- //

    // Header structures, used by raw sockets only
    struct ether_hdr pkt_eth_hdr;
    struct ipv4_hdr pkt_ip_hdr;
    struct udp_hdr pkt_udp_hdr;

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
    const tsc_t tsc_incr = tsc_hz * conf->bst_size / conf->rate;
    tsc_t tsc_cur, tsc_next;

    // --------------------------- Initialization --------------------------- //

    // Structures initialization
    memset(frame_hdr, 0, sizeof(frame_hdr));
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

        // For UDP sockets, addresses are put automatically since they are
        // connected sockets.
        // For RAW sockets we will build below a common header for all the
        // packets
    }

    if (conf->socktype == SOCK_RAW)
    {
        // For RAW sockets we need to build a frame header, the same for each
        // packet
        dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, conf);
        rte_memcpy(frame_hdr + OFFSET_ETHER, &pkt_eth_hdr, sizeof(struct ether_hdr));
        rte_memcpy(frame_hdr + OFFSET_IPV4, &pkt_ip_hdr, sizeof(struct ipv4_hdr));
        rte_memcpy(frame_hdr + OFFSET_UDP, &pkt_udp_hdr, sizeof(struct udp_hdr));

        // Set the socket sending buffer size equal to the desired frame size
        // It doesn't seem to affect the system so much actually
        // NOTE: change this if necessary
        // sock_set_sndbuff(conf->sock_fd, 128 * 1024 * 1024UL);
    }

    // ------------------ Infinite loop variables and body ------------------ //

    tsc_cur = tsc_next = tsc_get_last();

    for (ever)
    {
        tsc_cur = tsc_get_last();

        // If it is already time for the next burst (or in general tsc_next is
        // less than tsc_cur), send the next burst
        if (tsc_cur > tsc_next)
        {
            tsc_next += tsc_incr;

            if (conf->use_mmsg)
            {
                // If using raw sockets, copy frame header into each packet
                if (conf->socktype == SOCK_RAW)
                {
                    for (size_t i = 0; i < conf->bst_size; ++i)
                    {
                        rte_memcpy(buf[i], frame_hdr, PKT_HEADER_SIZE);
                    }
                }

                // Put data in each packet
                for (size_t i = 0; i < conf->bst_size; ++i)
                {
                    // The first element will have the current value of the tsc,
                    // then a dummy payload will be inserted to fill the packet
                    // The tsc value is taken after producing the dummy data

                    // If data should be produced, fill each packet
                    if (conf->touch_data)
                    {
                        produce_data_offset(buf[i], data_offset, data_len);
                    }

                    tsc_cur = tsc_get_last();
                    put_i64_offset(buf[i], base_offset, tsc_cur);
                }

                // Use one only system call
                sendmmsg(conf->sock_fd, datagrams, conf->bst_size, 0);
            }
            else
            {
                // Use multiple system calls
                for (size_t i = 0; i < conf->bst_size; ++i)
                {
                    // If using raw sockets, copy frame header into each packet
                    if (conf->socktype == SOCK_RAW)
                        rte_memcpy(pkt, frame_hdr, PKT_HEADER_SIZE);

                    if (conf->touch_data)
                        produce_data_offset(pkt, data_offset, data_len);

                    tsc_cur = tsc_get_last();
                    put_i64_offset(pkt, base_offset, tsc_cur);

                    send(conf->sock_fd, pkt, used_len, 0);
                }
            }
        }
    }

    __builtin_unreachable();
}

static void *pong_body(void *arg) __attribute__((noreturn));

#define UNUSED(v) ((void)v)

static void *pong_body(void *arg)
{
    struct config *conf = (struct config *)arg;

    // ----------------------------- Constants ------------------------------ //

    const size_t frame_len = conf->pkt_size;
    const size_t payload_len = calc_payload_len(conf->pkt_size);

    // The actual size of buffers that need to be allocated
    const size_t used_len = (conf->socktype == SOCK_RAW) ? frame_len : payload_len;

    // The starting point for packet payload, zero for UDP sockets
    const size_t base_offset = (conf->socktype == SOCK_RAW) ? OFFSET_PAYLOAD : 0;

    const size_t data_offset = base_offset + sizeof(tsc_t);
    const size_t data_len = calc_data_len(conf->pkt_size, sizeof(tsc_t));

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
    tsc_t tsc_pkt, tsc_diff;

    // Stats variables
    struct stats stats = STATS_INIT;
    stats.type = STATS_DELAY;
    stats_ptr = &stats;

    struct stats_data_delay stats_period = {0, 0};

    // --------------------------- Initialization --------------------------- //

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

        // For UDP sockets, addresses are put automatically since they are
        // connected sockets.
        // For RAW sockets we will build below a common header for all the
        // packets
    }

    // ------------------ Infinite loop variables and body ------------------ //

    int res;
    tsc_cur = tsc_prev = tsc_get_last();

    for (ever)
    {
        tsc_cur = tsc_get_last();

        // If more than a second elapsed
        if (tsc_cur - tsc_prev > tsc_out)
        {
            // If there is some stat to actually save
            if (stats_period.num)
            {
                // Calculate the actual average (until now it is a total, despite
                // the name)
                stats_period.avg = stats_period.avg / stats_period.num;

                // Save stats
                stats_save(&stats, (union stats_data *)&stats_period);

                // If not silent, print them
                if (!conf->silent)
                {
                    stats_print(STATS_DELAY, (union stats_data *)&stats_period);
                }
            }

            // Reset stats for the new period
            stats_period.avg = 0;
            stats_period.num = 0;

            // Update timers
            tsc_prev = tsc_cur;
        }

        if (conf->use_mmsg)
        {
            // Use one only system call
            res = recvmmsg(conf->sock_fd, datagrams, conf->bst_size, 0, NULL);
            if (res < 0)
                res = 0;

            for (int i = 0; i < res; ++i)
            {
                tsc_pkt = get_i64_offset(buf[i], base_offset);
                if (conf->touch_data)
                {
                    consume_data_offset(buf[i], data_offset, data_len);
                }

                tsc_cur = tsc_get_last();

                // NOTICE: With this, packets with more than 0.1s delay are
                // considered dropped
                tsc_diff = tsc_cur - tsc_pkt;
                if (tsc_diff < tsc_hz / 10)
                {
                    // Add calculated delay to the avg attribute (which is actually
                    // the sum of all delays, the actual average is calculated only
                    // once per second).
                    stats_period.avg += tsc_diff;
                    ++stats_period.num;
                }
                else if (!conf->silent)
                {
                    printf("ERR: Received message with very big time difference: TSC DIFF %lu (TSC_HZ %lu)\n", tsc_diff, tsc_hz);
                }
            }
        }
        else
        {
            res = recv(conf->sock_fd, pkt, used_len, 0);

            // In UDP you either receive it all or not receive anything at all
            if (res > 0 && (size_t)res == used_len)
            {
                // Compare current timer value with the one inside the packet
                tsc_pkt = get_i64_offset(pkt, base_offset);
                if (conf->touch_data)
                    produce_data_offset(pkt, data_offset, data_len);
                tsc_cur = tsc_get_last();

                // NOTICE: With this, packets with more than 0.1s delay are
                // considered dropped
                tsc_diff = tsc_cur - tsc_pkt;
                if (tsc_diff < tsc_hz / 10)
                {
                    // Add calculated delay to the avg attribute (which is actually
                    // the sum of all delays, the actual average is calculated only
                    // once per second).
                    stats_period.avg += tsc_diff;
                    ++stats_period.num;
                }
                else if (!conf->silent)
                {
                    printf("ERR: Received message with very big time difference: TSC DIFF %lu (TSC_HZ %lu)\n", tsc_diff, tsc_hz);
                }
            }
        }
    }

    __builtin_unreachable();
}

static void *tsc_body(void *arg)
{
    UNUSED(arg);

    for (ever)
    {
        tsc_get_update();
    }

    __builtin_unreachable();
}

int client_body(int argc, char *argv[])
{
    struct config conf = CONFIG_STRUCT_INITIALIZER;

    // Default configuration for local program
    conf.local_port = CLIENT_PORT;
    conf.remote_port = SERVER_PORT;
    conf.socktype = SOCK_DGRAM;
    strcpy(conf.local_ip, CLIENT_ADDR_IP);
    strcpy(conf.remote_ip, SERVER_ADDR_IP);
    strcpy(conf.local_mac, CLIENT_ADDR_MAC);
    strcpy(conf.remote_mac, SERVER_ADDR_MAC);

    parameters_parse(argc, argv, &conf);

    print_config(&conf);

    int res;

    res = sock_create(&conf, O_NONBLOCK, true);
    if (res)
        return EXIT_FAILURE;

    signal(SIGINT, handle_sigint);

    tsc_init();

    // Time to start another thread
    pthread_t tsc_thread;
    pthread_create(&tsc_thread, NULL, tsc_body, NULL);

    sleep(1);

    pthread_t pong_thread;
    pthread_create(&pong_thread, NULL, pong_body, (void *)&conf);

    main_loop(&conf);

    // Should never get here actually
    close(conf.sock_fd);

    return EXIT_SUCCESS;
}
