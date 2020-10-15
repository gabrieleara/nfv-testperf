/* -------------------------------- Includes -------------------------------- */

#include <stdint.h>

#include <rte_eal.h>
#include <rte_ethdev.h>

#include <rte_errno.h>

#include "config.h"
#include "dpdk.h"

typedef unsigned int uint_t;

/* ------------------------------- Constants -------------------------------- */

static const struct rte_eth_conf PORT_CONF_INIT = {
    .rxmode =
        {
            .split_hdr_size = 0,
        },
    .txmode =
        {
            .mq_mode = ETH_MQ_TX_NONE,
        },
};

#define PRINT_DPDK_ERROR(str, ...)                                             \
    fprintf(stderr, "DPDK ERROR: " str, __VA_ARGS__)

/* --------------------------- Private Functions ---------------------------- */

/**
 * Get the appropriate amount of cache size for the given number of buffers.
 *
 * The cache size must be:
 * \li a divisor of the number of buffers;
 * \li smaller than CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE.
 *
 * \param n_mbufs is the number of buffers.
 *
 * \return the size of the cache.
 * */
static inline int get_cache_size(uint_t n_mbufs)
{
    /*
     * Idea behind this loop: the biggest divisor is equal to N / the smallest
     * divisor. However, the biggest divisor may be very big, so we keep
     * iterating until we find the biggest divisor that is also smaller than
     * CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE.
     * */
    uint_t s_divisor = 1;
    uint_t b_divisor = n_mbufs;
    do {
        do {
            ++s_divisor;
            /* Iterate over odds number only after checking 2 */
        } while ((n_mbufs % s_divisor != 0) &&
                 (!(s_divisor & 0x1) || s_divisor == 2));

        b_divisor = n_mbufs / s_divisor;
    } while (b_divisor > 512);
    /* CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE=512 [from DPDK configuration file] */

    return b_divisor;
}

/* ---------------------------- Public Functions ---------------------------- */

/**
 * Initialize DPDK environment with the appropriate parameters.
 *
 * This function should be invoked AFTER invoking the
 * config_parse_application_parameters function.
 *
 * This means that it will operate on the parameters of the application starting
 * from the one equal to "--".
 *
 * \return 0 on success, an error code otherwise.
 * */
