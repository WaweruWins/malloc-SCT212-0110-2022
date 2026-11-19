# Task 8: Allocator experiment report

**Name:** EDWIN WAWERU · **Student ID:** SCT212-0110/2022

## Method

`bench.c` runs the same random sequence of 50,000 operations (seed 2305) over
1,000 slots: if a slot is empty it allocates 1 to 4,096 bytes and fills them,
otherwise it frees the block. Each policy runs in a fresh process
(`./bench first`, `./bench best`) so neither inherits the other's heap.
Statistics are read with `heap_stats()` at the end of the run, while the
surviving blocks are still allocated. Times use `clock_gettime(CLOCK_MONOTONIC)`.
The same workload is timed on glibc `malloc`/`free` (`./bench libc`).
Build: `gcc -std=c11 -Wall -Wextra -g -O0`. Run with `make bench`.

## Results

| Metric | FIT_FIRST | FIT_BEST |
|---|---|---|
| heap_bytes (from sbrk) | 1,376,256 | 1,310,720 |
| free_bytes | 314,256 | 249,968 |
| free_blocks | 221 | 166 |
| largest_free | 98,928 | 70,480 |
| External fragmentation (1 − largest_free / free_bytes) | 0.685 | 0.718 |
| heap_check() | 1 | 1 |

| Allocator | Time (s) | Relative to glibc |
|---|---|---|
| my allocator, FIT_FIRST | 0.2549 | ≈ 37× slower |
| my allocator, FIT_BEST | 0.3544 | ≈ 51× slower |
| glibc malloc/free | 0.0069 | 1× |

## Discussion

I would choose first fit, as it was better in terms of speed. Best fit took 39% longer to complete (0.354 s) compared to first fit (0.255 s), because best fit searches the entire list for every allocation. Although best fit uses less memory than first fit (65,536 bytes less, one fewer 64 KB chunk allocated by sbrk) and wastes less memory, the difference is not significant. However, best fit does suffer from more external fragmentation (0.718) than first fit (0.685). This is due to best fit allocating the smallest block of memory possible, creating many small fragments. As there is no significant improvement in memory efficiency, I cannot justify the large decrease in speed.

Both implementations were much slower than glibc (around 37× slower for first fit). This is partly due to my implementation having only one list of blocks (both free and allocated), meaning that searching for a block in my_malloc and coalescing in my_free both take O(n) time. In contrast, glibc has multiple lists of free blocks depending on their size. Therefore, when allocating memory, it can find an appropriate block very quickly, in approximately constant time.
