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

#include "timestamp.h"
#include "newcommon.h"
#include "stats.h"

// main loop function shall not return because the signal handler for SIGINT
// terminates the program non-gracefully
static inline void main_loop(struct config *conf) __attribute__((noreturn));

#define ever \
    ;        \
    ;

// This version of the client sends only one packet at the time and wait for its
// return spinning
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

    // Timers and counters
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz;
    const tsc_t tsc_max_delay = tsc_hz / 100; // ~ 10 milliseconds is definitely a big timeout*[citation needed]
    tsc_t tsc_cur, tsc_next, tsc_prev;
    tsc_t tsc_last, tsc_pkt;

    // Stats variables
    struct stats stats = STATS_INIT;
    stats.type = STATS_DELAY;
    stats_ptr = &stats;

    struct stats_data_delay stats_period = {0, 0};

    // --------------------------- Initialization --------------------------- //

    // Structures initialization
    memset(frame_hdr, 0, sizeof(frame_hdr));
    memset(pkt, 0, sizeof(pkt));

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

    int res;

    tsc_last = tsc_cur = tsc_prev = tsc_next = tsc_get_update();

    for (ever)
    {
        tsc_cur = tsc_get_update();

        // If more than a second elapsed
        if (tsc_cur - tsc_prev > tsc_out)
        {
            // If there is some stat to actually save
            if (stats_period.num)
            {
                // Calculate the actual average (until now it is a total,
                // despite the name)
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

        // NOTICE: this application ignores sending rate and sends a new packet
        // each time an old one is received (or long timer expires)

        // If using raw sockets, copy frame header into each packet
        if (conf->socktype == SOCK_RAW)
            rte_memcpy(pkt, frame_hdr, PKT_HEADER_SIZE);

        // Produce dummy data (after the expected tsc value as first data of the packet)
        if (conf->touch_data)
            produce_data_offset(pkt, data_offset, data_len);

        do
        {
            tsc_last = tsc_cur = tsc_get_update();
            put_i64_offset(pkt, base_offset, tsc_cur);
            res = send(conf->sock_fd, pkt, used_len, 0);
        } while (res < 0 || (size_t)res != used_len);

        do
        {
            // I could technically use the same socket, but the server should be
            // rewritten for that case, so I'll stick with two sockets for
            // bidirectional communication
            res = recv(conf->sock_fd, pkt, used_len, 0);
            tsc_cur = tsc_get_update();

            if (res >= 0 && (size_t)res == used_len)
            {
                // Compare current timer value with the one inside the packet
                tsc_pkt = get_i64_offset(pkt, base_offset);

                // Ignore this packet if not the last sent
                if (tsc_pkt != tsc_last)
                {
                    res = 0;
                    fprintf(stderr, "ERR: last timestamp is not the same!\n");

                    // Besides printing this error, there is no need to do anything,
                    // eventually the program re-aligns with itself, even after some time
                }
            }

            // Exit if the right packet has been received or too much time
            // elapsed
        } while ((res < 0 || (size_t)res != used_len) && tsc_cur - tsc_last < tsc_max_delay);

        if (tsc_cur - tsc_last < tsc_max_delay)
        {
            // Consume data
            if (conf->touch_data)
                consume_data_offset(pkt, data_offset, data_len);
            tsc_cur = tsc_get_update();

            stats_period.avg += tsc_cur - tsc_pkt;
            ++stats_period.num;
        }
    }

    __builtin_unreachable();
}

int clientst_body(int argc, char *argv[])
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

    main_loop(&conf);

    // Should never get here actually
    close(conf.sock_fd);

    return EXIT_SUCCESS;
}
