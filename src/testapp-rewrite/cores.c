#include <rte_lcore.h>
#include "cores.h"

/* ---------------------------- GLOBAL VARIABLES ---------------------------- */

/**
 * Both are used only when USE_DPDK returns false.
 * */
static cpu_set_t cores_set;
static core_t cores_master;

/* --------------------------- UTILITY FUNCTIONS ---------------------------- */

/**
 * Returns the first CPU in the set.
 * */
static inline int cores_first(struct config *conf)
{
    return cores_next(conf, -1, 0, 0);
}

/**
 * Returns the affinity set of the current thread.
 * */
static inline cpu_set_t cores_getaffinity()
{
    cpu_set_t set;
    if (pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &set))
        CPU_ZERO(&set);
    return set;
}

/* ---------------------------- PUBLIC FUNCTIONS ---------------------------- */

void cores_init(struct config *conf)
{
    if (!USE_DPDK(conf))
    {
        cores_set = cores_getaffinity();
        cores_master = cores_first(conf);
    }
}

int cores_setaffinity(core_t core_id)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core_id, &set);

    // Uses pthread_self, assumes to run on master thread
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

core_t cores_get_master(struct config *conf)
{
    if (USE_DPDK(conf))
    {
        return rte_get_master_lcore();
    }
    else
    {
        return cores_master;
    }
}

core_t cores_count(struct config *conf)
{
    if (USE_DPDK(conf))
    {
        return rte_lcore_count();
    }
    else
    {
        return CPU_COUNT(&cores_set);
    }
}

core_t cores_next(struct config *conf, core_t i,
                  int skip_master, int wrap)
{
    if (USE_DPDK(conf))
    {
        return rte_get_next_lcore(i, skip_master, wrap);
    }
    else
    {
        if (CPU_COUNT(&cores_set) < 2)
        {
            if (skip_master)
                return CORE_MAX;

            if (!wrap && i >= cores_master)
                return CORE_MAX;

            return cores_master;
        }

    redo:
        for (++i; !CPU_ISSET(i, &cores_set); ++i)
        {
            if (i >= CORE_MAX)
            {
                if (!wrap)
                    return CORE_MAX;
                i = (core_t)-1;
            }
        }

        if (skip_master && i == cores_master)
            goto redo;

        return i;
    }
}
