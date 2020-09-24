#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* -------------------------------- INCLUDES -------------------------------- */

#include <signal.h>
#include <stdbool.h>

#include "config.h"
#include "loops.h"
#include "nfv_socket.h"
#include "pkt_util.h"
#include "stats.h"
#include "timestamp.h"

typedef struct nfv_socket *nfv_socket_ptr;

/* -------------------------------- DEFINES --------------------------------- */

/* Used to suppress compiler warnings about unused arguments */
#define UNUSED(x) ((void)x)

/* Used to write forever loops */
#define ever                                                                   \
    ;                                                                          \
    ;

/* -------------------------------- GLOBALS --------------------------------- */

struct stats *stats_ptr = NULL;

/* --------------------------- UTILITY  FUNCTIONS --------------------------- */

/* --------------------------- LOOP EXIT FUNCTION --------------------------- */

void handle_sigint(int sig) {
    printf("\nCaught signal %s!\n", strsignal(sig));

    if (sig == SIGINT) {
        if (stats_ptr != NULL) {
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
void tsc_loop(void *arg) {
    UNUSED(arg);

    for (ever) {
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
void send_loop(struct config *conf) {
    nfv_socket_ptr socket = nfv_socket_factory_get(conf);

    /* ----------------------------- Constants ------------------------------ */
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz;
    const tsc_t tsc_incr = tsc_hz * conf->bst_size / conf->rate;

    /* ------------------- Variables and data structures -------------------- */

    // Pointer to payload buffers
    buffer_t buffers[conf->bst_size];

    // Timers and counters
    tsc_t tsc_cur, tsc_prev, tsc_next;

    // Stats variables
    struct stats stats = STATS_INIT;

    stats.type = STATS_TX;
    stats_ptr = &stats;

    struct stats_data_tx stats_period = {0, 0};

    /* --------------------------- Initialization --------------------------- */

    /* ------------------ Infinite loop variables and body ------------------ */

    // FIXME: change the pointed function to get the tsc from the one that
    // updates every time to the one that simply reads the shared variable
    // if the client is used instead of the sender (tsc_get_last())

    tsc_prev = tsc_cur = tsc_next = tsc_get_update();

    int num_sent;

    for (ever) {
        tsc_cur = tsc_get_update(); // FIXME: What about tsc_get_update for
                                    // sender application?

        // If more than a second elapsed
        if (tsc_cur - tsc_prev > tsc_out) {
            // Save stats
            stats_save(&stats, (union stats_data *)&stats_period);

            // If not silent, print them
            if (!conf->silent) {
                stats_print(STATS_TX, (union stats_data *)&stats_period);
            }

            // Reset stats for the new period
            stats_period.tx = 0;
            stats_period.dropped = 0;

            // Update timers
            tsc_prev = tsc_cur;
        }

        /*  If it is already time for the next burst (or in general tsc_next is
            less than tsc_cur), send the next burst */
        if (tsc_cur > tsc_next) {
            tsc_next += tsc_incr;

            // Request buffers where to write our packets from the library The
            // library will fill header information if necessary (example.
            // SOCK_RAW or DPDK)
            // FIXME: work with less packets if returned less
            nfv_socket_request_out_buffers(socket, buffers, conf->bst_size);

            // Put payload data in each packet
            for (size_t i = 0; i < conf->bst_size; ++i) {
                // The first element will have the current value of the tsc,
                // then a dummy payload will be inserted to fill the packet The
                // tsc value is taken after producing the dummy data

                // If data should be produced, fill each packet
                if (conf->touch_data) {
                    produce_data_offset(buffers[i],
                                        conf->payload_size - sizeof(tsc_cur),
                                        sizeof(tsc_cur));
                }

                tsc_cur = tsc_get_update();
                put_i64_offset(buffers[i], 0, tsc_cur);
            }

            // Use one single call to send data (the library will then convert
            // to the appropriate single or multiple calls)
            num_sent = nfv_socket_send(socket, conf->bst_size);

            // Errors are considered all dropped packets
            if (num_sent < 0) {
                num_sent = 0;
            }

            stats_period.tx += num_sent;
            stats_period.dropped += conf->bst_size - num_sent;
        }
    }

    __builtin_unreachable();
}

/**
 * Infinite loop that receives packets grouping them in bursts.
 */
void recv_loop(struct config *conf) {
    // TODO:
    nfv_socket_ptr socket = nfv_socket_factory_get(conf);

    /* ----------------------------- Constants ------------------------------ */

    // Timers and counters
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz; // Save stats once each second

    /* ------------------- Variables and data structures -------------------- */

    // Pointer to payload buffers
    buffer_t buffers[conf->bst_size];

    // Timers and counters
    tsc_t tsc_cur, tsc_prev;

    // Stats variables
    struct stats stats = STATS_INIT;

    stats.type = STATS_RX;
    stats_ptr = &stats;

    struct stats_data_rx stats_period = {0};

    /* --------------------------- Initialization --------------------------- */

    /* ------------------ Infinite loop variables and body ------------------ */

    int num_recv, num_ok;

    tsc_cur = tsc_prev = tsc_get_update();

    for (ever) {
        tsc_cur = tsc_get_update();

        // If more than a second elapsed, print stats
        if (tsc_cur - tsc_prev > tsc_out) {
            // Save stats
            stats_save(&stats, (union stats_data *)&stats_period);

            // If not conf->silent, print them
            if (!conf->silent) {
                stats_print(STATS_RX, (union stats_data *)&stats_period);
            }

            // Reset stats for the new period
            stats_period.rx = 0;

            // Update timers
            tsc_prev = tsc_cur;
        }

        num_recv = nfv_socket_recv(socket, buffers, conf->bst_size);

        // If data should be consumed, do that
        if (conf->touch_data) {
            num_ok = 0;
            for (int i = 0; i < num_recv; ++i) {
                if (consume_data_offset(buffers[i],
                                        conf->payload_size - sizeof(tsc_t),
                                        sizeof(tsc_t)))
                    ++num_ok;
            }

            num_recv = num_ok;
        }

        if (num_recv < 0) {
            num_recv = 0;
        }

        stats_period.rx += num_recv;
    }

    __builtin_unreachable();
}

void pong_loop(struct config *conf) {
    nfv_socket_ptr socket = nfv_socket_factory_get(conf);

    /* ----------------------------- Constants ------------------------------ */
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz; // Print stats once per second

    /* ------------------- Variables and data structures -------------------- */

    // Pointer to payload buffers
    buffer_t buffers[conf->bst_size];

    // Timers and counters
    tsc_t tsc_cur, tsc_prev;
    tsc_t tsc_pkt, tsc_diff;

    // Stats variables
    struct stats stats = STATS_INIT;

    stats.type = STATS_DELAY;
    stats_ptr = &stats;

    struct stats_data_delay stats_period = {0, 0};

    // --------------------------- Initialization --------------------------- //

    // ------------------ Infinite loop variables and body ------------------ //

    int num_recv;
    tsc_cur = tsc_prev = tsc_get_last();

    for (ever) {
        tsc_cur = tsc_get_last();

        // If more than a second elapsed
        if (tsc_cur - tsc_prev > tsc_out) {
            // If there is some stat to actually save
            if (stats_period.num) {
                // Calculate the actual average (until now it is a total,
                // despite the name)
                stats_period.avg = stats_period.avg / stats_period.num;

                // Save stats
                stats_save(&stats, (union stats_data *)&stats_period);

                // If not conf->silent, print them
                if (!conf->silent) {
                    stats_print(STATS_DELAY, (union stats_data *)&stats_period);
                }
            }

            // Reset stats for the new period
            stats_period.avg = 0;
            stats_period.num = 0;

            // Update timers
            tsc_prev = tsc_cur;
        }

        num_recv = nfv_socket_recv(socket, buffers, conf->bst_size);

        for (int i = 0; i < num_recv; ++i) {
            tsc_pkt = get_i64_offset(buffers[i], 0);
            if (conf->touch_data) {
                consume_data_offset(buffers[i],
                                    conf->payload_size - sizeof(tsc_t),
                                    sizeof(tsc_t));
            }

            tsc_cur = tsc_get_last();

            // NOTICE: With this, packets with more than 0.1s delay are
            // considered dropped
            tsc_diff = tsc_cur - tsc_pkt;
            if (tsc_diff < tsc_hz / 10) {
                // Add calculated delay to the avg attribute (which is actually
                // the sum of all delays, the actual average is calculated only
                // once per second).
                stats_period.avg += tsc_diff;
                ++stats_period.num;
            } else if (!conf->silent) {
                printf("ERR: Received message with very big time difference: "
                       "TSC DIFF %lu (TSC_HZ %lu)\n",
                       tsc_diff, tsc_hz);
            }
        }

        int num_sent = nfv_socket_send_back(socket, num_recv);
    }

    __builtin_unreachable();
}

void server_loop(struct config *conf) {
    // TODO:
    UNUSED(conf);
    __builtin_unreachable();
}
