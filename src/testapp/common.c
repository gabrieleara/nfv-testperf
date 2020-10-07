/* ******************** INCLUDES ******************** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <strings.h>
#include <getopt.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <rte_ether.h>

#include "newcommon.h"
#include "stats.h"

// FIXME: ORGANIZE THIS MESS!

/* ******************** CONSTANTS ******************** */

const struct config CONFIG_STRUCT_INITIALIZER = {
    .rate = DEFAULT_RATE,
    .pkt_size = DEFAULT_PKT_SIZE,
    .bst_size = DEFAULT_BST_SIZE,
    .sock_fd = -1,
    .socktype = SOCK_NONE,
    .local_port = 0,
    .remote_port = 0,
    .local_mac = "",
    .remote_mac = "",
    .local_ip = "",
    .remote_ip = "",
    .local_interf = "eth0",
    .use_block = false,
    .use_mmsg = false,
    .silent = false,
    .touch_data = false,
    .direction = DIRECTION_TXRX,
};

const char usage_format_string[] =
    "USAGE\n"
    "\t%s <program-options-unordered-list> <addresses-ordered-list> -- <dpdk-parameters>\n"
    "\n"
    "Where:\n"
    " - <program-options-unordered-list> is a list of non-ordered parameters (see PARAMETERS section);\n"
    " - <adresses-ordered-list> is a list of IP and MAC addresses used by the program\n"
    "\t(see ADDRESSES section);\n"
    " - <dpdk-parameters> is a list of parameters for EAL DPDK environment\n"
    "\t(see online - used only by programs starting with 'dpdk-').\n"
    "\n"
    "PARAMETERS\n"
    "\n"
    "    -r <rate=1000000>\tThe sending/receiving rate in pps.\n"
    "    -p <packet_size=64>\tThe size of each frame in bytes.\n"
    "    -b <burst_size=32>\tThe size of each packet burst in number of packets.\n"
    "\n"
    "    -c\t\t\tGenerate actual data/Calculate a checksum on each received payload.\n"
    "\t\t\tThis ensures that each byte in a message payload is actually touched.\n"
    "\n"
    "    -R <interf_name>\tUse RAW sockets instead of UDP ones (default are UDP sockets).\n"
    "\t\t\tThe argument is the name of the interface to use.\n"
    "\t\t\tValid only for sockets-based programs, not DPDK ones.\n"
    "\n"
    "    -B\t\t\tUse blocking sockets instead of nonblocking ones.\n"
    "\t\t\tValid only for sockets-based programs, not DPDK ones.\n"
    "\n"
    "    -m\t\t\tUse SENDMMSG/RECVMMSG API to exchange packets.\n"
    "\t\t\tValid only for sockets-based programs, not DPDK ones.\n"
    "\n"
    "    -s\t\t\tRun in silent mode. Prints no stats until the termination SIGINT is received.\n"
    "\n"
    "\n"
    "ADDRESSES\n"
    "\n"
    "\tThe application expects some addresses to be given as argument in a specific order.\n"
    "\tThe following is the list of each address expected.\n"
    "\tDefault values will be used if not all addresses are provided and not all parameters are always used.\n"
    "\n"
    "\tSince the program will communicate with another application, we will refer as this\n"
    "\tprogram as the LOCAL application and use the REMOTE term to indicate the other one.\n"
    "\n"
    "\t<LOCAL_IP> <LOCAL_MAC> <REMOTE_IP> <REMOTE_MAC> \n"
    "\n";

/* ******************** PRIVATE ******************** */

static inline bool check_doubledash(char *s)
{
    return strcmp(s, "--") == 0;
}

static inline bool check_dash(char *s)
{
    return s[0] == '-';
}

// NOTICE: some of the options/parameters are effectively ignored by some of the
// programs
static inline int options_parse(int argc, char *argv[], struct config *conf, int argind)
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
            // NOTICE: option is ignored by DPDK-based programs
            conf->socktype = SOCK_RAW;
            strncpy(conf->local_interf, optarg, sizeof(conf->local_interf - 1));
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

static inline int addr_ip_set(struct sockaddr_in *addr, const char *ip, int port)
{
    int res;

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    res = inet_aton(ip, &addr->sin_addr);
    if (res == 0)
    {
        return -1;
    }

    return 0;
}

static inline int macaddr_parse(byte_t *addr, const char *mac)
{
    int res;

    res = sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", addr,
                 addr + 1,
                 addr + 2,
                 addr + 3,
                 addr + 4,
                 addr + 5);

    if (res != 6)
    {
        memset(addr, 0, 6);
        return -1;
    }

    return 0;
}

