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

/* --------------------------- COMMON SUB-BODIES ---------------------------- */

static inline void stats_save_reset_tx(struct stats *stats,
                                       struct stats_data_tx *stats_period,
                                       struct config *conf)
{
    // Save stats
    stats_save(stats, (union stats_data *)stats_period);

    // If not silent, print them
    if (!conf->silent)
    {
        stats_print(STATS_TX, (union stats_data *)stats_period);
    }

    // Reset stats for the new period
    stats_period->tx = 0;
    stats_period->dropped = 0;
}

static inline void stats_save_reset_rx(struct stats *stats,
                                       struct stats_data_rx *stats_period,
                                       struct config *conf)
{
    // Save stats
    stats_save(stats, (union stats_data *)stats_period);

    // If not silent, print them
    if (!conf->silent)
    {
        stats_print(STATS_RX, (union stats_data *)stats_period);
    }

    // Reset stats for the new period
    stats_period->rx = 0;
}

static inline void stats_save_reset_delay(struct stats *stats,
                                          struct stats_data_delay *stats_period,
                                          struct config *conf)
{
    // If there is some stat to actually save
    if (stats_period->num)
    {
        // Calculate the actual average (until now it is a total,
        // despite the name)
        stats_period->avg = stats_period->avg / stats_period->num;

        // Save stats
        stats_save(stats, (union stats_data *)stats_period);

        // If not conf->silent, print them
        if (!conf->silent)
        {
            stats_print(STATS_DELAY, (union stats_data *)stats_period);
        }
    }

    // Reset stats for the new period
    stats_period->avg = 0;
    stats_period->num = 0;
}

static inline ssize_t prepare_send_burst(struct config *conf,
                                         nfv_socket_ptr socket,
                                         buffer_t buffers[],
                                         size_t burst_size)
{
    tsc_t tsc_cur;
    size_t howmany =
        nfv_socket_request_out_buffers(socket, buffers, burst_size);

    // Put payload data in each packet
    for (size_t i = 0; i < howmany; ++i)
    {
        // The first element will have the current value of the tsc,
        // then a dummy payload will be inserted to fill the packet The
        // tsc value is taken after producing the dummy data

        // If data should be produced, fill each packet
        if (conf->touch_data)
        {
            produce_data_offset(buffers[i],
                                conf->payload_size - sizeof(tsc_cur),
                                sizeof(tsc_cur));
        }

        tsc_cur = tsc_get_last();
        put_i64_offset(buffers[i], 0, tsc_cur);
    }

    return nfv_socket_send(socket, howmany);
}

static inline ssize_t recv_consume_burst(struct config *conf,
                                         nfv_socket_ptr socket,
                                         buffer_t buffers[],
                                         size_t burst_size)
{
    ssize_t num_recv = nfv_socket_recv(socket, buffers, burst_size);

    // If data should be consumed, do that
    if (num_recv > 0 && conf->touch_data)
    {
        size_t num_ok = 0;

        for (ssize_t i = 0; i < num_recv; ++i)
        {
            if (consume_data_offset(buffers[i],
                                    conf->payload_size - sizeof(tsc_t),
                                    sizeof(tsc_t)))
                ++num_ok;
        }

        num_recv = num_ok;
    }

    return num_recv;
}

/* ----------------------------- LOOP FUNCTIONS ----------------------------- */

/**
 * Infinite loop that updates the global tsc timer.
 */
int tsc_loop(void *arg)
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
 */
