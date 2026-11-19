#define _DEFAULT_SOURCE /* must come before any #include, so sbrk() is declared */
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "myalloc.h"

/* ---------- Task 1: block header and helpers ---------- */
typedef struct block
{
    size_t size;        /* payload size in bytes             */
    struct block *next; /* next block, in increasing address */
    int free;           /* 1 = free, 0 = in use              */
    int magic;          /* constant to detect bad pointers   */
} block_t;

#define ALIGN 16
#define MAGIC 0x4D414C43 /* "MALC" */
#define MIN_PAYLOAD 16
#define CHUNK (64 * 1024)
#define HDR ((sizeof(block_t) + (ALIGN - 1)) & ~(size_t)(ALIGN - 1))

static size_t align_up(size_t n) { return (n + (ALIGN - 1)) & ~(size_t)(ALIGN - 1); }
static void *payload(block_t *b) { return (char *)b + HDR; }
static block_t *header(void *p) { return (block_t *)((char *)p - HDR); }

/* a's payload ends exactly where b's header starts */
static int adjacent(block_t *a, block_t *b)
{
    return (char *)payload(a) + a->size == (char *)b;
}

static block_t *head = NULL;
static block_t *tail = NULL;
static size_t heap_total = 0; /* bytes obtained from the OS */

/* ---------- Task 2: grow the heap ---------- */
static block_t *grow(size_t need)
{
    size_t want = need + HDR;
    if (want < CHUNK)
        want = CHUNK; /* at least 64 KB per sbrk */

    void *cur = sbrk(0); /* current break */
    if (cur == (void *)-1)
        return NULL;
    size_t pad = (ALIGN - (uintptr_t)cur % ALIGN) % ALIGN; /* bytes to reach a 16 boundary */

    if (want > (size_t)INTPTR_MAX - pad)
        return NULL;
    void *mem = sbrk((intptr_t)(want + pad));
    if (mem == (void *)-1)
        return NULL;
    heap_total += want + pad;

    block_t *b = (block_t *)((char *)mem + pad);
    b->size = want - HDR;
    b->next = NULL;
    b->free = 1;
    b->magic = MAGIC;

    /* new memory directly after a free tail: extend it instead of adding a block */
    if (tail != NULL && tail->free && adjacent(tail, b))
    {
        tail->size += HDR + b->size;
        b->magic = 0;
        return tail;
    }
    if (head == NULL)
        head = b;
    else
        tail->next = b;
    tail = b;
    return b;
}
/* ---------- Task 3: placement ---------- */
static block_t *find_fit(size_t need)
{
    for (block_t *b = head; b != NULL; b = b->next)
        if (b->free && b->size >= need)
            return b;                                 /* first fit */
    return NULL;
}

/* ---------- API stubs (each task replaces some of these) ---------- */
void *my_malloc(size_t size)
{
    if (size == 0 || size > SIZE_MAX / 2)            /* 0 → NULL; huge → avoid overflow */
        return NULL;
    size_t need = align_up(size);

    block_t *b = find_fit(need);
    if (b == NULL) {
        b = grow(need);
        if (b == NULL)
            return NULL;
    }
    b->free = 0;
    return payload(b);                                /* the PAYLOAD, never the header */
}
void my_free(void *ptr) { (void)ptr; }
void *my_calloc(size_t nmemb, size_t size)
{
    (void)nmemb;
    (void)size;
    return NULL;
}
void *my_realloc(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
    return NULL;
}
void heap_stats(heap_stats_t *out)
{
    if (out)
        memset(out, 0, sizeof *out);
}
void heap_dump(void) {}
void set_policy(fit_policy_t p) { (void)p; }
int heap_check(void) { return 1; }