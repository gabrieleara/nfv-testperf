#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* -------------------------------- INCLUDES -------------------------------- */

#include <stdbool.h>
#include <signal.h>

#include "timestamp.h"
#include "nfv_socket.h"
#include "loops.h"
#include "stats.h"

/* -------------------------------- DEFINES --------------------------------- */

/* Used to suppress compiler warnings about unused arguments */
#define UNUSED(x) ((void)x)

/* Used to write forever loops */
#define ever \
    ;        \
    ;

/* -------------------------------- GLOBALS --------------------------------- */

struct stats *stats_ptr = NULL;

/* --------------------------- UTILITY  FUNCTIONS --------------------------- */

/* --------------------------- LOOP EXIT FUNCTION --------------------------- */

void handle_sigint(int sig)
{
    printf("\nCaught signal %s!\n", strsignal(sig));

    if (sig == SIGINT)
    {
        if (stats_ptr != NULL)
        {
            // Print average stats
            printf("-------------------------------------\n");
            printf("FINAL STATS\n");

            stats_print_all(stats_ptr);
        }

        exit(EXIT_SUCCESS);
    }
}

/* ----------------------------- LOOP FUNCTIONS ----------------------------- */

/**
 * Infinite loop that updates the global tsc timer.
 */
void tsc_loop(void *arg)
{
    UNUSED(arg);

    for (ever)
    {
        tsc_get_update();
    }

    __builtin_unreachable();
}

/**
 * Infinite loop that sends packets at a constant packet rate, grouping them in
 * bursts.
 *
 * TODO: stats (optional, the client send body does not produce any!)
 */
void send_loop(struct config *conf)
{
    UNUSED(conf);
    for (ever)
        ;

    __builtin_unreachable();
}

/**
 * Infinite loop that receives packets grouping them in bursts.
 */
void recv_loop(struct config *conf)
{
    UNUSED(conf);
    for (ever)
        ;

    __builtin_unreachable();
}

void pong_loop(struct config *conf)
{
    UNUSED(conf);
    for (ever)
        ;

    __builtin_unreachable();
}

void server_loop(struct config *conf)
{
    //TODO:
    UNUSED(conf);
    for (ever)
        ;

    __builtin_unreachable();
}
