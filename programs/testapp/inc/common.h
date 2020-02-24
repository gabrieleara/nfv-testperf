#ifndef COMMON_H
#define COMMON_H

// ------------------------------ UNUSED HEADER FILE ---------------------------

/* ******************** INCLUDES ******************** */

#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
// #include <net/ethernet.h>

/* ******************** TYPEDEFS ******************** */

typedef char macaddr_str[18];
typedef char ipaddr_str[16];

typedef uint64_t rate_t;
typedef unsigned int uint_t;

typedef uint8_t byte_t;

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

#define SOCK_NONE 0

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
};

/* **************** INLINE FUNCTIONS **************** */

static inline void produce_data(ssize_t payload_size, void *payload_p)
{
    ssize_t checksum_index = payload_size - 1;

    uint8_t *payload = (uint8_t *)payload_p;
    uint8_t sum = 0;

    for (ssize_t i = 0; i < checksum_index; ++i)
    {
        // IDEA: produce random data?
        payload[i] = i;
        sum += payload[i];
    }

    payload[checksum_index] = ~sum + 1;
}

static inline bool check_checksum(ssize_t payload_size, void *payload_p)
{
    uint8_t *payload = (uint8_t *)payload_p;
    uint8_t sum = 0;

    for (ssize_t i = 0; i < payload_size; ++i)
    {
        sum += payload[i];
    }

    return sum == 0;
}

static inline void consume_data(ssize_t payload_size, void *payload_p)
{
    bool checksum_valid = check_checksum(payload_size, payload_p);

    if (!checksum_valid)
    {
        fprintf(stderr, "ERROR: received message checksum is not correct!\n");
    }
}

/* ******************** FUNCTIONS ******************** */

extern int parameters_parse(int argc, char *argv[], struct config *conf);
extern void print_config(struct config *conf);

extern int sock_set_sndbuff(int sock_fd, unsigned int size);

extern int sock_create(struct config *conf, uint32_t flags, bool toconnect);

extern void handle_sigint(int sig);

/* ********************  DEFINES  ******************** */

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

/* ******************** CONSTANTS ******************** */

extern const struct config CONFIG_STRUCT_INITIALIZER;
extern struct stats *stats_ptr;

#endif // COMMON_H
