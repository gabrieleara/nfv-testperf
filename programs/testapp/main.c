#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

int run_command(int argc, char *argv[])
{
    int i;

    for (i = 0; i < num_commands; ++i)
    {
        if (strcmp(argv[0], commands_n[i]) == 0)
            break;
    }

    if (i == num_commands)
    {
        fprintf(stderr, "Wrong command name: %s.\n", argv[0]);
        return EXIT_FAILURE;
    }

    return commands_f[i](argc, argv);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        // IDEA: PRINT USAGE, DOES NOTHING
        return EXIT_SUCCESS;
    }

    return run_command(argc - 1, argv + 1);
}
