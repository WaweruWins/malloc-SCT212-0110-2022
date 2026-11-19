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
static fit_policy_t policy = FIT_FIRST;

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
    block_t *best = NULL;
    for (block_t *b = head; b != NULL; b = b->next)
    {
        if (!b->free || b->size < need)
            continue;
        if (policy == FIT_FIRST)
            return b; /* first block that fits */
        if (best == NULL || b->size < best->size)
            best = b; /* smallest block that fits */
    }
    return best;
}
/* ---------- Task 4: splitting ---------- */
static void split(block_t *b, size_t need)
{
    if (b->size < need + HDR + MIN_PAYLOAD)
        return;                                             /* remainder too small: give it all */
    block_t *rest = (block_t *)((char *)payload(b) + need); /* char* arithmetic */
    rest->size = b->size - need - HDR;
    rest->free = 1;
    rest->magic = MAGIC;
    rest->next = b->next;
    b->next = rest;
    b->size = need;
    if (tail == b)
        tail = rest;
}
/* ---------- Task 5: free and coalesce ---------- */
static void msg(const char *s) /* no stdio inside the allocator */
{
    if (write(STDERR_FILENO, s, strlen(s)) < 0)
    { /* nothing more we can do */
    }
}

static void coalesce(void)
{
    block_t *b = head;
    while (b != NULL && b->next != NULL)
    {
        block_t *n = b->next;
        if (b->free && n->free && adjacent(b, n))
        { /* list neighbours AND memory neighbours */
            b->size += HDR + n->size;
            b->next = n->next;
            n->magic = 0; /* old header is now just payload */
            if (tail == n)
                tail = b;
        }
        else
        {
            b = n; /* only advance when nothing merged */
        }
    }
}

static int valid_ptr(void *ptr)
{
    if (head == NULL || (uintptr_t)ptr % ALIGN != 0)
        return 0;
    if ((char *)ptr < (char *)payload(head) || (char *)ptr >= (char *)sbrk(0))
        return 0; /* outside our heap: don't touch it */
    return header(ptr)->magic == MAGIC;
}
/* ---------- API stubs (each task replaces some of these) ---------- */
void *my_malloc(size_t size)
{
    if (size == 0 || size > SIZE_MAX / 2) /* 0 → NULL; huge → avoid overflow */
        return NULL;
    size_t need = align_up(size);

    block_t *b = find_fit(need);
    if (b == NULL)
    {
        b = grow(need);
        if (b == NULL)
            return NULL;
    }
    split(b, need);
    b->free = 0;
    return payload(b); /* the PAYLOAD, never the header */
}
void my_free(void *ptr)
{
    if (ptr == NULL)
        return;
    if (!valid_ptr(ptr))
    {
        msg("my_free: invalid pointer\n");
        return;
    }
    block_t *b = header(ptr);
    if (b->free)
    {
        msg("my_free: double free detected\n");
        return;
    }
    b->free = 1;
    coalesce();
}
/* ---------- Task 6: calloc and realloc ---------- */
void *my_calloc(size_t nmemb, size_t size)
{
    if (nmemb != 0 && size > SIZE_MAX / nmemb)
        return NULL; /* nmemb * size would overflow */
    size_t total = nmemb * size;
    void *p = my_malloc(total);
    if (p != NULL)
        memset(p, 0, total);
    return p;
}

void *my_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return my_malloc(size);
    if (size == 0)
    {
        my_free(ptr);
        return NULL;
    }
    if (!valid_ptr(ptr) || header(ptr)->free || size > SIZE_MAX / 2)
        return NULL;

    block_t *b = header(ptr);
    size_t need = align_up(size);

    if (need <= b->size)
    { /* still fits: shrink in place */
        split(b, need);
        coalesce(); /* new tail may touch a free block */
        return ptr;
    }
    block_t *n = b->next;
    if (n != NULL && n->free && adjacent(b, n) && b->size + HDR + n->size >= need)
    {
        b->size += HDR + n->size; /* absorb the free neighbour */
        b->next = n->next;
        n->magic = 0;
        if (tail == n)
            tail = b;
        split(b, need);
        return ptr;
    }
    void *np = my_malloc(size); /* otherwise move */
    if (np == NULL)
        return NULL;
    memcpy(np, ptr, b->size < size ? b->size : size); /* copy min(old, new) */
    my_free(ptr);
    return np;
}
/* ---------- Task 7: policy, stats, dump, check ---------- */
void set_policy(fit_policy_t p)
{
    policy = p;
}

void heap_stats(heap_stats_t *out)
{
    if (out == NULL)
        return;
    memset(out, 0, sizeof *out);
    out->heap_bytes = heap_total;
    for (block_t *b = head; b != NULL; b = b->next)
    {
        out->blocks++;
        if (b->free)
        {
            out->free_bytes += b->size;
            out->free_blocks++;
            if (b->size > out->largest_free)
                out->largest_free = b->size;
        }
        else
        {
            out->used_bytes += b->size;
        }
    }
}

void heap_dump(void)
{
    char line[96];
    size_t i = 0;
    for (block_t *b = head; b != NULL; b = b->next, i++)
    {
        int n = snprintf(line, sizeof line, "[%2zu] %p size=%zu %s\n",
                         i, payload(b), b->size, b->free ? "FREE" : "USED");
        if (n > 0)
        {
            size_t len = (size_t)n < sizeof line ? (size_t)n : sizeof line - 1;
            if (write(STDOUT_FILENO, line, len) < 0)
                return;
        }
    }
}

int heap_check(void)
{
    block_t *last = NULL;
    for (block_t *b = head; b != NULL; b = b->next)
    {
        if (b->magic != MAGIC)
            return 0; /* 1. valid header      */
        if ((uintptr_t)payload(b) % ALIGN != 0)
            return 0; /* 2. aligned payload   */
        if (b->next != NULL)
        {
            if ((char *)b->next < (char *)payload(b) + b->size)
                return 0; /* 3. increasing order  */
            if (b->free && b->next->free && adjacent(b, b->next))
                return 0; /* 4. no unmerged pair  */
        }
        last = b;
    }
    return last == tail;
}