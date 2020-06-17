#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* -------------------------------- Includes -------------------------------- */

#include <stdbool.h>

#include <unistd.h>
#include <stdio.h>

#include <linux/if_ether.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "constants.h"
#include "config.h"

// #include "dpdk.h"

/* ------------------------------- Constants -------------------------------- */

#define NO_ADDR_PORT            \
    {                           \
        .ip = {0}, .mac = { 0 } \
    }

static const struct config CONFIG_STRUCT_INITIALIZER = {
    .rate = DEFAULT_RATE,
    .pkt_size = DEFAULT_PKT_SIZE,
    .bst_size = DEFAULT_BST_SIZE,

    .use_block = false,
    .use_mmsg = false,

    .silent = false,
    .touch_data = false,

    .local = NO_ADDR_PORT,
    .remote = NO_ADDR_PORT,

    .sock_type = NFV_SOCK_NONE,
    .sock_fd = -1,

    .dpdk = {
        .portid = 0,
        .direction = DIRECTION_TXRX,
        .mbufs = NULL,
    },
};

const char usage_format_string[] =
    "USAGE\n"
    "       %s <program-options-unordered-list> <addresses-ordered-list> -- <dpdk-parameters>\n"
    "\n"
    "Where:\n"
    " - <program-options-unordered-list> is a list of non-ordered parameters (see PARAMETERS section);\n"
    " - <adresses-ordered-list> is a list of IP and MAC addresses used by the program\n"
    "       (see ADDRESSES section);\n"
    " - <dpdk-parameters> is a list of parameters for EAL DPDK environment\n"
    "       (see online - used only by programs starting with 'dpdk-').\n"
    "\n"
    "PARAMETERS\n"
    "\n"
    "    -r <rate=1000000>      The sending/receiving rate in pps.\n"
    "    -p <packet_size=64>    The size of each frame in bytes.\n"
    "    -b <burst_size=32>     The size of each packet burst in number of packets.\n"
    "\n"
    "    -c                     Generate actual data/Calculate a checksum on each received payload.\n"
    "                           This ensures that each byte in a message payload is actually touched.\n"
    "\n"
    "    -R <interf_name>       Use RAW sockets instead of UDP ones (default are UDP sockets).\n"
    "                           The argument is the name of the interface to use.\n"
    "                           Valid only for sockets-based programs, not DPDK ones.\n"
    "\n"
    "    -B                     Use blocking sockets instead of nonblocking ones.\n"
    "                           Valid only for sockets-based programs, not DPDK ones.\n"
    "\n"
    "    -m                     Use SENDMMSG/RECVMMSG API to exchange packets.\n"
    "                           Valid only for sockets-based programs, not DPDK ones.\n"
    "\n"
    "    -s                     Run in silent mode. Prints no stats until the termination SIGINT is received.\n"
    "\n"
    "\n"
    "ADDRESSES\n"
    "\n"
    "       The application expects some addresses to be given as argument in a specific order.\n"
    "       The following is the list of each address expected.\n"
    "       Default values will be used if not all addresses are provided and not all parameters are always used.\n"
    "\n"
    "       Since the program will communicate with another application, we will refer as this\n"
    "       program as the LOCAL application and use the REMOTE term to indicate the other one.\n"
    "\n"
    "       <LOCAL_IP> <LOCAL_MAC> <REMOTE_IP> <REMOTE_MAC> \n"
    "\n";

/* --------------------------- Private Functions ---------------------------- */

/**
 * Check whether the string is equal to "--".
 *
 * \return true on success.
 * */
static inline bool check_doubledash(char *s)
{
    return strcmp(s, "--") == 0;
}

/**
 * Check whether the first character is equal to '-'.
 *
 * \return true on success.
 * */
static inline bool check_dash(char *s)
{
    return s[0] == '-';
}

