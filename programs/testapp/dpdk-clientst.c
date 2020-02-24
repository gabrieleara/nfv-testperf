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

    const size_t data_offset = OFFSET_DATA_CS;
    const size_t data_len = conf->pkt_size - data_offset;

    // Timer constants
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz;
    const tsc_t tsc_max_delay = tsc_hz / 100; // ~ 10 milliseconds is definitely a big timeout*[citation needed]

    // ------------------- Variables and data structures -------------------- //

    // Header structures (same for each message)
    struct ether_hdr pkt_eth_hdr;
    struct ipv4_hdr pkt_ip_hdr;
    struct udp_hdr pkt_udp_hdr;

    // Set of messages to be send/received (actually only one)
    struct rte_mbuf *pkts_burst[1];
    struct rte_mbuf *pkt;

    // Number of packets prepared and sent in current iteration
    size_t pkts_tx;
    size_t pkts_rx;

    // Timers
    tsc_t tsc_cur, tsc_next, tsc_prev;
    tsc_t tsc_last, tsc_pkt;

    // Stats variables
    struct stats stats = STATS_INIT;
    stats.type = STATS_DELAY;
    stats_ptr = &stats;

    struct stats_data_delay stats_period = {0, 0};

    // --------------------------- Initialization --------------------------- //

    // Setup packet headers
    dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, conf);

    // ------------------ Infinite loop variables and body ------------------ //

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

        // Prepare a single packet
        do
        {
            pkt = rte_mbuf_raw_alloc(conf->dpdk.mbufs);
        } while (unlikely(pkt == NULL));

        dpdk_pkt_prepare(pkt, conf, &pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr);

        pkts_burst[0] = pkt; // A burst of a single packet

        // Produce data if required
        if (conf->touch_data)
        {
            dpdk_produce_data_offset(pkt, data_offset, data_len);
        }

        // Packet is ready to be sent
        do
        {
            // Put TSC data into packet after headers
            tsc_last = tsc_cur = tsc_get_update();
            dpdk_put_i64_offset(pkt, OFFSET_TIMESTAMP, tsc_cur);

            pkts_tx = rte_eth_tx_burst(conf->dpdk.portid, 0, pkts_burst, 1);
        } while (pkts_tx != 1);

        do
        {
            // Receive a packet
            pkts_rx = rte_eth_rx_burst(conf->dpdk.portid, 0, pkts_burst, 1);
            tsc_cur = tsc_get_update();

            // A packet is considered valid if:
            //  a. it is meant for this process;
            //  b. it has the same timestamp as the last sent packet.
            // Otherwise we reset pkts_rx to zero

            pkt = pkts_burst[0];

            // Performing check a.
            if (pkts_rx == 1 && !dpdk_check_dst_addr_port(pkt, conf))
            {
                // fprintf(stderr, "RECEIVED PACKET BUT HAD WRONG DESTINATION! DROPPED!!\n");

                // Free packet buffer and set back to zero
                rte_pktmbuf_free(pkts_burst[0]);
                pkts_rx = 0;
            }

            // Performing check b.
            if (pkts_rx == 1)
            {
                // fprintf(stderr, "RECEIVED PACKET WITH CORRECT DESTINATION!\n");

                // Consume data if required
                if (conf->touch_data)
                {
                    dpdk_consume_data_offset(pkt, data_offset, data_len);
                }

                // Get timestamp
                tsc_pkt = dpdk_get_i64_offset(pkt, OFFSET_TIMESTAMP);

                // Free packet buffer
                rte_pktmbuf_free(pkt);

                // Check timestamp is the same as the last sent one
                if (tsc_pkt != tsc_last)
                {
                    pkts_rx = 0;
                    fprintf(stderr, "ERR: last timestamp is not the same!\n");

                    // Besides printing this error, there is no need to do anything,
                    // eventually the program re-aligns with itself, even after some time
                }
            }

            // If pkts_rx is still 1 we have a valid packet and
            // tsc_pkt contains its timestamp

            // Exit if the right packet has been received or too much time
            // elapsed
        } while (pkts_rx != 1 && tsc_cur - tsc_last < tsc_max_delay);

        // This is used to check whether we got out for the loop for timeout
        if (tsc_cur - tsc_last < tsc_max_delay)
        {
            stats_period.avg += tsc_cur - tsc_pkt;
            ++stats_period.num;
        }
    }

    __builtin_unreachable();
}

int dpdk_clientst_body(int argc, char *argv[])
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

    main_loop(&conf);

    // Should never get here actually
    // close(conf.sock_fd);

    return EXIT_SUCCESS;
}
