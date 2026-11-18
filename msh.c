
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_ARGS 100

int last_status = 0;

void handle_sigint(int signal_number) {
    (void)signal_number;
    write(STDOUT_FILENO, "\nUse 'exit' to quit the shell.\n", 31);
}

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
    printf("  command &       - Run command in background\n");
    printf("  command > file  - Redirect output to a file\n");
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

void execute_command(char **args, int background) {
    char *output_file = NULL;
    int i = 0;

    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL || strcmp(args[i + 1], "&") == 0) {
                fprintf(stderr, "Error: missing output filename\n");
                last_status = 1;
                return;
            }

            output_file = args[i + 1];
            args[i] = NULL;
            break;
        }
        i++;
    }

    if (args[0] == NULL) {
        fprintf(stderr, "Error: missing command\n");
        last_status = 1;
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        last_status = 1;
        return;
    }

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);

        if (output_file != NULL) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd < 0) {
                perror("open");
                exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                close(fd);
                exit(EXIT_FAILURE);
            }

            close(fd);
        }

        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    if (background) {
        printf("[Background process started: PID %d]\n", pid);
        last_status = 0;
        return;
    }

    int status;
    pid_t result;

    do {
        result = waitpid(pid, &status, 0);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        perror("waitpid");
        last_status = 1;
    } else if (WIFEXITED(status)) {
        last_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        last_status = 128 + WTERMSIG(status);
    }
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t capacity = 0;
    char *args[MAX_ARGS];

    printf("Welcome to Mini Shell!\n");

    while (1) {
        printf("msh[%d]> ", last_status);
        fflush(stdout);

        errno = 0;
        ssize_t input_length = getline(&line, &capacity, stdin);

        if (input_length == -1) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }

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
            int i = 0;

            while (args[i] != NULL) {
                i++;
            }

            int background = 0;

            if (i > 0 && strcmp(args[i - 1], "&") == 0) {
                background = 1;
                args[i - 1] = NULL;
            }

            if (args[0] != NULL) {
                execute_command(args, background);
            }
        }
    }

    free(line);
    return 0;
}
