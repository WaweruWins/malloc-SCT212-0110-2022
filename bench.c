#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "myalloc.h"

#define OPS 50000
#define SLOTS 1000
#define SEED 2305

static void *slot[SLOTS];

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* The same random workload, with whichever allocator is passed in. */
static double run(void *(*alloc)(size_t), void (*release)(void *))
{
    memset(slot, 0, sizeof slot);
    srand(SEED);
    double t0 = now();
    for (int i = 0; i < OPS; i++)
    {
        int k = rand() % SLOTS;
        if (slot[k] != NULL)
        {
            release(slot[k]);
            slot[k] = NULL;
        }
        else
        {
            size_t size = 1 + (size_t)(rand() % 4096);
            slot[k] = alloc(size);
            if (slot[k] != NULL)
                memset(slot[k], 0xAB, size);
        }
    }
    return now() - t0;
}

static void free_all(void (*release)(void *))
{
    for (int k = 0; k < SLOTS; k++)
        if (slot[k] != NULL)
        {
            release(slot[k]);
            slot[k] = NULL;
        }
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "first";

    if (strcmp(mode, "libc") == 0)
    {
        double t = run(malloc, free);
        free_all(free);
        printf("libc   time=%.4f s\n", t);
        return 0;
    }

    set_policy(strcmp(mode, "best") == 0 ? FIT_BEST : FIT_FIRST);
    double t = run(my_malloc, my_free);

    heap_stats_t s;
    heap_stats(&s); /* measured while blocks are still live */
    double ext = s.free_bytes ? 1.0 - (double)s.largest_free / (double)s.free_bytes : 0.0;
    printf("%-6s time=%.4f s heap_bytes=%zu free_bytes=%zu free_blocks=%zu "
           "largest_free=%zu ext_frag=%.3f heap_ok=%d\n",
           mode, t, s.heap_bytes, s.free_bytes, s.free_blocks, s.largest_free,
           ext, heap_check());
    free_all(my_free);
    return 0;
}