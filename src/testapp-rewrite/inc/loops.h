#ifndef LOOPS_H
#define LOOPS_H

/* If we're not using GNU C, elide __attribute__ */
#ifndef __GNUC__
#define __attribute__(x) /*NOTHING*/
#endif

#include "config.h"
#include "stats.h"

/* -------------------- LOOP EXIT FUNCTION  DECLARATION --------------------- */

extern struct stats *stats_ptr;
extern void handle_sigint(int sig);

/* ---------------------- LOOP FUNCTIONS  DECLARATIONS ---------------------- */

extern int tsc_loop(void *) __attribute__((noreturn));
extern int send_loop(void *) __attribute__((noreturn));
extern int recv_loop(void *) __attribute__((noreturn));
extern int server_loop(void *) __attribute__((noreturn));
extern int client_loop(void *) __attribute__((noreturn));

#endif /* LOOPS_H */
