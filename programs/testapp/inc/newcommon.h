#ifndef NEWCOMMON_H
#define NEWCOMMON_H

/* ******************** INCLUDES ******************** */

//#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

#include <rte_ether.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_ethdev.h>
// #include <rte_ethdev.h>
// #include <rte_mempool.h>

// #include "timestamp.h"

/* ******************** TYPEDEFS ******************** */

typedef char macaddr_str[18];
typedef char ipaddr_str[16];

typedef uint64_t rate_t;
typedef unsigned int uint_t;

typedef uint8_t byte_t;
typedef uint64_t tsc_t;

/* ********************* DEFINES ********************* */

#define DEFAULT_RATE 10000000
#define DEFAULT_PKT_SIZE 64
#define DEFAULT_BST_SIZE 32

#define MAX_FRAME_SIZE 1500
#define MIN_FRAME_SIZE 64

#define SEND_PORT 13994
#define RECV_PORT 16994
#define SERVER_PORT 16196
#define CLIENT_PORT 16803

#define SEND_ADDR_IP "127.0.0.1"
#define RECV_ADDR_IP "127.0.0.1"
#define SERVER_ADDR_IP "127.0.0.1"
#define CLIENT_ADDR_IP "127.0.0.1"

#define SEND_ADDR_MAC "02:00:00:00:00:02"
#define RECV_ADDR_MAC "02:00:00:00:00:01"
#define SERVER_ADDR_MAC "02:00:00:00:00:02"
#define CLIENT_ADDR_MAC "02:00:00:00:00:01"

#define SOCK_NONE 0

#define IP_DEFAULT_TTL 64 /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HEADER_LEN 0x05 /* default IP header length == five 32-bits words. */ // FIXME: check the actual length of the structure
#define IP_VERSION_HDRLEN (IP_VERSION | IP_HEADER_LEN)

#define OFFSET_ETHER (0)
#define OFFSET_IPV4 (OFFSET_ETHER + sizeof(struct ether_hdr))
#define OFFSET_UDP (OFFSET_IPV4 + sizeof(struct ipv4_hdr))
#define OFFSET_PAYLOAD (OFFSET_UDP + sizeof(struct udp_hdr))
#define OFFSET_TIMESTAMP (OFFSET_PAYLOAD)
#define OFFSET_DATA_SR (OFFSET_PAYLOAD)
#define OFFSET_DATA_CS (OFFSET_PAYLOAD + sizeof(tsc_t))

#define PKT_HEADER_SIZE (OFFSET_PAYLOAD - OFFSET_ETHER)

/* ******************** STRUCTS ******************** */

struct sock_addr_pair
{
    struct sockaddr_in ip;
    struct sockaddr_ll mac;
};

struct dpdk_conf
{
    uint_t portid; // always zero
    struct rte_mempool *mbufs;
};

struct config
{
    rate_t rate;
    uint_t pkt_size;
    uint_t bst_size;

    int sock_fd;
    int socktype;

    int local_port;
    int remote_port;

    struct sock_addr_pair local_addr;
    struct sock_addr_pair remote_addr;

    char local_interf[16];

    macaddr_str local_mac;
    macaddr_str remote_mac;

    ipaddr_str local_ip;
    ipaddr_str remote_ip;

    struct dpdk_conf dpdk;

    bool use_block;
    bool use_mmsg;
    bool silent;
    bool touch_data;

    uint8_t direction;
};

#define DIRECTION_TX_ONLY 0x1
#define DIRECTION_RX_ONLY 0x2
#define DIRECTION_TXRX (DIRECTION_TX_ONLY | DIRECTION_RX_ONLY)

/* ******************** CONSTANTS ******************** */

extern const struct config CONFIG_STRUCT_INITIALIZER;
extern struct stats *stats_ptr; // FIXME: why is this variable here

/* ************** FUNCTION DECLARATIONS ************** */

extern int addr_mac_set(struct sockaddr_ll *addr, const char *mac, const char *ifname);

extern int parameters_parse(int argc, char *argv[], struct config *conf);
extern void print_config(struct config *conf);

extern int dpdk_init(int argc, char *argv[], struct config *conf);
extern void dpdk_setup_pkt_headers(
    struct ether_hdr *eth_hdr,
    struct ipv4_hdr *ip_hdr,
    struct udp_hdr *udp_hdr,
    struct config *conf);

extern int sock_set_sndbuff(int sock_fd, unsigned int size);
extern int sock_create(struct config *conf, uint32_t flags, bool toconnect);
extern void handle_sigint(int sig);

extern int dpdk_advertise_host_mac(struct config *conf);

// TODO: STATIC INLINE DECLARATIONS

