
#include "myalloc.h"
#include <stdio.h>

int main(void)
{
    printf("Testing memory allocator diagnostics\n");

    void *a = my_malloc(100);
    void *b = my_malloc(200);

    if (a == NULL || b == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }

    my_free(a);

    heap_stats_t stats;
    heap_stats(&stats);

    printf("\nHeap Statistics:\n");
    printf("Total heap bytes: %zu\n", stats.heap_bytes);
    printf("Used bytes: %zu\n", stats.used_bytes);
    printf("Free bytes: %zu\n", stats.free_bytes);
    printf("Total blocks: %zu\n", stats.blocks);
    printf("Free blocks: %zu\n", stats.free_blocks);
    printf("Largest free block: %zu\n", stats.largest_free);

    printf("\nHeap integrity: %s\n",
           heap_check() ? "VALID" : "INVALID");

    printf("\nHeap Dump:\n");
    heap_dump();

    printf("\nTesting First Fit policy...\n");
    set_policy(FIT_FIRST);

    printf("Testing Best Fit policy...\n");
    set_policy(FIT_BEST);

    my_free(b);

    printf("\nFinal heap integrity: %s\n",
           heap_check() ? "VALID" : "INVALID");

    return 0;
}
