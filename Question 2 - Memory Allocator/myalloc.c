#define _DEFAULT_SOURCE

#include "myalloc.h"

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#define ALIGNMENT 16
#define MAGIC 0xCAFEBABE
#define HEAP_CHUNK_SIZE (64 * 1024)

typedef struct block {
    size_t size;
    struct block *next;
    struct block *prev;
    int free;
    uint32_t magic;
} block_t;

static block_t *heap_head = NULL;
static block_t *heap_tail = NULL;

static fit_policy_t allocation_policy = FIT_FIRST;
static size_t total_heap_bytes = 0;

#define HEADER_SIZE \
    ((sizeof(block_t) + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1))

static size_t align_up(size_t size)
{
    if (size > SIZE_MAX - (ALIGNMENT - 1)) {
        return 0;
    }

    return (size + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
}

static void *get_payload(block_t *block)
{
    return (void *)((char *)block + HEADER_SIZE);
}

static int validate_block(block_t *block)
{
    if (block == NULL) {
        return 0;
    }

    if ((uintptr_t)block % ALIGNMENT != 0) {
        return 0;
    }

    if (block->magic != MAGIC || block->size == 0) {
        return 0;
    }

    if (block->size % ALIGNMENT != 0) {
        return 0;
    }

    if (block->free != 0 && block->free != 1) {
        return 0;
    }

    return 1;
}

static block_t *find_block_for_payload(void *ptr)
{
    block_t *current = heap_head;

    while (current != NULL) {
        if (!validate_block(current)) {
            return NULL;
        }

        if (get_payload(current) == ptr) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

static int blocks_are_adjacent(block_t *first, block_t *second)
{
    uintptr_t first_end = (uintptr_t)get_payload(first);

    if (first->size > UINTPTR_MAX - first_end) {
        return 0;
    }

    first_end += first->size;

    return first_end == (uintptr_t)second;
}

static void merge_with_next(block_t *block)
{
    block_t *next = block->next;

    if (next == NULL || !next->free) {
        return;
    }

    if (!blocks_are_adjacent(block, next)) {
        return;
    }

    if (next->size > SIZE_MAX - HEADER_SIZE ||
        block->size > SIZE_MAX - HEADER_SIZE - next->size) {
        return;
    }

    block->size += HEADER_SIZE + next->size;
    block->next = next->next;

    if (block->next != NULL) {
        block->next->prev = block;
    } else {
        heap_tail = block;
    }

    next->magic = 0;
}

static void coalesce_blocks(block_t *block)
{
    if (block == NULL || !block->free) {
        return;
    }

    /*
     * Existing tail-block behavior is preserved.
     * We will revisit the outstanding coalescing issue later.
     */
    if (block == heap_tail) {
        return;
    }

    if (block->prev != NULL &&
        block->prev->free &&
        blocks_are_adjacent(block->prev, block)) {

        block = block->prev;
        merge_with_next(block);
    }

    while (block->next != NULL &&
           block->next->free &&
           blocks_are_adjacent(block, block->next)) {

        merge_with_next(block);
    }
}

/* Find a suitable free block using the selected policy */
static block_t *find_free_block(size_t size)
{
    block_t *current = heap_head;
    block_t *best = NULL;

    while (current != NULL) {
        if (!validate_block(current)) {
            return NULL;
        }

        if (current->free && current->size >= size) {

            /*
             * First-fit returns the first suitable free block.
             */
            if (allocation_policy == FIT_FIRST) {
                return current;
            }

            /*
             * Best-fit searches for the smallest suitable block.
             */
            if (allocation_policy == FIT_BEST) {
                if (best == NULL || current->size < best->size) {
                    best = current;
                }
            }
        }

        current = current->next;
    }

    return best;
}

static void split_block(block_t *block, size_t size)
{
    if (block->size < size) {
        return;
    }

    size_t remaining = block->size - size;

    if (remaining < HEADER_SIZE + ALIGNMENT) {
        return;
    }

    block_t *new_block = (block_t *)
        ((char *)get_payload(block) + size);

    new_block->size = remaining - HEADER_SIZE;
    new_block->next = block->next;
    new_block->prev = block;
    new_block->free = 1;
    new_block->magic = MAGIC;

    if (new_block->next != NULL) {
        new_block->next->prev = new_block;
    } else {
        heap_tail = new_block;
    }

    block->next = new_block;
    block->size = size;
}

/* Allocate memory */
void *my_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    size_t aligned_size = align_up(size);

    if (aligned_size == 0) {
        return NULL;
    }

    /*
     * First, search for an existing free block.
     */
    block_t *block = find_free_block(aligned_size);

    if (block != NULL) {
        split_block(block, aligned_size);
        block->free = 0;

        return get_payload(block);
    }

    /*
     * Calculate the minimum memory required.
     */
    if (aligned_size > SIZE_MAX - HEADER_SIZE) {
        return NULL;
    }

    size_t total_size = HEADER_SIZE + aligned_size;

    /*
     * Request at least 64 KB from the operating system.
     * Larger allocations receive enough memory for the request.
     */
    size_t growth_size = HEAP_CHUNK_SIZE;

    if (total_size > growth_size) {
        growth_size = total_size;
    }

    /*
     * Protect against integer overflow and invalid sbrk increments.
     */
    if (total_heap_bytes > SIZE_MAX - growth_size ||
        growth_size > (size_t)INTPTR_MAX) {
        return NULL;
    }

    /*
     * Request memory from the operating system.
     */
    void *memory = sbrk((intptr_t)growth_size);

    if (memory == (void *)-1) {
        return NULL;
    }

    /*
     * Create one large free block from the new memory.
     */
    block = (block_t *)memory;

    block->size = growth_size - HEADER_SIZE;
    block->next = NULL;
    block->prev = heap_tail;
    block->free = 1;
    block->magic = MAGIC;

    if (heap_head == NULL) {
        heap_head = block;
    } else {
        heap_tail->next = block;
    }

    heap_tail = block;

    total_heap_bytes += growth_size;

    /*
     * Split the large block to satisfy the requested allocation.
     */
    split_block(block, aligned_size);
    block->free = 0;

    return get_payload(block);
}

/* Free allocated memory */
void my_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    block_t *block = find_block_for_payload(ptr);

    if (block == NULL || block->free) {
        return;
    }

    block->free = 1;

    coalesce_blocks(block);
}

/* Allocate and initialize memory to zero */
void *my_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    if (nmemb > SIZE_MAX / size) {
        return NULL;
    }

    size_t total_size = nmemb * size;

    void *ptr = my_malloc(total_size);

    if (ptr == NULL) {
        return NULL;
    }

    memset(ptr, 0, total_size);

    return ptr;
}