int send_loop(void *arg)
{
    struct config *conf = (struct config *)arg;

    nfv_socket_ptr socket = nfv_socket_factory_get(conf);

    /* ----------------------------- Constants ------------------------------ */
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz;
    const tsc_t tsc_incr = tsc_hz * conf->bst_size / conf->rate;

    // TODO: more constants derived from conf
    const bool should_save_stats = !conf->silent ||
                                   !(strstr(conf->cmdname, "client") != NULL);

    /* ------------------- Variables and data structures -------------------- */

    // Pointer to payload buffers
    buffer_t buffers[conf->bst_size];

    // Timers and counters
    tsc_t tsc_cur, tsc_prev, tsc_next;

    // Stats variables
    struct stats stats = STATS_INIT;

    stats.type = STATS_TX;
    if (should_save_stats) //FIXME:
        stats_ptr = &stats;

    struct stats_data_tx stats_period = {0, 0};

    /* --------------------------- Initialization --------------------------- */

    /* ------------------ Infinite loop variables and body ------------------ */

    tsc_prev = tsc_cur = tsc_next = tsc_get_last();

    ssize_t num_sent;

    for (ever)
    {
        tsc_cur = tsc_get_last();

        // If more than a second elapsed
        if (tsc_cur - tsc_prev > tsc_out)
        {
            // Save, (print,) and reset stats
            if (should_save_stats)
                stats_save_reset_tx(&stats, &stats_period, conf);
            // Update timers
            tsc_prev = tsc_cur;
        }

        //  If it is already time for the next burst, send new burst
        if (tsc_cur > tsc_next)
        {
            tsc_next += tsc_incr;

            num_sent = prepare_send_burst(conf, socket, buffers, conf->bst_size);

            // Errors are considered all dropped packets
            if (num_sent < 0)
            {
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
int recv_loop(void *arg)
{
    struct config *conf = (struct config *)arg;
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

    ssize_t num_recv;

    tsc_cur = tsc_prev = tsc_get_last();

    for (ever)
    {
        tsc_cur = tsc_get_last();

        // If more than a second elapsed, print stats
        if (tsc_cur - tsc_prev > tsc_out)
        {
            // Save, (print,) and reset stats
            stats_save_reset_rx(&stats, &stats_period, conf);
            // Update timers
            tsc_prev = tsc_cur;
        }

        num_recv = recv_consume_burst(conf, socket, buffers, conf->bst_size);

        // Errors are not counted of course
        if (num_recv < 0)
        {
            num_recv = 0;
        }

        stats_period.rx += num_recv;
    }

    __builtin_unreachable();
}

int client_loop(void *arg)
{
    struct config *conf = (struct config *)arg;
    nfv_socket_ptr socket = nfv_socket_factory_get(conf);

    /* ----------------------------- Constants ------------------------------ */
    const tsc_t tsc_hz = tsc_get_hz();
    const tsc_t tsc_out = tsc_hz; // Print stats once per second
    const tsc_t tsc_incr = tsc_hz * conf->bst_size / conf->rate;

    const bool send_in_this_thread = strstr(conf->cmdname, "clientst") != NULL;

    /* ------------------- Variables and data structures -------------------- */

    // Pointer to payload buffers
    buffer_t buffers[conf->bst_size];

    // Timers and counters
    tsc_t tsc_cur, tsc_prev;
    tsc_t tsc_pkt, tsc_diff;
    tsc_t tsc_next;

    // Stats variables
    struct stats stats = STATS_INIT;

    stats.type = STATS_DELAY;
    stats_ptr = &stats;

    struct stats_data_delay stats_period = {0, 0};

    // --------------------------- Initialization --------------------------- //

    // ------------------ Infinite loop variables and body ------------------ //

    ssize_t num_recv;

    tsc_cur = tsc_prev = tsc_next = tsc_get_last();

    for (ever)
    {
        tsc_cur = tsc_get_last();

        // If more than a second elapsed
        if (tsc_cur - tsc_prev > tsc_out)
        {
            // Save, (print,) and reset stats
            stats_save_reset_delay(&stats, &stats_period, conf);
            // Update timers
            tsc_prev = tsc_cur;
        }

        if (tsc_cur > tsc_next)
        {
            tsc_next += tsc_incr;

            if (send_in_this_thread)
            {
                // Don't care if actually sent or not
                prepare_send_burst(conf, socket, buffers, conf->bst_size);
            }
        }

        num_recv = recv_consume_burst(conf, socket, buffers, conf->bst_size);

        for (ssize_t i = 0; i < num_recv; ++i)
        {
            tsc_pkt = get_i64_offset(buffers[i], 0);
            tsc_cur = tsc_get_last();

            // NOTICE: With this, packets with more than 0.1s delay are
            // considered dropped
            tsc_diff = tsc_cur - tsc_pkt;
            if (tsc_diff < tsc_hz / 10)
            {
                // Add calculated delay to the avg attribute,
                // which, to be honest, is actually the sum of all delays.
                // The actual average is calculated only before saving it.
                stats_period.avg += tsc_diff;
                ++stats_period.num;
            }
            else if (!conf->silent)
            {
                printf("ERR: Received message with very big time difference: TSC DIFF %lu (TSC_HZ %lu)\n", tsc_diff, tsc_hz);
            }
        }
    }

    __builtin_unreachable();
}

int server_loop(void *arg)
{
    struct config *conf = (struct config *)arg;
    nfv_socket_ptr socket = nfv_socket_factory_get(conf);

    // Pointer to payload buffers
    buffer_t buffers[conf->bst_size];

    ssize_t num_recv;

    for (ever)
    {
        num_recv = recv_consume_burst(conf, socket, buffers, conf->bst_size);

        if (num_recv < 0)
            num_recv = 0;

        nfv_socket_send_back(socket, num_recv);
    }

    __builtin_unreachable();
}
