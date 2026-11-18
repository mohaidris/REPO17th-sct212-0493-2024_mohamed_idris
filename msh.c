
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 100

int last_status = 0;

// Split the command into individual arguments
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

// Execute an external command and check its result
void execute_command(char *args[])
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        last_status = 1;
        return;
    }

    if (pid == 0)
    {
        // Child process
        execvp(args[0], args);

        perror("Command execution failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Parent process waits for the child
        int status;

        if (waitpid(pid, &status, 0) == -1)
        {
            perror("waitpid");
            last_status = 1;
        }
        else if (WIFEXITED(status))
        {
            last_status = WEXITSTATUS(status);
        }
        else
        {
            last_status = 1;
        }
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
        // Display the previous command's exit status
        printf("msh[%d]> ", last_status);
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
