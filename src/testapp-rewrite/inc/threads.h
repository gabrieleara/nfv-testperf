#ifndef THREADS_H
#define THREADS_H

#include "config.h"
#include "cores.h"
#include <pthread.h>
#include <rte_launch.h>

typedef int (*thread_body_t)(void *);

struct thread_info {
    core_t core_id;
    thread_body_t tbody;
    pthread_t tid;
    void *arg;
};

extern int thread_starter(void *arg);

static inline int thread_start(struct config *conf, struct thread_info *tinfo) {
    if (USE_DPDK(conf))
        return rte_eal_remote_launch(tinfo->tbody, tinfo->arg, tinfo->core_id);
    return pthread_create(&tinfo->tid, NULL, (void *(*)(void *))thread_starter,
                          tinfo);
}

static inline int thread_join(struct config *conf, struct thread_info *tinfo,
                              void **thread_return) {
    if (USE_DPDK(conf)) {
        /* int return_v = */ rte_eal_wait_lcore(tinfo->core_id);
        // FIXME:
        if (thread_return)
            thread_return = NULL;
        return 0;
    }

    return pthread_join(tinfo->tid, thread_return);
}

#endif // THREADS_H