int addr_mac_set(struct sockaddr_ll *addr, const char *mac, const char *ifname)
{
    memset(addr, 0, sizeof(struct sockaddr_ll));
    addr->sll_family = AF_PACKET;
    addr->sll_protocol = htons(ETH_P_ALL);
    addr->sll_ifindex = (ifname == NULL) ? 0 : if_nametoindex(ifname);
    addr->sll_halen = ETHER_ADDR_LEN;

    return macaddr_parse(addr->sll_addr, mac);

    // setting mac address here
    // sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac_ptr[0], &mac_ptr[1], &mac_ptr[2], &mac_ptr[3], &mac_ptr[4], &mac_ptr[5]);
}

static inline int args_parse(int argc, char *argv[], struct config *conf, int argind)
{
    static int parameter_index = 0;

    // If first argument is "-<something>", return immediately
    while (argind < argc && !check_dash(argv[argind]))
    {
        // Order is: local_ip, local_mac, remote_ip, remote_mac
        switch (parameter_index)
        {
        case 0:
            strcpy(conf->local_ip, argv[argind]);
            break;
        case 1:
            strcpy(conf->local_mac, argv[argind]);
            break;
        case 2:
            strcpy(conf->remote_ip, argv[argind]);
            break;
        case 3:
            strcpy(conf->remote_mac, argv[argind]);
            break;
        default:
            // Should never happen, other parameters are ignored
            break;
        }

        ++argind;
        ++parameter_index;
    }

    return argind;
}

static inline int sock_create_dgram(
    int *sock_fd,
    char *local_ip, int local_port,
    char *remote_ip, int remote_port,
    uint32_t flags,
    bool toconnect)
{
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int res;

    res = socket(AF_INET, SOCK_DGRAM, 0);
    if (res < 0)
    {
        perror("Could not create socket");
        return res;
    }

    *sock_fd = res;

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

    addr_ip_set(&local_addr, local_ip, local_port);

    res = bind(*sock_fd,
               (struct sockaddr *)&local_addr,
               sizeof(local_addr));
    if (res < 0)
    {
        perror("Could not bind to local ip/port");
        close(*sock_fd);
        return res;
    }

    if (toconnect)
    {
        addr_ip_set(&remote_addr, remote_ip, remote_port);

        int res = connect(*sock_fd,
                          (struct sockaddr *)&remote_addr,
                          sizeof(remote_addr));
        if (res < 0)
        {
            perror("Could not connect to remote ip/port");
            close(*sock_fd);
            return res;
        }
    }

    return 0;
}

#define UNUSED(v) ((void)v)

// Create a raw socket, returns the file descriptor on success, negative error
// otherwise
int raw_socket_open(const char *ifname)
{
    int sock_fd;
    int res;

    struct sockaddr_ll ll; // Link-layer socket address descriptor
    struct ifreq ifr;      // Structure used to configure the network device

    // initialize structures
    memset(&ifr, 0, sizeof(struct ifreq));
    memset(&ll, 0, sizeof(struct sockaddr_ll));

    // open a raw socket
    sock_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock_fd < 0)
    {
        perror("ERR: socket failed");
        return -1;
    }

    // set bind options for the given interface, actually we bind to any
    // possible address/protocol for that interface
    memset(&ll, 0, sizeof(ll));
    ll.sll_family = AF_PACKET;
    ll.sll_protocol = htons(ETH_P_ALL);
    ll.sll_ifindex = if_nametoindex(ifname);

    res = bind(sock_fd, (struct sockaddr *)&ll, sizeof(struct sockaddr_ll));
    if (res < 0)
    {
        perror("ERR: failed to bind with interface");
        close(sock_fd);
        return -3;
    }

    return sock_fd;
}

