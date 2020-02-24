#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <rte_cycles.h>

typedef uint64_t tsc_t;

extern volatile tsc_t __tsc_last;

// gotta declare this here because otherwise I can't see this function and
// initialize whole EAL which is totally unnecessary
extern int rte_eal_timer_init(void);

static inline int tsc_init()
{
    return rte_eal_timer_init();
}

static inline tsc_t tsc_get_last()
{
    return __tsc_last;
}

static inline tsc_t tsc_get_update()
{
    return (__tsc_last = rte_rdtsc());
}

static inline tsc_t tsc_get_hz()
{
    return rte_get_tsc_hz();
}

#endif
