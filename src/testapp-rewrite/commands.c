#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include "commands.h"
#include "config.h"
#include "constants.h"
#include "loops.h"
#include "threads.h"

static const struct config_defaults defaults_server = {
    .local =
        {
            .mac = SERVER_ADDR_MAC,
            .ip = SERVER_ADDR_IP,
            .port_number = SERVER_PORT,
        },
    .remote =
        {
            .mac = CLIENT_ADDR_MAC,
            .ip = CLIENT_ADDR_IP,
            .port_number = CLIENT_PORT,
        },
};

static const struct config_defaults defaults_client = {
    .local =
        {
            .mac = CLIENT_ADDR_MAC,
            .ip = CLIENT_ADDR_IP,
            .port_number = CLIENT_PORT,
        },
    .remote =
        {
            .mac = SERVER_ADDR_MAC,
            .ip = SERVER_ADDR_IP,
            .port_number = SERVER_PORT,
        },
};

static const struct config_defaults defaults_send = {
    .local =
        {
            .mac = SEND_ADDR_MAC,
            .ip = SEND_ADDR_IP,
            .port_number = SEND_PORT,
        },
    .remote =
        {
            .mac = RECV_ADDR_MAC,
            .ip = RECV_ADDR_IP,
            .port_number = RECV_PORT,
        },
};

static const struct config_defaults defaults_recv = {
    .local =
        {
            .mac = RECV_ADDR_MAC,
            .ip = RECV_ADDR_IP,
            .port_number = RECV_PORT,
        },
    .remote =
        {
            .mac = SEND_ADDR_MAC,
            .ip = SEND_ADDR_IP,
            .port_number = SEND_PORT,
        },
};

static inline void perror_exit(const char *fmt, ...)
    __attribute__((noreturn, format(printf, 1, 2)));

static inline void perror_exit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}

static inline core_t check_cores(struct config *conf,
                                 core_t *const needed_cores) {
    if (cores_count(conf) < *needed_cores)
        perror_exit("ERR: Need more cores; available %d, needed %d.\n",
                    cores_count(conf), *needed_cores);

    return *needed_cores;
}

static inline int command_body(int argc, char *argv[],
                               const struct config_defaults *defaults,
                               const thread_body_t loops[],
                               const int howmany_loops) {
    struct config conf;

    config_initialize(&conf, defaults);

    // Needs at least program name, hence res must be > 0
    int res = config_parse_arguments(&conf, argc, argv);
    if (res <= 0)
        perror_exit("ERR: Could not parse arguments correctly!");

    config_print(&conf);

    // Shift arguments, certain NFV sockets may require additional arguments
    // (see DPDK one)
    argc -= res;
    argv += res;

    res = config_initialize_socket(&conf, argc, argv);
    if (res)
        return EXIT_FAILURE;

    // Register the termination callback
    signal(SIGINT, handle_sigint);

    // Initialize the Time Stamp Counter handle for loop usage
    tsc_init();

    // Initialize cores management, works only after initialization of both
    // configuration and sockets
    cores_init(&conf);

    // Check that the user started the application with the right number of
    // cores
    core_t num_cores = check_cores(&conf, (core_t *)&howmany_loops);

    // Prepare the data for each worker. NOTICE: the last worker shall be
    // executed on the master core by setting this thread affinity to the
    // master core and running the function

    struct thread_info workers_info[howmany_loops];

    core_t i = 0;
    core_t core_id;

    // Since the TSC thread should be the first to start, the master core will
    // actually run one of the application loops

    CORES_FOREACH_SLAVE(&conf, core_id) {
        if (i >= num_cores - 1)
            break;

        workers_info[i] = (struct thread_info){core_id, loops[i], 0, &conf};
        ++i;
    }

    // This last loop will run on the master core
    workers_info[i] =
        (struct thread_info){cores_get_master(&conf), loops[i], 0, &conf};

    printf("-------------------------------------\n");
    printf("STARTING WORKER THREADS...\n");

    // Start all workers on the slave cores (first N workers)
    for (i = 0; i < num_cores - 1; ++i) {
        res = thread_start(&conf, &workers_info[i]);
        if (res)
            perror_exit(
                "ERR: failed to start worker thread %d/%d.\nCause: %s\n", i,
                howmany_loops, strerror(errno));
    }

    printf("\n");

    if (!USE_DPDK(&conf)) {
        // Run the last worker on the current thread, after setting its affinity
        // to the master core
        // TODO: check if this works, even if you believe it should
        cores_setaffinity(workers_info[num_cores - 1].core_id);
    }

    printf("STARTING FUNCTION ON MASTER THREAD...\n");
    printf("-------------------------------------\n");

    // Run the worker on the current thread
    workers_info[num_cores - 1].tbody(workers_info[num_cores - 1].arg);

    // Finally, wait for termination of all other worker threads
    for (i = 0; i < num_cores - 1; ++i)
        thread_join(&conf, &workers_info[i], NULL);

    return EXIT_SUCCESS;
}

int recv_body(int argc, char *argv[]) {
    thread_body_t loops[] = {recv_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_recv, loops, howmany_loops);
}

int send_body(int argc, char *argv[]) {
    thread_body_t loops[] = {send_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_send, loops, howmany_loops);
}

int server_body(int argc, char *argv[]) {
    thread_body_t loops[] = {server_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_server, loops, howmany_loops);
}

int client_body(int argc, char *argv[]) {
    thread_body_t loops[] = {
        tsc_loop,
        client_loop,
        send_loop,
    };
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_client, loops, howmany_loops);
}

int clientst_body(int argc, char *argv[]) {
    thread_body_t loops[] = {client_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_client, loops, howmany_loops);
}
