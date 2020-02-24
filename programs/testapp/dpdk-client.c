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

#include <rte_ethdev.h>

#include "timestamp.h"

#include "newcommon.h"
#include "stats.h"

#define ever \
    ;        \
    ;

#define UNUSED(x) ((void)x)

// main loop function shall not return because the signal handler for SIGINT
// terminates the program non-gracefully
static inline int main_loop(struct config *conf) __attribute__((noreturn));
static inline int main_loop(struct config *conf)
{
    // ----------------------------- Constants ------------------------------ //

    const size_t data_offset = OFFSET_DATA_CS;
    const size_t data_len = conf->pkt_size - data_offset;

    // Timer constants
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_incr = tsc_hz * conf->bst_size / conf->rate;

    // ------------------- Variables and data structures -------------------- //

    // Header structures (same for each message)
    struct ether_hdr pkt_eth_hdr;
    struct ipv4_hdr pkt_ip_hdr;
    struct udp_hdr pkt_udp_hdr;

    // Set of messages to be send/received
    struct rte_mbuf *pkts_burst[conf->bst_size];
    struct rte_mbuf *pkt;

    // Number of packets prepared and sent in current iteration
    size_t pkts_prepared;
    size_t pkts_tx;

    // Timers
    tsc_t tsc_cur, tsc_next;

    // --------------------------- Initialization --------------------------- //

    // Setup packet headers
    dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, conf);

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

            // Prepare each packet in the burst
            for (pkts_prepared = 0; pkts_prepared < conf->bst_size; ++pkts_prepared)
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

                tsc_cur = tsc_get_last();
                dpdk_put_i64_offset(pkt, OFFSET_PAYLOAD, tsc_cur);

                if (conf->touch_data)
                {
                    dpdk_produce_data_offset(pkt, data_offset, data_len);
                }

                pkts_burst[pkts_prepared] = pkt;
            }

            pkts_tx = rte_eth_tx_burst(conf->dpdk.portid, 0, pkts_burst, pkts_prepared);

            // Release all unsent messages
            for (; pkts_tx < pkts_prepared; ++pkts_tx)
            {
                rte_pktmbuf_free(pkts_burst[pkts_tx]);
            }
        }
    }

    __builtin_unreachable();
}

static int pong_body(void *arg) __attribute__((noreturn));
static int pong_body(void *arg)
{
    struct config *conf = (struct config *)arg;

    // ----------------------------- Constants ------------------------------ //

    const size_t data_offset = OFFSET_DATA_CS;
    const size_t data_len = conf->pkt_size - data_offset;

    // Timer constants
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz;

    // ------------------- Variables and data structures -------------------- //

    // Set of messages to be send/received
    struct rte_mbuf *pkts_burst[conf->bst_size];
    struct rte_mbuf *pkt;

    // Num packets received
    size_t pkts_rx;
    size_t i;

    // Timers
    tsc_t tsc_cur, tsc_prev;
    tsc_t tsc_pkt;

    // Stats variables
    struct stats stats = STATS_INIT;
    stats.type = STATS_DELAY;
    stats_ptr = &stats;

    struct stats_data_delay stats_period = {0, 0};

    // --------------------------- Initialization --------------------------- //

    // ------------------ Infinite loop variables and body ------------------ //

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
                // Calculate the actual average.
                // Before this instruction, the avg attribute is actually the
                // sum of delay values.
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

        pkts_rx = rte_eth_rx_burst(conf->dpdk.portid, 0, pkts_burst, conf->bst_size);

        for (i = 0; i < pkts_rx; ++i)
        {
            pkt = pkts_burst[i];

            // First we check whether the packet was meant for this process
            if (dpdk_check_dst_addr_port(pkt, conf))
            {
                // We can count it as a good message
                ++stats_period.num;

                // It was, now we can consume data if required
                if (conf->touch_data)
                {
                    dpdk_consume_data_offset(pkt, data_offset, data_len);
                }

                // Get and compare timestamps
                tsc_pkt = dpdk_get_i64_offset(pkt, OFFSET_TIMESTAMP);
                tsc_cur = tsc_get_last();

                // Add calculated delay to the avg attribute (which is actually
                // the sum of all delays, the actual average is calculated only
                // once per second).
                stats_period.avg += tsc_cur - tsc_pkt;
            }

            // Release the buffer
            rte_pktmbuf_free(pkt);
        }
    }

    __builtin_unreachable();
}

// static int *tsc_body(void *arg) __attribute__((noreturn));
static int tsc_body(void *arg)
{
    UNUSED(arg);

    for (ever)
    {
        tsc_get_update();
    }

    __builtin_unreachable();
}

int dpdk_client_body(int argc, char *argv[])
{
    struct config conf = CONFIG_STRUCT_INITIALIZER;

    // Default configuration for local program
    conf.local_port = CLIENT_PORT;
    conf.remote_port = SERVER_PORT;
    conf.socktype = SOCK_NONE;
    strcpy(conf.local_ip, CLIENT_ADDR_IP);
    strcpy(conf.remote_ip, SERVER_ADDR_IP);
    strcpy(conf.local_mac, CLIENT_ADDR_MAC);
    strcpy(conf.remote_mac, SERVER_ADDR_MAC);

    int argind = parameters_parse(argc, argv, &conf);

    argc -= argind;
    argv += argind;

    int res = dpdk_init(argc, argv, &conf);
    if (res)
        return EXIT_FAILURE;

    print_config(&conf);

    signal(SIGINT, handle_sigint);

    tsc_init();

    // Time to start another thread
    int core_id;

    // Launch on first available slave core
    core_id = rte_get_next_lcore(-1, true, false);
    if (core_id == RTE_MAX_LCORE)
    {
        fprintf(stderr, "NOT ENOUGH RTE CORES! NEEDED: %d\n", 2);
        return EXIT_FAILURE;
    }

    res = rte_eal_remote_launch(tsc_body, NULL, core_id);
    if (res != 0)
    {
        fprintf(stderr, "COULD NOT LAUNCH TSC_BODY THREAD!\n");
        return EXIT_FAILURE;
    }

    sleep(1);

    // Launch on first available slave core
    core_id = rte_get_next_lcore(core_id, true, false);
    if (core_id == RTE_MAX_LCORE)
    {
        fprintf(stderr, "NOT ENOUGH RTE CORES! NEEDED: %d\n", 3);
        return EXIT_FAILURE;
    }

    res = rte_eal_remote_launch(pong_body, (void *)&conf, core_id);
    if (res != 0)
    {
        fprintf(stderr, "COULD NOT LAUNCH PONG_BODY THREAD!\n");
        return EXIT_FAILURE;
    }

    main_loop(&conf);

    // Should never get here actually
    return EXIT_SUCCESS;
}