/* Resize an allocated memory block */
void *my_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return my_malloc(size);
    }

    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    block_t *block = find_block_for_payload(ptr);

    if (block == NULL || block->free) {
        return NULL;
    }

    size_t aligned_size = align_up(size);

    if (aligned_size == 0) {
        return NULL;
    }

    if (aligned_size <= block->size) {
        return ptr;
    }

    void *new_ptr = my_malloc(size);

    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_size = block->size;

    if (copy_size > size) {
        copy_size = size;
    }

    memcpy(new_ptr, ptr, copy_size);

    my_free(ptr);

    return new_ptr;
}

/* Collect heap statistics */
void heap_stats(heap_stats_t *out)
{
    if (out == NULL) {
        return;
    }

    out->heap_bytes = total_heap_bytes;
    out->used_bytes = 0;
    out->free_bytes = 0;
    out->blocks = 0;
    out->free_blocks = 0;
    out->largest_free = 0;

    block_t *current = heap_head;

    while (current != NULL) {
        if (!validate_block(current)) {
            break;
        }

        out->blocks++;

        if (current->free) {
            out->free_blocks++;
            out->free_bytes += current->size;

            if (current->size > out->largest_free) {
                out->largest_free = current->size;
            }
        } else {
            out->used_bytes += current->size;
        }

        current = current->next;
    }
}

/* Display information about all memory blocks */
void heap_dump(void)
{
    block_t *current = heap_head;
    size_t number = 1;

    printf("\n========== HEAP DUMP ==========\n");

    while (current != NULL) {
        if (!validate_block(current)) {
            printf("Invalid memory block detected.\n");
            break;
        }

        printf("Block %zu\n", number);
        printf("Address: %p\n", (void *)current);
        printf("Size: %zu bytes\n", current->size);
        printf("Status: %s\n",
               current->free ? "FREE" : "USED");
        printf("------------------------------\n");

        current = current->next;
        number++;
    }

    printf("Total heap memory: %zu bytes\n", total_heap_bytes);
    printf("========== END DUMP ==========\n");
}

/* Change allocation policy */
void set_policy(fit_policy_t p)
{
    if (p == FIT_FIRST || p == FIT_BEST) {
        allocation_policy = p;
    }
}

/* Check heap integrity */
int heap_check(void)
{
    block_t *current = heap_head;
    block_t *previous = NULL;
    size_t calculated_bytes = 0;

    if (heap_head == NULL) {
        return heap_tail == NULL && total_heap_bytes == 0;
    }

    if (heap_tail == NULL || heap_head->prev != NULL) {
        return 0;
    }

    while (current != NULL) {
        if (!validate_block(current)) {
            return 0;
        }

        if (current->prev != previous) {
            return 0;
        }

        if (current->size > SIZE_MAX - HEADER_SIZE) {
            return 0;
        }

        size_t block_total = HEADER_SIZE + current->size;

        if (calculated_bytes > SIZE_MAX - block_total) {
            return 0;
        }

        calculated_bytes += block_total;

        previous = current;
        current = current->next;
    }

    if (previous != heap_tail) {
        return 0;
    }

    if (heap_tail->next != NULL) {
        return 0;
    }

    if (calculated_bytes != total_heap_bytes) {
        return 0;
    }

    return 1;
}
