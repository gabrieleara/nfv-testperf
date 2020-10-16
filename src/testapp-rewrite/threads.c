#include "threads.h"

int thread_starter(void *arg) {
    struct thread_info *tinfo = (struct thread_info *)arg;
    int res = cores_setaffinity(tinfo->core_id);
    if (res)
        return -1;

    return tinfo->tbody(tinfo->arg);
}
