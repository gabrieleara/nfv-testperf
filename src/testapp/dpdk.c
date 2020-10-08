//#include "dpdk.h"

#include <rte_malloc.h>
#include "timestamp.h"

#include "newcommon.h"

#include <rte_ether.h>

#define PRINT_DPDK_ERROR(str, ...) fprintf(stderr, "DPDK ERROR: " str, __VA_ARGS__)

static const struct rte_eth_conf PORT_CONF_INIT = {
    .rxmode = {
        .split_hdr_size = 0,
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    },
};

static inline uint16_t dpdk_calc_ipv4_checksum(struct rte_ipv4_hdr *ip_hdr) {
    /*
    uint16_t *ptr16 = (unaligned_uint16_t *)ip_hdr;
    uint32_t ip_cksum;

    ptr16 = (unaligned_uint16_t *)ip_hdr;
    ip_cksum = 0;
    ip_cksum += ptr16[0];
    ip_cksum += ptr16[1];
    ip_cksum += ptr16[2];
    ip_cksum += ptr16[3];
    ip_cksum += ptr16[4];
    ip_cksum += ptr16[6];
    ip_cksum += ptr16[7];
    ip_cksum += ptr16[8];
    ip_cksum += ptr16[9];

    // Reduce 32 bit checksum to 16 bits and complement it
    ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
               (ip_cksum & 0x0000FFFF);
    if (ip_cksum > 65535)
        ip_cksum -= 65535;
    ip_cksum = (~ip_cksum) & 0x0000FFFF;
    if (ip_cksum == 0)
        ip_cksum = 0xFFFF;

    return (uint16_t)ip_cksum;
    */
    (void)(ip_hdr);
    return 0;
}

// Setting up ETH, IP and UDP headers for later use
void dpdk_setup_pkt_headers(struct rte_ether_hdr *eth_hdr,
                            struct rte_ipv4_hdr *ip_hdr,
                            struct rte_udp_hdr *udp_hdr,
                            struct config *conf)
{
    uint16_t pkt_len;
    uint16_t payload_len =
        (uint16_t)(conf->pkt_size -
                   (sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) +
                    sizeof(struct rte_udp_hdr)));

    // Initialize ETH header
    /*
    rte_ether_addr_copy((struct rte_ether_addr *)conf->local_addr.mac.sll_addr,
                        &eth_hdr->s_addr);
    rte_ether_addr_copy((struct rte_ether_addr *)conf->remote_addr.mac.sll_addr,
                        &eth_hdr->d_addr);
    */
    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

    // Initialize UDP header
    pkt_len = (uint16_t)(payload_len + sizeof(struct rte_udp_hdr));
    udp_hdr->src_port = rte_cpu_to_be_16(conf->local_port);
    udp_hdr->dst_port = rte_cpu_to_be_16(conf->remote_port);
    udp_hdr->dgram_len = rte_cpu_to_be_16(pkt_len);
    udp_hdr->dgram_cksum = 0; /* No UDP checksum. */

    // Initialize IP header
    pkt_len = (uint16_t)(pkt_len + sizeof(struct rte_ipv4_hdr));
    ip_hdr->version_ihl = IP_VERSION_HDRLEN;
    ip_hdr->type_of_service = 0;
    ip_hdr->fragment_offset = 0;
    ip_hdr->time_to_live = IP_DEFAULT_TTL;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->packet_id = 0;
    ip_hdr->total_length = rte_cpu_to_be_16(pkt_len);
    ip_hdr->src_addr = rte_cpu_to_be_32(conf->local_addr.ip.sin_addr.s_addr);
    ip_hdr->dst_addr = rte_cpu_to_be_32(conf->remote_addr.ip.sin_addr.s_addr);

    // Compute IP header checksum
    ip_hdr->hdr_checksum = dpdk_calc_ipv4_checksum(ip_hdr);
}

