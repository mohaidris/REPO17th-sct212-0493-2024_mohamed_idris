
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char *line = NULL;
    size_t line_size = 0;
    ssize_t characters_read;

    printf("Welcome to Mini Shell!\n");

    while (1)
    {
        // Display the shell prompt
        printf("msh[0]> ");
        fflush(stdout);

        //read user info
        characters_read = getline(&line, &line_size, stdin);

        // Check whether the user pressed Ctrl+D
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

        // Ignore empty lines for now
        if (characters_read == 1 && line[0] == '\n')
        {
            continue;
        }

        printf("You entered: %s", line);
    }

    // Release the memory allocated by getline
    free(line);

    return 0;
}