/**
 * Parse option arguments.
 *
 * Option arguments are characterized by a minus '-' as starting character and
 * they are typically followed by one letter only. If the option has an argument
 * value, this function will parse it as well and fill the configuration
 * structure accordingly.
 *
 * NOTICE: some of the options may be ignored, depending on the command that has
 * been requested to the application.
 *
 * \param argc the number of arguments.
 * 
 * \param argv the array of arguments.
 * 
 * \param conf the pointer to the program configuration structure.
 * 
 * \param argind
 * the index of the first argument to be processed.
 *
 * \return the index of the last non-processed argument (either a "--" or a
 * non-option argument).
 * */
static inline int options_parse(
    int argc,
    char *argv[],
    struct config *conf,
    int argind)
{
    int opt;
    optind = argind;

    while ((opt = getopt(argc, argv, "+r:p:b:R:cmsB")) != -1)
    {
        switch (opt)
        {
        case 'r':
            conf->rate = atoi(optarg);
            break;
        case 'p':
            conf->pkt_size = atoi(optarg);
            break;
        case 'b':
            conf->bst_size = atoi(optarg);
            break;
        case 'R':
            /* NOTICE: option is ignored by non-Linux based configurations */
            if ((conf->sock_type & (NFV_SOCK_DGRAM | NFV_SOCK_RAW)) == 0)
                continue;

            conf->sock_type = NFV_SOCK_RAW;

            const size_t buflen = sizeof(conf->local_interf);
            assert(buflen > 0);
            strncpy(conf->local_interf, optarg, buflen - 1);
            conf->local_interf[buflen - 1] = '\0';

            break;
        case 'B':
            conf->use_block = true;
            break;
        case 'c':
            conf->touch_data = true;
            break;
        case 'm':
            conf->use_mmsg = true;
            break;
        case 's':
            conf->silent = true;
            break;
        default: /* '?' */
            fprintf(stderr, usage_format_string, argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (check_doubledash(argv[optind - 1]))
        return optind - 1;

    return optind;
}

/**
 * Set the port number of the given address.
 * */
static inline void addr_port_number_set(struct sockaddr_in *addr, int port)
{
    addr->sin_port = htons(port);
}

/**
 * Get the port number from the given address structure.
 * 
 * \return the requested port number.
 * */
static inline int addr_port_number_get(const struct sockaddr_in *addr)
{
    return ntohs(addr->sin_port);
}

/**
 * Construct an IPv4 address from the given string.
 *
 * \return 0 on success, an error code otherwise.
 * */
static inline int addr_ip_set(struct sockaddr_in *addr, const char *str)
{
    int res;

    addr->sin_family = AF_INET;
    res = inet_aton(str, &addr->sin_addr);
    if (res == 0)
    {
        return -1;
    }

    return 0;
}

/**
 * Construct an IPv4 address string from the given data structure.
 *
 * NOTICE: assumes that str argument to be at least INET_ADDRSTRLEN chars long.
 * 
 * \return 0 on success, an error code otherwise.
 * */
static inline int addr_ip_get(char *str, const struct sockaddr_in *addr)
{
    // Assumes sin_family = AF_INET
    if (inet_ntop(AF_INET, &(addr->sin_addr), str, INET_ADDRSTRLEN) == str)
        return 0;
    return -1;
}

/**
 * Construct a MAC address from the given string.
 *
 * \return 0 on success, -1 otherwise.
 * */
static inline int addr_mac_set(struct sockaddr_ll *addr, const char *str, const char *ifname)
{
    memset(addr, 0, sizeof(struct sockaddr_ll));
    addr->sll_family = AF_PACKET;
    addr->sll_protocol = htons(ETH_P_ALL);
    addr->sll_ifindex = (ifname == NULL) ? 0 : if_nametoindex(ifname);
    addr->sll_halen = sizeof(addr->sll_addr); // TODO: check if it's right, otherwise just put 6 here

    int res;

    res = sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                 addr->sll_addr + 0,
                 addr->sll_addr + 1,
                 addr->sll_addr + 2,
                 addr->sll_addr + 3,
                 addr->sll_addr + 4,
                 addr->sll_addr + 5);

    if (res != 6)
    {
        memset(addr, 0, 6);
        return -1;
    }

    return 0;
}

/**
 * Construct a MAC address string from the given data structure.
 * 
 * NOTICE: assumes that str argument is big enough to fit a MAC address string
 * and the \0 at the end.
 *
 * \return 0 on success, an error code otherwise.
 * */
static inline int addr_mac_get(char *str, const struct sockaddr_ll *addr)
{
    sprintf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            addr->sll_addr[0],
            addr->sll_addr[1],
            addr->sll_addr[2],
            addr->sll_addr[3],
            addr->sll_addr[4],
            addr->sll_addr[5]);

    return 0;
}

/**
 * Parse non-option arguments.
 *
 * These arguments are positional and they must be issued one after the other.
 * They can be interleaved with option parameters, but it wouldn't be pretty.
 *
 * The expected ordering is: \li Local IPv4 Addres \li Local MAC Addres \li
 * Remote IPv4 Addres \li Remote MAC Addres
 *
 * For any non-supplied parameter, default values will be used. This means that
 * some arguments may be optional, depending on the command to execute.
 *
 * \param argc the number of arguments.
 * 
 * \param argv the array of arguments.
 * 
 * \param conf the pointer to the program configuration structure.
 * 
 * \param argind the index of the first argument to be processed.
 *
 * \return the index of the last non-processed argument.
 * */
static inline int args_parse(
    int argc, char *argv[],
    struct config *conf,
    int argind)
{
    static int parameter_index = 0;
    int res;
    // union {
    //     ipaddr_str ip;
    //     macaddr_str mac;
    // } addr;

    /* If first argument is "-<something>", return immediately */
    while (argind < argc && !check_dash(argv[argind]))
    {
        /* Order is: local_ip, local_mac, remote_ip, remote_mac */
        switch (parameter_index)
        {
        case 0:
            res = addr_ip_set(&conf->local.ip, argv[argind]);
            if (res != 0)
            {
                fprintf(stderr, "Failed to parse local IP address (argv[%d]=%s)\n", argind, argv[argind]);
                exit(EXIT_FAILURE);
            }
            break;
        case 1:
            // The interface is not set here, but it is binded during raw socket
            // initialization below (if raw sockets are used)
            res = addr_mac_set(&conf->local.mac, argv[argind], NULL);
            if (res != 0)
            {
                fprintf(stderr, "Failed to parse local MAC address (argv[%d]=%s)\n", argind, argv[argind]);
                exit(EXIT_FAILURE);
            }
            break;
        case 2:
            res = addr_ip_set(&conf->remote.ip, argv[argind]);
            if (res != 0)
            {
                fprintf(stderr, "Failed to parse remote IP address (argv[%d]=%s)\n", argind, argv[argind]);
                exit(EXIT_FAILURE);
            }
            break;
        case 3:
            // For remote MAC addresses, no interface can be specified of course
            res = addr_mac_set(&conf->remote.mac, argv[argind], NULL);
            if (res != 0)
            {
                fprintf(stderr, "Failed to parse remote MAC address (argv[%d]=%s)\n", argind, argv[argind]);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            /* Should never happen, other parameters are ignored */
            assert(false);
            break;
        }

        ++argind;
        ++parameter_index;
    }

    return argind;
}

/**
 * Open an UDP socket descriptor.
 *
 * \param sock_fd will be filled with the actual file descriptor of the opened
 * socket.
 * 
 * \param local_addr points to a pair ip address/port number to be
 * binded with the new socket.
 * 
 * \param flags used to set options on the new
 * socket, optional, see fcntl.
 *
 * \return 0 on success, non-zero otherwise.
 * */
static inline int sock_create_dgram(
    int *sock_fd,
    const struct sockaddr_in *local_addr,
    uint32_t flags)
{
    int res;

    res = socket(AF_INET, SOCK_DGRAM, 0);
    if (res < 0)
    {
        perror("Could not create socket");
        return res;
    }

    *sock_fd = res;

    res = bind(*sock_fd, (const struct sockaddr *)local_addr, sizeof(*local_addr));
    if (res < 0)
    {
        perror("Could not bind to local ip/port");
        close(*sock_fd);
        return res;
    }

    // TODO: check if this should be before or after bind or it does not mind
    if (flags)
    {
        int oldflags = fcntl(*sock_fd, F_GETFL, 0);
        res = fcntl(*sock_fd, F_SETFL, oldflags | flags);

        if (res < 0)
        {
            perror("Could not set file descriptor flags");
            close(*sock_fd);

            return res;
        }
    }

    return 0;
}

/**
 * Open a raw socket descriptor.
 *
 * \param sock_fd will be filled with the actual file descriptor of the opened
 * socket.
 * 
 * \param ifname the name of the interface to be used with the new raw
 * socket.
 * 
 * \param flags used to set options on the new socket, optional, see
 * fcntl.
 *
 * \return 0 on success, non-zero otherwise.
 * */
static inline int sock_create_raw(
    int *sock_fd,
    const char *ifname,
    uint32_t flags)
{
    int res;

    struct sockaddr_ll ll; /* Link-layer socket address descriptor */

    /* Initialize structures */
    memset(&ll, 0, sizeof(struct sockaddr_ll));

    /* Open a raw socket */
    res = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (res < 0)
    {
        perror("ERR: socket failed");
        return -1;
    }

    *sock_fd = res;

    /* Set bind options for the given socket. To be more precise, we bind to any
     * possible address/protocol that can be used on the given interface
     * */
    memset(&ll, 0, sizeof(ll));
    ll.sll_family = AF_PACKET;
    ll.sll_protocol = htons(ETH_P_ALL);
    ll.sll_ifindex = if_nametoindex(ifname);

    res = bind(*sock_fd, (struct sockaddr *)&ll, sizeof(struct sockaddr_ll));
    if (res < 0)
    {
        perror("ERR: failed to bind with interface");
        close(*sock_fd);
        return -3;
    }

    // TODO: check if this should be before or after bind or it does not mind
    if (flags)
    {
        int oldflags = fcntl(*sock_fd, F_GETFL, 0);
        res = fcntl(*sock_fd, F_SETFL, oldflags | flags);

        if (res < 0)
        {
            perror("Could not set file descriptor flags");
            close(*sock_fd);

            return res;
        }
    }

    return 0;
}

/* ---------------------------- Public Functions ---------------------------- */

/**
 * Initialize configuration with the given default parameters.
 * 
 * NOTICE: this will revert a configuration to default parameters even in the
 * case in which no defaults are supplied (NULL).
 * 
 * \param conf the configuration to be initialized.
 * \param defaults values to be used to fill mac address, ip address and port
 * numbers, if provided. Can be NULL. 
 * 
 * */
void config_initialize(struct config *conf, const struct config_defaults *defaults)
{
    memcpy(conf, &CONFIG_STRUCT_INITIALIZER, sizeof(struct config));

    if (!defaults)
        return;

    addr_mac_set(&conf->local.mac, defaults->local.mac, NULL);
    addr_ip_set(&conf->local.ip, defaults->local.ip);
    addr_port_number_set(&conf->local.ip, defaults->local.port_number);

    addr_mac_set(&conf->remote.mac, defaults->remote.mac, NULL);
    addr_ip_set(&conf->remote.ip, defaults->remote.ip);
    addr_port_number_set(&conf->remote.ip, defaults->remote.port_number);
}

/**
 * Parse application parameters.
 *
 * NOTICE: Assumes that the first argument is the command name (which is NOT the
 * program name, but instead which command shall be executed inside this
 * program, since this is developed to be a busy-box like application).
 *
 * \param argc the number of arguments.
 * \param argv the array of arguments.
 * \param conf the pointer to the program configuration structure.
 *
 * \return This function returns the total number of arguments processed. After
 * this call, either the return value is greater or equal to argc, or it is the
 * index of the first argument equal to "--".
 * */
int config_parse_arguments(struct config *conf, int argc, char *argv[])
{
    int argind = 1;
    char *cmdname = argv[0];

    if (strstr(cmdname, "dpdk-") != NULL)
    {
        // Will use DPDK
        conf->sock_type = NFV_SOCK_DPDK;
    }
    else
    {
        // Will use regular sockets... if RAW sockets will be used, the option
        // -R will change this parameter to SOCK_RAW
        conf->sock_type = NFV_SOCK_DGRAM;
    }

    // TODO: there is no way to change port number from the command line

    /* This loop keeps alternating between options and non-option arguments,
     * until either -- is found or no arguments are left.
     *  */
    while (argind < argc && !check_doubledash(argv[argind]))
    {
        argind = options_parse(argc, argv, conf, argind);
        argind = args_parse(argc, argv, conf, argind);
    }

    // addr_port_number_set(conf->local.ip, conf->local.port_number);
    // addr_port_number_set(conf->remote.ip, conf->remote.port_number);

    // TODO: Copy probably not necessary, but whatever
    if (argind < argc)
        argv[argind] = cmdname;

    return argind;
}

#define UNUSED(x) ((void)x)
int config_initialize_socket(struct config *conf, int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    // TODO: change constants here
    switch (conf->sock_type)
    {
    case NFV_SOCK_DGRAM:
        // TODO: additional flags?
        return sock_create_dgram(&conf->sock_fd, &conf->local.ip, 0); // TODO: initialize data structures too
    case NFV_SOCK_RAW:
        // TODO: additional flags?
        return sock_create_raw(&conf->sock_fd, conf->local_interf, 0); // TODO: initialize data structures too
    case NFV_SOCK_DPDK:
        // TODO: initialize DPDK stuff here
        // return dpdk_init(argc, argv, &conf);
        return 0;
    default:
        break;
    }

    assert(false);
    return -1;
}

/**
 * Print the current configuration to stdout.
 * */
void config_print(struct config *conf)
{
    printf("CONFIGURATION\n");
    printf("-------------------------------------\n");
    printf("rate (pps)\t%lu\n", conf->rate);
    printf("pkt size\t%lu\n", conf->pkt_size);
    printf("bst size\t%lu\n", conf->bst_size);

    printf("sock type\t");
    switch (conf->sock_type)
    {
    case NFV_SOCK_DGRAM:
        printf("udp");
        break;
    case NFV_SOCK_RAW:
        printf("raw");
        break;
    case NFV_SOCK_DPDK:
        printf("dpdk");
        break;
    default:
        printf("ERROR!");
        break;
    }
    printf("\n");

    ipaddr_str ipstr;
    macaddr_str macstr;
    int portn;

    portn = addr_port_number_get(&conf->local.ip);
    printf("port local\t%d\n", portn);

    portn = addr_port_number_get(&conf->remote.ip);
    printf("port remote\t%d\n", portn);

    addr_ip_get(ipstr, &conf->local.ip);
    printf("ip local\t%s\n", ipstr);

    addr_ip_get(ipstr, &conf->remote.ip);
    printf("ip remote\t%s\n", ipstr);

    addr_mac_get(macstr, &conf->local.mac);
    printf("mac local\t%s\n", macstr);

    addr_mac_get(macstr, &conf->remote.mac);
    printf("mac remote\t%s\n", macstr);

    printf("using mmmsg API\t%s\n", conf->use_mmsg ? "yes" : "no");
    printf("silent\t\t%s\n", conf->silent ? "yes" : "no");
    printf("touch data\t%s\n", conf->touch_data ? "yes" : "no");

    printf("-------------------------------------\n");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// // TODO: MAKE THIS THE SINGLE POINT TO CREATE THE "SOCKET" FOR THE MAIN FUNCTION
// /**
//  * Creates an nfv_socket structure from the given configuration.
//  * */
// static inline int sock_create(struct config *conf, uint32_t flags)
// {
//     switch (conf->socktype)
//     {
//     case SOCK_DGRAM:
//         return sock_create_dgram(&conf->sock_fd, &conf->local.ip, 0); // TODO: flags?
//     case SOCK_RAW:
//         return sock_create_raw(&conf->sock_fd, conf->local_interf, flags);
//     }

//     assert(false);
//     return -1;
// }