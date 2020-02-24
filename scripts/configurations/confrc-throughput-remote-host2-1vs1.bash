#!/bin/bash

LXC_CONT_NAMES=(        \
    dpdk_c0             \
)

LXC_CONT_VFS=(          \
    enp4s2              \
)

LXC_CONT_MACS=(         \
    02:00:00:00:00:20   \
)

LXC_CONT_IPS=(          \
    10.0.3.20           \
)

LXC_CONT_NMASKS=(       \
    24                  \
)

LXC_CONT_VFDEVS=(       \
    uio0                \
)

LXC_CONT_VFPCIS=(       \
    04:02.0             \
)

LXC_CONT_SOCKS=(        \
    sock0               \
)

LXC_CONT_OTHER_IPS=(    \
    10.0.3.10           \
)

LXC_CONT_OTHER_MACS=(   \
    02:00:00:00:00:10   \
)

# Can be either send, recv, client, clientst, server
LXC_CONT_CMDNAMES=(     \
    recv                \
)