/* **************** INLINE FUNCTIONS **************** */

#define dpdk_pkt_offset(pkt, t, offset) \
    rte_pktmbuf_mtod_offset(pkt, t, offset)

static inline void produce_data(byte_t *data, size_t len)
{
    if (len == 0)
        return;

    size_t checksum_index = len - 1;
    byte_t sum = 0;

    for (size_t i = 0; i < checksum_index; ++i)
    {
        // IDEA: produce random data?
        data[i] = i;
        sum += data[i];
    }

    data[checksum_index] = ~sum + 1;
}

static inline void produce_data_offset(void *data, size_t offset, size_t len)
{
    produce_data((byte_t *)data + offset, len);
}

static inline void dpdk_produce_data_offset(struct rte_mbuf *pkt, ssize_t offset, ssize_t len)
{
    produce_data(dpdk_pkt_offset(pkt, byte_t *, offset), len);
}

static inline bool check_checksum(byte_t *data, size_t len)
{
    if (len == 0)
        return true;

    byte_t sum = 0;

    for (size_t i = 0; i < len; ++i)
    {
        sum += data[i];
    }

    return sum == 0;
}

static inline void consume_data(byte_t *data, size_t len)
{
    bool checksum_valid = check_checksum(data, len);

    if (!checksum_valid)
    {
        fprintf(stderr, "ERROR: received message checksum is not correct!\n");
    }
}

static inline void consume_data_offset(void *data, size_t offset, size_t len)
{
    consume_data((byte_t *)data + offset, len);
}

static inline void dpdk_consume_data_offset(struct rte_mbuf *pkt, ssize_t offset, ssize_t len)
{
    consume_data(dpdk_pkt_offset(pkt, byte_t *, offset), len);
}

// Assuming that a packet will always fit into a buffer
static inline void copy_buf_to_pkt(
    void *buf, unsigned len, struct rte_mbuf *pkt, unsigned offset)
{
    rte_memcpy(rte_pktmbuf_mtod_offset(pkt, char *, offset), buf, (size_t)len);
}

static inline void copy_pkt_to_buf(
    void *buf, unsigned len, struct rte_mbuf *pkt, unsigned offset)
{
    rte_memcpy(buf, rte_pktmbuf_mtod_offset(pkt, char *, offset), (size_t)len);
}

static inline void dpdk_pkt_prepare(struct rte_mbuf *pkt,
                                    struct config *conf,
                                    struct ether_hdr *pkt_eth_hdr,
                                    struct ipv4_hdr *pkt_ip_hdr,
                                    struct udp_hdr *pkt_udp_hdr)
{
    rte_pktmbuf_reset_headroom(pkt);
    pkt->data_len = conf->pkt_size;
    pkt->pkt_len = pkt->data_len;
    pkt->next = NULL;

    copy_buf_to_pkt(pkt_eth_hdr,
                    sizeof(struct ether_hdr),
                    pkt,
                    OFFSET_ETHER);
    copy_buf_to_pkt(pkt_ip_hdr,
                    sizeof(struct ipv4_hdr),
                    pkt,
                    OFFSET_IPV4);
    copy_buf_to_pkt(pkt_udp_hdr,
                    sizeof(struct udp_hdr),
                    pkt,
                    OFFSET_UDP);
    pkt->nb_segs = 1;
    pkt->ol_flags = 0;
    pkt->vlan_tci = 0;
    pkt->vlan_tci_outer = 0;
    pkt->l2_len = sizeof(struct ether_hdr);
    pkt->l3_len = sizeof(struct ipv4_hdr);
}

static inline size_t calc_payload_len(size_t pkt_len)
{
    return pkt_len - PKT_HEADER_SIZE;
}

static inline size_t calc_data_len(size_t pkt_len, size_t offset)
{
    return calc_payload_len(pkt_len) - offset;
}

static inline void put_i64(byte_t *data, uint64_t value)
{
    uint64_t value_be = htobe64(value);
    rte_memcpy(data, &value_be, sizeof(uint64_t));
}

static inline uint64_t get_i64(byte_t *data)
{
    uint64_t value_be;
    rte_memcpy(&value_be, data, sizeof(uint64_t));

    return be64toh(value_be);
}

static inline void put_i64_offset(void *data, size_t offset, uint64_t value)
{
    put_i64((byte_t *)data + offset, value);
}

static inline uint64_t get_i64_offset(void *data, size_t offset)
{
    return get_i64((byte_t *)data + offset);
}

static inline void dpdk_put_i64_offset(struct rte_mbuf *pkt, size_t offset, uint64_t value)
{
    put_i64(dpdk_pkt_offset(pkt, byte_t *, offset), value);
}

