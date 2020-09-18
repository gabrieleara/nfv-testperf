#ifndef DPDK_MINE_H
#define DPDK_MINE_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------- FUNCTIONS --------------------------------*/

extern int dpdk_init(int argc, char *argv[], struct config *conf);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DPDK_MINE_H
