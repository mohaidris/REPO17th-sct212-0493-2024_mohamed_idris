
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARGS 100

// Split the command entered by the user into arguments
void parse_command(char *line, char *args[])
{
    int i = 0;

    char *token = strtok(line, " \t\n");

    while (token != NULL && i < MAX_ARGS - 1)
    {
        args[i] = token;
        i++;

        token = strtok(NULL, " \t\n");
    }

    args[i] = NULL;
}

int main(void)
{
    char *line = NULL;
    size_t line_size = 0;
    ssize_t characters_read;

    char *args[MAX_ARGS];

    printf("Welcome to Mini Shell!\n");

    while (1)
    {
        printf("msh[0]> ");
        fflush(stdout);

        characters_read = getline(&line, &line_size, stdin);

        if (characters_read == -1)
        {
            if (feof(stdin))
            {
                printf("\nGoodbye!\n");
                break;
            }

            perror("getline");
            clearerr(stdin);
            continue;
        }

        // Separate the command into individual arguments
        parse_command(line, args);

        // Ignore an empty command
        if (args[0] == NULL)
        {
            continue;
        }

        printf("Command: %s\n", args[0]);

        for (int i = 1; args[i] != NULL; i++)
        {
            printf("Argument %d: %s\n", i, args[i]);
        }
    }

    free(line);

    return 0;
}
