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

#include <rte_ethdev.h>

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

    const size_t data_offset = OFFSET_DATA_SR;
    const size_t data_len = conf->pkt_size - data_offset;

    // Timer constants
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz;
    const int advertisement_period = 5;

    // ------------------- Variables and data structures -------------------- //

    // Header structures (same for each message)
    //struct ether_hdr pkt_eth_hdr;
    //struct ipv4_hdr pkt_ip_hdr;
    //struct udp_hdr pkt_udp_hdr;

    // Set of messages to be send/received
    struct rte_mbuf *pkts_burst[conf->bst_size];

    // Number of packets prepared and sent in current iteration
    size_t pkts_actually_rx;
    size_t pkts_rx;
    size_t i;

    // Timers
    tsc_t tsc_cur, tsc_prev, tsc_next;

    // Stats variables
    struct stats stats = STATS_INIT;
    stats.type = STATS_RX;
    stats_ptr = &stats;

    struct stats_data_rx stats_period = {0};

    int adv_counter = advertisement_period;

    // --------------------------- Initialization --------------------------- //

    //dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, conf);

    // ------------------ Infinite loop variables and body ------------------ //

    tsc_cur = tsc_prev = tsc_next = tsc_get_update();

    for (ever)
    {
        tsc_cur = tsc_get_update();

        // If more than a second elapsed
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
            ++adv_counter;
        }

        // THIS IS USED TO INFORM LEARNING SWITCHES WHERE THE MAC ADDRESS OF
        // THIS APPLICATION RESIDES (A SORT OF ARP ADVERTISEMENT)
        if (adv_counter >= advertisement_period) {
            dpdk_advertise_host_mac(conf);
            adv_counter = 0;
        }

        pkts_rx = rte_eth_rx_burst(conf->dpdk.portid, 0, pkts_burst, conf->bst_size);
        pkts_actually_rx = 0;

        // Check which messages are actually meant for this application
        for (i = 0; i < pkts_rx; ++i)
        {
            if (dpdk_check_dst_addr_port(pkts_burst[i], conf))
            {
                // Consume data if required
                if (conf->touch_data)
                {
                    dpdk_consume_data_offset(pkts_burst[i], data_offset, data_len);
                }

                ++pkts_actually_rx;
            }

            rte_pktmbuf_free(pkts_burst[i]);
        }

        stats_period.rx += pkts_actually_rx;
    }

    __builtin_unreachable();
}

int dpdk_recv_body(int argc, char *argv[])
{
    struct config conf = CONFIG_STRUCT_INITIALIZER;

    // Default configuration for local program
    conf.local_port = RECV_PORT;
    conf.remote_port = SEND_PORT;
    conf.socktype = SOCK_NONE;
    conf.direction = DIRECTION_RX_ONLY;
    strcpy(conf.local_ip, RECV_ADDR_IP);
    strcpy(conf.remote_ip, SEND_ADDR_IP);
    strcpy(conf.local_mac, RECV_ADDR_MAC);
    strcpy(conf.remote_mac, SEND_ADDR_MAC);

    int argind = parameters_parse(argc, argv, &conf);

    argc -= argind;
    argv += argind;

    int res = dpdk_init(argc, argv, &conf);
    if (res)
        return EXIT_FAILURE;

    print_config(&conf);

    signal(SIGINT, handle_sigint);

    tsc_init();

    main_loop(&conf);

    // Should never get here actually
    // close(conf.sock_fd);

    return EXIT_SUCCESS;
}