static inline uint64_t dpdk_get_i64_offset(struct rte_mbuf *pkt, size_t offset)
{
    return get_i64(dpdk_pkt_offset(pkt, byte_t *, offset));
}

// Swap the memory areas pointed by p1 and p2, using the area pointed by tmp as
// support memory.
// All the pointers must point to an area of at least size bytes.
static inline void swap_ptrs(void *p1, void *p2, void *tmp, size_t size)
{
    rte_memcpy(tmp, p1, size);
    rte_memcpy(p1, p2, size);
    rte_memcpy(p2, tmp, size);
}

// Swap source with destination mac addresses
static inline void swap_ether_addr(struct ether_hdr *ether_hdr_ptr)
{
    struct ether_addr support;

    swap_ptrs(
        &ether_hdr_ptr->s_addr,
        &ether_hdr_ptr->d_addr,
        &support,
        sizeof(struct ether_addr));
}

static inline void swap_ipv4_addr(struct ipv4_hdr *ipv4_hdr_ptr)
{
    uint32_t support;

    swap_ptrs(
        &ipv4_hdr_ptr->src_addr,
        &ipv4_hdr_ptr->dst_addr,
        &support,
        sizeof(uint32_t));
}

static inline void swap_udp_port(struct udp_hdr *udp_hdr_ptr)
{
    uint16_t support;

    swap_ptrs(
        &udp_hdr_ptr->src_port,
        &udp_hdr_ptr->dst_port,
        &support,
        sizeof(uint16_t));
}

static inline void dpdk_swap_ether_addr(struct rte_mbuf *pkt)
{
    swap_ether_addr(dpdk_pkt_offset(pkt, struct ether_hdr *, OFFSET_ETHER));
}

static inline void dpdk_swap_ipv4_addr(struct rte_mbuf *pkt)
{
    swap_ipv4_addr(dpdk_pkt_offset(pkt, struct ipv4_hdr *, OFFSET_IPV4));
}

static inline void dpdk_swap_udp_port(struct rte_mbuf *pkt)
{
    swap_udp_port(dpdk_pkt_offset(pkt, struct udp_hdr *, OFFSET_UDP));
}

static inline uint16_t ipv4_hdr_checksum(struct ipv4_hdr *ip_hdr)
{
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
}

static inline bool check_dst_addr_port(const byte_t *frame, const struct config *conf)
{
    const struct ether_hdr *ether_hdr = (struct ether_hdr *)(frame + OFFSET_ETHER);
    const struct ipv4_hdr *ipv4_hdr = (struct ipv4_hdr *)(frame + OFFSET_IPV4);
    const struct udp_hdr *udp_hdr = (struct udp_hdr *)(frame + OFFSET_UDP);

    const byte_t *dest_mac = (byte_t *)&ether_hdr->d_addr;
    const byte_t *local_mac = (byte_t *)&conf->local_addr.mac.sll_addr;

    /*
    printf("DEBUG: PRINTING WHOLE FRAME CONTENT:\n----------------------------------------------------------\n");
    for (size_t i = 0; i < conf->pkt_size; ++i)
    {
        printf("%02X", frame[i]);
    }

    printf("\n----------------------------------------------------------\n");

    printf("DEST MAC: ");
    for (int i = 0; i < 6; ++i)
    {
        printf("%02x", dest_mac[i]);
    }

    printf("\t EXPECTED: ");
    for (int i = 0; i < 6; ++i)
    {
        printf("%02x", local_mac[i]);
    }

    printf("\n");
    */

    for (int i = 0; i < 6; ++i)
    {
        if (dest_mac[i] != local_mac[i])
        {
            return false;
        }
    }

    // DEST MAC ADDRESS IS RIGHT, GOTTA CHECK IP ADDRESS
    const uint32_t dest_ip = ntohl(ipv4_hdr->dst_addr);
    const uint32_t local_ip = conf->local_addr.ip.sin_addr.s_addr;

    //printf("DEST IP: %08x\t EXPECTED: %08x \n", dest_ip, local_ip);

    if (dest_ip != local_ip)
    {
        return false;
    }

    // DEST IP ADDRESS IS RIGHT, GOTTA CHECK UDP PORT NUMBER
    const uint16_t dest_port = ntohs(udp_hdr->dst_port);
    const uint16_t local_port = conf->local_port;

    if (dest_port != local_port)
    {
        return false;
    }

    return true;
}

static inline bool dpdk_check_dst_addr_port(struct rte_mbuf *pkt, struct config *conf)
{
    return check_dst_addr_port(dpdk_pkt_offset(pkt, byte_t *, 0), conf);
}

#endif // NEWCOMMON_H