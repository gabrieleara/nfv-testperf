#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdbool.h>
#include <signal.h>

#include "commands.h"
#include "constants.h"
#include "config.h"
#include "loops.h"

// static const struct config_defaults defaults_server = {
//     .local = {
//         .mac = SERVER_ADDR_MAC,
//         .ip = SERVER_ADDR_IP,
//         .port_number = SERVER_PORT,
//     },
//     .remote = {
//         .mac = CLIENT_ADDR_MAC,
//         .ip = CLIENT_ADDR_IP,
//         .port_number = CLIENT_PORT,
//     },
// };

// static const struct config_defaults defaults_client = {
//     .local = {
//         .mac = CLIENT_ADDR_MAC,
//         .ip = CLIENT_ADDR_IP,
//         .port_number = CLIENT_PORT,
//     },
//     .remote = {
//         .mac = SERVER_ADDR_MAC,
//         .ip = SERVER_ADDR_IP,
//         .port_number = SERVER_PORT,
//     },
// };

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

static inline int command_body(int argc, char *argv[], const struct config_defaults *defaults, void (*loop_fn)(struct config *))
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

    // Enter the main infinite loop
    loop_fn(&conf);

    __builtin_unreachable();

    return EXIT_SUCCESS;
}

// int server_body(int argc, char *argv[])
// {
//     return command_body(argc, argv, &defaults_server, server_loop);
// }

// int client_body(int argc, char *argv[])
// {
//     struct config conf = CONFIG_STRUCT_INITIALIZER;

//     // TODO: SET SOCKET TYPE THAT YOU WANT TO USE SOMEHOW
//     int res = config_parse_application_parameters(argc, argv, &conf, &defaults_client);
//     print_config(&conf);

//     // TODO: create a socket handle before and "clone" it in each thread, if
//     // more than one is used

//     int res = sock_create(&conf, O_NONBLOCK, false);
//     if (res)
//         return EXIT_FAILURE;

//     // TODO: REGISTER SIGNAL HANDLER
//     signal(SIGINT, handle_sigint);

//     tsc_init();

//     // START A SECOND THREAD ONLY FOR THE TSC COUNTER
//     pthread_t tsc_thread;
//     pthread_create(&tsc_thread, NULL, tsc_loop, NULL);

//     // START A PONG THREAD
//     pthread_t pong_thread;
//     pthread_create(&pong_thread, NULL, pong_loop, (void *)&conf);

//     // ENTER CLIENT SEND LOOP
//     client_loop(&conf);

//     // Should never get here actually
//     close(conf.sock_fd);

//     return EXIT_SUCCESS;
// }

// int clientst_body(int argc, char *argv[])
// {
//     return command_body(argc, argv, &defaults_client, clientst_loop); // TODO: should use send_loop WITH NO STATS COLLECTION
// }

int recv_body(int argc, char *argv[])
{
    return command_body(argc, argv, &defaults_recv, recv_loop);
}

int send_body(int argc, char *argv[])
{
    return command_body(argc, argv, &defaults_send, send_loop);
}

#define UNUSED(x) ((void)x)
int server_body(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    return EXIT_FAILURE;
    // return command_body(argc, argv, &defaults_server, server_loop);
}

int client_body(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    return EXIT_FAILURE;
    // return command_body(argc, argv, &defaults_client, client_loop);
}

int clientst_body(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    return EXIT_FAILURE;
    // return command_body(argc, argv, &defaults_client, clientst_loop);
}
