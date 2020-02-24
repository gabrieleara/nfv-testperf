#!/bin/bash

LXC_CONT_NAMES=(        \
    dpdk_c0             \
    dpdk_c1             \
)

LXC_CONT_VFS=(          \
    enp4s2              \
    enp4s2f1            \
)

LXC_CONT_MACS=(         \
    02:00:00:00:00:10   \
    02:00:00:00:00:11   \
)

LXC_CONT_IPS=(          \
    10.0.3.10           \
    10.0.3.11           \
)

LXC_CONT_NMASKS=(       \
    24                  \
    24                  \
)

LXC_CONT_VFDEVS=(       \
    uio0                \
    uio1                \
)

LXC_CONT_VFPCIS=(       \
    04:02.0             \
    04:02.1             \
)

LXC_CONT_SOCKS=(        \
    sock0               \
    sock1               \
)

LXC_CONT_OTHER_IPS=(    \
    10.0.3.11           \
    10.0.3.10           \
)

LXC_CONT_OTHER_MACS=(   \
    02:00:00:00:00:11   \
    02:00:00:00:00:10   \
)

# Can be either send, recv, client, clientst, server
LXC_CONT_CMDNAMES=(     \
    clientst            \
    server              \
)