#ifndef CORES_H
#define CORES_H

#include <pthread.h>
#include <rte_config.h>
#include "config.h"

#define CORE_MAX RTE_MAX_LCORE

typedef unsigned int core_t;

/**
 * Initializes the cores library.
 *
 * If using DPDK, assumes DPDK is already initialized and with the correct
 * number and list of cores.
 * */
extern void cores_init(struct config *conf);

/**
 * Limits the affinity of the current thread to a single core id.
 * */
extern int cores_setaffinity(core_t core_id);

/**
 * Returns the master core identifier.
 * */
extern core_t cores_get_master(struct config *conf);

/**
 * Returns the number of cores available, including the master core.
 * */
extern core_t cores_count(struct config *conf);

/**
 * Use this to iterate through the list of cores. If possible, prefer the two
 * macros provided by this header file, CORES_FOREACH and CORES_FOREACH_SLAVE.
 * */
extern core_t cores_next(struct config *conf, core_t i,
                         int skip_master, int wrap);

/******************************************************************************/

/**
 * Do not use this directly, prefer the other two macros CORES_FOREACH and
 * CORES_FOREACH_SLAVE.
 * */
#define CORES_FOREACH_(conf, i, skipmaster)       \
    for (i = cores_next(conf, -1, skipmaster, 0); \
         i < CORE_MAX;                            \
         i = cores_next(conf, i, skipmaster, 0))

/**
 * Iterate through the set of available cores, including the master core.
 * */
#define CORES_FOREACH(conf, i) CORES_FOREACH_(conf, i, 0)

/**
 * Iterate through the set of available cores, excluding the master core.
 * */
#define CORES_FOREACH_SLAVE(conf, i) CORES_FOREACH_(conf, i, 1)

#endif // CORES_H
