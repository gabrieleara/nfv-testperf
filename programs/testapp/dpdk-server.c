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

#include "timestamp.h"

#include <rte_ethdev.h>

#include "newcommon.h"
#include "stats.h"

// main loop function shall not return because the signal handler for SIGINT
// terminates the program no-gracefully
static inline void main_loop(struct config *conf) __attribute__((noreturn));

#define ever \
    ;        \
    ;

static inline void main_loop(struct config *conf)
{
    // ----------------------------- Constants ------------------------------ //

    const size_t data_offset = OFFSET_DATA_CS;
    const size_t data_len = conf->pkt_size - data_offset;

    // ------------------- Variables and data structures -------------------- //

    // Set of messages to be send/received
    struct rte_mbuf *pkts_burst[conf->bst_size];
    struct rte_mbuf *pkt;

    // Num packets received
    size_t pkts_rx;
    size_t pkts_tx;
    size_t pkts_actually_rx;

    // --------------------------- Initialization --------------------------- //

    // ------------------ Infinite loop variables and body ------------------ //

    // Pointer to the IPv4 header in current packet (to re-calculate checksum)
    struct ipv4_hdr *pkt_ip_hdr;

    size_t i;
    size_t j;

    for (ever)
    {
        // This program does not print anything ever (unless certain errors occur)

        pkts_rx = rte_eth_rx_burst(conf->dpdk.portid, 0, pkts_burst, conf->bst_size);

        if (pkts_rx == 0)
            continue;

        // Now we iterate through packets and drop every packet that is not meant
        // for this process, while we send back correct packets (after swapping
        // addresses and ports)
        j = 0;
        for (i = 0; i < pkts_rx; ++i)
        {
            pkt = pkts_burst[i];

            // First we check that the packet is meant for this process
            if (dpdk_check_dst_addr_port(pkt, conf))
            {
                // Consume data if required
                if (conf->touch_data)
                {
                    dpdk_consume_data_offset(pkt, data_offset, data_len);
                }

                /*
                struct ether_hdr *eh;
                byte_t *src_mac;
                byte_t *dst_mac;

                printf("MAC ADDRESSES BEFORE\n");
                eh = dpdk_pkt_offset(pkt, struct ether_hdr *, OFFSET_ETHER);
                src_mac = (byte_t*) &eh->s_addr;
                dst_mac = (byte_t*) &eh->d_addr;
                printf("SRC MAC: ");
                for (int i = 0; i < 6; ++i)
                {
                    printf("%02x", src_mac[i]);
                }
                printf("\t DST MAC: ");
                for (int i = 0; i < 6; ++i)
                {
                    printf("%02x", dst_mac[i]);
                }
                printf("\n");
                */

                // Matches, add to the burst to send
                // But swap info first!
                dpdk_swap_ether_addr(pkt);
                dpdk_swap_ipv4_addr(pkt);
                dpdk_swap_udp_port(pkt);

                /*
                printf("MAC ADDRESSES AFTER\n");
                eh = dpdk_pkt_offset(pkt, struct ether_hdr *, OFFSET_ETHER);
                src_mac = (byte_t*) &eh->s_addr;
                dst_mac = (byte_t*) &eh->d_addr;
                printf("SRC MAC: ");
                for (int i = 0; i < 6; ++i)
                {
                    printf("%02x", src_mac[i]);
                }
                printf("\t DST MAC: ");
                for (int i = 0; i < 6; ++i)
                {
                    printf("%02x", dst_mac[i]);
                }
                printf("\n");
                */

                pkt_ip_hdr = dpdk_pkt_offset(pkt, struct ipv4_hdr *, OFFSET_IPV4);
                pkt_ip_hdr->hdr_checksum = ipv4_hdr_checksum(pkt_ip_hdr);

                // Finally we put it in position j (j <= i always)
                pkts_burst[j] = pkt;
                ++j;
            }
            else
            {
                // Does not match, release the buffer
                rte_pktmbuf_free(pkt); // pkts_burst[i]

                // Do not increase j of course!
            }
        }

        // Now j is the number of packets not freed and that should be sent
        // back; first j positions are occupied by said packets, remaining
        // pointers are invalid (mostly already freed or pointing to same
        // packets pointed by positions lower than j) and must not be used!
        pkts_actually_rx = j;

        // Send back the correct packets
        pkts_tx = rte_eth_tx_burst(conf->dpdk.portid, 0, pkts_burst, pkts_actually_rx);

        // Free any unsent packet among the correct ones
        // The others have already been freed

        for (; pkts_tx < pkts_actually_rx; ++pkts_tx)
        {
            rte_pktmbuf_free(pkts_burst[pkts_tx]);
        }
    }

    __builtin_unreachable();
}

int dpdk_server_body(int argc, char *argv[])
{
    struct config conf = CONFIG_STRUCT_INITIALIZER;

    // Default configuration for local program
    conf.local_port = SERVER_PORT;
    conf.remote_port = CLIENT_PORT;
    conf.socktype = SOCK_NONE;
    strcpy(conf.local_ip, SERVER_ADDR_IP);
    strcpy(conf.remote_ip, CLIENT_ADDR_IP);
    strcpy(conf.local_mac, SERVER_ADDR_MAC);
    strcpy(conf.remote_mac, CLIENT_ADDR_MAC);

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
