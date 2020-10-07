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
    const tsc_t tsc_incr = tsc_hz * conf->bst_size / conf->rate;

    // ------------------- Variables and data structures -------------------- //

    // Header structures (same for each message)
    struct rte_ether_hdr pkt_eth_hdr;
    struct rte_ipv4_hdr pkt_ip_hdr;
    struct rte_udp_hdr pkt_udp_hdr;

    // Set of messages to be send/received
    struct rte_mbuf *pkts_burst[conf->bst_size];
    struct rte_mbuf *pkt;

    // Number of packets prepared and sent in current iteration
    size_t pkts_prepared;
    size_t pkts_tx;

    // Timers
    tsc_t tsc_cur, tsc_prev, tsc_next;

    // Stats
    struct stats stats = STATS_INIT;
    stats.type = STATS_TX;
    stats_ptr = &stats;

    struct stats_data_tx stats_period = {0, 0};

    // --------------------------- Initialization --------------------------- //

    // Setup packet headers
    dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, conf);

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
        if (tsc_cur > tsc_next)
        {
            tsc_next += tsc_incr;

            for (pkts_prepared = 0; pkts_prepared < conf->bst_size; pkts_prepared++)
            {
                pkt = rte_mbuf_raw_alloc(conf->dpdk.mbufs);

                if (unlikely(pkt == NULL))
                {
                    fprintf(
                        stderr,
                        "WARN: Could not allocate a buffer, using less packets than required burst size!\n");
                    break;
                }

                dpdk_pkt_prepare(pkt, conf, &pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr);

                if (conf->touch_data)
                {
                    dpdk_produce_data_offset(pkt, data_offset, data_len);
                }

                pkts_burst[pkts_prepared] = pkt;
            }

            pkts_tx = rte_eth_tx_burst(conf->dpdk.portid, 0, pkts_burst, pkts_prepared);

            stats_period.tx += pkts_tx;
            stats_period.dropped += pkts_prepared - pkts_tx;

            for (; pkts_tx < pkts_prepared; ++pkts_tx)
            {
                rte_pktmbuf_free(pkts_burst[pkts_tx]);
            }
        }
    }

    __builtin_unreachable();
}

int dpdk_send_body(int argc, char *argv[])
{
    struct config conf = CONFIG_STRUCT_INITIALIZER;

    // Default configuration for local program
    conf.local_port = SEND_PORT;
    conf.remote_port = RECV_PORT;
    conf.socktype = SOCK_NONE;
    conf.direction = DIRECTION_TX_ONLY;
    strcpy(conf.local_ip, SEND_ADDR_IP);
    strcpy(conf.remote_ip, RECV_ADDR_IP);
    strcpy(conf.local_mac, SEND_ADDR_MAC);
    strcpy(conf.remote_mac, RECV_ADDR_MAC);

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
