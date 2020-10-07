#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

extern int server_body(int argc, char *argv[]);
extern int client_body(int argc, char *argv[]);
extern int clientst_body(int argc, char *argv[]);

extern int recv_body(int argc, char *argv[]);
extern int send_body(int argc, char *argv[]);

// CUSTOM FUNCTIONS FOR DPDK ARE NO NEEDED ANYMORE NOW THAT THE FRAMEWORK IS
// GENERIC! USE THE SAME BODIES AS THE OTHERS!

// extern int dpdk_server_body(int argc, char *argv[]);
// extern int dpdk_client_body(int argc, char *argv[]);
// extern int dpdk_clientst_body(int argc, char *argv[]);

// extern int dpdk_recv_body(int argc, char *argv[]);
// extern int dpdk_send_body(int argc, char *argv[]);

typedef int (*main_body_t)(int, char **);

static const char *const commands_n[] = {
    "server",
    "client",
    "clientst",
    "send",
    "recv",

    "dpdk-server",
    "dpdk-client",
    "dpdk-clientst",
    "dpdk-send",
    "dpdk-recv",
};

static const main_body_t commands_f[] = {
    server_body,
    client_body,
    clientst_body,
    send_body,
    recv_body,

    server_body,
    client_body,
    clientst_body,
    send_body,
    recv_body,
};

static const int num_commands = sizeof(commands_f) / sizeof(main_body_t);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
