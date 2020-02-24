#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * Convert CPU cores lists to bitmask.
 *
 * This program converts a CPU cores list to a CPU mask that can be used with
 * the command taskset.
 *
 * Common implementations of the taskset command already accept a list of CPU
 * cores in the same format accepted by this program. However, some
 * implementations (including the one shipped inside the version of busybox that
 * we use to generate container applications) still require a CPU mask.
 *
 * Hence the need of writing this program to be used inside the busyboxes.
 *
 * Example of usage:
 *  \code{.sh}
    $ cpu_list_to_mask 1,3,4-8
        fa
    \endcode
 */
int main(int argc, char *argv[])
{
    if (argc < 2)
        return EXIT_FAILURE;

    typedef unsigned long long cpu_t;

    cpu_t cpu_mask = 0;
    cpu_t cpu_last = 0;
    cpu_t cpu_cur = 0;

    char *str = argv[1];
    char *delim = ",-";
    char delim_last = ',';

    for (char *haschar = strchr(delim, str[0]); haschar != NULL; haschar = strchr(delim, str[0]))
    {
        delim_last = str[0];
        ++str;
    }

    char *copy = strdup(str);
    char *token = strtok(str, delim);

    char *token_last = str;
    int token_last_len = -1;

    while (token)
    {
        cpu_cur = strtol(token, NULL, 0);

        switch (delim_last)
        {
        case ',':
            cpu_mask |= 1ULL << cpu_cur;
            break;
        case '-':
            for (; cpu_last < cpu_cur; ++cpu_last)
                cpu_mask |= 1ULL << cpu_last;
            break;
        }

        cpu_last = cpu_cur;
        delim_last = copy[token + strlen(token) - str];

        if ((token - token_last) != token_last_len + 1)
        {
            return EXIT_FAILURE;
        }

        token_last = token;
        token_last_len = strlen(token);

        token = strtok(NULL, delim);
    }

    printf("%llx\n", cpu_mask);

    return EXIT_SUCCESS;
}
