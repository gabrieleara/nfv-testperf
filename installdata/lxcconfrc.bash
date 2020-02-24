#!/bin/bash

# This file contains the configuration for containers that shall be installed in
# the machine, there is another copy that mirrors this one with different
# MAC/IP addresses, for use between multiple machines.
# Swap them to install on the second machine.

LXC_CONT_NAMES=(    \
    dpdk_c0         \
    dpdk_c1         \
    dpdk_c2         \
    dpdk_c3         \
    dpdk_c4         \
    dpdk_c5         \
    dpdk_c6         \
    dpdk_c7         \
    dpdk_three0     \
    dpdk_three1     \
)

LXC_CONT_CPUS=(         \
    4,6                 \
    8,10                \
    12,14               \
    16,18               \
    5,7                 \
    9,11                \
    13,15               \
    17,19               \
    4,6,8               \
    10,12,14            \
)

LXC_CONT_VFS=(          \
    enp4s2              \
    enp4s2f1            \
    enp4s2f2            \
    enp4s2f3            \
    enp4s2f4            \
    enp4s2f5            \
    enp4s2f6            \
    enp4s2f7            \
    enp4s2              \
    enp4s2f1            \
)

LXC_CONT_MACS=(         \
    02:00:00:00:00:10   \
    02:00:00:00:00:11   \
    02:00:00:00:00:12   \
    02:00:00:00:00:13   \
    02:00:00:00:00:14   \
    02:00:00:00:00:15   \
    02:00:00:00:00:16   \
    02:00:00:00:00:17   \
    02:00:00:00:00:10   \
    02:00:00:00:00:11   \
)

LXC_CONT_IPS=(          \
    10.0.3.10           \
    10.0.3.11           \
    10.0.3.12           \
    10.0.3.13           \
    10.0.3.14           \
    10.0.3.15           \
    10.0.3.16           \
    10.0.3.17           \
    10.0.3.10           \
    10.0.3.11           \
)

LXC_CONT_NMASKS=(       \
    24                  \
    24                  \
    24                  \
    24                  \
    24                  \
    24                  \
    24                  \
    24                  \
    24                  \
    24                  \
)

LXC_CONT_VFDEVS=(       \
    uio0                \
    uio1                \
    uio2                \
    uio3                \
    uio4                \
    uio5                \
    uio6                \
    uio7                \
    uio0                \
    uio1                \
)

LXC_CONT_VFPCIS=(       \
    04:02.0             \
    04:02.1             \
    04:02.2             \
    04:02.3             \
    04:02.4             \
    04:02.5             \
    04:02.6             \
    04:02.7             \
    04:02.0             \
    04:02.1             \
)

LXC_CONT_SOCKS=(        \
    sock0               \
    sock1               \
    sock2               \
    sock3               \
    sock4               \
    sock5               \
    sock6               \
    sock7               \
    sock0               \
    sock1               \
)
