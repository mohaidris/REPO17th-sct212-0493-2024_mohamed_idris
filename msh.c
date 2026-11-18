
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 100

// Separate the command into arguments
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

// Execute an external Linux command
void execute_command(char *args[])
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return;
    }

    if (pid == 0)
    {
        // Child process executes the command
        execvp(args[0], args);

        // This runs only if execvp fails
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent waits for the child process
        waitpid(pid, NULL, 0);
    }
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

        parse_command(line, args);

        if (args[0] == NULL)
        {
            continue;
        }

        execute_command(args);
    }

    free(line);

    return 0;
}
