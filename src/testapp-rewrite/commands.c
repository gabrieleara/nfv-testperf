#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#include "commands.h"
#include "constants.h"
#include "config.h"
#include "loops.h"
#include "threads.h"

static const struct config_defaults defaults_server = {
    .local = {
        .mac = SERVER_ADDR_MAC,
        .ip = SERVER_ADDR_IP,
        .port_number = SERVER_PORT,
    },
    .remote = {
        .mac = CLIENT_ADDR_MAC,
        .ip = CLIENT_ADDR_IP,
        .port_number = CLIENT_PORT,
    },
};

static const struct config_defaults defaults_client = {
    .local = {
        .mac = CLIENT_ADDR_MAC,
        .ip = CLIENT_ADDR_IP,
        .port_number = CLIENT_PORT,
    },
    .remote = {
        .mac = SERVER_ADDR_MAC,
        .ip = SERVER_ADDR_IP,
        .port_number = SERVER_PORT,
    },
};

static const struct config_defaults defaults_send = {
    .local = {
        .mac = SEND_ADDR_MAC,
        .ip = SEND_ADDR_IP,
        .port_number = SEND_PORT,
    },
    .remote = {
        .mac = RECV_ADDR_MAC,
        .ip = RECV_ADDR_IP,
        .port_number = RECV_PORT,
    },
};

static const struct config_defaults defaults_recv = {
    .local = {
        .mac = RECV_ADDR_MAC,
        .ip = RECV_ADDR_IP,
        .port_number = RECV_PORT,
    },
    .remote = {
        .mac = SEND_ADDR_MAC,
        .ip = SEND_ADDR_IP,
        .port_number = SEND_PORT,
    },
};

static inline int command_body(int argc, char *argv[],
                               const struct config_defaults *defaults,
                               const thread_body_t loops[],
                               const int howmany_loops)
{
    struct config conf;

    config_initialize(&conf, defaults);

    int res = config_parse_arguments(&conf, argc, argv);
    if (res <= 0)
    {
        fprintf(stderr, "ERR: Could not parse arguments correctly!");
        return EXIT_FAILURE;
    }

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

    core_t num_cores = cores_count(&conf);

    // Check that the user started the application with enough cores
    if (num_cores < (core_t)howmany_loops + 1)
    {
        fprintf(stderr,
                "ERROR: Needs more cores. Available %d. Needed %d.\n",
                num_cores,
                howmany_loops + 1);
        return EXIT_FAILURE;
    }

    if (num_cores > (core_t)howmany_loops + 1)
        num_cores = (core_t)howmany_loops + 1;

    // Prepare the data for each worker. NOTICE: the last worker shall be
    // executed on the master core by setting this thread affinity to the
    // master core and running the function

    struct thread_info workers_info[howmany_loops + 1];

    core_t i = 0;
    core_t core_id;

    // Since the TSC thread should be the first to start, the master core will
    // actually run one of the application loops

    CORES_FOREACH_SLAVE(&conf, core_id)
    {
        if (i >= num_cores - 1)
            break;

        if (i == 0)
        {
            workers_info[i] = (struct thread_info){
                core_id,
                tsc_loop,
                0,
                NULL};
        }
        else
        {
            workers_info[i] = (struct thread_info){
                core_id,
                loops[i - 1],
                0,
                &conf};
        }
        ++i;
    }

    // This last loop will run on the master core
    workers_info[i] = (struct thread_info){
        cores_get_master(&conf),
        loops[i - 1],
        0,
        &conf};

    // Start all workers on the slave cores (first N workers)
    for (i = 0; i < num_cores - 1; ++i)
    {
        res = thread_start(&conf, &workers_info[i]);
        if (res)
        {
            fprintf(
                stderr,
                "Failed to start worker thread number %d of %d. Cause: %s\n",
                i + 1,
                howmany_loops + 1,
                strerror(errno));
            return EXIT_FAILURE;
        }

        // To "ensure" that the TSC thread will be already running before
        // spawining the other worker threads
        if (i == 0)
            sleep(1);
    }

    // Run the last worker on the current thread, after setting its affinity to
    // the master core
    // TODO: check if this works, even if you believe it should
    cores_setaffinity(workers_info[num_cores - 1].core_id);

    // Run the worker on the current thread
    workers_info[num_cores - 1].tbody(workers_info[num_cores - 1].arg);

    // Finally, wait for termination of all other worker threads
    for (i = 0; i < num_cores; ++i)
    {
        /* res = */ thread_join(&conf, &workers_info[i], NULL);
    }

    return EXIT_SUCCESS;
}

int recv_body(int argc, char *argv[])
{
    thread_body_t loops[] = {recv_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_recv, loops, howmany_loops);
}

int send_body(int argc, char *argv[])
{
    thread_body_t loops[] = {send_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_send, loops, howmany_loops);
}

int server_body(int argc, char *argv[])
{
    thread_body_t loops[] = {server_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_server, loops, howmany_loops);
}

int client_body(int argc, char *argv[])
{
    thread_body_t loops[] = {send_loop, client_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_client, loops, howmany_loops);
}

int clientst_body(int argc, char *argv[])
{
    thread_body_t loops[] = {client_loop};
    int howmany_loops = sizeof(loops) / sizeof(thread_body_t);
    return command_body(argc, argv, &defaults_client, loops, howmany_loops);
}
