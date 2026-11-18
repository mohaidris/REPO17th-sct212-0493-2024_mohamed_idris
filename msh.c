
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 100

int last_status = 0;

void parse_command(char *line, char **args) {
    int i = 0;
    char *token = strtok(line, " \t\n");

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }

    args[i] = NULL;
}

void show_help(void) {
    printf("Mini Shell Commands:\n");
    printf("  cd [directory] - Change directory\n");
    printf("  help            - Show this help message\n");
    printf("  exit            - Exit the shell\n");
    printf("You can also run external Linux commands.\n");
}

int handle_builtin(char **args) {
    if (args[0] == NULL) {
        return 0;
    }

    if (strcmp(args[0], "exit") == 0) {
        return 1;
    }

    if (strcmp(args[0], "help") == 0) {
        show_help();
        last_status = 0;
        return 0;
    }

    if (strcmp(args[0], "cd") == 0) {
        char *directory = args[1];

        if (directory == NULL) {
            directory = getenv("HOME");
        }

        if (directory == NULL || chdir(directory) != 0) {
            perror("cd");
            last_status = 1;
        } else {
            last_status = 0;
        }

        return 0;
    }

    return -1;
}

void execute_command(char **args) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        last_status = 1;
        return;
    }

    if (pid == 0) {
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        last_status = 1;
    } else if (WIFEXITED(status)) {
        last_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        last_status = 128 + WTERMSIG(status);
    }
}

int main(void) {
    char *line = NULL;
    size_t capacity = 0;
    char *args[MAX_ARGS];

    printf("Welcome to Mini Shell!\n");

    while (1) {
        printf("msh[%d]> ", last_status);
        fflush(stdout);

        if (getline(&line, &capacity, stdin) == -1) {
            printf("\n");
            break;
        }

        parse_command(line, args);

        if (args[0] == NULL) {
            continue;
        }

        int result = handle_builtin(args);

        if (result == 1) {
            break;
        }

        if (result == -1) {
            execute_command(args);
        }
    }

    free(line);
    return 0;
}