// This function shall be called after parameters_parse in this case, and its
// arguments shall be separated by a --
int dpdk_init(int argc, char *argv[], struct config *conf)
{
    uint_t ports;   // number of ports available, error if this is different than 1
    uint_t n_mbufs; // number of mbufs to create in a pool
    uint_t port_id;

    uint16_t tx_ring_descriptors, rx_ring_descriptors;

    switch (conf->direction)
    {
    case DIRECTION_TXRX:
        tx_ring_descriptors = 2048;
        rx_ring_descriptors = 2048;
        break;
    case DIRECTION_TX_ONLY:
        tx_ring_descriptors = 4096;
        rx_ring_descriptors = 0;
        break;
    case DIRECTION_RX_ONLY:
        tx_ring_descriptors = 512;
        rx_ring_descriptors = 4096;
        break;
    }

    int res = rte_eal_init(argc, argv);

    if (res < 0)
    {
        PRINT_DPDK_ERROR("Unable to init RTE: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    argc -= res;
    argv += res;

    ports = rte_eth_dev_count_avail();

    if (ports != 1)
    {
        PRINT_DPDK_ERROR("Wrong number of ports, %d != 1.\n", ports);
        return -1;
    }

    n_mbufs = RTE_MAX((rx_ring_descriptors + tx_ring_descriptors + conf->bst_size + 512), 8192U * 2);

    // Set it to an even number (easier to determine cache size)
    if (n_mbufs & 0x01)
        ++n_mbufs;

    // FINDING THE BIGGEST DIVISOR UNDER CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE
    uint_t s_divisor = 1;
    uint_t b_divisor = n_mbufs;
    do
    {
        do
        {
            ++s_divisor;
            // Iterate over odds number only after checking 2
        } while ((n_mbufs % s_divisor != 0) && (!(s_divisor & 0x1) || s_divisor == 2));

        b_divisor = n_mbufs / s_divisor;
    } while (b_divisor > 512); // CONFIG_RTE_MEMPOOL_CACHE_MAX_SIZE=512 in DPDK configuration file
    uint_t cache_size = b_divisor;

    conf->dpdk.mbufs = rte_pktmbuf_pool_create(
        "mbuf_pool",
        n_mbufs,
        cache_size,
        0,
        RTE_MBUF_DEFAULT_BUF_SIZE,
        rte_socket_id());
    if (conf->dpdk.mbufs == NULL)
    {
        PRINT_DPDK_ERROR("Unable to allocate mbufs: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    struct rte_eth_txconf txq_conf;
    struct rte_eth_conf local_port_conf = PORT_CONF_INIT;
    struct rte_eth_dev_info dev_info;

    port_id = 0;
    conf->dpdk.portid = port_id;
    rte_eth_dev_info_get(port_id, &dev_info);

    // If able to offload TX to device, do it FIXME: this sentence
    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
    {
        local_port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    // Configure device
    res = rte_eth_dev_configure(port_id, 1, 1, &local_port_conf);
    if (res < 0)
    {
        PRINT_DPDK_ERROR("Cannot configure device: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    // Adjust number of TX and RX descriptors
    res = rte_eth_dev_adjust_nb_rx_tx_desc(
        port_id, &rx_ring_descriptors, &tx_ring_descriptors);
    if (res < 0)
    {
        PRINT_DPDK_ERROR("Cannot adjust number of descriptors: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    // Save source mac address (the address associated with the port)
    // rte_eth_macaddr_get(port_id, &conf->dpdk.src_mac_addr); // TODO: IF USED, change attribute

    // Configure TX queue
    txq_conf = dev_info.default_txconf;
    txq_conf.offloads = local_port_conf.txmode.offloads;
    res = rte_eth_tx_queue_setup(
        port_id, 0, tx_ring_descriptors,
        rte_eth_dev_socket_id(port_id), &txq_conf);
    if (res < 0)
    {
        PRINT_DPDK_ERROR("Cannot configure TX: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    // Configure RX queue
    res = rte_eth_rx_queue_setup(
        port_id,
        0,
        rx_ring_descriptors,
        rte_eth_dev_socket_id(port_id),
        NULL, conf->dpdk.mbufs);
    if (res < 0)
    {
        PRINT_DPDK_ERROR("Cannot configure RX: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    /*
    // TODO: IF USED
    //struct rte_eth_dev_tx_buffer *tx_buffer;
    // Allocate TX buffer
    tx_buffer = rte_zmalloc_socket(
        "tx_buffer",
        RTE_ETH_TX_BUFFER_SIZE(conf->bst_size),
        0,
        rte_eth_dev_socket_id(port_id));
    if (tx_buffer == NULL)
    {
        PRINT_DPDK_ERROR("Cannot allocate TX buffer: %s.\n", rte_strerror(rte_errno));
        return -1;
    }
    // Then set the maximum number of packets for a single burst
    rte_eth_tx_buffer_init(tx_buffer, conf->bst_size);
    */

    // Start device
    res = rte_eth_dev_start(port_id);
    if (res < 0)
    {
        PRINT_DPDK_ERROR("Cannot start device: %s.\n", rte_strerror(rte_errno));
        return -1;
    }

    // Enable promiscuous mode
    rte_eth_promiscuous_enable(port_id);

    return 0;
}

int dpdk_advertise_host_mac(struct config *conf)
{
    // Header structures
    struct rte_ether_hdr pkt_eth_hdr = {0};
    struct rte_ipv4_hdr pkt_ip_hdr = {0};
    struct rte_udp_hdr pkt_udp_hdr = {0};

    // The message to be sent
    struct rte_mbuf *pkts_burst[1];
    struct rte_mbuf *pkt;

    struct config conf_temp = *conf;

    strcpy(conf_temp.remote_mac, "FF:FF:FF:FF:FF:FF");
    addr_mac_set(&conf_temp.remote_addr.mac, conf_temp.remote_mac, NULL);

    dpdk_setup_pkt_headers(&pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr, &conf_temp);
    pkt = rte_mbuf_raw_alloc(conf->dpdk.mbufs);

    if (unlikely(pkt == NULL))
    {
        // FIXME: PROBABLY FATAL ERROR
        fprintf(
            stderr,
            "WARN: Could not allocate a buffer for advertising message, using less packets than required burst size!\n");
        return EAGAIN;
    }

    dpdk_pkt_prepare(pkt, &conf_temp, &pkt_eth_hdr, &pkt_ip_hdr, &pkt_udp_hdr);
    pkts_burst[0] = pkt;

    // Packet is ready to be sent

    const int maxcount = 10;
    int count = 0;
    size_t pkts_tx;

    do
    {
        pkts_tx = rte_eth_tx_burst(conf->dpdk.portid, 0, pkts_burst, 1);
        ++count;
    } while (count < maxcount && pkts_tx != 1);

    return (count <= maxcount && pkts_tx == 1) ? 0 : 1;
}