int dpdk_init(int argc, char *argv[], struct config *conf) {
    uint_t ports;   /* Number of ports available, must be equal to 1 for this
                       application if DPDK is used. */
    uint_t n_mbufs; /* Number of mbufs to create in a pool. */
    uint_t port_id; /* The id of the DPDK port to be used. */

    uint16_t tx_ring_descriptors, rx_ring_descriptors;

    // FIXME: arbitrary numbers
    switch (conf->dpdk.direction) {
    case DIRECTION_TXRX:
        tx_ring_descriptors = 2048;
        rx_ring_descriptors = 2048;
        break;
    case DIRECTION_TX:
        tx_ring_descriptors = 4096;
        rx_ring_descriptors = 0;
        break;
    case DIRECTION_RX:
        tx_ring_descriptors = 512;
        rx_ring_descriptors = 4096;
        break;
    }

    /* Give application parameters to DPDK initialization function */
    int res = rte_eal_init(argc, argv);

    if (res < 0) {
        PRINT_DPDK_ERROR("Unable to init RTE: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    /* Never used after this, useless operation */
    /* argc -= res; */
    /* argv += res; */

    /* Get the number of DPDK ports available */
    ports = rte_eth_dev_count_avail();
    if (ports != 1) {
        PRINT_DPDK_ERROR("Wrong number of ports, %d != 1.\n", ports);
        return -1;
    }

    /* Get the number of desired buffers and descriptors */
    n_mbufs = RTE_MAX(
        (rx_ring_descriptors + tx_ring_descriptors + conf->bst_size + 512),
        8192U * 2);

    /* Set it to an even number (easier to determine cache size) */
    if (n_mbufs & 0x01)
        ++n_mbufs;

    /* Create the appropriate pool of buffers in hugepages memory. */
    conf->dpdk.mbufs =
        rte_pktmbuf_pool_create("mbuf_pool", n_mbufs, get_cache_size(n_mbufs),
                                0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (conf->dpdk.mbufs == NULL) {
        PRINT_DPDK_ERROR("Unable to allocate mbufs: %s.\n",
                         rte_strerror(rte_errno));
        return -1;
    }

    struct rte_eth_txconf txq_conf;
    struct rte_eth_conf local_port_conf = PORT_CONF_INIT;
    struct rte_eth_dev_info dev_info;

    /* Since we checked that there must be only one port, its port id is 0. */
    port_id = 0;
    conf->dpdk.portid = port_id;
    rte_eth_dev_info_get(port_id, &dev_info);

    /* If able to offload TX to device, do it */
    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE) {
        local_port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    /* Configure device */
    res = rte_eth_dev_configure(port_id, 1, 1, &local_port_conf);
    if (res < 0) {
        PRINT_DPDK_ERROR("Cannot configure device: %s.\n",
                         rte_strerror(rte_errno));
        return -1;
    }

    /* Adjust number of TX and RX descriptors */
    res = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &rx_ring_descriptors,
                                           &tx_ring_descriptors);
    if (res < 0) {
        PRINT_DPDK_ERROR("Cannot adjust number of descriptors: %s.\n",
                         rte_strerror(rte_errno));
        return -1;
    }

    /* Get the source mac address that is associated with the given port */
    // FIXME: NOT USED, USER MUST CONFIGURE THE MAC ADDRESS MANUALLY FROM
    // COMMAND LINE
    /* rte_eth_macaddr_get(port_id, &conf->dpdk.src_mac_addr); */

    /* Configure TX queue */
    txq_conf = dev_info.default_txconf;
    txq_conf.offloads = local_port_conf.txmode.offloads;
    res = rte_eth_tx_queue_setup(port_id, 0, tx_ring_descriptors,
                                 rte_eth_dev_socket_id(port_id), &txq_conf);
    if (res < 0) {
        PRINT_DPDK_ERROR("Cannot configure TX: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    /* Configure RX queue */
    res = rte_eth_rx_queue_setup(port_id, 0, rx_ring_descriptors,
                                 rte_eth_dev_socket_id(port_id), NULL,
                                 conf->dpdk.mbufs);
    if (res < 0) {
        PRINT_DPDK_ERROR("Cannot configure RX: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    /* Bring the device up */
    res = rte_eth_dev_start(port_id);
    if (res < 0) {
        PRINT_DPDK_ERROR("Cannot start device: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    /* Enable promiscuous mode */
    /* NOTICE: The device will show packets that are not meant for the
     * device MAC address too.
     * */
    rte_eth_promiscuous_enable(port_id);

    return 0;
}

/**
 * For receiver-only applications. From time to time, this function can be
 * invoked to send an empty frame to the device.
 *
 * Use this if virtual switches fail to locate the receiver application.
 *
 * \return 0 on success, an error code otherwise.
 * */
/*
 // FIXME: re-implement this in appropriate loops!
int dpdk_advertise_host_mac(struct config *conf) {
    // Header structures
    struct rte_ether_hdr pkt_eth_hdr;
    struct rte_ipv4_hdr pkt_ip_hdr;
    struct rte_udp_hdr pkt_udp_hdr;

    // The message to be sent
    struct rte_mbuf *pkts_burst[1];
    struct rte_mbuf *pkt;

    struct config conf_temp = *conf;

    struct sockaddr_ll mac_addr;
    addr_mac_set(mac_addr, "FF:FF:FF:FF:FF:FF", NULL);

    // FIXME: this function should be rewritten to accept a structure
    // sockaddr_ll and a structure sockaddr_in. Or a pair of the two.
    dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, &conf_temp);
    pkt = rte_mbuf_raw_alloc(conf->dpdk.mbufs);
    if (unlikely(pkt == NULL)) {
        // FIXME: PROBABLY FATAL ERROR
        fprintf(stderr,
                "WARN: Could not allocate a buffer for advertising message!\n");
        return EAGAIN;
    }

    dpdk_pkt_prepare(pkt, &conf_temp, &pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr);
    pkts_burst[0] = pkt;

    // Packet is ready to be sent
    const int maxcount = 10;
    int count = 0;
    size_t pkts_tx;

    // Try sending packet at most maxcount times
    do {
        pkts_tx = rte_eth_tx_burst(conf->dpdk.portid, 0, pkts_burst, 1);
        ++count;
    } while (count < maxcount && pkts_tx != 1);

    return (count <= maxcount && pkts_tx == 1) ? 0 : 1;
}
*/
