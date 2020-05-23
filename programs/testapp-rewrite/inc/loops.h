#ifndef LOOPS_H
#define LOOPS_H

/* If we're not using GNU C, elide __attribute__ */
#ifndef __GNUC__
#define __attribute__(x) /*NOTHING*/
#endif

/* ---------------------- LOOP FUNCTIONS  DECLARATIONS ---------------------- */

extern void tsc_loop(struct config *conf) __attribute__((noreturn));
extern void send_loop(struct config *conf) __attribute__((noreturn));
extern void recv_loop(struct config *conf) __attribute__((noreturn));
extern void server_loop(struct config *conf) __attribute__((noreturn));
extern void pong_loop(struct config *conf) __attribute__((noreturn));

#endif /* LOOPS_H */