static inline int sock_create_raw( //struct config *conf, uint32_t flags, bool toconnect)
    int *sock_fd,
    const char *local_interf,
    // char *local_mac,
    // char *remote_mac,
    uint32_t flags)
{
    int res = 0;

    res = raw_socket_open(local_interf);
    if (res < 0)
    {
        perror("Could not create raw socket");
        return res;
    }

    *sock_fd = res;

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

/* ******************** PUBLIC ******************** */

int parameters_parse(int argc, char *argv[], struct config *conf)
{
    int argind = 1;
    char *cmdname = argv[0];

    // Alternate between parsing options and non-option arguments
    // until either -- is found or no arguments at all are around
    while (argind < argc && !check_doubledash(argv[argind]))
    {
        argind = options_parse(argc, argv, conf, argind);
        argind = args_parse(argc, argv, conf, argind);
    }

    addr_ip_set(&conf->remote_addr.ip, conf->remote_ip, conf->remote_port);
    addr_ip_set(&conf->local_addr.ip, conf->local_ip, conf->local_port);
    addr_mac_set(&conf->remote_addr.mac, conf->remote_mac, NULL);
    addr_mac_set(&conf->local_addr.mac, conf->local_mac, NULL);

    /*    
    for (int i = 0; i < argind; ++i)
    {
        argv[i] = argv[i+1];
    }
*/
    // Copy probably not necessary, but whatever
    if (argind < argc)
        argv[argind] = cmdname;

    return argind;
}

// Set the socket sending buffer size
// Returns the set size on success, a negative error on failure.
// It may set a smaller size than the requested one if it is bigger than the
// maximum allowed. Also it will fall back to a minimum size if a smaller one is
// requested.
int sock_set_sndbuff(int sock_fd, unsigned int size)
{
    int set_value;
    int res;
    socklen_t value_size = sizeof(set_value);

    set_value = size;
    res = setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &set_value, value_size);
    if (res < 0)
    {
        perror("WARN: failed setsockopt");
        return -1;
    }

    set_value = 0;
    res = getsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &set_value, &value_size);
    if (res < 0)
    {
        perror("WARN: failed getsockopt");
        return -2;
    }

    /**
     * NOTICE: from `man 7 socket # SO_SNDBUF`:
     * 
     * Sets or gets the maximum socket send buffer in bytes.
     * The kernel doubles this value (to allow space for bookkeeping  overhead)
     * when it is set using  setsockopt(2), and this doubled value is returned
     * by getsockopt(2).
     * 
     * The default value is set by the /proc/sys/net/core/wmem_default file and
     * the maximum allowed value  is set by the /proc/sys/net/core/wmem_max
     * file.
     * 
     * The minimum (doubled) value for this option is 2048.
     */

    // If the retrieved value is >= 2*size it's all okay
    if ((unsigned)set_value < 2 * size)
    {
        // NOT EXACTLY OKAY, BUT WE WILL MAKE DO
        fprintf(stderr, "WARN: socket send buffer too small: %d < 2*%u !\n",
                set_value, size);
    }

    return set_value / 2;
}

int sock_create(struct config *conf, uint32_t flags, bool toconnect)
{
    switch (conf->socktype)
    {
    case SOCK_DGRAM:
        return sock_create_dgram(&conf->sock_fd,
                                 conf->local_ip, conf->local_port,
                                 // &conf->local_addr.ip,
                                 conf->remote_ip, conf->remote_port,
                                 // &conf->remote_addr.ip,
                                 flags, toconnect);
    case SOCK_RAW:
        return sock_create_raw(
            &conf->sock_fd,
            conf->local_interf,
            flags);
    }

    assert(false);
    return -1;
}

struct stats *stats_ptr = NULL;

// IDEA: add cleanup callback registration

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

void print_config(struct config *conf)
{
    printf("CONFIGURATION\n");
    printf("-------------------------------------\n");
    printf("rate (pps)\t%lu\n", conf->rate);
    printf("pkt size\t%u\n", conf->pkt_size);
    printf("bst size\t%u\n", conf->bst_size);

    printf("sock type\t%s\n", (conf->socktype == SOCK_DGRAM) ? "udp" : "raw");

    printf("port local\t%d\n", conf->local_port);
    printf("port remote\t%d\n", conf->remote_port);

    printf("ip local\t%s\n", conf->local_ip);
    printf("ip remote\t%s\n", conf->remote_ip);

    printf("mac local\t%s\n", conf->local_mac);
    printf("mac remote\t%s\n", conf->remote_mac);

    /*
    char prova[120];

    sprintf(prova, "%02x:%02x:%02x:%02x:%02x:%02x",
        conf->local_addr.mac.sll_addr[0],
        conf->local_addr.mac.sll_addr[1],
        conf->local_addr.mac.sll_addr[2],
        conf->local_addr.mac.sll_addr[3],
        conf->local_addr.mac.sll_addr[4],
        conf->local_addr.mac.sll_addr[5]);

    printf("CHECK mac local\t%s\n", prova);

    sprintf(prova, "%02x:%02x:%02x:%02x:%02x:%02x",
        conf->remote_addr.mac.sll_addr[0],
        conf->remote_addr.mac.sll_addr[1],
        conf->remote_addr.mac.sll_addr[2],
        conf->remote_addr.mac.sll_addr[3],
        conf->remote_addr.mac.sll_addr[4],
        conf->remote_addr.mac.sll_addr[5]);

    printf("CHECK mac remote\t%s\n", prova);
    */

    printf("using mmmsg API\t%s\n", conf->use_mmsg ? "yes" : "no");
    printf("silent\t\t%s\n", conf->silent ? "yes" : "no");
    printf("touch data\t%s\n", conf->touch_data ? "yes" : "no");

    printf("-------------------------------------\n");
}
