#ifndef CONFIG_H
#define CONFIG_H

/* -------------------------------- Includes -------------------------------- */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <netinet/in.h>
#include <netpacket/packet.h>

// #include "nfv_socket.h"

#ifdef __cplusplus
extern "C" {
#endif

// FIXME: move around stuff
enum nfv_sock_type {
    NFV_SOCK_NONE = 0, /* This is only for error-checking */
    NFV_SOCK_DGRAM = 0x1,
    NFV_SOCK_RAW = 0x2,
    NFV_SOCK_DPDK = 0x4,
};

enum comm_dir {
    DIRECTION_RX = 0x1,
    DIRECTION_TX = 0x02,
    DIRECTION_TXRX = 0x03,
};

#define NFV_SOCK_SIMPLE (NFV_SOCK_DGRAM | NFV_SOCK_RAW)

/* ---------------------------- Type definitions ---------------------------- */

#define RAW_ADDRSTRLEN 18

typedef char
    macaddr_str[18]; /* String type that can contain an Ethernet MAC address */
typedef char ipaddr_str[INET_ADDRSTRLEN]; /* String type that can contain an
                                             IPv4 address */

typedef uint64_t rate_t; /* The type of the packet sending rate */

typedef uint8_t dpdk_port_t; /* The type of the DPDK port descriptor */

typedef uint8_t byte_t; /* A type that can contain a single unsigned raw byte */
// typedef uint64_t tsc_t; /* The type of the timestamp counter */

/* ------------------------- Common Data Structures ------------------------- */

struct portaddr {
    struct sockaddr_in ip; // NOTICE: the port number is in this structure
    struct sockaddr_ll mac;
};

struct dpdk_conf {
    /* NOTE: always zero */
    dpdk_port_t portid;
    enum comm_dir direction; /* Indicates whether this application will only
                                send, only receive, or both */
    struct rte_mempool
        *mbufs; /* Pointer to the mempool to take DPDK buffers from */
};

// enum nfv_sock_type
// {
//     NFV_SOCK_NONE = 0, /* This is only for error-checking */
//     NFV_SOCK_DGRAM = 0x1,
//     NFV_SOCK_RAW = 0x2,
//     NFV_SOCK_DPDK = 0x4,
// };

/* ------------------- Configuration Structure Definition ------------------- */

struct config {
    rate_t rate;         /* Desired packet rate [pps] */
    size_t pkt_size;     /* Packet size [bytes] */
    size_t payload_size; /* Payload size [bytes] */
    size_t bst_size;     /* Burst size [packets] */

    bool use_block; /* Whether the sockets shall be configured to be blocking or
                       non-blocking [system socket only] */
    bool use_mmsg;  /* Whether the *mmsg variants of kernel socket system calls
                       shall be used [system socket only] */

    bool silent; /* Whether the application should print periodically data to
                    standard output */
    bool touch_data; /* Whether the application should produce/consume each byte
                        of the packet payload */

    struct portaddr local;  /* The addresses (IP and MAC) and UDP port number
                               assigned to this application */
    struct portaddr remote; /* The addresses (IP and MAC) and UDP port number
                               assigned to the remote application (if any) */

    enum nfv_sock_type
        sock_type; /* The type of NFV socket requested by the application */

    /* ------------ NFV sockets custom configuration parameters ------------- */

    int sock_fd; /* Socket file descriptor (NFC_SOCK_DGRAM or NFV_SOCK_RAW only)
                  */

    char local_interf[16]; /* The name of the local interface to be used
                              (NFV_SOCK_RAW only) */

    struct dpdk_conf
        dpdk; /* DPDK-related configuration only (NFC_SOCK_DPDK only) */
};

struct config_defaults_triple {
    macaddr_str mac;
    ipaddr_str ip;
    int port_number;
};

/**
 * I dont really like this solution, but it's better than
 * nothing...
 *  */
struct config_defaults {
    struct config_defaults_triple local;
    struct config_defaults_triple remote;
};

/* ---------------------------- Public Functions ---------------------------- */

extern void config_initialize(struct config *conf,
                              const struct config_defaults *defaults);
extern int config_parse_arguments(struct config *conf, int argc, char *argv[]);
extern int config_initialize_socket(struct config *conf, int argc,
                                    char *argv[]);
extern void config_print(struct config *conf);

#define PKT_SIZE_TO_PAYLOAD(pkt_size) (pkt_size - PKT_HEADER_SIZE)

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* CONFIG_H */
