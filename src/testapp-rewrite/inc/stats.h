#ifndef STATS_H
#define STATS_H

/* ******************** INCLUDES ******************** */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "timestamp.h"
#include <rte_memory.h>

/* ******************** STRUCTS ******************** */

#ifdef __cplusplus
extern "C" {
#endif

struct stats_data_tx {
    uint64_t tx;
    uint64_t dropped;
} __rte_cache_aligned;

struct stats_data_rx {
    uint64_t rx;
} __rte_cache_aligned;

struct stats_data_delay {
    uint64_t avg;
    uint64_t num;
} __rte_cache_aligned;

union stats_data {
    struct stats_data_tx t;
    struct stats_data_rx r;
    struct stats_data_delay d;
} __rte_cache_aligned;

enum stats_type {
    STATS_TX,
    STATS_RX,
    STATS_DELAY,
};

struct stats {
    enum stats_type type;
    uint8_t first;
    uint8_t last;
    uint8_t count;
    union stats_data data[64];
};

/* **************** INLINE FUNCTIONS **************** */

static inline void stats_print(enum stats_type t, union stats_data *d) {
    switch (t) {
    case STATS_TX:
        printf("Tx-pps: %lu %lu %lu\n", d->t.tx, d->t.dropped,
               d->t.tx + d->t.dropped);
        return;
    case STATS_RX:
        printf("Rx-pps: %lu\n", d->r.rx);
        return;
    case STATS_DELAY:
        printf("Avg delay (us) and rx count: %f %lu\n",
               ((double)d->d.avg) / ((double)tsc_get_hz()) * 1000000.,
               d->d.num);
        return;
    }
}

/* ******************** FUNCTIONS ******************** */

extern void stats_save(struct stats *s, union stats_data *d);

extern void stats_print_all(struct stats *s);

/* ******************** CONSTANTS ******************** */

extern const struct stats STATS_INIT;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // STATS_H
