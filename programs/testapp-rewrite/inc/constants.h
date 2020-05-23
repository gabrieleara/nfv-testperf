#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

#include <rte_ether.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_ethdev.h>

#include "timestamp.h"

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
