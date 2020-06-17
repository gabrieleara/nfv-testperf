#ifndef CONSTANTS_H
#define CONSTANTS_H

/* -------------------------------- Includes -------------------------------- */

#include "timestamp.h"

#include <rte_ether.h>
#include <rte_udp.h>
#include <rte_ip.h>

/* ------------------------ Default Parameter Values ------------------------ */

#define DEFAULT_RATE 10000000 /* Default Transmission Rate [pps]] */
#define DEFAULT_PKT_SIZE 64   /* Default packet size [bytes] */
#define DEFAULT_BST_SIZE 32   /* Default burst size [# of packkets] */

#define MIN_PKT_SIZE 64   /* Minimum acceptable packet size [bytes] */
#define MAX_PKT_SIZE 1500 /* Maximum acceptable packet size [bytes] */

/* TODO: move port numbers somewhere else */
#define SEND_PORT 13994
#define RECV_PORT 16994
#define SERVER_PORT 16196
#define CLIENT_PORT 16803

/* TODO: move addresses somewhere else */
#define SEND_ADDR_IP "127.0.0.1"
#define RECV_ADDR_IP "127.0.0.1"
#define SERVER_ADDR_IP "127.0.0.1"
#define CLIENT_ADDR_IP "127.0.0.1"

#define SEND_ADDR_MAC "02:00:00:00:00:02"
#define RECV_ADDR_MAC "02:00:00:00:00:01"
#define SERVER_ADDR_MAC "02:00:00:00:00:02"
#define CLIENT_ADDR_MAC "02:00:00:00:00:01"

/* ----------------------- Default IPv4 Packet Values ----------------------- */

#define IP_DEFAULT_TTL 64                              /* Default TTL [from RFC 1340] */
#define IP_VERSION 0x40                                /* IPv4 */
                                                       // FIXME: check the actual length of the structure
#define IP_HEADER_LEN 0x05                             /* Default IP header length == five 32-bits words. */
#define IP_VERSION_HDRLEN (IP_VERSION | IP_HEADER_LEN) /* Actual field of the IPv4 header */

/* ------------------------ Default Packet Structure ------------------------ */

#define OFFSET_PAYLOAD_TIMESTAMP (0)
#define OFFSET_PAYLOAD_DATA (OFFSET_PAYLOAD_TIMESTAMP + sizeof(tsc_t))

#define OFFSET_PKT_ETHER (0)
#define OFFSET_PKT_IPV4 (OFFSET_PKT_ETHER + sizeof(struct rte_ether_hdr))
#define OFFSET_PKT_UDP (OFFSET_PKT_IPV4 + sizeof(struct rte_ipv4_hdr))
#define OFFSET_PKT_PAYLOAD (OFFSET_PKT_UDP + sizeof(struct rte_udp_hdr))
#define OFFSET_PKT_TIMESTAMP (OFFSET_PKT_PAYLOAD + OFFSET_PAYLOAD_TIMESTAMP)
#define OFFSET_PKT_DATA (OFFSET_PKT_PAYLOAD + OFFSET_PAYLOAD_DATA)

#define PKT_HEADER_SIZE (OFFSET_PKT_PAYLOAD - OFFSET_PKT_ETHER)

#endif /* CONSTANTS_H */
