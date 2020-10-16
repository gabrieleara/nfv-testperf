#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <rte_cycles.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t tsc_t;

extern volatile tsc_t __tsc_last;

/**
 * If I don't declare this function as extern here I can't see this private
 * DPDK function and to initialize the timer I need to initialize the whole
 * DPDK EAL, which is totally unnecessary in some cases...
 * */
extern int rte_eal_timer_init(void);

/* ------------------------- FUNCTION  DECLARATIONS ------------------------- */

static inline int tsc_init(void)
{
    return rte_eal_timer_init();
}

static inline tsc_t tsc_read(void)
{
    return rte_rdtsc();
}

static inline tsc_t tsc_get_last(void)
{
    return __tsc_last;
}

static inline tsc_t tsc_get_update(void)
{
    return (__tsc_last = tsc_read());
}

static inline tsc_t tsc_get_hz(void)
{
    return rte_get_tsc_hz();
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif
