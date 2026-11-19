#define _DEFAULT_SOURCE            /* must come before any #include, so sbrk() is declared */
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "myalloc.h"

/* ---------- Task 1: block header and helpers ---------- */
typedef struct block {
    size_t        size;    /* payload size in bytes             */
    struct block *next;    /* next block, in increasing address */
    int           free;    /* 1 = free, 0 = in use              */
    int           magic;   /* constant to detect bad pointers   */
} block_t;

#define ALIGN       16
#define MAGIC       0x4D414C43     /* "MALC" */
#define MIN_PAYLOAD 16
#define CHUNK       (64 * 1024)
#define HDR         ((sizeof(block_t) + (ALIGN - 1)) & ~(size_t)(ALIGN - 1))

static size_t   align_up(size_t n)  { return (n + (ALIGN - 1)) & ~(size_t)(ALIGN - 1); }
static void    *payload(block_t *b) { return (char *)b + HDR; }
static block_t *header(void *p)     { return (block_t *)((char *)p - HDR); }

/* a's payload ends exactly where b's header starts */
static int adjacent(block_t *a, block_t *b)
{
    return (char *)payload(a) + a->size == (char *)b;
}

/* ---------- API stubs (each task replaces some of these) ---------- */
void *my_malloc(size_t size)                 { (void)size; return NULL; }
void  my_free(void *ptr)                     { (void)ptr; }
void *my_calloc(size_t nmemb, size_t size)   { (void)nmemb; (void)size; return NULL; }
void *my_realloc(void *ptr, size_t size)     { (void)ptr; (void)size; return NULL; }
void  heap_stats(heap_stats_t *out)          { if (out) memset(out, 0, sizeof *out); }
void  heap_dump(void)                        { }
void  set_policy(fit_policy_t p)             { (void)p; }
int   heap_check(void)                       { return 1; }